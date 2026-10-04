// NereusSDR for iOS: the Core's reports about other devices, read, and this phone's answers to its questions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-17, R-IOS-30: `connectedDevices`, `marker:<id>`, `confirm.request`
/// and `notice` read as the link document and the Core's fixtures write
/// them; Confirm, Cancel and Take it back send exactly the Core's verbs,
/// and nothing changes on the phone until the Core answers.
@MainActor
@Suite struct SeveralDevicesTests {
    private let sent = SentMessages()

    // MARK: Reading the reports

    private static func controlWire(_ id: String) throws -> LinkMessage {
        let fixture = try #require(try LinkFixtureLoader.manifest().first { $0.id == id })
        let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(fixture.file))
        return try LinkCodec.decode(try LinkJSON(foundation: try #require(object["wire"])).compactText)
    }

    @Test func aPanMoveQuestionReadsAsTheCoreWroteIt() throws {
        guard case .confirmRequest(let wire) = try Self.controlWire("control-station-confirm-request") else {
            Issue.record("not a confirm.request")
            return
        }
        let question = SeveralDevices.Question(wire)
        #expect(question.id == 7)
        #expect(question.kind == .panMove)
        #expect(question.reason == SeveralDevices.waitingReason)
        #expect(question.change == SeveralDevices.Change(label: "Receiver 1", from: "40 m", to: "20 m"))
        #expect(question.forWriteId == 501)
        #expect(question.expiresInMs == 60000)
        let device = try #require(question.affected.first)
        #expect(device.deviceShortName == "iPad")
        #expect(device.state == .listening)
        #expect(device.slices == [SeveralDevices.AffectedSlice(sliceId: 1, letter: "B", frequencyHz: 7_150_000, band: 3,
                                                               mode: 1, adc: 0, streamIndex: 0, effect: .moves)])
        #expect(question.choices.isEmpty)
    }

    @Test func aReceiverTakenNoticeNamesWhoWhatAndWhen() throws {
        guard case .notice(let wire) = try Self.controlWire("control-station-notice") else {
            Issue.record("not a notice")
            return
        }
        let notice = SeveralDevices.Notice(wire)
        #expect(notice.kind == .receiverTaken)
        #expect(notice.takeBack)
        #expect(notice.by == SeveralDevices.NoticeBy(deviceId: "example-device-a", name: "iPhone", shortName: "iPhone",
                                                     kind: "phone", radioPtt: false))
        #expect(notice.slices == [SeveralDevices.NoticeSlice(sliceId: 1, letter: "B", frequencyHz: 14_074_000, mode: 1,
                                                             band: 5)])
        // The time of day comes from the phone's own clock.
        let received = Date(timeIntervalSince1970: 1_000_000)
        #expect(notice.happened(received: received) == received.addingTimeInterval(-30))
    }

    @Test func aTakeReceiverQuestionListsEachReceiverWithItsDevicesAndSlices() throws {
        let question = try #require(try FixtureReplay.stationMessages("session-take-receiver").compactMap { message in
            if case .confirmRequest(let wire) = message {
                return SeveralDevices.Question(wire)
            }
            return nil
        }.first)
        #expect(question.kind == .takeReceiver)
        #expect(question.forCommandId == 1352)
        #expect(question.choices.map(\.choice) == [0, 1])
        #expect(question.choices[0].takeable == false)
        #expect(question.choices[0].why == "Your panadapter already uses this receiver.")
        let other = question.choices[1]
        #expect(other.takeable && other.streamIndex == 1)
        #expect(other.devices.map(\.shortName) == ["Tablet B"])
        #expect(other.slices.map(\.letter) == ["B"])
        #expect(other.slices.first?.frequencyHz == 7_074_000)
        #expect(question.firstTakeable == other)
    }

    @Test func aTakeSliceChoiceIsReadAsOneSliceOnItsReceiver() throws {
        let wire = LinkMessage.ConfirmRequest(id: 3, kind: "takeSlice", reason: SeveralDevices.waitingReason, affected: [],
                                              expiresInMs: 60000, choices: [.object([
            "adc": .number(0), "band": .number(5), "choice": .number(0), "deviceId": .string("d1"),
            "deviceName": .string("Other device 1"), "deviceShortName": .string("Tablet B"),
            "frequencyHz": .number(14_225_000), "letter": .string("B"), "mode": .number(1), "sliceId": .number(1),
            "state": .string("listening"), "streamIndex": .number(0), "takeable": .bool(true),
            "txSlice": .bool(false), "why": .string(""),
        ])], forCommandId: 1361)
        let choice = try #require(SeveralDevices.Question(wire).choices.first)
        #expect(choice.slices.map(\.letter) == ["B"])
        #expect(choice.devices.map(\.shortName) == ["Tablet B"])
        #expect(choice.devices.first?.state == .listening)
        #expect(choice.takeable)
    }

    @Test func whoIsOnTheCoreReadsFromTheListTheCoreSends() throws {
        let messages = try FixtureReplay.stationMessages("session-connected-devices")
        let lists: [String] = messages.compactMap { message in
            guard case .delta(let delta) = message, delta.key == SeveralDevices.connectedDevicesKey,
                  case .utf8(let text)? = delta.properties.first(where: { $0.name == "listJson" })?.value else {
                return nil
            }
            return text
        }
        #expect(lists.count >= 2)
        let first = SeveralDevices.connectedDevices(fromListJson: lists[0])
        #expect(first.count == 2)
        #expect(first[0].name == "Conformance device")
        #expect(first[0].shortName == "Conformance")
        #expect(first[0].state == .listening)
        #expect(first[0].paired && first[0].revocable && !first[0].hostsCore)
        #expect(first[1].shortName == "Tablet B")
        #expect(first[1].listeningOn.first?.letter == "B")
        #expect(SeveralDevices.connectedDevices(fromListJson: "not json").isEmpty)
    }

    @Test func anotherDevicesSliceReadsFromItsMarker() throws {
        let store = MirrorStore(send: sent.sender)
        // The connect sequence, up to its snapshot.complete.
        for message in try FixtureReplay.stationMessages("session-two-devices", throughSnapshot: true) {
            store.apply(message)
        }
        let markers = SeveralDevices.markers(in: store)
        #expect(markers.count == 1)
        let marker = try #require(markers.first)
        #expect(marker.sliceId == 0 && marker.letter == "A")
        #expect(marker.ownerName == "Other device 1")
        #expect(marker.ownerShortName == "Tablet B")
        #expect(marker.ownerKind == "tablet")
        #expect(!marker.ownerAway)
        #expect(marker.frequencyHz == 14_225_000)
        #expect(marker.filterLowHz == 100 && marker.filterHighHz == 3000)
        // The other device holds no transmit in this fixture, so its slice
        // is not marked as the transmit slice (the trunk at 6c3f543d).
        #expect(!marker.txSlice)
        #expect(SeveralDevices.available(in: store))
    }

    @Test func aCoreWithoutTheFeatureSharesNothing() {
        let store = MirrorStore(send: sent.sender)
        store.apply(FixtureReplay.stationHello())
        store.apply(FixtureReplay.capabilities([:]))
        #expect(!SeveralDevices.available(in: store))
        store.apply(FixtureReplay.capabilities([SeveralDevices.capability: .i64(1)]))
        #expect(SeveralDevices.available(in: store))
        let older = MirrorStore(send: sent.sender)
        older.apply(FixtureReplay.stationHello(minor: 10))
        older.apply(FixtureReplay.capabilities([SeveralDevices.capability: .i64(1)]))
        #expect(!SeveralDevices.available(in: older))
    }

    // MARK: Answering

    private func client(_ store: MirrorStore, _ settings: SettingsProxyClient, _ commands: CommandClient,
                        now: Date = Date(timeIntervalSince1970: 2_000_000)) async -> SeveralDevicesClient {
        let client = SeveralDevicesClient(store: store, settings: settings, commands: commands, now: { now })
        await commands.handle(.stateChanged(.receivingSnapshot))
        client.handle(.stateChanged(.receivingSnapshot))
        await commands.handle(.stateChanged(.ready))
        client.handle(.stateChanged(.ready))
        return client
    }

    private static func shared(_ id: Int64 = 1, key: String? = nil) -> LinkMessage {
        .confirmRequest(LinkMessage.ConfirmRequest(
            id: id, kind: "sharedSetting", reason: SeveralDevices.waitingReason,
            affected: [.object(["deviceId": .string("d1"), "deviceName": .string("Other device 1"),
                                "deviceShortName": .string("Tablet B"), "state": .string("listening"),
                                "holdsTransmit": .bool(false), "slices": .array([])])],
            expiresInMs: 60000,
            change: ["label": .string("Receive on the transmit antenna"), "from": .string("Off"), "to": .string("On")],
            forWriteId: key == nil ? 3 : nil, forSettingsKey: key))
    }

    private static func answer(_ verb: String, id: UInt32, accepted: Bool, reason: String = "",
                               values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
        .commandResult(LinkMessage.CommandResult(verb: verb, id: id, accepted: accepted, reason: reason, affected: [],
                                                 values: values))
    }

    @Test func confirmSendsTheQuestionsIdAndAppliesTheReadbackOnlyWhenTheCoreAnswers() async throws {
        let store = MirrorStore(send: sent.sender)
        // The receive antenna relay asks first; the preamp now applies at once
        // with a notice (the Core's ruling 7.1a), so the question's example is the relay.
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "alexAntennas", className: "AlexAntennaFacade",
                                                           properties: [
            LinkMessage.PropertyEntry(ordinal: 2, name: "useTxAntennaForRx", value: .bool(false)),
        ])))
        let settings = SettingsProxyClient(send: sent.sender)
        let commands = CommandClient(send: sent.sender)
        let devices = await client(store, settings, commands)
        devices.receive(Self.shared())
        #expect(devices.question?.kind == .sharedSetting)
        #expect(devices.questionReceived == Date(timeIntervalSince1970: 2_000_000))

        let task = Task { await devices.proceed() }
        #expect(await sent.settle(untilCount: 1))
        #expect(sent.messages.last == .commandInvoke(LinkMessage.CommandInvoke(verb: "confirm.proceed", id: 1, args: [
            LinkMessage.PropertyEntry(name: "id", value: .i64(1)),
            LinkMessage.PropertyEntry(name: "choice", value: .i64(-1)),
        ])))
        #expect(devices.answering == .proceeding)
        // Nothing changes before the Core's answer.
        #expect(store.object("alexAntennas")?["useTxAntennaForRx"] == .bool(false))
        #expect(devices.question != nil)

        await commands.receive(Self.answer("confirm.proceed", id: 1, accepted: true, values: [
            LinkMessage.PropertyEntry(name: "objectKey", value: .utf8("alexAntennas")),
            LinkMessage.PropertyEntry(ordinal: 2, name: "useTxAntennaForRx", value: .bool(true)),
        ]))
        await task.value
        #expect(store.object("alexAntennas")?["useTxAntennaForRx"] == .bool(true))
        #expect(devices.question == nil)
        #expect(devices.answering == .idle)
    }

    @Test func aSettingsReadbackGoesIntoTheCache() async throws {
        let store = MirrorStore(send: sent.sender)
        let settings = SettingsProxyClient(send: sent.sender)
        let commands = CommandClient(send: sent.sender)
        let devices = await client(store, settings, commands)
        devices.receive(Self.shared(key: "DspOptionsFilterSizeRx"))
        let task = Task { await devices.proceed() }
        #expect(await sent.settle(untilCount: 1))
        await commands.receive(Self.answer("confirm.proceed", id: 1, accepted: true, values: [
            LinkMessage.PropertyEntry(name: "settingsKey", value: .utf8("DspOptionsFilterSizeRx")),
            LinkMessage.PropertyEntry(name: "value", value: .utf8("4096")),
        ]))
        await task.value
        #expect(settings.value("DspOptionsFilterSizeRx") == "4096")
    }

    @Test func aTakeSendsTheChosenReceiversChoice() async throws {
        let store = MirrorStore(send: sent.sender)
        let commands = CommandClient(send: sent.sender)
        let devices = await client(store, SettingsProxyClient(send: sent.sender), commands)
        for message in try FixtureReplay.stationMessages("session-take-receiver") {
            if case .confirmRequest = message {
                devices.receive(message)
                break
            }
        }
        let choice = try #require(devices.question?.firstTakeable)
        let task = Task { await devices.proceed(choice: choice.choice) }
        #expect(await sent.settle(untilCount: 1))
        #expect(sent.messages.last == .commandInvoke(LinkMessage.CommandInvoke(verb: "confirm.proceed", id: 1, args: [
            LinkMessage.PropertyEntry(name: "id", value: .i64(1)),
            LinkMessage.PropertyEntry(name: "choice", value: .i64(1)),
        ])))
        await commands.receive(Self.answer("confirm.proceed", id: 1, accepted: true))
        await task.value
        #expect(devices.question == nil)
    }

    @Test func aRefusedConfirmKeepsTheQuestionWithTheCoresWords() async throws {
        let store = MirrorStore(send: sent.sender)
        let commands = CommandClient(send: sent.sender)
        let devices = await client(store, SettingsProxyClient(send: sent.sender), commands)
        devices.receive(Self.shared())
        let task = Task { await devices.proceed() }
        #expect(await sent.settle(untilCount: 1))
        await commands.receive(Self.answer("confirm.proceed", id: 1, accepted: false,
                                           reason: "That question has expired. Make the change again."))
        await task.value
        #expect(devices.answering == .refused("That question has expired. Make the change again."))
        #expect(devices.question?.id == 1)
        devices.closeQuestion()
        #expect(devices.question == nil && devices.answering == .idle)
    }

    /// R5: a confirm the Core never answered may still be open there, so
    /// closing it sends confirm.cancel; a confirm the Core refused has
    /// already been dropped there (ConfirmStep::answer takes it), so
    /// closing that one sends nothing.
    @Test func closingAQuestionTheCoreNeverAnsweredCancelsIt() async throws {
        let clock = ManualLinkClock()
        let commands = CommandClient(clock: clock, send: sent.sender)
        let devices = await client(MirrorStore(send: sent.sender), SettingsProxyClient(send: sent.sender), commands)
        devices.receive(Self.shared(7))
        let task = Task { await devices.proceed() }
        #expect(await sent.settle(untilCount: 1))
        for _ in 0..<60 where devices.answering == .proceeding {
            await clock.advance(by: 1_000)
            await Task.yield()
        }
        #expect(await task.value == false)
        #expect(devices.answering == .refused(SeveralDevicesClient.noAnswerText))
        devices.closeQuestion()
        #expect(devices.question == nil && devices.answering == .idle)
        #expect(await sent.settle(untilCount: 2))
        guard case .commandInvoke(let cancel)? = sent.messages.last else {
            Issue.record("closing the unanswered question sent nothing")
            return
        }
        #expect(cancel.verb == SeveralDevices.cancelVerb)
        #expect(cancel.args == [LinkMessage.PropertyEntry(name: "id", value: .i64(7))])
    }

    /// R5: closing a question after the Core refused its confirm sends
    /// nothing, even when the Core's words read like the phone's own for
    /// an answer that never came: only a confirm that had no answer is
    /// cancelled.
    @Test func closingAfterARefusedConfirmSendsNothingWhateverItsWords() async throws {
        let commands = CommandClient(send: sent.sender)
        let devices = await client(MirrorStore(send: sent.sender), SettingsProxyClient(send: sent.sender), commands)
        func lastInvoke() throws -> LinkMessage.CommandInvoke {
            guard case .commandInvoke(let invoke)? = sent.messages.last else {
                throw CancellationError()
            }
            return invoke
        }
        let refusals = ["That question has expired. Make the change again.", SeveralDevicesClient.noAnswerText]
        for (index, words) in refusals.enumerated() {
            let id = Int64(index + 1)
            devices.receive(Self.shared(id))
            let task = Task { await devices.proceed() }
            #expect(await sent.settle(untilCount: index + 1))
            let invoke = try lastInvoke()
            #expect(invoke.verb == SeveralDevices.proceedVerb)
            await commands.receive(Self.answer(invoke.verb, id: invoke.id, accepted: false, reason: words))
            #expect(await task.value == false)
            #expect(devices.answering == .refused(words))
            devices.closeQuestion()
            #expect(devices.question == nil && devices.answering == .idle)
        }
        // A last confirm, answered: had either close sent confirm.cancel,
        // it would be among what was sent before it.
        devices.receive(Self.shared(3))
        let accepted = Task { await devices.proceed() }
        #expect(await sent.settle(untilCount: refusals.count + 1))
        let last = try lastInvoke()
        await commands.receive(Self.answer(last.verb, id: last.id, accepted: true))
        #expect(await accepted.value == true)
        let verbs = sent.messages.compactMap { message -> String? in
            guard case .commandInvoke(let invoke) = message else {
                return nil
            }
            return invoke.verb
        }
        #expect(verbs == Array(repeating: SeveralDevices.proceedVerb, count: refusals.count + 1))
    }

    @Test func whenTheCoreAsksAgainItsNewQuestionStands() async throws {
        let commands = CommandClient(send: sent.sender)
        let devices = await client(MirrorStore(send: sent.sender), SettingsProxyClient(send: sent.sender), commands)
        devices.receive(Self.shared(1))
        let task = Task { await devices.proceed() }
        #expect(await sent.settle(untilCount: 1))
        await commands.receive(Self.answer("confirm.proceed", id: 1, accepted: false,
                                           reason: SeveralDevices.waitingReason,
                                           values: [LinkMessage.PropertyEntry(name: "phase",
                                                                              value: .utf8("needsConfirmation"))]))
        devices.receive(Self.shared(2))
        await task.value
        #expect(devices.question?.id == 2)
        #expect(devices.answering == .idle)
    }

    @Test func cancelSendsConfirmCancelAndClosesAtOnce() async throws {
        let commands = CommandClient(send: sent.sender)
        let devices = await client(MirrorStore(send: sent.sender), SettingsProxyClient(send: sent.sender), commands)
        devices.receive(Self.shared(5))
        devices.cancel()
        #expect(devices.question == nil)
        #expect(await sent.settle(untilCount: 1))
        #expect(sent.messages.last == .commandInvoke(LinkMessage.CommandInvoke(verb: "confirm.cancel", id: 1, args: [
            LinkMessage.PropertyEntry(name: "id", value: .i64(5)),
        ])))
        await commands.receive(Self.answer("confirm.cancel", id: 1, accepted: true))
    }

    @Test func takeItBackSendsTheNoticesIdAndAwaitsTheQuestion() async throws {
        let commands = CommandClient(send: sent.sender)
        let devices = await client(MirrorStore(send: sent.sender), SettingsProxyClient(send: sent.sender), commands)
        devices.receive(try Self.controlWire("control-station-notice"))
        #expect(devices.notices.map(\.id) == [8])
        let task = Task { await devices.takeBack(8) }
        #expect(await sent.settle(untilCount: 1))
        #expect(sent.messages.last == .commandInvoke(LinkMessage.CommandInvoke(verb: "notice.takeBack", id: 1, args: [
            LinkMessage.PropertyEntry(name: "id", value: .i64(8)),
        ])))
        #expect(devices.notices.first?.takingBack == true)
        await commands.receive(Self.answer("notice.takeBack", id: 1, accepted: false,
                                           reason: SeveralDevices.waitingReason))
        await task.value
        #expect(devices.notices.first?.takeBackRefusal == nil)
        devices.receive(Self.shared(9))
        #expect(devices.notices.first?.takingBack == false)
        #expect(devices.question?.id == 9)
        // Confirming that question takes it back: the notice closes.
        let proceed = Task { await devices.proceed() }
        #expect(await sent.settle(untilCount: 2))
        #expect(devices.notices.count == 1)
        await commands.receive(Self.answer("confirm.proceed", id: 2, accepted: true))
        await proceed.value
        #expect(devices.notices.isEmpty)
    }

    @Test func aTakeBackTheCoreRefusesSaysWhy() async throws {
        let commands = CommandClient(send: sent.sender)
        let devices = await client(MirrorStore(send: sent.sender), SettingsProxyClient(send: sent.sender), commands)
        devices.receive(try Self.controlWire("control-station-notice"))
        let task = Task { await devices.takeBack(8) }
        #expect(await sent.settle(untilCount: 1))
        await commands.receive(Self.answer("notice.takeBack", id: 1, accepted: false,
                                           reason: "That can no longer be taken back."))
        await task.value
        #expect(devices.notices.first?.takeBackRefusal == "That can no longer be taken back.")
        #expect(devices.notices.first?.takingBack == false)
    }

    /// Taking transmit (Task 77): the Core's question names the holder in
    /// the shape of a `connectedDevices` entry, asked plain while it
    /// listens and again, red, once it is on the air.
    @Test func aTakeTransmitQuestionNamesTheHolderAndWhetherItIsOnTheAir() throws {
        // The two questions of the take-transmit-keyed session, as its
        // fixture writes them, with its placeholders filled.
        func asked(id: Int64, keyed: Bool) throws -> SeveralDevices.Question {
            let text = """
            {"affected": [], "expiresInMs": 60000, "forCommandId": 12, "holder": {"awayForSeconds": 0, \
            "connectedForSeconds": 30, "deviceId": "d2", "keyed": \(keyed), "kind": "tablet", \
            "lastActivitySeconds": 1, "name": "Other device 1", "shortName": "Tablet B", "source": "device", \
            "state": "\(keyed ? "transmitting" : "listening")", "transmittingForSeconds": 0}, "id": \(id), \
            "kind": "takeTransmit", "reason": "Waiting for you to confirm.", "type": "confirm.request"}
            """
            guard case .confirmRequest(let wire) = try LinkCodec.decode(text) else {
                throw LinkFixtureLoader.Malformed(description: "not a confirm.request")
            }
            return SeveralDevices.Question(wire)
        }
        let questions = [try asked(id: 1, keyed: false), try asked(id: 2, keyed: true)]
        let listening = try #require(questions.first)
        #expect(listening.kind == .takeTransmit)
        let holder = try #require(listening.holder)
        #expect(holder.name == "Other device 1" && holder.shortName == "Tablet B" && holder.kind == "tablet")
        #expect(holder.state == .listening && !holder.keyed && !holder.onAir && !holder.radioPtt)
        let keyed = try #require(questions.last?.holder)
        #expect(keyed.state == .transmitting && keyed.keyed && keyed.onAir)
        // A question of another kind names no holder.
        guard case .confirmRequest(let panMove) = try Self.controlWire("control-station-confirm-request") else {
            Issue.record("not a confirm.request")
            return
        }
        #expect(SeveralDevices.Question(panMove).holder == nil)
    }

    @Test func aTakeTransmitQuestionsHolderSurvivesTheCodec() throws {
        let holder: [String: LinkJSON] = ["name": .string("Radio"), "shortName": .string("Radio"),
                                          "kind": .string("station"), "source": .string("radioPtt"),
                                          "state": .string("transmitting"), "keyed": .bool(true),
                                          "transmittingForSeconds": .number(12)]
        let wire = LinkMessage.confirmRequest(LinkMessage.ConfirmRequest(
            id: 3, kind: "takeTransmit", reason: SeveralDevices.waitingReason, affected: [], expiresInMs: 60000,
            forCommandId: 4, holder: holder))
        let again = try LinkCodec.decode(LinkCodec.encode(wire))
        #expect(again == wire)
        guard case .confirmRequest(let request) = again else {
            return
        }
        let read = try #require(SeveralDevices.Question(request).holder)
        #expect(read.radioPtt && read.onAir && read.transmittingForSeconds == 12)
    }

    @Test func proceedSaysWhetherTheCoreAcceptedTheAnswer() async throws {
        let commands = CommandClient(send: sent.sender)
        let devices = await client(MirrorStore(send: sent.sender), SettingsProxyClient(send: sent.sender), commands)
        devices.receive(Self.shared(1))
        let refused = Task { await devices.proceed() }
        #expect(await sent.settle(untilCount: 1))
        await commands.receive(Self.answer("confirm.proceed", id: 1, accepted: false, reason: "No."))
        #expect(await refused.value == false)
        devices.closeQuestion()
        devices.receive(Self.shared(2))
        let accepted = Task { await devices.proceed() }
        #expect(await sent.settle(untilCount: 2))
        await commands.receive(Self.answer("confirm.proceed", id: 2, accepted: true))
        #expect(await accepted.value == true)
        // Nothing open: nothing is sent, and nothing was accepted.
        #expect(await devices.proceed() == false)
        #expect(sent.messages.count == 2)
    }

    @Test func aLostLinkDropsTheQuestionAndTheNotices() async throws {
        let devices = await client(MirrorStore(send: sent.sender), SettingsProxyClient(send: sent.sender),
                             CommandClient(send: sent.sender))
        devices.receive(Self.shared())
        devices.receive(try Self.controlWire("control-station-notice"))
        devices.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(devices.question == nil)
        #expect(devices.notices.isEmpty)
    }

    @Test func aHeldChangeIsReadByItsPhaseOrElseByTheCoresWords() {
        func result(reason: String, phase: String?) -> CommandResult {
            CommandResult(accepted: false, reason: reason, affectedKeys: [], values: [:], phase: phase)
        }
        #expect(SeveralDevices.waitsForConfirmation(result(reason: "", phase: "needsConfirmation")))
        #expect(SeveralDevices.waitsForConfirmation(result(reason: "Waiting for you to confirm.", phase: nil)))
        #expect(!SeveralDevices.waitsForConfirmation(result(reason: "Transmit is changing hands. Try again in a moment.",
                                                            phase: nil)))
    }
}
