// NereusSDR for iOS: offscreen renders of the band, compared at chosen pixels
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import LinkTestSupport
import Metal
import NereusMedia
import NereusModels
import Testing
@testable import NereusBand

/// R-IOS-11, D7: the band drawn offscreen and read back. Every palette,
/// plan and frame here is synthetic (D4).
@MainActor
@Suite(.serialized) struct BandRendererTests {
    static let width = 400
    static let height = 600
    static let centre = 7_200_000.0
    static let span = 40_000.0

    /// A plain band: no grid, no strip, so a pixel is what the test drew.
    static func plain() -> BandDisplaySettings {
        var settings = BandDisplaySettings.desktopDefaults
        settings.grid = false
        settings.bandPlanSize = .off
        return settings
    }

    static func overlays(_ settings: BandDisplaySettings, plan: StationCatalog.BandPlan? = nil,
                         palette: StationCatalog.Palette? = nil, scale: CGFloat = 1) -> BandOverlays {
        BandOverlays(centerHz: centre, spanHz: span, scale: scale, settings: settings, bandPlan: plan, palette: palette)
    }

    static func layout(_ settings: BandDisplaySettings, scale: CGFloat = 1) -> BandLayout {
        BandLayout(size: CGSize(width: width, height: height), scale: scale, settings: settings)
    }

    static func geometry(_ settings: BandDisplaySettings) -> BandGeometry {
        layout(settings).spectrumGeometry(centerHz: centre, spanHz: span, dbmRange: settings.scaleRange)
    }

    static func flat(_ samples: Int, _ dbm: Float = -135) -> [Float] {
        [Float](repeating: dbm, count: samples)
    }

    // MARK: The trace

    @Test func theTracesPeakSitsAtItsBinAndLevel() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        let settings = Self.plain()
        var trace = Self.flat(200)
        trace[57] = -60
        let frame = BandFixtures.frame(trace: trace)
        let image = try target.render(renderer, frame: frame, history: WaterfallHistory(capacity: 1), extras: nil,
                                      overlays: Self.overlays(settings))
        let geometry = Self.geometry(settings)
        let x = geometry.x(forTraceSample: 57, traceSamples: 200)
        let y = geometry.y(forDbm: -60)
        // The topmost bright trace pixel near the peak, and the column it is in.
        var top: (x: Int, y: Int)?
        for column in Int(x) - 3...Int(x) + 3 {
            for row in 0..<Int(geometry.size.height) where image.isTrace(column, row) {
                if top == nil || row < top!.y {
                    top = (column, row)
                }
                break
            }
        }
        let peak = try #require(top)
        #expect(abs(Double(peak.x) + 0.5 - x) <= 1, "peak x \(peak.x) against \(x)")
        #expect(abs(Double(peak.y) + 0.5 - y) <= 1, "peak y \(peak.y) against \(y)")
        // Away from the peak the trace is at the floor's level.
        let floorRow = Int(geometry.y(forDbm: -135))
        #expect((floorRow - 1...floorRow + 1).contains { image.isTrace(40, $0) })
    }

    /// Rows of trace colour in column `x` of `image` within the spectrum.
    static func traceRows(_ image: Image, x: Int, height: Int) -> Int {
        (0..<height).filter { image.isTrace(x, $0) }.count
    }

    @Test func theTraceIsDrawnAtTheLinesWidthAndNeverThinnerThanAPixel() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        var settings = Self.plain()
        settings.traceFill = false
        let frame = BandFixtures.frame(trace: Self.flat(200, -90))
        let height = Int(Self.geometry(settings).size.height)
        var rows: [Double: Int] = [:]
        for width in [0.2, 0.5, 1, 3] {
            settings.traceWidthPoints = width
            let image = try target.render(renderer, frame: frame, history: WaterfallHistory(capacity: 1), extras: nil,
                                          overlays: Self.overlays(settings))
            rows[width] = Self.traceRows(image, x: 200, height: height)
        }
        // At one pixel per point: a fifth and half a point draw one pixel, 3 points three.
        #expect(rows[0.2] == 1, "0.2 pt drew \(rows[0.2] ?? 0) rows")
        #expect(rows[0.5] == 1, "0.5 pt drew \(rows[0.5] ?? 0) rows")
        #expect(rows[1] == 1)
        #expect((3...4).contains(rows[3] ?? 0), "3 pt drew \(rows[3] ?? 0) rows")
    }

    @Test func theTracesVerticesAreTheLinesWidthApart() {
        var vertices: [BandRenderer.ColourVertex] = []
        for (points, scale, pixels) in [(0.5, 3.0, 1.5), (3.0, 3.0, 9.0), (0.25, 3.0, 1.0)] {
            var settings = BandDisplaySettings.desktopDefaults
            settings.traceWidthPoints = points
            vertices.removeAll()
            BandRenderer.addPolyline([SIMD2(0, 50), SIMD2(100, 50)],
                                     width: Float(settings.traceWidthPixels(scale: CGFloat(scale))),
                                     colour: SIMD4(1, 1, 1, 1), to: &vertices)
            let ys = vertices.map(\.position.y)
            #expect(Double((ys.max() ?? 0) - (ys.min() ?? 0)) == pixels, "\(points) pt at \(scale)x")
        }
    }

    @Test func aViewAheadOfTheCoreDrawsTheFrameAtItsOwnFrequenciesAndLeavesTheNewEdgeEmpty() throws {
        // D74: the view panned a quarter of the span up before the Core
        // answered. The frame and its waterfall line still cover the old
        // centre, so they shift a quarter of the width left, and the right
        // quarter, where nothing has arrived yet, is the background.
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        var settings = Self.plain()
        settings.traceFill = false
        let greys = try BandFixtures.palette(id: 7, [(0, "#000000"), (1, "#FFFFFF")])
        var trace = Self.flat(200, -135)
        trace[100] = -60
        let frame = BandFixtures.frame(trace: trace, waterfall: Self.flat(200, -62))
        let covered = BandCoverage(centerHz: Self.centre, spanHz: Self.span)
        var history = WaterfallHistory(capacity: 4)
        history.append(line: Self.flat(200, -62), levels: settings.manualLevels, coverage: covered)
        let geometry = Self.geometry(settings)
        let peakX = geometry.x(forTraceSample: 100, traceSamples: 200)
        let top = Int(Self.layout(settings).waterfall.minY)
        func columnsWithTrace(_ image: Image, in range: Range<Int>) -> [Int] {
            range.filter { x in (0..<Int(geometry.size.height)).contains { image.isTrace(x, $0) } }
        }

        let still = try target.render(renderer, frame: frame, history: history, extras: nil,
                                      overlays: BandOverlays(centerHz: Self.centre, spanHz: Self.span, scale: 1,
                                                             settings: settings, palette: greys, frameCoverage: covered))
        #expect(still.isTrace(Int(peakX), Int(geometry.y(forDbm: -60)) + 1))
        #expect(still.grey(Self.width - 10, top).isNear(255))

        let moved = try target.render(renderer, frame: frame, history: history, extras: nil,
                                      overlays: BandOverlays(centerHz: Self.centre + Self.span / 4, spanHz: Self.span,
                                                             scale: 1, settings: settings, palette: greys,
                                                             frameCoverage: covered))
        let shift = Double(Self.width) / 4
        // The peak moved left by the dragged distance.
        let peakRow = Int(geometry.y(forDbm: -60)) + 1
        let peakColumns = (0..<Self.width).filter { moved.isTrace($0, peakRow) }
        #expect(!peakColumns.isEmpty)
        #expect(peakColumns.allSatisfy { abs(Double($0) + 0.5 - (peakX - shift)) <= 2 }, "\(peakColumns)")
        // The new right quarter is empty in the spectrum and the waterfall.
        #expect(columnsWithTrace(moved, in: (Self.width * 3 / 4 + 2)..<Self.width).isEmpty)
        #expect(moved.grey(Self.width - 10, top).isNear(Self.backgroundGrey, tolerance: 12))
        #expect(moved.grey(20, top).isNear(255))
        // The old view's left edge is off the band; the band still shows data up to the frame's right edge.
        #expect(!columnsWithTrace(moved, in: 10..<(Self.width * 3 / 4 - 2)).isEmpty)
    }

    // MARK: The waterfall

    @Test func theWaterfallScrollsOneLinePerFrameEachColouredAgainstItsLevels() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        let settings = Self.plain()
        let greys = try BandFixtures.palette(id: 7, [(0, "#000000"), (1, "#FFFFFF")])
        let overlays = Self.overlays(settings, palette: greys)
        let layout = Self.layout(settings)
        var state = BandState(waterfallLines: layout.waterfallLines, expectsExtras: true)
        let manual = settings.manualLevels

        let first = BandFixtures.frame(trace: Self.flat(100, -50), sequence: 1)
        state.receive(frame: first, manualLevels: manual)
        state.receive(extras: BandFixtures.extras(for: first, levels: (-100, 0)), manualLevels: manual)
        var image = try target.render(renderer, frame: state.frame, history: state.history,
                                      extras: state.frameExtras, overlays: overlays)
        let top = Int(layout.waterfall.minY)
        #expect(image.grey(200, top).isNear(128))
        #expect(image.grey(200, top + 1).isNear(Self.backgroundGrey, tolerance: 12))

        let second = BandFixtures.frame(trace: Self.flat(100, -50), sequence: 2)
        state.receive(frame: second, manualLevels: manual)
        state.receive(extras: BandFixtures.extras(for: second, levels: (-50, 0)), manualLevels: manual)
        image = try target.render(renderer, frame: state.frame, history: state.history, extras: state.frameExtras,
                                  overlays: overlays)
        #expect(image.grey(200, top).isNear(0))
        #expect(image.grey(200, top + 1).isNear(128))
        #expect(image.grey(200, top + 2).isNear(Self.backgroundGrey, tolerance: 12))
    }

    static let backgroundGrey = 14

    /// A frame still on the graphics processor draws the waterfall as it
    /// was when the frame was made: the next frame's new line, landing in
    /// the row the oldest line held, does not reach it.
    @Test func aFrameInFlightKeepsItsWaterfallWhileTheNextFrameAddsALine() throws {
        let first = try Offscreen(width: Self.width, height: Self.height)
        let second = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: first.device)
        let settings = Self.plain()
        let overlays = Self.overlays(settings, palette: try BandFixtures.palette(id: 7, [(0, "#000000"), (1, "#FFFFFF")]))
        let layout = Self.layout(settings)
        // A full waterfall of quiet lines: the next line takes the oldest one's row, at the waterfall's foot.
        var history = WaterfallHistory(capacity: layout.waterfallLines)
        let levels = WaterfallHistory.Levels(lowDbm: -100, highDbm: 0)
        for _ in 0..<layout.waterfallLines {
            history.append(line: Self.flat(100, -100), levels: levels)
        }
        let gate = try #require(first.device.makeSharedEvent())
        renderer.commandBufferMade = { buffer in
            buffer.encodeWaitForEvent(gate, value: 1)
        }
        let held = try #require(renderer.draw(frame: nil, history: history, extras: nil, overlays: overlays,
                                              into: first.texture))
        renderer.commandBufferMade = nil
        history.append(line: Self.flat(100, 0), levels: levels)
        let next = try #require(renderer.draw(frame: nil, history: history, extras: nil, overlays: overlays,
                                              into: second.texture))
        gate.signaledValue = 1
        held.waitUntilCompleted()
        next.waitUntilCompleted()
        let foot = Int(layout.waterfall.maxY) - 1
        let top = Int(layout.waterfall.minY)
        let before = try first.read()
        #expect(before.grey(200, foot).isNear(0), "the held frame drew the next frame's line")
        #expect(before.grey(200, top).isNear(0))
        let after = try second.read()
        #expect(after.grey(200, top).isNear(255))
        #expect(after.grey(200, foot).isNear(0))
    }

    /// A burst of rows (a history caught up after a pause) grows a frame's
    /// staging buffer; once the bursts stop, the buffers go back to their
    /// everyday size rather than holding the burst's memory for good.
    @Test func stagingBuffersShrinkBackAfterABurst() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        let settings = Self.plain()
        let overlays = Self.overlays(settings, palette: try BandFixtures.palette(id: 7, [(0, "#000000"), (1, "#FFFFFF")]))
        let layout = Self.layout(settings)
        var history = WaterfallHistory(capacity: layout.waterfallLines)
        let levels = WaterfallHistory.Levels(lowDbm: -100, highDbm: 0)
        history.append(line: Self.flat(1_000, -100), levels: levels)
        renderer.draw(frame: nil, history: history, extras: nil, overlays: overlays, into: target.texture)?
            .waitUntilCompleted()
        // The burst: a whole history's worth of lines in one frame.
        for _ in 0..<layout.waterfallLines {
            history.append(line: Self.flat(1_000, -100), levels: levels)
        }
        renderer.draw(frame: nil, history: history, extras: nil, overlays: overlays, into: target.texture)?
            .waitUntilCompleted()
        let burst = layout.waterfallLines * 1_000
        #expect(renderer.stagingBufferLengths.max() ?? 0 >= burst)
        // Then a line a frame, long enough for every slot to see it.
        for _ in 0..<(RowUploads.oversizedFramesKept + 2) * BandRenderer.framesInFlight {
            history.append(line: Self.flat(1_000, -100), levels: levels)
            renderer.draw(frame: nil, history: history, extras: nil, overlays: overlays, into: target.texture)?
                .waitUntilCompleted()
        }
        #expect(renderer.stagingBufferLengths.allSatisfy { $0 <= RowUploads.smallestBuffer },
                "\(renderer.stagingBufferLengths)")
    }

    /// The shaders compile once a device: every renderer after the first
    /// takes the same library, so building a band's view does not compile
    /// them again on the main thread.
    @Test func everyRendererOnADeviceSharesOneCompiledLibrary() throws {
        let device = try #require(MTLCreateSystemDefaultDevice())
        let one = try BandRenderer(device: device)
        let two = try BandRenderer(device: device)
        #expect(one.library === two.library)
    }

    // MARK: The band-plan strip

    @Test func theStripShowsAPlansSegmentsWithTheirLabelsAtTheirFrequencies() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        var settings = Self.plain()
        settings.bandPlanSize = .medium
        let plan = try BandFixtures.plan(id: "t", isDefault: true, [
            (7_180_000, 7_200_000, "CW", "#C00000"),
            (7_200_000, 7_220_000, "SSB", "#0000C0"),
        ])
        let image = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                      overlays: Self.overlays(settings, plan: plan))
        let strip = Self.layout(settings).strip
        let geometry = Self.geometry(settings)
        let row = Int(strip.minY) + 1
        // Each segment's colour, at its frequencies, away from its label.
        let cw = image.pixel(Int(geometry.x(forHz: 7_182_000)), row)
        #expect(cw.r > 90 && cw.b < 40)
        let ssb = image.pixel(Int(geometry.x(forHz: 7_202_000)), row)
        #expect(ssb.b > 90 && ssb.r < 40)
        // Each label drawn at the middle of what shows of its segment: the
        // SSB segment runs on under the dBm scale, where the strip stops.
        let pieces = BandPlanStrip(plan: plan, geometry: geometry, rightX: Double(strip.maxX)).pieces
        #expect(pieces.map(\.highX) == [geometry.x(forHz: 7_200_000), Double(strip.maxX)])
        for piece in pieces {
            let x = Int(piece.labelX)
            let lit = (Int(strip.minY)..<Int(strip.maxY)).contains { y in
                (x - 6...x + 6).contains { image.pixel($0, y).isWhitish }
            }
            #expect(lit, "a label at \(piece.label)")
        }
        // Where there is no label, no white.
        let bare = Int(geometry.x(forHz: 7_183_000))
        #expect(!(Int(strip.minY)..<Int(strip.maxY)).contains { image.pixel(bare, $0).isWhitish })
    }

    /// One strip row of a render at `size` of `plan`, at `scale`.
    static func stripRender(_ plan: StationCatalog.BandPlan?, size: BandPlanSize, scale: CGFloat = 1) throws
        -> (image: Image, layout: BandLayout, geometry: BandGeometry) {
        let target = try Offscreen(width: width, height: height)
        let renderer = try BandRenderer(device: target.device)
        var settings = plain()
        settings.bandPlanSize = size
        let image = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                      overlays: overlays(settings, plan: plan, scale: scale))
        let layout = layout(settings, scale: scale)
        return (image, layout, layout.spectrumGeometry(centerHz: centre, spanHz: span, dbmRange: settings.scaleRange))
    }

    @Test func eachSegmentsColourIsDimmedByItsLicenceClassIntoTheBandsBackground() throws {
        // Five segments of one colour, each with another licence.
        let licences = ["E", "E,G", "E,G,T", "", "N"]
        let shares = [0.20, 0.40, 0.60, 0.50, 0.60]
        let colour = "#F08040"
        let step = 6_000.0
        let low = Self.centre - Self.span / 2 + 2_000
        let plan = try BandFixtures.plan(id: "l", isDefault: true, licensed: licences.enumerated().map { index, licence in
            (low + Double(index) * step, low + Double(index + 1) * step, "", licence, colour)
        })
        let (image, layout, geometry) = try Self.stripRender(plan, size: .small)
        for (index, licence) in licences.enumerated() {
            #expect(BandPlanStrip.colourShare(licence: licence) == shares[index])
            let fill = try #require(BandPlanStrip.fill(colour: colour, licence: licence))
            // The blend rounded down: #F08040 over #0A0A14.
            let share = shares[index]
            #expect(fill.red == Int(240 * share + 10 * (1 - share)))
            #expect(fill.green == Int(128 * share + 10 * (1 - share)))
            #expect(fill.blue == Int(64 * share + 20 * (1 - share)))
            let x = Int(geometry.x(forHz: low + (Double(index) + 0.5) * step))
            let pixel = image.pixel(x, Int(layout.strip.midY))
            #expect(pixel.r.isNear(fill.red, tolerance: 1) && pixel.g.isNear(fill.green, tolerance: 1)
                        && pixel.b.isNear(fill.blue, tolerance: 1), "licence \(licence): \(pixel)")
        }
    }

    @Test func aSeparatorMarksEachSegmentsLeftEdge() throws {
        let plan = try BandFixtures.plan(id: "s", isDefault: true, licensed: [
            (7_185_000, 7_195_000, "", "", "#C0C0C0"), (7_195_000, 7_210_000, "", "", "#C0C0C0"),
        ])
        let (image, layout, geometry) = try Self.stripRender(plan, size: .small)
        let fill = try #require(BandPlanStrip.fill(colour: "#C0C0C0", licence: ""))
        let separator = BandPlanStrip.separator
        let alpha = Double(separator.alpha) / 255
        let edgeRed = Int((Double(separator.red) * alpha + Double(fill.red) * (1 - alpha)).rounded())
        let y = Int(layout.strip.midY)
        for hz in [7_185_000.0, 7_195_000.0] {
            let edge = Int(geometry.x(forHz: hz).rounded(.down))
            #expect(image.pixel(edge, y).r.isNear(edgeRed, tolerance: 2), "edge at \(hz): \(image.pixel(edge, y))")
            #expect(image.pixel(edge + 3, y).r.isNear(fill.red, tolerance: 1))
        }
    }

    @Test func theStripIsItsSizePlusFourPointsHighAndEndsAtTheDbmScale() throws {
        let plan = try BandFixtures.plan(id: "w", isDefault: true, [(7_000_000, 7_400_000, "", "#C0C0C0")])
        let fill = try #require(BandPlanStrip.fill(colour: "#C0C0C0", licence: ""))
        for scale in [1.0, 2.0] as [CGFloat] {
            for size in BandPlanSize.allCases where size != .off {
                let (image, layout, _) = try Self.stripRender(plan, size: size, scale: scale)
                #expect(layout.strip.height == (size.stripHeightPoints * scale).rounded())
                #expect(layout.strip.maxX == layout.dbmScale.minX)
                #expect(layout.strip.maxY == layout.spectrum.maxY)
                let y = Int(layout.strip.midY)
                // Filled up to the dBm scale's left edge, and not under it.
                #expect(image.pixel(Int(layout.strip.maxX) - 1, y).r.isNear(fill.red, tolerance: 1))
                #expect(image.pixel(Int(layout.strip.maxX) + 1, y).r.isNear(Self.backgroundGrey, tolerance: 12))
                // Filled from its top row to its foot, and not above it.
                #expect(image.pixel(20, Int(layout.strip.minY)).r.isNear(fill.red, tolerance: 1))
                #expect(image.pixel(20, Int(layout.strip.maxY) - 1).r.isNear(fill.red, tolerance: 1))
                #expect(!image.pixel(20, Int(layout.strip.minY) - 1).r.isNear(fill.red, tolerance: 10))
            }
        }
    }

    @Test func offDrawsNoStrip() throws {
        let plan = try BandFixtures.plan(id: "o", isDefault: true, licensed: [(7_000_000, 7_400_000, "X", "", "#C0C0C0")],
                                         spots: [(7_200_000, "Dot")])
        let (image, layout, geometry) = try Self.stripRender(plan, size: .off)
        #expect(layout.strip.height == 0)
        #expect(geometry.size.height == layout.spectrum.height)
        // The spectrum's foot, where a strip would be, is the background.
        for y in Int(layout.spectrum.maxY) - 20..<Int(layout.spectrum.maxY) {
            #expect(image.pixel(100, y).r.isNear(Self.backgroundGrey, tolerance: 12))
            #expect(!image.pixel(Int(geometry.x(forHz: 7_200_000)), y).isWhitish)
        }
    }

    @Test func spotDotsAreDrawnMidStripOnlyWhenThePlanHasSpots() throws {
        let segment = [(7_000_000.0, 7_400_000.0, "", "", "#303030")]
        let spotHz = 7_195_000.0
        let withSpots = try BandFixtures.plan(id: "d", isDefault: true, licensed: segment, spots: [(spotHz, "Calling")])
        let without = try BandFixtures.plan(id: "n", isDefault: true, licensed: segment)
        for size in [BandPlanSize.small, .huge] {
            let (image, layout, geometry) = try Self.stripRender(withSpots, size: size)
            let x = Int(geometry.x(forHz: spotHz))
            let y = Int(layout.strip.midY)
            #expect(image.pixel(x, y).isWhitish, "a dot at \(size)")
            // Radius 4: white 2 points either side of the centre, and not 6.
            #expect(image.pixel(x + 2, y).isWhitish && image.pixel(x, y - 2).isWhitish)
            #expect(!image.pixel(x + 6, y).isWhitish)
            let (bare, _, _) = try Self.stripRender(without, size: size)
            #expect(!bare.pixel(x, y).isWhitish)
        }
    }

    // MARK: The Core's extras

    @Test func theCoresExtrasAreDrawnWhereTheCorePutThemAndKeptWithoutADatagram() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        var settings = Self.plain()
        settings.traceFill = false
        settings.noiseFloorLine = true
        settings.activePeakHold = true
        settings.peakBlobs = true
        let overlays = Self.overlays(settings)
        let geometry = Self.geometry(settings)
        let frame = BandFixtures.frame(trace: Self.flat(200, -139), sequence: 5)
        var hold = Self.flat(200, -100)
        hold[150] = -95
        let blobs = [DisplayExtras.PeakBlob(traceSample: 50, dbm: -70), DisplayExtras.PeakBlob(traceSample: 120, dbm: -75)]
        let extras = BandFixtures.extras(for: frame, blobs: blobs, hold: hold, floor: -80)

        func check(_ image: Image) {
            // The floor line at the floor's y (dashed: somewhere along a dash's length).
            let floorY = geometry.y(forDbm: -80)
            #expect((Int(floorY) - 1...Int(floorY)).contains { y in (300...306).contains { image.pixel($0, y).isFloor } },
                    "floor at \(floorY)")
            // The hold trace at its level (dashed too).
            let holdY = geometry.y(forDbm: -100)
            #expect((Int(holdY) - 1...Int(holdY)).contains { y in (20...26).contains { image.pixel($0, y).isHold } },
                    "hold at \(holdY)")
            // The blobs at their trace samples' x and their levels: a ring
            // about the point, its radius either side.
            for blob in blobs {
                let x = Int(geometry.x(forTraceSample: blob.traceSample, traceSamples: 200))
                let y = Int(geometry.y(forDbm: Double(blob.dbm)))
                #expect((x - 4...x - 2).contains { image.pixel($0, y).isBlob }, "blob ring left at \(x), \(y)")
                #expect((x + 2...x + 4).contains { image.pixel($0, y).isBlob }, "blob ring right at \(x), \(y)")
                #expect(!image.pixel(x + 8, y).isBlob)
            }
        }

        check(try target.render(renderer, frame: frame, history: WaterfallHistory(capacity: 1), extras: extras,
                                overlays: overlays))
        // The next frame comes without its datagram: the extras stay.
        let next = BandFixtures.frame(trace: Self.flat(200, -139), sequence: 6)
        check(try target.render(renderer, frame: next, history: WaterfallHistory(capacity: 1), extras: nil,
                                overlays: overlays))
        // A new context drops them.
        let moved = BandFixtures.frame(trace: Self.flat(200, -139), sequence: 1, generation: 2)
        let image = try target.render(renderer, frame: moved, history: WaterfallHistory(capacity: 1), extras: nil,
                                      overlays: overlays)
        #expect(!image.pixel(300, Int(geometry.y(forDbm: -80))).isFloor)
        #expect(renderer.heldExtras == nil)
    }

    // MARK: The phone's settings

    @Test func thePaletteChangesTheWaterfall() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        let settings = Self.plain()
        var history = WaterfallHistory(capacity: 4)
        history.append(line: Self.flat(100, -62), levels: settings.manualLevels)
        let reds = try BandFixtures.palette(id: 1, [(0, "#000000"), (1, "#FF0000")])
        let greens = try BandFixtures.palette(id: 2, [(0, "#000000"), (1, "#00FF00")])
        #expect(BandPalette.palette(id: 2, in: [reds, greens]) == greens)
        #expect(BandPalette.palette(id: 9, in: [reds, greens]) == reds)
        let top = Int(Self.layout(settings).waterfall.minY)
        let red = try target.render(renderer, frame: nil, history: history, extras: nil,
                                    overlays: Self.overlays(settings, palette: reds)).pixel(200, top)
        let green = try target.render(renderer, frame: nil, history: history, extras: nil,
                                      overlays: Self.overlays(settings, palette: greens)).pixel(200, top)
        #expect(red.r > 240 && red.g < 10)
        #expect(green.g > 240 && green.r < 10)
    }

    @Test func theGridChangesTheSpectrum() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        var settings = Self.plain()
        let y = Int(Self.geometry(settings).y(forDbm: -90).rounded(.down))
        let off = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                    overlays: Self.overlays(settings)).pixel(100, y)
        settings.grid = true
        let on = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                   overlays: Self.overlays(settings)).pixel(100, y)
        #expect(on.g > off.g + 15)
    }

    @Test func theTraceColourChangesTheTrace() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        var settings = Self.plain()
        settings.traceColour = "#FF0000"
        let frame = BandFixtures.frame(trace: Self.flat(200, -90))
        let image = try target.render(renderer, frame: frame, history: WaterfallHistory(capacity: 1), extras: nil,
                                      overlays: Self.overlays(settings))
        let y = Int(Self.geometry(settings).y(forDbm: -90))
        let stroke = try #require((y - 1...y).map { image.pixel(100, $0) }.max { $0.r < $1.r })
        #expect(stroke.r > 200 && stroke.g < 40 && stroke.b < 60)
    }

    @Test func theFillChangesBelowTheTrace() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        var settings = Self.plain()
        settings.traceFillOpacity = 1
        let frame = BandFixtures.frame(trace: Self.flat(200, -60))
        let below = Int(Self.geometry(settings).y(forDbm: -80))
        let filled = try target.render(renderer, frame: frame, history: WaterfallHistory(capacity: 1), extras: nil,
                                       overlays: Self.overlays(settings)).pixel(100, below)
        settings.traceFill = false
        let bare = try target.render(renderer, frame: frame, history: WaterfallHistory(capacity: 1), extras: nil,
                                     overlays: Self.overlays(settings)).pixel(100, below)
        #expect(filled.g > bare.g + 40)
        #expect(bare.g.isNear(Self.backgroundGreen, tolerance: 6))
    }

    static let backgroundGreen = 14

    @Test func theCoresPlanChoiceChangesTheStrip() throws {
        let target = try Offscreen(width: Self.width, height: Self.height)
        let renderer = try BandRenderer(device: target.device)
        var settings = Self.plain()
        settings.bandPlanSize = .small
        let usual = try BandFixtures.plan(id: "usual", isDefault: true, [(7_100_000, 7_300_000, "A", "#F00000")])
        let other = try BandFixtures.plan(id: "other", isDefault: false, [(7_100_000, 7_300_000, "B", "#00F000")])
        let row = Int(Self.layout(settings).strip.minY) + 1
        func stripPixel(_ name: String?) throws -> Image.Pixel {
            let plan = BandPlanStrip.plan(in: [usual, other], stationPlanName: name)
            return try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                     overlays: Self.overlays(settings, plan: plan)).pixel(20, row)
        }
        let byDefault = try stripPixel(nil)
        #expect(byDefault.r > 100 && byDefault.g < 40)
        let chosen = try stripPixel(other.name)
        #expect(chosen.g > 100 && chosen.r < 40)
    }

    // MARK: Speed

    /// Task 52's acceptance (under 8 ms a frame, 1179 pixels wide) is
    /// proven on the iPhone; here the test guards against a renderer that
    /// got slower. This Mac's GPU is shared with other sessions, so each
    /// band draw is paired with a reference draw into a target of the same
    /// size, timed in the same loop: contention slows both, a slower
    /// renderer slows only the band.
    @Test func aPhoneWideBandDrawsInUnderEightMilliseconds() throws {
        let width = 1179
        let height = 1200
        let target = try Offscreen(width: width, height: height)
        let reference = try ReferenceDraw(device: target.device, width: width, height: height)
        let renderer = try BandRenderer(device: target.device)
        var settings = BandDisplaySettings.desktopDefaults
        settings.peakBlobs = true
        settings.activePeakHold = true
        settings.noiseFloorLine = true
        let plan = try BandFixtures.plan(id: "t", isDefault: true, [
            (7_125_000, 7_175_000, "ONE", "#806000"), (7_175_000, 7_300_000, "TWO", "#604000"),
        ])
        let overlays = BandOverlays(centerHz: Self.centre, spanHz: 48_000, scale: 3, settings: settings, bandPlan: plan,
                                    palette: try BandFixtures.palette(id: 0, [(0, "#000020"), (0.5, "#0060FF"),
                                                                              (1, "#FFFFFF")]))
        let layout = BandLayout(size: CGSize(width: width, height: height), scale: 3)
        var state = BandState(waterfallLines: layout.waterfallLines, expectsExtras: true)
        var generator = SystemRandomNumberGenerator()
        var times: [Double] = []
        var referenceTimes: [Double] = []
        func milliseconds(since start: UInt64) -> Double {
            Double(DispatchTime.now().uptimeNanoseconds - start) / 1e6
        }
        for sequence in 1...90 {
            let trace = (0..<width).map { _ in Float.random(in: -135 ... -95, using: &generator) }
            let frame = BandFixtures.frame(trace: trace, sequence: UInt32(sequence))
            let blobs = (0..<3).map { DisplayExtras.PeakBlob(traceSample: $0 * 300 + 17, dbm: -80) }
            state.receive(frame: frame, manualLevels: settings.manualLevels)
            state.receive(extras: BandFixtures.extras(for: frame, blobs: blobs, hold: trace.map { $0 + 5 }, floor: -120,
                                                      levels: (-130, -70)),
                          manualLevels: settings.manualLevels)
            var start = DispatchTime.now().uptimeNanoseconds
            let buffer = renderer.draw(frame: state.frame, history: state.history, extras: state.frameExtras,
                                       overlays: overlays, into: target.texture)
            buffer?.waitUntilCompleted()
            let elapsed = milliseconds(since: start)
            start = DispatchTime.now().uptimeNanoseconds
            try reference.draw()
            let referenceElapsed = milliseconds(since: start)
            if sequence > 10 {
                times.append(elapsed)
                referenceTimes.append(referenceElapsed)
            }
        }
        times.sort()
        referenceTimes.sort()
        let median = times[times.count / 2]
        let worst = times[times.count - 1]
        let mean = times.reduce(0, +) / Double(times.count)
        // The mean over the fastest 95%: a frame the host's scheduler held
        // up cannot decide the test on its own.
        let kept = times.prefix(times.count - times.count / 20)
        let trimmedMean = kept.reduce(0, +) / Double(kept.count)
        let referenceMedian = referenceTimes[referenceTimes.count / 2]
        let ratio = median / referenceMedian
        func ms(_ value: Double) -> String { String(format: "%.2f", value) }
        print("BandRenderer 1179 x 1200 px on \(target.device.name): median \(ms(median)) ms, "
              + "mean of the fastest \(kept.count) \(ms(trimmedMean)) ms, mean \(ms(mean)) ms, "
              + "worst \(ms(worst)) ms over \(times.count) frames; reference median \(ms(referenceMedian)) ms, "
              + "ratio \(ms(ratio))")
        #expect(ratio < Self.speedRatioLimit, "the band's median against the reference draw's")
        // A backstop for a renderer slow whatever the host is doing.
        #expect(median < Self.speedCeilingMs)
    }

    /// Measured on an M5 Max on 2026-09-25: the renderer as built ran at
    /// 1.06 to 1.41 times the reference (load averages 13 to 134), and one
    /// doing three times its drawing work ran at 2.20 to 3.32.
    static let speedRatioLimit = 1.8
    static let speedCeilingMs = 25.0
}

/// A fixed reference draw for the speed test, shaped like a band frame:
/// a fixed amount of vertex building on the CPU, copied into a buffer, then
/// a full-target clear and one trivial pass over a target of the band's
/// size, committed and waited for. The band's time is mostly the CPU's
/// encoding plus the wait for the GPU, so the reference has both: a busy
/// CPU or a shared GPU slows it as it slows the band.
@MainActor
final class ReferenceDraw {
    private let queue: any MTLCommandQueue
    private let texture: any MTLTexture
    private let pipeline: any MTLRenderPipelineState
    private let buffer: any MTLBuffer
    private var vertices: [SIMD4<Float>] = []

    /// The reference's vertices a frame.
    static let vertexCount = 10_000

    static let source = """
    #include <metal_stdlib>
    using namespace metal;
    vertex float4 referenceVertex(uint id [[vertex_id]]) {
        float2 corner = float2((id << 1) & 2, id & 2);
        return float4(corner * 2 - 1, 0, 1);
    }
    fragment float4 referenceFragment() { return float4(0.2, 0.4, 0.6, 1); }
    """

    init(device: any MTLDevice, width: Int, height: Int) throws {
        guard let queue = device.makeCommandQueue(),
              let buffer = device.makeBuffer(length: Self.vertexCount * MemoryLayout<SIMD4<Float>>.stride,
                                             options: .storageModeShared) else {
            throw BandRenderer.SetupError.noDevice
        }
        let descriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .bgra8Unorm, width: width,
                                                                  height: height, mipmapped: false)
        descriptor.usage = [.renderTarget]
        descriptor.storageMode = .private
        guard let texture = device.makeTexture(descriptor: descriptor) else {
            throw BandRenderer.SetupError.noDevice
        }
        let library = try device.makeLibrary(source: Self.source, options: nil)
        let pipelineDescriptor = MTLRenderPipelineDescriptor()
        pipelineDescriptor.vertexFunction = library.makeFunction(name: "referenceVertex")
        pipelineDescriptor.fragmentFunction = library.makeFunction(name: "referenceFragment")
        pipelineDescriptor.colorAttachments[0].pixelFormat = .bgra8Unorm
        self.queue = queue
        self.texture = texture
        self.buffer = buffer
        self.pipeline = try device.makeRenderPipelineState(descriptor: pipelineDescriptor)
    }

    func draw() throws {
        vertices.removeAll(keepingCapacity: true)
        for index in 0..<Self.vertexCount {
            let x = Float(index) / Float(Self.vertexCount)
            vertices.append(SIMD4<Float>(x * 2 - 1, sin(x * 40), cos(x * 40), 1))
        }
        vertices.withUnsafeBytes { bytes in
            buffer.contents().copyMemory(from: bytes.baseAddress!, byteCount: bytes.count)
        }
        let pass = MTLRenderPassDescriptor()
        pass.colorAttachments[0].texture = texture
        pass.colorAttachments[0].loadAction = .clear
        pass.colorAttachments[0].storeAction = .store
        pass.colorAttachments[0].clearColor = MTLClearColor(red: 0, green: 0, blue: 0, alpha: 1)
        guard let commands = queue.makeCommandBuffer(),
              let encoder = commands.makeRenderCommandEncoder(descriptor: pass) else {
            throw BandRenderer.SetupError.noDevice
        }
        encoder.setRenderPipelineState(pipeline)
        encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
        encoder.endEncoding()
        commands.commit()
        commands.waitUntilCompleted()
    }
}

// MARK: Reading a render back

/// An offscreen target and its read-back.
@MainActor
final class Offscreen {
    let device: any MTLDevice
    let texture: any MTLTexture
    private let queue: any MTLCommandQueue
    private let buffer: any MTLBuffer

    init(width: Int, height: Int) throws {
        guard let device = MTLCreateSystemDefaultDevice(), let queue = device.makeCommandQueue() else {
            throw BandRenderer.SetupError.noDevice
        }
        let descriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .bgra8Unorm, width: width,
                                                                  height: height, mipmapped: false)
        descriptor.usage = [.renderTarget, .shaderRead]
        descriptor.storageMode = .private
        guard let texture = device.makeTexture(descriptor: descriptor),
              let buffer = device.makeBuffer(length: width * height * 4, options: .storageModeShared) else {
            throw BandRenderer.SetupError.noDevice
        }
        self.device = device
        self.queue = queue
        self.texture = texture
        self.buffer = buffer
    }

    func render(_ renderer: BandRenderer, frame: DisplayFrame?, history: WaterfallHistory, extras: DisplayExtras?,
                overlays: BandOverlays, diagnostic: HostedDiagnosticReceipts? = nil) throws -> Image {
        // The renderer commits on its own queue: wait for it before reading.
        diagnostic?.mark("renderer.draw entry")
        let commandBuffer = renderer.draw(frame: frame, history: history, extras: extras, overlays: overlays, into: texture)
        diagnostic?.mark("renderer.draw returned")
        diagnostic?.mark("render command-buffer wait entry")
        commandBuffer?.waitUntilCompleted()
        diagnostic?.mark("render command-buffer wait returned")
        return try read(diagnostic: diagnostic)
    }

    func read(diagnostic: HostedDiagnosticReceipts? = nil) throws -> Image {
        diagnostic?.mark("blit setup entry")
        guard let commands = queue.makeCommandBuffer(), let blit = commands.makeBlitCommandEncoder() else {
            throw BandRenderer.SetupError.noDevice
        }
        blit.copy(from: texture, sourceSlice: 0, sourceLevel: 0, sourceOrigin: MTLOrigin(x: 0, y: 0, z: 0),
                  sourceSize: MTLSize(width: texture.width, height: texture.height, depth: 1), to: buffer,
                  destinationOffset: 0, destinationBytesPerRow: texture.width * 4,
                  destinationBytesPerImage: texture.width * texture.height * 4)
        blit.endEncoding()
        diagnostic?.mark("blit commit entry")
        commands.commit()
        diagnostic?.mark("blit commit returned")
        diagnostic?.mark("blit command-buffer wait entry")
        commands.waitUntilCompleted()
        diagnostic?.mark("blit command-buffer wait returned")
        let bytes = [UInt8](UnsafeRawBufferPointer(start: buffer.contents(), count: buffer.length))
        return Image(width: texture.width, height: texture.height, bgra: bytes)
    }
}

/// A read-back render, BGRA.
struct Image {
    struct Pixel: Equatable {
        let r: Int
        let g: Int
        let b: Int

        var isWhitish: Bool { r > 170 && g > 170 && b > 170 }
        /// The renderer's noise-floor magenta.
        var isFloor: Bool { r > 200 && g < 120 && b > 200 }
        /// The peak-hold trace's gold.
        var isHold: Bool { r > 200 && (170...235).contains(g) && b < 60 }
        /// The peak blobs' orange-red ring and their green value, the desktop's.
        var isBlob: Bool { r > 200 && (30...110).contains(g) && b < 60 }
        var isBlobText: Bool { (90...170).contains(r) && g > 200 && b < 80 }
        /// The noise floor's yellow value, the desktop's.
        var isYellow: Bool { r > 200 && g > 200 && b < 90 }
    }

    let width: Int
    let height: Int
    let bgra: [UInt8]

    func pixel(_ x: Int, _ y: Int) -> Pixel {
        let x = min(max(x, 0), width - 1)
        let y = min(max(y, 0), height - 1)
        let offset = (y * width + x) * 4
        return Pixel(r: Int(bgra[offset + 2]), g: Int(bgra[offset + 1]), b: Int(bgra[offset]))
    }

    func grey(_ x: Int, _ y: Int) -> Int {
        let p = pixel(x, y)
        return (p.r + p.g + p.b) / 3
    }

    /// A bright pixel of the default trace colour.
    func isTrace(_ x: Int, _ y: Int) -> Bool {
        let p = pixel(x, y)
        return p.g > 150 && p.b > 150 && p.r < 120
    }
}

extension Int {
    func isNear(_ value: Int, tolerance: Int = 3) -> Bool {
        abs(self - value) <= tolerance
    }
}
