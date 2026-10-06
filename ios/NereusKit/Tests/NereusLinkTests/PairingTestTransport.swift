// NereusSDR for iOS: a pairing connection whose Core's end the tests play by hand
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusLink

/// One connection under the pairing trust, its far end played by a test:
/// what the test delivers reaches the app in order, even before the app has
/// opened the connection, and what the app sends waits for the test to take
/// it.
final class PairingTestTransport: LinkTransport, @unchecked Sendable {
    let certificateSHA256: Data
    private let lock = NSLock()
    private let events: AsyncStream<LinkTransportEvent>
    private let sink: AsyncStream<LinkTransportEvent>.Continuation
    private var sent: [String] = []
    private var closedByApp = false
    private var trusts: [StationTrust] = []
    private let waiters = ConditionWaiters()

    init(certificateSHA256: Data = Data((0..<32).map { _ in UInt8.random(in: 0...255) })) {
        self.certificateSHA256 = certificateSHA256
        (events, sink) = AsyncStream.makeStream(of: LinkTransportEvent.self)
    }

    /// Hands out this connection, recording the trust it was dialled under.
    var factory: LinkTransportFactory {
        { [self] _, trust in
            lock.withLock { trusts.append(trust) }
            return self
        }
    }

    /// The trusts the app dialled under, one per connection.
    var dialledTrusts: [StationTrust] { lock.withLock { trusts } }

    // MARK: The app's end

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        let stream = events
        Task {
            for await event in stream {
                await onEvent(event)
            }
        }
        return certificateSHA256
    }

    @discardableResult func send(_ text: String) -> Bool {
        let admitted = lock.withLock { () -> Bool in
            guard !closedByApp else { return false }
            sent.append(text)
            return true
        }
        if admitted { waiters.release() }
        return admitted
    }

    func ping() {}

    func close() {
        lock.withLock { closedByApp = true }
        waiters.release()
    }

    // MARK: The Core's end

    var isClosedByApp: Bool { lock.withLock { closedByApp } }

    /// Frames the app sent that the test has not taken.
    var pending: [String] { lock.withLock { sent } }

    /// The next message the app sent, waiting for it; nil if none came in time.
    func nextSent(within timeout: Duration = .seconds(30)) async -> LinkMessage? {
        _ = await waiters.wait(within: timeout) { [self] in !pending.isEmpty }
        let text: String? = lock.withLock { sent.isEmpty ? nil : sent.removeFirst() }
        return text.flatMap { try? LinkCodec.decode($0) }
    }

    /// Waits until the app has closed the connection.
    func waitUntilClosed(within timeout: Duration = .seconds(30)) async -> Bool {
        await waiters.wait(within: timeout) { [self] in isClosedByApp }
    }

    func deliver(_ message: LinkMessage) {
        sink.yield(.text(LinkCodec.encode(message)))
    }

    func deliverText(_ text: String) {
        sink.yield(.text(text))
    }

    /// The Core closes the connection.
    func dropLink() {
        sink.yield(.closed)
    }
}
