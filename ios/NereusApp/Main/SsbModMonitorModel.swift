// NereusSDR for iOS: the SSB Mod Monitor on the TX panel: the Core's readings shown as sent, with this phone's OVER threshold
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMirror
import os

/// The SSB Mod Monitor (link document sections 6.3, 7.7 and 9.1): shown on
/// the TX panel, below its usual controls, while the transmitting slice is in
/// USB or LSB, as the AM monitor is shown by its modes. It watches the Core's
/// `txSsbModulation` or `txSsbModulationFeedback` stream only while it is on
/// screen and draws each record as sent; the Core does all the measuring. It
/// sends RESET to the Core (`txSsbModMonitor.reset`) for every device watching.
///
/// The source, the OVER threshold and the meter style are this phone's own.
@MainActor
final class SsbModMonitorModel: ObservableObject {
    typealias Source = SsbModulationReading.Source
    typealias Availability = ModMonitorModel.Availability
    typealias MeterStyle = ModMonitorModel.MeterStyle

    /// The signal lamp, in the desktop's order.
    enum Lamp: Equatable {
        /// No readings to judge (the monitor is greyed).
        case blank
        case noSignal
        case over
        case on

        var text: String {
            switch self {
            case .blank:
                return "--"
            case .noSignal:
                return "NO RF"
            case .over:
                return "OVER"
            case .on:
                return "ON"
            }
        }
    }

    // MARK: The desktop's words

    static let olderCoreText = "This Core does not send the SSB modulation monitor. Updating the Core may help."
    static let notConnectedText = "Connect to the Core to see the SSB modulation monitor."
    static let pureSignalOffText = "PA FB reads PureSignal\u{2019}s feedback. PureSignal is off, or this radio does not have it."
    static let waitingText = "Readings appear while the radio transmits in USB or LSB."
    static let crestFootText = "CREST is peak over RMS of the envelope, over the last 300 ms of signal. Pauses are left out."
    static let resetFootText = "RESET clears the peak hold and the OVER lamp for every device watching."
    static let clearedText = "Peak hold and OVER lamp cleared."
    static let defaultsText = "Desktop defaults: OVER at -1 dBFS, Meters. Each device keeps its own."

    /// The modes whose transmission the monitor measures.
    static let ssbFamily: Set<String> = ["USB", "LSB"]
    /// The OVER threshold's range and the desktop's default.
    static let overRange = -20...0
    static let defaultOver = -1
    /// The bars' scale, yellow and red zones, and ticks (the desktop's gauges).
    static let scale = (max: 120.0, yellow: 80.0, red: 100.0, ticks: [0, 50, 100, 120])
    /// The meter styles, needles first, as the desktop starts.
    static let styles: [MeterStyle] = [.meters, .bars]

    /// This phone's keys, under ``PhoneSettings/keyPrefix``.
    static let sourceKey = "ssbModMonitor.source"
    static let overKey = "ssbModMonitor.overDbfs"
    static let styleKey = "ssbModMonitor.meterStyle"

    // MARK: State

    /// The transmitting slice is in USB or LSB: the TX panel shows the monitor.
    @Published private(set) var shown = false
    /// The transmitting slice's mode and letter, for the title's chip.
    @Published private(set) var modeLabel: String?
    @Published private(set) var sliceLetter: String?
    @Published private(set) var availability = Availability.notConnected
    /// The Core's newest record for the watched source; nil while it sends none.
    @Published private(set) var reading: SsbModulationReading?
    @Published private(set) var source: Source
    @Published private(set) var overDbfs: Int
    @Published private(set) var meterStyle: MeterStyle
    /// PureSignal runs on the Core's radio: PA FB can be watched.
    @Published private(set) var pureSignalRunning = false
    /// The cleared line, or the Core's words refusing RESET.
    @Published private(set) var note: String?

    private let mirror: MirrorStore
    private let commands: CommandClient?
    private let records: RecordStreamClient
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
    private static let logger = Logger(subsystem: "NereusSDR", category: "tx.ssbModMonitor")

    init(mirror: MirrorStore, commands: CommandClient?, records: RecordStreamClient, slices: BandSlicesModel,
         phone: PhoneSettings) {
        self.mirror = mirror
        self.commands = commands
        self.records = records
        self.slices = slices
        self.phone = phone
        source = Source(rawValue: Int64(phone.integer(Self.sourceKey, default: 0))) ?? .txIq
        overDbfs = Self.clamp(phone.integer(Self.overKey, default: Self.defaultOver), Self.overRange)
        meterStyle = MeterStyle(rawValue: phone.string(Self.styleKey, default: MeterStyle.meters.rawValue)) ?? .meters
        mirror.$objectKeys.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        mirror.$isSnapshotComplete.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        records.$streams.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        records.$available.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
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

    /// The monitor's readings are shown live: the Core reports a signal.
    var live: Bool {
        availability == .available && reading?.signalPresent == true
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

    /// Why PA FB cannot be chosen now; nil while PureSignal runs.
    var paFeedbackReason: String? {
        pureSignalRunning ? nil : Self.pureSignalOffText
    }

    /// "USB · Slice A".
    var chipText: String? {
        guard let modeLabel, let sliceLetter else {
            return nil
        }
        return "\(modeLabel) \u{00B7} Slice \(sliceLetter)"
    }

    /// The held peak has reached this phone's threshold. It follows the
    /// Core's hold down, as on the desktop; it does not latch.
    var overLit: Bool {
        live && (reading?.peakHoldDbfs ?? -.infinity) >= Double(overDbfs)
    }

    var lamp: Lamp {
        guard availability == .available else {
            return .blank
        }
        guard live else {
            return .noSignal
        }
        return overLit ? .over : .on
    }

    /// The peak needle or bar: the higher of the window's peak and the hold.
    var peakBar: Double { live ? max(reading?.peakPct ?? 0, reading?.peakHoldPct ?? 0) : 0 }
    var avgBar: Double { live ? reading?.avgPct ?? 0 : 0 }
    /// The hold marker, nil when there is none to draw.
    var peakHold: Double? { live && (reading?.peakHoldPct ?? 0) > 0 ? reading?.peakHoldPct : nil }
    /// The envelope trace, empty without a signal.
    var scope: [Double] { live ? reading?.scopePct ?? [] : [] }

    var peakText: String { live ? Self.dbfsText(reading?.peakHoldDbfs ?? 0) : "--" }
    var avgText: String { live ? Self.dbfsText(reading?.avgDbfs ?? 0) : "--" }
    var crestText: String {
        guard live, let crest = reading?.crestDb else {
            return "--"
        }
        return String(format: "%.1f dB", crest)
    }
    var percentText: String { live ? ModMonitorModel.percentText(reading?.peakHoldPct ?? 0) : "--" }

    static func dbfsText(_ value: Double) -> String {
        ModMonitorModel.dbfsText(value)
    }

    /// Where a level in dBFS falls on the scope's percent scale, for its OVER line.
    static func percent(dbfs: Int) -> Double {
        100 * pow(10, Double(dbfs) / 20)
    }

    // MARK: Changing

    /// Watches `source`; PA FB only while PureSignal runs. A saved PA FB stays
    /// chosen with PureSignal off and reads nothing, as on the desktop. A new
    /// source starts its peaks afresh.
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

    /// RESET: the Core starts the source's analyzer afresh for every device watching.
    func reset() {
        reset(announce: true)
    }

    /// `announce` shows the Core's answer under the buttons; the reset a
    /// new source starts with says nothing unless the Core refuses it.
    private func reset(announce: Bool) {
        note = nil
        // Only a monitor on screen speaks to the Core, as on the desktop.
        guard enabled, shown, onScreen > 0, let commands else {
            return
        }
        let source = source
        Task { [weak self] in
            do {
                let result = try await commands.invoke(SsbModulationReading.resetVerb, arguments: [
                    CommandArgument(name: "source", value: .int(source.rawValue)),
                ], timeout: .seconds(5))
                guard let self, self.source == source else {
                    return
                }
                if !result.accepted {
                    self.note = result.reason
                } else if announce {
                    self.note = Self.clearedText
                }
            } catch {
                Self.logger.info("RESET was not answered: \(String(describing: error), privacy: .public)")
            }
        }
    }

    func setOver(_ dbfs: Int) {
        overDbfs = Self.clamp(dbfs, Self.overRange)
        phone.setInteger(overDbfs, for: Self.overKey)
    }

    func setMeterStyle(_ style: MeterStyle) {
        meterStyle = style
        phone.setString(style.rawValue, for: Self.styleKey)
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
        set(\.shown, mode.map(Self.ssbFamily.contains) ?? false)

        let next: Availability
        if !mirror.isSnapshotComplete {
            next = .notConnected
        } else if mirror.capabilityVersion(SsbModulationReading.capabilityName) >= 1 {
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

        updateWatch()
        let record = availability == .available ? records.records(source.stream).last : nil
        set(\.reading, record.flatMap(SsbModulationReading.init(record:)))
        if availability != .available {
            set(\.note, nil)
        } else if overLit, note == Self.clearedText {
            // Over again since the RESET: the cleared line no longer holds.
            set(\.note, nil)
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
            records.want(wanted, backlog: SsbModulationReading.capacity, by: self)
        }
    }

    private func set<Value: Equatable>(_ path: ReferenceWritableKeyPath<SsbModMonitorModel, Value>, _ value: Value) {
        if self[keyPath: path] != value {
            self[keyPath: path] = value
        }
    }
}
