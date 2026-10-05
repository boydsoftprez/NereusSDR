// NereusSDR for iOS: the steady clock the microphone's sends keep time by: host time, and a thread that wakes at absolute deadlines
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The time ``MicrophoneSender`` keeps its 20 ms send cadence by. `now` is
/// monotonic nanoseconds; `schedule(at:_:)` runs `work` once, as close
/// after the absolute `deadline` as the clock can. At most one piece of
/// work waits at a time: a second schedule replaces the first. `close()`
/// drops whatever waits and ends the clock; nothing runs after it.
public protocol MicrophoneSendClock: AnyObject, Sendable {
    var now: UInt64 { get }
    func schedule(at deadline: UInt64, _ work: @escaping @Sendable () -> Void)
    func close()
}

/// The phone's own send clock: host time (`mach_absolute_time`) and one
/// thread that sleeps with `mach_wait_until` to each absolute deadline.
/// Deadlines are absolute, so wake-up lateness never adds up the way a
/// repeating timer's can. One clock serves one key; `close()` ends its
/// thread.
public final class HostSendClock: MicrophoneSendClock, @unchecked Sendable {
    private let worker: Worker
    private let readNow: @Sendable () -> UInt64

    public convenience init() {
        self.init(now: { HostSendClock.nanoseconds(fromHostTicks: mach_absolute_time()) },
                  waitUntil: { deadline in mach_wait_until(HostSendClock.hostTicks(fromNanoseconds: deadline)) })
    }

    /// The clock with its time source and its sleep given: `now` in
    /// nanoseconds, and `waitUntil` returning once that time has come. The
    /// phone's clock is host time and `mach_wait_until`; a test gives time
    /// it moves by hand, so nothing it checks depends on how busy the
    /// computer is.
    init(now: @escaping @Sendable () -> UInt64, waitUntil: @escaping @Sendable (UInt64) -> Void) {
        readNow = now
        worker = Worker(waitUntil: waitUntil)
        let worker = self.worker
        let thread = Thread {
            worker.run()
        }
        thread.name = "NereusSDR.audio.microphone.send"
        thread.qualityOfService = .userInteractive
        thread.start()
    }

    deinit {
        worker.close()
    }

    public var now: UInt64 {
        readNow()
    }

    public func schedule(at deadline: UInt64, _ work: @escaping @Sendable () -> Void) {
        worker.schedule(at: deadline, work)
    }

    public func close() {
        worker.close()
    }

    private static let timebase: mach_timebase_info_data_t = {
        var info = mach_timebase_info_data_t()
        mach_timebase_info(&info)
        return info
    }()

    static func nanoseconds(fromHostTicks ticks: UInt64) -> UInt64 {
        let info = timebase
        return UInt64((Double(ticks) * Double(info.numer) / Double(info.denom)).rounded())
    }

    static func hostTicks(fromNanoseconds nanoseconds: UInt64) -> UInt64 {
        let info = timebase
        return UInt64((Double(nanoseconds) * Double(info.denom) / Double(info.numer)).rounded())
    }

    /// The thread's side: one waiting slot under a condition.
    private final class Worker: @unchecked Sendable {
        private let condition = NSCondition()
        private let waitUntil: @Sendable (UInt64) -> Void
        // Read and written under `condition`.
        private var slot: (id: UInt64, deadline: UInt64, work: @Sendable () -> Void)?
        private var nextId: UInt64 = 0
        private var closed = false

        init(waitUntil: @escaping @Sendable (UInt64) -> Void) {
            self.waitUntil = waitUntil
        }

        func schedule(at deadline: UInt64, _ work: @escaping @Sendable () -> Void) {
            condition.lock()
            defer { condition.unlock() }
            guard !closed else {
                return
            }
            nextId &+= 1
            slot = (nextId, deadline, work)
            condition.signal()
        }

        func close() {
            condition.lock()
            closed = true
            slot = nil
            condition.signal()
            condition.unlock()
        }

        func run() {
            while true {
                condition.lock()
                while slot == nil, !closed {
                    condition.wait()
                }
                guard !closed, let waiting = slot else {
                    condition.unlock()
                    return
                }
                condition.unlock()
                waitUntil(waiting.deadline)
                condition.lock()
                // Run it only if nothing replaced or cancelled it meanwhile.
                let due = !closed && slot?.id == waiting.id
                if due {
                    slot = nil
                }
                condition.unlock()
                if due {
                    waiting.work()
                }
            }
        }
    }
}
