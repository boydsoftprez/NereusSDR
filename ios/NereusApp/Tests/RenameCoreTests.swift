// NereusSDR for iOS: renaming a Core from Your Cores against the fake Core: accepted, refused, too old, and not connected
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// D76, R-IOS-08, D23: Rename from a Core's row, driven through
/// ``ConnectionFlow`` against ``FakeStation`` keeping a name as the Core
/// does. Keys are made at run time; nothing dials a real Core.
@Suite("Renaming a Core", .serialized)
@MainActor
struct RenameCoreTests {
    struct Harness {
        let app: AppModel
        let flow: ConnectionFlow
        let stations: PairedStationStore
        let settings: PhoneSettings
        let reach: ConnectionFlowTests.Reach
    }

    private static func harness(_ station: FakeStation, label: String = "",
                                defaults suite: String = "RenameCoreTests-\(UUID().uuidString)",
                                stations: PairedStationStore? = nil,
                                service: FakeRemoteAccess? = nil) throws -> Harness {
        let defaults = try #require(UserDefaults(suiteName: suite))
        let settings = PhoneSettings(defaults: defaults)
        let app = AppModel(phoneSettings: settings)
        let store = stations ?? PairedStationStore(item: InMemorySecretItem())
        if stations == nil {
            try store.save(PairedStation(identityKey: station.identity.publicKey, label: label,
                                         endpoints: [station.endpoint]))
        }
        let reach = ConnectionFlowTests.Reach()
        let factory: LinkTransportFactory = { endpoint, trust in
            switch reach.way {
            case .reaches:
                return station.transportFactory(endpoint, trust)
            case .fails:
                return ConnectionFlowTests.DeadTransport(error: .failed("waiting: no network"))
            case .hangs:
                return ConnectionFlowTests.HangingTransport()
            }
        }
        var dependencies = ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()), stations: store, kind: .phone,
            transportFactory: factory, clock: TestLinkClock(), microphone: ConnectionFlowTests.FakeMicrophone(),
            network: ConnectionFlowTests.FakeNetwork(), appMajors: [1], now: Date.init, browser: nil)
        dependencies.serviceRoute = service?.maker
        let flow = ConnectionFlow(app: app, dependencies: dependencies)
        return Harness(app: app, flow: flow, stations: store, settings: settings, reach: reach)
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

    private static func renames(_ station: FakeStation) -> [String] {
        station.messages.compactMap { message in
            guard case .commandInvoke(let invoke) = message, invoke.verb == FakeStation.renameVerb,
                  case .utf8(let label)? = invoke.args.first?.value else {
                return nil
            }
            return label
        }
    }

    // MARK: A Core that isn't connected

    @Test("a Core with no name shows its address, and its sheet opens empty")
    func noNameShowsAddress() throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "")
        let harness = try Self.harness(station)
        let row = try #require(harness.flow.cores.first)
        #expect(row.label == station.endpoint.host)
        #expect(harness.flow.renameAvailability(row) == .available)
        harness.flow.startRename(row)
        #expect(harness.flow.renaming == row.id)
        #expect(harness.flow.renameText == "")
    }

    @Test("renaming a Core that isn't connected signs in, renames, closes, and leaves the phone as it was")
    func renameSignsInFirst() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "")
        let harness = try Self.harness(station)
        let flow = harness.flow
        #expect(flow.screen == .cores)
        flow.startRename(try #require(flow.cores.first))
        flow.renameText = "  KG4VCF/shack "
        await flow.saveRename()
        #expect(flow.renameProblem == nil)
        #expect(flow.renaming == nil, "accepted, the sheet closes")
        #expect(Self.renames(station) == ["KG4VCF/shack"])
        // The row and the stored Core show the name the Core reported.
        #expect(flow.cores.first?.label == "KG4VCF/shack")
        #expect(try harness.stations.all().first?.label == "KG4VCF/shack")
        #expect(station.stationLabel == "KG4VCF/shack")
        // One sign-in with the device key, closed after the rename: no band, no session.
        #expect(station.connectionCount == 1)
        let signIn = station.messages.compactMap { message -> LinkMessage.AuthRequest? in
            if case .authRequest(let request) = message {
                return request
            }
            return nil
        }
        #expect(signIn.count == 1)
        #expect(signIn.first?.device != nil)
        #expect(flow.screen == .cores)
        #expect(harness.app.session == nil)
        #expect(harness.app.connection == .notConnected)
        #expect(!flow.renameBusy)

        // The name is kept: a later connection shows it on the band's toolbar too.
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await settle { harness.app.main.coreName == "KG4VCF/shack" })
        await harness.app.disconnect()
    }

    @Test("Rename reaches the service after a saved address no longer answers")
    func renameAfterStaleAddress() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "")
        let store = PairedStationStore(item: InMemorySecretItem())
        let stale = StationEndpoint(host: "192.0.2.10")
        try store.save(PairedStation(identityKey: station.identity.publicKey, label: "KG4VCF/attic",
                                     endpoints: [stale]))
        let service = FakeRemoteAccess(.reaches(station))
        let harness = try Self.harness(station, stations: store, service: service)
        harness.reach.way = .fails
        let flow = harness.flow
        flow.startRename(try #require(flow.cores.first))
        flow.renameText = "KG4VCF/shack"
        await flow.saveRename()
        #expect(service.dials == 1)
        #expect(Self.renames(station) == ["KG4VCF/shack"])
        #expect(flow.cores.first?.label == "KG4VCF/shack")
        #expect(flow.trouble == nil)
        #expect(flow.renaming == nil)
    }

    @Test("a refused name keeps the sheet open with the Core's words, and nothing is stored")
    func refusedName() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "")
        let service = FakeRemoteAccess(.reaches(station))
        let harness = try Self.harness(station, service: service)
        let flow = harness.flow
        flow.startRename(try #require(flow.cores.first))
        flow.renameText = "KG4VCF/shack!"
        await flow.saveRename()
        #expect(flow.renaming != nil)
        #expect(flow.renameProblem == FakeStation.renameRuleReason)
        #expect(flow.renameText == "KG4VCF/shack!")
        #expect(try harness.stations.all().first?.label == "")
        #expect(flow.cores.first?.label == station.endpoint.host)
        #expect(station.stationLabel == "")
        #expect(harness.app.session == nil)
        #expect(service.dials == 0, "a refused rename is not sent again through the service")

        // Any refusal the Core words, as sent.
        station.refuseNext(FakeStation.renameVerb, reason: "The Core could not save its new name. Try again.")
        flow.renameText = "KG4VCF"
        await flow.saveRename()
        #expect(flow.renameProblem == "The Core could not save its new name. Try again.")
        #expect(try harness.stations.all().first?.label == "")
    }

    @Test("a Core without deviceAdminVersion 1 needs a newer Core, and its Rename is greyed after")
    func olderCoreGreyed() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: nil,
                                      withoutCapabilities: ["deviceAdminVersion"])
        let suite = "RenameCoreTests-\(UUID().uuidString)"
        let harness = try Self.harness(station, label: "KG4VCF/attic", defaults: suite)
        let flow = harness.flow
        let row = try #require(flow.cores.first)
        #expect(flow.renameAvailability(row) == .available, "not known yet")
        flow.startRename(row)
        #expect(flow.renameText == "KG4VCF/attic")
        flow.renameText = "KG4VCF/shack"
        await flow.saveRename()
        #expect(flow.renameProblem == ConnectionFlow.needsNewerCoreText)
        #expect(Self.renames(station).isEmpty, "never sent to a Core that hasn't advertised it")
        await flow.cancelRename()
        #expect(flow.renameAvailability(try #require(flow.cores.first)) == .needsNewerCore)
        #expect(ConnectionFlow.renameReason(.needsNewerCore) == "Needs a newer Core")
        flow.startRename(try #require(flow.cores.first))
        #expect(flow.renaming == nil, "a greyed Rename opens nothing")

        // Kept on the phone: the next launch greys it too.
        let again = try Self.harness(station, defaults: suite, stations: harness.stations)
        #expect(again.flow.renameAvailability(try #require(again.flow.cores.first)) == .needsNewerCore)
    }

    @Test("connected to an older Core, its Rename is greyed and stays greyed after leaving")
    func connectedRecordsSupport() async throws {
        let older = try FakeStation(fixture: "session-device-sign-in", withoutCapabilities: ["deviceAdminVersion"])
        let harness = try Self.harness(older)
        await harness.flow.connect(to: try #require(harness.flow.cores.first))
        #expect(await settle { harness.flow.screen == .band })
        let row = try #require(harness.flow.cores.first)
        #expect(harness.flow.renameAvailability(row) == .needsNewerCore)
        await harness.flow.leaveBand()
        #expect(harness.flow.renameAvailability(try #require(harness.flow.cores.first)) == .needsNewerCore)
    }

    @Test("a Core that isn't answering shows its trouble sheet, and Try again renames it")
    func notAnsweringThenTryAgain() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "")
        let harness = try Self.harness(station)
        let flow = harness.flow
        harness.reach.way = .fails
        flow.startRename(try #require(flow.cores.first))
        flow.renameText = "KG4VCF/shack"
        await flow.saveRename()
        guard case .notAnswering(let core, let tried, false, nil)? = flow.trouble else {
            Issue.record("no Core-not-answering sheet: \(String(describing: flow.trouble))")
            return
        }
        #expect(core == station.endpoint.host)
        #expect(tried == ConnectionFlowTests.directNoReply)
        #expect(flow.renaming == nil)
        harness.reach.way = .reaches
        await flow.tryAgain()
        #expect(flow.trouble == nil)
        #expect(flow.renaming == nil)
        #expect(flow.cores.first?.label == "KG4VCF/shack")
        #expect(flow.screen == .cores)
        #expect(harness.app.session == nil)
    }

    @Test("Cancel while signing in ends the sign-in and renames nothing")
    func cancelWhileSigningIn() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "")
        let service = FakeRemoteAccess(.reaches(station))
        let harness = try Self.harness(station, service: service)
        let flow = harness.flow
        harness.reach.way = .hangs
        flow.startRename(try #require(flow.cores.first))
        flow.renameText = "KG4VCF/shack"
        let saving = Task { await flow.saveRename() }
        #expect(await settle { flow.renameBusy })
        #expect(flow.renameAvailability(try #require(flow.cores.first)) == .busy)
        await flow.cancelRename()
        await saving.value
        #expect(flow.renaming == nil)
        #expect(!flow.renameBusy)
        #expect(flow.trouble == nil)
        #expect(try harness.stations.all().first?.label == "")
        #expect(service.dials == 0)
    }

    @Test("a Core that removed this phone refuses the sign-in: the notice, and Pair again")
    func signInRefused() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true, stationLabel: "")
        let service = FakeRemoteAccess(.reaches(station))
        let harness = try Self.harness(station, label: "KG4VCF/attic", service: service)
        let flow = harness.flow
        flow.startRename(try #require(flow.cores.first))
        flow.renameText = "KG4VCF/shack"
        await flow.saveRename()
        #expect(flow.renaming == nil)
        #expect(flow.notice == .removed(core: "KG4VCF/attic"))
        let row = try #require(flow.cores.first)
        #expect(row.needsPairing)
        #expect(flow.renameAvailability(row) == .needsPairing)
        #expect(Self.renames(station).isEmpty)
        #expect(service.dials == 0, "a refused sign-in is not tried through the service")
    }

    // MARK: The connected Core

    @Test("the connected Core is renamed over its own session, and the toolbar shows the name at once")
    func renameConnected() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "")
        let harness = try Self.harness(station)
        let flow = harness.flow
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await station.waitUntilLive())
        #expect(harness.app.main.coreName == nil)
        let row = try #require(flow.cores.first)
        #expect(flow.renameAvailability(row) == .available)
        flow.startRename(row)
        flow.renameText = "KG4VCF/shack"
        await flow.saveRename()
        #expect(flow.renaming == nil)
        #expect(station.connectionCount == 1, "no second sign-in")
        #expect(await settle { harness.app.main.coreName == "KG4VCF/shack" })
        #expect(flow.cores.first?.label == "KG4VCF/shack")
        #expect(try harness.stations.all().first?.label == "KG4VCF/shack")
        #expect(await settle { harness.app.mirror.object("devices")?["stationLabel"] == .text("KG4VCF/shack") })
        #expect(harness.app.main.coreName == "KG4VCF/shack")
        #expect(flow.screen == .band)
        await harness.app.disconnect()
    }

    @Test("a name given on another device reaches the row and the stored Core")
    func renamedElsewhere() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "KG4VCF/shack")
        let harness = try Self.harness(station)
        let flow = harness.flow
        #expect(flow.cores.first?.label == station.endpoint.host)
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await settle { flow.cores.first?.label == "KG4VCF/shack" })
        await station.deliver(.delta(LinkMessage.Delta(key: "devices", properties: [
            .init(ordinal: 2, name: "stationLabel", value: .utf8("KG4VCF/attic")),
        ])))
        #expect(await settle { flow.cores.first?.label == "KG4VCF/attic" })
        #expect(try harness.stations.all().first?.label == "KG4VCF/attic")
        // A Core that loses its name keeps the one the phone knows.
        await station.deliver(.delta(LinkMessage.Delta(key: "devices", properties: [
            .init(ordinal: 2, name: "stationLabel", value: .utf8("")),
        ])))
        #expect(await settle { harness.app.main.coreName == nil })
        #expect(flow.cores.first?.label == "KG4VCF/attic")
        await harness.app.disconnect()
    }

    @Test("Rename waits while a connection is being made")
    func busyWhileConnecting() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "")
        let harness = try Self.harness(station)
        harness.reach.way = .hangs
        let row = try #require(harness.flow.cores.first)
        let connecting = Task { await harness.flow.connect(to: row) }
        #expect(await settle { harness.flow.connectingTo != nil })
        #expect(harness.flow.renameAvailability(row) == .busy)
        harness.flow.startRename(row)
        #expect(harness.flow.renaming == nil)
        await harness.flow.cancelConnecting()
        await connecting.value
        #expect(harness.flow.renameAvailability(row) == .available)
    }
}
