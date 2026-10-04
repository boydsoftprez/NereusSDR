// NereusSDR for iOS: the band drawn with a made-up scene for side-by-side pictures against the desktop's display
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusMedia
import NereusModels
import Testing
@testable import NereusBand

/// R-IOS-11 (Task 54e): the waterfall, the trace and fill, the noise
/// floor, peaks and peak hold, drawn over one made-up 40 m scene (D4: the
/// signals and noise are generated here) at 804 by 1206 pixels, 2 pixels a
/// point, the Core's catalogue palette. With `NEREUS_PARITY_SHOTS` set to a
/// directory the pictures are written there, to set beside the desktop's
/// renders of the same scene.
@MainActor
@Suite(.serialized) struct DisplayParityShotTests {
    static let width = 804
    static let height = 1206
    static let scale: CGFloat = 2
    /// The scale drawn at: ``scale``, or `NEREUS_PARITY_SCALE` (1 draws a
    /// point a pixel, as the desktop's own renders are, so sizes compare).
    static var shotScale: CGFloat {
        let asked = ProcessInfo.processInfo.environment["NEREUS_PARITY_SCALE"].flatMap(Double.init)
        return asked.map { CGFloat($0) } ?? scale
    }
    static let centre = 7_224_000.0
    static let span = 48_000.0
    /// The newest line's number; older lines count down from it.
    static let newest = 400

    /// One line of the scene: four signals of different widths over noise,
    /// drifting a little from line to line.
    static func line(_ width: Int, index: Int) -> [Float] {
        var state = UInt32(truncatingIfNeeded: index &* 7919 &+ 17)
        func noise() -> Float {
            state = (state &* 1_103_515_245 &+ 12_345) & 0x7fff_ffff
            return Float(state % 10_000) / 10_000 * 6 - 3
        }
        let signals: [(at: Double, level: Float, width: Double)] = [(0.22, -72, 3), (0.47, -88, 6), (0.63, -100, 2),
                                                                    (0.81, -80, 10)]
        return (0..<width).map { sample in
            var value = -128 + noise()
            for signal in signals {
                let distance = (Double(sample) - signal.at * Double(width - 1)) / signal.width
                let drift = Float(sin(Double(index) * 0.07 + signal.at * 10) * 2)
                value = max(value, signal.level + drift - Float(6 * distance * distance))
            }
            return value
        }
    }

    struct Scene: Sendable, CustomStringConvertible {
        let name: String
        var gradient = false
        var noiseFloor = false
        var peaks = false
        var peakHold = false
        /// Part 2: 1 grid and zero line, 2 classic peak hold, 3 the
        /// waterfall's filter, zero line and opacity, 4 labels left.
        var more = 0
        var description: String { name }
    }

    nonisolated static let scenes = [
        Scene(name: "phone-1-waterfall-trace-fill"),
        Scene(name: "phone-2-gradient-fill", gradient: true),
        Scene(name: "phone-3-noise-floor", noiseFloor: true),
        Scene(name: "phone-4-peaks", peaks: true),
        Scene(name: "phone-5-peak-hold", peakHold: true),
        Scene(name: "phone-6-grid-zero-line", more: 1),
        Scene(name: "phone-7-classic-peak-hold", more: 2),
        Scene(name: "phone-8-waterfall-filter-zero-line-opacity", more: 3),
        Scene(name: "phone-9-labels-left", more: 4),
    ]

    @Test(arguments: scenes)
    func theSceneDrawsAsTheDesktopsDoes(_ scene: Scene) throws {
        let catalog = try BandCatalogueRenderTests.catalogue()
        var settings = BandDisplaySettings.desktopDefaults
        settings.bandPlanSize = .off
        settings.traceGradient = scene.gradient
        settings.noiseFloorLine = scene.noiseFloor
        settings.peakBlobs = scene.peaks
        settings.activePeakHold = scene.peakHold
        settings.activePeakHoldFill = scene.peakHold
        switch scene.more {
        case 1:
            settings.showZeroLine = true
            settings.scaleTopDbm = 10
            settings.scaleBottomDbm = -90
        case 2:
            settings.peakHold = true
        case 3:
            settings.showRxFilterOnWaterfall = true
            settings.showRxZeroLineOnWaterfall = true
            settings.waterfallOpacityPercent = 50
        case 4:
            settings.frequencyLabelAlignment = .left
        default:
            break
        }
        var overlays = BandOverlays(centerHz: Self.centre, spanHz: Self.span, scale: Self.shotScale, settings: settings,
                                    catalog: catalog)
        let layout = BandLayout(size: CGSize(width: Self.width, height: Self.height), scale: Self.shotScale,
                                settings: settings)
        var state = BandState(waterfallLines: layout.waterfallLines, expectsExtras: true)
        state.levelAdjustment = settings.waterfallAdjustment
        var frame: DisplayFrame?
        var trace: [Float] = []
        let first = Self.newest - layout.waterfallLines + 1
        for index in first...Self.newest {
            trace = Self.line(Self.width, index: index)
            let next = BandFixtures.frame(trace: trace, sequence: UInt32(index - first + 1))
            state.receive(frame: next, manualLevels: settings.manualLevels)
            let blobs = [0.22, 0.47, 0.81].map { at -> DisplayExtras.PeakBlob in
                let around = Int(at * Double(Self.width - 1))
                let sample = (around - 8...around + 8).max { trace[$0] < trace[$1] } ?? around
                return DisplayExtras.PeakBlob(traceSample: sample, dbm: trace[sample])
            }
            let hold = trace.indices.map { sample in
                (max(0, sample - 3)...min(trace.count - 1, sample + 3)).map { trace[$0] }.max()! + 3
            }
            state.receive(extras: BandFixtures.extras(for: next, blobs: blobs, hold: hold, floor: -126,
                                                      levels: (-125, -65)),
                          manualLevels: settings.manualLevels)
            frame = next
        }
        if scene.more == 2 {
            overlays.peakHold = trace.indices.map { sample in
                (max(0, sample - 3)...min(trace.count - 1, sample + 3)).map { trace[$0] }.max()! + 3
            }
        }
        if scene.more == 3 {
            let hz = Self.centre + 5_000
            overlays.markers = [SliceMarkers.Marker(sliceId: 0, centerHz: hz, passbandHz: (hz + 100)...(hz + 2_900),
                                                    style: SliceMarkers.style(for: "#00B4D8", selected: true))]
        }
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        let image = try target.render(renderer, frame: frame, history: state.history, extras: state.frameExtras,
                                      overlays: overlays)
        // The waterfall's top line was coloured after Color Gain and Black Level.
        #expect(state.history.currentLevels.map { abs($0.lowDbm - (-116.6)) < 0.01 } == true)
        #expect(image.width == Self.width)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_PARITY_SHOTS"], !directory.isEmpty {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(scene.name).png")
            try image.writePNG(to: url)
        }
    }
}
