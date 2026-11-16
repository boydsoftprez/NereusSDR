// NereusSDR for iOS: tests of the iPad's Recordings panel: its rows, its words, its taps, its switch and its place in the column
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 19, the iPad's Recordings panel.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import Testing

/// Recording plan Task 19 (design sections 3, 6.4 and 6.8): the iPad's
/// Recordings panel lists a recording slice first and then the Core's
/// latest, six rows in all, plays and stops a row on this iPad, stops a
/// slice's recording, says how many the Core keeps, keeps its place under
/// the S-Meter only while this iPad turns it on, and shows the reason on a
/// Core without recordings or with none connected.
@Suite("iPad Recordings panel", .timeLimit(.minutes(1)))
@MainActor
struct IPadRecordingsAppletTests {
    typealias Flag = RecordingFlagTests

    /// Seven recordings, newest first: one more than the panel shows.
    static let sevenJSON: String = {
        let objects: [[String: Any]] = (1...7).reversed().map { n in
            ["id": "r-\(n)", "kind": n == 6 ? "WholePan" : "Contact", "slice": 0,
             "startUtc": "2026-11-1\(n)T14:3\(n):00Z", "lengthMs": 60_000 * n, "frequencyHz": 7_236_400,
             "mode": "LSB", "call": "", "name": "Recording \(n)", "fileName": "r\(n).mp3", "sizeBytes": 1_000]
        }
        return String(decoding: (try? JSONSerialization.data(withJSONObject: objects)) ?? Data(), as: UTF8.self)
    }()

    static func rig(version: Int64? = 3, connected: Bool = true, state: String = "idle",
                    keepMyVoice: Bool = true) async -> (Flag.Rig, RecordingsAppletModel) {
        let rig = await Flag.rig(version: version, connected: connected)
        if connected {
            var document = RecordingsPagesShotTests.document()
            document["slices"] = Flag.document(state: state, elapsedMs: 12_400)["slices"]
            if var settings = document["settings"] as? [String: Any] {
                settings["keepMyVoice"] = keepMyVoice
                document["settings"] = settings
            }
            Flag.publish(rig, document, revision: 1)
        }
        return (rig, RecordingsAppletModel(client: rig.client))
    }

    /// The panel's list, answered with `json`.
    static func listed(_ rig: Flag.Rig, _ model: RecordingsAppletModel, json: String = sevenJSON) async {
        let task = Task { await model.refresh() }
        await Flag.answer(rig, 0, values: [.init(name: "entries", value: .utf8(json)),
                                           .init(name: "offset", value: .i64(0)), .init(name: "total", value: .i64(7)),
                                           .init(name: "reader", value: .i64(RecordingsAppletModel.reader))])
        await task.value
    }

    // MARK: The rows

    @Test("it asks the Core for the latest six with its own reader, once per revision, and lists them newest first")
    func listsTheLatestSix() async {
        let (rig, model) = await Self.rig()
        await Self.listed(rig, model)
        #expect(rig.sent.invokes.map(\.verb) == ["recordings.list"])
        #expect(rig.sent.invokes[0].args == [.init(name: "offset", value: .i64(0)), .init(name: "count", value: .i64(6)),
                                             .init(name: "reader", value: .i64(RecordingsAppletModel.reader))])
        #expect(model.rows.map(\.id) == ["r-7", "r-6", "r-5", "r-4", "r-3", "r-2"])
        #expect(model.footer == "4 on the Core, 1.4 GB")
        // The same revision asks nothing again.
        await model.refresh()
        #expect(rig.sent.invokes.count == 1)
    }

    @Test("a slice recording now heads the list in its own row, and the six rows still stop at six")
    func recordingSliceFirst() async {
        let (rig, model) = await Self.rig(state: "recording")
        await Self.listed(rig, model)
        let rows = model.rows
        #expect(rows.count == RecordingsAppletModel.rowCount)
        #expect(rows.first == .recording(slice: 0, elapsedMs: 12_400))
        #expect(rows.map(\.id) == ["slice:0", "r-7", "r-6", "r-5", "r-4", "r-3"])
        #expect(model.recordingDetail == "what you hear and your voice")
        let (_, without) = await Self.rig(state: "recording", keepMyVoice: false)
        #expect(without.recordingDetail == "what you hear")
    }

    @Test("the row this iPad plays shows its place, blue on the speaker and red on the air")
    func playingRow() async throws {
        let (rig, model) = await Self.rig()
        await Self.listed(rig, model)
        let entry = try #require(model.actions.entries.first { $0.id == "r-5" })
        Flag.playback(rig, id: "r-5", positionMs: 12_000, lengthMs: 300_000, playing: true, onAir: false)
        #expect(model.rows[2] == .entry(entry, look: .playing(positionMs: 12_000)))
        #expect(RecordingsAppletModel.lengthText(entry, look: .playing(positionMs: 12_000)) == "0:12/5:00")
        #expect(RecordingsAppletModel.lengthText(entry, look: .idle) == "5:00")
        Flag.playback(rig, id: "r-5", positionMs: 13_000, lengthMs: 300_000, playing: true, onAir: true)
        #expect(model.rows[2] == .entry(entry, look: .onAir(positionMs: 13_000)))
        Flag.playback(rig, id: "r-5", positionMs: 0, lengthMs: 300_000, playing: false, onAir: false)
        #expect(model.rows[2] == .entry(entry, look: .idle))
    }

    // MARK: Taps

    @Test("a row's play plays it on this iPad and stops it while it plays; a whole-pan stretch says why instead")
    func playAndStop() async throws {
        let (rig, model) = await Self.rig()
        await Self.listed(rig, model)
        let entry = try #require(model.actions.entries.first { $0.id == "r-7" })
        let play = Task { await model.tapPlay(entry) }
        await Flag.answer(rig, 1)
        await play.value
        #expect(rig.sent.invokes[1].verb == "recordings.play")
        #expect(rig.sent.invokes[1].args == [.init(name: "id", value: .utf8("r-7"))])
        Flag.playback(rig, id: "r-7", positionMs: 1_000, lengthMs: 420_000, playing: true, onAir: false)
        let stop = Task { await model.tapPlay(entry) }
        await Flag.answer(rig, 2)
        await stop.value
        #expect(rig.sent.invokes[2].verb == "recordings.stopPlay")

        let pan = try #require(model.actions.entries.first { $0.kind == .wholePan })
        await model.tapPlay(pan)
        #expect(rig.sent.invokes.count == 3)
        #expect(model.note == RecordingsListModel.wholePanReason)
    }

    @Test("a recording row's stop square stops that slice on the Core, and a refusal shows the Core's words")
    func stopsTheRecording() async {
        let (rig, model) = await Self.rig(state: "recording")
        let stop = Task { await model.stopRecording(slice: 0) }
        await Flag.answer(rig, 0)
        await stop.value
        #expect(rig.sent.invokes[0].verb == "recordings.stop")
        #expect(rig.sent.invokes[0].args == [.init(name: "slice", value: .i64(0))])
        #expect(model.note == nil)
        let refused = Task { await model.stopRecording(slice: 0) }
        await Flag.answer(rig, 1, accepted: false, reason: "Slice A is not recording.")
        await refused.value
        #expect(model.note == "Slice A is not recording.")
    }

    // MARK: Without a Core

    @Test("an older Core and no Core leave the panel with the reason, no rows and no count")
    func reasons() async {
        let (older, olderModel) = await Self.rig(version: 0)
        #expect(olderModel.reason == RecordingsClient.updateReason)
        #expect(olderModel.rows.isEmpty && olderModel.footer == nil)
        await olderModel.refresh()
        #expect(older.sent.invokes.isEmpty)
        let (_, away) = await Self.rig(connected: false)
        #expect(away.reason == RecordingFlagModel.notConnectedReason)
        #expect(away.rows.isEmpty && away.footer == nil)
    }

    // MARK: Words

    @Test("each row says the day and the UTC time, as the desktop panel does")
    func dayText() throws {
        let entry = try #require(RecordingEntry.list(json: Flag.entriesJSON)?.first)
        let now = try #require(RecordingsListModel.date("2026-11-16T20:00:00Z"))
        #expect(RecordingsAppletModel.dayText(entry, now: now) == "today 1432Z")
        let tomorrow = try #require(RecordingsListModel.date("2026-11-17T00:10:00Z"))
        #expect(RecordingsAppletModel.dayText(entry, now: tomorrow) == "yesterday 1432Z")
        let later = try #require(RecordingsListModel.date("2026-11-20T09:00:00Z"))
        #expect(RecordingsAppletModel.dayText(entry, now: later) == "4 days ago 1432Z")
    }

    // MARK: The switch and the column

    @Test("the panel is off until this iPad turns it on, and the choice is kept on the device")
    func offByDefault() throws {
        let suite = "IPadRecordingsAppletTests-\(UUID().uuidString)"
        let store = try #require(UserDefaults(suiteName: suite))
        defer { store.removePersistentDomain(forName: suite) }
        let settings = PhoneSettings(defaults: store, isPad: true)
        #expect(!settings.recordingsAppletShown)
        settings.recordingsAppletShown = true
        #expect(PhoneSettings(defaults: store, isPad: true).recordingsAppletShown)
        #expect(store.object(forKey: PhoneSettings.keyPrefix + PhoneSettings.recordingsAppletShownKey) as? Bool == true)
    }

    @Test("turned on, it sits under the S-Meter and pushes the rest down; Roundtable keeps its own place")
    func columnOrder() {
        #expect(AppletOrder.order(roundtableAt: AppletOrder.roundtableDefault, recordings: false)
            == [.sMeter, .roundtable, .bandStack, .rx, .tx])
        #expect(AppletOrder.order(roundtableAt: AppletOrder.roundtableDefault, recordings: true)
            == [.sMeter, .recordings, .roundtable, .bandStack, .rx, .tx])
        #expect(AppletOrder.order(roundtableAt: 0, recordings: true)
            == [.roundtable, .sMeter, .recordings, .bandStack, .rx, .tx])
        #expect(AppletOrder.order(roundtableAt: AppletOrder.lastPosition, recordings: true)
            == [.sMeter, .recordings, .bandStack, .rx, .tx, .roundtable])
    }
}
