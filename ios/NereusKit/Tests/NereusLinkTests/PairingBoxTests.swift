// NereusSDR for iOS: the confirmation boxes seal, open only under their key, and carry what they should
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusLink

/// R-IOS-08: the `pair.confirm` boxes (link document section 3.6).
@Suite struct PairingBoxTests {
    private static func key() -> Data {
        Data((0..<32).map { _ in UInt8.random(in: 0...255) })
    }

    @Test func aBoxIsANonceThenTheCiphertextAndItsTag() throws {
        let key = Self.key()
        let plaintext = Data("{\"kind\":\"phone\"}".utf8)
        let box = try #require(PairingBox.seal(plaintext, key: key))
        #expect(box.count == 24 + plaintext.count + 16)
        #expect(PairingBox.open(box, key: key) == plaintext)
        // A fresh nonce each time.
        let again = try #require(PairingBox.seal(plaintext, key: key))
        #expect(again.prefix(24) != box.prefix(24))
    }

    @Test func aBoxOpensOnlyUnderItsKeyAndUntouched() throws {
        let key = Self.key()
        let box = try #require(PairingBox.seal(Data("hello".utf8), key: key))
        #expect(PairingBox.open(box, key: Self.key()) == nil)
        for index in [0, 23, 24, box.count - 1] {
            var tampered = box
            tampered[index] ^= 0x01
            #expect(PairingBox.open(tampered, key: key) == nil)
        }
        #expect(PairingBox.open(box.prefix(39), key: key) == nil)
        #expect(PairingBox.seal(Data(), key: Data(count: 31)) == nil)
    }

    @Test func theDevicesBoxIsCompactJSONWithSortedKeys() throws {
        let contents = PairingBox.DeviceContents(publicKey: "KEY", name: "Grant's iPhone", kind: "phone")
        let text = String(decoding: PairingBox.encode(contents), as: UTF8.self)
        #expect(text == #"{"kind":"phone","name":"Grant's iPhone","publicKey":"KEY"}"#)
        #expect(PairingBox.deviceContents(Data(text.utf8)) == contents)
    }

    @Test func theCoresBoxIsParsedNeverCompared() throws {
        let claim = LinkMessage.StationIdentityClaim(publicKey: "PK", certBinding: "CB")
        let contents = PairingBox.StationContents(identity: claim, label: "")
        let text = String(decoding: PairingBox.encode(contents), as: UTF8.self)
        #expect(text == #"{"identity":{"certBinding":"CB","publicKey":"PK"},"label":""}"#)
        // Spacing and key order the app does not write still read the same.
        let loose = #"{ "label" : "Shack",  "identity" : { "publicKey":"PK", "certBinding":"CB" } }"#
        #expect(PairingBox.stationContents(Data(loose.utf8))
                == PairingBox.StationContents(identity: claim, label: "Shack"))
        #expect(PairingBox.stationContents(Data(#"{"identity":{"publicKey":"PK"},"label":""}"#.utf8)) == nil)
        #expect(PairingBox.stationContents(Data(#"{"identity":{"publicKey":"PK","certBinding":"CB"}}"#.utf8)) == nil)
        #expect(PairingBox.stationContents(Data("not json".utf8)) == nil)
    }
}
