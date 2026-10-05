// NereusSDR for iOS: the device's own key, made once and kept, and where it is kept
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation

/// This device as the Cores it pairs with know it (link document section
/// 3.5): its P-256 key and the id derived from it. The key is made the
/// first time it is asked for and is the same key at every later sign-in.
public struct DeviceIdentity: Sendable {
    /// The key; its private half never leaves its store.
    public let key: any DeviceSigningKey
    /// The public key, its 91-byte SubjectPublicKeyInfo DER.
    public let publicKey: Data
    /// base64url of the public key's SHA-256, 43 characters.
    public let id: String

    /// The device's identity from `store`, whose key is made on first use.
    /// A stored key that cannot be read (after an erase and restore, say)
    /// throws `DeviceKeyError.unreadableKey` and is left as it is, so the
    /// app can offer to pair again rather than silently becoming a new
    /// device; a key that cannot be made throws `.couldNotCreate`.
    public static func load(store: any DeviceKeyStore) throws -> DeviceIdentity {
        let key = try store.loadOrCreateKey()
        let publicKey = key.publicKey
        guard P256Wire.isCanonicalKey(publicKey) else {
            throw DeviceKeyError.unreadableKey
        }
        return DeviceIdentity(key: key, publicKey: publicKey, id: P256Wire.deviceId(spki: publicKey))
    }

    /// Replaces the device's key with a new one and returns the identity it
    /// makes (D72). This is the only way a stored key is ever replaced, and
    /// the app calls it only when the operator asks for a new key after
    /// ``load(store:)`` found the stored one unreadable: a new key is a new
    /// device to every Core, so each Core then needs pairing again, and
    /// keeps listing the old key until it is removed there.
    public static func makeNewKey(store: any DeviceKeyStore) throws -> DeviceIdentity {
        let key = try store.makeNewKey()
        let publicKey = key.publicKey
        guard P256Wire.isCanonicalKey(publicKey) else {
            throw DeviceKeyError.couldNotCreate
        }
        return DeviceIdentity(key: key, publicKey: publicKey, id: P256Wire.deviceId(spki: publicKey))
    }

    /// The store for this device: the Secure Enclave where it has one, a
    /// software key in the Keychain where it has not.
    public static func standardStore() -> any DeviceKeyStore {
        SecureEnclave.isAvailable ? SecureEnclaveKeyStore() : KeychainKeyStore()
    }

    /// Signs `message` with the device's key: raw `r || s`, 64 bytes.
    public func signature(for message: Data) throws -> Data {
        try key.signature(for: message)
    }
}

/// Where the device's key lives. Its key is made once, on the first call,
/// and every later call, from this store object or a new one over the same
/// place, returns that key. A Keychain refusal reaches the caller as a
/// ``DeviceKeyError``, never as the Keychain's own error.
public protocol DeviceKeyStore: Sendable {
    func loadOrCreateKey() throws -> any DeviceSigningKey
    /// Makes a new key and stores it in place of whatever is stored,
    /// readable or not. Only ``DeviceIdentity/makeNewKey(store:)`` calls it.
    func makeNewKey() throws -> any DeviceSigningKey
}
