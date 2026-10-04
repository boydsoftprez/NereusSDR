// NereusSDR for iOS: the fake Core's station tools: its TCI server and connected apps, and its support bundle
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The fake Core's tools for the Tools tab's pages. With
/// ``Additions/stationTci`` it plays a Core that runs its station TCI
/// server at `stationTciVersion` 2 (the accessory control document, "The
/// `stationTci` object and the station TCI server"): ``deliverStationTci(_:)``
/// sends the `stationTci` object, `setStationTci` saves the switch and port
/// and sends them back in a delta, `records.subscribe` for `tciClients`
/// answers with a reset of the scene's apps, and
/// `disconnectStationTciClient` removes one, and `setStationTciOptions` saves
/// the four options and sends them back. With ``Additions/supportBundle``
/// it answers `support.collect` with ``supportBundle`` in base64 (link
/// document section 9.1), `support.setLogCategories` by saving the
/// categories and sending `radio`'s `logCategories`, and `records.subscribe`
/// for `coreLog` with a reset of the newest lines; ``deliverCoreLog(_:)``
/// adds lines as the Core's log grows. A refusal queued with
/// ``refuseNext(_:reason:)`` answers any of these verbs. No app and no file
/// leaves the test.
extension FakeStation {
    /// One app connected to the fake Core's TCI server.
    public struct SceneTciClient: Sendable, Equatable {
        public var id: String
        public var name: String
        public var address: String
        public var subscriptions: [String]
        public var transmitting: Bool
        public var lastCommand: String

        public init(id: String, name: String, address: String, subscriptions: [String] = [],
                    transmitting: Bool = false, lastCommand: String = "") {
            self.id = id
            self.name = name
            self.address = address
            self.subscriptions = subscriptions
            self.transmitting = transmitting
            self.lastCommand = lastCommand
        }
    }

    /// The fake Core's TCI server.
    public struct SceneTci: Sendable, Equatable {
        public var enabled: Bool
        public var port: Int64
        public var stationAddress: String
        public var error: String
        public var clients: [SceneTciClient]
        /// The four version-2 options, in `setStationTciOptions`'s order.
        public var options: [Bool]
        /// The server's settings (`stationTciSettingsVersion` 1), by
        /// property name, at the Core's own defaults.
        public var settings: [String: LinkMessage.PropertyValue] = SceneTci.coreSettings

        /// The Core's defaults for the TCI server's settings.
        public static let coreSettings: [String: LinkMessage.PropertyValue] = [
            "rateLimitMs": .i64(100), "cwBecomesCwuAbove10mhz": .bool(false), "iqSwap": .bool(true),
            "alwaysStreamIq": .bool(false), "audioBlockSamples": .i64(2048), "txChannel": .i64(2),
            "rxSensorIntervalMs": .i64(200), "txSensorIntervalMs": .i64(200),
            "forgetRx2VfoBOnDisconnect": .bool(false), "useRx1VfoaForRx2Vfoa": .bool(false),
            "copyRx2VfobToVfoa": .bool(false),
        ]

        public init(enabled: Bool, port: Int64 = 50001, stationAddress: String = "", error: String = "",
                    clients: [SceneTciClient] = [], options: [Bool] = [true, true, false, true]) {
            self.enabled = enabled
            self.port = port
            self.stationAddress = stationAddress
            self.error = error
            self.clients = clients
            self.options = options
        }

        /// Two apps on a running server, at a documentation address.
        public static let board = SceneTci(enabled: true, stationAddress: "192.0.2.10", clients: [
            SceneTciClient(id: "1", name: "WSJT-X", address: "192.0.2.20", subscriptions: ["audio", "sensors"],
                           lastCommand: "vfo:0,0;"),
            SceneTciClient(id: "2", name: "RF2K-S", address: "192.0.2.30", lastCommand: "trx:0;"),
        ])
    }

    final class StationToolsState: @unchecked Sendable {
        private let lock = NSLock()
        private var scene = SceneTci(enabled: false)
        private var generation: Int64 = 1
        /// The Core's logging categories that are on, joined by commas.
        private var categories = ""
        /// The Core's log, oldest first, numbered from 1.
        private var log: [String] = FakeStation.coreLogLines

        func set(_ scene: SceneTci) {
            lock.withLock { self.scene = scene }
        }

        func read<Result>(_ body: (inout SceneTci, inout Int64) -> Result) -> Result {
            lock.withLock { body(&scene, &generation) }
        }

        func support<Result>(_ body: (inout String, inout [String]) -> Result) -> Result {
            lock.withLock { body(&categories, &log) }
        }
    }

    public static let tciClass = "StationTciModel"
    public static let tciKey = "stationTci"
    public static let tciClientsStream = "tciClients"
    public static let coreLogStream = "coreLog"
    /// The fake Core's log, as its log file has it.
    public static let coreLogLines = [
        "[18:34:00.120] INF: Core started, version 0.5.2",
        "[18:34:00.410] INF: Radio found at 192.0.2.40",
        "[18:34:01.002] INF: Connected to the radio",
    ]
    public static let tciPortReason = "Choose a TCI port from 1024 to 65535."
    public static let tciUnknownClientReason = "That app is not connected to the Core's TCI server."
    /// The bundle the fake Core makes: a stored ZIP's opening bytes and a line, enough to tell it apart.
    public static let supportBundle = Data([0x50, 0x4B, 0x03, 0x04]) + Data("fake Core bundle\n".utf8)

    static func stationToolCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        additions.contains(.stationTci) ? [("stationTciVersion", 2)] : []
    }

    /// Sets the TCI server's scene and sends it as a Core at version 2
    /// would: the class's schema and the `stationTci` object, then a reset
    /// of the connected apps for an app that asked for them.
    public func deliverStationTci(_ scene: SceneTci = .board) async throws {
        stationToolsState.set(scene)
        await deliver(.schema(try Self.schema(ofClass: Self.tciClass)))
        await deliver(.objectCreate(try Self.objectCreate(key: Self.tciKey, className: Self.tciClass,
                                                          values: Self.tciValues(scene))))
        let reset = stationToolsState.read { scene, generation in
            Self.tciReset(scene, generation: generation)
        }
        await deliver(reset)
    }

    /// Adds `lines` to the fake Core's log, and sends them to an app
    /// following it as upserts, each numbered after the last.
    public func deliverCoreLog(_ lines: [String]) async {
        let upserts = stationToolsState.support { _, log -> [LinkMessage.RecordBatch.Record] in
            let first = log.count + 1
            log.append(contentsOf: lines)
            return lines.enumerated().map { offset, line in
                Self.coreLogRecord(number: first + offset, line: line)
            }
        }
        await deliver(.recordBatch(LinkMessage.RecordBatch(stream: Self.coreLogStream, generation: 1, reset: false,
                                                           upserts: upserts, removes: [])))
    }

    /// The Core's logging categories that are on, as it last saved them.
    public var coreLogCategories: String {
        stationToolsState.support { categories, _ in categories }
    }

    func stationToolReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        func text(_ name: String) -> String? {
            if case .utf8(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "",
                    values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: values))
        }
        let tciVerb = ["setStationTci", "setStationTciOptions", "disconnectStationTciClient"].contains(invoke.verb)
            || (["records.subscribe", "records.unsubscribe"].contains(invoke.verb)
                && text("stream") == Self.tciClientsStream)
        if tciVerb, additions.contains(.stationTci) {
            if let reason = takeRefusal(invoke.verb) {
                return [result(false, reason)]
            }
            return stationToolsState.read { scene, generation -> [LinkMessage] in
                switch invoke.verb {
                case "setStationTci":
                    guard case .bool(let on)? = invoke.args.first(where: { $0.name == "enabled" })?.value,
                          case .i64(let port)? = invoke.args.first(where: { $0.name == "port" })?.value,
                          invoke.args.count == 2 else {
                        return [result(false, "The request to turn the station's TCI server on or off was not understood.")]
                    }
                    guard (1024...65535).contains(port) else {
                        return [result(false, Self.tciPortReason)]
                    }
                    scene.enabled = on
                    scene.port = port
                    return [result(true), .delta(LinkMessage.Delta(key: Self.tciKey, properties: [
                        .init(ordinal: 0, name: "enabled", value: .bool(on)),
                        .init(ordinal: 1, name: "port", value: .i64(port)),
                        .init(ordinal: 2, name: "listening", value: .bool(on)),
                    ]))]
                case "setStationTciOptions":
                    let names = ["emulateExpertSdr3", "emulateSunSdr2Pro", "cwluBecomesCw", "sendInitialState"]
                    let flags = names.compactMap { name -> Bool? in
                        if case .bool(let flag)? = invoke.args.first(where: { $0.name == name })?.value {
                            return flag
                        }
                        return nil
                    }
                    guard flags.count == 4, invoke.args.count == 4 else {
                        return [result(false, "The request to change the Core's TCI server settings was not understood.")]
                    }
                    scene.options = flags
                    return [result(true), .delta(LinkMessage.Delta(key: Self.tciKey, properties: names.enumerated()
                        .map { index, name in
                            .init(ordinal: UInt16(5 + index), name: name, value: .bool(flags[index]))
                        }))]
                case "disconnectStationTciClient":
                    guard let id = text("id"), scene.clients.contains(where: { $0.id == id }) else {
                        return [result(false, Self.tciUnknownClientReason)]
                    }
                    scene.clients.removeAll { $0.id == id }
                    return [result(true), .recordBatch(LinkMessage.RecordBatch(
                        stream: Self.tciClientsStream, generation: generation, reset: false, upserts: [],
                        removes: [id]))]
                case "records.subscribe":
                    return [result(true), Self.tciReset(scene, generation: generation)]
                default:
                    return [result(true)]
                }
            }
        }
        let supportVerb = invoke.verb == "support.setLogCategories"
            || (["records.subscribe", "records.unsubscribe"].contains(invoke.verb)
                && text("stream") == Self.coreLogStream)
        if supportVerb, additions.contains(.supportBundle) {
            if let reason = takeRefusal(invoke.verb) {
                return [result(false, reason)]
            }
            return stationToolsState.support { categories, log -> [LinkMessage] in
                switch invoke.verb {
                case "support.setLogCategories":
                    guard let next = text("categories"), invoke.args.count == 1 else {
                        return [result(false, "The Core could not read this request.")]
                    }
                    categories = next
                    let ordinal = (try? Self.schema(ofClass: "RadioModel").fields
                        .first { $0.name == "logCategories" }?.ordinal) ?? 26
                    return [result(true), .delta(LinkMessage.Delta(key: "radio", properties: [
                        .init(ordinal: ordinal, name: "logCategories", value: .utf8(next)),
                    ]))]
                case "records.subscribe":
                    var backlog = 0
                    if case .i64(let asked)? = invoke.args.first(where: { $0.name == "backlog" })?.value {
                        backlog = Int(max(0, asked))
                    }
                    let first = max(0, log.count - backlog)
                    let upserts = log.enumerated().dropFirst(first).map { index, line in
                        Self.coreLogRecord(number: index + 1, line: line)
                    }
                    return [result(true), .recordBatch(LinkMessage.RecordBatch(
                        stream: Self.coreLogStream, generation: 1, reset: true, upserts: Array(upserts), removes: []))]
                default:
                    return [result(true)]
                }
            }
        }
        if invoke.verb == "support.collect", additions.contains(.supportBundle) {
            if let reason = takeRefusal(invoke.verb) {
                return [result(false, reason)]
            }
            guard invoke.args.isEmpty else {
                return [result(false, "The Core could not read this request.")]
            }
            return [result(true, values: [.init(name: "bundle",
                                                value: .utf8(Self.supportBundle.base64EncodedString()))])]
        }
        return nil
    }

    private static func tciValues(_ scene: SceneTci) -> [String: LinkMessage.PropertyValue] {
        ["enabled": .bool(scene.enabled), "port": .i64(scene.port), "listening": .bool(scene.enabled),
         "stationAddress": .utf8(scene.enabled ? scene.stationAddress : ""), "error": .utf8(scene.error),
         "emulateExpertSdr3": .bool(scene.options[0]), "emulateSunSdr2Pro": .bool(scene.options[1]),
         "cwluBecomesCw": .bool(scene.options[2]), "sendInitialState": .bool(scene.options[3])]
            .merging(scene.settings) { own, _ in own }
    }

    private static func coreLogRecord(number: Int, line: String) -> LinkMessage.RecordBatch.Record {
        LinkMessage.RecordBatch.Record(id: "\(number)", fields: ["line": .string(line)])
    }

    private static func tciReset(_ scene: SceneTci, generation: Int64) -> LinkMessage {
        .recordBatch(LinkMessage.RecordBatch(stream: tciClientsStream, generation: generation, reset: true,
                                             upserts: scene.clients.map { client in
                                                 LinkMessage.RecordBatch.Record(id: client.id, fields: [
                                                     "id": .string(client.id), "name": .string(client.name),
                                                     "address": .string(client.address),
                                                     "subscriptions": .array(client.subscriptions.map { .string($0) }),
                                                     "transmitting": .bool(client.transmitting),
                                                     "lastCommand": .string(client.lastCommand),
                                                 ])
                                             }, removes: []))
    }
}

extension FakeStation.Additions {
    /// The Core's station TCI server at `stationTciVersion` 2. Not in ``all``.
    public static let stationTci = FakeStation.Additions(rawValue: 1 << 21)
    /// The Core answers `support.collect` with a bundle. Not in ``all``.
    public static let supportBundle = FakeStation.Additions(rawValue: 1 << 22)
}
