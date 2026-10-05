// NereusSDR for iOS: the real Keychain keeps the device key and the paired Cores
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import Testing

/// The Keychain tests that must run. NereusKit's own tests of the real
/// Keychain are turned off where their process has no Keychain, which is
/// every package test run, on a Mac and on the simulator alike. This bundle
/// runs inside the app, which has the Keychain, so nothing here is gated:
/// a fault in ``KeychainItem`` fails these tests instead of hiding them.
@Suite("The real Keychain")
struct RealKeychainTests {
    private static let message = Data("a sign-in transcript".utf8)

    @Test("The device key is made once and kept for a new store")
    func theRealKeychainKeepsTheKey() throws {
        let item = KeychainItem(service: "NereusSDR tests", account: "device-key-\(UUID().uuidString)")
        defer { try? item.delete() }
        let first = try DeviceIdentity.load(store: KeychainKeyStore(item: item))
        let again = try DeviceIdentity.load(store: KeychainKeyStore(item: item))
        #expect(again.publicKey == first.publicKey)
        #expect(P256Wire.isCanonicalKey(again.publicKey))
        let signature = try again.signature(for: Self.message)
        #expect(P256Wire.verify(signature: signature, over: Self.message, spki: first.publicKey))
    }

    @Test("A paired Core is kept for a new store")
    func theRealKeychainKeepsTheCores() throws {
        let item = KeychainItem(service: "NereusSDR tests", account: "paired-cores-\(UUID().uuidString)")
        defer { try? item.delete() }
        let station = try FakeStation(fixture: "session-device-sign-in")
        let shack = PairedStation(identityKey: station.identity.publicKey, label: "KG4VCF/shack",
                                  endpoints: [StationEndpoint(host: "192.0.2.10"),
                                              StationEndpoint(host: "shack.example.net", port: 47911)])
        try PairedStationStore(item: item).save(shack)
        #expect(try PairedStationStore(item: item).all() == [shack])
    }
}
