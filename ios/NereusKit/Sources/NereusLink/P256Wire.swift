// NereusSDR for iOS: P-256 keys and signatures as the link carries them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation

/// P-256 on the wire (link document section 3.4): a public key is its
/// SubjectPublicKeyInfo DER with the point uncompressed, 91 bytes, and its
/// fingerprint is the SHA-256 of that DER; a signature is ECDSA P-256 over
/// SHA-256, raw `r || s`, 64 bytes. The Core accepts only the canonical
/// 91-byte form, so the app holds itself to it too.
public enum P256Wire {
    /// The length of a P-256 SubjectPublicKeyInfo with its point uncompressed.
    public static let spkiLength = 91
    /// The length of a raw `r || s` signature.
    public static let signatureLength = 64

    /// Everything before the point's coordinates: the SPKI sequence, the
    /// algorithm (id-ecPublicKey, prime256v1), the bit string's header and
    /// the uncompressed point's leading 0x04.
    static let spkiPrefix = Data([
        0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04,
    ])

    /// The key a canonical 91-byte P-256 SPKI holds, or nil when `spki` is
    /// not one: another length, another algorithm or curve, a compressed
    /// point, or a point not on the curve.
    public static func publicKey(spki: Data) -> P256.Signing.PublicKey? {
        guard spki.count == spkiLength, spki.prefix(spkiPrefix.count) == spkiPrefix,
              let key = try? P256.Signing.PublicKey(derRepresentation: spki),
              key.derRepresentation == spki else {
            return nil
        }
        return key
    }

    /// True when `spki` is a canonical 91-byte P-256 key.
    public static func isCanonicalKey(_ spki: Data) -> Bool {
        publicKey(spki: spki) != nil
    }

    /// The fingerprint of a key: the SHA-256 of its SPKI DER.
    public static func fingerprint(spki: Data) -> Data {
        Data(SHA256.hash(data: spki))
    }

    /// A device's id: base64url of its key's fingerprint, 43 characters.
    public static func deviceId(spki: Data) -> String {
        Base64URL.encode(fingerprint(spki: spki))
    }

    /// True when `signature`, raw `r || s`, is `spki`'s signature over `message`.
    public static func verify(signature: Data, over message: Data, spki: Data) -> Bool {
        guard signature.count == signatureLength, let key = publicKey(spki: spki),
              let parsed = try? P256.Signing.ECDSASignature(rawRepresentation: signature) else {
            return false
        }
        return key.isValidSignature(parsed, for: message)
    }
}
