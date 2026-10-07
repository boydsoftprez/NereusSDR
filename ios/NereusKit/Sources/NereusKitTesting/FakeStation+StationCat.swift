// NereusSDR for iOS: the fake Core's CAT: the stationCat object, its four verbs and the catLog record stream
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The CAT the fake Core runs for its station. With ``Additions/stationCat``
/// it plays a Core at `stationCatVersion` 1: ``deliverStationCat(_:)`` sends
/// the `stationCat` object (class `StationCatModel`, seven JSON texts);
/// `setStationCatChannel` and `setStationCatGlobal` lay the config they carry
/// over the scene's, as the Core does, and send the result back in a delta;
/// `testStationCatCommand` answers with a `reply` value, logs both lines and
/// sends `lastTest`; `refreshStationCatDevices` sends `platform` again; and
/// `records.subscribe` for `catLog` answers with a reset of the newest lines.
/// A refusal queued with ``refuseNext(_:reason:)`` answers any of these verbs.
extension FakeStation {
    /// The fake Core's CAT, each property as the Core's JSON.
    public struct SceneCat: Sendable, Equatable {
        public var global: [String: LinkJSON]
        /// CAT 1 to CAT 4.
        public var channels: [[String: LinkJSON]]
        public var platform: [String: LinkJSON]
        public var lastTest: [String: LinkJSON]
        /// The CAT log, oldest first.
        public var log: [LogLine]

        public init(global: [String: LinkJSON], channels: [[String: LinkJSON]], platform: [String: LinkJSON],
                    lastTest: [String: LinkJSON] = [:], log: [LogLine] = []) {
            self.global = global
            self.channels = channels
            self.platform = platform
            self.lastTest = lastTest
            self.log = log
        }

        /// CAT 1 on with two TCP programs and its virtual serial port open,
        /// CAT 2 on a USB serial adapter, CAT 3 off with its slice closed,
        /// CAT 4 off; a Core on a Mac with two serial devices.
        public static let board = SceneCat(
            global: ["config": .object(FakeStation.catGlobalConfig), "aiActive": .bool(true),
                     "pttState": .string("Disabled")],
            channels: [
                FakeStation.catChannel(1, config: ["tcpEnabled": .bool(true), "ptyEnabled": .bool(true),
                                                   "primarySliceId": .number(0), "secondarySliceId": .number(1)],
                                       status: ["state": .string("Listening"), "tcp": .string("Listening"),
                                                "pty": .string("Listening"), "tcpBoundAddress": .string("127.0.0.1"),
                                                "tcpBoundPort": .number(13013), "tcpClients": .number(2),
                                                "ptyPath": .string("/dev/ttys004")]),
                FakeStation.catChannel(2, config: ["serialEnabled": .bool(true),
                                                   "serialDevice": .string("/dev/cu.usbserial-A10K"),
                                                   "serialBaud": .number(38400), "primarySliceId": .number(1)],
                                       status: ["state": .string("Listening"), "serial": .string("Listening")]),
                FakeStation.catChannel(3, config: ["primarySliceId": .number(5)], status: ["state": .string("Disabled")],
                                       primaryValid: false),
                FakeStation.catChannel(4, config: [:], status: ["state": .string("Disabled")]),
            ],
            platform: ["serial": .bool(true), "pty": .bool(true), "markSpaceParity": .bool(false),
                       "oneAndHalfStop": .bool(false),
                       "serialDevices": .array([.string("/dev/cu.usbserial-A10K"), .string("/dev/cu.usbmodem1101")]),
                       "ptyDialects": .array([
                           .object(["value": .string(FakeStation.catNativeDialect),
                                    "label": .string("Kenwood and ZZ commands")]),
                           .object(["value": .string("Rigctld"), "label": .string("Hamlib rigctld")]),
                       ])],
            log: [
                LogLine(channel: 1, inbound: true, text: "FA;", timeMs: 1_790_000_000_100),
                LogLine(channel: 1, inbound: false, text: "FA00014074000;", timeMs: 1_790_000_000_102),
                LogLine(channel: 2, inbound: true, text: "ZZFB;", timeMs: 1_790_000_000_350),
                LogLine(channel: 2, inbound: false, text: "ZZFB00014076000;", timeMs: 1_790_000_000_352),
            ])
    }

    /// One line of the fake Core's CAT traffic.
    public struct LogLine: Sendable, Equatable {
        public var channel: Int64
        public var inbound: Bool
        public var text: String
        public var timeMs: Int64

        public init(channel: Int64, inbound: Bool, text: String, timeMs: Int64) {
            self.channel = channel
            self.inbound = inbound
            self.text = text
            self.timeMs = timeMs
        }
    }

    final class StationCatState: @unchecked Sendable {
        private let lock = NSLock()
        private var scene = SceneCat.board
        private var following = false

        func read<Result>(_ body: (inout SceneCat, inout Bool) -> Result) -> Result {
            lock.withLock { body(&scene, &following) }
        }
    }

    public static let catClass = "StationCatModel"
    public static let catKey = "stationCat"
    public static let catLogStream = "catLog"
    /// The value the fake Core gives its own PTY dialect. A real Core names
    /// it after the program whose commands it speaks; the fake stands in.
    public static let catNativeDialect = "Native"
    public static let catNotUnderstoodReason = "The request to set up the Core's CAT was not understood."
    public static let catChannelRefusedReason =
        "Configuration refused: check address, port, format and exclusive device assignment."
    /// The replies the fake Core's tester gives; anything else answers `?;`.
    public static let catTestReplies = ["FA;": "FA00014074000;", "FB;": "FB00014076000;", "ID;": "ID019;",
                                        "ZZFA;": "ZZFA00014074000;", "MD;": "MD2;"]

    /// The Core's global settings at its own defaults, AI on for TCP.
    static let catGlobalConfig: [String: LinkJSON] = [
        "sendWelcome": .bool(true), "rigIdentity": .string("TS-2000"), "allowKenwoodAi": .bool(true),
        "aiEnabled": .bool(true), "aiSerial1": .bool(false), "aiSerial2": .bool(true), "aiSerial3": .bool(false),
        "aiSerial4": .bool(false), "aiTcp": .bool(true), "digitalReportsSideband": .bool(false),
        "recenterVfo": .bool(false), "serialNumber": .string("0000-0000"), "limitReportedPower": .bool(false),
        "rttyOffsetAEnabled": .bool(false), "rttyOffsetBEnabled": .bool(false), "rttyDiguHz": .number(2125),
        "rttyDiglHz": .number(2125), "pttEnabled": .bool(false), "pttDeviceSource": .string("None"),
        "pttSerialDevice": .string(""), "pttUseCts": .bool(false), "pttUseDsr": .bool(false),
        "pttChannel": .number(1), "pttSerialBaud": .number(115200), "pttSerialParity": .string("None"),
        "pttSerialDataBits": .number(8), "pttSerialStopBits": .string("1"),
    ]

    /// One channel at the Core's defaults, with `config` and `status` over them.
    public static func catChannel(_ number: Int, config: [String: LinkJSON], status: [String: LinkJSON],
                                  primaryValid: Bool = true, secondaryValid: Bool = true) -> [String: LinkJSON] {
        let defaults: [String: LinkJSON] = [
            "channel": .number(Double(number)), "primarySliceId": .number(0), "secondarySliceId": .number(-1),
            "tcpEnabled": .bool(false), "serialEnabled": .bool(false), "ptyEnabled": .bool(false),
            "rigctldEnabled": .bool(false), "tcpBindAddress": .string("127.0.0.1"),
            "rigctldBindAddress": .string("127.0.0.1"), "tcpPort": .number(Double(13012 + number)),
            "rigctldPort": .number(Double(4531 + number)), "serialDevice": .string(""), "serialBaud": .number(115200),
            "serialParity": .string("None"), "serialDataBits": .number(8), "serialStopBits": .string("1"),
            "ptyDialect": .string(catNativeDialect),
        ]
        let statusDefaults: [String: LinkJSON] = [
            "state": .string("Disabled"), "tcp": .string("Stopped"), "serial": .string("Stopped"),
            "pty": .string("Stopped"), "rigctld": .string("Stopped"), "tcpBoundAddress": .string(""),
            "tcpBoundPort": .number(0), "rigctldBoundAddress": .string(""), "rigctldBoundPort": .number(0),
            "tcpClients": .number(0), "rigctldClients": .number(0), "ptyPath": .string(""),
        ]
        return ["config": .object(defaults.merging(config) { _, given in given }),
                "primaryValid": .bool(primaryValid), "secondaryValid": .bool(secondaryValid),
                "status": .object(statusDefaults.merging(status) { _, given in given })]
    }

    static func stationCatCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        additions.contains(.stationCat) ? [("stationCatVersion", 1)] : []
    }

    /// Sets the CAT scene and sends it as a Core at version 1 would: the
    /// class's schema and the `stationCat` object.
    public func deliverStationCat(_ scene: SceneCat = .board) async throws {
        stationCatState.read { current, _ in current = scene }
        await deliver(.schema(try Self.schema(ofClass: Self.catClass)))
        await deliver(.objectCreate(try Self.objectCreate(key: Self.catKey, className: Self.catClass,
                                                          values: Self.catValues(scene))))
    }

    /// Adds `lines` to the fake Core's CAT log, and sends them to an app following it.
    public func deliverCatLog(_ lines: [LogLine]) async {
        let batch = stationCatState.read { scene, following -> LinkMessage? in
            let first = scene.log.count + 1
            scene.log.append(contentsOf: lines)
            guard following else {
                return nil
            }
            return .recordBatch(LinkMessage.RecordBatch(
                stream: Self.catLogStream, generation: 1, reset: false,
                upserts: lines.enumerated().map { Self.catLogRecord(number: first + $0.offset, line: $0.element) },
                removes: []))
        }
        if let batch {
            await deliver(batch)
        }
    }

    /// The CAT scene as the fake Core holds it now.
    public var catScene: SceneCat {
        stationCatState.read { scene, _ in scene }
    }

    func stationCatReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        func text(_ name: String) -> String? {
            if case .utf8(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        func int(_ name: String) -> Int64? {
            if case .i64(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "",
                    values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: values))
        }
        let verbs = ["setStationCatChannel", "setStationCatGlobal", "testStationCatCommand", "refreshStationCatDevices"]
        let logVerb = ["records.subscribe", "records.unsubscribe"].contains(invoke.verb)
            && text("stream") == Self.catLogStream
        guard verbs.contains(invoke.verb) || logVerb, additions.contains(.stationCat) else {
            return nil
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        return stationCatState.read { scene, following -> [LinkMessage] in
            switch invoke.verb {
            case "setStationCatChannel":
                guard invoke.args.count == 2, let channel = int("channel"), (1...4).contains(channel),
                      let configText = text("config") else {
                    return [result(false, Self.catNotUnderstoodReason)]
                }
                guard case .object(var sent)? = try? LinkJSON.parse(configText) else {
                    return [result(false, "The CAT channel's settings were not understood.")]
                }
                let primaryRebind = sent.removeValue(forKey: "primaryRebind") == .bool(true)
                let secondaryRebind = sent.removeValue(forKey: "secondaryRebind") == .bool(true)
                for port in ["tcpPort", "rigctldPort"] {
                    if case .number(let value)? = sent[port], !(0...65535).contains(value) {
                        return [result(false, Self.catChannelRefusedReason)]
                    }
                }
                let index = Int(channel) - 1
                var entry = scene.channels[index]
                guard case .object(let old)? = entry["config"] else {
                    return [result(false, Self.catNotUnderstoodReason)]
                }
                var config = old.merging(sent) { _, given in given }
                config["channel"] = .number(Double(channel))
                if primaryRebind || config["primarySliceId"] != old["primarySliceId"] {
                    entry["primaryValid"] = .bool(true)
                }
                if secondaryRebind || config["secondarySliceId"] != old["secondarySliceId"] {
                    entry["secondaryValid"] = .bool(true)
                }
                entry["config"] = .object(config)
                entry["status"] = .object(Self.catStatus(config, old: entry["status"]))
                scene.channels[index] = entry
                return [result(true), Self.catDelta(scene)]
            case "setStationCatGlobal":
                guard invoke.args.count == 1, let configText = text("config") else {
                    return [result(false, Self.catNotUnderstoodReason)]
                }
                guard case .object(let sent)? = try? LinkJSON.parse(configText) else {
                    return [result(false, "The CAT settings were not understood.")]
                }
                guard case .object(let old)? = scene.global["config"] else {
                    return [result(false, Self.catNotUnderstoodReason)]
                }
                let config = old.merging(sent) { _, given in given }
                scene.global["config"] = .object(config)
                scene.global["pttState"] = .string(config["pttEnabled"] == .bool(true) ? "Armed" : "Disabled")
                return [result(true), Self.catDelta(scene)]
            case "testStationCatCommand":
                guard invoke.args.count == 3, let requestId = int("requestId"), let channel = int("channel"),
                      (1...4).contains(channel), let command = text("command") else {
                    return [result(false, Self.catNotUnderstoodReason)]
                }
                let reply = Self.catTestReplies[command] ?? "?;"
                scene.lastTest = ["requestId": .number(Double(requestId)), "channel": .number(Double(channel)),
                                  "command": .string(command), "reply": .string(reply),
                                  "accepted": .bool(!reply.hasPrefix("?"))]
                let first = scene.log.count + 1
                let lines = [LogLine(channel: channel, inbound: true, text: command, timeMs: 1_790_000_100_000),
                             LogLine(channel: channel, inbound: false, text: reply, timeMs: 1_790_000_100_002)]
                scene.log.append(contentsOf: lines)
                var replies: [LinkMessage] = [result(true, values: [.init(name: "reply", value: .utf8(reply))]),
                                              Self.catDelta(scene)]
                if following {
                    replies.append(.recordBatch(LinkMessage.RecordBatch(
                        stream: Self.catLogStream, generation: 1, reset: false,
                        upserts: lines.enumerated().map {
                            Self.catLogRecord(number: first + $0.offset, line: $0.element)
                        }, removes: [])))
                }
                return replies
            case "refreshStationCatDevices":
                guard invoke.args.isEmpty else {
                    return [result(false, Self.catNotUnderstoodReason)]
                }
                return [result(true), Self.catDelta(scene)]
            case "records.subscribe":
                following = true
                var backlog = 0
                if case .i64(let asked)? = invoke.args.first(where: { $0.name == "backlog" })?.value {
                    backlog = Int(max(0, asked))
                }
                let first = max(0, scene.log.count - backlog)
                let upserts = scene.log.enumerated().dropFirst(first).map { index, line in
                    Self.catLogRecord(number: index + 1, line: line)
                }
                return [result(true), .recordBatch(LinkMessage.RecordBatch(
                    stream: Self.catLogStream, generation: 1, reset: true, upserts: Array(upserts), removes: []))]
            default:
                following = false
                return [result(true)]
            }
        }
    }

    /// A channel's status as the Core would show it after `config`: each
    /// switched-on way listening, the listeners bound where they were asked.
    private static func catStatus(_ config: [String: LinkJSON], old: LinkJSON?) -> [String: LinkJSON] {
        guard case .object(var status)? = old else {
            return [:]
        }
        func on(_ name: String) -> Bool {
            config[name] == .bool(true)
        }
        status["tcp"] = .string(on("tcpEnabled") ? "Listening" : "Stopped")
        status["serial"] = .string(on("serialEnabled") ? "Listening" : "Stopped")
        status["pty"] = .string(on("ptyEnabled") ? "Listening" : "Stopped")
        status["rigctld"] = .string(on("rigctldEnabled") ? "Listening" : "Stopped")
        status["tcpBoundAddress"] = on("tcpEnabled") ? config["tcpBindAddress"] ?? .string("") : .string("")
        status["tcpBoundPort"] = on("tcpEnabled") ? config["tcpPort"] ?? .number(0) : .number(0)
        status["rigctldBoundAddress"] = on("rigctldEnabled") ? config["rigctldBindAddress"] ?? .string("") : .string("")
        status["rigctldBoundPort"] = on("rigctldEnabled") ? config["rigctldPort"] ?? .number(0) : .number(0)
        if !on("tcpEnabled") {
            status["tcpClients"] = .number(0)
        }
        if !on("rigctldEnabled") {
            status["rigctldClients"] = .number(0)
        }
        if !on("ptyEnabled") {
            status["ptyPath"] = .string("")
        } else if status["ptyPath"] == .string("") {
            status["ptyPath"] = .string("/dev/ttys00\(Int(Self.catNumber(config["channel"])) + 3)")
        }
        let any = ["tcpEnabled", "serialEnabled", "ptyEnabled", "rigctldEnabled"].contains(where: on)
        status["state"] = .string(any ? "Listening" : "Disabled")
        return status
    }

    private static func catNumber(_ value: LinkJSON?) -> Double {
        if case .number(let number)? = value {
            return number
        }
        return 0
    }

    private static func catValues(_ scene: SceneCat) -> [String: LinkMessage.PropertyValue] {
        var values: [String: LinkMessage.PropertyValue] = [
            "global": .utf8(LinkJSON.object(scene.global).compactText),
            "platform": .utf8(LinkJSON.object(scene.platform).compactText),
            "lastTest": .utf8(scene.lastTest.isEmpty ? "" : LinkJSON.object(scene.lastTest).compactText),
        ]
        for (index, channel) in scene.channels.enumerated() {
            values["channel\(index + 1)"] = .utf8(LinkJSON.object(channel).compactText)
        }
        return values
    }

    /// Every property, as the Core sends them all after a change.
    private static func catDelta(_ scene: SceneCat) -> LinkMessage {
        let ordinals = (try? schema(ofClass: catClass).fields) ?? []
        let values = catValues(scene)
        return .delta(LinkMessage.Delta(key: catKey, properties: ordinals.compactMap { field in
            values[field.name].map { LinkMessage.PropertyEntry(ordinal: field.ordinal, name: field.name, value: $0) }
        }))
    }

    private static func catLogRecord(number: Int, line: LogLine) -> LinkMessage.RecordBatch.Record {
        LinkMessage.RecordBatch.Record(id: "\(number)", fields: [
            "channel": .number(Double(line.channel)), "inbound": .bool(line.inbound), "text": .string(line.text),
            "time": .number(Double(line.timeMs)),
        ])
    }
}

extension FakeStation.Additions {
    /// The Core's CAT at `stationCatVersion` 1. Not in ``all``.
    public static let stationCat = FakeStation.Additions(rawValue: 1 << 24)
}
