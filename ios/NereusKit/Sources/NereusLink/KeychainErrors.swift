// NereusSDR for iOS: the Keychain's refusals, as the device key's own errors
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Security

/// The device key's stores read and write through these, so a Keychain
/// refusal reaches the app as a ``DeviceKeyError`` it can show, never as a
/// raw status. A read the Keychain refuses because the phone has not been
/// unlocked since it started is `couldNotCreate`, a wait: it never offers
/// a new key over one that is only locked away. Any other refused read is
/// `unreadableKey`. A refused add or replace is `couldNotCreate`.
enum KeychainErrors {
    static func read(_ item: any SecretItem) throws -> Data? {
        do {
            return try item.read()
        } catch let error as KeychainError {
            throw error.status == errSecInteractionNotAllowed ? DeviceKeyError.couldNotCreate
                : DeviceKeyError.unreadableKey
        }
    }

    static func add(_ data: Data, to item: any SecretItem) throws -> Bool {
        do {
            return try item.add(data)
        } catch is KeychainError {
            throw DeviceKeyError.couldNotCreate
        }
    }

    static func write(_ data: Data, to item: any SecretItem) throws {
        do {
            try item.write(data)
        } catch is KeychainError {
            throw DeviceKeyError.couldNotCreate
        }
    }
}
