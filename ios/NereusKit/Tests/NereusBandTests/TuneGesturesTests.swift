// NereusSDR for iOS: dragging and tapping the band to tune, snapping, pacing the writes, and zooming
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Testing
@testable import NereusBand

/// R-IOS-12, spec section 5.1 items 3 and 9: tap or drag the band to tune
/// the active slice, a tap optionally snapped to the step, the drag's
/// writes paced, pinch and the zoom buttons.
@Suite struct TuneGesturesTests {
    static let geometry = BandGeometry(centerHz: 7_245_000, spanHz: 48_000, size: CGSize(width: 390, height: 300),
                                       dbmRange: -140 ... -40)

    // MARK: Drag

    @Test func aDragOf100PointsTunesBy100PointsWorthOfHertz() {
        let perPoint = Self.geometry.spanHz / Double(Self.geometry.size.width)
        let start = 7_236_400.0
        #expect(TuneGestures.dragged(fromHz: start, translation: 100, geometry: Self.geometry)
            == (start + 100 * perPoint).rounded())
        #expect(TuneGestures.dragged(fromHz: start, translation: -100, geometry: Self.geometry)
            == (start - 100 * perPoint).rounded())
        #expect(TuneGestures.dragged(fromHz: start, translation: 0, geometry: Self.geometry) == start)
    }

    @Test func aDragStaysOnTheBand() {
        #expect(TuneGestures.dragged(fromHz: 7_260_000, translation: 1_000, geometry: Self.geometry)
            == Self.geometry.highHz)
        #expect(TuneGestures.dragged(fromHz: 7_230_000, translation: -1_000, geometry: Self.geometry)
            == Self.geometry.lowHz)
    }

    // MARK: Pan and slide (D74)

    @Test func aDragOnEmptyBandMovesTheViewByTheDraggedDistance() {
        let start = TuneGestures.View(centerHz: 7_245_000, spanHz: 48_000)
        let right = TuneGestures.panned(start, translation: 100, widthPoints: 390)
        // The band follows the finger: dragging right brings lower frequencies in.
        #expect(abs(right.centerHz - (7_245_000 - 100.0 / 390 * 48_000)) < 1e-6)
        #expect(right.spanHz == 48_000)
        let left = TuneGestures.panned(start, translation: -390, widthPoints: 390)
        #expect(left.centerHz == 7_293_000)
    }

    @Test func aDragOnAFlagMovesTheSliceInItsStep() {
        let perPoint = Self.geometry.spanHz / Double(Self.geometry.size.width)
        let hz = TuneGestures.slid(fromHz: 7_236_400, translation: 10, geometry: Self.geometry, stepHz: 100)
        #expect(hz == TuneGestures.snapped(7_236_400 + 10 * perPoint, stepHz: 100))
        #expect(hz.truncatingRemainder(dividingBy: 100) == 0)
        #expect(TuneGestures.slid(fromHz: 7_236_400, translation: 0, geometry: Self.geometry, stepHz: 1_000) == 7_236_000)
        #expect(TuneGestures.slid(fromHz: 7_236_450, translation: 0, geometry: Self.geometry, stepHz: nil) == 7_236_450)
    }

    @Test func aDragStartingOnAFlagOrItsPassbandTunesThatSliceAndElsewhereMovesTheBand() {
        let a = BandSlice(id: 0, frequencyHz: 7_236_400, filterLowHz: -2_900, filterHighHz: -100, colour: "#102030",
                          lowerSideband: true)
        let b = BandSlice(id: 1, frequencyHz: 7_255_000, filterLowHz: 100, filterHighHz: 2_900, colour: "#102030",
                          lowerSideband: false)
        let placements = FlagLayout.layout(slices: [a, b], activeSliceId: 0, geometry: Self.geometry)
        func target(_ point: CGPoint, tunable: Set<Int> = [0, 1]) -> TuneGestures.DragTarget {
            TuneGestures.dragTarget(at: point, slices: [a, b], placements: placements, activeSliceId: 0,
                                    geometry: Self.geometry, tunable: tunable)
        }
        let flag = placements[0].rect
        #expect(target(CGPoint(x: flag.midX, y: flag.midY)) == .slice(0))
        // Down in the waterfall, on A's passband.
        let passband = (Self.geometry.x(forHz: 7_236_400 - 1_500))
        #expect(target(CGPoint(x: passband, y: 280)) == .slice(0))
        let bLine = Self.geometry.x(forHz: 7_255_000 + 1_500)
        #expect(target(CGPoint(x: bLine, y: 280)) == .slice(1))
        // Empty band.
        #expect(target(CGPoint(x: 5, y: 280)) == .band)
        // A slice this phone may not change gives way to the band.
        #expect(target(CGPoint(x: flag.midX, y: flag.midY), tunable: [1]) == .band)
        #expect(target(CGPoint(x: passband, y: 280), tunable: [1]) == .band)
    }

    @Test func aNarrowPassbandIsStillWideEnoughForAFinger() {
        let cw = BandSlice(id: 0, frequencyHz: 7_245_000, filterLowHz: 100, filterHighHz: 200, colour: "#102030",
                           lowerSideband: false)
        let line = Self.geometry.x(forHz: 7_245_000)
        #expect(TuneGestures.passbandContains(line + 11, slice: cw, geometry: Self.geometry))
        #expect(!TuneGestures.passbandContains(line + 20, slice: cw, geometry: Self.geometry))
    }

    @Test func aPanPastTheReceiversWindowMovesItTheLeastAndOtherwiseStaysInside() {
        let window = TuneGestures.receiverWindow(centreHz: 7_200_000, spanHz: 192_000)
        #expect(window == 7_104_000...7_296_000)
        let inside = TuneGestures.View(centerHz: 7_250_000, spanHz: 48_000)
        #expect(TuneGestures.fits(inside, inside: window))
        #expect(TuneGestures.fitted(inside, inside: window) == inside)
        let past = TuneGestures.View(centerHz: 7_290_000, spanHz: 48_000)
        #expect(!TuneGestures.fits(past, inside: window))
        #expect(TuneGestures.fitted(past, inside: window).centerHz == 7_272_000)
        #expect(TuneGestures.windowCentre(covering: past, window: window) == 7_218_000)
        let below = TuneGestures.View(centerHz: 7_100_000, spanHz: 48_000)
        #expect(TuneGestures.windowCentre(covering: below, window: window) == 7_172_000)
        // A view as wide as the window sits in its middle when it cannot move.
        let full = TuneGestures.View(centerHz: 7_210_000, spanHz: 192_000)
        #expect(TuneGestures.fitted(full, inside: window).centerHz == 7_200_000)
    }

    // MARK: Tap

    @Test func aTapTunesToTheFrequencyUnderTheFinger() {
        let hz = TuneGestures.tapped(atX: 195, geometry: Self.geometry, snap: false, stepHz: 500)
        #expect(hz == 7_245_000)
        let off = TuneGestures.tapped(atX: 100, geometry: Self.geometry, snap: false, stepHz: 500)
        #expect(off == Self.geometry.hz(forX: 100).rounded())
        #expect(off.truncatingRemainder(dividingBy: 500) != 0)
    }

    @Test func withSnapOnATapLandsOnTheNearestStep() {
        // x 100 is 7,233,307.69 Hz.
        #expect(TuneGestures.tapped(atX: 100, geometry: Self.geometry, snap: true, stepHz: 500) == 7_233_500)
        #expect(TuneGestures.tapped(atX: 100, geometry: Self.geometry, snap: true, stepHz: 1_000) == 7_233_000)
        #expect(TuneGestures.tapped(atX: 100, geometry: Self.geometry, snap: true, stepHz: 10) == 7_233_310)
        // No step known: the frequency under the finger.
        #expect(TuneGestures.tapped(atX: 100, geometry: Self.geometry, snap: true, stepHz: nil) == 7_233_308)
        #expect(TuneGestures.snapped(7_233_250, stepHz: 500) == 7_233_500)
    }

    @Test func theStepIsTheSlicesOwnOrElseTheCoresSmallest() {
        #expect(TuneGestures.snapStep(sliceStepHz: 500, catalogueStepsHz: [1, 10, 100]) == 500)
        #expect(TuneGestures.snapStep(sliceStepHz: 0, catalogueStepsHz: [1, 10, 100]) == 1)
        #expect(TuneGestures.snapStep(sliceStepHz: nil, catalogueStepsHz: []) == nil)
    }

    // MARK: Pacing the writes

    @Test func aDragWritesAtMostEvery50MillisecondsAndTheFinalValueAtTheEnd() {
        var throttle = TuneThrottle()
        var sent: [Double] = []
        func take(_ decision: TuneThrottle.Decision) {
            if case .send(let hz) = decision {
                sent.append(hz)
            }
        }
        // The finger moves every 10 ms for 200 ms: 7,000,000 then up by 10 Hz a move.
        for move in 0...20 {
            take(throttle.offer(7_000_000 + Double(move) * 10, at: Double(move * 10) / 1000))
        }
        // One write at 0 ms, then one each time 50 ms have passed.
        #expect(sent == [7_000_000, 7_000_050, 7_000_100, 7_000_150, 7_000_200])
        take(throttle.offer(7_000_205, at: 0.205))
        #expect(sent.count == 5)
        // The finger lifts: the final value goes.
        #expect(throttle.finish() == 7_000_205)
        // Nothing left over for the next drag.
        #expect(throttle.pending == nil && throttle.lastSent == nil)
    }

    @Test func aHeldValueGoesWhenItsTimeComes() {
        var throttle = TuneThrottle()
        #expect(throttle.offer(1_000, at: 0) == .send(1_000))
        #expect(throttle.offer(1_010, at: 0.02) == .hold(until: 0.05))
        #expect(throttle.due(at: 0.03) == nil)
        #expect(throttle.due(at: 0.05) == 1_010)
        // Sent already: nothing more at the end.
        #expect(throttle.finish() == nil)
    }

    @Test func anUnchangedValueIsNotSentAgain() {
        var throttle = TuneThrottle()
        #expect(throttle.offer(1_000, at: 0) == .send(1_000))
        #expect(throttle.offer(1_000, at: 0.1) == .nothing)
        #expect(throttle.finish() == nil)
    }

    // MARK: Zoom

    @Test func theZoomButtonsCloseInAndBackOutAboutTheActiveSlice() {
        let slice = 7_236_400.0
        let limits = TuneGestures.spanLimits(sampleRateHz: 192_000)
        let closer = TuneGestures.zoomed(Self.geometry, by: TuneGestures.buttonZoomFactor, aboutHz: slice,
                                         spanLimits: limits)
        #expect(abs(closer.spanHz - 48_000 * 0.7) < 1e-6)
        // The slice keeps its place across the band.
        var zoomed = Self.geometry
        zoomed.centerHz = closer.centerHz
        zoomed.spanHz = closer.spanHz
        #expect(abs(zoomed.x(forHz: slice) - Self.geometry.x(forHz: slice)) < 1e-6)
        let wider = TuneGestures.zoomed(Self.geometry, by: 1 / TuneGestures.buttonZoomFactor, aboutHz: slice,
                                        spanLimits: limits)
        #expect(abs(wider.spanHz - 48_000 / 0.7) < 1e-6)
    }

    @Test func theSpanStaysWithinItsLimits() {
        let limits = TuneGestures.spanLimits(sampleRateHz: 48_000)
        #expect(limits == 1_000...48_000)
        #expect(TuneGestures.zoomed(Self.geometry, by: 4, aboutHz: 7_245_000, spanLimits: limits).spanHz == 48_000)
        #expect(TuneGestures.zoomed(Self.geometry, by: 0.001, aboutHz: 7_245_000, spanLimits: limits).spanHz == 1_000)
        #expect(TuneGestures.spanLimits(sampleRateHz: nil).lowerBound == TuneGestures.minimumSpanHz)
    }

    @Test func aPinchZoomsAboutTheFingers() {
        let limits = TuneGestures.spanLimits(sampleRateHz: 192_000)
        let about = Self.geometry.hz(forX: 300)
        // Fingers spreading to twice their distance halve the span.
        let view = TuneGestures.zoomed(Self.geometry, by: 1 / 2.0, aboutHz: about, spanLimits: limits)
        var zoomed = Self.geometry
        zoomed.centerHz = view.centerHz
        zoomed.spanHz = view.spanHz
        #expect(view.spanHz == 24_000)
        #expect(abs(zoomed.x(forHz: about) - 300) < 1e-6)
    }

    @Test func aDoubleTapDoesWhatSetupSays() {
        // x 100 is 7,233,307.69 Hz.
        #expect(TuneGestures.doubleTapped(.none, atX: 100, geometry: Self.geometry, snap: false, stepHz: 500) == nil)
        #expect(TuneGestures.doubleTapped(.tune, atX: 100, geometry: Self.geometry, snap: false, stepHz: 500)
            == .tune(7_233_308))
        #expect(TuneGestures.doubleTapped(.tune, atX: 100, geometry: Self.geometry, snap: true, stepHz: 500)
            == .tune(7_233_500))
        // Center keeps the span and retunes nothing.
        let center = TuneGestures.doubleTapped(.center, atX: 100, geometry: Self.geometry, snap: true, stepHz: 500)
        #expect(center == .view(TuneGestures.View(centerHz: Self.geometry.hz(forX: 100), spanHz: 48_000)))
        // The desktop's Double-click action: Tune, Center, None, Tune first.
        #expect(TuneGestures.DoubleTapAction.allCases.map(\.rawValue) == ["tune", "center", "none"])
        #expect(TuneGestures.DoubleTapAction.standard == .tune)
    }

    @Test func aTapTunesAtOnceWhenADoubleTapWouldDoTheSame() {
        // The defaults: tap to tune on, double tap Tune. No wait.
        let defaults = TuneGestures.tapHandling(tapToTune: true, doubleTap: .standard)
        #expect(defaults == .singleAtOnce)
        #expect(!defaults.waitsForDoubleTap)
        #expect(TuneGestures.tapHandling(tapToTune: true, doubleTap: .none) == .singleAtOnce)
    }

    @Test func aTapWaitsOnlyWhenADoubleTapDoesSomethingElse() {
        let center = TuneGestures.tapHandling(tapToTune: true, doubleTap: .center)
        #expect(center == .doubleThenSingle)
        #expect(center.waitsForDoubleTap)
        // Tap to tune off: only the double tap acts.
        #expect(TuneGestures.tapHandling(tapToTune: false, doubleTap: .tune) == .doubleOnly)
        #expect(TuneGestures.tapHandling(tapToTune: false, doubleTap: .center) == .doubleOnly)
        #expect(TuneGestures.tapHandling(tapToTune: false, doubleTap: .none) == TuneGestures.TapHandling.none)
    }
}
