// NereusSDR for iOS: actual approved Phone picture and rendering regressions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-12: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import CoreGraphics
import Foundation
import NereusMedia
import Testing
@testable import NereusBand

@MainActor
@Suite(.serialized) struct DisplayV29RendererTests {
    @Test func pixelWidthReachesTheActualTraceVerticesAtBothApprovedEnds() {
        for (wanted, scale) in [(0.1, 3.0), (5.0, 3.0), (0.1, 1.0), (5.0, 1.0)] {
            var settings = BandDisplaySettings.desktopDefaults
            settings.traceWidthPoints = wanted / scale
            let pixels = settings.traceWidthPixels(scale: CGFloat(scale))
            #expect(abs(Double(pixels) - wanted) < 0.0001)
            var vertices: [BandRenderer.ColourVertex] = []
            BandRenderer.addPolyline([SIMD2(0, 50), SIMD2(100, 50)], width: Float(pixels),
                                     colour: SIMD4(1, 1, 1, 1), to: &vertices)
            let heights = vertices.map(\.position.y)
            let paintedWidth = Double((heights.max() ?? 0) - (heights.min() ?? 0))
            #expect(abs(paintedWidth - wanted) < 0.0001)
        }
        var oversized = BandDisplaySettings.desktopDefaults
        oversized.traceWidthPoints = 20 / 3
        #expect(oversized.traceWidthPixels(scale: 3) == 5)
        #expect(BandDisplaySettings.desktopDefaults.traceWidthPoints == 0.5) // Existing client initial default.
    }

    @Test func freshReceiveCustomStartsFromTheAcceptedPhoneDefaultWhileTransmitKeepsItsInitialDefinition() {
        let expected = [PaletteStop(at: 0, colour: "#000000"),
            PaletteStop(at: Double(Float(0.15)), colour: "#000080"),
            PaletteStop(at: Double(Float(0.30)), colour: "#0040FF"),
            PaletteStop(at: Double(Float(0.45)), colour: "#00C8FF"),
            PaletteStop(at: Double(Float(0.60)), colour: "#00DC00"),
            PaletteStop(at: Double(Float(0.80)), colour: "#FFFF00"), PaletteStop(at: 1, colour: "#FF0000")]
        let settings = BandDisplaySettings.desktopDefaults
        #expect(settings.customPalette == expected)
        #expect(settings.txCustomPalette == PaletteStop.customDefault)
        #expect(settings.waterfallPaletteId == 0) // ef74a7707 BandDisplaySettings.swift:131 existing Phone initial choice.
    }

    @Test func offlineAmateurBoundariesDriveLookupAndActualSharedColourPixels() throws {
        #expect(BandRenderer.bandEdgeHz(in: BandCoverage(centerHz: 7_150_000, spanHz: 400_000))
            == [7_000_000, 7_300_000])
        #expect(BandRenderer.bandEdgeHz(in: BandCoverage(centerHz: 146_000_000, spanHz: 5_000_000))
            == [144_000_000, 148_000_000])
        #expect(BandRenderer.bandEdgeHz(in: BandCoverage(centerHz: 10_000_000, spanHz: 100_000)).isEmpty)
        let target = try Offscreen(width: 400, height: 600)
        let renderer = try BandRenderer(device: target.device)
        var settings = BandRendererTests.plain()
        settings.bandEdgeColour = "#00FF00FF"
        let overlays = BandOverlays(centerHz: 7_150_000, spanHz: 400_000, scale: 1, settings: settings)
        let image = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1),
                                      extras: nil, overlays: overlays)
        let geometry = BandRendererTests.layout(settings).spectrumGeometry(centerHz: 7_150_000,
                                                                          spanHz: 400_000, dbmRange: settings.scaleRange)
        for frequency in [7_000_000.0, 7_300_000.0] {
            let x = Int(geometry.x(forHz: frequency).rounded())
            let green = (x - 2...x + 2).contains { column in
                let pixel = image.pixel(column, 80)
                return pixel.g > 180 && pixel.r < 60 && pixel.b < 60
            }
            #expect(green, "Real edge at \(frequency)Hz must use the shared Appearance color")
        }
    }

    private func frame(_ sequence: UInt32, generation: UInt32 = 1, spike: Bool = false) -> DisplayFrame {
        var exact = [Float](repeating: -125, count: 100)
        var wide = [Float](repeating: -130, count: 200)
        if spike { exact[25] = -40; wide[10] = -35 }
        return DisplayFrame(endpointId: 1, contextGeneration: generation, encoderSequence: sequence,
                            producerTimestamp: UInt64(sequence), isKeyframe: sequence == 1,
                            waterfallAdvance: true, minDbm: -160, maxDbm: 0,
                            traceDbm: exact, waterfallDbm: exact, wideDbm: wide)
    }

    @Test func threeDSpeedPeakFoldsExactAndWideTicksInsteadOfDroppingShortSignals() throws {
        var state = BandState(waterfallLines: 12)
        state.stacksRows = true
        state.stackRowDivider = 3
        state.note(context: 1, coverage: BandCoverage(centerHz: 7_200_000, spanHz: 40_000),
                   wide: BandCoverage(centerHz: 7_200_000, spanHz: 100_000))
        let levels = WaterfallHistory.Levels(lowDbm: -130, highDbm: -60)
        state.receive(frame: frame(1, spike: true), manualLevels: levels)
        state.receive(frame: frame(2), manualLevels: levels)
        #expect(state.stack.count == 0)
        state.receive(frame: frame(3), manualLevels: levels)
        #expect(state.stack.count == 1 && state.history.count == 3)
        let row = try #require(state.stack.row(age: 0))
        #expect(row.contains(-40) && row.contains(-35))
        state.waterfallHeld = true
        state.receive(frame: frame(4, spike: true), manualLevels: levels)
        state.waterfallHeld = false
        state.receive(frame: frame(5), manualLevels: levels)
        state.receive(frame: frame(6), manualLevels: levels)
        #expect(state.stack.count == 1) // Held ticks do not enter a fold.
        state.receive(frame: frame(7), manualLevels: levels)
        let next = try #require(state.stack.row(age: 0))
        #expect(!next.contains(-40) && !next.contains(-35))
    }

    @Test func matchUsesVisibleRowsAndSourceReplacementDropsOnlyAnIncompleteFold() throws {
        var state = BandState(waterfallLines: 4000) // Retained rewind depth is not visible height.
        state.stacksRows = true
        state.stackRowDivider = 0
        state.stackVisibleWaterfallRows = 288 // Native Match=round(visible/96), clamped1..64.
        let pan = BandCoverage(centerHz: 7_200_000, spanHz: 40_000)
        state.note(context: 1, coverage: pan)
        let levels = WaterfallHistory.Levels(lowDbm: -130, highDbm: -60)
        state.receive(frame: frame(1), manualLevels: levels)
        #expect(state.stack.count == 0)
        state.receive(frame: frame(2), manualLevels: levels)
        #expect(state.stack.count == 0)
        state.receive(frame: frame(3), manualLevels: levels)
        #expect(state.stack.count == 1)
        state.receive(frame: frame(4, spike: true), manualLevels: levels)
        state.receive(frame: frame(5), manualLevels: levels)
        state.note(context: 2, coverage: BandCoverage(centerHz: pan.centerHz + 20_000, spanHz: pan.spanHz))
        state.receive(frame: frame(6, generation: 2), manualLevels: levels)
        #expect(state.stack.count == 1) // Fresh capture starts its own 3-tick fold.
        state.receive(frame: frame(7, generation: 2), manualLevels: levels)
        state.receive(frame: frame(8, generation: 2), manualLevels: levels)
        #expect(state.stack.count == 2)
        let current = try #require(state.stack.row(age: 0))
        #expect(!current.contains(-40))
    }

    // 2026-10-23: transmit's end now resets the blanker, as the desktop's
    // does (PR 625 parity), so the impulse is checked before keying.
    @Test func waterfallBlankerSuppressesAnImpulseAndStartsOverWhenTransmitEnds() {
        var blanker = WaterfallBlanker()
        let quiet = [Float](repeating: -100, count: 16)
        let impulse = [Float](repeating: -80, count: 16)
        for _ in 0..<8 {
            #expect(blanker.filtered(quiet, enabled: true, transmitting: false, thresholdDb: 6) == quiet)
        }
        #expect(blanker.filtered(impulse, enabled: true, transmitting: false, thresholdDb: 6) == quiet)
        #expect(blanker.filtered(impulse, enabled: true, transmitting: true, thresholdDb: 6) == impulse)
        #expect(blanker.filtered(impulse, enabled: true, transmitting: false, thresholdDb: 6) == impulse)
        #expect(blanker.historyRows == 1)
        #expect(blanker.filtered(impulse, enabled: false, transmitting: false, thresholdDb: 6) == impulse)
    }

    @Test func trackFloorUsesTheStoredDbStepForItsFinalPairGapWithoutChangingKeptEndpoints() {
        var settings = BandDisplaySettings.desktopDefaults
        settings.waterfallLevelMode = .manual
        settings.gridFollowsNoiseFloor = true
        settings.gridKeepsRange = false
        settings.gridStepDb = 7
        let kept = settings.scaleRange
        var tracker = GridFloorTracker()
        tracker.noteDisplayFloor(-44, fastAttack: false)
        let changed = tracker.run(settings, transmitting: false, at: 10)
        #expect(changed)
        #expect(tracker.range == -47 ... -40)
        #expect(settings.scaleRange == kept)
    }
}
