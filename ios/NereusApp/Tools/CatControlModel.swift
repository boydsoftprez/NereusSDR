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
/// config as the page shows it with the change over it. The change stays
/// shown until the Core answers and its new value arrives; a refusal shows
/// the Core's words and its own value again. The Core does not refuse CAT
/// changes on the air, so the page does not either. The tester runs a
/// command on a channel (`testStationCatCommand`) and shows each reply by
/// its request; the Core's CAT log (`catLog`) is asked for only while the
/// Test and log page is open. A channel page or the PTT page asks the Core
/// to read its serial devices again when it opens.
@MainActor
final class CatControlModel: ObservableObject {
    static let timeout: Duration = .seconds(10)
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

    /// Which lines of the log show.
    enum LogFilter: Int64, CaseIterable {
        case all
        case received
        case sent

        var label: String {
            switch self {
            case .all: return "All"
            case .received: return "Received"
            case .sent: return "Sent"
            }
        }
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
        let id: Int64
        let channel: Int
        let command: String
        /// The reply, or the words for no reply or a refusal; nil while waiting.
        var answer: String?
        var refused = false
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
    @Published private(set) var aiActive = false
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
    /// The log's lines as shown: filtered, and held while paused.
    @Published private(set) var logLines: [StationCat.LogLine] = []
    @Published private(set) var logReason: String?
    @Published private(set) var logFilter: LogFilter = .all
    @Published private(set) var paused = false
    @Published var followNewest = true

    private static let logger = Logger(subsystem: "NereusSDR", category: "tools.cat")

    private let mirror: MirrorStore
    private let commands: CommandClient?
    private let records: RecordStreamClient?
    private var watch: ToolMirrorWatch?
    /// The pages open now; the log is asked for only while Test and log is.
    private var openPages: [Route: Int] = [:]
    private var wanting = false
    private var channelPending: [Int: PendingChoice<StationCat.ChannelConfig>] = [:]
    private let globalPending = PendingChoice<StationCat.GlobalConfig>()
    /// Each slot's sends go one after another, in the order chosen.
    private var chains: [Slot: Task<Void, Never>] = [:]
    private var inFlight: [Slot: Int] = [:]
    private var nextTestId: Int64 = 0
    /// The log's newest line when Pause was pressed.
    private var pausedAt: Int?
    /// Lines that arrived while paused, never shown, as the desktop's window drops them.
    private var dropped: [ClosedRange<Int>] = []

    init(mirror: MirrorStore, commands: CommandClient?, records: RecordStreamClient?) {
        self.mirror = mirror
        self.commands = commands
        self.records = records
        for number in StationCat.channels {
            channelPending[number] = PendingChoice<StationCat.ChannelConfig>()
        }
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        self.watch = watch
        if let records {
            watch.watch(records.$streams)
            watch.watch(records.$available)
            watch.watch(records.$refusals)
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
        var shown: [StationCat.Channel] = []
        for number in StationCat.channels {
            var channel = cat.channel(number)
            guard let pending = channelPending[number] else {
                shown.append(channel)
                continue
            }
            pending.follow(object) { values in StationCat(values: values).channel(number).config }
            if why != nil {
                pending.drop()
            } else {
                pending.settle(mirrored: channel.config, sending: (inFlight[.channel(number)] ?? 0) > 0)
            }
            if let value = pending.value {
                channel.config = value
            }
            shown.append(channel)
        }
        set(\.channels, shown)
        globalPending.follow(object) { values in StationCat(values: values).global.config }
        if why != nil {
            globalPending.drop()
        } else {
            globalPending.settle(mirrored: cat.global.config, sending: (inFlight[.global] ?? 0) > 0)
        }
        set(\.global, why == nil ? (globalPending.value ?? cat.global.config) : nil)
        set(\.pttState, cat.global.pttState)
        set(\.aiActive, cat.global.aiActive)
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
    }

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
            wanting = true
            records?.want(StationCat.logStream, backlog: StationCat.logCapacity, by: self)
        } else if wanting {
            wanting = false
            records?.unwant(StationCat.logStream, by: self)
        }
        let all = logs ? (records?.records(StationCat.logStream) ?? []).map(StationCat.LogLine.init(record:)) : []
        let newest = all.compactMap { Int($0.id) }.max() ?? 0
        if let upper = dropped.map(\.upperBound).max(), newest < upper {
            // The Core's log started again: what was dropped is gone with it.
            dropped = []
        }
        let lines = all.filter { line in
            let number = Int(line.id) ?? 0
            if let pausedAt, number > pausedAt {
                return false
            }
            if dropped.contains(where: { $0.contains(number) }) {
                return false
            }
            switch logFilter {
            case .all: return true
            case .received: return line.inbound
            case .sent: return !line.inbound
            }
        }
        if logLines != lines {
            logLines = lines
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
        if shown {
            switch route {
            case .channel, .ptt: refreshDevices()
            case .options, .test: break
            }
        }
        refresh()
    }

    private func refreshDevices() {
        guard reason == nil, let commands else {
            return
        }
        Task {
            do {
                _ = try await commands.invoke(StationCat.refreshDevicesVerb, arguments: [], timeout: Self.timeout)
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
    /// the closed one does nothing.
    func pickSlice(_ number: Int, _ vfo: Vfo, _ choice: String) {
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
    func change(_ number: Int, primaryRebind: Bool = false, secondaryRebind: Bool = false,
                _ edit: (inout StationCat.ChannelConfig) -> Void) {
        guard let arguments = prepare(number, primaryRebind: primaryRebind, secondaryRebind: secondaryRebind, edit)
        else {
            return
        }
        Task { _ = await send(.channel(number), StationCat.setChannelVerb, arguments) }
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
                                guard let self, let arguments = self.prepare(number, primaryRebind: false,
                                                                             secondaryRebind: false, { config in
                                    switch listener {
                                    case .tcp: config.tcpPort = next
                                    case .rigctld: config.rigctldPort = next
                                    }
                                }) else {
                                    return nil
                                }
                                return await self.send(.channel(number), StationCat.setChannelVerb, arguments)
                            }, close: { [weak self] in self?.pad = nil })
    }

    private func prepare(_ number: Int, primaryRebind: Bool, secondaryRebind: Bool,
                         _ edit: (inout StationCat.ChannelConfig) -> Void) -> [CommandArgument]? {
        guard reason == nil, StationCat.channels.contains(number), let pending = channelPending[number] else {
            return nil
        }
        var next = channel(number).config
        edit(&next)
        pending.choose(next)
        publish(number, next)
        return [
            CommandArgument(name: "channel", value: .int(Int64(number))),
            CommandArgument(name: "config", value: .text(next.commandText(primaryRebind: primaryRebind,
                                                                           secondaryRebind: secondaryRebind))),
        ]
    }

    private func publish(_ number: Int, _ config: StationCat.ChannelConfig) {
        guard let index = channels.firstIndex(where: { $0.number == number }), channels[index].config != config else {
            return
        }
        channels[index].config = config
    }

    // MARK: Changing the shared settings

    /// Changes the settings every channel shares: the whole config as shown, with `edit` over it.
    func changeGlobal(_ edit: (inout StationCat.GlobalConfig) -> Void) {
        guard let arguments = prepareGlobal(edit) else {
            return
        }
        Task { _ = await send(.global, StationCat.setGlobalVerb, arguments) }
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
                                guard let self, let arguments = self.prepareGlobal({ config in
                                    if digu {
                                        config.rttyDiguHz = next
                                    } else {
                                        config.rttyDiglHz = next
                                    }
                                }) else {
                                    return nil
                                }
                                return await self.send(.global, StationCat.setGlobalVerb, arguments)
                            }, close: { [weak self] in self?.pad = nil })
    }

    private func prepareGlobal(_ edit: (inout StationCat.GlobalConfig) -> Void) -> [CommandArgument]? {
        guard reason == nil, var next = global else {
            return nil
        }
        edit(&next)
        globalPending.choose(next)
        if global != next {
            global = next
        }
        return [CommandArgument(name: "config", value: .text(next.text))]
    }

    // MARK: Sending

    /// Sends one change after the slot's earlier ones, and notes the Core's answer.
    private func send(_ slot: Slot, _ verb: String, _ arguments: [CommandArgument]) async -> PropertyWriteOutcome {
        guard let commands else {
            drop(slot)
            refresh()
            return .notSent
        }
        let previous = chains[slot]
        inFlight[slot, default: 0] += 1
        let job = Task { @MainActor () -> PropertyWriteOutcome in
            await previous?.value
            do {
                let result = try await commands.invoke(verb, arguments: arguments, timeout: Self.timeout)
                return PropertyWriteOutcome(.success(result))
            } catch let error as CommandError {
                Self.logger.info("A CAT change had no answer from the Core")
                return PropertyWriteOutcome(.failure(error))
            } catch {
                return .notSent
            }
        }
        chains[slot] = Task { _ = await job.value }
        let outcome = await job.value
        inFlight[slot, default: 1] -= 1
        if outcome.accepted {
            notes[slot] = nil
        } else if let text = outcome.noteText(refused: Self.refusedText) {
            notes[slot] = text
        }
        if !outcome.accepted && (inFlight[slot] ?? 0) == 0 {
            // Refused or unanswered: show the Core's value again.
            drop(slot)
        }
        refresh()
        return outcome
    }

    private func drop(_ slot: Slot) {
        switch slot {
        case .channel(let number): channelPending[number]?.drop()
        case .global: globalPending.drop()
        }
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
        Task { [weak self] in
            var answer = Self.unansweredText
            var refused = false
            do {
                let result = try await commands.invoke(StationCat.testVerb, arguments: [
                    CommandArgument(name: "requestId", value: .int(id)),
                    CommandArgument(name: "channel", value: .int(Int64(channel))),
                    CommandArgument(name: "command", value: .text(text)),
                ], timeout: Self.timeout)
                if result.accepted {
                    var reply = ""
                    if case .text(let value)? = result.values["reply"] {
                        reply = value
                    }
                    answer = reply.isEmpty ? Self.noReplyText : Self.escaped(reply)
                } else {
                    answer = result.reason.isEmpty ? Self.refusedText : result.reason
                    refused = true
                }
            } catch {
                Self.logger.info("A CAT test had no answer from the Core")
            }
            guard let self, let index = self.tests.firstIndex(where: { $0.id == id }) else {
                return
            }
            self.tests[index].answer = answer
            self.tests[index].refused = refused
        }
    }

    // MARK: The log

    func setLogFilter(_ filter: LogFilter) {
        guard logFilter != filter else {
            return
        }
        logFilter = filter
        refresh()
    }

    /// Pauses the log or resumes it. As the desktop's window does, lines that
    /// arrive while paused are dropped: Resume starts with what comes next.
    func setPaused(_ on: Bool) {
        guard paused != on else {
            return
        }
        let newest = (records?.records(StationCat.logStream) ?? []).compactMap { Int($0.id) }.max() ?? 0
        if on {
            pausedAt = newest
        } else {
            if let pausedAt, newest > pausedAt {
                dropped.append((pausedAt + 1)...newest)
            }
            pausedAt = nil
        }
        paused = on
        refresh()
    }

    /// A log line as the page shows it: when, which channel, which way, and
    /// its bytes, each one outside printable text written as `\xNN`.
    static func lineText(_ line: StationCat.LogLine) -> String {
        let way = line.inbound ? "in " : "out"
        return "CAT\(line.channel) \(way)  \(escaped(line.text))"
    }

    /// The time the Core logged a line, on this phone's clock face.
    static func timeText(_ line: StationCat.LogLine) -> String {
        guard let ms = line.timeMs else {
            return ""
        }
        return timeFormatter.string(from: Date(timeIntervalSince1970: Double(ms) / 1000))
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
