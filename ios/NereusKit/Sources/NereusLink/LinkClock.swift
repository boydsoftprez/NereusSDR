// NereusSDR for iOS: the timers a session runs on, injectable for tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Runs an action after a delay. A session's connect deadline, heartbeat
/// and redial waits all run on one, so a test can move time by hand.
public protocol LinkClock: Sendable {
    var nowMilliseconds: Int64 { get }
    func schedule(after delay: Duration, _ action: @escaping @Sendable () async -> Void) -> any LinkTimer
}

extension LinkClock {
    public var nowMilliseconds: Int64 { Int64(ProcessInfo.processInfo.systemUptime * 1_000) }
}

/// A scheduled action that has not run yet.
public protocol LinkTimer: Sendable {
    /// Stops the action if it has not started.
    func cancel()
}
