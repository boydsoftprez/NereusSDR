// NereusSDR for iOS: the Display sheet's state: this pan's look on this phone, and what it asks the Core to compute
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusMedia
import NereusMirror
import NereusModels
import os

/// What the Display sheet shows and changes (R-IOS-11, R-IOS-27, D73; spec
/// section 5.1 item 16): this pan's display settings on this phone. The
/// palettes are the Core's catalogue's; the Fill, Top and Range ranges are
/// the desktop's (Fill Alpha 0 to 100, `DisplaySetupPages.cpp:782-792`; dB
/// Max -200 to 0, `DisplaySetupPages.cpp:2010`; the bottom no lower than its
/// dB Min floor of -200 dBm).
///
/// Every change is kept for the pan in ``BandDisplaySettingsStore`` and
/// redraws the band at once; a change to what the Core computes reaches it
/// through the band's subscription, which is sent again only when the
/// request itself changes. What this Core cannot compute stays on the
/// sheet, greyed, with "Needs a newer Core" (D23).
///
/// The band plan is the exception (D79): which plan shows is the Core's
/// `BandPlanName` setting, written through the settings proxy so the
/// desktop and every device follow; only its size is this phone's.
@MainActor
final class DisplaySheetModel: ObservableObject {
    /// The three level buttons, in their order.
    enum Level: CaseIterable, Equatable {
        case clarity
        case auto
        case manual

        var label: String {
            switch self {
            case .clarity:
                return "Clarity"
            case .auto:
                return "Auto"
            case .manual:
                return "Manual"
            }
        }

        var mode: BandDisplaySettings.WaterfallLevelMode {
            switch self {
            case .clarity:
                return .clarity
            case .auto:
                return .agc
            case .manual:
                return .manual
            }
        }
    }

    /// One of the band's three switches the Core computes. The desktop's
    /// classic Peak hold, which this phone draws itself, is its own switch
    /// (``classicPeakHold``).
    enum Feature: CaseIterable, Equatable {
        case peakHold
        case peaks
        case noiseFloor

        var label: String {
            switch self {
            case .peakHold:
                // The desktop's Active Peak Hold (audit row 28): its classic
                // Peak hold is the other switch.
                return "Active peak hold"
            case .peaks:
                return "Peaks"
            case .noiseFloor:
                return "Noise floor"
            }
        }
    }

    // The desktop's ranges, cited above.
    static let fillRange = StationCatalog.Range(min: 0, max: 100, step: 1)
    static let topRange = StationCatalog.Range(min: -200, max: 0, step: 1)
    static let bottomFloorDbm = -200.0
    static let smallestRangeDb = 1.0

    // The notes under the level buttons (the board's, and the three new ones).
    static let clarityNote = "Clarity sets the waterfall's levels from the noise floor."
    static let autoNote = "Auto follows each line's weakest and strongest signals."
    static let manualNote = "Manual uses the levels set in Setup."

    /// The Core's words for the last Re-tune it refused.
    @Published private(set) var note: String?
    /// The Core's words for the last band plan it refused.
    @Published private(set) var planNote: String?
    /// The Band plan picker is open under its row.
    @Published var planPickerOpen = false
    /// Set whenever the band's settings, the catalogue or the Core's offer change.
    @Published private(set) var revision = 0

    private let band: BandModel
    private let slices: BandSlicesModel
    private let subscriber: BandSubscriber
    private let store: MirrorStore
    private let catalogFeed: CatalogFeed
    private let changeDisplay: (_ change: (inout BandDisplaySettings) -> Void) -> Void
    private let settingsProxy: SettingsProxyClient
    /// This phone's band plan writes the Core has not answered. While any
    /// is waiting, the band keeps the plan the Core last confirmed, so the
    /// tick moves with the Core's echo, not with the tap.
    private var planWritesWaiting = 0
    private var watches: Set<AnyCancellable> = []
    private static let logger = Logger(subsystem: "NereusSDR", category: "display.sheet")

    /// The Core's setting that names its band plan (the desktop's View >
    /// Band Plan choice), by the plan's `name`.
    static let bandPlanKey = "BandPlanName"
    /// What the picker says when the Core refuses a plan without a reason.
    static let planRefusedText = "The Core did not change the band plan."

    init(band: BandModel, slices: BandSlicesModel, subscriber: BandSubscriber, store: MirrorStore,
         catalogFeed: CatalogFeed, settingsProxy: SettingsProxyClient,
         changeDisplay: @escaping (_ change: (inout BandDisplaySettings) -> Void) -> Void) {
        self.band = band
        self.slices = slices
        self.subscriber = subscriber
        self.store = store
        self.catalogFeed = catalogFeed
        self.settingsProxy = settingsProxy
        self.changeDisplay = changeDisplay
        band.stationPlanName = settingsProxy.value(Self.bandPlanKey)
        band.$settings.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        band.$transmit.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        band.$showsTransmit.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        band.$stackOffered.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        band.$stackConditions.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        band.$stackStage.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        store.$capabilities.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        catalogFeed.$catalog.sink { [weak self] _ in self?.changed() }.store(in: &watches)
        settingsProxy.$values.sink { [weak self] values in
            guard let self, self.planWritesWaiting == 0 else {
                return
            }
            self.band.stationPlanName = values[Self.bandPlanKey]
            self.changed()
        }.store(in: &watches)
    }

    private func changed() {
        Task { @MainActor [weak self] in
            self?.revision &+= 1
        }
    }

    private var settings: BandDisplaySettings { band.settings }

    /// What this Core offers the pan's display.
    var gates: MediaFeatureGates { subscriber.gates }

    /// The Core computes the display extras: Clarity, Auto and the band's switches.
    var extrasAvailable: Bool { BandDisplaySettings.extrasAvailable(gates: gates) }

    /// The Core re-tunes Clarity for the phone: `displayExtrasVersion` 2.
    var retuneAvailable: Bool { gates.clarityRetune }

    // MARK: The waterfall

    /// The catalogue's palettes; empty without a catalogue.
    var palettes: [StationCatalog.Palette] { catalogFeed.catalog?.palettes ?? [] }

    /// The current palette's name, or nil without a catalogue (this
    /// phone's own gradient has a name on any Core).
    var paletteName: String? {
        if settings.waterfallPaletteId == BandPalette.customPaletteId {
            return BandPalette.customPaletteName
        }
        return palettes.first { $0.id == settings.waterfallPaletteId }?.name ?? palettes.first?.name
    }

    func selectPalette(_ id: Int) {
        changeDisplay { $0.waterfallPaletteId = id }
    }

    /// The level the waterfall uses: without the Core's extras it is Manual
    /// whatever is kept; with them, noise-floor AGC (set in Setup) lights none.
    var level: Level? {
        guard extrasAvailable else {
            return .manual
        }
        switch settings.waterfallLevelMode {
        case .clarity:
            return .clarity
        case .agc:
            return .auto
        case .manual:
            return .manual
        case .noiseFloorAgc:
            return nil
        }
    }

    /// Whether a level button can be chosen on this Core.
    func isAvailable(_ level: Level) -> Bool {
        level == .manual || extrasAvailable
    }

    func select(_ level: Level) {
        guard isAvailable(level) else {
            return
        }
        changeDisplay { $0.waterfallLevelMode = level.mode }
    }

    /// The note under the level buttons, following the choice.
    var levelNote: String? {
        switch level {
        case .clarity?:
            return Self.clarityNote
        case .auto?:
            return Self.autoNote
        case .manual?:
            return Self.manualNote
        case nil:
            return nil
        }
    }

    /// Colour gain, 0 to 100, and Black level, 0 to 125: the desktop's
    /// waterfall Color Gain and Black Level, applied on this phone to
    /// whatever levels colour each new line.
    static let colorGainRange = StationCatalog.Range(min: Double(BandDisplaySettings.colorGainRange.lowerBound),
                                                     max: Double(BandDisplaySettings.colorGainRange.upperBound),
                                                     step: 1)
    static let blackLevelRange = StationCatalog.Range(min: Double(BandDisplaySettings.blackLevelRange.lowerBound),
                                                      max: Double(BandDisplaySettings.blackLevelRange.upperBound),
                                                      step: 1)

    var colorGain: Double { Double(settings.waterfallColorGain) }
    var blackLevel: Double { Double(settings.waterfallBlackLevel) }

    func setColorGain(_ value: Double) {
        guard value.isFinite else {
            return
        }
        let gain = Int(min(max(value.rounded(), Self.colorGainRange.min), Self.colorGainRange.max))
        changeDisplay { $0.waterfallColorGain = gain }
    }

    func setBlackLevel(_ value: Double) {
        guard value.isFinite else {
            return
        }
        let black = Int(min(max(value.rounded(), Self.blackLevelRange.min), Self.blackLevelRange.max))
        changeDisplay { $0.waterfallBlackLevel = black }
    }

    /// Re-tune: the Core re-estimates the noise floor for this pan's Clarity now.
    func retune() {
        guard retuneAvailable, level == .clarity else {
            return
        }
        let subscriber = subscriber
        Task {
            do {
                try await subscriber.retuneClarity()
            } catch {
                Self.logger.info("Re-tune did not reach the Core: \(String(describing: error), privacy: .public)")
            }
        }
    }

    /// One of the media client's events: the Core's refusal of a Re-tune.
    func receive(_ event: MediaControlEvent) {
        if case .clarityRetuneRefused(let rejection) = event, rejection.endpointId == band.endpointId {
            note = rejection.reason
        }
    }

    // MARK: The spectrum

    /// The fill under the trace is on (the desktop's Fill spectrum trace).
    var fillOn: Bool { settings.traceFill }

    func toggleFill() {
        changeDisplay { $0.traceFill.toggle() }
    }

    /// Fill, 0 to 100.
    var fill: Double { (settings.traceFillOpacity * 100).rounded() }

    func setFill(_ value: Double) {
        let clamped = min(max(value.rounded(), Self.fillRange.min), Self.fillRange.max)
        changeDisplay { $0.traceFillOpacity = clamped / 100 }
    }

    /// Line: the trace's width in points (D75).
    var line: Double { settings.traceWidthPoints }

    /// Line runs from one screen pixel to 3 points, in steps of one pixel,
    /// on a screen of `scale` pixels per point.
    static func lineRange(scale: CGFloat) -> StationCatalog.Range {
        let limits = BandDisplaySettings.traceWidthRange(scale: scale)
        return StationCatalog.Range(min: limits.lowerBound, max: limits.upperBound, step: limits.lowerBound)
    }

    /// A new width, on the nearest whole pixel within the range.
    func setLine(_ value: Double, scale: CGFloat) {
        let range = Self.lineRange(scale: scale)
        guard value.isFinite else {
            return
        }
        let pixels = (min(max(value, range.min), range.max) / range.step).rounded()
        let width = min(max(pixels * range.step, range.min), range.max)
        changeDisplay { $0.traceWidthPoints = width }
    }

    /// The width as the Line row reads it: points, to two places at most.
    static func lineText(_ points: Double) -> String {
        let hundredths = (points * 100).rounded()
        var text = String(format: "%.2f", hundredths / 100)
        while text.hasSuffix("0") {
            text.removeLast()
        }
        if text.hasSuffix(".") {
            text.removeLast()
        }
        return "\(text) pt"
    }

    /// Spectrum height: the spectrum's share of the band, 20 to 80 percent,
    /// kept for the pan; the desktop starts at 40.
    static let spectrumHeightRange = StationCatalog.Range(min: BandDisplaySettings.spectrumShareRange.lowerBound,
                                                          max: BandDisplaySettings.spectrumShareRange.upperBound,
                                                          step: 1)

    var spectrumHeight: Double { (settings.spectrumShare * 100).rounded() }

    func setSpectrumHeight(_ value: Double) {
        guard value.isFinite else {
            return
        }
        let percent = min(max(value.rounded(), Self.spectrumHeightRange.min), Self.spectrumHeightRange.max)
        changeDisplay { $0.currentSpectrumSharePercent = percent }
    }

    /// The split as a drag on the frequency-scale row leaves it: smoothly,
    /// within 20 and 80 percent (JJ, 2026-09-26).
    func dragSpectrumHeight(_ percent: Double) {
        guard percent.isFinite else {
            return
        }
        let clamped = min(max(percent, Self.spectrumHeightRange.min), Self.spectrumHeightRange.max)
        changeDisplay { $0.currentSpectrumSharePercent = clamped }
    }

    /// The scale as a drag on it leaves it: its top and bottom moved
    /// together, its range kept, the top no higher than 0 dBm (200 dBm for
    /// the transmit grid) and the bottom no lower than -200 dBm.
    func dragScale(topDbm: Double, bottomDbm: Double) {
        guard topDbm.isFinite, bottomDbm.isFinite else {
            return
        }
        let depth = max(topDbm - bottomDbm, smallestRange)
        var top = min(topDbm, topLimits.max)
        if top - depth < Self.bottomFloorDbm {
            top = Self.bottomFloorDbm + depth
        }
        writeScale(top: top, bottom: top - depth)
    }

    // MARK: The scale while keyed (Task 54f)

    /// The scale Top, Range, the scale's arrows and a drag on it move: the
    /// pan's own, or while the Core's radio is keyed on the band its
    /// transmit grid, as the desktop's scale moves its transmit grid while
    /// keyed. Kept on this phone either way.
    private var scaleSettings: BandDisplaySettings { band.drawnSettings }

    /// The scale moved now is the transmit grid.
    var keyedScale: Bool { band.transmitOverlay }

    /// Top's limits: the receive scale's -200 to 0 dBm, or the transmit
    /// grid's -200 to 200 dBm (the desktop's).
    var topLimits: StationCatalog.Range {
        keyedScale ? StationCatalog.Range(min: BandDisplaySettings.txGridTopRange.lowerBound,
                                          max: BandDisplaySettings.txGridTopRange.upperBound, step: 1)
            : Self.topRange
    }

    /// The smallest range: 1 dB, or the transmit grid's 10 dB.
    private var smallestRange: Double {
        keyedScale ? BandDisplaySettings.txGridRangeRange.lowerBound : Self.smallestRangeDb
    }

    private func writeScale(top: Double, bottom: Double) {
        if keyedScale {
            let range = min(top - bottom, BandDisplaySettings.txGridRangeRange.upperBound)
            changeDisplay {
                $0.txGridTopDbm = top
                $0.txGridRangeDb = range
            }
        } else {
            changeDisplay {
                $0.scaleTopDbm = top
                $0.scaleBottomDbm = bottom
            }
        }
    }

    // MARK: Display duplex (Task 54f)

    /// Why DUP is greyed: the desktop remote window's words for a Core
    /// below `txDisplayVersion` 3.
    static let duplexUnavailableText = "This Core does not show the receiver while transmitting for this app. "
        + "Updating the Core may help."
    static let duplexOnNote = "While transmitting, the band keeps the receiver."
    static let duplexOffNote = "While transmitting, the band shows the transmit display."

    /// DUP as kept on this phone.
    var duplex: Bool { settings.displayDuplex }

    /// The Core keeps the receiver on the band while keyed for this phone.
    var duplexAvailable: Bool { gates.displayDuplex }

    /// DUP's line: what it does, or why it is greyed.
    var duplexNote: String {
        guard duplexAvailable else {
            return Self.duplexUnavailableText
        }
        return duplex ? Self.duplexOnNote : Self.duplexOffNote
    }

    /// Display duplex on or off: kept on this phone; while keyed the band
    /// swaps at once.
    func setDuplex(_ on: Bool) {
        guard duplexAvailable, on != duplex else {
            return
        }
        changeDisplay { $0.displayDuplex = on }
    }

    /// Spectrum height as the row reads it.
    static func percentText(_ value: Double) -> String {
        "\(Int(value.rounded()))%"
    }

    // MARK: Task 54e part 2

    /// The desktop's classic Peak hold, drawn on this phone: on any Core.
    var classicPeakHold: Bool { settings.peakHold }

    func toggleClassicPeakHold() {
        changeDisplay { $0.peakHold.toggle() }
    }

    static let ctunOnNote = "The band stays where you put it while the slice tunes."
    static let ctunOffNote = "The band follows the active slice."

    /// CTUN: on, the band stays where it is put; off, it follows the
    /// active slice. On a Core that keeps the receiver's window still for
    /// this phone (``BandSlicesModel/canMoveReceiverWindow``), the switch
    /// also asks it to, as the desktop's remote window does.
    var ctun: Bool { settings.ctun }

    func setCtun(_ on: Bool) {
        guard on != settings.ctun else {
            return
        }
        changeDisplay { $0.ctun = on }
        if let active = slices.active {
            if !on {
                // Off: the band goes back to the slice at once.
                band.requestView(TuneGestures.View(centerHz: active.slice.frequencyHz, spanHz: band.view.spanHz))
            }
            slices.setReceiverPinned(on, sliceId: active.id)
        }
    }

    /// Look back: the seconds the waterfall is looked back from its newest
    /// line, 0 while live, up to what it holds less one screen.
    var lookBackSeconds: Double { Double(band.lookBackLines) * band.secondsPerLine }

    var lookBackRange: StationCatalog.Range {
        let most = Double(max(0, band.state.history.count - band.visibleLines)) * band.secondsPerLine
        return StationCatalog.Range(min: 0, max: max(most.rounded(.down), 0), step: 1)
    }

    func setLookBack(seconds: Double) {
        guard seconds.isFinite else {
            return
        }
        band.lookBack(lines: Int((max(0, seconds) / band.secondsPerLine).rounded()), visibleLines: band.visibleLines)
        changed()
    }

    /// Back to the live waterfall.
    func goLive() {
        band.goLive()
        changed()
    }

    /// Look back as its row reads it: Live, or minutes and seconds.
    static func lookBackText(_ seconds: Double) -> String {
        let whole = Int(seconds.rounded())
        guard whole > 0 else {
            return "Live"
        }
        return String(format: "-%d:%02d", whole / 60, whole % 60)
    }

    /// Top: the scale's top, in dBm.
    var top: Double { scaleSettings.scaleTopDbm }

    /// How far each of the dBm scale's arrows moves the top, as the
    /// desktop's do; the bottom stays where it is.
    static let arrowStepDb = 10.0

    /// The scale's ▲ can raise the top: it is under 0 dBm.
    var canRaiseTop: Bool { top < topLimits.max }

    /// The scale's ▼ can lower the top: it is more than 1 dB over the bottom.
    var canLowerTop: Bool { top - scaleSettings.scaleBottomDbm > smallestRange }

    /// The scale's ▲ (`up`) or ▼: the top 10 dB higher or lower, the bottom
    /// kept, within 0 dBm and 1 dB over the bottom.
    func nudgeTop(up: Bool) {
        let bottom = scaleSettings.scaleBottomDbm
        let next = up ? min(top + Self.arrowStepDb, topLimits.max)
            : max(top - Self.arrowStepDb, bottom + smallestRange)
        guard next != top else {
            return
        }
        writeScale(top: next, bottom: bottom)
    }

    /// Range: the dB from the top to the bottom.
    var range: Double { scaleSettings.scaleTopDbm - scaleSettings.scaleBottomDbm }

    /// Range runs from 1 dB to the depth that puts the bottom at -200 dBm.
    var rangeRange: StationCatalog.Range {
        StationCatalog.Range(min: smallestRange, max: max(top - Self.bottomFloorDbm, smallestRange),
                             step: 1)
    }

    /// A new top keeps the range, as far as the -200 dBm floor allows.
    func setTop(_ value: Double) {
        let top = min(max(value.rounded(), topLimits.min), topLimits.max)
        let depth = clampedRange(range, top: top)
        writeScale(top: top, bottom: top - depth)
    }

    func setRange(_ value: Double) {
        let depth = clampedRange(value.rounded(), top: top)
        let top = top
        writeScale(top: top, bottom: top - depth)
    }

    private func clampedRange(_ depth: Double, top: Double) -> Double {
        min(max(depth, smallestRange), max(top - Self.bottomFloorDbm, smallestRange))
    }

    // MARK: The band plan (D79)

    /// The Core's band plans, in its catalogue's order; empty without a catalogue.
    var plans: [StationCatalog.BandPlan] { catalogFeed.catalog?.bandPlans ?? [] }

    /// The plan the band shows, with the tick; nil without a catalogue.
    var shownPlan: StationCatalog.BandPlan? {
        BandPlanStrip.plan(in: plans, stationPlanName: band.stationPlanName)
    }

    /// Changes the Core's band plan to `plan`: one `settings.write` of its
    /// name. The tick moves when the Core's echo arrives; a refusal leaves
    /// it and shows the Core's words.
    func pickPlan(_ plan: StationCatalog.BandPlan) {
        guard plans.contains(plan), plan.id != shownPlan?.id else {
            return
        }
        planNote = nil
        planWritesWaiting += 1
        let proxy = settingsProxy
        Task { @MainActor [weak self] in
            let outcome = await proxy.write(Self.bandPlanKey, plan.name)
            guard let self else {
                return
            }
            self.planWritesWaiting -= 1
            switch outcome {
            case .rejected(let reason):
                self.planNote = reason.isEmpty ? Self.planRefusedText : reason
            case .accepted, .keptOnThisDevice, .notSent, .linkLost, .notConfirmed:
                break
            }
            if self.planWritesWaiting == 0 {
                self.band.stationPlanName = proxy.value(Self.bandPlanKey)
            }
            self.changed()
        }
    }

    /// The strip's size on this phone.
    var bandPlanSize: BandPlanSize { settings.bandPlanSize }

    func setBandPlanSize(_ size: BandPlanSize) {
        changeDisplay { $0.bandPlanSize = size }
    }

    // MARK: On the band

    func isOn(_ feature: Feature) -> Bool {
        guard extrasAvailable else {
            return false
        }
        switch feature {
        case .peakHold:
            return settings.activePeakHold
        case .peaks:
            return settings.peakBlobs
        case .noiseFloor:
            return settings.noiseFloorLine
        }
    }

    func toggle(_ feature: Feature) {
        guard extrasAvailable else {
            return
        }
        changeDisplay { settings in
            switch feature {
            case .peakHold:
                settings.activePeakHold.toggle()
            case .peaks:
                settings.peakBlobs.toggle()
            case .noiseFloor:
                settings.noiseFloorLine.toggle()
            }
        }
    }

    // MARK: The 3D view (Display V12's 3D View page, JJ's board of 2026-09-29)

    /// The View row's two choices, the desktop's names.
    static let flatViewLabel = "2D Waterfall"
    static let stackViewLabel = "3D Stacked Trace"
    /// Why Fill and Top are greyed in 3D.
    static let flatOnlyText = "Fill and Top shape the 2D trace only."
    /// The Span row's note while the band saves data (recommendation 5).
    static let spanSavesDataText = "Span is off while the band saves data."
    /// Slice shadow's line in the sheet.
    static let sliceShadowNote = "Darken each passband onto the surface."
    /// Reset 3D to defaults asks the desktop's question, as the Core's row
    /// sends it, with No the default.
    static let resetStackTitle = "Reset 3D to defaults"
    static let resetStackQuestion = "This will restore Spectrum render mode, 3D Floor, 3D Gain, 3D Span, 3D Angle "
        + "and 3D Slice Shadow to their ship defaults.\n\nContinue?"

    static let floorRange = StationCatalog.Range(min: Double(StackedTrace.floorRange.lowerBound),
                                                 max: Double(StackedTrace.floorRange.upperBound), step: 1)
    static let percentRange = StationCatalog.Range(min: Double(StackedTrace.percentRange.lowerBound),
                                                   max: Double(StackedTrace.percentRange.upperBound), step: 1)

    /// The Core offers the 3D view; without it the View row is greyed with
    /// ``StackedTraceHold/coreOlderText``.
    var stackOffered: Bool { band.stackOffered }

    /// The pan's view as chosen: 3D shows the five 3D rows and Reset.
    var stackChosen: Bool { settings.spectrumView == .stacked }

    /// The View row's reason while greyed.
    var viewNote: String? { stackOffered ? nil : StackedTraceHold.coreOlderText }

    func selectView(_ view: SpectrumView) {
        guard stackOffered, view != settings.spectrumView else {
            return
        }
        changeDisplay { $0.spectrumView = view }
    }

    var stackFloor: Double { Double(settings.threeDFloorDb) }
    var stackGain: Double { Double(settings.threeDGain) }
    var stackSpan: Double { Double(settings.threeDSpan) }
    var stackAngle: Double { Double(settings.threeDAngle) }
    var sliceShadow: Bool { settings.threeDSliceShadow }

    /// The band 3D Floor is kept for, as the Core names bands; nil before
    /// the Core names one.
    var floorBand: String? {
        settings.scaleBand.flatMap(Int.init).flatMap(SetupDescription.bandName)
    }

    func setStackFloor(_ value: Double) {
        guard value.isFinite else {
            return
        }
        let floor = Int(min(max(value.rounded(), Self.floorRange.min), Self.floorRange.max))
        changeDisplay { $0.threeDFloorDb = floor }
    }

    func setStackGain(_ value: Double) {
        guard let percent = Self.percent(value) else { return }
        changeDisplay { $0.threeDGain = percent }
    }

    func setStackSpan(_ value: Double) {
        guard let percent = Self.percent(value) else { return }
        changeDisplay { $0.threeDSpan = percent }
    }

    func setStackAngle(_ value: Double) {
        guard let percent = Self.percent(value) else { return }
        changeDisplay { $0.threeDAngle = percent }
    }

    func setSliceShadow(_ on: Bool) {
        changeDisplay { $0.threeDSliceShadow = on }
    }

    /// Reset 3D to defaults, after Yes.
    func resetStack() {
        changeDisplay { $0.resetThreeD() }
    }

    /// The Span row's note, or nil: while the band saves data, 3D Span is
    /// asked of the Core as 0.
    var spanNote: String? { band.stackConditions.savesData ? Self.spanSavesDataText : nil }

    /// Fill and Top change only the flat trace: greyed while the band draws 3D.
    var flatControlsAvailable: Bool { !band.drawsStack }

    private static func percent(_ value: Double) -> Int? {
        guard value.isFinite else {
            return nil
        }
        return Int(min(max(value.rounded(), percentRange.min), percentRange.max))
    }

    // MARK: The dBm scale's arrows in 3D (recommendation 2)

    /// In 3D the scale's arrows step 3D Floor by this much.
    static let floorStepDb = 1

    /// The arrows move 3D Floor, not the scale's top: the band draws 3D.
    var arrowsMoveFloor: Bool { band.drawsStack }

    var canRaiseFloor: Bool { settings.threeDFloorDb < StackedTrace.floorRange.upperBound }
    var canLowerFloor: Bool { settings.threeDFloorDb > StackedTrace.floorRange.lowerBound }

    /// A drag on the dBm scale in 3D: 3D Floor at `value`, to the whole dB,
    /// within 0 to 24.
    func dragFloor(to value: Double) {
        guard value.isFinite else {
            return
        }
        let floor = Int(min(max(value.rounded(), Self.floorRange.min), Self.floorRange.max))
        guard floor != settings.threeDFloorDb else {
            return
        }
        changeDisplay { $0.threeDFloorDb = floor }
    }

    /// ▲ (`up`) or ▼ in 3D: 3D Floor 1 dB deeper or shallower, within 0 to 24.
    func nudgeFloor(up: Bool) {
        let next = settings.threeDFloorDb + (up ? Self.floorStepDb : -Self.floorStepDb)
        guard StackedTrace.floorRange.contains(next) else {
            return
        }
        changeDisplay { $0.threeDFloorDb = next }
    }
}

