// NereusSDR for iOS: the voice keyer page, VOICE and PTT against a recordings client fed by hand
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import Testing

/// Recording plan Task 18 (design sections 3, 6.6, 6.8, V9, A8, A9): a tap
/// sends a message, and only a tap; VOICE, the page's Stop and PTT stop
/// this phone's message, PTT keying nothing; another station holding
/// transmit dims the messages with the Core's words and Take transmit; a
/// message on the air refuses clear and import; a clip from this phone's
/// microphone loads as the import verbs; the microphone is asked for only
/// at the first Record; and the app keeps its keepalives going while the
/// Core sends the message this page sent.
@Suite("Voice keyer", .timeLimit(.minutes(1)))
@MainActor
struct VoiceKeyerTests {
    typealias Flag = RecordingFlagTests

    /// The microphone question, counted.
    final class Access: MicrophoneAccess {
        var asking = true
        var allows = true
        private(set) var asks = 0

        var needsAsking: Bool { asking }

        func ask() async -> Bool {
            asks += 1
            asking = false
            return allows
        }
    }

    /// A microphone that writes a short WAV-shaped file and records no sound.
    final class Capture: MicClipCapture {
        var starts = true
        var running = false
        var written: TimeInterval = 0
        var level: Float = 0.3
        private(set) var urls: [URL] = []

        func begin(url: URL, limit: TimeInterval) -> Bool {
            guard starts else {
                return false
            }
            FileManager.default.createFile(atPath: url.path, contents: Data(repeating: 0, count: 4_000))
            urls.append(url)
            running = true
            return true
        }

        func end() { running = false }
        var isRecording: Bool { running }
        var seconds: TimeInterval { written }
        func peak() -> Float { running ? level : 0 }
    }

    struct Rig {
        let sent: Flag.Sent
        let mirror: MirrorStore
        let commands: CommandClient
        let client: RecordingsClient
        let model: VoiceKeyerModel
        let access: Access
        let capture: Capture
    }

    /// A connected Core at `version`; `client` builds the recordings client, the app's way when given.
    static func rig(version: Int64? = RecordingsClient.voiceKeyerVersion, connected: Bool = true,
                    client make: ((MirrorStore, CommandClient) -> RecordingsClient)? = nil) async -> Rig {
        let sent = Flag.Sent()
        let mirror = MirrorStore(send: { sent.add($0) })
        let commands = CommandClient(send: { sent.add($0) })
        mirror.apply(.hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
        var capabilities: [LinkMessage.PropertyEntry] = [.init(ordinal: 0, name: "stationName", value: .utf8(""))]
        if let version {
            capabilities.append(.init(ordinal: 1, name: RecordingsClient.capabilityName, value: .i64(version)))
        }
        mirror.apply(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        let client = make?(mirror, commands) ?? RecordingsClient(mirror: mirror, commands: commands)
        if connected {
            await commands.handle(.stateChanged(.receivingSnapshot))
            client.handle(.stateChanged(.receivingSnapshot))
            await commands.handle(.stateChanged(.ready))
            client.handle(.stateChanged(.ready))
        }
        let access = Access()
        let capture = Capture()
        let folder = FileManager.default.temporaryDirectory
            .appendingPathComponent("VoiceKeyerTests-\(UUID().uuidString)", isDirectory: true)
        let recorder = PhoneMicClipRecorder(access: access, permitted: { true }, capture: capture,
                                            folder: folder.appendingPathComponent("clips", isDirectory: true))
        let model = VoiceKeyerModel(client: client, recorder: recorder,
                                    folder: folder.appendingPathComponent("files", isDirectory: true))
        return Rig(sent: sent, mirror: mirror, commands: commands, client: client, model: model, access: access,
                   capture: capture)
    }

    /// The Core's document: M1 "CQ", M2 "CQ contest", M4 "73", the rest
    /// empty; `sending` the message going out, if any.
    static func document(sending: [String: Any]? = nil, repeat: Bool = false, delay: Int = 5) -> [String: Any] {
        var document = Flag.document()
        var slots: [[String: Any]] = (1...8).map { ["n": $0, "name": "", "lengthMs": 0, "empty": true] }
        slots[0] = ["n": 1, "name": "CQ", "lengthMs": 4_000, "empty": false]
        slots[1] = ["n": 2, "name": "CQ contest", "lengthMs": 6_000, "empty": false]
        slots[3] = ["n": 4, "name": "73", "lengthMs": 2_500, "empty": false]
        document["voiceKeyer"] = ["slots": slots, "repeat": `repeat`, "repeatDelaySec": delay,
                                  "sending": sending.map { $0 as Any } ?? NSNull()]
        return document
    }

    static func sending(_ n: Int, deviceId: String, by: String = "This iPhone", waitingMs: Int = 0) -> [String: Any] {
        ["n": n, "positionMs": 1_000, "lengthMs": 6_000, "waitingMs": waitingMs, "by": by, "deviceId": deviceId]
    }

    static func publish(_ rig: Rig, _ document: [String: Any], revision: Int64) {
        Flag.publish(document, mirror: rig.mirror, client: rig.client, revision: revision)
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

    /// Sends Mn from the model, the Core answering that it came from `wireId`.
    static func send(_ rig: Rig, _ n: Int, at index: Int, wireId: String = "wire-7") async -> VoiceKeyerModel.TapResult {
        let tap = Task { await rig.model.tapMessage(n) }
        await answer(rig, index, values: [.init(name: "deviceId", value: .utf8(wireId))])
        return await tap.value
    }

    // MARK: Sending

    @Test("a tap on a message sends it as voiceKeyer.send, and opening the page sends nothing")
    func tapSends() async {
        let rig = await Self.rig()
        Self.publish(rig, Self.document(), revision: 1)
        #expect(rig.model.reason == nil)
        #expect(rig.model.slots.map(\.empty) == [false, false, true, false, true, true, true, true])
        #expect(rig.model.label(2) == "CQ contest" && rig.model.label(3) == "M3")
        #expect(rig.model.rowDetail(2, progress: nil) == "0:06")
        #expect(rig.sent.invokes.isEmpty)
        #expect(await Self.send(rig, 2, at: 0) == .sent)
        #expect(rig.sent.invokes.map(\.verb) == ["voiceKeyer.send"])
        #expect(rig.sent.invokes[0].args == [.init(name: "n", value: .i64(2))])
    }

    @Test("an empty message points Record at it and sends nothing")
    func emptyPointsRecord() async {
        let rig = await Self.rig()
        Self.publish(rig, Self.document(), revision: 1)
        #expect(await rig.model.tapMessage(3) == .empty)
        #expect(rig.model.recordInto == 3)
        #expect(rig.model.note == .init(text: VoiceKeyerModel.emptyText(3), refusal: false))
        #expect(rig.model.rowDetail(3, progress: nil) == "Tap to record or import")
        #expect(rig.model.importDetail == "Fills the first empty message, M3")
        #expect(rig.sent.invokes.isEmpty)
    }

    @Test("VOICE, the tapped message and the page's Stop each stop this phone's message as voiceKeyer.stop")
    func voiceStops() async {
        let rig = await Self.rig()
        Self.publish(rig, Self.document(), revision: 1)
        #expect(await Self.send(rig, 2, at: 0) == .sent)
        Self.publish(rig, Self.document(sending: Self.sending(2, deviceId: "wire-7")), revision: 2)
        #expect(rig.client.voiceMessageSending)
        let progress = rig.model.progress()
        #expect(progress?.n == 2 && progress?.mine == true && progress?.waiting == false)
        #expect(rig.model.airText(progress!).title == "On the air: M2 CQ contest")

        // VOICE.
        let voice = Task { await rig.model.voiceTapped() }
        await Self.answer(rig, 1)
        #expect(await voice.value)
        // The message itself, tapped again.
        let again = Task { await rig.model.tapMessage(2) }
        await Self.answer(rig, 2)
        #expect(await again.value == .stopped)
        // The page's Stop.
        let stop = Task { await rig.model.stopTapped() }
        await Self.answer(rig, 3)
        await stop.value
        #expect(rig.sent.invokes.map(\.verb) == ["voiceKeyer.send", "voiceKeyer.stop", "voiceKeyer.stop",
                                                 "voiceKeyer.stop"])
        #expect(rig.sent.invokes[1].args.isEmpty)
    }

    @Test("PTT tapped while the Core sends this phone's message stops it and keys nothing")
    func pttStops() async {
        let rig = await Self.rig()
        let app = AppModel()
        let transmit = app.main.transmit
        AppModel.wireVoiceKeyer(rig.model, transmit: transmit)
        // The wiring carries transmit's own refusal: with no Core, none is offered here.
        #expect(rig.model.held != nil)
        // Transmit offered to this phone, as a connected Core would.
        rig.model.held = nil
        Self.publish(rig, Self.document(), revision: 1)
        // Before a message goes out, PTT is the PTT's own: the voice keyer leaves it alone.
        #expect(rig.model.pttTapped() == false)
        #expect(await Self.send(rig, 1, at: 0) == .sent)
        Self.publish(rig, Self.document(sending: Self.sending(1, deviceId: "wire-7")), revision: 2)
        #expect(rig.client.voiceMessageSending)

        transmit.tapPtt()
        await Self.answer(rig, 1)
        #expect(rig.sent.invokes.map(\.verb) == ["voiceKeyer.send", "voiceKeyer.stop"])
        // Nothing keyed: the PTT never left Tap, and it shows no refusal.
        #expect(await transmit.controller.state == .idle)
        #expect(transmit.heldNote == nil)

        // Another device's message is not this phone's to stop with PTT.
        Self.publish(rig, Self.document(sending: Self.sending(1, deviceId: "shack-desktop", by: "Desktop")),
                     revision: 3)
        #expect(rig.client.voiceMessageSending == false)
        #expect(rig.model.pttTapped() == false)
    }

    @Test("another station's transmit dims the messages with its words and Take transmit, and keys nothing")
    func heldElsewhere() async {
        let rig = await Self.rig()
        Self.publish(rig, Self.document(), revision: 1)
        let words = "The Shack Mac has the transmitter. Take it to transmit from here."
        rig.model.held = TxRefusalInfo(reason: words, code: "otherDeviceHolds", fix: TxRefusalInfo.takeTransmit)
        #expect(rig.model.sendReason == words && rig.model.offersTake)
        #expect(await rig.model.tapMessage(1) == .refused)
        #expect(rig.model.note == .init(text: words, refusal: true))
        #expect(rig.sent.invokes.isEmpty)
        // Listening still works.
        let listen = Task { await rig.model.listen(1) }
        await Self.answer(rig, 0)
        await listen.value
        #expect(rig.sent.invokes.map(\.verb) == ["voiceKeyer.listen"])

        // Another device's message going out: its Stop is refused by the Core (ruling 8.5), shown as said.
        rig.model.held = nil
        Self.publish(rig, Self.document(sending: Self.sending(2, deviceId: "shack-desktop", by: "Desktop")),
                     revision: 2)
        let progress = rig.model.progress()
        #expect(progress?.mine == false)
        #expect(rig.model.airText(progress!).title == "On the air from Desktop: M2 CQ contest")
        let ruling = "Desktop has the transmitter. Take it to stop the transmission."
        let stop = Task { await rig.model.stop() }
        await Self.answer(rig, 1, accepted: false, reason: ruling)
        #expect(await stop.value == false)
        #expect(rig.model.note == .init(text: ruling, refusal: true))
        // A second send while it goes out is refused in the Core's words.
        let send = Task { await rig.model.tapMessage(1) }
        await Self.answer(rig, 2, accepted: false, reason: "The radio is already transmitting.")
        #expect(await send.value == .refused)
        #expect(rig.model.note == .init(text: "The radio is already transmitting.", refusal: true))
    }

    @Test("a message on the air, or waiting to repeat, refuses clear, import and recording into it")
    func onAirRefuses() async {
        let rig = await Self.rig()
        Self.publish(rig, Self.document(sending: Self.sending(1, deviceId: "wire-7", waitingMs: 4_000), repeat: true),
                     revision: 1)
        let progress = rig.model.progress()
        #expect(progress?.waiting == true && progress?.waitSeconds == 4)
        #expect(rig.model.rowDetail(1, progress: progress) == "Sends again in 4 s")
        rig.model.askClear(1)
        #expect(rig.model.pendingClear == nil && rig.model.note?.text == "M1 is on the air.")
        rig.model.askImport(1)
        #expect(rig.model.importing == nil && rig.model.note?.text == "M1 is on the air.")
        rig.model.recordInto = 1
        await rig.model.record()
        #expect(!rig.model.recorder.isRecording && rig.access.asks == 0)
        #expect(rig.sent.invokes.isEmpty)
        // Another message clears after its confirm.
        rig.model.askClear(4)
        #expect(rig.model.pendingClear == 4)
        let clear = Task { await rig.model.confirmClear() }
        await Self.answer(rig, 0)
        await clear.value
        #expect(rig.sent.invokes.map(\.verb) == ["voiceKeyer.clear"])
        #expect(rig.sent.invokes[0].args == [.init(name: "n", value: .i64(4))])
    }

    @Test("an older Core or none leaves everything greyed with the reason and sends nothing")
    func olderCore() async {
        let older = await Self.rig(version: RecordingsClient.onAirVersion)
        #expect(older.model.reason == RecordingsClient.updateReason)
        #expect(older.model.sendReason == RecordingsClient.updateReason)
        #expect(await older.model.tapMessage(1) == .refused)
        #expect(await older.model.voiceTapped() == false)
        await older.model.record()
        #expect(older.access.asks == 0 && !older.model.recorder.isRecording)
        #expect(older.sent.invokes.isEmpty)
        let away = await Self.rig(connected: false)
        #expect(away.model.reason == RecordingFlagModel.notConnectedReason)
        #expect(away.model.slots.allSatisfy { $0.empty })
    }

    // MARK: Recording a message

    @Test("the microphone is asked for only at the first Record; the clip loads into the message as the import verbs")
    func recordAsksOnceAndLoads() async throws {
        let rig = await Self.rig()
        Self.publish(rig, Self.document(), revision: 1)
        // Opening the page and choosing where to record ask nothing.
        rig.model.recordInto = 3
        #expect(rig.access.asks == 0)

        await rig.model.record()
        #expect(rig.access.asks == 1)
        #expect(rig.model.recorder.isRecording && rig.model.recorder.slot == 3)
        rig.capture.written = 3.24
        await rig.model.tick()
        #expect(rig.model.recorder.elapsedMs == 3_240 && rig.model.recorder.level > 0)
        let url = try #require(rig.capture.urls.first)

        let stop = Task { await rig.model.stopTapped() }
        await Self.answer(rig, 0)
        await Self.answer(rig, 1)
        await Self.answer(rig, 2)
        await stop.value
        #expect(rig.sent.invokes.map(\.verb) == ["voiceKeyer.beginImport", "voiceKeyer.chunk",
                                                 "voiceKeyer.finishImport"])
        #expect(rig.sent.invokes[0].args == [.init(name: "n", value: .i64(3)), .init(name: "name", value: .utf8("")),
                                             .init(name: "size", value: .i64(4_000))])
        #expect(rig.model.note == .init(text: "Saved M3.", refusal: false))
        // The clip on this phone goes once loaded; Record moves on to the next empty message.
        #expect(!FileManager.default.fileExists(atPath: url.path))
        #expect(rig.model.recordInto == 5)

        // The second Record asks nothing: iOS has its answer.
        await rig.model.record()
        #expect(rig.access.asks == 1 && rig.model.recorder.isRecording)
        rig.model.recorder.cancel()
    }

    @Test("a clip is 48 kHz mono 16-bit and stops by itself at two minutes, then loads")
    func clipStopsAtTwoMinutes() async {
        #expect(PhoneMicClipRecorder.sampleRate == 48_000)
        #expect(PhoneMicClipRecorder.channels == 1 && PhoneMicClipRecorder.bitDepth == 16)
        #expect(PhoneMicClipRecorder.limitSeconds == 120)
        let rig = await Self.rig()
        Self.publish(rig, Self.document(), revision: 1)
        rig.model.recordInto = 5
        await rig.model.record()
        rig.capture.written = 120
        rig.capture.running = false
        let tick = Task { await rig.model.tick() }
        await Self.answer(rig, 0)
        await Self.answer(rig, 1)
        await Self.answer(rig, 2)
        await tick.value
        #expect(!rig.model.recorder.isRecording)
        #expect(rig.sent.invokes.first?.verb == "voiceKeyer.beginImport")
        #expect(rig.model.note?.text == "Saved M5.")
    }

    @Test("a microphone that is not allowed says where to allow it, and records nothing")
    func microphoneNotAllowed() async {
        let rig = await Self.rig()
        Self.publish(rig, Self.document(), revision: 1)
        rig.access.allows = false
        await rig.model.record()
        #expect(rig.access.asks == 1 && !rig.model.recorder.isRecording)
        #expect(rig.model.note == .init(text: PhoneMicClipRecorder.notAllowedText, refusal: true))
        #expect(rig.capture.urls.isEmpty)
    }

    // MARK: Import, rename, repeat

    @Test("Import WAV fills the first empty message, and a full set says so")
    func importFirstEmpty() async throws {
        let rig = await Self.rig()
        Self.publish(rig, Self.document(), revision: 1)
        rig.model.importFirstEmpty()
        #expect(rig.model.importing == 3)
        rig.model.importing = nil
        let file = FileManager.default.temporaryDirectory.appendingPathComponent("cq-field-day-\(UUID().uuidString).wav")
        try Data(repeating: 1, count: 1_000).write(to: file)
        defer { try? FileManager.default.removeItem(at: file) }
        let load = Task { await rig.model.importFile(file, into: 3) }
        await Self.answer(rig, 0)
        await Self.answer(rig, 1)
        await Self.answer(rig, 2)
        await load.value
        #expect(rig.sent.invokes.map(\.verb) == ["voiceKeyer.beginImport", "voiceKeyer.chunk",
                                                 "voiceKeyer.finishImport"])
        // An empty message takes the file's name.
        let name = file.deletingPathExtension().lastPathComponent
        #expect(rig.sent.invokes[0].args[1] == .init(name: "name", value: .utf8(String(name.prefix(32)))))
        #expect(rig.model.note?.text == "Saved M3.")

        var full = Self.document()
        full["voiceKeyer"] = ["slots": (1...8).map { ["n": $0, "name": "M", "lengthMs": 1_000, "empty": false] },
                              "repeat": false, "repeatDelaySec": 5, "sending": NSNull()]
        Self.publish(rig, full, revision: 2)
        rig.model.importFirstEmpty()
        #expect(rig.model.importing == nil && rig.model.note?.text == VoiceKeyerModel.allFullText)
    }

    @Test("Rename sends at most 32 characters; Repeat and its wait go as voiceKeyer.setRepeat, 1 to 60 s")
    func renameAndRepeat() async {
        let rig = await Self.rig()
        Self.publish(rig, Self.document(), revision: 1)
        rig.model.startRename(2)
        #expect(rig.model.renaming == 2 && rig.model.renameText == "CQ contest")
        rig.model.renameText = String(repeating: "x", count: 40)
        let rename = Task { await rig.model.commitRename() }
        await Self.answer(rig, 0)
        await rename.value
        #expect(rig.sent.invokes[0].verb == "voiceKeyer.rename")
        #expect(rig.sent.invokes[0].args == [.init(name: "n", value: .i64(2)),
                                             .init(name: "name", value: .utf8(String(repeating: "x", count: 32)))])

        let on = Task { await rig.model.setRepeat(true) }
        await Self.answer(rig, 1)
        await on.value
        #expect(rig.sent.invokes[1].args == [.init(name: "on", value: .bool(true)), .init(name: "delaySec", value: .i64(5))])
        Self.publish(rig, Self.document(repeat: true, delay: 60), revision: 2)
        #expect(rig.model.repeatSummary == "Sends again every 60 s until you stop it")
        // Past 60 s nothing more is sent.
        await rig.model.setDelay(61)
        #expect(rig.sent.invokes.count == 2)
        let shorter = Task { await rig.model.setDelay(59) }
        await Self.answer(rig, 2)
        await shorter.value
        #expect(rig.sent.invokes[2].args == [.init(name: "on", value: .bool(true)), .init(name: "delaySec", value: .i64(59))])
    }

    // MARK: The app's wiring

    @Test("a message sent from this page keeps the PTT's keepalives going while the Core sends it, and they stop with it")
    func sentHereKeepsTheKeepalive() async {
        let app = AppModel()
        let transmit = app.main.transmit
        let ptt = transmit.controller
        await ptt.linkChanged(up: true)
        // The app's own wiring of the client to this phone's PTT; this phone's id is not known, so
        // the Core's answer to the send names it.
        let rig = await Self.rig { mirror, commands in
            AppModel.recordingsClient(mirror: mirror, commands: commands, transmit: transmit)
        }
        Self.publish(rig, Self.document(), revision: 1)
        #expect(await Self.send(rig, 2, at: 0, wireId: "wire-phone") == .sent)
        Self.publish(rig, Self.document(sending: Self.sending(2, deviceId: "wire-phone")), revision: 2)
        await rig.client.voiceMessageSinkSettled()
        #expect(rig.client.voiceMessageSending)
        #expect(await ptt.snapshot.voiceMessageSending)
        #expect(await ptt.snapshot.keepaliveRunning)
        // Between repeats it is still this phone's message.
        Self.publish(rig, Self.document(sending: Self.sending(2, deviceId: "wire-phone", waitingMs: 3_000)),
                     revision: 3)
        await rig.client.voiceMessageSinkSettled()
        #expect(await ptt.snapshot.keepaliveRunning)

        // The message ends: the keepalives stop.
        Self.publish(rig, Self.document(), revision: 4)
        await rig.client.voiceMessageSinkSettled()
        #expect(rig.client.voiceMessageSending == false)
        #expect(await ptt.snapshot.voiceMessageSending == false)
        #expect(await ptt.snapshot.keepaliveRunning == false)
        await ptt.linkChanged(up: false)
    }
}
