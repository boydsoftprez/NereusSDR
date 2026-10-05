// NereusSDR for iOS: the fake Core's spots, spot sources and their consoles, as record streams and verbs
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The fake Core's spots (``Additions/spots``, link document sections 7.7
/// and 9.1): the `spots` record stream and each station source's
/// `spotConsole:<source>`, answered to `records.subscribe` with a reset
/// carrying the newest records, and the spot verbs: `spots.connect` and
/// `spots.disconnect` move the source's state on `spotSources`,
/// `spots.sendCommand` adds `> <text>` to the source's console, and
/// `spots.clearAll` empties the spots with a new generation's reset. A test
/// sets the scene with ``deliverSpots(_:)``. Without the addition the fake
/// advertises `recordStreamVersion` 0, as a Core before record streams.
/// Nothing reaches a cluster.
extension FakeStation {
    /// One spot of the scene, as the Core's `spots` record carries it.
    public struct SceneSpot: Sendable {
        public var call: String
        public var frequencyHz: Double
        public var mode: String
        public var source: String
        public var spotter: String
        public var comment: String
        public var timeUtc: String
        public var band: Int
        public var dxccColour: String
        public var dxccPriority: Int
        /// The slice mode the Core worked out for the spot (`resolvedMode`);
        /// sent only when set, as a Core at `recordStreamVersion` 2 sends it.
        public var resolvedMode: Int?

        public init(call: String, frequencyHz: Double, mode: String = "SSB", source: String = "Cluster",
                    spotter: String, comment: String = "", timeUtc: String, band: Int = 3, dxccColour: String = "",
                    dxccPriority: Int = 0, resolvedMode: Int? = nil) {
            self.call = call
            self.frequencyHz = frequencyHz
            self.mode = mode
            self.source = source
            self.spotter = spotter
            self.comment = comment
            self.timeUtc = timeUtc
            self.band = band
            self.dxccColour = dxccColour
            self.dxccPriority = dxccPriority
            self.resolvedMode = resolvedMode
        }
    }

    /// What the fake's spot sources show.
    public struct SpotScene: Sendable {
        /// Oldest first, as the Core keeps them.
        public var spots: [SceneSpot]
        /// Each station source's state and words, by its name (`dxCluster`).
        public var sources: [String: (state: String, text: String)]
        /// Each station source's console lines, oldest first.
        public var consoles: [String: [String]]
        /// FreeDV Reporter's state, words and "Hide my station" on
        /// `spotSources`; nil leaves them as the snapshot sent them.
        public var freedv: (state: String, text: String, hidden: Bool)?

        public init(spots: [SceneSpot], sources: [String: (state: String, text: String)] = [:],
                    consoles: [String: [String]] = [:], freedv: (state: String, text: String, hidden: Bool)? = nil) {
            self.spots = spots
            self.sources = sources
            self.consoles = consoles
            self.freedv = freedv
        }

        /// The board's pictures 04 and 20: thirteen spots on 40 metres from
        /// the cluster and POTA, in the DXCC colours, the cluster and POTA
        /// running and the cluster's console.
        public static var board: SpotScene {
            let worked = ("#606060", 1)
            let newBand = ("#FF8C00", 3)
            let newMode = ("#FFD700", 2)
            let newCountry = ("#FF3030", 4)
            func spot(_ call: String, _ mhz: Double, _ source: String, _ dxcc: (String, Int), _ by: String,
                      _ note: String, _ time: String) -> SceneSpot {
                SceneSpot(call: call, frequencyHz: (mhz * 1_000_000).rounded(), source: source, spotter: by,
                          comment: note, timeUtc: "2026-09-26T\(time)Z", dxccColour: dxcc.0, dxccPriority: dxcc.1)
            }
            let spots = [
                spot("N3ABC", 7.2400, "Cluster", worked, "K3XYZ", "", "19:22:40"),
                spot("W2XYZ", 7.2275, "POTA", worked, "POTA", "US-5678", "19:29:51"),
                spot("ON4ABC", 7.2470, "Cluster", worked, "W3ABC", "", "19:30:26"),
                spot("VE3ABC", 7.2230, "Cluster", worked, "K2XYZ", "59 into NY", "19:31:05"),
                spot("DL1ABC", 7.2462, "Cluster", worked, "K1XYZ", "", "19:33:02"),
                spot("G4ABC", 7.2460, "Cluster", worked, "W1XYZ", "", "19:35:18"),
                spot("EA8XYZ", 7.2260, "Cluster", newBand, "N4ABC", "Canary Is., strong", "19:36:44"),
                spot("F5ABC", 7.2465, "Cluster", newMode, "N1ABC", "", "19:39:47"),
                spot("K1ABC", 7.2245, "POTA", worked, "POTA", "US-1234, park activation", "19:40:12"),
                spot("KP4XYZ", 7.2455, "Cluster", newBand, "N2ABC", "working split?", "19:41:55"),
                spot("JA1ABC", 7.2310, "Cluster", newMode, "W6ABC", "long path", "19:42:03"),
                spot("VK2XYZ", 7.2330, "Cluster", newCountry, "W2ABC", "CQ DX, 5/9", "19:42:10"),
                spot("ZL2ABC", 7.2585, "Cluster", newCountry, "K6ABC", "grey line", "19:43:30"),
            ]
            return SpotScene(spots: spots,
                             sources: ["dxCluster": ("connected", ""), "pota": ("connected", "Polling api.pota.app")],
                             consoles: ["dxCluster": [
                                 "DX de W2ABC:     7233.0  VK2XYZ       CQ DX, 5/9           1942Z",
                                 "DX de N2ABC:     7245.5  KP4XYZ       working split?       1941Z",
                                 "DX de K6ABC:     7258.5  ZL2ABC       grey line            1943Z",
                                 "DX de N1ABC:     7246.5  F5ABC                             1939Z",
                             ]])
        }
    }

    /// The four sources the Core runs, by name, as `spotSources` orders them.
    public static let coreSpotSources = ["dxCluster", "rbn", "pota", "pskReporter"]
    /// The capability under which the Core runs FreeDV Reporter and sends
    /// its three `spotSources` properties (link section 7.1).
    public static let freedvCapability = "stationFreedvVersion"
    static let freedvSourceProperties: Set<String> = ["freedvReporterState", "freedvReporterText", "freedvReporterHidden"]

    /// A `spotSources` schema or create as a Core before FreeDV Reporter
    /// sends it: without FreeDV Reporter's three properties. Anything else
    /// is returned as it was.
    static func withoutFreedvSourceProperties(_ object: [String: Any]) -> [String: Any] {
        guard object["class"] as? String == "SpotSourceHost" else {
            return object
        }
        var object = object
        for field in ["fields", "properties"] {
            if let entries = object[field] as? [[String: Any]] {
                object[field] = entries.filter { !freedvSourceProperties.contains($0["name"] as? String ?? "") }
            }
        }
        return object
    }
    public static let spotsNotConnectedReasons = [
        "dxCluster": "The DX cluster is not connected.", "rbn": "The Reverse Beacon Network is not connected.",
    ]
    public static let unknownListReason = "The Core does not keep that list."
    public static let noCommandsReason = "Only the DX cluster and the Reverse Beacon Network take typed commands."

    /// What the fake's spot sources are doing now, behind its own lock.
    final class SpotState: @unchecked Sendable {
        private let lock = NSLock()
        private var scene = SpotScene(spots: [])
        private var generation: Int64 = 1

        func set(_ scene: SpotScene) {
            lock.withLock { self.scene = scene }
        }

        func read<Result>(_ body: (inout SpotScene, inout Int64) -> Result) -> Result {
            lock.withLock { body(&scene, &generation) }
        }
    }

    /// Sets the scene and sends it as the Core would: the sources' states,
    /// and a reset of the spots for an app that has asked for them.
    public func deliverSpots(_ scene: SpotScene = .board) async {
        guard additions.contains(.spots) else {
            return
        }
        spotState.set(scene)
        await deliver(Self.sourcesDelta(scene))
        let reset = spotState.read { scene, generation in
            Self.batch("spots", generation: generation, reset: true, records: Self.spotRecords(scene.spots))
        }
        await deliver(reset)
    }

    /// The fake's answer to a record or spot verb; nil leaves the verb to the rest of the fake.
    func spotReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        guard additions.contains(.spots),
              ["records.subscribe", "records.unsubscribe", "spots.connect", "spots.disconnect", "spots.sendCommand",
               "spots.clearAll"].contains(invoke.verb) else {
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "") -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: nil))
        }
        func text(_ name: String) -> String? {
            if case .utf8(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        return spotState.read { scene, generation -> [LinkMessage] in
            switch invoke.verb {
            case "records.subscribe":
                guard let stream = text("stream") else {
                    return [result(false, "The Core could not read this request.")]
                }
                var backlog = 500
                if case .i64(let value)? = invoke.args.first(where: { $0.name == "backlog" })?.value {
                    backlog = Int(value)
                }
                if stream == "spots" {
                    let records = Self.spotRecords(scene.spots)
                    return [result(true), Self.batch(stream, generation: generation, reset: true,
                                                     records: Array(records.suffix(backlog)))]
                }
                let console = stream.replacingOccurrences(of: "spotConsole:", with: "")
                guard stream.hasPrefix("spotConsole:"), Self.coreSpotSources.contains(console) else {
                    return [result(false, Self.unknownListReason)]
                }
                return [result(true), Self.batch(stream, generation: 1, reset: true,
                                                 records: Array(Self.consoleRecords(scene.consoles[console] ?? [])
                                                    .suffix(backlog)))]
            case "records.unsubscribe":
                return [result(true)]
            case "spots.connect", "spots.disconnect":
                guard let source = text("source"), Self.coreSpotSources.contains(source) else {
                    return [result(false, "The Core does not run that spot source.")]
                }
                let on = invoke.verb == "spots.connect"
                scene.sources[source] = (on ? "connected" : "off", "")
                return [result(true), Self.sourcesDelta(scene)]
            case "spots.sendCommand":
                guard let source = text("source"), let line = text("text") else {
                    return [result(false, "The Core could not read this request.")]
                }
                guard let notConnected = Self.spotsNotConnectedReasons[source] else {
                    return [result(false, Self.noCommandsReason)]
                }
                guard scene.sources[source]?.state == "connected" else {
                    return [result(false, notConnected)]
                }
                var lines = scene.consoles[source] ?? []
                lines.append("> \(line)")
                scene.consoles[source] = lines
                let record = LinkMessage.RecordBatch.Record(id: String(lines.count), fields: ["line": .string("> \(line)")])
                return [result(true), .recordBatch(LinkMessage.RecordBatch(
                    stream: "spotConsole:\(source)", generation: 1, reset: false, upserts: [record], removes: []))]
            default:
                scene.spots = []
                generation += 1
                return [result(true), Self.batch("spots", generation: generation, reset: true, records: [])]
            }
        }
    }

    private static func batch(_ stream: String, generation: Int64, reset: Bool,
                              records: [LinkMessage.RecordBatch.Record]) -> LinkMessage {
        .recordBatch(LinkMessage.RecordBatch(stream: stream, generation: generation, reset: reset, upserts: records,
                                             removes: []))
    }

    private static func spotRecords(_ spots: [SceneSpot]) -> [LinkMessage.RecordBatch.Record] {
        spots.enumerated().map { index, spot in
            var fields: [String: LinkJSON] = [
                "timeUtc": .string(spot.timeUtc), "frequencyHz": .number(spot.frequencyHz), "call": .string(spot.call),
                "mode": .string(spot.mode), "source": .string(spot.source), "spotter": .string(spot.spotter),
                "comment": .string(spot.comment), "band": .number(Double(spot.band)),
                "dxccColour": .string(spot.dxccColour), "dxccPriority": .number(Double(spot.dxccPriority)),
            ]
            if let mode = spot.resolvedMode {
                fields["resolvedMode"] = .number(Double(mode))
            }
            return LinkMessage.RecordBatch.Record(id: String(index), fields: fields)
        }
    }

    private static func consoleRecords(_ lines: [String]) -> [LinkMessage.RecordBatch.Record] {
        lines.enumerated().map { index, line in
            LinkMessage.RecordBatch.Record(id: String(index + 1), fields: ["line": .string(line)])
        }
    }

    /// `spotSources` as the scene has it, every source's state and words.
    private static func sourcesDelta(_ scene: SpotScene) -> LinkMessage {
        var properties: [LinkMessage.PropertyEntry] = []
        for (index, source) in coreSpotSources.enumerated() {
            let entry = scene.sources[source] ?? ("off", "")
            properties.append(.init(ordinal: UInt16(index * 2), name: "\(source)State", value: .utf8(entry.state)))
            properties.append(.init(ordinal: UInt16(index * 2 + 1), name: "\(source)Text", value: .utf8(entry.text)))
        }
        if let freedv = scene.freedv {
            properties.append(.init(ordinal: 8, name: "freedvReporterState", value: .utf8(freedv.state)))
            properties.append(.init(ordinal: 9, name: "freedvReporterText", value: .utf8(freedv.text)))
            properties.append(.init(ordinal: 10, name: "freedvReporterHidden", value: .bool(freedv.hidden)))
        }
        return .delta(LinkMessage.Delta(key: "spotSources", properties: properties))
    }
}

extension FakeStation.Additions {
    /// The Core's spots and spot sources, with `recordStreamVersion` 1. Not in ``all``.
    public static let spots = FakeStation.Additions(rawValue: 1 << 17)
    /// With ``spots``, `recordStreamVersion` 2: the Core names each spot's
    /// mode (`resolvedMode`) where a scene spot sets one. Not in ``all``.
    public static let spotModes = FakeStation.Additions(rawValue: 1 << 23)
}
