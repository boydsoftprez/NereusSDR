// NereusSDR for iOS: the band's slices come from the mirror, and its touches reach the Core as writes and commands
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// R-IOS-11, R-IOS-12: the slices a band shows and the Core's writes its
/// touches make. The Core here is a recording send closure.
@Suite("BandSlicesModel")
@MainActor
struct BandSlicesModelTests {
    /// What the app sent, in order.
    final class Outbox: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: [LinkMessage] = []

        var messages: [LinkMessage] {
            lock.lock()
            defer { lock.unlock() }
            return stored
        }

        func record(_ message: LinkMessage) {
            lock.lock()
            stored.append(message)
            lock.unlock()
        }

        /// The frequencies written, in order.
        var frequencies: [Double] {
            messages.compactMap { message in
                guard case .propertyWrite(let write) = message, write.properties.first?.name == "frequency",
                      case .f64(let hz)? = write.properties.first?.value else {
                    return nil
                }
                return hz
            }
        }
    }

    final class Clock: @unchecked Sendable {
        var now: TimeInterval = 0
    }

    private func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<50_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    private func slice(_ index: Int64, hz: Double, active: Bool, locked: Bool = false) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: "slice:\(index)", className: "SliceModel", properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(hz)),
            LinkMessage.PropertyEntry(ordinal: 3, name: "filterLow", value: .i64(-3000)),
            LinkMessage.PropertyEntry(ordinal: 4, name: "filterHigh", value: .i64(-100)),
            LinkMessage.PropertyEntry(ordinal: 11, name: "active", value: .bool(active)),
            LinkMessage.PropertyEntry(ordinal: 12, name: "txSlice", value: .bool(false)),
            LinkMessage.PropertyEntry(ordinal: 13, name: "sliceIndex", value: .i64(index)),
            LinkMessage.PropertyEntry(ordinal: 35, name: "locked", value: .bool(locked)),
        ]))
    }

    private func makeModel(locked: Bool = false) -> (BandSlicesModel, MirrorStore, Outbox, Clock, CommandClient) {
        let outbox = Outbox()
        let store = MirrorStore(send: { message in outbox.record(message) })
        store.apply(slice(0, hz: 7_236_400, active: true, locked: locked))
        store.apply(slice(1, hz: 7_249_000, active: false))
        let commands = CommandClient(send: { message in outbox.record(message) })
        let clock = Clock()
        let model = BandSlicesModel(store: store, commands: commands, now: { clock.now })
        return (model, store, outbox, clock, commands)
    }

    /// Answers the frequency write last sent.
    private func answer(_ store: MirrorStore, _ outbox: Outbox, slice: Int = 0) {
        for hz in outbox.frequencies.suffix(1) {
            store.apply(.delta(LinkMessage.Delta(key: "slice:\(slice)", properties: [
                LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(hz)),
            ])))
        }
    }

    @Test("the band shows each slice the Core mirrors, lettered, with the active one marked")
    func readsTheSlices() async {
        let (model, _, _, _, _) = makeModel()
        #expect(await settle { model.entries.count == 2 })
        #expect(model.entries.map(\.slice.letter) == ["A", "B"])
        #expect(model.entries.map(\.slice.frequencyHz) == [7_236_400, 7_249_000])
        #expect(model.activeSliceId == 0)
        #expect(model.entries[0].slice.bandwidthText == "2.9K")
        // No catalogue yet: no colour is assumed.
        #expect(model.entries.allSatisfy { $0.slice.colour == BandSlice.colourUnknown })
    }

    @Test("a drag writes the active slice's frequency at most every 50 ms, then its final value")
    func dragIsPaced() async {
        let (model, store, outbox, clock, _) = makeModel()
        #expect(await settle { model.active != nil })
        model.drag(to: 7_237_000)
        clock.now = 0.01
        model.drag(to: 7_237_100)
        clock.now = 0.02
        model.drag(to: 7_237_200)
        // The finger's frequency shows at once, before the Core answers.
        #expect(model.active?.slice.frequencyHz == 7_237_200)
        clock.now = 0.03
        model.finishDrag()
        // One write in flight at a time: the final value follows the Core's answer.
        #expect(await settle { outbox.frequencies.count == 1 })
        answer(store, outbox)
        #expect(await settle { outbox.frequencies.count == 2 })
        #expect(outbox.frequencies == [7_237_000, 7_237_200])
        #expect(outbox.messages.allSatisfy { message in
            guard case .propertyWrite(let write) = message else {
                return true
            }
            return write.key == "slice:0"
        })
        answer(store, outbox)
        #expect(await settle { model.active?.slice.frequencyHz == 7_237_200 })
    }

    @Test("a tap writes one frequency; a locked slice is not tuned")
    func tapWritesOnce() async {
        let (model, store, outbox, _, _) = makeModel()
        #expect(await settle { model.active != nil })
        model.tap(to: 7_240_000)
        #expect(await settle { outbox.frequencies == [7_240_000] })
        answer(store, outbox)

        let (lockedModel, _, lockedOutbox, _, _) = makeModel(locked: true)
        #expect(await settle { lockedModel.active?.locked == true })
        lockedModel.tap(to: 7_240_000)
        lockedModel.drag(to: 7_240_000)
        lockedModel.finishDrag()
        for _ in 0..<100 {
            await Task.yield()
        }
        #expect(lockedOutbox.frequencies.isEmpty)
    }

    @Test("a tap on another slice's flag asks the Core to make it active")
    func activatesThroughTheCore() async {
        let (model, _, outbox, _, commands) = makeModel()
        #expect(await settle { model.entries.count == 2 })
        await commands.handle(.stateChanged(.ready))
        model.activate(0)
        model.activate(1)
        #expect(await settle {
            outbox.messages.contains { message in
                guard case .commandInvoke(let invoke) = message else {
                    return false
                }
                return invoke.verb == "setActiveSliceById" && invoke.args.first?.name == "sliceId"
                    && invoke.args.first?.value == .i64(1)
            }
        })
        // The already active slice asks nothing.
        #expect(outbox.messages.filter { if case .commandInvoke = $0 { return true } else { return false } }.count == 1)
        // Nothing changes until the Core says so.
        #expect(model.activeSliceId == 0)
    }

    // MARK: The Core's refusals

    /// A mirror at minor 11 with property results, so each write has the
    /// Core's own answer, and a command client that is ready.
    private func makeAnsweredModel(linkClock: any LinkClock = SystemLinkClock()) async -> (BandSlicesModel, MirrorStore, Outbox, Clock, CommandClient) {
        let outbox = Outbox()
        let store = MirrorStore(send: { message in outbox.record(message) }, clock: linkClock)
        store.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        store.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "propertyResultVersion", value: .i64(1)),
            .init(name: "remoteCtunVersion", value: .i64(1)),
        ])))
        store.apply(.authResult(LinkMessage.AuthResult(accepted: true, reason: "", retryable: false)))
        store.apply(slice(0, hz: 7_236_400, active: true))
        store.apply(slice(1, hz: 7_249_000, active: false))
        store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 2, name: "dspMode", value: .i64(0)),
            .init(ordinal: 6, name: "stepHz", value: .i64(100)),
            .init(ordinal: 24, name: "streamCtunPinned", value: .bool(false)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
        ])))
        store.apply(.snapshotComplete)
        let commands = CommandClient(send: { message in outbox.record(message) })
        await commands.handle(.stateChanged(.ready))
        let clock = Clock()
        let model = BandSlicesModel(store: store, commands: commands, now: { clock.now })
        return (model, store, outbox, clock, commands)
    }

    /// The last property write sent for `property`.
    private func lastWrite(_ outbox: Outbox, _ property: String) -> LinkMessage.PropertyWrite? {
        outbox.messages.reversed().compactMap { message -> LinkMessage.PropertyWrite? in
            if case .propertyWrite(let write) = message, write.properties.first?.name == property {
                return write
            }
            return nil
        }.first
    }

    /// The Core answers the last write of `property` with `accepted` and `reason`.
    private func answerWrite(_ store: MirrorStore, _ outbox: Outbox, _ property: String, accepted: Bool,
                             reason: String) async -> Bool {
        guard await settle({ lastWrite(outbox, property) != nil }), let write = lastWrite(outbox, property),
              let writeId = write.writeId, let entry = write.properties.first else {
            return false
        }
        store.apply(.propertyResult(LinkMessage.PropertyResult(key: write.key, writeId: writeId, results: [
            .init(property: property, accepted: accepted, reason: reason, value: accepted ? entry : nil),
        ])))
        if accepted {
            store.apply(.delta(LinkMessage.Delta(key: write.key, properties: [entry])))
        }
        return true
    }

    /// The Core answers the last `verb` it was asked with `accepted` and `reason`.
    private func answerCommand(_ commands: CommandClient, _ outbox: Outbox, _ verb: String, accepted: Bool,
                               reason: String) async -> Bool {
        func last() -> LinkMessage.CommandInvoke? {
            outbox.messages.reversed().compactMap { message -> LinkMessage.CommandInvoke? in
                if case .commandInvoke(let invoke) = message, invoke.verb == verb {
                    return invoke
                }
                return nil
            }.first
        }
        guard await settle({ last() != nil }), let invoke = last() else {
            return false
        }
        await commands.receive(.commandResult(LinkMessage.CommandResult(
            verb: invoke.verb, id: invoke.id, accepted: accepted, reason: reason, affected: [], values: nil)))
        return true
    }

    static let foreignWords = "That slice belongs to MacBook Pro. It can be changed only there."

    @Test("a refused tune shows the Core's words as sent; the slice keeps the Core's frequency; the next kept one clears them")
    func refusedTuneShowsTheCoresWords() async {
        let (model, store, outbox, _, _) = await makeAnsweredModel()
        #expect(await settle { model.active != nil })
        model.tap(to: 7_240_000)
        #expect(await answerWrite(store, outbox, "frequency", accepted: false, reason: Self.foreignWords))
        #expect(await settle { model.refusal?.text == Self.foreignWords })
        #expect(model.active?.slice.frequencyHz == 7_236_400)
        model.tap(to: 7_241_000)
        #expect(await settle { outbox.frequencies == [7_240_000, 7_241_000] })
        #expect(await answerWrite(store, outbox, "frequency", accepted: true, reason: ""))
        #expect(await settle { model.refusal == nil && model.active?.slice.frequencyHz == 7_241_000 })
    }

    @Test("a refused drag puts the slice back on the Core's frequency")
    func refusedDragReturnsToTheCore() async {
        let (model, store, outbox, clock, _) = await makeAnsweredModel()
        #expect(await settle { model.active != nil })
        model.drag(to: 7_237_000)
        // The finger's frequency shows at once.
        #expect(model.active?.slice.frequencyHz == 7_237_000)
        clock.now = 0.1
        model.finishDrag()
        #expect(await answerWrite(store, outbox, "frequency", accepted: false, reason: Self.foreignWords))
        #expect(await settle { model.refusal?.text == Self.foreignWords })
        #expect(await settle { model.active?.slice.frequencyHz == 7_236_400 })
    }

    @Test("a frequency typed on the pad shows its refusal on the pad, not over the band")
    func typedRefusalStaysOnThePad() async {
        let (model, store, outbox, _, _) = await makeAnsweredModel()
        #expect(await settle { model.active != nil })
        let typed = Task { await model.enter(999_000_000, sliceId: 0) }
        #expect(await answerWrite(store, outbox, "frequency", accepted: false, reason: "Out of range."))
        let outcome = await typed.value
        #expect(!outcome.accepted && outcome.reason == "Out of range.")
        #expect(model.refusal == nil)
    }

    @Test("a refused mode, step or lock shows the Core's words; a refusal without words shows the phone's")
    func refusedSettingsShowTheCoresWords() async {
        let (model, store, outbox, _, _) = await makeAnsweredModel()
        #expect(await settle { model.active != nil })
        model.setMode(1, sliceId: 0)
        #expect(await answerWrite(store, outbox, "dspMode", accepted: false, reason: "The mode is held while transmitting."))
        #expect(await settle { model.refusal?.text == "The mode is held while transmitting." })
        #expect(store.object("slice:0")?["dspMode"] == .int(0))

        model.setStep(1_000, sliceId: 0)
        #expect(await answerWrite(store, outbox, "stepHz", accepted: false, reason: Self.foreignWords))
        #expect(await settle { model.refusal?.text == Self.foreignWords })
        #expect(model.active?.stepHz == 100)

        model.setLocked(true, sliceId: 0)
        #expect(await answerWrite(store, outbox, "locked", accepted: false, reason: ""))
        #expect(await settle { model.refusal?.text == BandSlicesModel.refusedText })
        #expect(model.active?.locked == false)

        // Closed by the operator.
        model.dismissRefusal()
        #expect(model.refusal == nil)
    }

    @Test("a refused close, activate or receiver hold shows the Core's words")
    func refusedCommandsShowTheCoresWords() async {
        let (model, _, outbox, _, commands) = await makeAnsweredModel()
        #expect(await settle { model.entries.count == 2 })
        model.close(1)
        #expect(await answerCommand(commands, outbox, BandSlicesModel.removeVerb, accepted: false,
                                    reason: Self.foreignWords))
        #expect(await settle { model.refusal?.text == Self.foreignWords })
        #expect(model.entries.count == 2)

        model.activate(1)
        #expect(await answerCommand(commands, outbox, BandSlicesModel.activateVerb, accepted: false,
                                    reason: "Slice B is on another receiver."))
        #expect(await settle { model.refusal?.text == "Slice B is on another receiver." })
        #expect(model.activeSliceId == 0)

        model.setReceiverPinned(true, sliceId: 0)
        #expect(await answerCommand(commands, outbox, BandSlicesModel.pinVerb, accepted: false, reason: ""))
        #expect(await settle { model.refusal?.text == BandSlicesModel.refusedText })
        let first = model.refusal?.id

        // A kept command clears the words.
        model.setReceiverPinned(false, sliceId: 0)
        #expect(await settle {
            outbox.messages.filter { message in
                if case .commandInvoke(let invoke) = message { return invoke.verb == BandSlicesModel.pinVerb }
                return false
            }.count == 2
        })
        #expect(await answerCommand(commands, outbox, BandSlicesModel.pinVerb, accepted: true, reason: ""))
        #expect(await settle { model.refusal == nil })
        #expect(first != nil)
    }

    @Test("a receiver's window hold and sample rate show at the touch; refused, the Core's value returns with its words")
    func commandValuesShowAtTheTouch() async {
        // JJ, 2026-10-01: every control acts at the touch (StationClient.cpp:1040-1068).
        let (model, _, outbox, _, commands) = await makeAnsweredModel()
        #expect(await settle { model.entries.first { $0.id == 0 }?.sampleRateHz == 192_000 })
        model.setReceiverPinned(true, sliceId: 0)
        #expect(await settle { model.entries.first { $0.id == 0 }?.receiverPinned == true })
        #expect(await answerCommand(commands, outbox, BandSlicesModel.pinVerb, accepted: false,
                                    reason: Self.foreignWords))
        #expect(await settle { model.entries.first { $0.id == 0 }?.receiverPinned == false })
        #expect(model.refusal?.text == Self.foreignWords)

        model.requestSampleRate(96_000, sliceId: 0)
        #expect(await settle { model.entries.first { $0.id == 0 }?.sampleRateHz == 96_000 })
        #expect(await answerCommand(commands, outbox, BandSlicesModel.sampleRateVerb, accepted: true, reason: ""))
        #expect(await settle { model.refusal == nil })
        #expect(model.entries.first { $0.id == 0 }?.sampleRateHz == 96_000)
    }

    @Test("a refused filter edge shows the Core's words on the RX panel; one without words shows the phone's")
    func refusedFilterShowsTheCoresWords() async throws {
        let (model, store, outbox, _, commands) = await makeAnsweredModel()
        #expect(await settle { model.active != nil })
        let rx = RxPanelModel(store: store, slices: model, catalogFeed: CatalogFeed(store: store), commands: commands)
        let pad = try #require(rx.filterEdgePad(low: true, close: {}))
        for key: ValuePadModel.Key in [.delete, .delete, .delete, .delete, .digit(2), .digit(0), .digit(0), .digit(0)] {
            pad.press(key)
        }
        let first = Task { await pad.enter() }
        #expect(await answerWrite(store, outbox, "filterLow", accepted: false, reason: Self.foreignWords))
        #expect(await first.value == false)
        #expect(await settle { rx.note == Self.foreignWords })
        #expect(store.object("slice:0")?["filterLow"] == .int(-3000))
        let second = Task { await pad.enter() }
        #expect(await settle { outbox.messages.filter { message in
            if case .propertyWrite(let write) = message { return write.properties.first?.name == "filterLow" }
            return false
        }.count == 2 })
        #expect(await answerWrite(store, outbox, "filterLow", accepted: false, reason: ""))
        #expect(await second.value == false)
        #expect(await settle { rx.note == BandSlicesModel.refusedText })
    }

    // MARK: Taking control (R-IOS-42)

    /// A Core that shares slices: slice A is this phone's, slice B another
    /// device's that this phone listens to.
    private func makeSharedModel() async -> (BandSlicesModel, MirrorStore, Outbox, CommandClient) {
        let (model, store, outbox, _, commands) = await makeAnsweredModel()
        store.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "propertyResultVersion", value: .i64(1)),
            .init(name: SliceAccess.capability, value: .i64(2)),
            .init(name: "remoteTxVersion", value: .i64(1)),
        ])))
        func access(_ id: Int64, _ controller: String, _ revision: Int64) -> LinkMessage {
            .objectCreate(LinkMessage.ObjectCreate(key: "access:\(id)", className: SliceAccess.accessClass, properties: [
                .init(ordinal: 0, name: "sliceId", value: .i64(id)),
                .init(ordinal: 1, name: "incarnation", value: .i64(40 + id)),
                .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(controller)),
                .init(ordinal: 3, name: "controlRevision", value: .i64(revision)),
                .init(ordinal: 4, name: "listenerDeviceIds", value: .utf8("[]")),
            ]))
        }
        store.apply(access(0, "me", 1))
        store.apply(access(1, "shack-desktop", 12))
        return (model, store, outbox, commands)
    }

    private func controller(_ store: MirrorStore, _ id: String, revision: Int64) {
        store.apply(.delta(LinkMessage.Delta(key: "access:1", properties: [
            .init(ordinal: 2, name: "controllerDeviceId", value: .utf8(id)),
            .init(ordinal: 3, name: "controlRevision", value: .i64(revision)),
        ])))
    }

    @Test("a slice reads as this phone's until the phone knows its own id; then another's is listened, and nobody's too")
    func listenedSlices() async {
        let (model, store, _, _) = await makeSharedModel()
        #expect(await settle { model.entries.count == 2 })
        #expect(model.entries.allSatisfy { $0.control == .here })
        model.thisDeviceId = "me"
        #expect(model.entries[0].control == .here && model.isTunable(model.entries[0]))
        #expect(model.entries[1].control
                == .listening(ownerLine: "Slice B is controlled by another device. Take control to change it."))
        #expect(!model.isTunable(model.entries[1]))
        controller(store, "", revision: 13)
        #expect(await settle {
            model.entries[1].control == .listening(ownerLine: "Nobody controls slice B. Take control to change it.")
        })
    }

    @Test("an accepted take marks the slice for its first key even before the access change lands; moving away clears it")
    func takeMarksTheFirstKey() async {
        let (model, store, outbox, commands) = await makeSharedModel()
        model.thisDeviceId = "me"
        #expect(await settle { model.entries.count == 2 && model.entries[1].listening })
        model.takeControl(1)
        #expect(model.taking == [1])
        // A second tap while the first is on its way sends nothing more.
        model.takeControl(1)
        #expect(await settle { outbox.messages.contains { if case .commandInvoke = $0 { return true }; return false } })
        guard case .commandInvoke(let invoke)? = outbox.messages.last(where: {
            if case .commandInvoke = $0 { return true }
            return false
        }) else {
            Issue.record("no take")
            return
        }
        #expect(invoke.verb == "slice.takeControl")
        #expect(invoke.args == [.init(name: "sliceId", value: .i64(1)), .init(name: "incarnation", value: .i64(41)),
                                .init(name: "controlRevision", value: .i64(12))])
        await commands.receive(.commandResult(LinkMessage.CommandResult(
            verb: invoke.verb, id: invoke.id, accepted: true, reason: "", affected: ["access:1"],
            values: [.init(name: "controlRevision", value: .i64(13))])))
        #expect(await settle { model.taking.isEmpty && model.takenHere == [1] })
        // Another slice's change rebuilds while the access still reads 12: the mark stays.
        store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_236_500)),
        ])))
        #expect(await settle { model.entries[0].slice.frequencyHz == 7_236_500 })
        #expect(model.takenHere == [1])
        controller(store, "me", revision: 13)
        #expect(await settle { model.entries[1].control == .here })
        #expect(model.takenHere == [1])
        // Taken away again: the mark goes.
        controller(store, "shack-desktop", revision: 14)
        #expect(await settle { model.takenHere.isEmpty && model.entries[1].listening })
        #expect(outbox.messages.filter { if case .commandInvoke = $0 { return true }; return false }.count == 1)
    }

    @Test("a take the Core never answers shows the phone's no-answer words and the button comes back")
    func unansweredTake() async {
        let (model, _, _, commands) = await makeSharedModel()
        model.thisDeviceId = "me"
        #expect(await settle { model.entries.count == 2 && model.entries[1].listening })
        model.takeControl(1)
        #expect(model.taking == [1])
        await commands.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(await settle { model.taking.isEmpty && model.refusal?.text == SeveralDevicesClient.noAnswerText })
        #expect(model.takenHere.isEmpty)
    }
    @Test("a direct slice control's late refusal replaces its own timeout hint", arguments: ["dspMode", "stepHz", "locked"])
    func directControlLateRefusal(property: String) async throws {
        let linkClock = TestLinkClock()
        let (model, store, outbox, _, _) = await makeAnsweredModel(linkClock: linkClock)
        switch property {
        case "dspMode": model.setMode(1, sliceId: 0)
        case "stepHz": model.setStep(500, sliceId: 0)
        default: model.setLocked(true, sliceId: 0)
        }
        #expect(await settle { lastWrite(outbox, property) != nil })
        let write = try #require(lastWrite(outbox, property))
        await linkClock.advance(by: 5_000)
        #expect(await settle { model.refusal?.text == PropertyWriteOutcome.notConfirmed.reason })
        store.apply(.propertyResult(.init(key: write.key, writeId: try #require(write.writeId), results: [
            .init(property: property, accepted: false, reason: "This slice setting is unavailable.", value: nil),
        ])))
        #expect(await settle { model.refusal?.text == "This slice setting is unavailable." })
    }

}
