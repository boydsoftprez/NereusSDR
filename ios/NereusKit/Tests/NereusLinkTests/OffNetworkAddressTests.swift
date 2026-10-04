// NereusSDR for iOS: which of a Core's addresses a phone can reach from the network it is on
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusLink

/// R-IOS-16 (2026-09-29, JJ's build 13 on 5G): a private address learned on
/// the Core's home network is never raced from a network it cannot be on,
/// and the attempt record says how each path ended and which way it went.
@Suite struct OffNetworkAddressTests {
    private static func v6(_ groups: [UInt16]) -> [UInt8] {
        groups.flatMap { [UInt8($0 >> 8), UInt8($0 & 0xFF)] }
    }

    /// The phone on cellular behind NAT64: a global IPv6 address on the
    /// cellular interface, the 464XLAT CLAT address and loopback.
    private static let cellular = LocalNetworks(entries: [
        LocalNetworks.Entry(address: v6([0x2001, 0xdb8, 0x7700, 0x48, 0, 0, 0, 5]), prefixLength: 64),
        LocalNetworks.Entry(address: v6([0xfe80, 0, 0, 0, 0, 0, 0, 5]), prefixLength: 64),
        LocalNetworks.Entry(address: [192, 0, 0, 2], prefixLength: 32),
        LocalNetworks.Entry(address: [127, 0, 0, 1], prefixLength: 8, isLoopback: true),
    ])

    /// The phone on the Core's home Wi-Fi.
    private static let home = LocalNetworks(entries: [
        LocalNetworks.Entry(address: [192, 168, 109, 40], prefixLength: 24),
        LocalNetworks.Entry(address: v6([0x2001, 0xdb8, 0x467f, 0x66e7, 0, 0, 0, 0x40]), prefixLength: 64),
    ])

    @Test func aPrivateLanAddressIsNotReachedFromCellular() {
        #expect(!Self.cellular.mayReach("192.168.109.106"))
        #expect(!Self.cellular.mayReach("10.0.252.47"))
        #expect(!Self.cellular.mayReach("172.20.1.1"))
        #expect(!Self.cellular.mayReach("100.100.1.1"))
        #expect(!Self.cellular.mayReach("169.254.3.3"))
        #expect(!Self.cellular.mayReach("fd12:3456::1"))
        #expect(!Self.cellular.mayReach("::ffff:192.168.109.106"))
    }

    @Test func globalAddressesNamesAndThisNetworkAreReached() {
        #expect(Self.cellular.mayReach("2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2"))
        #expect(Self.cellular.mayReach("[2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2]"))
        #expect(Self.cellular.mayReach("203.0.113.67"))
        #expect(Self.cellular.mayReach("rock.example.net"))
        #expect(Self.cellular.mayReach("127.0.0.1"))
        #expect(Self.home.mayReach("192.168.109.106"))
        #expect(Self.home.mayReach("2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2"))
    }

    @Test func aVpnMayCarryAPrivateAddressOfItsOwnFamily() {
        var overlay = Self.cellular
        overlay.entries.append(LocalNetworks.Entry(address: [10, 147, 17, 5], prefixLength: 24, isTunnel: true))
        #expect(overlay.mayReach("192.168.109.106"))
        #expect(!overlay.mayReach("fd12:3456::1"))
        #expect(!overlay.mayReach("169.254.3.3"))
        // The system's own tunnels carry only link-local addresses.
        var system = Self.cellular
        system.entries.append(LocalNetworks.Entry(address: Self.v6([0xfe80, 0, 0, 0, 0, 0, 0, 9]),
                                                  prefixLength: 64, isTunnel: true))
        #expect(!system.mayReach("fd12:3456::1"))
        system.entries.append(LocalNetworks.Entry(address: Self.v6([0xfd7a, 0x115c, 0xa1e0, 0, 0, 0, 0, 1]),
                                                  prefixLength: 128, isTunnel: true))
        #expect(system.mayReach("fd12:3456::1"))
    }

    @Test func aTunnelInterfaceIsKnownByItsName() {
        #expect(LocalNetworks.isTunnelInterface("utun4"))
        #expect(LocalNetworks.isTunnelInterface("ipsec0"))
        #expect(LocalNetworks.isTunnelInterface("ppp0"))
        #expect(!LocalNetworks.isTunnelInterface("pdp_ip0"))
        #expect(!LocalNetworks.isTunnelInterface("en0"))
    }

    @Test func theServicePathSaysItWentThroughTheService() {
        var attempt = ConnectionAttempt(tries: [
            .init(path: .direct, address: "[2001:db8:467f:66e7::15f2]:50055", outcome: .refused),
            .init(path: .direct, address: "203.0.113.67:50055", outcome: .unreachable),
            .init(path: .direct, address: "rv.nereussdr.com", outcome: .connected, throughService: true),
        ])
        #expect(attempt.summary == "Tried direct ([2001:db8:467f:66e7::15f2]:50055): the connection was refused; "
                + "direct (203.0.113.67:50055): no route to it from this network; "
                + "through the internet service (rv.nereussdr.com): connected.")
        attempt.tries[2].path = .relay
        #expect(attempt.summary.hasSuffix("relay through the internet service (rv.nereussdr.com): connected."))
        #expect(ConnectionAttempt.Try(path: .direct, address: "x").throughService == false)
    }
}
