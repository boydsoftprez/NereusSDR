// NereusSDR for iOS: the device key made in the Secure Enclave, its handle kept in the Keychain
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import Security

/// A key made in the Secure Enclave. The Keychain keeps its opaque handle
/// (D67): readable after first unlock, this device only, and kept when the
/// app is deleted and installed again. The key itself never leaves the chip.
public struct SecureEnclaveKeyStore: DeviceKeyStore {
    public static let defaultAccount = "device-key-secure-enclave"

    private let item: any SecretItem

    public init(item: any SecretItem = KeychainItem(account: SecureEnclaveKeyStore.defaultAccount)) {
        self.item = item
    }

    public func loadOrCreateKey() throws -> any DeviceSigningKey {
        if let stored = try KeychainErrors.read(item) {
            return try Self.key(from: stored)
        }
        let made = try Self.newKey()
        if try KeychainErrors.add(made.dataRepresentation, to: item) {
            return SecureEnclaveSigningKey(key: made)
        }
        // Another caller stored one first; that one is the device's key.
        guard let stored = try KeychainErrors.read(item) else {
            throw DeviceKeyError.unreadableKey
        }
        return try Self.key(from: stored)
    }

    public func makeNewKey() throws -> any DeviceSigningKey {
        let made = try Self.newKey()
        try KeychainErrors.write(made.dataRepresentation, to: item)
        return SecureEnclaveSigningKey(key: made)
    }

    private static func newKey() throws -> SecureEnclave.P256.Signing.PrivateKey {
        var error: Unmanaged<CFError>?
        guard let access = SecAccessControlCreateWithFlags(nil, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
                                                           .privateKeyUsage, &error) else {
            _ = error?.takeRetainedValue()
            throw DeviceKeyError.couldNotCreate
        }
        guard let made = try? SecureEnclave.P256.Signing.PrivateKey(accessControl: access) else {
            throw DeviceKeyError.couldNotCreate
        }
        return made
    }

    private static func key(from stored: Data) throws -> SecureEnclaveSigningKey {
        guard let key = try? SecureEnclave.P256.Signing.PrivateKey(dataRepresentation: stored) else {
            throw DeviceKeyError.unreadableKey
        }
        return SecureEnclaveSigningKey(key: key)
    }
}
