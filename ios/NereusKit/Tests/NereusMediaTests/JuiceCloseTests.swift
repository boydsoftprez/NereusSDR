// NereusSDR for iOS: bounded TURN release and libjuice close ownership against the loopback fake
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if os(macOS)
import CJuice
import CJuiceCloseTestSupport
import Foundation
import LinkSessionTestSupport
import Testing

@Suite("libjuice close", .serialized)
struct JuiceCloseTests {
    private func agent(turn: LocalRendezvous.TurnFake, user: String,
                       mode: juice_concurrency_mode_t = JUICE_CONCURRENCY_MODE_POLL) -> OpaquePointer? {
        let credentials = turn.credentials(user: user)
        return "127.0.0.1".withCString { host in
            credentials.username.withCString { username in
                credentials.password.withCString { password in
                    var server = juice_turn_server_t()
                    server.host = host
                    server.port = turn.port
                    server.username = username
                    server.password = password
                    return withUnsafeMutablePointer(to: &server) { pointer in
                        var config = juice_config_t()
                        config.turn_servers = pointer
                        config.turn_servers_count = 1
                        config.concurrency_mode = mode
                        return juice_create(&config)
                    }
                }
            }
        }
    }

    private func agentWithResolverGate(turn: LocalRendezvous.TurnFake,
                                       lateTurn: LocalRendezvous.TurnFake,
                                       gate: URL) -> OpaquePointer? {
        let credentials = turn.credentials(user: "resolver")
        return "127.0.0.1".withCString { firstHost in
            "resolver-barrier.nereus.invalid".withCString { secondHost in
                credentials.username.withCString { username in
                    credentials.password.withCString { password in
                        ("gate:" + gate.path).withCString { gateUsername in
                            var servers = [juice_turn_server_t(), juice_turn_server_t()]
                            servers[0].host = firstHost
                            servers[0].port = turn.port
                            servers[0].username = username
                            servers[0].password = password
                            servers[1].host = secondHost
                            servers[1].port = lateTurn.port
                            servers[1].username = gateUsername
                            servers[1].password = password
                            return servers.withUnsafeMutableBufferPointer { buffer in
                                var config = juice_config_t()
                                config.turn_servers = buffer.baseAddress
                                config.turn_servers_count = 2
                                return juice_create(&config)
                            }
                        }
                    }
                }
            }
        }
    }

    @Test("no relay completes close without a retry window")
    func noRelayNeedsNoWait() throws {
        var config = juice_config_t()
        config.cb_state_changed = { _, _, _ in }
        config.cb_candidate = { _, _, _ in }
        config.cb_gathering_done = { _, _ in }
        config.cb_recv = { _, _, _, _ in }
        config.user_ptr = UnsafeMutableRawPointer(bitPattern: 1)
        let agent = try #require(juice_create(&config))
        #expect(juice_gather_candidates(agent) == 0)
        #expect(juice_begin_close(agent) == 0)
        #expect(juice_begin_close(agent) == 0)
        #expect(juice_close_status(agent) == JUICE_CLOSE_COMPLETE)
        #expect(juice_test_callbacks_cleared(agent) == 1)
        juice_destroy(agent)
    }

    @Test("mux has no TURN release and closes without retaining a network window")
    func muxNoTurnClose() throws {
        var config = juice_config_t()
        config.concurrency_mode = JUICE_CONCURRENCY_MODE_MUX
        let agent = try #require(juice_create(&config))
        #expect(juice_gather_candidates(agent) == 0)
        #expect(juice_begin_close(agent) == 0)
        #expect(juice_close_status(agent) == JUICE_CLOSE_COMPLETE)
        juice_destroy(agent)
    }

    @Test("a granted relay still releases after its entry is marked failed")
    func failedKnownAllocationReleases() async throws {
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let turn = try await LocalRendezvous.TurnFake(in: directory)
        defer { turn.stop() }
        let agent = try #require(self.agent(turn: turn, user: "failed"))
        defer { juice_destroy(agent) }
        #expect(juice_gather_candidates(agent) == 0)
        try await LocalRendezvous.waitUntil("a granted relay before failure", within: TestBackstop.hang,
                                            whileAlive: { turn.exitDiagnostic }) {
            turn.allocationCount("created") == 1
        }
        try await LocalRendezvous.waitUntil("the granted relay entry", within: .seconds(3)) {
            juice_test_mark_granted_relay_failed(agent) == 1
        }
        #expect(juice_begin_close(agent) == 0)
        try await turn.waitUntilReleased(1, within: .seconds(5))
        try await LocalRendezvous.waitUntil("failed-entry release acknowledgement", within: .seconds(2)) {
            juice_close_status(agent) == JUICE_CLOSE_COMPLETE
        }
        #expect(turn.allocationCount("created") == 1)
        #expect(turn.allocationCount("released") == 1)
        #expect(turn.allocationCount("live") == 0)
    }

    @Test("thread mode releases on its own receiver before final destruction")
    func threadModeRelease() async throws {
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let turn = try await LocalRendezvous.TurnFake(in: directory)
        defer { turn.stop() }
        let agent = try #require(self.agent(turn: turn, user: "thread", mode: JUICE_CONCURRENCY_MODE_THREAD))
        defer { juice_destroy(agent) }
        #expect(juice_gather_candidates(agent) == 0)
        try await LocalRendezvous.waitUntil("thread-mode allocation", within: TestBackstop.hang,
                                            whileAlive: { turn.exitDiagnostic }) {
            turn.allocationCount("created") == 1
        }
        #expect(juice_begin_close(agent) == 0)
        try await turn.waitUntilReleased(1, within: .seconds(5))
        try await LocalRendezvous.waitUntil("thread-mode release acknowledgement", within: .seconds(2)) {
            juice_close_status(agent) == JUICE_CLOSE_COMPLETE
        }
        #expect(turn.allocationCount("live") == 0)
    }

    @Test("total loss expires at the bounded deadline while another poll agent gathers")
    func totalLossLeavesAnotherAgentResponsive() async throws {
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let turn = try await LocalRendezvous.TurnFake(
            in: directory, options: ["--drop-all-release-requests"])
        defer { turn.stop() }
        let firstAgent = try #require(agent(turn: turn, user: "first"))
        let secondAgent = try #require(agent(turn: turn, user: "second"))
        var first: OpaquePointer? = firstAgent
        var second: OpaquePointer? = secondAgent
        defer {
            if let first { juice_destroy(first) }
            if let second { juice_destroy(second) }
        }
        #expect(juice_gather_candidates(firstAgent) == 0)
        try await LocalRendezvous.waitUntil("the first allocation", within: .seconds(20)) {
            turn.allocationCount("created") == 1
        }
        let started = ContinuousClock.now
        #expect(juice_begin_close(firstAgent) == 0)
        #expect(juice_begin_close(firstAgent) == 0, "duplicate close begins only one transaction")
        #expect(juice_close_status(firstAgent) == JUICE_CLOSE_PENDING)
        // The deadline is libjuice's, on its own clock; a poll on a busy
        // cooperative worker sees it as late as the pool lets that task run
        // (8.9 s on the CI runner, 2026-10-09). The status is read on a
        // thread of its own from here, and the time it first reads expired
        // is what the bound checks.
        let watch = CloseStatusWatch(firstAgent)
        defer { watch.stop() }
        let expiry = Task { await watch.firstExpiry(after: started) }

        #expect(juice_gather_candidates(secondAgent) == 0)
        try await LocalRendezvous.waitUntil("the second agent's allocation", within: .seconds(20)) {
            turn.allocationCount("created") == 2
        }
        try await LocalRendezvous.waitUntil("four bounded release requests", within: .seconds(6)) {
            turn.releaseTransactions.count == 4
        }
        let expiredAfter = try #require(await expiry.value, "the close deadline")
        #expect(expiredAfter < .seconds(7))
        #expect(juice_close_status(firstAgent) == JUICE_CLOSE_EXPIRED)
        #expect(turn.releaseTransactions.count == 4)
        #expect(turn.allocationCount("created") == 2)
        #expect(turn.allocationCount("released") == 0)
        #expect(turn.allocationCount("live") == 2)
        juice_destroy(firstAgent)
        first = nil

        // A second close is independent; ownership is consumed once each.
        #expect(juice_begin_close(secondAgent) == 0)
        try await LocalRendezvous.waitUntil("the second close deadline", within: .seconds(6)) {
            juice_close_status(secondAgent) == JUICE_CLOSE_EXPIRED
        }
        juice_destroy(secondAgent)
        second = nil
    }

    @Test("a resolver held outside the lock cannot revive a closing agent")
    func delayedResolverRechecksClose() async throws {
        let directory = try LocalRendezvous.scratch()
        defer { LocalRendezvous.remove(directory) }
        let gate = directory.appendingPathComponent("resolver-release")
        // Even a cancelled or failed assertion releases the test-only barrier.
        defer { FileManager.default.createFile(atPath: gate.path, contents: Data()) }
        let turn = try await LocalRendezvous.TurnFake(
            in: directory, options: ["--drop-all-release-requests"])
        defer { turn.stop() }
        let lateDirectory = directory.appendingPathComponent("late")
        try FileManager.default.createDirectory(at: lateDirectory, withIntermediateDirectories: true)
        let lateTurn = try await LocalRendezvous.TurnFake(in: lateDirectory)
        defer { lateTurn.stop() }
        let agent = try #require(agentWithResolverGate(turn: turn, lateTurn: lateTurn, gate: gate))
        var owned: OpaquePointer? = agent
        defer { if let owned { juice_destroy(owned) } }
        #expect(juice_gather_candidates(agent) == 0)
        try await LocalRendezvous.waitUntil("resolver test gate", within: .seconds(5),
                                            whileAlive: { turn.exitDiagnostic }) {
            FileManager.default.fileExists(atPath: gate.path + ".entered")
        }
        try await LocalRendezvous.waitUntil("allocation before resolver release", within: .seconds(5),
                                            whileAlive: { turn.exitDiagnostic }) {
            turn.allocationCount("created") == 1
        }
        let closeStarted = ContinuousClock.now
        #expect(juice_begin_close(agent) == 0)
        #expect(juice_close_status(agent) == JUICE_CLOSE_PENDING)
        try await LocalRendezvous.waitUntil("four release attempts while DNS is held", within: .seconds(6),
                                            whileAlive: { turn.exitDiagnostic }) {
            turn.releaseTransactions.count == 4
        }
        let deadline = closeStarted + .seconds(5.25)
        if ContinuousClock.now < deadline {
            try await Task.sleep(until: deadline)
        }
        #expect(juice_close_status(agent) == JUICE_CLOSE_PENDING,
                "network deadline alone cannot make an active resolver safe to destroy")
        #expect(turn.allocationCount("created") == 1)
        #expect(lateTurn.allocationCount("created") == 0)
        #expect(FileManager.default.createFile(atPath: gate.path, contents: Data()))
        try await LocalRendezvous.waitUntil("resolver exit after close", within: .seconds(3),
                                            whileAlive: { turn.exitDiagnostic }) {
            juice_close_status(agent) == JUICE_CLOSE_EXPIRED
        }
        #expect(turn.allocationCount("created") == 1, "relocking must skip the late relay entry")
        #expect(lateTurn.allocationCount("created") == 0,
                "a distinct late relay must not allocate after the close guard")
        #expect(turn.releaseTransactions.count == 4)
        juice_destroy(agent)
        owned = nil
    }

}
#endif

/// Reads one agent's close status on a thread of its own until it first
/// reads expired. The read and the stop share a lock, so once ``stop()``
/// returns the agent is not read again and the test may destroy it.
private final class CloseStatusWatch: @unchecked Sendable {
    private let lock = NSLock()
    private let agent: OpaquePointer
    private var stopped = false

    init(_ agent: OpaquePointer) {
        self.agent = agent
    }

    func stop() {
        lock.withLock { stopped = true }
    }

    /// How long after `started` the status first read expired, or nil if
    /// it never did within the hang backstop or the watch was stopped.
    func firstExpiry(after started: ContinuousClock.Instant) async -> Duration? {
        await TestNativeThread.run { [self] in
            while ContinuousClock.now - started < TestBackstop.hang {
                let expired = lock.withLock { () -> Bool? in
                    stopped ? nil : juice_close_status(agent) == JUICE_CLOSE_EXPIRED
                }
                switch expired {
                case nil:
                    return nil
                case true?:
                    return ContinuousClock.now - started
                case false?:
                    usleep(1_000)
                }
            }
            return nil
        }
    }
}
