// NereusSDR for iOS: the Panadapter tab's swipes: their settings, their zones, and the TX panel by swipe that never keys
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 18, the swipes on the panadapter and the
// never-key rule for the TX panel opened by one. J.J. Boyd (KG4VCF),
// AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 19, the phone's defaults asked for as a
// phone's, so the suite reads the same on an iPad. J.J. Boyd (KG4VCF),
// AI-assisted via Claude Code.

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Spec 11.3 (Swipes) and S1: three swipes mapped in Setup, saved on this
/// phone, RX panel, TX panel and Roundtable on my frequency by default;
/// 18-point edge zones that end above PTT, a drag that heads in and not
/// along; nothing while a sheet or panel is up or off the Panadapter tab;
/// and the TX panel opened by a swipe never keys. With `NEREUS_MAIN_SHOTS`
/// set, a swipe part way in, the panels opened from either side, and
/// Setup's rows are written there for the board.
@Suite("Swipes on the panadapter", .serialized)
@MainActor
struct SwipeShortcutsTests {
    private let platform = TestPlatform()

    // MARK: Settings

    private static func settings() throws -> (PhoneSettings, UserDefaults) {
        let defaults = try #require(UserDefaults(suiteName: "SwipeShortcutsTests-\(UUID().uuidString)"))
        return (PhoneSettings(defaults: defaults, isPad: false), defaults)
    }

    @Test("the defaults are RX panel, TX panel and Roundtable on my frequency, and nothing is changed")
    func defaults() throws {
        let (settings, _) = try Self.settings()
        #expect(settings.swipeLeftEdge == .rxPanel)
        #expect(settings.swipeRightEdge == .txPanel)
        #expect(settings.swipeUpTabBar == .roundtableMine)
        #expect(!settings.swipesChanged)
        #expect(SwipeTarget.allCases.map(\.rawValue)
                == ["off", "rxPanel", "txPanel", "roundtableMine", "roundtableAll", "spotHub", "tools"])
        #expect(SwipeTarget.allCases.map(\.title)
                == ["Off", "RX panel", "TX panel", "Roundtable, my frequency", "Roundtable, all frequencies",
                    "Spot Hub", "Tools"])
    }

    @Test("a mapping is saved on this phone, lights Reset, and Reset brings back every default")
    func mappingAndReset() throws {
        let (settings, defaults) = try Self.settings()
        settings.swipeLeftEdge = .off
        settings.swipeUpTabBar = .spotHub
        #expect(settings.swipesChanged)
        #expect(defaults.string(forKey: "phone.swipe.leftEdge") == "off")
        #expect(defaults.string(forKey: "phone.swipe.upTabBar") == "spotHub")
        // Kept: a new settings object on the same store reads them back.
        let again = PhoneSettings(defaults: defaults, isPad: false)
        #expect(again.swipeLeftEdge == .off && again.swipeUpTabBar == .spotHub && again.swipeRightEdge == .txPanel)
        // Mapped back by hand, nothing is changed.
        settings.swipeLeftEdge = .rxPanel
        settings.swipeUpTabBar = .roundtableMine
        #expect(!settings.swipesChanged)
        settings.swipeRightEdge = .tools
        #expect(settings.swipesChanged)
        settings.resetSwipes()
        #expect(!settings.swipesChanged)
        #expect(settings.swipeRightEdge == .txPanel)
        #expect(defaults.string(forKey: "phone.swipe.rightEdge") == nil)
        // A value this app does not know reads as the default.
        defaults.set("sideways", forKey: "phone.swipe.leftEdge")
        #expect(settings.swipeLeftEdge == .rxPanel)
    }

    // MARK: Zones and drags

    @Test("the edge zones are 18 points wide, inside a sideways band's insets, and end above PTT")
    func zones() {
        let size = CGSize(width: 402, height: 600)
        let ptt = PttButton.frame(bandSize: size, leadingInset: 0)
        let foot = ptt.minY - SwipeShortcuts.aboveControls
        func origin(_ x: CGFloat, _ y: CGFloat, leading: CGFloat = 0, trailing: CGFloat = 0) -> SwipeOrigin? {
            SwipeShortcuts.edgeOrigin(start: CGPoint(x: x, y: y), bandSize: size, zoneFoot: foot,
                                      leadingInset: leading, trailingInset: trailing)
        }
        #expect(origin(0, 100) == .leftEdge)
        #expect(origin(17.9, 100) == .leftEdge)
        #expect(origin(18, 100) == nil)
        #expect(origin(201, 100) == nil)
        #expect(origin(384.1, 100) == .rightEdge)
        #expect(origin(401, 100) == .rightEdge)
        #expect(origin(384, 100) == nil)
        // Sideways, under the rounded corners: the zone begins inside the inset.
        #expect(origin(50, 100, leading: 44) == .leftEdge)
        #expect(origin(63, 100, leading: 44) == nil)
        #expect(origin(402 - 50, 100, trailing: 44) == .rightEdge)
        // A drag that starts on PTT, or just above it, is never a swipe.
        #expect(origin(ptt.midX, ptt.midY) == nil)
        #expect(origin(ptt.minX + 1, ptt.minY + 1) == nil)
        #expect(origin(2, foot - 1) == .leftEdge)
        #expect(origin(2, foot) == nil)
        #expect(origin(400, foot + 1) == nil)
        #expect(origin(2, -1) == nil)
    }

    @Test("a swipe is a drag in, more in than along; any other drag there is the band's")
    func claims() {
        #expect(SwipeShortcuts.claims(.leftEdge, translation: CGSize(width: 20, height: 5)))
        #expect(!SwipeShortcuts.claims(.leftEdge, translation: CGSize(width: -20, height: 0)))
        #expect(!SwipeShortcuts.claims(.leftEdge, translation: CGSize(width: 6, height: 30)))
        #expect(SwipeShortcuts.claims(.rightEdge, translation: CGSize(width: -20, height: 5)))
        #expect(!SwipeShortcuts.claims(.rightEdge, translation: CGSize(width: 20, height: 0)))
        #expect(SwipeShortcuts.claims(.tabBar, translation: CGSize(width: 3, height: -20)))
        #expect(!SwipeShortcuts.claims(.tabBar, translation: CGSize(width: 30, height: -10)))
        #expect(!SwipeShortcuts.claims(.tabBar, translation: CGSize(width: 0, height: 20)))
        #expect(SwipeShortcuts.inward(.rightEdge, CGSize(width: -90, height: 0)) == 90)
        #expect(SwipeShortcuts.inward(.tabBar, CGSize(width: 0, height: -40)) == 40)
    }

    @Test("let go 72 points in, or flicked to 160, the page opens; shorter, it springs back")
    func opens() {
        #expect(SwipeShortcuts.opens(distance: 72, predicted: 72))
        #expect(!SwipeShortcuts.opens(distance: 71, predicted: 100))
        #expect(SwipeShortcuts.opens(distance: 30, predicted: 160))
        #expect(SwipeTarget.txPanel.detail == "Opening it never keys")
        #expect(SwipeTarget.off.detail == "The edge goes back to dragging the band")
        #expect(SwipeOrigin.leftEdge.edge == .leading && SwipeOrigin.rightEdge.edge == .trailing
                && SwipeOrigin.tabBar.edge == .bottom)
    }

    @Test("a swipe follows the finger, opens what it is mapped to, and a short one springs back")
    func model() throws {
        let (settings, _) = try Self.settings()
        let swipes = SwipeShortcutModel(settings: settings)
        #expect(swipes.begin(.rightEdge))
        #expect(swipes.drag == .init(origin: .rightEdge, target: .txPanel, distance: 0))
        // One at a time.
        #expect(!swipes.begin(.leftEdge))
        swipes.move(120)
        #expect(swipes.drag?.distance == 120)
        swipes.move(-10)
        #expect(swipes.drag?.distance == 0)
        #expect(!swipes.end(distance: 40, predicted: 60, reduceMotion: true))
        #expect(swipes.opened == nil && swipes.drag == nil)
        #expect(swipes.begin(.leftEdge))
        #expect(swipes.end(distance: 200, predicted: 200))
        #expect(swipes.opened == .init(serial: 1, origin: .leftEdge, target: .rxPanel))
        swipes.finish()
        #expect(swipes.drag == nil)
        settings.swipeUpTabBar = .tools
        #expect(swipes.begin(.tabBar))
        #expect(swipes.end(distance: 100, predicted: 100))
        #expect(swipes.opened == .init(serial: 2, origin: .tabBar, target: .tools))
        swipes.finish()
    }

    @Test("nothing swipes while Off, while a sheet or panel is up, or off the Panadapter tab")
    func blocked() throws {
        let (settings, _) = try Self.settings()
        let swipes = SwipeShortcutModel(settings: settings)
        settings.swipeLeftEdge = .off
        #expect(!swipes.begin(.leftEdge))
        #expect(swipes.begin(.rightEdge))
        swipes.cancel(reduceMotion: true)
        #expect(swipes.drag == nil)
        swipes.panelsUp = true
        #expect(!swipes.begin(.rightEdge) && !swipes.begin(.tabBar))
        swipes.panelsUp = false
        swipes.panadapterShown = false
        #expect(!swipes.begin(.rightEdge))
        swipes.panadapterShown = true
        #expect(swipes.begin(.rightEdge))
        swipes.cancel(reduceMotion: true)
    }

    @Test("a flag's menu or the number pad on the band stops a swipe")
    func bandPopoversBlock() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .roundtable])
        try await MainScreenShotTests.fill(station)
        #expect(await settle { model.main.slices.entries.count == 2 })
        let swipes = model.swipes
        swipes.attach(tuning: model.main.tuning, flagControls: model.main.flagControls)
        #expect(!swipes.blocked)
        model.main.flagControls.toggle(.more(0))
        #expect(model.main.flagControls.open != nil)
        #expect(!swipes.begin(.rightEdge))
        model.main.flagControls.close()
        model.main.tuning.openPad(sliceId: 0)
        #expect(model.main.tuning.pad != nil)
        #expect(!swipes.begin(.leftEdge))
        model.main.tuning.closePad()
        #expect(swipes.begin(.leftEdge))
        swipes.cancel(reduceMotion: true)
        await model.disconnect()
    }

    // MARK: Against a fake Core

    @Test("REGRESSION: the TX panel opened by a swipe never keys: no tx.key, the radio unkeyed")
    func txPanelBySwipeNeverKeys() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        var asks: [String] = []
        transmit.keyObserver = { asks.append($0) }
        defer { transmit.keyObserver = nil }
        let (window, _) = try host(SwipeHost(model: model))
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        #expect(SetupTypedEntryTests.element("txPanelDrawer", in: window) == nil)
        let swipes = model.swipes
        #expect(await settle { !swipes.blocked })
        #expect(swipes.begin(.rightEdge))
        swipes.move(160)
        #expect(swipes.end(distance: 160, predicted: 240))
        #expect(await settle { SetupTypedEntryTests.element("txPanelDrawer", in: window) != nil })
        #expect(await settle { swipes.drag == nil && swipes.panelsUp })
        // Give anything that would key time to reach the Core.
        try await Task.sleep(for: .seconds(1))
        #expect(asks.isEmpty)
        #expect(Self.verbs(station, "tx.key") == 0)
        #expect(!station.keyed)
        #expect(transmit.ptt.state == .idle)
        // With the panel up, another swipe opens nothing.
        #expect(!swipes.begin(.leftEdge))
        // The control: MOX on the same panel does key, so the count above can see a key.
        transmit.tapPtt()
        #expect(asks == ["tap"])
        #expect(await settle { transmit.ptt.state.isKeyed && station.keyed })
        #expect(Self.verbs(station, "tx.key") >= 1, "tx.key sent \(Self.verbs(station, "tx.key")) times")
        transmit.tapPtt()
        #expect(await settle { transmit.ptt.state == .idle && !station.keyed })
        await model.disconnect()
    }

    @Test("the RX and TX panels follow the finger in from the swipe's side, and a short drag springs back")
    func panelsFollowAndSpringBack() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await connected()
        let (window, _) = try host(SwipeHost(model: model))
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let swipes = model.swipes
        #expect(await settle { !swipes.blocked })
        // Part way in: the RX panel shows, its right edge under the finger.
        #expect(swipes.begin(.leftEdge))
        swipes.move(120)
        let drawer = try #require(await element("rxPanelDrawer", in: window))
        let frame = drawer.accessibilityFrame
        #expect(abs(frame.maxX - 120) < 2)
        // Let go short: it springs back and is gone.
        #expect(!swipes.end(distance: 40, predicted: 50))
        #expect(await settle { swipes.drag == nil })
        #expect(await settle { SetupTypedEntryTests.element("rxPanelDrawer", in: window) == nil })
        // Mapped to the left edge, the TX panel comes in from the left.
        model.phoneSettings.swipeLeftEdge = .txPanel
        #expect(swipes.begin(.leftEdge))
        swipes.move(100)
        let tx = try #require(await element("txPanelDrawer", in: window))
        #expect(abs(tx.accessibilityFrame.maxX - 100) < 2)
        #expect(swipes.end(distance: 100, predicted: 100))
        #expect(await settle { swipes.panelsUp && swipes.drag == nil })
        #expect(await settle {
            (SetupTypedEntryTests.element("txPanelDrawer", in: window)?.accessibilityFrame.minX ?? -1) < 1
        })
        #expect(Self.verbs(station, "tx.key") == 0)
        await model.disconnect()
    }

    // MARK: Shots

    @Test("shots: a swipe part way in, the TX panel from the right and the left, and Setup's rows")
    func shots() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .roundtable])
        try await MainScreenShotTests.fill(station)
        #expect(await settle { model.main.slices.entries.count == 2 })
        model.main.band.endpointId = 1
        model.main.band.receive(.context(try #require(BandFlagShotTests.context())))
        try await station.deliverRoundtable(RoundtableToolsPageTests.bandScene())
        #expect(await settle { model.roundtable.rooms.count == RoundtableToolsPageTests.bandRooms.count })
        let swipes = model.swipes
        try await shootBand("swipe-rx-part-way", model: model) {
            #expect(await settle { !swipes.blocked })
            #expect(swipes.begin(.leftEdge))
            swipes.move(150)
        }
        swipes.cancel(reduceMotion: true)
        try await shootBand("swipe-tx-open-right", model: model) {
            #expect(await settle { !swipes.blocked })
            #expect(swipes.begin(.rightEdge))
            swipes.move(200)
            #expect(swipes.end(distance: 200, predicted: 200))
            #expect(await settle { swipes.drag == nil })
        }
        model.phoneSettings.swipeLeftEdge = .txPanel
        try await shootBand("swipe-tx-open-left", model: model) {
            #expect(await settle { !swipes.blocked })
            #expect(swipes.begin(.leftEdge))
            swipes.move(200)
            #expect(swipes.end(distance: 200, predicted: 200))
            #expect(await settle { swipes.drag == nil })
        }
        try await shootSetup("swipe-setup-changed", settings: model.phoneSettings)
        model.phoneSettings.resetSwipes()
        try await shootSetup("swipe-setup-defaults", settings: model.phoneSettings)
        #expect(Self.verbs(station, "tx.key") == 0)
        await model.disconnect()
    }

    // MARK: Helpers

    private func connected(additions: FakeStation.Additions = [.remoteTx]) async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "SwipeShortcutsTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults, isPad: false),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await settle { model.main.transmit.permitted && model.connection == .connected })
        return (model, station)
    }

    private static func verbs(_ station: FakeStation, _ verb: String) -> Int {
        station.messages.compactMap(TransmitScreenTests.invoke).filter { $0.verb == verb }.count
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    private func element(_ id: String, in window: UIWindow) async -> NSObject? {
        var found: NSObject?
        _ = await settle(seconds: 10) {
            found = SetupTypedEntryTests.element(id, in: window)
            return found != nil
        }
        return found
    }

    private func host<Content: View>(_ content: Content,
                                     size: CGSize = CGSize(width: 402, height: 874)) throws -> (UIWindow, UIView) {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let host = ShotHosting.host(content.preferredColorScheme(.dark))
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        return (window, host.view)
    }

    private func write(_ name: String, _ window: UIWindow) throws {
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

    private func shootBand(_ name: String, model: AppModel, then setUp: () async throws -> Void) async throws {
        let size = CGSize(width: 402, height: 874)
        let (window, _) = try host(SwipeHost(model: model), size: size)
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        try await setUp()
        try await Task.sleep(for: .milliseconds(400))
        try write(name, window)
    }

    private func shootSetup(_ name: String, settings: PhoneSettings) async throws {
        settings.swipeLeftEdge = name.hasSuffix("changed") ? .txPanel : settings.swipeLeftEdge
        let (window, _) = try host(NavigationStack { NavigationPage(settings: settings) }, size: CGSize(width: 402, height: 1100))
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        try await Task.sleep(for: .milliseconds(300))
        try write(name, window)
    }
}

/// The app's Panadapter tab as `RootView` lays it out, with the swipes on.
private struct SwipeHost: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, swipes: model.swipes)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
