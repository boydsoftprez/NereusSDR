// NereusSDR for iOS: record subscription ownership across uncertain and stale outcomes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@MainActor
@Suite struct RecordStreamLifecycleTests {
    private final class User {}

    /// Observations are event-driven. A held sender returns only when released;
    /// accepted handoffs enter `sent` before any delayed return, while rejected
    /// attempts never enter it. Only unsubscribe is auto-answered.
    private actor Sender {
        private let held: Set<Int>
        private let rejected: Set<Int>
        private var attempts: [LinkMessage.CommandInvoke] = []
        private var observers: [Int: CheckedContinuation<LinkMessage.CommandInvoke, Never>] = [:]
        private var releases: [Int: CheckedContinuation<Void, Never>] = [:]
        private weak var commands: CommandClient?
        private(set) var sent: [LinkMessage.CommandInvoke] = []

        init(held: Set<Int>, rejected: Set<Int>) {
            self.held = held
            self.rejected = rejected
        }

        func bind(_ commands: CommandClient) { self.commands = commands }

        func send(_ message: LinkMessage) async throws {
            guard case .commandInvoke(let invoke) = message else { return }
            let position = attempts.count + 1
            attempts.append(invoke)
            if !rejected.contains(position) { sent.append(invoke) }
            observers.removeValue(forKey: position)?.resume(returning: invoke)
            if held.contains(position) {
                await withCheckedContinuation { releases[position] = $0 }
            }
            if rejected.contains(position) { throw LinkSendError.notConnected }
            if invoke.verb == "records.unsubscribe" {
                await commands?.receive(.commandResult(.init(
                    verb: invoke.verb, id: invoke.id, accepted: true, reason: "",
                    affected: [], values: nil)))
            }
        }

        func attempt(_ position: Int) async -> LinkMessage.CommandInvoke {
            if attempts.count >= position { return attempts[position - 1] }
            return await withCheckedContinuation { observers[position] = $0 }
        }

        func release(_ position: Int) {
            releases.removeValue(forKey: position)?.resume()
        }
    }

    private struct Rig {
        let client: RecordStreamClient
        let commands: CommandClient
        let clock: ManualLinkClock
        let sender: Sender
    }

    private func rig(held: Set<Int> = [], rejected: Set<Int> = []) async -> Rig {
        let sender = Sender(held: held, rejected: rejected)
        let send: CommandClient.Sender = { try await sender.send($0) }
        let clock = ManualLinkClock()
        let commands = CommandClient(clock: clock, send: send)
        await sender.bind(commands)
        let mirror = MirrorStore(send: send)
        mirror.apply(FixtureReplay.stationHello())
        mirror.apply(FixtureReplay.capabilities([RecordStreamClient.capabilityName: .i64(1)]))
        return Rig(client: RecordStreamClient(mirror: mirror, commands: commands),
                   commands: commands, clock: clock, sender: sender)
    }

    private func ready(_ rig: Rig) async {
        await rig.commands.handle(.stateChanged(.receivingSnapshot))
        rig.client.handle(.stateChanged(.receivingSnapshot))
        await rig.commands.handle(.stateChanged(.ready))
        rig.client.handle(.stateChanged(.ready))
    }

    private func task(_ rig: Rig) throws -> Task<Void, Never> {
        try #require(rig.client.recordCommandTask)
    }

    private func answer(_ invoke: LinkMessage.CommandInvoke, in rig: Rig,
                        accepted: Bool = true, reason: String = "") async {
        await rig.commands.receive(.commandResult(.init(
            verb: invoke.verb, id: invoke.id, accepted: accepted, reason: reason,
            affected: [], values: nil)))
    }

    private func expectStopped(_ rig: Rig, verbs: [String]) async {
        // A broken last-owner guard leaves the previous completed task here;
        // awaiting it still finishes, then exact sent-command assertions fail.
        await rig.client.recordCommandTask?.value
        let sent = await rig.sender.sent
        #expect(sent.map(\.verb) == verbs)
        #expect(sent.last?.verb == RecordStreamClient.unsubscribeVerb)
        #expect(sent.last?.args == [.init(name: "stream", value: .utf8("spots"))])
        #expect(!rig.client.isWanted("spots"))
        #expect(rig.client.streams["spots"] == nil)
    }

    @Test func aSentSubscribeTimeoutStillStopsOnlyWhenItsLastUserLeaves() async throws {
        let rig = await rig()
        let first = User()
        let last = User()
        rig.client.want("spots", backlog: 500, by: first)
        rig.client.want("spots", backlog: 500, by: last)
        #expect(rig.client.recordCommandTask == nil)
        await ready(rig)
        let subscribe = try task(rig)
        _ = await rig.sender.attempt(1)
        #expect(rig.clock.pendingDueTimes == [5_000])
        await rig.clock.advance(by: 5_000)
        await subscribe.value
        #expect(await rig.commands.waitingCount == 0)
        rig.client.apply(.init(stream: "spots", generation: 1, reset: true,
                               upserts: [.init(id: "1", fields: ["call": .string("VE3ABC")])], removes: []))
        rig.client.unwant("spots", by: first)
        let beforeLast = await rig.sender.sent
        #expect(beforeLast.map(\.verb) == ["records.subscribe"])
        #expect(rig.client.isWanted("spots", by: last))
        #expect(!rig.client.isWanted("spots", by: first))
        #expect(rig.client.records("spots").map(\.id) == ["1"])
        rig.client.unwant("spots", by: last)
        await expectStopped(rig, verbs: ["records.subscribe", "records.unsubscribe"])
        rig.client.apply(.init(stream: "spots", generation: 1, reset: false,
                               upserts: [.init(id: "2", fields: [:])], removes: []))
        #expect(rig.client.streams["spots"] == nil)
    }

    @Test func aRefusedFirstSubscribeHasNoRemoteSubscriptionToStop() async throws {
        let rig = await rig()
        rig.client.want("spots", backlog: 500)
        await ready(rig)
        let subscribe = try task(rig)
        let invoke = await rig.sender.attempt(1)
        await answer(invoke, in: rig, accepted: false, reason: "Unavailable")
        await subscribe.value
        #expect(rig.client.refusals["spots"] == "Unavailable")
        rig.client.unwant("spots")
        await rig.client.recordCommandTask?.value
        let sent = await rig.sender.sent
        #expect(sent.map(\.verb) == ["records.subscribe"])
        #expect(rig.client.refusals["spots"] == nil)
        #expect(!rig.client.isWanted("spots"))
    }

    @Test func aDefinitelyUnsentFirstSubscribeHasNothingToStop() async throws {
        let rig = await rig(held: [1], rejected: [1])
        rig.client.want("spots", backlog: 500)
        await ready(rig)
        let subscribe = try task(rig)
        _ = await rig.sender.attempt(1)
        await rig.sender.release(1)
        await subscribe.value
        rig.client.unwant("spots")
        await rig.client.recordCommandTask?.value
        let sent = await rig.sender.sent
        #expect(sent.isEmpty)
        #expect(!rig.client.isWanted("spots"))
        #expect(await rig.commands.waitingCount == 0)
    }

    @Test func aFailedRefreshDoesNotEraseAnEarlierAcceptedSubscription() async throws {
        let rig = await rig(held: [2], rejected: [2])
        rig.client.want("spots", backlog: 500)
        await ready(rig)
        let firstTask = try task(rig)
        let first = await rig.sender.attempt(1)
        await answer(first, in: rig)
        await firstTask.value
        rig.client.resubscribe("spots")
        let refresh = try task(rig)
        _ = await rig.sender.attempt(2)
        await rig.sender.release(2)
        await refresh.value
        rig.client.unwant("spots")
        await expectStopped(rig, verbs: ["records.subscribe", "records.unsubscribe"])
    }

    @Test func anOlderUnsentRefreshCannotRetireTheNewerSubscribe() async throws {
        let rig = await rig(held: [1], rejected: [1])
        rig.client.want("spots", backlog: 20)
        await ready(rig)
        let oldTask = try task(rig)
        _ = await rig.sender.attempt(1)
        // A newer backlog request exists before the old outcome is released.
        rig.client.want("spots", backlog: 200)
        let newTask = try task(rig)
        await rig.sender.release(1)
        await oldTask.value
        let newer = await rig.sender.attempt(2)
        #expect(newer.args == [.init(name: "stream", value: .utf8("spots")),
                              .init(name: "backlog", value: .i64(200))])
        await answer(newer, in: rig)
        await newTask.value
        rig.client.unwant("spots")
        await expectStopped(rig, verbs: ["records.subscribe", "records.unsubscribe"])
    }

    @Test func anOlderRefusalCannotReplaceTheNewerAcceptedRefresh() async throws {
        let rig = await rig()
        rig.client.want("spots", backlog: 500)
        await ready(rig)
        let oldTask = try task(rig)
        let old = await rig.sender.attempt(1)
        rig.client.resubscribe("spots")
        let newTask = try task(rig)
        let newer = await rig.sender.attempt(2)
        await answer(newer, in: rig)
        await newTask.value
        await answer(old, in: rig, accepted: false, reason: "Old refusal")
        await oldTask.value
        #expect(rig.client.refusals["spots"] == nil)
        rig.client.unwant("spots")
        await expectStopped(rig, verbs: ["records.subscribe", "records.subscribe", "records.unsubscribe"])
    }

    @Test func anOldAnswerAfterReconnectCannotChangeTheNewLifetime() async throws {
        let rig = await rig(held: [1])
        rig.client.want("spots", backlog: 500)
        await ready(rig)
        let oldTask = try task(rig)
        let old = await rig.sender.attempt(1)
        // Invoke cannot return until its sender returns; its final result is
        // already buffered. Reconnect therefore precedes the client handler.
        await answer(old, in: rig, accepted: false, reason: "Old session refusal")
        await rig.commands.handle(.stateChanged(.waitingToRetry(seconds: 5)))
        rig.client.handle(.stateChanged(.waitingToRetry(seconds: 5)))
        #expect(!rig.client.available && rig.client.streams.isEmpty)
        await ready(rig)
        let newTask = try task(rig)
        await rig.sender.release(1)
        await oldTask.value
        #expect(rig.client.refusals["spots"] == nil)
        let newer = await rig.sender.attempt(2)
        await answer(newer, in: rig)
        await newTask.value
        rig.client.unwant("spots")
        await expectStopped(rig, verbs: ["records.subscribe", "records.subscribe", "records.unsubscribe"])
    }

    @Test func severalDefinitelyUnsentAttemptsCreateNoCleanupResponsibility() async throws {
        let rig = await rig(held: [1, 2], rejected: [1, 2])
        rig.client.want("spots", backlog: 20)
        await ready(rig)
        let oldTask = try task(rig)
        _ = await rig.sender.attempt(1)
        rig.client.resubscribe("spots")
        let newTask = try task(rig)
        await rig.sender.release(1)
        await oldTask.value
        _ = await rig.sender.attempt(2)
        await rig.sender.release(2)
        await newTask.value
        rig.client.unwant("spots")
        await rig.client.recordCommandTask?.value
        let sent = await rig.sender.sent
        #expect(sent.isEmpty)
        #expect(!rig.client.isWanted("spots"))
        #expect(await rig.commands.waitingCount == 0)
    }
}
