// NereusSDR for iOS: the Core's antenna rotor as the Rotor page shows it, and the commands that turn it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusMirror
import os

/// The Core's antenna rotor (remote rotor control version 1): the `rotor`
/// object the Core mirrors at `remoteRotorControlVersion` 1, and its eight
/// commands. The Core owns the rotor; this phone reads the object and asks
/// the Core to act.
///
/// The phone's touch rule (design, Touch safety): a drag on the dial only
/// selects a heading, drawn in the target colour, and sends nothing; Turn
/// sends that selection once as `setRotorTarget`. A selection not sent
/// within ``selectionLapse`` is dropped. A sent target is the Core's from
/// then on and never lapses here. Presets, Stop and the nudge holds are one
/// tap. A held nudge repeats `nudgeRotor` every ``holdRepeat`` (the Core
/// stops the rotor when the repeats lapse for 750 ms); leaving the page or
/// the app going to the background ends the hold.
///
/// Every control that cannot act now stays shown and greyed with its
/// reason: no Core, a Core too old to control a rotor, no rotor set up, or
/// the rotor not connected.
@MainActor
final class RotorModel: ObservableObject {
    /// `driver` (document, Enum tables).
    enum Driver: Int64, CaseIterable, Sendable {
        case none = 0
        case gs232a
        case gs232b
        case rotctldRunning
        case rotctldStarted

        /// The setup menu's words.
        var title: String {
            switch self {
            case .none: return "None"
            case .gs232a: return "GS-232A on a serial port"
            case .gs232b: return "GS-232B on a serial port"
            case .rotctldRunning: return "Hamlib rotctld already running"
            case .rotctldStarted: return "Hamlib rotctld started by the Core"
            }
        }

        /// The driver talks to a serial port on the Core's computer.
        var usesSerialPort: Bool { self == .gs232a || self == .gs232b || self == .rotctldStarted }
    }

    /// `axes`.
    enum Axes: Int64, CaseIterable, Sendable {
        case azimuth = 0
        case azimuthElevation

        var title: String { self == .azimuth ? "Azimuth" : "Azimuth and elevation" }
    }

    /// `endStop`.
    enum EndStop: Int64, CaseIterable, Sendable {
        case none = 0
        case north
        case south

        var title: String {
            switch self {
            case .none: return "None (turns all the way round)"
            case .north: return "North"
            case .south: return "South"
            }
        }
    }

    /// `motion`.
    enum Motion: Int64, Sendable {
        case stopped = 0
        case turning
        case nudging
    }

    /// `nudgeRotor`'s `direction`.
    enum Direction: Int64, Sendable {
        case counterClockwise = 0
        case clockwise
        case down
        case up

        var isElevation: Bool { self == .down || self == .up }
    }

    /// One preset: its name and compass heading.
    struct Preset: Equatable, Identifiable, Sendable {
        let id: Int
        let name: String
        let degrees: Double
    }

    /// The `rotor` object as the Core last sent it.
    struct State: Equatable {
        var phase = AccessoriesModel.Phase.disabled
        var connectionError = ""
        var driver = Driver.none
        var label = ""
        var serialPort = ""
        var baud: Int64 = 9600
        var host = ""
        var port: Int64 = 4533
        var serialPorts: [String] = []
        var axes = Axes.azimuth
        var rangeDeg: Int64 = 360
        var endStop = EndStop.north
        var spanPositionDeg = -1.0
        var travelDeg = 0.0
        var routeKnown = false
        var offsetDeg = 0.0
        var hamlibModel: Int64 = 0
        var rotctldAvailable = false
        var positionFresh = false
        var azimuthDeg = -1.0
        var elevationDeg = -1.0
        var targetAzimuthDeg = -1.0
        var targetElevationDeg = -1.0
        var motion = Motion.stopped
        var presets: [Preset] = []
        var fault = ""

        var connected: Bool { phase == .connected }
        var heading: Double? { azimuthDeg >= 0 ? azimuthDeg : nil }
        var elevation: Double? { axes == .azimuthElevation && elevationDeg >= 0 ? elevationDeg : nil }
        var target: Double? { targetAzimuthDeg >= 0 ? targetAzimuthDeg : nil }
        var targetElevation: Double? { targetElevationDeg >= 0 ? targetElevationDeg : nil }
    }

    /// What `configureRotor` sends: Setup's Rotor page's values.
    struct Setup: Equatable {
        var driver = Driver.gs232b
        var serialPort = ""
        var baud: Int64 = 9600
        var host = ""
        var port: Int64 = 4533
        var hamlibModel: Int64 = 404
        var axes = Axes.azimuth
        var endStop = EndStop.north
        var rangeDeg: Int64 = 360
        var offsetDeg = 0.0

        init() {}

        /// The setup the Core reports, as Setup's Rotor page starts.
        init(_ state: State) {
            driver = state.driver == .none ? .gs232b : state.driver
            serialPort = state.serialPort.isEmpty ? (state.serialPorts.first ?? "") : state.serialPort
            baud = state.baud > 0 ? state.baud : 9600
            host = state.host
            port = state.port > 0 ? state.port : 4533
            // Hamlib's own ERC driver, model 404, as the setup offers first.
            hamlibModel = state.hamlibModel > 0 ? state.hamlibModel : 404
            axes = state.axes
            endStop = state.endStop
            rangeDeg = state.rangeDeg == 450 ? 450 : 360
            offsetDeg = state.offsetDeg
        }

        var arguments: [CommandArgument] {
            [CommandArgument(name: "driver", value: .enumeration(driver.rawValue)),
             CommandArgument(name: "serialPort", value: .text(driver.usesSerialPort ? serialPort : "")),
             CommandArgument(name: "baud", value: .int(baud)),
             CommandArgument(name: "host", value: .text(driver == .rotctldRunning ? host : "")),
             CommandArgument(name: "port", value: .int(port)),
             CommandArgument(name: "hamlibModel", value: .int(driver == .rotctldStarted ? hamlibModel : 0)),
             CommandArgument(name: "axes", value: .enumeration(axes.rawValue)),
             CommandArgument(name: "endStop", value: .enumeration(endStop.rawValue)),
             CommandArgument(name: "rangeDeg", value: .int(rangeDeg)),
             CommandArgument(name: "offsetDeg", value: .double(offsetDeg))]
        }
    }

    // MARK: Words

    static let olderCoreReason = StationToolList.rotorOlderCoreReason
    static let noRotorReason = StationToolList.noRotorReason
    static let notConnectedReason = "The rotor is not connected."
    static let azimuthOnlyReason = "This rotor turns in azimuth only."
    static let coreNotConnectedReason = StationToolList.notConnectedReason
    /// A spot whose bearing the Core does not know (`bearingDeg` -1: no grid
    /// square at the Core, or a callsign it cannot place). `turnRotorToCall`
    /// would be refused for the same reasons, so Turn beam stays greyed.
    static let noBearingReason = "The Core has no bearing for this spot. It needs your grid square and a callsign it can place."

    // MARK: Timing (this design's choices, not device facts)

    /// How long a selection the operator has not sent stays (design, Touch safety).
    static let selectionLapse: Duration = .seconds(15)
    /// How often a held nudge repeats (document, The hold dead man).
    static let holdRepeat: Duration = .milliseconds(250)
    /// How far from the target counts as there (the Core's arrival rule).
    nonisolated static let arrivedDeg = 1.5
    /// A route longer than this is the long way round, as the Core's route
    /// planner and the desktop readout say it (RotorRoute::kLongWayDeg).
    nonisolated static let longWayDeg = 270.0
    /// The Core marks the heading stale after this long without a reply.
    static let staleAfterMs: Int64 = 1_500
    /// How often Setup's Rotor page asks the Core to keep reading its
    /// serial ports while on screen (document, `refreshRotorPorts`; the Core's
    /// lease is 30 s).
    static let setupAskRepeat: Duration = .seconds(20)

    static let objectKey = "rotor"
    static let capability = "remoteRotorControlVersion"

    // MARK: State

    /// The Core's rotor; nil while the Core sends no `rotor` object.
    @Published private(set) var state: State?
    /// The Core's `remoteRotorControlVersion`; 0 when it does not advertise it.
    @Published private(set) var version: Int64 = 0
    /// The Core is connected and its snapshot current.
    @Published private(set) var coreConnected = false
    /// The heading the operator dragged to and has not sent; nil when none.
    @Published private(set) var selection: Double?
    /// A target this phone sent that the Core has not answered yet.
    @Published private(set) var awaitingTarget: Double?
    /// The nudge button held now; nil when none.
    @Published private(set) var holding: Direction?
    /// Presets and a reversed selection take the long path.
    @Published private(set) var longPath = false
    /// The Core's refusal or the phone's words for no answer; a tap clears it.
    @Published private(set) var note: String?
    /// When the heading went stale, on this model's clock; nil while it is fresh.
    @Published private(set) var staleSinceMs: Int64?

    let clock: any LinkClock
    private let mirror: MirrorStore
    private let commands: CommandClient?
    private var watch: ToolMirrorWatch?
    private var lapseTimer: (any LinkTimer)?
    private var holdTimer: (any LinkTimer)?
    private var holdGeneration: UInt64 = 0
    private var setupAskTimer: (any LinkTimer)?
    private var setupAskGeneration: UInt64 = 0
    private var selectionGeneration: UInt64 = 0
    private var targetGeneration: UInt64 = 0
    private let outcomes = ControlOutcomeOwner()
    static let logger = Logger(subsystem: "NereusSDR", category: "rotor")

    init(mirror: MirrorStore, commands: CommandClient?, clock: any LinkClock = SystemLinkClock()) {
        self.mirror = mirror
        self.commands = commands
        self.clock = clock
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.refresh() }
        self.watch = watch
        refresh()
    }

    // MARK: Why a control cannot act

    /// Why the turn controls (the dial, Turn, presets, nudges, Stop) cannot act now, or nil.
    var turnReason: String? {
        if !coreConnected {
            return Self.coreNotConnectedReason
        }
        guard version >= 1, let state else {
            return Self.olderCoreReason
        }
        if state.driver == .none {
            return Self.noRotorReason
        }
        return state.connected ? nil : Self.notConnectedReason
    }

    /// Why Up and Down cannot act now, or nil.
    var elevationReason: String? {
        if let reason = turnReason {
            return reason
        }
        return state?.axes == .azimuthElevation ? nil : Self.azimuthOnlyReason
    }

    /// Why Turn beam on a spot cannot act now, or nil: the rotor's reason
    /// first, then a bearing the Core does not know.
    func beamReason(bearing: Double?) -> String? {
        if let reason = turnReason {
            return reason
        }
        return bearing.flatMap(Self.strict) == nil ? Self.noBearingReason : nil
    }

    /// Why the rotor setup cannot be changed now, or nil.
    var setupReason: String? {
        if !coreConnected {
            return Self.coreNotConnectedReason
        }
        return version >= 1 && state != nil ? nil : Self.olderCoreReason
    }

    // MARK: What the dial shows

    /// The target the dial draws: the selection, else a target sent and not
    /// yet answered, else the Core's target.
    var dialTarget: Double? {
        selection ?? awaitingTarget ?? state?.target
    }

    /// The signed turn to the dial's target along the route the controller
    /// will take (negative is counter-clockwise); nil when the route is not
    /// known or there is no target.
    var dialTravel: Double? {
        guard let state, let heading = state.heading else {
            return nil
        }
        if let local = selection ?? awaitingTarget {
            return RotorRoute.travel(heading: heading, spanDeg: state.spanPositionDeg, target: local,
                                     endStop: state.endStop, rangeDeg: state.rangeDeg,
                                     offsetDeg: state.offsetDeg)
        }
        guard state.target != nil, state.routeKnown else {
            return nil
        }
        return state.travelDeg
    }

    /// The rotor is at the Core's target and still.
    var arrived: Bool {
        guard selection == nil, awaitingTarget == nil, let state, let heading = state.heading,
              let target = state.target, state.motion == .stopped else {
            return false
        }
        return abs(RotorRoute.shortest(from: heading, to: target)) < Self.arrivedDeg
    }

    /// The presets as the Core lists them.
    var presets: [Preset] { state?.presets ?? [] }

    // MARK: The operator

    /// A drag on the dial at compass `azimuth`: selects the whole degree
    /// nearest it, drawn in the target colour. Sends nothing.
    func select(azimuth: Double) {
        guard turnReason == nil, azimuth.isFinite else {
            return
        }
        setSelection(Self.compass(azimuth.rounded()))
    }

    /// Turn: sends the selection once as `setRotorTarget`. The target is
    /// the Core's from then on.
    func turn() {
        guard let selection, allowed(turnReason) else {
            return
        }
        dropSelection()
        sendTarget(selection)
    }

    /// A preset chip: turns to it at once (one tap), the long way when the
    /// long path is chosen.
    func turn(to preset: Preset) {
        guard allowed(turnReason), let heading = Self.strict(preset.degrees) else {
            return
        }
        dropSelection()
        sendTarget(longPath ? Self.compass(heading + 180) : heading)
    }

    /// Turn beam on a spot: turns to its bearing at once (one tap), sent as
    /// `setRotorTarget` like a preset. An unsent selection on the dial goes.
    func turnBeam(toBearing bearing: Double?) {
        guard allowed(beamReason(bearing: bearing)), let heading = bearing.flatMap(Self.strict) else {
            return
        }
        dropSelection()
        sendTarget(heading)
    }

    /// Short path or long path. Changing it with a selection, or with a
    /// target set, selects the other way round, still to be sent with Turn.
    func setLongPath(_ on: Bool) {
        guard on != longPath else {
            return
        }
        longPath = on
        guard turnReason == nil, let target = selection ?? awaitingTarget ?? state?.target else {
            return
        }
        setSelection(Self.compass(target + 180))
    }

    /// Stop: sent at once (one tap). A held nudge and a selection end with it.
    func stop() {
        guard allowed(turnReason) else {
            return
        }
        cancelHold()
        dropSelection()
        send("stopRotor", [])
    }

    /// A nudge button pressed: starts the hold, repeated every
    /// ``holdRepeat`` until ``endHold()``.
    func startHold(_ direction: Direction) {
        guard holding != direction else {
            return
        }
        endHold()
        guard allowed(direction.isElevation ? elevationReason : turnReason) else {
            return
        }
        dropSelection()
        holding = direction
        holdGeneration &+= 1
        let generation = holdGeneration
        let arguments = Self.nudgeArguments(direction, active: true)
        Task { [weak self] in
            guard let self, let result = await self.invoke("nudgeRotor", arguments) else {
                return
            }
            // A refused start ends the hold here; the Core never started it.
            if !result.accepted, self.holdGeneration == generation {
                self.cancelHold()
            }
        }
        scheduleHoldRepeat(generation)
    }

    /// The nudge button let go: ends the hold with `active` false.
    func endHold() {
        guard let direction = holding else {
            return
        }
        cancelHold()
        send("nudgeRotor", Self.nudgeArguments(direction, active: false))
    }

    /// The page went off screen: the hold ends and an unsent selection goes.
    func pageLeft() {
        endHold()
        dropSelection()
    }

    /// The app left the foreground: the hold ends.
    func sceneLeft() {
        endHold()
    }

    /// Saves the setup at the Core and (re)connects (`configureRotor`).
    func configure(_ setup: Setup) {
        guard allowed(setupReason) else {
            return
        }
        send("configureRotor", setup.arguments)
    }

    /// Setup's Rotor page came on screen: the Core reads its serial ports
    /// and looks for rotctld only while a rotor is set up or a setup view
    /// asks (`refreshRotorPorts`), so the page asks now and every 20 s while
    /// it stays. Quiet: nothing shows if the Core cannot be asked.
    func setupShown() {
        setupAskGeneration &+= 1
        askForPorts(setupAskGeneration)
    }

    /// Setup's Rotor page left the screen: no more asks.
    func setupHidden() {
        setupAskTimer?.cancel()
        setupAskTimer = nil
        setupAskGeneration &+= 1
    }

    private func askForPorts(_ generation: UInt64) {
        guard generation == setupAskGeneration else {
            return
        }
        if setupReason == nil, let commands {
            Task {
                _ = try? await commands.invoke("refreshRotorPorts", arguments: [], timeout: .seconds(5))
            }
        }
        setupAskTimer = clock.schedule(after: Self.setupAskRepeat) { [weak self] in
            await self?.askForPorts(generation)
        }
    }

    /// Replaces the Core's presets (`setRotorPresets`) with `presets`, one
    /// `name<TAB>degrees` per line in order (``RotorPresetsDraft/serialized()``).
    /// True once the Core accepts them; a refusal shows as the note.
    @discardableResult
    func savePresets(_ presets: String) async -> Bool {
        guard allowed(setupReason) else {
            return false
        }
        let result = await invoke("setRotorPresets", [CommandArgument(name: "presets", value: .text(presets))])
        return result?.accepted == true
    }

    /// Disconnects, keeping the setup (`disconnectRotor`).
    func disconnect() {
        guard allowed(setupReason) else {
            return
        }
        send("disconnectRotor", [])
    }

    /// Clears the note on the page.
    func dismissNote() {
        _ = outcomes.begin()
        note = nil
    }

    // MARK: Sending

    private func allowed(_ reason: String?) -> Bool {
        guard let reason else {
            return true
        }
        _ = outcomes.begin()
        note = reason
        return false
    }

    private func setSelection(_ heading: Double) {
        selection = heading
        lapseTimer?.cancel()
        selectionGeneration &+= 1
        let generation = selectionGeneration
        lapseTimer = clock.schedule(after: Self.selectionLapse) { [weak self] in
            await self?.lapse(generation)
        }
    }

    private func lapse(_ generation: UInt64) {
        guard generation == selectionGeneration else {
            return
        }
        dropSelection()
    }

    private func dropSelection() {
        lapseTimer?.cancel()
        lapseTimer = nil
        selectionGeneration &+= 1
        if selection != nil {
            selection = nil
        }
    }

    private func sendTarget(_ heading: Double) {
        targetGeneration &+= 1
        let generation = targetGeneration
        awaitingTarget = heading
        let arguments = [CommandArgument(name: "azimuthDeg", value: .double(heading)),
                         CommandArgument(name: "elevationDeg", value: .double(-1))]
        Task { [weak self] in
            _ = await self?.invoke("setRotorTarget", arguments)
            guard let self, self.targetGeneration == generation else {
                return
            }
            self.awaitingTarget = nil
        }
    }

    private func scheduleHoldRepeat(_ generation: UInt64) {
        holdTimer = clock.schedule(after: Self.holdRepeat) { [weak self] in
            await self?.repeatHold(generation)
        }
    }

    private func repeatHold(_ generation: UInt64) {
        guard generation == holdGeneration, let direction = holding else {
            return
        }
        guard turnReason == nil else {
            // The Core went away or the rotor dropped: the Core stops it; nothing more to send.
            cancelHold()
            return
        }
        if let commands {
            let arguments = Self.nudgeArguments(direction, active: true)
            Task {
                do {
                    try await commands.post("nudgeRotor", arguments: arguments)
                } catch {
                    Self.logger.info("A held nudge repeat was not sent")
                }
            }
        }
        scheduleHoldRepeat(generation)
    }

    private func cancelHold() {
        holdTimer?.cancel()
        holdTimer = nil
        holdGeneration &+= 1
        if holding != nil {
            holding = nil
        }
    }

    private func send(_ verb: String, _ arguments: [CommandArgument]) {
        Task { [weak self] in
            _ = await self?.invoke(verb, arguments)
        }
    }

    /// Sends `verb` and shows its refusal or missing answer; nil when no answer came.
    private func invoke(_ verb: String, _ arguments: [CommandArgument]) async -> CommandResult? {
        let edit = outcomes.begin()
        guard let commands else {
            note = PropertyWriteOutcome.notSent.reason
            return nil
        }
        let outcome: PropertyWriteOutcome
        var answer: CommandResult?
        do {
            let result = try await commands.invoke(verb, arguments: arguments, timeout: .seconds(5))
            answer = result
            outcome = PropertyWriteOutcome(.success(result))
        } catch let error as CommandError {
            outcome = PropertyWriteOutcome(.failure(error))
        } catch {
            Self.logger.info("A \(verb, privacy: .public) request had no answer from the Core")
            return nil
        }
        if outcomes.isCurrent(edit) {
            note = outcome.noteText(refused: BandSlicesModel.refusedText)
        }
        return answer
    }

    nonisolated static func nudgeArguments(_ direction: Direction, active: Bool) -> [CommandArgument] {
        [CommandArgument(name: "direction", value: .enumeration(direction.rawValue)),
         CommandArgument(name: "active", value: .bool(active))]
    }

    // MARK: Headings

    /// A compass heading 0 to under 360 for any finite angle.
    nonisolated static func compass(_ degrees: Double) -> Double {
        let value = degrees.truncatingRemainder(dividingBy: 360)
        let wrapped = value < 0 ? value + 360 : value
        return wrapped >= 360 ? 0 : wrapped
    }

    /// The strict heading rule (document, Strict headings): a number from
    /// 0 to 360, never wrapped; 360 is north and reads 0. Anything else is nil.
    nonisolated static func strict(_ degrees: Double) -> Double? {
        guard degrees.isFinite, degrees >= 0, degrees <= 360 else {
            return nil
        }
        return degrees == 360 ? 0 : degrees
    }

    /// The presets property: one per line, `name<TAB>degrees`, in order.
    /// A line that cannot be read is left out.
    nonisolated static func presets(_ text: String) -> [Preset] {
        var presets: [Preset] = []
        for line in text.split(whereSeparator: \.isNewline) {
            let parts = line.split(separator: "\t", maxSplits: 1, omittingEmptySubsequences: false)
            guard parts.count == 2, let value = Double(parts[1].trimmingCharacters(in: .whitespaces)),
                  let degrees = strict(value) else {
                continue
            }
            let name = parts[0].trimmingCharacters(in: .whitespaces)
            presets.append(Preset(id: presets.count, name: name, degrees: degrees))
        }
        return presets
    }

    // MARK: Words for a heading

    /// "047°", or "---°" with no heading.
    nonisolated static func headingText(_ degrees: Double?) -> String {
        guard let degrees else {
            return "---\u{00B0}"
        }
        let whole = Int(compass(degrees.rounded()))
        return String(format: "%03d\u{00B0}", whole)
    }

    /// "73° to go", the long way round said, or "on target".
    nonisolated static func toGoText(_ travel: Double) -> String {
        let magnitude = Int(abs(travel).rounded())
        if abs(travel) < arrivedDeg {
            return "on target"
        }
        return abs(travel) > longWayDeg ? "\(magnitude)\u{00B0} to go, the long way round" : "\(magnitude)\u{00B0} to go"
    }

    /// The line under the heading.
    var targetLine: String {
        if let selection {
            return "\(Self.headingText(selection)) selected \u{00B7} tap Turn within 15 s"
        }
        guard let target = awaitingTarget ?? state?.target else {
            return "No target"
        }
        let head = Self.headingText(target)
        if arrived {
            return "\(head) \u{00B7} on target"
        }
        guard let travel = dialTravel else {
            return "\(head) \u{00B7} the route shows once the rotor moves"
        }
        return "\(head) \u{00B7} \(Self.toGoText(travel))"
    }

    /// The elevation line on an az/el rotor; nil on an azimuth rotor.
    var elevationLine: String? {
        guard let state, state.axes == .azimuthElevation else {
            return nil
        }
        var line = "El " + (state.elevation.map { "\(Int($0.rounded()))\u{00B0}" } ?? "--\u{00B0}")
        if let target = state.targetElevation {
            line += " \u{00B7} to \(Int(target.rounded()))\u{00B0}"
        }
        return line
    }

    /// "Last heard 5 s ago" while the heading is stale; nil while it is live.
    func staleText(nowMs: Int64) -> String? {
        guard let staleSinceMs, state?.heading != nil else {
            return nil
        }
        let seconds = max(Int((nowMs - staleSinceMs + Self.staleAfterMs) / 1000), 1)
        return "Last heard \(seconds) s ago"
    }

    /// The status line: the rotor's label and what it is doing, or where it stands.
    var statusLine: String {
        if let reason = turnReason, reason != Self.notConnectedReason {
            return reason
        }
        guard let state else {
            return Self.olderCoreReason
        }
        let name = state.label.isEmpty ? "Rotor" : state.label
        if let words = AccessoryStatusLine.phaseWords(AccessoriesModel.Link(phase: state.phase)) {
            return "\(name) \u{00B7} \(words)"
        }
        let doing: String
        switch state.motion {
        case .stopped: doing = "Stopped"
        case .turning: doing = "Turning"
        case .nudging: doing = "Turning by hand"
        }
        return "\(name) \u{00B7} \(doing)"
    }

    /// The Tools row's line: the heading and where it is turning, or why it cannot open.
    var toolLine: String {
        if let reason = turnReason {
            return reason
        }
        var line = Self.headingText(state?.heading)
        if state?.motion == .turning, let target = state?.target {
            line += " \u{00B7} turning to \(Self.headingText(target))"
        }
        return line
    }

    // MARK: Reading

    func refresh() {
        let connected = mirror.isSnapshotComplete && !mirror.isStale
        set(\.coreConnected, connected)
        set(\.version, mirror.capabilityVersion(Self.capability))
        let next = watch?.object(Self.objectKey).map(Self.state)
        set(\.state, next)
        if let next, !next.positionFresh {
            if staleSinceMs == nil {
                staleSinceMs = clock.nowMilliseconds
            }
        } else if staleSinceMs != nil {
            staleSinceMs = nil
        }
        if holding != nil, turnReason != nil {
            cancelHold()
        }
        if selection != nil, turnReason != nil {
            dropSelection()
        }
    }

    static func state(_ object: MirrorObject) -> State {
        var state = State()
        state.phase = AccessoriesModel.Phase(rawValue: ToolValue.whole(object["connectionPhase"]) ?? 0) ?? .disabled
        state.connectionError = ToolValue.text(object["connectionError"]) ?? ""
        state.driver = Driver(rawValue: ToolValue.whole(object["driver"]) ?? 0) ?? .none
        state.label = ToolValue.text(object["label"]) ?? ""
        state.serialPort = ToolValue.text(object["serialPort"]) ?? ""
        state.baud = ToolValue.whole(object["baud"]) ?? 9600
        state.host = ToolValue.text(object["host"]) ?? ""
        state.port = ToolValue.whole(object["port"]) ?? 4533
        state.serialPorts = (ToolValue.text(object["serialPorts"]) ?? "")
            .split(whereSeparator: \.isNewline).map(String.init).filter { !$0.isEmpty }
        state.axes = Axes(rawValue: ToolValue.whole(object["axes"]) ?? 0) ?? .azimuth
        state.rangeDeg = ToolValue.whole(object["rangeDeg"]) ?? 360
        state.endStop = EndStop(rawValue: ToolValue.whole(object["endStop"]) ?? 1) ?? .north
        state.spanPositionDeg = ToolValue.number(object["spanPositionDeg"]) ?? -1
        state.travelDeg = ToolValue.number(object["travelDeg"]) ?? 0
        state.routeKnown = ToolValue.flag(object["routeKnown"]) ?? false
        state.offsetDeg = ToolValue.number(object["offsetDeg"]) ?? 0
        state.hamlibModel = ToolValue.whole(object["hamlibModel"]) ?? 0
        state.rotctldAvailable = ToolValue.flag(object["rotctldAvailable"]) ?? false
        state.positionFresh = ToolValue.flag(object["positionFresh"]) ?? false
        state.azimuthDeg = ToolValue.number(object["azimuthDeg"]) ?? -1
        state.elevationDeg = ToolValue.number(object["elevationDeg"]) ?? -1
        state.targetAzimuthDeg = ToolValue.number(object["targetAzimuthDeg"]) ?? -1
        state.targetElevationDeg = ToolValue.number(object["targetElevationDeg"]) ?? -1
        state.motion = Motion(rawValue: ToolValue.whole(object["motion"]) ?? 0) ?? .stopped
        state.presets = presets(ToolValue.text(object["presets"]) ?? "")
        state.fault = ToolValue.text(object["fault"]) ?? ""
        return state
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<RotorModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }
}

/// The route the rotor will take to a compass heading (design, End stops
/// and overlap): on a rotor with an end stop the span runs clockwise from
/// the counter-clockwise stop, 0 to its range, and compass heading =
/// (stop + span) modulo 360. A heading in the overlap has two span
/// positions; the controller takes the nearer, as JJ's ERC was seen to do.
/// With no end stop the route is the shorter way.
enum RotorRoute {
    /// The signed shortest turn from `from` to `to`, -180 to under 180.
    static func shortest(from: Double, to: Double) -> Double {
        let difference = (to - from + 540).truncatingRemainder(dividingBy: 360)
        return (difference < 0 ? difference + 360 : difference) - 180
    }

    /// The signed travel to `target` from span position `spanDeg` (-1 when
    /// not known); nil when the route cannot be known. `heading` and
    /// `target` are as shown, after the calibration offset; the span is the
    /// controller's own reading (the Core's `spanPositionDeg`), so the
    /// target is planned with the offset removed, as the Core sends it.
    static func travel(heading: Double, spanDeg: Double, target: Double, endStop: RotorModel.EndStop,
                       rangeDeg: Int64, offsetDeg: Double = 0) -> Double? {
        if endStop == .none {
            return shortest(from: heading, to: target)
        }
        guard spanDeg >= 0 else {
            return nil
        }
        let stop: Double = endStop == .north ? 0 : 180
        let base = RotorModel.compass(target - offsetDeg - stop)
        let range = Double(rangeDeg)
        let candidates = [base, base + 360].filter { $0 <= range + 1e-9 }
        guard let best = candidates.min(by: { abs($0 - spanDeg) < abs($1 - spanDeg) }) else {
            return nil
        }
        return best - spanDeg
    }
}
