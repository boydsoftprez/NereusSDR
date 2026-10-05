// NereusSDR for iOS: long sessions on screen: back after a while locked, the first time on cellular, and the two Setup pages
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-22, R-IOS-23: the real main screen and Setup pages, connected to a
/// fake Core, for comparing with `16-long-sessions.jpg` and
/// `12-audio-and-data.jpg`. With `NEREUS_MAIN_SHOTS` set to a directory
/// (through `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each is written there as a PNG.
@Suite("Long sessions on screen", .serialized)
@MainActor
struct LongSessionShotTests {
    final class Clock {
        var now: Date

        init(_ now: Date) {
            self.now = now
        }
    }

    final class Traffic {
        var totals = TrafficCounter.Totals()
    }

    /// Today at 19:42 on the phone's clock, as picture 16 has it.
    static func evening() -> Date {
        Calendar.current.date(bySettingHour: 19, minute: 42, second: 0, of: Date()) ?? Date()
    }

    @Test("back after a while locked, and the first time on cellular")
    func bandShots() async throws {
        let suite = "LongSessionShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let network = ConnectionFlowTests.FakeNetwork()
        let clock = Clock(Self.evening())
        let traffic = Traffic()
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             sessionSources: SessionController.Sources(
                                 power: ThermalAndPowerWatcher(read: { .steady }), network: network,
                                 traffic: { traffic.totals }, now: { clock.now }, tick: .seconds(3600)))
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle(seconds: 5) { model.main.slices.entries.count == 2 && model.main.coreName != nil })
        #expect(await settle(seconds: 30) { model.roundTripMs != nil })
        network.set(NetworkPath(online: true, interfaces: ["en0"], wifi: true))
        // RootView normally reports the visible Panadapter tab. This
        // picture hosts MainScreen directly, so report that visibility here.
        model.longSession.bandShown(true)

        let band = model.main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))

        // Locked from 19:42 to 20:15 with Sound only: the band stops, then
        // picks up where it is now, and the stretch is marked.
        try await shoot("long-back-after-locked", model: model) {
            BandFlagShotTests.feed(band, width: band.requestedPixels, lines: 900)
            model.longSession.sceneChanged(inForeground: false)
            #expect(!model.main.subscriber.session.subscribes)
            model.longSession.phoneLocked()
            clock.now = clock.now.addingTimeInterval(33 * 60)
            model.longSession.sceneChanged(inForeground: true)
            #expect(band.awayMarks.count == 1)
            BandFlagShotTests.feed(band, width: band.requestedPixels, lines: 180)
        }

        // Off Wi-Fi for the first time: the note and the chip.
        network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        // Test-only measured app payload: 1.2 Mbps incoming, 24 kbps outgoing over five seconds.
        traffic.totals = TrafficCounter.Totals(bytesIn: 750_000, bytesOut: 15_000)
        clock.now.addTimeInterval(5)
        await model.longSession.tick()
        #expect(model.longSession.note?.kind == .cellular)
        #expect(!model.phoneSettings.cellularNoteShown)
        try await shoot("long-first-time-on-cellular", model: model) {
            BandFlagShotTests.feed(band, width: band.requestedPixels, lines: 900)
        }
        try await shoot("long-cellular-large-text", model: model, largeText: true) {}
        try await shoot("long-cellular-landscape", model: model,
                        size: CGSize(width: 874, height: 402)) {}
        #expect(model.phoneSettings.cellularNoteShown)
        model.longSession.bandShown(false)
        await model.disconnect()
    }

    @Test("the Battery and sessions and Data use pages, live")
    func setupShots() async throws {
        let suite = "LongSessionShotTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let traffic = Traffic()
        let network = ConnectionFlowTests.FakeNetwork()
        let settings = PhoneSettings(defaults: defaults)
        let app = AppModel(phoneSettings: settings, displaySettings: BandDisplaySettingsStore(defaults: defaults),
                           sessionSources: SessionController.Sources(
                               power: ThermalAndPowerWatcher(read: { .steady }), network: network,
                               traffic: { traffic.totals }, now: Date.init, tick: .seconds(3600)))
        let flow = ConnectionFlow(app: app, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()),
            stations: PairedStationStore(item: InMemorySecretItem()), kind: .phone,
            transportFactory: WebSocketLinkTransport.factory,
            clock: TestLinkClock(), microphone: ConnectionFlowTests.FakeMicrophone(), network: nil, appMajors: [1],
            now: Date.init, browser: nil))
        // A month on cellular, then this session: 18.6 MB so far, on cellular now.
        network.set(NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true))
        let meter = app.longSession.meter
        meter.sessionStarted()
        traffic.totals = TrafficCounter.Totals(bytesIn: 1_221_400_000, bytesOut: 0)
        meter.sample(cellular: true)
        meter.sessionStarted()
        traffic.totals.bytesIn += 17_900_000
        traffic.totals.bytesOut += 700_000
        meter.sample(cellular: true)
        #expect(DataUsePage.sessionText(meter) == "18.6 MB total")
        #expect(DataUseMeter.text(meter.monthCellular.total) == "1.24 GB")

        let router = SetupRouter()
        for (name, path) in [("long-setup-battery-and-sessions",
                              [SetupTree.Route.category("General"), .page(.batteryAndSessions)]),
                             ("long-setup-data-use", [SetupTree.Route.category("CAT & Network"), .page(.dataUse)])] {
            try await shootSetup(name, app: app, flow: flow, router: router, path: path, lower: false)
            try await shootSetup("\(name)-lower", app: app, flow: flow, router: router, path: path, lower: true)
            if name == "long-setup-data-use" {
                try await shootSetup("\(name)-lower-light", app: app, flow: flow, router: router,
                                     path: path, lower: true, colour: .light)
            }
            try await shootSetup("\(name)-large-text", app: app, flow: flow, router: router,
                                 path: path, lower: true, largeText: true)
            try await shootSetup("\(name)-landscape", app: app, flow: flow, router: router,
                                 path: path, lower: true, size: CGSize(width: 874, height: 402))
        }
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

    private func shoot(_ name: String, model: AppModel, size: CGSize = CGSize(width: 402, height: 874),
                       largeText: Bool = false, prepare: () -> Void) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            MainScreen(app: model, main: model.main)
            TabBar(selection: .constant(.panadapter), sideways: false)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .preferredColorScheme(.dark)
        .environment(\.dynamicTypeSize, largeText ? .accessibility1 : .large)
        let host = UIHostingController(rootView: root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        // The first draw sizes the waterfall; the frames then fill it.
        let band = model.main.band
        let draw = try await ShotWait.requireBandLaidOut(band, in: window)
        prepare()
        try await ShotWait.requireBandShown(band, in: window, after: draw)
        await ShotWait.laidOut(window)
        write(name, window: window)
    }

    private func shootSetup(_ name: String, app: AppModel, flow: ConnectionFlow, router: SetupRouter,
                            path: [SetupTree.Route], lower: Bool,
                            size: CGSize = CGSize(width: 402, height: 874), largeText: Bool = false,
                            colour: ColorScheme = .dark) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        router.path = path
        let root = VStack(spacing: 0) {
            SetupTab(app: app, flow: flow, router: router, buildTag: nil)
            TabBar(selection: .constant(.setup), sideways: false)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .preferredColorScheme(colour)
        .environment(\.dynamicTypeSize, largeText ? .accessibility1 : .large)
        let host = UIHostingController(rootView: root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        if lower {
            for _ in 0..<3 {
                host.view.layoutIfNeeded()
                let scrolls = scrollViews(in: host.view)
                let scroll = try #require(scrolls.max { first, second in
                    first.contentSize.height - first.bounds.height < second.contentSize.height - second.bounds.height
                })
                // Lists materialise later rows as the first offset moves.
                let bottom = max(0, scroll.contentSize.height - scroll.bounds.height
                                 + scroll.adjustedContentInset.bottom)
                scroll.setContentOffset(CGPoint(x: 0, y: bottom), animated: false)
                await ShotWait.laidOut(window)
            }
        }
        write(name, window: window)
    }

    private func scrollViews(in view: UIView) -> [UIScrollView] {
        let here = (view as? UIScrollView).map { [$0] } ?? []
        return here + view.subviews.flatMap { scrollViews(in: $0) }
    }

    private func write(_ name: String, window: UIWindow) {
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try? data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}
