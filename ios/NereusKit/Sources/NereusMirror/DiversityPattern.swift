// NereusSDR for iOS: the Diversity sensitivity pattern the Core sends on each slice, read as sent
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The Diversity dialog's sensitivity pattern as the Core sends it (link
/// document section 7.1, "The diversity pattern", `diversityPatternVersion`
/// 1): read-only compact JSON on each slice's `diversityPattern`, which a
/// device gets only after declaring `diversityPattern` 1 in its hello.
///
/// `points` are fractions of the peak, 0 to 1 (not dB), at bearing
/// `index * stepDeg` degrees from north, clockwise. The phone draws them as
/// sent and never works the pattern out itself (D4). The antenna spacing
/// and cross-fire are facts the Core states; the desktop has no setting for
/// either, so the phone offers none.
///
/// Key order does not matter and unknown keys are ignored. A value that
/// does not read as the pattern's shape reads as nil.
public struct DiversityPattern: Equatable, Sendable {
    /// The feature a device declares in its hello, and the capability the Core answers with.
    public static let featureName = "diversityPattern"
    public static let capabilityName = "diversityPatternVersion"
    /// The agreed minor the pattern arrived in.
    public static let minor: UInt16 = 11
    /// The slice property that carries it.
    public static let propertyName = "diversityPattern"

    /// Each bearing's share of the peak, in the order sent.
    public var points: [Double]
    /// Degrees between neighbouring points.
    public var stepDeg: Double
    /// The antenna spacing the pattern assumes, when the Core says.
    public var spacingMeters: Double?
    /// Whether the pattern assumes cross-fire, when the Core says.
    public var crossFire: Bool?

    public init(points: [Double], stepDeg: Double, spacingMeters: Double? = nil, crossFire: Bool? = nil) {
        self.points = points
        self.stepDeg = stepDeg
        self.spacingMeters = spacingMeters
        self.crossFire = crossFire
    }

    /// Reads the Core's value; nil when it sent none (an empty string) or
    /// when the value is not the pattern's shape.
    public init?(json: String) {
        guard !json.isEmpty, case .object(let object)? = try? LinkJSON.parse(json),
              case .array(let items)? = object["points"], !items.isEmpty,
              let stepDeg = Self.number(object["stepDeg"]), stepDeg > 0 else {
            return nil
        }
        var points: [Double] = []
        for item in items {
            guard let value = Self.number(item) else {
                return nil
            }
            points.append(value)
        }
        var crossFire: Bool?
        if case .bool(let value)? = object["crossFire"] {
            crossFire = value
        }
        self.init(points: points, stepDeg: stepDeg, spacingMeters: Self.number(object["spacingMeters"]),
                  crossFire: crossFire)
    }

    /// The bearing of point `index`, in degrees from north, clockwise.
    public func bearing(of index: Int) -> Double {
        Double(index) * stepDeg
    }

    private static func number(_ value: LinkJSON?) -> Double? {
        if case .number(let number)? = value, number.isFinite {
            return number
        }
        return nil
    }
}
