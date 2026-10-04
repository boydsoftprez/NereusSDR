// NereusSDR for iOS: tests for finding Cores over Bonjour: the TXT record, the identity prefix and a real browse
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import Network
import Testing
@testable import NereusLink

/// D36, D71, R-IOS-16, link document section 14.2: the record a Core
/// registers, read as the Core writes it (the `media-dnssd-txt` vector, read
/// from `tests/data/link/v1` at run time and never bundled), the records the
/// app can't read, and on macOS a record registered with `dns-sd -R` found by
/// the browser.
@Suite struct StationBrowserTests {
    private static func record(_ entries: [(String, String)]) -> Data {
        var data = Data()
        for (key, value) in entries {
            let entry = Data("\(key)=\(value)".utf8)
            data.append(UInt8(entry.count))
            data.append(entry)
        }
        return data
    }

    private static let goodId = "EBceJSwzOkFIT1ZdZGtyeY"

    private static func good(_ changes: [String: String?] = [:]) -> [(String, String)] {
        var entries: [(String, String)] = [("v", "1"), ("id", goodId), ("claimed", "0"), ("pair", "click"),
                                            ("name", "KG4VCF/shack")]
        for (key, value) in changes {
            entries.removeAll { $0.0 == key }
            if let value {
                entries.append((key, value))
            }
        }
        return entries
    }

    @Test func theCoresVectorReadsAsItsExpectationSays() throws {
        let vector = try #require(try LinkFixtureLoader.mediaVectors()["media-dnssd-txt"])
        #expect(vector.codec == "dnssd-txt")
        #expect(vector.expect["serviceType"] as? String == StationBrowser.serviceType)
        let txt = try #require(vector.expect["txt"] as? [String: String])
        #expect(FoundStation.entries(of: vector.bytes) == txt)
        // The seven entries, in the fixed order, each after a one-byte length:
        // the five every Core sends, the device count a Core with several
        // devices adds, which the app reads from Task 56c, and the radio's
        // state.
        var keys: [String] = []
        var index = 0
        let bytes = [UInt8](vector.bytes)
        while index < bytes.count {
            let length = Int(bytes[index])
            let entry = String(decoding: bytes[(index + 1)..<(index + 1 + length)], as: UTF8.self)
            keys.append(String(entry.prefix { $0 != "=" }))
            index += 1 + length
        }
        #expect(keys == ["v", "id", "claimed", "pair", "name", "devices", "radio"])

        let station = try #require(FoundStation.parse(txtRecord: vector.bytes, instanceName: "KG4VCF/shack (2)"))
        #expect(station.claimed == (txt["claimed"] == "1"))
        #expect(station.pairing.rawValue == txt["pair"])
        #expect(station.label == txt["name"])
        #expect(station.identityPrefix == txt["id"])
        #expect(station.instanceName == "KG4VCF/shack (2)")
        #expect(station.displayName == txt["name"])
        #expect(station.endpoint == nil)
        // The count of devices on the Core (ruling 10.4).
        #expect(station.devices == Int(try #require(txt["devices"])))
        // The radio's state (link section 14.2).
        #expect(station.radio?.rawValue == txt["radio"])
        #expect(station.radio == .connected)
    }

    /// `radio` is `offline`, `connected` or `waiting`; anything else, or
    /// none (an older Core), is a state not known, and the Core still lists.
    @Test func theRadioStateReadsItsThreeWords() throws {
        for state in [FoundStation.RadioState.offline, .connected, .waiting] {
            let station = try #require(FoundStation.parse(
                txtRecord: Self.record(Self.good() + [("radio", state.rawValue)]), instanceName: "Core"))
            #expect(station.radio == state)
        }
        for text in ["", "Waiting", "online", "1"] {
            let station = try #require(FoundStation.parse(txtRecord: Self.record(Self.good() + [("radio", text)]),
                                                          instanceName: "Core"))
            #expect(station.radio == nil, "\(text)")
        }
        let older = try #require(FoundStation.parse(txtRecord: Self.record(Self.good()), instanceName: "Core"))
        #expect(older.radio == nil)
    }

    /// `devices` is a count from 0 to 4; anything else, or none, is no count,
    /// and the Core still lists.
    @Test func theDeviceCountReadsFromZeroToFour() throws {
        for count in 0...4 {
            let station = try #require(FoundStation.parse(txtRecord: Self.record(Self.good() + [("devices", "\(count)")]),
                                                          instanceName: "Core"))
            #expect(station.devices == count)
        }
        for text in ["5", "-1", "", "04", "two", "1.0"] {
            let station = try #require(FoundStation.parse(txtRecord: Self.record(Self.good() + [("devices", text)]),
                                                          instanceName: "Core"))
            #expect(station.devices == nil, "\(text)")
        }
        let older = try #require(FoundStation.parse(txtRecord: Self.record(Self.good()), instanceName: "Core"))
        #expect(older.devices == nil)
    }

    @Test func theIdentityPrefixIsTheFingerprintsFirstTwentyTwoCharacters() throws {
        let vector = try #require(try LinkFixtureLoader.mediaVectors()["media-dnssd-txt"])
        let txt = try #require(vector.expect["txt"] as? [String: String])
        // The same Core's LAN announcement carries the whole fingerprint.
        let announcement = try #require(try LinkFixtureLoader.mediaVectors()["media-lan-announcement-2"])
        let identity = try #require(announcement.expect["identity"] as? String)
        #expect(String(identity.prefix(22)) == txt["id"])

        // A paired Core's key matches the record made from it, and no other.
        let key = Data((0..<91).map { UInt8($0) })
        let prefix = FoundStation.identityPrefix(of: key)
        #expect(prefix.count == 22)
        let found = try #require(FoundStation.parse(txt: ["v": "1", "id": prefix, "claimed": "1", "pair": "code"],
                                                    instanceName: "Core"))
        #expect(found.matches(identityKey: key))
        #expect(!found.matches(identityKey: Data(key.reversed())))
    }

    @Test func eachPairingStateReads() throws {
        for (claimed, pair) in [("0", "click"), ("0", "code"), ("1", "code"), ("0", "closed"), ("1", "closed")] {
            let station = try #require(FoundStation.parse(
                txtRecord: Self.record(Self.good(["claimed": claimed, "pair": pair])), instanceName: "Core"))
            #expect(station.claimed == (claimed == "1"))
            #expect(station.pairing == FoundStation.Pairing(rawValue: pair))
        }
    }

    @Test func aKeyTheAppDoesNotKnowIsIgnored() throws {
        let newer = Self.good() + [("devices", "2"), ("later", "x")]
        let station = try #require(FoundStation.parse(txtRecord: Self.record(newer), instanceName: "Core"))
        #expect(station.label == "KG4VCF/shack")
        #expect(station.pairing == .click)
        // Keys are read without case, and the first of a repeat counts.
        let shouted = [("V", "1"), ("ID", Self.goodId), ("Claimed", "1"), ("PAIR", "code"), ("pair", "click")]
        let read = try #require(FoundStation.parse(txtRecord: Self.record(shouted), instanceName: "Core"))
        #expect(read.claimed && read.pairing == .code && read.label.isEmpty)
    }

    @Test("a record the app can't read", arguments: [
        ["v": "2"], ["v": nil], ["v": ""], ["id": nil], ["id": "short"], ["id": "EBceJSwzOkFIT1ZdZGtye="],
        ["id": "EBceJSwzOkFIT1ZdZGtyeYX"], ["claimed": nil], ["claimed": "yes"], ["claimed": "2"],
        ["pair": nil], ["pair": "open"], ["pair": "CLICK"],
    ] as [[String: String?]])
    func unreadableRecords(_ changes: [String: String?]) {
        #expect(FoundStation.parse(txtRecord: Self.record(Self.good(changes)), instanceName: "Core") == nil)
    }

    @Test func anEmptyOrMissingNameShowsTheInstanceName() throws {
        let empty = try #require(FoundStation.parse(txtRecord: Self.record(Self.good(["name": ""])),
                                                    instanceName: "NereusSDR Core"))
        #expect(empty.displayName == "NereusSDR Core")
        let missing = try #require(FoundStation.parse(txtRecord: Self.record(Self.good(["name": nil])),
                                                      instanceName: "Shack Core"))
        #expect(missing.label.isEmpty && missing.displayName == "Shack Core")
    }

    @Test func aTruncatedOrMalformedRecordKeepsWhatReads() {
        var truncated = Self.record(Self.good())
        truncated.append(40)
        truncated.append(contentsOf: Data("pair=code".utf8))
        let entries = FoundStation.entries(of: truncated)
        #expect(entries["pair"] == "click")
        var odd = Data([0, 3]) + Data("=ab".utf8)
        odd.append(4)
        odd.append(contentsOf: Data("flag".utf8))
        odd.append(contentsOf: Self.record([("name", "x")]))
        #expect(FoundStation.entries(of: odd) == ["name": "x"])
        #expect(FoundStation.entries(of: Data([7]) + Data([0x6E, 0x61, 0x6D, 0x65, 0x3D, 0xFF, 0xFE])).isEmpty)
    }

    @Test func resolvedEndpointsAreKeptAsATypedAddressIs() throws {
        let v4 = try #require(StationBrowser.stationEndpoint(.hostPort(host: "192.0.2.10", port: 50055)))
        #expect(v4 == StationEndpoint(host: "192.0.2.10", port: 50055))
        let v6 = try #require(StationBrowser.stationEndpoint(.hostPort(host: "2001:DB8::10", port: 47910)))
        #expect(v6 == StationEndpoint(host: "2001:db8::10", port: 47910))
        let name = try #require(StationBrowser.stationEndpoint(.hostPort(host: "Shack.local.", port: 47910)))
        #expect(name == StationEndpoint(host: "shack.local", port: 47910))
        // A link-local address keeps its zone, which the transport escapes.
        let zoned = try #require(IPv6Address("fe80::1%lo0"))
        let linkLocal = try #require(StationBrowser.stationEndpoint(.hostPort(host: .ipv6(zoned), port: 47910)))
        #expect(linkLocal == StationEndpoint(host: "fe80::1%lo0", port: 47910))
        #expect(ManualAddress.parse(host: linkLocal.host, port: "47910") == linkLocal)
        #expect(StationBrowser.stationEndpoint(.service(name: "x", type: StationBrowser.serviceType, domain: "local.",
                                                        interface: nil)) == nil)
    }

    #if os(macOS)
    /// A record registered with `dns-sd -R`, on a port of its own, is found
    /// and resolved to that port.
    @Test(.timeLimit(.minutes(1))) func aRegisteredCoreIsFoundAndResolved() async throws {
        let tool = URL(fileURLWithPath: "/usr/bin/dns-sd")
        guard FileManager.default.isExecutableFile(atPath: tool.path) else {
            Issue.record("dns-sd is missing")
            return
        }
        let port = UInt16.random(in: 40_000...49_000)
        let instance = "NereusSDR test \(UUID().uuidString.prefix(8))"
        let label = "Test \(UUID().uuidString.prefix(6))"
        let id = FoundStation.identityPrefix(of: Data(UUID().uuidString.utf8))
        let process = Process()
        process.executableURL = tool
        process.arguments = ["-R", instance, StationBrowser.serviceType, "local", String(port), "v=1", "id=\(id)",
                             "claimed=0", "pair=click", "name=\(label)", "devices=0"]
        process.standardOutput = FileHandle.nullDevice
        process.standardError = FileHandle.nullDevice
        try process.run()
        defer { process.terminate() }

        let browser = StationBrowser()
        let (stream, continuation) = AsyncStream.makeStream(of: StationBrowser.Update.self)
        await browser.start { continuation.yield($0) }
        var found: FoundStation?
        for await update in stream {
            if let station = update.stations.first(where: { $0.identityPrefix == id }) {
                found = station
                break
            }
        }
        await browser.stop()
        let station = try #require(found)
        #expect(station.instanceName == instance)
        #expect(station.label == label)
        #expect(station.displayName == label)
        #expect(!station.claimed && station.pairing == .click)
        #expect(station.endpoint?.port == port)
        #expect(station.endpoint?.host.isEmpty == false)
    }

    /// A Core whose host Bonjour answers with an address of its own
    /// (`dns-sd -P`, a proxy registration naming the host and its address)
    /// is found with that address among those it is dialled at, with the
    /// service's port: the browser resolves the host's own records, not only
    /// the one address a path to the service picks (R-IOS-16).
    @Test(.timeLimit(.minutes(1))) func everyAddressOfTheCoresHostIsResolved() async throws {
        let tool = URL(fileURLWithPath: "/usr/bin/dns-sd")
        guard FileManager.default.isExecutableFile(atPath: tool.path) else {
            Issue.record("dns-sd is missing")
            return
        }
        let port = UInt16.random(in: 40_000...49_000)
        let tag = UUID().uuidString.prefix(8).lowercased()
        let instance = "NereusSDR host test \(tag)"
        let host = "nereus-test-\(tag).local"
        let address = "2001:db8:4e52::\(Int.random(in: 1...0xfff))"
        let id = FoundStation.identityPrefix(of: Data(UUID().uuidString.utf8))
        let process = Process()
        process.executableURL = tool
        process.arguments = ["-P", instance, StationBrowser.serviceType, "local", String(port), host, address,
                             "v=1", "id=\(id)", "claimed=1", "pair=code", "name=Test \(tag)"]
        process.standardOutput = FileHandle.nullDevice
        process.standardError = FileHandle.nullDevice
        try process.run()
        defer { process.terminate() }

        let browser = StationBrowser()
        let (stream, continuation) = AsyncStream.makeStream(of: StationBrowser.Update.self)
        await browser.start { continuation.yield($0) }
        var found: FoundStation?
        for await update in stream {
            if let station = update.stations.first(where: { $0.identityPrefix == id }),
               station.endpoints.contains(StationEndpoint(host: address, port: port)) {
                found = station
                break
            }
        }
        await browser.stop()
        let station = try #require(found)
        #expect(station.dialable.contains(StationEndpoint(host: address, port: port)))
    }
    #endif
}
