// NereusSDR for iOS: every connecting screen and trouble screen on screen, for comparing with the board's pictures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-16, R-IOS-08: the real connecting screens, through the app's
/// root, each driven into its state against ``FakeStation`` and hosted in
/// a window on the simulator. With `NEREUS_CONNECT_SHOTS` set to a
/// directory (through `TEST_RUNNER_NEREUS_CONNECT_SHOTS`), each screen is
/// written there as a PNG for comparing with `06-first-launch.jpg`,
/// `07-connecting.jpg`, `10-trouble.jpg` and
/// `24-pairing-and-connecting-states.jpg`. Codes are the fake's, made at
/// run time.
@Suite("Connecting screens on screen", .serialized)
@MainActor
struct ConnectionFlowShotTests {
    private final class Microphone: MicrophoneAccess {
        var needsAsking = true

        func ask() async -> Bool {
            needsAsking = false
            return true
        }
    }

    private final class Network: NetworkWatch {
        var changed: (@MainActor (NetworkPath) -> Void)?

        func start(_ changed: @escaping @MainActor (NetworkPath) -> Void) {
            self.changed = changed
        }
    }

    private struct DeadTransport: LinkTransport {
        let error: LinkTransportError

        func open(onEvent: @escaping @Sendable (LinkTransportEvent) async -> Void) async throws -> Data {
            throw error
        }

        @discardableResult func send(_ text: String) -> Bool { false }
        func ping() {}
        func close() {}
    }

    private struct Setup {
        let app: AppModel
        let flow: ConnectionFlow
        let clock: TestLinkClock
        let network: Network
    }

    private func setup(factory: @escaping LinkTransportFactory, paired: [PairedStation] = [],
                       keyItem: InMemorySecretItem = InMemorySecretItem(), appMajors: [UInt16] = [1],
                       browser: FakeStationBrowser? = nil, service: FakeRemoteAccess? = nil,
                       rendezvous: FakeStation? = nil) throws -> Setup {
        let defaults = try #require(UserDefaults(suiteName: "ConnectionFlowShotTests-\(UUID().uuidString)"))
        let displayDefaults = try #require(UserDefaults(suiteName: "ConnectionFlowShotTests-display"))
        let app = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                           displaySettings: BandDisplaySettingsStore(defaults: displayDefaults))
        let stations = PairedStationStore(item: InMemorySecretItem())
        for station in paired {
            try stations.save(station)
        }
        let clock = TestLinkClock()
        let network = Network()
        var dependencies = ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: keyItem), stations: stations, kind: .phone, transportFactory: factory,
            clock: clock, microphone: Microphone(), network: network, appMajors: appMajors, now: Date.init,
            browser: browser)
        dependencies.serviceRoute = service?.maker
        if let rendezvous {
            // A mailbox pairing holds its code's number in PairingClient's
            // process-wide gate; a number no other test's pairing holds keeps
            // parallel suites from refusing each other as already pairing.
            rendezvous.showCodesOnMailboxNumbers()
            dependencies.rendezvousTransportFactory = rendezvous.rendezvousTransportFactory
        }
        let flow = ConnectionFlow(app: app, dependencies: dependencies)
        return Setup(app: app, flow: flow, clock: clock, network: network)
    }

    /// The Cores the board lists: the fake as KG4VCF/shack, and two more.
    private func boardCores(_ station: FakeStation) throws -> [PairedStation] {
        let attic = try FakeStation(fixture: "session-device-sign-in")
        let field = try FakeStation(fixture: "session-device-sign-in")
        return [
            PairedStation(identityKey: station.identity.publicKey, label: "KG4VCF/shack",
                          endpoints: [StationEndpoint(host: "2001:db8:5a1:e8f0:dea6:32ff:fe12:3456")]),
            PairedStation(identityKey: attic.identity.publicKey, label: "KG4VCF/attic",
                          endpoints: [StationEndpoint(host: "192.0.2.40")]),
            PairedStation(identityKey: field.identity.publicKey, label: "KG4VCF/field",
                          endpoints: [StationEndpoint(host: "field.example", port: 50055)]),
        ]
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            await Task.yield()
            try? await Task.sleep(for: .milliseconds(10))
        }
        return condition()
    }

    /// A code the fake does not hold: its words on a number no fake Core
    /// shows and no other pairing in the process holds.
    private static func wrongCode(_ code: String) -> String {
        let parts = code.split(separator: "-")
        return "\(FakeStation.unshownNameplate())-\(parts[1])-\(parts[2])"
    }

    // MARK: The shots

    @Test("the fifth-device question and taken-over result follow the several-devices boards")
    func fifthDeviceScreens() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let setup = try self.setup(factory: station.transportFactory, paired: try boardCores(station))
        let flow = setup.flow
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        let session = try #require(setup.app.session)
        func device(_ id: String, _ name: String, _ short: String,
                    _ state: LinkMessage.SessionHeld.Device.State, replaceable: Bool = true,
                    seconds: Int64 = 180) -> LinkMessage.SessionHeld.Device {
            let slice = LinkMessage.SessionHeld.Slice(sliceId: 1, letter: "B", frequencyHz: 7_249_000,
                                                       mode: 0, band: 3)
            return .init(deviceId: id, name: name, shortName: short, kind: "computer", state: state,
                         from: "relay", replaceable: replaceable, holdsTransmit: state == .transmitting,
                         lastActivitySeconds: seconds, connectedForSeconds: 7_200,
                         awayForSeconds: state == .away ? 65 : 0,
                         transmittingForSeconds: state == .transmitting ? 47 : 0,
                         listeningOn: [slice], transmittingOn: state == .transmitting ? slice : nil)
        }
        let roster = [device("host", "Core desktop", "Desktop", .listening, replaceable: false),
                      device("ipad", "iPad Pro", "iPad", .away, seconds: 720),
                      device("mac", "MacBook Pro", "MacBook", .transmitting, seconds: 0),
                      device("phone", "iPhone SE", "iPhone", .listening, seconds: 300)]
        flow.show(.cores)
        flow.presentHeld(.init(devices: roster, revision: 5), from: session)
        #expect(flow.heldChoiceID == "ipad")
        try await shoot("46-fifth-device-away", setup)
        try await shoot("47-fifth-device-away-landscape", setup, sideways: true)
        flow.selectHeldDevice("mac")
        try await shoot("48-fifth-device-on-air-red", setup)
        flow.presentHeld(.init(devices: roster, revision: 6,
                               placeFreed: .init(secondsAgo: 15)), from: session)
        try await shoot("49-place-freed-full-core", setup)
        flow.clearHeld(from: session)
        await station.deliver(.sessionEnd(.init(reason: "MacBook Pro took this device's place on the Core.",
                                                 retryable: false, code: "takenOver", takenOverBy: "MacBook Pro",
                                                 takenOverById: "mac", secondsAgo: 45)))
        #expect(await settle {
            if case .placeTaken = flow.notice { return true }
            return false
        })
        try await shoot("50-place-taken", setup)
        try await shoot("51-place-taken-landscape", setup, sideways: true)
    }

    @Test("first launch and pairing: welcome, set up, Your Cores, an address, the code, a wrong code, pairing closed, the microphone")
    func firstLaunchAndPairing() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let setup = try setup(factory: station.transportFactory)
        let flow = setup.flow
        try await shoot("01-welcome", setup)
        flow.show(.setUpCore)
        try await shoot("02-set-up-a-core", setup)
        flow.show(.welcome)
        flow.findMyCore()
        try await shoot("03-your-cores-none-yet", setup)
        flow.enterAddress()
        flow.addressText = "2001:db8:5a1:e8f0:dea6:32ff:fe12:3456"
        try await shoot("04-enter-an-address", setup)
        flow.portText = "0"
        await flow.connectToTypedAddress()
        try await shoot("05-enter-an-address-bad-port", setup)
        flow.portText = "47910"
        await flow.connectToTypedAddress()
        flow.codeText = station.pairingCode
        try await shoot("06-pair-with-a-code-after-an-address", setup)
        flow.codeText = "7 \(PairingCodeText.words[0]) \(PairingCodeText.words[5])x"
        await flow.pair()
        try await shoot("07-pair-with-a-code-typing-slip", setup)
        flow.codeText = Self.wrongCode(station.pairingCode)
        await flow.pair()
        #expect(await settle { if case .wrongCode = flow.pairingProblem { return true }; return false })
        try await shoot("08-a-wrong-code", setup)
        for _ in 2...5 {
            station.endPairingWait()
            await setup.clock.advance(by: 60_000)
            flow.codeText = Self.wrongCode(station.pairingCode)
            await flow.pair()
        }
        #expect(flow.pairingProblem == .pairingClosed)
        try await shoot("09-pairing-closed", setup)
        station.openPairing()
        flow.clearPairingProblem()
        flow.codeText = station.pairingCode
        await flow.pair()
        #expect(flow.screen == .microphone)
        try await shoot("10-paired-the-microphone-question", setup)
        await flow.answerMicrophone(allow: false)
        await setup.app.disconnect()
    }

    @Test("finding a Core: looking on this Wi-Fi, not allowed to look, Found it, a refused one tap, and Your Cores on this network")
    func findingACore() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", requiresPairing: true)
        let browser = FakeStationBrowser()
        let setup = try setup(factory: station.transportFactory, browser: browser)
        let flow = setup.flow
        flow.findMyCore()
        #expect(await settle { flow.screen == .findCore })
        try await shoot("27-looking-on-this-wifi", setup)
        await browser.announce([], localNetworkDenied: true)
        #expect(await settle { flow.lookingDenied })
        try await shoot("28-not-allowed-to-look", setup)
        let found = FoundStation(instanceName: "NereusSDR Core", label: "Unclaimed Core",
                                 identityPrefix: station.foundStation.identityPrefix, claimed: false, pairing: .click,
                                 endpoint: StationEndpoint(host: "192.168.1.40"))
        await browser.announce([found])
        #expect(await settle { flow.nearby.count == 1 })
        try await shoot("29-found-it", setup)
        station.reachedOnItsOwnNetwork = false
        await flow.pairNearby(try #require(flow.nearby.first))
        #expect(flow.nearbyProblem != nil)
        try await shoot("30-found-it-one-tap-refused", setup)

        // Your Cores with every case of On this network (picture 24).
        let cores = try boardCores(station)
        let other = try FakeStation(fixture: "session-device-sign-in")
        let closed = try FakeStation(fixture: "session-device-sign-in")
        let listedBrowser = FakeStationBrowser()
        let withNearby = try self.setup(factory: station.transportFactory, paired: Array(cores.prefix(2)),
                                        browser: listedBrowser)
        #expect(await settle { withNearby.flow.screen == .cores })
        // The browser keeps what it announces and hands it over when the flow starts it,
        // so the order of the two does not matter.
        await listedBrowser.announce([
            FoundStation(instanceName: "Unclaimed Core", label: "Unclaimed Core", identityPrefix: "a",
                         claimed: false, pairing: .click, endpoint: StationEndpoint(host: "192.168.1.40")),
            FoundStation(instanceName: "KG4VCF/remote", label: "KG4VCF/remote",
                         identityPrefix: other.foundStation.identityPrefix, claimed: true, pairing: .code,
                         endpoint: StationEndpoint(host: "192.168.1.61")),
            FoundStation(instanceName: "Unclaimed Core (2)", label: "",
                         identityPrefix: closed.foundStation.identityPrefix, claimed: false, pairing: .closed,
                         endpoint: StationEndpoint(host: "192.168.1.62")),
        ])
        #expect(await settle { withNearby.flow.nearby.count == 3 })
        try await shoot("31-your-cores-on-this-network", withNearby)
    }

    @Test("Your Cores in every case: the list, a removed phone, the key the phone can't read, and a new key")
    func yourCores() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let cores = try boardCores(station)
        let setup = try setup(factory: station.transportFactory, paired: cores)
        try await shoot("11-your-cores", setup)

        // KG4VCF/shack removes this phone mid-session.
        let flow = setup.flow
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await station.waitUntilLive())
        await station.deliver(.sessionEnd(LinkMessage.SessionEnd(reason: "This device was removed from the Core.",
                                                                 retryable: false, code: "deviceRemoved")))
        #expect(await settle { flow.screen == .cores })
        try await shoot("12-your-cores-a-core-removed-this-phone", setup)
        await flow.connect(to: try #require(flow.cores.first))
        try await shoot("13-pair-again-with-the-address-filled-in", setup)

        let unreadable = try self.setup(factory: station.transportFactory, paired: cores,
                                        keyItem: InMemorySecretItem(Data([1, 2, 3])))
        try await shoot("14-your-cores-the-key-cant-be-read", unreadable)
        unreadable.flow.makeNewKey()
        try await shoot("15-your-cores-after-a-new-key", unreadable)
    }

    @Test("Your Cores with a Core on this network waiting for a radio to be chosen")
    func yourCoresWaitingForARadio() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let cores = try boardCores(station)
        let browser = FakeStationBrowser()
        let setup = try setup(factory: station.transportFactory, paired: cores, browser: browser)
        let flow = setup.flow
        #expect(await settle { flow.screen == .cores })
        // The browser keeps what it announces and hands it over when the flow starts it,
        // so the order of the two does not matter.
        await browser.announce([
            FoundStation(instanceName: "KG4VCF/shack", label: "KG4VCF/shack",
                         identityPrefix: station.foundStation.identityPrefix, claimed: true, pairing: .code,
                         devices: 2, radio: .waiting,
                         endpoint: StationEndpoint(host: "2001:db8:5a1:e8f0:dea6:32ff:fe12:3456")),
        ])
        #expect(await settle { flow.cores.first?.found?.radio == .waiting })
        let row = try #require(flow.cores.first)
        #expect(YourStationsScreen.detail(row.address, found: row.found)
                == "\(row.address) \u{00B7} 2 devices on it \u{00B7} Waiting for a radio")
        try await shoot("11a-your-cores-waiting-for-a-radio", setup)
    }

    @Test("the trouble sheets: the Core isn't answering, the local network refused, the Core needs updating")
    func troubleSheets() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let cores = try boardCores(station)
        // With the remote access service in the picture: this Wi-Fi, direct, relay (picture 10).
        let dead = try setup(factory: { _, _ in DeadTransport(error: .failed("no route")) }, paired: cores,
                             service: FakeRemoteAccess(.offline))
        await dead.flow.connect(to: try #require(dead.flow.cores.first))
        #expect(await settle { dead.flow.trouble != nil })
        try await shoot("16-the-core-isnt-answering", dead)

        let denied = try setup(factory: { _, _ in DeadTransport(error: .localNetworkDenied) }, paired: cores)
        await denied.flow.connect(to: try #require(denied.flow.cores.first))
        #expect(await settle { denied.flow.trouble != nil })
        try await shoot("17-local-network-not-allowed", denied)

        let old = try FakeStation(fixture: "session-version-app-two-ahead")
        let versions = try setup(factory: old.transportFactory, paired: try boardCores(old))
        await versions.flow.connect(to: try #require(versions.flow.cores.first))
        #expect(await settle { versions.flow.trouble != nil })
        try await shoot("18-the-core-needs-updating", versions)
    }

    @Test("connecting from anywhere (Task 56): a Core with no address, the code alone, what was tried, keyed and by relay")
    func fromAnywhere() async throws {
        // Your Cores with a Core paired through the service, and its Addresses page.
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.remoteTx],
                                      stationLabel: "KG4VCF/shack")
        var cores = try boardCores(station)
        cores[1].endpoints = []
        let service = FakeRemoteAccess(.offline)
        let listed = try setup(factory: { _, _ in DeadTransport(error: .failed("no route")) }, paired: cores,
                               service: service, rendezvous: station)
        listed.flow.show(.cores)
        try await shoot("50-your-cores-one-from-anywhere", listed)
        listed.flow.showAddresses(try #require(listed.flow.cores.first { $0.label == "KG4VCF/attic" }))
        try await shoot("51-addresses-none-kept", listed)
        listed.flow.back()

        // Pair with a code: the code alone, from anywhere.
        listed.flow.pairWithCode()
        listed.flow.codeText = "7-anvil-harbor"
        try await shoot("52-pair-with-a-code-from-anywhere", listed)
        listed.flow.back()

        // Nothing reaches the Core: this Wi-Fi, direct and relay.
        await listed.flow.connect(to: try #require(listed.flow.cores.first))
        #expect(await settle { listed.flow.trouble != nil })
        try await shoot("53-the-core-isnt-answering-wifi-direct-relay", listed)
        listed.flow.dismissTrouble()
        service.way = .unreachable
        await listed.flow.connect(to: try #require(listed.flow.cores.first))
        #expect(await settle { listed.flow.trouble != nil })
        try await shoot("54-the-core-isnt-answering-service-unreachable", listed)
        listed.flow.dismissTrouble()

        // A fresh authenticated version 0 keeps service discovery disabled.
        var older = cores
        older[0].controlChannelVersion = 0
        older[0].controlChannelObservedAtUnixMs = Int64(Date().timeIntervalSince1970 * 1_000)
        let old = try setup(factory: { _, _ in DeadTransport(error: .failed("no route")) }, paired: older,
                            service: FakeRemoteAccess(.reaches(station)))
        await old.flow.connect(to: try #require(old.flow.cores.first))
        #expect(await settle { old.flow.trouble != nil })
        try await shoot("55-the-core-isnt-answering-older-core", old)

        // Through the service by relay: keyed when the link goes, then back on the air by relay.
        var remote = cores
        remote[0].endpoints = []
        let relay = try setup(factory: { _, _ in DeadTransport(error: .failed("no route")) }, paired: remote,
                              service: FakeRemoteAccess(.reaches(station), relayed: true))
        let flow = relay.flow
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        await TransmitScreenTests.fillTransmit(station)
        let transmit = relay.app.main.transmit
        #expect(await settle(seconds: 5) { transmit.permitted })
        transmit.tapPtt()
        #expect(await settle(seconds: 5) { transmit.ptt.state.isKeyed })
        await station.dropLink()
        #expect(await settle { flow.linkLost?.keyed == true })
        try await shoot("56-link-lost-while-keyed", relay, band: true)
        // Advance only to the next redial. A large jump also runs the
        // heartbeat and the four-second Back on the air dismissal before
        // the fake's asynchronous pong can be handled.
        await relay.clock.advance(by: 1_000)
        #expect(await settle(seconds: 15) { flow.backOnAir && flow.linkLost == nil })
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(relay.app.linkRelayed)
        // The relay's two-second heartbeat gives the chip a measured round
        // trip. Let the asynchronous fake pong settle before moving time on.
        await relay.clock.advance(by: 2_000)
        #expect(await settle { relay.app.roundTripMs != nil })
        try await shoot("57-back-on-the-air-by-relay", relay, band: true)
        await relay.app.disconnect()
    }

    @Test("renaming a Core (D76): the sheet empty and filled, a refusal, and the renamed row")
    func renamingACore() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "")
        var cores = try boardCores(station)
        // The fake has no name yet, so its row shows its address.
        cores[0].label = ""
        let setup = try setup(factory: station.transportFactory, paired: cores)
        let flow = setup.flow
        #expect(await settle { flow.screen == .cores })
        let unnamed = try #require(flow.cores.first { $0.id == station.identity.publicKey })
        flow.startRename(unnamed)
        try await shoot("32-rename-sheet-empty", setup)
        let attic = try #require(flow.cores.first { $0.station.label == "KG4VCF/attic" })
        await flow.cancelRename()
        flow.startRename(attic)
        try await shoot("33-rename-sheet-filled", setup)
        await flow.cancelRename()
        flow.startRename(unnamed)
        flow.renameText = "KG4VCF/shack!"
        await flow.saveRename()
        #expect(flow.renameProblem == FakeStation.renameRuleReason)
        try await shoot("34-rename-refused", setup)
        flow.renameText = "KG4VCF/shack"
        await flow.saveRename()
        #expect(flow.renaming == nil)
        #expect(flow.cores.contains { $0.label == "KG4VCF/shack" })
        try await shoot("35-renamed-row", setup)
    }

    @Test("a Core's addresses (JJ, 2026-09-26): the page, adding one, another Core refused, and the fourth")
    func aCoresAddresses() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let other = try FakeStation(fixture: "session-device-sign-in")
        let factory: LinkTransportFactory = { endpoint, trust in
            endpoint.canonical == other.endpoint.canonical ? other.transportFactory(endpoint, trust)
                : station.transportFactory(endpoint, trust)
        }
        var cores = try boardCores(station)
        let lan = StationEndpoint(host: "192.0.2.41")
        cores[0].endpoints = [lan, StationEndpoint(host: "2001:db8:5a1:e8f0:dea6:32ff:fe12:3456")]
        cores[0].lastGood = lan
        let setup = try setup(factory: factory, paired: cores)
        let flow = setup.flow
        #expect(await settle { flow.screen == .cores })
        let shack = try #require(flow.cores.first { $0.id == station.identity.publicKey })
        flow.showAddresses(shack)
        try await shoot("40-addresses", setup)
        flow.addAddress()
        flow.addressText = "198.51.100.40"
        try await shoot("41-add-an-address", setup)
        flow.addressText = other.endpoint.host
        await flow.connectToTypedAddress()
        #expect(flow.addressProblem == ConnectionFlow.otherCoreText("KG4VCF/shack"))
        try await shoot("42-add-an-address-another-core", setup)
        flow.addressText = "198.51.100.40"
        await flow.connectToTypedAddress()
        #expect(flow.screen == .addresses)
        try await shoot("43-addresses-added", setup)
        flow.addAddress()
        flow.addressText = "203.0.113.41"
        await flow.connectToTypedAddress()
        #expect(flow.addressesStation?.endpoints.count == PairedStation.maxEndpoints)
        try await shoot("44-addresses-four", setup)
        flow.removeAddress(try #require(flow.addressesStation?.endpoints.first))
        flow.removeAddress(try #require(flow.addressesStation?.endpoints.first))
        flow.removeAddress(try #require(flow.addressesStation?.endpoints.first))
        #expect(!flow.canRemoveAddress)
        try await shoot("45-addresses-only-one", setup)
    }

    @Test("a Core's addresses learned from it (JJ, 2026-09-29): read-only rows under their own label, upright and in large type")
    func addressesLearnedFromTheCore() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        var cores = try boardCores(station)
        let lan = StationEndpoint(host: "192.0.2.41")
        let global = StationEndpoint(host: "2001:db8:1:0:211:22ff:fe33:4455", port: 47910)
        let public4 = StationEndpoint(host: "198.51.100.7", port: 47910)
        cores[0].endpoints = [lan]
        cores[0].lastGood = lan
        cores[0].keepCoreAddresses([global, public4])
        let setup = try setup(factory: station.transportFactory, paired: cores)
        let flow = setup.flow
        #expect(await settle { flow.screen == .cores })
        let shack = try #require(flow.cores.first { $0.id == station.identity.publicKey })
        flow.showAddresses(shack)
        #expect(flow.addressesStation.map(ConnectionFlow.learnedAddresses(of:)) == [global, public4])
        // The typed address keeps its Remove; the learned ones have none.
        #expect(!flow.canRemoveAddress)
        try await shoot("58-addresses-learned-from-the-core", setup)
        try await shoot("59-addresses-learned-large-type", setup, largeType: true)
    }

    @Test("on the band: link lost, back on the air, the radio off, this phone offline, and an older Core")
    func onTheBand() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in")
        let setup = try setup(factory: station.transportFactory, paired: try boardCores(station), appMajors: [1, 2])
        let flow = setup.flow
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await settle { setup.app.main.slices.entries.count == 2 })
        let connected = try await shoot("19-connected-the-band", setup, band: true)
        let strip = try #require(Self.bandPlanStrip(in: connected), "no band-plan strip on the connected band")
        let zoomIn = try #require(Self.zoomInGlyph(in: connected), "no zoom buttons on the connected band")
        let probes = [strip, zoomIn]
        let connectedSideways = try await shoot("connected-sideways", setup, band: true, sideways: true, save: false)

        await station.dropLink()
        #expect(await settle { flow.linkLost != nil })
        let lost = try await shoot("20-link-lost-while-listening", setup, band: true)
        Self.expectDimmed(lost, connected, at: probes)
        Self.expectFadedUnderWords(lost, connected, "20-link-lost-while-listening", sideways: false)
        let lostSideways = try await shoot("26-link-lost-sideways", setup, band: true, sideways: true)
        Self.expectFadedUnderWords(lostSideways, connectedSideways, "26-link-lost-sideways", sideways: true)
        await flow.cancelReconnecting()
        let stopped = try await shoot("21-link-lost-stopped", setup, band: true)
        Self.expectDimmed(stopped, connected, at: probes)
        Self.expectFadedUnderWords(stopped, connected, "21-link-lost-stopped", sideways: false)
        await flow.reconnect()
        #expect(await settle { flow.backOnAir })
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        try await shoot("22-back-on-the-air", setup, band: true)
        // The notice goes after a few seconds.
        await setup.clock.advance(by: 4000)
        #expect(!flow.backOnAir)
        let beforeRadioOff = try await shoot("before-the-radio-is-off", setup, band: true, save: false)

        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "radioModel", value: .utf8("ANAN-G2")),
            .init(name: "radioConnected", value: .bool(false)),
            .init(name: "radioAddress", value: .utf8("192.168.1.20")),
        ])))
        #expect(await settle { flow.radioOff != nil })
        let radioOff = try await shoot("23-the-radio-is-off", setup, band: true)
        Self.expectDimmed(radioOff, connected, at: probes)
        Self.expectFadedUnderWords(radioOff, beforeRadioOff, "23-the-radio-is-off", sideways: false)
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: [
            .init(name: "radioConnected", value: .bool(true)),
        ])))
        #expect(await settle { flow.radioOff == nil })
        let beforeOffline = try await shoot("before-this-phone-is-offline", setup, band: true, save: false)

        setup.network.changed?(.offline)
        #expect(await settle { flow.offline })
        let offline = try await shoot("24-this-phone-is-offline", setup, band: true)
        Self.expectDimmed(offline, connected, at: probes)
        Self.expectFadedUnderWords(offline, beforeOffline, "24-this-phone-is-offline", sideways: false)
        setup.network.changed?(NetworkPath(online: true, interfaces: ["en0"]))
        #expect(await settle { !flow.offline })

        #expect(flow.olderCore == "KG4VCF/shack")
        try await shoot("25-an-older-core-tools", setup, tab: .tools)
        await setup.app.disconnect()
    }

    @Test("the Radio tab: the Core and link with Disconnect, and Your Cores after Disconnect")
    func radioTabAndDisconnect() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "KG4VCF/shack")
        let setup = try setup(factory: station.transportFactory, paired: try boardCores(station))
        let flow = setup.flow
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(await station.waitUntilLive())
        #expect(await settle { setup.app.main.coreName == "KG4VCF/shack" })
        // A ping answered: the round trip shows.
        await setup.app.retryNow()
        #expect(await settle { setup.app.roundTripMs != nil })
        try await shoot("31-radio-tab", setup, tab: .radio)
        await flow.disconnect()
        #expect(flow.screen == .cores)
        #expect(setup.app.session == nil)
        try await shoot("32-your-cores-after-disconnect", setup)
    }

    // MARK: Inside

    @discardableResult
    private func shoot(_ name: String, _ setup: Setup, band: Bool = false, sideways: Bool = false,
                       tab: AppTab = .panadapter, largeType: Bool = false, save: Bool = true) async throws -> UIImage {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let window = try BandFlagShotTests.window(size: size)
        // Sideways, the window sits clear of the upright status bar and home
        // indicator and takes the sideways phone's own safe area, as the
        // main screen's sideways shots do.
        if sideways {
            window.frame.origin.y = 120
        }
        let view = RootView(selection: tab)
            .environmentObject(setup.app)
            .environmentObject(setup.flow)
            .preferredColorScheme(.dark)
        // Large type: the accessibility size the other large-type shots use.
        let root = largeType ? AnyView(view.environment(\.dynamicTypeSize, .accessibility2)) : AnyView(view)
        let host = UIHostingController(rootView: root)
        if sideways {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw: ShotWait.BandDrawing?
        if band {
            bandDraw = try await ShotWait.requireBandLaidOut(setup.app.main.band, in: window)
        } else {
            bandDraw = nil
            await ShotWait.laidOut(window)
        }
        if let bandDraw {
            let model = setup.app.main.band
            model.endpointId = 1
            if let context = BandFlagShotTests.context() {
                model.receive(.context(context))
            }
            BandFlagShotTests.feed(model, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
            try await ShotWait.requireBandShown(model, in: window, after: bandDraw)
        }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if save, let directory = ProcessInfo.processInfo.environment["NEREUS_CONNECT_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
        return image
    }

    // MARK: Under a cover

    /// The cover's backing, rgba(10, 12, 20, 0.784), as the board paints it.
    private static let backing = (red: 10.0, alpha: 0.784)

    /// A point on the band-plan strip in the connected band's shot: the
    /// first pixel down the band's left edge in the phone segment's orange
    /// as the strip draws it, dimmed for Extra and General (about 108, 57,
    /// 12; D79), in image pixels.
    private static func bandPlanStrip(in image: UIImage) -> CGPoint? {
        guard let pixels = Pixels(image) else {
            return nil
        }
        let x = 6
        for y in stride(from: pixels.height / 5, to: pixels.height * 4 / 5, by: 1) {
            let (r, g, b) = pixels.at(x, y)
            if (85...135).contains(Int(r)) && (35...80).contains(Int(g)) && b < 40 {
                return CGPoint(x: x, y: y + 3)
            }
        }
        return nil
    }

    /// The zoom-in button's white plus in the connected band's shot: the
    /// first white pixel in the band's bottom-right corner, in image pixels.
    private static func zoomInGlyph(in image: UIImage) -> CGPoint? {
        guard let pixels = Pixels(image) else {
            return nil
        }
        for y in stride(from: pixels.height * 3 / 4, to: pixels.height * 9 / 10, by: 1) {
            for x in stride(from: pixels.width - 1, to: pixels.width * 3 / 4, by: -1) {
                let (r, g, b) = pixels.at(x, y)
                if min(r, g, b) > 170 {
                    return CGPoint(x: x, y: y)
                }
            }
        }
        return nil
    }

    /// Under a cover the band is faded almost away, then under the 0.784
    /// backing. At each probe (the band-plan strip, drawn in Metal, and the
    /// zoom-in plus, drawn in SwiftUI) the red, less the backing's share,
    /// must be at most 4% of what the connected band shows there (the
    /// backing's share is approximate, so this is the coarse check; the
    /// text block's contrast below is the fine one). Without any fade it is
    /// about 22%, and at the board's 0.4 about 9%.
    private static func expectDimmed(_ covered: UIImage, _ connected: UIImage, at probes: [CGPoint]) {
        guard let under = Pixels(covered), let bare = Pixels(connected) else {
            Issue.record("the shots have no pixels")
            return
        }
        for point in probes {
            let bandRed = Double(bare.at(Int(point.x), Int(point.y)).0)
            let coveredRed = Double(under.at(Int(point.x), Int(point.y)).0)
            let shown = (coveredRed - backing.red * backing.alpha) / bandRed
            print("Under the cover the band at \(point) shows at \(shown) of its strength (red \(coveredRed) of \(bandRed))")
            #expect(shown <= 0.04, "the band at \(point) shows at \(shown) of its strength under the cover")
        }
    }

    /// Nothing from the band reads behind a cover's words. Inside the
    /// cover's text block (the word, the lead, the body, down to the strip,
    /// whose panel is opaque), the band's strongest element (the brightest
    /// 1% of the band there, the scale's digits upright and the flag's
    /// frequency sideways) may keep at most 2% of the contrast it has
    /// against the band's darker half on the connected band. Pixels near
    /// the cover's own letters are left out; everywhere else in the block
    /// only the band and the flat backing are drawn, so the change between
    /// the band's bright and dark pixels is the band's alone.
    private static func expectFadedUnderWords(_ covered: UIImage, _ connected: UIImage, _ name: String,
                                              sideways: Bool) {
        guard let under = Pixels(covered), let bare = Pixels(connected),
              under.width == bare.width, under.height == bare.height else {
            Issue.record("\(name): the shots have no pixels")
            return
        }
        guard let block = textBlock(in: under, sideways: sideways) else {
            Issue.record("\(name): no text block found on the cover")
            return
        }
        // The cover's letters, and 4 pixels round them, are the cover's.
        let letters = under.near(radius: 4, in: block) { r, g, b in max(r, g, b) > 90 }
        var samples: [(bare: Double, under: Double)] = []
        for y in block.minY..<block.maxY {
            for x in block.minX..<block.maxX where !letters[(y - block.minY) * block.width + (x - block.minX)] {
                samples.append((bare.luminance(x, y), under.luminance(x, y)))
            }
        }
        guard samples.count > 1000 else {
            Issue.record("\(name): too little of the band inside the text block (\(samples.count) pixels)")
            return
        }
        samples.sort { $0.bare < $1.bare }
        let strongest = samples.suffix(max(30, samples.count / 100))
        let dark = samples.prefix(samples.count / 2)
        func mean(_ values: some Collection<(bare: Double, under: Double)>, _ key: (Double, Double) -> Double) -> Double {
            values.reduce(0) { $0 + key($1.bare, $1.under) } / Double(values.count)
        }
        let bareContrast = mean(strongest) { a, _ in a } - mean(dark) { a, _ in a }
        let underContrast = mean(strongest) { _, b in b } - mean(dark) { _, b in b }
        let kept = underContrast / bareContrast
        print("\(name): text block x \(block.minX)..<\(block.maxX), y \(block.minY)..<\(block.maxY), \(samples.count) band pixels; "
              + "the band's strongest element keeps \(kept) of its contrast (\(underContrast) of \(bareContrast))")
        #expect(bareContrast > 40, "\(name): the connected band shows little inside the text block (\(bareContrast))")
        #expect(kept <= 0.02, "\(name): the band keeps \(kept) of its contrast behind the cover's words")
    }

    /// Where the cover's text block sits, in image pixels: from 6 pixels
    /// above the word to 6 pixels above the strip's panel, as wide as the
    /// cover's words and strip.
    private static func textBlock(in pixels: Pixels, sideways: Bool) -> Pixels.Block? {
        // The band's rows, clear of the toolbar and the tab bar.
        let top = Int(Double(pixels.height) * (sideways ? 0.28 : 0.13))
        let bottom = Int(Double(pixels.height) * (sideways ? 0.76 : 0.89))
        var minX = pixels.width, maxX = 0, minY = pixels.height
        for y in top..<bottom {
            for x in 0..<pixels.width {
                let (r, g, b) = pixels.at(x, y)
                if max(r, g, b) > 120 {
                    minX = min(minX, x)
                    maxX = max(maxX, x)
                    minY = min(minY, y)
                }
            }
        }
        guard minY < bottom else {
            return nil
        }
        // The strip's panel, rgb(0x0D, 0x1B, 0x28), opaque: the first row
        // with 400 pixels of it in a row (the band never draws that).
        for y in minY..<bottom {
            var run = 0
            for x in minX...maxX {
                let (r, g, b) = pixels.at(x, y)
                run = abs(Int(r) - 0x0D) <= 3 && abs(Int(g) - 0x1B) <= 3 && abs(Int(b) - 0x28) <= 3 ? run + 1 : 0
                if run >= 400 {
                    return Pixels.Block(minX: max(0, minX - 6), maxX: min(pixels.width, maxX + 7),
                                        minY: max(0, minY - 6), maxY: y - 6)
                }
            }
        }
        return nil
    }

    /// An image's pixels, 8-bit RGBA.
    private struct Pixels {
        let width: Int
        let height: Int
        let bytes: [UInt8]

        init?(_ image: UIImage) {
            guard let cg = image.cgImage else {
                return nil
            }
            width = cg.width
            height = cg.height
            var bytes = [UInt8](repeating: 0, count: width * height * 4)
            let drawn = bytes.withUnsafeMutableBytes { buffer -> Bool in
                guard let context = CGContext(data: buffer.baseAddress, width: cg.width, height: cg.height,
                                              bitsPerComponent: 8, bytesPerRow: cg.width * 4,
                                              space: CGColorSpaceCreateDeviceRGB(),
                                              bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
                    return false
                }
                context.draw(cg, in: CGRect(x: 0, y: 0, width: cg.width, height: cg.height))
                return true
            }
            guard drawn else {
                return nil
            }
            self.bytes = bytes
        }

        func at(_ x: Int, _ y: Int) -> (UInt8, UInt8, UInt8) {
            let i = (y * width + x) * 4
            return (bytes[i], bytes[i + 1], bytes[i + 2])
        }

        func luminance(_ x: Int, _ y: Int) -> Double {
            let (r, g, b) = at(x, y)
            return 0.2126 * Double(r) + 0.7152 * Double(g) + 0.0722 * Double(b)
        }

        /// A rectangle of pixels, its max edges exclusive.
        struct Block {
            let minX: Int
            let maxX: Int
            let minY: Int
            let maxY: Int
            var width: Int { maxX - minX }
            var height: Int { maxY - minY }
        }

        /// Inside `block`, row by row, which pixels lie within `radius` of
        /// one that passes `test`.
        func near(radius: Int, in block: Block, _ test: (UInt8, UInt8, UInt8) -> Bool) -> [Bool] {
            var rows = [Bool](repeating: false, count: block.width * block.height)
            for y in block.minY..<block.maxY {
                for x in block.minX..<block.maxX {
                    var hit = false
                    for dx in -radius...radius where !hit {
                        let px = x + dx
                        if px >= 0 && px < width {
                            let (r, g, b) = at(px, y)
                            hit = test(r, g, b)
                        }
                    }
                    rows[(y - block.minY) * block.width + (x - block.minX)] = hit
                }
            }
            var near = rows
            for y in block.minY..<block.maxY {
                for x in 0..<block.width {
                    var hit = false
                    for dy in -radius...radius where !hit {
                        let row = y + dy - block.minY
                        if row >= 0 && row < block.height {
                            hit = rows[row * block.width + x]
                        }
                    }
                    near[(y - block.minY) * block.width + x] = hit
                }
            }
            return near
        }
    }
}
