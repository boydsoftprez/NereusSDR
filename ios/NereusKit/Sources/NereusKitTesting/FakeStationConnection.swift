// NereusSDR for iOS: one connection to the fake Core, delivering its events in order
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// One connection to a ``FakeStation``. Events reach the session one at a
/// time and in order, through a single queue.
final class FakeStationConnection: LinkTransport, @unchecked Sendable {
    private weak var station: FakeStation?
    private let lock = NSLock()
    private var queue: AsyncStream<Queued>.Continuation?
    private var pump: Task<Void, Never>?
    private var position = 0
    private var live = false
    private var closed = false

    private enum Queued {
        case event(LinkTransportEvent)
        /// Resumes once every event queued before it has been handled.
        case marker(CheckedContinuation<Void, Never>)
    }

    init(station: FakeStation) {
        self.station = station
    }

    deinit {
        queue?.finish()
        pump?.cancel()
    }

    var isLive: Bool { lock.withLock { live } }

    // MARK: LinkTransport

    func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
        let (stream, continuation) = AsyncStream.makeStream(of: Queued.self)
        let pumping = Task {
            for await item in stream {
                switch item {
                case .event(let event):
                    await onEvent(event)
                case .marker(let continuation):
                    continuation.resume()
                }
            }
        }
        lock.withLock {
            queue = continuation
            pump = pumping
        }
        // The session buffers what arrives while it finishes opening.
        advance()
        return station?.certificateSHA256 ?? Data()
    }

    @discardableResult func send(_ text: String) -> Bool {
        guard let station, lock.withLock({ !closed && queue != nil }) else {
            return false
        }
        let message = station.record(text)
        let expected = station.expectedKind(at: lock.withLock { position })
        if expected == nil, let message, lock.withLock({ position }) >= station.stepCount {
            // The fixture is played: the fake answers as the Core it plays.
            for reply in station.replies(to: message) {
                enqueue(.event(.text(LinkCodec.encode(reply))))
            }
            if station.closesAfterReplying(to: message) {
                enqueue(.event(.closed))
                lock.withLock { closed = true }
            }
            return true
        }
        guard let expected, let message, message.kind == expected else {
            return true
        }
        if case .authRequest(let request) = message, let refusal = station.refusal(for: request) {
            enqueue(.event(.text(LinkCodec.encode(.authResult(refusal)))))
            enqueue(.event(.closed))
            lock.withLock { closed = true }
            return true
        }
        lock.withLock { position += 1 }
        advance()
        return true
    }

    func ping() {
        enqueue(.event(.pong))
    }

    func close() {
        lock.withLock { closed = true }
        lock.withLock { queue }?.finish()
    }

    // MARK: The fake's end

    /// Queues the station messages up to the next client step.
    private func advance() {
        guard let station else {
            return
        }
        let run = station.stationRun(from: lock.withLock { position })
        for text in run.texts {
            enqueue(.event(.text(text)))
        }
        lock.withLock { position = run.next }
        guard run.next >= station.stepCount else {
            return
        }
        // Live once the session has handled the whole fixture.
        Task { [self] in
            await drained()
            lock.withLock { live = true }
            self.station?.connectionChanged()
        }
    }

    func deliverNow(_ text: String) async {
        enqueue(.event(.text(text)))
        await drained()
    }

    func dropNow() async {
        enqueue(.event(.closed))
        await drained()
        lock.withLock { closed = true }
    }

    private func enqueue(_ item: Queued) {
        let continuation = lock.withLock { queue }
        guard let continuation, case .enqueued = continuation.yield(item) else {
            if case .marker(let waiting) = item {
                waiting.resume()
            }
            return
        }
    }

    /// Returns once every event queued so far has been handled.
    private func drained() async {
        await withCheckedContinuation { continuation in
            enqueue(.marker(continuation))
        }
    }
}
