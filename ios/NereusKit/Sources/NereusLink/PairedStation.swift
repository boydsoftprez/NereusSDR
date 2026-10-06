// NereusSDR for iOS: one Core this device has paired with
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// A Core this device paired with: its identity key, which the app checks
/// at every connection, its label, where it was reached, and the path the
/// last session took.
public struct PairedStation: Codable, Sendable, Equatable {
    /// The Core's identity key, its 91-byte SubjectPublicKeyInfo DER.
    public var identityKey: Data
    /// The Core's label, as it gave it at pairing or later.
    public var label: String
    /// Where the Core has been reached, most recent first.
    public var endpoints: [StationEndpoint]
    /// How the last session reached it, when one has: a
    /// ``ConnectionAttempt/Path`` raw value.
    public var lastPath: String?
    /// The address the last session reached it at, when one has. Absent in
    /// what older builds kept, which then reads as nil.
    public var lastGood: StationEndpoint?
    /// The Core's `controlChannelVersion` at its last sign-in (link document
    /// section 6.3; 0 when it sent none), nil until a session has run (a
    /// Core paired through a mailbox). Absent in what older builds kept.
    public var controlChannelVersion: Int?
    /// When a verified Core last declared version 0, Unix milliseconds.
    /// An older saved 0 without this field is stale and may be probed.
    public var controlChannelObservedAtUnixMs: Int64?
    /// The Core's last verified `relayAllowed` capability. Nil until a
    /// session has authenticated and received capabilities from it.
    public var relayAllowed: Bool?
    /// Where the Core can be dialled directly from any network (R-IOS-16):
    /// the list the Core itself sends in `devices`' `coreAddresses`, which
    /// replaces what was kept (``keepCoreAddresses(_:)``), and the global
    /// addresses this phone proved the Core answers at on its network
    /// (``addProvenAddresses(_:)``). Kept apart from ``endpoints`` so it
    /// never pushes out where the Core was reached or the operator's own
    /// address; at most ``CoreAddressList/maximumCount``. The addresses say
    /// where the operator is: they stay with the paired Core in the
    /// Keychain, never in a log. Absent in what older builds kept.
    public var directAddresses: [StationEndpoint]
    /// Which of ``directAddresses`` the Core's own list named, as
    /// canonical addresses: a proven address never pushes one of these out
    /// (``addProvenAddresses(_:)``). Absent in what older builds kept.
    public var coreListed: [StationEndpoint]

    public init(identityKey: Data, label: String, endpoints: [StationEndpoint], lastPath: String? = nil,
                lastGood: StationEndpoint? = nil, controlChannelVersion: Int? = nil,
                controlChannelObservedAtUnixMs: Int64? = nil,
                relayAllowed: Bool? = nil, directAddresses: [StationEndpoint] = [],
                coreListed: [StationEndpoint] = []) {
        self.identityKey = identityKey
        self.label = label
        self.endpoints = endpoints
        self.lastPath = lastPath
        self.lastGood = lastGood
        self.controlChannelVersion = controlChannelVersion
        self.controlChannelObservedAtUnixMs = controlChannelObservedAtUnixMs
        self.relayAllowed = relayAllowed
        self.directAddresses = directAddresses
        self.coreListed = coreListed
    }

    private enum CodingKeys: String, CodingKey {
        case identityKey, label, endpoints, lastPath, lastGood, controlChannelVersion
        case controlChannelObservedAtUnixMs, relayAllowed, directAddresses, coreListed
    }

    public init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        identityKey = try values.decode(Data.self, forKey: .identityKey)
        label = try values.decode(String.self, forKey: .label)
        endpoints = try values.decode([StationEndpoint].self, forKey: .endpoints)
        lastPath = try values.decodeIfPresent(String.self, forKey: .lastPath)
        lastGood = try values.decodeIfPresent(StationEndpoint.self, forKey: .lastGood)
        controlChannelVersion = try values.decodeIfPresent(Int.self, forKey: .controlChannelVersion)
        // Only the advisory timestamp tolerates malformed data. Identity,
        // endpoints and other saved fields keep their strict decoding.
        controlChannelObservedAtUnixMs = try? values.decodeIfPresent(Int64.self,
                                                                    forKey: .controlChannelObservedAtUnixMs)
        relayAllowed = try values.decodeIfPresent(Bool.self, forKey: .relayAllowed)
        directAddresses = try values.decodeIfPresent([StationEndpoint].self, forKey: .directAddresses) ?? []
        coreListed = try values.decodeIfPresent([StationEndpoint].self, forKey: .coreListed) ?? []
    }

    public func encode(to encoder: Encoder) throws {
        var values = encoder.container(keyedBy: CodingKeys.self)
        try values.encode(identityKey, forKey: .identityKey)
        try values.encode(label, forKey: .label)
        try values.encode(endpoints, forKey: .endpoints)
        try values.encodeIfPresent(lastPath, forKey: .lastPath)
        try values.encodeIfPresent(lastGood, forKey: .lastGood)
        try values.encodeIfPresent(controlChannelVersion, forKey: .controlChannelVersion)
        try values.encodeIfPresent(controlChannelObservedAtUnixMs, forKey: .controlChannelObservedAtUnixMs)
        try values.encodeIfPresent(relayAllowed, forKey: .relayAllowed)
        if !directAddresses.isEmpty {
            try values.encode(directAddresses, forKey: .directAddresses)
        }
        if !coreListed.isEmpty {
            try values.encode(coreListed, forKey: .coreListed)
        }
    }

    /// Whether connecting from anywhere (through the remote access service)
    /// is offered for this Core: to a Core that declared the control
    /// channel (version 1 or later) at its last sign-in, or that has had no
    /// session yet. A Core that declared none cannot answer an
    /// introduction, so the control is shown disabled with
    /// ``updateToReachFromAnywhereText``.
    public var reachableFromAnywhere: Bool { reachableFromAnywhere(now: Date()) }

    /// A verified negative capability is fresh for five minutes. Missing,
    /// invalid and future timestamps permit a bounded service discovery;
    /// only a later authenticated capability can make another fresh 0.
    public func reachableFromAnywhere(now: Date) -> Bool {
        guard controlChannelVersion == 0 else { return true }
        guard let observed = controlChannelObservedAtUnixMs, observed > 0 else { return true }
        let milliseconds = now.timeIntervalSince1970 * 1_000
        guard milliseconds.isFinite, milliseconds >= 0, milliseconds < Double(Int64.max) else { return true }
        let current = Int64(milliseconds)
        guard current >= observed else { return true }
        return current - observed >= 300_000
    }

    /// Why connecting from anywhere is disabled for an older Core.
    public static let updateToReachFromAnywhereText = "Update the Core to reach it from anywhere."

    /// The trust a session to this Core signs in under.
    public var trust: StationTrust { .identity(publicKey: identityKey) }

    /// How many of the Core's last good addresses are kept: the desktop's
    /// `RemoteStationOptions::kMaxCachedAddresses`.
    public static let maxEndpoints = 4

    /// The Core's id on the remote access service, derived from its
    /// identity key (the rendezvous document, section 4.2).
    public var rendezvousId: String { RendezvousIdentity.stationId(spki: identityKey) }

    /// The Core was reached at `endpoint` by `path`: that address goes
    /// first, so the next connect tries it first, and the oldest beyond
    /// ``maxEndpoints`` is dropped.
    public mutating func reached(_ endpoint: StationEndpoint, by path: ConnectionAttempt.Path) {
        let key = endpoint.canonical
        endpoints = [endpoint] + endpoints.filter { $0.canonical != key }
        lastPath = path.rawValue
        lastGood = endpoint
        trim()
    }

    /// The Core was reached through the remote access service, by `path`
    /// (direct or relay): no address of its own was used, so none moves
    /// and none is marked as the one that worked last.
    public mutating func reachedThroughService(by path: ConnectionAttempt.Path) {
        lastPath = path.rawValue
        lastGood = nil
    }

    /// True when `endpoint` is the address the last session reached the Core at.
    public func isLastGood(_ endpoint: StationEndpoint) -> Bool {
        lastGood?.canonical == endpoint.canonical
    }

    /// A new address for the Core, one whose identity the phone has just
    /// checked: it goes first, so the next connect tries it first, and the
    /// oldest beyond ``maxEndpoints`` is dropped. An address the Core
    /// already has only moves first. The path the last session took is
    /// kept, since no session has used the new address yet.
    public mutating func add(_ endpoint: StationEndpoint) {
        let key = endpoint.canonical
        endpoints = [endpoint] + endpoints.filter { $0.canonical != key }
        trim()
    }

    /// The addresses in the order a connect tries them: the one the last
    /// session reached the Core at first, then the rest as kept.
    public var dialOrder: [StationEndpoint] {
        guard let good = lastGood?.canonical, let index = endpoints.firstIndex(where: { $0.canonical == good }) else {
            return endpoints
        }
        var order = endpoints
        order.insert(order.remove(at: index), at: 0)
        return order
    }

    /// Keeps at most ``maxEndpoints``, dropping the oldest but never the
    /// address that worked last, and forgets ``lastGood`` when its address
    /// is no longer kept.
    private mutating func trim() {
        while endpoints.count > Self.maxEndpoints {
            let good = lastGood?.canonical
            guard let index = endpoints.lastIndex(where: { $0.canonical != good }) else {
                break
            }
            endpoints.remove(at: index)
        }
        if let good = lastGood?.canonical, !endpoints.contains(where: { $0.canonical == good }) {
            lastGood = nil
        }
    }

    /// The port the phone dials this Core's control channel on directly:
    /// the one in its kept dialable addresses (the Core's own list names the
    /// port its listener holds), else the one the last session reached it
    /// at, else the one it was first reached at, else
    /// ``StationEndpoint/defaultPort``, the Core's default listener. An
    /// address seen in an introduction through the service is kept with
    /// this port, never the candidate's ICE UDP port.
    public var directDialPort: UInt16 {
        directAddresses.first?.port ?? lastGood?.port ?? endpoints.first?.port ?? StationEndpoint.defaultPort
    }

    /// The Core's own list of where it can be dialled (``CoreAddressList``)
    /// replaces what was kept, at most ``CoreAddressList/maximumCount``. An
    /// empty list is nothing new: the Core has none to name now, which does
    /// not make the kept ones wrong.
    public mutating func keepCoreAddresses(_ addresses: [StationEndpoint]) {
        guard !addresses.isEmpty else {
            return
        }
        var seen = Set<StationEndpoint>()
        directAddresses = Array(addresses.filter { seen.insert($0.canonical).inserted }
            .prefix(CoreAddressList.maximumCount))
        coreListed = directAddresses.map(\.canonical)
    }

    /// Addresses this phone has just seen the Core answer at, its identity
    /// checked: each global one (``LocalNetworks/isGlobal(_:)``) goes first,
    /// in the order given, and beyond ``CoreAddressList/maximumCount`` the
    /// oldest one the Core's own list did not name (``coreListed``) is
    /// dropped, so a proven address never pushes out one the Core named. A
    /// unique local, link-local or private address reaches the Core only on
    /// its own network, so it is not kept here.
    public mutating func addProvenAddresses(_ addresses: [StationEndpoint]) {
        let global = addresses.filter { LocalNetworks.isGlobal($0.host) }.map(\.canonical)
        guard !global.isEmpty else {
            return
        }
        var seen = Set<StationEndpoint>()
        var merged = (global + directAddresses.map(\.canonical)).filter { seen.insert($0).inserted }
        let listed = Set(coreListed.map(\.canonical))
        while merged.count > CoreAddressList.maximumCount,
              let index = merged.lastIndex(where: { !listed.contains($0) }) {
            merged.remove(at: index)
        }
        directAddresses = Array(merged.prefix(CoreAddressList.maximumCount))
    }

    /// Forgets one of the Core's addresses. False, and nothing changed, when
    /// it is the Core's only address or not one of its addresses: a paired
    /// Core always keeps at least one.
    @discardableResult
    public mutating func remove(_ endpoint: StationEndpoint) -> Bool {
        let key = endpoint.canonical
        guard endpoints.count > 1, endpoints.contains(where: { $0.canonical == key }) else {
            return false
        }
        endpoints.removeAll { $0.canonical == key }
        if lastGood?.canonical == key {
            lastGood = nil
        }
        return true
    }
}
