// NereusSDR for iOS: a control's commands show the operator's value at the touch and keep it until the Core answers
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// JJ, 2026-10-01: every control acts at the touch, those that send a
/// command as well as those that write a property (`StationClient.cpp:1040-1068`).
/// The Core here answers a command only when a test says so.
@MainActor
@Suite struct CommandHoldQueueTests {
    private let sent = SentMessages()
    private let clock = ManualLinkClock()

    /// The Core's side of the commands: each waits until a test answers it.
    @MainActor
    final class FakeCore {
        private(set) var asked: [String] = []
        private var waiting: [CheckedContinuation<CommandResult, any Error>] = []

        func invoke(_ what: String) async throws -> CommandResult {
            asked.append(what)
            return try await withCheckedThrowingContinuation { waiting.append($0) }
        }

        var isWaiting: Bool {
            !waiting.isEmpty
        }

        func answer(accepted: Bool, reason: String = "") {
            waiting.removeFirst().resume(returning: CommandResult(accepted: accepted, reason: reason, affectedKeys: [],
                                                                  values: [:], phase: nil))
        }

        func fail(_ error: CommandError) {
            waiting.removeFirst().resume(throwing: error)
        }
    }

    private func store() -> MirrorStore {
        let store = MirrorStore(send: sent.sender, clock: clock)
        store.handle(.stateChanged(.receivingSnapshot))
        store.apply(FixtureReplay.stationHello(minor: 11))
        store.apply(FixtureReplay.accepted)
        store.apply(.schema(LinkMessage.Schema(className: "SliceModel", fields: [
            LinkMessage.SchemaField(ordinal: 3, name: "txAntenna", kind: .utf8),
            LinkMessage.SchemaField(ordinal: 4, name: "txSlice", kind: .bool),
        ])))
        for id in 0..<3 {
            store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:\(id)", className: "SliceModel", properties: [
                .init(ordinal: 3, name: "txAntenna", value: .utf8("ANT1")),
                .init(ordinal: 4, name: "txSlice", value: .bool(id == 0)),
            ])))
        }
        store.apply(.snapshotComplete)
        store.handle(.stateChanged(.ready))
        return store
    }

    private func delta(_ key: String, _ name: String, _ ordinal: UInt16, _ value: LinkMessage.PropertyValue) -> LinkMessage {
        .delta(LinkMessage.Delta(key: key, properties: [.init(ordinal: ordinal, name: name, value: value)]))
    }

    /// Lets the main actor run until `done`, without sleeping.
    private func turns(_ done: () -> Bool) async -> Bool {
        for _ in 0..<2_000 {
            if done() {
                return true
            }
            await Task.yield()
        }
        return done()
    }

    private func antenna(_ queue: CommandHoldQueue, _ core: FakeCore, _ label: String,
                         outcomes: @escaping @MainActor (CommandHoldQueue.Outcome) -> Void = { _ in }) {
        queue.send("txAntenna", shows: [.init("slice:0", "txAntenna", .text(label))],
                   invoke: { try await core.invoke(label) }, onOutcome: outcomes)
    }

    @Test("an old command timeout or late answer cannot restore over a newer queued touch")
    func timeoutCannotRestoreOverNewerTouch() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let client = CommandClient(clock: clock, send: sent.sender)
        await client.handle(.stateChanged(.ready))
        let queue = CommandHoldQueue(store: store)
        var outcomes: [CommandHoldQueue.Outcome] = []
        var lateDelivered = false
        func choose(_ label: String) {
            queue.send("antenna", shows: [.init("slice:0", "txAntenna", .text(label))], invokeWithLate: { late in
                try await client.invokeHeld("antenna", arguments: [], timeout: .seconds(5), onLateOutcome: { answer in
                    await MainActor.run { late(answer); lateDelivered = true }
                })
            }, onOutcome: { outcomes.append($0) })
        }
        choose("ANT2")
        #expect(await sent.settle(untilCount: 1))
        guard case .commandInvoke(let first)? = sent.messages.last else { Issue.record("missing first invoke"); return }
        await clock.advance(by: 4_000)
        choose("ANT3")
        store.apply(delta("slice:0", "txAntenna", 3, .utf8("ANT1")))
        await clock.advance(by: 1_000)
        #expect(await sent.settle(untilCount: 2))
        guard case .commandInvoke(let second)? = sent.messages.last else { Issue.record("missing second invoke"); return }
        #expect(slice["txAntenna"] == .text("ANT3") && outcomes.isEmpty)
        #expect(!store.isUnconfirmed("slice:0", property: "txAntenna"))
        await client.receive(.commandResult(.init(verb: "antenna", id: first.id, accepted: false,
                                                   reason: "Obsolete antenna refusal.", affected: [], values: nil)))
        #expect(await turns { lateDelivered })
        #expect(slice["txAntenna"] == .text("ANT3") && outcomes.isEmpty)
        await clock.advance(by: 4_999)
        #expect(slice["txAntenna"] == .text("ANT3"))
        await clock.advance(by: 1)
        #expect(await turns { !queue.isSending("antenna") })
        #expect(slice["txAntenna"] == .text("ANT1"))
        #expect(outcomes == [.failure(.timedOut)])
        #expect(store.isUnconfirmed("slice:0", property: "txAntenna"))
        await client.receive(.commandResult(.init(verb: "antenna", id: second.id, accepted: true,
                                                   reason: "", affected: [], values: nil)))
        #expect(await turns { outcomes.count == 2 })
        #expect(slice["txAntenna"] == .text("ANT1"), "a bare late acceptance cannot re-create the expired operator draft")
        #expect(!store.isUnconfirmed("slice:0", property: "txAntenna"))
        #expect(sent.count == 2)
    }

    @Test("a timed-out moved-object command restores all latest Core slots without replay")
    func timeoutRestoresLatestCoreAcrossMovedObjects() async throws {
        let store = store()
        let slices = try (0..<3).map { try #require(store.object("slice:\($0)")) }
        let client = CommandClient(clock: clock, send: sent.sender)
        await client.handle(.stateChanged(.ready))
        let queue = CommandHoldQueue(store: store)
        var outcomes: [CommandHoldQueue.Outcome] = []
        var lateDelivered = false
        queue.send("txSlice", shows: (0..<3).map { .init("slice:\($0)", "txSlice", .bool($0 == 1)) },
                   invokeWithLate: { late in
            try await client.invokeHeld("txSlice", arguments: [], timeout: .seconds(5), onLateOutcome: { answer in
                await MainActor.run { late(answer); lateDelivered = true }
            })
        }, onOutcome: { outcomes.append($0) })
        #expect(await sent.settle(untilCount: 1))
        guard case .commandInvoke(let invoke)? = sent.messages.last else { Issue.record("missing invoke"); return }
        for id in 0..<3 { store.apply(delta("slice:\(id)", "txSlice", 4, .bool(id == 2))) }
        await clock.advance(by: 4_999)
        #expect(slices.map { $0["txSlice"] } == [.bool(false), .bool(true), .bool(false)])
        await clock.advance(by: 1)
        #expect(await turns { !queue.isSending("txSlice") })
        #expect(outcomes == [.failure(.timedOut)])
        #expect(slices.map { $0["txSlice"] } == [.bool(false), .bool(false), .bool(true)])
        #expect((0..<3).allSatisfy { store.isUnconfirmed("slice:\($0)", property: "txSlice") })
        for id in 0..<3 { store.apply(delta("slice:\(id)", "txSlice", 4, .bool(id == 0))) }
        #expect(slices.map { $0["txSlice"] } == [.bool(true), .bool(false), .bool(false)])
        #expect(sent.count == 1)
        await client.receive(.commandResult(.init(verb: "txSlice", id: invoke.id, accepted: false,
                                                   reason: "Current TX slice refusal.", affected: [], values: nil)))
        #expect(await turns { lateDelivered })
        #expect(slices.map { $0["txSlice"] } == [.bool(true), .bool(false), .bool(false)])
        #expect((0..<3).allSatisfy { !store.isUnconfirmed("slice:\($0)", property: "txSlice") })
        #expect(try outcomes.last?.get().reason == "Current TX slice refusal.")
    }

    @Test("a slow answer: the value shows at the touch and stays through another device's delta; taken, the next change shows as it comes")
    func slowAnswerThenRemoteChange() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let core = FakeCore()
        let queue = CommandHoldQueue(store: store)
        antenna(queue, core, "ANT2")
        #expect(slice["txAntenna"] == .text("ANT2"))
        #expect(await turns { core.isWaiting })
        store.apply(delta("slice:0", "txAntenna", 3, .utf8("ANT3")))
        #expect(slice["txAntenna"] == .text("ANT2"))
        core.answer(accepted: true)
        #expect(await turns { !queue.isSending("txAntenna") })
        #expect(slice["txAntenna"] == .text("ANT2"))
        store.apply(delta("slice:0", "txAntenna", 3, .utf8("ANT2")))
        store.apply(delta("slice:0", "txAntenna", 3, .utf8("ANT3")))
        #expect(slice["txAntenna"] == .text("ANT3"))
    }

    @Test("a refusal shows the Core's value again and hands its words to the control")
    func refusalShowsTheCoresValue() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let core = FakeCore()
        let queue = CommandHoldQueue(store: store)
        var reasons: [String] = []
        antenna(queue, core, "ANT2") { outcome in
            if case .success(let result) = outcome {
                reasons.append(result.reason)
            }
        }
        #expect(await turns { core.isWaiting })
        core.answer(accepted: false, reason: "That antenna is not on this radio.")
        #expect(await turns { !reasons.isEmpty })
        #expect(reasons == ["That antenna is not on this radio."])
        #expect(slice["txAntenna"] == .text("ANT1"))
    }

    @Test("no answer in time: latest Core values show, marked not confirmed until the command answers")
    func droppedAnswerRestoresCore() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let core = FakeCore()
        let queue = CommandHoldQueue(store: store)
        antenna(queue, core, "ANT2")
        #expect(await turns { core.isWaiting })
        core.fail(.timedOut)
        #expect(await turns { !queue.isSending("txAntenna") })
        #expect(slice["txAntenna"] == .text("ANT1"))
        #expect(store.isUnconfirmed("slice:0", property: "txAntenna"))
        store.apply(delta("slice:0", "txAntenna", 3, .utf8("ANT1")))
        #expect(slice["txAntenna"] == .text("ANT1"))
        #expect(store.isUnconfirmed("slice:0", property: "txAntenna"))
        store.apply(delta("slice:0", "txAntenna", 3, .utf8("ANT2")))
        #expect(store.isUnconfirmed("slice:0", property: "txAntenna"))

        // Even a matching delta is not the missing command answer.
        antenna(queue, core, "ANT3")
        #expect(await turns { core.isWaiting })
        store.apply(delta("slice:0", "txAntenna", 3, .utf8("ANT3")))
        core.fail(.timedOut)
        #expect(await turns { !queue.isSending("txAntenna") })
        #expect(store.isUnconfirmed("slice:0", property: "txAntenna"))
        store.apply(delta("slice:0", "txAntenna", 3, .utf8("ANT2")))
        #expect(slice["txAntenna"] == .text("ANT2"))
        #expect(store.isUnconfirmed("slice:0", property: "txAntenna"))
    }

    @Test("touches while a command waits show at once; only the newest is sent next, and wins")
    func newestTouchWins() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let core = FakeCore()
        let queue = CommandHoldQueue(store: store)
        antenna(queue, core, "ANT2")
        #expect(await turns { core.isWaiting })
        antenna(queue, core, "ANT3")
        antenna(queue, core, "ANT1")
        #expect(slice["txAntenna"] == .text("ANT1"))
        // The older command's refusal leaves the newer touch shown.
        core.answer(accepted: false, reason: "No.")
        #expect(await turns { core.isWaiting })
        #expect(slice["txAntenna"] == .text("ANT1"))
        #expect(core.asked == ["ANT2", "ANT1"])
        core.answer(accepted: true)
        #expect(await turns { !queue.isSending("txAntenna") })
        #expect(slice["txAntenna"] == .text("ANT1"))
    }

    @Test("a lost link shows the Core's value; the touch waiting its turn is never sent")
    func lostLinkSendsNothingAgain() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let core = FakeCore()
        let queue = CommandHoldQueue(store: store)
        var outcomes: [CommandHoldQueue.Outcome] = []
        antenna(queue, core, "ANT2") { outcomes.append($0) }
        #expect(await turns { core.isWaiting })
        antenna(queue, core, "ANT3") { outcomes.append($0) }
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(slice["txAntenna"] == .text("ANT1"))
        core.fail(.linkLost)
        #expect(await turns { outcomes.count == 1 })
        #expect(outcomes == [.failure(.linkLost)])
        #expect(core.asked == ["ANT2"])
        #expect(slice["txAntenna"] == .text("ANT1"))
    }

    @Test("a deliberate touch after reconnect survives the old command's loss continuation")
    func freshReconnectTouchSurvivesOldLoss() async throws {
        let store = store()
        let core = FakeCore()
        let queue = CommandHoldQueue(store: store)
        var oldOutcomes: [CommandHoldQueue.Outcome] = []
        var newOutcomes: [CommandHoldQueue.Outcome] = []
        antenna(queue, core, "ANT2") { oldOutcomes.append($0) }
        #expect(await turns { core.isWaiting })
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        // All replacement snapshot events and the new touch occur before
        // resuming the old command, making the scheduling boundary exact.
        store.handle(.stateChanged(.receivingSnapshot))
        store.apply(FixtureReplay.accepted)
        store.apply(.objectCreate(.init(key: "slice:0", className: "SliceModel", properties: [
            .init(ordinal: 3, name: "txAntenna", value: .utf8("ANT1")),
        ])))
        store.apply(.snapshotComplete)
        store.handle(.stateChanged(.ready))
        let slice = try #require(store.object("slice:0"))
        antenna(queue, core, "ANT3") { newOutcomes.append($0) }
        #expect(slice["txAntenna"] == .text("ANT3"))
        core.fail(.linkLost)
        #expect(await turns { core.asked.count == 2 || !queue.isSending("txAntenna") })
        #expect(core.asked == ["ANT2", "ANT3"])
        #expect(slice["txAntenna"] == .text("ANT3"))
        #expect(oldOutcomes.isEmpty && newOutcomes.isEmpty)
        // Cleanup does not depend on a passing admission assertion.
        if core.isWaiting { core.answer(accepted: true) }
        #expect(await turns { !queue.isSending("txAntenna") })
        #expect(newOutcomes.count == 1)
        if let result = newOutcomes.first { #expect(try result.get().accepted) }
    }

    @Test("a command that moves a value between objects shows both ends; a newer touch lets go of the older one's ends")
    func valueMovesBetweenObjects() async throws {
        let store = store()
        let slices = try (0..<3).map { try #require(store.object("slice:\($0)")) }
        let core = FakeCore()
        let queue = CommandHoldQueue(store: store)
        func transmit(_ id: Int) {
            queue.send("txSlice", shows: [.init("slice:0", "txSlice", .bool(id == 0)),
                                          .init("slice:\(id)", "txSlice", .bool(true))],
                       invoke: { try await core.invoke("slice \(id)") }, onOutcome: { _ in })
        }
        transmit(1)
        #expect(slices.map { $0["txSlice"] } == [.bool(false), .bool(true), .bool(false)])
        #expect(await turns { core.isWaiting })
        transmit(2)
        #expect(slices.map { $0["txSlice"] } == [.bool(false), .bool(true), .bool(true)])
        core.answer(accepted: false, reason: "No.")
        #expect(await turns { core.asked.count == 2 })
        #expect(slices.map { $0["txSlice"] } == [.bool(false), .bool(false), .bool(true)])
        core.answer(accepted: false, reason: "No.")
        #expect(await turns { !queue.isSending("txSlice") })
        #expect(slices.map { $0["txSlice"] } == [.bool(true), .bool(false), .bool(false)])
    }
    @Test("held command late answers belong to the current touch after timeout", arguments: [false, true])
    func lateAnswerBelongsToTouch(newerCompleted: Bool) async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let client = CommandClient(clock: clock, send: sent.sender)
        await client.handle(.stateChanged(.ready))
        let queue = CommandHoldQueue(store: store)
        var outcomes: [CommandHoldQueue.Outcome] = []
        var lateDelivered = false
        func choose(_ label: String) {
            queue.send("antenna", shows: [.init("slice:0", "txAntenna", .text(label))], invokeWithLate: { late in
                try await client.invokeHeld("antenna", arguments: [], timeout: .seconds(5), onLateOutcome: { answer in
                    await MainActor.run { late(answer); lateDelivered = true }
                })
            }, onOutcome: { outcomes.append($0) })
        }
        func answer(_ id: UInt32, accepted: Bool, reason: String = "") async {
            await client.receive(.commandResult(.init(verb: "antenna", id: id, accepted: accepted, reason: reason,
                                                      affected: [], values: nil)))
        }
        choose("ANT2")
        #expect(await sent.settle(untilCount: 1))
        guard case .commandInvoke(let first)? = sent.messages.last else { Issue.record("missing invoke"); return }
        await clock.advance(by: 5_000)
        #expect(await turns { !queue.isSending("antenna") })
        #expect(outcomes == [.failure(.timedOut)])
        store.apply(delta("slice:0", "txAntenna", 3, .utf8("ANT1")))
        #expect(slice["txAntenna"] == .text("ANT1"))
        #expect(store.isUnconfirmed("slice:0", property: "txAntenna"))
        if newerCompleted {
            choose("ANT3")
            #expect(await sent.settle(untilCount: 2))
            guard case .commandInvoke(let second)? = sent.messages.last else { Issue.record("missing invoke"); return }
            await answer(second.id, accepted: true)
            #expect(await turns { !queue.isSending("antenna") })
            store.apply(delta("slice:0", "txAntenna", 3, .utf8("ANT3")))
        }
        let before = outcomes
        await answer(first.id, accepted: false, reason: "That antenna is unavailable.")
        #expect(await turns { lateDelivered })
        if newerCompleted {
            #expect(slice["txAntenna"] == .text("ANT3"))
            #expect(outcomes == before)
        } else {
            #expect(slice["txAntenna"] == .text("ANT1"))
            #expect(outcomes.last == .success(.init(accepted: false, reason: "That antenna is unavailable.",
                                                    affectedKeys: [], values: [:], phase: nil)))
        }
        #expect(!store.isUnconfirmed("slice:0", property: "txAntenna"))
        #expect(await client.waitingCount == 0)
    }

}
