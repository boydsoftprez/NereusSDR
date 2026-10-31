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
        /// Sideways: the band column's open menu, or the Display pill (R-IOS-58).
        var columnMenu: BandColumnMenu? = nil
        /// Sideways with the pill: its rows take the pill's height with the
        /// footer last among them; a second shot scrolls to that footer.
        var pillScrolls = false
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
                            transportFactory: station.transportFactory, clock: HeartbeatSoonLinkClock())
        #expect(await station.waitUntilLive())
        try await Self.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        // The link's round trip is timed at its first heartbeat, which the
        // test clock brings to 1 s in (20 s in the app).
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
        // A current Core reports measured pressure without an actual limit.
        var capabilities = model.mirror.capabilities
        capabilities["nnrLoadWarningVersion"] = .int(1)
        await station.deliver(.capabilities(.init(properties: capabilities.map {
            .init(name: $0.key, value: $0.value.wireValue)
        })))
        var schema = try FakeStation.schema(ofClass: "SliceModel")
        if !schema.fields.contains(where: { $0.name == "nnrLoadWarning" }) {
            schema.fields.append(.init(ordinal: 155, name: "nnrLoadWarning", kind: .utf8))
        }
        await station.deliver(.schema(schema))
        await station.deliver(.delta(.init(key: "slice:0", properties: [
            .init(ordinal: 60, name: "nnrModelSlot", value: .i64(1)),
            .init(ordinal: 72, name: "nnrStandardAvailable", value: .bool(true)),
            .init(ordinal: 84, name: "nnrStatus", value: .utf8("")),
            .init(ordinal: 86, name: "nnrLimit", value: .i64(0)),
            .init(ordinal: 155, name: "nnrLoadWarning", value: .utf8(
                "Receiver A: receive processing is falling behind. The selected noise reduction remains running.")),
        ])))
        #expect(await settle(seconds: 30) { model.main.rx.nnr?.loadWarning != nil })
        try await shoot(Screen(name: "upright-rx-panel-nnr-load-warning", sideways: false, rxPanelOpen: true), model: model)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let warningWindow = UIWindow(windowScene: scene)
        let warningHost = ShotHosting.host(NnrStepBack(rx: model.main.rx, identifier: "narrowNnr")
            .padding(8).background(ChromeColours.page).preferredColorScheme(.dark))
        warningHost.safeAreaRegions = []
        let fitted = warningHost.sizeThatFits(in: CGSize(width: 240, height: 1000))
        #expect(fitted.width <= 240 && fitted.height < 360, "warning and actions wrap within a narrow phone panel")
        // A small standalone window must sit clear of the scene's status bar,
        // otherwise its safe-area inset consumes the measured content height.
        warningWindow.frame = CGRect(origin: CGPoint(x: 0, y: 120), size: CGSize(width: 240, height: ceil(fitted.height)))
        warningWindow.windowLevel = .alert + 1
        warningHost.view.frame = warningWindow.bounds
        warningWindow.rootViewController = warningHost
        warningWindow.isHidden = false
        await ShotWait.laidOut(warningWindow)
        #expect(warningHost.sizeThatFits(in: CGSize(width: 240, height: 1000)).height <= warningWindow.bounds.height)
        try write(warningWindow, name: "nnr-load-warning-narrow")
        warningWindow.isHidden = true
        warningWindow.rootViewController = nil
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

    /// R-IOS-58, DC8: the iPhone held sideways shows the band column at
    /// the band's top-left inside the safe area: alone, with each of its
    /// menus open, and with the Display pill as tall as the band, the PTT and the
    /// keyed meters right of what is open.
    @Test("sideways, the band column, each of its menus and the Display pill", .timeLimit(.minutes(5)))
    func sidewaysColumnShots() async throws {
        let suite = "MainScreenShotTests-column-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: [.bands, .bandSelect, .notchAtSlice])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await Self.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        try await IPadShotTests.describeDisplayPill(model, station)

        try await shoot(Screen(name: "sideways-column", sideways: true, rxPanelOpen: false), model: model)
        let menus: [(BandColumnMenu, String)] = [(.band, "band"), (.antenna, "antenna"), (.vax, "vax"),
                                                 (.attenuator, "attenuator")]
        for (menu, name) in menus {
            try await shoot(Screen(name: "sideways-column-\(name)", sideways: true, rxPanelOpen: false, columnMenu: menu),
                            model: model)
        }
        try await shoot(Screen(name: "sideways-column-pill", sideways: true, rxPanelOpen: false, columnMenu: .display,
                               pillScrolls: true), model: model)
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
        let root = ShotRoot(model: model, rxPanelOpen: screen.rxPanelOpen, columnMenu: screen.columnMenu)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
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

        try write(window, name: screen.name)
        if screen.name == "upright-rx-panel-nnr-load-warning" {
            let rows = try #require(Self.scrollViews(in: window).first {
                $0.bounds.width < window.bounds.width * 0.9
                    && $0.convert($0.bounds, to: window).minX < 2
                    && $0.contentSize.height > $0.bounds.height + 1
            })
            rows.setContentOffset(CGPoint(x: 0, y: rows.contentSize.height - rows.bounds.height), animated: false)
            await ShotWait.laidOut(window)
            try write(window, name: screen.name + "-scrolled")
        }
        if screen.pillScrolls {
            // The pill sits right of the two columns; its rows scroll under its title.
            let pillLeft = 62 + BandColumnLayout.origin + BandColumnLayout.columnWidth(columns: 2)
            let rows = try #require(Self.scrollViews(in: window).first {
                $0.convert($0.bounds, to: window).minX >= pillLeft
                    && $0.contentSize.height > $0.bounds.height + 1
            })
            // The rows keep the pill's height under its title, about 150 of
            // its 174 points; a pinned footer left them about 75.
            #expect(rows.bounds.height > 140)
            rows.setContentOffset(CGPoint(x: 0, y: rows.contentSize.height - rows.bounds.height), animated: false)
            try? await Task.sleep(for: .milliseconds(300))
            try write(window, name: "\(screen.name)-scrolled")
        }
    }

    private func write(_ window: UIWindow, name: String) throws {
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    private static func scrollViews(in view: UIView) -> [UIScrollView] {
        var found: [UIScrollView] = []
        if let scroll = view as? UIScrollView {
            found.append(scroll)
        }
        for subview in view.subviews {
            found += scrollViews(in: subview)
        }
        return found
    }
}

/// The app's root as `RootView` lays it out, with the RX panel open or not.
private struct ShotRoot: View {
    @ObservedObject var model: AppModel
    let rxPanelOpen: Bool
    var columnMenu: BandColumnMenu?

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, rxPanelOpen: rxPanelOpen, columnMenu: columnMenu)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
