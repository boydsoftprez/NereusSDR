// NereusSDR for iOS: the fake Core's FreeDV Reporter: its station list, console, state and verbs
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The fake Core's FreeDV Reporter (link document sections 7.1, 7.7 and
/// 9.1), played with ``Additions/spots`` on a fake that advertises
/// `stationFreedvVersion` (the fake plays the suite's Core at 1;
/// ``Additions/freedvBand`` plays it at 2, as the suite's fixtures now send). It answers `records.subscribe` for `freedvStations` and
/// `spotConsole:freedvReporter` with a reset, `spots.connect` and
/// `spots.disconnect` for `freedvReporter` by moving its state on
/// `spotSources` (and starting its list again, as the Core does),
/// `freedv.setMessage` by keeping the message, `freedv.sendQsy` as the Core
/// checks it, and `freedv.setHidden` by moving "Hide my station". A test sets
/// the scene with ``deliverFreedv(_:)`` and plays one station's news with
/// ``deliverFreedvStation(_:)``. Nothing reaches qso.freedv.org.
extension FakeStation {
    /// One station as the Core's `freedvStations` record carries it.
    public struct SceneStation: Sendable {
        public var id: String
        public var callsign: String
        public var gridSquare: String
        public var distanceKm: Double
        public var headingDeg: Double
        public var headingCardinal: String
        public var version: String
        public var frequencyHz: Double
        public var txMode: String
        public var status: String
        public var userMessage: String
        public var lastTxUtc: String
        public var lastRxCallsign: String
        public var lastRxMode: String
        public var snrDb: Double
        public var lastUpdateUtc: String
        public var transmitting: Bool
        public var receivingFrom: String
        public var messageChangedAtMs: Double
        public var lastRxUtc: String
        /// Sent only when set (a Core at `stationFreedvVersion` 2 names it).
        public var band: Int?

        public init(id: String, callsign: String, gridSquare: String, distanceKm: Double = 0, headingDeg: Double = 0,
                    headingCardinal: String = "", version: String = "freedv-gui 2.0", frequencyHz: Double,
                    txMode: String = "RADE", status: String = "Active", userMessage: String = "",
                    lastTxUtc: String = "", lastRxCallsign: String = "", lastRxMode: String = "", snrDb: Double = -99,
                    lastUpdateUtc: String, transmitting: Bool = false, receivingFrom: String = "",
                    messageChangedAtMs: Double = 0, lastRxUtc: String = "", band: Int? = nil) {
            self.id = id
            self.callsign = callsign
            self.gridSquare = gridSquare
            self.distanceKm = distanceKm
            self.headingDeg = headingDeg
            self.headingCardinal = headingCardinal
            self.version = version
            self.frequencyHz = frequencyHz
            self.txMode = txMode
            self.status = status
            self.userMessage = userMessage
            self.lastTxUtc = lastTxUtc
            self.lastRxCallsign = lastRxCallsign
            self.lastRxMode = lastRxMode
            self.snrDb = snrDb
            self.lastUpdateUtc = lastUpdateUtc
            self.transmitting = transmitting
            self.receivingFrom = receivingFrom
            self.messageChangedAtMs = messageChangedAtMs
            self.lastRxUtc = lastRxUtc
            self.band = band
        }

        var record: LinkMessage.RecordBatch.Record {
            var fields: [String: LinkJSON] = [
                "callsign": .string(callsign), "gridSquare": .string(gridSquare), "distanceKm": .number(distanceKm),
                "headingDeg": .number(headingDeg), "headingCardinal": .string(headingCardinal),
                "version": .string(version), "frequencyHz": .number(frequencyHz), "txMode": .string(txMode),
                "status": .string(status), "userMessage": .string(userMessage), "lastTxUtc": .string(lastTxUtc),
                "lastRxCallsign": .string(lastRxCallsign), "lastRxMode": .string(lastRxMode),
                "snrDb": .number(snrDb), "lastUpdateUtc": .string(lastUpdateUtc), "transmitting": .bool(transmitting),
                "receivingFrom": .string(receivingFrom), "messageChangedAtMs": .number(messageChangedAtMs),
                "lastRxUtc": .string(lastRxUtc),
            ]
            if let band {
                fields["band"] = .number(Double(band))
            }
            return LinkMessage.RecordBatch.Record(id: id, fields: fields)
        }
    }

    /// What the fake's FreeDV Reporter shows.
    public struct FreeDVScene: Sendable {
        /// Oldest first, as the Core keeps them.
        public var stations: [SceneStation]
        /// `off`, `connecting`, `connected` or `error`, with the Core's words.
        public var state: String
        public var text: String
        public var hidden: Bool
        public var console: [String]

        public init(stations: [SceneStation], state: String = "connected", text: String = "", hidden: Bool = false,
                    console: [String] = []) {
            self.stations = stations
            self.state = state
            self.text = text
            self.hidden = hidden
            self.console = console
        }

        /// Picture 21's stations: W1ABC transmitting, K4XYZ hearing it,
        /// G4ABC with a new message, and four more, on 20, 40 and 80
        /// metres. No grid square is set at the Core, so distance and
        /// heading are not known. Each carries its band (the catalogue's
        /// numbering), which only a Core at version 2 sends.
        public static var board: FreeDVScene {
            func time(_ clock: String) -> String { "2026-09-28T\(clock)Z" }
            let stations = [
                SceneStation(id: "s7", callsign: "JA1XYZ", gridSquare: "PM95", frequencyHz: 14_236_000,
                             lastTxUtc: time("17:20:00"), lastUpdateUtc: time("18:52:00"), band: 5),
                SceneStation(id: "s6", callsign: "N0ABC", gridSquare: "EN34", frequencyHz: 3_625_000, txMode: "1600",
                             lastTxUtc: time("17:58:00"), lastRxCallsign: "K9XYZ", lastRxMode: "1600", snrDb: 9,
                             lastUpdateUtc: time("19:09:00"), band: 1),
                SceneStation(id: "s5", callsign: "DL1XYZ", gridSquare: "JO62", version: "freedv-gui 1.9",
                             frequencyHz: 7_177_000, txMode: "700E", userMessage: "Sked 7.177 at 20Z",
                             lastTxUtc: time("18:15:00"), lastUpdateUtc: time("19:15:00"),
                             messageChangedAtMs: 1_790_000_000_000, band: 3),
                SceneStation(id: "s4", callsign: "VK3XYZ", gridSquare: "QF22", frequencyHz: 14_236_000,
                             lastTxUtc: time("18:40:00"), lastRxCallsign: "G4ABC", lastRxMode: "700D", snrDb: 2,
                             lastUpdateUtc: time("19:28:00"), band: 5),
                SceneStation(id: "s3", callsign: "G4ABC", gridSquare: "IO91", frequencyHz: 14_236_000, txMode: "700D",
                             userMessage: "Listening on 20, beam north-west", lastTxUtc: time("18:52:00"),
                             lastUpdateUtc: time("19:38:00"), messageChangedAtMs: 1_790_000_100_000, band: 5),
                SceneStation(id: "s2", callsign: "K4XYZ", gridSquare: "EM73", version: "NereusSDR 0.5.2",
                             frequencyHz: 7_177_000, lastTxUtc: time("19:31:00"), lastRxCallsign: "W1ABC",
                             lastRxMode: "RADE", snrDb: 6, lastUpdateUtc: time("19:39:00"), receivingFrom: "W1ABC",
                             lastRxUtc: time("19:39:00"), band: 3),
                SceneStation(id: "s1", callsign: "W1ABC", gridSquare: "FN42", frequencyHz: 14_236_000, status: "TX",
                             userMessage: "QRV RADE, 5 W and a dipole", lastTxUtc: time("19:40:00"),
                             lastUpdateUtc: time("19:40:00"), transmitting: true,
                             messageChangedAtMs: 1_789_000_000_000, band: 5),
            ]
            return FreeDVScene(stations: stations, console: [
                "Connected to qso.freedv.org",
                "new_connection W1ABC FN42",
                "tx_report W1ABC RADE",
            ])
        }
    }

    /// What the fake's FreeDV Reporter is doing now, behind its own lock.
    final class FreeDVState: @unchecked Sendable {
        private let lock = NSLock()
        private var scene = FreeDVScene(stations: [], state: "off")
        private var generation: Int64 = 1
        private var message = ""
        private var qsy: [(callsign: String, frequencyHz: Int64)] = []
        /// The fake plays a Core that runs FreeDV Reporter (it was not made without its capability).
        var runs = true

        func read<Result>(_ body: (inout FreeDVScene, inout Int64, inout String,
                                   inout [(callsign: String, frequencyHz: Int64)]) -> Result) -> Result {
            lock.withLock { body(&scene, &generation, &message, &qsy) }
        }
    }

    public static let freedvStationsStream = "freedvStations"
    public static let freedvConsoleStream = "spotConsole:freedvReporter"
    public static let freedvNotRunningReason = "The Core does not run FreeDV Reporter."
    public static let freedvNotConnectedReason = "FreeDV Reporter is not connected on the Core."
    public static let freedvNoFrequencyReason = "Enter a frequency for the QSY request first."
    public static let freedvNoStationReason = "Choose a station for the QSY request first."

    /// The status message the app last sent with `freedv.setMessage`.
    public var freedvMessage: String {
        freedvState.read { _, _, message, _ in message }
    }

    /// Each QSY request the Core took, oldest first.
    public var freedvQsyRequests: [(callsign: String, frequencyHz: Int64)] {
        freedvState.read { _, _, _, qsy in qsy }
    }

    /// Sets the scene and sends it as the Core would: FreeDV Reporter's state
    /// on `spotSources`, then its list from the start.
    public func deliverFreedv(_ scene: FreeDVScene = .board) async {
        guard additions.contains(.spots) else {
            return
        }
        let reset = freedvState.read { current, generation, _, _ -> LinkMessage in
            current = scene
            generation += 1
            return Self.freedvReset(scene, generation: generation)
        }
        await deliver(Self.freedvStateDelta(scene))
        await deliver(reset)
    }

    /// One station's news: the Core's upsert of its record.
    public func deliverFreedvStation(_ station: SceneStation) async {
        let batch = freedvState.read { scene, generation, _, _ -> LinkMessage in
            if let index = scene.stations.firstIndex(where: { $0.id == station.id }) {
                scene.stations[index] = station
            } else {
                scene.stations.append(station)
            }
            return .recordBatch(LinkMessage.RecordBatch(stream: Self.freedvStationsStream, generation: generation,
                                                        reset: false, upserts: [station.record], removes: []))
        }
        await deliver(batch)
    }

    /// The fake's answer to a FreeDV Reporter verb or stream; nil leaves it to the rest of the fake.
    func freedvReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        func text(_ name: String) -> String? {
            if case .utf8(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        let ownVerb = invoke.verb.hasPrefix("freedv.")
        let ownStream = invoke.verb == "records.subscribe"
            && [Self.freedvStationsStream, Self.freedvConsoleStream].contains(text("stream") ?? "")
        let ownSource = ["spots.connect", "spots.disconnect"].contains(invoke.verb)
            && text("source") == FreeDVStationSource.name
        guard additions.contains(.spots), ownVerb || ownStream || ownSource else {
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "") -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: nil))
        }
        guard freedvState.runs else {
            return [result(false, ownVerb ? Self.unknownVerbReason : Self.freedvNotRunningReason)]
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        return freedvState.read { scene, generation, message, qsy -> [LinkMessage] in
            switch invoke.verb {
            case "records.subscribe":
                var backlog = 1000
                if case .i64(let value)? = invoke.args.first(where: { $0.name == "backlog" })?.value {
                    backlog = Int(value)
                }
                if text("stream") == Self.freedvStationsStream {
                    return [result(true), Self.freedvReset(scene, generation: generation, backlog: backlog)]
                }
                let records = scene.console.enumerated().map { index, line in
                    LinkMessage.RecordBatch.Record(id: String(index + 1), fields: ["line": .string(line)])
                }
                return [result(true), .recordBatch(LinkMessage.RecordBatch(
                    stream: Self.freedvConsoleStream, generation: 1, reset: true,
                    upserts: Array(records.suffix(backlog)), removes: []))]
            case "spots.connect", "spots.disconnect":
                scene.state = invoke.verb == "spots.connect" ? "connected" : "off"
                scene.text = ""
                if scene.state == "off" {
                    scene.stations = []
                }
                generation += 1
                return [result(true), Self.freedvStateDelta(scene), Self.freedvReset(scene, generation: generation)]
            case "freedv.setMessage":
                guard let value = text("text") else {
                    return [result(false, "The Core could not read this request.")]
                }
                message = value
                return [result(true)]
            case "freedv.setHidden":
                guard case .bool(let on)? = invoke.args.first(where: { $0.name == "on" })?.value else {
                    return [result(false, "The Core could not read this request.")]
                }
                scene.hidden = on
                return [result(true), Self.freedvStateDelta(scene)]
            case "freedv.sendQsy":
                var hz: Int64 = 0
                if case .i64(let value)? = invoke.args.first(where: { $0.name == "frequencyHz" })?.value {
                    hz = value
                }
                let callsign = text("callsign") ?? ""
                guard scene.state == "connected" else {
                    return [result(false, Self.freedvNotConnectedReason)]
                }
                guard !callsign.isEmpty else {
                    return [result(false, Self.freedvNoStationReason)]
                }
                guard hz > 0 else {
                    return [result(false, Self.freedvNoFrequencyReason)]
                }
                guard scene.stations.contains(where: { $0.callsign.caseInsensitiveCompare(callsign) == .orderedSame })
                else {
                    return [result(false, "\(callsign) is not on FreeDV Reporter now.")]
                }
                qsy.append((callsign, hz))
                return [result(true)]
            default:
                return [result(false, Self.unknownVerbReason)]
            }
        }
    }

    private static func freedvReset(_ scene: FreeDVScene, generation: Int64, backlog: Int = 1000) -> LinkMessage {
        .recordBatch(LinkMessage.RecordBatch(stream: freedvStationsStream, generation: generation, reset: true,
                                             upserts: Array(scene.stations.map(\.record).suffix(backlog)),
                                             removes: []))
    }

    /// FreeDV Reporter's three properties on `spotSources` (after the four sources' eight).
    private static func freedvStateDelta(_ scene: FreeDVScene) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "spotSources", properties: [
            .init(ordinal: 8, name: "freedvReporterState", value: .utf8(scene.state)),
            .init(ordinal: 9, name: "freedvReporterText", value: .utf8(scene.text)),
            .init(ordinal: 10, name: "freedvReporterHidden", value: .bool(scene.hidden)),
        ]))
    }
}

/// FreeDV Reporter's source name on the link.
enum FreeDVStationSource {
    static let name = "freedvReporter"
}

extension FakeStation.Additions {
    /// A Core whose FreeDV Reporter records name each station's band:
    /// `stationFreedvVersion` 2. Not in ``all``.
    public static let freedvBand = FakeStation.Additions(rawValue: 1 << 20)
}
