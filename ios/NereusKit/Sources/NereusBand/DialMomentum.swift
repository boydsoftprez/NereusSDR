// NereusSDR for iOS: the tuning dial's feel: following the finger, then coasting to a stop
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// How the dial feels (R-IOS-12, JJ on 2026-09-26: a spin advances quickly
/// then slows, a slow roll is smooth). Every number that shapes the feel
/// lives here, in dial radians (the thumbwheel turns its points into the
/// same radians, one detent every 12 points).
public struct DialFeel: Equatable, Sendable {
    /// How fast a coast dies away: the speed falls by e^-friction each
    /// second, as a flywheel in light grease slows.
    public var friction: Double
    /// Below this speed, in radians a second, the dial stops; a release
    /// slower than it does not coast at all.
    public var minimumSpeed: Double
    /// The fastest the dial coasts, in radians a second, so one wild flick
    /// cannot run away with the band.
    public var maximumSpeed: Double
    /// How far back the release speed is measured, in seconds.
    public var sampleWindow: Double
    /// A finger that held still this long before lifting releases no speed.
    public var stillBeforeRelease: Double
    /// The least time between two light ticks, and between two bumps, in
    /// seconds, so a fast spin stays a train of ticks rather than a buzz.
    public var minimumTickInterval: Double

    public init(friction: Double, minimumSpeed: Double, maximumSpeed: Double, sampleWindow: Double,
                stillBeforeRelease: Double, minimumTickInterval: Double) {
        self.friction = friction
        self.minimumSpeed = minimumSpeed
        self.maximumSpeed = maximumSpeed
        self.sampleWindow = sampleWindow
        self.stillBeforeRelease = stillBeforeRelease
        self.minimumTickInterval = minimumTickInterval
    }

    /// The dial as it ships. A strong flick (40 rad/s, the cap: about 6.4
    /// turns a second) coasts ln(40 / 0.5) / 2.5 = 1.75 s and runs on
    /// (40 - 0.5) / 2.5 = 15.8 rad, 90 detents; a firm one at 20 rad/s
    /// coasts 1.48 s over 44 detents; a gentle 2 rad/s release coasts
    /// 0.55 s over 3 detents. Ticks come at most one every 1/60 s: two
    /// ProMotion frames, about the shortest tap the Taptic Engine keeps
    /// distinct.
    public static let standard = DialFeel(friction: 2.5, minimumSpeed: 0.5, maximumSpeed: 40,
                                          sampleWindow: 0.1, stillBeforeRelease: 0.05,
                                          minimumTickInterval: 1.0 / 60)

    /// How long a coast from `speed` lasts, in seconds (none at or below the
    /// minimum speed).
    public func coastDuration(from speed: Double) -> Double {
        let start = min(abs(speed), maximumSpeed)
        guard start > minimumSpeed else {
            return 0
        }
        return log(start / minimumSpeed) / friction
    }

    /// How far a coast from `speed` turns the dial, in radians, signed.
    public func coastDistance(from speed: Double) -> Double {
        let start = min(abs(speed), maximumSpeed)
        guard start > minimumSpeed else {
            return 0
        }
        return (start - minimumSpeed) / friction * (speed < 0 ? -1 : 1)
    }
}

/// The dial's motion under the finger and after it (R-IOS-12): while the
/// finger is down the dial turns exactly as far as the finger does; when
/// it lifts while moving, the dial keeps its speed and slows under a
/// steady friction to a stop. A touch during a coast stops it at once.
/// Times are seconds on any one clock; the caller gives them, so tests
/// run on their own clock.
public struct DialMomentum: Sendable {
    public let feel: DialFeel

    /// The dial's speed while it coasts, radians a second, clockwise
    /// positive; zero otherwise.
    public private(set) var velocity = 0.0
    /// The dial is turning on its own.
    public var isCoasting: Bool { coast != nil }

    private struct Sample: Sendable {
        var time: Double
        var angle: Double
    }

    private struct Coast: Sendable {
        var start: Double
        var speed: Double
        var reached: Double
    }

    /// The finger's recent turn: when, and its total turn by then.
    private var samples: [Sample] = []
    /// The total turn under the finger since it came down.
    private var fingerAngle = 0.0
    /// The coast under way: its start, starting speed and how far it had
    /// turned by the last frame.
    private var coast: Coast?

    public init(feel: DialFeel = .standard) {
        self.feel = feel
    }

    /// A finger comes down at `time`: any coast stops where it is.
    public mutating func touch(at time: Double) {
        coast = nil
        velocity = 0
        fingerAngle = 0
        samples = [Sample(time: time, angle: 0)]
    }

    /// The finger turned the dial by `radians` at `time`.
    public mutating func follow(byRadians radians: Double, at time: Double) {
        guard radians.isFinite else {
            return
        }
        fingerAngle += radians
        samples.append(Sample(time: time, angle: fingerAngle))
        let horizon = time - feel.sampleWindow * 2
        if let keep = samples.firstIndex(where: { $0.time >= horizon }), keep > 0 {
            samples.removeFirst(keep)
        }
    }

    /// The finger's speed as it lifts at `time`: its turn over the last
    /// sample window, or none if it had come to rest first.
    public func releaseSpeed(at time: Double) -> Double {
        guard let last = samples.last, time - last.time <= feel.stillBeforeRelease,
              let first = samples.first(where: { $0.time >= time - feel.sampleWindow }),
              last.time - first.time > 0 else {
            return 0
        }
        let speed = (last.angle - first.angle) / (last.time - first.time)
        return max(-feel.maximumSpeed, min(feel.maximumSpeed, speed))
    }

    /// The finger lifts at `time`: the dial coasts on from the finger's
    /// speed, if that is above the minimum. Whether it coasts.
    @discardableResult
    public mutating func release(at time: Double) -> Bool {
        let speed = releaseSpeed(at: time)
        samples.removeAll()
        return launch(speed: speed, at: time)
    }

    /// Sets the dial coasting at `speed` from `time`, as a release at that
    /// speed would. Whether it coasts.
    @discardableResult
    public mutating func launch(speed: Double, at time: Double) -> Bool {
        let clamped = max(-feel.maximumSpeed, min(feel.maximumSpeed, speed))
        guard clamped.isFinite, abs(clamped) > feel.minimumSpeed else {
            coast = nil
            velocity = 0
            return false
        }
        coast = Coast(start: time, speed: clamped, reached: 0)
        velocity = clamped
        return true
    }

    /// The coast's turn since the last frame, for a frame at `time`. The
    /// coast ends when its speed has fallen to the minimum.
    public mutating func advance(to time: Double) -> Double {
        guard let current = coast else {
            return 0
        }
        let duration = feel.coastDuration(from: current.speed)
        let elapsed = min(max(0, time - current.start), duration)
        let decay = exp(-feel.friction * elapsed)
        let reached = current.speed / feel.friction * (1 - decay)
        let delta = reached - current.reached
        if elapsed >= duration {
            coast = nil
            velocity = 0
        } else {
            coast = Coast(start: current.start, speed: current.speed, reached: reached)
            velocity = current.speed * decay
        }
        return delta
    }

    /// Stops a coast where it is.
    public mutating func stop() {
        coast = nil
        velocity = 0
    }
}
