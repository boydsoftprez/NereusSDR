// NereusSDR for iOS: the Digital Spectrum Recorder on the pan and its Setup page against a recordings client fed by hand
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import Testing

/// Recording plan Task 28 (design sections 6.7, 6.8, A11, A12, A16): the
/// strip dragged back draws the Core's rows in chunks with "Drawing" while
/// they come; letting go in the past plays Slice A from the top at its
/// frequency, a tap there tunes, and LIVE returns it; any tune in the
/// past plays from the top when it is not playing yet, and playing, a
/// frequency outside what was kept then says so;
/// a key while rewound takes it live; the hours map jumps; SAVE's bracket
/// and panel; the Setup page's SD card warning, its one-field changes and
/// erase; an older Core or the recorder off leaves every control disabled
/// with the reason.
@Suite("Digital Spectrum Recorder", .timeLimit(.minutes(1)))
@MainActor
struct DsrTests {
    typealias Rig = RecordingFlagTests.Rig

    /// Now, for every model here: 2026-11-16 13:12:00 UTC.
    static let now = 1_763_298_720.0
    /// Two hours kept.
    static let oldestMs: Int64 = 1_763_291_520_000
    static let spp = 0.5
    static let points = 383.0
    static let view = DsrPanView(fftSize: 4096, windowType: 1, centreHz: 7_236_000, spanHz: 48_000, pixels: 402)

    static func dsrPart(enabled: Bool = true, recording: Bool = true, reason: String = "",
                        rewound: [[String: Any]] = [], disk: String = "") -> [String: Any] {
        ["enabled": enabled, "disk": disk, "sizeLimitGb": 10, "recording": recording, "reason": reason,
         "sizeLimitBytes": 10_000_000_000, "usedBytes": 10_000_000_000, "writePosition": 6_300_000_000,
         "oldestUtcMs": recording ? oldestMs : 0, "newestUtcMs": Int64(now * 1000),
         "hoursHeld": recording ? 2.0 : 0, "overflows": 0, "rewound": rewound, "stretch": NSNull(), "saving": 0,
         "receivers": [["receiver": 0, "rateHz": 192_000]], "bytesPerHour": 2_764_800_000]
    }

    static func document(dsr: [String: Any]?, disks: [[String: Any]] = []) -> [String: Any] {
        var document = RecordingFlagTests.document()
        document["version"] = 4
        document["dsr"] = dsr ?? NSNull()
        if var settings = document["settings"] as? [String: Any] {
            settings["disks"] = disks
            document["settings"] = settings
        }
        return document
    }

    static func rewound(_ utcNs: Int64) -> [[String: Any]] {
        [["slice": 0, "utcNs": utcNs, "frequencyHz": 7_240_500, "liveFrequencyHz": 7_236_400]]
    }

    /// A pan model on a Core at `version`, with the band holding a minute of its own.
    static func pan(version: Int64 = 4, dsr: [String: Any]? = dsrPart(),
                    connected: Bool = true) async -> (Rig, DsrPanModel, LookBacks) {
        let rig = await RecordingFlagTests.rig(version: version, connected: connected)
        if version >= 4 {
            RecordingFlagTests.publish(rig, document(dsr: dsr), revision: 1)
        }
        let model = DsrPanModel(client: rig.client)
        let looks = LookBacks()
        model.clock = { now }
        model.panView = { view }
        model.activeSlice = { 0 }
        model.activeSliceHz = { 7_240_500 }
        model.ownSeconds = { 60 }
        model.lookBack = { looks.seconds.append($0) }
        model.secondsPerPoint = spp
        model.waterfallPoints = points
        return (rig, model, looks)
    }

    final class LookBacks {
        var seconds: [Double] = []
    }

    /// The first command with `verb`, once it has gone.
    static func invoke(_ rig: Rig, _ verb: String, after: Int = 0) async -> (Int, LinkMessage.CommandInvoke)? {
        _ = await Eventually.holds { rig.sent.invokes.dropFirst(after).contains { $0.verb == verb } }
        let all = rig.sent.invokes
        guard let index = all.indices.dropFirst(after).first(where: { all[$0].verb == verb }) else {
            return nil
        }
        return (index, all[index])
    }

    static func argument(_ invoke: LinkMessage.CommandInvoke, _ name: String) -> LinkMessage.PropertyValue? {
        invoke.args.first { $0.name == name }?.value
    }

    /// The Core's rows for `moments`, each one row of three bins.
    static func rows(_ moments: [Double], rowsLeft: Int, next: Int64) -> [LinkMessage.PropertyEntry] {
        let times = moments.map { Int64(($0 * 1e9).rounded()) }
        let bins = [Int16](repeating: -1_100, count: times.count * 3)
        return [.init(name: "slice", value: .i64(0)), .init(name: "firstBinHz", value: .f64(7_212_000)),
                .init(name: "hzPerBin", value: .f64(16_000)), .init(name: "binCount", value: .i64(3)),
                .init(name: "times", value: .utf8(DsrPastFrames.packTimes(times).base64EncodedString())),
                .init(name: "bins", value: .utf8(DsrPastFrames.packBins(bins).base64EncodedString())),
                .init(name: "transmit", value: .utf8(Data(repeating: 0, count: times.count).base64EncodedString())),
                .init(name: "rowsLeft", value: .i64(Int64(rowsLeft))), .init(name: "nextToUtcNs", value: .i64(next))]
    }

    // MARK: The pan

    /// Dragged back past the band's own minute, the Core draws the picture:
    /// its rows come in chunks, "Drawing" shows while a quarter or more
    /// waits, and goes once the rest arrive.
    @Test func draggedBackTheRowsArriveInChunksWithDrawing() async throws {
        let (rig, model, looks) = await Self.pan()
        #expect(!model.inPast && !model.liveLit && model.usable)
        model.drag(from: model.top, by: -1_000)
        #expect(model.viewTop == Self.now - 500 && model.inPast && model.liveLit && model.coreDraws)
        #expect(looks.seconds.last == 500)
        model.dragEnded()
        let (first, ask) = try #require(await Self.invoke(rig, "dsr.rows"))
        #expect(Self.argument(ask, "pixels") == .i64(402) && Self.argument(ask, "centreHz") == .i64(7_236_000))
        #expect(model.drawingText == "Digital Spectrum Recorder: drawing 13:03")
        let top = model.top
        let moments = (0..<Int(Self.points)).map { top - Double($0) * Self.spp }
        await RecordingFlagTests.answer(rig, first, values: Self.rows(Array(moments.prefix(100)), rowsLeft: 283,
                                                                      next: Int64((moments[100] * 1e9).rounded())))
        #expect(await Eventually.holds { model.row(at: moments[50]) != nil })
        #expect(model.drawingText != nil)
        // The rest, asked for from where the first chunk ended.
        let (second, more) = try #require(await Self.invoke(rig, "dsr.rows", after: first + 1))
        #expect(Self.argument(more, "rowCount") == .i64(283))
        await RecordingFlagTests.answer(rig, second, values: Self.rows(Array(moments.dropFirst(100)), rowsLeft: 0,
                                                                       next: 0))
        #expect(await Eventually.holds { model.drawingText == nil })
        #expect(model.missingShare == 0 && model.rowsLayout?.count == 3)
    }

    /// Letting go of the strip in the past plays Slice A from the picture's
    /// top at the frequency it is on; a tap while it plays tunes and asks
    /// for no rewind; LIVE returns it to the radio.
    @Test func lettingGoPlaysFromTheTopAndLiveReturns() async throws {
        let (rig, model, _) = await Self.pan()
        // Live, a tap is the band's: it tunes as ever.
        #expect(!model.tapWaterfall(pointsDown: 20, hz: 7_240_500))
        model.drag(from: model.top, by: -1_000)
        model.dragEnded()
        let top = model.top
        let (index, rewind) = try #require(await Self.invoke(rig, "dsr.rewind"))
        #expect(Self.argument(rewind, "slice") == .i64(0))
        #expect(Self.argument(rewind, "utcNs") == .i64(Int64((top * 1e9).rounded())))
        #expect(Self.argument(rewind, "frequencyHz") == .i64(7_240_500))
        await RecordingFlagTests.answer(rig, index)
        #expect(await Eventually.holds { model.mine.contains(0) })
        #expect(model.viewTop == top)
        RecordingFlagTests.publish(rig, Self.document(dsr: Self.dsrPart(rewound: Self.rewound(Int64(top * 1e9)))),
                                   revision: 2)
        model.documentChanged()
        #expect(model.rewound?.frequencyHz == 7_240_500 && model.playedMoment == top && model.liveLit)
        // Playing, a tap tunes as live: no second rewind.
        let before = rig.sent.invokes.count
        #expect(!model.tapWaterfall(pointsDown: 20, hz: 7_238_000))
        try? await Task.sleep(for: .milliseconds(50))
        #expect(!rig.sent.invokes.dropFirst(before).contains { $0.verb == "dsr.rewind" })

        let live = Task { await model.goLive() }
        let (liveIndex, goLive) = try #require(await Self.invoke(rig, "dsr.live"))
        #expect(goLive.args == [LinkMessage.PropertyEntry(name: "slice", value: .i64(0))])
        await RecordingFlagTests.answer(rig, liveIndex)
        await live.value
        #expect(model.viewTop == nil && !model.inPast)
        RecordingFlagTests.publish(rig, Self.document(dsr: Self.dsrPart()), revision: 3)
        model.documentChanged()
        #expect(!model.liveLit)
    }

    /// No active slice: letting go plays nothing and says so. The Core's
    /// refusal (nothing kept at that frequency then) shows its words and
    /// leaves the picture in the past; a tap on a signal there then plays
    /// from the picture's top at the tapped frequency.
    @Test func whyItCannotPlayShowsAndATapThenPlays() async throws {
        let (rig, model, _) = await Self.pan()
        var slice: Int?
        model.activeSlice = { slice }
        model.drag(from: model.top, by: -1_000)
        model.dragEnded()
        #expect(model.note == DsrPanModel.noSliceReason)
        slice = 0
        model.dragEnded()
        let (index, _) = try #require(await Self.invoke(rig, "dsr.rewind"))
        let notHere = "Nothing was kept at this frequency then."
        await RecordingFlagTests.answer(rig, index, accepted: false, reason: notHere)
        #expect(await Eventually.holds { model.note == notHere })
        #expect(model.inPast && model.mine.isEmpty)
        let top = model.top
        #expect(!model.tapWaterfall(pointsDown: 20, hz: 7_238_000))
        let (second, again) = try #require(await Self.invoke(rig, "dsr.rewind", after: index + 1))
        #expect(Self.argument(again, "utcNs") == .i64(Int64((top * 1e9).rounded())))
        #expect(Self.argument(again, "frequencyHz") == .i64(7_238_000))
        await RecordingFlagTests.answer(rig, second)
        #expect(await Eventually.holds { model.mine.contains(0) })
    }

    /// After a refusal, any tune of the active slice (a drag, the knob, a
    /// typed frequency, as AppModel feeds them) plays from the picture's top
    /// at the tuned frequency, one rewind at a time. Another slice becoming
    /// active is not a tune; live, a tune is only a tune.
    @Test func aTuneInThePastPlaysFromTheTop() async throws {
        let (rig, model, _) = await Self.pan()
        model.sliceTuned(slice: 0, hz: 7_240_500)
        model.sliceTuned(slice: 0, hz: 7_240_600)
        try? await Task.sleep(for: .milliseconds(50))
        #expect(!rig.sent.invokes.contains { $0.verb == "dsr.rewind" })
        model.drag(from: model.top, by: -1_000)
        model.dragEnded()
        let (index, _) = try #require(await Self.invoke(rig, "dsr.rewind"))
        await RecordingFlagTests.answer(rig, index, accepted: false, reason: DsrPanModel.noFrequencyReason)
        #expect(await Eventually.holds { model.note == DsrPanModel.noFrequencyReason })
        #expect(model.inPast && model.mine.isEmpty)
        let top = model.top
        model.sliceTuned(slice: 0, hz: 7_238_100)
        model.sliceTuned(slice: 0, hz: 7_238_200)
        let (second, again) = try #require(await Self.invoke(rig, "dsr.rewind", after: index + 1))
        #expect(Self.argument(again, "utcNs") == .i64(Int64((top * 1e9).rounded())))
        #expect(Self.argument(again, "frequencyHz") == .i64(7_238_100))
        // The knob went on while the Core had the rewind: no second one.
        #expect(rig.sent.invokes.dropFirst(second + 1).allSatisfy { $0.verb != "dsr.rewind" })
        await RecordingFlagTests.answer(rig, second)
        #expect(await Eventually.holds { model.mine.contains(0) })
    }

    /// Playing this device's rewind, a tune outside what was kept at that
    /// moment says so on the waterfall at once; back inside, the words go.
    /// An older Core that does not say what was kept shows nothing.
    @Test func tuningOutsideWhatWasKeptSaysSo() async throws {
        let (rig, model, _) = await Self.pan()
        var hz = 7_240_500.0
        model.activeSliceHz = { hz }
        let moment = Self.now - 600
        try await Self.rewindHere(rig, model, moment: moment, revision: 2)
        // Kept then: 7.236 MHz, 48 kHz wide (7.212 to 7.260 MHz).
        let kept: [String: Any] = ["slice": 0, "utcNs": Int64((moment * 1e9).rounded()), "frequencyHz": 7_240_500,
                                   "liveFrequencyHz": 7_236_400, "keptCentreHz": 7_236_000, "keptSpanHz": 48_000]
        RecordingFlagTests.publish(rig, Self.document(dsr: Self.dsrPart(rewound: [kept])), revision: 3)
        model.documentChanged()
        #expect(model.followsSound && model.keptNote == nil)
        model.sliceTuned(slice: 0, hz: hz)
        hz = 7_270_000
        model.sliceTuned(slice: 0, hz: hz)
        #expect(model.keptNote == DsrPanModel.noFrequencyReason)
        model.documentChanged()
        #expect(model.keptNote == DsrPanModel.noFrequencyReason)
        hz = 7_250_000
        model.sliceTuned(slice: 0, hz: hz)
        #expect(model.keptNote == nil)
        // An older Core: nothing said about what was kept.
        try await Self.rewindHere(rig, model, moment: moment, revision: 4)
        hz = 14_074_000
        model.sliceTuned(slice: 0, hz: hz)
        #expect(model.keptNote == nil)
    }

    /// Rewinds Slice A from this device at `moment` and publishes it rewound.
    static func rewindHere(_ rig: Rig, _ model: DsrPanModel, moment: Double, slice: Int = 0,
                           revision: Int64) async throws {
        let before = rig.sent.invokes.count
        let rewind = Task { await model.rewind(slice: slice, moment: moment, hz: 7_240_500) }
        let (index, _) = try #require(await invoke(rig, "dsr.rewind", after: before))
        await RecordingFlagTests.answer(rig, index)
        await rewind.value
        let entry: [String: Any] = ["slice": slice, "utcNs": Int64((moment * 1e9).rounded()), "frequencyHz": 7_240_500,
                                    "liveFrequencyHz": 7_236_400]
        RecordingFlagTests.publish(rig, document(dsr: dsrPart(rewound: [entry])), revision: revision)
        model.documentChanged()
    }

    /// The Core ends a rewind when you key: the picture goes live and LIVE
    /// stays red until pressed, which still sends the slice home. After that
    /// LIVE, a second rewind and a second key leave LIVE red again.
    @Test func aKeyWhileRewoundTakesItLive() async throws {
        let (rig, model, _) = await Self.pan()
        for round in 0..<2 {
            try await Self.rewindHere(rig, model, moment: Self.now - 600, revision: Int64(10 * round + 2))
            #expect(model.inPast && model.followsSound)
            let before = rig.sent.invokes.count
            RecordingFlagTests.publish(rig, Self.document(dsr: Self.dsrPart()), revision: Int64(10 * round + 3))
            model.documentChanged()
            #expect(model.viewTop == nil && model.rewound == nil && model.liveLit && model.liveOwed)
            #expect(!rig.sent.invokes.dropFirst(before).contains { $0.verb == "dsr.live" })
            let live = Task { await model.goLive() }
            let (index, _) = try #require(await Self.invoke(rig, "dsr.live", after: before))
            await RecordingFlagTests.answer(rig, index)
            await live.value
            #expect(!model.liveLit)
        }
    }

    /// Another device's rewind of the active slice never moves this picture;
    /// the Core's refusals of a rewind (letting go plays) or LIVE on a slice
    /// this device does not own show its words and leave the picture where
    /// it was put.
    @Test func anotherDevicesRewindLeavesThePictureAlone() async throws {
        let theirs = Self.rewound(Int64((Self.now - 600) * 1e9))
        let (rig, model, _) = await Self.pan(dsr: Self.dsrPart(rewound: theirs))
        model.documentChanged()
        model.tick()
        #expect(model.viewTop == nil && !model.followsSound && model.playedMoment == nil)
        model.drag(from: model.top, by: -200)
        model.dragEnded()
        let put = try #require(model.viewTop)
        model.tick()
        #expect(model.viewTop == put)
        let refusal = "Slice A belongs to another device."
        let (index, _) = try #require(await Self.invoke(rig, "dsr.rewind"))
        await RecordingFlagTests.answer(rig, index, accepted: false, reason: refusal)
        #expect(await Eventually.holds { model.note == refusal })
        #expect(model.viewTop == put && model.mine.isEmpty)
        // LIVE takes this picture live and asks for the slice; the Core says no.
        let live = Task { await model.goLive() }
        let (liveIndex, _) = try #require(await Self.invoke(rig, "dsr.live"))
        await RecordingFlagTests.answer(rig, liveIndex, accepted: false, reason: refusal)
        await live.value
        #expect(model.note == refusal && model.viewTop == nil)
        // Their rewind ending owes nothing here.
        RecordingFlagTests.publish(rig, Self.document(dsr: Self.dsrPart()), revision: 2)
        model.documentChanged()
        #expect(!model.liveLit)
    }

    /// A slice rewound here stays in LIVE's reach after another slice becomes active.
    @Test func liveReachesASliceRewoundBeforeTheActiveOneChanged() async throws {
        let (rig, model, _) = await Self.pan()
        var active = 0
        model.activeSlice = { active }
        try await Self.rewindHere(rig, model, moment: Self.now - 600, revision: 2)
        active = 1
        #expect(model.liveLit && !model.followsSound)
        let live = Task { await model.goLive() }
        let (index, sent) = try #require(await Self.invoke(rig, "dsr.live"))
        #expect(sent.args == [LinkMessage.PropertyEntry(name: "slice", value: .i64(0))])
        await RecordingFlagTests.answer(rig, index)
        await live.value
        RecordingFlagTests.publish(rig, Self.document(dsr: Self.dsrPart()), revision: 3)
        model.documentChanged()
        #expect(!model.liveLit)
    }

    /// A link drop owes no LIVE, before or after the reconnect, and a call
    /// made while not connected says so.
    @Test func aDropOwesNoLive() async throws {
        let (rig, model, _) = await Self.pan()
        try await Self.rewindHere(rig, model, moment: Self.now - 600, revision: 2)
        // The picture dragged live: LIVE is red for the slice still playing the past.
        model.drag(from: model.top, by: 10_000)
        model.dragEnded()
        #expect(model.viewTop == nil && model.liveLit)
        rig.client.handle(.stateChanged(.connecting))
        model.documentChanged()
        #expect(!model.liveLit && !model.liveOwed)
        await model.goLive()
        #expect(model.note == nil)
        await model.rewind(slice: 0, moment: Self.now - 600, hz: 7_240_500)
        #expect(model.note == DsrPanModel.notConnectedReason)
        rig.client.handle(.stateChanged(.receivingSnapshot))
        rig.client.handle(.stateChanged(.ready))
        RecordingFlagTests.publish(rig, Self.document(dsr: Self.dsrPart()), revision: 5)
        model.documentChanged()
        model.tick()
        #expect(!model.liveLit && !model.liveOwed && model.mine.isEmpty)
    }

    /// The hours map: half way down is half way back through what is kept,
    /// with the picture centred there, and its label says when.
    @Test func theHoursMapJumps() async throws {
        let (rig, model, _) = await Self.pan()
        model.askForHoursIfDue()
        let (index, _) = try #require(await Self.invoke(rig, "dsr.hours"))
        let starts: [Int64] = [1_763_298_000_000_000_000, 1_763_294_400_000_000_000, 1_763_290_800_000_000_000]
        await RecordingFlagTests.answer(rig, index, values: [
            .init(name: "starts", value: .utf8(DsrPastFrames.packTimes(starts).base64EncodedString())),
            .init(name: "activity", value: .utf8("[0.9,0.2,0.5]")),
        ])
        #expect(await Eventually.holds { model.hours.count == 3 })
        #expect(model.activity(atMapFraction: 0) == 0.9 && model.activity(atMapFraction: 0.75) == 0.5)
        let span = Self.now - Double(Self.oldestMs) / 1000
        #expect(model.mapLabel(fraction: 0.5) == "12:12 · 1 h 0 min ago")
        model.jump(toMapFraction: 0.5, commit: true)
        let halfPicture: Double = Self.points * Self.spp / 2
        let expectedTop: Double = Self.now - span / 2 + halfPicture
        #expect(model.viewTop == expectedTop)
        let mark = try #require(model.mapMark)
        let expectedLow: Double = 0.5 - halfPicture / span
        #expect(abs(mark.lowerBound - expectedLow) < 1e-9)
        #expect(await Self.invoke(rig, "dsr.rows") != nil)
    }

    /// SAVE puts the bracket up under the picture's top; its panel names
    /// the stretch and both sizes, and a save sends the stretch and shows
    /// the Core's note.
    @Test func saveShowsTheBracketAndThePanel() async throws {
        let (rig, model, _) = await Self.pan()
        model.toggleSave()
        let stretch = try #require(model.stretch)
        #expect(stretch.from == Self.now - 125 && stretch.to == Self.now - 35)
        #expect(model.saveTitle == "Save 13:09:55 to 13:11:25 UTC")
        #expect(model.saveDetail == "1 min 30 s. Drag the yellow handles on the time strip to change it.")
        #expect(model.wholePanSize == "69 MB, every signal")
        #expect(model.soundTitle == "Sound only (MP3)")
        #expect(model.soundSize(sliceFrequencyHz: 7_236_400) == "Slice A at 7.236.400, about 1 MB")
        model.moveHandle(0, toPointsDown: 10)
        #expect(model.stretch?.to == Self.now - 5)
        // A fast waterfall still brackets the shortest stretch, 30 s.
        model.cancelSave()
        model.secondsPerPoint = 0.05
        model.toggleSave()
        let fast = try #require(model.stretch)
        #expect(abs(fast.to - (Self.now - 3.5)) < 1e-6 && abs(fast.to - fast.from - 30) < 1e-6)
        model.secondsPerPoint = Self.spp
        model.cancelSave()
        model.toggleSave()
        model.moveHandle(0, toPointsDown: 10)
        let save = Task { await model.save(.sound) }
        let (index, sent) = try #require(await Self.invoke(rig, "dsr.saveStretch"))
        #expect(Self.argument(sent, "fromUtcNs") == .i64(Int64(((Self.now - 125) * 1e9).rounded())))
        #expect(Self.argument(sent, "kind") == .utf8("Sound"))
        await RecordingFlagTests.answer(rig, index, values: [.init(name: "note", value: .utf8("Saving to Recordings."))])
        await save.value
        #expect(!model.saving && model.note == "Saving to Recordings.")
    }

    /// The clock's sheet takes a typed time, says when one is too old, and
    /// says how to type one.
    @Test func theTimeSheetGoesToATypedTime() async {
        let (_, model, _) = await Self.pan()
        model.openTimeSheet()
        #expect(model.timeSheetOpen && model.typedStart == "13:02:00")
        #expect(model.keptText == "Kept from 11:12 to now (2 h 0 min). Older stretches you saved are in Tools, Recordings.")
        #expect(model.go(toTyped: "1305") == "Type a time like 13:05:00.")
        #expect(model.go(toTyped: "10:00") == "That is older than what is kept (from 11:12). Saved stretches are in "
                    + "Tools, Recordings.")
        #expect(model.go(toTyped: "12:00:00") == nil)
        #expect(!model.timeSheetOpen && model.viewTop == 1_763_294_400 + 95.75)
    }

    /// An older Core, the recorder off or not connected: every control is
    /// there, sends nothing and says why.
    @Test func withoutTheRecorderEveryControlSaysWhy() async {
        let cases: [(Int64, [String: Any]?, Bool, String)] = [
            (3, nil, true, RecordingsClient.updateReason),
            (4, Self.dsrPart(enabled: false, recording: false), true, DsrPanModel.offReason),
            (4, Self.dsrPart(recording: false, reason: "The recorder's disk is missing."), true,
             "The recorder's disk is missing."),
            (4, Self.dsrPart(), false, DsrPanModel.notConnectedReason),
        ]
        for (version, dsr, connected, reason) in cases {
            let (rig, model, _) = await Self.pan(version: version, dsr: dsr, connected: connected)
            #expect(model.reason == reason && !model.usable)
            model.toggleSave()
            #expect(!model.saving && model.note == reason)
            model.openTimeSheet()
            #expect(!model.timeSheetOpen)
            model.jump(toMapFraction: 0.5, commit: true)
            model.askForHoursIfDue()
            // The strip still moves through the band's own minute, and the Core is asked nothing.
            model.drag(from: model.top, by: -60)
            model.dragEnded()
            #expect(model.viewTop == Self.now - 30 && !model.coreDraws)
            try? await Task.sleep(for: .milliseconds(100))
            #expect(!rig.sent.invokes.contains { $0.verb.hasPrefix("dsr.") })
        }
    }

    static func count(_ rig: Rig, _ verb: String) -> Int {
        rig.sent.invokes.filter { $0.verb == verb }.count
    }

    /// Moments the Core leaves out, or refuses, are not asked again until
    /// the display changes; "Drawing" shows only while an ask is with the Core.
    @Test func momentsNotKeptAreNotAskedAgain() async throws {
        let (rig, model, _) = await Self.pan()
        model.drag(from: model.top, by: -1_000)
        model.dragEnded()
        let (first, _) = try #require(await Self.invoke(rig, "dsr.rows"))
        let top = model.top
        let moments = (0..<100).map { top - Double($0) * Self.spp }
        await RecordingFlagTests.answer(rig, first, values: Self.rows(moments, rowsLeft: 0, next: 0))
        #expect(await Eventually.holds { !model.awaitingRows && model.row(at: moments[50]) != nil })
        #expect(model.drawingText == nil && model.missingShare == 0)
        model.dragEnded()
        model.tick()
        try? await Task.sleep(for: .milliseconds(50))
        #expect(Self.count(rig, "dsr.rows") == 1)
        // Another display asks again; a refusal is remembered the same way.
        model.panView = { DsrPanView(fftSize: 4096, windowType: 1, centreHz: 7_236_000, spanHz: 96_000, pixels: 402) }
        model.dragEnded()
        let (second, _) = try #require(await Self.invoke(rig, "dsr.rows", after: first + 1))
        await RecordingFlagTests.answer(rig, second, accepted: false, reason: "Nothing is kept there.")
        #expect(await Eventually.holds { !model.awaitingRows })
        #expect(model.note == "Nothing is kept there." && model.drawingText == nil)
        model.dragEnded()
        try? await Task.sleep(for: .milliseconds(50))
        #expect(Self.count(rig, "dsr.rows") == 2)
    }

    /// While the sound plays the picture's top moves every display frame
    /// and the past spectrum is asked once a frame, never two at a time;
    /// an answer that comes a frame or more late asks again at once, for the
    /// moment playing then. The 250 ms tick asks for none.
    @Test func thePastSpectrumFollowsTheDisplayFrames() async throws {
        let (rig, model, _) = await Self.pan()
        var now = Self.now
        model.clock = { now }
        let period = 1.0 / 60
        try await Self.rewindHere(rig, model, moment: Self.now - 600, revision: 2)
        let (first, _) = try #require(await Self.invoke(rig, "dsr.spectrum"))
        // The first ask is still with the Core: no second.
        model.frame(periodSeconds: period)
        model.tick()
        try? await Task.sleep(for: .milliseconds(50))
        #expect(Self.count(rig, "dsr.spectrum") == 1)
        await RecordingFlagTests.answer(rig, first, values: Self.rows([Self.now - 600], rowsLeft: 0, next: 0))
        #expect(await Eventually.holds { model.pastSpectrum != nil })
        // Answered within the frame: nothing more until the next frame.
        try? await Task.sleep(for: .milliseconds(50))
        #expect(Self.count(rig, "dsr.spectrum") == 1)
        now += period
        model.frame(periodSeconds: period)
        #expect(abs((model.viewTop ?? 0) - (Self.now - 600 + period)) < 1e-6)
        let (second, ask) = try #require(await Self.invoke(rig, "dsr.spectrum", after: first + 1))
        #expect(Self.argument(ask, "utcNs") == .i64(Int64(((Self.now - 600 + period) * 1e9).rounded())))
        // Frames go on while it is with the Core: the picture moves, no second ask.
        for _ in 0..<5 {
            now += period
            model.frame(periodSeconds: period)
        }
        #expect(abs((model.viewTop ?? 0) - (Self.now - 600 + 6 * period)) < 1e-6)
        try? await Task.sleep(for: .milliseconds(50))
        #expect(Self.count(rig, "dsr.spectrum") == 2)
        // Late: the next is asked at once, for the moment playing now, without waiting for a frame.
        await RecordingFlagTests.answer(rig, second, values: Self.rows([Self.now - 600], rowsLeft: 0, next: 0))
        let (_, again) = try #require(await Self.invoke(rig, "dsr.spectrum", after: second + 1))
        #expect(Self.argument(again, "utcNs") == .i64(Int64(((Self.now - 600 + 6 * period) * 1e9).rounded())))
        // The tick never asks for the spectrum.
        let asked = Self.count(rig, "dsr.spectrum")
        now += period / 2
        model.tick()
        try? await Task.sleep(for: .milliseconds(50))
        #expect(Self.count(rig, "dsr.spectrum") == asked)
    }

    /// Playing at a steady pace near the edge of the band's own minute,
    /// whose reach moves by a line as lines come and go, the picture stays
    /// with whoever drew it: the Core's rows never swap with the band's own.
    /// Brought well inside the band's own minute, the band draws it again.
    @Test func whoDrawsThePictureDoesNotFlipWhilePlaying() async throws {
        let (rig, model, _) = await Self.pan()
        var now = Self.now
        model.clock = { now }
        var reach = 60.0
        model.ownSeconds = { reach }
        // Just past the band's own reach (a quarter of a point of slack, at 0.5 s a point).
        try await Self.rewindHere(rig, model, moment: Self.now - 60.3, revision: 2)
        #expect(model.coreDraws && model.followsSound)
        var seen: Set<Bool> = []
        for step in 0..<120 {
            now += 1.0 / 60
            // The band's reach steps by a line either way as its lines arrive and drop.
            reach = step % 2 == 0 ? 60 + Self.spp : 60 - Self.spp
            model.frame(periodSeconds: 1.0 / 60)
            if step % 15 == 0 {
                model.tick()
            }
            seen.insert(model.coreDraws)
        }
        #expect(seen == [true])
        // Within the band's own minute by more than the slack, the band draws it.
        reach = 60
        model.drag(from: model.top, by: 30 / Self.spp)
        #expect(!model.coreDraws && model.inPast)
    }

    /// While the past shows the live spectrum is hidden, also before the
    /// first past answer; live again it shows.
    @Test func theLiveSpectrumHidesWhileThePastShows() async throws {
        let (rig, model, _) = await Self.pan()
        #expect(!model.hidesLiveSpectrum)
        try await Self.rewindHere(rig, model, moment: Self.now - 600, revision: 2)
        #expect(model.hidesLiveSpectrum && model.pastSpectrum == nil)
        let live = Task { await model.goLive() }
        let (index, _) = try #require(await Self.invoke(rig, "dsr.live"))
        await RecordingFlagTests.answer(rig, index)
        await live.value
        #expect(!model.hidesLiveSpectrum)
    }

    /// The time strip redraws every display frame only in the past or while
    /// this device's rewind plays; live, and live again after LIVE, it keeps
    /// its old pace (the phone's heat). Another device's rewind leaves it live.
    @Test func theStripRunsEveryFrameOnlyInThePast() async throws {
        let (rig, model, _) = await Self.pan()
        model.tick()
        #expect(!model.inPast && !model.stripRunsEveryFrame)
        // Dragged back, paused in the past.
        model.drag(from: model.top, by: -200)
        #expect(model.inPast && model.stripRunsEveryFrame)
        model.dragEnded()
        let (index, _) = try #require(await Self.invoke(rig, "dsr.rewind"))
        await RecordingFlagTests.answer(rig, index, accepted: false, reason: "No.")
        #expect(await Eventually.holds { model.note == "No." })
        #expect(model.inPast && !model.followsSound && model.stripRunsEveryFrame)
        // Nothing plays here, so LIVE asks the Core for nothing.
        await model.goLive()
        #expect(!model.inPast && !model.stripRunsEveryFrame)
        // This device's rewind playing.
        try await Self.rewindHere(rig, model, moment: Self.now - 600, revision: 2)
        model.frame(periodSeconds: DsrPanModel.defaultFrameSeconds)
        #expect(model.followsSound && model.stripRunsEveryFrame)
        let before = rig.sent.invokes.count
        let live = Task { await model.goLive() }
        let (liveIndex, _) = try #require(await Self.invoke(rig, "dsr.live", after: before))
        await RecordingFlagTests.answer(rig, liveIndex)
        await live.value
        RecordingFlagTests.publish(rig, Self.document(dsr: Self.dsrPart()), revision: 3)
        model.documentChanged()
        model.tick()
        #expect(!model.inPast && !model.stripRunsEveryFrame)
        // Another device's rewind of the active slice: the picture stays live.
        let theirs = Self.rewound(Int64((Self.now - 600) * 1e9))
        RecordingFlagTests.publish(rig, Self.document(dsr: Self.dsrPart(rewound: theirs)), revision: 4)
        model.documentChanged()
        model.tick()
        #expect(model.rewound != nil && !model.inPast && !model.stripRunsEveryFrame)
    }

    /// The hours are asked once the recorder is usable, not again on every
    /// tick, and again after an erase or a reconnect, which also drop the
    /// hours and the past drawn.
    @Test func theHoursFollowTheRecorder() async throws {
        let (rig, model, _) = await Self.pan(dsr: Self.dsrPart(recording: false))
        model.tick()
        try? await Task.sleep(for: .milliseconds(50))
        #expect(Self.count(rig, "dsr.hours") == 0)
        RecordingFlagTests.publish(rig, Self.document(dsr: Self.dsrPart()), revision: 2)
        model.tick()
        let (index, _) = try #require(await Self.invoke(rig, "dsr.hours"))
        let starts: [Int64] = [1_763_298_000_000_000_000, 1_763_294_400_000_000_000]
        let hours: [LinkMessage.PropertyEntry] = [
            .init(name: "starts", value: .utf8(DsrPastFrames.packTimes(starts).base64EncodedString())),
            .init(name: "activity", value: .utf8("[0.9,0.2]")),
        ]
        await RecordingFlagTests.answer(rig, index, values: hours)
        #expect(await Eventually.holds { model.hours.count == 2 })
        model.tick()
        model.tick()
        try? await Task.sleep(for: .milliseconds(50))
        #expect(Self.count(rig, "dsr.hours") == 1)
        // Rows drawn, then erased on the Core.
        model.drag(from: model.top, by: -1_000)
        model.dragEnded()
        let (rowsIndex, _) = try #require(await Self.invoke(rig, "dsr.rows"))
        await RecordingFlagTests.answer(rig, rowsIndex, values: Self.rows([model.top], rowsLeft: 0, next: 0))
        #expect(await Eventually.holds { !model.rows.isEmpty })
        var erased = Self.dsrPart()
        erased["usedBytes"] = 1_000_000
        erased["oldestUtcMs"] = Int64(Self.now * 1000) - 5_000
        RecordingFlagTests.publish(rig, Self.document(dsr: erased), revision: 3)
        model.tick()
        #expect(model.rows.isEmpty && model.hours.isEmpty)
        let (again, _) = try #require(await Self.invoke(rig, "dsr.hours", after: index + 1))
        await RecordingFlagTests.answer(rig, again, values: hours)
        #expect(await Eventually.holds { model.hours.count == 2 })
        // A reconnect drops them and asks again.
        rig.client.handle(.stateChanged(.connecting))
        rig.client.handle(.stateChanged(.receivingSnapshot))
        rig.client.handle(.stateChanged(.ready))
        RecordingFlagTests.publish(rig, Self.document(dsr: erased), revision: 4)
        model.tick()
        #expect(model.hours.isEmpty)
        #expect(await Self.invoke(rig, "dsr.hours", after: again + 1) != nil)
    }

    /// A second tap while a save is with the Core sends nothing; the
    /// handles move by a step without a drag.
    @Test func oneSaveAtATimeAndTheHandlesStep() async throws {
        let (rig, model, _) = await Self.pan()
        model.toggleSave()
        model.stepHandle(0, later: true)
        model.stepHandle(1, later: false)
        #expect(model.stretch?.to == Self.now - 30 && model.stretch?.from == Self.now - 130)
        let save = Task { await model.save(.wholePan) }
        let (index, _) = try #require(await Self.invoke(rig, "dsr.saveStretch"))
        #expect(model.saveInFlight)
        await model.save(.wholePan)
        #expect(Self.count(rig, "dsr.saveStretch") == 1)
        await RecordingFlagTests.answer(rig, index)
        await save.value
        #expect(!model.saveInFlight && !model.saving)
    }

    // MARK: Setup

    static func setup(version: Int64 = 4, dsr: [String: Any]? = dsrPart(), disks: [[String: Any]] = [],
                      connected: Bool = true) async -> (Rig, DsrSetupModel) {
        let rig = await RecordingFlagTests.rig(version: version, connected: connected)
        if version >= 4 {
            RecordingFlagTests.publish(rig, document(dsr: dsr, disks: disks), revision: 1)
        }
        return (rig, DsrSetupModel(client: rig.client))
    }

    static let ssd: [String: Any] = ["root": "/media/ssd", "name": "Samsung T7", "free": 812_000_000_000,
                                     "total": 1_000_000_000_000, "sd": false, "own": false]
    static let sdCard: [String: Any] = ["root": "/", "name": "", "free": 20_000_000_000, "total": 32_000_000_000,
                                        "sd": true, "own": true]

    @Test func theSetupPageReadsTheRecorder() async {
        let (_, model) = await Self.setup(disks: [Self.sdCard, Self.ssd])
        #expect(model.reason == nil && !model.onlySdCard && model.enabled)
        #expect(model.statusText == "Recording. Reaches back 2 h 0 min.")
        #expect(model.recordsText == "Receiver 1, the whole 192 kHz it hears. About 2.8 GB an hour.")
        #expect(model.holdsText == "Holds about 3 h 37 min at this pan width.")
        #expect(model.fullText == "Full. The oldest part is erased as each new second comes in.")
        #expect(model.meter.kept == 1 && model.meter.head == 0.63 && model.meter.erasedNext > 0)
        #expect(model.diskChoices.map(\.text) == ["SD card (20 GB free)", "Samsung T7 (812 GB free)"])
        #expect(model.eraseQuestion == "The last 2 h 0 min of the pan goes. Saved stretches in Recordings stay. "
                    + "Recording carries on from now.")
    }

    /// A Core whose only disk is an SD card: the warning, and the switch off.
    @Test func anSdCardCoreShowsTheWarning() async {
        let (_, model) = await Self.setup(dsr: Self.dsrPart(recording: false, reason: DsrSetupModel.onlySdReason),
                                          disks: [Self.sdCard])
        #expect(model.onlySdCard && !model.canErase)
        #expect(model.statusText == "Off. This Core has no proper storage device.")
        #expect(model.fullText == "Nothing is kept on the SD card.")
        #expect(model.stoppedReason == nil)
        // Turned off on such a Core, the warning still shows from the disks.
        let (_, off) = await Self.setup(dsr: Self.dsrPart(enabled: false, recording: false), disks: [Self.sdCard])
        #expect(off.onlySdCard)
    }

    /// Each change sends its one field; erase sends `dsr.eraseAll`.
    @Test func theSetupPageSendsOneFieldEachAndErases() async throws {
        let (rig, model) = await Self.setup(disks: [Self.ssd])
        let off = Task { await model.setEnabled(false) }
        let (index, sent) = try #require(await Self.invoke(rig, "recordings.setSettings"))
        // Shown at once, before the Core answers.
        #expect(model.enabled == false)
        #expect(sent.args == [LinkMessage.PropertyEntry(name: "dsrEnabled", value: .bool(false))])
        await RecordingFlagTests.answer(rig, index)
        await off.value
        model.draggedSizeGb = 25
        #expect(model.shownSizeGb == 25 && model.holdsText == "Holds about 9 h 2 min at this pan width.")
        let size = Task { await model.commitSize() }
        let (sizeIndex, sizeSent) = try #require(await Self.invoke(rig, "recordings.setSettings", after: index + 1))
        #expect(sizeSent.args == [LinkMessage.PropertyEntry(name: "dsrSizeLimitGb", value: .i64(25))])
        await RecordingFlagTests.answer(rig, sizeIndex)
        await size.value
        let erase = Task { await model.eraseAll() }
        let (eraseIndex, eraseSent) = try #require(await Self.invoke(rig, "dsr.eraseAll"))
        #expect(eraseSent.args.isEmpty)
        await RecordingFlagTests.answer(rig, eraseIndex)
        await erase.value
        #expect(model.erasedNote == "Erased. The recorder starts again from now." && model.refusal == nil)
        // A reconnect clears it.
        rig.client.handle(.stateChanged(.connecting))
        rig.client.handle(.stateChanged(.receivingSnapshot))
        rig.client.handle(.stateChanged(.ready))
        #expect(model.erasedNote == nil)
    }

    /// The Size slider reaches down to the Core's least, 1 GB.
    @Test func theSizeSliderStartsAtTheCoresLeast() async {
        let (_, model) = await Self.setup()
        #expect(DsrSetupModel.sizeRange.lowerBound == 1 && model.sliderRange.lowerBound == 1)
    }

    @Test func anOlderCoreOrNotConnectedGreysTheSetupPage() async {
        let (older, model) = await Self.setup(version: 3)
        #expect(model.reason == RecordingsClient.updateReason && !model.canErase)
        await model.setEnabled(true)
        await model.eraseAll()
        #expect(older.sent.invokes.isEmpty)
        let (_, away) = await Self.setup(connected: false)
        #expect(away.notConnected && away.reason == DsrSetupModel.notConnectedTitle)
        #expect(away.statusText == "Unknown while not connected.")
    }
}
