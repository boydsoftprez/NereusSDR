// NereusSDR for iOS: records what a media control client sends and reports, for its tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
@testable import NereusMedia

/// Collects the operations a ``MediaControlClient`` sends, the events it
/// reports and the peers it makes.
final class MediaControlRecorder: @unchecked Sendable {
    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var sentPayloads: [[String: LinkJSON]] = []
    private var reported: [MediaControlEvent] = []
    private var made: [ScriptedMediaPeer] = []
    private var connectedCalls = 0
    private var nowMs: UInt64 = 1_000
    private var task: Task<Void, Never>?
    private var waiters: [(op: String, count: Int, resume: CheckedContinuation<Void, Never>)] = []

    var sender: MediaControlClient.Sender {
        { [self] message in
            guard case .mediaControl(let control) = message else {
                return
            }
            let ready = lock.withLock { () -> [CheckedContinuation<Void, Never>] in
                sentPayloads.append(control.payload)
                let met = waiters.filter { countSent($0.op) >= $0.count }
                waiters.removeAll { countSent($0.op) >= $0.count }
                return met.map(\.resume)
            }
            ready.forEach { $0.resume() }
        }
    }

    /// How many operations with this `op` were sent; call under `lock`.
    private func countSent(_ op: String) -> Int {
        sentPayloads.count { $0["op"] == .string(op) }
    }

    /// Returns once `count` operations with this `op` have been sent: woken
    /// by the send itself, so it neither polls nor gives up early.
    func waitUntilSent(_ op: String, count: Int = 1) async {
        await withCheckedContinuation { (resume: CheckedContinuation<Void, Never>) in
            let ready = lock.withLock { () -> Bool in
                if countSent(op) >= count {
                    return true
                }
                waiters.append((op, count, resume))
                return false
            }
            if ready {
                resume.resume()
            }
        }
    }

    var peerFactory: MediaControlClient.PeerFactory {
        { [self] in
            let peer = ScriptedMediaPeer()
            lock.withLock { made.append(peer) }
            return peer
        }
    }

    var clock: MediaControlClient.MillisecondClock {
        { [self] in lock.withLock { nowMs } }
    }

    var onMediaConnected: @Sendable () async -> Void {
        { [self] in lock.withLock { connectedCalls += 1 } }
    }

    func listen(to client: MediaControlClient) {
        task = Task { [weak self] in
            for await event in client.events {
                self?.lock.withLock { self?.reported.append(event) }
            }
        }
    }

    deinit {
        task?.cancel()
    }

    func advanceClock(by ms: UInt64) {
        lock.withLock { nowMs += ms }
    }

    var sent: [[String: LinkJSON]] { lock.withLock { sentPayloads } }
    var events: [MediaControlEvent] { lock.withLock { reported } }
    var peers: [ScriptedMediaPeer] { lock.withLock { made } }
    var mediaConnectedCalls: Int { lock.withLock { connectedCalls } }

    /// The operations sent with this `op`, in order.
    func sent(_ op: String) -> [[String: LinkJSON]] {
        sent.filter { $0["op"] == .string(op) }
    }

    /// Waits, yielding, for `condition` to hold; false if it never does.
    @discardableResult
    func settle(until condition: () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }
}
