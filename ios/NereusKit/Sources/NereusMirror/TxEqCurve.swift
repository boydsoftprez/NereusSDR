// NereusSDR for iOS: the TX EQ curve the Core sends on transmit.txEqCurve, read as sent and turned into its line
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The TX EQ curve as the Core sends it (link document section 7.1, "The
/// TX EQ curve (`txEqCurve`)", `txEqCurveVersion` 1): read-only compact
/// JSON on `transmit`'s `txEqCurve`, which a device gets only after
/// declaring `txEqCurve` 1 or later in its hello. The Core has already put the points
/// in the order its desktop dialog draws them; the phone keeps that order.
///
/// Key order does not matter and unknown keys are ignored. A state the
/// phone does not know, or a value that does not read as the curve's shape,
/// reads as `unavailable`, which has no line.
public struct TxEqCurve: Equatable, Sendable {
    public enum State: String, Equatable, Sendable {
        /// A curve the operator saved.
        case saved
        /// No saved curve: the Core's flat default.
        case `default`
        /// The saved value cannot be read; the Core applies the default in its place.
        case unavailable
    }

    public struct Point: Equatable, Sendable {
        public var frequencyHz: Double
        public var gainDb: Double
        /// The bell's width; unused when the curve is not parametric.
        public var q: Double

        public init(frequencyHz: Double, gainDb: Double, q: Double) {
            self.frequencyHz = frequencyHz
            self.gainDb = gainDb
            self.q = q
        }
    }

    /// The feature a device declares in its hello, and the capability the Core answers with.
    public static let featureName = "txEqCurve"
    public static let capabilityName = "txEqCurveVersion"
    /// The agreed minor the curve arrived in.
    public static let minor: UInt16 = 11
    /// The `transmit` property that carries it.
    public static let propertyName = "txEqCurve"
    /// The scale the line is drawn on.
    public static let scaleDb: ClosedRange<Double> = -24...24

    public var state: State
    /// True: a bell at each point, width `q`. False: straight lines between the points.
    public var parametric: Bool
    /// Added to the whole curve.
    public var preampDb: Double
    public var minHz: Double
    public var maxHz: Double
    /// As sent, never reordered.
    public var points: [Point]

    public init(state: State, parametric: Bool = false, preampDb: Double = 0, minHz: Double = 0, maxHz: Double = 0,
                points: [Point] = []) {
        self.state = state
        self.parametric = parametric
        self.preampDb = preampDb
        self.minHz = minHz
        self.maxHz = maxHz
        self.points = points
    }

    public static let unavailable = TxEqCurve(state: .unavailable)

    /// Reads the Core's value; nil when it sent none (an empty string).
    public init?(json: String) {
        guard !json.isEmpty else {
            return nil
        }
        self = Self.read(json) ?? .unavailable
    }

    private static func read(_ json: String) -> TxEqCurve? {
        guard case .object(let object)? = try? LinkJSON.parse(json),
              case .string(let stateText)? = object["state"],
              let state = State(rawValue: stateText), state != .unavailable,
              case .bool(let parametric)? = object["parametric"],
              let preampDb = number(object["preampDb"]), let minHz = number(object["minHz"]),
              let maxHz = number(object["maxHz"]), minHz < maxHz,
              case .array(let items)? = object["points"], items.count >= 2 else {
            return nil
        }
        var points: [Point] = []
        for item in items {
            guard case .object(let fields) = item, let frequencyHz = number(fields["frequencyHz"]),
                  let gainDb = number(fields["gainDb"]), let q = number(fields["q"]) else {
                return nil
            }
            points.append(Point(frequencyHz: frequencyHz, gainDb: gainDb, q: q))
        }
        return TxEqCurve(state: state, parametric: parametric, preampDb: preampDb, minHz: minHz, maxHz: maxHz,
                         points: points)
    }

    private static func number(_ value: LinkJSON?) -> Double? {
        if case .number(let number)? = value, number.isFinite {
            return number
        }
        return nil
    }

    /// Whether there is a line to draw.
    public var drawable: Bool {
        state != .unavailable && points.count >= 2 && minHz < maxHz
    }

    /// The curve's level at `hz`: its response there plus `preampDb`; nil
    /// when there is no line.
    public func levelDb(atHz hz: Double) -> Double? {
        guard drawable else {
            return nil
        }
        return (parametric ? bells(hz) : lines(hz)) + preampDb
    }

    /// `samples` points evenly spaced from `minHz` to `maxHz`, each at the
    /// curve's level; empty when there is no line.
    public func line(samples: Int) -> [(hz: Double, db: Double)] {
        guard drawable, samples >= 2 else {
            return []
        }
        let span = maxHz - minHz
        return (0..<samples).compactMap { index in
            let hz = index == samples - 1 ? maxHz : minHz + span * Double(index) / Double(samples - 1)
            return levelDb(atHz: hz).map { (hz, $0) }
        }
    }

    /// A level held to the drawing's scale.
    public static func drawnDb(_ db: Double) -> Double {
        min(max(db, scaleDb.lowerBound), scaleDb.upperBound)
    }

    /// Straight lines: the first point's gain at and below it, the last's at
    /// and above it, a line between each two neighbours.
    private func lines(_ hz: Double) -> Double {
        guard let first = points.first, let last = points.last else {
            return 0
        }
        if hz <= first.frequencyHz {
            return first.gainDb
        }
        if hz >= last.frequencyHz {
            return last.gainDb
        }
        for index in 0..<(points.count - 1) {
            let low = points[index]
            let high = points[index + 1]
            guard hz >= low.frequencyHz, hz <= high.frequencyHz, high.frequencyHz > low.frequencyHz else {
                continue
            }
            let along = (hz - low.frequencyHz) / (high.frequencyHz - low.frequencyHz)
            return low.gainDb + (high.gainDb - low.gainDb) * along
        }
        return last.gainDb
    }

    /// The sum of the bells: each point's gain times a Gaussian whose full
    /// width at half height is the range over three times its `q`, never
    /// less than the range over 6000.
    private func bells(_ hz: Double) -> Double {
        let span = maxHz - minHz
        let narrowest = span / 6000
        var sum = 0.0
        for point in points {
            let width = point.q > 0 ? max(span / (point.q * 3), narrowest) : narrowest
            let sigma = width / 2.3548200450309493
            let offset = (hz - point.frequencyHz) / sigma
            sum += point.gainDb * exp(-0.5 * offset * offset)
        }
        return sum
    }
}
