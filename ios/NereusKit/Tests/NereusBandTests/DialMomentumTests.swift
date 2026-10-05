// NereusSDR for iOS: the tuning dial's feel: following the finger, then coasting to a stop
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusBand

/// R-IOS-12 (Task 61, fix round 1): the dial follows the finger smoothly,
/// coasts on after a flick under a steady friction and stops, and a touch
/// stops a coast. Every time here is on a test clock, 120 frames a second.
@Suite struct DialMomentumTests {
    static let frame = 1.0 / 120
    static let detent = DialModel.radiansPerDetent

    private func detents(_ events: [DialEvent]) -> Int {
        events.filter { if case .detent = $0 { true } else { false } }.count
    }

    @Test func aSlowDragTurnsContinuouslyAndClicksAtEachDetentAngle() {
        var momentum = DialMomentum()
        var dial = DialModel(hz: 14_200_000, stepHz: 100)
        momentum.touch(at: 0)
        // A slow roll: a tenth of a detent a frame, 1.45 rad/s.
        var angle = 0.0
        var clicks: [Double] = []
        for index in 1...95 {
            let delta = Self.detent / 10
            angle += delta
            momentum.follow(byRadians: delta, at: Double(index) * Self.frame)
            if detents(dial.rotate(byRadians: delta)) > 0 {
                clicks.append(angle)
            }
        }
        // The drawing moved every frame (the angle is the finger's), and the
        // dial clicked at each whole detent: 9 clicks in 9.5 detents.
        #expect(abs(angle - 9.5 * Self.detent) < 1e-9)
        #expect(clicks.count == 9)
        for (index, at) in clicks.enumerated() {
            #expect(abs(at - Double(index + 1) * Self.detent) < 1e-9)
        }
        #expect(dial.hz == 14_200_900)
    }

    @Test func aFlickCoastsForTheTimeTheFrictionSetsAndClicksTheDetentsItPredicts() {
        let feel = DialFeel.standard
        var momentum = DialMomentum(feel: feel)
        var dial = DialModel(hz: 14_200_000, stepHz: 100)
        // The finger spins at 20 rad/s for 60 ms, then lifts while moving.
        momentum.touch(at: 0)
        var time = 0.0
        var fingerDetents = 0
        for _ in 0..<8 {
            time += Self.frame
            momentum.follow(byRadians: 20 * Self.frame, at: time)
            fingerDetents += detents(dial.rotate(byRadians: 20 * Self.frame))
        }
        #expect(abs(momentum.releaseSpeed(at: time) - 20) < 1e-9)
        let coasts = momentum.release(at: time)
        #expect(coasts)
        #expect(momentum.isCoasting)
        let start = time
        var coast = 0.0
        var coastDetents = 0
        var frames = 0
        while momentum.isCoasting, frames < 1_000 {
            time += Self.frame
            frames += 1
            let delta = momentum.advance(to: time)
            #expect(delta >= 0)
            coast += delta
            coastDetents += detents(dial.rotate(byRadians: delta))
        }
        let expected = feel.coastDuration(from: 20)
        #expect(abs(expected - log(40) / 2.5) < 1e-12)
        // It stops on the first frame at or after the coast's end: 1.48 s.
        #expect(time - start >= expected)
        #expect(time - start < expected + Self.frame)
        #expect(time - start > 1 && time - start < 2)
        #expect(abs(coast - feel.coastDistance(from: 20)) < 1e-9)
        // The finger's 1.33 rad (7.64 detents) and the coast's (20 - 0.5) / 2.5
        // = 7.8 rad make 52.3 detents: 7 under the finger, 45 coasting.
        let finger = 8 * 20 * Self.frame
        let predicted = Int(((finger + feel.coastDistance(from: 20)) / Self.detent).rounded(.down)) - fingerDetents
        #expect(fingerDetents == 7)
        #expect(coastDetents == predicted)
        #expect(coastDetents == 45)
        #expect(momentum.velocity == 0)
        let after = momentum.advance(to: time + 1)
        #expect(after == 0)
    }

    @Test func theFeelsCoastTimesMatchTheReport() {
        let feel = DialFeel.standard
        #expect(abs(feel.coastDuration(from: 40) - 1.7527) < 1e-3)
        #expect(Int(feel.coastDistance(from: 40) / Self.detent) == 90)
        #expect(Int(feel.coastDistance(from: 2) / Self.detent) == 3)
        #expect(abs(feel.coastDuration(from: 2) - 0.5545) < 1e-3)
        // Faster than the cap coasts as the cap.
        #expect(feel.coastDuration(from: 400) == feel.coastDuration(from: 40))
        #expect(feel.coastDistance(from: -40) == -feel.coastDistance(from: 40))
        #expect(feel.coastDuration(from: 0.4) == 0)
    }

    @Test func aStrongerFlickRunsFurtherAndLongerThanAGentleOne() {
        let feel = DialFeel.standard
        #expect(feel.coastDuration(from: 30) > feel.coastDuration(from: 3))
        #expect(feel.coastDistance(from: 30) > 10 * feel.coastDistance(from: 3))
    }

    @Test func aTouchDuringTheCoastStopsItAtOnce() {
        var momentum = DialMomentum()
        let launched = momentum.launch(speed: -30, at: 0)
        #expect(launched)
        var moved = 0.0
        for index in 1...30 {
            moved += momentum.advance(to: Double(index) * Self.frame)
        }
        #expect(moved < 0)
        #expect(momentum.isCoasting)
        momentum.touch(at: 30 * Self.frame + 0.001)
        #expect(!momentum.isCoasting)
        #expect(momentum.velocity == 0)
        let afterTouch = momentum.advance(to: 1)
        #expect(afterTouch == 0)
    }

    @Test func aFingerThatStoppedBeforeLiftingOrMovedSlowlyDoesNotCoast() {
        var momentum = DialMomentum()
        momentum.touch(at: 0)
        for index in 1...10 {
            momentum.follow(byRadians: 0.2, at: Double(index) * Self.frame)
        }
        // Held still 80 ms, then lifted.
        let heldStill = momentum.release(at: 10 * Self.frame + 0.08)
        #expect(!heldStill)
        #expect(!momentum.isCoasting)

        momentum.touch(at: 1)
        for index in 1...10 {
            momentum.follow(byRadians: 0.003, at: 1 + Double(index) * Self.frame)
        }
        // 0.36 rad/s is under the minimum speed.
        let slow = momentum.release(at: 1 + 10 * Self.frame)
        #expect(!slow)
    }

    @Test func releaseSpeedLooksOnlyAtTheLastTenthOfASecond() {
        var momentum = DialMomentum()
        momentum.touch(at: 0)
        // Fast for a while, then slow for the last 150 ms.
        var time = 0.0
        for _ in 0..<30 {
            time += Self.frame
            momentum.follow(byRadians: 0.3, at: time)
        }
        for _ in 0..<18 {
            time += Self.frame
            momentum.follow(byRadians: 0.01, at: time)
        }
        #expect(abs(momentum.releaseSpeed(at: time) - 1.2) < 1e-9)
    }
}
