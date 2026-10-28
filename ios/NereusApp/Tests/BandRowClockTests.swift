// NereusSDR for iOS: the band's waterfall lines go in on the row clock, driven by the screen, and the 3D glides by it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import CoreGraphics
import Foundation
import NereusBand
import NereusMedia
import QuartzCore
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// The band's lines go in on the row clock's steady grid while a view on
/// screen drives them (JJ's build 7950 recording, 2026-10-25: the waterfall
/// and the 3D moved in 16.7 and 50 ms pairs as each line went in on arrival).
@Suite("Band lines on the row clock", .serialized)
@MainActor
struct BandRowClockTests {
    /// A view that can drive the band's lines, without a screen.
    final class Driver: BandLineDriver {
        var drivesLines = true
    }

    static let frame = 1.0 / 60

    /// A band on the Core's context (30 frames a second, a line a frame),
    /// 3D at four lines a row when `stacked`.
    static func band(stacked: Bool) throws -> BandModel {
        var settings = BandDisplaySettings.desktopDefaults
        settings.spectrumView = stacked ? .stacked : .flat
        settings.threeDSpeed = 4
        let band = BandModel(settings: settings)
        band.stackOffered = true
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        _ = band.prepareToDraw(size: CGSize(width: 400, height: 800), scale: 1)
        return band
    }

    @Test("10 ms moves 100 logical points per second on screen at any received cadence", arguments: [10, 30, 60])
    func visualTimeScaleOnScreen(fps: Int) async throws {
        let band = try Self.band(stacked: false)
        band.settings.waterfallPeriodMs = 10
        band.receive(.context(try #require(BandFlagShotTests.context(fps: fps))))
        let window = try BandFlagShotTests.window(size: CGSize(width: 402, height: 874))
        window.rootViewController = ShotHosting.host(BandView(model: band).ignoresSafeArea())
        window.isHidden = false
        defer { window.isHidden = true; window.rootViewController = nil }
        let ready = try await ShotWait.requireBandLaidOut(band, in: window)
        let origin = CACurrentMediaTime() + 1
        var now = origin
        band.lineClock = { now }
        let traces = BandFlagShotTests.traces(width: 128, lines: fps * 8,
            low: BandFlagShotTests.centre - BandFlagShotTests.span / 2, stations: BandFlagShotTests.stations)
        var next = 1
        var before: Double?
        for tick in 0...480 {
            let displayTime = origin + Double(tick) / 60
            while next <= fps * 8 && origin + Double(next) / Double(fps) <= displayTime + 1e-8 {
                now = origin + Double(next) / Double(fps)
                let line = traces[next - 1]
                band.receive(.displayFrame(DisplayFrame(endpointId: 1, contextGeneration: 1,
                    encoderSequence: UInt32(next), producerTimestamp: UInt64(next) * UInt64(1_000_000_000 / fps),
                    isKeyframe: next == 1, waterfallAdvance: true, minDbm: -160, maxDbm: 0,
                    traceDbm: line, waterfallDbm: line, wideDbm: [])))
                next += 1
            }
            now = displayTime
            band.advanceLines(at: now, frameSeconds: Self.frame)
            if tick == 420 { before = band.waterfallTimeAxis?.topTime }
        }
        #expect(band.pacingLines)
        let after = try #require(band.waterfallTimeAxis)
        #expect(abs(after.topTime - (try #require(before)) - 1) < 0.00001)
        #expect(band.state.history.linesAppended <= UInt64(fps * 8))
        #expect(band.state.history.linesAppended + UInt64(band.state.linesWaiting) == UInt64(fps * 8))
        for scale in [1.0, 3.0] {
            _ = band.prepareToDraw(size: CGSize(width: 402 * scale, height: 874 * scale), scale: scale)
            let axis = try #require(band.overlays(scale: scale).waterfallTimeAxis)
            let old = WaterfallTimeAxis(topTime: axis.topTime - 1, periodMs: 10)
            let age = min(fps, band.state.history.count - 1)
            let distance = try #require(axis.yPoints(age: age, history: band.state.history))
                - #require(old.yPoints(age: age, history: band.state.history))
            #expect(abs(distance - 100) < 0.00001)
        }
        // Holding freezes the visual head even when the display link advances.
        band.paused = true
        let frozen = band.waterfallTimeAxis
        now += 1
        band.advanceLines(at: now, frameSeconds: Self.frame)
        #expect(band.waterfallTimeAxis == frozen)
        band.paused = false
        band.lookBack(lines: 10, visibleLines: 1)
        let anchor = band.waterfallTimeAxis?.topTime
        let rowTime = band.topLineTime
        // A later Core FPS changes only future periods, never this older anchor.
        band.receive(.context(try #require(BandFlagShotTests.context(fps: fps == 10 ? 30 : 10))))
        now += 0.1
        band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(next))))
        band.advanceLines(at: now + 0.2, frameSeconds: Self.frame)
        #expect(band.waterfallTimeAxis?.topTime == anchor)
        #expect(band.topLineTime == rowTime)
        if fps == 30, let directory = ProcessInfo.processInfo.environment["NEREUS_BAND_SHOTS"], !directory.isEmpty {
            // The same hosted fixture supplies a review image, with no product-only screenshot hooks.
            band.goLive()
            band.settings.waterfallStopOnTx = true
            band.keyed = true
            try await ShotWait.requireBandShown(band, in: window, after: ready)
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            let data = try #require(image.pngData())
            try data.write(to: URL(fileURLWithPath: directory).appendingPathComponent("waterfall-10ms-30fps.png"))
        }
    }

    @Test("rewind reaches old low-FPS history through a newer high-FPS viewport")
    func mixedRateRewindReachesOlderHistory() throws {
        let band = try Self.band(stacked: false)
        band.settings.waterfallPeriodMs = 10
        var now = 1000.0
        band.lineClock = { now }
        band.receive(.context(try #require(BandFlagShotTests.context(fps: 10))))
        for sequence in 0..<100 {
            now = 1000 + Double(sequence) / 10
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence + 1))))
        }
        band.receive(.context(try #require(BandFlagShotTests.context(fps: 60))))
        for sequence in 0..<300 {
            now = 1010 + Double(sequence) / 60
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence + 101))))
        }
        _ = band.prepareToDraw(size: CGSize(width: 400, height: 800), scale: 1)
        #expect(band.state.history.count == 400)
        #expect(band.mostLookBackSeconds >= 10)
        band.lookBack(seconds: 10)
        #expect(band.lookBackLines >= 349 && band.lookBackLines <= 351)
        #expect(abs(band.lookBackElapsedSeconds - 10) <= 0.1)
        let anchor = band.waterfallTimeAxis?.topTime
        now += 1.0 / 60
        band.receive(.displayFrame(BandFixturesForApp.frame(sequence: 401)))
        #expect(band.waterfallTimeAxis?.topTime == anchor)
        // A line-based drag must reach the same timed destination, too.
        band.goLive()
        band.lookBack(lines: 351, visibleLines: band.visibleLines)
        #expect(band.lookBackLines == 351)
    }

    @Test("the model's lines go in every second frame, and the 3D glide reads the clock")
    func glideAndFoldReadTheClock() throws {
        let band = try Self.band(stacked: true)
        #expect(band.drawsStack)
        let driver = Driver()
        band.lineDriver = driver
        var now = 500.0
        band.lineClock = { now }
        // Lines every 33.3 ms, up to 8 ms early or late.
        let arrivals = (0 ..< 240).map { k in
            500.2 + Double(k) / 30 + Double(WaterfallRowClockTestsJitter.ns(k)) / 1e9
        }.sorted()
        var next = 0
        var putFrames: [Int] = []
        var positions: [Double] = []
        var number = 0
        while 500 + Double(number) * Self.frame <= arrivals[arrivals.count - 1] + 0.5 {
            defer { number += 1 }
            let frameTime = 500 + Double(number) * Self.frame
            while next < arrivals.count && arrivals[next] <= frameTime {
                now = arrivals[next]
                band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(next + 1))))
                next += 1
            }
            now = frameTime
            let lines = band.state.history.linesAppended
            band.advanceLines(at: frameTime, frameSeconds: Self.frame)
            for _ in 0 ..< Int(band.state.history.linesAppended - lines) {
                putFrames.append(number)
            }
            // The glide the renderer is given follows the clock, not the
            // time it is asked at: a draw a moment later shows the same.
            let asked = Date().timeIntervalSince1970
            let glide = band.stackGlide(at: asked)
            if band.state.stack.count > 0 {
                #expect(glide == band.stackGlide(at: asked + 0.02))
            }
            positions.append(Double(band.state.stack.rowsAppended) + glide)
        }
        #expect(band.pacingLines)
        #expect(putFrames.count == arrivals.count)
        let steady = zip(putFrames, putFrames.dropFirst()).filter { $0.0 >= 120 }.map { $1 - $0 }
        #expect(steady.allSatisfy { $0 == 2 }, "\(steady)")
        // The stack moves on every frame while lines flow, an eighth of a row each.
        let flowing = positions.dropFirst(120).prefix(putFrames.count * 2 - 240)
        let steps = zip(flowing, flowing.dropFirst()).map { $1 - $0 }
        #expect(steps.allSatisfy { abs($0 - 0.125) < 0.04 }, "\(steps)")
    }

    @Test("looking back stays on the same lines as the clock puts lines in; a mark and a hold put the waiting lines in first")
    func consumersKeepWorking() throws {
        let band = try Self.band(stacked: false)
        let driver = Driver()
        band.lineDriver = driver
        var now = 100.0
        band.lineClock = { now }
        for sequence in 1 ... 40 {
            now = 100 + Double(sequence) / 30
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence))))
            band.advanceLines(at: now, frameSeconds: Self.frame)
        }
        // A short viewport leaves older observations available for rewind.
        _ = band.prepareToDraw(size: CGSize(width: 400, height: 40), scale: 1)
        band.lookBack(lines: 10, visibleLines: 5)
        #expect(band.lookBackLines == 10)
        let before = band.state.history.linesAppended
        for sequence in 41 ... 50 {
            now = 100 + Double(sequence) / 30
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence))))
            band.advanceLines(at: now, frameSeconds: Self.frame)
            band.advanceLines(at: now + Self.frame, frameSeconds: Self.frame)
        }
        let put = Int(band.state.history.linesAppended - before)
        #expect(put > 0)
        #expect(band.lookBackLines == 10 + put)
        // A mark goes above every line that came before it.
        band.receive(.displayFrame(BandFixturesForApp.frame(sequence: 51)))
        #expect(band.state.linesWaiting > 0)
        band.markAway("Sound only")
        #expect(band.state.linesWaiting == 0)
        // Stop on TX: the lines from before the key go in, then the waterfall holds.
        band.receive(.displayFrame(BandFixturesForApp.frame(sequence: 52)))
        #expect(band.state.linesWaiting == 1)
        band.settings.waterfallStopOnTx = true
        band.keyed = true
        #expect(band.state.linesWaiting == 0 && band.state.waterfallHeld)
        band.keyed = false
        // The view gone: lines go in as they come.
        driver.drivesLines = false
        band.receive(.displayFrame(BandFixturesForApp.frame(sequence: 53)))
        #expect(!band.pacingLines && band.state.linesWaiting == 0)
    }

    @Test("on screen, the display link puts the lines in and the band draws them; in 3D the stack glides between lines")
    func linesFlowOnScreen() async throws {
        for stacked in [false, true] {
            let band = try Self.band(stacked: stacked)
            let window = try BandFlagShotTests.window(size: CGSize(width: 402, height: 874))
            window.rootViewController = ShotHosting.host(BandView(model: band).ignoresSafeArea())
            window.isHidden = false
            defer {
                window.isHidden = true
                window.rootViewController = nil
            }
            let ready = try await ShotWait.requireBandLaidOut(band, in: window)
            var glides = Set<Double>()
            var waited = false
            // Lines as the Core sends them, one every 33 ms of real time:
            // 60 of them, and in 3D more on a busy simulator until the stack
            // has glided through more than 10 values, for up to 20 s.
            let deadline = Date().addingTimeInterval(20)
            var fed = 0
            while fed < 60 || (stacked && glides.count <= 10 && Date() < deadline) {
                fed += 1
                band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(fed))))
                waited = waited || band.state.linesWaiting > 0
                for _ in 0 ..< 3 {
                    try await Task.sleep(for: .milliseconds(11))
                    glides.insert(band.stackGlide(at: Date().timeIntervalSince1970))
                }
            }
            #expect(band.pacingLines, "3D \(stacked)")
            #expect(waited, "the lines waited for the clock")
            #expect(await ShotWait.until { band.state.linesWaiting == 0 && band.state.history.linesAppended >= fed })
            try await ShotWait.requireBandShown(band, in: window, after: ready)
            if stacked {
                #expect(band.state.stack.rowsAppended >= 14)
                #expect(glides.count > 10, "the stack glided between lines: \(glides.count) values in \(fed) lines")
            }
        }
    }

    @Test("at 3D half rate, a line that comes on a frame not drawn still wakes the view's display link")
    func aSkippedFrameStillWakesTheLink() throws {
        let band = try Self.band(stacked: true)
        var now = 1_000.0
        band.clock = { now }
        // Slow frames: 80 ms each, under 20 a second.
        for _ in 0 ..< 15 {
            now += 0.08
            band.noteStackFrame(drawSeconds: 0.08)
        }
        #expect(band.stackStage == .halfRate && band.drawsStack)
        let driver = Driver()
        band.lineDriver = driver
        var steady = 500.0
        band.lineClock = { steady }
        var draws = 0
        let watch = band.draws.sink { draws += 1 }
        // After a rest the link sleeps: only a draw request wakes it, so each
        // line that waits for the clock asks for one, drawn or not.
        for sequence in 1 ... 4 {
            steady += 1.0 / 30
            let before = draws
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: UInt32(sequence))))
            #expect(band.state.linesWaiting > 0)
            #expect(draws > before, "line \(sequence) waits with nothing to wake the link")
        }
        watch.cancel()
    }

    @Test("a hidden tab's band stops its display link and its lines go in as they come; shown again, they wait for the clock")
    func aHiddenTabStopsTheLink() async throws {
        let band = try Self.band(stacked: false)
        let tab = TabShown()
        let window = try BandFlagShotTests.window(size: CGSize(width: 402, height: 874))
        window.rootViewController = ShotHosting.host(TabbedBand(tab: tab, band: band))
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let ready = try await ShotWait.requireBandLaidOut(band, in: window)
        let linkRuns = {
            (ShotWait.bandView(for: band, in: window)?.delegate as? BandView.Coordinator)?.linkRuns ?? false
        }
        var sequence: UInt32 = 0
        for _ in 0 ..< 10 {
            sequence += 1
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: sequence)))
            try await Task.sleep(for: .milliseconds(33))
        }
        #expect(band.pacingLines)
        // Another tab shows: the band keeps its window, at no opacity.
        tab.shown = false
        #expect(await ShotWait.until(seconds: 10) { !band.pacingLines && !linkRuns() },
                "the hidden band's link still runs")
        let before = band.state.history.linesAppended
        for _ in 0 ..< 10 {
            sequence += 1
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: sequence)))
            #expect(band.state.linesWaiting == 0, "a hidden band's line goes in as it comes")
            try await Task.sleep(for: .milliseconds(33))
        }
        #expect(band.state.history.linesAppended == before + 10)
        #expect(!linkRuns(), "the hidden band's link stays stopped")
        // The tab shows again: the lines wait for the clock, and all go in.
        tab.shown = true
        #expect(await ShotWait.until(seconds: 10) { band.pacingLines })
        var waited = false
        for _ in 0 ..< 10 {
            sequence += 1
            band.receive(.displayFrame(BandFixturesForApp.frame(sequence: sequence)))
            waited = waited || band.state.linesWaiting > 0
            try await Task.sleep(for: .milliseconds(33))
        }
        #expect(waited, "the lines waited for the clock")
        #expect(await ShotWait.until { band.state.linesWaiting == 0 && band.state.history.linesAppended == before + 20 })
        try await ShotWait.requireBandShown(band, in: window, after: ready)
    }

    #if DEBUG
    @Test("the timing trace keeps at most two seconds of lines for a view that does not draw")
    func theTraceKeepsFewLinesForADraw() {
        let trace = WaterfallLineTrace()
        for sequence in 1 ... 1_000 {
            let line = BandState.PutLine(sequence: UInt32(sequence), arrivedAt: Double(sequence),
                                         putAt: Double(sequence))
            trace.note([line], clock: WaterfallRowClock(), lock: WaterfallFrameLock(), waiting: 0)
        }
        let kept = trace.takeLinesForDraw()
        #expect(kept.count == WaterfallLineTrace.mostForDraw)
        #expect(kept.last?.sequence == 1_000)
    }
    #endif
}

/// Whether the tab holding the band shows.
@MainActor
final class TabShown: ObservableObject {
    @Published var shown = true
}

/// The band in a tab, kept alive while hidden as the app's tabs are.
struct TabbedBand: View {
    @ObservedObject var tab: TabShown
    let band: BandModel

    var body: some View {
        BandView(model: band)
            .ignoresSafeArea()
            .tabShown(tab.shown)
    }
}

/// The Kit tests' jitter, ±8 ms, for the app's own runs.
enum WaterfallRowClockTestsJitter {
    static func ns(_ k: Int) -> Int64 {
        let span: Int64 = 8_000_000
        var seed = UInt32(12_345) &+ UInt32(truncatingIfNeeded: k) &* 2_654_435_761
        seed = seed &* 1_664_525 &+ 1_013_904_223
        seed ^= seed >> 13
        return Int64(seed % UInt32(2 * span + 1)) - span
    }
}
