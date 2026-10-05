// NereusSDR for iOS: the AM Mod Monitor on the TX panel: the Core's readings, this phone's flashers and the shared feedback receiver
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMirror
import os

/// The AM Mod Monitor (R-IOS-13, R-IOS-18, D102; link document sections
/// 6.3, 7.7, 8.1 and 9.1): shown on the TX panel, below its usual
/// controls, while the transmitting slice is in AM, SAM or DSB. It watches
/// the Core's `txAmModulation` or `txAmModulationFeedback` stream only while
/// it is on screen, draws each record as sent (the holds fall as the Core
/// lets them fall, never here), latches its flashers at this phone's
/// thresholds until RESET, and sends RESET to the Core
/// (`txModMonitor.reset`) for every device watching.
///
/// The source, the flasher thresholds and the meter style are this
/// phone's own; the PA feedback receiver (`ModMon/FbStream`) is the
/// Core's, shared by every device.
@MainActor
final class ModMonitorModel: ObservableObject {
    typealias Source = AmModulationReading.Source

    /// Whether the Core can show the monitor to this phone now.
    enum Availability: Equatable {
        case notConnected
        case olderCore
        case available
    }

    /// Bars, or the lit analog pair (the desktop's vintage meters).
    enum MeterStyle: String, CaseIterable {
        case bars
        case meters

        var label: String {
            switch self {
            case .bars:
                return "Bars"
            case .meters:
                return "Meters"
            }
        }
    }

    /// The carrier lamp, in the desktop's order.
    enum Lamp: Equatable {
        /// No readings to judge (the monitor is greyed).
        case blank
        case noCarrier
        case high
        case low
        case ok

        var text: String {
            switch self {
            case .blank:
                return "--"
            case .noCarrier:
                return "NO CARRIER"
            case .high:
                return "CARRIER HIGH"
            case .low:
                return "CARRIER LOW"
            case .ok:
                return "CARRIER OK"
            }
        }
    }

    // MARK: The desktop's words

    static let olderCoreText = "This Core does not send the modulation monitor. Updating the Core may help."
    static let notConnectedText = "Connect to the Core to see the modulation monitor."
    static let pureSignalOffText = "PA feedback works only while PureSignal runs on the Core\u{2019}s radio."
    static let waitingText = "Readings appear while the radio transmits in AM, SAM or DSB."
    static let resetFootText = "RESET clears the peaks and flashers for every device watching."
    static let clearedText = "Peaks and flashers cleared."
    static let defaultsText = "Desktop defaults: positive 125%, negative 95%, Bars. Each device keeps its own."
    static let receiverText = "The receiver that carries PureSignal feedback. Hermes Lite 2: rx1. "
        + "A change moves the Core\u{2019}s measurement at once, for every device."

    /// The modes whose transmission the monitor measures.
    static let amFamily: Set<String> = ["AM", "SAM", "DSB"]
    /// The flashers' ranges and the desktop's defaults.
    static let posFlashRange = 50...160
    static let negFlashRange = 50...100
    static let defaultPosFlash = 125
    static let defaultNegFlash = 95
    /// The bars' scales, yellow and red zones, and ticks (the desktop's gauges).
    static let posScale = (max: 160.0, yellow: 100.0, red: 125.0, ticks: [0, 50, 100, 125, 160])
    static let negScale = (max: 100.0, yellow: 90.0, red: 98.0, ticks: Array(stride(from: 0, through: 100, by: 25)))

    /// This phone's keys, under ``PhoneSettings/keyPrefix``.
    static let sourceKey = "modMonitor.source"
    static let posFlashKey = "modMonitor.posFlashPct"
    static let negFlashKey = "modMonitor.negFlashPct"
    static let styleKey = "modMonitor.meterStyle"

    // MARK: State

    /// The transmitting slice is in AM, SAM or DSB: the TX panel shows the monitor.
    @Published private(set) var shown = false
    /// The transmitting slice's mode and letter, for the title's chip.
    @Published private(set) var modeLabel: String?
    @Published private(set) var sliceLetter: String?
    @Published private(set) var availability = Availability.notConnected
    /// The Core's newest record for the watched source; nil while it sends none.
    @Published private(set) var reading: AmModulationReading?
    /// The flashers, latched until RESET.
    @Published private(set) var posLit = false
    @Published private(set) var negLit = false
    @Published private(set) var source: Source
    @Published private(set) var posFlashPct: Int
    @Published private(set) var negFlashPct: Int
    @Published private(set) var meterStyle: MeterStyle
    /// The Core's PA feedback receiver, 0 to 4.
    @Published private(set) var feedbackReceiver = AmModulationReading.defaultFeedbackReceiver
    /// PureSignal runs on the Core's radio: PA feedback can be watched.
    @Published private(set) var pureSignalRunning = false
    /// "Peaks and flashers cleared.", or the Core's words refusing RESET.
    @Published private(set) var note: String?
    /// The Core's words refusing a feedback receiver.
    @Published private(set) var receiverNote: String?

    private let mirror: MirrorStore
    private let commands: CommandClient?
    private let records: RecordStreamClient
    private let settings: SettingsProxyClient
    private let slices: BandSlicesModel
    private let phone: PhoneSettings
    private var watches: Set<AnyCancellable> = []
    private var objectWatches: [String: AnyCancellable] = [:]
    private var watchedObjects: [String: ObjectIdentifier] = [:]
    private var refreshQueued = false
    /// How many monitors are on screen now (the phone's TX panel, or an iPad's column).
    private var onScreen = 0
    /// The stream asked of the Core now.
    private var watching: String?
    private static let logger = Logger(subsystem: "NereusSDR", category: "tx.modMonitor")

    init(mirror: MirrorStore, commands: CommandClient?, records: RecordStreamClient, settings: SettingsProxyClient,
         slices: BandSlicesModel, phone: PhoneSettings) {
        self.mirror = mirror
        self.commands = commands
        self.records = records
        self.settings = settings
        self.slices = slices
        self.phone = phone
        source = Source(rawValue: Int64(phone.integer(Self.sourceKey, default: 0))) ?? .txIq
        posFlashPct = Self.clamp(phone.integer(Self.posFlashKey, default: Self.defaultPosFlash), Self.posFlashRange)
        negFlashPct = Self.clamp(phone.integer(Self.negFlashKey, default: Self.defaultNegFlash), Self.negFlashRange)
        meterStyle = MeterStyle(rawValue: phone.string(Self.styleKey, default: MeterStyle.bars.rawValue)) ?? .bars
        mirror.$objectKeys.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$isSnapshotComplete.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        records.$streams.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        records.$available.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        settings.$values.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        refresh()
    }

    // MARK: On screen

    /// A monitor came on screen: the Core's readings are asked for.
    func appear() {
        onScreen += 1
        updateWatch()
    }

    /// A monitor left the screen: once none is, they are stopped.
    func disappear() {
        onScreen = max(0, onScreen - 1)
        updateWatch()
    }

    /// The monitor's readings are shown live: the Core sends a carrier.
    var live: Bool {
        availability == .available && reading?.carrierPresent == true
    }

    /// The monitor's controls work: the Core sends it to this phone.
    var enabled: Bool {
        availability == .available
    }

    /// Why the monitor is greyed, in the desktop's words; nil while it works.
    var reason: String? {
        switch availability {
        case .notConnected:
            return Self.notConnectedText
        case .olderCore:
            return Self.olderCoreText
        case .available:
            return nil
        }
    }

    /// "AM · Slice A".
    var chipText: String? {
        guard let modeLabel, let sliceLetter else {
            return nil
        }
        return "\(modeLabel) \u{00B7} Slice \(sliceLetter)"
    }

    var lamp: Lamp {
        guard availability == .available else {
            return .blank
        }
        guard let reading, reading.carrierPresent else {
            return .noCarrier
        }
        if reading.carrierHigh {
            return .high
        }
        return reading.carrierLow ? .low : .ok
    }

    /// The bars' values: the window peaks while a carrier is measured, else 0.
    var posBar: Double { live ? reading?.posPeakPct ?? 0 : 0 }
    var negBar: Double { live ? reading?.negPeakPct ?? 0 : 0 }
    /// The hold markers, nil when there is none to draw.
    var posHold: Double? { live && (reading?.posHoldPct ?? 0) > 0 ? reading?.posHoldPct : nil }
    var negHold: Double? { live && (reading?.negHoldPct ?? 0) > 0 ? reading?.negHoldPct : nil }
    /// The envelope trace, empty without a carrier.
    var scope: [Double] { live ? reading?.scopePct ?? [] : [] }

    var posText: String { live ? Self.percentText(reading?.posHoldPct ?? 0) : "--" }
    var negText: String { live ? Self.percentText(reading?.negHoldPct ?? 0) : "--" }
    var asymmetryText: String { live ? Self.signedPercentText(reading?.asymmetryPct ?? 0) : "--" }
    var carrierText: String { live ? Self.dbfsText(reading?.carrierDbfs ?? 0) : "--" }
    var asymmetry: Double { live ? reading?.asymmetryPct ?? 0 : 0 }

    static func percentText(_ value: Double) -> String {
        "\(Int(value.rounded()))%"
    }

    /// "+18%", "-3%": the difference, signed, as the desktop shows it.
    static func signedPercentText(_ value: Double) -> String {
        let whole = Int(value.rounded())
        return (whole >= 0 ? "+" : "-") + "\(abs(whole))%"
    }

    static func dbfsText(_ value: Double) -> String {
        String(format: "%.1f dBFS", value)
    }

    // MARK: Changing

    /// Watches `source`; PA feedback only while PureSignal runs. As on the
    /// desktop, a new source starts its peaks afresh.
    func choose(_ source: Source) {
        guard enabled, source != self.source, source != .paFeedback || pureSignalRunning else {
            return
        }
        self.source = source
        phone.setInteger(Int(source.rawValue), for: Self.sourceKey)
        reading = nil
        updateWatch()
        reset(announce: false)
    }

    /// RESET: this phone's flashers go out, and the Core starts the
    /// source's analyzer afresh for every device watching.
    func reset() {
        reset(announce: true)
    }

    /// `announce` shows the Core's answer under the buttons; the reset a
    /// new source starts with says nothing unless the Core refuses it.
    private func reset(announce: Bool) {
        posLit = false
        negLit = false
        note = nil
        guard enabled, let commands else {
            return
        }
        let source = source
        Task { [weak self] in
            do {
                let result = try await commands.invoke(AmModulationReading.resetVerb, arguments: [
                    CommandArgument(name: "source", value: .int(source.rawValue)),
                ], timeout: .seconds(5))
                guard let self, self.source == source else {
                    return
                }
                if !result.accepted {
                    self.note = result.reason
                } else if announce, !self.posLit, !self.negLit {
                    self.note = Self.clearedText
                }
            } catch {
                Self.logger.info("RESET was not answered: \(String(describing: error), privacy: .public)")
            }
        }
    }

    func setPosFlash(_ percent: Int) {
        posFlashPct = Self.clamp(percent, Self.posFlashRange)
        phone.setInteger(posFlashPct, for: Self.posFlashKey)
    }

    func setNegFlash(_ percent: Int) {
        negFlashPct = Self.clamp(percent, Self.negFlashRange)
        phone.setInteger(negFlashPct, for: Self.negFlashKey)
    }

    func setMeterStyle(_ style: MeterStyle) {
        meterStyle = style
        phone.setString(style.rawValue, for: Self.styleKey)
    }

    /// Moves the Core's feedback receiver, for every device.
    func setFeedbackReceiver(_ receiver: Int) {
        guard enabled, AmModulationReading.feedbackReceivers.contains(receiver) else {
            return
        }
        receiverNote = nil
        let settings = settings
        Task { [weak self] in
            let outcome = await settings.write(AmModulationReading.feedbackReceiverKey, String(receiver))
            if case .rejected(let reason) = outcome, !reason.isEmpty {
                self?.receiverNote = reason
            }
        }
    }

    /// The receiver `text` names: text that is not a number in range is rx1, as the Core reads it.
    static func receiver(_ text: String?) -> Int {
        guard let text, let value = Int(text.trimmingCharacters(in: .whitespaces)),
              AmModulationReading.feedbackReceivers.contains(value) else {
            return AmModulationReading.defaultFeedbackReceiver
        }
        return value
    }

    // MARK: Inside

    private static func clamp(_ value: Int, _ range: ClosedRange<Int>) -> Int {
        min(max(value, range.lowerBound), range.upperBound)
    }

    private func queueRefresh() {
        guard !refreshQueued else {
            return
        }
        refreshQueued = true
        Task { @MainActor [weak self] in
            self?.refreshQueued = false
            self?.refresh()
        }
    }

    private func watch(_ key: String) -> MirrorObject? {
        let object = mirror.object(key)
        let id = object.map(ObjectIdentifier.init)
        if watchedObjects[key] != id {
            watchedObjects[key] = id
            objectWatches[key] = object?.$values.dropFirst().sink { [weak self] _ in self?.queueRefresh() }
        }
        return object
    }

    func refresh() {
        let state = watch(TransmitModel.txStateKey)
        let pureSignal = watch(TransmitModel.pureSignalKey)

        // The transmitting slice: the Core's `txSliceId`, else the slice
        // marked for transmit, else the active slice.
        var entry: BandSlicesModel.Entry?
        if case .int(let id)? = state?["txSliceId"], id >= 0 {
            entry = slices.entries.first { $0.id == Int(id) }
        }
        entry = entry ?? slices.entries.first { $0.slice.txSlice } ?? slices.active
        let mode = entry?.modeLabel
        set(\.modeLabel, mode)
        set(\.sliceLetter, entry?.slice.letter)
        set(\.shown, mode.map(Self.amFamily.contains) ?? false)

        let next: Availability
        if !mirror.isSnapshotComplete {
            next = .notConnected
        } else if mirror.capabilityVersion(AmModulationReading.capabilityName) >= 1 {
            next = .available
        } else {
            next = .olderCore
        }
        set(\.availability, next)

        var running = false
        if case .text(let json)? = pureSignal?["statusJson"] {
            running = PureSignalStatus.parse(json)?.enabled ?? false
        }
        set(\.pureSignalRunning, running)
        set(\.feedbackReceiver, Self.receiver(settings.value(AmModulationReading.feedbackReceiverKey)))

        updateWatch()
        let record = availability == .available ? records.records(source.stream).last : nil
        set(\.reading, record.flatMap(AmModulationReading.init(record:)))
        if availability != .available {
            set(\.posLit, false)
            set(\.negLit, false)
            set(\.note, nil)
        } else if let reading, reading.carrierPresent {
            // The flashers latch when a window's peak reaches this phone's threshold.
            if reading.posPeakPct >= Double(posFlashPct) {
                set(\.posLit, true)
            }
            if reading.negPeakPct >= Double(negFlashPct) {
                set(\.negLit, true)
            }
            if (posLit || negLit), note == Self.clearedText {
                // Lit again since the RESET: the cleared line no longer holds.
                set(\.note, nil)
            }
        }
    }

    /// Asks the Core for the watched source's stream while a monitor is on
    /// screen and can show it, and stops it otherwise.
    private func updateWatch() {
        let wanted = onScreen > 0 && shown && availability == .available ? source.stream : nil
        guard wanted != watching else {
            return
        }
        if let watching {
            records.unwant(watching, by: self)
        }
        watching = wanted
        if let wanted {
            records.want(wanted, backlog: AmModulationReading.capacity, by: self)
        }
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<ModMonitorModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }
}
