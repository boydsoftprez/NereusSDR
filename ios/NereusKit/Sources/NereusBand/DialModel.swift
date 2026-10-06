// NereusSDR for iOS: the tuning dial's arithmetic: detents, steps, direction and whole kilohertz
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// What one detent of the dial did.
public enum DialEvent: Equatable, Sendable {
    /// The dial clicked one detent and the slice moves to this frequency.
    case detent(Double)
    /// That detent reached or passed a whole kilohertz, landing here: the
    /// firmer bump.
    case wholeKilohertz(Double)
}

/// The tuning dial's arithmetic (D12, R-IOS-12, spec section 5.1 item 8):
/// 36 detents in a full turn, each moving the frequency one step, the
/// slice's own step (D74). Clockwise tunes up unless the direction is
/// reversed. Every detent yields ``DialEvent/detent(_:)``; one that reaches
/// or passes a whole kilohertz is followed by
/// ``DialEvent/wholeKilohertz(_:)``. A turn smaller than a detent is kept
/// and adds to the next one. A new step applies from the next detent. The
/// thumbwheel rolls the same dial, one detent every 12 points, rolling
/// left bringing higher frequencies to its mark, like a ruler.
public struct DialModel: Equatable, Sendable {
    /// Detents in one full turn.
    public static let detentsPerTurn = 36
    /// The turn of one detent, in radians.
    public static let radiansPerDetent = 2 * Double.pi / Double(detentsPerTurn)
    /// The thumbwheel's roll for one detent, in points.
    public static let pointsPerNotch = 12.0

    /// The frequency the dial has reached.
    public var hz: Double
    /// The step one detent moves; none keeps the dial from tuning.
    public var stepHz: Double?
    /// Turns the other way: clockwise tunes down.
    public var reversed: Bool

    /// The turn not yet made into detents, in detents (less than one either way).
    private var pending = 0.0

    /// A detent's worth less a sliver, so a full turn in floating point
    /// still makes all 36.
    private static let wholeDetent = 1 - 1e-9

    public init(hz: Double, stepHz: Double?, reversed: Bool = false) {
        self.hz = hz
        self.stepHz = stepHz
        self.reversed = reversed
    }

    /// Starts again from `hz`, the slice's frequency, dropping any part-turn.
    public mutating func start(atHz hz: Double) {
        self.hz = hz
        pending = 0
    }

    /// Turns the dial by `radians`, clockwise positive (as the screen's
    /// angles run, y down): the detents it clicks through, in order.
    public mutating func rotate(byRadians radians: Double) -> [DialEvent] {
        guard radians.isFinite else {
            return []
        }
        pending += (reversed ? -radians : radians) / Self.radiansPerDetent
        var events: [DialEvent] = []
        while pending >= Self.wholeDetent {
            pending -= 1
            guard click(up: true, into: &events) else {
                pending = 0
                break
            }
        }
        while pending <= -Self.wholeDetent {
            pending += 1
            guard click(up: false, into: &events) else {
                pending = 0
                break
            }
        }
        return events
    }

    /// Rolls the thumbwheel by `points`, rightward positive: rolling left
    /// tunes up, one detent every 12 points.
    public mutating func roll(byPoints points: Double) -> [DialEvent] {
        rotate(byRadians: -points / Self.pointsPerNotch * Self.radiansPerDetent)
    }

    /// One detent: the frequency moves one step, unless there is no step or
    /// it would fall below zero hertz.
    private mutating func click(up: Bool, into events: inout [DialEvent]) -> Bool {
        guard let stepHz, stepHz > 0 else {
            return false
        }
        let next = up ? hz + stepHz : hz - stepHz
        guard next >= 0 else {
            return false
        }
        let passed = Self.reachesWholeKilohertz(from: hz, to: next)
        hz = next
        events.append(.detent(next))
        if passed {
            events.append(.wholeKilohertz(next))
        }
        return true
    }

    /// Whether moving from `from` to `to` reaches or passes a whole
    /// kilohertz: one lies in (from, to] going up, or in [to, from) going down.
    static func reachesWholeKilohertz(from: Double, to: Double) -> Bool {
        let a = Int64(from.rounded())
        let b = Int64(to.rounded())
        if b > a {
            return floorDiv(b, 1_000) > floorDiv(a, 1_000)
        }
        if b < a {
            return ceilDiv(a, 1_000) > ceilDiv(b, 1_000)
        }
        return false
    }

    private static func floorDiv(_ value: Int64, _ divisor: Int64) -> Int64 {
        let quotient = value / divisor
        return (value % divisor != 0 && value < 0) ? quotient - 1 : quotient
    }

    private static func ceilDiv(_ value: Int64, _ divisor: Int64) -> Int64 {
        let quotient = value / divisor
        return (value % divisor != 0 && value > 0) ? quotient + 1 : quotient
    }
}
