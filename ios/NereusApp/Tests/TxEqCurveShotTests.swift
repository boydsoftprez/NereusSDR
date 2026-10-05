// NereusSDR for iOS: pictures of the TX Equalizer page's curve in the board's states, for comparing with the board
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Plan Task 59a (R-IOS-18): the TX Equalizer page with its curve drawn as
/// the app draws it, against a fake Core, in each of the board's states
/// (board-src/shots/eqcurve-*.png): saved, default, ten bands, off, straight
/// lines, a change answered, the first point, an old profile's count, a
/// refusal, a change from another device, the number pad, on the air at and
/// below transmitSettingsVersion 13, another device holding transmit, a
/// curve the Core cannot read, an older Core, a Core that cannot take
/// changes, no Core, large type, and sideways. Upright pictures are the whole
/// page, so every card shows in one picture. With `NEREUS_MAIN_SHOTS` set,
/// each picture is written there.
@Suite("TX EQ curve pictures", .serialized)
@MainActor
struct TxEqCurveShotTests {
    private let platform = TestPlatform()
    /// The whole page upright, every card in one picture.
    private static let pageHeight: CGFloat = 2300

    @Test("the curve page draws in each of the board's states")
    func pictures() async throws {
        let harness = try await TxEqCurveHarness.connected(platform: platform)
        let eq = harness.eq
        let fixture = try TxEqCurveHarness.fixture()

        // The Core's flat default, as the fixture's transmit object carries it.
        try await harness.deliverCurve(Self.defaultCurve)
        try await setOnAir(harness, enabled: true, legacy: false)
        #expect(await harness.settle { eq.curve?.state == .default && eq.onAir == .curve })
        try await shoot("eqcurve-portrait-default", eq)

        // The worked example, saved.
        try await harness.deliverCurve(fixture.setCurveAnswer)
        #expect(await harness.settle { eq.curve?.state == .saved })
        eq.choose(2)
        try await shoot("eqcurve-portrait-saved", eq)
        try await shoot("eqcurve-landscape", eq, sideways: true)
        try await shoot("eqcurve-portrait-large-type", eq, large: true)
        try await shoot("eqcurve-landscape-large-type", eq, sideways: true, large: true)

        // Legacy EQ on, then TX EQ off.
        try await setOnAir(harness, enabled: true, legacy: true)
        #expect(await harness.settle { eq.onAir == .tenBands })
        try await shoot("eqcurve-portrait-ten-band", eq)
        try await setOnAir(harness, enabled: false, legacy: false)
        #expect(await harness.settle { eq.onAir == .off })
        try await shoot("eqcurve-portrait-eq-off", eq)
        try await setOnAir(harness, enabled: true, legacy: false)
        #expect(await harness.settle { eq.onAir == .curve })

        // A change the Core answered.
        eq.stepGain(up: true)
        let sent = try #require(await harness.nextSetCurve())
        var kept = try #require(TxEqCurve(json: fixture.setCurveAnswer))
        kept.points[2].gainDb = -1
        await harness.answer(sent.id, curve: kept)
        #expect(await harness.settle { eq.curve?.points[2].gainDb == -1 })
        try await shoot("eqcurve-portrait-after-change", eq)

        // The first point, at the low end.
        eq.choose(0)
        try await shoot("eqcurve-portrait-first-point", eq)

        // The number pad, over point 3's gain.
        eq.choose(2)
        eq.openGainPad()
        let pad = try #require(eq.pad)
        TxEqCurveHarness.type(pad, "2.5")
        try await shoot("eqcurve-portrait-number-pad", eq, height: 874)
        pad.cancel()

        // Straight lines.
        var lines = kept
        lines.parametric = false
        try await harness.deliverCurve(lines.savedJson)
        #expect(await harness.settle { eq.curve?.parametric == false })
        try await shoot("eqcurve-portrait-straight-lines", eq)

        // Another device's change.
        var other = kept
        other.points[3].gainDb = 6
        try await harness.deliverCurve(other.savedJson)
        #expect(await harness.settle { eq.changedElsewhere && eq.curve == other })
        try await shoot("eqcurve-portrait-changed-elsewhere", eq)

        // An old profile's count, then the Core's refusal of a change to it.
        try await harness.deliverCurve(TxEqCurveHarness.sevenPoints)
        #expect(await harness.settle { eq.curve?.points.count == 7 })
        try await shoot("eqcurve-portrait-other-count", eq)
        eq.choose(3)
        eq.stepGain(up: true)
        let refused = try #require(await harness.nextSetCurve())
        await harness.refuse(refused.id, fixture.countRefusal)
        #expect(await harness.settle { eq.curveNote == fixture.countRefusal })
        try await shoot("eqcurve-portrait-refused", eq)

        // On the air: live at 13 and later, greyed below.
        try await harness.deliverCurve(kept.savedJson)
        await harness.setTransmitting(true)
        #expect(await harness.settle { eq.transmitting && eq.curveNote == nil })
        try await shoot("eqcurve-portrait-transmitting", eq)
        SetupDescribedPagesTests.withCapabilities(harness.app, ["transmitSettingsVersion": .i64(12)])
        #expect(await harness.settle { eq.curveReason == TransmitModel.onAirText })
        try await shoot("eqcurve-portrait-transmitting-older-core", eq)
        SetupDescribedPagesTests.withCapabilities(harness.app, ["transmitSettingsVersion": .i64(15)])
        await harness.setTransmitting(false)
        #expect(await harness.settle { !eq.transmitting && eq.curveReason == nil })

        // A curve the Core cannot read.
        try await harness.deliverCurve(#"{"state":"unavailable"}"#)
        #expect(await harness.settle { eq.curveReason == TxEqualizerModel.unavailableReason })
        try await shoot("eqcurve-portrait-unavailable", eq)
        try await harness.deliverCurve(kept.savedJson)

        // A Core that cannot take changes, then an older Core.
        SetupDescribedPagesTests.withCapabilities(harness.app, ["txEqCurveVersion": .i64(1)])
        #expect(await harness.settle { eq.curveReason == TxEqCurve.readOnlyReason })
        try await shoot("eqcurve-portrait-cannot-take-changes", eq)
        SetupDescribedPagesTests.withCapabilities(harness.app, ["txEqCurveVersion": .i64(0)])
        #expect(await harness.settle { eq.curveReason == TxEqualizerModel.olderCoreReason })
        try await shoot("eqcurve-portrait-older-core", eq)
        SetupDescribedPagesTests.withCapabilities(harness.app, ["txEqCurveVersion": .i64(2)])
        #expect(await harness.settle { eq.curveReason == nil })

        // No Core.
        await harness.app.disconnect()
        #expect(await harness.settle { eq.curveReason == TxEqualizerModel.notConnectedReason })
        try await shoot("eqcurve-portrait-no-core", eq)

        // Another device holding transmit.
        let held = try await TxEqCurveHarness.connected(platform: platform, additions: [.remoteTx])
        try await held.deliverCurve(kept.savedJson)
        try await setOnAir(held, enabled: true, legacy: false)
        held.app.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        await held.station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("ipad-1", short: "JJ\u{2019}s iPad", keyed: false))))
        let words = TxEqualizerModel.holderReason("JJ\u{2019}s iPad")
        #expect(await held.settle { held.eq.curveReason == words && held.eq.curve != nil })
        try await shoot("eqcurve-portrait-transmitter-held", held.eq)
        await held.app.disconnect()
    }

    /// The Core's flat default: ten points from 0 to 4000 Hz at 0 dB.
    static let defaultCurve: String = {
        let points = TxEqualizerModel.flatPoints(10, from: 0, to: 4000)
        return TxEqCurve(state: .default, parametric: true, preampDb: 0, minHz: 0, maxHz: 4000, points: points)
            .savedJson
    }()

    private func setOnAir(_ harness: TxEqCurveHarness, enabled: Bool, legacy: Bool) async throws {
        await harness.station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [
            .init(ordinal: try TxEqCurveHarness.ordinal("txEqEnabled"), name: "txEqEnabled", value: .bool(enabled)),
            .init(ordinal: try TxEqCurveHarness.ordinal("txEqUseLegacy"), name: "txEqUseLegacy", value: .bool(legacy)),
        ])))
    }

    private func shoot(_ name: String, _ model: TxEqualizerModel, height: CGFloat = pageHeight,
                       sideways: Bool = false, large: Bool = false) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: height)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        if sideways {
            // Clear of the upright status bar and home indicator, as the other sideways shots sit.
            window.frame.origin.y = 120
        }
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            ConnectChrome.NavBar(title: "TX Equalizer", back: "Tools") {
                EmptyView()
            }
            TxEqualizerScreen(model: model)
            TabBar(selection: .constant(.tools), sideways: sideways)
        }
        .background(ChromeColours.page)
        .environment(\.dynamicTypeSize, large ? .accessibility3 : .large)
        .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        if sideways {
            // A sideways phone's safe area, as ToolsPagesShotTests sets it.
            let upright = window.safeAreaInsets
            host.additionalSafeAreaInsets = UIEdgeInsets(top: -upright.top, left: 62 - upright.left,
                                                         bottom: 21 - upright.bottom, right: 62 - upright.right)
        }
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        #expect(image.size == size)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try? data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}
