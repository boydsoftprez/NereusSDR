// NereusSDR for iOS: the Core's TX equalizer as the desktop's editor offers it, read and written on its transmit settings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusMirror
import os

/// The TX Equalizer page's model (spec section 5.2 item 4, R-IOS-18): the
/// Core's transmit equalizer on its `transmit` object (link document
/// section 7.1, `transmit` at `transmitSettingsVersion` 2 and 4), which the
/// desktop's TX EQ dialog edits: the switch, Legacy EQ, the preamp, the ten
/// bands' gains and centres, the filter's size, minimum phase, cutoff mode
/// and window, the TX profile the EQ is saved with, and the parametric
/// curve (`txEqCurve`, changed through `txEq.setCurve` and
/// `txEq.resetCurve` at `txEqCurveVersion` 2; plan Task 59a).
///
/// Each setting changes while the Core takes it at its version, as the
/// desktop's own controls do: at `transmitSettingsVersion` 13 and later on
/// the air too, below 13 only off the air, and at every version only while
/// no other device holds transmit. Otherwise it stays shown, greyed, with
/// the reason.
///
/// The curve drawn is always the Core's: each change sends the whole curve
/// the Core last sent with only the changed value different, and the page
/// then shows the curve the Core answers with. A change from another device
/// redraws the page, says so, and closes a curve number pad without sending it.
@MainActor
final class TxEqualizerModel: ObservableObject {
    // MARK: The Core's ranges (link document section 7.1)

    static let bandCount = 10
    static let bandRange: ClosedRange<Int> = -12...15
    static let preampRange: ClosedRange<Int> = -12...15
    static let frequencyRange: ClosedRange<Int64> = 10...22000
    static let sizeRange: ClosedRange<Int64> = 32...8192
    /// `txEqEnabled` came with version 2, the TX profile with 3, the rest with 4.
    static let switchVersion: Int64 = 2
    static let profileVersion: Int64 = 3
    static let editorVersion: Int64 = 4
    /// From this `transmitSettingsVersion` the Core takes the transmit
    /// settings on the air, as a local window does (link document section
    /// 7.3, "Version 13").
    static let onAirVersion: Int64 = 13

    /// The cutoff modes and windows, as `txEqCtfmode` and `txEqWintype` number them.
    static let cutoffChoices: [(id: Int64, label: String)] = [(0, "Peaking"), (1, "Notch")]
    static let windowChoices: [(id: Int64, label: String)] = [(0, "Blackman-Harris"), (1, "Hann")]

    // MARK: The curve's ranges and steps (link document section 7.1, "Changing the curve")

    /// The desktop's 5-band, 10-band and 18-band choices, the only counts the Core takes.
    static let curveCounts = [5, 10, 18]
    static let curveGainRange: ClosedRange<Double> = -24...24
    static let curveQRange: ClosedRange<Double> = 0.2...20
    static let curveHzRange: ClosedRange<Int64> = 0...20000
    /// The steps of the curve's − and + buttons.
    static let curveFrequencyStepHz = 10.0
    static let curveGainStepDb = 0.5
    static let curveQStep = 0.1
    /// A flat point's Q, as the desktop's Bands choice and Reset set it.
    static let flatQ = 4.0
    /// How close a stepped frequency comes to its neighbours.
    static let neighbourGapHz = 5.0

    // MARK: Words

    static let notConnectedReason = SpotsModel.notConnectedReason
    static let notSentReason = "This Core does not send this setting. Updating the Core may help."
    static let olderCoreReason = "This Core does not send the TX EQ curve. Updating the Core may help."
    static let unavailableText = "The Core cannot read this profile\u{2019}s saved curve. Reset, under Curve values, "
        + "gives it the flat curve: ten points from 0 to 4000 Hz at 0 dB."
    static let unavailableReason = "The saved curve cannot be read. Reset, under Curve values, replaces it."
    static let bandsWayText = "Bands, under Curve values, is the way to change this curve: choose 5-band, "
        + "10-band or 18-band."
    static let changedElsewhereText = "The curve was changed on another device. This page now shows the Core\u{2019}s curve."
    static let straightLinesQReason = "Q shapes bells only. Set Shape to Bells to use it."

    /// The Core's words while another device holds transmit.
    static func holderReason(_ holder: String) -> String {
        "\(holder) has the transmitter."
    }

    /// Why an end point's frequency is not changed here.
    static func endReason(point: Int, low: Bool) -> String {
        "Point \(point) sits at the \(low ? "low" : "high") end of the range. Change \(low ? "Low" : "High") "
            + "under Curve values to move it."
    }

    static let transmitKey = TransmitModel.transmitKey
    static let saveProfileVerb = "txProfile.save"

    /// Which EQ reaches the air, from `txEqEnabled` and `txEqUseLegacy`.
    enum OnAir: Equatable {
        /// TX EQ on, Legacy EQ off: the curve.
        case curve
        /// Legacy EQ on: the ten bands.
        case tenBands
        /// TX EQ off.
        case off
    }

    // MARK: State

    @Published private(set) var enabled: Bool?
    /// Legacy EQ: the ten bands reach the transmitter; off, the parametric curve does.
    @Published private(set) var legacy: Bool?
    @Published private(set) var preamp: Int?
    /// The ten bands' gains in dB and centres in Hz, lowest band first.
    @Published private(set) var bands: [Int]?
    @Published private(set) var frequencies: [Int]?
    @Published private(set) var size: Int64?
    @Published private(set) var minimumPhase: Bool?
    @Published private(set) var cutoff: Int64?
    @Published private(set) var window: Int64?
    /// The Core holds a saved parametric curve (`txEqParaEqData`).
    @Published private(set) var hasCurve = false
    /// The Core's TX profiles and the active one.
    @Published private(set) var profiles: [String] = []
    @Published private(set) var activeProfile: String?
    /// Why each group cannot change now, or nil when it can.
    @Published private(set) var switchReason: String?
    @Published private(set) var editorReason: String?
    @Published private(set) var profileReason: String?
    /// The Core's words for the last change it refused.
    @Published private(set) var note: String?
    /// The number pad open over the page, if any.
    @Published private(set) var pad: ValuePadModel?

    /// The curve the page draws: the Core's, as it last sent or answered
    /// with it; nil when the Core sends none.
    @Published private(set) var curve: TxEqCurve?
    /// The chosen point, by index.
    @Published private(set) var selected = 0
    /// Why the curve's controls cannot change it now, or nil when they can.
    @Published private(set) var curveReason: String?
    /// Why Reset cannot be pressed now, or nil when it can.
    @Published private(set) var resetReason: String?
    /// Why Save cannot be pressed now, or nil when it can.
    @Published private(set) var saveReason: String?
    /// The Core's words for the last curve change it refused.
    @Published private(set) var curveNote: String?
    /// The curve last changed on another device.
    @Published private(set) var changedElsewhere = false
    /// The Core's radio is on the air.
    @Published private(set) var transmitting = false
    /// The Core takes the TX EQ settings on the air (`transmitSettingsVersion` 13).
    @Published private(set) var takesOnAir = false

    private static let logger = Logger(subsystem: "NereusSDR", category: "tools.txEqualizer")

    private let mirror: MirrorStore
    private let transmit: TransmitModel
    private let commands: CommandClient?
    private var watch: ToolMirrorWatch?
    /// Each write shows at the touch and stays until the Core answers
    /// (`StationClient.cpp:1040-1068`), one at a time per property, the
    /// newest value next (``PropertyWriteQueue``).
    private lazy var writes = PropertyWriteQueue(store: mirror) { [weak self] property, outcome in
        self?.noteOutcome(outcome, property)
    }
    private lazy var commandHolds = CommandHoldQueue(store: mirror)
    /// The curve as the mirror last held it, to tell another device's change from this phone's.
    private var mirrorCurve: TxEqCurve?
    private var profileSeen: String?
    /// Curve changes sent and not answered; a change seen meanwhile is this phone's.
    private var curveChangesInFlight = 0
    private var curveTail: Task<Void, Never>?
    private let curveOutcomeOwner = ControlOutcomeOwner()

    @MainActor
    private final class CurveOperation {
        let edit: UInt64
        let snapshot: UInt64
        let profile: String?
        let fromPad: Bool
        weak var pad: ValuePadModel?
        let padEntry: UInt64?
        var before: TxEqCurve?
        let onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)?

        init(edit: UInt64, snapshot: UInt64, profile: String?, fromPad: Bool, pad: ValuePadModel?,
             onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)?) {
            self.edit = edit
            self.snapshot = snapshot
            self.profile = profile
            self.fromPad = fromPad
            self.pad = pad
            padEntry = pad?.outcomeIdentity
            self.onLateOutcome = onLateOutcome
        }
    }
    /// The open pad sets a value of the curve, typed against this curve.
    private var padIsCurve = false
    private var padCurve: TxEqCurve?

    init(mirror: MirrorStore, transmit: TransmitModel, commands: CommandClient?) {
        self.mirror = mirror
        self.transmit = transmit
        self.commands = commands
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.objectWillChange.send(); self?.refresh() }
        watch.watch(transmit.$settingsVersion)
        watch.watch(transmit.$coreOnAir)
        watch.watch(transmit.$profiles)
        watch.watch(transmit.$activeProfile)
        watch.watch(transmit.$report)
        self.watch = watch
        refresh()
    }

    // MARK: Reading

    func isUnconfirmed(_ property: String) -> Bool { mirror.isUnconfirmed(Self.transmitKey, property: property) }

    func refresh() {
        let object = watch?.object(Self.transmitKey)
        func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<TxEqualizerModel, Value>, _ value: Value) {
            if self[keyPath: path] != value {
                self[keyPath: path] = value
            }
        }
        set(\.enabled, ToolValue.flag(object?["txEqEnabled"]))
        set(\.legacy, ToolValue.flag(object?["txEqUseLegacy"]))
        set(\.preamp, ToolValue.whole(object?["txEqPreamp"]).map { Int($0) })
        set(\.bands, ToolValue.wholeArray(ToolValue.text(object?["txEqBandsJson"]), count: Self.bandCount))
        set(\.frequencies, ToolValue.wholeArray(ToolValue.text(object?["txEqFreqsJson"]), count: Self.bandCount))
        set(\.size, ToolValue.whole(object?["txEqNc"]))
        set(\.minimumPhase, ToolValue.flag(object?["txEqMp"]))
        set(\.cutoff, ToolValue.whole(object?["txEqCtfmode"]))
        set(\.window, ToolValue.whole(object?["txEqWintype"]))
        set(\.hasCurve, !(ToolValue.text(object?["txEqParaEqData"]) ?? "").isEmpty)
        set(\.profiles, transmit.profiles)
        set(\.activeProfile, transmit.activeProfile)
        set(\.switchReason, reason(Self.switchVersion))
        set(\.editorReason, reason(Self.editorVersion))
        set(\.profileReason, reason(Self.profileVersion))
        set(\.saveReason, reason(Self.profileVersion))
        set(\.transmitting, transmit.coreOnAir)
        set(\.takesOnAir, transmit.settingsVersion >= Self.onAirVersion)
        readCurve(object)
    }

    /// The curve the Core sends, told apart from this phone's own change.
    private func readCurve(_ object: MirrorObject?) {
        let offered = mirror.capabilityVersion(TxEqCurve.capabilityName) >= 1
        let sent = offered ? ToolValue.text(object?[TxEqCurve.propertyName]).flatMap(TxEqCurve.init(json:)) : nil
        // The active profile as the same delta carries it: a new profile brings its own curve.
        let profile = ToolValue.text(object?["activeTxProfile"])
        let profileChanged = profileSeen != profile
        profileSeen = profile
        if sent != mirrorCurve {
            if curveChangesInFlight == 0, mirrorCurve != nil, sent != nil {
                // Another device's change, or another profile: whatever the
                // pad holds was typed against a curve that is gone.
                if padIsCurve {
                    closePad()
                }
                changedElsewhere = !profileChanged
                curveNote = nil
            }
            mirrorCurve = sent
            curve = sent
        }
        let count = curve?.points.count ?? 0
        let chosen = count == 0 ? 0 : min(max(selected, 0), count - 1)
        if selected != chosen {
            selected = chosen
        }
        let curveReason = curveBlock(reset: false)
        if self.curveReason != curveReason {
            self.curveReason = curveReason
        }
        let resetReason = curveBlock(reset: true)
        if self.resetReason != resetReason {
            self.resetReason = resetReason
        }
    }

    /// Why a setting that came with `version` cannot change now, or nil:
    /// no Core, a Core that does not take it from this phone, another
    /// device holding transmit, or the radio on the air below version 13.
    private func reason(_ version: Int64) -> String? {
        guard mirror.isSnapshotComplete, !mirror.isStale else {
            return Self.notConnectedReason
        }
        guard (mirror.agreedMinor ?? 0) >= TransmitModel.settingsMinor,
              transmit.settingsVersion >= max(version, 1) else {
            return TransmitModel.settingsNotTakenText
        }
        let report = transmit.report
        if report.heldElsewhere, !report.holderLabel.isEmpty {
            return Self.holderReason(report.holderLabel)
        }
        if transmit.coreOnAir, transmit.settingsVersion < Self.onAirVersion {
            return TransmitModel.onAirText
        }
        return nil
    }

    /// Why the curve's controls (or Reset alone) cannot change it now.
    private func curveBlock(reset: Bool) -> String? {
        guard mirror.isSnapshotComplete, !mirror.isStale else {
            return Self.notConnectedReason
        }
        guard let curve else {
            return Self.olderCoreReason
        }
        guard TxEqCurve.canChange(capabilityVersion: mirror.capabilityVersion(TxEqCurve.capabilityName)) else {
            return TxEqCurve.readOnlyReason
        }
        if let gate = reason(Self.editorVersion) {
            return gate
        }
        if !reset, curve.state == .unavailable {
            return Self.unavailableReason
        }
        return nil
    }

    /// Which EQ reaches the air, or nil before the Core says.
    var onAir: OnAir? {
        guard let enabled else {
            return nil
        }
        if !enabled {
            return .off
        }
        return legacy == true ? .tenBands : .curve
    }

    /// The chosen point, when there is a curve to choose from.
    var chosenPoint: TxEqCurve.Point? {
        guard let curve, curve.drawable, curve.points.indices.contains(selected) else {
            return nil
        }
        return curve.points[selected]
    }

    /// The chosen point is the first or the last, fixed to the range's end.
    var chosenIsEnd: Bool {
        guard let curve, chosenPoint != nil else {
            return false
        }
        return selected == 0 || selected == curve.points.count - 1
    }

    /// Why the chosen point's frequency cannot change, or nil when it can.
    var frequencyReason: String? {
        if let curveReason {
            return curveReason
        }
        guard chosenIsEnd else {
            return nil
        }
        return Self.endReason(point: selected + 1, low: selected == 0)
    }

    /// Why Q cannot change, or nil when it can.
    var qReason: String? {
        if let curveReason {
            return curveReason
        }
        return curve?.parametric == false ? Self.straightLinesQReason : nil
    }

    /// The curve's count is one of the desktop's three, or nil.
    var chosenCount: Int? {
        guard let count = curve?.points.count, curve?.drawable == true, Self.curveCounts.contains(count) else {
            return nil
        }
        return count
    }

    // MARK: Changing

    func setEnabled(_ on: Bool) {
        guard switchReason == nil else {
            return
        }
        write("txEqEnabled", .bool(on))
    }

    func setLegacy(_ on: Bool) {
        guard editorReason == nil else {
            return
        }
        write("txEqUseLegacy", .bool(on))
    }

    func setPreamp(_ db: Int) {
        guard editorReason == nil else {
            return
        }
        write("txEqPreamp", .int(Int64(min(max(db, Self.preampRange.lowerBound), Self.preampRange.upperBound))))
    }

    /// One band's gain: the Core takes the ten together.
    func setBand(_ index: Int, _ db: Int) {
        guard editorReason == nil, var next = bands, next.indices.contains(index) else {
            return
        }
        next[index] = min(max(db, Self.bandRange.lowerBound), Self.bandRange.upperBound)
        write("txEqBandsJson", .text(ToolValue.wholeArrayText(next)))
    }

    func setMinimumPhase(_ on: Bool) {
        guard editorReason == nil else {
            return
        }
        write("txEqMp", .bool(on))
    }

    func setCutoff(_ mode: Int64) {
        guard editorReason == nil else {
            return
        }
        write("txEqCtfmode", .int(mode))
    }

    func setWindow(_ type: Int64) {
        guard editorReason == nil else {
            return
        }
        write("txEqWintype", .int(type))
    }

    /// A TX profile, by the Core's name for it (`txProfile.select`).
    func selectProfile(_ name: String) {
        guard profileReason == nil, name != activeProfile else {
            return
        }
        guard let commands else {
            noteOutcome(.notSent, "activeTxProfile")
            return
        }
        // Lit at the touch and kept until the Core answers
        // (StationClient.cpp:1040-1068), as the TX panel's profile is.
        let verb = TransmitModel.txProfileVerb
        commandHolds.send(verb, shows: [.init(Self.transmitKey, "activeTxProfile", .text(name))], invokeWithLate: { late in
            try await commands.invokeHeld(verb, arguments: [CommandArgument(name: "name", value: .text(name))],
                                      timeout: .seconds(5), onLateOutcome: { outcome in
                await MainActor.run { late(outcome) }
            })
        }, onOutcome: { [weak self] outcome in
            self?.noteOutcome(PropertyWriteOutcome(outcome), "activeTxProfile")
        })
    }

    /// Save: the active TX profile, this curve with it (`txProfile.save`).
    func saveProfile() {
        guard saveReason == nil, let name = activeProfile, !name.isEmpty else {
            return
        }
        invoke(Self.saveProfileVerb, [CommandArgument(name: "name", value: .text(name))])
    }

    /// Opens the number pad for one band's centre frequency.
    func openFrequencyPad(_ index: Int) {
        guard editorReason == nil, let frequencies, frequencies.indices.contains(index) else {
            return
        }
        openPad(ValuePadModel(title: "Band \(index + 1) center", unit: "Hz", range: Self.frequencyRange,
                              current: Int64(frequencies[index]), sendWithLate: { [weak self] hz, late in
                                  guard let self, self.editorReason == nil, var next = self.frequencies,
                                        next.indices.contains(index) else {
                                      return nil
                                  }
                                  next[index] = Int(hz)
                                  return await self.send("txEqFreqsJson", .text(ToolValue.wholeArrayText(next)), onLateOutcome: late)
                              }, onOutcome: { [weak self] in self?.noteOutcome($0, "TX EQ") },
                              readCurrent: { [weak self] in
                                  guard let values = self?.frequencies, values.indices.contains(index) else { return nil }
                                  return Double(values[index])
                              }, close: { [weak self] in self?.closePad() }), curve: false)
    }

    /// Opens the number pad for the filter's size.
    func openSizePad() {
        guard editorReason == nil else {
            return
        }
        openPad(ValuePadModel(title: "Filter size", unit: "taps", range: Self.sizeRange, current: size,
                              sendWithLate: { [weak self] taps, late in
                                  guard let self, self.editorReason == nil else {
                                      return nil
                                  }
                                  return await self.send("txEqNc", .int(taps), onLateOutcome: late)
                              }, onOutcome: { [weak self] in self?.noteOutcome($0, "TX EQ") },
                              readCurrent: { [weak self] in self?.size.map(Double.init) }, close: { [weak self] in self?.closePad() }), curve: false)
    }

    // MARK: Changing the curve

    /// Chooses a point by index; it sends nothing.
    func choose(_ index: Int) {
        guard let count = curve?.points.count, (0..<count).contains(index), index != selected else {
            return
        }
        selected = index
    }

    func choosePrevious() {
        choose(selected - 1)
    }

    func chooseNext() {
        choose(selected + 1)
    }

    /// The chosen point's frequency, 10 Hz down or up, whole hertz, kept
    /// 5 Hz inside its neighbours. The ends follow Low and High instead.
    func stepFrequency(up: Bool) {
        let index = selected
        guard frequencyReason == nil else {
            return
        }
        changeCurve { curve in
            guard index > 0, index < curve.points.count - 1 else {
                return nil
            }
            var next = curve
            let low = curve.points[index - 1].frequencyHz + Self.neighbourGapHz
            let high = curve.points[index + 1].frequencyHz - Self.neighbourGapHz
            guard low <= high else {
                return nil
            }
            let stepped = curve.points[index].frequencyHz.rounded() + (up ? 1 : -1) * Self.curveFrequencyStepHz
            next.points[index].frequencyHz = min(max(stepped, low), high)
            return next
        }
    }

    /// The chosen point's gain, 0.5 dB down or up.
    func stepGain(up: Bool) {
        let index = selected
        guard curveReason == nil else {
            return
        }
        changeCurve { curve in
            guard curve.points.indices.contains(index) else {
                return nil
            }
            var next = curve
            next.points[index].gainDb = Self.stepped(curve.points[index].gainDb, by: Self.curveGainStepDb, up: up,
                                                     places: 1, within: Self.curveGainRange)
            return next
        }
    }

    /// The chosen point's Q, 0.1 down or up; bells only.
    func stepQ(up: Bool) {
        let index = selected
        guard qReason == nil else {
            return
        }
        changeCurve { curve in
            guard curve.points.indices.contains(index), curve.parametric else {
                return nil
            }
            var next = curve
            next.points[index].q = Self.stepped(curve.points[index].q, by: Self.curveQStep, up: up, places: 2,
                                                within: Self.curveQRange)
            return next
        }
    }

    /// The curve's own preamp, 0.5 dB down or up.
    func stepCurvePreamp(up: Bool) {
        guard curveReason == nil else {
            return
        }
        changeCurve { curve in
            var next = curve
            next.preampDb = Self.stepped(curve.preampDb, by: Self.curveGainStepDb, up: up, places: 1,
                                         within: Self.curveGainRange)
            return next
        }
    }

    /// Shape: bells (`parametric`) or straight lines.
    func setShape(bells: Bool) {
        guard curveReason == nil, curve?.parametric != bells else {
            return
        }
        changeCurve { curve in
            var next = curve
            next.parametric = bells
            return next
        }
    }

    /// Bands: `count` flat points spread evenly from Low to High, the
    /// curve preamp and Shape kept, as the desktop's 5-band, 10-band and
    /// 18-band choices reset the points.
    func setBands(_ count: Int) {
        guard curveReason == nil, Self.curveCounts.contains(count) else {
            return
        }
        changeCurve { curve in
            var next = curve
            next.points = Self.flatPoints(count, from: curve.minHz, to: curve.maxHz)
            return next
        }
    }

    /// Reset: the Core's flat curve (`txEq.resetCurve`), which keeps the
    /// count, the range and Shape; it also replaces a curve it cannot read.
    func reset() {
        guard resetReason == nil else {
            return
        }
        let operation = curveOperation()
        Task { [weak self] in
            await self?.sendCurveChange(nil, operation: operation)
        }
    }

    /// Opens the number pad for the chosen point's frequency, in whole hertz.
    func openPointFrequencyPad() {
        guard frequencyReason == nil, let curve, let point = chosenPoint else {
            return
        }
        let index = selected
        let range = Int64(curve.minHz.rounded(.up))...Int64(curve.maxHz.rounded(.down))
        openCurvePad(ValuePadModel(title: "Point \(index + 1) frequency", unit: "Hz", range: range,
                                   current: Int64(point.frequencyHz.rounded()),
                                   sendWithLate: { [weak self] hz, late in
                                       await self?.padChange(onLateOutcome: late) { curve in
                                           guard curve.points.indices.contains(index) else {
                                               return nil
                                           }
                                           var next = curve
                                           next.points[index].frequencyHz = Double(hz)
                                           return next
                                       }
                                   }, onOutcome: { [weak self] outcome in self?.noteCurveOutcome(outcome) },
                                   readCurrent: { [weak self] in guard let points = self?.curve?.points, points.indices.contains(index) else { return nil }; return points[index].frequencyHz }, close: { [weak self] in self?.closePad() }))
    }

    /// Opens the number pad for the chosen point's gain, to 0.1 dB.
    func openGainPad() {
        guard curveReason == nil, let point = chosenPoint else {
            return
        }
        let index = selected
        openCurvePad(ValuePadModel(title: "Point \(index + 1) gain", unit: "dB", range: Self.curveGainRange,
                                   decimals: 1, step: "0.1", current: point.gainDb,
                                   sendWithLate: { [weak self] db, late in
                                       await self?.padChange(onLateOutcome: late) { curve in
                                           guard curve.points.indices.contains(index) else {
                                               return nil
                                           }
                                           var next = curve
                                           next.points[index].gainDb = db
                                           return next
                                       }
                                   }, onOutcome: { [weak self] outcome in self?.noteCurveOutcome(outcome) },
                                   readCurrent: { [weak self] in guard let points = self?.curve?.points, points.indices.contains(index) else { return nil }; return points[index].gainDb }, close: { [weak self] in self?.closePad() }))
    }

    /// Opens the number pad for the chosen point's Q, to 0.01.
    func openQPad() {
        guard qReason == nil, let point = chosenPoint else {
            return
        }
        let index = selected
        openCurvePad(ValuePadModel(title: "Point \(index + 1) Q", unit: "", range: Self.curveQRange, decimals: 2,
                                   step: "0.01", current: point.q,
                                   sendWithLate: { [weak self] q, late in
                                       await self?.padChange(onLateOutcome: late) { curve in
                                           guard curve.points.indices.contains(index) else {
                                               return nil
                                           }
                                           var next = curve
                                           next.points[index].q = q
                                           return next
                                       }
                                   }, onOutcome: { [weak self] outcome in self?.noteCurveOutcome(outcome) },
                                   readCurrent: { [weak self] in guard let points = self?.curve?.points, points.indices.contains(index) else { return nil }; return points[index].q }, close: { [weak self] in self?.closePad() }))
    }

    /// Opens the number pad for the curve's own preamp, to 0.1 dB.
    func openCurvePreampPad() {
        guard curveReason == nil, let curve, curve.drawable else {
            return
        }
        openCurvePad(ValuePadModel(title: "Curve preamp", unit: "dB", range: Self.curveGainRange, decimals: 1,
                                   step: "0.1", current: curve.preampDb,
                                   sendWithLate: { [weak self] db, late in
                                       await self?.padChange(onLateOutcome: late) { curve in
                                           var next = curve
                                           next.preampDb = db
                                           return next
                                       }
                                   }, onOutcome: { [weak self] outcome in self?.noteCurveOutcome(outcome) },
                                   readCurrent: { [weak self] in return self?.curve?.preampDb }, close: { [weak self] in self?.closePad() }))
    }

    /// Opens the number pad for the curve's low or high end, in whole hertz.
    func openRangePad(low: Bool) {
        guard curveReason == nil, let curve, curve.drawable else {
            return
        }
        let current = low ? curve.minHz : curve.maxHz
        openCurvePad(ValuePadModel(title: low ? "Low" : "High", unit: "Hz", range: Self.curveHzRange,
                                   current: Int64(current.rounded()),
                                   sendWithLate: { [weak self] hz, late in
                                       await self?.padChange(onLateOutcome: late) { curve in
                                           Self.ranged(curve, low ? Double(hz) : curve.minHz,
                                                       low ? curve.maxHz : Double(hz))
                                       }
                                   }, onOutcome: { [weak self] outcome in self?.noteCurveOutcome(outcome) },
                                   readCurrent: { [weak self] in return low ? self?.curve?.minHz : self?.curve?.maxHz }, close: { [weak self] in self?.closePad() }))
    }

    /// The curve with a new Low and High, as the desktop's editor moves
    /// them: each point keeps its place between the ends (its share of the
    /// old span, held to 0 to 1, is its share of the new one), and the
    /// points are then put in the link document's order (section 7.1,
    /// "Ordering"). A low end not below the high end leaves the points as
    /// they are, as the desktop's does, and the Core's refusal says why.
    static func ranged(_ curve: TxEqCurve, _ low: Double, _ high: Double) -> TxEqCurve {
        var next = curve
        next.minHz = low
        next.maxHz = high
        guard low.isFinite, high.isFinite, low < high else {
            return next
        }
        let oldSpan = curve.maxHz - curve.minHz > 0 ? curve.maxHz - curve.minHz : 1
        let newSpan = high - low
        for index in next.points.indices {
            let share = min(max((next.points[index].frequencyHz - curve.minHz) / oldSpan, 0), 1)
            next.points[index].frequencyHz = low + share * newSpan
        }
        next.points = ordered(next.points, from: low, to: high)
        return next
    }

    /// `points` in the link document's order (section 7.1, "Ordering"):
    /// lowest frequency first, two at one frequency kept as they were;
    /// each held to `low` to `high`, the first at `low` and the last at
    /// `high`; the points between kept a spacing apart, 5 Hz or less when
    /// the span is too narrow for that.
    static func ordered(_ points: [TxEqCurve.Point], from low: Double, to high: Double) -> [TxEqCurve.Point] {
        var sorted = points.enumerated().sorted { first, second in
            first.element.frequencyHz != second.element.frequencyHz
                ? first.element.frequencyHz < second.element.frequencyHz : first.offset < second.offset
        }.map(\.element)
        guard !sorted.isEmpty else {
            return sorted
        }
        for index in sorted.indices {
            sorted[index].frequencyHz = min(max(sorted[index].frequencyHz, low), high)
        }
        let last = sorted.count - 1
        sorted[0].frequencyHz = low
        sorted[last].frequencyHz = high
        guard sorted.count >= 3 else {
            return sorted
        }
        let gap = max(min(neighbourGapHz, (high - low) / Double(last)), 0)
        for index in 1..<last {
            let floor = low + gap * Double(index)
            let ceiling = max(high - gap * Double(last - index), floor)
            sorted[index].frequencyHz = min(max(sorted[index].frequencyHz, floor), ceiling)
        }
        for index in 1..<last {
            sorted[index].frequencyHz = max(sorted[index].frequencyHz, sorted[index - 1].frequencyHz + gap)
        }
        for index in stride(from: last - 1, through: 1, by: -1) {
            sorted[index].frequencyHz = min(sorted[index].frequencyHz, sorted[index + 1].frequencyHz - gap)
        }
        sorted[0].frequencyHz = low
        sorted[last].frequencyHz = high
        return sorted
    }

    /// `count` points at 0 dB and Q 4, evenly from `low` to `high`, as the
    /// desktop's ParametricEqWidget::resetPointsDefault spreads them.
    static func flatPoints(_ count: Int, from low: Double, to high: Double) -> [TxEqCurve.Point] {
        guard count >= 2 else {
            return []
        }
        return (0..<count).map { index in
            let hz = index == count - 1 ? high : low + (high - low) * Double(index) / Double(count - 1)
            return TxEqCurve.Point(frequencyHz: hz, gainDb: 0, q: flatQ)
        }
    }

    /// `value` a step down or up, rounded to `places` and held to `range`.
    static func stepped(_ value: Double, by step: Double, up: Bool, places: Int,
                        within range: ClosedRange<Double>) -> Double {
        let scale = pow(10, Double(places))
        let next = ((value + (up ? step : -step)) * scale).rounded() / scale
        return min(max(next, range.lowerBound), range.upperBound)
    }

    // MARK: Sending the curve

    /// A curve change from a − or + button or a choice: sent after any
    /// change before it has its answer, on the curve the Core then holds.
    private func curveOperation(fromPad: Bool = false,
                                onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)? = nil)
        -> CurveOperation {
        CurveOperation(edit: curveOutcomeOwner.begin(), snapshot: mirror.snapshotIdentity,
                       profile: profileSeen, fromPad: fromPad, pad: fromPad ? pad : nil,
                       onLateOutcome: onLateOutcome)
    }

    private func changeCurve(_ make: @escaping (TxEqCurve) -> TxEqCurve?) {
        // Note ownership starts at the touch, before task admission.
        let operation = curveOperation()
        Task { [weak self] in
            await self?.sendCurveChange(make, operation: operation)
        }
    }

    /// This phone's earlier changes carry the open pad along before its
    /// serialized turn; a replaced pad or newly typed entry cannot send it.
    private func padChange(onLateOutcome: @escaping @MainActor (PropertyWriteOutcome) -> Void,
                           _ make: @escaping (TxEqCurve) -> TxEqCurve?) async -> PropertyWriteOutcome? {
        guard padIsCurve, padCurve != nil else { return nil }
        let operation = curveOperation(fromPad: true, onLateOutcome: onLateOutcome)
        return await sendCurveChange(make, operation: operation)
    }

    private func dropPadChangedElsewhere() {
        closePad()
        changedElsewhere = true
        curveNote = nil
    }

    /// Each turn applies its change to the curve the Core most recently
    /// kept, preserving whole-curve serialization and Core rounding.
    @discardableResult
    private func sendCurveChange(_ make: ((TxEqCurve) -> TxEqCurve?)?, operation: CurveOperation)
        async -> PropertyWriteOutcome? {
        let previous = curveTail
        let turn = Task { @MainActor [weak self] () -> PropertyWriteOutcome? in
            await previous?.value
            return await self?.sendNow(make, operation: operation)
        }
        curveTail = Task { _ = await turn.value }
        return await turn.value
    }

    private func ownsCurveContext(_ operation: CurveOperation) -> Bool {
        guard operation.snapshot == mirror.snapshotIdentity, operation.profile == profileSeen else { return false }
        if operation.fromPad {
            guard let original = operation.pad, original === pad,
                  let entry = operation.padEntry, original.outcomeIdentity == entry else { return false }
        }
        return true
    }

    private func sendNow(_ make: ((TxEqCurve) -> TxEqCurve?)?, operation: CurveOperation) async
        -> PropertyWriteOutcome? {
        guard let commands, ownsCurveContext(operation) else { return nil }
        if operation.fromPad, !padIsCurve || padCurve != mirrorCurve {
            if padIsCurve { dropPadChangedElsewhere() }
            return nil
        }
        operation.before = mirrorCurve
        curveChangesInFlight += 1
        defer { curveChangesInFlight -= 1 }
        let late: @Sendable (Result<TxEqCurve.EditOutcome, CommandError>) async -> Void = { [weak self] result in
            await self?.receiveLateCurve(result, operation: operation)
        }
        do {
            let outcome: TxEqCurve.EditOutcome
            if let make {
                guard curveReason == nil, let current = curve, current.drawable,
                      let next = make(current), next != current else { return nil }
                outcome = try await TxEqCurve.set(next, through: commands, onLateOutcome: late)
            } else {
                guard resetReason == nil else { return nil }
                outcome = try await TxEqCurve.reset(through: commands, onLateOutcome: late)
            }
            return receiveCurve(outcome, operation: operation, late: false)
        } catch let error as CommandError {
            return receiveCurveError(error, operation: operation)
        } catch {
            Self.logger.info("A TX EQ curve change had no answer from the Core")
            return nil
        }
    }

    private func receiveLateCurve(_ result: Result<TxEqCurve.EditOutcome, CommandError>,
                                  operation: CurveOperation) {
        guard curveOutcomeOwner.isCurrent(operation.edit), ownsCurveContext(operation) else { return }
        let outcome: PropertyWriteOutcome?
        switch result {
        case .success(let result): outcome = receiveCurve(result, operation: operation, late: true)
        case .failure(let error): outcome = receiveCurveError(error, operation: operation)
        }
        if let outcome { operation.onLateOutcome?(outcome) }
    }

    private func receiveCurveError(_ error: CommandError, operation: CurveOperation) -> PropertyWriteOutcome? {
        guard error != .notSent else { return nil }
        let outcome = error == .linkLost ? PropertyWriteOutcome.linkLost : .notConfirmed
        guard curveOutcomeOwner.isCurrent(operation.edit), ownsCurveContext(operation) else {
            return PropertyWriteOutcome(accepted: false, reason: outcome.reason, value: nil,
                                        answeredByCore: false, isCurrent: false)
        }
        if !operation.fromPad { noteCurveOutcome(outcome) }
        return outcome
    }

    private func receiveCurve(_ result: TxEqCurve.EditOutcome, operation: CurveOperation, late: Bool)
        -> PropertyWriteOutcome {
        refresh()
        let latest = curveOutcomeOwner.isCurrent(operation.edit)
        let matchesDelta: Bool
        switch result {
        case .taken(nil):
            // With no returned curve the authoritative delta already carries
            // the value; there is no old returned value to overwrite it with.
            matchesDelta = true
        case .taken(let kept): matchesDelta = mirrorCurve == operation.before || kept == mirrorCurve
        case .refused: matchesDelta = mirrorCurve == operation.before
        }
        guard ownsCurveContext(operation), matchesDelta, !late || latest else {
            return PropertyWriteOutcome(accepted: false, reason: "", value: nil, isCurrent: false)
        }
        switch result {
        case .taken(let kept):
            // A prior serialized turn can supply the next turn's Core curve;
            // it cannot replace a different, newer Core delta or a pad owner.
            if padIsCurve, padCurve == operation.before, !operation.fromPad {
                padCurve = mirrorCurve
            }
            if let kept, kept != curve { curve = kept }
            let outcome = PropertyWriteOutcome(accepted: true, reason: "", value: nil, isCurrent: latest)
            if latest {
                if !operation.fromPad { noteCurveOutcome(outcome) }
                if changedElsewhere { changedElsewhere = false }
            }
            return outcome
        case .refused(let words):
            let outcome = PropertyWriteOutcome(accepted: false,
                                               reason: words.isEmpty ? BandSlicesModel.refusedText : words,
                                               value: nil, isCurrent: latest)
            if !operation.fromPad { noteCurveOutcome(outcome) }
            return outcome
        }
    }

    private func noteCurveOutcome(_ outcome: PropertyWriteOutcome) {
        guard outcome.isCurrent, !outcome.heldForQuestion else { return }
        curveNote = outcome.noteText(refused: BandSlicesModel.refusedText)
    }

    // MARK: The pad

    private func openPad(_ model: ValuePadModel, curve: Bool) {
        pad?.retire()
        padIsCurve = curve
        pad = model
    }

    private func openCurvePad(_ model: ValuePadModel) {
        padCurve = mirrorCurve
        openPad(model, curve: true)
    }

    private func closePad() {
        pad?.retire()
        padIsCurve = false
        padCurve = nil
        pad = nil
    }

    // MARK: Sending

    /// Writes to one property go one at a time, each after the Core has
    /// answered the one before; a value that arrives meanwhile replaces any
    /// other still waiting, so a slider's last position is the one kept.
    private func write(_ property: String, _ value: MirrorValue) {
        writes.write(Self.transmitKey, property, value)
    }

    /// One answered write, for a number pad that waits for the Core's word.
    private func send(_ property: String, _ value: MirrorValue,
                      onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)? = nil) async -> PropertyWriteOutcome {
        await mirror.write(Self.transmitKey, property: property, value: value, onLateOutcome: onLateOutcome)
    }

    private func noteOutcome(_ outcome: PropertyWriteOutcome, _ property: String) {
        guard outcome.isCurrent else { return }
        if outcome.heldForQuestion {
            // Held for this phone's question, not refused (StationClient.cpp:7308-7317).
            return
        }
        if outcome.accepted {
            if note != nil {
                note = nil
            }
        } else if outcome.answeredByCore, !outcome.reason.isEmpty {
            note = outcome.reason
        } else if !outcome.answeredByCore {
            // Not sent, not answered in time, or cut off by a dropped link.
            Self.logger.info("A \(property, privacy: .public) change: \(outcome.reason, privacy: .private)")
            note = outcome.reason
        }
    }

    /// A profile command; the Core's words show when it refuses.
    private func invoke(_ verb: String, _ arguments: [CommandArgument]) {
        guard let commands else {
            return
        }
        Task { [weak self] in
            do {
                let result = try await commands.invoke(verb, arguments: arguments, timeout: .seconds(5))
                guard let self else {
                    return
                }
                if result.accepted {
                    if self.note != nil {
                        self.note = nil
                    }
                } else {
                    // A refusal with no words still says it was refused.
                    self.note = result.reason.isEmpty ? BandSlicesModel.refusedText : result.reason
                }
            } catch {
                Self.logger.info("A \(verb, privacy: .public) request had no answer from the Core")
            }
        }
    }
}
