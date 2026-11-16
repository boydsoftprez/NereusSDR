// NereusSDR for iOS: the flag's Record and Play, the playing bar and the Kept notice against a recordings client fed by hand
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import Testing

/// Recording plan Task 16 (design sections 3, 6.2, 6.3, V9, A1, A5, A15):
/// what each flag's Record and Play show and send, the playing bar's words,
/// the Kept notice, and the app's recordings client wired to this phone's PTT.
@Suite("Recording flag", .timeLimit(.minutes(1)))
@MainActor
struct RecordingFlagTests {
    /// Everything the client sent.
    final class Sent: @unchecked Sendable {
        private let lock = NSLock()
        private var all: [LinkMessage] = []
        func add(_ message: LinkMessage) { lock.withLock { all.append(message) } }
        var invokes: [LinkMessage.CommandInvoke] {
            lock.withLock {
                all.compactMap { message in
                    if case .commandInvoke(let invoke) = message {
                        return invoke
                    }
                    return nil
                }
            }
        }
    }

    struct Rig {
        let sent: Sent
        let mirror: MirrorStore
        let commands: CommandClient
        let client: RecordingsClient
        let model: RecordingFlagModel
    }

    static func rig(version: Int64?, connected: Bool = true) async -> Rig {
        let sent = Sent()
        let mirror = MirrorStore(send: { sent.add($0) })
        let commands = CommandClient(send: { sent.add($0) })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        var capabilities: [LinkMessage.PropertyEntry] = [.init(ordinal: 0, name: "stationName", value: .utf8(""))]
        if let version {
            capabilities.append(.init(ordinal: 1, name: RecordingsClient.capabilityName, value: .i64(version)))
        }
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        let client = RecordingsClient(mirror: mirror, commands: commands)
        if connected {
            await commands.handle(.stateChanged(.receivingSnapshot))
            client.handle(.stateChanged(.receivingSnapshot))
            await commands.handle(.stateChanged(.ready))
            client.handle(.stateChanged(.ready))
        }
        return Rig(sent: sent, mirror: mirror, commands: commands, client: client,
                   model: RecordingFlagModel(client: client))
    }

    // MARK: The Core's document

    static func document(state: String = "idle", elapsedMs: Int = 0, kept: [String: Any]? = nil,
                         revision: Int = 3, sending: [String: Any]? = nil) -> [String: Any] {
        var slots: [[String: Any]] = (1...8).map { ["n": $0, "name": "", "lengthMs": 0, "empty": true] }
        slots[0] = ["n": 1, "name": "CQ", "lengthMs": 4_000, "empty": false]
        return [
            "version": 3,
            "slices": [["slice": 0, "state": state, "startUtc": state == "recording" ? "2026-11-16T14:32:00.000Z" : "",
                        "startedBy": "", "elapsedMs": elapsedMs, "myVoice": false]],
            "kept": kept ?? ["seq": 0, "slice": -1, "id": "", "name": "", "lengthMs": 0, "reason": "", "device": ""],
            "index": ["revision": revision, "count": 2, "usedBytes": 1_000, "freeBytes": 9_000, "folder": "/r"],
            "settings": ["keepMyVoice": true, "sidesApart": false, "autoStart": state == "armed", "autoStop": true,
                         "quietMinutes": 2, "format": "mp3", "disk": "", "disks": [[String: Any]]()],
            "voiceKeyer": ["slots": slots, "repeat": false, "repeatDelaySec": 5,
                           "sending": sending.map { $0 as Any } ?? NSNull()],
        ]
    }

    static func kept(seq: Int, reason: String = "device", device: String = "iPhone") -> [String: Any] {
        ["seq": seq, "slice": 0, "id": "r-\(seq)", "name": "2026-11-16 1432Z 7.236.400 LSB K4ABC",
         "lengthMs": 205_000, "reason": reason, "device": device]
    }

    /// Publishes the document as the Core's mirror does, and feeds the client.
    static func publish(_ document: [String: Any], mirror: MirrorStore, client: RecordingsClient, revision: Int64) {
        let json = String(decoding: (try? JSONSerialization.data(withJSONObject: document)) ?? Data(), as: UTF8.self)
        let properties: [LinkMessage.PropertyEntry] = [.init(ordinal: 0, name: "json", value: .utf8(json)),
                                                       .init(ordinal: 1, name: "revision", value: .i64(revision))]
        let message: LinkMessage
        if mirror.object(RecordingsDocument.objectKey) == nil {
            mirror.apply(.schema(LinkMessage.Schema(className: "RecordingsFacade", fields: [
                .init(ordinal: 0, name: "json", kind: .utf8), .init(ordinal: 1, name: "revision", kind: .i64),
            ])))
            message = .objectCreate(LinkMessage.ObjectCreate(key: RecordingsDocument.objectKey,
                                                             className: "RecordingsFacade", properties: properties))
        } else {
            message = .delta(LinkMessage.Delta(key: RecordingsDocument.objectKey, properties: properties))
        }
        mirror.apply(message)
        client.handle(.message(message))
    }

    static func publish(_ rig: Rig, _ document: [String: Any], revision: Int64) {
        publish(document, mirror: rig.mirror, client: rig.client, revision: revision)
    }

    /// This phone's playback, as the Core pushes it.
    static func playback(_ rig: Rig, id: String, positionMs: Int64, lengthMs: Int64, playing: Bool, onAir: Bool) {
        rig.client.handle(.message(.commandResult(LinkMessage.CommandResult(
            verb: RecordingsClient.playbackVerb, id: 0, accepted: true, reason: "", affected: [],
            values: [.init(name: "id", value: .utf8(id)), .init(name: "positionMs", value: .i64(positionMs)),
                     .init(name: "lengthMs", value: .i64(lengthMs)), .init(name: "playing", value: .bool(playing)),
                     .init(name: "onAir", value: .bool(onAir))]))))
    }

    /// Answers the `index`th command once it has gone.
    static func answer(_ rig: Rig, _ index: Int, accepted: Bool = true, reason: String = "",
                       values: [LinkMessage.PropertyEntry]? = nil) async {
        #expect(await Eventually.holds { rig.sent.invokes.count > index })
        guard index < rig.sent.invokes.count else {
            return
        }
        let invoke = rig.sent.invokes[index]
        await rig.commands.handle(.message(.commandResult(LinkMessage.CommandResult(
            verb: invoke.verb, id: invoke.id, accepted: accepted, reason: reason,
            affected: accepted ? ["recordings"] : [], values: values))))
    }

    static let entriesJSON = """
    [{"id":"r-9","kind":"Contact","slice":0,"startUtc":"2026-11-16T14:32:00Z","lengthMs":205000,\
    "frequencyHz":7236400,"mode":"LSB","call":"K4ABC","name":"2026-11-16 1432Z 7.236.400 LSB K4ABC",\
    "fileName":"a.mp3","sizeBytes":100},
    {"id":"r-8","kind":"Contact","slice":0,"startUtc":"2026-11-16T13:02:00Z","lengthMs":61000,\
    "frequencyHz":7188000,"mode":"LSB","call":"","name":"2026-11-16 1302Z 7.188.000 LSB",\
    "fileName":"b.mp3","sizeBytes":100}]
    """

    /// Gives the model slice A's two recordings through its list.
    static func listed(_ rig: Rig, at index: Int) async {
        let task = Task { await rig.model.refreshList(slice: 0) }
        await answer(rig, index, values: [.init(name: "entries", value: .utf8(entriesJSON)),
                                          .init(name: "offset", value: .i64(0)), .init(name: "total", value: .i64(2))])
        await task.value
    }

    // MARK: Record

    @Test("Record starts the slice on the Core, shows the running time while it records, and stops it")
    func recordStartsAndStops() async {
        let rig = await Self.rig(version: 1)
        Self.publish(rig, Self.document(), revision: 1)
        #expect(rig.model.look(slice: 0).record == .idle)
        #expect(rig.model.look(slice: 0).recordReason == nil)
        let start = Task { await rig.model.tapRecord(slice: 0) }
        await Self.answer(rig, 0)
        await start.value
        #expect(rig.sent.invokes[0].verb == "recordings.start")
        #expect(rig.sent.invokes[0].args == [.init(name: "slice", value: .i64(0))])

        // Every device's flag shows the Core's recording, with the same time.
        Self.publish(rig, Self.document(state: "recording", elapsedMs: 12_400), revision: 2)
        #expect(rig.model.look(slice: 0).record == .recording(elapsedMs: 12_400))
        #expect(RecordingFlagModel.lengthText(12_400) == "0:12")
        let stop = Task { await rig.model.tapRecord(slice: 0) }
        await Self.answer(rig, 1)
        await stop.value
        #expect(rig.sent.invokes[1].verb == "recordings.stop")
        #expect(rig.sent.invokes[1].args == [.init(name: "slice", value: .i64(0))])
    }

    @Test("armed shows the hollow ring, and a tap records at once")
    func armedRecordsAtOnce() async {
        let rig = await Self.rig(version: 1)
        Self.publish(rig, Self.document(state: "armed"), revision: 1)
        #expect(rig.model.look(slice: 0).record == .armed)
        let start = Task { await rig.model.tapRecord(slice: 0) }
        await Self.answer(rig, 0)
        await start.value
        #expect(rig.sent.invokes.map(\.verb) == ["recordings.start"])
    }

    @Test("the Core's refusal reaches the screen in its words")
    func refusalIsShown() async {
        let rig = await Self.rig(version: 1)
        Self.publish(rig, Self.document(), revision: 1)
        var shown: [String] = []
        rig.model.showRefusal = { shown.append($0) }
        let start = Task { await rig.model.tapRecord(slice: 0) }
        await Self.answer(rig, 0, accepted: false, reason: "The disk is full.")
        await start.value
        #expect(shown == ["The disk is full."])
    }

    // MARK: Play and the playing bar

    @Test("Play plays the slice's latest; the bar shows only while playing, and turns red on the air")
    func playAndTheBar() async {
        let rig = await Self.rig(version: 2)
        Self.publish(rig, Self.document(), revision: 1)
        await Self.listed(rig, at: 0)
        #expect(rig.sent.invokes[0].verb == "recordings.list")
        #expect(rig.sent.invokes[0].args == [.init(name: "offset", value: .i64(0)), .init(name: "count", value: .i64(8)),
                                             .init(name: "slice", value: .i64(0)), .init(name: "kind", value: .utf8("Contact"))])
        #expect(rig.model.others(slice: 0).map(\.id) == ["r-9", "r-8"])
        #expect(rig.model.look(slice: 0).playReason == nil)
        #expect(rig.model.bar(slice: 0) == nil)

        let play = Task { await rig.model.tapPlay(slice: 0) }
        await Self.answer(rig, 1)
        await play.value
        #expect(rig.sent.invokes[1].verb == "recordings.play")
        #expect(rig.sent.invokes[1].args == [.init(name: "id", value: .utf8("r-9"))])

        Self.playback(rig, id: "r-9", positionMs: 12_000, lengthMs: 205_000, playing: true, onAir: false)
        #expect(rig.model.look(slice: 0).play == .playing(positionMs: 12_000))
        let bar = rig.model.bar(slice: 0)
        #expect(bar?.title == "7.236.400 LSB K4ABC")
        #expect(bar?.detail == "0:12 / 3:25 · speaker")
        // Only under the flag that plays it.
        #expect(rig.model.bar(slice: 1) == nil)

        Self.playback(rig, id: "r-9", positionMs: 12_000, lengthMs: 205_000, playing: true, onAir: true)
        #expect(rig.model.look(slice: 0).play == .onAir(positionMs: 12_000))
        #expect(rig.model.bar(slice: 0)?.detail == "ON AIR · 0:12 / 3:25")

        // Play again stops it; the bar's Stop does the same.
        let stop = Task { await rig.model.tapPlay(slice: 0) }
        await Self.answer(rig, 2)
        await stop.value
        #expect(rig.sent.invokes[2].verb == "recordings.stopPlay")
        Self.playback(rig, id: "", positionMs: 0, lengthMs: 0, playing: false, onAir: false)
        #expect(rig.model.bar(slice: 0) == nil)
        #expect(rig.model.look(slice: 0).play == .idle)

        // Others plays any of the last eight.
        let other = Task { await rig.model.play(rig.model.others(slice: 0)[1], slice: 0) }
        await Self.answer(rig, 3)
        await other.value
        #expect(rig.sent.invokes[3].args == [.init(name: "id", value: .utf8("r-8"))])
        Self.playback(rig, id: "r-8", positionMs: 1_000, lengthMs: 61_000, playing: true, onAir: false)
        #expect(rig.model.bar(slice: 0)?.title == "7.188.000 LSB")
    }

    @Test("Play with nothing recorded on the slice is greyed, and its tap says why")
    func playWithNothingRecorded() async {
        let rig = await Self.rig(version: 1)
        Self.publish(rig, Self.document(), revision: 1)
        #expect(rig.model.look(slice: 0).playReason == "Nothing recorded on Slice A.")
        await rig.model.tapPlay(slice: 0)
        #expect(rig.sent.invokes.isEmpty)
        #expect(rig.model.notice(slice: 0) == .reason("Nothing recorded on Slice A."))
    }

    // MARK: Older Core and no link

    @Test("an older Core leaves Record and Play visible and greyed with the update reason, and sends nothing")
    func olderCore() async {
        let rig = await Self.rig(version: nil)
        #expect(rig.client.state == .needsCoreUpdate)
        let look = rig.model.look(slice: 0)
        #expect(look.recordReason == "This Core needs an update for recordings.")
        #expect(look.playReason == "This Core needs an update for recordings.")
        await rig.model.tapRecord(slice: 0)
        #expect(rig.model.notice(slice: 0) == .reason("This Core needs an update for recordings."))
        await rig.model.tapPlay(slice: 0)
        #expect(rig.sent.invokes.isEmpty)
        // The reason stays its six seconds.
        rig.model.expireNotices(now: Date().addingTimeInterval(5))
        #expect(rig.model.notice(slice: 0) != nil)
        rig.model.expireNotices(now: Date().addingTimeInterval(7))
        #expect(rig.model.notice(slice: 0) == nil)
    }

    @Test("with no link both are greyed with the not connected note")
    func notConnected() async {
        let rig = await Self.rig(version: 1, connected: false)
        #expect(rig.model.look(slice: 0).recordReason == RecordingFlagModel.notConnectedReason)
        #expect(RecordingFlagModel.notConnectedReason == "Connect to a radio to use recordings.")
    }

    // MARK: Kept

    @Test("Kept shows once, under its slice's flag, with the desktop's words and Play, for six seconds")
    func keptNotice() async {
        let rig = await Self.rig(version: 1)
        let start = Date(timeIntervalSince1970: 1_000)
        // The Kept a document carries when it first comes is old news.
        Self.publish(rig, Self.document(kept: Self.kept(seq: 4)), revision: 1)
        rig.model.keptChanged(now: start)
        #expect(rig.model.notice(slice: 0) == nil)

        Self.publish(rig, Self.document(kept: Self.kept(seq: 5)), revision: 2)
        rig.model.keptChanged(now: start)
        #expect(rig.model.notice(slice: 0) == .kept(id: "r-5",
                                                    title: "Kept in Recordings: 2026-11-16 1432Z 7.236.400 LSB K4ABC, 3:25",
                                                    reason: "Stopped on the iPhone"))
        // The same Kept again is not news.
        rig.model.expireNotices(now: start.addingTimeInterval(6))
        rig.model.keptChanged(now: start.addingTimeInterval(7))
        #expect(rig.model.notice(slice: 0) == nil)

        Self.publish(rig, Self.document(kept: Self.kept(seq: 6, reason: "quiet")), revision: 3)
        rig.model.keptChanged(now: start.addingTimeInterval(10))
        rig.model.expireNotices(now: start.addingTimeInterval(15.9))
        #expect(rig.model.notice(slice: 0) != nil)
        // Recording again takes it down.
        Self.publish(rig, Self.document(state: "recording", kept: Self.kept(seq: 6, reason: "quiet")), revision: 4)
        #expect(rig.model.notice(slice: 0) == nil)
        Self.publish(rig, Self.document(kept: Self.kept(seq: 6, reason: "quiet")), revision: 5)
        let play = Task { await rig.model.playKept(slice: 0) }
        await Self.answer(rig, 0)
        await play.value
        #expect(rig.sent.invokes[0].verb == "recordings.play")
        #expect(rig.sent.invokes[0].args == [.init(name: "id", value: .utf8("r-6"))])
        #expect(rig.model.notice(slice: 0) == nil)
    }

    @Test("why it stopped reads as the desktop flag says it")
    func keptReasons() {
        func kept(_ reason: RecordingsDocument.KeptReason?, _ device: String = "") -> RecordingsDocument.Kept {
            RecordingsDocument.Kept(seq: 1, slice: 0, id: "r", name: "n", lengthMs: 0, reason: reason, device: device)
        }
        #expect(RecordingFlagModel.keptReason(kept(.device, "iPhone"), quietMinutes: 2) == "Stopped on the iPhone")
        #expect(RecordingFlagModel.keptReason(kept(.device, "JJ's iPad"), quietMinutes: 2) == "Stopped on JJ's iPad")
        #expect(RecordingFlagModel.keptReason(kept(.device, ""), quietMinutes: 2) == "Stopped on another device")
        #expect(RecordingFlagModel.keptReason(kept(.quiet), quietMinutes: 2)
            == "Stopped by itself after two quiet minutes")
        #expect(RecordingFlagModel.keptReason(kept(.quiet), quietMinutes: 1) == "Stopped by itself after one quiet minute")
        #expect(RecordingFlagModel.keptReason(kept(.diskFull), quietMinutes: 2) == "Stopped: the disk is full")
        #expect(RecordingFlagModel.keptReason(kept(.wav), quietMinutes: 2) == "Kept as WAV: the MP3 encoder failed")
        #expect(RecordingFlagModel.keptReason(kept(.here), quietMinutes: 2) == "Stopped on the Core")
        #expect(RecordingFlagModel.lengthText(3_723_000) == "1:02:03")
    }

    // MARK: The app's wiring

    @Test("the app's recordings client names this phone by its device id and keeps its PTT keepalives going for its voice message")
    func theAppWiresThePtt() async {
        let app = AppModel()
        let transmit = app.main.transmit
        transmit.thisDeviceId = "phone-7"
        let ptt = transmit.controller
        await ptt.linkChanged(up: true)
        app.mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        app.mirror.apply(.capabilities(LinkMessage.Capabilities(properties: [
            .init(ordinal: 0, name: RecordingsClient.capabilityName, value: .i64(RecordingsClient.voiceKeyerVersion)),
        ])))
        app.recordings.handle(.stateChanged(.receivingSnapshot))
        app.recordings.handle(.stateChanged(.ready))
        let sending: [String: Any] = ["n": 1, "positionMs": 500, "lengthMs": 4_000, "waitingMs": 0,
                                      "by": "Shack phone", "deviceId": "phone-7"]
        Self.publish(Self.document(sending: sending), mirror: app.mirror, client: app.recordings, revision: 1)
        await app.recordings.voiceMessageSinkSettled()
        #expect(app.recordings.voiceMessageSending)
        #expect(await ptt.snapshot.voiceMessageSending)
        #expect(await ptt.snapshot.keepaliveRunning)

        // Another device's message is not this phone's to keep going.
        var other = sending
        other["deviceId"] = "shack-desktop"
        Self.publish(Self.document(sending: other), mirror: app.mirror, client: app.recordings, revision: 2)
        await app.recordings.voiceMessageSinkSettled()
        #expect(await ptt.snapshot.voiceMessageSending == false)
        #expect(app.recordingFlag.client === app.recordings)
        await ptt.linkChanged(up: false)
    }
}
