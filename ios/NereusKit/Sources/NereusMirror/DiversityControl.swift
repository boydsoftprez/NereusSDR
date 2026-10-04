// NereusSDR for iOS: complete Core Diversity v1 summaries and coordinated participant actions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The authenticated Core's station-wide summary, independent of joined SliceModels.
/// All displayed values remain exactly as sent. Eligibility is a Core fact, not a board count.
public struct DiversityState: Equatable, Sendable {
    public static let featureName = "diversityControl"
    public static let capabilityName = "diversityControlVersion"
    public static let propertyName = "diversityState"
    public static let minimumMinor: UInt16 = 11
    public static let ordinal: UInt16 = 37
    public static let maximumSliceCount = 5
    public static let maximumExactInteger: Int64 = 9_007_199_254_740_991
    public static let updateReason = "Update the Core to move Diversity between slices."

    public struct Identity: Equatable, Sendable {
        public let sliceId: Int
        public let incarnation: Int64
        public let controlRevision: Int64
        public let controllerDeviceId: String
        /// Zero is readable in the JSON shape, but is never an admitted nonabsent command participant.
        public var canParticipate: Bool { incarnation > 0 && controlRevision > 0 }
    }

    public struct Live: Equatable, Sendable {
        public let identity: Identity
        public let letter: String
        public let band: Int64
        public let frequencyHz: Double
        public let phaseDeg: Double
        public let gainDb: Double
        public let fineNullEnabled: Bool
        public let pattern: String?
    }

    public struct Target: Equatable, Sendable {
        public let identity: Identity
        public let eligible: Bool
        public let reasonCode: String
        public let reason: String
    }

    public let revision: Int64
    public let requested: Bool
    public let running: Bool
    public let paused: Bool
    public let reasonCode: String
    public let reason: String
    public let live: Live?
    public let targets: [Target]

    @MainActor
    public static func available(in store: MirrorStore) -> Bool {
        (store.agreedMinor ?? 0) >= minimumMinor && store.capabilityVersion(capabilityName) == 1
    }

    public init?(json: String) {
        // Reuse the existing control-frame ceiling; no private string/pattern or radio limit is invented.
        guard json.utf8.count <= WebSocketLinkTransport.maxInboundMessageBytes,
              case .object(let object)? = try? LinkJSON.parse(json), Self.whole(object["version"]) == 1,
              let revision = Self.whole(object["revision"]), let requested = Self.flag(object["requested"]),
              let running = Self.flag(object["running"]), let paused = Self.flag(object["paused"]),
              let reasonCode = Self.text(object["reasonCode"]), let reason = Self.text(object["reason"]),
              let liveValue = object["live"], case .array(let rows)? = object["targets"],
              rows.count <= Self.maximumSliceCount else { return nil }
        let live: Live?
        switch liveValue {
        case .null: live = nil
        case .object(let fields):
            guard let identity = Self.identity(fields), let letter = Self.text(fields["letter"]),
                  let band = Self.whole(fields["band"]), band < 28,
                  let frequency = Self.number(fields["frequencyHz"]), frequency >= 0,
                  let phase = Self.number(fields["phaseDeg"]), (0...360).contains(phase),
                  let gain = Self.number(fields["gainDb"]), (-20...20).contains(gain),
                  let fineNull = Self.flag(fields["fineNullEnabled"]), let patternValue = fields["pattern"] else { return nil }
            let pattern: String?
            switch patternValue { case .null: pattern = nil; case .string(let sent): pattern = sent; default: return nil }
            live = Live(identity: identity, letter: letter, band: band, frequencyHz: frequency,
                        phaseDeg: phase, gainDb: gain, fineNullEnabled: fineNull, pattern: pattern)
        default: return nil
        }
        guard requested == (live != nil), !running || (requested && !paused),
              !paused || (requested && !running && !reason.isEmpty) else { return nil }
        var targets: [Target] = []
        var ids: Set<Int> = []
        for row in rows {
            guard case .object(let fields) = row, let identity = Self.identity(fields),
                  ids.insert(identity.sliceId).inserted, let eligible = Self.flag(fields["eligible"]),
                  let code = Self.text(fields["reasonCode"]), let words = Self.text(fields["reason"]) else { return nil }
            targets.append(Target(identity: identity, eligible: eligible, reasonCode: code, reason: words))
        }
        self.revision = revision
        self.requested = requested
        self.running = running
        self.paused = paused
        self.reasonCode = reasonCode
        self.reason = reason
        self.live = live
        self.targets = targets
    }

    private static func identity(_ fields: [String: LinkJSON]) -> Identity? {
        guard let id = whole(fields["sliceId"]), id < Int64(maximumSliceCount),
              let incarnation = whole(fields["incarnation"]), let revision = whole(fields["controlRevision"]),
              let controller = text(fields["controllerDeviceId"]) else { return nil }
        return Identity(sliceId: Int(id), incarnation: incarnation, controlRevision: revision, controllerDeviceId: controller)
    }
    private static func whole(_ value: LinkJSON?) -> Int64? {
        guard let value = number(value), value >= 0, value <= Double(maximumExactInteger), value == value.rounded() else { return nil }
        return Int64(value)
    }
    private static func number(_ value: LinkJSON?) -> Double? {
        guard case .number(let number)? = value, number.isFinite else { return nil }; return number
    }
    private static func text(_ value: LinkJSON?) -> String? {
        guard case .string(let text)? = value else { return nil }; return text
    }
    private static func flag(_ value: LinkJSON?) -> Bool? {
        guard case .bool(let flag)? = value else { return nil }; return flag
    }
}

/// Exact wire participants; caller/device authority is supplied by the captured authenticated session.
/// This value describes one coordinator request, never separate source-off/target-on writes.
public struct DiversityTargetAction: Equatable, Sendable {
    public static let verb = "diversity.setTarget"
    public let revision: Int64
    public let source: DiversityState.Identity?
    public let target: DiversityState.Identity?
    public let enabled: Bool

    public init?(state: DiversityState, enabled: Bool, targetSliceId: Int?) {
        let source = state.live?.identity
        guard source?.canParticipate != false else { return nil }
        let target: DiversityState.Identity?
        if enabled {
            guard let targetSliceId, let row = state.targets.first(where: { $0.identity.sliceId == targetSliceId }),
                  row.eligible, row.identity.canParticipate else { return nil }
            target = row.identity
        } else {
            guard targetSliceId == nil else { return nil }; target = nil
        }
        self.revision = state.revision
        self.source = source
        self.target = target
        self.enabled = enabled
    }

    public var arguments: [CommandArgument] {
        [.init(name: "enabled", value: .bool(enabled)), .init(name: "stateRevision", value: .int(revision)),
         .init(name: "sourceSliceId", value: .int(source.map { Int64($0.sliceId) } ?? -1)),
         .init(name: "sourceIncarnation", value: .int(source?.incarnation ?? 0)),
         .init(name: "sourceControlRevision", value: .int(source?.controlRevision ?? 0)),
         .init(name: "targetSliceId", value: .int(target.map { Int64($0.sliceId) } ?? -1)),
         .init(name: "targetIncarnation", value: .int(target?.incarnation ?? 0)),
         .init(name: "targetControlRevision", value: .int(target?.controlRevision ?? 0))]
    }
}
