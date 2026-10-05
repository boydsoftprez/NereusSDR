// NereusSDR for iOS: one pan's band as the screens see it: the Core's display events in, what to draw out
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import CoreGraphics
import NereusBand
import NereusMedia
import NereusModels

/// One pan's band (R-IOS-11): it takes the Core's display events for the
/// pan's endpoint and holds what ``BandView`` draws: the latest frame and
/// its extras, the waterfall, the context's centre and span, the pan's
/// display settings and the Core's catalogue. It also reports the width
/// the band asks the Core for, its width in pixels.
@MainActor
final class BandModel: ObservableObject {
    var binDiagnostic: (DisplayBinDiagnostic) -> Void = DisplayBinDiagnostic.log
    private var lastBinGrant: DisplayBinDiagnostic?

    /// The endpoint this band draws.
    var endpointId: UInt32? {
        didSet {
            if endpointId != oldValue {
                lastBinGrant = nil
                state.discardPending()
                contextRevisions.removeAll()
                contextOrder.removeAll()
                retiredFrameSerial = state.committedSerial
            }
        }
    }
    /// The accepted context revision for each frame generation on this endpoint.
    private var contextRevisions: [UInt32: UInt32] = [:]
    private var contextOrder: [UInt32] = []
    /// The visible waterfall may remain after a connection ends. A frame
    /// committed no later than this serial belongs to the former media.
    private var retiredFrameSerial: UInt64 = 0
    /// Matches BandState's eight remembered frame contexts.
    private static let rememberedContexts = 8
    /// The revision that produced the latest committed frame, nil when
    /// that frame belongs to an old endpoint or an ended media session.
    var frameRevision: UInt32? {
        guard state.committedSerial > retiredFrameSerial,
              let frame = state.frame, frame.endpointId == endpointId else {
            return nil
        }
        return contextRevisions[frame.contextGeneration]
    }
    @Published var settings: BandDisplaySettings {
        didSet {
            state.expectsExtras = (settings.extrasRequest(gates: gates)?.sections ?? 0) != 0
            if settings.gridFollowsNoiseFloor != oldValue.gridFollowsNoiseFloor
                || settings.scaleBand != oldValue.scaleBand || settings.scaleTopDbm != oldValue.scaleTopDbm
                || settings.scaleBottomDbm != oldValue.scaleBottomDbm {
                // A new band or a scale set by hand: the tracking starts again from it.
                gridTracker.reset()
            }
            if settings.displayDuplex != oldValue.displayDuplex {
                duplexChanged()
            }
            if settings.spectrumView != oldValue.spectrumView {
                // 3D chosen or given up: measured afresh, its notice shown again.
                stackChanged()
            }
            applyToState()
        }
    }

    // MARK: The split while another slice's band shows

    /// While the band shows another slice's band upright, the least share of
    /// the band the spectrum takes, so the flag under Back to your band ends
    /// above the frequency scale (JJ, 2026-09-30; ``JumpBar/spectrumFloor(flagFoot:bandHeight:savedShare:)``);
    /// nil otherwise. It is never saved: ``settings`` keep the pan's own
    /// split, which comes back as soon as the floor is cleared. A drag on
    /// the split still moves and saves it as usual; this floor only holds
    /// the jumped view's spectrum down.
    @Published private(set) var jumpedShareFloor: CGFloat? {
        didSet {
            if jumpedShareFloor != oldValue {
                changed()
            }
        }
    }

    /// Sets or clears (nil) the jumped view's floor under the split.
    func setJumpedShareFloor(_ floor: CGFloat?) {
        let kept = floor.flatMap { $0.isFinite && $0 > 0 ? $0 : nil }
        if kept != jumpedShareFloor {
            jumpedShareFloor = kept
        }
    }

    /// The spectrum's share as shown: the pan's own, or the jumped view's
    /// floor when that is lower down.
    var shownSpectrumShare: CGFloat {
        Self.raised(settings, floor: jumpedShareFloor).spectrumShare
    }

    /// The pan's settings as the band is laid out now: ``settings`` with the
    /// split held at the jumped view's floor. For drawing and placing only;
    /// never kept.
    var shownSettings: BandDisplaySettings {
        Self.raised(settings, floor: jumpedShareFloor)
    }

    /// `settings` with the split at least at `floor`, within the split's limits.
    static func raised(_ settings: BandDisplaySettings, floor: CGFloat?) -> BandDisplaySettings {
        guard let floor, floor > settings.spectrumShare else {
            return settings
        }
        var raised = settings
        raised.currentSpectrumSharePercent = min(Double(floor) * 100, BandDisplaySettings.spectrumShareRange.upperBound)
        return raised
    }

    // MARK: The 3D view (Display V12's 3D View page)

    /// The Core offers the 3D view: its Setup description reaches version
    /// 12 and its media control carries the 3D choice's gate
    /// (`remoteMediaVersion` 1). An older Core greys the choice with
    /// ``StackedTraceHold/coreOlderText``.
    @Published var stackOffered = false {
        didSet {
            if stackOffered != oldValue {
                stackChanged()
            }
        }
    }
    /// The phone's circumstances the 3D view follows (JJ's board,
    /// recommendations 4 and 5): Low Power Mode, a hot phone (the session's
    /// heat rule), and a band saving data (Saver, or cellular without Full).
    struct StackConditions: Equatable {
        var lowPower = false
        var hot = false
        var savesData = false
    }
    @Published var stackConditions = StackConditions() {
        didSet {
            if stackConditions != oldValue {
                if stackConditions.lowPower != oldValue.lowPower || stackConditions.hot != oldValue.hot {
                    stackNoticeDismissed = false
                }
                applyToState()
                changed()
            }
        }
    }
    /// Keeps the stack within what this phone can draw (recommendation 3).
    private(set) var pacer = StackedTracePacer() {
        didSet {
            if pacer.stage != stackStage {
                stackStage = pacer.stage
            }
        }
    }
    /// The pacer's stage, published when it changes.
    @Published private(set) var stackStage: StackedTracePacer.Stage = .full
    /// The band's notice for why it shows 2D has been put away with OK.
    @Published private(set) var stackNoticeDismissed = false
    /// The Core's last display noise floor for this band, with the pan's
    /// NF shift taken off, the floor the 3D surface stands on.
    private(set) var stackFloorDbm: Double?
    /// What the Core's latest context's wide row covers, in the pan's widths.
    private(set) var wideSpanFactorNow = 1.0
    /// The frames a second the Core's latest context sends.
    private(set) var contextFps = 0
    /// The display frames received, for drawing every second one at half rate.
    private var stackFrameCount: UInt64 = 0

    /// Why this pan shows 2D while 3D is its choice; nil while it draws 3D
    /// or has 2D chosen.
    var stackHold: StackedTraceHold? {
        StackedTraceHold.reason(chosen: settings.spectrumView == .stacked, offered: stackOffered,
                                lowPower: stackConditions.lowPower, hot: stackConditions.hot, stage: pacer.stage)
    }

    /// The band draws the 3D view now.
    var drawsStack: Bool { settings.spectrumView == .stacked && stackHold == nil }

    /// The notice the band shows over the waterfall while it holds 3D back
    /// for a reason of this phone's (not the Core's: the sheet says that).
    var stackNotice: StackedTraceHold? {
        guard let hold = stackHold, hold != .coreOlder, !stackNoticeDismissed else {
            return nil
        }
        return hold
    }

    /// The 3D view is held still while this phone transmits (Stop on TX):
    /// the band says so.
    var stackPausedWhileTransmitting: Bool { drawsStack && state.waterfallHeld }

    /// The least time between two 3D draws: no faster than the frames the
    /// Core sends this band, never over 60 a second, half as often at half rate.
    var stackDrawInterval: Double { pacer.leastInterval(askedFps: Double(contextFps)) }

    /// One 3D frame drawn, taking `seconds` from the start of its drawing
    /// to the graphics finishing it.
    func noteStackFrame(drawSeconds seconds: Double) {
        guard drawsStack else {
            return
        }
        if pacer.noteFrame(drawSeconds: seconds, at: clock()) {
            if pacer.stage == .fellBack {
                stackNoticeDismissed = false
            }
            applyToState()
            changed()
        }
    }

    /// Try 3D again: every frame, measured afresh.
    func tryStackAgain() {
        pacer.tryAgain()
        stackNoticeDismissed = false
        applyToState()
        changed()
    }

    /// OK on the band's notice: it goes; 2D stays until the reason passes.
    func dismissStackNotice() {
        stackNoticeDismissed = true
        changed()
    }

    private func stackChanged() {
        pacer.tryAgain()
        stackNoticeDismissed = false
        applyToState()
        changed()
    }

    /// How far the stack has glided toward the next line: the time since
    /// the newest row over the line period, 0 while held still.
    func stackGlide(at now: Double) -> Double {
        guard !state.waterfallHeld, let newest = state.stack.newestTime else {
            return 0
        }
        return min(max((now - newest) / secondsPerLine, 0), 1)
    }
    /// The grid's minimum following the noise floor (``GridFloorTracker``):
    /// what it sets is drawn, never kept.
    private(set) var gridTracker = GridFloorTracker()

    /// Runs the tracking rule now; a change is drawn.
    private func trackGrid() {
        if gridTracker.run(settings, transmitting: transmitOverlay, at: clock()) {
            changed()
        }
    }

    /// Keeps the pan's settings on this phone when the band changes them
    /// itself (a keyed zoom remembered); the main screen points it at the
    /// settings store.
    var keepSettings: (BandDisplaySettings) -> Void = { _ in }

    // MARK: The transmit display (Task 54f, desktop PR #317)

    /// The Core's transmitter as it touches this band: keyed on one of its
    /// slices, the carrier, and the high-SWR state.
    @Published var transmit = TransmitDisplay() {
        didSet {
            if transmit != oldValue {
                transmitChanged(from: oldValue)
            }
        }
    }
    /// The Core's latest context for this band is its transmit display
    /// (`transmit` true).
    @Published private(set) var showsTransmit = false
    /// The transmit display's view is set by another device watching it
    /// (the context's `limit` is `shared`).
    @Published private(set) var transmitViewShared = false
    /// The receive view to go back to at the unkey: the one asked for when
    /// the key came, or nil when none was.
    private var receiveView: TuneGestures.View?
    /// Contexts this band holds, by generation, that are the transmit display.
    private var transmitGenerations: Set<UInt32> = []
    /// This phone is transmitting: with Stop on TX the waterfall stands still.
    var keyed = false {
        didSet {
            if keyed != oldValue {
                applyToState()
            }
        }
    }
    /// How many lines back the waterfall is looked at: 0 is live. While
    /// looking back the view stays on the same lines as new ones arrive.
    @Published private(set) var lookBackLines = 0
    /// Where the finger is on the band while it drags it, for the cursor
    /// frequency readout; nil when no finger is down.
    @Published var cursorHz: Double?
    /// The Core's waterfall levels to the nearest 5 dB: changes only when
    /// they move that far, so the band's subscription looks at its
    /// quantisation window again only then.
    @Published private(set) var coarseCoreLevels: [Double]?
    /// The Core's bin width in hertz, from its granted FFT size; nil
    /// while it has said none.
    @Published private(set) var binWidthHz: Double?
    /// The frames the Core sent in the last whole second, for the FPS readout.
    @Published private(set) var framesPerSecond: Double?
    private var frameCount = 0
    private var frameCountStart: Double?
    /// Now, in seconds since 1970; the tests set their own.
    var clock: () -> Double = { Date().timeIntervalSince1970 }
    @Published var catalog: StationCatalog?
    /// The Core's band plan by its `BandPlanName` setting, as this app
    /// last knew it (nil when the Core has none): the plan the strip shows
    /// when the catalogue marks none `active` (D79).
    @Published var stationPlanName: String? {
        didSet {
            if stationPlanName != oldValue {
                changed()
            }
        }
    }
    @Published var gates: MediaFeatureGates = .none {
        didSet {
            state.expectsExtras = (settings.extrasRequest(gates: gates)?.sections ?? 0) != 0
        }
    }
    /// The display is suspended for want of room: the frozen band shows paused.
    @Published var paused = false
    /// The endpoint width to ask for: the view's width in pixels.
    @Published private(set) var requestedPixels = 1
    /// The endpoint context's centre and span: what the Core's frames cover.
    @Published private(set) var centerHz: Double = 0
    @Published private(set) var spanHz: Double = 0
    /// The widest span the Core's accepted context allows while it offers
    /// the extended view (``MediaControlEvent/DisplayContext/maxSpanHz``);
    /// nil when its latest context does not.
    @Published private(set) var extendedSpanCeilingHz: Double?
    /// The receiver the context reads: its centre and sample rate
    /// (`sourceCentreHz`, `sampleRateHz`); 0 before the first context.
    private(set) var sourceCentreHz: Double = 0
    private(set) var sourceRateHz: Double = 0

    /// The slices' markers, drawn over the band (D10); see ``SliceMarkers``.
    @Published var markers: [SliceMarkers.Marker] = [] {
        didSet {
            if markers != oldValue {
                changed()
            }
        }
    }
    /// The centre and span a zoom or a double tap asks for. Whoever sends
    /// the band's subscription asks the Core for it; the band shows it once
    /// the Core's context says so.
    @Published private(set) var requestedView: TuneGestures.View?

    /// A stretch the band was not sent (sound only while locked or in
    /// another app), marked across the waterfall where it fell (spec
    /// section 5.5 item 12): its words, and how many lines have come since.
    struct AwayMark: Equatable {
        let text: String
        var linesSince: Int
    }

    /// The marks still on the waterfall, newest first.
    @Published private(set) var awayMarks: [AwayMark] = []
    /// The waterfall's line count when the marks were last moved.
    private var markedLines: UInt64 = 0

    private(set) var state = BandState(waterfallLines: 1)
    /// The waterfall's rows on screen at the last draw.
    private(set) var visibleLines = 1
    /// Counts changes worth a new draw.
    @Published private(set) var revision: UInt64 = 0
    #if DEBUG
    /// Screenshot evidence only: one drawable generation of one view.
    struct DrawKey: Hashable {
        let viewID: UUID
        let generation: UUID
        let width: Int
        let height: Int
    }

    private var currentDrawByView: [UUID: DrawKey] = [:]
    private(set) var presentedDraws: [DrawKey: UInt64] = [:]

    func drawingStarted(_ key: DrawKey) {
        if let previous = currentDrawByView[key.viewID], previous != key {
            presentedDraws.removeValue(forKey: previous)
        }
        currentDrawByView[key.viewID] = key
    }

    func stoppedDrawing(in viewID: UUID) {
        if let previous = currentDrawByView.removeValue(forKey: viewID) {
            presentedDraws.removeValue(forKey: previous)
        }
    }

    /// A late completion from a retired view or drawable size is ignored.
    func notePresented(_ key: DrawKey, revision: UInt64) {
        guard currentDrawByView[key.viewID] == key else { return }
        presentedDraws[key] = max(presentedDraws[key] ?? 0, revision)
    }
    #endif

    init(settings: BandDisplaySettings = .desktopDefaults) {
        self.settings = settings
        applyToState()
    }

    /// The settings the band's state follows between frames.
    private func applyToState() {
        let drawn = drawnSettings
        state.levelAdjustment = drawn.waterfallAdjustment
        // While keyed on this band the transmit levels colour each line.
        state.phoneSetsLevels = drawn.useSpectrumMinMax || transmitOverlay
        state.waterfallHeld = keyed && settings.waterfallStopOnTx
        state.peakHoldDelayMs = settings.peakHold ? settings.peakHoldDelayMs : nil
        // The rows are kept while 3D is chosen on a Core that offers it.
        state.stacksRows = settings.spectrumView == .stacked && stackOffered
    }

    /// Marks the stretch the band was not sent, at the waterfall's newest
    /// line: every line after it arrives above the mark.
    func markAway(_ text: String) {
        markedLines = state.history.linesAppended
        awayMarks.insert(AwayMark(text: text, linesSince: 0), at: 0)
        changed()
    }

    /// Moves the marks down by the lines added since they last moved, and
    /// drops those gone past the waterfall's oldest line. A history laid
    /// out again (a new width) keeps its lines, so it moves nothing.
    private func moveMarks() {
        let lines = state.history.linesAppended
        defer { markedLines = lines }
        guard !awayMarks.isEmpty, lines > markedLines else {
            return
        }
        let added = Int(min(lines - markedLines, UInt64(state.history.capacity)))
        var moved = awayMarks
        for index in moved.indices {
            moved[index].linesSince += added
        }
        moved.removeAll { $0.linesSince > state.history.capacity }
        awayMarks = moved
    }

    /// Looks back `lines` lines at the waterfall (0 goes back to live),
    /// within what it holds less one screen.
    func lookBack(lines: Int, visibleLines: Int) {
        let most = max(0, state.history.count - visibleLines)
        let next = min(max(0, lines), most)
        if next != lookBackLines {
            lookBackLines = next
            changed()
        }
    }

    /// Back to the live waterfall.
    func goLive() {
        lookBack(lines: 0, visibleLines: 0)
    }

    /// The seconds a line stands for at the pan's line period.
    var secondsPerLine: Double { Double(max(settings.waterfallPeriodMs, 1)) / 1000 }

    /// When the waterfall's top line arrived (the newest while live).
    var topLineTime: Double? { state.history.time(age: lookBackLines) }

    private func countFrame() {
        let now = clock()
        frameCount += 1
        guard let start = frameCountStart else {
            frameCountStart = now
            frameCount = 0
            return
        }
        if now - start >= 1 {
            framesPerSecond = Double(frameCount) / (now - start)
            frameCount = 0
            frameCountStart = now
        }
    }

    /// One of the Core's media control events; anything not for this band is ignored.
    func receive(_ event: MediaControlEvent) {
        switch event {
        case .context(let context) where context.endpointId == endpointId:
            if contextRevisions[context.contextGeneration] == nil {
                contextOrder.append(context.contextGeneration)
            }
            contextRevisions[context.contextGeneration] = context.revision
            while contextOrder.count > Self.rememberedContexts {
                contextRevisions[contextOrder.removeFirst()] = nil
            }
            noteTransmit(context)
            if context.centreHz != centerHz || context.spanHz != spanHz {
                centerHz = context.centreHz
                spanHz = context.spanHz
            }
            state.note(context: context.contextGeneration,
                       coverage: BandCoverage(centerHz: context.centreHz, spanHz: context.spanHz),
                       wide: BandCoverage(centerHz: context.wideCentreHz, spanHz: context.wideSpanHz))
            wideSpanFactorNow = context.spanHz > 0 ? max(1, context.wideSpanHz / context.spanHz) : 1
            contextFps = context.fps
            sourceCentreHz = context.sourceCentreHz
            sourceRateHz = context.sampleRateHz
            // The bin width the Core's engine runs at, when it says its FFT size.
            let bin = context.grant.flatMap { $0.grantedFftSize > 0 ? context.sampleRateHz / Double($0.grantedFftSize) : nil }
            if bin != binWidthHz {
                binWidthHz = bin
            }
            let record = DisplayBinDiagnostic(stage: .grant, fftSize: context.grant?.grantedFftSize,
                                              rateHz: context.sampleRateHz, revision: context.revision,
                                              widthHz: bin, limit: context.grant?.limit)
            if record != lastBinGrant {
                lastBinGrant = record
                binDiagnostic(record)
            }
            let ceiling = context.wideband?.available == true ? context.maxSpanHz : nil
            if ceiling != extendedSpanCeilingHz {
                extendedSpanCeilingHz = ceiling
            }
            changed()
        case .mediaState(.closed), .mediaState(.failed):
            state.discardPending()
            if binWidthHz != nil {
                binWidthHz = nil
            }
            if lastBinGrant != nil {
                lastBinGrant = nil
                binDiagnostic(.init(stage: .reset))
            }
            contextRevisions.removeAll()
            contextOrder.removeAll()
            retiredFrameSerial = state.committedSerial
            if extendedSpanCeilingHz != nil {
                extendedSpanCeilingHz = nil
            }
            transmitGenerations.removeAll()
            if showsTransmit || transmitViewShared {
                showsTransmit = false
                transmitViewShared = false
                applyToState()
            }
        case .displayFrame(let frame) where frame.endpointId == endpointId:
            guard !holdsReceiveFrame(frame) else {
                // The receiver hearing its own transmitter while the band
                // waits for the transmit display, or on a Core that sends
                // none: never drawn, so the band holds its last picture.
                return
            }
            let lines = state.history.linesAppended
            state.receive(frame: frame, manualLevels: drawnSettings.phoneLevels)
            keepLookingBack(since: lines)
            moveMarks()
            countFrame()
            trackGrid()
            stackFrameCount &+= 1
            if drawsStack, !pacer.draws(frame: stackFrameCount) {
                // Half rate: this frame is kept but not drawn.
                return
            }
            changed()
        case .noiseFloor(let floor) where floor.endpointId == endpointId:
            // The Core's full-source floor, Clarity's estimate, for the grid.
            gridTracker.noteClarityFloor(floor.floorDbm, at: clock())
            trackGrid()
        case .displayExtras(let extras) where extras.endpointId == endpointId:
            if let floor = extras.noiseFloorDbm, floor.isFinite {
                // The 3D surface stands on the floor without the NF shift.
                stackFloorDbm = Double(floor) - settings.noiseFloorShiftDb
            }
            if let floor = extras.noiseFloorDbm {
                gridTracker.noteDisplayFloor(floor, fastAttack: extras.noiseFloorFastAttack)
                trackGrid()
            }
            let lines = state.history.linesAppended
            state.receive(extras: extras, manualLevels: drawnSettings.phoneLevels)
            keepLookingBack(since: lines)
            moveMarks()
            if let levels = state.history.heldLevels {
                let coarse = [levels.lowDbm, levels.highDbm].map { (Double($0) / 5).rounded() * 5 }
                if coarse != coarseCoreLevels {
                    coarseCoreLevels = coarse
                }
            }
            changed()
        default:
            break
        }
    }

    /// The spans a zoom may reach: down to the finest, and out to the DDC's
    /// sample rate, or with Extended view on to the ceiling the Core's
    /// accepted context allows.
    func spanLimits(sampleRateHz: Double?) -> ClosedRange<Double> {
        if keyedView {
            // The keyed view: what the transmit display can fill.
            return TransmitDisplay.spanRange
        }
        let base = TuneGestures.spanLimits(sampleRateHz: sampleRateHz)
        guard settings.extendedView, let ceiling = extendedSpanCeilingHz, ceiling > base.upperBound else {
            return base
        }
        return base.lowerBound...ceiling
    }

    /// The frequencies the band can show without moving the receiver: its
    /// window about the receiver's centre, as wide as the widest span a
    /// zoom may reach (the sample rate, or with Extended view the Core's
    /// ceiling); nil before the Core's first context.
    func receiverWindow() -> ClosedRange<Double>? {
        guard sourceRateHz > 0, sourceCentreHz > 0 else {
            return nil
        }
        let span = max(spanLimits(sampleRateHz: sourceRateHz).upperBound, sourceRateHz)
        return TuneGestures.receiverWindow(centreHz: sourceCentreHz, spanHz: span)
    }

    /// The view on screen: the one asked for (a pan, zoom or double tap
    /// shows at once, frame by frame, before the Core answers, as the
    /// desktop's does), else the Core's context. The Core's frames are drawn
    /// where their own frequencies fall in it (D74).
    var view: TuneGestures.View {
        if let requestedView, requestedView.spanHz > 0 {
            return requestedView
        }
        return TuneGestures.View(centerHz: centerHz, spanHz: spanHz)
    }

    /// While looking back, the lines added since `lines` push the view
    /// further back, so it stays on the same lines.
    private func keepLookingBack(since lines: UInt64) {
        guard lookBackLines > 0 else {
            return
        }
        let added = Int(min(state.history.linesAppended &- lines, UInt64(state.history.capacity)))
        if added > 0 {
            lookBackLines = min(lookBackLines + added, max(0, state.history.count - 1))
        }
    }

    /// What the renderer draws around the frame.
    func overlays(scale: CGFloat) -> BandOverlays {
        let view = view
        var overlays = BandOverlays(centerHz: view.centerHz, spanHz: view.spanHz, scale: scale,
                                    settings: Self.raised(drawnSettings, floor: jumpedShareFloor),
                                    catalog: catalog, stationPlanName: stationPlanName, paused: paused,
                                    markers: markers, frameCoverage: state.frameCoverage)
        overlays.peakHold = state.peakHold
        overlays.lookBackLines = lookBackLines
        if drawsStack {
            overlays.stacked = StackedTraceOverlay(noiseFloorDbm: stackFloorDbm, glide: stackGlide(at: clock()),
                                                   availableSpanFactor: wideSpanFactorNow)
        }
        return overlays
    }

    /// The band plan the strip shows: the Core's (D79); nil without a
    /// catalogue or plans.
    var shownPlan: StationCatalog.BandPlan? {
        catalog.flatMap { BandPlanStrip.plan(in: $0.bandPlans, stationPlanName: stationPlanName) }
    }

    /// The band's parts and its spectrum's axes in points, for a band of
    /// `size` points; nil before the Core's first context.
    func pointGeometry(size: CGSize) -> (layout: BandLayout, geometry: BandGeometry)? {
        let view = view
        guard spanHz > 0, view.spanHz > 0, size.width > 0, size.height > 0 else {
            return nil
        }
        let drawn = drawnSettings
        let layout = BandLayout(size: size, scale: 1, settings: Self.raised(drawn, floor: jumpedShareFloor))
        return (layout, layout.spectrumGeometry(centerHz: view.centerHz, spanHz: view.spanHz,
                                                dbmRange: drawn.scaleRange))
    }

    /// Asks for a new centre and span: a zoom, a double tap or a pan.
    func requestView(_ view: TuneGestures.View) {
        guard view != requestedView else {
            return
        }
        requestedView = view
        changed()
    }

    /// Called by the view before each draw: sizes the waterfall to the
    /// band's pixel rows, notes the width to ask for, and commits a frame
    /// still waiting for its datagram.
    func prepareToDraw(size: CGSize, scale: CGFloat) -> BandState {
        let layout = BandLayout(size: size, scale: scale, settings: shownSettings)
        state.resizeWaterfall(lines: Self.waterfallCapacity(size: size, scale: scale, settings: shownSettings))
        visibleLines = layout.waterfallLines
        state.commit(manualLevels: drawnSettings.phoneLevels)
        moveMarks()
        let pixels = BandGeometry.requestedPixels(forWidthPixels: Double(size.width))
        if pixels != requestedPixels {
            requestedPixels = pixels
        }
        return state
    }

    /// The waterfall's lines kept for a band of `size` pixels: the tallest
    /// screen of lines the split allows (the spectrum at its least share),
    /// and the lines kept for looking back. The split does not change it,
    /// so a drag of the split keeps the history and the renderer's texture
    /// and moves only the lines on screen.
    static func waterfallCapacity(size: CGSize, scale: CGFloat, settings: BandDisplaySettings) -> Int {
        var tallest = settings
        tallest.currentSpectrumSharePercent = BandDisplaySettings.spectrumShareRange.lowerBound
        let lines = BandLayout(size: size, scale: scale, settings: tallest).waterfallLines
        return min(lines + settings.rewindLines, BandDisplaySettings.waterfallRowsLimit)
    }

    // MARK: Keyed

    /// The Core's radio is keyed on this band, or the Core is sending it the
    /// transmit display: the band is drawn with the transmit grid, levels
    /// and palette (``BandDisplaySettings/keyedOverlay``).
    var transmitOverlay: Bool { transmit.keyedHere || showsTransmit }

    /// The settings the band is drawn with now: the pan's own, or while
    /// keyed on it their transmit overlay.
    var drawnSettings: BandDisplaySettings {
        // The keyed overlay follows no floor; otherwise the tracked grid shows.
        transmitOverlay ? settings.keyedOverlay : gridTracker.applied(to: settings)
    }

    /// Display duplex is on and the Core takes it (`txDisplayVersion` 3):
    /// the band keeps the receiver while keyed.
    var duplexActive: Bool { settings.displayDuplex && gates.displayDuplex }

    /// The band shows the keyed view: keyed on it, the Core sends its
    /// transmit display, and display duplex is off.
    var keyedView: Bool { transmit.keyedHere && gates.txDisplay && !duplexActive }

    /// Keyed on this band on a Core that sends no transmit display: the band
    /// holds its last picture and says why.
    var transmitDisplayMissing: Bool { transmit.keyedHere && !gates.txDisplay }

    /// The transmit window the band's subscription sends (`txMinDbm`,
    /// `txMaxDbm`) where the Core takes it.
    var txWindow: ClosedRange<Double>? { gates.txDisplay ? settings.txDbmWindow : nil }

    /// A receive frame the band does not draw: keyed on this band without
    /// display duplex, until the unkey.
    private func holdsReceiveFrame(_ frame: DisplayFrame) -> Bool {
        transmit.keyedHere && !duplexActive && !transmitGenerations.contains(frame.contextGeneration)
    }

    /// A zoom or pan while the keyed view shows: held to what the transmit
    /// display can fill about the carrier, and its span kept for the next key.
    func keyedViewAsked(_ view: TuneGestures.View) -> TuneGestures.View {
        let clamped = TransmitDisplay.clamped(view, carrierHz: transmit.carrierHz)
        if clamped.spanHz != settings.txViewSpanHz {
            var kept = settings
            kept.txViewSpanHz = clamped.spanHz
            settings = kept
            keepSettings(kept)
        }
        return clamped
    }

    private func noteTransmit(_ context: MediaControlEvent.DisplayContext) {
        let transmitting = context.transmit == true
        if transmitting {
            transmitGenerations.insert(context.contextGeneration)
        }
        let shared = transmitting && context.grant?.limit == .shared
        if shared != transmitViewShared {
            transmitViewShared = shared
        }
        guard transmitting != showsTransmit else {
            return
        }
        showsTransmit = transmitting
        if !transmitting {
            transmitGenerations.removeAll()
        }
        // The transmit display, or the receiver back: nothing blends across.
        state.resetSmoothing()
        applyToState()
    }

    /// The rise and fall of a key on this band, and the carrier moving while keyed.
    private func transmitChanged(from old: TransmitDisplay) {
        let wasKeyedView = old.keyedHere && gates.txDisplay && !duplexActive
        if transmit.keyedHere != old.keyedHere {
            if transmit.keyedHere {
                // The rise: the receive view is kept for the unkey, and the
                // keyed view takes the band at this phone's own span.
                receiveView = requestedView
                if keyedView {
                    requestView(TransmitDisplay.firstView(carrierHz: transmit.carrierHz,
                                                          spanHz: settings.txViewSpanHz))
                }
            } else {
                // The fall: back to the receive view the band had.
                if wasKeyedView {
                    requestedView = receiveView
                }
                receiveView = nil
                transmitGenerations.removeAll()
            }
            state.resetSmoothing()
        } else if keyedView, transmit.carrierHz != old.carrierHz, old.carrierHz > 0, let current = requestedView {
            // XIT or the frequency moved while keyed: the view follows the carrier.
            requestView(TransmitDisplay.followed(current, from: old.carrierHz, to: transmit.carrierHz))
        }
        applyToState()
        changed()
    }

    /// Display duplex changed: while keyed the band swaps at once, to the
    /// receiver (its receive view back) or to the keyed view.
    private func duplexChanged() {
        guard transmit.keyedHere, gates.txDisplay, gates.displayDuplex else {
            return
        }
        if settings.displayDuplex {
            requestedView = receiveView
        } else {
            requestedView = TransmitDisplay.firstView(carrierHz: transmit.carrierHz, spanHz: settings.txViewSpanHz)
        }
        state.resetSmoothing()
        changed()
    }

    /// Starts over: a new session or a new endpoint.
    func reset() {
        state.clear()
        stackFloorDbm = nil
        contextRevisions.removeAll()
        contextOrder.removeAll()
        retiredFrameSerial = state.committedSerial
        lookBackLines = 0
        changed()
    }

    private func changed() {
        revision &+= 1
    }
}
