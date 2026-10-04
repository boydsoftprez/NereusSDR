// NereusSDR for iOS: tests for the Core's own dialable addresses, from Bonjour on its network and from the Core itself
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Darwin
import Foundation
import LinkSessionTestSupport
import Testing
@testable import NereusLink

/// R-IOS-16, part 2 (2026-09-29): the phone learns the Core's real global
/// addresses, every one Bonjour resolves on the Core's network and the list
/// the Core itself sends in `devices`' `coreAddresses` (link document
/// sections 6.1, 6.3, 7.1 and 21.1), keeps them with the paired Core and
/// races them from anywhere.
@Suite struct CoreDirectAddressTests {
    /// The shape of the Rock's addresses (2026-09-29): a stable global IPv6
    /// on end1 and on wlan0, a unique local and a link-local IPv6, and a
    /// private IPv4. The global ones are documentation-range stand-ins
    /// (2001:db8::/32): a real address is never committed or logged.
    static let end1 = "2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2"
    static let wlan0 = "2001:db8:467f:66e7:8a00:44ff:fe00:4602"
    static let uniqueLocal = "fd4e:1a2b:3c4d:1:ec1f:31ff:fe8e:15f2"
    static let linkLocal = "fe80::ec1f:31ff:fe8e:15f2"
    static let lan = "192.168.109.106"

    private static func v6(_ text: String) -> [UInt8] {
        var address = in6_addr()
        _ = inet_pton(AF_INET6, text, &address)
        return withUnsafeBytes(of: address) { Array($0) }
    }

    private static func v4(_ text: String) -> [UInt8] {
        var address = in_addr()
        _ = inet_pton(AF_INET, text, &address)
        return withUnsafeBytes(of: address) { Array($0) }
    }

    // MARK: Which addresses are global

    @Test func onlyGlobalUnicastLiteralsAreGlobal() {
        #expect(LocalNetworks.isGlobal(Self.end1))
        #expect(LocalNetworks.isGlobal("[\(Self.wlan0)]"))
        #expect(LocalNetworks.isGlobal("1.2.3.4"))
        #expect(!LocalNetworks.isGlobal(Self.uniqueLocal))
        #expect(!LocalNetworks.isGlobal(Self.linkLocal))
        #expect(!LocalNetworks.isGlobal("\(Self.linkLocal)%en0"))
        #expect(!LocalNetworks.isGlobal(Self.lan))
        #expect(!LocalNetworks.isGlobal("10.0.252.47"))
        #expect(!LocalNetworks.isGlobal("100.64.1.1"))
        #expect(!LocalNetworks.isGlobal("169.254.3.3"))
        #expect(!LocalNetworks.isGlobal("127.0.0.1"))
        #expect(!LocalNetworks.isGlobal("::1"))
        #expect(!LocalNetworks.isGlobal("ff02::1"))
        #expect(!LocalNetworks.isGlobal("224.0.0.251"))
        #expect(!LocalNetworks.isGlobal("::ffff:192.168.109.106"))
        #expect(!LocalNetworks.isGlobal("rock-5c.local"))
    }

    /// The Core lists an IPv4 address only outside RFC 6890's special-purpose
    /// blocks (`CoreAddresses::isPublicIpv4`, src/core/session/CoreAddresses.cpp:41-60);
    /// the phone reads the same blocks as not global, the edges of each
    /// included, and the addresses just outside them as global.
    @Test func theCoresSpecialPurposeIpv4BlocksAreNotGlobal() {
        for special in ["0.0.0.1", "0.255.255.255", "10.255.255.255", "100.64.0.0", "100.127.255.255",
                        "127.255.255.254", "169.254.0.1", "172.16.0.1", "172.31.255.255",
                        "192.0.0.0", "192.0.0.255", "192.0.2.0", "192.0.2.255", "192.88.99.0", "192.88.99.255",
                        "192.168.255.255", "198.18.0.0", "198.19.255.255", "198.51.100.0", "198.51.100.255",
                        "203.0.113.0", "203.0.113.255", "224.0.0.1", "239.255.255.255", "240.0.0.1",
                        "255.255.255.255"] {
            #expect(!LocalNetworks.isGlobal(special), "\(special) is in a special-purpose block")
        }
        for outside in ["1.0.0.1", "100.63.255.255", "100.128.0.0", "172.15.255.255", "172.32.0.0",
                        "192.0.1.0", "192.0.3.0", "192.88.98.255", "192.88.100.0", "198.17.255.255",
                        "198.20.0.0", "198.51.99.255", "198.51.101.0", "203.0.112.255", "203.0.114.0",
                        "223.255.255.255"] {
            #expect(LocalNetworks.isGlobal(outside), "\(outside) is outside every special-purpose block")
        }
    }

    @Test func directAddressesAreTriedGlobalIPv6First() {
        let order = LocalNetworks.directPreference([
            StationEndpoint(host: Self.lan, port: 50055),
            StationEndpoint(host: "\(Self.linkLocal)%en0", port: 50055),
            StationEndpoint(host: "1.2.3.4", port: 50055),
            StationEndpoint(host: Self.uniqueLocal, port: 50055),
            StationEndpoint(host: "rock-5c.local", port: 50055),
            StationEndpoint(host: Self.end1, port: 50055),
            StationEndpoint(host: Self.wlan0, port: 50055),
        ])
        #expect(order.map(\.host) == [Self.end1, Self.wlan0, Self.uniqueLocal, "\(Self.linkLocal)%en0",
                                      "1.2.3.4", Self.lan, "rock-5c.local"])
    }

    // MARK: Every address Bonjour resolves

    /// A fake Bonjour answer for the Core's host: a global IPv6, a unique
    /// local, a link-local on en0 and a private IPv4, each kept as a typed
    /// address is, tried global IPv6 first.
    @Test func everyAddressBonjourResolvesIsKept() {
        let answer: [BonjourAddress] = [
            BonjourAddress(bytes: Self.v4(Self.lan), interfaceName: "en0"),
            BonjourAddress(bytes: Self.v6(Self.linkLocal), interfaceName: "en0"),
            BonjourAddress(bytes: Self.v6(Self.uniqueLocal), interfaceName: "en0"),
            BonjourAddress(bytes: Self.v6(Self.end1), interfaceName: "en0"),
            // The same address again, as a second interface may report it.
            BonjourAddress(bytes: Self.v6(Self.end1), interfaceName: "en1"),
        ]
        #expect(StationBrowser.endpoints(answer, port: 50055) == [
            StationEndpoint(host: Self.end1, port: 50055),
            StationEndpoint(host: Self.uniqueLocal, port: 50055),
            StationEndpoint(host: "\(Self.linkLocal)%en0", port: 50055),
            StationEndpoint(host: Self.lan, port: 50055),
        ])
        // A link-local address with no interface to name cannot be dialled.
        #expect(StationBrowser.endpoints([BonjourAddress(bytes: Self.v6(Self.linkLocal), interfaceName: nil)],
                                         port: 50055).isEmpty)
        #expect(StationBrowser.endpoints(answer, port: 0).isEmpty)
    }

    @Test func aSocketAddressReadsWithItsInterface() throws {
        var six = sockaddr_in6()
        six.sin6_len = UInt8(MemoryLayout<sockaddr_in6>.size)
        six.sin6_family = sa_family_t(AF_INET6)
        _ = inet_pton(AF_INET6, Self.linkLocal, &six.sin6_addr)
        let read = withUnsafePointer(to: &six) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                BonjourAddress(socketAddress: $0, interfaceIndex: if_nametoindex("lo0"))
            }
        }
        #expect(read == BonjourAddress(bytes: Self.v6(Self.linkLocal), interfaceName: "lo0"))
        var four = sockaddr_in()
        four.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        four.sin_family = sa_family_t(AF_INET)
        _ = inet_pton(AF_INET, Self.lan, &four.sin_addr)
        let readFour = withUnsafePointer(to: &four) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                BonjourAddress(socketAddress: $0, interfaceIndex: 0)
            }
        }
        #expect(readFour == BonjourAddress(bytes: Self.v4(Self.lan), interfaceName: nil))
    }

    @Test func aFoundCoreDialsEveryAddressItWasFoundAt() {
        var found = FoundStation(instanceName: "Rock", label: "KG4VCF/rock", identityPrefix: "EBceJSwzOkFIT1ZdZGtyeY",
                                 claimed: true, pairing: .code,
                                 endpoint: StationEndpoint(host: Self.lan, port: 50055))
        #expect(found.dialable == [StationEndpoint(host: Self.lan, port: 50055)])
        found.endpoints = [StationEndpoint(host: Self.uniqueLocal, port: 50055),
                           StationEndpoint(host: Self.lan, port: 50055),
                           StationEndpoint(host: Self.end1, port: 50055)]
        #expect(found.dialable == [StationEndpoint(host: Self.end1, port: 50055),
                                   StationEndpoint(host: Self.uniqueLocal, port: 50055),
                                   StationEndpoint(host: Self.lan, port: 50055)])
        found.endpoint = nil
        found.endpoints = []
        #expect(found.dialable.isEmpty)
    }

    // MARK: The Core's own list (devices' coreAddresses)

    @Test func theCoresListReadsAsTheNoteWritesIt() throws {
        let list = try #require(CoreAddressList.parse(
            #"{"addresses":["[2001:db8:1:0:211:22ff:fe33:4455]:47910","1.2.3.4:47910"]}"#))
        #expect(list == [StationEndpoint(host: "2001:db8:1:0:211:22ff:fe33:4455", port: 47910),
                         StationEndpoint(host: "1.2.3.4", port: 47910)])
        let rock = try #require(CoreAddressList.parse(
            "{\"addresses\":[\"[\(Self.end1)]:50055\",\"[\(Self.wlan0)]:50055\"]}"))
        #expect(rock == [StationEndpoint(host: Self.end1, port: 50055), StationEndpoint(host: Self.wlan0, port: 50055)])
        // A Core that listens nowhere, or has none: nothing new.
        #expect(CoreAddressList.parse(#"{"addresses":[]}"#) == [])
    }

    /// The value the Rock's installed Core (trunk 2fcd67770) sends a signed-in
    /// device, byte for byte in shape: two bracketed global IPv6 addresses
    /// with the control port, wlan0 first, and nothing temporary, deprecated,
    /// link-local, loopback or private. The addresses are documentation-range
    /// stand-ins for the real ones.
    static let installedCoreValue =
        #"{"addresses":["[2001:db8:467f:66e7:8a00:44ff:fe00:4602]:50055","[2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2]:50055"]}"#

    @Test func theInstalledCoresValueReadsInItsOrderAndIsKept() throws {
        let list = try #require(CoreAddressList.parse(Self.installedCoreValue))
        #expect(list == [StationEndpoint(host: Self.wlan0, port: 50055), StationEndpoint(host: Self.end1, port: 50055)])
        #expect(list.allSatisfy { LocalNetworks.isGlobal($0.host) })
        var station = Self.rock()
        station.keepCoreAddresses(list)
        #expect(station.directAddresses == list)
        #expect(station.endpoints == [StationEndpoint(host: Self.lan, port: 50055)])
    }

    @Test func anEntryTheAppCannotDialIsLeftOut() throws {
        let list = try #require(CoreAddressList.parse("""
            {"addresses":["2001:db8::1:47910","[2001:db8::2]","[2001:db8::3]:0","[2001:db8::4]:70000",\
            "rock.example.net:47910","[fe80::1]:47910","[fd00::1]:47910","192.168.109.106:50055",\
            "[2001:db8::5]:47910",7,"203.0.113.7:x"]}
            """))
        #expect(list == [StationEndpoint(host: "2001:db8::5", port: 47910)])
        #expect(CoreAddressList.parse("not json") == nil)
        #expect(CoreAddressList.parse(#"{"list":[]}"#) == nil)
        #expect(CoreAddressList.parse(#"["[2001:db8::5]:47910"]"#) == nil)
    }

    @Test func atMostEightAreKept() throws {
        let entries = (1...10).map { "\"[2001:db8::\($0)]:47910\"" }.joined(separator: ",")
        let list = try #require(CoreAddressList.parse("{\"addresses\":[\(entries)]}"))
        #expect(list.count == CoreAddressList.maximumCount)
        #expect(CoreAddressList.maximumCount == 8)
        #expect(list.first == StationEndpoint(host: "2001:db8::1", port: 47910))
    }

    @Test func theListIsOfferedAtMinorElevenAndVersionOne() {
        #expect(CoreAddressList.featureName == "coreAddresses")
        #expect(CoreAddressList.capabilityName == "coreAddressesVersion")
        #expect(CoreAddressList.propertyName == "coreAddresses")
        #expect(CoreAddressList.isOffered(agreedMinor: 11, capabilityVersion: 1))
        #expect(CoreAddressList.isOffered(agreedMinor: 12, capabilityVersion: 2))
        #expect(!CoreAddressList.isOffered(agreedMinor: 10, capabilityVersion: 1))
        #expect(!CoreAddressList.isOffered(agreedMinor: 11, capabilityVersion: 0))
        #expect(LinkFeatures.app[CoreAddressList.featureName] == 1)
    }

    // MARK: Kept with the paired Core

    private static func rock() -> PairedStation {
        PairedStation(identityKey: TestStationIdentity().publicKey, label: "KG4VCF/rock",
                      endpoints: [StationEndpoint(host: Self.lan, port: 50055)], lastPath: "thisNetwork")
    }

    @Test func eachListFromTheCoreReplacesTheLastAndAnEmptyOneChangesNothing() {
        var station = Self.rock()
        #expect(station.directAddresses.isEmpty)
        station.keepCoreAddresses([StationEndpoint(host: Self.end1, port: 50055),
                                   StationEndpoint(host: Self.wlan0, port: 50055)])
        #expect(station.directAddresses == [StationEndpoint(host: Self.end1, port: 50055),
                                            StationEndpoint(host: Self.wlan0, port: 50055)])
        station.keepCoreAddresses([])
        #expect(station.directAddresses.count == 2)
        station.keepCoreAddresses([StationEndpoint(host: "2001:db8:467f:1::15f2", port: 50055)])
        #expect(station.directAddresses == [StationEndpoint(host: "2001:db8:467f:1::15f2", port: 50055)])
        // Where the phone reached the Core is kept apart.
        #expect(station.endpoints == [StationEndpoint(host: Self.lan, port: 50055)])
    }

    @Test func provenGlobalAddressesJoinTheListAndNeverPushOutWhereTheCoreWasReached() {
        var station = Self.rock()
        station.endpoints = [StationEndpoint(host: Self.lan, port: 50055), StationEndpoint(host: "shack.example.net"),
                             StationEndpoint(host: "192.0.2.1"), StationEndpoint(host: "192.0.2.2")]
        station.addProvenAddresses([StationEndpoint(host: Self.end1, port: 50055),
                                    StationEndpoint(host: Self.uniqueLocal, port: 50055),
                                    StationEndpoint(host: "\(Self.linkLocal)%en0", port: 50055),
                                    StationEndpoint(host: Self.lan, port: 50055)])
        // Only a global address is kept to dial from anywhere.
        #expect(station.directAddresses == [StationEndpoint(host: Self.end1, port: 50055)])
        #expect(station.endpoints.count == 4)
        station.addProvenAddresses([StationEndpoint(host: Self.wlan0, port: 50055),
                                    StationEndpoint(host: Self.end1.uppercased(), port: 50055)])
        #expect(station.directAddresses == [StationEndpoint(host: Self.wlan0, port: 50055),
                                            StationEndpoint(host: Self.end1, port: 50055)])
        station.addProvenAddresses((1...9).map { StationEndpoint(host: "2001:db8::\($0)", port: 50055) })
        #expect(station.directAddresses.count == CoreAddressList.maximumCount)
    }

    /// The Core's own list is what it says it can be dialled at (contract
    /// section 7.1, `coreAddresses`): an address this phone proved joins
    /// it, first, but never pushes one of the Core's out. Only the Core's
    /// next list replaces them.
    @Test func provenAddressesNeverPushOutTheCoresOwnList() throws {
        var station = Self.rock()
        let listed = (1...8).map { StationEndpoint(host: "2001:db8:c::\($0)", port: 50055) }
        station.keepCoreAddresses(listed)
        station.addProvenAddresses([StationEndpoint(host: Self.end1, port: 50055)])
        #expect(station.directAddresses == listed)

        station.keepCoreAddresses(Array(listed.prefix(3)))
        let proven = (1...7).map { StationEndpoint(host: "2001:db8:a::\($0)", port: 50055) }
        station.addProvenAddresses(proven)
        #expect(station.directAddresses.count == CoreAddressList.maximumCount)
        #expect(station.directAddresses == Array(proven.prefix(5)) + Array(listed.prefix(3)))

        // Kept as it was through the Keychain's copy.
        let read = try JSONDecoder().decode(PairedStation.self, from: JSONEncoder().encode(station))
        var again = read
        again.addProvenAddresses([StationEndpoint(host: Self.end1, port: 50055)])
        #expect(Array(again.directAddresses.suffix(3)) == Array(listed.prefix(3)))
    }

    @Test func whatOlderBuildsKeptReadsWithNoDirectAddresses() throws {
        let station = Self.rock()
        var older = try #require(try JSONSerialization.jsonObject(with: JSONEncoder().encode(station))
                                 as? [String: Any])
        older["directAddresses"] = nil
        let read = try JSONDecoder().decode(PairedStation.self,
                                            from: JSONSerialization.data(withJSONObject: older))
        #expect(read.directAddresses.isEmpty)
        var kept = station
        kept.keepCoreAddresses([StationEndpoint(host: Self.end1, port: 50055)])
        #expect(try JSONDecoder().decode(PairedStation.self, from: JSONEncoder().encode(kept)) == kept)
    }

    @Test func theStoreKeepsBothKinds() throws {
        let store = PairedStationStore(item: InMemorySecretItem())
        let station = Self.rock()
        try store.save(station)
        let kept = try store.recordCoreAddresses(identityKey: station.identityKey,
                                                 [StationEndpoint(host: Self.end1, port: 50055)])
        #expect(kept?.directAddresses == [StationEndpoint(host: Self.end1, port: 50055)])
        let proven = try store.recordProvenAddresses(identityKey: station.identityKey,
                                                     [StationEndpoint(host: Self.wlan0, port: 50055)])
        #expect(proven?.directAddresses == [StationEndpoint(host: Self.wlan0, port: 50055),
                                            StationEndpoint(host: Self.end1, port: 50055)])
        #expect(try store.station(identityKey: station.identityKey)?.directAddresses.count == 2)
        #expect(try store.recordCoreAddresses(identityKey: Data([1]), []) == nil)
    }

    // MARK: Seen in an introduction through the service

    /// An answer from the Core as the service passes it on: host candidates
    /// on a global IPv6 and a public IPv4 address, then every kind that is
    /// never kept (reflexive, relay, unique local, link-local, private IPv4,
    /// an mDNS name, and a host on an IPv4 documentation range, which the
    /// Core never lists). Documentation-range stand-ins, but for the public
    /// IPv4 host, which cannot be one: 1.2.3.44 stands in for it.
    static let answer = """
        v=0
        o=- 1 1 IN IP4 0.0.0.0
        s=-
        a=candidate:1 1 UDP 2122317823 \(end1) 50001 typ host
        a=candidate:2 1 UDP 2122252543 1.2.3.44 50002 typ host
        a=candidate:3 1 UDP 1686052607 198.51.100.9 60001 typ srflx raddr 0.0.0.0 rport 0
        a=candidate:4 1 UDP 41885439 203.0.113.200 3478 typ relay raddr 0.0.0.0 rport 0
        a=candidate:5 1 UDP 2122187007 \(uniqueLocal) 50003 typ host
        a=candidate:6 1 UDP 2122121471 \(linkLocal) 50004 typ host
        a=candidate:7 1 UDP 2122055935 \(lan) 50005 typ host
        a=candidate:8 1 UDP 2121990399 8f6c1d2e-0b1a-4c3d-9e8f-123456789abc.local 50006 typ host
        a=candidate:10 1 UDP 2121924863 203.0.113.44 50009 typ host
        a=candidate:9 1 TCP 1518280447 2001:db8:467f:66e7::99 9 typ host tcptype active
        """

    @Test func onlyGlobalHostCandidatesAreKeptWithTheControlPort() {
        let seen = CoreAddressList.introducedAddresses([Self.answer], port: 50055)
        #expect(seen == [StationEndpoint(host: Self.end1, port: 50055),
                         StationEndpoint(host: "1.2.3.44", port: 50055)])
        // A trickled candidate reads as one in the answer does, and one
        // seen twice is kept once.
        let trickled = CoreAddressList.introducedAddresses(
            [Self.answer, "candidate:10 1 UDP 2122000000 \(Self.wlan0) 50007 typ host",
             "candidate:11 1 UDP 2122000000 \(Self.end1.uppercased()) 50008 typ host", ""], port: 50055)
        #expect(trickled == [StationEndpoint(host: Self.end1, port: 50055),
                             StationEndpoint(host: "1.2.3.44", port: 50055),
                             StationEndpoint(host: Self.wlan0, port: 50055)])
        #expect(CoreAddressList.introducedAddresses([], port: 50055).isEmpty)
        #expect(CoreAddressList.introducedAddresses(["v=0\ns=-"], port: 50055).isEmpty)
    }

    @Test func theControlPortIsTheOneThePhoneDialsTheCoreOn() {
        var station = Self.rock()
        station.endpoints = [StationEndpoint(host: "shack.example.net", port: 40000)]
        #expect(station.directDialPort == 40000)
        station.lastGood = StationEndpoint(host: Self.lan, port: 50055)
        station.endpoints.append(StationEndpoint(host: Self.lan, port: 50055))
        #expect(station.directDialPort == 50055)
        // The Core's own list names the port its listener holds.
        station.keepCoreAddresses([StationEndpoint(host: Self.end1, port: 50066)])
        #expect(station.directDialPort == 50066)
        let unreached = PairedStation(identityKey: Data([1]), label: "Mailbox", endpoints: [])
        #expect(unreached.directDialPort == StationEndpoint.defaultPort)
    }

    @Test func addressesProvedFromAnIntroductionJoinTheLearnedListAndTheCoresListStillReplacesIt() throws {
        let store = PairedStationStore(item: InMemorySecretItem())
        let station = Self.rock()
        try store.save(station)
        _ = try store.recordCoreAddresses(identityKey: station.identityKey,
                                          [StationEndpoint(host: Self.wlan0, port: 50055)])
        let seen = CoreAddressList.introducedAddresses([Self.answer], port: 50055)
        var kept = try #require(try store.recordProvenAddresses(identityKey: station.identityKey, seen))
        #expect(kept.directAddresses == [StationEndpoint(host: Self.end1, port: 50055),
                                         StationEndpoint(host: "1.2.3.44", port: 50055),
                                         StationEndpoint(host: Self.wlan0, port: 50055)])
        // Nothing seen changes nothing.
        kept = try #require(try store.recordProvenAddresses(
            identityKey: station.identityKey, CoreAddressList.introducedAddresses([], port: 50055)))
        #expect(kept.directAddresses.count == 3)
        // At most eight, newest first.
        let many = (1...9).map { "candidate:\($0) 1 UDP 1 2001:db8:5::\($0) 5000\($0) typ host" }
        kept = try #require(try store.recordProvenAddresses(
            identityKey: station.identityKey, CoreAddressList.introducedAddresses(many, port: 50055)))
        #expect(kept.directAddresses.count == CoreAddressList.maximumCount)
        #expect(kept.directAddresses.first == StationEndpoint(host: "2001:db8:5::1", port: 50055))
        // The Core's own list, when a session brings it, replaces them all.
        kept = try #require(try store.recordCoreAddresses(identityKey: station.identityKey,
                                                          [StationEndpoint(host: Self.end1, port: 50055)]))
        #expect(kept.directAddresses == [StationEndpoint(host: Self.end1, port: 50055)])
    }
}
