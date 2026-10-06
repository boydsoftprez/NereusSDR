// NereusSDR for iOS: a secret in the Keychain, kept on this device and through a reinstall
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Security

/// Why the Keychain refused.
public struct KeychainError: Error, Equatable {
    /// The Security framework's status.
    public let status: OSStatus

    public init(status: OSStatus) {
        self.status = status
    }
}

/// A generic-password item in the Keychain (D67): readable after the
/// device's first unlock, kept on this device only (never synced, never in
/// a backup restored elsewhere), and, because the Keychain outlives the
/// app, still there after the app is deleted and installed again.
public struct KeychainItem: SecretItem {
    /// The service every item of the app shares.
    public static let defaultService = "NereusSDR"

    public let service: String
    public let account: String

    public init(service: String = KeychainItem.defaultService, account: String) {
        self.service = service
        self.account = account
    }

    private var query: [String: Any] {
        [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecUseDataProtectionKeychain as String: true,
        ]
    }

    public func read() throws -> Data? {
        var request = query
        request[kSecReturnData as String] = true
        request[kSecMatchLimit as String] = kSecMatchLimitOne
        var result: CFTypeRef?
        let status = SecItemCopyMatching(request as CFDictionary, &result)
        switch status {
        case errSecSuccess:
            guard let data = result as? Data else {
                throw KeychainError(status: errSecDecode)
            }
            return data
        case errSecItemNotFound:
            return nil
        default:
            throw KeychainError(status: status)
        }
    }

    public func add(_ data: Data) throws -> Bool {
        var attributes = query
        attributes[kSecValueData as String] = data
        attributes[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        attributes[kSecAttrSynchronizable as String] = false
        let status = SecItemAdd(attributes as CFDictionary, nil)
        switch status {
        case errSecSuccess:
            return true
        case errSecDuplicateItem:
            return false
        default:
            throw KeychainError(status: status)
        }
    }

    public func write(_ data: Data) throws {
        if try add(data) {
            return
        }
        let changes: [String: Any] = [
            kSecValueData as String: data,
            kSecAttrAccessible as String: kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
        ]
        let status = SecItemUpdate(query as CFDictionary, changes as CFDictionary)
        guard status == errSecSuccess else {
            throw KeychainError(status: status)
        }
    }

    public func delete() throws {
        let status = SecItemDelete(query as CFDictionary)
        guard status == errSecSuccess || status == errSecItemNotFound else {
            throw KeychainError(status: status)
        }
    }
}
