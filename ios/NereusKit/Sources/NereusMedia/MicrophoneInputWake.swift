// NereusSDR for iOS: the microphone input's wake: the audio input thread sets a flag and signals, and one thread of its own drains
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CAudioRing
import Foundation

/// Wakes the microphone's drain from the audio input thread without
/// queuing a block there. ``inputArrived()`` is all the input thread does
/// after its copy into ``MicrophoneRing``: one lock-free compare-and-swap
/// on a pending flag and, only when that flag was clear, one semaphore
/// signal. It never locks, allocates or queues, so callbacks that come
/// while a drain runs wake one more drain, not one each.
///
/// The drain runs on a thread of the wake's own. ``close()`` ends that
/// thread and returns once it has ended; nothing drains after it.
public final class MicrophoneInputWake: @unchecked Sendable {
    private let worker: Worker

    /// A wake whose thread runs `drain` once for each wake, or nil when
    /// the memory for its flags cannot be had.
    public init?(_ drain: @escaping @Sendable () -> Void) {
        guard let worker = Worker(drain: drain) else {
            return nil
        }
        self.worker = worker
        let thread = Thread {
            worker.run()
        }
        thread.name = "NereusSDR.audio.microphone.drain"
        thread.qualityOfService = .userInteractive
        thread.start()
    }

    deinit {
        worker.end(waiting: false)
    }

    /// The input thread: samples are in the ring. Lock-free.
    public func inputArrived() {
        worker.wake()
    }

    /// Ends the drain thread and waits for it, unless called on it.
    public func close() {
        worker.end(waiting: true)
    }

    private final class Worker: @unchecked Sendable {
        private let drain: @Sendable () -> Void
        /// 1 while a wake is signalled and its drain has not begun.
        private let pending: OpaquePointer
        /// 1 once closed.
        private let closed: OpaquePointer
        private let signal = DispatchSemaphore(value: 0)
        private let ended = DispatchSemaphore(value: 0)
        private let endedLock = NSLock()
        private var hasEnded = false
        private var thread: Thread?

        init?(drain: @escaping @Sendable () -> Void) {
            guard let pending = nereus_shared_value_create(0) else {
                return nil
            }
            guard let closed = nereus_shared_value_create(0) else {
                nereus_shared_value_destroy(pending)
                return nil
            }
            self.drain = drain
            self.pending = pending
            self.closed = closed
        }

        deinit {
            nereus_shared_value_destroy(pending)
            nereus_shared_value_destroy(closed)
        }

        func wake() {
            if nereus_shared_value_load(closed) == 0, nereus_shared_value_claim(pending) != 0 {
                signal.signal()
            }
        }

        func run() {
            endedLock.withLock { thread = Thread.current }
            while true {
                signal.wait()
                if nereus_shared_value_load(closed) != 0 {
                    break
                }
                // Clear before draining: input that lands during the drain
                // wakes the next one.
                nereus_shared_value_store(pending, 0)
                drain()
            }
            endedLock.withLock { hasEnded = true }
            ended.signal()
        }

        func end(waiting: Bool) {
            let first = nereus_shared_value_load(closed) == 0
            nereus_shared_value_store(closed, 1)
            if first {
                signal.signal()
            }
            let (onThread, done) = endedLock.withLock { (thread === Thread.current, hasEnded) }
            guard waiting, !onThread, !done else {
                return
            }
            ended.wait()
            // For any later close() that waits too.
            ended.signal()
        }
    }
}
