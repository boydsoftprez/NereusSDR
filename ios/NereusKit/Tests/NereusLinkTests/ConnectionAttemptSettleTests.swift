// NereusSDR for iOS: an attempt record catches up with how its concurrent paths ended
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusLink

struct ConnectionAttemptSettleTests {
    @Test func pathsStillTryingTakeTheirFinalOutcome() {
        var attempt = ConnectionAttempt(tries: [
            .init(path: .relay, address: "relay.example", outcome: .connected),
            .init(path: .direct, address: "192.0.2.10 port 50055", outcome: .trying),
            .init(path: .thisNetwork, address: "192.0.2.11 port 50055", outcome: .noAnswer),
        ])
        attempt.settle(from: [
            .init(path: .relay, address: "relay.example", outcome: .trying),
            .init(path: .direct, address: "192.0.2.10 port 50055", outcome: .cancelled),
            .init(path: .thisNetwork, address: "192.0.2.11 port 50055", outcome: .cancelled),
            .init(path: .direct, address: "198.51.100.7 port 50055", outcome: .timedOut),
        ], keeping: 0)
        #expect(attempt.tries.map(\.outcome) == [.connected, .cancelled, .noAnswer])
        #expect(attempt.summary == "Tried relay (relay.example): connected; "
                + "direct (192.0.2.10 port 50055): stopped after another path connected; "
                + "this network (192.0.2.11 port 50055): no answer.")
    }

    @Test func theKeptRowAndPathsStillOpenStayAsTheyAre() {
        var attempt = ConnectionAttempt(tries: [
            .init(path: .direct, address: "192.0.2.10 port 50055", outcome: .trying),
            .init(path: .relay, address: "relay.example", outcome: .trying),
        ])
        attempt.settle(from: [
            .init(path: .direct, address: "192.0.2.10 port 50055", outcome: .cancelled),
            .init(path: .relay, address: "relay.example", outcome: .trying),
        ], keeping: 0)
        #expect(attempt.tries.map(\.outcome) == [.trying, .trying])
    }
}
