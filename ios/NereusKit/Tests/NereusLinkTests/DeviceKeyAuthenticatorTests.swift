// NereusSDR for iOS: tests for signing in with the device key, and for the runners' check of it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import Testing
@testable import NereusLink

@Suite struct DeviceKeyAuthenticatorTests {
    private struct Rig {
        let core = TestStationIdentity()
        let device: DeviceIdentity
        let authenticator: DeviceKeyAuthenticator
        let certificateSHA256 = Data((0..<32).map { _ in UInt8.random(in: 0...255) })
        let challenge = Data((0..<32).map { _ in UInt8.random(in: 0...255) })

        init(kind: DeviceKeyAuthenticator.Kind = .phone) throws {
            device = try DeviceIdentity.load(store: InMemoryKeyStore())
            authenticator = try DeviceKeyAuthenticator(identity: device, name: "Shack iPhone", kind: kind)
        }

        func hello(features: [String: Int]? = ["deviceAuth": 1, "pairing": 1],
                   challenge written: String? = nil) throws -> LinkMessage.Hello {
            LinkMessage.Hello(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd", majors: [1],
                              features: features, identity: try core.claim(certificateSHA256: certificateSHA256),
                              challenge: written ?? Base64URL.encode(challenge))
        }

        func request(_ hello: LinkMessage.Hello) async throws -> LinkMessage.AuthRequest {
            try await authenticator.authRequest(stationHello: hello, certificateSHA256: certificateSHA256)
        }

        /// The block as JSON, for the runners' check.
        func json(_ request: LinkMessage.AuthRequest) -> LinkJSON? {
            guard case .object(let fields) = LinkCodec.json(.authRequest(request)) else {
                return nil
            }
            return fields["device"]
        }
    }

    @Test func itSignsThisConnectionsTranscript() async throws {
        let rig = try Rig()
        let request = try await rig.request(try rig.hello())
        #expect(request.token == "")
        let block = try #require(request.device)
        #expect(block.id == rig.device.id)
        #expect(block.id.count == 43)
        #expect(Base64URL.decode(block.publicKey) == rig.device.publicKey)
        #expect(block.name == "Shack iPhone")
        #expect(block.kind == "phone")
        #expect(block.shortName == "iPhone")
        let signature = try #require(Base64URL.decode(block.signature))
        #expect(signature.count == 64)

        // The transcript, written out by hand: 25 + 32 + 32 + 32 + 32 bytes.
        let label = Data("NereusSDR device-auth v1\n".utf8)
        #expect(label.count == 25)
        let transcript = label + rig.challenge + rig.certificateSHA256
            + Data(SHA256.hash(data: rig.core.publicKey)) + Data(SHA256.hash(data: rig.device.publicKey))
        #expect(transcript.count == 153)
        #expect(DeviceKeyAuthenticator.transcript(challenge: rig.challenge, certificateSHA256: rig.certificateSHA256,
                                                  stationKey: rig.core.publicKey,
                                                  deviceKey: rig.device.publicKey) == transcript)
        #expect(P256Wire.verify(signature: signature, over: transcript, spki: rig.device.publicKey))
        let json = try #require(rig.json(request))
        #expect(DeviceBlockCheck.failure(json, challenge: rig.challenge, certificateSHA256: rig.certificateSHA256,
                                         stationKey: rig.core.publicKey) == nil)
    }

    @Test func anIPadSignsInAsATablet() async throws {
        let rig = try Rig(kind: .tablet)
        let block = try #require(try await rig.request(try rig.hello()).device)
        #expect(block.kind == "tablet")
        #expect(block.shortName == "iPad")
    }

    @Test(arguments: [nil, [:], ["deviceAuth": 0], ["pairing": 1]] as [[String: Int]?])
    func itWillNotSignForACoreWithoutDeviceAuth(features: [String: Int]?) async throws {
        let rig = try Rig()
        await #expect(throws: StationAuthenticationError(reason: DeviceKeyAuthenticator.coreTooOldText)) {
            _ = try await rig.request(try rig.hello(features: features))
        }
    }

    @Test(arguments: [
        Base64URL.encode(Data(repeating: 1, count: 31)),
        Base64URL.encode(Data(repeating: 1, count: 33)),
        Base64URL.encode(Data(repeating: 1, count: 32)) + "=",
        "not base64url!",
    ])
    func itWillNotSignAChallengeThatIsNotThirtyTwoBytes(challenge: String) async throws {
        let rig = try Rig()
        await #expect(throws: StationAuthenticationError(reason: DeviceKeyAuthenticator.signInFailedText)) {
            _ = try await rig.request(try rig.hello(challenge: challenge))
        }
    }

    @Test func itWillNotSignWithoutTheCoresKey() async throws {
        let rig = try Rig()
        var hello = try rig.hello()
        hello.identity = nil
        await #expect(throws: StationAuthenticationError.self) {
            _ = try await rig.request(hello)
        }
        hello = try rig.hello()
        hello.challenge = nil
        await #expect(throws: StationAuthenticationError.self) {
            _ = try await rig.request(hello)
        }
    }

    @Test(arguments: ["", "   ", String(repeating: "a", count: 65), "Shack\u{0007}iPhone", "Shack\u{200B}iPhone",
                      "Shack\u{2028}iPhone", "Shack\u{2029}iPhone", "Shack\niPhone"])
    func anUnusableNameIsRefused(name: String) throws {
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        #expect(throws: DeviceKeyAuthenticator.UnusableName.self) {
            try DeviceKeyAuthenticator(identity: device, name: name, kind: .phone)
        }
    }

    @Test func namesUpToTheirCapsAreUsable() {
        #expect(DeviceName.isUsable(String(repeating: "a", count: 64)))
        #expect(!DeviceName.isUsable(String(repeating: "a", count: 65)))
        #expect(DeviceName.isUsableShortName("Grant's iPhone"))
        // Bytes of UTF-8, not characters: 22 two-byte letters are 44 bytes.
        #expect(!DeviceName.isUsableShortName(String(repeating: "é", count: 22)))
        #expect(DeviceName.isUsableShortName(String(repeating: "é", count: 16)))
        for kind in DeviceKeyAuthenticator.Kind.allCases {
            #expect(DeviceName.isUsableShortName(kind.shortName))
        }
    }

    /// The app's rule is stricter than the Core's where they differ: a
    /// subdivision flag's tag characters (format characters outside the
    /// Basic Multilingual Plane) are refused, and the authenticator refuses
    /// exactly what the rule refuses.
    @Test func theAppsRuleRefusesTagCharactersAndTheAuthenticatorFollowsIt() throws {
        let scotland = "Shack \u{1F3F4}\u{E0067}\u{E0062}\u{E0073}\u{E0063}\u{E0074}\u{E007F}"
        #expect(!DeviceName.isUsable(scotland))
        let device = try DeviceIdentity.load(store: InMemoryKeyStore())
        for name in [scotland, "Shack iPhone", ""] {
            let accepted = (try? DeviceKeyAuthenticator(identity: device, name: name, kind: .phone)) != nil
            #expect(accepted == DeviceName.isUsable(name), "\(name)")
        }
    }

    // MARK: The runners' check of a device block

    @Test func theCheckCatchesEveryWayABlockCanBeWrong() async throws {
        let rig = try Rig()
        let request = try await rig.request(try rig.hello())
        let json = try #require(rig.json(request))
        guard case .object(let good) = json else {
            Issue.record("not an object")
            return
        }
        let check = { (block: LinkJSON) in
            DeviceBlockCheck.failure(block, challenge: rig.challenge, certificateSHA256: rig.certificateSHA256,
                                     stationKey: rig.core.publicKey)
        }
        #expect(check(json) == nil)
        // Another connection's challenge, another certificate, another Core.
        #expect(DeviceBlockCheck.failure(json, challenge: Data(repeating: 0, count: 32),
                                         certificateSHA256: rig.certificateSHA256, stationKey: rig.core.publicKey) != nil)
        #expect(DeviceBlockCheck.failure(json, challenge: rig.challenge, certificateSHA256: Data(repeating: 0, count: 32),
                                         stationKey: rig.core.publicKey) != nil)
        #expect(DeviceBlockCheck.failure(json, challenge: rig.challenge, certificateSHA256: rig.certificateSHA256,
                                         stationKey: TestStationIdentity().publicKey) != nil)
        var altered = good
        altered["id"] = .string(Base64URL.encode(Data(repeating: 0, count: 32)))
        #expect(check(.object(altered)) != nil)
        altered = good
        altered["signature"] = nil
        #expect(check(.object(altered)) != nil)
        altered = good
        altered["shortName"] = .string(" ")
        #expect(check(.object(altered)) != nil)
        altered = good
        altered["extra"] = .string("x")
        #expect(check(.object(altered)) != nil)
        altered = good
        altered["shortName"] = nil
        #expect(check(.object(altered)) == nil)
    }
}
