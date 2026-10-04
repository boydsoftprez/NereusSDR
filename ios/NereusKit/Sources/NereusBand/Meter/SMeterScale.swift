// NereusSDR for iOS: the S-meter's scales: where a reading sits, the ticks, labels and red zone of each face
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels

/// One scale on the S-meter (D86): its ticks and where its red starts.
/// The receive scale is the Core's S-meter from the catalogue, laid out as
/// the desktop's meter lays it (S0 to S9 over the left 60% of the arc, S9
/// to the top over the rest); the transmit scales are the desktop meter's
/// (D83: its values, the phone's own code). Power takes the catalogue's
/// power scale, which knows the radio.
public struct SMeterScale: Equatable, Sendable {
    /// One tick: where it sits (0 to 1), whether it is a major tick, its
    /// label if it has one, and whether it is drawn red.
    public struct Tick: Equatable, Sendable {
        public let fraction: Double
        public let major: Bool
        public let label: String?
        public let red: Bool

        public init(fraction: Double, major: Bool, label: String?, red: Bool) {
            self.fraction = fraction
            self.major = major
            self.label = label
            self.red = red
        }
    }

    /// How a face lays out its scale: the classic face labels a few ticks
    /// and draws its arc red from the red point; a vintage card ticks
    /// finely and lays a red band from the red point.
    public enum Style: Equatable, Sendable {
        case classic
        case vintage
    }

    public let ticks: [Tick]
    /// Where the red starts along the scale; nil where the scale has none.
    public let redFrom: Double?

    public init(ticks: [Tick], redFrom: Double?) {
        self.ticks = ticks
        self.redFrom = redFrom
    }

    // MARK: The desktop meter's values

    /// The share of the arc S0 to S9 takes.
    public static let s9Share = 0.6
    /// The SWR scale, 1 to 3, red from 2.5.
    public static let swrRange = 1.0 ... 3.0
    public static let swrRedFrom = 2.5
    /// The mic level scale, -40 to +5 dB, red from 0.
    public static let levelRange = -40.0 ... 5.0
    public static let levelRedFrom = 0.0
    /// The compression scale, -25 to 0 dB, no red.
    public static let compressionRange = -25.0 ... 0.0
    /// The vintage band starts a hair past S9, so the 9 stays in ink.
    static let vintageRedNudge = 0.002

    // MARK: Where a reading sits

    /// Where `dbm` sits on the receive scale, 0 at the Core's floor and 1 at its top.
    public static func receiveFraction(dbm: Double, meter: StationCatalog.Meters.SMeter) -> Double {
        guard meter.s9Dbm > meter.minDbm, meter.maxDbm > meter.s9Dbm else {
            return 0
        }
        let clamped = min(max(dbm, meter.minDbm), meter.maxDbm)
        if clamped <= meter.s9Dbm {
            return s9Share * (clamped - meter.minDbm) / (meter.s9Dbm - meter.minDbm)
        }
        return s9Share + (1 - s9Share) * (clamped - meter.s9Dbm) / (meter.maxDbm - meter.s9Dbm)
    }

    /// Where a transmit reading sits on `mode`'s scale; power on the
    /// catalogue's power scale (0 without one).
    public static func transmitFraction(_ value: Double, mode: SMeterTxMode,
                                        power: StationCatalog.Meters.RfPower?) -> Double {
        switch mode {
        case .power:
            guard let power, power.maxW > power.minW else {
                return 0
            }
            return clampUnit((value - power.minW) / (power.maxW - power.minW))
        case .swr:
            return clampUnit(unit(value, in: swrRange))
        case .level:
            return clampUnit(unit(value, in: levelRange))
        case .compression:
            return clampUnit(unit(value, in: compressionRange))
        }
    }

    private static func unit(_ value: Double, in range: ClosedRange<Double>) -> Double {
        (value - range.lowerBound) / (range.upperBound - range.lowerBound)
    }

    private static func clampUnit(_ value: Double) -> Double {
        min(max(value, 0), 1)
    }

    // MARK: The scales

    /// The receive scale from the Core's S-meter. Classic: the odd S-units
    /// numbered and +20 and +40 in red. Vintage: a tick at every S-unit and
    /// every step over S9, the odd units and every 20 dB over S9 numbered.
    public static func receive(_ meter: StationCatalog.Meters.SMeter, style: Style) -> SMeterScale {
        let s9 = receiveFraction(dbm: meter.s9Dbm, meter: meter)
        var ticks: [Tick] = []
        for unit in meter.sUnits {
            let odd = Int(unit.label.dropFirst()).map { $0 % 2 == 1 } ?? false
            let label = odd ? String(unit.label.dropFirst()) : nil
            let at = receiveFraction(dbm: unit.dbm, meter: meter)
            switch style {
            case .classic:
                if odd {
                    ticks.append(Tick(fraction: at, major: true, label: label, red: unit.dbm > meter.redFromDbm))
                }
            case .vintage:
                ticks.append(Tick(fraction: at, major: odd, label: label, red: at >= s9 + vintageRedNudge))
            }
        }
        for over in meter.overS9 where over.dbm > meter.s9Dbm {
            let above = Int((over.dbm - meter.s9Dbm).rounded())
            let twenty = above % 20 == 0
            let at = receiveFraction(dbm: over.dbm, meter: meter)
            switch style {
            case .classic:
                if twenty && over.dbm < meter.maxDbm {
                    ticks.append(Tick(fraction: at, major: true, label: over.label, red: true))
                }
            case .vintage:
                ticks.append(Tick(fraction: at, major: twenty, label: twenty ? over.label : nil, red: true))
            }
        }
        let redFrom = style == .classic ? receiveFraction(dbm: meter.redFromDbm, meter: meter) : s9 + vintageRedNudge
        return SMeterScale(ticks: ticks, redFrom: redFrom)
    }

    /// `mode`'s transmit scale, power on the catalogue's power scale.
    public static func transmit(_ mode: SMeterTxMode, power: StationCatalog.Meters.RfPower?,
                                style: Style) -> SMeterScale {
        switch mode {
        case .power:
            return powerScale(power, style: style)
        case .swr:
            return steppedScale(range: swrRange, redFrom: swrRedFrom, style: style,
                                classicValues: [1, 1.5, 2, 2.5, 3], vintageStep: 0.1, vintageMajorEvery: 5) {
                $0 == $0.rounded() ? String(Int($0)) : String(format: "%.1f", $0)
            }
        case .level:
            return steppedScale(range: levelRange, redFrom: levelRedFrom, style: style,
                                classicValues: [-40, -30, -20, -10, 0], vintageStep: 5, vintageMajorEvery: 2) {
                String(Int($0))
            }
        case .compression:
            return steppedScale(range: compressionRange, redFrom: nil, style: style,
                                classicValues: [-25, -20, -15, -10, -5, 0], vintageStep: 1, vintageMajorEvery: 5) {
                String(Int($0))
            }
        }
    }

    /// A fixed scale: the classic face ticks and labels `classicValues`;
    /// a vintage card ticks every `vintageStep` and labels every
    /// `vintageMajorEvery`th tick.
    private static func steppedScale(range: ClosedRange<Double>, redFrom: Double?, style: Style,
                                     classicValues: [Double], vintageStep: Double, vintageMajorEvery: Int,
                                     label: (Double) -> String) -> SMeterScale {
        let red = redFrom.map { unit($0, in: range) }
        var ticks: [Tick] = []
        switch style {
        case .classic:
            for value in classicValues {
                ticks.append(Tick(fraction: unit(value, in: range), major: true, label: label(value),
                                  red: redFrom.map { value >= $0 } ?? false))
            }
        case .vintage:
            let count = Int(((range.upperBound - range.lowerBound) / vintageStep).rounded())
            for step in 0 ... count {
                let value = range.lowerBound + Double(step) * vintageStep
                let major = step % vintageMajorEvery == 0
                let at = unit(value, in: range)
                ticks.append(Tick(fraction: at, major: major, label: major ? label(value) : nil,
                                  red: red.map { at >= $0 } ?? false))
            }
        }
        return SMeterScale(ticks: ticks, redFrom: red)
    }

    /// The power scale: ticks every 10 W up to 600 W, every 50 W from
    /// 600 W and every 100 W from 2 kW; the classic face labels every
    /// 40 W (100 W, 500 W), the top and the red point, a vintage card
    /// every 20 W (100 W, 500 W).
    private static func powerScale(_ power: StationCatalog.Meters.RfPower?, style: Style) -> SMeterScale {
        guard let power, power.maxW > power.minW else {
            return SMeterScale(ticks: [], redFrom: nil)
        }
        let maxW = Int(power.maxW)
        let redW = Int(power.redFromW)
        let tickStep: Int
        let labelStep: Int
        if maxW >= 2000 {
            tickStep = 100
            labelStep = 500
        } else if maxW >= 600 {
            tickStep = 50
            labelStep = 100
        } else {
            tickStep = 10
            labelStep = style == .classic ? 40 : 20
        }
        let redAt = transmitFraction(power.redFromW, mode: .power, power: power)
        var ticks: [Tick] = []
        for watts in stride(from: Int(power.minW), through: maxW, by: tickStep) {
            let at = transmitFraction(Double(watts), mode: .power, power: power)
            let labelled: Bool
            switch style {
            case .classic:
                labelled = watts % labelStep == 0 || watts == maxW || watts == redW
            case .vintage:
                labelled = watts % labelStep == 0
            }
            ticks.append(Tick(fraction: at, major: labelled, label: labelled ? wattsLabel(watts) : nil,
                              red: watts >= redW))
        }
        return SMeterScale(ticks: ticks, redFrom: redAt)
    }

    /// `1500` as `1.5k`, `2000` as `2k`, below a kilowatt the watts.
    static func wattsLabel(_ watts: Int) -> String {
        guard watts >= 1000 else {
            return String(watts)
        }
        return watts % 1000 == 0 ? "\(watts / 1000)k" : String(format: "%.1fk", Double(watts) / 1000)
    }
}
