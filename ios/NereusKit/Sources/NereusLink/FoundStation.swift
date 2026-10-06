// NereusSDR for iOS: one Core found on this network over Bonjour, read from its TXT record
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation

/// A Core ``StationBrowser`` found on this network (link document section
/// 14.2, D36). Its TXT record says whether the Core is claimed and how it
/// pairs; `label` is the Core's own label from `name`, since Bonjour may
/// rename the instance on a clash. `identityPrefix` names the Core in a
/// list and proves nothing: a sign-in or a pairing still checks the whole
/// identity key the Core's `hello` carries.
public struct FoundStation: Hashable, Sendable {
    /// How the Core takes a new device, from the record's `pair`.
    public enum Pairing: String, Hashable, Sendable {
        /// One tap pairs (the Core is unclaimed and allows it).
        case click
        /// Only the code pairs.
        case code
        /// Nothing pairs now.
        case closed
    }

    /// The Core's radio as its record's `radio` says (link document
    /// section 14.2).
    public enum RadioState: String, Hashable, Sendable {
        /// The Core has no radio connected.
        case offline
        /// The Core's radio is connected.
        case connected
        /// The Core is waiting for a radio to be chosen.
        case waiting
    }

    /// The record's version this app reads.
    public static let recordVersion = "1"
    /// How many base64url characters of the identity fingerprint `id` carries.
    public static let identityPrefixCharacters = 22

    /// The Bonjour instance name: the label or the Core's name, as registered.
    public let instanceName: String
    /// The Core's label, possibly empty.
    public let label: String
    /// The first 22 characters of the identity fingerprint, in base64url.
    public let identityPrefix: String
    /// Some device is paired with the Core.
    public let claimed: Bool
    public let pairing: Pairing
    /// How many devices hold places on the Core, 0 to 4, from the record's
    /// `devices` (the several-devices design, ruling 10.4); nil when a Core
    /// older than the count leaves it out, or sends a value outside 0 to 4.
    public let devices: Int?
    /// The Core's radio, from the record's `radio`; nil when a Core older
    /// than it leaves it out, or sends a value this app does not know.
    public let radio: RadioState?
    /// Where the Core listens, once the service is resolved: the address a
    /// path to the service picks, which a pairing dials.
    public var endpoint: StationEndpoint?
    /// Every address Bonjour resolves the Core's host to, with the
    /// service's port (R-IOS-16): its global IPv6 addresses among them,
    /// which the phone proves and keeps to dial from anywhere.
    public var endpoints: [StationEndpoint]

    /// The most devices a Core holds places for (`maxDeviceSessions`).
    public static let maximumDevices = 4

    public init(instanceName: String, label: String, identityPrefix: String, claimed: Bool, pairing: Pairing,
                devices: Int? = nil, radio: RadioState? = nil, endpoint: StationEndpoint? = nil,
                endpoints: [StationEndpoint] = []) {
        self.instanceName = instanceName
        self.label = label
        self.identityPrefix = identityPrefix
        self.claimed = claimed
        self.pairing = pairing
        self.devices = devices
        self.radio = radio
        self.endpoint = endpoint
        self.endpoints = endpoints
    }

    /// Every address the Core was found at, each once, in the order a
    /// direct dial prefers them (``LocalNetworks/directPreference(_:)``):
    /// its global IPv6 addresses first.
    public var dialable: [StationEndpoint] {
        var seen = Set<StationEndpoint>()
        let all = (endpoint.map { [$0] } ?? []) + endpoints
        return LocalNetworks.directPreference(all.filter { seen.insert($0.canonical).inserted })
    }

    /// What a row shows: the label, or the instance name when the label is empty.
    public var displayName: String {
        label.isEmpty ? instanceName : label
    }

    /// Whether this is the Core whose identity key is `identityKey` (its
    /// SubjectPublicKeyInfo DER), by the fingerprint's first 22 characters.
    public func matches(identityKey: Data) -> Bool {
        identityPrefix == Self.identityPrefix(of: identityKey)
    }

    /// The first 22 characters of base64url(SHA-256(`identityKey`)), as a
    /// Core's record carries them.
    public static func identityPrefix(of identityKey: Data) -> String {
        String(Base64URL.encode(Data(SHA256.hash(data: identityKey))).prefix(identityPrefixCharacters))
    }

    // MARK: Reading the record

    /// The Core a TXT record in its wire form describes (RFC 6763 section
    /// 6.1: each entry after a one-byte length), or nil when the record is
    /// one the app cannot read.
    public static func parse(txtRecord: Data, instanceName: String) -> FoundStation? {
        parse(txt: entries(of: txtRecord), instanceName: instanceName)
    }

    /// The Core the record's entries describe, or nil when the app cannot
    /// read it: a `v` other than `1`, or a missing or malformed `v`, `id`,
    /// `claimed` or `pair`. A missing `name` reads as an empty label, a
    /// missing or unreadable `devices` as no count, and a missing or unknown
    /// `radio` as a radio state not known. A key the app does not
    /// know is ignored, so a newer Core still lists.
    public static func parse(txt: [String: String], instanceName: String) -> FoundStation? {
        guard txt["v"] == recordVersion,
              let id = txt["id"], id.utf8.count == identityPrefixCharacters, id.utf8.allSatisfy(isBase64URL),
              let claimed = txt["claimed"], claimed == "0" || claimed == "1",
              let pair = txt["pair"].flatMap(Pairing.init(rawValue:)) else {
            return nil
        }
        var devices: Int?
        if let text = txt["devices"], text.utf8.count == 1, let count = Int(text), (0...maximumDevices).contains(count) {
            devices = count
        }
        return FoundStation(instanceName: instanceName, label: txt["name"] ?? "", identityPrefix: id,
                            claimed: claimed == "1", pairing: pair, devices: devices,
                            radio: txt["radio"].flatMap(RadioState.init(rawValue:)))
    }

    /// A TXT record's entries by key: keys compared without case, the first
    /// of a repeated key kept, and an entry that runs past the record's end,
    /// has no key or no `=`, or whose value is not UTF-8, left out (RFC 6763
    /// section 6).
    public static func entries(of record: Data) -> [String: String] {
        let bytes = [UInt8](record)
        var entries: [String: String] = [:]
        var index = 0
        while index < bytes.count {
            let length = Int(bytes[index])
            index += 1
            guard index + length <= bytes.count else {
                break
            }
            let entry = bytes[index..<(index + length)]
            index += length
            guard let equals = entry.firstIndex(of: UInt8(ascii: "=")), equals > entry.startIndex,
                  let key = String(bytes: entry[entry.startIndex..<equals], encoding: .ascii),
                  let value = String(bytes: entry[(equals + 1)...], encoding: .utf8) else {
                continue
            }
            let lowered = key.lowercased()
            if entries[lowered] == nil {
                entries[lowered] = value
            }
        }
        return entries
    }

    private static func isBase64URL(_ byte: UInt8) -> Bool {
        (byte >= UInt8(ascii: "A") && byte <= UInt8(ascii: "Z"))
            || (byte >= UInt8(ascii: "a") && byte <= UInt8(ascii: "z"))
            || (byte >= UInt8(ascii: "0") && byte <= UInt8(ascii: "9"))
            || byte == UInt8(ascii: "-") || byte == UInt8(ascii: "_")
    }
}
