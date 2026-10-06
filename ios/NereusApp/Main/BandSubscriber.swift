// NereusSDR for iOS: asks the Core for the band's display, within the Core's display budget, reductions first
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMedia
import NereusMirror
import NereusModels
import os

/// Sends the band's display subscription (R-IOS-11; the media control
/// document's `subscribe`, and the display budget design's GUI lifecycle,
/// `docs/architecture/2026-09-22-session-display-budget-design.md`).
///
/// What it asks for: the active slice's stream, as wide as the band is in
/// pixels (``BandModel/requestedPixels``), at the centre and span the band
/// wants (``BandModel/requestedView`` after a zoom, a double tap or a pan,
/// else the active slice in the middle of all its sample rate), with the display
/// extras the phone's display settings ask for while the Core offers them.
/// The FFT size, window and frame rate are the Core's own settings
/// (`DisplayFftSize`, `DisplayFftWindow`, `DisplayHzPerBinTarget`,
/// `DisplaySpectrumFps`), read through the settings proxy. When the Core
/// has not set one, its catalogue's `display` gives the default and the
/// FFT sizes it computes (link 7.4); a Core without `display` gets the
/// desktop's defaults as before. When the active slice moves out
/// of the view, the view moves to keep it in the middle; a pan that leaves
/// it off the band stays.
///
/// How it asks, on the display budget wire: ``DisplayQualityAllocator``
/// settles the quality that fits the Core's budget; a reduction (or an
/// unsubscribe) is sent before anything that grows; nothing more is sent
/// while an operation waits for its `allocation-result`; an increase goes
/// only when it fits the headroom the Core has confirmed. A refused request
/// is not sent again until the budget or the wishes change. After 10 s with
/// no result the allocation is ``stalled``: increases stop, a reduction can
/// still go, and a late result reconciles it. Without the budget wire each
/// request goes as it changes and the Core's `rejected` says when one fails.
///
/// It asks for what the operator wants (the several-devices design, 9.3,
/// "Every client asks for what the operator wants"): at the start of each
/// media session, when the band wants more (at once for more frames or
/// planes, once a wider band has held its width for 200 ms), and when
/// ``transmitHolderChanged()`` says the transmit holder changed, it
/// subscribes the band as wanted, whatever the share; the Core's answer
/// ends the ask, and a refusal for the budget is answered by planning
/// inside the share the Core's capabilities carry. A new budget generation
/// alone never asks again. A display the Core refused and the band then
/// drops is unsubscribed, so it stops counting against the others.
@MainActor
final class BandSubscriber: ObservableObject {
    /// How the subscriber reaches the Core; the app's are its media client's.
    struct Operations {
        var subscribe: @MainActor (DisplaySubscription) async throws -> Void
        var unsubscribe: @MainActor (UInt32) async throws -> Void
        /// The Core's Re-tune of an endpoint's Clarity.
        var retuneClarity: @MainActor (UInt32) async throws -> Void = { _ in throw MediaControlError.noMediaConnection }
    }

    /// The pan the phone's band is, for the allocator's pan order.
    static let panId = "1"
    /// How long an operation waits for its `allocation-result` (the budget
    /// design's acknowledgment deadline).
    static let acknowledgementDeadline: Duration = .seconds(10)
    /// How long a band made wider must hold its width before it asks again.
    static let resizeSettle: Duration = .milliseconds(200)
    /// The waterfall's line period by default, the desktop's (its
    /// `SpectrumWidget` `m_wfUpdatePeriodMs`); the pan's own
    /// (``BandDisplaySettings/waterfallPeriodMs``) sets `framesPerLine`.
    static let waterfallPeriodMs = 30
    /// The Core's display settings' defaults, as the desktop's remote
    /// display reads them (`RemoteMediaController.cpp` `requestFor`).
    static let defaultFps = 30
    static let defaultFftSize = 4096
    /// `WindowFunction::BlackmanHarris4`.
    static let defaultWindowType = 1
    /// FFTEngine's largest size.
    static let largestFftSize = 262_144
    /// The quantisation window the desktop's remote display asks for.
    static let minDbm = -180.0
    static let maxDbm = 0.0
    /// `SpectrumDetectorMode::Peak`; `SpectrumAvenger` none and recursive logarithmic.
    static let peakDetector = 0
    static let noAveraging = 0
    static let logRecursiveAveraging = 3

    /// The quantisation window last asked for, kept while the Core's
    /// levels still fit it (``BandDisplaySettings/quantisationWindow(coreLevels:held:)``).
    private var heldWindow: ClosedRange<Double>?

    /// An operation's result is overdue: increases wait until it comes.
    @Published private(set) var stalled = false
    /// The FFT size the band asks for now, and the pan's sample rate, for
    /// the bin width readout before the Core's grant; nil before the first plan.
    @Published private(set) var plannedFftSize: Int?
    @Published private(set) var plannedSampleRateHz: Double?
    /// The frame rate the Core holds the band to while it is slowed below
    /// what the band asks for because the Core is shared or busy (D54): the
    /// Sharing chip's number. Nil while the band has what it asks for.
    @Published private(set) var sharingFps: Int?
    /// The quality the phone's half of the budget settled on.
    private(set) var allocator = DisplayQualityAllocator()
    /// What the long session allows the band (R-IOS-22, R-IOS-23): its data
    /// mode's frame rate and detail, or no display at all while the phone
    /// is locked with Sound only on, or on Audio only. The band never asks
    /// for more than this; with none it unsubscribes, and asks again for
    /// what it wants when the display is allowed back, so the Core's new
    /// context brings a keyframe.
    var session: SessionPolicy.Request = SessionPolicy.Mode.full.request {
        didSet {
            guard session != oldValue else {
                return
            }
            if session.subscribes && !oldValue.subscribes {
                beginAsking()
            } else {
                queueReplan()
            }
        }
    }

    private let band: BandModel
    private let slices: BandSlicesModel
    private let mirror: MirrorStore
    private let settings: SettingsProxyClient
    private let operations: Operations
    private let deadline: Duration
    private static let logger = Logger(subsystem: "NereusSDR", category: "band.subscription")

    /// What the Core holds for the band, or asks to hold.
    private enum Holding: Equatable {
        case none
        case endpoint(DisplaySubscription)
    }

    /// An operation sent on the budget wire, waiting for its result.
    private struct Operation {
        let endpointId: UInt32
        let revision: UInt32
        let holding: Holding
        let charge: DisplayQualityAllocator.Charge
        /// The pan key of the slice when this subscription was sent.
        var panKey: String? = nil
        /// Sent as the ask for what the band wants.
        var ask = false
    }

    private var mediaUp = false
    private var endpointId: UInt32 = 1
    private var revision: UInt32 = 0
    /// What the Core has confirmed it holds, and its charge.
    private var accepted: Holding = .none
    private var acceptedPanKey: String?
    /// The display the Core has accepted, with the pan it belongs to.
    var acceptedDisplay: (subscription: DisplaySubscription, panKey: String?)? {
        guard mediaUp, case .endpoint(let subscription) = accepted,
              case .endpoint(let wanted) = requested,
              wanted.endpointId == subscription.endpointId,
              wanted.sliceId == subscription.sliceId else {
            return nil
        }
        return (subscription, acceptedPanKey)
    }
    private var acceptedCharge = DisplayQualityAllocator.Charge()
    /// What the newest operation asked for.
    private var requested: Holding = .none
    /// The request the Core last refused, not sent again until the wishes change.
    private var refused: DisplaySubscription?
    private var outstanding: [Operation] = []
    private var deadlineTask: Task<Void, Never>?
    private var watches: Set<AnyCancellable> = []
    private var replanQueued = false
    /// The active slice and its frequency when the view was last planned.
    private var lastActive: (id: Int, hz: Double)?
    /// The band asks for what it wants, whatever the share, until the Core answers.
    private(set) var asking = false
    /// What the band wanted when last planned.
    private var lastWish: DisplayQualityAllocator.Intent?
    /// A wider band waiting to hold its width before it asks.
    private var settling: Task<Void, Never>?
    /// An endpoint the Core refused before holding it, still counted against
    /// the others until it is asked for again or unsubscribed.
    private var refusedEndpoint: UInt32?

    init(band: BandModel, slices: BandSlicesModel, mirror: MirrorStore, settings: SettingsProxyClient,
         operations: Operations, deadline: Duration = BandSubscriber.acknowledgementDeadline) {
        self.band = band
        self.slices = slices
        self.mirror = mirror
        self.settings = settings
        self.operations = operations
        self.deadline = deadline
        band.$requestedPixels.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        band.$requestedView.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        band.$settings.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        band.$coarseCoreLevels.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        band.$extendedSpanCeilingHz.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        band.$transmit.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        band.$catalog.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        // The 3D view's spectrum beside the pan follows what it draws.
        band.$stackOffered.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        band.$stackConditions.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        band.$stackStage.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        slices.$entries.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        slices.$activeSliceId.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        mirror.$capabilities.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
        settings.$values.sink { [weak self] _ in self?.queueReplan() }.store(in: &watches)
    }

    var binDiagnostic: (DisplayBinDiagnostic) -> Void = DisplayBinDiagnostic.log
    private var lastBinPlan: DisplayBinDiagnostic?

    // MARK: The Core's media events

    /// One of the media client's events.
    func receive(_ event: MediaControlEvent) {
        switch event {
        case .mediaState(.connected):
            if !mediaUp {
                startOver()
                mediaUp = true
                queueReplan()
            }
        case .mediaState(.closed), .mediaState(.failed):
            mediaUp = false
            startOver()
        case .allocationResult(let result):
            settle(result)

        case .rejected(let rejection):
            guard !rejection.refusesWholePeer, rejection.endpointId == endpointId,
                  case .endpoint(let held) = requested else {
                return
            }
            // Without the budget wire the Core refused the request outright.
            refused = Self.normalised(held)
            requested = .none
            accepted = .none
            acceptedPanKey = nil
            acceptedCharge = .init()
            // Another pan's slice the phone jumped to: back to its own band, with the Core's words.
            slices.displayRefused(sliceId: held.sliceId, reason: rejection.reason)
            queueReplan()
        default:
            break
        }
    }

    // MARK: Planning

    /// The media feature gates the Core and the agreed minor allow.
    var gates: MediaFeatureGates {
        MediaFeatureGates(agreedMinor: mirror.agreedMinor ?? 0) { [mirror] in mirror.capabilityVersion($0) }
    }

    /// The Core's display budget, or nil without one.
    var budget: DisplayQualityAllocator.Budget? {
        DisplayQualityAllocator.Budget(gates: gates, integer: { [mirror] name in
            switch mirror.capabilities[name] {
            case .int(let value)?, .enumeration(let value)?:
                return value
            default:
                return nil
            }
        }, text: { [mirror] name in
            if case .text(let value)? = mirror.capabilities[name] {
                return value
            }
            return nil
        })
    }

    /// The store and the settings publish before they change, so the plan
    /// is made on the main actor's next turn.
    private func queueReplan() {
        guard !replanQueued else {
            return
        }
        replanQueued = true
        Task { @MainActor [weak self] in
            self?.replanQueued = false
            self?.replan()
        }
    }

    /// Settles what to ask for now and sends what the order allows.
    func replan() {
        noteSharing()
        guard let active = slices.active, let sampleRate = active.sampleRateHz, sampleRate > 0 else {
            plannedFftSize = nil
            plannedSampleRateHz = nil
            return
        }
        // Before media is up, plan the FFT without changing the band's pan.
        // The first media context can still choose its actual view later.
        let view = mediaUp ? view(for: active, sampleRateHz: sampleRate)
                           : planningView(for: active, sampleRateHz: sampleRate)
        let planned = plan(sampleRateHz: sampleRate, view: view)
        guard mediaUp else {
            return
        }
        let gates = self.gates
        let budget = self.budget
        guard session.subscribes else {
            // Sound only, or Audio only: no display endpoint at all.
            if band.paused {
                band.paused = false
            }
            send(.none, charge: .init(), gates: gates, budget: budget)
            return
        }
        if band.gates != gates {
            band.gates = gates
        }
        band.endpointId = endpointId
        if allocator.budget != budget {
            allocator.update(budget: budget)
            refused = nil
        }
        let wish = request(sliceId: active.id, view: view, planned: planned, gates: gates)
        let intent = DisplayQualityAllocator.Intent(panId: Self.panId, pixels: wish.pixels, fps: wish.fps,
                                                    includesWidePlane: wish.wideSpanFactor > 1,
                                                    waterfallPeriodMs: min(max(band.settings.waterfallPeriodMs,
                                                        BandDisplaySettings.waterfallPeriodRange.lowerBound),
                                                        BandDisplaySettings.waterfallPeriodRange.upperBound),
                                                    active: true, extras: wish.extras)
        if allocator.intents != [intent] {
            allocator.update(intents: [intent])
            refused = nil
        }
        noteWish(intent)
        if asking, gates.displayBudget, budget != nil,
           case .success(let whole) = DisplayQualityAllocator.allocate(budget: nil, intents: [intent]),
           let wanted = whole.quality(forPan: Self.panId) {
            // Asking: the band as wanted; the Core's answer ends the ask.
            if band.paused {
                band.paused = false
            }
            var target = wish
            target.framesPerLine = wanted.framesPerLine
            send(.endpoint(target), charge: wanted.charge, gates: gates, budget: budget, asking: true)
            return
        }
        guard let quality = allocator.allocation.quality(forPan: Self.panId) else {
            Self.logger.info("The band's display could not be sized: \(String(describing: self.allocator.lastProblem), privacy: .public)")
            return
        }
        if band.paused != quality.suspended {
            band.paused = quality.suspended
        }
        var target = wish
        target.pixels = quality.pixels
        target.fps = quality.fps
        target.framesPerLine = quality.framesPerLine
        send(quality.suspended ? .none : .endpoint(target), charge: quality.charge, gates: gates, budget: budget)
    }

    /// The transmit holder changed (it became this device, another device,
    /// the radio's PTT, nobody, or went away): the band asks again for what
    /// it wants, since the Core shares its display differently now.
    func transmitHolderChanged() {
        beginAsking()
    }

    private func beginAsking() {
        settling?.cancel()
        settling = nil
        asking = true
        // An ask may repeat what the Core refused before: its share may differ now.
        refused = nil
        queueReplan()
    }

    /// Asks again when the band wants more than it did: at once for more
    /// frames, planes or extras, and once a wider band has held its width
    /// for ``resizeSettle``.
    private func noteWish(_ intent: DisplayQualityAllocator.Intent) {
        defer { lastWish = intent }
        guard let last = lastWish, last != intent,
              case .success(let before) = DisplayQualityAllocator.allocate(budget: nil, intents: [last]),
              case .success(let after) = DisplayQualityAllocator.allocate(budget: nil, intents: [intent]) else {
            return
        }
        let grew = after.total.applicationBytesPerSecond > before.total.applicationBytesPerSecond
            || after.total.spectrumSampleUnitsPerSecond > before.total.spectrumSampleUnitsPerSecond
            || after.total.messagesPerSecond > before.total.messagesPerSecond
        guard grew else {
            return
        }
        var widthOnly = last
        widthOnly.pixels = intent.pixels
        guard widthOnly == intent else {
            beginAsking()
            return
        }
        settling?.cancel()
        settling = Task { @MainActor [weak self] in
            try? await Task.sleep(for: Self.resizeSettle)
            guard !Task.isCancelled, let self, self.lastWish == intent else {
                return
            }
            self.beginAsking()
        }
    }

    /// The view the band asks for: the zoom's, the double tap's or the
    /// pan's, else the active slice in the middle of its whole sample rate.
    /// When the active slice itself moves out of view (another slice made
    /// active, or tuned from elsewhere), the view moves to keep it in the
    /// middle; a pan that leaves it off the band stays where it was dropped
    /// (D74).
    private func view(for active: BandSlicesModel.Entry, sampleRateHz: Double) -> TuneGestures.View {
        if band.keyedView {
            // Keyed on this band: the keyed view, about the carrier, its
            // span kept for the next key. The receive view waits in the band.
            let asked = band.requestedView
                ?? TransmitDisplay.firstView(carrierHz: band.transmit.carrierHz, spanHz: band.settings.txViewSpanHz)
            let keyed = band.keyedViewAsked(asked)
            if band.requestedView != keyed {
                band.requestView(keyed)
            }
            return keyed
        }
        let limits = band.spanLimits(sampleRateHz: sampleRateHz)
        var view = band.requestedView
            ?? TuneGestures.View(centerHz: active.slice.frequencyHz, spanHz: sampleRateHz)
        view.spanHz = min(max(view.spanHz, limits.lowerBound), limits.upperBound)
        let hz = active.slice.frequencyHz
        let moved = lastActive.map { $0.id != active.id || $0.hz != hz } ?? true
        lastActive = (active.id, hz)
        if moved, hz < view.centerHz - view.spanHz / 2 || hz > view.centerHz + view.spanHz / 2 {
            view.centerHz = hz
        } else if moved, !band.settings.ctun {
            // CTUN off: the band follows the slice (audit row 34).
            view.centerHz = hz
        }
        if band.requestedView != view {
            band.requestView(view)
        }
        return view
    }

    /// The same span the request would use, without publishing a view or
    /// advancing the active-slice memory before the first media context.
    private func planningView(for active: BandSlicesModel.Entry, sampleRateHz: Double) -> TuneGestures.View {
        if band.keyedView {
            let asked = band.requestedView
                ?? TransmitDisplay.firstView(carrierHz: band.transmit.carrierHz, spanHz: band.settings.txViewSpanHz)
            return TransmitDisplay.clamped(asked, carrierHz: band.transmit.carrierHz)
        }
        let limits = band.spanLimits(sampleRateHz: sampleRateHz)
        var view = band.requestedView
            ?? TuneGestures.View(centerHz: active.slice.frequencyHz, spanHz: sampleRateHz)
        view.spanHz = min(max(view.spanHz, limits.lowerBound), limits.upperBound)
        return view
    }

    /// The current plan, available as soon as the slice and its rate are known.
    /// The same result becomes the request when media connects.
    private func plan(sampleRateHz: Double, view: TuneGestures.View) -> (fftSize: Int, fine: Bool) {
        let described = band.catalog?.display
        let sizeSetting = Double(setting("DisplayFftSize")
                                 ?? Self.describedDefault(described, "DisplayFftSize") ?? Self.defaultFftSize)
        let pixels = session.pixels(band.requestedPixels)
        // The Size setting is the floor; a deep zoom, or the Hz/bin target,
        // asks for a finer engine of its own (link 7.4, `fftPlan`).
        let hzPerBin = settings.value("DisplayHzPerBinTarget").flatMap { Double($0.trimmingCharacters(in: .whitespaces)) }
        let planned = Self.fftPlan(described).plan(sampleRateHz: sampleRateHz, pixels: pixels, spanHz: view.spanHz,
                                                   sizeSetting: sizeSetting, hzPerBinTarget: hzPerBin)
        let size = planned.fftSize
        let record = DisplayBinDiagnostic(stage: .plan, value: hzPerBin, fftSize: size, floorSize: sizeSetting,
                                          rateHz: sampleRateHz, spanHz: view.spanHz, pixels: pixels,
                                          widthHz: sampleRateHz / Double(size))
        if record != lastBinPlan {
            lastBinPlan = record
            binDiagnostic(record)
        }
        if plannedFftSize != size {
            plannedFftSize = size
        }
        if plannedSampleRateHz != sampleRateHz {
            plannedSampleRateHz = sampleRateHz
        }
        return planned
    }

    /// The whole subscription the band would like, before the budget.
    private func request(sliceId: Int, view: TuneGestures.View, planned: (fftSize: Int, fine: Bool),
                         gates: MediaFeatureGates) -> DisplaySubscription {
        let described = band.catalog?.display
        // The selected data mode caps frames and may halve the detail.
        let fps = session.fps(min(max(setting("DisplaySpectrumFps")
                              ?? Self.describedDefault(described, "DisplaySpectrumFps") ?? Self.defaultFps,
                          DisplayEndpointRequest.fpsRange.lowerBound),
                      DisplayEndpointRequest.fpsRange.upperBound))
        let fftWindow = setting("DisplayFftWindow").flatMap { $0 >= 0 ? $0 : nil }
            ?? Self.describedDefault(described, "DisplayFftWindow") ?? Self.defaultWindowType
        let pixels = session.pixels(band.requestedPixels)
        // The pan's detectors and averaging (the desktop's Spectrum and
        // Waterfall Detector and Averaging). With the display extras the
        // Core times the averaging from the phone's settings; an older
        // Core gets both planes unaveraged, as it cannot time them.
        let display = band.settings
        let trace = DisplaySubscription.Plane(detector: display.spectrumDetector.rawValue,
                                              averageMode: gates.displayExtras ? display.spectrumAveraging.rawValue
                                                                               : Self.noAveraging,
                                              averageAlpha: 0)
        let waterfall = DisplaySubscription.Plane(detector: display.waterfallDetector.rawValue,
                                                  averageMode: gates.displayExtras
                                                      ? display.waterfallAveraging.rawValue : Self.noAveraging,
                                                  averageAlpha: 0)
        // The quantisation window follows the scale and the waterfall's
        // levels, as the desktop's remote window's does (audit row 19).
        let window = display.quantisationWindow(coreLevels: band.state.history.heldLevels, held: heldWindow)
        heldWindow = window
        let period = min(max(display.waterfallPeriodMs, BandDisplaySettings.waterfallPeriodRange.lowerBound),
                         BandDisplaySettings.waterfallPeriodRange.upperBound)
        var subscription = DisplaySubscription(
            endpointId: 0, revision: 0, sliceId: sliceId, tier: planned.fine ? .fine : .wide,
            fftSize: planned.fftSize,
            windowType: fftWindow, centreHz: view.centerHz, spanHz: view.spanHz, pixels: pixels, fps: fps,
            framesPerLine: DisplayQualityAllocator.framesPerLine(periodMs: period, fps: fps),
            trace: trace, waterfall: waterfall, minDbm: window.lowerBound, maxDbm: window.upperBound,
            // The Core's 3D factor only while the 3D view is drawn with 3D
            // Span over 0, and not while the band saves data (JJ's board,
            // recommendation 5).
            wideSpanFactor: display.wideSpanFactor(drawsStack: band.drawsStack,
                                                   savesData: band.stackConditions.savesData))
        // Decimation only to a Core that takes it, and only when not 1.
        let decimation = min(max(display.decimation, BandDisplaySettings.decimationRange.lowerBound),
                             BandDisplaySettings.decimationRange.upperBound)
        subscription.decimation = gates.decimation && decimation != 1 ? decimation : nil
        var applied = display.applied(to: subscription, gates: gates)
        // The extended view goes only to a Core that offers it, and only on.
        applied.extendedView = gates.wideband && band.settings.extendedView ? true : nil
        // The transmit display's window from this phone's transmit grid and
        // levels, and display duplex, to a Core that takes them.
        applied.txWindow = band.txWindow
        applied.duplex = gates.displayDuplex && band.duplexActive ? true : nil
        return applied
    }

    /// Asks the Core to re-tune the band's Clarity now, on the endpoint it
    /// holds; returns that endpoint.
    @discardableResult
    func retuneClarity() async throws -> UInt32 {
        guard case .endpoint(let held) = accepted else {
            throw MediaControlError.unknownEndpoint
        }
        try await operations.retuneClarity(held.endpointId)
        return held.endpointId
    }

    /// The smallest FFT size from 1024 that reaches `target`, at most FFTEngine's largest.
    static func fftSize(for target: Double) -> Int {
        defaultFftPlan.rounded(target)
    }

    /// The FFT sizes a Core without `display` computes: from 1024 to FFTEngine's largest.
    static let defaultFftPlan = StationCatalog.Display.FftPlan(minFftSize: DisplayEndpointRequest.minimumFftSize,
                                                               maxFftSize: largestFftSize)

    /// The Core's FFT plan, or the earlier one when it describes none, or
    /// one the subscription could not carry (a size under 1024, or not a
    /// power of two).
    static func fftPlan(_ described: StationCatalog.Display?) -> StationCatalog.Display.FftPlan {
        guard let plan = described?.fftPlan, plan.minFftSize >= DisplayEndpointRequest.minimumFftSize,
              plan.maxFftSize >= plan.minFftSize, plan.minFftSize & (plan.minFftSize - 1) == 0 else {
            return defaultFftPlan
        }
        return plan
    }

    /// The default the Core's catalogue gives the control that writes `key`.
    static func describedDefault(_ described: StationCatalog.Display?, _ key: String) -> Int? {
        guard let value = described?.control(settingsKey: key)?.defaultValue, value.isFinite else {
            return nil
        }
        return Int(value.rounded())
    }

    private func setting(_ key: String) -> Int? {
        settings.value(key).flatMap { Int($0.trimmingCharacters(in: .whitespaces)) }
    }

    // MARK: Sending, in the budget's order

    private func send(_ target: Holding, charge: DisplayQualityAllocator.Charge, gates: MediaFeatureGates,
                      budget: DisplayQualityAllocator.Budget?, asking: Bool = false) {
        if case .none = target, case .none = requested, let id = refusedEndpoint, outstanding.isEmpty {
            // A display the Core refused, now dropped: it stops counting.
            refusedEndpoint = nil
            // The Core holds nothing for it, so its answer is not waited for
            // (the desktop's `sendRefusedRelease`).
            release(id, revision: revision, byResult: false)
            return
        }
        // A slice may move to another Core pan without changing the
        // frequency or display request. A new revision still has to prove
        // this frame belongs to its new pan before Max Bin can read it.
        let panChanged: Bool
        if case .endpoint = target {
            panChanged = slices.active?.panKey != acceptedPanKey
        } else {
            panChanged = false
        }
        guard !Self.same(target, requested) || panChanged else {
            return
        }
        if case .endpoint(let subscription) = target, subscription == refused {
            return
        }
        let byResult = gates.displayBudget
        let waiting = !outstanding.isEmpty
        if waiting && !stalled {
            return
        }
        let reduction: Bool
        switch target {
        case .none:
            reduction = true
        case .endpoint:
            reduction = charge.applicationBytesPerSecond <= acceptedCharge.applicationBytesPerSecond
                && charge.spectrumSampleUnitsPerSecond <= acceptedCharge.spectrumSampleUnitsPerSecond
                && charge.messagesPerSecond <= acceptedCharge.messagesPerSecond
                && accepted != .none
        }
        if !reduction && byResult {
            // An increase waits for every result, and goes only into
            // headroom the Core has confirmed. The band is the phone's one
            // pan, so the others hold nothing.
            if waiting {
                return
            }
            if !asking, let budget, charge.applicationBytesPerSecond > budget.applicationBytesPerSecond
                || charge.spectrumSampleUnitsPerSecond > budget.spectrumSampleUnitsPerSecond {
                return
            }
        }
        switch target {
        case .none:
            guard case .endpoint(let held) = requested else {
                requested = .none
                return
            }
            requested = .none
            release(endpointId, revision: held.revision, byResult: byResult)
        case .endpoint(var subscription):
            if refusedEndpoint == endpointId {
                // Asking for the endpoint again replaces the refused request.
                refusedEndpoint = nil
            }
            revision &+= 1
            if revision == 0 {
                revision = 1
            }
            subscription.endpointId = endpointId
            subscription.revision = revision
            let normalised = Self.normalised(subscription)
            requested = .endpoint(subscription)
            begin(Operation(endpointId: endpointId, revision: revision, holding: .endpoint(subscription),
                            charge: charge, panKey: slices.active?.panKey, ask: asking), byResult: byResult)
            Task { @MainActor in
                do {
                    self.binDiagnostic(.init(stage: .subscription, fftSize: subscription.fftSize,
                                             spanHz: subscription.spanHz, pixels: subscription.pixels,
                                             revision: subscription.revision))
                    try await self.operations.subscribe(subscription)
                } catch {
                    self.binDiagnostic(.init(stage: .subscriptionError, fftSize: subscription.fftSize,
                                             revision: subscription.revision))
                    Self.logger.info("The band's display was not asked for: \(String(describing: error), privacy: .public)")
                    self.forget(normalised)
                }
            }
        }
    }

    /// Unsubscribes endpoint `id`, whose last revision was `revision`; a
    /// released endpoint ID is never used again on this connection.
    private func release(_ id: UInt32, revision held: UInt32, byResult: Bool) {
        endpointId &+= 1
        band.endpointId = endpointId
        var next = held &+ 1
        if next == 0 {
            next = 1
        }
        begin(Operation(endpointId: id, revision: next, holding: .none, charge: .init()), byResult: byResult)
        Task { @MainActor in
            do {
                try await self.operations.unsubscribe(id)
            } catch {
                Self.logger.info("The band's display was not released: \(String(describing: error), privacy: .public)")
            }
        }
    }

    /// Records an operation: on the budget wire it waits for its result,
    /// with the deadline running; without it the Core is taken to hold it.
    private func begin(_ operation: Operation, byResult: Bool) {
        guard byResult else {
            accepted = operation.holding
            acceptedPanKey = operation.panKey
            acceptedCharge = operation.charge
            return
        }
        outstanding.append(operation)
        deadlineTask?.cancel()
        let revision = operation.revision
        deadlineTask = Task { @MainActor [weak self, deadline] in
            try? await Task.sleep(for: deadline)
            guard !Task.isCancelled, let self, self.outstanding.contains(where: { $0.revision == revision }) else {
                return
            }
            Self.logger.warning("The Core has not answered the band's display request in time")
            self.stalled = true
            self.queueReplan()
        }
    }

    /// A request that never left: nothing is held or waited for on its account.
    private func forget(_ normalised: DisplaySubscription) {
        outstanding.removeAll { operation in
            if case .endpoint(let sent) = operation.holding {
                return Self.normalised(sent) == normalised
            }
            return false
        }
        if case .endpoint(let sent) = requested, Self.normalised(sent) == normalised {
            requested = accepted
        }
        if outstanding.isEmpty {
            stalled = false
        }
    }

    /// The Core's answer to one of the operations sent.
    private func settle(_ result: MediaControlEvent.AllocationResult) {
        guard let index = outstanding.firstIndex(where: {
            $0.endpointId == result.endpointId && $0.revision == result.revision
        }) else {
            return
        }
        let operation = outstanding.remove(at: index)
        allocator.note(result)
        if operation.ask {
            // The Core has answered the ask: it ends, and a refusal is
            // answered by planning inside the share.
            asking = false
        }
        if result.accepted {
            accepted = operation.holding
            acceptedPanKey = operation.panKey
            acceptedCharge = operation.charge
        } else {
            if case .endpoint(let sent) = operation.holding {
                refused = Self.normalised(sent)
            }
            if !operation.ask, result.acceptedRevision == 0, case .endpoint(let sent) = operation.holding {
                // Nothing held for another pan's slice the phone jumped to:
                // back to its own band, with the Core's words (JJ's ruling 3).
                slices.displayRefused(sliceId: sent.sliceId, reason: result.reason)
            }
            if result.acceptedRevision == 0 {
                accepted = .none
                acceptedPanKey = nil
                acceptedCharge = .init()
                if case .endpoint = operation.holding {
                    refusedEndpoint = operation.endpointId
                }
            }
            // The newest request stands only if it is still waiting.
            if outstanding.isEmpty {
                requested = accepted
            }
        }
        if outstanding.isEmpty {
            stalled = false
            deadlineTask?.cancel()
            deadlineTask = nil
        }
        queueReplan()
    }

    /// A new media connection, or none: nothing is held, and IDs start again.
    private func startOver() {
        lastBinPlan = nil
        lastActive = nil
        // Each media session starts by asking for what the band wants.
        asking = true
        lastWish = nil
        settling?.cancel()
        settling = nil
        refusedEndpoint = nil
        deadlineTask?.cancel()
        deadlineTask = nil
        outstanding.removeAll()
        accepted = .none
        acceptedPanKey = nil
        acceptedCharge = .init()
        requested = .none
        refused = nil
        stalled = false
        endpointId = 1
        revision = 0
        band.endpointId = endpointId
        noteSharing()
    }

    /// The Sharing chip's number: the frame rate the Core holds, while it is
    /// below the band's wish and the Core says it is shared or busy.
    private func noteSharing() {
        var fps: Int?
        if case .endpoint(let held) = accepted, let wish = lastWish, held.fps < wish.fps {
            switch budget?.reason {
            case .sharedConnection?, .sharedProcessing?, .coreBusy?:
                fps = held.fps
            default:
                break
            }
        }
        if sharingFps != fps {
            sharingFps = fps
        }
    }

    /// A subscription without its endpoint and revision, to compare two requests.
    private static func normalised(_ subscription: DisplaySubscription) -> DisplaySubscription {
        var copy = subscription
        copy.endpointId = 0
        copy.revision = 0
        return copy
    }

    /// Whether two holdings ask for the same thing, whatever their endpoint and revision.
    private static func same(_ left: Holding, _ right: Holding) -> Bool {
        switch (left, right) {
        case (.none, .none):
            return true
        case (.endpoint(let a), .endpoint(let b)):
            return normalised(a) == normalised(b)
        default:
            return false
        }
    }
}
