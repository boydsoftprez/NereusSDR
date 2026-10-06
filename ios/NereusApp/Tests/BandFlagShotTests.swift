// NereusSDR for iOS: the band with one, two and three slices on screen, upright and sideways
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-11, D9, D10: the real band view with its flags, markers and zoom
/// buttons, hosted in a window on the simulator and fed a mirror and
/// synthetic frames. The Core's catalogue is read from the link's
/// conformance suite at run time and never bundled (D4). With
/// `NEREUS_BAND_SHOTS` set to a directory, each screen is also written
/// there as a PNG file, for comparing with the board's pictures.
@Suite("Band flags on screen", .serialized)
@MainActor
struct BandFlagShotTests {
    struct Shot: Sendable, CustomStringConvertible {
        let slices: Int
        let sideways: Bool
        var description: String { "\(slices)-slice\(slices == 1 ? "" : "s")-\(sideways ? "sideways" : "upright")" }
        /// The band's area on an iPhone 17, between the toolbar and the tab bar.
        var size: CGSize { sideways ? CGSize(width: 874, height: 300) : CGSize(width: 402, height: 640) }
    }

    nonisolated static let shots = [1, 2, 3].flatMap { count in [false, true].map { Shot(slices: count, sideways: $0) } }

    static let centre = 7_244_500.0
    static let span = 48_000.0
    /// Slice A, B and C: A and B where the board draws them, C upper sideband.
    static let slices: [(hz: Double, mode: Int64, low: Int64, high: Int64)] = [
        (7_236_400, 0, -3000, -100), (7_249_000, 0, -3000, -100), (7_259_500, 1, 100, 2_900),
    ]

    @Test(arguments: shots)
    func theBandShowsItsFlagsAndMarkers(_ shot: Shot) async throws {
        let catalog = Self.catalogue()
        let store = MirrorStore(send: { _ in })
        for index in 0..<shot.slices {
            store.apply(Self.slice(index, active: index == 0))
        }
        let slices = BandSlicesModel(store: store, commands: nil, catalog: catalog)
        let band = BandModel()
        band.catalog = catalog
        band.endpointId = 1
        guard let context = Self.context() else {
            Issue.record("the context payload did not decode")
            return
        }
        band.receive(.context(context))
        let settings = PhoneSettings(defaults: UserDefaults(suiteName: "BandFlagShotTests") ?? .standard)

        let window = try Self.window(size: shot.size)
        let root = ZStack {
            BandView(model: band)
            BandGestureLayer(band: band, slices: slices, settings: settings,
                             foreign: ForeignSlicesModel(store: MirrorStore(send: { _ in })))
        }
        .frame(width: shot.size.width, height: shot.size.height)
        .background(Color.black)
        let host = UIHostingController(rootView: root)
        // The band's area alone: no status bar or home indicator to keep clear of.
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: shot.size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        // The first draw sizes the waterfall; the frames then fill it.
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        Self.feed(band, width: bandDraw.requestedPixels, lines: Int(shot.size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)

        // The overlay handed the renderer one marker per slice, the active one last.
        #expect(band.markers.count == shot.slices)
        #expect(band.markers.last?.sliceId == 0)
        #expect(band.markers.last?.style.selected == true)
        if shot.slices == 2 {
            let b = try #require(band.markers.first { $0.sliceId == 1 })
            if shot.sideways {
                // B's line is 229 points from A's, closer than a flag with
                // its round buttons (242), so B folds sideways too, and its
                // triangle hangs from its tag, kept inside the spectrum.
                let geometry = try #require(band.pointGeometry(size: shot.size)).geometry
                let placements = FlagLayout.layout(slices: slices.entries.map(\.slice), activeSliceId: 0,
                                                   geometry: geometry)
                let tag = try #require(placements.last)
                #expect(tag.isFolded)
                #expect(b.triangleTopPoints == min(tag.rect.maxY,
                                                   geometry.size.height - SliceMarkers.triangleSize.height))
            } else {
                // B folds under A's flag: its triangle hangs from the tag.
                #expect(b.triangleTopPoints == FlagLayout.flagSize.height + FlagLayout.stackGap + FlagLayout.foldedHeight)
            }
        }

        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_BAND_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(shot).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    // MARK: Inside

    static func window(size: CGSize) throws -> UIWindow {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        return window
    }

    static func slice(_ index: Int, active: Bool) -> LinkMessage {
        let spec = slices[index]
        return .objectCreate(LinkMessage.ObjectCreate(key: "slice:\(index)", className: "SliceModel", properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(spec.hz)),
            LinkMessage.PropertyEntry(ordinal: 2, name: "dspMode", value: .enumeration(spec.mode)),
            LinkMessage.PropertyEntry(ordinal: 3, name: "filterLow", value: .i64(spec.low)),
            LinkMessage.PropertyEntry(ordinal: 4, name: "filterHigh", value: .i64(spec.high)),
            LinkMessage.PropertyEntry(ordinal: 6, name: "stepHz", value: .i64(100)),
            LinkMessage.PropertyEntry(ordinal: 9, name: "rxAntenna", value: .utf8("ANT1")),
            LinkMessage.PropertyEntry(ordinal: 10, name: "txAntenna", value: .utf8("ANT1")),
            LinkMessage.PropertyEntry(ordinal: 11, name: "active", value: .bool(active)),
            LinkMessage.PropertyEntry(ordinal: 12, name: "txSlice", value: .bool(false)),
            LinkMessage.PropertyEntry(ordinal: 13, name: "sliceIndex", value: .i64(Int64(index))),
            LinkMessage.PropertyEntry(ordinal: 15, name: "signalStrengthDbm", value: .f64(-99 + Double(index) * 17)),
            LinkMessage.PropertyEntry(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
        ]))
    }

    /// A context for endpoint 1, as the Core sends it.
    static func context(centreHz: Double = centre) -> MediaControlEvent.DisplayContext? {
        let payload: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("shots"), "endpointId": .number(1),
            "revision": .number(1), "contextGeneration": .number(1), "sourceStream": .number(0),
            "sourceCentreHz": .number(centreHz), "sampleRateHz": .number(192_000), "centreHz": .number(centreHz),
            "spanHz": .number(span), "wideCentreHz": .number(0), "wideSpanHz": .number(0),
            "traceSamples": .number(1_206), "waterfallSamples": .number(1_206), "wideSamples": .number(0),
            "minDbm": .number(-160), "maxDbm": .number(0), "fps": .number(30), "framesPerLine": .number(1),
        ]
        return MediaControlDecoder.context(payload, wideband: false, grant: false)
    }

    /// Band noise and the three slices' signals, drifting a little.
    static func feed(_ band: BandModel, width: Int, lines: Int) {
        var seed: UInt64 = 7
        func random() -> Float {
            seed = seed &* 6_364_136_223_846_793_005 &+ 1_442_695_040_888_963_407
            return Float(seed >> 40) / Float(1 << 24)
        }
        let low = centre - span / 2
        let stations: [(hz: Double, dbm: Float, width: Double)] = [(7_234_900, -72, 2_600), (7_247_500, -84, 2_600),
                                                                   (7_261_000, -96, 2_400)]
        for sequence in 1...lines {
            let trace = (0..<width).map { index -> Float in
                let hz = low + (Double(index) + 0.5) / Double(width) * span
                var level = -128 + random() * 12
                for station in stations where abs(hz - station.hz) < station.width / 2 && sequence % 40 < 32 {
                    level = max(level, station.dbm - random() * 8)
                }
                return level
            }
            band.receive(.displayFrame(DisplayFrame(endpointId: 1, contextGeneration: 1,
                                                    encoderSequence: UInt32(sequence),
                                                    producerTimestamp: UInt64(sequence) * 33_000_000,
                                                    isKeyframe: sequence == 1, waterfallAdvance: true, minDbm: -160,
                                                    maxDbm: 0, traceDbm: trace, waterfallDbm: trace, wideDbm: [])))
        }
    }

    /// The Core's catalogue from the conformance suite, or nil when the
    /// checkout is not reachable (the band then draws without it).
    static func catalogue() -> StationCatalog? {
        let file = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json")
        guard let data = try? Data(contentsOf: file),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return nil
        }
        for step in object["steps"] as? [[String: Any]] ?? [] {
            guard let message = step["message"] as? [String: Any], message["key"] as? String == "catalog",
                  let properties = message["properties"] as? [[String: Any]],
                  let json = properties.first(where: { $0["name"] as? String == "json" })?["value"] as? String else {
                continue
            }
            return StationCatalog.parse(json: json)
        }
        return nil
    }
}
