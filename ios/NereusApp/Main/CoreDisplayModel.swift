// NereusSDR for iOS: the Core's display settings as its catalogue describes them: FFT size, window, Hz/bin, FPS, detectors and averaging
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
import NereusModels
import os

/// Passive, numeric-only evidence for the actual bin-width readout. The
/// phone log in Support Bundle already collects the NereusSDR subsystem.
/// No Core reason, connection identifier, address or arbitrary string is carried.
struct DisplayBinDiagnostic: Equatable, Sendable {
    enum Stage: String, Sendable { case offer, writeAttempt, writeQueued, writeOutcome, proxyValue, plan, subscription, subscriptionError, grant, readout, reset }
    enum Field: String, Sendable, Hashable { case size, target }
    enum WidthSource: String, Sendable { case granted, planned, absent }
    enum Outcome: String, Sendable { case accepted, rejected, notConfirmed, linkLost, notSent, keptOnThisDevice }
    var stage: Stage
    var field: Field? = nil
    var value: Double? = nil
    var fftSize: Int? = nil
    var floorSize: Double? = nil
    var rateHz: Double? = nil
    var spanHz: Double? = nil
    var pixels: Int? = nil
    var revision: UInt32? = nil
    var widthHz: Double? = nil
    var widthSource: WidthSource? = nil
    var limit: MediaControlEvent.Grant.Limit? = nil
    var outcome: Outcome? = nil
    var edit: UInt64? = nil
    var current: Bool? = nil

    private static let logger = Logger(subsystem: "NereusSDR", category: "display.bin")
    /// Fixed keys, enum tokens and bounded finite numbers only. -1 means absent.
    var logText: String {
        func number(_ value: Double?) -> String {
            guard let value, value.isFinite, value >= 0, value <= 1e10 else { return "-1" }
            return String(format: "%.6f", value)
        }
        return "stage=\(stage.rawValue) field=\(field?.rawValue ?? "none") value=\(number(value)) fft=\(number(fftSize.map(Double.init))) floor=\(number(floorSize)) rate=\(number(rateHz)) span=\(number(spanHz)) pixels=\(number(pixels.map(Double.init))) revision=\(revision ?? 0) width=\(number(widthHz)) widthSource=\(widthSource?.rawValue ?? "absent") limit=\(limit?.rawValue ?? "absent") outcome=\(outcome?.rawValue ?? "none") edit=\(edit ?? 0) current=\(current.map { $0 ? "1" : "0" } ?? "-1")"
    }
    static func readoutSource(granted: Double?, plannedRate: Double?, plannedSize: Int?) -> WidthSource {
        if let granted, granted.isFinite, granted > 0 { return .granted }
        guard let rate = plannedRate, rate.isFinite, rate > 0,
              let size = plannedSize, size > 0 else { return .absent }
        let width = rate / Double(size)
        return width.isFinite && width > 0 ? .planned : .absent
    }
    static func log(_ record: Self) {
        logger.info("Display bin \(record.logText, privacy: .public)")
    }
    static func field(for key: String) -> Field? {
        switch key {
        case "DisplayFftSize": return .size
        case "DisplayHzPerBinTarget": return .target
        default: return nil
        }
    }
    static func outcome(_ result: SettingsWriteOutcome) -> Outcome {
        switch result {
        case .accepted: return .accepted
        case .rejected: return .rejected
        case .notConfirmed: return .notConfirmed
        case .linkLost: return .linkLost
        case .notSent: return .notSent
        case .keptOnThisDevice: return .keptOnThisDevice
        }
    }
}

/// The Core's Setup > Display controls on this phone (R-IOS-06, R-IOS-18,
/// R-IOS-27; link document section 7.4, `display`). Each control is drawn
/// from the catalogue: its label, options or range, default and unit.
///
/// A `station` control (FFT size, window, Hz/bin target, FPS) is the Core's
/// own value, shared by every device: it is written with `settings.write`
/// and read back from the settings proxy. A `device` control (the spectrum
/// and waterfall detectors, averaging and decimation) is kept for the pan on
/// this phone and goes to the Core in the display subscription.
///
/// A Core whose catalogue has no `display` keeps the phone's earlier
/// behaviour: the station controls are greyed with ``olderCoreText``, and
/// the subscription asks with the desktop's defaults.
@MainActor
final class CoreDisplayModel: ObservableObject {
    /// Why the Core's display settings are greyed on a Core that does not describe them.
    static let olderCoreText = "This Core can't change its display settings from a phone. Updating the Core may help."
    /// Why a choice the Core lists is greyed when this app cannot send it.
    static let choiceNotSentText = "This app can't ask the Core for this choice."
    /// What the phone says when the Core refuses a setting without its own words.
    static let refusedText = "The Core did not change this setting."
    /// The readout's label on a Core that does not describe it.
    static let binWidthFallbackLabel = "Bin width"

    // The Core's settings (link 7.4; station keys in the surface's settingsScope).
    static let fftSizeKey = "DisplayFftSize"
    static let windowKey = "DisplayFftWindow"
    static let hzPerBinKey = "DisplayHzPerBinTarget"
    static let fpsKey = "DisplaySpectrumFps"
    static let stationKeys = [fftSizeKey, windowKey, hzPerBinKey, fpsKey]

    /// Set whenever what the controls show may have changed.
    @Published private(set) var revision = 0
    /// The Core's words for the last setting it refused.
    @Published private(set) var note: String?

    // Injectable observation only; it never participates in admission or planning.
    var binDiagnostic: (DisplayBinDiagnostic) -> Void = DisplayBinDiagnostic.log
    private var binSnapshotIdentity: UInt64?
    private var lastBinReadout: DisplayBinDiagnostic?
    private var lastBinOffer: DisplayBinDiagnostic?
    private var lastBinProxy: [DisplayBinDiagnostic.Field: DisplayBinDiagnostic] = [:]

    private let band: BandModel
    private let slices: BandSlicesModel
    private let subscriber: BandSubscriber
    private let catalogFeed: CatalogFeed
    private let settingsProxy: SettingsProxyClient
    private let changeDisplay: (_ change: (inout BandDisplaySettings) -> Void) -> Void
    /// Keys with a write the Core has not answered yet, and the newest value
    /// to write once it has: a slider sends one write at a time, then its
    /// latest value.
    private var writing: Set<String> = []
    private var queued: [String: (text: String, edit: UInt64, snapshot: UInt64)] = [:]
    private let outcomeOwner = ControlOutcomeOwner()
    private var watches: Set<AnyCancellable> = []
    private static let logger = Logger(subsystem: "NereusSDR", category: "display.core")

    init(band: BandModel, slices: BandSlicesModel, subscriber: BandSubscriber, catalogFeed: CatalogFeed,
         settingsProxy: SettingsProxyClient,
         changeDisplay: @escaping (_ change: (inout BandDisplaySettings) -> Void) -> Void) {
        self.band = band
        self.slices = slices
        self.subscriber = subscriber
        self.catalogFeed = catalogFeed
        self.settingsProxy = settingsProxy
        self.changeDisplay = changeDisplay
        catalogFeed.$catalog.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        settingsProxy.$currentSnapshotIdentity.sink { [weak self] identity in
            guard let self, identity != self.binSnapshotIdentity else { return }
            self.binSnapshotIdentity = identity
            self.lastBinOffer = nil
            self.lastBinProxy.removeAll()
            self.lastBinReadout = nil
        }.store(in: &watches)
        settingsProxy.$unconfirmedKeys.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        settingsProxy.$values.sink { [weak self] values in
            self?.recordBinProxy(values)
            self?.changed()
        }.store(in: &watches)
        band.$settings.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        band.$binWidthHz.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        subscriber.$plannedFftSize.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        subscriber.$plannedSampleRateHz.sink { [weak self] _ in self?.changed() }.store(in: &watches)
    }

    private func changed() {
        Task { @MainActor [weak self] in
            self?.revision &+= 1
            self?.recordBinReadout()
        }
    }

    // MARK: What the Core describes

    /// The Core's description; nil on a Core that sends none.
    var description: StationCatalog.Display? { catalogFeed.catalog?.display }

    /// The Core describes its display settings.
    var available: Bool { description != nil }

    /// The controls in the desktop pages' order, grouped by page and group.
    var groups: [(title: String, controls: [StationCatalog.Display.Control])] {
        var result: [(title: String, controls: [StationCatalog.Display.Control])] = []
        for control in description?.controls ?? [] {
            let title = control.group.isEmpty ? control.page : "\(control.page): \(control.group)"
            if let last = result.indices.last, result[last].title == title {
                result[last].controls.append(control)
            } else {
                result.append((title, [control]))
            }
        }
        return result
    }

    func control(_ key: String) -> StationCatalog.Display.Control? {
        description?.control(settingsKey: key)
    }

    /// Where a group's settings live (D16): the Core's, this phone's, or both.
    static func tag(of controls: [StationCatalog.Display.Control]) -> SetupTag {
        if controls.allSatisfy(\.isStation) {
            return .core
        }
        if controls.allSatisfy({ !$0.isStation }) {
            return .thisPhone
        }
        return .both
    }

    func isUnconfirmed(_ key: String) -> Bool { queued[key] == nil && settingsProxy.isUnconfirmed(key) }

    // MARK: Values

    /// The control's value now: the Core's setting (or its default) for a
    /// station control, this pan's for a device one.
    func value(_ control: StationCatalog.Display.Control) -> Double? {
        if control.isStation {
            guard let key = control.settingsKey else {
                return nil
            }
            let set = setting(key).flatMap { Double($0.trimmingCharacters(in: .whitespaces)) }
            guard let value = set ?? control.defaultValue, value.isFinite else {
                return nil
            }
            return value
        }
        return deviceValue(control.subscribe).map(Double.init)
    }

    /// Whether the control can be changed now, and if not, why.
    func reason(_ control: StationCatalog.Display.Control) -> String? {
        if case .other = control.kind {
            return Self.choiceNotSentText
        }
        if control.isStation {
            return control.settingsKey == nil ? Self.choiceNotSentText : nil
        }
        let gates = subscriber.gates
        switch control.subscribe {
        case Field.traceAveraging, Field.waterfallAveraging, Field.traceAverageTime, Field.waterfallAverageTime:
            return BandDisplaySettings.extrasAvailable(gates: gates) ? nil : CatalogFeed.needsNewerCoreText
        case Field.decimation:
            return gates.decimation ? nil : CatalogFeed.needsNewerCoreText
        default:
            return Self.deviceFields.contains(control.subscribe) ? nil : Self.choiceNotSentText
        }
    }

    /// Whether this app can send `option` for `control`.
    func canSend(_ option: StationCatalog.Display.Option, of control: StationCatalog.Display.Control) -> Bool {
        guard !control.isStation else {
            return true
        }
        return Self.deviceValueIsKnown(control.subscribe, Int(option.value.rounded()))
    }

    /// Sets the control to `value`, kept within it: a station control is
    /// written to the Core, a device one kept for the pan.
    func set(_ control: StationCatalog.Display.Control, to value: Double) {
        guard reason(control) == nil else {
            return
        }
        let kept = control.clamped(value)
        if let key = control.settingsKey, let field = DisplayBinDiagnostic.field(for: key) {
            let record = DisplayBinDiagnostic(stage: .offer, field: field, value: kept)
            if record != lastBinOffer {
                lastBinOffer = record
                binDiagnostic(record)
            }
        }
        if let option = control.options.first(where: { $0.value == kept }), !canSend(option, of: control) {
            return
        }
        if control.isStation {
            guard let key = control.settingsKey, self.value(control) != kept || settingsProxy.value(key) == nil else {
                return
            }
            write(key, Self.settingText(kept))
        } else {
            setDevice(control.subscribe, Int(kept.rounded()))
        }
    }

    /// A value as the Core's setting holds it: a whole number without a
    /// point, otherwise as few places as it needs.
    static func settingText(_ value: Double) -> String {
        if value == value.rounded(), abs(value) < 1e15 {
            return String(Int64(value))
        }
        var text = String(format: "%.6f", value)
        while text.hasSuffix("0") {
            text.removeLast()
        }
        if text.hasSuffix(".") {
            text.removeLast()
        }
        return text
    }

    /// A Core setting's value as the settings proxy holds it, or "".
    func settingValueText(_ key: String) -> String {
        setting(key) ?? ""
    }

    /// A Core setting as shown: a value waiting its turn behind a write the
    /// Core has not answered shows at once (`StationClient.cpp:1040-1068`).
    private func setting(_ key: String) -> String? {
        queued[key]?.text ?? settingsProxy.value(key)
    }

    // MARK: The FFT size and the bin width

    /// The Size control, which steps through its options.
    var sizeControl: StationCatalog.Display.Control? { control(Self.fftSizeKey) }
    /// The Hz/bin target.
    var hzPerBinControl: StationCatalog.Display.Control? { control(Self.hzPerBinKey) }

    /// The Size setting's place among its options, for a slider over them.
    var sizeIndex: Double? {
        guard let size = sizeControl, let value = value(size) else {
            return nil
        }
        let kept = size.clamped(value)
        return size.options.firstIndex { $0.value == kept }.map(Double.init)
    }

    /// The slider's range over the Size options; nil without them.
    var sizeIndexRange: StationCatalog.Range? {
        guard let count = sizeControl?.options.count, count > 1 else {
            return nil
        }
        return StationCatalog.Range(min: 0, max: Double(count - 1), step: 1)
    }

    /// The Size option at a slider's place.
    func sizeOption(at index: Double) -> StationCatalog.Display.Option? {
        guard let options = sizeControl?.options, !options.isEmpty, index.isFinite else {
            return nil
        }
        return options[min(max(Int(index.rounded()), 0), options.count - 1)]
    }

    func setSizeIndex(_ index: Double) {
        guard let size = sizeControl, let option = sizeOption(at: index) else {
            return
        }
        set(size, to: option.value)
    }

    /// The bin width readout's label: the Core's, or the phone's own.
    var binWidthLabel: String { description?.binWidth.label ?? Self.binWidthFallbackLabel }

    /// The width of one bin now, in hertz, to the readout's places: the
    /// granted size's once the Core grants one, the planned size's before;
    /// nil before either.
    var binWidthText: String? {
        let decimals = description?.binWidth.decimals ?? 3
        if let granted = band.binWidthHz, granted.isFinite, granted > 0 {
            return String(format: "%.\(max(0, decimals))f", granted)
        }
        guard let rate = subscriber.plannedSampleRateHz, let size = subscriber.plannedFftSize else {
            return nil
        }
        let width = description?.binWidth
            ?? StationCatalog.Display.BinWidth(label: Self.binWidthFallbackLabel, decimals: decimals)
        return width.text(sampleRateHz: rate, fftSize: size)
    }

    // MARK: Writing to the Core

    private func write(_ key: String, _ text: String) {
        let edit = outcomeOwner.begin()
        guard let snapshot = settingsProxy.currentSnapshotIdentity else {
            recordBinWrite(key, text, edit: edit, stage: .writeOutcome, outcome: .notSent)
            return
        }
        guard !writing.contains(key) else {
            recordBinWrite(key, text, edit: edit, stage: .writeQueued)
            queued[key] = (text, edit, snapshot)
            changed()
            return
        }
        startWrite(key, text, edit: edit, snapshot: snapshot)
    }

    private func startWrite(_ key: String, _ text: String, edit: UInt64, snapshot: UInt64) {
        writing.insert(key)
        recordBinWrite(key, text, edit: edit, stage: .writeAttempt)
        let proxy = settingsProxy
        Task { @MainActor [weak self] in
            let outcome = await proxy.writeBound(key, text, expectedSnapshotIdentity: snapshot,
                                                 authority: CommandSendPermit(), onLateOutcome: { [weak self] outcome in
                self?.recordBinWrite(key, text, edit: edit, stage: .writeOutcome, outcome: DisplayBinDiagnostic.outcome(outcome))
                self?.receive(outcome, edit: edit)
            })
            guard let self else { return }
            self.writing.remove(key)
            self.recordBinWrite(key, text, edit: edit, stage: .writeOutcome, outcome: DisplayBinDiagnostic.outcome(outcome))
            self.receive(outcome, edit: edit)
            if let next = self.queued.removeValue(forKey: key) {
                if outcome == .linkLost && !proxy.isCurrent(next.snapshot) {
                    // The newest old-session touch retires with that session.
                    self.recordBinWrite(key, next.text, edit: next.edit, stage: .writeOutcome, outcome: .linkLost)
                    self.receive(.linkLost, edit: next.edit)
                } else if proxy.isCurrent(next.snapshot), next.text != proxy.value(key) {
                    self.startWrite(key, next.text, edit: next.edit, snapshot: next.snapshot)
                } else if !proxy.isCurrent(next.snapshot) {
                    self.recordBinWrite(key, next.text, edit: next.edit, stage: .writeOutcome, outcome: .notSent)
                }
            }
            self.changed()
        }
    }

    private func recordBinReadout() {
        let source = DisplayBinDiagnostic.readoutSource(granted: band.binWidthHz,
                                                        plannedRate: subscriber.plannedSampleRateHz,
                                                        plannedSize: subscriber.plannedFftSize)
        let record = DisplayBinDiagnostic(stage: .readout, widthHz: binWidthText.flatMap { Double($0) },
                                          widthSource: source)
        if lastBinReadout != record {
            lastBinReadout = record
            binDiagnostic(record)
        }
    }

    private func recordBinWrite(_ key: String, _ text: String, edit: UInt64, stage: DisplayBinDiagnostic.Stage,
                                outcome: DisplayBinDiagnostic.Outcome? = nil) {
        guard let field = DisplayBinDiagnostic.field(for: key) else { return }
        binDiagnostic(.init(stage: stage, field: field, value: Double(text), outcome: outcome,
                            edit: edit, current: outcomeOwner.isCurrent(edit)))
    }

    private func recordBinProxy(_ values: [String: String]) {
        for key in [Self.fftSizeKey, Self.hzPerBinKey] {
            guard let field = DisplayBinDiagnostic.field(for: key) else { continue }
            let record = DisplayBinDiagnostic(stage: .proxyValue, field: field,
                                               value: values[key].flatMap { Double($0.trimmingCharacters(in: .whitespaces)) })
            if lastBinProxy[field] != record {
                lastBinProxy[field] = record
                binDiagnostic(record)
            }
        }
    }

    private func receive(_ outcome: SettingsWriteOutcome, edit: UInt64) {
        guard outcomeOwner.isCurrent(edit), !outcome.propertyOutcome.heldForQuestion else { return }
        switch outcome {
        case .rejected(let reason): note = reason.isEmpty ? Self.refusedText : reason
        case .notConfirmed:
            Self.logger.info("A display setting had no answer from the Core in time")
            note = PropertyWriteOutcome.notConfirmed.reason
        case .linkLost: note = PropertyWriteOutcome.linkLost.reason
        case .accepted, .keptOnThisDevice: note = nil
        case .notSent: break
        }
    }

    // MARK: This pan's controls

    /// The subscription fields a device control sets (link 7.4, `subscribe`).
    enum Field {
        static let traceDetector = "trace.detector"
        static let traceAveraging = "trace.averageMode"
        static let traceAverageTime = "averageTimeMs"
        static let decimation = "decimation"
        static let waterfallDetector = "waterfall.detector"
        static let waterfallAveraging = "waterfall.averageMode"
        static let waterfallAverageTime = "waterfallAverageTimeMs"
    }

    static let deviceFields: Set<String> = [
        Field.traceDetector, Field.traceAveraging, Field.traceAverageTime, Field.decimation,
        Field.waterfallDetector, Field.waterfallAveraging, Field.waterfallAverageTime,
    ]

    private func deviceValue(_ field: String) -> Int? {
        let settings = band.settings
        switch field {
        case Field.traceDetector:
            return settings.spectrumDetector.rawValue
        case Field.traceAveraging:
            return settings.spectrumAveraging.rawValue
        case Field.traceAverageTime:
            return settings.spectrumAverageTimeMs
        case Field.decimation:
            return settings.decimation
        case Field.waterfallDetector:
            return settings.waterfallDetector.rawValue
        case Field.waterfallAveraging:
            return settings.waterfallAveraging.rawValue
        case Field.waterfallAverageTime:
            return settings.waterfallAverageTimeMs
        default:
            return nil
        }
    }

    private static func deviceValueIsKnown(_ field: String, _ value: Int) -> Bool {
        switch field {
        case Field.traceDetector, Field.waterfallDetector:
            return SpectrumDetector(rawValue: value) != nil
        case Field.traceAveraging, Field.waterfallAveraging:
            return SpectrumAveraging(rawValue: value) != nil
        default:
            return deviceFields.contains(field)
        }
    }

    private func setDevice(_ field: String, _ value: Int) {
        guard deviceValue(field) != value else {
            return
        }
        switch field {
        case Field.traceDetector:
            guard let detector = SpectrumDetector(rawValue: value) else {
                return
            }
            changeDisplay { $0.spectrumDetector = detector }
        case Field.traceAveraging:
            guard let averaging = SpectrumAveraging(rawValue: value) else {
                return
            }
            changeDisplay { $0.spectrumAveraging = averaging }
        case Field.traceAverageTime:
            changeDisplay { $0.spectrumAverageTimeMs = value }
        case Field.decimation:
            changeDisplay { $0.decimation = value }
        case Field.waterfallDetector:
            guard let detector = SpectrumDetector(rawValue: value) else {
                return
            }
            changeDisplay { $0.waterfallDetector = detector }
        case Field.waterfallAveraging:
            guard let averaging = SpectrumAveraging(rawValue: value) else {
                return
            }
            changeDisplay { $0.waterfallAveraging = averaging }
        case Field.waterfallAverageTime:
            changeDisplay { $0.waterfallAverageTimeMs = value }
        default:
            Self.logger.info("A display control this app does not know was left alone")
        }
    }
}
