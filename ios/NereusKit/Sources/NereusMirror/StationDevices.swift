// NereusSDR for iOS: the Core's paired devices, its name, its key backup and its pairing window, from its devices object
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The Core's `devices` object (link document section 7.1,
/// `StationDevicesFacade`), read in the Core's own words: the paired
/// devices in pairing order, the Core's name, whether its key backup was
/// confirmed, and its pairing window. Its verbs (section 9.1) are named
/// here too, with the capability and agreed minor that carry them.
public enum StationDevices {
    /// The object's key.
    public static let key = "devices"
    /// The verbs that remove a device, confirm the key backup, and open and close pairing.
    public static let revokeVerb = "devices.revoke"
    public static let acknowledgeKeyBackupVerb = "station.acknowledgeKeyBackup"
    public static let openPairingVerb = "pairing.open"
    public static let closePairingVerb = "pairing.close"
    /// The capabilities that gate them, and the agreed minor they need.
    public static let adminCapability = "deviceAdminVersion"
    public static let pairingCapability = "pairingVersion"
    public static let minimumMinor: UInt16 = 11

    /// One paired device, an entry of `listJson`.
    public struct PairedDevice: Equatable, Sendable, Identifiable {
        /// The device's key fingerprint in base64url, as `devices.revoke` names it.
        public var id: String
        public var name: String
        public var shortName: String
        /// `phone`, `tablet` or `computer`.
        public var kind: String
        /// When it paired, and when the Core last saw it (nil when never).
        public var pairedAt: Date?
        public var lastSeen: Date?
        /// It holds a session now.
        public var connected: Bool

        public init(id: String, name: String, shortName: String, kind: String, pairedAt: Date?, lastSeen: Date?,
                    connected: Bool) {
            self.id = id
            self.name = name
            self.shortName = shortName
            self.kind = kind
            self.pairedAt = pairedAt
            self.lastSeen = lastSeen
            self.connected = connected
        }
    }

    /// Whether the Core takes the device verbs from this app: `deviceAdminVersion` 1 at minor 11.
    @MainActor
    public static func administers(_ store: MirrorStore) -> Bool {
        (store.agreedMinor ?? 0) >= minimumMinor && store.capabilityVersion(adminCapability) >= 1
    }

    /// Whether the Core opens and closes pairing for this app: `pairingVersion` 1 at minor 11.
    @MainActor
    public static func pairs(_ store: MirrorStore) -> Bool {
        (store.agreedMinor ?? 0) >= minimumMinor && store.capabilityVersion(pairingCapability) >= 1
    }

    /// The paired devices, in pairing order, from `listJson`; empty when the
    /// text is not the list. An entry without an id is skipped.
    public static func pairedDevices(fromListJson text: String) -> [PairedDevice] {
        guard case .array(let entries)? = try? LinkJSON.parse(text) else {
            return []
        }
        return entries.compactMap { entry in
            guard case .object(let o) = entry, let id = string(o["id"]), !id.isEmpty else {
                return nil
            }
            return PairedDevice(id: id, name: string(o["name"]) ?? "", shortName: string(o["shortName"]) ?? "",
                                kind: string(o["kind"]) ?? "", pairedAt: date(string(o["pairedAt"])),
                                lastSeen: date(string(o["lastSeen"])), connected: o["connected"] == .bool(true))
        }
    }

    /// The paired devices, from the mirror's `devices` object.
    @MainActor
    public static func pairedDevices(in store: MirrorStore) -> [PairedDevice] {
        guard case .text(let text)? = store.object(key)?["listJson"] else {
            return []
        }
        return pairedDevices(fromListJson: text)
    }

    private static func string(_ value: LinkJSON?) -> String? {
        if case .string(let text)? = value {
            return text
        }
        return nil
    }

    /// An ISO 8601 UTC time, with or without fractions of a second; nil for
    /// `""` (never) or anything else.
    static func date(_ text: String?) -> Date? {
        guard let text, !text.isEmpty else {
            return nil
        }
        let plain = ISO8601DateFormatter()
        plain.formatOptions = [.withInternetDateTime]
        if let date = plain.date(from: text) {
            return date
        }
        let fractional = ISO8601DateFormatter()
        fractional.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return fractional.date(from: text)
    }
}
