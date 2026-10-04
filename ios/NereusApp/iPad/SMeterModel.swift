// NereusSDR for iOS: the analog S-meter's model: the Core's readings, the menu's choices kept on this device, the needle's swing
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMirror
import NereusModels

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

    private let slices: BandSlicesModel
    private let band: BandModel
    private let subscriber: BandSubscriber
    private let transmit: TransmitModel
    private let store: SMeterSettingsStore
    private let clock: () -> Double
    private var watches: Set<AnyCancellable> = []
    private var frames: Timer?
    private var lastFrame: Double?
    private var visible = 0

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
        }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        slices.$activeSliceId.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
        transmit.objectWillChange.sink { [weak self] _ in self?.queueRefresh() }.store(in: &watches)
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
        let readings = SMeterReadings(
            peakDbm: active?.signalPeakDbm, averageDbm: active?.signalAverageDbm,
            maxBinDbm: maxBin.value, maxBinAvailable: maxBin.available,
            transmitting: transmit.report.keyed || transmit.coreOnAir || transmit.ptt.transmitting,
            transmitSent: transmit.readingsSent,
            txReadingsVersion: transmit.txReadingsVersion, forwardWatts: transmit.forwardWatts, swr: transmit.swr,
            micLevelDb: transmit.micLevelDb, compressionDb: transmit.compressionDb,
            compressionSent: transmit.compressionDb != nil)
        state.apply(readings, at: clock())
        startFrames()
    }

    /// The phone's one band must be the live display accepted for this
    /// active slice and its pan. A cached frame from a former subscription
    /// or media session cannot light Max Bin.
    private func maxBinReading(for active: BandSlicesModel.Entry?) -> (available: Bool, value: Double?) {
        guard let active, let panKey = active.panKey, !panKey.isEmpty,
              let accepted = subscriber.acceptedDisplay,
              accepted.subscription.sliceId == active.id,
              accepted.panKey == panKey,
              accepted.subscription.endpointId == band.endpointId,
              accepted.subscription.revision == band.frameRevision,
              let frame = band.state.frame,
              let coverage = band.state.frameCoverage,
              frame.endpointId == band.endpointId,
              !band.paused, !band.showsTransmit else {
            return (false, nil)
        }
        let slice = active.slice
        let value = SMeterMaxBin.measure(trace: frame.traceDbm, centerHz: coverage.centerHz,
                                         spanHz: coverage.spanHz,
                                         lowHz: slice.frequencyHz + slice.filterLowHz,
                                         highHz: slice.frequencyHz + slice.filterHighHz)
        return (value != nil, value)
    }
}
