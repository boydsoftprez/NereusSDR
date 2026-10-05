// NereusSDR for iOS: one secret the app keeps, the device key or the paired Cores
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// One stored secret: the device key or the list of paired Cores. Its
/// bytes never reach a log.
public protocol SecretItem: Sendable {
    /// The stored bytes, or nil when nothing is stored.
    func read() throws -> Data?
    /// Stores `data` only when nothing is stored yet; false when something was.
    func add(_ data: Data) throws -> Bool
    /// Stores `data`, replacing whatever was stored.
    func write(_ data: Data) throws
    /// Removes what is stored, if anything.
    func delete() throws
}
