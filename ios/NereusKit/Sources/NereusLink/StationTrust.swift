// NereusSDR for iOS: how the app knows it has reached the right Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// What the app checks a Core against before it sends any credential.
public enum StationTrust: Sendable, Hashable {
    /// The SHA-256 of the Core's certificate in DER form (link document
    /// section 3.2), checked during the TLS handshake.
    case certificate(pinSHA256: Data)
    /// The Core's identity key, its 91-byte SubjectPublicKeyInfo DER
    /// (section 3.4). The connection accepts any certificate and reports
    /// its SHA-256; the session then checks that the Core's `hello` names
    /// this key and that its certificate binding verifies for that
    /// certificate, before it sends anything.
    case identity(publicKey: Data)
    /// A connection that pairs (section 3.6): no pin and no identity yet.
    /// Any certificate is accepted and its SHA-256 reported, for the
    /// pairing to bind. Never a trust a session signs in with.
    case pairing

    /// True for an identity trust.
    public var isIdentity: Bool {
        if case .identity = self {
            return true
        }
        return false
    }

    /// What the Core's identity key signs to bind its certificate:
    /// `"NereusSDR cert-binding v1\n"`, 26 bytes, then the certificate's SHA-256.
    public static let certBindingLabel = Data("NereusSDR cert-binding v1\n".utf8)

    /// True when `identity` is the Core with key `publicKey` and its binding
    /// verifies for the certificate whose SHA-256 is `certificateSHA256`.
    public static func identityVerifies(_ identity: LinkMessage.StationIdentityClaim?, publicKey: Data,
                                        certificateSHA256: Data) -> Bool {
        guard let identity, certificateSHA256.count == 32,
              let presented = Base64URL.decode(identity.publicKey), presented == publicKey,
              let binding = Base64URL.decode(identity.certBinding) else {
            return false
        }
        return P256Wire.verify(signature: binding, over: certBindingLabel + certificateSHA256, spki: publicKey)
    }
}
