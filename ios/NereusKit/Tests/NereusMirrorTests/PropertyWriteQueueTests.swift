// NereusSDR for iOS: a control's writes go one at a time per property and the newest value waits its turn
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// JJ, 2026-10-01: a slider's values show at the touch while one write at a
/// time goes to the Core, the newest value next (`StationClient.cpp:1040-1068`).
@MainActor
@Suite struct PropertyWriteQueueTests {
    private let sent = SentMessages()
    private let clock = ManualLinkClock()

    private func store() -> MirrorStore {
        let store = MirrorStore(send: sent.sender, clock: clock)
        store.handle(.stateChanged(.receivingSnapshot))
        store.apply(FixtureReplay.stationHello(minor: 11))
        store.apply(FixtureReplay.accepted)
        store.apply(FixtureReplay.capabilities(["propertyResultVersion": .i64(1)]))
        store.apply(.schema(LinkMessage.Schema(className: "SliceModel", fields: [
            LinkMessage.SchemaField(ordinal: 7, name: "afGain", kind: .i64),
        ])))
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel", properties: [
            .init(ordinal: 7, name: "afGain", value: .i64(50)),
        ])))
        store.apply(.snapshotComplete)
        store.handle(.stateChanged(.ready))
        return store
    }

    private func lastWrite() throws -> LinkMessage.PropertyWrite {
        guard case .propertyWrite(let write)? = sent.messages.last else {
            Issue.record("no property.write was sent")
            throw CancellationError()
        }
        return write
    }

    private func accept(_ store: MirrorStore, _ write: LinkMessage.PropertyWrite) {
        let entry = write.properties[0]
        store.apply(.propertyResult(LinkMessage.PropertyResult(key: write.key, writeId: write.writeId ?? 0, results: [
            .init(property: entry.name, accepted: true, reason: "",
                  value: LinkMessage.PropertyEntry(ordinal: entry.ordinal, name: entry.name, value: entry.value)),
        ])))
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

    @Test("a captured bound touch revoked before queued admission sends nothing and restores the Core value")
    func boundTouchRevokedBeforeAdmission() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let queue = PropertyWriteQueue(store: store) { _, _ in }
        let permit = CommandSendPermit()
        let before = sent.count
        let heldEdit = queue.writeBound("slice:0", "afGain", .int(70),
            sender: { [sent] message, authority in
                guard !authority.isRevoked else { throw LinkSendError.notConnected }
                try await sent.sender(message)
            }, authority: permit, stillAllowed: { !permit.isRevoked })
        let edit = try #require(heldEdit)
        #expect(slice["afGain"] == .int(70))
        permit.revoke()
        store.retireBoundEdit("slice:0", property: "afGain", edit: edit)
        #expect(slice["afGain"] == .int(50))
        #expect(await turns { !queue.isWriting("slice:0", "afGain") })
        #expect(sent.count == before)
        #expect(!store.isCurrent("slice:0", property: "afGain", edit: edit))
    }

    @Test("an authority-retired bound edit cannot overwrite the new Core value with a late result")
    func boundLateResultAfterRetirement() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        var outcomes: [PropertyWriteOutcome] = []
        let queue = PropertyWriteQueue(store: store) { _, value in outcomes.append(value) }
        let permit = CommandSendPermit()
        let before = sent.count
        let heldEdit = queue.writeBound("slice:0", "afGain", .int(70),
            sender: { [sent] message, authority in
                guard !authority.isRevoked else { throw LinkSendError.notConnected }
                try await sent.sender(message)
            }, authority: permit, stillAllowed: { !permit.isRevoked })
        let edit = try #require(heldEdit)
        #expect(await sent.settle(untilCount: before + 1))
        let old = try lastWrite()
        permit.revoke()
        store.retireBoundEdit("slice:0", property: "afGain", edit: edit)
        store.apply(.delta(.init(key: "slice:0", properties: [.init(ordinal: 7, name: "afGain", value: .i64(55))])))
        accept(store, old)
        #expect(await turns { !queue.isWriting("slice:0", "afGain") })
        #expect(slice["afGain"] == .int(55) && outcomes.isEmpty)
    }

    @Test("the ordinary five-second bound-write deadline revokes a held physical sender and retains not-confirmed wording")
    func boundDeadlineRevokesHeldSender() async throws {
        let store = store()
        let queue = PropertyWriteQueue(store: store) { _, _ in }
        let gate = BoundHandoffGate()
        let permit = CommandSendPermit()
        let before = sent.count
        queue.writeBound("slice:0", "afGain", .int(70), sender: { [sent] message, authority in
            await gate.hold()
            guard !authority.isRevoked else { throw LinkSendError.notConnected }
            try await sent.sender(message)
        }, authority: permit, stillAllowed: { !permit.isRevoked })
        var entered = false
        for _ in 0..<2_000 { if await gate.entered { entered = true; break }; await Task.yield() }
        #expect(entered)
        await clock.advance(by: 4_999)
        #expect(!permit.isRevoked)
        await clock.advance(by: 1)
        #expect(permit.isRevoked)
        #expect(await turns { !queue.isWriting("slice:0", "afGain") })
        #expect(store.object("slice:0")?["afGain"] == .int(50))
        #expect(store.isUnconfirmed("slice:0", property: "afGain"))
        await gate.release()
        #expect(sent.count == before)
    }

    private actor BoundHandoffGate {
        private var waiting: CheckedContinuation<Void, Never>?
        private(set) var entered = false
        func hold() async { entered = true; await withCheckedContinuation { waiting = $0 } }
        func release() { waiting?.resume(); waiting = nil }
    }

    @Test("values made while a write waits show at once; only the newest is sent next")
    func newestWaitingValueIsSentNext() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        var outcomes: [PropertyWriteOutcome] = []
        let queue = PropertyWriteQueue(store: store) { _, outcome in outcomes.append(outcome) }
        let before = sent.count
        queue.write("slice:0", "afGain", .int(60))
        #expect(slice["afGain"] == .int(60))
        #expect(await sent.settle(untilCount: before + 1))
        let first = try lastWrite()
        queue.write("slice:0", "afGain", .int(65))
        queue.write("slice:0", "afGain", .int(70))
        #expect(slice["afGain"] == .int(70))
        accept(store, first)
        // The Core kept 60, but 70 waits its turn: 70 stays shown.
        #expect(await sent.settle(untilCount: before + 2))
        #expect(slice["afGain"] == .int(70))
        let second = try lastWrite()
        #expect(second.properties[0].value == .i64(70))
        accept(store, second)
        #expect(await turns { !queue.isWriting("slice:0", "afGain") })
        // Only the newest control's outcome may affect its current note.
        #expect(outcomes == [.init(accepted: true, reason: "", value: .int(70))])
        #expect(sent.count == before + 2)
        #expect(!queue.isWriting("slice:0", "afGain"))
        #expect(slice["afGain"] == .int(70))
    }

    @Test("two touches before writer task admission keep the newest shown")
    func newestBeforeTaskAdmission() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        let queue = PropertyWriteQueue(store: store) { _, _ in }
        let before = sent.count
        queue.write("slice:0", "afGain", .int(60))
        queue.write("slice:0", "afGain", .int(70))
        #expect(slice["afGain"] == .int(70))
        #expect(await sent.settle(untilCount: before + 1))
        #expect(slice["afGain"] == .int(70))
        let first = try lastWrite()
        accept(store, first)
        #expect(await sent.settle(untilCount: before + 2))
        #expect(slice["afGain"] == .int(70))
        accept(store, try lastWrite())
        #expect(await turns { !queue.isWriting("slice:0", "afGain") })
        #expect(slice["afGain"] == .int(70))
    }

    @Test("a superseded timeout's late acceptance or refusal cannot replace a newer completed value or note", arguments: [true, false])
    func lateSupersededResultIsRetired(accepted: Bool) async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        var outcomes: [PropertyWriteOutcome] = []
        let queue = PropertyWriteQueue(store: store) { _, outcome in outcomes.append(outcome) }
        let before = sent.count
        queue.write("slice:0", "afGain", .int(60))
        #expect(await sent.settle(untilCount: before + 1))
        let first = try lastWrite()
        await clock.advance(by: 5_000)
        #expect(await turns { !queue.isWriting("slice:0", "afGain") })
        queue.write("slice:0", "afGain", .int(70))
        #expect(await sent.settle(untilCount: before + 2))
        accept(store, try lastWrite())
        #expect(await turns { !queue.isWriting("slice:0", "afGain") })
        let beforeLate = outcomes
        let entry = first.properties[0]
        store.apply(.propertyResult(LinkMessage.PropertyResult(key: first.key, writeId: first.writeId ?? 0, results: [
            .init(property: entry.name, accepted: accepted, reason: accepted ? "" : "Older refusal.",
                  value: .init(ordinal: entry.ordinal, name: entry.name, value: .i64(accepted ? 60 : 50))),
        ])))
        #expect(slice["afGain"] == .int(70))
        #expect(outcomes == beforeLate)
        #expect(!store.isUnconfirmed("slice:0", property: "afGain"))
    }

    @Test("a lost link drops the value waiting its turn; the Core's value shows and nothing is sent again")
    func lostLinkDropsTheWaitingValue() async throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        var outcomes: [PropertyWriteOutcome] = []
        let queue = PropertyWriteQueue(store: store) { _, outcome in outcomes.append(outcome) }
        let before = sent.count
        queue.write("slice:0", "afGain", .int(60))
        #expect(await sent.settle(untilCount: before + 1))
        queue.write("slice:0", "afGain", .int(70))
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(await turns { !queue.isWriting("slice:0", "afGain") })
        #expect(outcomes == [.linkLost])
        #expect(slice["afGain"] == .int(50))
        #expect(sent.count == before + 1)
        #expect(!queue.isWriting("slice:0", "afGain"))
    }

    @Test("an answer that comes after the write was marked not confirmed reaches the control")
    func lateAnswerReachesTheOwner() async throws {
        let store = store()
        var outcomes: [PropertyWriteOutcome] = []
        let queue = PropertyWriteQueue(store: store) { _, outcome in outcomes.append(outcome) }
        let before = sent.count
        queue.write("slice:0", "afGain", .int(60))
        #expect(await sent.settle(untilCount: before + 1))
        let write = try lastWrite()
        await clock.advance(by: 5_000)
        #expect(await turns { !outcomes.isEmpty })
        #expect(outcomes == [.notConfirmed])
        let entry = write.properties[0]
        store.apply(.propertyResult(LinkMessage.PropertyResult(key: write.key, writeId: write.writeId ?? 0, results: [
            .init(property: "afGain", accepted: false, reason: "No.",
                  value: LinkMessage.PropertyEntry(ordinal: entry.ordinal, name: "afGain", value: .i64(50))),
        ])))
        #expect(outcomes.last == PropertyWriteOutcome(accepted: false, reason: "No.", value: .int(50)))
        #expect(try #require(store.object("slice:0"))["afGain"] == .int(50))
    }

    @Test("a shown value that will not be sent lets go: the Core's value shows again")
    func releaseUnsentShowsTheCoresValue() throws {
        let store = store()
        let slice = try #require(store.object("slice:0"))
        store.hold("slice:0", property: "afGain", value: .int(90))
        #expect(slice["afGain"] == .int(90))
        store.releaseUnsent("slice:0", property: "afGain")
        #expect(slice["afGain"] == .int(50))
    }
    @Test("a current lost-link hint survives retirement; an old loss cannot replace a newer touch", arguments: [false, true])
    func lossDeliveryOwnsItsTouch(newer: Bool) async throws {
        let store = store()
        var outcomes: [PropertyWriteOutcome] = []
        let queue = PropertyWriteQueue(store: store) { _, outcome in outcomes.append(outcome) }
        queue.write("slice:0", "afGain", .int(60))
        #expect(await sent.settle(untilCount: 1))
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        if newer {
            store.handle(.stateChanged(.ready))
            queue.write("slice:0", "afGain", .int(70))
        }
        #expect(await turns { !queue.isWriting("slice:0", "afGain") || sent.count == 2 })
        if newer {
            #expect(outcomes.isEmpty)
            #expect(await sent.settle(untilCount: 2))
            accept(store, try lastWrite())
            #expect(await turns { !queue.isWriting("slice:0", "afGain") })
            #expect(outcomes == [.init(accepted: true, reason: "", value: .int(70))])
            #expect(try #require(store.object("slice:0"))["afGain"] == .int(70))
        } else {
            #expect(outcomes == [.linkLost])
            #expect(sent.count == 1)
        }
    }

}
