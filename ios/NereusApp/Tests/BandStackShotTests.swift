// NereusSDR for iOS: the band stack on the real main screen over a drawn band, on a phone and an iPad, on a Core with it and an older one
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Band stack spec section 8, acceptance 16: the block, the markers and their
/// switch over a band with a real waterfall behind them, for comparing with
/// `docs/architecture/2026-11-05-band-stack-design/phone-and-ipad.html`. With
/// `NEREUS_MAIN_SHOTS` set on a phone, or `NEREUS_IPAD_SHOTS` on an iPad
/// (through `TEST_RUNNER_`), each screen is written there as a PNG. The iPad's
/// applet either way up is `IPadShotTests.bandStackAppletShots`. What a finger
/// reaches, the menus and the Setup page, is `BandStackUITests`, which writes
/// to the same folders.
@Suite("The band stack on screen", .serialized, .timeLimit(.minutes(10)))
@MainActor
struct BandStackShotTests {
    /// As long as a Core's refusal gets.
    private static let refusal = "You are only listening to Slice A. Take control of it to change its frequency."

    private struct Rig {
        let model: AppModel
        let station: FakeStation
        let settings: PhoneSettings
        let stack: BandStackModel
    }

    private struct Screen {
        let name: String
        var sideways = false
        var sheet: OpenSheet? = nil
        var columnMenu: BandColumnMenu? = nil
        var displayTab: DisplayPillTab = .spectrum
        /// The pill's rows scrolled to their end, where its "On this phone" switches are.
        var pillAtItsEnd = false
        /// An element that must be on the screen before it is drawn.
        var shows: [String] = []
        /// Run with the screen up, for what its closing would clear.
        var prepare: () -> Void = {}
    }

    @Test("the Pan sheet, Reorder, the BAND menu and the markers on a phone",
          .enabled("The phone's screens are drawn on an iPhone simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .phone }
          })
    func phoneShots() async throws {
        // What each screen must show is found through the accessibility tree.
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let ready = try await connect(older: false)
        let stack = ready.stack
        try await shoot(Screen(name: "band-stack-main-pan-sheet", sheet: .pan,
                               shows: ["bandStackCount", "bandStackRow.40m-net", "bandStackSetupLink"]), rig: ready)
        try await shoot(Screen(name: "band-stack-main-reorder", sheet: .pan, shows: ["bandStackGrip.40m-net"],
                               prepare: { stack.toggleReorder() }), rig: ready)
        stack.endViewing()
        try await shoot(Screen(name: "band-stack-main-band-menu-sideways", sideways: true, columnMenu: .band,
                               shows: ["bandStackCount", "bandGrid.40"]), rig: ready)
        ready.settings.showBandStackMarkers = true
        for sideways in [false, true] {
            try await shoot(Screen(name: "band-stack-main-markers\(sideways ? "-sideways" : "")", sideways: sideways,
                                   shows: ["bandStackMarker.40m-net", "bandStackMarker.40m-ssb"]), rig: ready)
        }
        try await shoot(Screen(name: "band-stack-main-markers-sideways-note", sideways: true,
                               shows: ["bandStackMarkerNote"], prepare: { stack.note = Self.refusal }), rig: ready)
        stack.note = nil
        await ready.model.disconnect()

        let older = try await connect(older: true)
        try await shoot(Screen(name: "band-stack-main-older-pan-sheet", sheet: .pan, shows: ["bandStackUnavailable"]),
                        rig: older)
        try await shoot(Screen(name: "band-stack-main-older-band-menu-sideways", sideways: true, columnMenu: .band,
                               shows: ["bandStackUnavailable"]), rig: older)
        await older.model.disconnect()
    }

    @Test("the markers and their switch, the Pan sheet and the BAND menu on an iPad",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          })
    func iPadShots() async throws {
        // What each screen must show is found through the accessibility tree.
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let ready = try await connect(older: false)
        try await IPadShotTests.describeDisplayPill(ready.model, ready.station)
        try await shoot(Screen(name: "band-stack-main-ipad-pan-sheet", sheet: .pan, shows: ["bandStackSetupLink"]),
                        rig: ready)
        ready.settings.showBandStackMarkers = true
        for sideways in [false, true] {
            let tag = sideways ? "sideways" : "upright"
            try await shoot(Screen(name: "band-stack-main-ipad-markers-\(tag)", sideways: sideways,
                                   shows: ["bandStackMarker.40m-net", "bandStackMarker.40m-ssb"]), rig: ready)
            try await shoot(Screen(name: "band-stack-main-ipad-switch-\(tag)", sideways: sideways, columnMenu: .display,
                                   displayTab: .view, pillAtItsEnd: true, shows: ["displayPillBandStack"]), rig: ready)
        }
        try await shoot(Screen(name: "band-stack-main-ipad-markers-note", shows: ["bandStackMarkerNote"],
                               prepare: { ready.stack.note = Self.refusal }), rig: ready)
        ready.stack.note = nil
        await ready.model.disconnect()

        let older = try await connect(older: true)
        try await IPadShotTests.describeDisplayPill(older.model, older.station)
        try await shoot(Screen(name: "band-stack-main-ipad-older-switch", sideways: true, columnMenu: .display,
                               displayTab: .view, pillAtItsEnd: true, shows: ["displayPillBandStack"]), rig: older)
        await older.model.disconnect()
    }

    // MARK: Inside

    /// A Core with slice A on 40 m and, unless `older`, the sample stack with
    /// two entries in the band's 48 kHz view.
    private func connect(older: Bool) async throws -> Rig {
        let suite = "BandStackShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let settings = PhoneSettings(defaults: defaults)
        let model = AppModel(phoneSettings: settings, displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: older ? [.bands, .bandSelect, .notchAtSlice]
                                                       : [.bands, .bandSelect, .notchAtSlice, .bandStack])
        if !older {
            let file = URL(fileURLWithPath: #filePath)
                .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
                .deletingLastPathComponent()
                .appendingPathComponent("tests/data/bandstack/phone-markers-document.json")
            station.setBandStack(try String(contentsOf: file, encoding: .utf8))
        }
        let stack = model.main.controls(for: .first).bandStack
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await Eventually.holds { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        let json = try #require(MainScreenTests.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(3)),
        ])))
        #expect(await Eventually.holds {
            older ? stack.availability == .needsNewerCore : stack.currentBand == "40m" && stack.stack != nil
        })
        return Rig(model: model, station: station, settings: settings, stack: stack)
    }

    private static func scrollViews(in view: UIView) -> [UIScrollView] {
        ((view as? UIScrollView).map { [$0] } ?? []) + view.subviews.flatMap(scrollViews(in:))
    }

    /// Waits for `id`, for no longer than a miss should cost the rest of the screens.
    private func shown(_ id: String, in window: UIWindow) async -> Bool {
        let deadline = Date().addingTimeInterval(15)
        while Date() < deadline {
            if IPadShotTests.element(id, in: window) != nil {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return IPadShotTests.element(id, in: window) != nil
    }

    private func shoot(_ screen: Screen, rig: Rig) async throws {
        let iPad = UIDevice.current.userInterfaceIdiom == .pad
        let upright = iPad ? CGSize(width: 834, height: 1210) : CGSize(width: 402, height: 874)
        let size = screen.sideways ? CGSize(width: upright.height, height: upright.width) : upright
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        // Sideways, the window sits clear of the upright status bar and home
        // indicator, and takes the device's own sideways safe area instead.
        window.frame = CGRect(origin: CGPoint(x: 0, y: screen.sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = BandStackShotRoot(model: rig.model, iPad: iPad, sheet: screen.sheet, columnMenu: screen.columnMenu,
                                     displayTab: screen.displayTab)
            .environment(\.horizontalSizeClass, iPad ? .regular : .compact)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        if screen.sideways {
            host.additionalSafeAreaInsets = iPad ? UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
                                                 : UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let band = rig.model.main.band
        let bandDraw = try await ShotWait.requireBandLaidOut(band, in: window)
        band.reset()
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        screen.prepare()
        if screen.pillAtItsEnd {
            let rows = try #require(Self.scrollViews(in: window).first { view in
                let frame = view.convert(view.bounds, to: window)
                return (60...200).contains(frame.minX) && frame.minY < 300 && view.contentSize.height > view.bounds.height + 1
            }, "\(screen.name) has no pill that scrolls")
            rows.setContentOffset(CGPoint(x: 0, y: rows.contentSize.height - rows.bounds.height), animated: false)
        }
        for id in screen.shows {
            let found = await shown(id, in: window)
            #expect(found, "\(screen.name) does not show \(id)")
        }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        let variable = iPad ? "NEREUS_IPAD_SHOTS" : "NEREUS_MAIN_SHOTS"
        if let directory = ProcessInfo.processInfo.environment[variable], !directory.isEmpty, let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(screen.name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The app's root as `RootView` lays it out, a sheet or a column's menu open.
private struct BandStackShotRoot: View {
    @ObservedObject var model: AppModel
    let iPad: Bool
    let sheet: OpenSheet?
    let columnMenu: BandColumnMenu?
    let displayTab: DisplayPillTab

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, sheet: sheet, displayTab: displayTab, columnMenu: columnMenu)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height, iPad: iPad)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
