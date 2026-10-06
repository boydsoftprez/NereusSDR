// NereusSDR for iOS: the fake Core's amplifiers and tuner: the Power Genius, the Tuner Genius and the RF2K-S
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusLink

/// The fake Core's accessories (``Additions/accessories``): the `amplifier`,
/// `tuner`, `rfkit`, `accessoryData` and `accessorySettings` objects a test
/// delivers with ``deliverAccessories(_:)``, and the Core's switching verbs
/// (remote accessory control document, Commands), each accepted with the
/// delta the device's report brings, as a Core connected to real devices
/// sends it: the buttons follow the report, never the tap. Nothing reaches
/// a device or a radio.
///
/// With remote transmit as well (``Additions/remoteTx``) the fake enforces
/// the transmit interlock as the Core does: with the Power Genius present
/// and in standby and the interlock on Block, `tx.key` is refused with
/// `ampStandby` and the fix `operateAmp` (link document section 18.3).
extension FakeStation {
    /// What the fake's accessories show and do.
    public struct AccessoryScene: Sendable {
        public var ampConnected = true
        public var ampOperate = true
        public var ampForwardW = 0.0
        public var interlockMode: Int64 = 2
        public var tunerConnected = true
        public var tunerOperate = true
        public var tunerBypass = false
        /// The tuner's antenna as the Core reports it: 0-based, ANT 1 is 0.
        public var tunerAntenna: Int64 = 0
        public var tunerRelays: [Int64] = [112, 47, 186]
        public var rfkitConnected = true
        public var rfkitOperate = false
        public var rfkitAntenna: Int64 = 1
        /// Bit N-1 for each internal antenna N the amp lists.
        public var rfkitAntennaPresentMask: Int64 = 0b0111
        public var rfkitAntennaDisabledMask: Int64 = 0
        /// The station's 4O3A and RF-Kit switches.
        public var fourO3AEnabled = true
        public var rfKitEnabled = true
        /// The stored tunes cleared, by their `TGXL_TuneMemory_Ant<N>_Band<band>` settings.
        public var clearedTunes: Set<String> = []
        /// When the Power Genius's recorded faults happened, newest first.
        public var faultTimes: [Date] = [Date().addingTimeInterval(-3 * 86_400), Date().addingTimeInterval(-9 * 86_400)]

        public init() {}

        /// The board's picture 11: the Power Genius operating, the Tuner
        /// Genius operating on ANT 1, the RF2K-S in standby.
        public static var board: AccessoryScene { AccessoryScene() }
    }

    /// What the fake's accessories are doing now, changed by the verbs.
    final class AccessoryState: @unchecked Sendable {
        private let lock = NSLock()
        private var scene: AccessoryScene?
        private var faultRevision: Int64 = 1

        func set(_ scene: AccessoryScene) {
            lock.withLock { self.scene = scene }
        }

        /// Changes the scene if the accessories were delivered; nil otherwise.
        func change<Result>(_ body: (inout AccessoryScene) -> Result) -> Result? {
            lock.withLock {
                guard var current = scene else {
                    return nil
                }
                let result = body(&current)
                scene = current
                return result
            }
        }

        func nextFaultRevision() -> Int64 {
            lock.withLock {
                faultRevision += 1
                return faultRevision
            }
        }
    }

    /// The Core's words for the refusals the fake gives.
    public static let ampStandbyReason = "The amplifier is in standby. Operate it, or change the interlock in Setup."
    public static let ampNotConnectedReason = "The Core is not connected to the Power Genius."
    public static let tunerNotConnectedReason = "The Core is not connected to the Tuner Genius."
    public static let rfkitNotConnectedReason = "The Core is not connected to the RF-Kit amplifier."
    public static let rfkitAntennaReason = "This antenna is not available on the RF-Kit amplifier."
    public static let tunerAntennaReason = "The Tuner Genius has no such antenna."

    /// The accessory versions a Core that owns its accessories advertises
    /// (remote accessory control document, Negotiation).
    static func accessoryCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        guard additions.contains(.accessories) else {
            return []
        }
        return [("remotePgxlControlVersion", 4), ("remoteRfKitControlVersion", 4), ("remoteTgxlControlVersion", 4),
                ("accessoryDataVersion", 3), ("remoteTgxlConfigVersion", 1), ("remoteFourO3AControlVersion", 1)]
    }

    /// The objects as a Core connected to the scene's devices sends them.
    public func deliverAccessories(_ scene: AccessoryScene = .board) async {
        guard additions.contains(.accessories) else {
            return
        }
        accessoryState.set(scene)
        for message in Self.accessoryObjects(scene) {
            await deliver(message)
        }
    }

    /// The fake's answer to an accessory verb, or to a key the interlock
    /// holds back; nil leaves the verb to the rest of the fake.
    func accessoryReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        guard additions.contains(.accessories) else {
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "",
                    values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: values))
        }
        func flag(_ name: String) -> Bool {
            if case .bool(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return false
        }
        func whole(_ name: String) -> Int64 {
            switch invoke.args.first(where: { $0.name == name })?.value {
            case .i64(let value)?, .enumeration(let value)?:
                return value
            default:
                return 0
            }
        }
        if Self.accessoryVerbs.contains(invoke.verb), let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        func text(_ name: String) -> String {
            if case .utf8(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return ""
        }
        if let replies = settingsVerbReplies(invoke, result: { result($0, $1, values: $2) }, flag: flag,
                                             whole: whole, text: text) {
            return replies
        }
        switch invoke.verb {
        case "tx.key":
            guard additions.contains(.remoteTx) else {
                return nil
            }
            let holds = accessoryState.change { scene in
                scene.ampConnected && !scene.ampOperate && scene.interlockMode == 2
            } ?? false
            guard holds else {
                return nil
            }
            return [result(false, Self.ampStandbyReason,
                           values: [.init(name: "refusalCode", value: .utf8("ampStandby")),
                                    .init(name: "refusalFix", value: .utf8("operateAmp"))])]
        case "setPgxlOperate":
            let on = flag("on")
            guard let connected = accessoryState.change({ scene -> Bool in
                if scene.ampConnected {
                    scene.ampOperate = on
                }
                return scene.ampConnected
            }) else {
                return nil
            }
            guard connected else {
                return [result(false, Self.ampNotConnectedReason)]
            }
            return [result(true), Self.accessoryDelta("amplifier", "AmplifierModel", [
                ("state", .enumeration(on ? 3 : 2)), ("deviceState", .utf8(on ? "IDLE" : "STANDBY")),
                ("operate", .bool(on)),
            ])]
        case "setTgxlOperate", "setTgxlBypass":
            let on = flag("on")
            let operateVerb = invoke.verb == "setTgxlOperate"
            guard let state = accessoryState.change({ scene -> [Bool] in
                if scene.tunerConnected {
                    if operateVerb {
                        scene.tunerOperate = on
                        if on {
                            scene.tunerBypass = false
                        }
                    } else {
                        scene.tunerBypass = on
                    }
                }
                return [scene.tunerConnected, scene.tunerOperate, scene.tunerBypass]
            }) else {
                return nil
            }
            guard state[0] else {
                return [result(false, Self.tunerNotConnectedReason)]
            }
            return [result(true), Self.accessoryDelta("tuner", "TunerModel", [
                ("isOperate", .bool(state[1])), ("isBypass", .bool(state[2])),
            ])]
        case "setTgxlAntenna":
            let port = whole("port")
            guard let connected = accessoryState.change({ scene -> Bool in
                if scene.tunerConnected, (1...3).contains(port) {
                    scene.tunerAntenna = port - 1
                }
                return scene.tunerConnected
            }) else {
                return nil
            }
            guard connected else {
                return [result(false, Self.tunerNotConnectedReason)]
            }
            guard (1...3).contains(port) else {
                return [result(false, Self.tunerAntennaReason)]
            }
            // The verb takes 1 to 3; the tuner and the Core report it 0-based.
            return [result(true), Self.accessoryDelta("tuner", "TunerModel", [("antennaA", .i64(port - 1))])]
        case "setRfKitOperate":
            let on = flag("on")
            guard let connected = accessoryState.change({ scene -> Bool in
                if scene.rfkitConnected {
                    scene.rfkitOperate = on
                }
                return scene.rfkitConnected
            }) else {
                return nil
            }
            guard connected else {
                return [result(false, Self.rfkitNotConnectedReason)]
            }
            return [result(true), Self.accessoryDelta("rfkit", "RfKitModel", [("operate", .bool(on))])]
        case "setRfKitAntenna":
            let port = whole("port")
            // 0: not connected, 1: not an antenna the amp offers, 2: switched.
            guard let outcome = accessoryState.change({ scene -> Int in
                guard scene.rfkitConnected else {
                    return 0
                }
                let bit: Int64 = (1...4).contains(port) ? 1 << (port - 1) : 0
                guard bit != 0, scene.rfkitAntennaPresentMask & bit != 0,
                      scene.rfkitAntennaDisabledMask & bit == 0 else {
                    return 1
                }
                scene.rfkitAntenna = port
                return 2
            }) else {
                return nil
            }
            if outcome == 0 {
                return [result(false, Self.rfkitNotConnectedReason)]
            }
            if outcome == 1 {
                return [result(false, Self.rfkitAntennaReason)]
            }
            return [result(true), Self.accessoryDelta("rfkit", "RfKitModel", [
                ("activeAntennaNumber", .i64(port)), ("activeAntennaExternal", .bool(false)),
            ])]
        case "clearAccessoryFaults":
            var device = ""
            if case .utf8(let text)? = invoke.args.first(where: { $0.name == "device" })?.value {
                device = text
            }
            guard ["pgxl", "tgxl", "rfkit"].contains(device), accessoryState.change({ _ in true }) != nil else {
                return nil
            }
            return [result(true), Self.accessoryDelta("accessoryData", "AccessoryDataModel", [
                ("faultRevision", .i64(accessoryState.nextFaultRevision())), ("\(device)Faults", .utf8("[]")),
            ])]
        default:
            return nil
        }
    }

    /// The accessory verbs the fake answers (remote accessory control document, Commands).
    static let accessoryVerbs: Set<String> = [
        "setPgxlOperate", "setTgxlOperate", "setTgxlBypass", "setTgxlAntenna", "setRfKitOperate", "setRfKitAntenna",
        "clearAccessoryFaults", "setPgxlName", "setTgxlName", "setPgxlHardware", "setPgxlNetwork", "setTgxlNetwork",
        "savePgxlSettings", "saveTgxlSettings", "readPgxlSettings", "readTgxlSettings", "setPgxlPowerCap",
        "setPgxlAddress", "setTgxlAddress", "setRfKitAddress", "configurePgxl", "configureTgxl", "configureRfKit",
        "disconnectPgxl", "disconnectTgxl", "disconnectRfKit", "scanPgxlLan", "scanTgxlLan", "setFourO3AEnabled",
        "setRfKitEnabled", "setPgxlConnectionSettings", "moveTgxlRelay", "resetRfKitError", "setRfKitTciMode",
    ]

    /// The device a verb talks to, the object that reports it and its class.
    private static func target(_ verb: String) -> (device: String, key: String, className: String) {
        if verb.contains("Pgxl") {
            return ("pgxl", "amplifier", "AmplifierModel")
        }
        if verb.contains("Tgxl") {
            return ("tgxl", "tuner", "TunerModel")
        }
        return ("rfkit", "rfkit", "RfKitModel")
    }

    /// The Core's answers to the settings, connection and maintenance verbs,
    /// with the report each brings; nil for any other verb.
    private func settingsVerbReplies(_ invoke: LinkMessage.CommandInvoke,
                                     result: (Bool, String, [LinkMessage.PropertyEntry]?) -> LinkMessage,
                                     flag: (String) -> Bool, whole: (String) -> Int64,
                                     text: (String) -> String) -> [LinkMessage]? {
        let verb = invoke.verb
        let target = Self.target(verb)
        guard let scene = accessoryState.change({ $0 }) else {
            return nil
        }
        let connected: Bool
        switch target.device {
        case "pgxl":
            connected = scene.ampConnected
        case "tgxl":
            connected = scene.tunerConnected
        default:
            connected = scene.rfkitConnected
        }
        let notConnected: String
        switch target.device {
        case "pgxl":
            notConnected = Self.ampNotConnectedReason
        case "tgxl":
            notConnected = Self.tunerNotConnectedReason
        default:
            notConnected = Self.rfkitNotConnectedReason
        }
        let settingsClass = "AccessorySettingsModel"
        switch verb {
        case "setPgxlName", "setTgxlName":
            guard connected else {
                return [result(false, notConnected, nil)]
            }
            return [result(true, "", nil),
                    Self.accessoryDelta("accessorySettings", settingsClass,
                                        [("\(target.device)Nickname", .utf8(text("name")))])]
        case "setPgxlHardware":
            guard connected else {
                return [result(false, notConnected, nil)]
            }
            guard let argument = invoke.args.first else {
                return [result(false, "The request to change the Power Genius hardware was not understood.", nil)]
            }
            let names = ["biasMode": "pgxlBiasMode", "fanMode": "pgxlFanMode", "ledIntensity": "pgxlLedIntensity"]
            guard let property = names[argument.name] else {
                return [result(false, "The request to change the Power Genius hardware was not understood.", nil)]
            }
            return [result(true, "", nil),
                    Self.accessoryDelta("accessorySettings", settingsClass, [(property, argument.value)])]
        case "setPgxlNetwork", "setTgxlNetwork":
            guard connected else {
                return [result(false, notConnected, nil)]
            }
            let prefix = target.device
            return [result(true, "", nil), Self.accessoryDelta("accessorySettings", settingsClass, [
                ("\(prefix)NetworkKnown", .bool(true)), ("\(prefix)Dhcp", .bool(flag("dhcp"))),
                ("\(prefix)Address", .utf8(text("address"))), ("\(prefix)Netmask", .utf8(text("netmask"))),
                ("\(prefix)Gateway", .utf8(text("gateway"))),
            ])]
        case "savePgxlSettings", "saveTgxlSettings", "readPgxlSettings", "readTgxlSettings":
            return [result(connected, connected ? "" : notConnected, nil)]
        case "setPgxlPowerCap":
            let watts = whole("watts")
            guard (100...2000).contains(watts) else {
                return [result(false, "The request to change the Power Genius output limit was not understood.", nil)]
            }
            return [result(true, "", nil), Self.accessoryDelta("accessoryData", "AccessoryDataModel", [
                ("powerCapEnabled", .bool(flag("enabled"))), ("powerCapW", .i64(watts)),
            ])]
        case "setPgxlAddress", "setTgxlAddress", "setRfKitAddress", "configurePgxl", "configureTgxl",
             "configureRfKit":
            let port = whole("port")
            guard (1...65535).contains(port) else {
                let names = ["pgxl": "the Power Genius's", "tgxl": "the Tuner Genius XL's",
                             "rfkit": "the RF-Kit amplifier's"]
                return [result(false, "Enter \(names[target.device] ?? "its") IP address or host name, and a port "
                               + "from 1 to 65535.", nil)]
            }
            var values: [(String, LinkMessage.PropertyValue)] = [("configuredHost", .utf8(text("host"))),
                                                                  ("configuredPort", .i64(port))]
            if verb.hasPrefix("configure") {
                values.append(("connectionPhase", .enumeration(3)))
            }
            return [result(true, "", nil), Self.accessoryDelta(target.key, target.className, values)]
        case "disconnectPgxl", "disconnectTgxl", "disconnectRfKit":
            _ = accessoryState.change { scene in
                switch target.device {
                case "pgxl":
                    scene.ampConnected = false
                case "tgxl":
                    scene.tunerConnected = false
                default:
                    scene.rfkitConnected = false
                }
            }
            let present = target.device == "tgxl" ? "isPresent" : "present"
            return [result(true, "", nil), Self.accessoryDelta(target.key, target.className, [
                ("connectionPhase", .enumeration(1)), (present, .bool(false)),
            ])]
        case "scanPgxlLan", "scanTgxlLan":
            let heard: [[String: Any]] = target.device == "pgxl"
                ? [["address": "192.168.109.235", "port": 9008, "model": "PowerGeniusXL",
                    "serial": "10-200/24-0046", "nickname": "PowerGeniusXL"]]
                : [["address": "192.168.109.236", "port": 9010, "model": "TunerGeniusXL",
                    "serial": "10-300/24-0112", "nickname": "TunerGeniusXL"]]
            return [result(true, "", [.init(name: "devicesJson", value: .utf8(Self.json(heard)))])]
        case "setFourO3AEnabled":
            let on = flag("enabled")
            _ = accessoryState.change { $0.fourO3AEnabled = on }
            return [result(true, "", nil), Self.accessoryDelta("radio", "RadioModel", [("fourO3AEnabled", .bool(on))])]
        case "setRfKitEnabled":
            let on = flag("enabled")
            _ = accessoryState.change { $0.rfKitEnabled = on }
            return [result(true, "", nil), Self.accessoryDelta("radio", "RadioModel", [("rfKitEnabled", .bool(on))])]
        case "setPgxlConnectionSettings":
            let keepalive = whole("keepaliveSec")
            let ping = whole("pingSec")
            guard (1...3600).contains(keepalive), (0...3600).contains(ping) else {
                return [result(false, "The Core could not read this request.", nil)]
            }
            return [result(true, "", nil),
                    Self.settingEcho(Self.pgxlAutoReconnectSetting, flag("autoReconnect") ? "True" : "False"),
                    Self.settingEcho(Self.pgxlKeepaliveSetting, String(keepalive)),
                    Self.settingEcho(Self.pgxlPingSetting, String(ping))]
        case "moveTgxlRelay":
            let relay = whole("relay")
            let direction = whole("direction")
            guard (0...2).contains(relay), direction == 1 || direction == -1 else {
                return [result(false, "The request to move a Tuner Genius relay was not understood.", nil)]
            }
            guard connected else {
                return [result(false, notConnected, nil)]
            }
            let value = accessoryState.change { scene -> Int64 in
                var relays = scene.tunerRelays + Array(repeating: 0, count: max(0, 3 - scene.tunerRelays.count))
                relays[Int(relay)] = min(max(relays[Int(relay)] + direction, 0), 255)
                scene.tunerRelays = relays
                return relays[Int(relay)]
            } ?? 0
            let names = ["relayC1", "relayL", "relayC2"]
            return [result(true, "", nil), Self.accessoryDelta("tuner", "TunerModel", [(names[Int(relay)], .i64(value))])]
        case "resetRfKitError":
            return [result(connected, connected ? "" : notConnected, nil)]
        case "setRfKitTciMode":
            guard connected else {
                return [result(false, notConnected, nil)]
            }
            return [result(true, "", nil),
                    Self.accessoryDelta("rfkit", "RfKitModel", [("operationalInterface", .utf8("TCI"))])]
        default:
            return nil
        }
    }

    // MARK: Station settings

    static let pgxlAutoReconnectSetting = "PGXL_AutoReconnect"
    static let pgxlKeepaliveSetting = "PGXL_KeepaliveSec"
    static let pgxlPingSetting = "PGXL_PingSec"

    /// A setting's value as the Core sends it; `origin` is the writer's, or empty.
    static func settingEcho(_ key: String, _ value: String, origin: String = "") -> LinkMessage {
        .settingsValue(LinkMessage.SettingsValue(key: key, origin: origin,
                                                 properties: [.init(name: key, value: .utf8(value))]))
    }

    /// The Core's answer to a station setting the accessory pages write: the
    /// value echoed to its writer, and for an antenna name or the tune
    /// memory recall, the `accessoryData` change the Core publishes. A
    /// refusal queued with ``refuseNext(_:reason:)`` under the key is sent
    /// as the Core's rejection. Nil for any other key.
    func accessorySettingReplies(_ write: LinkMessage.SettingsWrite) -> [LinkMessage]? {
        guard additions.contains(.accessories), write.key.hasPrefix("TGXL_") || write.key.hasPrefix("RfKit_")
            || write.key.hasPrefix("PGXL_") else {
            return nil
        }
        var value = ""
        if case .utf8(let text)? = write.properties.first?.value {
            value = text
        }
        if let reason = takeRefusal(write.key) {
            return [.settingsReject(LinkMessage.SettingsReject(key: write.key, properties: [], reason: reason))]
        }
        var replies = [Self.settingEcho(write.key, value, origin: write.origin)]
        if write.key.hasSuffix("_Label"), let port = Int(write.key.filter(\.isNumber)) {
            let prefix = write.key.hasPrefix("TGXL_") ? "tgxl" : "rfkit"
            replies.append(Self.accessoryDelta("accessoryData", "AccessoryDataModel",
                                               [("\(prefix)Antenna\(port)Label", .utf8(value))]))
        } else if write.key == "TGXL_AutoTuneMemoryRecall" {
            replies.append(Self.accessoryDelta("accessoryData", "AccessoryDataModel",
                                               [("autoTuneMemoryRecall", .bool(value == "True"))]))
        } else if write.key.hasPrefix("TGXL_TuneMemory_"), value.isEmpty,
                  let cleared = accessoryState.change({ scene -> Set<String> in
                      scene.clearedTunes.insert(write.key)
                      return scene.clearedTunes
                  }) {
            // An emptied stored tune leaves the Core's tune memory.
            replies.append(Self.accessoryDelta("accessoryData", "AccessoryDataModel",
                                               [("tuneMemory", .utf8(Self.tuneMemory(without: cleared)))]))
        }
        return replies
    }

    // MARK: The objects

    /// Each accessory class's property ordinals, from the suite's surface.
    private static let accessoryOrdinals: [String: [String: UInt16]] = {
        guard let classes = try? SessionFixtures.mirrorClasses() else {
            return [:]
        }
        return classes.mapValues { properties in
            var ordinals: [String: UInt16] = [:]
            for property in properties {
                if let name = property["name"] as? String, let ordinal = property["ordinal"] as? Int {
                    ordinals[name] = UInt16(ordinal)
                }
            }
            return ordinals
        }
    }()

    private static func entries(_ className: String,
                                _ values: [(String, LinkMessage.PropertyValue)]) -> [LinkMessage.PropertyEntry] {
        let ordinals = accessoryOrdinals[className] ?? [:]
        return values.map { .init(ordinal: ordinals[$0.0] ?? 0, name: $0.0, value: $0.1) }
    }

    /// A change to one of the accessory objects, as the Core sends it.
    public static func accessoryDelta(_ key: String, _ className: String,
                                      _ values: [(String, LinkMessage.PropertyValue)]) -> LinkMessage {
        .delta(LinkMessage.Delta(key: key, properties: entries(className, values)))
    }

    private static func create(_ key: String, _ className: String,
                               _ values: [(String, LinkMessage.PropertyValue)]) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: key, className: className, properties: entries(className, values)))
    }

    private static func connectedSinceMs(_ secondsAgo: Double) -> LinkMessage.PropertyValue {
        .i64(Int64(Date().addingTimeInterval(-secondsAgo).timeIntervalSince1970 * 1000))
    }

    private static func accessoryObjects(_ scene: AccessoryScene) -> [LinkMessage] {
        let phase: (Bool) -> LinkMessage.PropertyValue = { .enumeration($0 ? 6 : 1) }
        let relays = scene.tunerRelays + Array(repeating: 0, count: max(0, 3 - scene.tunerRelays.count))
        let amp = create("amplifier", "AmplifierModel", [
            ("connectionPhase", phase(scene.ampConnected)), ("configuredHost", .utf8("192.168.109.235")),
            ("configuredPort", .i64(9008)), ("connectionError", .utf8("")),
            ("deviceModel", .utf8("PowerGeniusXL")), ("deviceSerial", .utf8("10-200/24-0046")),
            ("deviceVersion", .utf8("3.8.9")), ("deviceNickname", .utf8("PowerGeniusXL")),
            ("present", .bool(scene.ampConnected)),
            ("state", .enumeration(scene.ampOperate ? 3 : 2)),
            ("deviceState", .utf8(scene.ampOperate ? "IDLE" : "STANDBY")), ("operate", .bool(scene.ampOperate)),
            ("transmitting", .bool(false)), ("forwardPowerW", .f64(scene.ampForwardW)), ("swr", .f64(1.0)),
            ("temperatureC", .f64(31)), ("mainsVoltageV", .f64(240)), ("drainCurrentA", .f64(0)),
            ("efficiencyText", .utf8("0.0")), ("bandFollow", .enumeration(scene.ampConnected ? 2 : 0)),
        ])
        let tuner = create("tuner", "TunerModel", [
            ("connectionPhase", phase(scene.tunerConnected)), ("configuredHost", .utf8("192.168.109.236")),
            ("configuredPort", .i64(9010)), ("connectionError", .utf8("")),
            ("deviceModel", .utf8("TunerGeniusXL")), ("deviceSerial", .utf8("10-300/24-0112")),
            ("deviceVersion", .utf8("1.2.13")), ("deviceNickname", .utf8("TunerGeniusXL")),
            ("relayC1", .i64(relays[0])), ("relayL", .i64(relays[1])), ("relayC2", .i64(relays[2])),
            ("isOperate", .bool(scene.tunerOperate)), ("isBypass", .bool(scene.tunerBypass)),
            ("isTuning", .bool(false)), ("antennaA", .i64(scene.tunerAntenna)), ("hasAntennaSwitch", .bool(true)),
            ("isPresent", .bool(scene.tunerConnected)), ("hasDirectConnection", .bool(scene.tunerConnected)),
            ("tgxlIp", .utf8("192.168.109.236")), ("fwdPower", .f64(0)), ("swr", .f64(1.0)),
        ])
        let rfkit = create("rfkit", "RfKitModel", [
            ("connectionPhase", phase(scene.rfkitConnected)), ("configuredHost", .utf8("192.168.109.240")),
            ("configuredPort", .i64(8080)), ("connectionError", .utf8("")),
            ("deviceModel", .utf8("RF2K-S")), ("deviceSerial", .utf8("")), ("deviceVersion", .utf8("G200C267")),
            ("deviceNickname", .utf8("KG4VCF")), ("present", .bool(scene.rfkitConnected)),
            ("operate", .bool(scene.rfkitOperate)), ("forwardPowerW", .f64(0)), ("reflectedPowerW", .f64(0)),
            ("swr", .f64(1.0)), ("temperatureC", .f64(31)), ("voltageV", .f64(53.2)), ("currentA", .f64(0)),
            ("operationalInterface", .utf8("TCI")), ("antennaPresentMask", .i64(scene.rfkitAntennaPresentMask)),
            ("antennaDisabledMask", .i64(scene.rfkitAntennaDisabledMask)),
            ("activeAntennaNumber", .i64(scene.rfkitAntenna)), ("activeAntennaExternal", .bool(false)),
            ("tunerMode", .enumeration(1)), ("tunerSetup", .utf8("LC")), ("tunerInductanceNh", .i64(0)),
            ("tunerCapacitancePf", .i64(0)), ("tunerFrequencyKhz", .i64(0)), ("tunerSegmentKhz", .i64(0)),
            ("bandFollow", .enumeration(scene.rfkitConnected ? 2 : 0)),
            ("bandFollowAddress", .utf8("192.168.109.10")), ("bandFollowPort", .i64(50001)),
        ])
        let data = create("accessoryData", "AccessoryDataModel", [
            ("faultRevision", .i64(1)), ("pgxlFaults", .utf8(faults(scene.faultTimes))),
            ("tgxlFaults", .utf8("[]")), ("rfkitFaults", .utf8("[]")),
            ("pgxlConnectedSinceMs", connectedSinceMs(7200)), ("pgxlLastRttMs", .i64(4)),
            ("pgxlKeepaliveMissed", .i64(0)), ("pgxlReconnectCount", .i64(1)),
            ("pgxlFramesIn", .i64(14_402)), ("pgxlFramesOut", .i64(1_203)), ("pgxlBytesIn", .i64(912_004)),
            ("pgxlBytesOut", .i64(40_210)), ("pgxlLastFrameMs", connectedSinceMs(1)), ("pgxlFaultsSession", .i64(0)),
            ("tgxlConnectedSinceMs", connectedSinceMs(7200)), ("tgxlLastRttMs", .i64(3)),
            ("tgxlKeepaliveMissed", .i64(0)), ("tgxlReconnectCount", .i64(0)),
            ("tgxlFramesIn", .i64(8_120)), ("tgxlFramesOut", .i64(944)), ("tgxlBytesIn", .i64(301_550)),
            ("tgxlBytesOut", .i64(21_004)), ("tgxlLastFrameMs", connectedSinceMs(1)), ("tgxlFaultsSession", .i64(0)),
            ("interlockMode", .enumeration(scene.interlockMode)), ("interlockGraceMs", .i64(3000)),
            ("interlockSwrGateEnabled", .bool(true)), ("interlockSwrGateMax", .f64(3.0)),
            ("powerCapEnabled", .bool(false)), ("powerCapW", .i64(1500)), ("powerCapExceeded", .bool(false)),
            ("powerCapAlertText", .utf8("")), ("powerCapAlertCount", .i64(0)),
            ("tuneMemory", .utf8(tuneMemory(without: scene.clearedTunes))), ("autoTuneMemoryRecall", .bool(true)),
            ("tgxlAntenna1Label", .utf8("Beam")), ("tgxlAntenna2Label", .utf8("Vertical")),
            ("tgxlAntenna3Label", .utf8("Dipole")),
            ("rfkitAntenna1Label", .utf8("")), ("rfkitAntenna2Label", .utf8("")),
            ("rfkitAntenna3Label", .utf8("")), ("rfkitAntenna4Label", .utf8("")),
            ("rfkitConnectedSinceMs", connectedSinceMs(3600)), ("rfkitPollsOk", .i64(3_600)),
            ("rfkitPollsFailed", .i64(2)), ("rfkitReconnectCount", .i64(0)),
            ("rfkitLastPollMs", connectedSinceMs(0)), ("rfkitRttAvgMs", .i64(12)),
        ])
        let settings = create("accessorySettings", "AccessorySettingsModel", [
            ("pgxlNickname", .utf8("PowerGeniusXL")), ("pgxlBiasMode", .utf8("ClassAB")),
            ("pgxlFanMode", .utf8("Auto")), ("pgxlLedIntensity", .i64(80)), ("pgxlNetworkKnown", .bool(true)),
            ("pgxlDhcp", .bool(true)), ("pgxlAddress", .utf8("192.168.109.235")),
            ("pgxlNetmask", .utf8("255.255.255.0")), ("pgxlGateway", .utf8("192.168.109.1")),
            ("pgxlAnswer", .utf8("")), ("pgxlAnswerAccepted", .bool(true)), ("pgxlAnswerCount", .i64(0)),
            ("tgxlNickname", .utf8("TunerGeniusXL")), ("tgxlNetworkKnown", .bool(true)), ("tgxlDhcp", .bool(true)),
            ("tgxlAddress", .utf8("192.168.109.236")), ("tgxlNetmask", .utf8("255.255.255.0")),
            ("tgxlGateway", .utf8("192.168.109.1")), ("tgxlAnswer", .utf8("")),
            ("tgxlAnswerAccepted", .bool(true)), ("tgxlAnswerCount", .i64(0)),
        ])
        let radio = accessoryDelta("radio", "RadioModel", [("fourO3AEnabled", .bool(scene.fourO3AEnabled)),
                                                           ("rfKitEnabled", .bool(scene.rfKitEnabled))])
        let stationSettings = [settingEcho(pgxlAutoReconnectSetting, "True"), settingEcho(pgxlKeepaliveSetting, "30"),
                               settingEcho(pgxlPingSetting, "10"), settingEcho("RfKit_AutoReconnect", "True"),
                               settingEcho("RfKit_PollIntervalMs", "1000")]
        return [amp, tuner, rfkit, data, settings, radio] + stationSettings
    }

    /// The Power Genius's faults, newest first, as the Core records them.
    private static func faults(_ times: [Date]) -> String {
        let records: [[String: Any]] = times.enumerated().map { index, time in
            let swr = index == 0
            return ["whenMs": Int64(time.timeIntervalSince1970 * 1000), "device": "pgxl", "state": "FAULT",
                    "text": swr ? "The Power Genius reported a fault. Likely cause: high SWR."
                        : "The Power Genius reported a fault. Likely cause: the amplifier was too hot.",
                    "detail": "", "fwdAtFaultW": swr ? 1820.0 : 1400.0, "swrAtFault": swr ? 2.85 : 1.2,
                    "tempAtFaultC": swr ? 58.0 : 81.0, "likelyCause": swr ? "SWR trip" : "Overtemp"]
        }
        return json(records)
    }

    /// Twelve stored tunes, sorted by band then antenna; ANT 1 on 40m holds
    /// the relays the board's Tuner Genius shows.
    private static func tuneMemory(without cleared: Set<String> = []) -> String {
        let bands = ["160m", "80m", "40m", "30m", "20m", "15m"]
        var entries: [[String: Any]] = []
        for (index, band) in bands.enumerated() {
            for antenna in 1...2 {
                let fortyOnOne = band == "40m" && antenna == 1
                guard !cleared.contains("TGXL_TuneMemory_Ant\(antenna)_Band\(band)") else {
                    continue
                }
                entries.append(["antenna": antenna, "band": band,
                                "c1": fortyOnOne ? 112 : 40 + index * 20 + antenna,
                                "l": fortyOnOne ? 47 : 20 + index * 9 + antenna,
                                "c2": fortyOnOne ? 186 : 60 + index * 17 + antenna,
                                "savedAtMs": Int64(1_790_000_000_000) + Int64(index * 2 + antenna)])
            }
        }
        return json(entries)
    }

    private static func json(_ object: [[String: Any]]) -> String {
        guard let data = try? JSONSerialization.data(withJSONObject: object, options: [.sortedKeys]) else {
            return "[]"
        }
        return String(decoding: data, as: UTF8.self)
    }
}

extension FakeStation.Additions {
    /// The Core's own accessories (``FakeStation/deliverAccessories(_:)``):
    /// `remotePgxlControlVersion`, `remoteRfKitControlVersion` and
    /// `remoteTgxlControlVersion` 4, `accessoryDataVersion` 3,
    /// `remoteTgxlConfigVersion` and `remoteFourO3AControlVersion` 1, and their
    /// switching verbs answered as the devices report them. A high bit, so
    /// another lane's addition does not collide with it. Not in ``all``.
    public static let accessories = FakeStation.Additions(rawValue: 1 << 16)
}
