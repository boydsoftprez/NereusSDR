// NereusSDR for iOS: relay frame wire conformance
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusLink

struct RelayFrameTests {
    @Test func theGrantIsOpaqueAndNeverPartOfTheURL() throws {
        let grant = try RelayGrant(url: URL(string: "wss://rv.nereussdr.com/v1/relay")!, token: "Ab_-09", expires: 123)
        #expect(grant.url.absoluteString == "wss://rv.nereussdr.com/v1/relay")
        #expect(try RelayFrame.join(grant).first == 0x80)
        #expect(try RelayFrame.join(grant).dropFirst() == Data("Ab_-09".utf8))
        #expect(throws: RelayFrameError.self) { try RelayGrant(url: URL(string: "ws://rv.example/v1/relay")!, token: "Ab_-09", expires: 123) }
        #expect(throws: RelayFrameError.self) { try RelayGrant(url: URL(string: "wss://rv.example/v1/relay")!, token: "bad=", expires: 123) }
        let configured = try RelayGrant(url: URL(string: "wss://rv.example/alternate/relay?hint=ok")!, token: "valid", expires: 123)
        #expect(configured.url.path == "/alternate/relay")
        #expect(configured.url.query == "hint=ok")
        #expect(throws: RelayFrameError.self) {
            try RelayGrant(urlString: "wss://rv.example/é", token: "valid", expires: 123)
        }
        #expect(throws: RelayFrameError.self) {
            try RelayGrant(urlString: "wss://rv.example/" + String(repeating: "x", count: 500),
                           token: "valid", expires: 123)
        }
        #expect(String(reflecting: grant).contains("Ab_-09") == false)
        #expect(String(describing: grant.customMirror.children.map(\.value)).contains("Ab_-09") == false)
    }

    @Test func dataBoundariesAndUnknownTags() throws {
        #expect(try RelayFrame.datagram(.control, Data(repeating: 7, count: 1500)).count == 1501)
        #expect(throws: RelayFrameError.self) { try RelayFrame.datagram(.media, Data()) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.datagram(.control, Data(repeating: 7, count: 1501)) }
        #expect(try RelayFrame.read(Data([0x01, 0xAA])) == .datagram(.control, Data([0xAA])))
        #expect(try RelayFrame.read(Data([0x05, 0xAA])) == .ignored)
        #expect(try RelayFrame.read(Data([0x84, 0xAA])) == .ignored)
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data()) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data([0x00])) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data([0x80, 0x41])) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data([0x01])) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data(repeating: 0x01, count: 1502)) }
    }

    @Test func routedMediaKeepsExactUuidThenOpaqueEncryptedBytes() throws {
        let id = UUID(uuidString: "00112233-4455-4677-8899-aabbccddeeff")!
        let ciphertext = Data([0x17, 0xfe, 0xfd])
        let frame = try RelayFrame.routedMedia(id, ciphertext)
        #expect(frame.prefix(17) == Data([2, 0, 17, 34, 51, 68, 85, 70, 119, 136, 153, 170, 187, 204, 221, 238, 255]))
        let received = try RelayFrame.read(frame)
        guard case .datagram(.media, let payload) = received else { Issue.record("lost media lane"); return }
        let decoded = try RelayFrame.unrouteMedia(payload)
        #expect(decoded.connectionId == id)
        #expect(decoded.datagram == ciphertext)
        #expect(throws: RelayFrameError.self) { try RelayFrame.routedMedia(id, Data(repeating: 1, count: 1485)) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.unrouteMedia(Data(repeating: 0, count: 16)) }
    }

    @Test func futureReadyAndTrailingBytesKeepVersionOneMeaning() throws {
        #expect(try RelayFrame.read(Data([0x81, 0x02, 0x01, 0xFF])) == .ready(peerPresent: true))
        #expect(try RelayFrame.read(Data([0x82, 0x00, 0xFF])) == .peer(present: false))
        #expect(try RelayFrame.read(Data([0x83] + Array("shuttingDown".utf8))) == .end(.shuttingDown))
        #expect(try RelayFrame.read(Data([0x83] + Array("newCode".utf8))) == .end(.unknown))
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data([0x81, 0x01])) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data([0x81, 0x00, 0x01])) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data([0x81, 0x01, 0x02])) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data([0x82])) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data([0x82, 0x02])) }
        #expect(throws: RelayFrameError.self) { try RelayFrame.read(Data([0x83])) }
    }
}
