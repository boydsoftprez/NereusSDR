// NereusSDR for iOS: accepted call56 retained real row quiet and actual Metal consumer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-12 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import CoreGraphics
import Foundation
import NereusMedia
import Testing
@testable import NereusBand

@MainActor
@Suite(.serialized) struct DisplayV29QuietOverhangTests {
    @Test func quietUsesRealFoldedStrideEightSamplesAndFiniteThirtyPercentOrderStatistic() {
        // Accepted HTML1610–1613 / native DssQuietOverhang.h: stride8,
        // index floor(finite sampled count*0.3), before resampling/smoothing.
        var folded = [Float](repeating: -20, count: 56) // Unsampled carriers are excluded.
        for (index, value) in [Float(-130), -124, -110, -119, -126, -132, -123].enumerated() {
            folded[index * 8] = value
        }
        #expect(StackedQuietOverhang.quiet(from: folded) == -126)
        folded[0] = .nan
        folded[8] = .infinity
        #expect(StackedQuietOverhang.quiet(from: folded) == -126) // five finite => index1.
        #expect(StackedQuietOverhang.quiet(from: []).isNil)
        #expect(StackedQuietOverhang.quiet(from: [.nan, .infinity]).isNil)
    }

    private func frame(_ row: [Float], sequence: UInt32 = 1, wide: [Float] = []) -> DisplayFrame {
        DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: sequence,
            producerTimestamp: UInt64(sequence), isKeyframe: sequence == 1, waterfallAdvance: true,
            minDbm: -160, maxDbm: 0, traceDbm: row, waterfallDbm: row, wideDbm: wide)
    }

    @Test func quietTravelsWithRetainedAgeAcrossWrapAndClearRatherThanFollowingTheCurrentFloor() {
        var history = StackedTraceHistory()
        let coverage = BandCoverage(centerHz: 7_200_000, spanHz: 40_000)
        var expected: [Float] = []
        for index in 0..<(history.capacity + 9) {
            let value = Float(-140 + index % 19)
            history.append(frame: frame([Float](repeating: value, count: 64), sequence: UInt32(index + 1)),
                           coverage: coverage, wide: nil, time: Double(index))
            expected.append(value)
        }
        #expect(history.count == history.capacity)
        for age in [0, 1, 31, history.capacity - 1] {
            #expect(history.quiet(age: age) == expected[expected.count - 1 - age])
        }
        #expect(history.quiet(age: -1).isNil && history.quiet(age: history.count).isNil)
        history.clear()
        #expect(history.quiet(age: 0).isNil)
        history.append(frame: frame([Float](repeating: .nan, count: 64)), coverage: coverage, wide: nil, time: 1)
        #expect(history.quiet(age: 0).isNil)
    }

    @Test func capturedChannelsWinAndQuietIsOnlyKnownExtendedOverhang() {
        let exact = StackedQuietOverhang.sample(unit: -0.2, exact: -55, wide: -65, quiet: -122, currentFloor: -150)
        #expect(exact == .init(dbm: -55, covered: true))
        let wide = StackedQuietOverhang.sample(unit: 1.2, exact: nil, wide: -65, quiet: -122, currentFloor: -150)
        #expect(wide == .init(dbm: -65, covered: true))
        let quiet = StackedQuietOverhang.sample(unit: 1.2, exact: nil, wide: nil, quiet: -122, currentFloor: -150)
        #expect(quiet == .init(dbm: -122, covered: true))
        for unit in [Float(0.2), 0, 1, .nan] {
            let inside = StackedQuietOverhang.sample(unit: unit, exact: nil, wide: nil, quiet: -122, currentFloor: -150)
            #expect(inside == .init(dbm: -150, covered: false))
        }
        let badMeasured = StackedQuietOverhang.sample(unit: -0.2, exact: .nan, wide: nil, quiet: -122, currentFloor: -150)
        #expect(badMeasured.dbm.isNaN && badMeasured.covered) // Never paint a quiet replacement for a bad measured bin.
        let unknown = StackedQuietOverhang.sample(unit: 1.2, exact: nil, wide: nil, quiet: nil, currentFloor: -150)
        #expect(unknown == .init(dbm: -150, covered: false))
    }

    @Test func retainedPeakIncludesPartiallyOverlappingLastBinWithoutAddingAnAdjacentBin() {
        let coverage = BandCoverage(centerHz: 7_200_000, spanHz: 4_000)
        let samples: [Float] = [-120, -50, -110, -115]
        #expect(StackedTraceHistory.peak(samples, over: coverage,
            from: coverage.lowHz + 500, to: coverage.lowHz + 1_500) == -50)
        // An exclusive endpoint exactly at bin1 does not include bin1.
        #expect(StackedTraceHistory.peak(samples, over: coverage,
            from: coverage.lowHz, to: coverage.lowHz + 1_000) == -120)
        // A fractional edge just beyond bin1 still includes its real carrier.
        #expect(StackedTraceHistory.peak(samples, over: coverage,
            from: coverage.lowHz + 500, to: coverage.lowHz + 1_000.001) == -50)
    }

    @Test func retainedActualSamplesKeepExactWidePrecedenceAndInsideViewHoles() {
        let view = BandCoverage(centerHz: 7_200_000, spanHz: 40_000)
        let wide = BandCoverage(centerHz: 7_200_000, spanHz: 100_000)
        var exact = [Float](repeating: -120, count: 80)
        exact[40] = -50
        // The samples as taken, before the rows are smoothed.
        var history = StackedTraceHistory(smoothsRows: false)
        history.append(frame: frame(exact, wide: [Float](repeating: -70, count: 200)), coverage: view, wide: wide, time: 1)
        #expect(history.sample(age: 0, frequencyHz: view.centerHz, view: view, currentFloor: -150).dbm == -50)
        #expect(history.sample(age: 0, frequencyHz: view.highHz + 10_000, view: view, currentFloor: -150).dbm == -70)
        #expect(history.sample(age: 0, frequencyHz: view.highHz + 70_000, view: view, currentFloor: -150) == .init(dbm: -120, covered: true))
        var holes = StackedTraceHistory()
        for index in 38...42 { exact[index] = .nan }
        holes.append(frame: frame(exact), coverage: view, wide: nil, time: 1)
        #expect(holes.sample(age: 0, frequencyHz: view.centerHz, view: view, currentFloor: -150) == .init(dbm: -150, covered: false))
    }

    @Test func actualMetalPaintsKnownQuietIntoTheExtendedWedgeAndKeepsUnknownRowsEmpty() throws {
        let target = try Offscreen(width: 400, height: 600)
        let renderer = try BandRenderer(device: target.device)
        var settings = BandRendererTests.plain()
        settings.spectrumView = .stacked
        settings.traceFill = false
        // Isolate the surface: the opaque dBm scale would cover x360..<400,
        // including the actual quiet wedge sampled below at x375..<396.
        settings.showDbmScale = false
        let coverage = BandCoverage(centerHz: 7_200_000, spanHz: 40_000)
        var known = StackedTraceHistory()
        var unknown = StackedTraceHistory()
        for index in 0..<80 {
            known.append(frame: frame([Float](repeating: -105, count: 160), sequence: UInt32(index + 1)),
                         coverage: coverage, wide: nil, time: Double(index))
            unknown.append(frame: frame([Float](repeating: .nan, count: 160), sequence: UInt32(index + 1)),
                           coverage: coverage, wide: nil, time: Double(index))
        }
        let palette = try BandFixtures.palette(id: 0, [(0, "#000000"), (1, "#FFFFFF")])
        var overlays = BandOverlays(centerHz: coverage.centerHz, spanHz: coverage.spanHz, scale: 1,
                                    settings: settings, palette: palette)
        overlays.stacked = .init(noiseFloorDbm: -120, glide: 0, availableSpanFactor: 3)
        renderer.draw(frame: nil, history: WaterfallHistory(capacity: 1), stack: known, extras: nil,
                      overlays: overlays, into: target.texture)?.waitUntilCompleted()
        let image = try target.read()
        // Independent control renderer: equal generation/serial counters from
        // two value histories are not a source replacement in one live cache.
        let emptyRenderer = try BandRenderer(device: target.device)
        emptyRenderer.draw(frame: nil, history: WaterfallHistory(capacity: 1), stack: unknown, extras: nil,
                      overlays: overlays, into: target.texture)?.waitUntilCompleted()
        let empty = try target.read()
        let height = Double(BandRendererTests.layout(settings).spectrum.height)
        var added = 0
        for x in 375..<396 {
            for y in Int(height * 0.25)..<Int(height * 0.55) {
                let pixel = image.pixel(x, y)
                let blank = empty.pixel(x, y)
                if Int(pixel.r) > Int(blank.r) + 20 && Int(pixel.g) > Int(blank.g) + 20 && Int(pixel.b) > Int(blank.b) + 20 { added += 1 }
            }
        }
        #expect(added > 40, "Actual Metal must fill a real retained quiet wedge; found \(added) pixels")
    }
}

private extension Optional { var isNil: Bool { if case .none = self { return true }; return false } }
