// NereusSDR for iOS: settings sends stay with the session that admitted them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

private actor SendHold {
    private var entered = false
    private var enteredWaiters: [CheckedContinuation<Void, Never>] = []
    private var releaseWaiter: CheckedContinuation<Void, Never>?
    private var completed = false
    private var completedWaiters: [CheckedContinuation<Void, Never>] = []

    func hold() async {
        entered = true
        for waiter in enteredWaiters { waiter.resume() }
        enteredWaiters = []
        await withCheckedContinuation { releaseWaiter = $0 }
    }

    func waitUntilEntered() async {
        if entered { return }
        await withCheckedContinuation { enteredWaiters.append($0) }
    }

    func release() { releaseWaiter?.resume(); releaseWaiter = nil }
    func finish() {
        completed = true
        for waiter in completedWaiters { waiter.resume() }
        completedWaiters = []
    }
    func waitUntilFinished() async {
        if completed { return }
        await withCheckedContinuation { completedWaiters.append($0) }
    }
}

private final class MutableSettingsRoute: @unchecked Sendable {
    private let lock = NSLock()
    private var current = "OLD"
    private var sent: [String] = []
    private var sentWaiters: [(Int, CheckedContinuation<Void, Never>)] = []

    func use(_ name: String) { lock.withLock { current = name } }
    var destinations: [String] { lock.withLock { sent } }
    func waitForCount(_ count: Int) async {
        await withCheckedContinuation { continuation in
            let immediate = lock.withLock { () -> Bool in
                if sent.count >= count { return true }
                sentWaiters.append((count, continuation))
                return false
            }
            if immediate { continuation.resume() }
        }
    }
    private func record(_ name: String) {
        let due = lock.withLock { () -> [CheckedContinuation<Void, Never>] in
            sent.append(name)
            let due = sentWaiters.filter { $0.0 <= sent.count }.map { $0.1 }
            sentWaiters.removeAll { $0.0 <= sent.count }
            return due
        }
        for waiter in due { waiter.resume() }
    }
    func send(_ message: LinkMessage) async throws {
        record(lock.withLock { current })
    }
    func capture(hold: SendHold? = nil, failOld: Bool = false) -> SettingsProxyClient.BoundSender {
        let admitted = lock.withLock { current }
        return { [self] _, permit in
            if admitted == "OLD", let hold { await hold.hold() }
            let handedOff = failOld && admitted == "OLD" ? false : permit.handoff {
                record(admitted)
                return true
            }
            if admitted == "OLD", let hold { await hold.finish() }
            guard handedOff else { throw LinkSendError.notConnected }
        }
    }
}

@MainActor
@Suite struct SettingsSessionBindingTests {
    private static func snapshot(_ value: String? = nil) -> LinkMessage {
        .settingsSnapshot(.init(properties: value.map { [.init(name: "CWPitch", value: .utf8($0))] } ?? []))
    }

    private func authenticate(_ proxy: SettingsProxyClient, value: String? = nil) {
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        proxy.apply(Self.snapshot(value))
    }

    @Test func retiredQueuedOldWriteCannotCrossIntoNewSession() async {
        let hold = SendHold()
        let route = MutableSettingsRoute()
        let proxy = SettingsProxyClient(origin: "phone", send: route.send,
                                        captureSender: { route.capture(hold: hold) })
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        proxy.apply(.settingsSnapshot(.init(properties: [])))
        let old = Task { await proxy.write("CWPitch", "650") }
        await hold.waitUntilEntered()
        proxy.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(await old.value == .linkLost)
        route.use("NEW")
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        proxy.apply(.settingsSnapshot(.init(properties: [])))
        await hold.release()
        await hold.waitUntilFinished()
        #expect(route.destinations.isEmpty)
    }

    @Test func freshnessRotatesAcrossAuthenticationButNotLateRadioSnapshots() throws {
        let proxy = SettingsProxyClient(send: { _ in })
        #expect(proxy.currentSnapshotIdentity == nil)
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        #expect(proxy.currentSnapshotIdentity == nil)
        proxy.apply(Self.snapshot("600"))
        let first = try #require(proxy.currentSnapshotIdentity)
        proxy.apply(Self.snapshot("650"))
        #expect(proxy.currentSnapshotIdentity == first)
        #expect(proxy.value("CWPitch") == "650")
        proxy.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(proxy.currentSnapshotIdentity == nil)
        #expect(proxy.value("CWPitch") == "650")
        proxy.handle(.stateChanged(.receivingSnapshot))
        proxy.apply(FixtureReplay.accepted)
        #expect(proxy.currentSnapshotIdentity == nil)
        proxy.apply(Self.snapshot("650"))
        let second = try #require(proxy.currentSnapshotIdentity)
        #expect(second != first)
        #expect(!proxy.isCurrent(first))
        #expect(proxy.isCurrent(second))
        proxy.apply(FixtureReplay.accepted)
        #expect(proxy.currentSnapshotIdentity == nil)
        proxy.apply(Self.snapshot("650"))
        #expect(proxy.currentSnapshotIdentity != second)
    }

    @Test func staleIdentityAndRevokedAuthorityNeverMutateCache() async throws {
        let route = MutableSettingsRoute()
        let proxy = SettingsProxyClient(send: route.send, captureSender: { route.capture() })
        authenticate(proxy, value: "600")
        let current = try #require(proxy.currentSnapshotIdentity)
        let stale: UInt64 = current == 1 ? 2 : 1
        #expect(await proxy.writeBound("CWPitch", "650", expectedSnapshotIdentity: stale,
                                       authority: CommandSendPermit()) == .notSent)
        #expect(await proxy.removeBound("CWPitch", expectedSnapshotIdentity: stale,
                                        authority: CommandSendPermit()) == .notSent)
        let revoked = CommandSendPermit()
        revoked.revoke()
        #expect(await proxy.writeBound("CWPitch", "650", expectedSnapshotIdentity: current,
                                       authority: revoked) == .notSent)
        #expect(proxy.value("CWPitch") == "600")
        #expect(route.destinations.isEmpty)
    }

    @Test func failedSenderCaptureDoesNotFallBackToMutableRoute() async throws {
        let route = MutableSettingsRoute()
        let proxy = SettingsProxyClient(send: route.send, captureSender: { nil })
        authenticate(proxy, value: "600")
        let identity = try #require(proxy.currentSnapshotIdentity)
        #expect(await proxy.writeBound("CWPitch", "650", expectedSnapshotIdentity: identity,
                                       authority: CommandSendPermit()) == .notSent)
        #expect(await proxy.removeBound("CWPitch", expectedSnapshotIdentity: identity,
                                        authority: CommandSendPermit()) == .notSent)
        #expect(proxy.value("CWPitch") == "600")
        #expect(route.destinations.isEmpty)
    }

    @Test func authorityRevokedWhileHeldRollsBackAndSendsNothing() async throws {
        let hold = SendHold()
        let route = MutableSettingsRoute()
        let proxy = SettingsProxyClient(send: route.send,
                                        captureSender: { route.capture(hold: hold) }, clock: ManualLinkClock())
        authenticate(proxy, value: "600")
        let identity = try #require(proxy.currentSnapshotIdentity)
        let authority = CommandSendPermit()
        let write = Task { await proxy.writeBound("CWPitch", "650", expectedSnapshotIdentity: identity,
                                                  authority: authority) }
        await hold.waitUntilEntered()
        #expect(proxy.value("CWPitch") == "650")
        authority.revoke()
        await hold.release()
        await hold.waitUntilFinished()
        #expect(await write.value == .notSent)
        #expect(proxy.value("CWPitch") == "600")
        #expect(route.destinations.isEmpty)
    }

    @Test func oldFailureCannotUndoNewSnapshotOrPendingValue() async throws {
        let hold = SendHold()
        let route = MutableSettingsRoute()
        let proxy = SettingsProxyClient(send: route.send,
                                        captureSender: { route.capture(hold: hold, failOld: true) })
        authenticate(proxy, value: "600")
        let old = Task { await proxy.write("CWPitch", "650") }
        await hold.waitUntilEntered()
        proxy.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(await old.value == .linkLost)
        route.use("NEW")
        authenticate(proxy, value: "700")
        let identity = try #require(proxy.currentSnapshotIdentity)
        let newer = Task { await proxy.writeBound("CWPitch", "750", expectedSnapshotIdentity: identity,
                                                  authority: CommandSendPermit()) }
        await route.waitForCount(1)
        #expect(proxy.value("CWPitch") == "750")
        await hold.release()
        await hold.waitUntilFinished()
        #expect(proxy.value("CWPitch") == "750")
        proxy.apply(.settingsValue(.init(key: "CWPitch", origin: proxy.origin,
                                          properties: [.init(name: "CWPitch", value: .utf8("750"))])))
        #expect(await newer.value == .accepted)
        #expect(route.destinations == ["NEW"])
    }

    @Test func revokeAfterHandoffStillSettlesOwnEchoBeforeLaterWrite() async throws {
        let clock = ManualLinkClock()
        let hold = SendHold()
        let route = MutableSettingsRoute()
        let proxy = SettingsProxyClient(origin: "phone", send: route.send,
                                        captureSender: {
            let captured = route.capture()
            return { message, permit in
                try await captured(message, permit)
                if case .settingsWrite(let write) = message,
                   case .utf8("650")? = write.properties.first?.value {
                    await hold.hold()
                    await hold.finish()
                }
            }
        }, clock: clock)
        authenticate(proxy, value: "600")
        let identity = try #require(proxy.currentSnapshotIdentity)
        let authority = CommandSendPermit()
        let first = Task { await proxy.writeBound("CWPitch", "650", expectedSnapshotIdentity: identity,
                                                  authority: authority) }
        await route.waitForCount(1)
        await hold.waitUntilEntered()
        authority.revoke()
        let second = Task { await proxy.writeBound("CWPitch", "700", expectedSnapshotIdentity: identity,
                                                   authority: CommandSendPermit()) }
        await hold.release()
        await hold.waitUntilFinished()
        await route.waitForCount(2)
        proxy.apply(.settingsValue(.init(key: "CWPitch", origin: "phone",
                                          properties: [.init(name: "CWPitch", value: .utf8("650"))])))
        #expect(await first.value == .accepted)
        #expect(proxy.value("CWPitch") == "700")
        proxy.apply(.settingsValue(.init(key: "CWPitch", origin: "phone",
                                          properties: [.init(name: "CWPitch", value: .utf8("700"))])))
        #expect(await second.value == .accepted)
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func boundRemovalWaitsForEchoAndPreservesLaterWrite() async throws {
        let clock = ManualLinkClock()
        let route = MutableSettingsRoute()
        let proxy = SettingsProxyClient(origin: "phone", send: route.send,
                                        captureSender: { route.capture() }, clock: clock)
        authenticate(proxy, value: "600")
        let identity = try #require(proxy.currentSnapshotIdentity)
        let removal = Task { await proxy.removeBound("CWPitch", expectedSnapshotIdentity: identity,
                                                     authority: CommandSendPermit()) }
        await route.waitForCount(1)
        #expect(proxy.value("CWPitch") == nil)
        let write = Task { await proxy.writeBound("CWPitch", "700", expectedSnapshotIdentity: identity,
                                                  authority: CommandSendPermit()) }
        await route.waitForCount(2)
        proxy.apply(.settingsValue(.init(key: "CWPitch", origin: "", properties: [])))
        #expect(await removal.value == .accepted)
        #expect(proxy.value("CWPitch") == "700")
        proxy.apply(.settingsValue(.init(key: "CWPitch", origin: "phone",
                                          properties: [.init(name: "CWPitch", value: .utf8("700"))])))
        #expect(await write.value == .accepted)
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }
}
