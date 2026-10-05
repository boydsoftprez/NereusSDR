// NereusSDR for iOS: what the S-meter's needle reads while on the air, the desktop meter menu's TX Mode
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The S-meter's TX Mode (D86): which transmit reading the needle swings
/// to while the radio is on the air, each on its own scale.
public enum SMeterTxMode: String, Equatable, Sendable, Codable, CaseIterable {
    case power
    case swr
    case level
    case compression

    /// The menu's words, and the classic face's caption while on the air.
    public var label: String {
        switch self {
        case .power:
            return "Power"
        case .swr:
            return "SWR"
        case .level:
            return "Level"
        case .compression:
            return "Compression"
        }
    }

    /// A vintage face's title over its scale.
    public var title: String {
        switch self {
        case .power:
            return "FORWARD POWER"
        case .swr:
            return "STANDING WAVE RATIO"
        case .level:
            return "MIC LEVEL"
        case .compression:
            return "COMPRESSION"
        }
    }

    /// A vintage face's legend under its scale.
    public var legend: String {
        switch self {
        case .power:
            return "WATTS"
        case .swr:
            return "S.W.R."
        case .level, .compression:
            return "DECIBELS"
        }
    }
}
