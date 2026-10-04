// NereusSDR for iOS: pictures of the Tools tab and the Core's tool pages, for comparing with the board
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

/// Spec section 5.2 items 3 to 5 and `05-tabs.jpg`: the Tools tab and each
/// station tool page drawn as the app draws them, against a fake Core with a
/// TCI server, PureSignal status, a support bundle and its log, upright and
/// on a phone turned sideways. With `NEREUS_MAIN_SHOTS` set, each picture is
/// written there.
@Suite("Tools pages pictures", .serialized)
@MainActor
struct ToolsPagesShotTests {
    private let platform = TestPlatform()

    @Test("the Tools tab and every station tool page draw, with their controls and reasons")
    func pictures() async throws {
        let defaults = try #require(UserDefaults(suiteName: "ToolsPagesShotTests-\(UUID().uuidString)"))
        let phone = PhoneSettings(defaults: defaults)
        phone.setString("123.5,-3.0", for: DiversityModel.memoryKey(band: 5, 0))
        phone.setString("45.0,2.5", for: DiversityModel.memoryKey(band: 5, 3))
        let model = AppModel(phoneSettings: phone, displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let station = try FakeStation(additions: [.spots, .stationTci, .supportBundle, .vax, .diversityPattern,
                                                  .logCategoryList])
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected && model.mirror.isSnapshotComplete })
        let catalogue = try #require(ModesTabBindingTests.catalogueJSON("catalog-anan-g2"))
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(catalogue)),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await settle { model.main.catalogFeed.revision == 2 })
        try await station.deliverStationTci(.board)
        try await station.deliverVax(.board)
        #expect(await settle { model.mirror.object(StationVax.objectKey) != nil })
        model.records.want(StationTciClient.streamName, backlog: StationTciClient.capacity)
        #expect(await settle { model.records.records(StationTciClient.streamName).count == 2 })
        model.records.want(CoreLogLine.streamName, backlog: CoreLogLine.capacity)
        #expect(await settle { model.records.records(CoreLogLine.streamName).count == FakeStation.coreLogLines.count })
        _ = try await model.commands.invoke(CoreLogLine.setCategoriesVerb, arguments: [
            CommandArgument(name: "categories", value: .text("nereus.discovery,nereus.connection,nereus.tci")),
        ], timeout: .seconds(10))
        #expect(await settle { model.mirror.object("radio")?["logCategories"] != .text("") })
        await station.deliver(.delta(LinkMessage.Delta(key: "pureSignal", properties: [
            .init(ordinal: 3, name: "statusJson", value: .utf8("""
            {"schema":1,"psEnabled":true,"mox":false,"engineState":0,"feedbackLevel":152,\
            "successfulCalibrations":9,"attemptedCalibrations":11,"correctionsApplied":true}
            """)),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "pureSignalSettings", properties: [
            .init(ordinal: 0, name: "autoCalEnabled", value: .bool(true)),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: try ordinal("SliceModel", "diversityEnabled"), name: "diversityEnabled", value: .bool(true)),
            .init(ordinal: try ordinal("SliceModel", "diversityPhaseDeg"), name: "diversityPhaseDeg", value: .f64(123.5)),
            .init(ordinal: try ordinal("SliceModel", "diversityGainDb"), name: "diversityGainDb", value: .f64(-3)),
        ])))
        #expect(await settle { model.main.transmit.psa })
        await model.supportBundle.collect()
        #expect(model.supportBundle.files.count == 2)
        let flow = ConnectionFlow(app: model, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()),
            stations: PairedStationStore(item: InMemorySecretItem()), kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
        // VAX Audio is ready once the Core's meters it asked for on opening are here.
        let vaxReady: ([ToolsTab.Page]) -> Bool = { route in
            route.last != .vaxAudio || !model.records.records(StationVax.levelsStream).isEmpty
        }
        let pages: [(String, [ToolsTab.Page], CGFloat)] = [
            ("tools-tab", [], 874),
            ("tools-tx-equalizer", [.txEqualizer], 1500),
            ("tools-puresignal", [.pureSignal], 874),
            ("tools-diversity", [.diversity], 1250),
            ("tools-tci-server", [.tciServer], 1250),
            ("tools-vax-audio", [.vaxAudio], 1500),
            ("tools-support-bundle", [.supportBundle], 1500),
        ]
        for (name, route, height) in pages {
            try await shoot(name, height: height, ready: { vaxReady(route) }) {
                ToolsTab(app: model, flow: flow, spots: model.spots, route: route)
            }
        }
        // Sideways: every page on the Tools tab, as a phone turned on its side shows it.
        let sideways: [(String, [ToolsTab.Page])] = [
            ("tools-tab-landscape", []),
            ("tools-spot-hub-landscape", [.spotHub]),
            ("tools-freedv-reporter-landscape", [.freedvReporter]),
            ("tools-tx-equalizer-landscape", [.txEqualizer]),
            ("tools-puresignal-landscape", [.pureSignal]),
            ("tools-diversity-landscape", [.diversity]),
            ("tools-tci-server-landscape", [.tciServer]),
            ("tools-vax-audio-landscape", [.vaxAudio]),
            ("tools-connection-performance-landscape", [.performance]),
            ("tools-support-bundle-landscape", [.supportBundle]),
        ]
        for (name, route) in sideways {
            try await shoot(name, sideways: true, ready: { vaxReady(route) }) {
                ToolsTab(app: model, flow: flow, spots: model.spots, route: route)
            }
        }
        // On the air the Core will not disconnect a TCI client: Disconnect greys with the reason, dark and light.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: try ordinal("RadioModel", "transmitting"), name: "transmitting", value: .bool(true)),
        ])))
        #expect(await settle { model.mirror.object("radio")?["transmitting"] == .bool(true) })
        for (name, scheme) in [("tools-tci-server-on-air", ColorScheme.dark),
                               ("tools-tci-server-on-air-light", ColorScheme.light)] {
            try await shoot(name, height: 1250, scheme: scheme) {
                ToolsTab(app: model, flow: flow, spots: model.spots, route: [.tciServer])
            }
        }
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: try ordinal("RadioModel", "transmitting"), name: "transmitting", value: .bool(false)),
        ])))
        #expect(await settle { model.mirror.object("radio")?["transmitting"] == .bool(false) })
        // With the Core away, the Core's tools grey with the reason.
        await model.disconnect()
        try await shoot("tools-tab-no-core") {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [])
        }
        try await shoot("tools-tx-equalizer-no-core", height: 1500) {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [.txEqualizer])
        }
        try await shoot("tools-vax-audio-no-core", height: 1500) {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [.vaxAudio])
        }
    }

    @Test("a greyed accessory choice and a greyed logging category draw their chosen one in grey, never blue")
    func greyedChosenPictures() async throws {
        let rows = VStack(alignment: .leading, spacing: 10) {
            ForEach([true, false], id: \.self) { enabled in
                Text(enabled ? "Can be pressed" : "Greyed")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(ChromeColours.caption)
                HStack(spacing: 4) {
                    AccessoryChrome.ChoiceButton(label: "ANT 1", detail: "Beam", lit: true, enabled: enabled) {}
                    AccessoryChrome.ChoiceButton(label: "ANT 2", detail: "Dipole", lit: false, enabled: enabled) {}
                    AccessoryChrome.ChoiceButton(label: "ANT 3", detail: "", lit: false, enabled: enabled) {}
                }
                HStack(spacing: 5) {
                    SupportBundlePage.CategoryButton(title: "Discovery", isOn: true, enabled: enabled,
                                                     identifier: "shot.on") {}
                    SupportBundlePage.CategoryButton(title: "Connection", isOn: false, enabled: enabled,
                                                     identifier: "shot.off") {}
                    SupportBundlePage.CategoryButton(title: "TCI", isOn: true, enabled: enabled,
                                                     identifier: "shot.tci") {}
                }
            }
        }
        .padding(12)
        try await shoot("greyed-chosen-choices", height: 330) {
            rows.frame(maxHeight: .infinity, alignment: .top)
        }
    }

    private func ordinal(_ className: String, _ property: String) throws -> UInt16 {
        try #require(try FakeStation.schema(ofClass: className).fields.first { $0.name == property }?.ordinal)
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(20))
        }
        return condition()
    }

    /// A Tools page over the tab bar on an upright phone, or on one turned
    /// sideways with its own safe area, written with `NEREUS_MAIN_SHOTS` set.
    private func shoot<Content: View>(_ name: String, height: CGFloat = 874, sideways: Bool = false,
                                      scheme: ColorScheme = .dark, ready: () -> Bool = { true },
                                      @ViewBuilder content: () -> Content) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: height)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        if sideways {
            // Clear of the upright status bar and home indicator, as the connection flow's sideways shots sit.
            window.frame.origin.y = 120
        }
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            content()
            TabBar(selection: .constant(.tools), sideways: sideways)
        }
        .background(ChromeColours.page)
        .preferredColorScheme(scheme)
        let host = UIHostingController(rootView: root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        if sideways {
            // A sideways phone's safe area: the sensor housing and the home
            // indicator, and nothing at the top. The test window sits in an
            // upright scene, so its own status bar and home indicator insets
            // are taken back out; left in, they leave a blank band above the
            // page and soften the top of a scroll view under them.
            let upright = window.safeAreaInsets
            host.additionalSafeAreaInsets = UIEdgeInsets(top: -upright.top, left: 62 - upright.left,
                                                         bottom: 21 - upright.bottom, right: 62 - upright.right)
        }
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        // A page that asks the Core for something when it opens (the VAX meters) waits for the answer.
        #expect(await settle(ready), "\(name) is ready")
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
