// NereusSDR for iOS: the band drawn with the Core's own catalogue, read from the conformance suite at run time
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import ImageIO
import LinkTestSupport
import NereusMedia
import NereusModels
import Testing
import UniformTypeIdentifiers
@testable import NereusBand

/// R-IOS-11, spec section 5.1 item 4: with the Core's catalogue the strip
/// shows the default plan (ARRL today) and the waterfall its default
/// palette. The catalogue is read from `tests/data/link/v1` at run time
/// (D4); nothing of it is written here. With `NEREUS_BAND_SHOTS` set to a
/// directory, the renders are also written there as PNG files, for
/// comparing with the board's pictures.
@MainActor
@Suite(.serialized) struct BandCatalogueRenderTests {
    struct Shot: Sendable, CustomStringConvertible {
        let name: String
        let width: Int
        let height: Int
        var size: BandPlanSize = .small
        var description: String { name }
    }

    /// An iPhone's band upright and turned sideways, in pixels at 3x, with
    /// the strip at Small (the default) and at Huge.
    nonisolated static let shots = [Shot(name: "band-upright", width: 1206, height: 1740),
                                    Shot(name: "band-sideways", width: 2622, height: 780),
                                    Shot(name: "band-upright-huge", width: 1206, height: 1740, size: .huge)]

    static func catalogue() throws -> StationCatalog {
        guard let entry = try LinkFixtureLoader.manifest().first(where: { $0.id == "session-catalog-anan-g2" }) else {
            throw LinkFixtureLoader.Malformed(description: "no catalogue fixture")
        }
        let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(entry.file))
        for step in object["steps"] as? [[String: Any]] ?? [] {
            guard let message = step["message"] as? [String: Any], message["key"] as? String == "catalog",
                  let properties = message["properties"] as? [[String: Any]],
                  let json = properties.first(where: { $0["name"] as? String == "json" })?["value"] as? String,
                  let catalog = StationCatalog.parse(json: json) else {
                continue
            }
            return catalog
        }
        throw LinkFixtureLoader.Malformed(description: "the fixture creates no catalogue")
    }

    @Test(arguments: shots)
    func theCoresCatalogueDrawsTheDefaultPlanAndPalette(_ shot: Shot) async throws {
        let diagnostic = HostedDiagnosticReceipts(shot.name)
        diagnostic.mark("body entry")
        defer { diagnostic.mark("body exit"); diagnostic.export() }
        diagnostic.mark("catalogue/layout setup entry")
        let catalog = try Self.catalogue()
        var settings = BandDisplaySettings.desktopDefaults
        settings.bandPlanSize = shot.size
        let centre = 7_244_500.0
        let span = 48_000.0
        let scale: CGFloat = 3
        let overlays = BandOverlays(centerHz: centre, spanHz: span, scale: scale, settings: settings, catalog: catalog)
        #expect(overlays.bandPlan == catalog.defaultBandPlan)
        #expect(overlays.palette?.id == settings.waterfallPaletteId)
        let layout = BandLayout(size: CGSize(width: shot.width, height: shot.height), scale: scale, settings: settings)
        let geometry = layout.spectrumGeometry(centerHz: centre, spanHz: span, dbmRange: settings.scaleRange)
        let strip = BandPlanStrip(plan: overlays.bandPlan, geometry: geometry, rightX: Double(layout.strip.maxX))
        #expect(!strip.pieces.isEmpty, "the default plan covers 40 metres")
        // Each segment is named with its lowest class where there is room,
        // as the desktop's strip does, and by its label alone where not.
        let classed = try #require(strip.pieces.first { !$0.lowestClass.isEmpty && $0.width > 60 * Double(scale) })
        let full = "\(classed.label) \(classed.lowestClass)"
        let widths: (String) -> CGFloat = {
            BandLabels.width(of: $0, points: shot.size.labelPoints, scale: scale, bold: true)
        }
        #expect(classed.text(scale: scale) { widths($0) <= widths(full) } == full)
        #expect(classed.text(scale: scale) { widths($0) < widths(full) } == classed.label)
        #expect(classed.text(scale: scale) { _ in false } == nil)

        diagnostic.mark("offscreen construction entry")
        let target = try Offscreen(width: shot.width, height: shot.height)
        diagnostic.mark("offscreen construction returned")
        diagnostic.mark("renderer construction entry")
        let renderer = try BandRenderer(device: target.device)
        diagnostic.mark("renderer construction returned")
        diagnostic.mark("catalogue/layout setup returned")
        var state = BandState(waterfallLines: layout.waterfallLines, expectsExtras: true)
        let samples = shot.width
        var generator = SeededGenerator(seed: 7)
        // Two stations and some band noise, drifting a little.
        let stations: [(hz: Double, dbm: Float, widthHz: Double)] = [(7_236_400, -72, 2_400), (7_249_000, -84, 2_600),
                                                                      (7_259_500, -98, 400)]
        var frame: DisplayFrame?
        diagnostic.mark("history entry; rows \(layout.waterfallLines)")
        for sequence in 1...layout.waterfallLines {
            let trace = (0..<samples).map { index -> Float in
                let hz = geometry.hz(forTraceSample: index, traceSamples: samples)
                var level = Float.random(in: -128 ... -116, using: &generator)
                for station in stations where abs(hz - station.hz) < station.widthHz / 2
                    && sequence % 40 < 32 {
                    level = max(level, station.dbm + Float.random(in: -8...0, using: &generator))
                }
                return level
            }
            let next = BandFixtures.frame(trace: trace, sequence: UInt32(sequence))
            state.receive(frame: next, manualLevels: settings.manualLevels)
            state.receive(extras: BandFixtures.extras(for: next, levels: (-126, -66)), manualLevels: settings.manualLevels)
            frame = next
            await Task.yield()
        }
        diagnostic.mark("history returned")
        let image = try target.render(renderer, frame: frame, history: state.history, extras: state.frameExtras,
                                      overlays: overlays, diagnostic: diagnostic)
        // The default plan's colour is on the strip, dimmed by its licence class.
        let piece = try #require(strip.pieces.first)
        let expected = try #require(BandPlanStrip.fill(colour: piece.colour, licence: piece.licence))
        let drawn = image.pixel(Int(piece.lowX) + 2 * Int(scale), Int(layout.strip.minY) + 1)
        #expect(drawn.r.isNear(expected.red, tolerance: 1) && drawn.g.isNear(expected.green, tolerance: 1)
                    && drawn.b.isNear(expected.blue, tolerance: 1), "\(drawn) against \(expected)")

        if let directory = ProcessInfo.processInfo.environment["NEREUS_BAND_SHOTS"], !directory.isEmpty {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(shot.name).png")
            try image.writePNG(to: url)
            print("Wrote \(url.path) from \(target.device.name)")
        }
    }
}

extension BandCatalogueRenderTests {
    /// Task 54d Step 3: the Core's own catalogue marks its plan `active` and
    /// carries each plan's spots, and the strip draws them as white dots
    /// mid-strip, over the whole 40 metre band, even with a setting that
    /// names another plan (the Core's mark wins).
    @Test func theCoresActivePlanDrawsItsSpotsAsDots() throws {
        let catalog = try Self.catalogue()
        let active = catalog.bandPlans.filter(\.isActive)
        #expect(active.count == 1)
        let plan = try #require(active.first)
        #expect(!plan.spots.isEmpty)
        let other = try #require(catalog.bandPlans.first { !$0.isActive })
        var settings = BandDisplaySettings.desktopDefaults
        settings.bandPlanSize = .small
        let centre = 7_150_000.0
        let span = 300_000.0
        let scale: CGFloat = 3
        let width = 1206
        let height = 1740
        let overlays = BandOverlays(centerHz: centre, spanHz: span, scale: scale, settings: settings, catalog: catalog,
                                    stationPlanName: other.name)
        #expect(overlays.bandPlan == plan)
        let layout = BandLayout(size: CGSize(width: width, height: height), scale: scale, settings: settings)
        let geometry = layout.spectrumGeometry(centerHz: centre, spanHz: span, dbmRange: settings.scaleRange)
        let strip = BandPlanStrip(plan: overlays.bandPlan, geometry: geometry, rightX: Double(layout.strip.maxX))
        let inSpan = plan.spots.filter { $0.hz >= geometry.lowHz && $0.hz <= geometry.highHz }
        #expect(!inSpan.isEmpty, "the Core's plan has spots on 40 metres")
        #expect(strip.spotXs.count == inSpan.filter { geometry.x(forHz: $0.hz) <= Double(layout.strip.maxX) }.count)

        let target = try Offscreen(width: width, height: height)
        let renderer = try BandRenderer(device: target.device)
        let image = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                      overlays: overlays)
        let y = Int(layout.strip.midY)
        for x in strip.spotXs {
            #expect(image.pixel(Int(x), y).isWhitish, "a dot at \(x)")
        }
        // Off draws neither the strip nor its dots.
        settings.bandPlanSize = .off
        let off = BandOverlays(centerHz: centre, spanHz: span, scale: scale, settings: settings, catalog: catalog)
        let bare = try target.render(renderer, frame: nil, history: WaterfallHistory(capacity: 1), extras: nil,
                                     overlays: off)
        for x in strip.spotXs {
            #expect(!bare.pixel(Int(x), y).isWhitish, "no dot at \(x) with the strip off")
        }
    }
}

/// A repeatable random source, so the renders are the same every run.
struct SeededGenerator: RandomNumberGenerator {
    private var state: UInt64

    init(seed: UInt64) {
        state = seed
    }

    mutating func next() -> UInt64 {
        state = state &* 6_364_136_223_846_793_005 &+ 1_442_695_040_888_963_407
        return state
    }
}

extension Image {
    func writePNG(to url: URL) throws {
        var rgba = [UInt8](repeating: 255, count: width * height * 4)
        for index in 0..<(width * height) {
            rgba[index * 4] = bgra[index * 4 + 2]
            rgba[index * 4 + 1] = bgra[index * 4 + 1]
            rgba[index * 4 + 2] = bgra[index * 4]
        }
        let space = CGColorSpace(name: CGColorSpace.sRGB) ?? CGColorSpaceCreateDeviceRGB()
        guard let provider = CGDataProvider(data: Data(rgba) as CFData),
              let image = CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32,
                                  bytesPerRow: width * 4, space: space,
                                  bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue),
                                  provider: provider, decode: nil, shouldInterpolate: false, intent: .defaultIntent),
              let destination = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil)
        else {
            throw LinkFixtureLoader.Malformed(description: "cannot write \(url.path)")
        }
        CGImageDestinationAddImage(destination, image, nil)
        guard CGImageDestinationFinalize(destination) else {
            throw LinkFixtureLoader.Malformed(description: "cannot write \(url.path)")
        }
    }
}
