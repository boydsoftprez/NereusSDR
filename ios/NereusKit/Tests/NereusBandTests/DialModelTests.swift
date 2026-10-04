// NereusSDR for iOS: the tuning dial's detents, steps, direction and whole kilohertz
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusBand

/// R-IOS-12, D12, spec section 5.1 item 8: 36 detents a turn, one step
/// each, a firmer bump on each whole kilohertz, the direction reversible.
@Suite struct DialModelTests {
    static let start = 14_200_000.0

    private func detents(_ events: [DialEvent]) -> [Double] {
        events.compactMap { event in
            if case .detent(let hz) = event {
                return hz
            }
            return nil
        }
    }

    private func wholeKilohertz(_ events: [DialEvent]) -> [Double] {
        events.compactMap { event in
            if case .wholeKilohertz(let hz) = event {
                return hz
            }
            return nil
        }
    }

    @Test func aFullTurnAt100HzMoves3600HzIn36DetentsWithThreeWholeKilohertz() {
        var dial = DialModel(hz: Self.start, stepHz: 100)
        let events = dial.rotate(byRadians: 2 * .pi)
        #expect(detents(events).count == 36)
        #expect(wholeKilohertz(events) == [14_201_000, 14_202_000, 14_203_000])
        #expect(dial.hz == Self.start + 3_600)
        #expect(detents(events).first == 14_200_100)
        #expect(detents(events).last == 14_203_600)
        // Each whole kilohertz follows the detent that reached it.
        let index = events.firstIndex(of: .wholeKilohertz(14_201_000))
        #expect(index.map { events[$0 - 1] } == .detent(14_201_000))
    }

    @Test func aFullTurnMadeOfSmallMovesMakesTheSame36Detents() {
        var dial = DialModel(hz: Self.start, stepHz: 100)
        var events: [DialEvent] = []
        for _ in 0..<720 {
            events += dial.rotate(byRadians: 2 * .pi / 720)
        }
        #expect(detents(events).count == 36)
        #expect(wholeKilohertz(events).count == 3)
        #expect(dial.hz == Self.start + 3_600)
    }

    @Test func reversedDirectionNegatesTheTurn() {
        var dial = DialModel(hz: Self.start, stepHz: 100, reversed: true)
        let events = dial.rotate(byRadians: 2 * .pi)
        #expect(detents(events).count == 36)
        #expect(dial.hz == Self.start - 3_600)
        // Down from 14.2000 the first whole kilohertz reached is 14.1990.
        #expect(wholeKilohertz(events) == [14_199_000, 14_198_000, 14_197_000])

        var anticlockwise = DialModel(hz: Self.start, stepHz: 100)
        _ = anticlockwise.rotate(byRadians: -2 * .pi)
        #expect(anticlockwise.hz == dial.hz)
    }

    @Test func aStepChangeAppliesFromTheNextDetent() {
        var dial = DialModel(hz: Self.start, stepHz: 100)
        // Half a detent, then the step changes: the next detent is the new step.
        #expect(dial.rotate(byRadians: DialModel.radiansPerDetent * 1.5) == [.detent(14_200_100)])
        dial.stepHz = 1_000
        #expect(dial.rotate(byRadians: DialModel.radiansPerDetent * 0.5)
            == [.detent(14_201_100), .wholeKilohertz(14_201_100)])
    }

    @Test func lessThanADetentMovesNothingAndIsKept() {
        var dial = DialModel(hz: Self.start, stepHz: 100)
        #expect(dial.rotate(byRadians: DialModel.radiansPerDetent * 0.6).isEmpty)
        #expect(dial.hz == Self.start)
        #expect(dial.rotate(byRadians: DialModel.radiansPerDetent * 0.6) == [.detent(14_200_100)])
        dial.start(atHz: 7_000_000)
        #expect(dial.rotate(byRadians: DialModel.radiansPerDetent * 0.6).isEmpty)
    }

    @Test func aStepOfAKilohertzOrMoreBumpsOnEveryDetent() {
        var dial = DialModel(hz: 7_236_400, stepHz: 2_500)
        let events = dial.rotate(byRadians: DialModel.radiansPerDetent * 3)
        #expect(detents(events) == [7_238_900, 7_241_400, 7_243_900])
        #expect(wholeKilohertz(events) == detents(events))
    }

    @Test func offTheKilohertzGridOnlyCrossingsBump() {
        var dial = DialModel(hz: 7_236_450, stepHz: 250)
        let events = dial.rotate(byRadians: DialModel.radiansPerDetent * 4)
        // 7.236700, 7.236950, 7.237200 (passes 7.237000), 7.237450.
        #expect(detents(events) == [7_236_700, 7_236_950, 7_237_200, 7_237_450])
        #expect(wholeKilohertz(events) == [7_237_200])
    }

    @Test func noStepNoTuning() {
        var dial = DialModel(hz: Self.start, stepHz: nil)
        #expect(dial.rotate(byRadians: 2 * .pi).isEmpty)
        #expect(dial.hz == Self.start)
        dial.stepHz = 0
        #expect(dial.rotate(byRadians: 2 * .pi).isEmpty)
    }

    @Test func theDialStopsAtZeroHertz() {
        var dial = DialModel(hz: 150, stepHz: 100)
        #expect(dial.rotate(byRadians: -DialModel.radiansPerDetent * 3) == [.detent(50)])
        #expect(dial.hz == 50)
    }

    @Test func theThumbwheelRollsOneDetentEvery12PointsLeftTuningUp() {
        var wheel = DialModel(hz: Self.start, stepHz: 100)
        #expect(wheel.roll(byPoints: -36) == [.detent(14_200_100), .detent(14_200_200), .detent(14_200_300)])
        #expect(wheel.roll(byPoints: 11).isEmpty)
        #expect(wheel.roll(byPoints: 1) == [.detent(14_200_200)])

        var reversed = DialModel(hz: Self.start, stepHz: 100, reversed: true)
        #expect(reversed.roll(byPoints: -12) == [.detent(14_199_900)])
    }

    @Test func theDialStartsOff() {
        #expect(DialKind.standard == .off)
        #expect(DialKind.allCases == [.off, .waterfallKnob, .sheetKnob, .thumbwheel])
    }
}
