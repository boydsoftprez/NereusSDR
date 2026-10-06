// NereusSDR for iOS: the device key in software, kept in the Keychain
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation

/// A software key kept in the Keychain, for a device without a Secure
/// Enclave (D67: readable after first unlock, this device only, kept
/// through a reinstall).
public struct KeychainKeyStore: DeviceKeyStore {
    public static let defaultAccount = "device-key"

    private let item: any SecretItem

    public init(item: any SecretItem = KeychainItem(account: KeychainKeyStore.defaultAccount)) {
        self.item = item
    }

    public func loadOrCreateKey() throws -> any DeviceSigningKey {
        if let stored = try KeychainErrors.read(item) {
            return try Self.key(from: stored)
        }
        let made = P256.Signing.PrivateKey()
        if try KeychainErrors.add(made.rawRepresentation, to: item) {
            return SoftwareSigningKey(key: made)
        }
        // Another caller stored one first; that one is the device's key.
        guard let stored = try KeychainErrors.read(item) else {
            throw DeviceKeyError.unreadableKey
        }
        return try Self.key(from: stored)
    }

    public func makeNewKey() throws -> any DeviceSigningKey {
        let made = P256.Signing.PrivateKey()
        try KeychainErrors.write(made.rawRepresentation, to: item)
        return SoftwareSigningKey(key: made)
    }

    private static func key(from stored: Data) throws -> SoftwareSigningKey {
        guard let key = try? P256.Signing.PrivateKey(rawRepresentation: stored) else {
            throw DeviceKeyError.unreadableKey
        }
        return SoftwareSigningKey(key: key)
    }
}
