// NereusSDR for iOS: what the S-meter's needle reads while receiving, the desktop meter menu's RX Mode
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The S-meter's RX Mode (D86): which of the Core's receive readings the
/// needle follows. Signal and Signal Peak both read the receiver's peak
/// meter; Signal Peak also holds the highest value and lets it fall back
/// slowly, as the desktop's meter does. Sig Avg reads the average meter,
/// Max Bin the strongest bin in the receiver's passband.
public enum SMeterRxMode: String, Equatable, Sendable, Codable, CaseIterable {
    case signal
    case signalAverage
    case signalPeak
    case maxBin

    /// The menu's words, the desktop's.
    public var label: String {
        switch self {
        case .signal:
            return "Signal"
        case .signalAverage:
            return "Sig Avg"
        case .signalPeak:
            return "Signal Peak"
        case .maxBin:
            return "Max Bin"
        }
    }

    /// The classic face's caption over the needle, the desktop's.
    public var caption: String {
        switch self {
        case .signal:
            return "S-Meter"
        case .signalAverage:
            return "Sig Avg"
        case .signalPeak:
            return "S-Meter Peak"
        case .maxBin:
            return "Max Bin"
        }
    }

    /// The legend a vintage face prints under its scale.
    public var legend: String {
        switch self {
        case .signal:
            return "S UNITS"
        case .signalAverage:
            return "S UNITS  AVERAGE"
        case .signalPeak:
            return "S UNITS  PEAK"
        case .maxBin:
            return "S UNITS  MAX BIN"
        }
    }
}
