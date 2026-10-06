// NereusSDR for iOS: how the phone prints a received signal level: the Multimeter page's units and decimal point
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels

/// The Multimeter page's Display units (description V12,
/// `MultimeterUnitMode`), in the Core's option order: 0 S, 1 dBm, 2 uV.
/// Kept as the desktop keeps them, by these words.
public enum SMeterUnit: String, Codable, CaseIterable, Sendable {
    case sUnits = "S"
    case dBm
    case microvolts = "uV"
}

/// How the phone prints a received signal level (JJ, 2026-09-29): the
/// Multimeter page's Display units and Show decimal point, which reach
/// every place the phone prints the level as text: the S-meter face, the
/// flag and the Live Activity. The needle, the bars and the scales' marks
/// do not change with them.
///
/// - S: the S-unit reading on the Core's S-meter: to a tenth of a unit
///   with the decimal point (`S5.3`, `S9+13.4`), the nearest unit
///   without it (`S5`, `S9+13`);
/// - dBm: one decimal place, or a whole number;
/// - uV: the level across 50 ohms in microvolts, the relation the
///   desktop's meter readouts use: two places under one microvolt and one
///   above with the decimal point, a whole number without it.
public struct SMeterReadout: Equatable, Sendable {
    public var unit: SMeterUnit
    public var showDecimal: Bool

    public init(unit: SMeterUnit = .dBm, showDecimal: Bool = true) {
        self.unit = unit
        self.showDecimal = showDecimal
    }

    /// The desktop's defaults, as the Core describes them: dBm, with the decimal point.
    public static let desktopDefaults = SMeterReadout()

    /// The resistance the microvolt reading is taken across, in ohms.
    public static let loadOhms = 50.0

    /// `dbm` as microvolts across ``loadOhms``: the voltage of that power
    /// in that load.
    public static func microvolts(dbm: Double) -> Double {
        let watts = pow(10, dbm / 10) / 1000
        return (watts * loadOhms).squareRoot() * 1_000_000
    }

    /// The S-unit reading of `dbm` on the Core's S-meter: `S0` at or below
    /// its floor, S-units up to S9, then the decibels over S9.
    public func sUnits(dbm: Double, meter: StationCatalog.Meters.SMeter) -> String {
        if dbm <= meter.minDbm {
            return showDecimal ? "S0.0" : "S0"
        }
        if dbm <= meter.s9Dbm {
            let units = meter.dbPerSUnit > 0 ? (dbm - meter.minDbm) / meter.dbPerSUnit : 0
            let held = min(max(units, 0), 9)
            return showDecimal ? String(format: "S%.1f", held) : "S\(Int(held.rounded()))"
        }
        let over = dbm - meter.s9Dbm
        return showDecimal ? String(format: "S9+%.1f", over) : "S9+\(Int(over.rounded()))"
    }

    /// `dbm` printed in the chosen unit; nil in S before the Core's
    /// S-meter has come.
    public func text(dbm: Double, meter: StationCatalog.Meters.SMeter?) -> String? {
        switch unit {
        case .sUnits:
            return meter.map { sUnits(dbm: dbm, meter: $0) }
        case .dBm:
            return "\(number(dbm, places: 1)) dBm"
        case .microvolts:
            let volts = Self.microvolts(dbm: dbm)
            return "\(number(volts, places: volts < 1 ? 2 : 1)) uV"
        }
    }

    /// What VoiceOver says for `dbm` in the chosen unit.
    public func spoken(dbm: Double, meter: StationCatalog.Meters.SMeter?) -> String? {
        switch unit {
        case .sUnits, .dBm:
            return text(dbm: dbm, meter: meter)
        case .microvolts:
            let volts = Self.microvolts(dbm: dbm)
            return "\(number(volts, places: volts < 1 ? 2 : 1)) microvolts"
        }
    }

    /// The printed reading with no level: dashes, with the unit.
    public var noReadingText: String {
        switch unit {
        case .sUnits:
            return "--"
        case .dBm:
            return "-- dBm"
        case .microvolts:
            return "-- uV"
        }
    }

    /// `value` to `places` decimal places with the decimal point, a whole
    /// number without it.
    private func number(_ value: Double, places: Int) -> String {
        showDecimal ? String(format: "%.\(places)f", value) : String(Int(value.rounded()))
    }
}
