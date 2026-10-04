// NereusSDR for iOS: a TLS listener on loopback that records the WebSocket opening request and never answers it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Network

/// Accepts TLS on `::1` or `127.0.0.1` with a generated certificate, reads
/// the app's WebSocket opening request and never answers it: the request is
/// what the tests read, and the silence is a Core that never opens.
final class SilentTLSListener: @unchecked Sendable {
    private let queue = DispatchQueue(label: "NereusSDR.tests.silent-tls")
    private let listener: NWListener
    private let host: NWEndpoint.Host
    private let observe: @Sendable (String) -> Void
    private let lock = NSLock()
    private var connections: [NWConnection] = []
    private var buffers: [ObjectIdentifier: Data] = [:]
    private var requests: [String] = []

    /// `address` is "::1" or "127.0.0.1".
    init(address: String, observe: @escaping @Sendable (String) -> Void = { _ in }) throws {
        self.observe = observe
        let certificate = try TestCertificate.make()
        let tls = NWProtocolTLS.Options()
        guard let identity = sec_identity_create(certificate.identity) else {
            throw TestCertificate.Failure(description: "could not wrap the identity")
        }
        sec_protocol_options_set_local_identity(tls.securityProtocolOptions, identity)
        sec_protocol_options_set_min_tls_protocol_version(tls.securityProtocolOptions, .TLSv12)
        let parameters = NWParameters(tls: tls, tcp: NWProtocolTCP.Options())
        host = NWEndpoint.Host(address)
        parameters.requiredLocalEndpoint = .hostPort(host: host, port: .any)
        listener = try NWListener(using: parameters)
    }

    /// Starts listening; returns the port.
    func start() async throws -> UInt16 {
        listener.newConnectionHandler = { [weak self] connection in
            self?.accept(connection)
        }
        return try await withCheckedThrowingContinuation { continuation in
            let once = FirstOnly()
            listener.stateUpdateHandler = { [listener] state in
                let first = { once.take() }
                switch state {
                case .ready:
                    if first(), let port = listener.port?.rawValue {
                        continuation.resume(returning: port)
                    }
                case .failed(let error):
                    if first() {
                        continuation.resume(throwing: error)
                    }
                default:
                    break
                }
            }
            listener.start(queue: queue)
        }
    }

    func stop() {
        listener.cancel()
        lock.withLock { connections }.forEach { $0.cancel() }
    }

    /// Every complete opening request received, headers and all.
    var receivedRequests: [String] { lock.withLock { requests } }

    private func accept(_ connection: NWConnection) {
        let identity = ObjectIdentifier(connection)
        observe("TLS listener accepted \(identity)")
        connection.stateUpdateHandler = { [observe] state in
            observe("TLS listener state \(identity): \(state)")
        }
        lock.withLock { connections.append(connection) }
        connection.start(queue: queue)
        receive(on: connection)
    }

    private func receive(on connection: NWConnection) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: 65_536) { [weak self] content, _, isComplete, error in
            guard let self else {
                return
            }
            if error != nil || isComplete {
                self.observe("TLS receive socket completed \(ObjectIdentifier(connection)): \(String(describing: error))")
            }
            if let content {
                let key = ObjectIdentifier(connection)
                self.lock.withLock {
                    var buffer = self.buffers[key, default: Data()]
                    buffer.append(content)
                    if let end = buffer.range(of: Data("\r\n\r\n".utf8)) {
                        self.observe("TLS listener opening entry \(ObjectIdentifier(connection))")
                        self.requests.append(String(decoding: buffer[..<end.lowerBound], as: UTF8.self))
                        buffer = Data(buffer[end.upperBound...])
                    }
                    self.buffers[key] = buffer
                }
            }
            if error == nil && !isComplete {
                self.receive(on: connection)
            }
        }
    }

    /// Lets one of several callbacks through.
    private final class FirstOnly: @unchecked Sendable {
        private let lock = NSLock()
        private var done = false

        func take() -> Bool {
            lock.withLock {
                defer { done = true }
                return !done
            }
        }
    }
}
