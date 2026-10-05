// NereusSDR for iOS: tests for reading a typed Core address
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusLink

@Suite struct ManualAddressTests {
    @Test(arguments: [
        ("shack.example.net", "shack.example.net", UInt16(47910)),
        ("shack.example.net:5000", "shack.example.net", 5000),
        ("192.0.2.7", "192.0.2.7", 47910),
        ("192.0.2.7:5000", "192.0.2.7", 5000),
        ("[2001:db8::1]", "2001:db8::1", 47910),
        ("[2001:db8::1]:5000", "2001:db8::1", 5000),
        ("  shack.example.net  ", "shack.example.net", 47910),
        ("[fe80::1%en0]:1", "fe80::1%en0", 1),
        ("host:65535", "host", 65535),
    ])
    func accepts(text: String, host: String, port: UInt16) {
        #expect(ManualAddress.parse(text) == StationEndpoint(host: host, port: port))
    }

    @Test(arguments: [
        "2001:db8::1", "host:0", "host:65536", "", "   ", "http://host", "host:", "[2001:db8::1",
        "[2001:db8::1]5000", "[192.0.2.7]", "host:+80", "host:80:81", "-host", "host name",
        "192.0.2", "192.0.2.256", "host/path", "user@host",
    ])
    func rejects(text: String) {
        #expect(ManualAddress.parse(text) == nil)
    }

    // MARK: The address and the port apart (D69)

    @Test(arguments: [
        ("2001:db8::10", "47910", "2001:db8::10", UInt16(47910)),
        ("[2001:db8::10]", "47910", "2001:db8::10", 47910),
        ("192.0.2.10", "47910", "192.0.2.10", 47910),
        ("core.example", "47910", "core.example", 47910),
        ("  2001:DB8:5A1:E8F0:DEA6:32FF:FE12:3456 ", " 50055 ", "2001:db8:5a1:e8f0:dea6:32ff:fe12:3456", 50055),
        ("fe80::1%en0", "1", "fe80::1%en0", 1),
        ("::1", "65535", "::1", 65535),
    ])
    func acceptsTheTwoFields(host: String, port: String, expectedHost: String, expectedPort: UInt16) {
        #expect(ManualAddress.parse(host: host, port: port)
            == StationEndpoint(host: expectedHost, port: expectedPort))
    }

    @Test(arguments: [
        ("2001:db8::10", "0"), ("2001:db8::10", "65536"), ("2001:db8::10", ""), ("2001:db8::10", "port"),
        ("2001:db8::10", "+80"), ("", "47910"), ("   ", "47910"), ("[2001:db8::10]:50055", "47910"),
        ("192.0.2.10:47910", "47910"), ("2001:db8::zz", "47910"), ("[192.0.2.10]", "47910"),
        ("[2001:db8::10", "47910"), ("192.0.2.256", "47910"), ("http://core.example", "47910"),
        ("core example", "47910"), ("-core", "47910"),
    ])
    func refusesTheTwoFields(host: String, port: String) {
        #expect(ManualAddress.parse(host: host, port: port) == nil)
    }

    @Test func thePortFieldIsAWholeNumberFrom1To65535() {
        #expect(ManualAddress.parsePort("47910") == 47910)
        #expect(ManualAddress.parsePort(" 1 ") == 1)
        #expect(ManualAddress.parsePort("65535") == 65535)
        #expect(ManualAddress.parsePort("0") == nil)
        #expect(ManualAddress.parsePort("65536") == nil)
        #expect(ManualAddress.parsePort("") == nil)
        #expect(ManualAddress.parsePort("4791O") == nil)
    }

    @Test func aPastedAddressGivesUpItsPort() {
        #expect(ManualAddress.split("[2001:db8::10]:50055") == ("2001:db8::10", "50055"))
        #expect(ManualAddress.split("192.0.2.10:47910") == ("192.0.2.10", "47910"))
        #expect(ManualAddress.split("core.example:50055") == ("core.example", "50055"))
        #expect(ManualAddress.split(" [fe80::1%en0]:1 ") == ("fe80::1%en0", "1"))
    }

    @Test func anAddressWithoutAPortKeepsNone() {
        // A bare IPv6 literal is a whole address, never an address and a port.
        #expect(ManualAddress.split("2001:db8::10") == ("2001:db8::10", nil))
        #expect(ManualAddress.split("2001:db8:5a1:e8f0:dea6:32ff:fe12:3456") == ("2001:db8:5a1:e8f0:dea6:32ff:fe12:3456", nil))
        #expect(ManualAddress.split("[2001:db8::10]") == ("2001:db8::10", nil))
        #expect(ManualAddress.split("192.0.2.10") == ("192.0.2.10", nil))
        #expect(ManualAddress.split("core.example") == ("core.example", nil))
    }

    @Test func theSplitPartsReadAsThePastedWhole() {
        for pasted in ["[2001:db8::10]:50055", "192.0.2.10:47910", "core.example:50055", "[2001:db8::10]"] {
            let parts = ManualAddress.split(pasted)
            #expect(ManualAddress.parse(host: parts.host, port: parts.port ?? "47910") == ManualAddress.parse(pasted))
        }
    }

    @Test func theDefaultPortIsTheCores() {
        #expect(StationEndpoint.defaultPort == 47910)
        #expect(StationEndpoint(host: "shack").port == 47910)
    }
}
