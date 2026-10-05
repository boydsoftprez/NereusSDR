// NereusSDR for iOS: tests for the list of paired Cores, kept in the Keychain
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

@Suite struct PairedStationStoreTests {
    private static func core(_ label: String) -> PairedStation {
        PairedStation(identityKey: TestStationIdentity().publicKey, label: label,
                      endpoints: [StationEndpoint(host: "192.0.2.10"), StationEndpoint(host: "shack.example.net",
                                                                                       port: 47911)],
                      lastPath: "direct")
    }

    @Test func aNewStoreHasNoCores() throws {
        #expect(try PairedStationStore(item: InMemorySecretItem()).all().isEmpty)
    }

    @Test func coresAreKeptForANewStoreObject() throws {
        let item = InMemorySecretItem()
        let shack = Self.core("KG4VCF/shack")
        let attic = Self.core("KG4VCF/attic")
        try PairedStationStore(item: item).save(shack)
        try PairedStationStore(item: item).save(attic)
        let again = PairedStationStore(item: item)
        #expect(try again.all() == [shack, attic])
        #expect(try again.station(identityKey: attic.identityKey) == attic)
        #expect(try again.station(identityKey: Data(repeating: 1, count: 91)) == nil)
        #expect(shack.identityKey.count == 91)
        #expect(shack.trust == .identity(publicKey: shack.identityKey))
    }

    @Test func savingACoreAgainReplacesIt() throws {
        let store = PairedStationStore(item: InMemorySecretItem())
        var shack = Self.core("KG4VCF/shack")
        try store.save(shack)
        shack.label = "KG4VCF/tower"
        shack.lastPath = nil
        try store.save(shack)
        #expect(try store.all() == [shack])
    }

    /// Link document section 6.3 (Task 28a): each sign-in keeps the Core's
    /// controlChannelVersion. Connecting from anywhere is offered to a Core
    /// that declared 1 or has had no session yet, and not to one that
    /// declared none; a record an older build kept reads as no session yet.
    @Test func theControlChannelIsKeptAndDecidesReachingFromAnywhere() throws {
        let item = InMemorySecretItem()
        let store = PairedStationStore(item: item)
        let shack = Self.core("KG4VCF/shack")
        try store.save(shack)
        #expect(shack.controlChannelVersion == nil && shack.reachableFromAnywhere)
        #expect(try store.recordControlChannelVersion(identityKey: shack.identityKey, 0)?.reachableFromAnywhere == false)
        #expect(try store.recordControlChannelVersion(identityKey: shack.identityKey, 1)?.reachableFromAnywhere == true)
        #expect(try PairedStationStore(item: item).station(identityKey: shack.identityKey)?.controlChannelVersion == 1)
        #expect(try store.recordControlChannelVersion(identityKey: Data(repeating: 1, count: 91), 1) == nil)
        #expect(PairedStation.updateToReachFromAnywhereText == "Update the Core to reach it from anywhere.")

        // What an older build kept, without the key.
        var older = try #require(try JSONSerialization.jsonObject(with: try JSONEncoder().encode([shack]))
                                    as? [[String: Any]])
        older[0].removeValue(forKey: "controlChannelVersion")
        let read = try JSONDecoder().decode([PairedStation].self,
                                            from: try JSONSerialization.data(withJSONObject: older))
        #expect(read.first?.controlChannelVersion == nil)
        #expect(read.first?.reachableFromAnywhere == true)
    }

    @Test func verifiedNegativeCapabilityExpiresWithoutRenewingOnFailedDiscovery() throws {
        let store = PairedStationStore(item: InMemorySecretItem())
        let core = Self.core("KG4VCF/shack")
        try store.save(core)
        let observed = Date(timeIntervalSince1970: 1_700_000_000)
        let denied = try #require(try store.recordRouteCapabilities(identityKey: core.identityKey,
                                                                     controlChannelVersion: 0,
                                                                     relayAllowed: false,
                                                                     observedAt: observed))
        #expect(!denied.reachableFromAnywhere(now: observed.addingTimeInterval(299)))
        #expect(denied.reachableFromAnywhere(now: observed.addingTimeInterval(300)))
        #expect(denied.reachableFromAnywhere(now: observed.addingTimeInterval(-1)))
        #expect(denied.relayAllowed == false)
        // Availability and no-answer are not authenticated capabilities; a
        // failed discovery must leave the old timestamp and relay denial.
        #expect(try store.station(identityKey: core.identityKey) == denied)
        let enabled = try #require(try store.recordRouteCapabilities(identityKey: core.identityKey,
                                                                      controlChannelVersion: 1,
                                                                      relayAllowed: false,
                                                                      observedAt: observed))
        #expect(enabled.reachableFromAnywhere)
        #expect(enabled.controlChannelObservedAtUnixMs == nil)
        #expect(enabled.relayAllowed == false)
    }

    @Test func legacyAndMalformedNegativeTimestampAreStaleWithoutLosingPairing() throws {
        let core = Self.core("KG4VCF/shack")
        var legacy = core
        legacy.controlChannelVersion = 0
        #expect(legacy.reachableFromAnywhere)
        var document = try #require(JSONSerialization.jsonObject(with: JSONEncoder().encode([legacy]))
                                    as? [[String: Any]])
        document[0]["controlChannelObservedAtUnixMs"] = "not-a-timestamp"
        let decoded = try JSONDecoder().decode([PairedStation].self,
                                               from: JSONSerialization.data(withJSONObject: document))
        let recovered = try #require(decoded.first)
        #expect(recovered.identityKey == core.identityKey)
        #expect(recovered.endpoints == core.endpoints)
        #expect(recovered.controlChannelObservedAtUnixMs == nil)
        #expect(recovered.reachableFromAnywhere)
        document[0]["identityKey"] = "not-base64-data!!!"
        #expect(throws: (any Error).self) {
            _ = try JSONDecoder().decode([PairedStation].self,
                                         from: JSONSerialization.data(withJSONObject: document))
        }
    }

    @Test func reachingACoreThroughTheServiceKeepsThePathAndMovesNoAddress() throws {
        let store = PairedStationStore(item: InMemorySecretItem())
        var shack = Self.core("KG4VCF/shack")
        let lan = StationEndpoint(host: "192.0.2.20")
        let vpn = StationEndpoint(host: "10.8.0.20")
        shack.endpoints = [vpn, lan]
        shack.reached(lan, by: .thisNetwork)
        try store.save(shack)
        let relayed = try #require(try store.recordReachedThroughService(identityKey: shack.identityKey, by: .relay))
        #expect(relayed.lastPath == "relay")
        #expect(relayed.endpoints == [lan, vpn])
        // No address worked this time, so none is marked as the last one.
        #expect(relayed.lastGood == nil)
        #expect(try store.station(identityKey: shack.identityKey) == relayed)
        // A Core paired through the service keeps no address at all.
        var mailbox = Self.core("KG4VCF/attic")
        mailbox.endpoints = []
        try store.save(mailbox)
        let direct = try #require(try store.recordReachedThroughService(identityKey: mailbox.identityKey, by: .direct))
        #expect(direct.lastPath == "direct" && direct.endpoints.isEmpty)
        #expect(try store.recordReachedThroughService(identityKey: Data([1, 2, 3]), by: .direct) == nil)
    }

    @Test func removingACoreForgetsItOnly() throws {
        let item = InMemorySecretItem()
        let store = PairedStationStore(item: item)
        let shack = Self.core("KG4VCF/shack")
        let attic = Self.core("KG4VCF/attic")
        try store.save(shack)
        try store.save(attic)
        try store.remove(identityKey: shack.identityKey)
        #expect(try store.all() == [attic])
        try store.remove(identityKey: attic.identityKey)
        #expect(try store.all().isEmpty)
        #expect(try item.read() == nil)
    }

    @Test(.enabled(if: keychainIsUsable, "the data protection Keychain is not available to this process"))
    func theRealKeychainKeepsTheCores() throws {
        let item = KeychainItem(service: "NereusSDR tests", account: "paired-cores-\(UUID().uuidString)")
        defer { try? item.delete() }
        let shack = Self.core("KG4VCF/shack")
        try PairedStationStore(item: item).save(shack)
        #expect(try PairedStationStore(item: item).all() == [shack])
    }
}
