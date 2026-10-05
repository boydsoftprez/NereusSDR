// NereusSDR for iOS: lets a test wait for a condition without polling
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Tasks waiting for a condition over their owner's state. The owner calls
/// `release()` after each change it makes (outside its own lock), and each
/// waiter whose condition now holds resumes at once. A waiter whose
/// condition never holds resumes with false at its deadline, so a broken
/// test fails instead of hanging; the deadline is never how a passing test
/// gets its answer.
public final class ConditionWaiters: @unchecked Sendable {
    private struct Waiter {
        let condition: @Sendable () -> Bool
        let continuation: CheckedContinuation<Bool, Never>
        var deadline: Task<Void, Never>?
    }

    private let lock = NSLock()
    private var nextId: UInt64 = 0
    private var waiting: [UInt64: Waiter] = [:]

    public init() {}

    /// Waits until `condition` holds, or `timeout` passes; returns whether it held.
    public func wait(within timeout: Duration, until condition: @escaping @Sendable () -> Bool) async -> Bool {
        await withCheckedContinuation { continuation in
            let id = lock.withLock { () -> UInt64 in
                nextId += 1
                waiting[nextId] = Waiter(condition: condition, continuation: continuation)
                return nextId
            }
            let deadline = Task { [weak self] in
                try? await Task.sleep(for: timeout)
                if !Task.isCancelled {
                    self?.expire(id)
                }
            }
            let stillWaiting = lock.withLock { () -> Bool in
                guard waiting[id] != nil else {
                    return false
                }
                waiting[id]?.deadline = deadline
                return true
            }
            if !stillWaiting {
                deadline.cancel()
            }
            // The condition may already hold.
            release()
        }
    }

    /// Resumes every waiter whose condition holds now.
    public func release() {
        let ready = lock.withLock { () -> [Waiter] in
            let ids = waiting.filter { $0.value.condition() }.map(\.key)
            return ids.compactMap { waiting.removeValue(forKey: $0) }
        }
        for waiter in ready {
            waiter.deadline?.cancel()
            waiter.continuation.resume(returning: true)
        }
    }

    private func expire(_ id: UInt64) {
        guard let waiter = lock.withLock({ waiting.removeValue(forKey: id) }) else {
            return
        }
        waiter.continuation.resume(returning: waiter.condition())
    }
}
