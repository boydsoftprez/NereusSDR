// NereusSDR for iOS: the iPad on its side and upright, and Setup beside its page, for comparing with the boards
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

/// R-IOS-24, D30, D31: the real main screen and tab bar on an 11-inch iPad,
/// connected to a fake Core with the ANAN-G2 catalogue and two slices where
/// the board draws them. On its side the band gives the applet column its
/// 320 points and takes them back when the column hides; upright the band
/// keeps the whole width over the front panel. With `NEREUS_IPAD_SHOTS` set
/// to a directory (through `TEST_RUNNER_NEREUS_IPAD_SHOTS`), each screen is
/// written there as a PNG for comparing with `18-ipad-landscape.jpg` and
/// `19-ipad-upright.jpg`. It runs on an iPad simulator only.
@Suite("iPad on screen", .serialized)
@MainActor
struct IPadShotTests {
    struct Screen {
        let name: String
        let onItsSide: Bool
        let columnShown: Bool
        /// The band column's open menu, or the Display pill (R-IOS-58).
        var columnMenu: BandColumnMenu? = nil
        /// With two pans, the pan whose column has `columnMenu` open.
        var columnPan: PanIdentity = .first
        var displayTab: DisplayPillTab = .spectrum

        /// The 11-inch iPad's screen, either way up.
        var size: CGSize {
            onItsSide ? CGSize(width: 1210, height: 834) : CGSize(width: 834, height: 1210)
        }

        /// The band's width in the layout, in points.
        var bandWidth: CGFloat {
            let layout: IPadLayout = onItsSide ? .column : .frontPanel
            return layout.bandWidth(screenWidth: size.width, columnShown: columnShown)
        }
    }

    @Test("the iPad on its side with the column, without it, and upright with the front panel",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          })
    func iPadShots() async throws {
        let defaults = try #require(UserDefaults(suiteName: "IPadShotTests"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        // The board's reading: S8, -79 dBm, on slice A's flag and on the S-meter.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 15, name: "signalStrengthDbm", value: .f64(-79)),
            .init(ordinal: 16, name: "signalPeakDbm", value: .f64(-79)),
        ])))
        #expect(await settle(seconds: 30) {
            model.main.slices.entries.count == 2 && model.main.coreName != nil
                && model.main.slices.active?.signalPeakDbm == -79
        })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })

        let band = model.main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))

        for screen in [Screen(name: "ipad-landscape", onItsSide: true, columnShown: true),
                       Screen(name: "ipad-landscape-column-hidden", onItsSide: true, columnShown: false),
                       Screen(name: "ipad-upright", onItsSide: false, columnShown: true)] {
            try await shoot(screen, model: model)
        }
        await model.disconnect()
    }

    /// R-IOS-58, DC7, DC8: the band column at the band's top-left, on its
    /// side and upright, with each of its menus open in turn and the Display
    /// pill on each of its five tabs, on a Core that describes Display at
    /// version 29 and opens bands for the phone; for comparing with the
    /// board's iPad frames. The PTT and the keyed meters sit right of
    /// whatever is open.
    @Test("the band column on the iPad with each menu open and the pill on each tab",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          },
          .timeLimit(.minutes(10)))
    func bandColumnShots() async throws {
        let suite = "IPadShotTests-column-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: [.bands, .bandSelect, .notchAtSlice])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        try await Self.describeDisplayPill(model, station)

        try await shoot(Screen(name: "ipad-column", onItsSide: true, columnShown: true), model: model)
        let menus: [(BandColumnMenu, String)] = [(.band, "band"), (.antenna, "antenna"), (.vax, "vax"),
                                                 (.attenuator, "attenuator")]
        for (menu, name) in menus {
            try await shoot(Screen(name: "ipad-column-\(name)", onItsSide: true, columnShown: true, columnMenu: menu),
                            model: model)
            try await shoot(Screen(name: "ipad-upright-column-\(name)", onItsSide: false, columnShown: true,
                                   columnMenu: menu), model: model)
        }
        for tab in DisplayPillTab.allCases {
            try await shoot(Screen(name: "ipad-column-pill-\(tab.rawValue)", onItsSide: true, columnShown: true,
                                   columnMenu: .display, displayTab: tab), model: model)
        }
        try await shoot(Screen(name: "ipad-upright-column-pill-spectrum", onItsSide: false, columnShown: true,
                               columnMenu: .display), model: model)
        await model.disconnect()
    }

    /// R-IOS-59, DC9, DC10: two pans on the iPad, Pan 1 on top and Pan 2
    /// below sharing the band's height, each with its own column; Pan 1's
    /// pill open, then Pan 2's, the toolbar naming the pan last touched;
    /// for comparing with the board's "Two pans, two pills".
    @Test("two pans on the iPad, each pill open in turn",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          },
          .timeLimit(.minutes(10)))
    func twoPanShots() async throws {
        let suite = "IPadShotTests-pans-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(additions: [.bands, .bandSelect, .notchAtSlice])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 30) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        try await Self.describeDisplayPill(model, station)
        let stack = model.main.panStack
        #expect(stack.allowsMorePans)
        stack.addPan()
        #expect(await settle(seconds: 30) { stack.status != nil && stack.open.count == 2 })
        let second = PanIdentity(number: 2)
        let pan2 = stack.band(for: second)
        #expect(await settle(seconds: 30) { !pan2.slices.entries.isEmpty })
        // Pan 2's slice on 40 m inside its view, with a receiver's rate.
        let sliceId = try #require(pan2.slices.entries.first?.id)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:\(sliceId)", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_247_500)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
        ])))
        for band in [model.main.band, pan2.band] {
            band.endpointId = 1
            band.receive(.context(try #require(BandFlagShotTests.context())))
        }
        try await shootPans(Screen(name: "ipad-two-pans-pill-1", onItsSide: true, columnShown: true,
                                   columnMenu: .display, columnPan: .first), model: model, touched: .first)
        #expect(model.main.toolbarPan == .first)
        try await shootPans(Screen(name: "ipad-two-pans-pill-2", onItsSide: true, columnShown: true,
                                   columnMenu: .display, columnPan: second), model: model, touched: second)
        #expect(model.main.toolbarPan == second)
        try await shootPans(Screen(name: "ipad-upright-two-pans-pill-2", onItsSide: false, columnShown: true,
                                   columnMenu: .display, columnPan: second), model: model, touched: second)
        await model.disconnect()
    }

    /// A Core that refuses Pan 2's display: Pan 2 says so in the Core's
    /// words, in the house card over its band, and Pan 1 says nothing.
    @Test("a pan whose display the Core refused says so in the Core's words",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          },
          .timeLimit(.minutes(10)))
    func refusedPanShot() async throws {
        let suite = "IPadShotTests-refused-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let station = try FakeStation(additions: .wideband)
        let model = AppModel(mediaPeerFactory: station.mediaPeerFactory,
                             displaySettings: BandDisplaySettingsStore(defaults: defaults))
        model.main.panStack.allowsMorePans = true
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        let json = try #require(MainScreenTests.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await settle(seconds: 30) { model.main.catalogFeed.catalog != nil && model.main.pan.sliceLetter == "A" })
        _ = model.main.band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        let words = "The Core has no display free for another pan."
        // Pan 1 subscribes on endpoint 1; Pan 2's first is the next.
        station.refuseNext(FakeStation.subscribeRefusalKey(2), reason: words)
        let stack = model.main.panStack
        stack.addPan()
        let second = PanIdentity(number: 2)
        #expect(await settle(seconds: 30) { stack.status != nil && station.panKey(ofSlice: 1) == "pan-1" })
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_213_000)),
            .init(ordinal: 28, name: "sampleRateHz", value: .i64(192_000)),
        ])))
        _ = stack.band(for: second).band.prepareToDraw(size: CGSize(width: 1206, height: 700), scale: 3)
        #expect(await settle(seconds: 30) { stack.displayRefusals[second] == words })

        let size = CGSize(width: 1210, height: 834)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: 120), size: size)
        window.windowLevel = .alert + 1
        let root = IPadShotRoot(model: model, columnShown: true)
            .environment(\.horizontalSizeClass, .regular)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        _ = try await ShotWait.requireBandLaidOut(stack.band(for: second).band, in: window)
        let notice = await settleElement(seconds: 10, "panDisplayRefusal", in: window)
        #expect(notice != nil)
        #expect(notice?.accessibilityLabel == words)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("ipad-two-pans-pan-2-refused.png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        await model.disconnect()
    }

    private func settleElement(seconds: Double, _ id: String, in window: UIWindow) async -> NSObject? {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if let found = SetupTypedEntryTests.element(id, in: window) {
                return found
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return SetupTypedEntryTests.element(id, in: window)
    }

    /// Draws the two pans with `touched` the pan last touched, each band fed.
    private func shootPans(_ screen: Screen, model: AppModel, touched: PanIdentity) async throws {
        model.main.panStack.lastTouched = touched
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: screen.onItsSide ? 120 : 0), size: screen.size)
        window.windowLevel = .alert + 1
        let root = IPadShotRoot(model: model, columnShown: screen.columnShown, columnMenu: screen.columnMenu,
                                columnPan: screen.columnPan, displayTab: screen.displayTab)
            .environment(\.horizontalSizeClass, .regular)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        if screen.onItsSide {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        }
        host.view.frame = CGRect(origin: .zero, size: screen.size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bands = model.main.panStack.open.map { model.main.panStack.band(for: $0).band }
        #expect(bands.count == 2)
        for band in bands {
            let drawn = try await ShotWait.requireBandLaidOut(band, in: window)
            BandFlagShotTests.feed(band, width: drawn.requestedPixels, lines: Int(screen.size.height * 2))
            try await ShotWait.requireBandShown(band, in: window, after: drawn)
        }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(screen.name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    /// The Core describes Display at version 29 with display extras 4, as
    /// DropSheetShotTests' pill shots have it, so the Display pill draws its
    /// rows; waits until the pill has the description.
    static func describeDisplayPill(_ model: AppModel, _ station: FakeStation) async throws {
        var properties = model.mirror.capabilities.keys.sorted().compactMap { name -> LinkMessage.PropertyEntry? in
            guard name != "setupDescriptionVersion", name != "displayExtrasVersion",
                  let value = model.mirror.capabilities[name] else { return nil }
            return LinkMessage.PropertyEntry(name: name, value: value.wireValue)
        }
        properties.append(.init(name: "setupDescriptionVersion", value: .i64(29)))
        // A Core with media on sends display extras 4
        // (src/core/session/StationServer.cpp:4776-4784).
        properties.append(.init(name: "displayExtrasVersion", value: .i64(4)))
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: properties)))
        let json = try String(contentsOf: DisplayPillTests.setupFolder.appendingPathComponent("display.json"),
                              encoding: .utf8)
        await station.deliverSetup([(id: "display", json: json)])
        let deadline = Date().addingTimeInterval(30)
        while model.displayPill.description == nil, Date() < deadline {
            try? await Task.sleep(for: .milliseconds(50))
        }
        #expect(model.displayPill.description != nil)
    }

    /// R-IOS-52, W8: Setup on the iPad on its side, the list beside DSP's
    /// AGC/ALC page with the page held to a readable width, compared with
    /// setup-board.html's chosen iPad frame; then the same router in a
    /// narrow, compact-width window: one list with the page slid in, still
    /// AGC/ALC.
    @Test("Setup on the iPad on its side beside its page, and the same page in a compact window",
          .enabled("The iPad's screens are drawn on an iPad simulator") {
              await MainActor.run { UIDevice.current.userInterfaceIdiom == .pad }
          })
    func setupShots() async throws {
        typealias Described = SetupDescribedPagesTests
        let rig = try await Described.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        await rig.station.deliverSetup(try Described.coreCategories())
        let app = rig.app
        #expect(await Described.settle { app.setupPages.isCurrent && app.setupFeed.description(for: "dsp") != nil })
        let numbers = try SetupWorkspaceShotTests.numberRowIds(app, category: "dsp", page: "dsp.agcAlc")
        let agcAlc = SetupWorkspaceShotTests.agcAlc
        rig.router.path = agcAlc

        let wide = CGSize(width: 1210, height: 834)
        let split = try await shootSetup("ipad-setup-split", rig: rig, size: wide, widthClass: .regular)
        // Each number row is on the right of the list, inside the readable width.
        let left = SetupSplit.listWidth + (wide.width - SetupSplit.listWidth - SetupSplit.readableWidth) / 2
        var measured = 0
        for id in numbers {
            guard let label = split.frame(id, .label), let value = split.frame(id, .value) else {
                continue
            }
            measured += 1
            #expect(label.minX >= left - 1, "\(id): the row is not held to the readable width")
            #expect(value.maxX <= left + SetupSplit.readableWidth + 1, "\(id): the row runs past the readable width")
        }
        #expect(measured > 0, "No number row was laid out beside the list")
        #expect(rig.router.path == agcAlc)

        // The window narrowed to compact: one list, the same page still open.
        let narrow = CGSize(width: 375, height: 834)
        let compact = try await shootSetup("ipad-setup-compact", rig: rig, size: narrow, widthClass: .compact)
        #expect(rig.router.path == agcAlc, "The open page closed when the window narrowed")
        let row = try #require(numbers.lazy.compactMap { compact.frame($0, .label) }.first,
                               "AGC/ALC is not open in the compact window")
        #expect(row.minX < SetupSplit.listWidth, "The compact window still has the list beside the page")
        await app.disconnect()
    }

    /// Draws the Setup tab and the tab bar in a window `size` wide with the
    /// width class `widthClass`, as the app's root lays them out, and returns
    /// where the page's value rows were laid out.
    private func shootSetup(_ name: String, rig: SetupDescribedPagesTests.Rig, size: CGSize,
                            widthClass: UserInterfaceSizeClass) async throws -> SetupLayoutProbe {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        let onItsSide = size.width > size.height
        window.frame = CGRect(origin: CGPoint(x: 0, y: onItsSide ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let probe = SetupLayoutProbe()
        let iPad = IPadLayout.arrangement(horizontal: widthClass, size: size).isIPad
        let root = VStack(spacing: 0) {
            SetupTab(app: rig.app, flow: rig.flow, router: rig.router, buildTag: nil)
            TabBar(selection: .constant(.setup), sideways: onItsSide, iPad: iPad)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .environment(\.horizontalSizeClass, widthClass)
        .environment(\.setupLayoutProbe, probe)
        .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        if onItsSide {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        return probe
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

    private func shoot(_ screen: Screen, model: AppModel) async throws {
        let size = screen.size
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        // On its side the window sits clear of the upright status bar and
        // home indicator, and takes the iPad's own on-its-side safe area.
        window.frame = CGRect(origin: CGPoint(x: 0, y: screen.onItsSide ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = IPadShotRoot(model: model, columnShown: screen.columnShown, columnMenu: screen.columnMenu,
                                displayTab: screen.displayTab)
            .environment(\.horizontalSizeClass, .regular)
            .preferredColorScheme(.dark)
        let host = ShotHosting.host(root)
        if screen.onItsSide {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 24, left: 0, bottom: 20, right: 0)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        // The band asks for its width in pixels: the column's 320 points are
        // the band's while the column hides, and upright it has the whole width.
        let scale = window.screen.scale
        #expect(band.requestedPixels == Int((screen.bandWidth * scale).rounded()))
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        #expect(band.markers.count == 2)

        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(screen.name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}

/// The app's root as `RootView` lays it out on an iPad, the column shown or not.
private struct IPadShotRoot: View {
    @ObservedObject var model: AppModel
    let columnShown: Bool
    var columnMenu: BandColumnMenu?
    var columnPan: PanIdentity = .first
    var displayTab: DisplayPillTab = .spectrum

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, displayTab: displayTab, columnShown: columnShown,
                           columnMenu: columnMenu, columnPan: columnPan)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height, iPad: true)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
