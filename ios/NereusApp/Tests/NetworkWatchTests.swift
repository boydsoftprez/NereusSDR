// NereusSDR for iOS: stable observations of a network generation
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

@testable import NereusSDR
import Testing

@Suite("Network watch")
struct NetworkWatchTests {
    @Test("address and gateway observations ignore callback ordering and duplicates")
    func normalizedSignature() {
        let first = NetworkSignature(
            addresses: [
                .init(interface: "en0", bytes: [192, 0, 2, 3], prefixLength: 24, scopeID: 0),
                .init(interface: "en0", bytes: [0xfe, 0x80] + Array(repeating: 0, count: 14),
                      prefixLength: 64, scopeID: 4)
            ], gateways: ["4:c0000201", "6:fe800000000000000000000000000001%en0"],
            supportsIPv4: true, supportsIPv6: true)
        let reordered = NetworkSignature(
            addresses: Array(first.addresses!).reversed() + [first.addresses!.first!],
            gateways: Array(first.gateways).reversed() + [first.gateways.first!],
            supportsIPv4: true, supportsIPv6: true)
        #expect(first == reordered)
        #expect(first != NetworkSignature(addresses: first.addresses!, gateways: first.gateways,
                                          supportsIPv4: true, supportsIPv6: false))
        #expect(first != NetworkSignature(addresses: first.addresses!, gateways: ["4:c0000202"],
                                          supportsIPv4: true, supportsIPv6: true))
        var moved = first.addresses!
        moved.insert(.init(interface: "en0", bytes: [192, 0, 2, 4], prefixLength: 24, scopeID: 0))
        #expect(first != NetworkSignature(addresses: moved, gateways: first.gateways,
                                          supportsIPv4: true, supportsIPv6: true))
        var rescoped = first.addresses!
        rescoped.remove(.init(interface: "en0", bytes: [0xfe, 0x80] + Array(repeating: 0, count: 14),
                              prefixLength: 64, scopeID: 4))
        rescoped.insert(.init(interface: "en0", bytes: [0xfe, 0x80] + Array(repeating: 0, count: 14),
                              prefixLength: 64, scopeID: 5))
        #expect(first != NetworkSignature(addresses: rescoped, gateways: first.gateways,
                                          supportsIPv4: true, supportsIPv6: true))
    }

    @Test("carrier classification coexists with topology generation")
    func carrierAndSignature() {
        let signature = NetworkSignature(addresses: [NetworkSignature.Address(interface: "en0", bytes: [192, 0, 2, 3],
                                                                            prefixLength: 24, scopeID: 0)],
                                         gateways: ["4:c0000201"], supportsIPv4: true, supportsIPv6: false)
        let wifi = NetworkPath(online: true, interfaces: ["en0"], wifi: true, signature: signature)
        let cellular = NetworkPath(online: true, interfaces: ["pdp_ip0"], cellular: true, signature: signature)
        #expect(!wifi.cellular)
        #expect(wifi.wifi && !cellular.wifi)
        #expect(cellular.cellular)
        #expect(wifi != cellular)
        #expect(cellular.signature == signature)
    }
}
