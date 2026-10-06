// NereusSDR for iOS: the real-time clock a session runs on outside tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Schedules each action on a task that sleeps on the continuous clock.
public struct SystemLinkClock: LinkClock {
    public init() {}

    public func schedule(after delay: Duration, _ action: @escaping @Sendable () async -> Void) -> any LinkTimer {
        TaskTimer(task: Task {
            do {
                try await Task.sleep(for: delay)
            } catch {
                return
            }
            await action()
        })
    }

    private struct TaskTimer: LinkTimer {
        let task: Task<Void, Never>

        func cancel() {
            task.cancel()
        }
    }
}
