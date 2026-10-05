// NereusSDR for iOS: pictures of the 3D view for comparing with JJ's board: the band, the sheet, Setup, sideways and iPad
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// JJ's 3D View board (2026-09-29), drawn by the real app: connected to a
/// fake Core raised to description 12 with its Setup descriptions, two
/// slices where the board has them, and synthetic frames with the spectrum
/// beside the pan (D4: no radio's data). With `NEREUS_V3D_SHOTS` set to a
/// directory (through `TEST_RUNNER_NEREUS_V3D_SHOTS`), each picture is
/// written there; the Setup pages go where `NEREUS_MAIN_SHOTS` points.
@Suite("3D view pictures", .serialized)
@MainActor
struct ThreeDViewShotTests {
    static let upright = CGSize(width: 402, height: 874)
    static let sideways = CGSize(width: 874, height: 402)
    static let iPad = CGSize(width: 1210, height: 834)

    @Test("the band and the Display sheet in 3D, upright, sideways, iPad, large type, and each of the board's states")
    func bandShots() async throws {
        let rig = try await SetupDescribedPagesTests.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        let main = app.main
        try await MainScreenShotTests.fill(rig.station)
        SetupDescribedPagesTests.asV12Core(app)
        #expect(await ShotWait.until { main.slices.entries.count == 2 && main.band.stackOffered })
        let band = main.band
        band.endpointId = 1
        band.gates = MediaFeatureGates(agreedMinor: 11) { name in
            ["remoteMediaVersion": 1, "spectrumGrantVersion": 1, "displayExtrasVersion": 4][name] ?? 0
        }
        band.receive(.context(try #require(Self.context())))
        // The pan on 40 m, as the board's (the Core's band 3).
        main.changeDisplay { $0.enterBand("3") }

        // 2D first: the View row with 2D chosen.
        try await shoot("view3d-portrait-display-2d", size: Self.upright, sheet: .display, app: app)
        main.display.selectView(.stacked)
        #expect(band.drawsStack)
        try await shoot("view3d-portrait-display-3d", size: Self.upright, sheet: .display, app: app)
        try await shoot("view3d-portrait-band", size: Self.upright, app: app)
        try await shoot("view3d-landscape-band", size: Self.sideways, app: app)
        try await shoot("view3d-landscape-display", size: Self.sideways, sheet: .display, app: app)
        try await shoot("view3d-ipad", size: Self.iPad, app: app, iPad: true)
        try await shoot("view3d-portrait-display-large-type", size: Self.upright, sheet: .display, app: app,
                        dynamicType: .accessibility3)

        main.display.setSliceShadow(true)
        try await shoot("view3d-portrait-slice-shadow", size: Self.upright, app: app)
        main.display.setSliceShadow(false)

        // Stop on TX while this phone transmits: the stack and waterfall hold.
        main.changeDisplay { $0.waterfallStopOnTx = true }
        band.keyed = true
        band.tryStackAgain()
        #expect(band.stackPausedWhileTransmitting)
        try await shoot("view3d-portrait-transmitting-stop-on-tx", size: Self.upright, app: app, feeds: false)
        band.keyed = false
        main.changeDisplay { $0.waterfallStopOnTx = false }

        // A phone that cannot keep up: slow frames until the pan shows 2D.
        var now = 5_000.0
        band.clock = { now }
        band.tryStackAgain()
        while band.stackStage != .fellBack && now < 5_030 {
            now += 0.16
            band.noteStackFrame(drawSeconds: 0.09)
        }
        #expect(band.stackNotice == .cannotKeepUp)
        band.clock = { Date().timeIntervalSince1970 }
        try await shoot("view3d-portrait-cannot-keep-up", size: Self.upright, app: app)
        band.tryStackAgain()

        // Low Power Mode: 2D with the reason.
        band.stackConditions = .init(lowPower: true)
        try await shoot("view3d-portrait-low-power", size: Self.upright, app: app)
        band.stackConditions = .init()

        // Saving data: Span asked as 0, and the row says so.
        band.stackConditions = .init(savesData: true)
        try await shoot("view3d-portrait-display-saves-data", size: Self.upright, sheet: .display, app: app)
        band.stackConditions = .init()

        // An older Core: the View row greyed with the reason.
        Self.lower(app)
        #expect(await ShotWait.until { !band.stackOffered })
        try await shoot("view3d-portrait-older-core", size: Self.upright, sheet: .display, app: app)
        await app.disconnect()
    }

    @Test("Setup's Display list and 3D View page, sideways and in large type, and an older Core's greyed entry")
    func setupShots() async throws {
        let rig = try await SetupDescribedPagesTests.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        SetupDescribedPagesTests.asV12Core(app)
        await rig.station.deliverSetup(try SetupDescribedPagesTests.coreCategories(peer: 12))
        #expect(await ShotWait.until { app.setupPages.isCurrent && app.main.band.stackOffered
            && app.setupPages.page("display.threeD", in: "display") != nil })
        let page: [SetupTree.Route] = [.category("Display"), .described(category: "display", page: "display.threeD")]
        try await SetupDescribedPagesTests.shoot("view3d-setup-display-list", rig: rig, path: [.category("Display")])
        try await SetupDescribedPagesTests.shoot("view3d-setup-page", rig: rig, path: page, height: 1_400)
        try await SetupDescribedPagesTests.shoot("view3d-setup-landscape", rig: rig, path: page, height: 1_400,
                                                 width: 874, sideways: true)
        try await SetupDescribedPagesTests.shoot("view3d-setup-large-type", rig: rig, path: page, height: 2_600,
                                                 dynamicType: .accessibility3, parts: 2)
        // The Core's rows reach this phone's settings.
        let controls = app.setupControls
        let gain = try #require(app.setupPages.page("display.threeD", in: "display")?.sections
            .flatMap(\.controls).first { $0.id == "display.threeD.gain" })
        #expect(await controls.edit(gain, in: "display", to: .integer(85)) == .applied)
        #expect(app.main.band.settings.threeDGain == 85)
        // An older Core: 3D View listed, greyed, with the reason.
        Self.lower(app)
        await rig.station.deliverSetup(try SetupDescribedPagesTests.coreCategories())
        #expect(await ShotWait.until { !app.main.band.stackOffered && app.setupPages.isCurrent
            && app.setupFeed.description(for: "display")?.version == 11 })
        try await SetupDescribedPagesTests.shoot("view3d-setup-older-core", rig: rig, path: [.category("Display")])
        await app.disconnect()
    }

    // MARK: Inside

    /// The Core lowered to description 11.
    static func lower(_ app: AppModel) {
        var entries = app.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(ordinal: 0, name: name, value: value.wireValue)
        }
        entries.removeAll { $0.name == "setupDescriptionVersion" }
        entries.append(.init(ordinal: 0, name: "setupDescriptionVersion", value: .i64(11)))
        app.mirror.apply(.capabilities(LinkMessage.Capabilities(properties: entries)))
    }

    static let centre = BandFlagShotTests.centre
    static let span = BandFlagShotTests.span
    /// The spectrum the Core sends beside the pan, the 3D factor wide.
    static let wideSpan = span * StackedTrace.widestRowSpan
    static let wideSamples = 768

    /// The pan's context with the wide row.
    static func context() -> MediaControlEvent.DisplayContext? {
        let payload: [String: LinkJSON] = [
            "op": .string("context"), "connectionId": .string("shots"), "endpointId": .number(1),
            "revision": .number(1), "contextGeneration": .number(1), "sourceStream": .number(0),
            "sourceCentreHz": .number(centre), "sampleRateHz": .number(192_000), "centreHz": .number(centre),
            "spanHz": .number(span), "wideCentreHz": .number(centre), "wideSpanHz": .number(wideSpan),
            "traceSamples": .number(1_206), "waterfallSamples": .number(1_206),
            "wideSamples": .number(Double(wideSamples)),
            "minDbm": .number(-160), "maxDbm": .number(0), "fps": .number(30), "framesPerLine": .number(1),
        ]
        return MediaControlDecoder.context(payload, wideband: false, grant: false)
    }

    /// The board's example signals (board-src part-script-view3d.js,
    /// `exampleRing` and `V3D_OUT`), in the pan's units: two voices where
    /// slices A and B are, a third voice and a carrier, and four stations
    /// beside the pan in the wide row, over noise spread about -123 dBm as
    /// a receiver's is. The Core's noise floor last, the board's -123.
    static let boardFloorDbm: Float = -123
    static let boardSignals: [(low: Double, high: Double, level: Float)] = [
        (0.27, 0.33, 38), (0.54, 0.6, 27), (0.8, 0.86, 44), (0.455, 0.458, 30),
    ]
    static let boardOutside: [(unit: Double, width: Double, level: Float)] = [
        (-0.62, 0.055, 24), (-0.2, 0.012, 30), (1.3, 0.06, 20), (1.72, 0.02, 34),
    ]

    static func feed(_ band: BandModel, width: Int, lines: Int) {
        var seed: UInt64 = 7
        func random() -> Float {
            seed = seed &* 6_364_136_223_846_793_005 &+ 1_442_695_040_888_963_407
            return Float(seed >> 40) / Float(1 << 24)
        }
        func noise() -> Float {
            boardFloorDbm + 10 * log10(-log(random() + 1e-9) + 1e-3)
        }
        /// A level at `unit` across the pan (outside 0 to 1 is beside it).
        func level(_ unit: Double, _ time: Double) -> Float {
            var dbm = noise()
            for (index, signal) in boardSignals.enumerated() where unit >= signal.low && unit <= signal.high {
                let k = Double(index)
                let envelope = Float(0.55 + 0.45 * sin(time * (2.1 + k) + k * 1.3) * sin(time * 0.7 + k))
                let slope = signal.high - signal.low > 0.01
                    ? Float(-14 * (unit - signal.low) / (signal.high - signal.low)) : 0
                dbm = max(dbm, boardFloorDbm + signal.level * envelope + slope + (random() - 0.5) * 7)
            }
            for (index, station) in boardOutside.enumerated() where abs(unit - station.unit) < station.width / 2 {
                let edge = Float(1 - pow(abs(unit - station.unit) / (station.width / 2), 4))
                let wobble = Float(3 * sin(time * 2.3 + Double(index) * 1.7))
                dbm = max(dbm, boardFloorDbm + station.level * edge + wobble + (random() - 0.5) * 6)
            }
            return dbm
        }
        let wideLow = 0.5 - StackedTrace.widestRowSpan / 2
        for sequence in 1...lines {
            let time = Double(sequence) * 0.055
            let trace = (0..<width).map { level((Double($0) + 0.5) / Double(width), time) }
            let wide = (0..<wideSamples).map {
                level(wideLow + (Double($0) + 0.5) / Double(wideSamples) * StackedTrace.widestRowSpan, time)
            }
            let frame = DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: UInt32(sequence),
                                     producerTimestamp: UInt64(sequence) * 33_000_000, isKeyframe: sequence == 1,
                                     waterfallAdvance: true, minDbm: -160, maxDbm: 0, traceDbm: trace,
                                     waterfallDbm: trace, wideDbm: wide)
            band.receive(.displayFrame(frame))
            band.receive(.displayExtras(DisplayExtras(endpointId: 1, contextGeneration: 1,
                                                      encoderSequence: UInt32(sequence), peakBlobs: nil,
                                                      peakHoldDbm: nil, noiseFloorDbm: boardFloorDbm,
                                                      waterfallLevels: .init(lowDbm: -126, highDbm: -70))))
        }
    }

    private func shoot(_ name: String, size: CGSize, sheet: OpenSheet? = nil, app: AppModel, iPad: Bool = false,
                       dynamicType: DynamicTypeSize = .large, feeds: Bool = true) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        let sideways = size.width > size.height
        window.frame = CGRect(origin: CGPoint(x: 0, y: sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = ThreeDShotRoot(model: app, sheet: sheet, iPad: iPad)
            .environment(\.horizontalSizeClass, iPad ? .regular : .compact)
            .environment(\.dynamicTypeSize, dynamicType)
            .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        if sideways && !iPad {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        } else if iPad {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let band = app.main.band
        let drawn = try await ShotWait.requireBandLaidOut(band, in: window)
        // A simulator under load draws slowly and the pacer may have shown
        // 2D (as it should on a phone); each picture starts it afresh unless
        // the picture is of that notice.
        if band.stackNotice != .cannotKeepUp {
            band.tryStackAgain()
        }
        if feeds {
            Self.feed(band, width: drawn.requestedPixels, lines: Int(size.height * 3))
        } else {
            band.receive(.displayFrame(DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: 9_999,
                                                    producerTimestamp: 1, isKeyframe: false, waterfallAdvance: true,
                                                    minDbm: -160, maxDbm: 0, traceDbm: [], waterfallDbm: [],
                                                    wideDbm: [])))
        }
        try await ShotWait.requireBandShown(band, in: window, after: drawn)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_V3D_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The app's root as `RootView` lays it out, with one sheet open or none.
private struct ThreeDShotRoot: View {
    @ObservedObject var model: AppModel
    let sheet: OpenSheet?
    let iPad: Bool

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, sheet: sheet)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height, iPad: iPad)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
