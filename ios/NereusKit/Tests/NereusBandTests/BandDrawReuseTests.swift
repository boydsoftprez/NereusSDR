// NereusSDR for iOS: what the band keeps between draws never changes what it draws
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-11-15: J.J. Boyd (KG4VCF), AI-assisted via Claude Code (Opus 5.5):
//   each draw through a kept renderer must match a brand-new renderer's draw byte for byte.
import CoreGraphics
import Foundation
import NereusMedia
import NereusModels
import Testing
@testable import NereusBand

/// The renderer keeps work between draws: the grid, the trace and its fill
/// (rebuilt only for a new frame, view or look), and each scale and strip
/// label's own small picture (made once for its text and moved as the view
/// moves). A run of draws through one renderer, with new frames, pans by
/// part of a pixel and more, zooms, settings changes, the extras, markers
/// and pause, must draw every picture byte for byte as a renderer drawing
/// it for the first time does. With `NEREUS_BAND_DRAW_SHOTS` set to a
/// directory, each picture's bytes (the kept renderer's and the fresh one's)
/// are written there, to compare one build against another.
@MainActor
@Suite(.serialized) struct BandDrawReuseTests {
    static let width = 780
    static let height = 1300
    static let scale: CGFloat = 3
    static let centre = 7_224_000.0
    static let span = 48_000.0
    static let samples = 780

    /// One step of the run: what changed, and the frame drawn.
    struct Step {
        var name: String
        var change: (inout BandOverlays) -> Void = { _ in }
        /// A new frame from the Core (its index into the made-up scene).
        var frame: Int?
        var extras = false
    }

    static let steps: [Step] = [
        Step(name: "first", frame: 1),
        Step(name: "unchanged"),
        Step(name: "new-frame", frame: 2),
        Step(name: "pan-part-of-a-pixel", change: { $0.centerHz += 7.3 }),
        Step(name: "pan-again", change: { $0.centerHz += 611.9 }),
        Step(name: "pan-far", change: { $0.centerHz += 9_250 }),
        Step(name: "frame-at-new-view", change: { $0.frameCoverage = nil }, frame: 3),
        Step(name: "zoom-in", change: { $0.spanHz *= 0.73 }),
        Step(name: "zoom-out", change: { $0.spanHz *= 1.9 }, frame: 4),
        Step(name: "fill-off", change: { $0.settings.traceFill = false }),
        Step(name: "fill-gradient", change: { $0.settings.traceFill = true; $0.settings.traceGradient = true }),
        Step(name: "trace-colours", change: { $0.settings.traceColour = "#FF8000"; $0.settings.traceLineColour = "#00FF00" }),
        Step(name: "trace-width", change: { $0.settings.traceWidthPoints = 1.5 }, frame: 5),
        Step(name: "scale-range", change: { $0.settings.scaleTopDbm = -20; $0.settings.scaleBottomDbm = -150 }),
        Step(name: "grid-colours", change: { $0.settings.gridFineColour = "#FF000040"; $0.settings.hGridColour = "#00FF0060" }),
        Step(name: "grid-spacing", change: { $0.settings.frequencyGridSpacingHz = 2_500 }),
        Step(name: "labels-left", change: { $0.settings.frequencyLabelAlignment = .left }),
        Step(name: "labels-right-bigger", change: {
            $0.settings.frequencyLabelAlignment = .right
            $0.settings.frequencyScaleFontPoints = 13
        }),
        Step(name: "strip-large", change: { $0.settings.bandPlanSize = .large }),
        Step(name: "extras", change: {
            $0.settings.noiseFloorLine = true
            $0.settings.peakBlobs = true
            $0.settings.activePeakHold = true
            $0.settings.activePeakHoldFill = true
        }, frame: 6, extras: true),
        Step(name: "classic-hold-and-markers", change: {
            $0.settings.peakHold = true
            $0.peakHold = BandDrawReuseTests.line(7).map { $0 + 4 }
            let hz = $0.centerHz + 3_000
            $0.markers = [SliceMarkers.Marker(sliceId: 0, centerHz: hz, passbandHz: (hz + 100)...(hz + 2_900),
                                              style: SliceMarkers.style(for: "#00B4D8", selected: true))]
            $0.settings.showRxFilterOnWaterfall = true
            $0.settings.showRxZeroLineOnWaterfall = true
        }, frame: 7, extras: true),
        Step(name: "pan-with-everything", change: { $0.centerHz -= 1_234.5 }),
        Step(name: "paused", change: { $0.paused = true }),
        Step(name: "unpaused-grid-off", change: { $0.paused = false; $0.settings.grid = false }, frame: 8),
        Step(name: "grid-back-strip-off", change: { $0.settings.grid = true; $0.settings.bandPlanSize = .off }),
        Step(name: "no-frame-trace", change: { $0.settings.bandPlanSize = .small }, frame: 9),
    ]

    /// Line `index` of a made-up 40 m scene: four signals over noise.
    static func line(_ index: Int) -> [Float] {
        DisplayParityShotTests.line(samples, index: index)
    }

    @Test func everyDrawMatchesAFreshRenderersDraw() async throws {
        let catalog = try BandCatalogueRenderTests.catalogue()
        var settings = BandDisplaySettings.desktopDefaults
        settings.bandPlanSize = .medium
        var overlays = BandOverlays(centerHz: Self.centre, spanHz: Self.span, scale: Self.scale, settings: settings,
                                    catalog: catalog)
        overlays.frameCoverage = BandCoverage(centerHz: Self.centre, spanHz: Self.span)
        let layout = BandLayout(size: CGSize(width: Self.width, height: Self.height), scale: Self.scale,
                                settings: settings)
        var state = BandState(waterfallLines: layout.waterfallLines)
        for index in 0..<40 {
            state.receive(frame: BandFixtures.frame(trace: Self.line(100 + index), sequence: UInt32(index + 1)),
                          manualLevels: settings.manualLevels)
        }
        var sequence: UInt32 = 100
        var frame: DisplayFrame?
        var extras: DisplayExtras?
        let target = try Offscreen(width: Self.width, height: Self.height)
        let kept = try BandRenderer(device: target.device)
        let shots = ProcessInfo.processInfo.environment["NEREUS_BAND_DRAW_SHOTS"].flatMap { $0.isEmpty ? nil : $0 }
        for (number, step) in Self.steps.enumerated() {
            step.change(&overlays)
            extras = nil
            if let index = step.frame {
                sequence += 1
                let trace = Self.line(index)
                let next = BandFixtures.frame(trace: trace, sequence: sequence)
                state.receive(frame: next, manualLevels: settings.manualLevels)
                frame = next
                if step.extras {
                    let blobs = [0.22, 0.47, 0.81].map { at -> DisplayExtras.PeakBlob in
                        let sample = Int(at * Double(Self.samples - 1))
                        return DisplayExtras.PeakBlob(traceSample: sample, dbm: trace[sample])
                    }
                    extras = BandFixtures.extras(for: next, blobs: blobs, hold: trace.map { $0 + 3 }, floor: -126)
                }
            }
            let drawn = try target.render(kept, frame: frame, history: state.history, extras: extras,
                                          overlays: overlays)
            let fresh = try target.render(try BandRenderer(device: target.device), frame: frame,
                                          history: state.history, extras: extras ?? kept.heldExtras,
                                          overlays: overlays)
            #expect(drawn.bgra == fresh.bgra, "step \(number) \(step.name)")
            if let shots {
                for (image, kind) in [(drawn, "kept"), (fresh, "fresh")] {
                    let name = String(format: "%02d-%@-%@.bgra", number, step.name, kind)
                    try Data(image.bgra).write(to: URL(fileURLWithPath: shots).appendingPathComponent(name))
                }
            }
        }
    }
}
