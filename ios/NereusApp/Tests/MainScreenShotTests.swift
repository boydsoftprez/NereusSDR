// NereusSDR for iOS: the main screen listening, upright and sideways, for comparing with the board's pictures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
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

/// R-IOS-11, D7, D11, D68: the real main screen and tab bar, connected to a
/// fake Core with the ANAN-G2 catalogue, two slices where the board draws
/// them and synthetic frames on the band, hosted in a window on the
/// simulator. With `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each screen is written there as a PNG
/// for comparing with `01-on-the-band.jpg`, `02-sideways.jpg`,
/// `05-tabs.jpg` and `12-audio-and-data.jpg`.
@Suite("Main screen on screen", .serialized)
@MainActor
struct MainScreenShotTests {
    struct Screen {
        let name: String
        let sideways: Bool
        let rxPanelOpen: Bool
    }

    @Test("the main screen listening, upright and sideways, with the RX panel and the sound notice")
    func mainScreenShots() async throws {
        let center = NotificationCenter()
        let session = FakeAudioSession(device: .wired("Headphones"))
        let audio = AudioSessionController(session: session, output: FakePlaybackOutput(), notificationCenter: center)
        let defaults = try #require(UserDefaults(suiteName: "MainScreenShotTests"))
        let model = AppModel(audio: audio, displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await Self.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        // The link's round trip is timed at its first heartbeat, 20 s in.
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })

        let band = model.main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))

        for screen in [Screen(name: "upright", sideways: false, rxPanelOpen: false),
                       Screen(name: "upright-rx-panel", sideways: false, rxPanelOpen: true),
                       Screen(name: "sideways", sideways: true, rxPanelOpen: false),
                       Screen(name: "sideways-rx-panel", sideways: true, rxPanelOpen: true)] {
            try await shoot(screen, model: model)
        }

        // The Core holds NNR at Standard: the RX panel marks it and offers Try again (C3).
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 59, name: "activeNr", value: .enumeration(8)),
            .init(ordinal: 69, name: "nnrAvailable", value: .bool(true)),
            .init(ordinal: 84, name: "nnrStatus", value: .utf8(
                "Noise reduction is using the Standard model. The Core computer could not keep up with Premium.")),
            .init(ordinal: 86, name: "nnrLimit", value: .i64(1)),
        ])))
        #expect(await settle(seconds: 30) { model.main.rx.nnr?.limit == 1 })
        try await shoot(Screen(name: "upright-rx-panel-nnr-stepped-back", sideways: false, rxPanelOpen: true), model: model)
        // A typed filter edge: the number pad over the RX panel (C2).
        model.main.rx.openFilterEdgePad(low: true)
        try await shoot(Screen(name: "upright-rx-panel-filter-pad", sideways: false, rxPanelOpen: true), model: model)
        model.main.rx.closePad()

        // Headphones go away: the sound pauses and the band says so.
        session.device = nil
        center.post(name: AVAudioSession.routeChangeNotification, object: nil,
                    userInfo: [AVAudioSessionRouteChangeReasonKey: AVAudioSession.RouteChangeReason.oldDeviceUnavailable.rawValue])
        await audio.settle()
        #expect(audio.notice == .headphonesDisconnected)
        try await shoot(Screen(name: "upright-headphones-disconnected", sideways: false, rxPanelOpen: false),
                        model: model)
        await model.disconnect()
    }

    // MARK: Inside

    private func settle(seconds: Double, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    /// The board's receiving scene: the ANAN-G2 catalogue as the fake's Core
    /// sends it (a band grid only with band select), the Core's name, slice A
    /// at 7.236.400 LSB and slice B at 7.249.000.
    static func fill(_ station: FakeStation) async throws {
        let json = try #require(MainScreenTests.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "devices", className: "StationDevicesFacade",
                                                                    properties: [
            .init(ordinal: 2, name: "stationLabel", value: .utf8("KG4VCF/shack")),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_236_400)),
            .init(ordinal: 2, name: "dspMode", value: .enumeration(0)),
            .init(ordinal: 3, name: "filterLow", value: .i64(-3000)),
            .init(ordinal: 4, name: "filterHigh", value: .i64(-100)),
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-105)),
            .init(ordinal: 58, name: "nbMode", value: .enumeration(1)),
            .init(ordinal: 59, name: "activeNr", value: .enumeration(2)),
            .init(ordinal: 39, name: "ssqlThresh", value: .f64(20)),
            .init(ordinal: 7, name: "afGain", value: .i64(62)),
        ])))
        await station.deliver(BandFlagShotTests.slice(1, active: false))
    }

    private func shoot(_ screen: Screen, model: AppModel) async throws {
        let size = screen.sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        // Sideways, the window sits clear of the upright status bar and home
        // indicator, and takes the sideways phone's own safe area instead.
        window.frame = CGRect(origin: CGPoint(x: 0, y: screen.sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = ShotRoot(model: model, rxPanelOpen: screen.rxPanelOpen)
            .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        if screen.sideways {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        // The first draw sizes the waterfall; the frames then fill it.
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        #expect(band.markers.count == 2)

        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(screen.name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The app's root as `RootView` lays it out, with the RX panel open or not.
private struct ShotRoot: View {
    @ObservedObject var model: AppModel
    let rxPanelOpen: Bool

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, rxPanelOpen: rxPanelOpen)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
