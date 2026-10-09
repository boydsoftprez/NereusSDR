// NereusSDR for iOS: run a test step that blocks its thread outside cooperative workers
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Runs `operation` on a thread of its own and resumes with its result. For
/// a step that blocks the thread it runs on, such as a `DispatchQueue.sync`
/// onto a queue still busy with work: on a cooperative worker it holds one
/// of the pool's threads, of which there are as many as the machine has
/// cores. On a three-core runner three such steps at once stop every other
/// test's tasks, and their real-time bounds run out together. The caller
/// checks the result in its own Swift Testing task, since an expectation
/// recorded on this thread belongs to no test.
public enum TestNativeThread {
    public static func run<Value: Sendable>(
        _ operation: @escaping @Sendable () -> Value
    ) async -> Value {
        await withCheckedContinuation { continuation in
            let worker = Thread {
                continuation.resume(returning: operation())
            }
            worker.name = "NereusSDR.tests.native-step"
            worker.start()
        }
    }
}
