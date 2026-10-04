// NereusSDR for iOS: ordered WebSocket proxy opening tests.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusLink

private actor DialRecord {
    var routes: [SystemProxyRoute] = []
    var budgets: [Duration] = []
    func add(_ route: SystemProxyRoute, budget: Duration) {
        routes.append(route)
        budgets.append(budget)
    }
}

// The opening has one monotonic deadline. Advancing this clock tests its
// arithmetic without allowing scheduler delay to consume the fixture's budget.
private final class OpeningClock: @unchecked Sendable {
    let startedAt = ContinuousClock().now
    private let lock = NSLock()
    private var elapsed: Duration = .zero
    func now() -> ContinuousClock.Instant { lock.withLock { startedAt + elapsed } }
    func advance(_ duration: Duration) { lock.withLock { elapsed += duration } }
}

private actor OpeningCallbackGate {
    private var entered = false
    private var released = false
    private var enteredWaiters: [CheckedContinuation<Void, Never>] = []
    private var releaseWaiters: [CheckedContinuation<Void, Never>] = []
    func hold() async {
        entered = true
        for waiter in enteredWaiters { waiter.resume() }
        enteredWaiters.removeAll()
        if !released { await withCheckedContinuation { releaseWaiters.append($0) } }
    }
    func waitUntilEntered() async {
        if !entered { await withCheckedContinuation { enteredWaiters.append($0) } }
    }
    func release() {
        released = true
        for waiter in releaseWaiters { waiter.resume() }
        releaseWaiters.removeAll()
    }
}

@Suite struct SystemProxyWebSocketOpeningTests {
    private let target = URL(string: "wss://core.example:47910/")!
    private let proxy = SystemProxyRoute.httpCONNECT(.init(host: "proxy.example", port: 3128))

    @Test func triesOnlyConfiguredRoutesInOrder() async throws {
        let record = DialRecord()
        let proxy = self.proxy
        let result = try await SystemProxyWebSocketOpening.run(
            target: target, timeout: .seconds(1),
            resolve: { _, _ in SystemProxyPlan(routes: [proxy, .direct]) },
            attempt: { route, budget in
                await record.add(route, budget: budget)
                if route == proxy { throw LinkTransportError.failed("proxy unavailable") }
                return "connected"
            })
        #expect(result == "connected")
        #expect(await record.routes == [proxy, .direct])
    }

    @Test func proxyOnlyFailureNeverFallsThroughToDirect() async throws {
        let record = DialRecord()
        let proxy = self.proxy
        await #expect(throws: LinkTransportError.failed("proxy unavailable")) {
            _ = try await SystemProxyWebSocketOpening.run(
                target: target, timeout: .seconds(1),
                resolve: { _, _ in SystemProxyPlan(routes: [proxy]) },
                attempt: { route, budget -> String in
                    await record.add(route, budget: budget)
                    throw LinkTransportError.failed("proxy unavailable")
                })
        }
        #expect(await record.routes == [proxy])
    }

    @Test func pacTimeConsumesTheSameOverallDeadline() async throws {
        let record = DialRecord()
        let clock = OpeningClock()
        let proxy = self.proxy
        await #expect(throws: SystemProxyError.timedOut) {
            _ = try await SystemProxyWebSocketOpening.run(
                target: target, timeout: .milliseconds(80),
                startedAt: clock.startedAt, now: { clock.now() },
                resolve: { _, _ in
                    clock.advance(.milliseconds(100))
                    return SystemProxyPlan(routes: [proxy])
                },
                attempt: { route, budget -> String in
                    await record.add(route, budget: budget)
                    return "incorrect opening"
                })
        }
        #expect(await record.routes.isEmpty)
    }

    @Test func laterChoicesReceiveOnlyTheUnspentTime() async throws {
        let record = DialRecord()
        let clock = OpeningClock()
        let proxy = self.proxy
        let result = try await SystemProxyWebSocketOpening.run(
            target: target, timeout: .seconds(1),
            startedAt: clock.startedAt, now: { clock.now() },
            resolve: { _, budget in
                #expect(budget == .seconds(1))
                clock.advance(.milliseconds(30))
                return SystemProxyPlan(routes: [proxy, .direct])
            },
            attempt: { route, budget in
                await record.add(route, budget: budget)
                if route == proxy {
                    clock.advance(.milliseconds(30))
                    throw LinkTransportError.failed("proxy unavailable")
                }
                return "connected"
            })
        #expect(result == "connected")
        let budgets = await record.budgets
        #expect(budgets.count == 2)
        #expect(budgets[0] < .seconds(1))
        #expect(budgets[1] < budgets[0])
        #expect(budgets == [.milliseconds(970), .milliseconds(940)])
    }

    @Test func cancelledLateAttemptCannotWinOrStartNextRoute() async throws {
        let record = DialRecord()
        let clock = OpeningClock()
        let gate = OpeningCallbackGate()
        let proxy = self.proxy
        let task = Task {
            try await SystemProxyWebSocketOpening.run(
                target: target, timeout: .seconds(1),
                startedAt: clock.startedAt, now: { clock.now() },
                resolve: { _, _ in SystemProxyPlan(routes: [proxy, .direct]) },
                attempt: { route, budget in
                    await record.add(route, budget: budget)
                    // Model a native callback that arrives after cancellation.
                    await gate.hold()
                    return "late success"
                })
        }
        await gate.waitUntilEntered()
        task.cancel()
        await gate.release()
        await #expect(throws: CancellationError.self) { _ = try await task.value }
        #expect(await record.routes == [proxy])
    }

    @Test func corePinMismatchIsTerminal() async throws {
        let record = DialRecord()
        let proxy = self.proxy
        await #expect(throws: LinkTransportError.certificateMismatch) {
            _ = try await SystemProxyWebSocketOpening.run(
                target: target, timeout: .seconds(1),
                resolve: { _, _ in SystemProxyPlan(routes: [proxy, .direct]) },
                attempt: { route, budget -> String in
                    await record.add(route, budget: budget)
                    throw LinkTransportError.certificateMismatch
                })
        }
        #expect(await record.routes == [proxy])
    }
}
