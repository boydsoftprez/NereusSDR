// NereusSDR for iOS: the confirmation boxes the device and the Core exchange at the end of a code pairing
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CSodium
import Foundation

/// The boxes `pair.confirm` carries (link document section 3.6): a fresh
/// 24-byte random nonce, then the XChaCha20-Poly1305 (IETF) ciphertext and
/// its 16-byte tag, with no additional data, under one of the two shared
/// keys. libsodium seals them: CryptoKit's ChaChaPoly takes a 12-byte
/// nonce and would not interoperate.
///
/// The device's box holds `{"kind", "name", "publicKey"}`; the Core's
/// `{"identity": {"certBinding", "publicKey"}, "label"}`, both compact JSON
/// with sorted keys. A box is read by parsing what it holds, never by
/// comparing bytes.
public enum PairingBox {
    public static let nonceBytes = 24
    public static let tagBytes = 16
    public static let keyBytes = 32

    /// What the device's box holds.
    public struct DeviceContents: Sendable, Equatable {
        public var publicKey: String
        public var name: String
        public var kind: String

        public init(publicKey: String, name: String, kind: String) {
            self.publicKey = publicKey
            self.name = name
            self.kind = kind
        }
    }

    /// What the Core's box holds.
    public struct StationContents: Sendable, Equatable {
        public var identity: LinkMessage.StationIdentityClaim
        /// The Core's label; it may be empty.
        public var label: String

        public init(identity: LinkMessage.StationIdentityClaim, label: String) {
            self.identity = identity
            self.label = label
        }
    }

    // MARK: Sealing and opening

    /// `plaintext` sealed under `key` (32 bytes) with a fresh nonce; nil
    /// when the key is the wrong length or libsodium cannot run.
    public static func seal(_ plaintext: Data, key: Data) -> Data? {
        guard key.count == keyBytes else {
            return nil
        }
        return key.withUnsafeBytes { secret in
            seal(plaintext, key: secret.bindMemory(to: UInt8.self).baseAddress!)
        }
    }

    /// What `box` holds, opened under `key`; nil when it does not open
    /// (tampered, another key, too short).
    public static func open(_ box: Data, key: Data) -> Data? {
        guard key.count == keyBytes else {
            return nil
        }
        return key.withUnsafeBytes { secret in
            open(box, key: secret.bindMemory(to: UInt8.self).baseAddress!)
        }
    }

    /// `plaintext` sealed under the 32 bytes at `key`, read in place so a
    /// shared key is never copied out of the exchange that holds it.
    static func seal(_ plaintext: Data, key: UnsafePointer<UInt8>) -> Data? {
        guard sodium_init() >= 0 else {
            return nil
        }
        var box = Data(count: nonceBytes + plaintext.count + tagBytes)
        var written: UInt64 = 0
        let result = box.withUnsafeMutableBytes { (out: UnsafeMutableRawBufferPointer) -> Int32 in
            let base = out.bindMemory(to: UInt8.self).baseAddress!
            randombytes_buf(base, nonceBytes)
            return plaintext.withUnsafeBytes { (message: UnsafeRawBufferPointer) -> Int32 in
                crypto_aead_xchacha20poly1305_ietf_encrypt(
                    base + nonceBytes, &written,
                    message.bindMemory(to: UInt8.self).baseAddress, UInt64(plaintext.count),
                    nil, 0, nil, base, key)
            }
        }
        guard result == 0 else {
            return nil
        }
        return box.prefix(nonceBytes + Int(written))
    }

    /// What `box` holds, opened under the 32 bytes at `key`, read in place.
    static func open(_ box: Data, key: UnsafePointer<UInt8>) -> Data? {
        guard box.count >= nonceBytes + tagBytes, sodium_init() >= 0 else {
            return nil
        }
        let sealed = Data(box)
        let cipherBytes = sealed.count - nonceBytes
        var plaintext = Data(count: cipherBytes - tagBytes)
        var written: UInt64 = 0
        let result = plaintext.withUnsafeMutableBytes { (out: UnsafeMutableRawBufferPointer) -> Int32 in
            sealed.withUnsafeBytes { (input: UnsafeRawBufferPointer) -> Int32 in
                let base = input.bindMemory(to: UInt8.self).baseAddress!
                return crypto_aead_xchacha20poly1305_ietf_decrypt(
                    out.bindMemory(to: UInt8.self).baseAddress, &written, nil,
                    base + nonceBytes, UInt64(cipherBytes), nil, 0, base, key)
            }
        }
        guard result == 0 else {
            return nil
        }
        return plaintext.prefix(Int(written))
    }

    // MARK: What the boxes hold

    /// The device's box contents as compact JSON.
    public static func encode(_ contents: DeviceContents) -> Data {
        Data(LinkJSON.object([
            "publicKey": .string(contents.publicKey),
            "name": .string(contents.name),
            "kind": .string(contents.kind),
        ]).compactText.utf8)
    }

    /// The Core's box contents as compact JSON.
    public static func encode(_ contents: StationContents) -> Data {
        Data(LinkJSON.object([
            "identity": .object([
                "certBinding": .string(contents.identity.certBinding),
                "publicKey": .string(contents.identity.publicKey),
            ]),
            "label": .string(contents.label),
        ]).compactText.utf8)
    }

    /// The device's box contents parsed from `plaintext`: an object whose
    /// three fields are strings. Nil otherwise.
    public static func deviceContents(_ plaintext: Data) -> DeviceContents? {
        guard case .object(let fields)? = parse(plaintext),
              case .string(let publicKey)? = fields["publicKey"], case .string(let name)? = fields["name"],
              case .string(let kind)? = fields["kind"] else {
            return nil
        }
        return DeviceContents(publicKey: publicKey, name: name, kind: kind)
    }

    /// The Core's box contents parsed from `plaintext`: an `identity`
    /// object of two strings and a string `label`, which may be empty. Nil
    /// otherwise.
    public static func stationContents(_ plaintext: Data) -> StationContents? {
        guard case .object(let fields)? = parse(plaintext),
              case .object(let identity)? = fields["identity"],
              case .string(let publicKey)? = identity["publicKey"],
              case .string(let certBinding)? = identity["certBinding"],
              case .string(let label)? = fields["label"] else {
            return nil
        }
        return StationContents(identity: LinkMessage.StationIdentityClaim(publicKey: publicKey,
                                                                          certBinding: certBinding),
                               label: label)
    }

    private static func parse(_ plaintext: Data) -> LinkJSON? {
        guard let text = String(data: plaintext, encoding: .utf8) else {
            return nil
        }
        return try? LinkJSON.parse(text)
    }
}
