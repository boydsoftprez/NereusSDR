// NereusSDR for iOS: a secret kept in memory, for tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// A secret kept in memory, for tests: every copy of one shares its bytes.
public final class InMemorySecretItem: SecretItem, @unchecked Sendable {
    private let lock = NSLock()
    private var stored: Data?

    public init(_ data: Data? = nil) {
        stored = data
    }

    public func read() throws -> Data? {
        lock.withLock { stored }
    }

    public func add(_ data: Data) throws -> Bool {
        lock.withLock {
            guard stored == nil else {
                return false
            }
            stored = data
            return true
        }
    }

    public func write(_ data: Data) throws {
        lock.withLock { stored = data }
    }

    public func delete() throws {
        lock.withLock { stored = nil }
    }
}
