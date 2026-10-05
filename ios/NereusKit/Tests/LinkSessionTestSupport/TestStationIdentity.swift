// NereusSDR for iOS: a Core identity key made at run time, for the runners' station hellos
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import NereusLink

/// The test station identity an app's runner puts in every station `hello`
/// (link document section 16.3): a P-256 key made at run time and never
/// written anywhere, and its binding of the certificate the runner's
/// transport reports.
public struct TestStationIdentity: Sendable {
    private let key: P256.Signing.PrivateKey

    public init() {
        key = P256.Signing.PrivateKey()
    }

    /// The key as its 91-byte SubjectPublicKeyInfo DER.
    public var publicKey: Data { key.publicKey.derRepresentation }

    /// The trust a session gives a Core it paired with under this identity.
    public var trust: StationTrust { .identity(publicKey: publicKey) }

    /// The `identity` object of a `hello`, binding the certificate whose
    /// SHA-256 is `certificateSHA256`.
    public func claim(certificateSHA256: Data) throws -> LinkMessage.StationIdentityClaim {
        LinkMessage.StationIdentityClaim(publicKey: Base64URL.encode(publicKey),
                                         certBinding: Base64URL.encode(try binding(certificateSHA256)))
    }

    /// The identity key's signature over the binding label and the digest.
    public func binding(_ certificateSHA256: Data) throws -> Data {
        try key.signature(for: StationTrust.certBindingLabel + certificateSHA256).rawRepresentation
    }

    /// 32 random bytes as base64url, a new sign-in challenge.
    public static func newChallenge() -> String {
        Base64URL.encode(Data((0..<32).map { _ in UInt8.random(in: 0...255) }))
    }
}
