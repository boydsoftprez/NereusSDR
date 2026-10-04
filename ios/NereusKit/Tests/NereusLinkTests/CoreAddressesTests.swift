// NereusSDR for iOS: a paired Core's several addresses, and reading which Core answers at an address
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// R-IOS-16 (JJ, 2026-09-26): a paired Core keeps up to four addresses,
/// the one that worked last first and marked; an address is added only
/// for a Core whose identity answered there, and a Core never loses its
/// last address.
@Suite struct CoreAddressesTests {
    private static let lan = StationEndpoint(host: "192.0.2.10")
    private static let vpn = StationEndpoint(host: "198.51.100.7")

    // MARK: The store

    @Test func anAddedAddressGoesFirstAndAtMostFourAreKept() throws {
        let store = PairedStationStore(item: InMemorySecretItem())
        let key = TestStationIdentity().publicKey
        try store.save(PairedStation(identityKey: key, label: "Rock", endpoints: [Self.lan]))
        var station = try #require(try store.addAddress(identityKey: key, Self.vpn))
        #expect(station.endpoints == [Self.vpn, Self.lan])
        // Adding one it has only moves it first.
        station = try #require(try store.addAddress(identityKey: key, StationEndpoint(host: "192.0.2.10.")))
        #expect(station.endpoints.map(\.host) == ["192.0.2.10.", "198.51.100.7"])
        for host in ["203.0.113.1", "203.0.113.2", "203.0.113.3"] {
            station = try #require(try store.addAddress(identityKey: key, StationEndpoint(host: host)))
        }
        #expect(station.endpoints.map(\.host) == ["203.0.113.3", "203.0.113.2", "203.0.113.1", "192.0.2.10."])
        #expect(try store.addAddress(identityKey: TestStationIdentity().publicKey, Self.lan) == nil)
    }

    @Test func theAddressThatWorkedLastIsMarkedAndGoesFirst() throws {
        let store = PairedStationStore(item: InMemorySecretItem())
        let key = TestStationIdentity().publicKey
        try store.save(PairedStation(identityKey: key, label: "Rock", endpoints: [Self.lan, Self.vpn]))
        let station = try #require(try store.recordReached(identityKey: key, at: Self.vpn, by: .direct))
        #expect(station.endpoints == [Self.vpn, Self.lan])
        #expect(station.isLastGood(Self.vpn))
        #expect(!station.isLastGood(Self.lan))
        #expect(station.lastPath == ConnectionAttempt.Path.direct.rawValue)
        // A new address goes first, but the one that worked stays marked.
        let added = try #require(try store.addAddress(identityKey: key, StationEndpoint(host: "203.0.113.9")))
        #expect(added.endpoints.first?.host == "203.0.113.9")
        #expect(added.isLastGood(Self.vpn))
        // A connect tries the one that worked last first, then the rest as kept.
        #expect(added.dialOrder == [Self.vpn, StationEndpoint(host: "203.0.113.9"), Self.lan])
    }

    @Test func theCapNeverDropsTheAddressThatWorkedLast() throws {
        let store = PairedStationStore(item: InMemorySecretItem())
        let key = TestStationIdentity().publicKey
        // The one that worked is the oldest in the list.
        try store.save(PairedStation(identityKey: key, label: "Rock", endpoints: [Self.vpn, Self.lan],
                                     lastGood: Self.lan))
        var station = try #require(try store.station(identityKey: key))
        for host in ["203.0.113.1", "203.0.113.2", "203.0.113.3"] {
            station = try #require(try store.addAddress(identityKey: key, StationEndpoint(host: host)))
        }
        #expect(station.endpoints.count == PairedStation.maxEndpoints)
        #expect(station.endpoints.contains(Self.lan))
        #expect(!station.endpoints.contains(Self.vpn))
        #expect(station.isLastGood(Self.lan))
        #expect(station.dialOrder.first == Self.lan)
    }

    @Test func aLastGoodNoLongerKeptIsForgotten() {
        // Kept by an older build with an address the list no longer holds.
        var station = PairedStation(identityKey: TestStationIdentity().publicKey, label: "Rock",
                                    endpoints: [Self.lan], lastGood: Self.vpn)
        #expect(station.dialOrder == [Self.lan])
        station.add(StationEndpoint(host: "203.0.113.1"))
        #expect(station.lastGood == nil)
    }

    @Test func removeNeverTakesTheLastAddress() throws {
        let store = PairedStationStore(item: InMemorySecretItem())
        let key = TestStationIdentity().publicKey
        try store.save(PairedStation(identityKey: key, label: "Rock", endpoints: [Self.lan, Self.vpn],
                                     lastGood: Self.vpn))
        let station = try #require(try store.removeAddress(identityKey: key, Self.vpn))
        #expect(station.endpoints == [Self.lan])
        #expect(station.lastGood == nil)
        #expect(try store.removeAddress(identityKey: key, Self.lan) == nil)
        #expect(try store.removeAddress(identityKey: key, StationEndpoint(host: "203.0.113.1")) == nil)
        #expect(try store.station(identityKey: key)?.endpoints == [Self.lan])
    }

    @Test func whatAnOlderBuildKeptStillReads() throws {
        // An older build wrote no lastGood.
        let key = TestStationIdentity().publicKey
        let older = """
        [{"identityKey":"\(key.base64EncodedString())","label":"Rock","endpoints":[{"host":"192.0.2.10","port":47910}],"lastPath":"direct"}]
        """
        let store = PairedStationStore(item: InMemorySecretItem(Data(older.utf8)))
        let station = try #require(try store.station(identityKey: key))
        #expect(station.endpoints == [Self.lan])
        #expect(station.lastGood == nil)
    }

    // MARK: Which Core answers

    private static func identify(_ transport: PairingTestTransport, clock: ManualLinkClock = ManualLinkClock())
        -> Task<CoreIdentityProbe.Outcome, Never> {
        Task {
            await CoreIdentityProbe.identify(Self.lan, transportFactory: transport.factory, clock: clock)
        }
    }

    private static func hello(_ claim: LinkMessage.StationIdentityClaim?) -> LinkMessage {
        .hello(LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1],
                                 features: ["deviceAuth": 1, "pairing": 1], identity: claim,
                                 challenge: TestStationIdentity.newChallenge()))
    }

    @Test func aCoreIsKnownByTheIdentityThatBindsItsCertificate() async throws {
        let transport = PairingTestTransport()
        let core = TestStationIdentity()
        let probe = Self.identify(transport)
        transport.deliver(Self.hello(try core.claim(certificateSHA256: transport.certificateSHA256)))
        #expect(await probe.value == .identified(publicKey: core.publicKey))
        // Read under the pairing trust, nothing sent, and closed.
        #expect(transport.dialledTrusts == [.pairing])
        #expect(transport.pending.isEmpty)
        #expect(await transport.waitUntilClosed())
    }

    @Test func anIdentityThatDoesNotBindThisCertificateIsNotKnown() async throws {
        let transport = PairingTestTransport()
        let core = TestStationIdentity()
        let probe = Self.identify(transport)
        transport.deliver(Self.hello(try core.claim(certificateSHA256: Data(repeating: 7, count: 32))))
        #expect(await probe.value == .unidentified)
    }

    @Test func aHelloWithNoIdentityIsNotKnown() async {
        let transport = PairingTestTransport()
        let probe = Self.identify(transport)
        transport.deliver(Self.hello(nil))
        #expect(await probe.value == .unidentified)
    }

    @Test func nothingAnsweringInTimeIsNotReached() async {
        let transport = PairingTestTransport()
        let clock = ManualLinkClock()
        let probe = Self.identify(transport, clock: clock)
        // The probe's deadline is armed at the dial.
        for _ in 0..<10_000 where clock.pendingDueTimes.isEmpty {
            await Task.yield()
        }
        #expect(clock.pendingDueTimes == [30_000])
        await clock.advance(by: 29_999)
        await clock.advance(by: 1)
        #expect(await probe.value == .notReached(localNetworkDenied: false))
    }
}
