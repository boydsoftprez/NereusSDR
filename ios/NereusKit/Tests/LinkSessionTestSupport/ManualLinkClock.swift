// NereusSDR for iOS: a clock the tests move by hand, so no test sleeps
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// Time moves only through `advance(by:)`, which runs every action that
/// falls due, in order, each to completion before the next, including
/// actions scheduled by earlier ones inside the same window.
public final class ManualLinkClock: LinkClock, @unchecked Sendable {
    private struct Entry {
        let id: Int
        let due: Int64
        let action: @Sendable () async -> Void
    }

    private struct Timer: LinkTimer {
        let clock: ManualLinkClock
        let id: Int

        func cancel() {
            clock.cancel(id)
        }
    }

    private let lock = NSLock()
    private var nowMs: Int64 = 0
    private var nextId = 0
    private var entries: [Entry] = []

    public init() {}

    /// Milliseconds since the clock was made.
    public var now: Int64 { lock.withLock { nowMs } }
    public var nowMilliseconds: Int64 { now }

    /// Every pending action's due time, soonest first.
    public var pendingDueTimes: [Int64] { lock.withLock { entries.map(\.due).sorted() } }

    public func schedule(after delay: Duration, _ action: @escaping @Sendable () async -> Void) -> any LinkTimer {
        let parts = delay.components
        let ms = parts.seconds * 1000 + parts.attoseconds / 1_000_000_000_000_000
        let id = lock.withLock { () -> Int in
            let id = nextId
            nextId += 1
            entries.append(Entry(id: id, due: nowMs + ms, action: action))
            return id
        }
        return Timer(clock: self, id: id)
    }

    private func cancel(_ id: Int) {
        lock.withLock { entries.removeAll { $0.id == id } }
    }

    /// Moves time forward by `ms`, running what falls due; `afterEach` runs
    /// after every action (a test's stand-in for the far end answering).
    public func advance(by ms: Int64, afterEach: @Sendable () async -> Void = {}) async {
        let target = now + ms
        while let entry = takeNext(until: target) {
            await entry.action()
            await afterEach()
        }
        lock.withLock { nowMs = max(nowMs, target) }
    }

    private func takeNext(until target: Int64) -> Entry? {
        lock.withLock { () -> Entry? in
            guard let index = entries.indices
                .filter({ entries[$0].due <= target })
                .min(by: { (entries[$0].due, entries[$0].id) < (entries[$1].due, entries[$1].id) }) else {
                return nil
            }
            let entry = entries.remove(at: index)
            nowMs = max(nowMs, entry.due)
            return entry
        }
    }
}
