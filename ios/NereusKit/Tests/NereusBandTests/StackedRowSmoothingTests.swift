// NereusSDR for iOS: tests for the 3D view's rows smoothed as they go in
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMedia
import Testing
@testable import NereusBand

/// Each 3D row is smoothed as it joins the stack, as the desktop's 3D
/// smooths its rows: the middle of the last three, then a quarter of each
/// neighbour and half of itself, then 0.6 of the row and 0.4 of the row
/// before. Every expected value here is worked by hand.
@Suite struct StackedRowSmoothingTests {
    static let origin = StackedRowSmoother.Origin(endpointId: 1, contextGeneration: 1,
        window: BandCoverage(centerHz: 7_200_000, spanHz: 100_000), wide: nil)
    static let line = [StackedRowSmoother.Source](repeating: .line, count: 5)

    static func expect(_ row: [Float], _ wanted: [Float], _ comment: Comment) {
        #expect(row.count == wanted.count)
        for (got, want) in zip(row, wanted) {
            #expect(abs(got - want) < 1e-4, comment)
        }
    }

    @Test func threeRowsWorkedByHand() {
        var smoother = StackedRowSmoother()
        // The first row: nothing to take the middle of, nothing before it.
        // 0.25 * -100 + 0.5 * -100 + 0.25 * -40 = -85, and so on; the ends
        // stand in for their missing neighbour.
        let first = smoother.smooth([-100, -100, -40, -100, -100], sources: Self.line, origin: Self.origin,
                                    previous: nil)
        Self.expect(first, [-100, -85, -70, -85, -100], "spread only")
        // The second: spread to [-100, -100, -100, -90, -70], then 0.6 of
        // that and 0.4 of the first.
        let second = smoother.smooth([-100, -100, -100, -100, -60], sources: Self.line, origin: Self.origin,
                                     previous: first[...])
        Self.expect(second, [-100, -94, -88, -88, -82], "spread, then 0.6 new and 0.4 before")
        // The third: a burst at column 1 on this row alone, and the -40
        // and -60 of the rows before, are each one of three: the middle of
        // three is -100 everywhere. Then 0.6 of -100 and 0.4 of the second.
        let third = smoother.smooth([-100, -40, -100, -100, -100], sources: Self.line, origin: Self.origin,
                                    previous: second[...])
        Self.expect(third, [-100, -97.6, -95.2, -95.2, -92.8], "middle of three first")
    }

    @Test func neighboursFromAnotherSourceOrWithNoLevelDoNotSpread() {
        var smoother = StackedRowSmoother()
        let row = smoother.smooth([-100, -40, -80, .nan, -60],
                                  sources: [.line, .line, .wide, .wide, .wide], origin: Self.origin, previous: nil)
        // Column 1's right neighbour is the wide row's: it stands in for it.
        // 0.25 * -100 + 0.5 * -40 + 0.25 * -40 = -55.
        #expect(abs(row[1] + 55) < 1e-4)
        #expect(abs(row[2] + 80) < 1e-4, "-80 with no neighbour of its own source")
        #expect(row[3].isNaN, "no level stays no level")
        #expect(abs(row[4] + 60) < 1e-4)
        #expect(StackedRowSmoother.middle(-50, .nan, -90) == -50, "the newest alone when one has none")
    }

    @Test func anotherViewOrCaptureStartsOver() {
        var smoother = StackedRowSmoother()
        let flat = [Float](repeating: -100, count: 5)
        let first = smoother.smooth(flat, sources: Self.line, origin: Self.origin, previous: nil)
        let moved = StackedRowSmoother.Origin(endpointId: 1, contextGeneration: 1,
            window: BandCoverage(centerHz: 7_210_000, spanHz: 100_000), wide: nil)
        let louder = [Float](repeating: -50, count: 5)
        Self.expect(smoother.smooth(louder, sources: Self.line, origin: moved, previous: first[...]), louder,
                    "a pan takes nothing from the row before")
        let recaptured = StackedRowSmoother.Origin(endpointId: 1, contextGeneration: 2,
            window: moved.window, wide: nil)
        Self.expect(smoother.smooth(flat, sources: Self.line, origin: recaptured, previous: louder[...]), flat,
                    "nor a new capture")
    }

    /// Through the stack, as the band feeds it: a row of -100 then a row
    /// of -50 makes -70 (0.6 * -50 + 0.4 * -100). A key or an unkey, a new
    /// band and a clear each start the smoothing over.
    @Test func theStackSmoothsAndStartsOverAtKeyBandAndClear() throws {
        let pan = BandCoverage(centerHz: 7_200_000, spanHz: 40_000)
        var state = BandState(waterfallLines: 10)
        state.stacksRows = true
        state.note(context: 1, coverage: pan)
        let levels = WaterfallHistory.Levels(lowDbm: -130, highDbm: -60)
        var sequence: UInt32 = 0
        func add(_ dbm: Float) {
            sequence += 1
            state.receive(frame: BandFixtures.frame(trace: [Float](repeating: dbm, count: 40), sequence: sequence),
                          manualLevels: levels)
        }
        func centre() -> Float {
            state.stack.sample(age: 0, frequencyHz: pan.centerHz, view: pan, currentFloor: -200).dbm
        }
        add(-100)
        #expect(abs(centre() + 100) < 1e-4)
        add(-50)
        #expect(abs(centre() + 70) < 1e-4, "0.6 of the new row, 0.4 of the one before")

        state.resetSmoothing()
        add(-50)
        #expect(abs(centre() + 50) < 1e-4, "after a key the row stands on its own")
        add(-100)
        #expect(abs(centre() + 80) < 1e-4, "and the next blends again: 0.6 * -100 + 0.4 * -50")

        let band = BandCoverage(centerHz: 14_200_000, spanHz: 40_000)
        state.note(context: 1, coverage: band)
        sequence += 1
        state.receive(frame: BandFixtures.frame(trace: [Float](repeating: -40, count: 40), sequence: sequence),
                      manualLevels: levels)
        let onBand = state.stack.sample(age: 0, frequencyHz: band.centerHz, view: band, currentFloor: -200).dbm
        #expect(abs(onBand + 40) < 1e-4, "a new band takes nothing from the old")

        var stack = state.stack
        stack.clear()
        stack.append(frame: BandFixtures.frame(trace: [Float](repeating: -60, count: 40), sequence: 99),
                     coverage: band, wide: nil, time: 0)
        let cleared = stack.sample(age: 0, frequencyHz: band.centerHz, view: band, currentFloor: -200).dbm
        #expect(abs(cleared + 60) < 1e-4, "after a clear the row stands on its own")
    }
}
