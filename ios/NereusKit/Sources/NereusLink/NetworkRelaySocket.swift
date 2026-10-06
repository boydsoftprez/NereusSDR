// NereusSDR for iOS: binary WebSocket connection to the web relay
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Network

/// Network framework's HTTP/1.1 WebSocket upgrade with ordinary system TLS
/// trust. It reads exactly one complete message when the leg asks for one,
/// so a data flood cannot evict READY, PEER or END from a socket-side queue.
/// The grant token is sent later by RelayLeg and never logged here.
final class NetworkRelaySocket: RelayBinarySocket, @unchecked Sendable {
    private enum Received {
        case event(RelaySocketEvent)
        case ignored
    }

    private let connection: NWConnection
    private let queue = DispatchQueue(label: "NereusSDR.relay.websocket")
    private let lock = NSLock()
    private var opening: CheckedContinuation<Void, Error>?
    private var receiving: CheckedContinuation<Received, Never>?
    private var finished = false

    deinit { connection.cancel() }

    private init(url: URL, route: SystemProxyRoute) {
        let tls = NWProtocolTLS.Options()
        sec_protocol_options_set_min_tls_protocol_version(tls.securityProtocolOptions, .TLSv12)
        let ws = NWProtocolWebSocket.Options()
        ws.autoReplyPing = true
        ws.maximumMessageSize = RelayFrame.maximumMessageBytes
        let port = url.port ?? 443
        let host = url.host ?? ""
        let hostHeader = host.contains(":") ? "[\(host)]:\(port)" : "\(host):\(port)"
        ws.setAdditionalHeaders([("Host", hostHeader)])
        let tcp = NWProtocolTCP.Options()
        tcp.noDelay = true
        let parameters = NWParameters(tls: tls, tcp: tcp)
        parameters.defaultProtocolStack.applicationProtocols.insert(ws, at: 0)
        SystemProxyNetworkAdapter.apply(route, to: parameters)
        connection = NWConnection(to: .url(url), using: parameters)
    }

    static func connect(to url: URL) async throws -> any RelayBinarySocket {
        try await connect(to: url, resolver: SystemProxyResolver())
    }

    static func connect(to url: URL, resolver: SystemProxyResolver) async throws -> any RelayBinarySocket {
        do {
            return try await SystemProxyWebSocketOpening.run(
                target: url, timeout: .seconds(10), resolver: resolver
            ) { route, remaining in
                let socket = NetworkRelaySocket(url: url, route: route)
                try await socket.open(timeout: remaining)
                return socket
            }
        } catch let error as SystemProxyError {
            switch error {
            case .cancelled: throw RelaySocketError.closed
            case .timedOut: throw RelaySocketError.timedOut
            default: throw RelaySocketError.network(error.openingFailureText)
            }
        }
    }

    private func open(timeout: Duration) async throws {
        let deadline = ContinuousClock().now + timeout
        try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (opening: CheckedContinuation<Void, Error>) in
                let cancelled = lock.withLock { () -> Bool in
                    if finished { return true }
                    self.opening = opening
                    return false
                }
                if cancelled {
                    opening.resume(throwing: RelaySocketError.closed)
                    return
                }
                connection.stateUpdateHandler = { [weak self] state in self?.stateChanged(state) }
                connection.start(queue: queue)
                let remaining = deadline - ContinuousClock().now
                guard remaining > .zero else {
                    openTimedOut()
                    return
                }
                let (seconds, attoseconds) = remaining.components
                let nanoseconds = Int(seconds) * 1_000_000_000 + Int(attoseconds / 1_000_000_000)
                queue.asyncAfter(deadline: .now() + .nanoseconds(nanoseconds)) { [weak self] in
                    self?.openTimedOut()
                }
            }
        } onCancel: {
            Task { await self.close() }
        }
    }

    private func stateChanged(_ state: NWConnection.State) {
        switch state {
        case .ready:
            takeOpening()?.resume()
        case .failed(let error), .waiting(let error):
            let reason: String
            if case .tls = error {
                reason = "TLS connection failed; network inspection or a sign-in page may be involved"
            } else {
                reason = "the network or its configured proxy did not open the relay"
            }
            takeOpening()?.resume(throwing: RelaySocketError.network(reason))
            finish()
        case .cancelled:
            finish()
        default: break
        }
    }

    private func openTimedOut() {
        guard let opening = takeOpening() else { return }
        opening.resume(throwing: RelaySocketError.timedOut)
        finish()
    }

    private func takeOpening() -> CheckedContinuation<Void, Error>? {
        lock.withLock {
            let taken = opening
            opening = nil
            return taken
        }
    }

    func send(_ message: Data) async throws {
        guard !lock.withLock({ finished }) else { throw RelaySocketError.closed }
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            let waiter = SendWaiter(continuation)
            let metadata = NWProtocolWebSocket.Metadata(opcode: .binary)
            let context = NWConnection.ContentContext(identifier: "relay.binary", metadata: [metadata])
            connection.send(content: message, contentContext: context, isComplete: true,
                            completion: .contentProcessed { [weak self] error in
                if error == nil {
                    waiter.complete()
                } else {
                    waiter.fail(.closed)
                    self?.finish()
                }
            })
            queue.asyncAfter(deadline: .now() + .seconds(10)) { [weak self] in
                if waiter.fail(.timedOut) { self?.finish() }
            }
        }
    }

    func nextEvent() async -> RelaySocketEvent? {
        while !lock.withLock({ finished }) {
            let result = await withCheckedContinuation { (waiter: CheckedContinuation<Received, Never>) in
                let alreadyClosed = lock.withLock { () -> Bool in
                    if finished { return true }
                    receiving = waiter
                    return false
                }
                if alreadyClosed {
                    waiter.resume(returning: .event(.closed))
                    return
                }
                connection.receiveMessage { [weak self] content, context, isComplete, error in
                    guard let self else { return }
                    if error != nil {
                        self.finish()
                        return
                    }
                    let metadata = context?.protocolMetadata(definition: NWProtocolWebSocket.definition)
                        as? NWProtocolWebSocket.Metadata
                    switch metadata?.opcode {
                    case .binary?:
                        if let content { self.resolve(.event(.binary(content))) }
                        else { self.resolve(.ignored) }
                    case .text?: self.resolve(.event(.text))
                    case .close?: self.finish()
                    case nil where isComplete && content == nil: self.finish()
                    default: self.resolve(.ignored) // WebSocket ping/pong is handled by Network framework.
                    }
                }
            }
            switch result {
            case .event(let event): return event
            case .ignored: continue
            }
        }
        return nil
    }

    private func resolve(_ result: Received) {
        let waiter = lock.withLock { () -> CheckedContinuation<Received, Never>? in
            let taken = receiving
            receiving = nil
            return taken
        }
        waiter?.resume(returning: result)
    }

    func close() async { finish() }

    private func finish() {
        let (opening, receiving, shouldFinish) = lock.withLock {
            () -> (CheckedContinuation<Void, Error>?, CheckedContinuation<Received, Never>?, Bool) in
            guard !finished else { return (nil, nil, false) }
            finished = true
            let oldOpening = self.opening
            let oldReceiving = self.receiving
            self.opening = nil
            self.receiving = nil
            return (oldOpening, oldReceiving, true)
        }
        guard shouldFinish else { return }
        opening?.resume(throwing: RelaySocketError.closed)
        receiving?.resume(returning: .event(.closed))
        connection.cancel()
    }

    private final class SendWaiter: @unchecked Sendable {
        private let lock = NSLock()
        private var continuation: CheckedContinuation<Void, Error>?

        init(_ continuation: CheckedContinuation<Void, Error>) { self.continuation = continuation }
        func complete() {
            let taken = lock.withLock { () -> CheckedContinuation<Void, Error>? in
                let taken = continuation; continuation = nil; return taken
            }
            taken?.resume()
        }
        @discardableResult func fail(_ error: RelaySocketError) -> Bool {
            let taken = lock.withLock { () -> CheckedContinuation<Void, Error>? in
                let taken = continuation; continuation = nil; return taken
            }
            taken?.resume(throwing: error)
            return taken != nil
        }
    }
}

enum RelaySocketError: Error, Sendable { case closed, timedOut, network(String) }
