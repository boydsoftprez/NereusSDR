// NereusSDR for iOS: connecting returns the operator to their tab and their Setup, Tools and Radio page
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-05: Connecting returns the operator to their place. J.J. Boyd
// (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
@testable import NereusSDR
import SwiftUI
import Testing

@Suite("SavedPlaces")
@MainActor
struct SavedPlacesTests {
    private func freshSettings() throws -> PhoneSettings {
        PhoneSettings(defaults: try #require(UserDefaults(suiteName: "SavedPlacesTests-\(UUID().uuidString)")))
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            await Task.yield()
            try? await Task.sleep(for: .milliseconds(5))
        }
        return condition()
    }

    // MARK: The stable form

    @Test("every Setup route reads back from its kept form")
    func setupRoundTrip() {
        let routes: [SetupTree.Route] = [.category("CAT & Network"), .page(.batteryAndSessions),
                                         .described(category: "dsp", page: "dsp.agcAlc"),
                                         .cfcBands(category: "dsp", control: "cfc|bands")]
        #expect(SavedPlaces.setupPath(routes.map(SavedPlaces.stored)) == routes)
    }

    @Test("every Tools page reads back, and a Roundtable room falls back to Roundtable's list")
    func toolsRoundTrip() {
        let routes: [[ToolsTab.Page]] = [
            [.spotHub, .spotHubPage(.source(.rbn))], [.spotHub, .spotHubPage(.list)], [.spotHub, .spotHubPage(.display)],
            [.spotHub, .spotHubPage(.identity)], [.performance], [.freedvReporter], [.txEqualizer], [.pureSignal],
            [.diversity], [.cwx], [.catControl, .catControlPage(.channel(3))], [.catControl, .catControlPage(.options)],
            [.catControl, .catControlPage(.ptt)], [.catControl, .catControlPage(.test)], [.tciServer], [.vaxAudio],
            [.supportBundle], [.rotor], [.roundtable],
        ]
        for route in routes {
            #expect(SavedPlaces.toolsRoute(SavedPlaces.stored(route)) == route)
        }
        #expect(SavedPlaces.toolsRoute(SavedPlaces.stored([.roundtable, .roundtableRoom(id: "r1", title: "7.236")]))
            == [.roundtable])
        #expect(SavedPlaces.toolsRoute(SavedPlaces.stored([.roundtableRoom(id: "r1", title: "7.236")])) == [.roundtable])
    }

    @Test("every Radio page reads back from its kept form")
    func radioRoundTrip() {
        let routes: [RadioView.Route] = [.accessory(.page(.rfKit)), .accessory(.faults(.tunerGenius)),
                                         .accessory(.advanced(.powerGenius)), .accessory(.tuneMemory),
                                         .accessory(.rotor), .antennaSetup, .manageRadios, .protocolInfo]
        #expect(SavedPlaces.radioRoute(routes.map(SavedPlaces.stored)) == routes)
    }

    @Test("reading stops at a page this build does not know, leaving the page above it")
    func unknownPageStops() {
        #expect(SavedPlaces.setupPath(["category|General", "page|gone", "page|about"]) == [.category("General")])
        #expect(SavedPlaces.toolsRoute(["spotHub", "spotHubPage|source|gone"]) == [.spotHub])
        #expect(SavedPlaces.radioRoute(["sideways"]) == [])
    }

    // MARK: When a place is kept

    @Test("a change of page is kept while the tabs show, and nothing is kept from any other screen")
    func keptOnlyOnTheBand() throws {
        let settings = try freshSettings()
        let setup = SetupRouter()
        let tools = ToolsRouter()
        let radio = RadioRouter()
        var onBand = true
        RootView.keepPlaces(setupRouter: setup, toolsRouter: tools, radioRouter: radio, settings: settings,
                            onBand: { onBand })
        setup.open(.batteryAndSessions)
        tools.routeChanged([.spotHub])
        radio.routeChanged([.protocolInfo])
        #expect(settings.navSetup == setup.path.map(SavedPlaces.stored))
        #expect(!settings.navSetup.isEmpty)
        #expect(settings.navTools == ["spotHub"])
        #expect(settings.navRadio == ["protocolInfo"])

        onBand = false
        let setupKept = settings.navSetup
        setup.path = []
        tools.routeChanged([])
        radio.routeChanged([])
        #expect(settings.navSetup == setupKept)
        #expect(settings.navTools == ["spotHub"])
        #expect(settings.navRadio == ["protocolInfo"])
    }

    // MARK: Where a place falls back

    private var described: [SetupTree.Category] {
        SetupTree.categories(described: [:], order: [], unreadable: [:])
    }

    @Test("Setup's place waits for the Core's pages, then lands on what remains of it")
    func setupFallsBack() {
        let router = SetupRouter()
        let saved: [SetupTree.Route] = [.category("General"), .described(category: "general", page: "general.gone")]
        router.restore(saved)
        router.applyRestore(tree: described, known: false)
        #expect(router.path == [])
        #expect(router.pendingRestore == saved)
        router.applyRestore(tree: described, known: true)
        #expect(router.path == [.category("General")])
        #expect(router.pendingRestore == nil)
    }

    @Test("a page of the app's own opens at once, before the Core has described anything")
    func nativePageOpensAtOnce() {
        let router = SetupRouter()
        let saved = SetupTree.route(to: .batteryAndSessions)
        router.restore(saved)
        router.applyRestore(tree: described, known: false)
        #expect(router.path == saved)
    }

    @Test("the operator going somewhere first wins over the kept place")
    func operatorWins() {
        let router = SetupRouter()
        router.restore([.category("Hardware")])
        router.open(.about)
        router.applyRestore(tree: described, known: true)
        #expect(router.path == [.page(.about)])
    }

    @Test("a tool the Core does not offer falls back to the list, and an open one stays")
    func toolsFallBack() {
        func entry(_ page: StationToolList.Page, reason: String? = nil) -> StationToolList.Entry {
            StationToolList.Entry(id: "x", title: "x", detail: "x", tag: .core, page: page, reason: reason)
        }
        #expect(ToolsTab.restored([.rotor], entries: [entry(.spotHub)]) == [])
        #expect(ToolsTab.restored([.rotor], entries: [entry(.rotor, reason: "No rotor")]) == [])
        #expect(ToolsTab.restored([.spotHub, .spotHubPage(.list)], entries: [entry(.spotHub)])
            == [.spotHub, .spotHubPage(.list)])
        #expect(ToolsTab.restored([.catControl, .catControlPage(.channel(9))], entries: [entry(.catControl)])
            == [.catControl])
    }

    @Test("a Radio row the Core does not offer falls back to the tab, and an accessory page stays")
    func radioFallsBack() {
        let open = RadioMenu.Entry(id: "x", title: "x", detail: "x", page: .protocolInfo, reason: nil)
        let greyed = RadioMenu.Entry(id: "x", title: "x", detail: "x", page: .manageRadios, reason: "older")
        #expect(RadioView.restored([.protocolInfo], menu: [open]) == [.protocolInfo])
        #expect(RadioView.restored([.manageRadios], menu: [open, greyed]) == [])
        #expect(RadioView.restored([.antennaSetup], menu: [open]) == [])
        #expect(RadioView.restored([.accessory(.rotor)], menu: []) == [.accessory(.rotor)])
    }

    // MARK: The whole way

    @Test("reaching ready through the flow brings Setup's page back, and Your Cores never disturbs it")
    func restoredOnReady() async throws {
        let settings = try freshSettings()
        let saved = SetupTree.route(to: .batteryAndSessions)
        settings.navSetup = saved.map(SavedPlaces.stored)
        settings.navTab = .setup
        settings.navTools = ["performance"]
        settings.navRadio = ["protocolInfo"]
        let app = AppModel(phoneSettings: settings)
        let station = try FakeStation()
        let stations = PairedStationStore(item: InMemorySecretItem())
        try stations.save(PairedStation(identityKey: station.identity.publicKey, label: "Rock",
                                        endpoints: [station.endpoint]))
        let flow = ConnectionFlow(app: app, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()), stations: stations, kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
        let router = SetupRouter()
        let window = UIWindow(frame: CGRect(x: 0, y: 0, width: 402, height: 874))
        let host = ShotHosting.host(RootView(setupRouter: router).environmentObject(app).environmentObject(flow))
        window.rootViewController = host
        window.makeKeyAndVisible()
        defer { window.isHidden = true }
        host.view.layoutIfNeeded()
        // On Your Cores: nothing restored, nothing overwritten.
        #expect(await settle(seconds: 2) { flow.screen == .cores })
        #expect(router.path == [])
        #expect(settings.navSetup == saved.map(SavedPlaces.stored))

        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await settle { router.path == saved })
        #expect(settings.navTab == .setup)
        await flow.disconnect()
        #expect(flow.screen == .cores)
        #expect(settings.navSetup == saved.map(SavedPlaces.stored))
        #expect(settings.navTab == .setup)
    }
}
