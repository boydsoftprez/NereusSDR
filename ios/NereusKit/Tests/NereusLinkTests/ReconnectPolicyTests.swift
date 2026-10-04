// NereusSDR for iOS: tests for the redial schedule
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusLink

@Suite struct ReconnectPolicyTests {
    @Test func theScheduleIsTheCoresThenSixtySecondsRepeating() {
        var policy = ReconnectPolicy()
        let waits = (0..<9).compactMap { _ in policy.nextDelay() }
        #expect(waits == [1, 2, 5, 10, 30, 60, 60, 60, 60])
    }

    @Test func resetStartsTheScheduleOver() {
        var policy = ReconnectPolicy()
        _ = policy.nextDelay()
        _ = policy.nextDelay()
        _ = policy.nextDelay()
        policy.reset()
        #expect(policy.nextDelay() == 1)
    }

    @Test func cancelStopsTheNextAttemptAtOnce() {
        var policy = ReconnectPolicy()
        _ = policy.nextDelay()
        policy.cancel()
        #expect(policy.isCancelled)
        #expect(policy.nextDelay() == nil)
        policy.reset()
        #expect(policy.nextDelay() == 1)
    }
}
