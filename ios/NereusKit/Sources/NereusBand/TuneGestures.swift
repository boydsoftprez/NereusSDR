// NereusSDR for iOS: the arithmetic behind the band's touches: pan, drag and tap to tune, snapping, pinch and zoom
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation

/// What a touch on the band does to the slices and to the band's view
/// (R-IOS-12, D74, spec section 5.1 items 3, 9 and 17). A drag that starts
/// on empty band moves the band; one that starts on a flag or its passband
/// tunes that slice in its step; a tap tunes the active slice there.
/// Everything here is in the band's points and hertz; the screens turn the
/// result into the slice's frequency write or the band's next view.
public enum TuneGestures {
    /// What a drag moves, settled where the finger first came down.
    public enum DragTarget: Equatable, Sendable {
        /// Empty band, or a slice this phone may not change: the view pans.
        case band
        /// This slice's flag or passband: the slice tunes.
        case slice(Int)
    }

    /// What a double tap on the band does (Setup, General, Navigation, Touch):
    /// the desktop's Double-click action, Tune, Center or None, Tune first.
    public enum DoubleTapAction: String, Equatable, Sendable, CaseIterable {
        /// Tunes the active slice to the frequency under the finger,
        /// snapped to the step when tap-to-tune snaps.
        case tune
        /// Moves the band so the frequency under the finger is in the
        /// middle, without retuning.
        case center
        /// Nothing: a tap tunes at once, without waiting to see if a second follows.
        case none

        /// Setup's default, as on the desktop.
        public static let standard = DoubleTapAction.tune
    }

    /// What a double tap asks for.
    public enum DoubleTapOutcome: Equatable, Sendable {
        /// Write this frequency to the active slice.
        case tune(Double)
        /// Ask for this view of the band.
        case view(View)
    }

    /// Which taps the band listens for, and whether a single tap waits to
    /// rule out a double tap.
    public enum TapHandling: Equatable, Sendable {
        /// Neither a tap nor a double tap does anything.
        case none
        /// A tap acts at once. A double tap would do the same thing again
        /// (Tune while tap to tune is on), so there is nothing to wait for.
        case singleAtOnce
        /// Only a double tap acts (tap to tune off).
        case doubleOnly
        /// A tap and a double tap do different things (Center while tap to
        /// tune is on): a tap waits until a second one is ruled out.
        case doubleThenSingle

        /// Whether a single tap waits to rule out a double tap.
        public var waitsForDoubleTap: Bool { self == .doubleThenSingle }
    }

    /// How the band handles taps with these Touch settings.
    public static func tapHandling(tapToTune: Bool, doubleTap: DoubleTapAction) -> TapHandling {
        switch (tapToTune, doubleTap) {
        case (true, .tune), (true, .none):
            return .singleAtOnce
        case (true, .center):
            return .doubleThenSingle
        case (false, .tune), (false, .center):
            return .doubleOnly
        case (false, .none):
            return .none
        }
    }

    /// A band's centre and span.
    public struct View: Equatable, Sendable {
        public var centerHz: Double
        public var spanHz: Double

        public init(centerHz: Double, spanHz: Double) {
            self.centerHz = centerHz
            self.spanHz = spanHz
        }
    }

    /// How far a finger moves before a touch is a drag and not a tap, in points.
    public static let dragThresholdPoints: CGFloat = 6
    /// The zoom buttons' step: plus shows this share of the span, minus the
    /// span divided by it.
    public static let buttonZoomFactor = 0.7
    /// The narrowest span the band asks for, in hertz.
    public static let minimumSpanHz = 1_000.0

    /// The frequency a drag of `translation` points across the band gives,
    /// from `startHz` where the drag began: the line follows the finger, so
    /// dragging right tunes up. Whole hertz, kept inside the band.
    public static func dragged(fromHz startHz: Double, translation: CGFloat, geometry: BandGeometry) -> Double {
        guard geometry.size.width > 0 else {
            return startHz
        }
        let hz = startHz + Double(translation) / Double(geometry.size.width) * geometry.spanHz
        return inside(hz.rounded(), geometry)
    }

    /// The frequency a drag on a slice's flag or passband gives: the slice
    /// moves by the drag's distance from `startHz`, where it was when the
    /// drag began, and lands on its step (whole hertz without one).
    public static func slid(fromHz startHz: Double, translation: CGFloat, geometry: BandGeometry,
                            stepHz: Double?) -> Double {
        let hz = dragged(fromHz: startHz, translation: translation, geometry: geometry)
        guard let stepHz, stepHz > 0 else {
            return hz
        }
        return snapped(hz, stepHz: stepHz)
    }

    /// The narrowest a passband is to a finger, in points: a narrower one
    /// (a CW filter zoomed out) is grabbed this wide about its middle.
    public static let passbandGrabPoints: CGFloat = 24

    /// What a drag starting at `point` moves: the topmost flag or tag under
    /// it (the active slice's is drawn on top), else a passband under it
    /// (the active slice's first), else the band. A slice not in `tunable`
    /// (one this phone may not change) gives way to the band.
    /// `placements` are in the order of `slices`, as ``FlagLayout`` gives them.
    public static func dragTarget(at point: CGPoint, slices: [BandSlice], placements: [FlagPlacement],
                                  activeSliceId: Int?, geometry: BandGeometry, tunable: Set<Int>) -> DragTarget {
        let pairs = Array(zip(slices, placements))
        let activeFirst = pairs.sorted { a, b in
            (a.0.id == activeSliceId ? 0 : 1, a.0.id) < (b.0.id == activeSliceId ? 0 : 1, b.0.id)
        }
        let hit = activeFirst.first { $0.1.rect.contains(point) }?.0
            ?? activeFirst.first { passbandContains(Double(point.x), slice: $0.0, geometry: geometry) }?.0
        guard let hit, tunable.contains(hit.id) else {
            return .band
        }
        return .slice(hit.id)
    }

    /// Whether `x` is on the slice's passband (with its line), at least
    /// ``passbandGrabPoints`` wide.
    static func passbandContains(_ x: Double, slice: BandSlice, geometry: BandGeometry) -> Bool {
        let line = geometry.x(forHz: slice.frequencyHz)
        var low = min(geometry.x(forHz: slice.passbandHz.lowerBound), line)
        var high = max(geometry.x(forHz: slice.passbandHz.upperBound), line)
        let grab = Double(passbandGrabPoints)
        if high - low < grab {
            let middle = (low + high) / 2
            low = middle - grab / 2
            high = middle + grab / 2
        }
        return x >= low && x <= high
    }

    /// The view after a drag of `translation` points across a band
    /// `widthPoints` wide, from `start`: the band follows the finger, so
    /// dragging right shows lower frequencies. The span stays.
    public static func panned(_ start: View, translation: CGFloat, widthPoints: CGFloat) -> View {
        guard widthPoints > 0, start.spanHz > 0 else {
            return start
        }
        let centre = start.centerHz - Double(translation) / Double(widthPoints) * start.spanHz
        return View(centerHz: centre, spanHz: start.spanHz)
    }

    /// The frequencies the receiver hears: `spanHz` about the receiver's
    /// centre (its sample rate, or with Extended view the wider ceiling).
    public static func receiverWindow(centreHz: Double, spanHz: Double) -> ClosedRange<Double> {
        let half = max(spanHz, 0) / 2
        return (centreHz - half)...(centreHz + half)
    }

    /// Whether `view` lies within `window`.
    public static func fits(_ view: View, inside window: ClosedRange<Double>) -> Bool {
        view.centerHz - view.spanHz / 2 >= window.lowerBound && view.centerHz + view.spanHz / 2 <= window.upperBound
    }

    /// `view` moved the least that puts it within `window`; a view wider
    /// than the window sits in its middle.
    public static func fitted(_ view: View, inside window: ClosedRange<Double>) -> View {
        let windowSpan = window.upperBound - window.lowerBound
        guard view.spanHz < windowSpan else {
            return View(centerHz: (window.lowerBound + window.upperBound) / 2, spanHz: view.spanHz)
        }
        let low = min(max(view.centerHz - view.spanHz / 2, window.lowerBound), window.upperBound - view.spanHz)
        return View(centerHz: low + view.spanHz / 2, spanHz: view.spanHz)
    }

    /// The receiver's centre, in whole hertz, moved the least that brings
    /// `view` within a window of the same width as `window`: where a pan
    /// past the receiver's window asks the Core to move it.
    public static func windowCentre(covering view: View, window: ClosedRange<Double>) -> Double {
        let centre = (window.lowerBound + window.upperBound) / 2
        let low = view.centerHz - view.spanHz / 2
        let high = view.centerHz + view.spanHz / 2
        if low < window.lowerBound {
            return (centre - (window.lowerBound - low)).rounded()
        }
        if high > window.upperBound {
            return (centre + (high - window.upperBound)).rounded()
        }
        return centre.rounded()
    }

    /// The frequency a tap at `x` gives: the frequency under the finger in
    /// whole hertz, or, with `snap` on, the nearest multiple of `stepHz`.
    public static func tapped(atX x: CGFloat, geometry: BandGeometry, snap: Bool, stepHz: Double?) -> Double {
        let hz = geometry.hz(forX: Double(x))
        if snap, let stepHz, stepHz > 0 {
            return snapped(hz, stepHz: stepHz)
        }
        return max(0, hz.rounded())
    }

    /// `hz` on the nearest multiple of `stepHz` (a half step rounds up).
    public static func snapped(_ hz: Double, stepHz: Double) -> Double {
        guard stepHz > 0, hz.isFinite else {
            return hz
        }
        return max(0, (hz / stepHz).rounded(.toNearestOrAwayFromZero) * stepHz)
    }

    /// The step a snapped tap lands on: the slice's own step while it is
    /// set, otherwise the smallest of the Core's steps.
    public static func snapStep(sliceStepHz: Double?, catalogueStepsHz: [Double]) -> Double? {
        if let sliceStepHz, sliceStepHz > 0 {
            return sliceStepHz
        }
        return catalogueStepsHz.first { $0 > 0 }
    }

    /// The view after zooming by `factor` (below one closes in) about
    /// `aboutHz`, which keeps its place across the band. The span stays
    /// within `spanLimits`.
    public static func zoomed(_ geometry: BandGeometry, by factor: Double, aboutHz: Double,
                              spanLimits: ClosedRange<Double>) -> View {
        guard factor.isFinite, factor > 0, geometry.spanHz > 0 else {
            return View(centerHz: geometry.centerHz, spanHz: geometry.spanHz)
        }
        let span = min(max(geometry.spanHz * factor, spanLimits.lowerBound), spanLimits.upperBound)
        let zoomed = geometry.zoomed(by: span / geometry.spanHz, about: aboutHz)
        return View(centerHz: zoomed.centerHz, spanHz: zoomed.spanHz)
    }

    /// The span limits: from ``minimumSpanHz`` to the slice's sample rate
    /// (all the radio sends for it), never below the minimum.
    public static func spanLimits(sampleRateHz: Double?) -> ClosedRange<Double> {
        guard let sampleRateHz, sampleRateHz.isFinite, sampleRateHz > minimumSpanHz else {
            return minimumSpanHz...Double.greatestFiniteMagnitude
        }
        return minimumSpanHz...sampleRateHz
    }

    /// What a double tap at `x` asks for, or nil when it asks for nothing.
    /// Tune lands as a tap does (`snap` and `stepHz` as for a tap); Center
    /// keeps the span and puts the frequency under the finger in the middle.
    public static func doubleTapped(_ action: DoubleTapAction, atX x: CGFloat, geometry: BandGeometry, snap: Bool,
                                    stepHz: Double?) -> DoubleTapOutcome? {
        switch action {
        case .none:
            return nil
        case .tune:
            return .tune(tapped(atX: x, geometry: geometry, snap: snap, stepHz: stepHz))
        case .center:
            return .view(View(centerHz: geometry.hz(forX: Double(x)), spanHz: geometry.spanHz))
        }
    }

    private static func inside(_ hz: Double, _ geometry: BandGeometry) -> Double {
        max(0, min(max(hz, geometry.lowHz.rounded(.up)), geometry.highHz.rounded(.down)))
    }
}

public extension BandGeometry {
    /// The band's centre and span as a view to ask for.
    var view: TuneGestures.View { TuneGestures.View(centerHz: centerHz, spanHz: spanHz) }
}
