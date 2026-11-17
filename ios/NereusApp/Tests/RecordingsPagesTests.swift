// NereusSDR for iOS: Tools > Recordings and Setup > Recording settings against a recordings client fed by hand
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import Testing

/// Recording plan Task 17 (design sections 6.4, 6.5, 6.8, V9, A4): the
/// Recordings page's list, filter, search and sort; delete only after the
/// confirm that says it cannot be undone; Select for several; the warning
/// on a big whole-pan copy; a rename sent to the Core; and Recording
/// settings sending each change as its one field, greyed when not connected
/// or on an older Core.
@Suite("Recordings pages", .timeLimit(.minutes(1)))
@MainActor
struct RecordingsPagesTests {
    typealias Flag = RecordingFlagTests

    /// Four recordings, newest first, as the Core lists them.
    static let entriesJSON = """
    [{"id":"r-4","kind":"Contact","slice":0,"startUtc":"2026-11-16T14:32:00Z","lengthMs":205000,\
    "frequencyHz":7236400,"mode":"LSB","call":"K4ABC","name":"2026-11-16 1432Z 7.236.400 LSB K4ABC",\
    "fileName":"2026-11-16 1432Z 7.236.400 LSB K4ABC.mp3","sizeBytes":1640000},
    {"id":"r-3","kind":"WholePan","slice":-1,"startUtc":"2026-11-15T01:15:00Z","lengthMs":900000,\
    "frequencyHz":7240500,"mode":"LSB","call":"","name":"Gray line opening",\
    "fileName":"Gray line opening.nsdr","sizeBytes":1400000000},
    {"id":"r-2","kind":"Sound","slice":0,"startUtc":"2026-11-14T23:40:00Z","lengthMs":1500000,\
    "frequencyHz":7230000,"mode":"LSB","call":"N4NET","name":"Tuesday night net",\
    "fileName":"Tuesday night net.mp3","sizeBytes":25000000},
    {"id":"r-1","kind":"Contact","slice":1,"startUtc":"2026-11-12T18:02:00Z","lengthMs":45000,\
    "frequencyHz":7253000,"mode":"LSB","call":"KG4VCF","name":"My audio check",\
    "fileName":"My audio check.mp3","sizeBytes":720000}]
    """

    static func entries() -> [RecordingEntry] {
        RecordingEntry.list(json: entriesJSON) ?? []
    }

    /// A list model on a connected Core at `version`, its document published.
    static func listRig(version: Int64 = 3) async -> (Flag.Rig, RecordingsListModel) {
        let rig = await Flag.rig(version: version)
        Flag.publish(rig, Flag.document(), revision: 1)
        let folder = FileManager.default.temporaryDirectory
            .appendingPathComponent("RecordingsPagesTests-\(UUID().uuidString)", isDirectory: true)
        return (rig, RecordingsListModel(client: rig.client, folder: folder))
    }

    // MARK: Tools > Recordings

    @Test("Tools lists Recordings after VAX Audio; a Core that lists its tools without it greys it with V9's words")
    func toolsRow() throws {
        let older = try JSONDecoder().decode([StationCatalog.Tool].self, from: Data("""
        [{"id":"vaxAudio","label":"VAX Audio","where":"station","offered":true},
         {"id":"networkDiagnostics","label":"Network Diagnostics","where":"both","offered":true}]
        """.utf8))
        let entries = StationToolList.entries(tools: older, connected: true, olderCore: false)
        #expect(entries.map(\.id) == ["vaxAudio", "voiceKeyer", "recordings", "networkDiagnostics", "rotor"])
        let row = try #require(entries.first { $0.id == "recordings" })
        #expect(row.title == "Recordings" && row.tag == .core && row.page == .recordings)
        #expect(!row.enabled && row.reason == RecordingsClient.updateReason && row.detail == RecordingsClient.updateReason)
        let away = StationToolList.entries(tools: older, connected: false, olderCore: false)
        #expect(away.first { $0.id == "recordings" }?.reason == StationToolList.notConnectedReason)

        let newer = try JSONDecoder().decode([StationCatalog.Tool].self, from: Data("""
        [{"id":"vaxAudio","label":"VAX Audio","where":"station","offered":true},
         {"id":"recordings","label":"Recordings","where":"station","offered":true}]
        """.utf8))
        let open = try #require(StationToolList.entries(tools: newer, connected: true, olderCore: false)
            .first { $0.id == "recordings" })
        #expect(open.enabled && open.page == .recordings && open.detail == "Contacts and saved stretches kept on the Core")
        #expect(ToolsTab.Page(.recordings) == .recordings && ToolsTab.Page.recordings.title == "Recordings")
    }

    @Test("the list asks for every page, then filters, searches name and call, and sorts")
    func listFilterSearchSort() async {
        let (rig, model) = await Self.listRig()
        let page1 = Array(Self.entries().prefix(3))
        let page2 = Array(Self.entries().suffix(1))
        func json(_ entries: [RecordingEntry]) -> String {
            let objects = entries.map { e -> [String: Any] in
                ["id": e.id, "kind": e.kind.rawValue, "slice": e.slice, "startUtc": e.startUtc, "lengthMs": e.lengthMs,
                 "frequencyHz": e.frequencyHz, "mode": e.mode, "call": e.call, "name": e.name,
                 "fileName": e.fileName, "sizeBytes": e.sizeBytes]
            }
            return String(decoding: (try? JSONSerialization.data(withJSONObject: objects)) ?? Data(), as: UTF8.self)
        }
        let task = Task { await model.refresh() }
        await Flag.answer(rig, 0, values: [.init(name: "entries", value: .utf8(json(page1))),
                                           .init(name: "offset", value: .i64(0)), .init(name: "total", value: .i64(4)),
                                           .init(name: "reader", value: .i64(RecordingsListModel.reader))])
        await Flag.answer(rig, 1, values: [.init(name: "entries", value: .utf8(json(page2))),
                                           .init(name: "offset", value: .i64(3)), .init(name: "total", value: .i64(4)),
                                           .init(name: "reader", value: .i64(RecordingsListModel.reader))])
        await task.value
        #expect(rig.sent.invokes.map(\.verb) == ["recordings.list", "recordings.list"])
        #expect(rig.sent.invokes[0].args == [.init(name: "offset", value: .i64(0)), .init(name: "count", value: .i64(100)),
                                             .init(name: "reader", value: .i64(RecordingsListModel.reader))])
        #expect(rig.sent.invokes[1].args.first == .init(name: "offset", value: .i64(3)))
        #expect(model.visible.map(\.id) == ["r-4", "r-3", "r-2", "r-1"])
        // The same revision asks nothing again.
        await model.refresh()
        #expect(rig.sent.invokes.count == 2)

        model.filter = .contact
        #expect(model.visible.map(\.id) == ["r-4", "r-1"])
        model.filter = .wholePan
        #expect(model.visible.map(\.id) == ["r-3"])
        model.filter = .sound
        #expect(model.visible.map(\.id) == ["r-2"])
        model.filter = .all
        // The desktop's search: name and call, either case.
        model.search = "n4net"
        #expect(model.visible.map(\.id) == ["r-2"])
        model.search = "gray"
        #expect(model.visible.map(\.id) == ["r-3"])
        model.search = "K4A"
        #expect(model.visible.map(\.id) == ["r-4"])
        model.search = ""
        model.sort = .oldest
        #expect(model.visible.map(\.id) == ["r-1", "r-2", "r-3", "r-4"])
        model.sort = .frequency
        #expect(model.visible.map(\.id) == ["r-2", "r-4", "r-3", "r-1"])
        model.sort = .longest
        #expect(model.visible.map(\.id) == ["r-2", "r-3", "r-4", "r-1"])
        model.sort = .largest
        #expect(model.visible.map(\.id) == ["r-3", "r-2", "r-4", "r-1"])

        // The rows' words, as the desktop says them.
        let e = Self.entries()
        #expect(RecordingsListModel.title(of: e[0]) == "7.236.400 LSB K4ABC")
        #expect(RecordingsListModel.title(of: e[1]) == "Gray line opening")
        #expect(RecordingsListModel.whereText(e[0]) == "16 Nov 14:32 · 7.236.400 LSB")
        #expect(RecordingsListModel.detailText(e[0]) == "Contact · 3:25 · 1.6 MB")
        #expect(RecordingsListModel.detailText(e[2]) == "Sound · 25:00 · 25 MB · N4NET")
        #expect(RecordingsListModel.sizeText(1_400_000_000) == "1.4 GB")
        #expect(RecordingsListModel.sizeText(12_400_000_000) == "12 GB")
        #expect(RecordingsListModel.sizeText(720_000) == "0.7 MB")
        #expect(RecordingsListModel.countText(1) == "1 recording")
        #expect(model.summary == "0.0 MB saved in 2 recordings. 0.0 MB free. " + RecordingsListModel.recorderNote)
    }

    @Test("delete sends nothing until the confirm, which says it cannot be undone")
    func deleteAsksFirst() async {
        let (rig, model) = await Self.listRig()
        model.received(Self.entries())
        model.askDelete(["r-4"])
        let pending = model.pendingDelete
        #expect(pending?.title == "Delete this recording from the Core?")
        #expect(pending?.message.contains("cannot be undone") == true)
        #expect(pending?.message.contains("1.6 MB") == true)
        #expect(pending?.button == "Delete Recording")
        // Nothing has gone: Cancel leaves it all as it was.
        #expect(rig.sent.invokes.isEmpty)
        model.pendingDelete = nil
        #expect(rig.sent.invokes.isEmpty && model.entries.count == 4)

        model.askDelete(["r-4"])
        let task = Task { await model.confirmDelete() }
        await Flag.answer(rig, 0)
        await task.value
        #expect(rig.sent.invokes.map(\.verb) == ["recordings.delete"])
        #expect(rig.sent.invokes[0].args == [.init(name: "ids", value: .utf8("[\"r-4\"]"))])
        #expect(model.entries.map(\.id) == ["r-3", "r-2", "r-1"])
        #expect(model.pendingDelete == nil)
    }

    @Test("Select picks several; Delete asks once for them all and sends their ids together")
    func selectSeveral() async {
        let (rig, model) = await Self.listRig()
        model.received(Self.entries())
        #expect(!model.selecting)
        model.toggleSelecting()
        #expect(model.selecting && model.selected.isEmpty)
        // A tap picks a row while selecting, and plays nothing.
        await model.tap(Self.entries()[3])
        await model.tap(Self.entries()[0])
        await model.tap(Self.entries()[2])
        await model.tap(Self.entries()[2])
        #expect(model.selected == ["r-1", "r-4"])
        #expect(rig.sent.invokes.isEmpty)
        // In the list's order, newest first.
        #expect(model.chosen.map(\.id) == ["r-4", "r-1"])
        model.askDelete(model.chosen.map(\.id))
        #expect(model.pendingDelete?.title == "Delete 2 recordings from the Core?")
        #expect(model.pendingDelete?.button == "Delete 2 Recordings")
        #expect(model.pendingDelete?.message.contains("cannot be undone") == true)
        let task = Task { await model.confirmDelete() }
        await Flag.answer(rig, 0)
        await task.value
        #expect(rig.sent.invokes[0].args == [.init(name: "ids", value: .utf8("[\"r-4\",\"r-1\"]"))])
        #expect(model.entries.map(\.id) == ["r-3", "r-2"] && model.selected.isEmpty && model.selecting)
        // Done leaves selecting with nothing picked.
        model.toggleSelecting()
        #expect(!model.selecting && model.selected.isEmpty)
    }

    @Test("a tap plays on this phone, again stops it; a whole-pan stretch on a Core without the recorder says why")
    func tapPlays() async {
        let (rig, model) = await Self.listRig()
        model.received(Self.entries())
        let play = Task { await model.tap(Self.entries()[0]) }
        await Flag.answer(rig, 0)
        await play.value
        #expect(rig.sent.invokes[0].verb == "recordings.play")
        #expect(rig.sent.invokes[0].args == [.init(name: "id", value: .utf8("r-4"))])
        Flag.playback(rig, id: "r-4", positionMs: 12_000, lengthMs: 205_000, playing: true, onAir: false)
        #expect(model.playingId == "r-4")
        #expect(model.bar?.title == "7.236.400 LSB K4ABC" && model.bar?.detail == "0:12 / 3:25 · speaker")
        let stop = Task { await model.tap(Self.entries()[0]) }
        await Flag.answer(rig, 1)
        await stop.value
        #expect(rig.sent.invokes[1].verb == "recordings.stopPlay")
        await model.tap(Self.entries()[1])
        #expect(rig.sent.invokes.count == 2)
        #expect(model.note == RecordingsClient.updateReason && model.stretchReason == RecordingsClient.updateReason)
    }

    @Test("a whole-pan stretch opens on the pan and saves its sound only; the Core's words show")
    func wholePanOpensAndSavesItsSound() async {
        let (rig, model) = await Self.listRig(version: 4)
        model.received(Self.entries())
        let pan = Self.entries()[1]
        #expect(model.stretchReason == nil)
        let open = Task { await model.tap(pan) }
        await Flag.answer(rig, 0)
        await open.value
        #expect(rig.sent.invokes[0].verb == "recordings.openStretch")
        #expect(rig.sent.invokes[0].args == [.init(name: "id", value: .utf8(pan.id))])
        #expect(model.note == nil)
        let refused = Task { await model.openStretch(pan) }
        await Flag.answer(rig, 1, accepted: false, reason: "Slice A belongs to another device.")
        await refused.value
        #expect(model.note == "Slice A belongs to another device.")
        let sound = Task { await model.saveSoundOnly(pan) }
        await Flag.answer(rig, 2, values: [.init(name: "note", value: .utf8("Saving the sound to Recordings."))])
        await sound.value
        #expect(rig.sent.invokes[2].verb == "recordings.saveSoundOnly")
        #expect(rig.sent.invokes[2].args == [.init(name: "id", value: .utf8(pan.id))])
        #expect(model.note == "Saving the sound to Recordings.")
    }

    @Test("a big whole-pan copy says how big it is before it starts; a small one copies at once")
    func bigShareWarns() async throws {
        let (rig, model) = await Self.listRig()
        model.received(Self.entries())
        await model.share([Self.entries()[1]])
        #expect(model.pendingShare?.map(\.id) == ["r-3"])
        #expect(RecordingsListModel.shareWarning(model.pendingShare ?? []).hasPrefix("This is a whole-pan stretch of 1.4 GB."))
        #expect(rig.sent.invokes.isEmpty)
        model.pendingShare = nil

        // A contact copies at once, a chunk at a time, then the share sheet opens.
        let data = Data("ID3 a small file".utf8)
        let share = Task { await model.share([Self.entries()[0]]) }
        await Flag.answer(rig, 0, values: [.init(name: "offset", value: .i64(0)),
                                           .init(name: "size", value: .i64(Int64(data.count))),
                                           .init(name: "data", value: .utf8(data.base64EncodedString()))])
        await share.value
        #expect(rig.sent.invokes[0].verb == "recordings.export")
        let shared = try #require(model.shared)
        #expect(shared.urls.map(\.lastPathComponent) == ["2026-11-16 1432Z 7.236.400 LSB K4ABC.mp3"])
        #expect(try Data(contentsOf: shared.urls[0]) == data)
        model.shareClosed(shared)
        #expect(!FileManager.default.fileExists(atPath: shared.urls[0].path))
    }

    @Test("a rename goes to the Core and shows at once")
    func renameReachesTheCore() async {
        let (rig, model) = await Self.listRig()
        model.received(Self.entries())
        model.startRename(Self.entries()[0])
        #expect(model.renameText == "2026-11-16 1432Z 7.236.400 LSB K4ABC")
        model.renameText = "  Bob in Atlanta "
        let task = Task { await model.commitRename() }
        await Flag.answer(rig, 0)
        await task.value
        #expect(rig.sent.invokes[0].verb == "recordings.rename")
        #expect(rig.sent.invokes[0].args == [.init(name: "id", value: .utf8("r-4")),
                                             .init(name: "name", value: .utf8("Bob in Atlanta"))])
        #expect(model.entries.first?.name == "Bob in Atlanta")
        #expect(RecordingsListModel.title(of: model.entries[0]) == "Bob in Atlanta")
    }

    @Test("not connected or an older Core: everything stays, greyed with the reason, and sends nothing")
    func greyed() async {
        let (older, olderModel) = await Self.listRig(version: 0)
        #expect(olderModel.reason == RecordingsClient.updateReason)
        await olderModel.refresh()
        olderModel.received(Self.entries())
        olderModel.askDelete(["r-4"])
        await olderModel.tap(Self.entries()[0])
        #expect(olderModel.pendingDelete == nil && older.sent.invokes.isEmpty)
        #expect(olderModel.note == RecordingsClient.updateReason)

        let away = await Flag.rig(version: 3, connected: false)
        let awayModel = RecordingsListModel(client: away.client)
        #expect(awayModel.reason == RecordingFlagModel.notConnectedReason)
        // No totals under the reason: it says why.
        #expect(awayModel.summary == nil && olderModel.summary == nil)
        await awayModel.share(Self.entries())
        #expect(away.sent.invokes.isEmpty && awayModel.pendingShare == nil)
    }

    // MARK: Setup > Recording settings

    @Test("each setting sends only its own field, and the on-air switches need version 2")
    func settingsSendOnlyTheChange() async {
        let rig = await Flag.rig(version: 2)
        Flag.publish(rig, Flag.document(), revision: 1)
        let model = RecordingSettingsModel(client: rig.client)
        #expect(model.reason == nil && model.onAirReason == nil)
        #expect(model.settings.keepMyVoice && !model.settings.autoStart && model.settings.quietMinutes == 2)

        let start = Task { await model.setAutoStart(true) }
        // Shown at once, before the Core answers.
        #expect(await Eventually.holds { rig.sent.invokes.count == 1 })
        #expect(model.settings.autoStart)
        await Flag.answer(rig, 0)
        await start.value
        #expect(rig.sent.invokes[0].verb == "recordings.setSettings")
        #expect(rig.sent.invokes[0].args == [.init(name: "autoStart", value: .bool(true))])

        let quiet = Task { await model.setQuietMinutes(5) }
        await Flag.answer(rig, 1)
        await quiet.value
        #expect(rig.sent.invokes[1].args == [.init(name: "quietMinutes", value: .i64(5))])

        let format = Task { await model.setFormat(.wav) }
        await Flag.answer(rig, 2)
        await format.value
        #expect(rig.sent.invokes[2].args == [.init(name: "format", value: .utf8("wav"))])

        let onAir = Task { await model.setOnAir(.compressor, true) }
        await Flag.answer(rig, 3)
        await onAir.value
        #expect(rig.sent.invokes[3].args == [.init(name: "onAirUseCompressor", value: .bool(true))])

        // The quiet time stays within 1 to 60 minutes; a step to where it is sends nothing.
        await model.setQuietMinutes(2)
        #expect(rig.sent.invokes.count == 4)

        // A refusal shows in the Core's words, and the page goes back to the Core's value.
        let refused = Task { await model.setKeepMyVoice(false) }
        await Flag.answer(rig, 4, accepted: false, reason: "The Core is busy.")
        await refused.value
        #expect(model.refusal == "The Core is busy.")
        #expect(model.settings.keepMyVoice)
    }

    @Test("Recording settings' words: size per hour, the name, the disk, and what is kept")
    func settingsWords() async {
        #expect(RecordingSettingsModel.megabytesPerHour(wav: false, apart: false) == 29)
        #expect(RecordingSettingsModel.megabytesPerHour(wav: false, apart: true) == 58)
        #expect(RecordingSettingsModel.megabytesPerHour(wav: true, apart: false) == 173)
        #expect(RecordingSettingsModel.megabytesPerHour(wav: true, apart: true) == 346)
        let rig = await Flag.rig(version: 1)
        Flag.publish(rig, Flag.document(), revision: 1)
        let model = RecordingSettingsModel(client: rig.client)
        #expect(model.formatDetail(.mp3) == "Small, plays anywhere. About 29 MB an hour.")
        #expect(model.formatDetail(.wav) == "Full quality, bigger. About 173 MB an hour.")
        #expect(model.nameExample == "2026-11-15 1432Z 7.236.400 LSB K4ABC.mp3")
        #expect(model.usageText.hasPrefix("2 recordings, 0.0 MB. 0.0 MB free on this disk"))
        #expect(RecordingSettingsModel.diskText(RecordingsDocument.Disk(root: "/Volumes/T7", name: "Samsung T7",
                                                                        free: 812_000_000_000, total: 1_000_000_000_000,
                                                                        sd: false, own: false))
                    == "Samsung T7 (812 GB free)")
        #expect(RecordingSettingsModel.diskText(RecordingsDocument.Disk(root: "/", name: "", free: 41_000_000_000,
                                                                        total: 0, sd: false, own: true))
                    == "The Core's own disk (41 GB free)")
        // Version 1: the on-air switches grey with V9's words; the rest work.
        #expect(model.reason == nil && model.onAirReason == RecordingsClient.updateReason)
        await model.setOnAir(.txEq, true)
        #expect(rig.sent.invokes.isEmpty)
    }

    @Test("not connected, every group dims with the note and nothing is sent")
    func settingsNotConnected() async {
        let away = await Flag.rig(version: 3, connected: false)
        let model = RecordingSettingsModel(client: away.client)
        // The desktop's words, as Tools > Recordings says them.
        #expect(RecordingSettingsModel.notConnectedNote == "Connect to a radio to use recordings.")
        #expect(model.reason == RecordingSettingsModel.notConnectedNote)
        #expect(model.onAirReason == RecordingSettingsModel.notConnectedNote)
        #expect(model.settings == RecordingSettingsModel.defaults)
        #expect(model.usageText == "Not known.")
        await model.setAutoStart(true)
        await model.setFormat(.wav)
        #expect(away.sent.invokes.isEmpty)
        let older = await Flag.rig(version: 0)
        #expect(RecordingSettingsModel(client: older.client).reason == RecordingsClient.updateReason)
    }

    @Test("Setup lists Recording settings near the foot; the flag's links open both pages")
    func setupAndLinks() {
        let tree = SetupTree.categories
        let titles = tree.map(\.title)
        #expect(titles.suffix(2) == ["Recording settings", "About this app"])
        let row = tree.first { $0.title == "Recording settings" }
        #expect(row?.tag == .core && row?.opensPage == .recordingSettings)
        #expect(row?.summary == "The Record button, file type and names")

        let setup = SetupRouter()
        let tools = ToolsRouter()
        var tab: String?
        let navigation = RootView.navigation(setupRouter: setup, toolsRouter: tools, selectSetup: { tab = "setup" },
                                             selectTools: { tab = "tools" })
        let before = tools.recordingsRequest
        navigation.openRecordings()
        #expect(tools.recordingsRequest == before &+ 1 && tab == "tools")
        navigation.openPage(.recordingSettings)
        #expect(setup.path == [.page(.recordingSettings)] && tab == "setup")
    }
}
