// NereusSDR for iOS: controlled HTTP CONNECT tunnel for proxy opening tests.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Network

final class LocalConnectProxy: @unchecked Sendable {
    private let queue = DispatchQueue(label: "NereusSDR.tests.connect-proxy")
    private let listener: NWListener
    private let upstreamPort: UInt16
    private let targetHost: String
    private let lock = NSLock()
    private var connections: [NWConnection] = []
    private var requests: [String] = []

    init(upstreamPort: UInt16, targetHost: String = "core.invalid") throws {
        self.upstreamPort = upstreamPort
        self.targetHost = targetHost
        let parameters = NWParameters.tcp
        parameters.requiredLocalEndpoint = .hostPort(host: "127.0.0.1", port: .any)
        listener = try NWListener(using: parameters)
    }

    func start() async throws -> UInt16 {
        listener.newConnectionHandler = { [weak self] connection in self?.accept(connection) }
        return try await withCheckedThrowingContinuation { continuation in
            let once = FirstOnly()
            listener.stateUpdateHandler = { [listener] state in
                switch state {
                case .ready:
                    if once.take(), let port = listener.port?.rawValue { continuation.resume(returning: port) }
                case .failed(let error):
                    if once.take() { continuation.resume(throwing: error) }
                default: break
                }
            }
            listener.start(queue: queue)
        }
    }

    var connectRequests: [String] { lock.withLock { requests } }

    func stop() {
        listener.cancel()
        lock.withLock { connections }.forEach { $0.cancel() }
    }

    private func accept(_ client: NWConnection) {
        lock.withLock { connections.append(client) }
        client.start(queue: queue)
        readHeader(from: client, buffer: Data())
    }

    private func readHeader(from client: NWConnection, buffer: Data) {
        client.receive(minimumIncompleteLength: 1, maximumLength: 65_536) { [weak self] content, _, complete, error in
            guard let self, error == nil, !complete else { return }
            var next = buffer
            if let content { next.append(content) }
            guard let end = next.range(of: Data("\r\n\r\n".utf8)) else {
                if next.count < 65_536 { self.readHeader(from: client, buffer: next) }
                return
            }
            let request = String(decoding: next[..<end.lowerBound], as: UTF8.self)
            self.lock.withLock { self.requests.append(request) }
            guard request.hasPrefix("CONNECT \(self.targetHost):\(self.upstreamPort) HTTP/") else {
                client.cancel()
                return
            }
            let remainder = Data(next[end.upperBound...])
            let upstream = NWConnection(host: "127.0.0.1", port: NWEndpoint.Port(rawValue: self.upstreamPort)!, using: .tcp)
            self.lock.withLock { self.connections.append(upstream) }
            upstream.stateUpdateHandler = { [weak self] state in
                guard let self else { return }
                if case .ready = state {
                    upstream.stateUpdateHandler = nil
                    client.send(content: Data("HTTP/1.1 200 Connection Established\r\n\r\n".utf8),
                                completion: .contentProcessed { _ in
                        if !remainder.isEmpty { upstream.send(content: remainder, completion: .idempotent) }
                        self.pipe(client, to: upstream)
                        self.pipe(upstream, to: client)
                    })
                }
            }
            upstream.start(queue: self.queue)
        }
    }

    private func pipe(_ source: NWConnection, to destination: NWConnection) {
        source.receive(minimumIncompleteLength: 1, maximumLength: 65_536) { [weak self] content, _, complete, error in
            guard let self, error == nil, !complete else {
                source.cancel()
                destination.cancel()
                return
            }
            destination.send(content: content, completion: .contentProcessed { _ in
                self.pipe(source, to: destination)
            })
        }
    }

    private final class FirstOnly: @unchecked Sendable {
        private let lock = NSLock()
        private var used = false
        func take() -> Bool { lock.withLock { defer { used = true }; return !used } }
    }
}
