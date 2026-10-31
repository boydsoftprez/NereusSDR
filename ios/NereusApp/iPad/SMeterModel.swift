// NereusSDR for iOS: the analog S-meter's model: the Core's readings, the menu's choices kept on this device, the needle's swing
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-26: CW transmit, Max Bin reads the passband from its origin, as the
//             desktop's peak in the passband does (R-CW-24). J.J. Boyd (KG4VCF),
//             AI-assisted via Claude Code.
// 2026-10-31: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
//   bounded passive MAX eligibility and display identity diagnostics.

import Combine
import Foundation
import NereusBand
import NereusMirror
import NereusModels
import os

/// The analog S-meter (D86): it feeds ``SMeterState`` the active slice's
/// receive readings and the radio's transmit readings as the Core sends
/// them, carries out the menu's choices and keeps them on this device,
/// and swings the needle 30 times a second while the meter is on screen
/// and anything on it moves.
@MainActor
final class SMeterModel: ObservableObject {
    /// The menu's choices, the readings and the peaks.
    @Published private(set) var state: SMeterState
    /// Where the needle is drawn, swinging toward the display's needle.
    @Published private(set) var needle = SMeterNeedle()
    /// The Core's scales, from its catalogue.
    @Published private(set) var meters: StationCatalog.Meters?

    private var slices: BandSlicesModel
    private var band: BandModel
    private var subscriber: BandSubscriber
    private let transmit: TransmitModel
    private let store: SMeterSettingsStore
    private let clock: () -> Double
    private var watches: Set<AnyCancellable> = []
    private var sliceWatches: Set<AnyCancellable> = []
    private var frames: Timer?
    private var lastFrame: Double?
    private var visible = 0
    /// Passive evidence captured while the flag/menu is visible, before
    /// navigating to Tools can change the display subscription.
    var maxBinDiagnostic: (MaxBinDiagnostic, Bool) -> Void = MaxBinDiagnostic.log
    private var maxBinDiagnosticLimiter = MaxBinDiagnosticLimiter()

    init(slices: BandSlicesModel, band: BandModel, subscriber: BandSubscriber,
         transmit: TransmitModel, catalogFeed: CatalogFeed,
         store: SMeterSettingsStore = SMeterSettingsStore(),
         clock: @escaping () -> Double = { ProcessInfo.processInfo.systemUptime }) {
        self.slices = slices
        self.band = band
        self.subscriber = subscriber
        self.transmit = transmit
        self.store = store
        self.clock = clock
        state = SMeterState(settings: store.settings)
        catalogFeed.$catalog.sink { [weak self] catalog in
            self?.meters = catalog?.meters
            self?.queueRefresh()
        }.store(in: &watches)
        watch(slices)
        transmit.objectWillChange.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        refresh()
    }

    private func watch(_ slices: BandSlicesModel) {
        sliceWatches = []
        slices.$entries.sink { [weak self] _ in self?.queueRefresh() }.store(in: &sliceWatches)
        slices.$activeSliceId.sink { [weak self] _ in self?.queueRefresh() }.store(in: &sliceWatches)
    }

    /// The meter reads the active slice of another pan from now on: on the
    /// iPad, the pan last touched (R-IOS-59), its band and its subscription.
    func follow(slices: BandSlicesModel, band: BandModel, subscriber: BandSubscriber) {
        guard slices !== self.slices || band !== self.band || subscriber !== self.subscriber else {
            return
        }
        self.slices = slices
        self.band = band
        self.subscriber = subscriber
        watch(slices)
        refresh()
    }

    /// The meter at this moment.
    var display: SMeterDisplay {
        state.display(at: clock(), meters: meters)
    }

    /// Why each mode is off, nil where it can be shown.
    func reason(for mode: SMeterRxMode) -> String? {
        state.readings.reason(for: mode)
    }

    func reason(for mode: SMeterTxMode) -> String? {
        state.readings.reason(for: mode)
    }

    // MARK: The menu

    func chooseRx(_ mode: SMeterRxMode) {
        state.chooseRx(mode, at: clock())
        changed()
    }

    func chooseTx(_ mode: SMeterTxMode) {
        state.chooseTx(mode)
        changed()
    }

    func setPeakHold(_ on: Bool) {
        state.setPeakHold(on)
        changed()
    }

    func choosePeakDecay(_ decay: SMeterPeakDecay) {
        state.choosePeakDecay(decay)
        changed()
    }

    func resetPeak() {
        state.resetPeak()
        startFrames()
    }

    func chooseFace(_ face: SMeterFace) {
        state.chooseFace(face)
        changed()
    }

    /// The Multimeter page's Display units: every printed signal reading follows.
    func chooseUnit(_ unit: SMeterUnit) {
        state.chooseUnit(unit)
        changed()
    }

    /// The Multimeter page's Show decimal point in readouts.
    func setShowDecimal(_ on: Bool) {
        state.setShowDecimal(on)
        changed()
    }

    private func changed() {
        store.setSettings(state.settings)
        startFrames()
    }

    // MARK: On screen

    /// A meter came on screen: the needle swings while it shows.
    func appeared() {
        visible += 1
        startFrames()
    }

    func disappeared() {
        visible = max(visible - 1, 0)
        if visible == 0 {
            stopFrames()
        }
    }

    /// One frame: the peaks' clock runs on and the needle swings toward
    /// the display's needle; with nothing left moving the frames stop.
    func frame() {
        let now = clock()
        let seconds = lastFrame.map { now - $0 } ?? SMeterNeedle.frameSeconds
        lastFrame = now
        state.advance(to: now)
        let shown = state.display(at: now, meters: meters)
        needle.step(toward: shown.needle, seconds: seconds)
        if needle.settled(on: shown.needle), !peaksMoving(at: now) {
            stopFrames()
        }
    }

    /// Something on the meter still moves by itself: the held peak falling
    /// in Signal Peak, or the peak hold line falling.
    private func peaksMoving(at now: Double) -> Bool {
        let peaks = state.peaks
        guard let level = peaks.level, !state.readings.transmitting else {
            return false
        }
        if state.settings.rxMode == .signalPeak, let peak = peaks.peak, peak > level {
            return true
        }
        if let hold = peaks.hold(at: now), hold > level + 0.01 {
            return true
        }
        return false
    }

    private func startFrames() {
        guard visible > 0, frames == nil else {
            return
        }
        lastFrame = clock()
        let timer = Timer(timeInterval: SMeterNeedle.frameSeconds, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.frame()
            }
        }
        RunLoop.main.add(timer, forMode: .common)
        frames = timer
    }

    private func stopFrames() {
        frames?.invalidate()
        frames = nil
        lastFrame = nil
    }

    // MARK: The Core's readings

    private var refreshQueued = false

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

    /// The Core's latest readings, fed to the meter.
    func refresh() {
        let active = slices.active
        let maxBin = maxBinReading(for: active)
        var readings = SMeterReadings(
            peakDbm: active?.signalPeakDbm, averageDbm: active?.signalAverageDbm,
            maxBinDbm: maxBin.value, maxBinAvailable: maxBin.available,
            transmitting: transmit.report.keyed || transmit.coreOnAir || transmit.ptt.transmitting,
            transmitSent: transmit.readingsSent,
            txReadingsVersion: transmit.txReadingsVersion, forwardWatts: transmit.forwardWatts, swr: transmit.swr,
            micLevelDb: transmit.micLevelDb, compressionDb: transmit.compressionDb,
            compressionSent: transmit.compressionDb != nil)
        readings.meterRule = transmit.meterRule
        state.apply(readings, at: clock())
        startFrames()
    }

    /// The phone's one band must be the live display accepted for this
    /// active slice and its pan. A cached frame from a former subscription
    /// or media session cannot light Max Bin.
    private func maxBinReading(for active: BandSlicesModel.Entry?) -> (available: Bool, value: Double?) {
        maxBinReading(for: active, subscribedSliceOnly: true)
    }

    // MARK: The flags

    /// The level `entry`'s flag shows: the reading the S-meter's RX Mode
    /// chooses (``FlagMeterChoice``), as a desktop window's flags follow its
    /// S-meter; nil for none.
    func flagLevel(for entry: BandSlicesModel.Entry) -> Double? {
        let mode = state.settings.rxMode
        let maxBin = FlagMeterChoice(mode) == .maxBin ? flagMaxBin(for: entry).value : nil
        return FlagMeterChoice.level(mode: mode, peakDbm: entry.signalPeakDbm, averageDbm: entry.signalAverageDbm,
                                     legacyDbm: entry.signalDbm, maxBinDbm: maxBin)
    }

    /// Max Bin for `entry`'s flag: the strongest point of its passband on
    /// the pan this device draws, when that is the slice's own pan.
    func flagMaxBin(for entry: BandSlicesModel.Entry) -> (available: Bool, value: Double?) {
        let reading = maxBinReading(for: entry, subscribedSliceOnly: false)
        recordMaxBinDiagnostic(for: entry, reading: reading)
        return reading
    }

    /// Why Max Bin is off on `entry`'s flag, nil when it can be shown: the
    /// slice's pan is not drawn here, or it is but the slice's passband is
    /// outside the span it shows.
    func flagMaxBinReason(for entry: BandSlicesModel.Entry) -> String? {
        if flagMaxBin(for: entry).available {
            return nil
        }
        return maxBinPanShown(for: entry, subscribedSliceOnly: false)
            ? SMeterReadings.maxBinOutsideSpanText : SMeterReadings.maxBinUnavailableText
    }

    /// The flags' choice of reading, from the more menu: it sets the
    /// S-meter's RX Mode, kept on this device as the meter's menu keeps it.
    func chooseFlagMeter(_ choice: FlagMeterChoice) {
        chooseRx(choice.rxMode(from: state.settings.rxMode))
    }

    /// `subscribedSliceOnly`: the accepted display must be the one this
    /// slice subscribed (the meter's active slice); a flag takes any slice
    /// on the drawn pan, whose trace is that pan's.
    private func maxBinReading(for active: BandSlicesModel.Entry?,
                               subscribedSliceOnly: Bool) -> (available: Bool, value: Double?) {
        guard let active, maxBinPanShown(for: active, subscribedSliceOnly: subscribedSliceOnly),
              let frame = band.state.frame,
              let coverage = band.state.frameCoverage else {
            return (false, nil)
        }
        let slice = active.slice
        let value = SMeterMaxBin.measure(trace: frame.traceDbm, centerHz: coverage.centerHz,
                                         spanHz: coverage.spanHz,
                                         lowHz: slice.passbandOriginHz + slice.filterLowHz,
                                         highHz: slice.passbandOriginHz + slice.filterHighHz)
        return (value != nil, value)
    }

    /// Whether this device shows a live frame of `active`'s pan that Max Bin
    /// may read, whatever span it covers.
    private func maxBinPanShown(for active: BandSlicesModel.Entry?, subscribedSliceOnly: Bool) -> Bool {
        maxBinPanGate(for: active, subscribedSliceOnly: subscribedSliceOnly) == .eligible
    }

    /// The same eligibility checks, in their original order. The first
    /// failed check is also the diagnostic; no identity guard is relaxed.
    private func maxBinPanGate(for active: BandSlicesModel.Entry?, subscribedSliceOnly: Bool) -> MaxBinGate {
        guard let active else { return .sliceMissing }
        guard let panKey = active.panKey, !panKey.isEmpty else { return .slicePanMissing }
        guard let accepted = subscriber.acceptedDisplay else { return .acceptedDisplayMissing }
        guard !subscribedSliceOnly || accepted.subscription.sliceId == active.id else { return .subscribedSliceMismatch }
        guard accepted.panKey == panKey else { return .panMismatch }
        guard accepted.subscription.endpointId == band.endpointId else { return .acceptedEndpointMismatch }
        guard accepted.subscription.revision == band.frameRevision else { return .committedRevisionMismatch }
        guard let frame = band.state.frame else { return .frameMissing }
        guard band.state.frameCoverage != nil else { return .coverageMissing }
        guard frame.endpointId == band.endpointId else { return .frameEndpointMismatch }
        guard !band.paused else { return .paused }
        guard !band.showsTransmit else { return .transmitDisplay }
        return .eligible
    }

    private func recordMaxBinDiagnostic(for entry: BandSlicesModel.Entry,
                                        reading: (available: Bool, value: Double?)) {
        let accepted = subscriber.acceptedDisplay
        let frame = band.state.frame
        let coverage = band.state.frameCoverage
        let association = maxBinPanGate(for: entry, subscribedSliceOnly: false)
        var snapshot = MaxBinDiagnostic(sliceId: entry.id,
            gate: association == .eligible && !reading.available ? .measurementUnavailable : association)
        snapshot.slicePanPresent = entry.panKey.map { !$0.isEmpty } ?? false
        snapshot.acceptedPresent = accepted != nil
        snapshot.acceptedSliceId = accepted?.subscription.sliceId
        snapshot.acceptedEndpoint = accepted?.subscription.endpointId
        snapshot.acceptedRevision = accepted?.subscription.revision
        snapshot.subscriberState = subscriber.displayDiagnostic
        snapshot.panMatches = accepted.map { $0.panKey == entry.panKey }
        snapshot.bandEndpoint = band.endpointId
        snapshot.frameEndpoint = frame?.endpointId
        snapshot.frameGeneration = frame?.contextGeneration
        snapshot.frameRevision = band.frameRevision
        snapshot.coveragePresent = coverage != nil
        snapshot.paused = band.paused
        snapshot.transmitDisplay = band.showsTransmit
        snapshot.traceCount = frame?.traceDbm.count ?? 0
        snapshot.frequencyHz = entry.slice.frequencyHz
        snapshot.passbandOriginHz = entry.slice.passbandOriginHz
        snapshot.filterLowHz = entry.slice.filterLowHz
        snapshot.filterHighHz = entry.slice.filterHighHz
        snapshot.coverageCenterHz = coverage?.centerHz
        snapshot.coverageSpanHz = coverage?.spanHz
        snapshot.valuePresent = SMeterReadings.reading(reading.value) != nil
        snapshot.mode = state.settings.rxMode
        if let repeated = maxBinDiagnosticLimiter.record(snapshot, at: clock()) {
            maxBinDiagnostic(snapshot, repeated)
        }
    }

    enum MaxBinGate: String, Sendable {
        case eligible, sliceMissing, slicePanMissing, acceptedDisplayMissing, subscribedSliceMismatch
        case panMismatch, acceptedEndpointMismatch, committedRevisionMismatch, frameMissing, coverageMissing
        case frameEndpointMismatch, paused, transmitDisplay, measurementUnavailable
    }

    /// Scalars only: no raw pan keys, trace arrays or connection identifiers.
    /// Value presence participates in deduplication; changing dBm does not.
    struct MaxBinDiagnostic: Equatable, Sendable {
        var sliceId: Int
        var gate: MaxBinGate
        var slicePanPresent = false
        var acceptedPresent = false
        var acceptedSliceId: Int?
        var acceptedEndpoint: UInt32?
        var acceptedRevision: UInt32?
        var subscriberState = BandSubscriber.DisplayDiagnostic(mediaUp: false, accepted: nil, requested: nil,
                                                               outstandingOperations: 0, asking: false)
        var panMatches: Bool?
        var bandEndpoint: UInt32?
        var frameEndpoint: UInt32?
        var frameGeneration: UInt32?
        var frameRevision: UInt32?
        var coveragePresent = false
        var paused = false
        var transmitDisplay = false
        var traceCount = 0
        var frequencyHz: Double?
        var passbandOriginHz: Double?
        var filterLowHz: Double?
        var filterHighHz: Double?
        var coverageCenterHz: Double?
        var coverageSpanHz: Double?
        var valuePresent = false
        var mode: SMeterRxMode = .signal

        /// All nonfinite geometry has one fingerprint, so a persistent NaN
        /// does not look like a new tuple on every evaluation.
        var fingerprint: Self {
            var copy = self
            func stable(_ value: Double?) -> Double? { value.map { $0.isFinite ? $0 : .infinity } }
            copy.frequencyHz = stable(frequencyHz)
            copy.passbandOriginHz = stable(passbandOriginHz)
            copy.filterLowHz = stable(filterLowHz)
            copy.filterHighHz = stable(filterHighHz)
            copy.coverageCenterHz = stable(coverageCenterHz)
            copy.coverageSpanHz = stable(coverageSpanHz)
            return copy
        }

        private static let logger = Logger(subsystem: "NereusSDR", category: "meter.max")

        func logText(repeated: Bool) -> String {
            func integer<T: BinaryInteger>(_ value: T?) -> String { value.map(String.init) ?? "absent" }
            func number(_ value: Double?) -> String {
                guard let value else { return "absent" }
                return value.isFinite ? String(format: "%.3f", value) : "nonfinite"
            }
            let raw = subscriberState
            let holding = "mediaUp=\(raw.mediaUp ? 1 : 0) rawAccepted=\(raw.accepted == nil ? 0 : 1) rawAcceptedSlice=\(integer(raw.accepted?.sliceId)) rawAcceptedEndpoint=\(integer(raw.accepted?.endpointId)) rawAcceptedRevision=\(integer(raw.accepted?.revision)) requested=\(raw.requested == nil ? 0 : 1) requestedSlice=\(integer(raw.requested?.sliceId)) requestedEndpoint=\(integer(raw.requested?.endpointId)) requestedRevision=\(integer(raw.requested?.revision)) outstanding=\(raw.outstandingOperations) asking=\(raw.asking ? 1 : 0)"
            return "gate=\(gate.rawValue) slice=\(sliceId) slicePan=\(slicePanPresent ? 1 : 0) accepted=\(acceptedPresent ? 1 : 0) acceptedSlice=\(integer(acceptedSliceId)) acceptedEndpoint=\(integer(acceptedEndpoint)) acceptedRevision=\(integer(acceptedRevision)) \(holding) panMatch=\(panMatches.map { $0 ? "1" : "0" } ?? "absent") bandEndpoint=\(integer(bandEndpoint)) frameEndpoint=\(integer(frameEndpoint)) frameGeneration=\(integer(frameGeneration)) frameRevision=\(integer(frameRevision)) coverage=\(coveragePresent ? 1 : 0) paused=\(paused ? 1 : 0) tx=\(transmitDisplay ? 1 : 0) traceCount=\(traceCount) frequencyHz=\(number(frequencyHz)) originHz=\(number(passbandOriginHz)) filterLowHz=\(number(filterLowHz)) filterHighHz=\(number(filterHighHz)) coverageCenterHz=\(number(coverageCenterHz)) coverageSpanHz=\(number(coverageSpanHz)) valuePresent=\(valuePresent ? 1 : 0) mode=\(mode.rawValue) repeated=\(repeated ? 1 : 0)"
        }

        static func log(_ snapshot: Self, repeated: Bool) {
            logger.info("Max Bin \(snapshot.logText(repeated: repeated), privacy: .public)")
        }
    }

    struct MaxBinDiagnosticLimiter {
        private struct Last {
            var snapshot: MaxBinDiagnostic
            var emittedAt: Double
            var repeated = false
        }
        private var last: [Int: Last] = [:]

        /// nil suppresses; false is a changed tuple; true is the single
        /// repeated unavailable tuple proving that its failure persisted.
        mutating func record(_ snapshot: MaxBinDiagnostic, at now: Double) -> Bool? {
            guard now.isFinite else { return nil }
            if let previous = last[snapshot.sliceId] {
                guard now - previous.emittedAt >= 1 else { return nil }
                if previous.snapshot.fingerprint == snapshot.fingerprint {
                    guard snapshot.gate != .eligible, !previous.repeated else { return nil }
                    last[snapshot.sliceId] = Last(snapshot: snapshot, emittedAt: now, repeated: true)
                    return true
                }
            }
            last[snapshot.sliceId] = Last(snapshot: snapshot, emittedAt: now)
            return false
        }
    }
}
