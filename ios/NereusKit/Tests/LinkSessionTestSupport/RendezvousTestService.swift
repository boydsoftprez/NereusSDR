// NereusSDR for iOS: a remote access service the tests play, one scripted connection per dial
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The service's end of the app's connections, played by a test. Each dial
/// makes a ``Connection``; what the test delivers reaches the app in order,
/// even before the app has opened it, and what the app sends waits for the
/// test to take it. A server can be made to refuse its connections, as a
/// service that cannot be reached.
public final class RendezvousTestService: @unchecked Sendable {
    /// One connection the app dialled.
    public final class Connection: LinkTransport, @unchecked Sendable {
        public let server: RendezvousServer
        private let lock = NSLock()
        private let events: AsyncStream<LinkTransportEvent>
        private let sink: AsyncStream<LinkTransportEvent>.Continuation
        private let refuse: Bool
        private var sent: [String] = []
        private var everSent: [String] = []
        private var closedByApp = false
        private let waiters = ConditionWaiters()

        init(server: RendezvousServer, refuse: Bool) {
            self.server = server
            self.refuse = refuse
            (events, sink) = AsyncStream.makeStream(of: LinkTransportEvent.self)
        }

        public func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            if refuse {
                throw LinkTransportError.failed("refused")
            }
            let stream = events
            Task {
                for await event in stream {
                    await onEvent(event)
                }
            }
            return Data()
        }

        @discardableResult public func send(_ text: String) -> Bool {
            let admitted = lock.withLock { () -> Bool in
                guard !closedByApp, !refuse else { return false }
                sent.append(text)
                everSent.append(text)
                return true
            }
            if admitted { waiters.release() }
            return admitted
        }

        public func ping() {}

        public func close() {
            lock.withLock { closedByApp = true }
            sink.finish()
            waiters.release()
        }

        public var isClosedByApp: Bool { lock.withLock { closedByApp } }
        /// Every text the app sent on this connection, taken or not.
        public var allSent: [String] { lock.withLock { everSent } }
        public var pending: [String] { lock.withLock { sent } }

        /// The next text the app sent; nil if none came within `timeout`.
        public func nextSent(within timeout: Duration = .seconds(30)) async -> String? {
            _ = await waiters.wait(within: timeout) { [self] in !pending.isEmpty }
            return lock.withLock { sent.isEmpty ? nil : sent.removeFirst() }
        }

        /// The next message the app sent, decoded as the service reads it.
        public func nextMessage(within timeout: Duration = .seconds(30)) async -> RendezvousMessage? {
            guard let text = await nextSent(within: timeout) else {
                return nil
            }
            return try? RendezvousMessage.decode(text, direction: .toService)
        }

        /// True when the app closed the connection within `timeout`.
        public func waitUntilClosed(within timeout: Duration = .seconds(30)) async -> Bool {
            await waiters.wait(within: timeout) { [self] in isClosedByApp }
        }

        public func deliver(_ text: String) {
            sink.yield(.text(text))
        }

        public func deliver(_ message: RendezvousMessage) {
            deliver(message.encoded)
        }

        /// The service closes the connection.
        public func drop() {
            sink.yield(.closed)
            sink.finish()
        }

        /// A hello with a fresh nonce, as the NereusSDR service sends it;
        /// returns the nonce's bytes.
        @discardableResult
        public func greet() -> Data {
            let nonce = Data((0..<32).map { _ in UInt8.random(in: 0...255) })
            deliver(.hello(RendezvousMessage.Hello(version: 1, nonce: Base64URL.encode(nonce),
                                                   stun: RendezvousTestService.stunUrls)))
            return nonce
        }
    }

    public static let stunUrls = ["stun:rv4.conformance.invalid:3478", "stun:rv6.conformance.invalid:3478"]

    private let lock = NSLock()
    private var made: [Connection] = []
    private var refused: Set<RendezvousServer> = []
    private let waiters = ConditionWaiters()

    public init() {}

    /// Makes `server` refuse every connection, as a service that cannot be reached.
    public func refuse(_ server: RendezvousServer) {
        lock.withLock { _ = refused.insert(server) }
    }

    public var factory: RendezvousTransportFactory {
        { [self] server in
            let connection = lock.withLock { () -> Connection in
                let connection = Connection(server: server, refuse: refused.contains(server))
                made.append(connection)
                return connection
            }
            waiters.release()
            return connection
        }
    }

    /// Every connection dialled, in order.
    public var connections: [Connection] { lock.withLock { made } }

    /// The `index`th connection dialled (from 0), waiting for it.
    public func connection(_ index: Int, within timeout: Duration = .seconds(30)) async -> Connection? {
        _ = await waiters.wait(within: timeout) { [self] in connections.count > index }
        let all = connections
        return all.count > index ? all[index] : nil
    }
}
