// NereusSDR for iOS: CAT Control, the Core's four CAT channels, their shared options and PTT, its tester and its log
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import Network
import NereusLink
import NereusMirror
import os

/// The CAT Control pages' model: a remote control for the CAT the Core runs
/// for its station (``StationCat``, `stationCatVersion` 1), the same surface
/// a connected desktop has. CAT never runs on the phone. It reads the Core's
/// four channels, their shared settings, the limits of the Core's computer
/// and the last test; it changes a channel with `setStationCatChannel` and
/// the shared settings with `setStationCatGlobal`, each carrying the whole
/// config as the page shows it with the change over it. As the desktop's
/// remote window keeps a setting it sent: the change stays shown until the
/// Core answers the latest send; a refusal or no answer shows the Core's
/// value again with the words; an accepted change gives way once the Core
/// holds it, or at the Core's next change to that channel (or to the
/// shared settings). The Core does not refuse CAT changes on the air, so
/// the page does not either. The tester runs a command on a channel
/// (`testStationCatCommand`) and shows each reply by its request; the
/// Core's CAT log (`catLog`) is asked for only while the Test and log page
/// is open, and the lines shown grow as the Core sends new ones, with this
/// phone's own lines for each change of a channel's state and of PTT, as
/// the desktop's CAT log window writes them. A channel page or the PTT
/// page asks the Core to read its serial devices again when it opens.
@MainActor
final class CatControlModel: ObservableObject {
    static let timeout: Duration = .seconds(10)
    /// How long a change, a test or a device read waits for the Core's answer.
    let timeout: Duration
    /// The tests the page keeps, newest first.
    static let testsKept = 6

    /// A page under CAT Control.
    enum Route: Hashable {
        case channel(Int)
        case options
        case ptt
        case test

        var title: String {
            switch self {
            case .channel(let number): return "CAT \(number)"
            case .options: return "CAT Options"
            case .ptt: return "CAT PTT"
            case .test: return "Test and log"
            }
        }
    }

    /// What a change is sent to: one channel, or the settings every channel shares.
    enum Slot: Hashable {
        case channel(Int)
        case global
    }

    /// A channel's two network listeners.
    enum Listener: Hashable {
        case tcp
        case rigctld
    }

    /// The VFO a channel's slice binding is for.
    enum Vfo: Hashable {
        case a
        case b
    }

    /// Which lines of the log show, as the desktop's window filters them.
    enum LogFilter: Int64, CaseIterable {
        case all
        case received
        case sent
        case diagnostics

        var label: String {
            switch self {
            case .all: return "All"
            case .received: return "Received"
            case .sent: return "Sent"
            case .diagnostics: return "Diagnostics"
            }
        }

        func shows(_ entry: LogEntry) -> Bool {
            switch self {
            case .all: return true
            case .received: return entry.kind == .received
            case .sent: return entry.kind == .sent
            case .diagnostics: return entry.kind == .diagnostic
            }
        }
    }

    /// One line of the log as the page shows it: the Core's bytes for a CAT
    /// program, or this phone's words for a change of state.
    struct LogEntry: Identifiable, Equatable {
        enum Kind: Equatable {
            /// From the CAT program.
            case received
            /// To the CAT program.
            case sent
            /// A channel's or PTT's change of state.
            case diagnostic
        }

        /// Rising in the order the page took the lines.
        let id: Int
        let kind: Kind
        /// When the Core logged it, or when it arrived here.
        let time: String
        /// "CAT1 in " before bytes; nil for a diagnostic.
        let head: String?
        /// The bytes as text, each outside printable text as `\xNN`; a diagnostic's words.
        let text: String
        let byteCount: Int
        /// Each byte as two hex digits, space between.
        let hex: String
    }

    /// One entry of a choice: the value sent, its name, and why it cannot be chosen, if it cannot.
    struct Choice: Identifiable, Equatable {
        let value: String
        let label: String
        var reason: String?

        var id: String { value }
        var enabled: Bool { reason == nil }
    }

    /// One test sent to the Core, and its answer once it came.
    struct TestRun: Identifiable, Equatable {
        /// How the Core answered a test.
        enum Answer: Equatable {
            /// The radio's reply, as text.
            case reply
            /// This phone's words: accepted with nothing back, or no answer.
            case note
            /// The Core's words for a refusal.
            case refused
        }

        let id: Int64
        let channel: Int
        let command: String
        /// The reply, or the words for no reply or a refusal; nil while waiting.
        var answer: String?
        var kind: Answer = .reply

        var refused: Bool { kind == .refused }
    }

    // MARK: Words

    static let notConnectedReason = SpotsModel.notConnectedReason
    static let olderCoreReason = StationToolList.noStationCatReason
    static let waitingReason = "The Core has not sent its CAT setup."
    static let noLogReason = "This Core does not send its CAT log to this app. Updating the Core may help."
    static let openToNetworkWarning =
        "Open to your network: any device that can reach this port can tune and key the radio. There is no password."
    static let slicesNote =
        "Each channel controls the slices assigned to it, whichever slice is selected on screen. A virtual serial port exists only while its channel is on."
    static let closedSliceReason = "The slice this channel controlled was closed. Pick another slice."
    static let noSerialReason = "The Core's computer cannot open serial ports."
    static let noPtyReason = "The Core's computer has no virtual serial ports. They are found only on macOS and Linux."
    static let markParityReason = "The Core's computer cannot use mark parity."
    static let spaceParityReason = "The Core's computer cannot use space parity."
    static let oneAndHalfStopReason = "The Core's computer cannot use 1.5 stop bits."
    static let recenterNote =
        "A frequency from CAT moves the panadapter the way tuning by hand does: it follows the VFO, and with CTUN on it stays put unless the band changes."
    static let pttNote =
        "CAT 1 to CAT 4 read the pins of a serial CAT port that is already open. Physical opens a serial device of its own. After a press, every selected input must be released before the next press can key the radio. Opening this page or restoring settings never keys the radio. These controls never drive the RTS or DTR pins."
    static let noPttReason = "Input PTT is not available: the Core's computer cannot open serial ports."
    static let testerNote =
        "Test commands act on the radio: receive and setting changes take effect. Commands that transmit (TX, Tune, Two Tone, VOX on, PureSignal single shot and calibration) are refused here."
    static let noReplyText = "Accepted (no reply)"
    static let unansweredText = "The Core did not answer this test."
    static let pausedNote = "Paused: new lines are dropped, and are not shown on Resume."
    static let showBytesTitle = "Bytes and hex"
    static let noLinesText = "No CAT traffic."
    static let refusedText = BandSlicesModel.refusedText

    // MARK: The Core's lists

    static let baudRates: [Int64] = [300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200]
    static let parities = ["None", "Odd", "Even", "Mark", "Space"]
    static let channelDataBits: [Int64] = Array((6...8).reversed())
    static let pttDataBits: [Int64] = Array(5...8)
    static let stopBits = ["1", "1.5", "2"]
    static let rigIdentities = ["PowerSDR", "TS-2000", "TS-50S", "TS-480"]
    static let pttSources: [(value: String, label: String)] = [("None", "None"), ("CAT1", "CAT 1"), ("CAT2", "CAT 2"),
                                                                ("CAT3", "CAT 3"), ("CAT4", "CAT 4"),
                                                                ("Physical", "Physical")]
    static let portRange: ClosedRange<Int64> = 0...65535
    static let rttyRange: ClosedRange<Int64> = -3000...3000

    // MARK: State

    /// Why the pages cannot be used now; nil when they can.
    @Published private(set) var reason: String?
    /// CAT 1 to CAT 4 as the pages show them: a change on its way, else the Core's.
    @Published private(set) var channels: [StationCat.Channel] = StationCat.channels.map {
        StationCat.Channel(number: $0, text: nil)
    }
    /// The shared settings as the pages show them; nil until the Core sends them.
    @Published private(set) var global: StationCat.GlobalConfig?
    @Published private(set) var pttState = ""
    @Published private(set) var platform = StationCat.Platform(text: nil)
    /// The slices on the Core, every device's, by id.
    @Published private(set) var sliceIds: [Int64] = []
    /// The Core's words for the last change it refused, by what was changed.
    @Published private(set) var notes: [Slot: String] = [:]
    /// The number pad open over the page, if any.
    @Published private(set) var pad: ValuePadModel?
    /// The tests sent from this page, newest first.
    @Published private(set) var tests: [TestRun] = []
    /// The channel the tester sends to.
    @Published var testChannel = 1
    /// The log's lines as shown, oldest first: filtered, and none that came while paused.
    @Published private(set) var logLines: [LogEntry] = []
    @Published private(set) var logReason: String?
    @Published private(set) var logFilter: LogFilter = .all
    @Published private(set) var paused = false
    /// Each line of CAT traffic with its byte count and its bytes in hex, as the desktop's window writes them.
    @Published var showBytes = false
    @Published var followNewest = true {
        didSet {
            if followNewest {
                follow()
            }
        }
    }
    /// The line the log keeps in view: the newest while Follow newest is on.
    @Published private(set) var scrollTarget: Int?
    /// How many times the log's lines were made again from the start
    /// (a filter change, Clear), and how many times the Core's lines were
    /// read; neither happens for a change that is not the log's.
    private(set) var logRebuilds = 0
    private(set) var logReads = 0

    private static let logger = Logger(subsystem: "NereusSDR", category: "tools.cat")

    private let mirror: MirrorStore
    private let commands: CommandClient?
    private let records: RecordStreamClient?
    private var watch: ToolMirrorWatch?
    private var logWatch: AnyCancellable?
    /// The pages open now; the log is asked for only while Test and log is.
    private var openPages: [Route: Int] = [:]
    private var wanting = false

    /// A change sent to the Core, shown over its value until the Core's
    /// answer settles it (the desktop's unconfirmed setting).
    private struct Held<Value: Equatable> {
        var value: Value
        /// The latest send of it; only that send's answer settles it.
        var send: Int
        /// The Core accepted the latest send: its next change gives way.
        var accepted = false
    }
    private var heldChannels: [Int: Held<StationCat.ChannelConfig>] = [:]
    private var heldGlobal: Held<StationCat.GlobalConfig>?
    /// Counts sends, in the order the changes were made.
    private var sends = 0
    /// Each slot's property as last read, so a change of it is seen.
    private var seen: [Slot: String] = [:]
    /// Each slot's sends go one after another, in the order the changes were made.
    private var chains: [Slot: Task<PropertyWriteOutcome, Never>] = [:]
    /// Rising from a start no other device's tester is likely to share, as
    /// the desktop's remote window seeds its own, since the Core's
    /// `lastTest` names a test by this id for every device.
    private var nextTestId = Int64(Date().timeIntervalSince1970 * 1000) * 1000

    /// Every line taken, oldest first, at most the log's capacity.
    private var logEntries: [LogEntry] = []
    private var nextLogId = 0
    /// The newest of the Core's lines read, and the stream's generation:
    /// the lines after it are new. Lines that come while paused are read
    /// and dropped, as the desktop's window drops them.
    private var lastLogId: Int64 = 0
    private var logGeneration: Int64?
    /// Each channel's state and the PTT state as last seen while the log
    /// was open, for its diagnostics; nil while it is not.
    private var seenStatus: [Int: StationCat.ChannelStatus]?
    private var seenPtt = ""

    init(mirror: MirrorStore, commands: CommandClient?, records: RecordStreamClient?,
         timeout: Duration = CatControlModel.timeout) {
        self.mirror = mirror
        self.timeout = timeout
        self.commands = commands
        self.records = records
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        self.watch = watch
        if let records {
            watch.watch(records.$available)
            watch.watch(records.$refusals)
            // The log alone follows its stream, and only when that stream
            // changes: the newest line, the count, or the generation.
            logWatch = records.$streams
                .map { $0[StationCat.logStream] }
                .removeDuplicates { Self.logStamp($0) == Self.logStamp($1) }
                .sink { [weak self] stream in self?.readLog(stream) }
        }
        refresh()
    }

    private var version: Int64 {
        (mirror.agreedMinor ?? 0) >= StationCat.minor ? mirror.capabilityVersion(StationCat.capabilityName) : 0
    }

    // MARK: Reading

    func refresh() {
        let object = watch?.object(StationCat.objectKey)
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<CatControlModel, Value>, _ value: Value) {
            if self[keyPath: path] != value {
                self[keyPath: path] = value
            }
        }
        let cat = connected && version >= 1 ? StationCat(values: object?.values ?? [:]) : StationCat(values: [:])
        let why: String? = !connected ? Self.notConnectedReason
            : version < 1 ? Self.olderCoreReason
            : object == nil || !cat.global.received || cat.channels.contains(where: { !$0.received }) ? Self.waitingReason
            : nil
        set(\.reason, why)
        if why != nil {
            // Gone, or not set up here: nothing sent will be answered, and
            // the words for what was refused belong to that session.
            heldChannels = [:]
            heldGlobal = nil
            seen = [:]
            set(\.notes, [:])
        } else {
            // The Core's change to what a kept setting covers lets it go,
            // once the Core has accepted it (CatControl.cpp:705-719).
            let texts = Self.slotTexts(object)
            for (slot, text) in texts where seen[slot] != text {
                seen[slot] = text
                switch slot {
                case .channel(let number):
                    if heldChannels[number]?.accepted == true {
                        heldChannels[number] = nil
                    }
                case .global:
                    if heldGlobal?.accepted == true {
                        heldGlobal = nil
                    }
                }
            }
        }
        var shown: [StationCat.Channel] = []
        for number in StationCat.channels {
            var channel = cat.channel(number)
            if let held = heldChannels[number] {
                channel.config = held.value
            }
            shown.append(channel)
        }
        set(\.channels, shown)
        set(\.global, why == nil ? (heldGlobal?.value ?? cat.global.config) : nil)
        set(\.pttState, cat.global.pttState)
        set(\.platform, cat.platform)
        var ids = Set<Int64>()
        for key in mirror.objectKeys {
            for prefix in ["slice:", SeveralDevices.markerPrefix] where key.hasPrefix(prefix) {
                if let id = Int64(key.dropFirst(prefix.count)) {
                    ids.insert(id)
                }
            }
        }
        set(\.sliceIds, ids.sorted())
        refreshLog(connected: connected)
        noteChanges(cat, live: wanting && why == nil)
    }

    /// Each slot's property text in the Core's object: its JSON, or "" when absent.
    private static func slotTexts(_ object: MirrorObject?) -> [(Slot, String)] {
        func text(_ name: String) -> String {
            if case .text(let value)? = object?.values[name] {
                return value
            }
            return ""
        }
        return StationCat.channels.map { (Slot.channel($0), text(StationCat.channelProperty($0))) }
            + [(Slot.global, text(StationCat.globalProperty))]
    }

    /// Asks for the log while Test and log is open and the Core sends it.
    private func refreshLog(connected: Bool) {
        let logs = connected && version >= 1 && records?.available == true
        let why: String? = !connected ? Self.notConnectedReason
            : version < 1 ? Self.olderCoreReason
            : !logs ? Self.noLogReason
            : records?.refusals[StationCat.logStream]
        if logReason != why {
            logReason = why
        }
        if logs && (openPages[.test] ?? 0) > 0 {
            if !wanting {
                wanting = true
                records?.want(StationCat.logStream, backlog: StationCat.logCapacity, by: self)
                // Its lines may already be here for another reader.
                readLog(records?.streams[StationCat.logStream])
            }
        } else if wanting {
            wanting = false
            records?.unwant(StationCat.logStream, by: self)
        }
    }

    /// CAT `number` as the pages show it.
    func channel(_ number: Int) -> StationCat.Channel {
        channels.first { $0.number == number } ?? StationCat.Channel(number: number, text: nil)
    }

    /// The Core's words for the last change of `slot` it refused, if any.
    func note(_ slot: Slot) -> String? {
        notes[slot]
    }

    // MARK: Pages opening

    /// A page under CAT Control opened or closed. A channel page and the
    /// PTT page ask the Core to read its serial devices again; Test and log
    /// asks for the log while it shows.
    func setOpen(_ route: Route, _ shown: Bool) {
        let count = max(0, (openPages[route] ?? 0) + (shown ? 1 : -1))
        openPages[route] = count == 0 ? nil : count
        // Read first, so a Core that has just gone is not asked.
        refresh()
        if shown {
            switch route {
            case .channel, .ptt: refreshDevices()
            case .options, .test: break
            }
        }
    }

    private func refreshDevices() {
        guard reason == nil, let commands else {
            return
        }
        let timeout = timeout
        Task {
            do {
                _ = try await commands.invoke(StationCat.refreshDevicesVerb, arguments: [], timeout: timeout)
            } catch {
                Self.logger.info("The Core did not answer a request to read its serial devices")
            }
        }
    }

    // MARK: A channel's lines

    /// The line on CAT Control's page for a channel: its state and clients.
    func summary(_ number: Int) -> String {
        let channel = channel(number)
        guard channel.received else {
            return "--"
        }
        let status = channel.status
        var parts = [status.state.isEmpty ? "--" : status.state]
        let config = channel.config
        if config.tcpEnabled {
            parts.append("TCP clients: \(status.tcpClients)")
        }
        if config.rigctldEnabled {
            parts.append("rigctld clients: \(status.rigctldClients)")
        }
        if config.serialEnabled {
            parts.append("Serial: \(status.serial)")
        }
        if config.ptyEnabled {
            parts.append(status.ptyPath.isEmpty ? "PTY: \(status.pty)" : status.ptyPath)
        }
        return parts.joined(separator: " · ")
    }

    /// Whether any way into a channel is switched on.
    func isOn(_ number: Int) -> Bool {
        let config = channel(number).config
        return config.tcpEnabled || config.rigctldEnabled || config.serialEnabled || config.ptyEnabled
    }

    func tcpStatus(_ number: Int) -> String {
        let status = channel(number).status
        return "TCP: \(status.tcp) · Bound: \(status.tcpBoundAddress):\(status.tcpBoundPort) · Clients: \(status.tcpClients)"
    }

    func rigctldStatus(_ number: Int) -> String {
        let status = channel(number).status
        return "rigctld: \(status.rigctld) · Bound: \(status.rigctldBoundAddress):\(status.rigctldBoundPort) · Clients: \(status.rigctldClients)"
    }

    func serialStatus(_ number: Int) -> String {
        "Serial: \(channel(number).status.serial)"
    }

    func ptyStatus(_ number: Int) -> String {
        let status = channel(number).status
        return status.ptyPath.isEmpty ? "PTY: \(status.pty)" : status.ptyPath
    }

    /// The commands CAT `number`'s virtual serial port speaks, by the Core's name for them.
    func ptyDialectLine(_ number: Int) -> String {
        let value = channel(number).config.ptyDialect
        let label = platform.ptyDialects.first { $0.value == value }?.label ?? value
        return "CAT \(number) PTY uses \(label) on the Core's computer."
    }

    /// Whether a listener is on at an address other devices can reach, as
    /// the desktop's page warns: on, an address that reads as one, and not
    /// the computer's own.
    static func opensToNetwork(enabled: Bool, address: String) -> Bool {
        guard enabled else {
            return false
        }
        let text = address.trimmingCharacters(in: .whitespaces)
        if let v4 = IPv4Address(text) {
            return !v4.isLoopback
        }
        if let v6 = IPv6Address(text) {
            return !v6.isLoopback
        }
        return false
    }

    func opensToNetwork(_ number: Int, _ listener: Listener) -> Bool {
        let config = channel(number).config
        switch listener {
        case .tcp: return Self.opensToNetwork(enabled: config.tcpEnabled, address: config.tcpBindAddress)
        case .rigctld: return Self.opensToNetwork(enabled: config.rigctldEnabled, address: config.rigctldBindAddress)
        }
    }

    // MARK: Limits of the Core's computer

    /// Why serial CAT cannot be set up, or nil when it can.
    var serialReason: String? {
        reason ?? (platform.serial ? nil : Self.noSerialReason)
    }

    /// Why virtual serial ports cannot be set up, or nil when they can.
    var ptyReason: String? {
        reason ?? (platform.pty ? nil : Self.noPtyReason)
    }

    /// Why input PTT cannot be set up, or nil when it can.
    var pttReason: String? {
        reason ?? (platform.serial ? nil : Self.noPttReason)
    }

    var parityChoices: [Choice] {
        Self.parities.map { parity in
            switch parity {
            case "Mark": return Choice(value: parity, label: parity,
                                       reason: platform.markSpaceParity ? nil : Self.markParityReason)
            case "Space": return Choice(value: parity, label: parity,
                                        reason: platform.markSpaceParity ? nil : Self.spaceParityReason)
            default: return Choice(value: parity, label: parity)
            }
        }
    }

    var stopBitChoices: [Choice] {
        Self.stopBits.map { stop in
            Choice(value: stop, label: stop,
                   reason: stop == "1.5" && !platform.oneAndHalfStop ? Self.oneAndHalfStopReason : nil)
        }
    }

    /// The Core's serial devices, with `current` kept when it is a path typed in.
    func deviceChoices(current: String) -> [Choice] {
        var devices = platform.serialDevices.map { Choice(value: $0, label: $0) }
        if !current.isEmpty, !platform.serialDevices.contains(current) {
            devices.append(Choice(value: current, label: current))
        }
        return devices
    }

    /// The commands a virtual serial port can speak, by the Core's names.
    func dialectChoices(_ number: Int) -> [Choice] {
        var choices = platform.ptyDialects.map { Choice(value: $0.value, label: $0.label) }
        let current = channel(number).config.ptyDialect
        if !current.isEmpty, !choices.contains(where: { $0.value == current }) {
            choices.append(Choice(value: current, label: current))
        }
        return choices
    }

    // MARK: Slices

    /// The choices for a channel's VFO A or VFO B slice: None, each slice on
    /// the Core, and, when the slice it was bound to was closed, that one,
    /// greyed with its reason. Picking a slice binds the channel to it again.
    func sliceChoices(_ number: Int, _ vfo: Vfo) -> [Choice] {
        let channel = channel(number)
        let bound = vfo == .a ? channel.config.primarySliceId : channel.config.secondarySliceId
        let valid = vfo == .a ? channel.primaryValid : channel.secondaryValid
        var choices = [Choice(value: "none", label: "None")]
        choices += sliceIds.map { Choice(value: "slice:\($0)", label: Self.sliceName($0)) }
        if bound >= 0 {
            if !valid {
                choices.append(Choice(value: "closed:\(bound)", label: "\(Self.sliceName(bound)), closed",
                                      reason: Self.closedSliceReason))
            } else if !sliceIds.contains(bound) {
                choices.append(Choice(value: "slice:\(bound)", label: Self.sliceName(bound)))
            }
        }
        return choices
    }

    /// The choice shown as a channel's VFO A or VFO B slice now.
    func sliceSelected(_ number: Int, _ vfo: Vfo) -> String {
        let channel = channel(number)
        let bound = vfo == .a ? channel.config.primarySliceId : channel.config.secondarySliceId
        let valid = vfo == .a ? channel.primaryValid : channel.secondaryValid
        if bound < 0 {
            return "none"
        }
        return valid ? "slice:\(bound)" : "closed:\(bound)"
    }

    /// Why a channel's VFO A or VFO B binding needs a new slice, or nil.
    func sliceProblem(_ number: Int, _ vfo: Vfo) -> String? {
        sliceSelected(number, vfo).hasPrefix("closed:") ? Self.closedSliceReason : nil
    }

    static func sliceName(_ id: Int64) -> String {
        "Slice \(SliceAccess.letter(Int(id)))"
    }

    /// Picks a slice for a channel's VFO A or VFO B, as the desktop's
    /// selector does: a slice binds again (the rebind flag), None unbinds,
    /// the closed one does nothing, and the one already shown sends nothing.
    func pickSlice(_ number: Int, _ vfo: Vfo, _ choice: String) {
        guard choice != sliceSelected(number, vfo) else {
            return
        }
        let id: Int64
        let rebind: Bool
        if choice == "none" {
            id = -1
            rebind = false
        } else if choice.hasPrefix("slice:"), let picked = Int64(choice.dropFirst("slice:".count)) {
            id = picked
            rebind = true
        } else {
            return
        }
        change(number, primaryRebind: vfo == .a && rebind, secondaryRebind: vfo == .b && rebind) { config in
            if vfo == .a {
                config.primarySliceId = id
            } else {
                config.secondarySliceId = id
            }
        }
    }

    // MARK: Changing a channel

    /// Changes one channel: the whole config as shown, with `edit` over it.
    /// A change that changes nothing sends nothing, as the desktop's
    /// controls send only on a change.
    func change(_ number: Int, primaryRebind: Bool = false, secondaryRebind: Bool = false,
                _ edit: (inout StationCat.ChannelConfig) -> Void) {
        _ = sendChannel(number, primaryRebind: primaryRebind, secondaryRebind: secondaryRebind, edit)
    }

    /// Sets a listener's address, as typed.
    func setAddress(_ number: Int, _ listener: Listener, _ address: String) {
        let text = address.trimmingCharacters(in: .whitespaces)
        change(number) { config in
            switch listener {
            case .tcp: config.tcpBindAddress = text
            case .rigctld: config.rigctldBindAddress = text
            }
        }
    }

    /// Opens the number pad for a listener's port.
    func openPortPad(_ number: Int, _ listener: Listener) {
        guard reason == nil else {
            return
        }
        let config = channel(number).config
        pad?.retire()
        pad = ValuePadModel(title: listener == .tcp ? "CAT \(number) TCP port" : "CAT \(number) rigctld port",
                            unit: "", range: Self.portRange,
                            current: listener == .tcp ? config.tcpPort : config.rigctldPort,
                            send: { [weak self] next in
                                let sent = self?.sendChannel(number, primaryRebind: false, secondaryRebind: false) {
                                    switch listener {
                                    case .tcp: $0.tcpPort = next
                                    case .rigctld: $0.rigctldPort = next
                                    }
                                }
                                return await sent?.value
                            }, close: { [weak self] in self?.pad = nil })
    }

    /// Shows the change and puts its send after the channel's earlier ones,
    /// now, so the sends go in the order the changes were made.
    private func sendChannel(_ number: Int, primaryRebind: Bool, secondaryRebind: Bool,
                             _ edit: (inout StationCat.ChannelConfig) -> Void) -> Task<PropertyWriteOutcome, Never>? {
        guard reason == nil, StationCat.channels.contains(number),
              let index = channels.firstIndex(where: { $0.number == number }) else {
            return nil
        }
        var next = channels[index].config
        edit(&next)
        guard next != channels[index].config || primaryRebind || secondaryRebind else {
            return nil
        }
        sends += 1
        heldChannels[number] = Held(value: next, send: sends)
        channels[index].config = next
        return enqueue(.channel(number), StationCat.setChannelVerb, send: sends, [
            CommandArgument(name: "channel", value: .int(Int64(number))),
            CommandArgument(name: "config", value: .text(next.commandText(primaryRebind: primaryRebind,
                                                                           secondaryRebind: secondaryRebind))),
        ])
    }

    // MARK: Changing the shared settings

    /// Changes the settings every channel shares: the whole config as
    /// shown, with `edit` over it. A change that changes nothing sends nothing.
    func changeGlobal(_ edit: (inout StationCat.GlobalConfig) -> Void) {
        _ = sendGlobal(edit)
    }

    /// Opens the number pad for an RTTY offset: DIGU's, or DIGL's.
    func openRttyPad(digu: Bool) {
        guard reason == nil, let global else {
            return
        }
        pad?.retire()
        pad = ValuePadModel(title: digu ? "DIGU offset" : "DIGL offset", unit: "Hz", range: Self.rttyRange,
                            current: digu ? global.rttyDiguHz : global.rttyDiglHz,
                            send: { [weak self] next in
                                let sent = self?.sendGlobal { config in
                                    if digu {
                                        config.rttyDiguHz = next
                                    } else {
                                        config.rttyDiglHz = next
                                    }
                                }
                                return await sent?.value
                            }, close: { [weak self] in self?.pad = nil })
    }

    private func sendGlobal(_ edit: (inout StationCat.GlobalConfig) -> Void) -> Task<PropertyWriteOutcome, Never>? {
        guard reason == nil, let shown = global else {
            return nil
        }
        var next = shown
        edit(&next)
        guard next != shown else {
            return nil
        }
        sends += 1
        heldGlobal = Held(value: next, send: sends)
        global = next
        return enqueue(.global, StationCat.setGlobalVerb, send: sends,
                       [CommandArgument(name: "config", value: .text(next.text))])
    }

    // MARK: Sending

    /// Puts one send after the slot's earlier ones and returns it; the
    /// Core's answer is noted as it comes.
    private func enqueue(_ slot: Slot, _ verb: String, send: Int,
                         _ arguments: [CommandArgument]) -> Task<PropertyWriteOutcome, Never> {
        let previous = chains[slot]
        let commands = commands
        let timeout = timeout
        let job = Task { @MainActor [weak self] () -> PropertyWriteOutcome in
            _ = await previous?.value
            var outcome = PropertyWriteOutcome.notSent
            if let commands {
                do {
                    outcome = PropertyWriteOutcome(.success(try await commands.invoke(verb, arguments: arguments,
                                                                                      timeout: timeout)))
                } catch let error as CommandError {
                    Self.logger.info("A CAT change had no answer from the Core")
                    outcome = PropertyWriteOutcome(.failure(error))
                } catch {
                    outcome = .notSent
                }
            }
            self?.answered(slot, send: send, outcome)
            return outcome
        }
        chains[slot] = job
        return job
    }

    /// The Core's answer to one send, as the desktop's remote window
    /// settles it (CatControl.cpp:630-655): a refusal or no answer to the
    /// latest send shows the Core's value again; the latest accepted gives
    /// way now if the Core already holds it, else at the Core's next change.
    /// An earlier send's answer changes only the words.
    private func answered(_ slot: Slot, send: Int, _ outcome: PropertyWriteOutcome) {
        guard reason == nil else {
            // Gone: the page already shows why, and holds nothing.
            return
        }
        if outcome.accepted {
            notes[slot] = nil
        } else if let text = outcome.noteText(refused: Self.refusedText) {
            notes[slot] = text
        }
        let cat = StationCat(values: watch?.object(StationCat.objectKey)?.values ?? [:])
        switch slot {
        case .channel(let number):
            guard var held = heldChannels[number], held.send == send else {
                break
            }
            if !outcome.accepted || held.value == cat.channel(number).config {
                heldChannels[number] = nil
            } else {
                held.accepted = true
                heldChannels[number] = held
            }
        case .global:
            guard var held = heldGlobal, held.send == send else {
                break
            }
            if !outcome.accepted || held.value == cat.global.config {
                heldGlobal = nil
            } else {
                held.accepted = true
                heldGlobal = held
            }
        }
        refresh()
    }

    // MARK: The tester

    /// Sends `command` to the tester's channel; its reply shows by its own request.
    func test(_ command: String) {
        let text = command.trimmingCharacters(in: .whitespaces)
        guard reason == nil, !text.isEmpty, let commands else {
            return
        }
        nextTestId += 1
        let id = nextTestId
        let channel = testChannel
        tests.insert(TestRun(id: id, channel: channel, command: text), at: 0)
        if tests.count > Self.testsKept {
            tests.removeLast(tests.count - Self.testsKept)
        }
        let timeout = timeout
        Task { [weak self] in
            var answer = Self.unansweredText
            var kind = TestRun.Answer.note
            do {
                let result = try await commands.invoke(StationCat.testVerb, arguments: [
                    CommandArgument(name: "requestId", value: .int(id)),
                    CommandArgument(name: "channel", value: .int(Int64(channel))),
                    CommandArgument(name: "command", value: .text(text)),
                ], timeout: timeout)
                if result.accepted {
                    var reply = ""
                    if case .text(let value)? = result.values["reply"] {
                        reply = value
                    }
                    answer = reply.isEmpty ? Self.noReplyText : Self.escaped(reply)
                    kind = reply.isEmpty ? .note : .reply
                } else {
                    answer = result.reason.isEmpty ? Self.refusedText : result.reason
                    kind = .refused
                }
            } catch {
                Self.logger.info("A CAT test had no answer from the Core")
            }
            guard let self, let index = self.tests.firstIndex(where: { $0.id == id }) else {
                return
            }
            self.tests[index].answer = answer
            self.tests[index].kind = kind
        }
    }

    // MARK: The log

    func setLogFilter(_ filter: LogFilter) {
        guard logFilter != filter else {
            return
        }
        logFilter = filter
        rebuildLog()
    }

    /// Pauses the log or resumes it. As the desktop's window does, lines that
    /// arrive while paused are dropped: Resume starts with what comes next.
    func setPaused(_ on: Bool) {
        guard paused != on else {
            return
        }
        paused = on
    }

    /// Empties the log, as the desktop's window's Clear does; the Core's
    /// lines already read do not come back.
    func clearLog() {
        logEntries = []
        rebuildLog()
    }

    /// What a stream's change is told by: the generation, the count and the
    /// newest line. The Core never changes a line it logged.
    private static func logStamp(_ stream: RecordStreamClient.Stream?) -> [String]? {
        stream.map { ["\($0.generation)", "\($0.records.count)", $0.records.last?.id ?? ""] }
    }

    /// Takes the Core's lines that are new since the last read, as the
    /// desktop's remote window does (CatControl.cpp:507-545): by their
    /// rising ids, counting again in a new generation or when the Core
    /// numbers from the start again.
    private func readLog(_ stream: RecordStreamClient.Stream?) {
        guard wanting, let stream else {
            return
        }
        logReads += 1
        if stream.generation != logGeneration {
            logGeneration = stream.generation
            lastLogId = 0
        }
        let newest = stream.records.last.flatMap { Int64($0.id) } ?? 0
        if newest < lastLogId {
            lastLogId = 0
        }
        var first = stream.records.endIndex
        while first > stream.records.startIndex, let id = Int64(stream.records[first - 1].id), id > lastLogId {
            first -= 1
        }
        lastLogId = max(lastLogId, newest)
        guard !paused, first < stream.records.endIndex else {
            return
        }
        let arrived = Self.nowMs()
        append(stream.records[first...].map { record in
            let line = StationCat.LogLine(record: record)
            return entry(kind: line.inbound ? .received : .sent, channel: line.channel, text: line.text,
                         timeMs: line.timeMs.flatMap { $0 > 0 ? $0 : nil } ?? arrived)
        })
    }

    /// The desktop window's diagnostics (CatLogWindow.cpp:61-73): each
    /// change of a channel's state, of its TCP, serial, PTY and rigctld,
    /// of its TCP client count, and of PTT, while the log is open.
    private func noteChanges(_ cat: StationCat, live: Bool) {
        guard live else {
            seenStatus = nil
            return
        }
        let now = Dictionary(uniqueKeysWithValues: StationCat.channels.map { ($0, cat.channel($0).status) })
        guard let was = seenStatus else {
            seenStatus = now
            seenPtt = cat.global.pttState
            return
        }
        var words: [(channel: Int64, text: String)] = []
        for number in StationCat.channels {
            guard let status = now[number], let before = was[number], status != before else {
                continue
            }
            let channel = Int64(number)
            for (name, state, old) in [("TCP", status.tcp, before.tcp), ("Serial", status.serial, before.serial),
                                       ("PTY", status.pty, before.pty), ("Rigctld", status.rigctld, before.rigctld)]
                where state != old {
                words.append((channel, "CAT\(number) \(name): \(state)"))
            }
            if status.state != before.state {
                words.append((channel, "CAT\(number) state: \(status.state)"))
            }
            if status.tcpClients != before.tcpClients {
                words.append((channel, "CAT\(number) TCP clients: \(status.tcpClients)"))
            }
        }
        if cat.global.pttState != seenPtt {
            words.append((0, "PTT: \(cat.global.pttState)"))
        }
        seenStatus = now
        seenPtt = cat.global.pttState
        guard !paused, !words.isEmpty else {
            return
        }
        let time = Self.nowMs()
        append(words.map { entry(kind: .diagnostic, channel: $0.channel, text: $0.text, timeMs: time) })
    }

    private func entry(kind: LogEntry.Kind, channel: Int64, text: String, timeMs: Int64) -> LogEntry {
        nextLogId += 1
        let time = Self.timeFormatter.string(from: Date(timeIntervalSince1970: Double(timeMs) / 1000))
        guard kind != .diagnostic else {
            return LogEntry(id: nextLogId, kind: kind, time: time, head: nil, text: text, byteCount: 0, hex: "")
        }
        let bytes = text.unicodeScalars.map { $0.value & 0xFF }
        return LogEntry(id: nextLogId, kind: kind, time: time, head: "CAT\(channel) \(kind == .received ? "in " : "out")",
                        text: Self.escaped(text), byteCount: bytes.count,
                        hex: bytes.map { String(format: "%02x", $0) }.joined(separator: " "))
    }

    /// New lines go last, the oldest going once the log holds its capacity.
    private func append(_ entries: [LogEntry]) {
        logEntries.append(contentsOf: entries)
        if logEntries.count > StationCat.logCapacity {
            logEntries.removeFirst(logEntries.count - StationCat.logCapacity)
        }
        let shown = entries.filter(logFilter.shows)
        guard !shown.isEmpty else {
            return
        }
        var lines = logLines
        lines.append(contentsOf: shown.suffix(StationCat.logCapacity))
        if lines.count > StationCat.logCapacity {
            lines.removeFirst(lines.count - StationCat.logCapacity)
        }
        logLines = lines
        follow()
    }

    /// The shown lines made again from every line taken: a filter change, or Clear.
    private func rebuildLog() {
        logRebuilds += 1
        let lines = logEntries.filter(logFilter.shows)
        if logLines != lines {
            logLines = lines
        }
        follow()
    }

    private func follow() {
        if followNewest, scrollTarget != logLines.last?.id {
            scrollTarget = logLines.last?.id
        }
    }

    private static func nowMs() -> Int64 {
        Int64(Date().timeIntervalSince1970 * 1000)
    }

    /// A log line as the page shows it: which channel, which way, and its
    /// bytes, each one outside printable text written as `\xNN`; with
    /// `bytes`, the byte count and the bytes in hex as well, as the
    /// desktop's window writes each line.
    static func lineText(_ line: LogEntry, bytes: Bool = false) -> String {
        guard let head = line.head else {
            return line.text
        }
        guard bytes else {
            return "\(head)  \(line.text)"
        }
        let way = head.trimmingCharacters(in: .whitespaces)
        return "\(way) bytes=\(line.byteCount)  \(line.text)  [hex \(line.hex)]"
    }

    private static let timeFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss.SSS"
        return formatter
    }()

    /// `text` with each character outside printable text, and the backslash, as `\xNN`.
    static func escaped(_ text: String) -> String {
        var shown = ""
        for scalar in text.unicodeScalars {
            if scalar.value >= 32, scalar.value <= 126, scalar != "\\" {
                shown.unicodeScalars.append(scalar)
            } else {
                shown += String(format: "\\x%02x", scalar.value & 0xFF)
            }
        }
        return shown
    }
}
