// NereusSDR for iOS: tests for the 3D view: each setting's effect on the drawn stack, its rows, pacing and data ask
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import Metal
import NereusMedia
import Testing
@testable import NereusBand

/// R-IOS-11, the 3D View board (JJ, 2026-09-29): the five 3D settings each
/// move the stack as the board shows, the rows follow the waterfall, the
/// phone paces the stack and falls back to 2D, and the spectrum beside the
/// pan is asked for only when it is used. Every frame here is synthetic (D4).
@MainActor
@Suite struct StackedTraceTests {
    static let plot = CGSize(width: 400, height: 200)
    static let scale: ClosedRange<Double> = -140 ... -40

    static func geometry(_ change: (inout BandDisplaySettings) -> Void = { _ in },
                         floor: Double = -120) -> StackedTraceGeometry {
        var settings = BandDisplaySettings.desktopDefaults
        change(&settings)
        return StackedTraceGeometry(size: plot, settings: settings, noiseFloorDbm: floor, scale: scale)
    }

    // MARK: Settings and their defaults

    @Test func theDefaultsAreTheCoresRows() {
        let settings = BandDisplaySettings.desktopDefaults
        #expect(settings.spectrumView == .flat)
        #expect(settings.threeDFloorDb == 6)
        #expect(settings.threeDGain == 70)
        #expect(settings.threeDSpan == 100)
        #expect(settings.threeDAngle == 50)
        #expect(!settings.threeDSliceShadow)
        #expect(SpectrumView.flat.rawValue == 0 && SpectrumView.stacked.rawValue == 1)
    }

    @Test func eachBandKeepsItsOwnFloor() {
        var settings = BandDisplaySettings.desktopDefaults
        settings.enterBand("5")
        settings.threeDFloorDb = 12
        settings.enterBand("7")
        #expect(settings.threeDFloorDb == 6, "a band not seen starts at the default")
        settings.threeDFloorDb = 3
        settings.enterBand("5")
        #expect(settings.threeDFloorDb == 12)
        settings.enterBand("7")
        #expect(settings.threeDFloorDb == 3)
        #expect(settings.threeDFloors == ["5": 12, "7": 3])
    }

    @Test func resetPutsTheSixBackWithTheFloorForThisBandOnly() {
        var settings = BandDisplaySettings.desktopDefaults
        settings.enterBand("5")
        settings.threeDFloorDb = 20
        settings.enterBand("7")
        settings.spectrumView = .stacked
        settings.threeDFloorDb = 11
        settings.threeDGain = 85
        settings.threeDSpan = 60
        settings.threeDAngle = 30
        settings.threeDSliceShadow = true
        settings.resetThreeD()
        #expect(settings.spectrumView == .flat)
        #expect(settings.threeDFloorDb == 6 && settings.threeDGain == 70 && settings.threeDSpan == 100)
        #expect(settings.threeDAngle == 50 && !settings.threeDSliceShadow)
        #expect(settings.threeDFloors["5"] == 20, "another band's floor stays")
        #expect(settings.threeDFloors["7"] == 6)
    }

    @Test func threeDAsksForTheNoiseFloorAndKeepsItsSettings() throws {
        var settings = BandDisplaySettings.desktopDefaults
        #expect(!settings.needsNoiseFloor)
        settings.spectrumView = .stacked
        #expect(settings.needsNoiseFloor)
        settings.threeDGain = 91
        let data = try JSONEncoder().encode(settings)
        let back = try JSONDecoder().decode(BandDisplaySettings.self, from: data)
        #expect(back.spectrumView == .stacked && back.threeDGain == 91)
    }

    // MARK: Each setting's effect on the drawn geometry

    @Test func floorMovesWhereTheSurfaceStarts() {
        let shallow = Self.geometry { $0.threeDFloorDb = 0 }
        let deep = Self.geometry { $0.threeDFloorDb = 18 }
        #expect(shallow.floorDbm == -120 && deep.floorDbm == -138)
        // At 0 the noise lies flat; deeper, the noise stands up as texture.
        #expect(shallow.ridgeY(dbm: -120, depth: 0) == shallow.baselineY(depth: 0))
        #expect(deep.ridgeY(dbm: -120, depth: 0) < deep.baselineY(depth: 0) - 10)
        // A signal stands on the floor: the deeper floor lifts it too.
        #expect(deep.ridgeY(dbm: -80, depth: 0) < shallow.ridgeY(dbm: -80, depth: 0))
    }

    @Test func gainChangesColourNotHeight() {
        let low = Self.geometry { $0.threeDGain = 20 }
        let middle = Self.geometry { $0.threeDGain = 50 }
        let high = Self.geometry { $0.threeDGain = 100 }
        #expect(abs(StackedTrace.gamma(gain: 70) - 0.5743) < 0.001)
        #expect(StackedTrace.gamma(gain: 50) == 1 && StackedTrace.gamma(gain: 100) == 0.25)
        #expect(StackedTrace.gamma(gain: 0) == 4)
        let weak = -110.0
        #expect(low.colourPosition(dbm: weak) < middle.colourPosition(dbm: weak))
        #expect(middle.colourPosition(dbm: weak) < high.colourPosition(dbm: weak))
        #expect(low.ridgeY(dbm: weak, depth: 0.2) == high.ridgeY(dbm: weak, depth: 0.2))
        // Colour reads through 45 dB at most, whatever the scale's range.
        #expect(middle.colourRangeDb == 45 && middle.heightRangeDb == 100)
        #expect(abs(middle.colourPosition(dbm: middle.floorDbm + 22.5) - 0.5) < 1e-9)
    }

    @Test func spanWidensTheNearRowsIntoTheSpectrumBesideThePan() {
        let shape = StackedTraceShape.forAngle(50)
        #expect(StackedTrace.rowSpan(spanPercent: 0, shape: shape, availableFactor: 2.5) == 1)
        let full = StackedTrace.rowSpan(spanPercent: 100, shape: shape, availableFactor: 2.5)
        #expect(abs(full - 1 / 0.6) < 1e-9, "never past where the back row reaches both edges")
        let half = StackedTrace.rowSpan(spanPercent: 50, shape: shape, availableFactor: 2.5)
        #expect(abs(half - (1 + 0.5 * (1 / 0.6 - 1))) < 1e-9)
        #expect(StackedTrace.rowSpan(spanPercent: 100, shape: shape, availableFactor: 1.2) == 1.2,
                "no wider than the Core's spectrum reaches")
        #expect(StackedTrace.rowSpan(spanPercent: 100, shape: shape, availableFactor: 1) == 1,
                "nothing beside the pan: the classic trapezoid")
    }

    @Test func angleShapesTheStackAndFiftyIsTheClassic() {
        let classic = StackedTraceShape.forAngle(50)
        #expect(abs(classic.backWidth - 0.60) < 1e-9 && abs(classic.depthSpan - 0.58) < 1e-9)
        #expect(abs(classic.ridge - 0.46) < 1e-9)
        let flat = StackedTraceShape.forAngle(0)
        let down = StackedTraceShape.forAngle(100)
        #expect(flat.backWidth == 0.35 && abs(down.backWidth - 0.85) < 1e-9)
        #expect(flat.depthSpan == 0.36 && abs(down.depthSpan - 0.80) < 1e-9)
        #expect(flat.ridge == StackedTrace.largestRidge)
        // A back row's peak stays inside the plot at every angle.
        for angle in stride(from: 0, through: 100, by: 5) {
            let shape = StackedTraceShape.forAngle(angle)
            #expect((1 - shape.depthSpan) - shape.ridge * shape.backWidth >= -1e-9)
        }
        let g0 = Self.geometry { $0.threeDAngle = 10 }
        let g1 = Self.geometry { $0.threeDAngle = 90 }
        // Higher looks down: the back row sits higher and wider.
        #expect(g1.baselineY(depth: 1) < g0.baselineY(depth: 1))
        #expect(g1.x(unit: 0, depth: 1) < g0.x(unit: 0, depth: 1))
        // The front row runs edge to edge at every angle.
        #expect(g0.x(unit: 0, depth: 0) == 0 && g1.x(unit: 1, depth: 0) == 400)
    }

    @Test func theWiderSpanTheCoreIsAskedForIsItsFactor() {
        #expect(abs(StackedTrace.widestRowSpan - 1 / 0.35) < 1e-12)
        #expect(abs(StackedTrace.widestRowSpan - 1 / StackedTraceShape.forAngle(0).backWidth) < 1e-12)
    }

    @Test func rowsRecedeAndGlide() {
        #expect(StackedTraceGeometry.depth(age: 0, glide: 0) == 0)
        #expect(StackedTraceGeometry.depth(age: 96, glide: 0) == 1)
        #expect(StackedTraceGeometry.depth(age: 4, glide: 0.5) == 4.5 / 96)
        let geometry = Self.geometry()
        #expect(geometry.baselineY(depth: 0) == 200)
        #expect(geometry.baselineY(depth: 1) < geometry.baselineY(depth: 0.5))
    }

    @Test func theScaleInThreeDReadsFromTheFloorUpTheFrontRow() {
        let geometry = Self.geometry()
        let ticks = geometry.scaleTicks(stepDb: 10)
        #expect(ticks.first?.dbm == -120, "the first 10 dB step at or over the floor, -126")
        #expect(ticks.last?.dbm == -30)
        #expect(ticks.map(\.y) == ticks.map { geometry.ridgeY(dbm: $0.dbm, depth: 0) })
        #expect(zip(ticks, ticks.dropFirst()).allSatisfy { $0.y > $1.y })
    }

    // MARK: The spectrum beside the pan (recommendation 5)

    @Test func theWideSpectrumIsAskedForOnlyWhenTheStackUsesIt() {
        var settings = BandDisplaySettings.desktopDefaults
        #expect(settings.wideSpanFactor(drawsStack: false, savesData: false) == 0)
        #expect(settings.wideSpanFactor(drawsStack: true, savesData: false) == StackedTrace.widestRowSpan)
        #expect(settings.wideSpanFactor(drawsStack: true, savesData: true) == 0, "Saver or cellular: Span as 0")
        settings.threeDSpan = 0
        #expect(settings.wideSpanFactor(drawsStack: true, savesData: false) == 0)
    }

    // MARK: The rows

    @Test func eachWaterfallLineBecomesARowWithTheSpectrumBesideIt() {
        var stack = StackedTraceHistory()
        let pan = BandCoverage(centerHz: 7_200_000, spanHz: 40_000)
        let wide = BandCoverage(centerHz: 7_200_000, spanHz: 100_000)
        var line = [Float](repeating: -130, count: 100)
        line[25] = -70
        var wideLine = [Float](repeating: -125, count: 200)
        wideLine[10] = -60 // 7_155_000 to 7_155_500, well left of the pan
        let frame = DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: 1, producerTimestamp: 1,
                                 isKeyframe: true, waterfallAdvance: true, minDbm: -160, maxDbm: 0,
                                 traceDbm: line, waterfallDbm: line, wideDbm: wideLine)
        let added = stack.append(frame: frame, coverage: pan, wide: wide, time: 10)
        #expect(added)
        #expect(stack.count == 1 && stack.columns == 286)
        let row = Array(stack.row(age: 0)!)
        let window = stack.window(age: 0)!
        #expect(abs(window.spanHz - 40_000 / 0.35) < 1e-6 && window.centerHz == 7_200_000)
        func value(at hz: Double) -> Float {
            row[Int((hz - window.lowHz) / window.spanHz * Double(row.count))]
        }
        #expect(value(at: 7_190_250) == -70, "inside the pan, the line's own level")
        #expect(value(at: 7_155_250) == -60, "beside it, the Core's wide row")
        #expect(value(at: 7_145_000).isNaN, "past the wide row, nothing")
        #expect(stack.newestTime == 10)
    }

    @Test func rowsFollowTheWaterfallsAdvanceAndStopWithIt() {
        var state = BandState(waterfallLines: 10)
        state.stacksRows = true
        state.note(context: 1, coverage: BandCoverage(centerHz: 7_200_000, spanHz: 40_000))
        let levels = WaterfallHistory.Levels(lowDbm: -130, highDbm: -60)
        state.receive(frame: BandFixtures.frame(trace: [Float](repeating: -120, count: 50)), manualLevels: levels)
        state.receive(frame: BandFixtures.frame(trace: [Float](repeating: -120, count: 50), sequence: 2,
                                                advance: false), manualLevels: levels)
        #expect(state.stack.count == 1, "only a waterfall advance adds a row")
        state.waterfallHeld = true
        state.receive(frame: BandFixtures.frame(trace: [Float](repeating: -120, count: 50), sequence: 3),
                      manualLevels: levels)
        #expect(state.stack.count == 1 && state.history.count == 1, "Stop on TX holds both")
        state.waterfallHeld = false
        state.receive(frame: BandFixtures.frame(trace: [Float](repeating: -120, count: 50), sequence: 4),
                      manualLevels: levels)
        #expect(state.stack.count == 2 && state.history.count == 2)
        state.stacksRows = false
        #expect(state.stack.count == 0, "2D keeps no rows")
    }

    /// Match divides the waterfall's height in points by 96, as the
    /// desktop divides its logical pixels: an iPhone 18 Pro Max's waterfall
    /// of about 960 pixel rows at 3x makes a row of three lines, not ten.
    @Test func theGlideRunsOnTheCoresRowPeriodLikeTheDesktop() {
        // The Core sends 30 frames a second, a line a frame: a line every 33.3 ms, not the 30 ms setting.
        let period = StackedTrace.rowPeriodSeconds(fps: 30, framesPerLine: 1, fallbackMs: 30)
        #expect(abs(period - 1.0 / 30) < 1e-12)
        #expect(StackedTrace.rowPeriodSeconds(fps: 30, framesPerLine: 2, fallbackMs: 30) == 2.0 / 30)
        // 90 ms into a row of three lines, the stack is 9/10 of the way, still moving.
        let glide = StackedTrace.glide(since: 0.09, rowPeriod: period, divider: 3)
        #expect(abs(glide - 0.9) < 1e-9, "on the 30 ms setting it reached 1 at 90 ms and stood still until the next row")
        #expect(StackedTrace.glide(since: 0.2, rowPeriod: period, divider: 3) == 1)
        #expect(StackedTrace.glide(since: -0.01, rowPeriod: period, divider: 3) == 0)
        // Before the Core says, the Update period setting stands.
        #expect(StackedTrace.rowPeriodSeconds(fps: 0, framesPerLine: 0, fallbackMs: 30) == 0.03)
        #expect(StackedTrace.rowPeriodSeconds(fps: 0, framesPerLine: 0, fallbackMs: 0) == 0.001)
    }

    @Test func matchCountsTheWaterfallInPointsLikeTheDesktop() {
        // 1014 pixels less the 54-pixel frequency scale: a waterfall of 960 pixel rows, 320 points.
        let layout = BandLayout(size: CGSize(width: 900, height: 1014), scale: 3, showsStrip: false, spectrumShare: 0)
        #expect(layout.waterfall.height == 960)
        #expect(layout.waterfallLines == 960, "the waterfall keeps a line a pixel")
        var state = BandState(waterfallLines: 10)
        state.noteVisibleWaterfall(layout)
        #expect(state.effectiveStackRowDivider == 3, "320 points over 96 rows, not 960 pixels")
        state.stackRowDivider = 7
        #expect(state.effectiveStackRowDivider == 7, "a chosen 3D speed stands")
    }

    /// JJ, 2026-10-25, on build 7777: upright and sideways "feel like
    /// different speeds". An iPhone 18 Pro Max's band, upright and turned,
    /// at the desktop's default settings and a Core at 30 fps, a line a frame.
    @Test func turningThePhoneKeepsMatchsRowsASecond() throws {
        let settings = BandDisplaySettings.desktopDefaults
        let upright = BandLayout(size: CGSize(width: 1320, height: 2319), scale: 3, settings: settings)
        let sideways = BandLayout(size: CGSize(width: 2868, height: 990), scale: 3, settings: settings)
        let wasSideways = min(max(Int((Double(sideways.waterfallPoints) / 96).rounded()), 1), 64)
        let period = StackedTrace.rowPeriodSeconds(fps: 30, framesPerLine: 1, fallbackMs: 30)
        func rowsASecond(_ state: BandState) -> Double { 1 / (period * Double(state.effectiveStackRowDivider)) }

        var state = BandState(waterfallLines: 10)
        state.noteVisibleWaterfall(upright)
        let uprightDivider = state.effectiveStackRowDivider
        let uprightRows = rowsASecond(state)
        let uprightGlide = StackedTrace.glide(since: 0.05, rowPeriod: period, divider: uprightDivider)
        #expect(uprightDivider > wasSideways, "the sideways waterfall alone gave Match \(wasSideways)")
        for layout in [sideways, upright, sideways] {
            state.noteVisibleWaterfall(layout)
            #expect(state.effectiveStackRowDivider == uprightDivider)
            #expect(rowsASecond(state) == uprightRows)
            #expect(StackedTrace.glide(since: 0.05, rowPeriod: period,
                                       divider: state.effectiveStackRowDivider) == uprightGlide)
        }

        // Started sideways: the band's width stands in for its upright height.
        var fresh = BandState(waterfallLines: 10)
        fresh.noteVisibleWaterfall(sideways)
        #expect(fresh.stackUprightWaterfallPoints == nil)
        // Sideways at 3x: the 54-pixel scale leaves 936 rows, the spectrum takes
        // 40% (374), so the waterfall is 990 - 428 = 562 of the 990. That share
        // of the 956-point width is 542.7 points, and 543 / 96 rounds to 6.
        #expect(fresh.stackVisibleWaterfallRows == 543)
        #expect(fresh.effectiveStackRowDivider == 6, "the sideways waterfall alone gave Match \(wasSideways)")
        fresh.noteVisibleWaterfall(upright)
        #expect(fresh.effectiveStackRowDivider == uprightDivider)
    }

    @Test func theStackKeepsNinetySevenRows() {
        var stack = StackedTraceHistory()
        let pan = BandCoverage(centerHz: 7_200_000, spanHz: 40_000)
        for sequence in 1...120 {
            stack.append(frame: BandFixtures.frame(trace: [Float](repeating: Float(-sequence), count: 20),
                                                   sequence: UInt32(sequence)),
                         coverage: pan, wide: nil, time: Double(sequence))
        }
        #expect(stack.count == 97 && stack.rowsAppended == 120)
        #expect(stack.row(age: 0)!.contains(-120) && stack.row(age: 96)!.contains(-24))
        #expect(stack.row(age: 97) == nil)
    }

    // MARK: Pacing and the fallback (recommendations 3 and 4)

    @Test func aPhoneThatCannotKeepUpHalvesThenShowsTwoD() {
        var pacer = StackedTracePacer()
        #expect(StackedTracePacer.smoothFloorFps == 20 && StackedTracePacer.sustainedSeconds == 5)
        // Quick frames: stays full.
        var now = 0.0
        for _ in 0..<60 {
            now += 1.0 / 30
            pacer.noteFrame(drawSeconds: 0.01, at: now)
        }
        #expect(pacer.stage == .full)
        // Each frame takes 80 ms: under 20 a second, so half rate after a window.
        for _ in 0..<15 {
            now += 0.08
            pacer.noteFrame(drawSeconds: 0.08, at: now)
        }
        #expect(pacer.stage == .halfRate)
        #expect(pacer.draws(frame: 2) && !pacer.draws(frame: 3))
        // Still slow at half rate, but not yet for 5 seconds.
        let halved = now
        while now - halved < 4 {
            now += 0.16
            pacer.noteFrame(drawSeconds: 0.07, at: now)
        }
        #expect(pacer.stage == .halfRate)
        while now - halved < 6.2 {
            now += 0.16
            pacer.noteFrame(drawSeconds: 0.07, at: now)
        }
        #expect(pacer.stage == .fellBack)
        #expect(!pacer.draws(frame: 2))
        // Nothing more is measured until Try 3D again.
        let changed = pacer.noteFrame(drawSeconds: 0.001, at: now + 1)
        #expect(!changed)
        pacer.tryAgain()
        #expect(pacer.stage == .full)
    }

    @Test func aLetUpAtHalfRateStartsTheFiveSecondsAgain() {
        var pacer = StackedTracePacer()
        var now = 0.0
        func run(seconds: Double, draw: Double) {
            let until = now + seconds
            while now < until {
                now += 0.1
                pacer.noteFrame(drawSeconds: draw, at: now)
            }
        }
        run(seconds: 1.2, draw: 0.08)
        #expect(pacer.stage == .halfRate)
        run(seconds: 4, draw: 0.08)
        run(seconds: 1.5, draw: 0.01)
        run(seconds: 4, draw: 0.08)
        #expect(pacer.stage == .halfRate, "the let-up restarted the count")
        run(seconds: 2, draw: 0.08)
        #expect(pacer.stage == .fellBack)
    }

    @Test func aClockThatGoesBackStartsANewWindow() {
        var pacer = StackedTracePacer()
        pacer.noteFrame(drawSeconds: 0.08, at: 1_000_000)
        var now = 10.0
        for _ in 0..<15 {
            now += 0.08
            pacer.noteFrame(drawSeconds: 0.08, at: now)
        }
        #expect(pacer.stage == .halfRate, "measured from the new window, not stuck behind the old one")
    }

    @Test func neverFasterThanTheFramesAskedNorSixty() {
        var pacer = StackedTracePacer()
        #expect(pacer.leastInterval(askedFps: 30) == 1.0 / 30)
        #expect(pacer.leastInterval(askedFps: 120) == 1.0 / 60)
        #expect(pacer.leastInterval(askedFps: 0) == 1.0 / 60)
        var now = 0.0
        for _ in 0..<15 {
            now += 0.08
            pacer.noteFrame(drawSeconds: 0.08, at: now)
        }
        #expect(pacer.leastInterval(askedFps: 30) == 2.0 / 30)
    }

    /// Runs `frames` (arrival seconds) through a throttle as the band view
    /// does: a wait's timer fires `timerLate` after it was due, and the view
    /// draws on the 60 Hz refresh after it is asked. Returns the draw times.
    private func stackDraws(frames: [Double], interval: Double, timerLate: Double = 0.002) -> [Double] {
        func refresh(after time: Double) -> Double {
            ((time * 60).rounded(.down) + 1) / 60
        }
        var throttle = StackedDrawThrottle()
        var draws: [Double] = []
        var timer: Double?
        for frame in frames {
            if let fires = timer, fires <= frame {
                throttle.waited()
                draws.append(refresh(after: fires))
                timer = nil
            }
            switch throttle.request(at: frame, interval: interval) {
            case .now:
                draws.append(refresh(after: frame))
            case .after(let wait):
                timer = frame + wait + timerLate
            case .waiting:
                break
            }
        }
        return draws
    }

    @Test func threeDDrawsEveryFrameTheCoreSends() {
        // 30 frames a second, each a few ms early or late, as JJ's phone got
        // them: timing from the draw drew 16.6 a second of these.
        let jitter = [0.004, -0.003, 0.002, -0.004, 0.001]
        let frames = (0..<300).map { Double($0) / 30 + jitter[$0 % jitter.count] }
        let draws = stackDraws(frames: frames, interval: 1.0 / 30)
        #expect(draws.count >= 297, "one draw a frame, not one every 50 ms")
        let gaps = zip(draws, draws.dropFirst()).map { (($1 - $0) * 60).rounded() }
        #expect(gaps.filter { $0 >= 3 }.count <= 3, "no refresh lost at each wait")
    }

    @Test func threeDNeverDrawsFasterThanAsked() {
        let frames = (0..<600).map { Double($0) / 60 }
        #expect(stackDraws(frames: frames, interval: 1.0 / 30).count <= 301)
        #expect(stackDraws(frames: frames, interval: 2.0 / 30).count <= 151, "half rate")
        var throttle = StackedDrawThrottle()
        #expect(throttle.request(at: 10, interval: 0.1) == .now)
        guard case .after(let wait) = throttle.request(at: 10.04, interval: 0.1) else {
            Issue.record("a change sooner than the interval waits")
            return
        }
        #expect(abs(wait - 0.06) < 1e-9, "until the interval from when the last draw was due")
        #expect(throttle.request(at: 10.05, interval: 0.1) == .waiting, "the waiting draw takes this change too")
        throttle.waited()
        #expect(throttle.request(at: 10.3, interval: 0.1) == .now)
    }

    @Test func whyAPanShowsTwoD() {
        typealias Hold = StackedTraceHold
        #expect(Hold.reason(chosen: false, offered: false, lowPower: true, hot: true, stage: .fellBack) == nil)
        #expect(Hold.reason(chosen: true, offered: true, lowPower: false, hot: false, stage: .full) == nil)
        #expect(Hold.reason(chosen: true, offered: true, lowPower: false, hot: false, stage: .halfRate) == nil)
        #expect(Hold.reason(chosen: true, offered: false, lowPower: true, hot: false, stage: .full) == .coreOlder)
        #expect(Hold.reason(chosen: true, offered: true, lowPower: true, hot: true, stage: .full) == .lowPower)
        #expect(Hold.reason(chosen: true, offered: true, lowPower: false, hot: true, stage: .full) == .hot)
        #expect(Hold.reason(chosen: true, offered: true, lowPower: false, hot: false, stage: .fellBack)
                == .cannotKeepUp)
        #expect(Hold.cannotKeepUp.notice(pan: 1)
                == "It could not keep the 3D view moving smoothly. 3D stays your choice for Pan 1.")
        for hold in [Hold.coreOlder, .cannotKeepUp, .lowPower, .hot] {
            let words = hold.notice(pan: 1)
            #expect(!words.contains("\u{2014}") && !words.contains(" yet"))
        }
    }

    // MARK: Drawn

    @Test func theStackIsDrawnInPlaceOfTheFlatTrace() throws {
        let target = try Offscreen(width: BandRendererTests.width, height: BandRendererTests.height)
        let renderer = try BandRenderer(device: target.device)
        var settings = BandRendererTests.plain()
        settings.spectrumView = .stacked
        var state = BandState(waterfallLines: 10)
        state.stacksRows = true
        let coverage = BandCoverage(centerHz: BandRendererTests.centre, spanHz: BandRendererTests.span)
        state.note(context: 1, coverage: coverage)
        var line = [Float](repeating: -125, count: 200)
        for index in 95..<105 {
            line[index] = -50
        }
        for sequence in 1...40 {
            state.receive(frame: BandFixtures.frame(trace: line, sequence: UInt32(sequence)),
                          manualLevels: .init(lowDbm: -130, highDbm: -60))
        }
        var overlays = BandRendererTests.overlays(settings)
        overlays.frameCoverage = coverage
        overlays.stacked = StackedTraceOverlay(noiseFloorDbm: -125, glide: 0, availableSpanFactor: 1)
        renderer.draw(frame: state.frame, history: state.history, stack: state.stack, extras: nil,
                      overlays: overlays, into: target.texture)?.waitUntilCompleted()
        let image = try target.read()
        let plot = BandRendererTests.geometry(settings)
        let geometry = StackedTraceGeometry(size: plot.size, settings: settings, noiseFloorDbm: -125,
                                            scale: settings.scaleRange)
        // The signal's ridge on the front row stands where the geometry says, lit.
        let top = geometry.ridgeY(dbm: -50, depth: 0)
        let middle = BandRendererTests.width / 2
        #expect(image.grey(middle, Int(top) + 4) > 40, "the front ridge is lit")
        let backmost = geometry.ridgeY(dbm: -50, depth: StackedTraceGeometry.depth(age: 39, glide: 0))
        #expect(image.grey(middle, max(0, Int(backmost) - 12)) < 30, "and nothing stands over the oldest row")
        // No flat trace is drawn in 3D.
        let flatY = Int(plot.y(forDbm: -125))
        #expect(!(flatY - 1...flatY + 1).contains { image.isTrace(20, $0) })
    }
}
