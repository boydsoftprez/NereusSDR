// NereusSDR for iOS: tests that the band takes the desktop's colours, sizes, dashes and number formats (D83)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusMedia
import Testing
@testable import NereusBand

/// D83: the phone draws the band with the desktop's exact colours and
/// sizes. Each test pins a value the desktop draws with and checks the
/// band as drawn. Every frame here is synthetic (D4).
@MainActor
@Suite(.serialized) struct DesktopValuesTests {
    static let width = BandRendererTests.width

    static func render(_ settings: BandDisplaySettings, frame: DisplayFrame? = nil, extras: DisplayExtras? = nil,
                       adjust: (inout BandOverlays) -> Void = { _ in }) throws -> Image {
        let target = try Offscreen(width: BandRendererTests.width, height: BandRendererTests.height)
        let renderer = try BandRenderer(device: target.device)
        var overlays = BandRendererTests.overlays(settings)
        adjust(&overlays)
        return try target.render(renderer, frame: frame, history: WaterfallHistory(capacity: 1), extras: extras,
                                 overlays: overlays)
    }

    /// The lengths of the lit and dark runs along `row`, from its first lit run.
    static func runs(_ row: [Bool]) -> (lit: [Int], dark: [Int]) {
        var lit: [Int] = []
        var dark: [Int] = []
        var index = row.firstIndex(of: true) ?? row.count
        while index < row.count {
            let value = row[index]
            var length = 0
            while index < row.count, row[index] == value {
                length += 1
                index += 1
            }
            if index < row.count {
                // A run cut off by the row's end is not whole.
                if value { lit.append(length) } else { dark.append(length) }
            }
        }
        return (lit, dark)
    }

    // MARK: The trace and its fill

    @Test func theTraceIsTheDesktopsCyanWithItsFillAlpha70() throws {
        let settings = BandDisplaySettings.desktopDefaults
        #expect(settings.traceColour == "#00E5FF")
        #expect(settings.traceFillOpacity == 0.70)
        var plain = BandRendererTests.plain()
        plain.traceFill = true
        let image = try Self.render(plain, frame: BandFixtures.frame(trace: BandRendererTests.flat(200, -50)))
        // Flat at 0.7 x 0.4 = 0.28 of cyan over the band's #0A0A14.
        let under = image.pixel(100, Int(BandRendererTests.geometry(plain).y(forDbm: -100)))
        #expect(under.r.isNear(Int((10 * 0.72).rounded()), tolerance: 3), "\(under)")
        #expect(under.g.isNear(Int((229 * 0.28 + 10 * 0.72).rounded()), tolerance: 3), "\(under)")
        #expect(under.b.isNear(Int((255 * 0.28 + 20 * 0.72).rounded()), tolerance: 3), "\(under)")
    }

    // MARK: The grid and scales

    @Test func theGridAndScalesTakeTheDesktopsColours() throws {
        let settings = BandDisplaySettings.desktopDefaults
        #expect(settings.gridTextColour == "#FFFF00FF")
        #expect(settings.rxZeroLineColour == "#FF0000FF")
        #expect(settings.txZeroLineColour == "#FFB800FF")
        var plain = BandRendererTests.plain()
        plain.grid = true
        let image = try Self.render(plain)
        let layout = BandRendererTests.layout(plain)
        // The frequency-scale row's background, #101520, clear of its labels and handle.
        let background = image.pixel(Int(layout.frequencyScale.minX) + 2, Int(layout.frequencyScale.maxY) - 1)
        #expect(background.r.isNear(16, tolerance: 1) && background.g.isNear(21, tolerance: 1)
                    && background.b.isNear(32, tolerance: 1), "\(background)")
        // Its labels in yellow: the brightest pixel is #FFFF00.
        let scaleRow = (Int(layout.frequencyScale.minX)..<Int(layout.frequencyScale.maxX)).flatMap { x in
            (Int(layout.frequencyScale.minY)..<Int(layout.frequencyScale.maxY)).map { image.pixel(x, $0) }
        }
        let brightest = try #require(scaleRow.max { $0.r + $0.g < $1.r + $1.g })
        #expect(brightest.r > 240 && brightest.g > 240 && brightest.b < 50, "\(brightest)")
        // The dBm labels in #80A0B0: the brightest pixel in their column.
        let dbm = layout.dbmScale
        let column = (Int(dbm.minX)..<Int(dbm.maxX)).flatMap { x in
            (Int(layout.dbmArrows.maxY)..<Int(dbm.maxY)).map { image.pixel(x, $0) }
        }
        let label = try #require(column.max { $0.g < $1.g })
        #expect(label.r.isNear(128, tolerance: 6) && label.g.isNear(160, tolerance: 6)
                    && label.b.isNear(176, tolerance: 6), "\(label)")
    }

    @Test func theFineGridIsDottedOneOnTwoOff() throws {
        var settings = BandRendererTests.plain()
        settings.grid = true
        settings.gridColour = "#00000000"
        settings.hGridColour = "#00000000"
        settings.gridFineColour = "#FFFFFFFF"
        let image = try Self.render(settings)
        let geometry = BandRendererTests.geometry(settings)
        let ticks = geometry.frequencyTicks(minimumSpacing: Double(BandRenderer.frequencyLabelSpacingPoints))
        let hz = ticks.ticks[0] + ticks.stepHz / 5
        let x = Int(geometry.x(forHz: hz).rounded(.down))
        let column = (0..<Int(BandRendererTests.layout(settings).strip.minY)).map { image.pixel(x, $0).r > 200 }
        let found = Self.runs(column)
        #expect(!found.lit.isEmpty && found.lit.allSatisfy { $0 == 1 }, "\(found.lit)")
        #expect(!found.dark.isEmpty && found.dark.allSatisfy { $0 == 2 }, "\(found.dark)")
    }

    @Test func theZeroLineIsDashedFourOnTwoOff() throws {
        var settings = BandRendererTests.plain()
        settings.scaleTopDbm = 10
        settings.scaleBottomDbm = -90
        settings.showZeroLine = true
        let y = Int(BandRendererTests.geometry(settings).y(forDbm: 0).rounded(.down))
        let image = try Self.render(settings)
        let found = Self.runs((0..<300).map { image.pixel($0, y).r > 200 && image.pixel($0, y).g < 60 })
        #expect(!found.lit.isEmpty && found.lit.allSatisfy { $0 == 4 }, "\(found.lit)")
        #expect(!found.dark.isEmpty && found.dark.allSatisfy { $0 == 2 }, "\(found.dark)")
    }

    // MARK: Peak hold

    @Test func activePeakHoldIsDashedFourWidthsOnTwoOff() throws {
        var settings = BandRendererTests.plain()
        settings.traceFill = false
        settings.activePeakHold = true
        #expect(settings.peakHoldColour == "#FFD700FF")
        let frame = BandFixtures.frame(trace: BandRendererTests.flat(200, -139))
        let extras = BandFixtures.extras(for: frame, hold: BandRendererTests.flat(200, -90))
        let y = Int(BandRendererTests.geometry(settings).y(forDbm: -90))
        let image = try Self.render(settings, frame: frame, extras: extras)
        let row = (20..<300).map { x in (y - 1...y).contains { image.pixel(x, $0).isHold } }
        let lit = row.filter { $0 }.count
        // At least a point wide: dashes 4 and gaps 2, two thirds lit.
        #expect(abs(Double(lit) / Double(row.count) - 2.0 / 3) < 0.08, "\(lit) of \(row.count)")
    }

    @Test func classicPeakHoldIsDottedOneOnTwoOffAtTheTracesColour() throws {
        var settings = BandRendererTests.plain()
        settings.traceFill = false
        settings.traceColour = "#FF0000"
        let hold = BandRendererTests.flat(200, -80)
        let y = Int(BandRendererTests.geometry(settings).y(forDbm: -80))
        let image = try Self.render(settings) { $0.peakHold = hold }
        let row = (20..<300).map { x in (y - 1...y).contains { image.pixel(x, $0).r > 100 } }
        let lit = row.filter { $0 }.count
        #expect(abs(Double(lit) / Double(row.count) - 1.0 / 3) < 0.08, "\(lit) of \(row.count)")
        // At 0.55 of the trace's colour.
        let brightest = (y - 1...y).flatMap { row in (20..<300).map { image.pixel($0, row).r } }.max() ?? 0
        #expect(brightest.isNear(Int((255 * 0.55 + 10 * 0.45).rounded()), tolerance: 4), "\(brightest)")
    }

    // MARK: The readout

    @Test func thePeakReadoutIsTheDesktopsBlueEvery500Ms() {
        let settings = BandDisplaySettings.desktopDefaults
        #expect(settings.peakValueColour == "#1E90FFFF")
        #expect(settings.peakValueDelayMs == 500)
        #expect(settings.noiseFloorTextColour == "#FFFF00FF")
    }

    // MARK: Settings kept before D83

    @Test func settingsKeptAtThePhonesEarlierDefaultsReadAsTheDesktops() throws {
        let suite = "band-settings-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let store = BandDisplaySettingsStore(defaults: defaults)
        var earlier = try #require(try JSONSerialization.jsonObject(
            with: JSONEncoder().encode(BandDisplaySettings.desktopDefaults)) as? [String: Any])
        earlier.removeValue(forKey: "desktopValuesVersion")
        earlier["traceColour"] = "#22D3EE"
        earlier["traceFillOpacity"] = 0.35
        earlier["gridTextColour"] = "#F5E663FF"
        earlier["rxZeroLineColour"] = "#FF6666FF"
        earlier["peakValueColour"] = "#7FB2FFFF"
        earlier["peakValueDelayMs"] = 400
        earlier["noiseFloorTextColour"] = "#F5E663FF"
        earlier["peakBlobColour"] = "#FF6B3DFF"
        earlier["peakBlobTextColour"] = "#FFFFFFFF"
        earlier["peakHoldColour"] = "#00FF00FF"
        defaults.set(try JSONSerialization.data(withJSONObject: earlier), forKey: BandDisplaySettingsStore.keyPrefix + "pan-1")
        var expected = BandDisplaySettings.desktopDefaults
        // A colour the operator chose is kept.
        expected.peakHoldColour = "#00FF00FF"
        #expect(store.settings(forPan: "pan-1") == expected)

        // Kept once the desktop's values are in: an earlier value chosen again stays.
        var chosen = BandDisplaySettings.desktopDefaults
        chosen.traceFillOpacity = 0.35
        chosen.traceColour = "#22D3EE"
        store.setSettings(chosen, forPan: "pan-2")
        #expect(store.settings(forPan: "pan-2") == chosen)
    }
}
