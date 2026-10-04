// NereusSDR for iOS: tests for the waterfall's history, the band-plan strip and the band's state
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Testing
import NereusMedia
@testable import NereusBand

/// R-IOS-11: the waterfall's lines, the strip, and a frame waiting for its datagram.
@Suite struct WaterfallHistoryTests {
    static let manual = WaterfallHistory.Levels(lowDbm: -122, highDbm: -62)

    @Test func linesAreNewestFirstAndEachKeepsItsOwnLevels() {
        var history = WaterfallHistory(capacity: 3)
        let first = BandFixtures.frame(trace: [-50, -50], sequence: 1)
        history.append(frame: first, extras: BandFixtures.extras(for: first, levels: (-100, 0)), manualLevels: Self.manual)
        let second = BandFixtures.frame(trace: [-50, -50], sequence: 2)
        history.append(frame: second, extras: BandFixtures.extras(for: second, levels: (-50, 0)), manualLevels: Self.manual)
        #expect(history.count == 2)
        #expect(Array(history.line(age: 0) ?? []) == [0, 0])
        #expect(Array(history.line(age: 1) ?? []) == [128, 128])
        #expect(history.line(age: 2) == nil)
    }

    @Test func eachLineKeepsTheFrequenciesItCoversThroughAResize() {
        var history = WaterfallHistory(capacity: 3)
        let a = BandCoverage(centerHz: 7_200_000, spanHz: 48_000)
        let b = BandCoverage(centerHz: 7_210_000, spanHz: 48_000)
        history.append(line: [-50], levels: Self.manual, coverage: a)
        history.append(line: [-50], levels: Self.manual, coverage: b)
        #expect(history.coverage(storageRow: history.storageRow(age: 0)) == b)
        #expect(history.coverage(storageRow: history.storageRow(age: 1)) == a)
        history.resize(capacity: 5)
        #expect(history.coverage(storageRow: history.storageRow(age: 0)) == b)
        #expect(history.coverage(storageRow: history.storageRow(age: 1)) == a)
    }

    @Test func aFrameIsCoveredByItsOwnContext() {
        var state = BandState(waterfallLines: 4)
        let first = BandCoverage(centerHz: 7_200_000, spanHz: 48_000)
        let second = BandCoverage(centerHz: 7_212_000, spanHz: 48_000)
        state.note(context: 1, coverage: first)
        state.note(context: 2, coverage: second)
        // A frame of the older context still in flight is drawn where it was.
        state.receive(frame: BandFixtures.frame(trace: [-50], sequence: 1, generation: 1), manualLevels: Self.manual)
        #expect(state.frameCoverage == first)
        state.receive(frame: BandFixtures.frame(trace: [-50], sequence: 2, generation: 2), manualLevels: Self.manual)
        #expect(state.frameCoverage == second)
        #expect(state.history.coverage(storageRow: state.history.storageRow(age: 0)) == second)
        #expect(state.history.coverage(storageRow: state.history.storageRow(age: 1)) == first)
        // An unknown context: drawn across the view, as before.
        state.receive(frame: BandFixtures.frame(trace: [-50], sequence: 3, generation: 9), manualLevels: Self.manual)
        #expect(state.frameCoverage == nil)
    }

    @Test func aFrameWithoutItsDatagramKeepsTheLastLevels() {
        var history = WaterfallHistory(capacity: 4)
        let first = BandFixtures.frame(trace: [-50], sequence: 1)
        history.append(frame: first, extras: BandFixtures.extras(for: first, levels: (-100, 0)), manualLevels: Self.manual)
        history.append(frame: BandFixtures.frame(trace: [-50], sequence: 2), extras: nil, manualLevels: Self.manual)
        #expect(Array(history.line(age: 0) ?? []) == [128])
        // Another frame's datagram is not this frame's.
        let third = BandFixtures.frame(trace: [-50], sequence: 3)
        let stray = BandFixtures.extras(for: BandFixtures.frame(trace: [-50], sequence: 9), levels: (-50, 0))
        history.append(frame: third, extras: stray, manualLevels: Self.manual)
        #expect(Array(history.line(age: 0) ?? []) == [128])
    }

    @Test func withoutTheCoresLevelsTheManualOnesApply() {
        var history = WaterfallHistory(capacity: 2)
        history.append(frame: BandFixtures.frame(trace: [-122, -92, -62, -30]), extras: nil, manualLevels: Self.manual)
        #expect(Array(history.line(age: 0) ?? []) == [0, 128, 255, 255])
        #expect(history.currentLevels == Self.manual)
    }

    @Test func onlyAFrameThatAdvancesTheWaterfallAddsALine() {
        var history = WaterfallHistory(capacity: 2)
        let added = history.append(frame: BandFixtures.frame(trace: [-60], advance: false), extras: nil,
                                   manualLevels: Self.manual)
        #expect(!added)
        #expect(history.count == 0)
    }

    @Test func resizingKeepsTheNewest() {
        var history = WaterfallHistory(capacity: 3)
        for value: Float in [-122, -92, -62, -122] {
            history.append(line: [value, value], levels: Self.manual)
        }
        #expect(history.count == 3)
        #expect(history.linesAppended == 4)
        let layout = history.layoutGeneration
        history.resize(capacity: 2)
        #expect(history.count == 2)
        #expect(Array(history.line(age: 0) ?? []) == [0, 0])
        #expect(Array(history.line(age: 1) ?? []) == [255, 255])
        #expect(history.layoutGeneration != layout)
    }

    /// Turning the phone changes the band's width, so the Core's lines come
    /// wider or narrower: the history carries on, re-mapped to the new
    /// width, each line where its frequencies were.
    @Test func aNewWidthKeepsTheHistoryReMappedToIt() {
        var history = WaterfallHistory(capacity: 4)
        let older = BandCoverage(centerHz: 7_200_000, spanHz: 48_000)
        let newer = BandCoverage(centerHz: 7_210_000, spanHz: 48_000)
        // Four samples: a signal in the second, a weaker one in the fourth.
        history.append(line: [-122, -62, -122, -92], levels: Self.manual, coverage: older)
        history.append(line: [-62, -122, -122, -122], levels: Self.manual, coverage: newer)
        let layout = history.layoutGeneration
        let appended = history.linesAppended

        // Upright to sideways: twice as wide.
        history.append(line: [-122, -122, -122, -122, -122, -122, -122, -62], levels: Self.manual, coverage: newer)
        #expect(history.columns == 8)
        #expect(history.count == 3)
        #expect(history.linesAppended == appended + 1)
        #expect(history.layoutGeneration != layout)
        #expect(Array(history.line(age: 0) ?? []) == [0, 0, 0, 0, 0, 0, 0, 255])
        #expect(Array(history.line(age: 1) ?? []) == [255, 255, 0, 0, 0, 0, 0, 0])
        #expect(Array(history.line(age: 2) ?? []) == [0, 0, 255, 255, 0, 0, 128, 128])
        #expect(history.coverage(storageRow: history.storageRow(age: 1)) == newer)
        #expect(history.coverage(storageRow: history.storageRow(age: 2)) == older)

        // And back: half as wide, each sample the strongest it covers.
        history.append(line: [-122, -122, -122, -122], levels: Self.manual)
        #expect(history.columns == 4)
        #expect(history.count == 4)
        #expect(Array(history.line(age: 1) ?? []) == [0, 0, 0, 255])
        #expect(Array(history.line(age: 3) ?? []) == [0, 255, 0, 128])
    }

    @Test func theStripPlacesAPlansSegmentsAtTheirFrequencies() throws {
        let plan = try BandFixtures.plan(id: "t", isDefault: true, [
            (7_000_000, 7_100_000, "LOW", "#AA0000"),
            (7_100_000, 7_300_000, "HIGH", "#00AA00"),
            (8_000_000, 8_100_000, "AWAY", "#0000AA"),
        ])
        let geometry = BandGeometry(centerHz: 7_100_000, spanHz: 400_000, size: CGSize(width: 800, height: 100),
                                    dbmRange: -140 ... -40)
        let strip = BandPlanStrip(plan: plan, geometry: geometry)
        #expect(strip.pieces.map(\.label) == ["LOW", "HIGH"])
        #expect(strip.pieces[0].lowX == 200)
        #expect(strip.pieces[0].highX == 400)
        #expect(strip.pieces[0].labelX == 300)
        // Clipped to the span, labelled at the middle of what shows.
        #expect(strip.pieces[1].lowX == 400)
        #expect(strip.pieces[1].highX == 800)
        #expect(strip.pieces[1].labelX == 600)
        #expect(BandPlanStrip(plan: nil, geometry: geometry).pieces.isEmpty)
    }

    @Test func aFrameWaitsForItsDatagramBeforeItsLineIsColoured() {
        var state = BandState(waterfallLines: 4, expectsExtras: true)
        let frame = BandFixtures.frame(trace: [-50], sequence: 1)
        state.receive(frame: frame, manualLevels: Self.manual)
        #expect(state.history.count == 0)
        state.receive(extras: BandFixtures.extras(for: frame, levels: (-100, 0)), manualLevels: Self.manual)
        #expect(state.frame == frame)
        #expect(state.frameExtras?.waterfallLevels?.lowDbm == -100)
        #expect(Array(state.history.line(age: 0) ?? []) == [128])
        // One that never gets its datagram is committed by the next frame.
        state.receive(frame: BandFixtures.frame(trace: [-50], sequence: 2), manualLevels: Self.manual)
        state.receive(frame: BandFixtures.frame(trace: [-50], sequence: 3), manualLevels: Self.manual)
        #expect(state.history.count == 2)
        #expect(state.frameExtras == nil)
        // Or by drawing.
        state.commit(manualLevels: Self.manual)
        #expect(state.history.count == 3)
        #expect(state.frame?.encoderSequence == 3)
    }

    @Test func withNoExtrasExpectedAFrameIsCommittedAtOnce() {
        var state = BandState(waterfallLines: 4)
        state.receive(frame: BandFixtures.frame(trace: [-92]), manualLevels: Self.manual)
        #expect(Array(state.history.line(age: 0) ?? []) == [128])
    }
}
