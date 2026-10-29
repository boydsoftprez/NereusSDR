// NereusSDR for iOS: waterfall time-axis projection and readout tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
import Foundation
import Testing
@testable import NereusBand

@Suite struct WaterfallTimeAxisTests {
    private let levels = WaterfallHistory.Levels(lowDbm: -100, highDbm: 0)

    private func history(_ times: [Double], capacity: Int = 512) -> WaterfallHistory {
        var result = WaterfallHistory(capacity: capacity)
        for (index, time) in times.enumerated() {
            result.append(line: [Float(index % 100) - 100], levels: levels,
                          time: 1_700_000_000 + time, presentationTime: time)
        }
        return result
    }

    @Test(arguments: [10, 30, 60])
    func logicalSpeedDoesNotDependOnInputFpsOrDpr(fps: Int) throws {
        let history = history((0...fps * 4).map { 1000 + Double($0) / Double(fps) })
        let before = WaterfallTimeAxis(topTime: 1003, periodMs: 10)
        let after = WaterfallTimeAxis(topTime: 1004, periodMs: 10)
        let markAge = fps * 2
        #expect(abs(try #require(after.yPoints(age: markAge, history: history))
            - #require(before.yPoints(age: markAge, history: history)) - 100) < 0.0001)
        for scale in [1.0, 2.0, 3.0] {
            var rows: [UInt32] = []
            after.fillAges(history: history, heightPixels: Int(100 * scale), scale: scale, into: &rows)
            let y = Int(50 * scale)
            let sampled = try #require(history.presentationTime(age: Int(rows[y])))
            #expect(abs(sampled - (1004 - (Double(y) + 0.5) / scale * 0.01)) <= 0.5 / Double(fps) + 0.00001)
        }
        #expect(history.count == fps * 4 + 1)
        #expect(history.linesAppended == UInt64(fps * 4 + 1))
    }

    @Test func largeEpochTimesKeepSubPixelPrecision() throws {
        let history = history([1_700_000_000, 1_700_000_000.1])
        let axis = WaterfallTimeAxis(topTime: 1_700_000_000.1, periodMs: 10)
        #expect(abs(try #require(axis.yPoints(age: 1, history: history)) - 10) < 0.0001)
    }

    @Test func aNewFrameRateDoesNotRetimeOldHistory() throws {
        let history = history([1000, 1000.03, 1000.06, 1000.16, 1000.26])
        let axis = WaterfallTimeAxis(topTime: 1000.26, periodMs: 10)
        #expect(abs(try #require(axis.yPoints(age: 2, history: history)) - 20) < 0.00001)
        #expect(abs(try #require(axis.yPoints(age: 3, history: history)) - 23) < 0.00001)
        #expect(abs(try #require(axis.yPoints(age: 4, history: history)) - 26) < 0.00001)
    }

    @Test func lookBackKeepsTheSameObservationWhileNewRowsArrive() throws {
        var history = history([1000, 1000.1, 1000.2], capacity: 8)
        let anchor = try #require(history.presentationTime(age: 1))
        let axis = WaterfallTimeAxis(topTime: anchor, periodMs: 10)
        let wall = history.time(age: 1)
        history.append(line: [-50], levels: levels, time: 1_700_001_000.3, presentationTime: 1000.3)
        #expect(axis.age(at: axis.topTime, history: history) == 2)
        #expect(history.time(age: 2) == wall)
        #expect(axis.yPoints(age: 2, history: history) == 0)
    }

    @Test func timestampsSurviveWrapResizeAndFrequencyRemap() throws {
        var history = history([10, 10.1, 10.2, 10.3], capacity: 3)
        history.resize(capacity: 8)
        history.append(line: [-50, -60], levels: levels, time: 900, presentationTime: 10.4)
        #expect(history.count == 4)
        #expect(history.presentationTime(age: 1) == 10.3)
        #expect(history.time(age: 1) == 1_700_000_010.3)
        #expect(history.time(age: 0) == 900) // A wall-clock correction does not reverse geometry.
        let axis = WaterfallTimeAxis(topTime: 10.4, periodMs: 10)
        #expect(abs(try #require(axis.yPoints(age: 1, history: history)) - 10) < 0.00001)
        history.clear()
        #expect(history.presentationTime(age: 0) == nil)
    }

    @Test func theLiveClockNeverInventsAFutureRow() throws {
        let history = history([1000, 1000.1])
        #expect(WaterfallTimeAxis.liveTop(history: history, progress: 0) == 1000)
        #expect(abs(try #require(WaterfallTimeAxis.liveTop(history: history, progress: 0.5)) - 1000.05) < 0.00001)
        #expect(WaterfallTimeAxis.liveTop(history: history, progress: 2) == 1000.1)
        var rows: [UInt32] = []
        WaterfallTimeAxis(topTime: 1000.1, periodMs: 10).fillAges(history: history, heightPixels: 100, scale: 1, into: &rows)
        #expect(rows[0] == 0 && rows[8] == 1 && rows[99] == WaterfallTimeAxis.blank)
        WaterfallTimeAxis(topTime: 1000, periodMs: 10).fillAges(history: WaterfallHistory(capacity: 2), heightPixels: 100, scale: 1, into: &rows)
        #expect(rows.allSatisfy { $0 == WaterfallTimeAxis.blank })
    }

    @Test func missingObservationsStayBlankAndTwoDGlidesBetweenRealRows() {
        // These are capture-domain coordinates with a real one-second RF
        // observation gap, not the local arrival times of delayed packets.
        var history = WaterfallHistory(capacity: 4)
        history.append(line: [-50], levels: levels, time: 1000, presentationTime: 10, presentationPeriod: 0.1)
        history.append(line: [-50], levels: levels, time: 1001, presentationTime: 11, presentationPeriod: 0.1, presentationBreakBefore: true)
        let axis = WaterfallTimeAxis(topTime: 11, periodMs: 10)
        #expect(axis.age(at: 10.5, history: history) == nil)
        #expect(axis.age(at: 11.5, history: history) == nil)
        var draws = BandLineDraws()
        let flowing = draws.draws(at: 1, frame: 1 / 60, interval: 1 / 30, stacked: false, linesPut: 0, dirty: false, flowing: true)
        let resting = draws.draws(at: 2, frame: 1 / 60, interval: 1 / 30, stacked: false, linesPut: 0, dirty: false, flowing: false)
        #expect(flowing && !resting)
    }

    @Test(arguments: [[0.0, 0], [1000, 1000], [1000, 999], [Double.nan, 1000], [Double.infinity, 1000]])
    func unknownOrUnorderedTimesKeepOriginalRowProjection(times: [Double]) {
        var history = WaterfallHistory(capacity: 8)
        for time in times { history.append(line: [-50], levels: levels, time: time) }
        #expect(!history.hasPresentationTimeline)
        #expect(WaterfallTimeAxis.liveTop(history: history, progress: 0.5) == nil)
        var rows: [UInt32] = []
        WaterfallTimeAxis(topTime: 1000, periodMs: 10).fillAges(history: history, heightPixels: 4, scale: 3, into: &rows)
        #expect(rows == [0, 1, WaterfallTimeAxis.blank, WaterfallTimeAxis.blank])
        #expect(history.count == 2)
    }

    @Test func lookupReusesStorageAndHasBoundedLinearCost() {
        let history = history((0..<8192).map { 1000 + Double($0) / 30 }, capacity: 8192)
        var rows: [UInt32] = []
        let axis = WaterfallTimeAxis(topTime: 1273, periodMs: 10)
        axis.fillAges(history: history, heightPixels: 2400, scale: 3, into: &rows)
        let start = ContinuousClock.now
        for _ in 0..<100 { axis.fillAges(history: history, heightPixels: 2400, scale: 3, into: &rows) }
        let elapsed = start.duration(to: .now)
        print("Waterfall map: 8192 stored rows, 2400 output pixels, 100 fills: \(elapsed)")
        #expect(rows.count == 2400 && rows.last != WaterfallTimeAxis.blank)
    }
}
