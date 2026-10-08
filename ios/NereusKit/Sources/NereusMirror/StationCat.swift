// NereusSDR for iOS: the Core's CAT setup, read from its stationCat object and catLog record, and written back
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The CAT the Core runs for its station (`stationCatVersion` 1): the
/// `stationCat` object (class `StationCatModel`), sent only to a device that
/// declared `stationCat` 1 in its hello. Seven text properties, each one
/// JSON object: `global`, `channel1` to `channel4`, `platform` and
/// `lastTest`. The phone changes a channel with `setStationCatChannel` and
/// the global settings with `setStationCatGlobal`, each carrying the whole
/// config as JSON; a field left out keeps the Core's value. The Core's CAT
/// traffic travels on the `catLog` record stream.
///
/// Reading is tolerant: a property or field missing or of another kind reads
/// as empty, never as a failure. A config keeps every field the Core sent,
/// known or not, and writes them all back with the change.
public struct StationCat: Equatable, Sendable {
    /// The feature a device declares in its hello, and the capability the Core answers with.
    public static let featureName = "stationCat"
    public static let capabilityName = "stationCatVersion"
    /// The agreed minor the CAT setup arrived in.
    public static let minor: UInt16 = 11
    public static let objectKey = "stationCat"
    public static let className = "StationCatModel"
    /// The channels, as the desktop names them CAT 1 to CAT 4.
    public static let channels: ClosedRange<Int> = 1...4
    /// The Core's CAT traffic, and the most lines it keeps.
    public static let logStream = "catLog"
    public static let logCapacity = 10000
    /// `{channel: i64, config: utf8}`.
    public static let setChannelVerb = "setStationCatChannel"
    /// `{config: utf8}`.
    public static let setGlobalVerb = "setStationCatGlobal"
    /// `{requestId: i64, channel: i64, command: utf8}`; an accepted result carries `reply`.
    public static let testVerb = "testStationCatCommand"
    /// No arguments: the Core reads its serial devices again.
    public static let refreshDevicesVerb = "refreshStationCatDevices"
    public static let globalProperty = "global"
    public static let platformProperty = "platform"
    public static let lastTestProperty = "lastTest"

    public static func channelProperty(_ number: Int) -> String {
        "channel\(number)"
    }

    public var global: Global
    /// CAT 1 to CAT 4, in order.
    public var channels: [Channel]
    public var platform: Platform
    public var lastTest: LastTest

    public init(global: Global = Global(text: nil), channels: [Channel]? = nil, platform: Platform = Platform(text: nil),
                lastTest: LastTest = LastTest(text: nil)) {
        self.global = global
        self.channels = channels ?? Self.channels.map { Channel(number: $0, text: nil) }
        self.platform = platform
        self.lastTest = lastTest
    }

    /// Reads the object's properties by name.
    public init(values: [String: MirrorValue]) {
        func text(_ name: String) -> String? {
            if case .text(let value)? = values[name] {
                return value
            }
            return nil
        }
        self.init(global: Global(text: text(Self.globalProperty)),
                  channels: Self.channels.map { Channel(number: $0, text: text(Self.channelProperty($0))) },
                  platform: Platform(text: text(Self.platformProperty)),
                  lastTest: LastTest(text: text(Self.lastTestProperty)))
    }

    /// CAT `number`, 1 to 4; an empty channel outside that.
    public func channel(_ number: Int) -> Channel {
        channels.first { $0.number == number } ?? Channel(number: number, text: nil)
    }

    // MARK: Reading helpers

    /// The JSON object a property's text holds; nil when it holds none.
    static func object(_ text: String?) -> [String: LinkJSON]? {
        guard let text, !text.isEmpty, case .object(let fields)? = try? LinkJSON.parse(text) else {
            return nil
        }
        return fields
    }

    static func text(_ fields: [String: LinkJSON], _ name: String) -> String {
        if case .string(let value)? = fields[name] {
            return value
        }
        return ""
    }

    static func int(_ fields: [String: LinkJSON], _ name: String, or fallback: Int64 = 0) -> Int64 {
        if case .number(let value)? = fields[name], value.isFinite, value == value.rounded(), abs(value) < 9.0e18 {
            return Int64(value)
        }
        return fallback
    }

    static func bool(_ fields: [String: LinkJSON], _ name: String, or fallback: Bool = false) -> Bool {
        if case .bool(let value)? = fields[name] {
            return value
        }
        return fallback
    }

    static func members(_ fields: [String: LinkJSON], _ name: String) -> [String: LinkJSON] {
        if case .object(let value)? = fields[name] {
            return value
        }
        return [:]
    }

    // MARK: Channels

    /// One channel's settings, as `channelN.config` carries them and
    /// `setStationCatChannel` takes them back.
    public struct ChannelConfig: Equatable, Sendable {
        /// Every field as the Core sent it, with the phone's changes over them.
        public private(set) var fields: [String: LinkJSON]

        public init(fields: [String: LinkJSON] = [:]) {
            self.fields = fields
        }

        private mutating func set(_ name: String, _ value: LinkJSON) {
            fields[name] = value
        }

        public var channel: Int64 {
            get { StationCat.int(fields, "channel") }
            set { set("channel", .number(Double(newValue))) }
        }
        /// The slice VFO A follows; -1 for none.
        public var primarySliceId: Int64 {
            get { StationCat.int(fields, "primarySliceId", or: -1) }
            set { set("primarySliceId", .number(Double(newValue))) }
        }
        /// The slice VFO B follows; -1 for none.
        public var secondarySliceId: Int64 {
            get { StationCat.int(fields, "secondarySliceId", or: -1) }
            set { set("secondarySliceId", .number(Double(newValue))) }
        }
        public var tcpEnabled: Bool {
            get { StationCat.bool(fields, "tcpEnabled") }
            set { set("tcpEnabled", .bool(newValue)) }
        }
        public var serialEnabled: Bool {
            get { StationCat.bool(fields, "serialEnabled") }
            set { set("serialEnabled", .bool(newValue)) }
        }
        public var ptyEnabled: Bool {
            get { StationCat.bool(fields, "ptyEnabled") }
            set { set("ptyEnabled", .bool(newValue)) }
        }
        public var rigctldEnabled: Bool {
            get { StationCat.bool(fields, "rigctldEnabled") }
            set { set("rigctldEnabled", .bool(newValue)) }
        }
        public var tcpBindAddress: String {
            get { StationCat.text(fields, "tcpBindAddress") }
            set { set("tcpBindAddress", .string(newValue)) }
        }
        public var rigctldBindAddress: String {
            get { StationCat.text(fields, "rigctldBindAddress") }
            set { set("rigctldBindAddress", .string(newValue)) }
        }
        public var tcpPort: Int64 {
            get { StationCat.int(fields, "tcpPort") }
            set { set("tcpPort", .number(Double(newValue))) }
        }
        public var rigctldPort: Int64 {
            get { StationCat.int(fields, "rigctldPort") }
            set { set("rigctldPort", .number(Double(newValue))) }
        }
        public var serialDevice: String {
            get { StationCat.text(fields, "serialDevice") }
            set { set("serialDevice", .string(newValue)) }
        }
        public var serialBaud: Int64 {
            get { StationCat.int(fields, "serialBaud") }
            set { set("serialBaud", .number(Double(newValue))) }
        }
        public var serialParity: String {
            get { StationCat.text(fields, "serialParity") }
            set { set("serialParity", .string(newValue)) }
        }
        public var serialDataBits: Int64 {
            get { StationCat.int(fields, "serialDataBits") }
            set { set("serialDataBits", .number(Double(newValue))) }
        }
        /// "1", "1.5" or "2".
        public var serialStopBits: String {
            get { StationCat.text(fields, "serialStopBits") }
            set { set("serialStopBits", .string(newValue)) }
        }
        /// One of the platform's `ptyDialects` values.
        public var ptyDialect: String {
            get { StationCat.text(fields, "ptyDialect") }
            set { set("ptyDialect", .string(newValue)) }
        }

        /// The `config` argument of `setStationCatChannel`: every field, and
        /// whether VFO A's and VFO B's slice was picked again, which binds
        /// the channel to that slice even when its id is unchanged.
        public func commandText(primaryRebind: Bool, secondaryRebind: Bool) -> String {
            var sent = fields
            sent["primaryRebind"] = .bool(primaryRebind)
            sent["secondaryRebind"] = .bool(secondaryRebind)
            return LinkJSON.object(sent).compactText
        }
    }

    /// What one channel is doing, as the Core's CAT applet shows it.
    public struct ChannelStatus: Equatable, Sendable {
        /// The channel's own state ("Listening", "Stopped" and the rest), as the Core words it.
        public var state: String
        public var tcp: String
        public var serial: String
        public var pty: String
        public var rigctld: String
        public var tcpBoundAddress: String
        public var tcpBoundPort: Int64
        public var rigctldBoundAddress: String
        public var rigctldBoundPort: Int64
        public var tcpClients: Int64
        public var rigctldClients: Int64
        /// The virtual serial port's path while it is open; empty otherwise.
        public var ptyPath: String

        public init(fields: [String: LinkJSON]) {
            state = StationCat.text(fields, "state")
            tcp = StationCat.text(fields, "tcp")
            serial = StationCat.text(fields, "serial")
            pty = StationCat.text(fields, "pty")
            rigctld = StationCat.text(fields, "rigctld")
            tcpBoundAddress = StationCat.text(fields, "tcpBoundAddress")
            tcpBoundPort = StationCat.int(fields, "tcpBoundPort")
            rigctldBoundAddress = StationCat.text(fields, "rigctldBoundAddress")
            rigctldBoundPort = StationCat.int(fields, "rigctldBoundPort")
            tcpClients = StationCat.int(fields, "tcpClients")
            rigctldClients = StationCat.int(fields, "rigctldClients")
            ptyPath = StationCat.text(fields, "ptyPath")
        }
    }

    /// One of CAT 1 to CAT 4.
    public struct Channel: Equatable, Sendable, Identifiable {
        public var number: Int
        /// The Core sent this channel.
        public var received: Bool
        public var config: ChannelConfig
        /// The slice VFO A was bound to is still open (no slice counts as open).
        public var primaryValid: Bool
        public var secondaryValid: Bool
        public var status: ChannelStatus

        public var id: Int { number }

        public init(number: Int, text: String?) {
            self.number = number
            let fields = StationCat.object(text)
            received = fields != nil
            let object = fields ?? [:]
            config = ChannelConfig(fields: StationCat.members(object, "config"))
            primaryValid = StationCat.bool(object, "primaryValid", or: true)
            secondaryValid = StationCat.bool(object, "secondaryValid", or: true)
            status = ChannelStatus(fields: StationCat.members(object, "status"))
        }
    }

    // MARK: Global

    /// The settings every channel shares: the CAT Options and CAT PTT pages.
    public struct GlobalConfig: Equatable, Sendable {
        public private(set) var fields: [String: LinkJSON]

        public init(fields: [String: LinkJSON] = [:]) {
            self.fields = fields
        }

        private func flag(_ name: String) -> Bool {
            StationCat.bool(fields, name)
        }

        private mutating func setFlag(_ name: String, _ value: Bool) {
            fields[name] = .bool(value)
        }

        public var sendWelcome: Bool {
            get { flag("sendWelcome") }
            set { setFlag("sendWelcome", newValue) }
        }
        public var rigIdentity: String {
            get { StationCat.text(fields, "rigIdentity") }
            set { fields["rigIdentity"] = .string(newValue) }
        }
        public var allowKenwoodAi: Bool {
            get { flag("allowKenwoodAi") }
            set { setFlag("allowKenwoodAi", newValue) }
        }
        public var aiEnabled: Bool {
            get { flag("aiEnabled") }
            set { setFlag("aiEnabled", newValue) }
        }
        public var aiSerial1: Bool {
            get { flag("aiSerial1") }
            set { setFlag("aiSerial1", newValue) }
        }
        public var aiSerial2: Bool {
            get { flag("aiSerial2") }
            set { setFlag("aiSerial2", newValue) }
        }
        public var aiSerial3: Bool {
            get { flag("aiSerial3") }
            set { setFlag("aiSerial3", newValue) }
        }
        public var aiSerial4: Bool {
            get { flag("aiSerial4") }
            set { setFlag("aiSerial4", newValue) }
        }
        /// Automatic frequency information goes to serial CAT `channel` (1 to 4).
        public func aiSerial(_ channel: Int) -> Bool {
            flag("aiSerial\(channel)")
        }

        public mutating func setAiSerial(_ channel: Int, _ value: Bool) {
            setFlag("aiSerial\(channel)", value)
        }

        public var aiTcp: Bool {
            get { flag("aiTcp") }
            set { setFlag("aiTcp", newValue) }
        }
        public var digitalReportsSideband: Bool {
            get { flag("digitalReportsSideband") }
            set { setFlag("digitalReportsSideband", newValue) }
        }
        public var recenterVfo: Bool {
            get { flag("recenterVfo") }
            set { setFlag("recenterVfo", newValue) }
        }
        public var serialNumber: String {
            get { StationCat.text(fields, "serialNumber") }
            set { fields["serialNumber"] = .string(newValue) }
        }
        public var limitReportedPower: Bool {
            get { flag("limitReportedPower") }
            set { setFlag("limitReportedPower", newValue) }
        }
        public var rttyOffsetAEnabled: Bool {
            get { flag("rttyOffsetAEnabled") }
            set { setFlag("rttyOffsetAEnabled", newValue) }
        }
        public var rttyOffsetBEnabled: Bool {
            get { flag("rttyOffsetBEnabled") }
            set { setFlag("rttyOffsetBEnabled", newValue) }
        }
        public var rttyDiguHz: Int64 {
            get { StationCat.int(fields, "rttyDiguHz") }
            set { fields["rttyDiguHz"] = .number(Double(newValue)) }
        }
        public var rttyDiglHz: Int64 {
            get { StationCat.int(fields, "rttyDiglHz") }
            set { fields["rttyDiglHz"] = .number(Double(newValue)) }
        }
        public var pttEnabled: Bool {
            get { flag("pttEnabled") }
            set { setFlag("pttEnabled", newValue) }
        }
        /// "None", "CAT1" to "CAT4" or "Physical".
        public var pttDeviceSource: String {
            get { StationCat.text(fields, "pttDeviceSource") }
            set { fields["pttDeviceSource"] = .string(newValue) }
        }
        public var pttSerialDevice: String {
            get { StationCat.text(fields, "pttSerialDevice") }
            set { fields["pttSerialDevice"] = .string(newValue) }
        }
        public var pttUseCts: Bool {
            get { flag("pttUseCts") }
            set { setFlag("pttUseCts", newValue) }
        }
        public var pttUseDsr: Bool {
            get { flag("pttUseDsr") }
            set { setFlag("pttUseDsr", newValue) }
        }
        public var pttChannel: Int64 {
            get { StationCat.int(fields, "pttChannel") }
            set { fields["pttChannel"] = .number(Double(newValue)) }
        }
        public var pttSerialBaud: Int64 {
            get { StationCat.int(fields, "pttSerialBaud") }
            set { fields["pttSerialBaud"] = .number(Double(newValue)) }
        }
        public var pttSerialParity: String {
            get { StationCat.text(fields, "pttSerialParity") }
            set { fields["pttSerialParity"] = .string(newValue) }
        }
        public var pttSerialDataBits: Int64 {
            get { StationCat.int(fields, "pttSerialDataBits") }
            set { fields["pttSerialDataBits"] = .number(Double(newValue)) }
        }
        public var pttSerialStopBits: String {
            get { StationCat.text(fields, "pttSerialStopBits") }
            set { fields["pttSerialStopBits"] = .string(newValue) }
        }

        /// The `config` argument of `setStationCatGlobal`.
        public var text: String {
            LinkJSON.object(fields).compactText
        }
    }

    public struct Global: Equatable, Sendable {
        public var received: Bool
        public var config: GlobalConfig
        /// Automatic frequency information is running.
        public var aiActive: Bool
        /// PTT's state as the Core words it.
        public var pttState: String

        public init(text: String?) {
            let fields = StationCat.object(text)
            received = fields != nil
            let object = fields ?? [:]
            config = GlobalConfig(fields: StationCat.members(object, "config"))
            aiActive = StationCat.bool(object, "aiActive")
            pttState = StationCat.text(object, "pttState")
        }
    }

    // MARK: Platform

    /// One way a virtual serial port can talk: the value the Core takes, and its name for it.
    public struct Dialect: Equatable, Sendable, Identifiable {
        public var value: String
        public var label: String

        public var id: String { value }

        public init(value: String, label: String) {
            self.value = value
            self.label = label
        }
    }

    /// What the Core's computer can do with serial ports.
    public struct Platform: Equatable, Sendable {
        public var received: Bool
        /// The Core has serial port support.
        public var serial: Bool
        /// The Core's computer has virtual serial ports (PTYs).
        public var pty: Bool
        public var markSpaceParity: Bool
        public var oneAndHalfStop: Bool
        public var serialDevices: [String]
        /// The dialects a virtual serial port takes, in the Core's order; empty when not sent.
        public var ptyDialects: [Dialect]

        public init(text: String?) {
            let fields = StationCat.object(text)
            received = fields != nil
            let object = fields ?? [:]
            serial = StationCat.bool(object, "serial")
            pty = StationCat.bool(object, "pty")
            markSpaceParity = StationCat.bool(object, "markSpaceParity")
            oneAndHalfStop = StationCat.bool(object, "oneAndHalfStop")
            var devices: [String] = []
            if case .array(let items)? = object["serialDevices"] {
                devices = items.compactMap { item in
                    if case .string(let value) = item {
                        return value
                    }
                    return nil
                }
            }
            serialDevices = devices
            var dialects: [Dialect] = []
            if case .array(let items)? = object["ptyDialects"] {
                dialects = items.compactMap { item in
                    guard case .object(let entry) = item else {
                        return nil
                    }
                    let value = StationCat.text(entry, "value")
                    guard !value.isEmpty else {
                        return nil
                    }
                    let label = StationCat.text(entry, "label")
                    return Dialect(value: value, label: label.isEmpty ? value : label)
                }
            }
            ptyDialects = dialects
        }
    }

    // MARK: Tests and the log

    /// The last command the Core's CAT tester ran, from any device.
    public struct LastTest: Equatable, Sendable {
        public var received: Bool
        public var requestId: Int64
        public var channel: Int64
        public var command: String
        public var reply: String
        public var accepted: Bool

        public init(text: String?) {
            let fields = StationCat.object(text)
            received = fields != nil
            let object = fields ?? [:]
            requestId = StationCat.int(object, "requestId")
            channel = StationCat.int(object, "channel")
            command = StationCat.text(object, "command")
            reply = StationCat.text(object, "reply")
            accepted = StationCat.bool(object, "accepted")
        }
    }

    /// One line of the Core's CAT traffic, from the `catLog` stream.
    public struct LogLine: Equatable, Sendable, Identifiable {
        /// The record's id, a rising number.
        public var id: String
        /// 1 to 4; 0 when not sent.
        public var channel: Int64
        /// Received from the CAT program, not sent to it.
        public var inbound: Bool
        public var text: String
        /// The Core's clock when it logged the line, in ms since 1970.
        public var timeMs: Int64?

        public init(id: String, channel: Int64, inbound: Bool, text: String, timeMs: Int64?) {
            self.id = id
            self.channel = channel
            self.inbound = inbound
            self.text = text
            self.timeMs = timeMs
        }

        public init(record: LinkMessage.RecordBatch.Record) {
            let fields = record.fields
            var time: Int64?
            if case .number(let value)? = fields["time"], value.isFinite, abs(value) < 9.0e18 {
                time = Int64(value)
            }
            self.init(id: record.id, channel: StationCat.int(fields, "channel"),
                      inbound: StationCat.bool(fields, "inbound"), text: StationCat.text(fields, "text"), timeMs: time)
        }
    }
}
