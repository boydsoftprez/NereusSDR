// NereusSDR for iOS: the recordings client: its verbs as the Core's traces send them, the gates, the copies and whose message is on the air
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink
import NereusModels
import Testing
@testable import NereusMirror

/// Recording plan Task 15 (design section 6.9, V9, A15): the phone's
/// recordings and voice keyer against the Core's own traces,
/// tests/data/link/v1/sessions/verbs-recordings.json and
/// verbs-voice-keyer.json, which the desktop's tests share. Task 28 adds
/// the Digital Spectrum Recorder's traces: dsr-rewind.json, dsr-past.json,
/// dsr-save.json and recordings-dsr-settings.json.
@MainActor
@Suite struct RecordingsClientTests {
    private let sent = SentMessages()

    /// What the voice message sink was told, in order.
    final class Told: @unchecked Sendable {
        private let lock = NSLock()
        private var values: [Bool] = []
        var all: [Bool] { lock.withLock { values } }
        func add(_ value: Bool) { lock.withLock { values.append(value) } }
    }

    private func rig(version: Int64?, thisDeviceId: String? = nil, told: Told? = nil)
        async -> (RecordingsClient, MirrorStore, CommandClient) {
        let mirror = MirrorStore(send: sent.sender)
        let commands = CommandClient(send: sent.sender)
        mirror.apply(FixtureReplay.stationHello())
        var capabilities: [String: LinkMessage.PropertyValue] = ["stationName": .utf8("")]
        if let version {
            capabilities[RecordingsClient.capabilityName] = .i64(version)
        }
        mirror.apply(FixtureReplay.capabilities(capabilities))
        var sink: (@Sendable (Bool) async -> Void)?
        if let told {
            sink = { @Sendable value in told.add(value) }
        }
        let client = RecordingsClient(mirror: mirror, commands: commands, thisDeviceId: { thisDeviceId },
                                      voiceMessageSending: sink)
        await commands.handle(.stateChanged(.receivingSnapshot))
        client.handle(.stateChanged(.receivingSnapshot))
        await commands.handle(.stateChanged(.ready))
        client.handle(.stateChanged(.ready))
        return (client, mirror, commands)
    }

    private func invokes() -> [LinkMessage.CommandInvoke] {
        sent.messages.compactMap {
            if case .commandInvoke(let invoke) = $0 {
                return invoke
            }
            return nil
        }
    }

    /// Answers the `index`th command sent, once it has gone.
    private func answer(_ index: Int, _ commands: CommandClient, accepted: Bool = true, reason: String = "",
                        values: [LinkMessage.PropertyEntry]? = nil) async {
        #expect(await sent.settle(untilCount: index + 1))
        guard index < invokes().count else {
            return
        }
        let invoke = invokes()[index]
        await commands.receive(.commandResult(LinkMessage.CommandResult(
            verb: invoke.verb, id: invoke.id, accepted: accepted, reason: reason,
            affected: accepted ? ["recordings"] : [], values: values)))
    }

    // MARK: The Core's traces

    /// One client message the trace expects and the station's answer to it.
    private struct Exchange {
        let invoke: [String: Any]
        let reply: [String: Any]
    }

    private static func exchanges(_ file: String) throws -> [Exchange] {
        let fixture = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("sessions/\(file)"))
        let steps = try #require(fixture["steps"] as? [[String: Any]])
        var out: [Exchange] = []
        for (index, step) in steps.enumerated() {
            // The trace's malformed requests are the station's tests, not the app's.
            guard step["from"] as? String == "client", step["role"] as? String == "behaviour",
                  let message = step["message"] as? [String: Any], message["type"] as? String == "command.invoke",
                  index + 1 < steps.count, let reply = steps[index + 1]["message"] as? [String: Any] else {
                continue
            }
            out.append(Exchange(invoke: message, reply: reply))
        }
        return out
    }

    /// The text the trace's invoke stands for with the id the client chose.
    private static func expected(_ exchange: Exchange, id: UInt32) throws -> String {
        var message = exchange.invoke
        message["id"] = Int(id)
        return try LinkJSON(foundation: message).compactText
    }

    private static func reply(_ exchange: Exchange, id: UInt32) throws -> LinkMessage {
        var message = exchange.reply
        message["id"] = Int(id)
        if message["values"] as? String == "$any", message["verb"] as? String == "dsr.hours" {
            // The trace leaves the hours open; none kept stands for them.
            message["values"] = [["kind": "utf8", "name": "starts", "ordinal": 0, "value": ""],
                                 ["kind": "utf8", "name": "activity", "ordinal": 0, "value": "[]"]]
        } else if message["values"] as? String == "$any" {
            // The trace leaves the list's page open; an empty one stands for it.
            message["values"] = [["kind": "utf8", "name": "entries", "ordinal": 0, "value": "[]"],
                                 ["kind": "i64", "name": "offset", "ordinal": 0, "value": 0],
                                 ["kind": "i64", "name": "total", "ordinal": 0, "value": 0]]
        }
        return try LinkCodec.decode(try LinkJSON(foundation: message).compactText)
    }

    private static func int(_ args: [[String: Any]], _ name: String) -> Int {
        (args.first { $0["name"] as? String == name }?["value"] as? NSNumber)?.intValue ?? -1
    }

    private static func text(_ args: [[String: Any]], _ name: String) -> String {
        args.first { $0["name"] as? String == name }?["value"] as? String ?? ""
    }

    /// The client call each trace step stands for; the voice message import
    /// is one call that sends three verbs.
    private func call(_ client: RecordingsClient, _ verb: String, _ args: [[String: Any]], file: URL) async throws {
        switch verb {
        case "recordings.start": try await client.start(slice: Self.int(args, "slice"))
        case "recordings.stop": try await client.stop(slice: Self.int(args, "slice"))
        case "recordings.list":
            let page = try await client.list(offset: Self.int(args, "offset"), count: Self.int(args, "count"))
            #expect(page == RecordingsPage(entries: [], offset: 0, total: 0, reader: nil))
        case "recordings.play": try await client.play(id: Self.text(args, "id"))
        case "recordings.stopPlay": try await client.stopPlay()
        case "recordings.seek": try await client.seek(ms: Int64(Self.int(args, "positionMs")))
        case "recordings.rename": try await client.rename(id: Self.text(args, "id"), name: Self.text(args, "name"))
        case "recordings.delete":
            let ids = try JSONDecoder().decode([String].self, from: Data(Self.text(args, "ids").utf8))
            try await client.delete(ids: ids)
        case "recordings.export": try await client.download(id: Self.text(args, "id"), to: file)
        case "recordings.setSettings":
            if args.contains(where: { $0["name"] as? String == "dsrSizeLimitGb" }) {
                try await client.setSettings(RecordingsSettingsChange(dsrSizeLimitGb: Self.int(args, "dsrSizeLimitGb")))
            } else if args.contains(where: { $0["name"] as? String == "dsrDisk" }) {
                try await client.setSettings(RecordingsSettingsChange(dsrDisk: Self.text(args, "dsrDisk")))
            } else {
                try await client.setSettings(RecordingsSettingsChange(quietMinutes: Self.int(args, "quietMinutes")))
            }
        case "dsr.rewind":
            try await client.rewind(slice: Self.int(args, "slice"), utcNs: Int64(Self.int(args, "utcNs")),
                                    frequencyHz: Int64(Self.int(args, "frequencyHz")))
        case "dsr.seek": try await client.seekPast(slice: Self.int(args, "slice"), utcNs: Int64(Self.int(args, "utcNs")))
        case "dsr.live": try await client.live(slice: Self.int(args, "slice"))
        case "dsr.rows":
            try await client.pastRows(slice: Self.int(args, "slice"), view: Self.view(args),
                                      fromUtcNs: Int64(Self.int(args, "fromUtcNs")),
                                      toUtcNs: Int64(Self.int(args, "toUtcNs")), rowCount: Self.int(args, "rowCount")) { _ in }
        case "dsr.spectrum":
            _ = try await client.pastSpectrum(slice: Self.int(args, "slice"), view: Self.view(args),
                                              utcNs: Int64(Self.int(args, "utcNs")))
        case "dsr.hours": #expect(try await client.hours().isEmpty)
        case "dsr.saveStretch":
            _ = try await client.saveStretch(fromUtcNs: Int64(Self.int(args, "fromUtcNs")),
                                             toUtcNs: Int64(Self.int(args, "toUtcNs")),
                                             kind: try #require(DsrStretchKind(rawValue: Self.text(args, "kind"))))
        case "recordings.saveSoundOnly": _ = try await client.saveSoundOnly(id: Self.text(args, "id"))
        case "recordings.openStretch": try await client.openStretch(id: Self.text(args, "id"))
        case "voiceKeyer.send": try await client.sendVoice(Self.int(args, "n"))
        case "voiceKeyer.stop": try await client.stopVoice()
        case "voiceKeyer.beginImport":
            // The trace's four bytes, "AAAAAA==".
            try Data(count: Self.int(args, "size")).write(to: file)
            let imported = args.first { $0["name"] as? String == "imported" }?["value"] as? Bool ?? false
            try await client.importVoice(Self.int(args, "n"), url: file, name: Self.text(args, "name"),
                                         imported: imported)
        case "voiceKeyer.export": try await client.exportVoice(Self.int(args, "n"), to: file)
        case "voiceKeyer.rename": try await client.renameVoice(Self.int(args, "n"), name: Self.text(args, "name"))
        case "voiceKeyer.clear": try await client.clearVoice(Self.int(args, "n"))
        case "voiceKeyer.setRepeat":
            let on = args.first { $0["name"] as? String == "on" }?["value"] as? Bool ?? false
            try await client.setVoiceRepeat(on: on, delaySec: Self.int(args, "delaySec"))
        case "voiceKeyer.listen": try await client.listenVoice(Self.int(args, "n"))
        default: Issue.record("no call for \(verb)")
        }
    }

    private static func view(_ args: [[String: Any]]) -> DsrPanView {
        DsrPanView(fftSize: int(args, "fftSize"), windowType: int(args, "windowType"),
                   centreHz: Int64(int(args, "centreHz")), spanHz: Int64(int(args, "spanHz")), pixels: int(args, "pixels"))
    }

    /// Each verb the client sends is the trace's, byte for byte (ids aside),
    /// and each answer in the trace comes back as the trace has it: done, or
    /// refused in the Core's words.
    @Test(arguments: ["verbs-recordings.json", "verbs-voice-keyer.json", "dsr-rewind.json", "dsr-past.json",
                      "dsr-save.json", "recordings-dsr-settings.json"])
    func theCoresTracesAreSentAndReadAsTheyStand(_ file: String) async throws {
        let (client, _, commands) = await rig(version: 4)
        let exchanges = try Self.exchanges(file)
        #expect(!exchanges.isEmpty)
        let scratch = FileManager.default.temporaryDirectory.appendingPathComponent("recordings-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: scratch) }
        var next = 0
        while next < exchanges.count {
            let first = exchanges[next]
            let verb = try #require(first.invoke["verb"] as? String)
            let args = first.invoke["args"] as? [[String: Any]] ?? []
            let task = Task { () -> String? in
                do {
                    try await call(client, verb, args, file: scratch)
                    return nil
                } catch let error as RecordingsError {
                    return error.reason ?? "\(error)"
                } catch {
                    return "\(error)"
                }
            }
            // Every verb this call sends, each answered as the trace answers it.
            var refusal: String?
            repeat {
                let exchange = exchanges[next]
                #expect(await sent.settle(untilCount: next + 1), "\(file): \(exchange.invoke["verb"] ?? "")")
                let invoke = try #require(invokes().last)
                #expect(LinkCodec.encode(.commandInvoke(invoke)) == (try Self.expected(exchange, id: invoke.id)))
                let reply = try Self.reply(exchange, id: invoke.id)
                await commands.receive(reply)
                client.handle(.message(reply))
                next += 1
                if case .commandResult(let result) = reply, !result.accepted {
                    refusal = result.reason
                }
                // The import goes on to its next verb only once this one is answered.
                if verb == "voiceKeyer.beginImport", refusal == nil, next < exchanges.count,
                   ["voiceKeyer.chunk", "voiceKeyer.finishImport"].contains(exchanges[next].invoke["verb"] as? String) {
                    continue
                }
                break
            } while true
            #expect(await task.value == refusal, "\(file): \(verb)")
        }
        #expect(invokes().count == exchanges.count)
        // A refused copy leaves nothing behind.
        #expect(!FileManager.default.fileExists(atPath: scratch.path))
    }

    // MARK: V9

    @Test func withoutTheCapabilityEveryCallIsRefusedUnsent() async throws {
        for version in [nil, Int64(0)] {
            let (client, _, _) = await rig(version: version)
            #expect(client.state == .needsCoreUpdate)
            #expect(client.unavailableReason(needs: 1) == "This Core needs an update for recordings.")
            let file = FileManager.default.temporaryDirectory.appendingPathComponent("v9-\(UUID().uuidString)")
            let calls: [() async throws -> Void] = [
                { try await client.start(slice: 0) }, { try await client.stop(slice: 0) },
                { _ = try await client.list(offset: 0, count: 10) }, { try await client.play(id: "r") },
                { try await client.stopPlay() }, { try await client.seek(ms: 0) },
                { try await client.rename(id: "r", name: "n") }, { try await client.delete(ids: ["r"]) },
                { try await client.download(id: "r", to: file) },
                { try await client.setSettings(RecordingsSettingsChange(autoStop: true)) },
                { try await client.sendVoice(1) }, { try await client.stopVoice() },
                { try await client.importVoice(1, url: file) }, { try await client.exportVoice(1, to: file) },
                { try await client.renameVoice(1, name: "CQ") }, { try await client.clearVoice(1) },
                { try await client.setVoiceRepeat(on: true, delaySec: 5) }, { try await client.listenVoice(1) },
            ]
            for call in calls {
                await #expect(throws: RecordingsError.needsCoreUpdate) { try await call() }
            }
            #expect(RecordingsError.needsCoreUpdate.reason == RecordingsClient.updateReason)
            #expect(!FileManager.default.fileExists(atPath: file.path))
        }
        #expect(sent.count == 0)
    }

    /// Version 1 has recordings but no on-air switches and no voice keyer;
    /// version 2 adds the switches.
    @Test func anOlderCoreGreysWhatItLacks() async throws {
        let (first, _, commands) = await rig(version: 1)
        #expect(first.state == .ready)
        #expect(first.unavailableReason(needs: RecordingsClient.recordingsVersion) == nil)
        #expect(first.unavailableReason(needs: RecordingsClient.onAirVersion) == RecordingsClient.updateReason)
        #expect(first.unavailableReason(needs: RecordingsClient.voiceKeyerVersion) == RecordingsClient.updateReason)
        await #expect(throws: RecordingsError.needsCoreUpdate) { try await first.sendVoice(1) }
        await #expect(throws: RecordingsError.needsCoreUpdate) { try await first.stopVoice() }
        await #expect(throws: RecordingsError.needsCoreUpdate) { try await first.listenVoice(1) }
        await #expect(throws: RecordingsError.needsCoreUpdate) {
            try await first.setSettings(RecordingsSettingsChange(onAirUseTxEq: true))
        }
        #expect(sent.count == 0)
        // What version 1 has goes.
        let task = Task { try await first.stopPlay() }
        await answer(0, commands)
        try await task.value
        #expect(invokes().map(\.verb) == ["recordings.stopPlay"])

        let (second, _, otherCommands) = await rig(version: 2)
        #expect(second.unavailableReason(needs: RecordingsClient.onAirVersion) == nil)
        await #expect(throws: RecordingsError.needsCoreUpdate) { try await second.clearVoice(1) }
        let settings = Task { try await second.setSettings(RecordingsSettingsChange(onAirUseTxEq: true)) }
        await answer(1, otherCommands)
        try await settings.value
        #expect(invokes().last?.args == [.init(name: "onAirUseTxEq", value: .bool(true))])
    }

    @Test func withNoSessionNothingGoes() async {
        let mirror = MirrorStore(send: sent.sender)
        let client = RecordingsClient(mirror: mirror, commands: CommandClient(send: sent.sender))
        #expect(client.state == .offline)
        #expect(client.unavailableReason(needs: 1) == nil)
        await #expect(throws: RecordingsError.notConnected) { try await client.stopPlay() }
        #expect(sent.count == 0)
    }

    // MARK: Copies

    /// A file comes back whole from chunks of at most 192 KiB, written as
    /// they come, with progress after each.
    @Test func aDownloadReassemblesTheChunks() async throws {
        let (client, _, commands) = await rig(version: 3)
        var bytes = Data(count: 2 * RecordingsClient.chunkBytes + 1_234)
        for index in bytes.indices {
            bytes[index] = UInt8(truncatingIfNeeded: index &* 31 &+ 7)
        }
        let file = FileManager.default.temporaryDirectory.appendingPathComponent("download-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: file) }
        final class Progress: @unchecked Sendable {
            var seen: [Int64] = []
        }
        let progress = Progress()
        let task = Task {
            try await client.download(id: "r-7", to: file) { received, size in
                #expect(size == Int64(bytes.count))
                progress.seen.append(received)
            }
        }
        var offset = 0
        var index = 0
        while offset < bytes.count {
            #expect(await sent.settle(untilCount: index + 1))
            let invoke = invokes()[index]
            #expect(invoke.verb == "recordings.export")
            #expect(invoke.args == [.init(name: "id", value: .utf8("r-7")), .init(name: "offset", value: .i64(Int64(offset)))])
            let end = min(bytes.count, offset + RecordingsClient.chunkBytes)
            await answer(index, commands, values: [
                .init(name: "id", value: .utf8("r-7")), .init(name: "offset", value: .i64(Int64(offset))),
                .init(name: "size", value: .i64(Int64(bytes.count))),
                .init(name: "data", value: .utf8(bytes[offset..<end].base64EncodedString())),
            ])
            offset = end
            index += 1
        }
        try await task.value
        #expect(index == 3)
        #expect(try Data(contentsOf: file) == bytes)
        #expect(progress.seen == [Int64(RecordingsClient.chunkBytes), Int64(2 * RecordingsClient.chunkBytes),
                                  Int64(bytes.count)])
    }

    /// A voice message's export reassembles the same way, and a copy whose
    /// answer is not the chunk asked for fails and leaves no file.
    @Test func aVoiceExportReassemblesAndABadChunkLeavesNoFile() async throws {
        let (client, _, commands) = await rig(version: 3)
        let file = FileManager.default.temporaryDirectory.appendingPathComponent("voice-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: file) }
        let wav = Data("RIFF....WAVE".utf8)
        let good = Task { try await client.exportVoice(2, to: file) }
        await answer(0, commands, values: [.init(name: "n", value: .i64(2)), .init(name: "offset", value: .i64(0)),
                                           .init(name: "size", value: .i64(Int64(wav.count))),
                                           .init(name: "data", value: .utf8(wav.base64EncodedString()))])
        try await good.value
        #expect(invokes().first?.args == [.init(name: "n", value: .i64(2)), .init(name: "offset", value: .i64(0))])
        #expect(try Data(contentsOf: file) == wav)

        let bad = Task { try await client.exportVoice(2, to: file) }
        await answer(1, commands, values: [.init(name: "n", value: .i64(2)), .init(name: "offset", value: .i64(5)),
                                           .init(name: "size", value: .i64(Int64(wav.count))),
                                           .init(name: "data", value: .utf8(wav.base64EncodedString()))])
        await #expect(throws: RecordingsError.badAnswer) { try await bad.value }
        #expect(!FileManager.default.fileExists(atPath: file.path))
    }

    /// An import sends the file in chunks, each after the one before is answered.
    @Test func anImportSendsTheFileInOrderedChunks() async throws {
        let (client, _, commands) = await rig(version: 3)
        let file = FileManager.default.temporaryDirectory.appendingPathComponent("import-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: file) }
        let bytes = Data((0..<(RecordingsClient.chunkBytes + 10)).map { UInt8(truncatingIfNeeded: $0) })
        try bytes.write(to: file)
        let task = Task { try await client.importVoice(5, url: file) }
        await answer(0, commands)
        // The second chunk waits for the first's answer.
        #expect(await sent.settle(untilCount: 2))
        await Task.yield()
        #expect(sent.count == 2)
        await answer(1, commands)
        await answer(2, commands)
        await answer(3, commands)
        try await task.value
        let sentVerbs = invokes().map(\.verb)
        #expect(sentVerbs == ["voiceKeyer.beginImport", "voiceKeyer.chunk", "voiceKeyer.chunk", "voiceKeyer.finishImport"])
        #expect(invokes()[0].args == [.init(name: "n", value: .i64(5)), .init(name: "name", value: .utf8("")),
                                      .init(name: "size", value: .i64(Int64(bytes.count)))])
        var joined = Data()
        for invoke in invokes()[1...2] {
            guard case .utf8(let text)? = invoke.args.first?.value, let chunk = Data(base64Encoded: text) else {
                Issue.record("a chunk is not base64")
                return
            }
            #expect(chunk.count <= RecordingsClient.chunkBytes)
            joined.append(chunk)
        }
        #expect(joined == bytes)
    }

    // MARK: The document and this device's playback

    private func publish(_ mirror: MirrorStore, _ client: RecordingsClient, _ document: [String: Any],
                         revision: Int64, create: Bool = false) {
        let json = String(decoding: (try? JSONSerialization.data(withJSONObject: document)) ?? Data(), as: UTF8.self)
        let properties: [LinkMessage.PropertyEntry] = [.init(ordinal: 0, name: "json", value: .utf8(json)),
                                                       .init(ordinal: 1, name: "revision", value: .i64(revision))]
        let message: LinkMessage
        if create {
            mirror.apply(.schema(LinkMessage.Schema(className: "RecordingsFacade", fields: [
                .init(ordinal: 0, name: "json", kind: .utf8), .init(ordinal: 1, name: "revision", kind: .i64),
            ])))
            message = .objectCreate(LinkMessage.ObjectCreate(key: "recordings", className: "RecordingsFacade",
                                                             properties: properties))
        } else {
            message = .delta(LinkMessage.Delta(key: "recordings", properties: properties))
        }
        mirror.apply(message)
        client.handle(.message(message))
    }

    private static func document(sending: [String: Any]? = nil) -> [String: Any] {
        var slots: [[String: Any]] = (1...8).map { ["n": $0, "name": "", "lengthMs": 0, "empty": true] }
        slots[0] = ["n": 1, "name": "CQ", "lengthMs": 4_000, "empty": false]
        return [
            "version": 3,
            "slices": [["slice": 0, "state": "recording", "startUtc": "2026-11-16T14:02:03.000Z",
                        "startedBy": "Shack phone", "elapsedMs": 2_000, "myVoice": true]],
            "kept": ["seq": 0, "slice": -1, "id": "", "name": "", "lengthMs": 0, "reason": "", "device": ""],
            "index": ["revision": 2, "count": 3, "usedBytes": 1_000, "freeBytes": 9_000, "folder": "/r"],
            "settings": ["keepMyVoice": false, "sidesApart": false, "autoStart": false, "autoStop": false,
                         "quietMinutes": 5, "format": "mp3", "disk": "", "disks": [[String: Any]](),
                         "onAirUseTxEq": false, "onAirUseCompressor": false, "onAirUseLeveler": false,
                         "onAirUseCfc": false, "onAirUsePhaseRotator": false],
            "voiceKeyer": ["slots": slots, "repeat": false, "repeatDelaySec": 5,
                           "sending": sending.map { $0 as Any } ?? NSNull()],
        ]
    }

    private static func sending(_ deviceId: String, waitingMs: Int = 0) -> [String: Any] {
        ["n": 1, "positionMs": 500, "lengthMs": 4_000, "waitingMs": waitingMs, "by": "Shack phone",
         "deviceId": deviceId]
    }

    @Test func theMirroredDocumentIsReadAndGoesWithTheLink() async {
        let (client, mirror, _) = await rig(version: 3)
        #expect(client.document == nil)
        publish(mirror, client, Self.document(), revision: 4, create: true)
        #expect(client.document?.revision == 4)
        #expect(client.slices.first?.state == .recording)
        #expect(client.slices.first?.startedBy == "Shack phone")
        #expect(client.index?.count == 3)
        #expect(client.settings?.format == .mp3)
        #expect(client.voiceKeyer?.slot(1)?.name == "CQ")
        #expect(client.kept?.seq == 0)
        client.handle(.stateChanged(.stopped))
        #expect(client.state == .offline)
        #expect(client.coreVersion == 0)
        #expect(client.document == nil)
    }

    @Test func thisDevicesPlaybackAndALateRefusalArriveOnTheirOwn() async throws {
        let (client, _, commands) = await rig(version: 2)
        let push = LinkMessage.commandResult(LinkMessage.CommandResult(
            verb: "recordings.playback", id: 0, accepted: true, reason: "", affected: [],
            values: [.init(name: "id", value: .utf8("r-2")), .init(name: "positionMs", value: .i64(1_500)),
                     .init(name: "lengthMs", value: .i64(65_000)), .init(name: "playing", value: .bool(true)),
                     .init(name: "onAir", value: .bool(true))]))
        await commands.receive(push)
        client.handle(.message(push))
        #expect(client.playback == RecordingsPlayback(id: "r-2", positionMs: 1_500, lengthMs: 65_000, playing: true,
                                                      onAir: true))
        let late = LinkMessage.commandResult(LinkMessage.CommandResult(
            verb: "recordings.play", id: 0, accepted: false, reason: "That recording cannot be read on the Core.",
            affected: []))
        client.handle(.message(late))
        #expect(client.playbackRefusal == "That recording cannot be read on the Core.")
        let ended = LinkMessage.commandResult(LinkMessage.CommandResult(
            verb: "recordings.playback", id: 0, accepted: true, reason: "", affected: [],
            values: [.init(name: "id", value: .utf8("")), .init(name: "positionMs", value: .i64(0)),
                     .init(name: "lengthMs", value: .i64(0)), .init(name: "playing", value: .bool(false)),
                     .init(name: "onAir", value: .bool(false))]))
        client.handle(.message(ended))
        #expect(client.playback == .stopped)
        // The next play clears the old refusal.
        let play = Task { try await client.play(id: "r-3") }
        await answer(0, commands)
        try await play.value
        #expect(client.playbackRefusal == nil)
    }

    @Test func aListPageCarriesItsFiltersAndReader() async throws {
        let (client, _, commands) = await rig(version: 1)
        let task = Task { try await client.list(offset: 20, count: 10, slice: 1, kind: .contact, reader: 7) }
        let entries = """
            [{"id":"r-9","kind":"Contact","slice":1,"startUtc":"","lengthMs":1,"frequencyHz":7074000,"mode":"USB",\
            "call":"","name":"n","fileName":"f.mp3","sizeBytes":10}]
            """
        await answer(0, commands, values: [.init(name: "entries", value: .utf8(entries)),
                                           .init(name: "offset", value: .i64(20)), .init(name: "total", value: .i64(21)),
                                           .init(name: "reader", value: .i64(7))])
        let page = try await task.value
        #expect(invokes().first?.args == [.init(name: "offset", value: .i64(20)), .init(name: "count", value: .i64(10)),
                                          .init(name: "slice", value: .i64(1)), .init(name: "kind", value: .utf8("Contact")),
                                          .init(name: "reader", value: .i64(7))])
        #expect(page.entries.map(\.id) == ["r-9"])
        #expect(page.offset == 20)
        #expect(page.total == 21)
        #expect(page.reader == 7)
    }

    // MARK: The voice keyer on the air

    /// While `sending.deviceId` names this device, the sink hears so in
    /// order, again with each document, and false when it ends or the link goes.
    @Test func thisDevicesMessageKeepsTheKeepaliveTold() async {
        let told = Told()
        let (client, mirror, _) = await rig(version: 3, thisDeviceId: "phone-id", told: told)
        publish(mirror, client, Self.document(), revision: 1, create: true)
        #expect(!client.voiceMessageSending)
        publish(mirror, client, Self.document(sending: Self.sending("phone-id")), revision: 2)
        #expect(client.voiceMessageSending)
        // The wait between repeats is still this device's message.
        publish(mirror, client, Self.document(sending: Self.sending("phone-id", waitingMs: 3_000)), revision: 3)
        #expect(client.voiceMessageSending)
        publish(mirror, client, Self.document(), revision: 4)
        #expect(!client.voiceMessageSending)
        // Another device's message is not this device's to keep alive.
        publish(mirror, client, Self.document(sending: Self.sending("tablet-id")), revision: 5)
        #expect(!client.voiceMessageSending)
        publish(mirror, client, Self.document(sending: Self.sending("phone-id")), revision: 6)
        client.handle(.stateChanged(.stopped))
        #expect(!client.voiceMessageSending)
        await client.voiceMessageSinkSettled()
        #expect(told.all == [true, true, false, true, false])
    }

    /// A token sign-in learns the Core's name for itself from its own send's answer.
    @Test func aSendsAnswerNamesThisDevice() async throws {
        let told = Told()
        let (client, mirror, commands) = await rig(version: 3, told: told)
        publish(mirror, client, Self.document(), revision: 1, create: true)
        let task = Task { try await client.sendVoice(1) }
        await answer(0, commands, values: [.init(name: "deviceId", value: .utf8("token-7"))])
        try await task.value
        #expect(invokes().first?.args == [.init(name: "n", value: .i64(1))])
        publish(mirror, client, Self.document(sending: Self.sending("token-7")), revision: 2)
        #expect(client.voiceMessageSending)
        await client.voiceMessageSinkSettled()
        #expect(told.all == [true])
    }

    /// Ruling 8.5: another device's Stop is refused with the holder's words,
    /// which the call throws for the screen to show.
    @Test func anotherDevicesStopIsRefusedInTheHoldersWords() async {
        let (client, _, commands) = await rig(version: 3)
        let words = "Shack phone has the transmitter. Take it to stop the transmission."
        let task = Task { try await client.stopVoice() }
        await answer(0, commands, accepted: false, reason: words)
        await #expect(throws: RecordingsError.refused(words)) { try await task.value }
        #expect(RecordingsError.refused(words).reason == words)
    }

    /// A slot on the air, going out or waiting to repeat, is cleared or
    /// loaded by no device: refused here in the Core's words, unsent.
    @Test func aSlotOnTheAirIsNeitherClearedNorImported() async throws {
        let (client, mirror, _) = await rig(version: 3, thisDeviceId: "phone-id")
        let file = FileManager.default.temporaryDirectory.appendingPathComponent("onair-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: file) }
        try Data(count: 4).write(to: file)
        for waitingMs in [0, 3_000] {
            publish(mirror, client, Self.document(sending: Self.sending("tablet-id", waitingMs: waitingMs)),
                    revision: Int64(10 + waitingMs), create: waitingMs == 0)
            await #expect(throws: RecordingsError.refused("M1 is on the air.")) { try await client.clearVoice(1) }
            await #expect(throws: RecordingsError.refused("M1 is on the air.")) {
                try await client.importVoice(1, url: file)
            }
        }
        #expect(sent.count == 0)
    }
}
