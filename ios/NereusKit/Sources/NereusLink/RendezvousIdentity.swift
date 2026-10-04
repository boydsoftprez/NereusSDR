// NereusSDR for iOS: a Core's rendezvous id, and what a device signs to introduce itself
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation

/// The identity values of the rendezvous document, sections 4.2 and 4.4.
public enum RendezvousIdentity {
    /// `"NereusSDR rendezvous id v1\n"`, 27 bytes.
    static let idPrefix = Data("NereusSDR rendezvous id v1\n".utf8)
    /// `"NereusSDR introduce v1\n"`, 23 bytes.
    static let introducePrefix = Data("NereusSDR introduce v1\n".utf8)
    /// A rendezvous id's length: 130 of the digest's 256 bits.
    public static let idLength = 26
    /// A hello nonce's length in bytes.
    public static let nonceBytes = 32

    private static let base32Alphabet = Array("abcdefghijklmnopqrstuvwxyz234567".utf8)

    /// The rendezvous id of the Core whose station identity key is `spki`
    /// (its 91-byte SubjectPublicKeyInfo DER, the key of its `hello` and
    /// `pair.accept`, never its certificate's): the first 26 characters of
    /// the lowercase, unpadded RFC 4648 base32 of
    /// SHA-256("NereusSDR rendezvous id v1\n" || spki).
    public static func stationId(spki: Data) -> String {
        let digest = Data(SHA256.hash(data: idPrefix + spki))
        return String(base32(digest).prefix(idLength))
    }

    /// True for exactly 26 characters of `a-z` and `2-7` (field kind `rid`).
    public static func isStationId(_ text: String) -> Bool {
        let bytes = Array(text.utf8)
        return bytes.count == idLength && bytes.allSatisfy { base32Alphabet.contains($0) }
    }

    /// What a device signs to introduce itself on a connection:
    /// "NereusSDR introduce v1\n" || the 26 ASCII bytes of the Core's id ||
    /// the 32 raw bytes of that connection's hello nonce. Nil when either
    /// is not of its kind.
    public static func introduceTranscript(stationId: String, nonce: Data) -> Data? {
        guard isStationId(stationId), nonce.count == nonceBytes else {
            return nil
        }
        return introducePrefix + Data(stationId.utf8) + nonce
    }

    /// The device's signature for an `introduce`: its key over the
    /// transcript, raw `r || s`, 64 bytes.
    public static func introduceSignature(device: DeviceIdentity, stationId: String, nonce: Data) throws -> Data {
        guard let transcript = introduceTranscript(stationId: stationId, nonce: nonce) else {
            throw RendezvousError.protocolViolation
        }
        return try device.signature(for: transcript)
    }

    /// RFC 4648 base32, standard alphabet, lowercase, no padding.
    static func base32(_ data: Data) -> String {
        var out: [UInt8] = []
        var buffer: UInt32 = 0
        var bits = 0
        for byte in data {
            buffer = buffer << 8 | UInt32(byte)
            bits += 8
            while bits >= 5 {
                bits -= 5
                out.append(base32Alphabet[Int(buffer >> UInt32(bits) & 31)])
            }
        }
        if bits > 0 {
            out.append(base32Alphabet[Int(buffer << UInt32(5 - bits) & 31)])
        }
        return String(decoding: out, as: UTF8.self)
    }
}
