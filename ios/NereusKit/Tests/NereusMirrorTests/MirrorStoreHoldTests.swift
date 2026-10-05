// NereusSDR for iOS: a write shows the operator's value at the touch and keeps it until the Core answers
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// JJ, 2026-10-01: the phone's controls act at the touch, as the desktop's
/// remote window does (`StationClient.cpp:1040-1068`, `handleDelta`
/// `:4174-4214`, `handlePropertyResult` `:4216-4268`, the lost link
/// `:2219`). The Core here is a recording sender; its answers come only when
/// a test delivers them, and time moves only on a manual clock.
@MainActor
@Suite struct MirrorStoreHoldTests {
    private let sent = SentMessages()
    private let clock = ManualLinkClock()

    /// A store holding `slice:0` with `afGain` 50 and `anfEnabled` off, on a
    /// link that is up; at minor 11 with property results, or at minor 4,
    /// where the next delta answers a write.
    private func store(results: Bool = true) -> MirrorStore {
        let store = MirrorStore(send: sent.sender, clock: clock)
        store.handle(.stateChanged(.receivingSnapshot))
        store.apply(FixtureReplay.stationHello(minor: results ? 11 : 4))
        store.apply(FixtureReplay.accepted)
        store.apply(FixtureReplay.capabilities(["propertyResultVersion": .i64(1)]))
        store.apply(.schema(LinkMessage.Schema(className: "SliceModel", fields: [
            LinkMessage.SchemaField(ordinal: 7, name: "afGain", kind: .i64),
            LinkMessage.SchemaField(ordinal: 9, name: "anfEnabled", kind: .bool),
            LinkMessage.SchemaField(ordinal: 1, name: "frequency", kind: .f64),
        ])))
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel", properties: [
            .init(ordinal: 7, name: "afGain", value: .i64(50)),
            .init(ordinal: 9, name: "anfEnabled", value: .bool(false)),
            .init(ordinal: 1, name: "frequency", value: .f64(7_074_000)),
        ])))
        store.apply(.snapshotComplete)
        store.handle(.stateChanged(.ready))
        return store
    }

    /// Starts a write and waits for it to leave.
    private func startWrite(_ store: MirrorStore, _ property: String, _ value: MirrorValue)
        async throws -> (Task<PropertyWriteOutcome, Never>, LinkMessage.PropertyWrite) {
        let before = sent.count
        let task = Task { await store.write("slice:0", property: property, value: value) }
        #expect(await sent.settle(untilCount: before + 1))
        guard case .propertyWrite(let write)? = sent.messages.last else {
            Issue.record("no property.write was sent")
            throw CancellationError()
        }
        return (task, write)
    }

    private func result(_ write: LinkMessage.PropertyWrite, accepted: Bool, reason: String = "",
                        kept: LinkMessage.PropertyValue?) -> LinkMessage {
        let entry = write.properties[0]
        return .propertyResult(LinkMessage.PropertyResult(key: write.key, writeId: write.writeId ?? 0, results: [
            .init(property: entry.name, accepted: accepted, reason: reason,
                  value: kept.map { LinkMessage.PropertyEntry(ordinal: entry.ordinal, name: entry.name, value: $0) }),
        ]))
    }

    private func delta(_ name: String, _ ordinal: UInt16, _ value: LinkMessage.PropertyValue) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "slice:0", properties: [.init(ordinal: ordinal, name: name, value: value)]))
    }

    @Test("five seconds without confirmation restores the latest Core scalar or compound value", arguments: [false, true])
    func timeoutRestoresLatestCore(compound: Bool) async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let property = compound ? "txEqBandsJson" : "afGain"
        let ordinal: UInt16 = compound ? 20 : 7
        let wanted: MirrorValue = compound ? .text("[2,3,4]") : .int(85)
        let core: LinkMessage.PropertyValue = compound ? .utf8("[0,1,0]") : .i64(60)
        let laterCore: LinkMessage.PropertyValue = compound ? .utf8("[1,0,1]") : .i64(65)
        if compound {
            store.apply(.schema(.init(className: "SliceModel", fields: [
                .init(ordinal: ordinal, name: property, kind: .utf8),
            ])))
            store.apply(delta(property, ordinal, .utf8("[0,0,0]")))
        }
        let (task, write) = try await startWrite(store, property, wanted)
        store.apply(delta(property, ordinal, core))
        await clock.advance(by: 4_999)
        #expect(slice[property] == wanted)
        #expect(!store.isUnconfirmed("slice:0", property: property))
        await clock.advance(by: 1)
        #expect(await task.value == .notConfirmed)
        #expect(slice[property] == MirrorValue(core))
        #expect(store.isUnconfirmed("slice:0", property: property))
        store.apply(delta(property, ordinal, laterCore))
        #expect(slice[property] == MirrorValue(laterCore))
        #expect(store.isUnconfirmed("slice:0", property: property))
        #expect(sent.count == 1, "an expired value never replays")
        store.apply(result(write, accepted: false, reason: "Current refusal.", kept: laterCore))
        #expect(slice[property] == MirrorValue(laterCore))
        #expect(!store.isUnconfirmed("slice:0", property: property))
    }

    @Test("an older deadline and late answer cannot restore over a newer pending or completed touch", arguments: [false, true])
    func timeoutCannotRestoreOverNewerTouch(completed: Bool) async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        var late: [MirrorStore.LateAnswer] = []
        let watch = store.lateAnswers.sink { late.append($0) }
        defer { watch.cancel() }
        let (first, firstWrite) = try await startWrite(store, "afGain", .int(80))
        await clock.advance(by: 4_000)
        let (second, secondWrite) = try await startWrite(store, "afGain", .int(90))
        store.apply(delta("afGain", 7, .i64(65)))
        if completed {
            store.apply(result(secondWrite, accepted: true, kept: .i64(90)))
            #expect(await second.value.accepted)
        }
        await clock.advance(by: 1_000)
        #expect(!(await first.value).isCurrent)
        #expect(slice["afGain"] == .int(90))
        #expect(!store.isUnconfirmed("slice:0", property: "afGain"))
        store.apply(result(firstWrite, accepted: false, reason: "Obsolete refusal.", kept: .i64(50)))
        #expect(slice["afGain"] == .int(90) && late.isEmpty)
        if !completed {
            await clock.advance(by: 4_000)
            #expect(await second.value == .notConfirmed)
            #expect(slice["afGain"] == .int(65))
            #expect(store.isUnconfirmed("slice:0", property: "afGain"))
        }
        #expect(sent.count == 2)
    }

    @Test("a write shows the operator's value at the touch and keeps it through a slow answer and a delta")
    func heldThroughASlowAnswerAndADelta() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let (task, write) = try await startWrite(store, "anfEnabled", .bool(true))
        // At the touch, before any answer.
        #expect(slice["anfEnabled"] == .bool(true))
        // A delta of the same property while the write waits does not put the old value back.
        store.apply(delta("anfEnabled", 9, .bool(false)))
        #expect(slice["anfEnabled"] == .bool(true))
        // An unrelated property's delta applies as it comes.
        store.apply(delta("afGain", 7, .i64(30)))
        #expect(slice["afGain"] == .int(30))
        #expect(slice["anfEnabled"] == .bool(true))
        // Four seconds on, still the operator's, with no answer.
        await clock.advance(by: 4_000)
        #expect(slice["anfEnabled"] == .bool(true))
        #expect(!store.isUnconfirmed("slice:0", property: "anfEnabled"))
        store.apply(result(write, accepted: true, kept: .bool(true)))
        #expect(await task.value.accepted)
        #expect(slice["anfEnabled"] == .bool(true))
        // A change from another device after the answer shows as it comes.
        store.apply(delta("anfEnabled", 9, .bool(false)))
        #expect(slice["anfEnabled"] == .bool(false))
    }

    @Test("a refusal puts the Core's value back, with its words")
    func refusalReturnsToTheCore() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let (task, write) = try await startWrite(store, "afGain", .int(80))
        #expect(slice["afGain"] == .int(80))
        store.apply(result(write, accepted: false, reason: "That slice belongs to MacBook Pro.", kept: .i64(50)))
        let outcome = await task.value
        #expect(outcome == PropertyWriteOutcome(accepted: false, reason: "That slice belongs to MacBook Pro.",
                                                value: .int(50)))
        #expect(slice["afGain"] == .int(50))
    }

    @Test("a refusal without a value returns to the Core's last value, one that came while the write waited")
    func refusalWithoutAValueReturnsToTheLastCoreValue() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let (task, write) = try await startWrite(store, "afGain", .int(80))
        store.apply(delta("afGain", 7, .i64(42)))
        #expect(slice["afGain"] == .int(80))
        store.apply(result(write, accepted: false, reason: "No.", kept: nil))
        #expect(await task.value.reason == "No.")
        #expect(slice["afGain"] == .int(42))
    }

    @Test("a newer touch replaces an older one; the older one's refusal leaves the newer shown")
    func newestWins() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let (first, firstWrite) = try await startWrite(store, "afGain", .int(60))
        // A newer value waiting its turn shows at once.
        store.hold("slice:0", property: "afGain", value: .int(70))
        #expect(slice["afGain"] == .int(70))
        store.apply(result(firstWrite, accepted: false, reason: "No.", kept: .i64(50)))
        #expect(await first.value.accepted == false)
        #expect(slice["afGain"] == .int(70))
        let (second, secondWrite) = try await startWrite(store, "afGain", .int(70))
        #expect(slice["afGain"] == .int(70))
        store.apply(result(secondWrite, accepted: true, kept: .i64(70)))
        #expect(await second.value.accepted)
        #expect(slice["afGain"] == .int(70))
    }

    @Test("a direct newer completed write survives an older timed-out result", arguments: [true, false])
    func completedEditSurvivesOlderResult(accepted: Bool) async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        var late: [MirrorStore.LateAnswer] = []
        let watch = store.lateAnswers.sink { late.append($0) }
        defer { watch.cancel() }
        let (first, firstWrite) = try await startWrite(store, "afGain", .int(60))
        await clock.advance(by: 5_000)
        #expect(await first.value == .notConfirmed)
        let (second, secondWrite) = try await startWrite(store, "afGain", .int(70))
        store.apply(result(secondWrite, accepted: true, kept: .i64(70)))
        #expect(await second.value.accepted)
        store.apply(result(firstWrite, accepted: accepted, reason: accepted ? "" : "Older refusal.",
                           kept: .i64(accepted ? 60 : 50)))
        #expect(slice["afGain"] == .int(70))
        #expect(late.isEmpty)
    }

    @Test("an answer that never comes restores the latest Core value, marked not confirmed, retaining its late owner")
    func droppedAnswerIsMarkedNotConfirmed() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        var late: [MirrorStore.LateAnswer] = []
        let watch = store.lateAnswers.sink { late.append($0) }
        defer { watch.cancel() }
        let (task, write) = try await startWrite(store, "afGain", .int(80))
        await clock.advance(by: 4_999)
        #expect(!store.isUnconfirmed("slice:0", property: "afGain"))
        await clock.advance(by: 1)
        let outcome = await task.value
        #expect(outcome == .notConfirmed)
        #expect(!outcome.answeredByCore)
        #expect(slice["afGain"] == .int(50))
        #expect(store.isUnconfirmed("slice:0", property: "afGain"))
        // Much later, still the authoritative value.
        await clock.advance(by: 60_000)
        #expect(slice["afGain"] == .int(50))
        // The answer arrives late: the mark goes and the outcome is handed on.
        store.apply(result(write, accepted: false, reason: "No.", kept: .i64(50)))
        #expect(slice["afGain"] == .int(50))
        #expect(!store.isUnconfirmed("slice:0", property: "afGain"))
        #expect(late == [MirrorStore.LateAnswer(key: "slice:0", property: "afGain",
                                                outcome: PropertyWriteOutcome(accepted: false, reason: "No.",
                                                                              value: .int(50)))])
    }

    @Test("a lost link shows the Core's value and says the change was not confirmed; nothing is sent again")
    func linkDropShowsTheCoresValue() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let (timedOut, _) = try await startWrite(store, "anfEnabled", .bool(true))
        await clock.advance(by: 5_000)
        #expect(await timedOut.value == .notConfirmed)
        let (task, _) = try await startWrite(store, "afGain", .int(80))
        var late: [MirrorStore.LateAnswer] = []
        let watch = store.lateAnswers.sink { late.append($0) }
        defer { watch.cancel() }
        let before = sent.count
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        let outcome = await task.value
        #expect(outcome == .linkLost)
        #expect(outcome.reason == "The connection to the Core dropped before it confirmed this change.")
        #expect(slice["afGain"] == .int(50))
        #expect(slice["anfEnabled"] == .bool(false))
        #expect(!store.isUnconfirmed("slice:0", property: "anfEnabled"))
        #expect(late.map(\.property) == ["anfEnabled"])
        #expect(late.first?.outcome == .linkLost)
        await clock.advance(by: 10_000)
        #expect(sent.count == before)
    }

    @Test("on a Core without results the next delta answers")
    func olderCoreDeltaAnswers() async throws {
        let store = store(results: false)
        let slice = try #require(store.object("slice:0"))
        let (task, _) = try await startWrite(store, "afGain", .int(80))
        #expect(slice["afGain"] == .int(80))
        store.apply(delta("afGain", 7, .i64(75)))
        #expect(await task.value == .keptOther(.int(75)))
        #expect(slice["afGain"] == .int(75))
    }

    @Test("a write that keeps no hold leaves the shown value to the Core, as the keying path does")
    func unheldWriteWaitsForTheCore() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let before = sent.count
        let task = Task {
            await store.write("slice:0", property: "anfEnabled", value: .bool(true), holdsOperatorValue: false)
        }
        #expect(await sent.settle(untilCount: before + 1))
        #expect(slice["anfEnabled"] == .bool(false))
        guard case .propertyWrite(let write)? = sent.messages.last else {
            Issue.record("no write")
            return
        }
        store.apply(result(write, accepted: true, kept: .bool(true)))
        #expect(await task.value.accepted)
        #expect(slice["anfEnabled"] == .bool(true))
    }

    @Test("an object the Core sends again keeps the operator's value over it")
    func objectCreateKeepsTheHold() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let (task, write) = try await startWrite(store, "afGain", .int(80))
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel", properties: [
            .init(ordinal: 7, name: "afGain", value: .i64(40)),
            .init(ordinal: 9, name: "anfEnabled", value: .bool(true)),
        ])))
        #expect(slice["afGain"] == .int(80))
        #expect(slice["anfEnabled"] == .bool(true))
        store.apply(result(write, accepted: false, reason: "No.", kept: nil))
        _ = await task.value
        #expect(slice["afGain"] == .int(40))
    }

    @Test("a value a command sends shows at the touch: refused, the Core's returns; taken, it stays until the Core's next value")
    func commandHold() throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        store.hold("slice:0", property: "afGain", value: .int(80))
        #expect(slice["afGain"] == .int(80))
        // A delta while the command waits does not undo it.
        store.apply(delta("afGain", 7, .i64(55)))
        #expect(slice["afGain"] == .int(80))
        store.commandAnswered("slice:0", property: "afGain", accepted: false)
        #expect(slice["afGain"] == .int(55))

        store.hold("slice:0", property: "afGain", value: .int(70))
        store.commandAnswered("slice:0", property: "afGain", accepted: true)
        #expect(slice["afGain"] == .int(70))
        store.apply(delta("afGain", 7, .i64(70)))
        #expect(slice["afGain"] == .int(70))
        // Held no longer: another device's change shows as it comes.
        store.apply(delta("afGain", 7, .i64(30)))
        #expect(slice["afGain"] == .int(30))
    }
}
