// NereusSDR for iOS: delayed timer dispatch must preserve the transmit keepalive grid
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusLink

/// R-IOS-13: dispatching a timer late sends once, then resumes at the next
/// deadline on the original 100 ms grid, without replaying missed ticks.
@Suite struct PttKeepaliveGridTests {
    @Test func lateTickReturnsToTheOriginalGrid() async {
        let rig = Rig(startMilliseconds: 0)
        await rig.key()
        #expect(rig.clock.pendingDueTimes == [100])

        // The callback due at 100 runs at 135, rather than at its due time.
        await rig.clock.advanceLate(by: 135)
        #expect(await rig.core.keepaliveSequences == [1])
        #expect(rig.clock.pendingDueTimes == [200])

        await rig.clock.advanceLate(by: 65)
        #expect(await rig.core.keepaliveSequences == [1, 2])
        #expect(rig.clock.pendingDueTimes == [300])
        await rig.release()
    }

    @Test func veryLateTickSendsOnceWithoutCatchUp() async {
        // A nonzero start proves that deadlines belong to this key's grid,
        // rather than to multiples of 100 measured from clock creation.
        let rig = Rig(startMilliseconds: 137)
        await rig.key()
        #expect(rig.clock.pendingDueTimes == [237])

        // The deadlines at 237, 337 and 437 have passed. Only one send goes.
        await rig.clock.advanceLate(by: 350)
        #expect(rig.clock.nowMilliseconds == 487)
        #expect(await rig.core.keepaliveSequences == [1])
        #expect(rig.clock.pendingDueTimes == [537])

        // A newly scheduled immediate callback would be a catch-up burst.
        await rig.clock.advanceLate(by: 0)
        #expect(await rig.core.keepaliveSequences == [1])
        await rig.clock.advanceLate(by: 49)
        #expect(await rig.core.keepaliveSequences == [1])
        await rig.clock.advanceLate(by: 1)
        #expect(await rig.core.keepaliveSequences == [1, 2])
        #expect(rig.clock.pendingDueTimes == [637])
        await rig.release()
    }

    private struct Rig {
        let core = PttControllerTests.RecordingCore()
        let clock: LateClock
        let ptt: PttController

        init(startMilliseconds: Int64) {
            let clock = LateClock(startMilliseconds: startMilliseconds)
            self.clock = clock
            ptt = PttController(commands: core, clock: clock)
        }

        func key() async {
            await ptt.linkChanged(up: true)
            await ptt.update(TransmitStateReport())
            await ptt.tap()
            await ptt.settle()
        }

        func release() async {
            await ptt.tap()
            await ptt.settle()
        }
    }

    /// An in-memory scheduler whose time advances before callbacks run.
    /// Unlike ManualLinkClock, it can dispatch a timer after its deadline.
    private final class LateClock: LinkClock, @unchecked Sendable {
        private struct Entry {
            let id: Int
            let due: Int64
            let action: @Sendable () async -> Void
        }

        private struct Timer: LinkTimer {
            let clock: LateClock
            let id: Int

            func cancel() { clock.cancel(id) }
        }

        private let lock = NSLock()
        private var nowMs: Int64
        private var nextId = 0
        private var entries: [Entry] = []

        init(startMilliseconds: Int64) { nowMs = startMilliseconds }

        var nowMilliseconds: Int64 { lock.withLock { nowMs } }
        var pendingDueTimes: [Int64] { lock.withLock { entries.map(\.due).sorted() } }

        func schedule(after delay: Duration, _ action: @escaping @Sendable () async -> Void) -> any LinkTimer {
            let parts = delay.components
            let milliseconds = parts.seconds * 1000 + parts.attoseconds / 1_000_000_000_000_000
            let id = lock.withLock { () -> Int in
                let id = nextId
                nextId += 1
                entries.append(Entry(id: id, due: nowMs + milliseconds, action: action))
                return id
            }
            return Timer(clock: self, id: id)
        }

        private func cancel(_ id: Int) {
            lock.withLock { entries.removeAll { $0.id == id } }
        }

        /// Runs the callbacks already due at the new time. New callbacks
        /// wait for another advance, including advanceLate(by: 0), so tests
        /// can check for an immediate catch-up callback without sleeping.
        func advanceLate(by milliseconds: Int64) async {
            let due = lock.withLock { () -> [Entry] in
                nowMs += milliseconds
                let due = entries.filter { $0.due <= nowMs }
                    .sorted { ($0.due, $0.id) < ($1.due, $1.id) }
                entries.removeAll { $0.due <= nowMs }
                return due
            }
            for entry in due { await entry.action() }
        }
    }
}
