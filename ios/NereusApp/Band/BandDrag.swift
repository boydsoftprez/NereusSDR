// NereusSDR for iOS: one drag on the band: the band pans, or a flag or passband tunes its slice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import NereusBand
import NereusModels

/// One drag on the band, as on the desktop (D74, spec section 5.1 item 3).
/// What it moves is settled where the finger came down and stays so until
/// it lifts:
///
/// - on a flag or passband of a slice this phone may change (with drag to
///   tune on), that slice tunes by the drag's distance, landing on its
///   step, its writes paced and in order (``BandSlicesModel``), with a
///   soft tick each time it moves a step (at most one a frame);
/// - anywhere else the band follows the finger: the view moves, within the
///   span limits, and the slices keep their frequencies. Inside the
///   receiver's window only the view moves. Past it the Core is asked to
///   move the window, as the desktop's remote window does (the window then
///   stays put while the slices tune inside it); a Core that cannot, or
///   refuses because a slice would leave the window, stops the view at the
///   window's edge for the rest of the drag, as it does on the band of a
///   slice this phone only listens to. The view stays where the drag
///   leaves it.
///
/// Two drags up or down move the band's own parts (D78, JJ 2026-09-26),
/// each a shortcut beside a visible control:
///
/// - from the frequency-scale row, marked by its handle at the left end,
///   the split between the spectrum and the waterfall follows the finger,
///   20 to 80 percent, as the Display sheet's Spectrum height sets it;
/// - from the dBm scale under its arrows, the scale follows the finger, its
///   top and bottom together, as its arrows move it; while the band draws
///   3D it moves 3D Floor instead, as the desktop's scale does in 3D and as
///   the arrows do there (JJ's 3D View board): down is deeper, as ▲.
///
/// Across, those places still pan the band or tune a slice, as before.
@MainActor
final class BandDrag {
    /// What the finger moves.
    enum Target: Equatable {
        /// The band: its view when the drag began, and the receiver's
        /// window centre the drag last asked the Core for.
        case band(start: TuneGestures.View, windowCentreHz: Double?, windowRefused: Bool)
        /// A slice, from its frequency when the drag began.
        case slice(id: Int, startHz: Double)
        /// The split, from the spectrum's share when the drag began, over a
        /// band whose spectrum and waterfall share `sharedPoints`.
        case split(startPercent: Double, sharedPoints: CGFloat)
        /// The dBm scale, from its top and bottom when the drag began, over
        /// a spectrum `heightPoints` high.
        case scale(startTopDbm: Double, startBottomDbm: Double, heightPoints: CGFloat)
        /// In 3D, 3D Floor, from its value when the drag began, moving this
        /// many dB for each point the finger moves.
        case floor(startDb: Int, dbPerPoint: Double)
    }

    private(set) var target: Target?
    private let haptics: DialHaptics
    /// Where the dragged slice was last sent, to tick on each step it moves.
    private var lastSliceHz: Double?

    init(haptics: DialHaptics = DialHaptics()) {
        self.haptics = haptics
    }

    /// The finger moved: `translation` points across and `rise` points down
    /// from `start`, where it came down. The split and the scale move only
    /// with the band's `layout` (in points) and the pan's `display` given.
    func changed(from start: CGPoint, translation: CGFloat, rise: CGFloat = 0, geometry: BandGeometry,
                 entries: [BandSlicesModel.Entry], placements: [FlagPlacement], band: BandModel,
                 slices: BandSlicesModel, dragToTune: Bool, layout: BandLayout? = nil,
                 display: DisplaySheetModel? = nil) {
        let current = target
            ?? Self.beginOnTheBandsParts(at: start, translation: translation, rise: rise, layout: layout,
                                         display: display, band: band)
            ?? begin(at: start, geometry: geometry, entries: entries, placements: placements,
                     band: band, slices: slices, dragToTune: dragToTune)
        target = current
        switch current {
        case .split(let startPercent, let shared):
            display?.dragSpectrumHeight(startPercent + Double(rise / max(shared, 1)) * 100)
        case .scale(let top, let bottom, let height):
            // The scale follows the finger: down shows higher levels.
            let dbPerPoint = (top - bottom) / Double(max(height, 1))
            display?.dragScale(topDbm: top + Double(rise) * dbPerPoint, bottomDbm: bottom + Double(rise) * dbPerPoint)
        case .floor(let startDb, let dbPerPoint):
            // Down is deeper, as ▲; whole dB, within 0 to 24.
            display?.dragFloor(to: Double(startDb) + Double(rise) * dbPerPoint)
        case .band(let view, _, _):
            pan(from: view, translation: translation, widthPoints: geometry.size.width, band: band, slices: slices)
        case .slice(let id, let startHz):
            let step = entries.first { $0.id == id }.flatMap { Self.snapStep($0, catalog: band.catalog) }
            let hz = TuneGestures.slid(fromHz: startHz, translation: translation, geometry: geometry, stepHz: step)
            if lastSliceHz == nil {
                lastSliceHz = startHz
                haptics.prepare()
            }
            // A tick per step moved; a slice with no step moves smoothly and does not tick.
            if let step, step > 0, hz != lastSliceHz {
                haptics.dragStep()
            }
            lastSliceHz = hz
            slices.drag(sliceId: id, to: hz)
        }
    }

    /// The finger lifted.
    func ended(slices: BandSlicesModel) {
        if case .slice = target {
            slices.finishDrag()
        }
        target = nil
        lastSliceHz = nil
    }

    /// The step a slice lands on: its own, or the Core's smallest.
    static func snapStep(_ entry: BandSlicesModel.Entry, catalog: StationCatalog?) -> Double? {
        TuneGestures.snapStep(sliceStepHz: entry.stepHz, catalogueStepsHz: catalog?.tuneSteps.map(\.hz) ?? [])
    }

    // MARK: Inside

    /// A drag that starts more up or down than across on the frequency-scale
    /// row moves the split; on the dBm scale under its arrows, the scale.
    /// Anything else is left to the band and the slices.
    static func beginOnTheBandsParts(at point: CGPoint, translation: CGFloat, rise: CGFloat, layout: BandLayout?,
                                     display: DisplaySheetModel?, band: BandModel) -> Target? {
        guard let layout, display != nil, abs(rise) > abs(translation) else {
            return nil
        }
        // While keyed on the band the scale is the transmit grid.
        let settings = band.drawnSettings
        if layout.frequencyScale.contains(point) {
            let shared = layout.spectrum.height + layout.waterfall.height
            // From the split as shown: while another slice's band holds it
            // down, the drag starts where the finger is and saves as usual.
            return .split(startPercent: Double(band.shownSpectrumShare) * 100, sharedPoints: shared)
        }
        if layout.dbmScale.contains(point), point.y >= layout.dbmArrows.maxY, band.drawsStack {
            // The front row's tallest ridge stands for the height range:
            // the finger moves the floor that many dB over that height.
            let plot = layout.spectrum.height - layout.strip.height
            let shape = StackedTraceShape.forAngle(settings.threeDAngle)
            let range = StackedTrace.heightRangeDb(scale: settings.scaleRange)
            return .floor(startDb: settings.threeDFloorDb,
                          dbPerPoint: range / Double(max(shape.ridge * Double(plot), 1)))
        }
        if layout.dbmScale.contains(point), point.y >= layout.dbmArrows.maxY {
            return .scale(startTopDbm: settings.scaleTopDbm, startBottomDbm: settings.scaleBottomDbm,
                          heightPoints: layout.spectrum.height - layout.strip.height)
        }
        return nil
    }

    private func begin(at point: CGPoint, geometry: BandGeometry, entries: [BandSlicesModel.Entry],
                       placements: [FlagPlacement], band: BandModel, slices: BandSlicesModel,
                       dragToTune: Bool) -> Target {
        let tunable = dragToTune ? slices.tunableIds : []
        let hit = TuneGestures.dragTarget(at: point, slices: entries.map(\.slice), placements: placements,
                                          activeSliceId: slices.activeSliceId, geometry: geometry, tunable: tunable)
        if case .slice(let id) = hit, let entry = entries.first(where: { $0.id == id }) {
            return .slice(id: id, startHz: entry.slice.frequencyHz)
        }
        return .band(start: band.requestedView ?? geometry.view, windowCentreHz: nil, windowRefused: false)
    }

    private func pan(from start: TuneGestures.View, translation: CGFloat, widthPoints: CGFloat, band: BandModel,
                     slices: BandSlicesModel) {
        guard case .band(_, let askedCentre, let refused) = target else {
            return
        }
        var wanted = TuneGestures.panned(start, translation: translation, widthPoints: widthPoints)
        let limits = band.spanLimits(sampleRateHz: slices.active?.sampleRateHz)
        wanted.spanHz = min(max(wanted.spanHz, limits.lowerBound), limits.upperBound)
        guard let window = band.receiverWindow() else {
            band.requestView(wanted)
            return
        }
        let width = window.upperBound - window.lowerBound
        let current = askedCentre.map { TuneGestures.receiverWindow(centreHz: $0, spanHz: width) } ?? window
        if TuneGestures.fits(wanted, inside: current) {
            band.requestView(wanted)
            return
        }
        // Only a slice this phone controls moves its receiver's window: the
        // Core refuses both verbs on a slice the phone only listens to
        // (SessionCommandDispatcher::refusedForAnotherDevice, the slice
        // verbs), so on a listened slice's band the view stops at the window.
        guard !refused, slices.canMoveReceiverWindow, let entry = slices.active, !entry.listening else {
            band.requestView(TuneGestures.fitted(wanted, inside: current))
            return
        }
        let active = entry.id
        let centre = TuneGestures.windowCentre(covering: wanted, window: current)
        target = .band(start: start, windowCentreHz: centre, windowRefused: false)
        band.requestView(wanted)
        slices.moveReceiverWindow(sliceId: active, centreHz: centre) { [weak self, weak band] _ in
            // The Core keeps the window where it is: the view goes back
            // inside it, and the rest of this drag asks no more.
            if let self, case .band(let start, _, _) = self.target {
                self.target = .band(start: start, windowCentreHz: nil, windowRefused: true)
            }
            if let band, let window = band.receiverWindow(), let view = band.requestedView {
                band.requestView(TuneGestures.fitted(view, inside: window))
            }
        }
    }
}
