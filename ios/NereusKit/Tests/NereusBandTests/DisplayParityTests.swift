// NereusSDR for iOS: tests that the band draws as the desktop's does: the waterfall, the fill, the extras, the split
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusMedia
import Testing
@testable import NereusBand

/// R-IOS-11, D73: the band at the desktop's display (Task 54e, the parity
/// audit's rows). Every frame, palette and level here is synthetic (D4).
@MainActor
@Suite(.serialized) struct DisplayParityTests {
    static let width = BandRendererTests.width
    static let height = BandRendererTests.height

    static func geometry(_ settings: BandDisplaySettings) -> BandGeometry {
        BandRendererTests.geometry(settings)
    }

    static func render(_ settings: BandDisplaySettings, frame: DisplayFrame?, extras: DisplayExtras? = nil,
                       history: WaterfallHistory = WaterfallHistory(capacity: 1)) throws -> Image {
        let target = try Offscreen(width: width, height: height)
        let renderer = try BandRenderer(device: target.device)
        return try target.render(renderer, frame: frame, history: history, extras: extras,
                                 overlays: BandRendererTests.overlays(settings))
    }

    // MARK: Row 1: the waterfall's Color Gain and Black Level

    @Test func colorGainAndBlackLevelStartAtTheDesktopsDefaults() {
        let settings = BandDisplaySettings.desktopDefaults
        #expect(settings.waterfallColorGain == 45)
        #expect(settings.waterfallBlackLevel == 104)
        #expect(settings.waterfallAdjustment == .init(colorGain: 45, blackLevel: 104))
    }

    @Test func colorGainAndBlackLevelNarrowTheLevelsAsTheDesktopDoes() {
        let levels = WaterfallHistory.Levels(lowDbm: -122, highDbm: -62)
        let adjusted = levels.adjusted(by: BandDisplaySettings.desktopDefaults.waterfallAdjustment)
        // The floor rises 0.4 dB a step under 125, the top falls 0.3 dB a step of gain.
        #expect(abs(adjusted.lowDbm - (-113.6)) < 0.001)
        #expect(abs(adjusted.highDbm - (-75.5)) < 0.001)
        // Black Level at its top and no gain change nothing.
        #expect(levels.adjusted(by: .none) == levels)
        // The top never comes down to the floor.
        let squeezed = WaterfallHistory.Levels(lowDbm: -100, highDbm: -90)
            .adjusted(by: .init(colorGain: 100, blackLevel: 0))
        #expect(squeezed.highDbm == squeezed.lowDbm + 1)
    }

    @Test func eachLineIsColouredAgainstTheCoresLevelsAfterGainAndBlack() {
        // The Core's Clarity says -100 to 0; with the desktop's defaults the
        // line is coloured against -91.6 to -13.5, so their middle is mid-palette.
        var state = BandState(waterfallLines: 4, expectsExtras: true)
        state.levelAdjustment = BandDisplaySettings.desktopDefaults.waterfallAdjustment
        let middle: Float = (-91.6 + -13.5) / 2
        let frame = BandFixtures.frame(trace: [Float](repeating: middle, count: 8), sequence: 1)
        let manual = BandDisplaySettings.desktopDefaults.manualLevels
        state.receive(frame: frame, manualLevels: manual)
        state.receive(extras: BandFixtures.extras(for: frame, levels: (-100, 0)), manualLevels: manual)
        let first = state.history.line(age: 0)?.first.map(Int.init) ?? -1
        #expect((126...129).contains(first), "palette position \(first)")
        #expect(state.history.currentLevels.map { abs($0.lowDbm - (-91.6)) < 0.001 } == true)
    }

    @Test func theManualLevelsTakeGainAndBlackToo() {
        var state = BandState(waterfallLines: 4)
        state.levelAdjustment = .init(colorGain: 0, blackLevel: 0)
        // Black Level 0 raises the floor 50 dB: -122 becomes -72.
        let frame = BandFixtures.frame(trace: [Float](repeating: -73, count: 4))
        state.receive(frame: frame, manualLevels: .init(lowDbm: -122, highDbm: -62))
        #expect(state.history.line(age: 0)?.first == 0)
    }

    // MARK: Row 5: the trace's fill

    @Test func theFillIsFlatAtFourTenthsOfItsStrength() throws {
        var settings = BandRendererTests.plain()
        settings.traceColour = "#FF0000"
        settings.traceFillOpacity = 0.5
        let geometry = Self.geometry(settings)
        let image = try Self.render(settings, frame: BandFixtures.frame(trace: BandRendererTests.flat(200, -50)))
        // Red at 0.2 over the background, the same just under the trace and at the foot.
        let expected = Int((255 * 0.2 + 10 * 0.8).rounded())
        let high = image.pixel(100, Int(geometry.y(forDbm: -60)))
        let low = image.pixel(100, Int(geometry.y(forDbm: -135)))
        #expect(high.r.isNear(expected, tolerance: 3), "under the trace \(high)")
        #expect(low.r.isNear(expected, tolerance: 3), "at the foot \(low)")
    }

    @Test func theGradientRunsFromTheWholeStrengthAtTheTopToNothingAtTheFoot() throws {
        var settings = BandRendererTests.plain()
        settings.traceColour = "#FF0000"
        settings.traceFillOpacity = 1
        settings.traceGradient = true
        let geometry = Self.geometry(settings)
        let image = try Self.render(settings, frame: BandFixtures.frame(trace: BandRendererTests.flat(200, -40)))
        let height = Double(geometry.size.height)
        let quarter = image.pixel(100, Int(height * 0.25))
        let expected = Int((255 * 0.75 + 10 * 0.25).rounded())
        #expect(quarter.r.isNear(expected, tolerance: 4), "a quarter down \(quarter)")
        let foot = image.pixel(100, Int(height) - 1)
        #expect(foot.r.isNear(10, tolerance: 4), "at the foot \(foot)")
        #expect(!BandDisplaySettings.desktopDefaults.traceGradient)
    }

    // MARK: Row 35: the background

    @Test func theBackgroundIsTheDesktops() throws {
        let image = try Self.render(BandRendererTests.plain(), frame: nil)
        let pixel = image.pixel(100, 40)
        #expect(pixel.r.isNear(10, tolerance: 1) && pixel.g.isNear(10, tolerance: 1) && pixel.b.isNear(20, tolerance: 1),
                "\(pixel)")
    }

    // MARK: Row 6: the noise floor

    @Test func theNoiseFloorIsADashedLineFromAMarkerWithItsValue() throws {
        var settings = BandRendererTests.plain()
        settings.traceFill = false
        settings.noiseFloorLine = true
        let frame = BandFixtures.frame(trace: BandRendererTests.flat(200, -139))
        let image = try Self.render(settings, frame: frame, extras: BandFixtures.extras(for: frame, floor: -80))
        let layout = BandRendererTests.layout(settings)
        let y = Int(Self.geometry(settings).y(forDbm: -80).rounded(.down))
        // D83: the desktop's 40 in from each side, its 8 square marker,
        // its dashes 2 on and 2 off, starting with a gap.
        #expect(BandRenderer.noiseFloorInsetPoints == 40)
        #expect(BandRenderer.noiseFloorMarkerPoints == 8)
        #expect(BandRenderer.noiseFloorDashPoints == 2 && BandRenderer.noiseFloorGapPoints == 2)
        #expect(BandRenderer.noiseFloorDashPhasePoints == 2)
        let inset = 40
        let marker = 8
        // Nothing left of the marker; the marker a square above the line.
        #expect(!(0..<inset - 1).contains { image.pixel($0, y).isFloor })
        #expect(image.pixel(inset + marker / 2, y - marker / 2).isFloor)
        #expect(!image.pixel(inset + marker / 2, y - marker - 1).isFloor)
        // Dashed along, a gap first: on and off in turn, every other 2.
        #expect(!image.pixel(inset, y).isFloor && !image.pixel(inset + 1, y).isFloor)
        #expect(image.pixel(inset + 2, y).isFloor && image.pixel(inset + 3, y).isFloor)
        let right = Int(layout.spectrum.maxX) - inset
        let along = (inset + marker + 4)..<right
        let lit = along.filter { image.pixel($0, y).isFloor }.count
        #expect(abs(lit - along.count / 2) <= 2, "\(lit) of \(along.count) lit")
        // It stops 40 in from the spectrum's right edge.
        #expect(!(right + 1..<Self.width).contains { image.pixel($0, y).isFloor })
        // The floor's value in yellow, its origin 2 right of the marker (the ink
        // starts after the font's side bearing, wider on iOS than on macOS), its
        // top about 6 above the marker's.
        let yellow = (inset + marker..<inset + marker + 60).flatMap { x in (y - 30..<y).map { (x, $0) } }
            .filter { image.pixel($0.0, $0.1).isYellow }
        #expect(!yellow.isEmpty)
        let textLeft = yellow.map(\.0).min() ?? 0
        let textTop = yellow.map(\.1).min() ?? 0
        #expect((inset + marker + 1...inset + marker + 10).contains(textLeft), "text starts at \(textLeft)")
        #expect((y - marker - 6...y - marker - 3).contains(textTop), "text top at \(textTop)")
        // To one decimal place, as the desktop shows it.
        #expect(BandRenderer.valueText(-80.44) == "-80.4")
        #expect(BandRenderer.valueText(-80) == "-80.0")
    }

    // MARK: Row 7: peak blobs

    @Test func aPeakBlobIsARingWithItsDbmBesideIt() throws {
        var settings = BandRendererTests.plain()
        settings.traceFill = false
        settings.peakBlobs = true
        let frame = BandFixtures.frame(trace: BandRendererTests.flat(200, -139))
        let blob = DisplayExtras.PeakBlob(traceSample: 60, dbm: -70)
        let image = try Self.render(settings, frame: frame, extras: BandFixtures.extras(for: frame, blobs: [blob]))
        let geometry = Self.geometry(settings)
        let x = Int(geometry.x(forTraceSample: 60, traceSamples: 200))
        let y = Int(geometry.y(forDbm: -70))
        // D83: the desktop's ring, radius 3, 1 wide, orange-red.
        #expect(BandRenderer.blobRadiusPoints == 3 && BandRenderer.blobStrokePoints == 1)
        #expect(BandDisplaySettings.desktopDefaults.peakBlobColour == "#FF4500FF")
        #expect(BandDisplaySettings.desktopDefaults.peakBlobTextColour == "#7FFF00FF")
        // Hollow: the centre is the background, the ring at its radius.
        #expect(!image.pixel(x, y).isBlob)
        #expect((x + 2...x + 4).contains { image.pixel($0, y).isBlob })
        #expect((y - 4...y - 2).contains { image.pixel(x, $0).isBlob })
        #expect(!image.pixel(x + 6, y).isBlob)
        // Its value in green, starting 5 right of the centre, its baseline 6 above it.
        let green = (x..<x + 60).flatMap { column in (y - 30..<y + 4).map { (column, $0) } }
            .filter { image.pixel($0.0, $0.1).isBlobText }
        #expect(!green.isEmpty)
        let left = green.map(\.0).min() ?? 0
        let bottom = green.map(\.1).max() ?? 0
        #expect((x + 5...x + 7).contains(left), "text starts at \(left)")
        #expect((y - 8...y - 5).contains(bottom), "text foot at \(bottom)")
    }

    // MARK: Row 8: active peak hold

    @Test func peakHoldIsDashedGoldAndItsFillShadesDownToTheTrace() throws {
        var settings = BandRendererTests.plain()
        settings.traceFill = false
        settings.activePeakHold = true
        let frame = BandFixtures.frame(trace: BandRendererTests.flat(200, -130))
        let extras = BandFixtures.extras(for: frame, hold: BandRendererTests.flat(200, -90))
        let geometry = Self.geometry(settings)
        let y = Int(geometry.y(forDbm: -90))
        var image = try Self.render(settings, frame: frame, extras: extras)
        let row = (10..<300).map { x in (y - 1...y).contains { image.pixel(x, $0).isHold } }
        let lit = row.filter { $0 }.count
        #expect(lit > row.count / 3 && lit < row.count, "\(lit) of \(row.count) lit")
        let between = Int(geometry.y(forDbm: -110))
        #expect(image.pixel(100, between).r < 30, "no fill while off")
        #expect(!BandDisplaySettings.desktopDefaults.activePeakHoldFill)

        settings.activePeakHoldFill = true
        image = try Self.render(settings, frame: frame, extras: extras)
        let shaded = image.pixel(100, between)
        // Gold at 0.18 over the background.
        #expect(shaded.r.isNear(Int((255 * 0.18 + 10 * 0.82).rounded()), tolerance: 3), "\(shaded)")
        #expect(shaded.b < shaded.r)
        // Nothing below the live trace.
        #expect(image.pixel(100, Int(geometry.y(forDbm: -135))).r < 30)
    }

    // MARK: Row 9: the split

    @Test func theSpectrumTakesTheDesktopsFortyPercentOrThePansOwnShare() {
        let size = CGSize(width: 400, height: 1018)
        let scaleRow = BandLayout.scaleHeightPoints
        var settings = BandDisplaySettings.desktopDefaults
        #expect(settings.spectrumSharePercent == 40)
        #expect(BandLayout(size: size, scale: 1, settings: settings).spectrum.height == ((1018 - scaleRow) * 0.4).rounded())
        #expect(BandLayout(size: size, scale: 1).spectrum.height == ((1018 - scaleRow) * 0.4).rounded())
        settings.spectrumSharePercent = 70
        #expect(BandLayout(size: size, scale: 1, settings: settings).spectrum.height == 700)
        // Kept within 20 and 80 percent.
        settings.spectrumSharePercent = 5
        #expect(settings.spectrumShare == 0.2)
        settings.spectrumSharePercent = 95
        #expect(settings.spectrumShare == 0.8)
    }

    @Test func theSplitsHandleSitsAtTheLeftEndOfTheFrequencyScaleRow() throws {
        let settings = BandRendererTests.plain()
        let image = try Self.render(settings, frame: nil)
        let layout = BandRendererTests.layout(settings)
        let handle = BandRenderer.splitHandleRect(layout: layout, scale: 1)
        #expect(handle.width == 14 && handle.height == 10)
        #expect(layout.frequencyScale.contains(handle), "inside the row, adding no height")
        // Three bars: the top, middle and foot rows lit, the rows between not.
        func lit(_ y: CGFloat) -> Bool {
            let pixel = image.pixel(Int(handle.midX), Int(y))
            return pixel.r > 110 && pixel.b > 140
        }
        #expect(lit(handle.minY) && lit(handle.midY) && lit(handle.maxY - 1))
        #expect(!lit(handle.minY + 2.5) && !lit(handle.maxY - 3.5))
        // No frequency label over it.
        let yellow = (Int(handle.minX)..<Int(handle.maxX)).contains { x in
            (Int(layout.frequencyScale.minY)..<Int(layout.frequencyScale.maxY)).contains { y in
                let pixel = image.pixel(x, y)
                return pixel.r > 150 && pixel.g > 150 && pixel.b < 110
            }
        }
        #expect(!yellow)
    }

    // MARK: Row 10: the dBm scale's arrows

    @Test func theDbmLabelsStartUnderTheArrowButtons() throws {
        var settings = BandRendererTests.plain()
        settings.scaleTopDbm = -40
        settings.scaleBottomDbm = -140
        let image = try Self.render(settings, frame: nil)
        let layout = BandRendererTests.layout(settings)
        #expect(layout.dbmArrows.height == 2 * BandLayout.dbmArrowHeightPoints)
        #expect(layout.dbmArrows.minX == layout.dbmScale.minX && layout.dbmArrows.minY == 0)
        let under = (Int(layout.dbmArrows.minX)..<Self.width).flatMap { x in (0..<Int(layout.dbmArrows.maxY)).map { (x, $0) } }
        #expect(!under.contains { image.pixel($0.0, $0.1).r > 90 }, "no label under the arrows")
        let below = (Int(layout.dbmArrows.minX)..<Self.width).flatMap { x in
            (Int(layout.dbmArrows.maxY)..<Int(layout.dbmScale.maxY)).map { (x, $0) }
        }
        #expect(below.contains { image.pixel($0.0, $0.1).r > 90 }, "labels below them")
    }

    // MARK: Row 11: each band's own range

    @Test func eachBandKeepsItsOwnTopAndBottom() {
        var settings = BandDisplaySettings.desktopDefaults
        settings.scaleTopDbm = -50
        settings.scaleBottomDbm = -120
        // The first band named keeps the scale as it was.
        settings.enterBand("5")
        #expect(settings.scaleTopDbm == -50 && settings.scaleBottomDbm == -120)
        settings.scaleTopDbm = -45
        #expect(settings.bandScales["5"] == .init(topDbm: -45, bottomDbm: -120))
        // A band that kept none starts at the defaults, and keeps its own.
        settings.enterBand("1")
        #expect(settings.scaleTopDbm == -40 && settings.scaleBottomDbm == -140)
        settings.scaleBottomDbm = -150
        // Back on the first band, its own comes back; and the other's stays kept.
        settings.enterBand("5")
        #expect(settings.scaleTopDbm == -45 && settings.scaleBottomDbm == -120)
        #expect(settings.bandScales["1"] == .init(topDbm: -40, bottomDbm: -150))
        // An unnamed band changes nothing.
        settings.enterBand(nil)
        #expect(settings.scaleBand == "5")
    }

    @Test func eachBandsRangeIsKeptOnThePhone() throws {
        let suite = "band-scales-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let store = BandDisplaySettingsStore(defaults: defaults)
        var settings = BandDisplaySettings.desktopDefaults
        settings.enterBand("3")
        settings.scaleTopDbm = -30
        settings.enterBand("8")
        store.setSettings(settings, forPan: "1")
        var read = store.settings(forPan: "1")
        #expect(read == settings)
        read.enterBand("3")
        #expect(read.scaleTopDbm == -30)
    }

    // MARK: Row 17: calibration

    @Test func calibrationIsNeverAskedOfTheCore() throws {
        var settings = BandDisplaySettings.desktopDefaults
        settings.calibrationOffsetDb = -3
        let request = try #require(settings.extrasRequest(gates: BandDisplaySettingsTests.open))
        #expect(request.calibrationOffsetDb == nil)
        #expect(BandDisplaySettings.calibrationReason == "The Core calibrates the display for its radio.")
    }
}
