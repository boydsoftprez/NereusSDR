// NereusSDR for iOS: how long to wait before dialling a Core again
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The redial schedule after a lost link or a retryable end: 1, 2, 5, 10,
/// 30 and 60 s, then 60 s for every attempt after (link document section
/// 12.4, `clientReconnectBackoffMs`).
public struct ReconnectPolicy: Sendable, Equatable {
    /// The waits, in seconds; the last one repeats.
    public static let delays: [Int] = [1, 2, 5, 10, 30, 60]

    private var scheduled = 0
    public private(set) var isCancelled = false

    public init() {}

    /// The wait before the next attempt, in seconds, or nil once cancelled.
    public mutating func nextDelay() -> Int? {
        guard !isCancelled else {
            return nil
        }
        let delay = Self.delays[min(scheduled, Self.delays.count - 1)]
        scheduled += 1
        return delay
    }

    /// Starts the schedule over, as after a session that reached ready, and
    /// lifts a cancellation, as when the operator connects again.
    public mutating func reset() {
        scheduled = 0
        isCancelled = false
    }

    /// Stops the next attempt at once: `nextDelay()` gives nil until `reset()`.
    public mutating func cancel() {
        isCancelled = true
    }
}
