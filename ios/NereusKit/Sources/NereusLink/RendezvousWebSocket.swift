// NereusSDR for iOS: the TLS WebSocket to the remote access service, on Network framework
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Network
import os

/// One connection to a remote access service (the rendezvous document,
/// section 2): `wss://host:port/`, opened over HTTP/1.1 with RFC 6455's
/// upgrade, which Network framework's WebSocket makes (`GET / HTTP/1.1`,
/// `Upgrade: WebSocket`), and never over HTTP/2. The service's certificate
/// is checked the ordinary way, against the system's trusted roots for the
/// service's name; nothing is pinned. The Host header carries the name and
/// port (`rv.nereussdr.com:443`), the exact opening proven through the
/// NereusSDR service's Caddy (rendezvous session, 2026-09-26). Messages
/// from the service are accepted up to 256 KiB, and its pings are answered
/// by the WebSocket itself.
///
/// It is a ``LinkTransport`` so the tests can play the service with the
/// link's scripted connection; `open` reports no certificate digest, since
/// nothing binds one here, and `ping` is never used.
public final class RendezvousWebSocket: LinkTransport, @unchecked Sendable {
    /// Makes the real connection to a service.
    public static let factory: RendezvousTransportFactory = { server in
        RendezvousWebSocket(server: server)
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "rendezvous.transport")

    private let server: RendezvousServer
    private let openDeadline: Duration
    private let proxyResolver: SystemProxyResolver
    private let plain: Bool
    private let queue = DispatchQueue(label: "NereusSDR.rendezvous.websocket")

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var connection: NWConnection?
    private var opening: CheckedContinuation<Data, Error>?
    private var events: AsyncStream<LinkTransportEvent>.Continuation?
    private var openingTask: Task<Data, Error>?
    private var established = false
    private var finished = false

    /// `openDeadline` bounds the opening: the service's own handshake time
    /// (section 3), after which ``RendezvousClient`` tries the next service.
    /// The service is always reached over TLS.
    public convenience init(server: RendezvousServer, openDeadline: Duration = RendezvousClient.helloTimeout) {
        self.init(server: server, openDeadline: openDeadline, plainOnLoopback: false)
    }

    /// With `plainOnLoopback`, a service on this computer (a loopback
    /// literal: `127.0.0.1` or `::1`) is reached as `ws://`, without TLS, as
    /// the desktop allows a service only there; for the tests that run the
    /// service on this computer. Any other host keeps TLS. Only this
    /// package's tests reach it; the app cannot.
    package init(server: RendezvousServer, openDeadline: Duration = RendezvousClient.helloTimeout,
                 plainOnLoopback: Bool) {
        self.server = server
        self.openDeadline = openDeadline
        self.proxyResolver = SystemProxyResolver()
        plain = plainOnLoopback && Self.isLoopbackLiteral(server.host)
    }

    init(server: RendezvousServer, openDeadline: Duration, plainOnLoopback: Bool,
         proxyResolver: SystemProxyResolver) {
        self.server = server
        self.openDeadline = openDeadline
        self.proxyResolver = proxyResolver
        plain = plainOnLoopback && Self.isLoopbackLiteral(server.host)
    }

    /// The host is `127.0.0.1` (any 127.0.0.0/8 address) or `::1`.
    static func isLoopbackLiteral(_ host: String) -> Bool {
        if host == "::1" {
            return true
        }
        let parts = host.split(separator: ".", omittingEmptySubsequences: false)
        return parts.count == 4 && parts[0] == "127" && parts.allSatisfy { UInt8($0) != nil }
    }

    public func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        let startedAt = ContinuousClock().now
        let endpoint = server.endpoint
        guard endpoint.port != 0,
              let url = URL(string: "\(plain ? "ws" : "wss")://\(WebSocketLinkTransport.hostHeader(for: endpoint))/") else {
            throw LinkTransportError.failed("not a service address")
        }
        let (stream, continuation) = AsyncStream.makeStream(of: LinkTransportEvent.self)
        Task {
            for await event in stream {
                await onEvent(event)
            }
        }

        let task = Task { [self] in
            try await SystemProxyWebSocketOpening.run(
                target: url, timeout: openDeadline, startedAt: startedAt, resolver: proxyResolver
            ) { route, remaining in
                try await self.openAttempt(url: url, route: route, timeout: remaining, events: continuation)
            }
        }
        let closed = lock.withLock { () -> Bool in
            if finished { return true }
            openingTask = task
            events = continuation
            return false
        }
        if closed {
            continuation.finish()
            task.cancel()
        }
        return try await withTaskCancellationHandler {
            defer { lock.withLock { openingTask = nil } }
            do {
                let result = try await task.value
                if lock.withLock({ finished }) { throw LinkTransportError.failed("closed") }
                return result
            } catch {
                let wasClosed = lock.withLock { finished }
                close()
                if wasClosed || Task.isCancelled || error is CancellationError {
                    throw LinkTransportError.failed("closed")
                }
                if let proxyError = error as? SystemProxyError {
                    throw LinkTransportError.failed(proxyError.openingFailureText)
                }
                throw error
            }
        } onCancel: {
            close()
        }
    }

    private func openAttempt(url: URL, route: SystemProxyRoute, timeout: Duration,
                             events continuation: AsyncStream<LinkTransportEvent>.Continuation) async throws -> Data {
        let deadline = ContinuousClock().now + timeout
        let endpoint = server.endpoint

        // TLS 1.2 or later, checked against the system's roots for the
        // service's name (no verify block: Network framework's own
        // evaluation), with no ALPN, so the WebSocket opens over HTTP/1.1.
        let tls = NWProtocolTLS.Options()
        sec_protocol_options_set_min_tls_protocol_version(tls.securityProtocolOptions, .TLSv12)

        let webSocket = NWProtocolWebSocket.Options()
        webSocket.autoReplyPing = true
        webSocket.maximumMessageSize = RendezvousMessage.maxReceivedBytes
        webSocket.setAdditionalHeaders([("Host", WebSocketLinkTransport.hostHeader(for: endpoint))])

        let parameters = NWParameters(tls: plain ? nil : tls, tcp: NWProtocolTCP.Options())
        parameters.defaultProtocolStack.applicationProtocols.insert(webSocket, at: 0)
        SystemProxyNetworkAdapter.apply(route, to: parameters)
        let connection = NWConnection(to: .url(url), using: parameters)

        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (opening: CheckedContinuation<Data, Error>) in
                let alreadyClosed = lock.withLock { () -> Bool in
                    if finished || Task.isCancelled {
                        return true
                    }
                    self.connection = connection
                    self.events = continuation
                    self.opening = opening
                    return false
                }
                if alreadyClosed {
                    continuation.finish()
                    opening.resume(throwing: LinkTransportError.failed("closed before opening"))
                    return
                }
                connection.stateUpdateHandler = { [weak self] state in
                    self?.stateChanged(state, on: connection)
                }
                connection.start(queue: queue)
                let remaining = deadline - ContinuousClock().now
                guard remaining > .zero else {
                    failAttempt("no reply", on: connection)
                    return
                }
                let (seconds, attoseconds) = remaining.components
                let nanoseconds = Int(seconds) * 1_000_000_000 + Int(attoseconds / 1_000_000_000)
                queue.asyncAfter(deadline: .now() + .nanoseconds(nanoseconds)) { [weak self] in
                    self?.openDeadlinePassed(on: connection)
                }
            }
        } onCancel: {
            failAttempt("cancelled", on: connection)
        }
    }

    @discardableResult public func send(_ text: String) -> Bool {
        guard let connection = lock.withLock({ finished ? nil : connection }) else {
            return false
        }
        let metadata = NWProtocolWebSocket.Metadata(opcode: .text)
        let context = NWConnection.ContentContext(identifier: "text", metadata: [metadata])
        connection.send(content: Data(text.utf8), contentContext: context, isComplete: true,
                        completion: .contentProcessed { [weak self] error in
                            if error != nil {
                                Self.logger.warning("Sending to the remote access service failed")
                                self?.finish(on: connection)
                            }
                        })
        return true
    }

    /// Unused: the service pings, and this end answers.
    public func ping() {}

    public func close() {
        let (connection, opening, task) = lock.withLock { () -> (NWConnection?, CheckedContinuation<Data, Error>?, Task<Data, Error>?) in
            finished = true
            let taken = (self.connection, self.opening, self.openingTask)
            self.opening = nil
            self.connection = nil
            self.openingTask = nil
            events?.finish()
            events = nil
            return taken
        }
        task?.cancel()
        opening?.resume(throwing: LinkTransportError.failed("closed"))
        guard let connection else {
            return
        }
        guard connection.state == .ready else {
            connection.cancel()
            return
        }
        let metadata = NWProtocolWebSocket.Metadata(opcode: .close)
        metadata.closeCode = .protocolCode(.normalClosure)
        let context = NWConnection.ContentContext(identifier: "close", metadata: [metadata])
        connection.send(content: nil, contentContext: context, isComplete: true,
                        completion: .contentProcessed { _ in connection.cancel() })
    }

    // MARK: Network framework callbacks, on `queue`

    private func openDeadlinePassed(on connection: NWConnection) {
        let pending = lock.withLock { self.connection === connection && self.opening != nil }
        guard pending else {
            return
        }
        Self.logger.info("The remote access service did not answer in time")
        failAttempt("no reply", on: connection)
    }

    private func stateChanged(_ state: NWConnection.State, on connection: NWConnection) {
        guard lock.withLock({ self.connection === connection && !finished }) else { return }
        switch state {
        case .ready:
            let opening = lock.withLock { () -> CheckedContinuation<Data, Error>? in
                guard self.connection === connection && !finished else { return nil }
                let taken = self.opening
                self.opening = nil
                if taken != nil { established = true }
                return taken
            }
            guard let opening else {
                return
            }
            opening.resume(returning: Data())
            receiveNext(on: connection)
        case .waiting(let error):
            if lock.withLock({ established }) { finish(on: connection) }
            else { failAttempt(Self.networkFailureText(error), on: connection) }
        case .failed(let error):
            if lock.withLock({ established }) { finish(on: connection) }
            else { failAttempt(Self.networkFailureText(error), on: connection) }
        case .cancelled:
            if lock.withLock({ established }) { finish(on: connection) }
            else { failAttempt("cancelled", on: connection) }
        default:
            break
        }
    }

    private func failAttempt(_ reason: String, on connection: NWConnection) {
        let opening = lock.withLock { () -> CheckedContinuation<Data, Error>? in
            guard self.connection === connection && !established else { return nil }
            let taken = self.opening
            self.opening = nil
            self.connection = nil
            return taken
        }
        opening?.resume(throwing: LinkTransportError.failed(reason))
        connection.cancel()
    }

    private static func networkFailureText(_ error: NWError) -> String {
        if case .tls = error {
            return "TLS connection failed; network inspection or a sign-in page may be involved"
        }
        return "the network or its configured proxy did not open the remote access service"
    }

    private func receiveNext(on connection: NWConnection) {
        connection.receiveMessage { [weak self] content, context, isComplete, error in
            guard let self else {
                return
            }
            if error != nil {
                Self.logger.info("The connection to the remote access service ended")
                self.finish(on: connection)
                connection.cancel()
                return
            }
            let metadata = context?.protocolMetadata(definition: NWProtocolWebSocket.definition)
                as? NWProtocolWebSocket.Metadata
            switch metadata?.opcode {
            case .text?:
                if let content, let text = String(data: content, encoding: .utf8) {
                    self.emit(.text(text))
                }
            case .close?:
                self.finish(on: connection)
                connection.cancel()
                return
            case nil where isComplete && content == nil:
                self.finish(on: connection)
                connection.cancel()
                return
            default:
                // A binary frame is not part of the rendezvous; pings are
                // answered by the WebSocket itself.
                break
            }
            self.receiveNext(on: connection)
        }
    }

    private func emit(_ event: LinkTransportEvent) {
        let events = lock.withLock { finished ? nil : self.events }
        events?.yield(event)
    }

    private func finish(on expectedConnection: NWConnection) {
        let (events, connection) = lock.withLock { () -> (AsyncStream<LinkTransportEvent>.Continuation?, NWConnection?) in
            guard !finished && self.connection === expectedConnection else {
                return (nil, nil)
            }
            finished = true
            let taken = self.events
            let connection = self.connection
            self.events = nil
            self.connection = nil
            return (taken, connection)
        }
        connection?.cancel()
        events?.yield(.closed)
        events?.finish()
    }
}

/// Makes the connection to one service.
public typealias RendezvousTransportFactory = @Sendable (RendezvousServer) -> any LinkTransport
