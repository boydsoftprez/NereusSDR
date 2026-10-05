// NereusSDR for iOS: signs in to a paired Core with the device's own key
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation

/// Signs in with the device's key (link document section 3.5): `token`
/// `""` and a `device` block whose signature covers this connection's
/// challenge, its certificate, the Core's identity key and the device's
/// own key. It signs only for a Core whose `hello` declares `deviceAuth` 1
/// or more and carries a 32-byte challenge. Use it with an identity trust,
/// under which the session has checked the Core's key and its binding of
/// the certificate before it asks for the sign-in.
public struct DeviceKeyAuthenticator: StationAuthenticator {
    /// What kind of device this is, as the Core lists it.
    public enum Kind: String, Sendable, CaseIterable {
        case phone
        case tablet

        /// The model's short name, sent at every sign-in for the places a
        /// screen has room for one word.
        public var shortName: String {
            switch self {
            case .phone: return "iPhone"
            case .tablet: return "iPad"
            }
        }
    }

    /// A name the Core would not store.
    public struct UnusableName: Error, Equatable {}

    /// What the device signs first: `"NereusSDR device-auth v1\n"`, 25 bytes.
    public static let transcriptLabel = Data("NereusSDR device-auth v1\n".utf8)
    /// The challenge is 32 bytes.
    public static let challengeLength = 32

    // Words the app shows when it decided not to sign in.
    static let coreTooOldText = "This Core needs an update before this device can sign in to it. Update the Core."
    static let signInFailedText = "This app could not sign in to the Core. Pair with it again."

    public let identity: DeviceIdentity
    public let name: String
    public let kind: Kind
    public let shortName: String

    /// `name` is the one the operator confirmed at pairing (D65). Throws
    /// `UnusableName` for a name `DeviceName.isUsable` refuses; the short
    /// name is the model's.
    public init(identity: DeviceIdentity, name: String, kind: Kind) throws {
        guard DeviceName.isUsable(name) else {
            throw UnusableName()
        }
        self.identity = identity
        self.name = name
        self.kind = kind
        shortName = kind.shortName
    }

    public var signsWithDeviceKey: Bool { true }

    public func authRequest(stationHello: LinkMessage.Hello, certificateSHA256: Data) async throws
        -> LinkMessage.AuthRequest {
        guard (stationHello.features?["deviceAuth"] ?? 0) >= 1 else {
            throw StationAuthenticationError(reason: Self.coreTooOldText)
        }
        guard let challengeText = stationHello.challenge, let challenge = Base64URL.decode(challengeText),
              challenge.count == Self.challengeLength,
              let stationKeyText = stationHello.identity?.publicKey,
              let stationKey = Base64URL.decode(stationKeyText), P256Wire.isCanonicalKey(stationKey),
              certificateSHA256.count == 32 else {
            throw StationAuthenticationError(reason: Self.signInFailedText)
        }
        let message = Self.transcript(challenge: challenge, certificateSHA256: certificateSHA256,
                                      stationKey: stationKey, deviceKey: identity.publicKey)
        let signature = try identity.signature(for: message)
        return LinkMessage.AuthRequest(token: "", device: LinkMessage.DeviceBlock(
            id: identity.id, publicKey: Base64URL.encode(identity.publicKey), name: name, kind: kind.rawValue,
            signature: Base64URL.encode(signature), shortName: shortName))
    }

    /// What a device signs (153 bytes): the label, the challenge, the
    /// certificate's SHA-256, then the SHA-256 of the Core's key and of the
    /// device's key.
    public static func transcript(challenge: Data, certificateSHA256: Data, stationKey: Data,
                                  deviceKey: Data) -> Data {
        transcriptLabel + challenge + certificateSHA256 + P256Wire.fingerprint(spki: stationKey)
            + P256Wire.fingerprint(spki: deviceKey)
    }
}
