// NereusSDR for iOS: the device key kept in memory, for tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation

/// A key kept in memory only, for tests: made on the first call and the
/// same key at every later one from this store object.
public final class InMemoryKeyStore: DeviceKeyStore, @unchecked Sendable {
    private let lock = NSLock()
    private var key: P256.Signing.PrivateKey?

    public init() {}

    public func loadOrCreateKey() throws -> any DeviceSigningKey {
        lock.withLock {
            if let key {
                return SoftwareSigningKey(key: key)
            }
            let made = P256.Signing.PrivateKey()
            key = made
            return SoftwareSigningKey(key: made)
        }
    }

    public func makeNewKey() throws -> any DeviceSigningKey {
        lock.withLock {
            let made = P256.Signing.PrivateKey()
            key = made
            return SoftwareSigningKey(key: made)
        }
    }
}
