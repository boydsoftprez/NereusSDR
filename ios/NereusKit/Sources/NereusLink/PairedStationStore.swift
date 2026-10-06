// NereusSDR for iOS: the Cores this device has paired with, kept in the Keychain
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The Cores this device has paired with, one list in one Keychain item
/// (D67): readable after first unlock, this device only, and kept when the
/// app is deleted and installed again, so a reinstall keeps its pairings.
/// A Core is known by its identity key; saving one with a key already in
/// the list replaces that entry.
public final class PairedStationStore: @unchecked Sendable {
    public static let defaultAccount = "paired-cores"

    private let item: any SecretItem
    private let lock = NSLock()

    public init(item: any SecretItem = KeychainItem(account: PairedStationStore.defaultAccount)) {
        self.item = item
    }

    /// Every paired Core, in the order they were first saved.
    public func all() throws -> [PairedStation] {
        try lock.withLock { try load() }
    }

    /// The paired Core with this identity key, if any.
    public func station(identityKey: Data) throws -> PairedStation? {
        try all().first { $0.identityKey == identityKey }
    }

    /// Saves `station`, replacing the entry with its identity key if there is one.
    public func save(_ station: PairedStation) throws {
        try lock.withLock {
            var stations = try load()
            if let index = stations.firstIndex(where: { $0.identityKey == station.identityKey }) {
                stations[index] = station
            } else {
                stations.append(station)
            }
            try item.write(try JSONEncoder().encode(stations))
        }
    }

    /// Keeps the Core's last good addresses up to date: the Core with this
    /// identity key was reached at `endpoint` by `path`, so that address is
    /// tried first on the next connect (``PairedStation/reached(_:by:)``).
    /// Returns the updated Core, or nil when no paired Core has the key.
    @discardableResult
    public func recordReached(identityKey: Data, at endpoint: StationEndpoint,
                              by path: ConnectionAttempt.Path) throws -> PairedStation? {
        try update(identityKey: identityKey) { $0.reached(endpoint, by: path) }
    }

    /// The Core with this identity key was reached through the remote
    /// access service by `path` (``PairedStation/reachedThroughService(by:)``).
    /// Returns the updated Core, or nil when no paired Core has the key.
    @discardableResult
    public func recordReachedThroughService(identityKey: Data, by path: ConnectionAttempt.Path) throws
        -> PairedStation? {
        try update(identityKey: identityKey) { $0.reachedThroughService(by: path) }
    }

    /// Records the `controlChannelVersion` the Core with this identity key
    /// declared at this sign-in (0 when it declared none). Returns the
    /// updated Core, or nil when no paired Core has the key.
    @discardableResult
    public func recordControlChannelVersion(identityKey: Data, _ version: Int) throws -> PairedStation? {
        try update(identityKey: identityKey) {
            $0.controlChannelVersion = max(version, 0)
            $0.controlChannelObservedAtUnixMs = version == 0
                ? Self.validUnixMilliseconds(Date()) : nil
        }
    }

    /// Keeps both route capabilities from one authenticated snapshot so a
    /// future dial does not ask for relay candidates after a known denial.
    @discardableResult
    public func recordRouteCapabilities(identityKey: Data, controlChannelVersion: Int,
                                        relayAllowed: Bool?, observedAt: Date = Date()) throws -> PairedStation? {
        try update(identityKey: identityKey) {
            $0.controlChannelVersion = max(controlChannelVersion, 0)
            $0.controlChannelObservedAtUnixMs = controlChannelVersion == 0
                ? Self.validUnixMilliseconds(observedAt) : nil
            $0.relayAllowed = relayAllowed
        }
    }

    private static func validUnixMilliseconds(_ date: Date) -> Int64? {
        let value = date.timeIntervalSince1970 * 1_000
        guard value.isFinite, value > 0, value < Double(Int64.max) else { return nil }
        return Int64(value)
    }

    /// Adds an address to the Core with this identity key, first in its list
    /// (``PairedStation/add(_:)``). Returns the updated Core, or nil when no
    /// paired Core has the key.
    @discardableResult
    public func addAddress(identityKey: Data, _ endpoint: StationEndpoint) throws -> PairedStation? {
        try update(identityKey: identityKey) { $0.add(endpoint) }
    }

    /// Keeps the list of addresses the Core with this identity key sent in
    /// `devices`' `coreAddresses`, replacing the last; an empty one changes
    /// nothing (``PairedStation/keepCoreAddresses(_:)``). Returns the
    /// updated Core, or nil when no paired Core has the key.
    @discardableResult
    public func recordCoreAddresses(identityKey: Data, _ addresses: [StationEndpoint]) throws -> PairedStation? {
        try update(identityKey: identityKey) { $0.keepCoreAddresses(addresses) }
    }

    /// Keeps the global addresses the Core with this identity key was just
    /// proved to answer at (``PairedStation/addProvenAddresses(_:)``).
    /// Returns the updated Core, or nil when no paired Core has the key.
    @discardableResult
    public func recordProvenAddresses(identityKey: Data, _ addresses: [StationEndpoint]) throws -> PairedStation? {
        try update(identityKey: identityKey) { $0.addProvenAddresses(addresses) }
    }

    /// Removes one of the Core's addresses, never its last
    /// (``PairedStation/remove(_:)``). Returns the updated Core, or nil when
    /// no paired Core has the key or the address was not removed.
    @discardableResult
    public func removeAddress(identityKey: Data, _ endpoint: StationEndpoint) throws -> PairedStation? {
        var removed = false
        let station = try update(identityKey: identityKey) { removed = $0.remove(endpoint) }
        return removed ? station : nil
    }

    private func update(identityKey: Data, _ change: (inout PairedStation) -> Void) throws -> PairedStation? {
        try lock.withLock {
            var stations = try load()
            guard let index = stations.firstIndex(where: { $0.identityKey == identityKey }) else {
                return nil
            }
            change(&stations[index])
            try item.write(try JSONEncoder().encode(stations))
            return stations[index]
        }
    }

    /// Forgets the Core with this identity key.
    public func remove(identityKey: Data) throws {
        try lock.withLock {
            let stations = try load().filter { $0.identityKey != identityKey }
            if stations.isEmpty {
                try item.delete()
            } else {
                try item.write(try JSONEncoder().encode(stations))
            }
        }
    }

    private func load() throws -> [PairedStation] {
        guard let data = try item.read() else {
            return []
        }
        return try JSONDecoder().decode([PairedStation].self, from: data)
    }
}
