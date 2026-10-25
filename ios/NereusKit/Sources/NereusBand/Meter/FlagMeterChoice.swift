// NereusSDR for iOS: which reading a slice flag's level bar shows, chosen from the flag's more menu
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The flag's S-meter source (JJ, 2026-10-23): Peak, Average or Max Bin,
/// chosen from the flag's more menu. It is the S-meter's own RX Mode
/// (``SMeterRxMode``), kept on this device with the rest of the meter's
/// choices, as a desktop window keeps its S-meter's RX Mode and feeds every
/// flag from it (MeterPoller::pollRemoteRxMeters): S-Meter and S-Meter Peak
/// read the receiver's peak meter, Sig Avg its average meter, Max Bin the
/// strongest point of the slice's passband on the pan this device draws.
public enum FlagMeterChoice: String, CaseIterable, Sendable {
    case peak
    case average
    case maxBin

    /// The menu's words.
    public var label: String {
        switch self {
        case .peak:
            return "Peak"
        case .average:
            return "Average"
        case .maxBin:
            return "Max Bin"
        }
    }

    /// The tag beside the flag's level, nil on the default (Peak, the
    /// desktop's S-Meter), so the operator sees which source is on.
    public var tag: String? {
        switch self {
        case .peak:
            return nil
        case .average:
            return "AV"
        case .maxBin:
            return "MAX"
        }
    }

    /// The choice an RX Mode shows on the flags.
    public init(_ mode: SMeterRxMode) {
        switch mode {
        case .signal, .signalPeak:
            self = .peak
        case .signalAverage:
            self = .average
        case .maxBin:
            self = .maxBin
        }
    }

    /// The RX Mode this choice sets, from `current`: Peak keeps Signal Peak
    /// (the meter's held peak) when that is already on, and otherwise
    /// takes the desktop's default, S-Meter.
    public func rxMode(from current: SMeterRxMode) -> SMeterRxMode {
        switch self {
        case .peak:
            return FlagMeterChoice(current) == .peak ? current : .signal
        case .average:
            return .signalAverage
        case .maxBin:
            return .maxBin
        }
    }

    /// The level the flag shows for `mode`, nil for none. `peakDbm` and
    /// `averageDbm` are the slice's `signalPeakDbm` and `signalAverageDbm`;
    /// `legacyDbm` is its `signalStrengthDbm`, used only by a Core that
    /// sends neither of the other two; `maxBinDbm` is this device's own
    /// Max Bin for the slice, nil where its pan is not drawn here. The
    /// Core's no-reading value, and anything not a number, is no reading.
    public static func level(mode: SMeterRxMode, peakDbm: Double?, averageDbm: Double?,
                             legacyDbm: Double?, maxBinDbm: Double?) -> Double? {
        if FlagMeterChoice(mode) == .maxBin {
            return SMeterReadings.reading(maxBinDbm)
        }
        if peakDbm == nil && averageDbm == nil {
            return SMeterReadings.reading(legacyDbm)
        }
        return SMeterReadings.reading(FlagMeterChoice(mode) == .average ? averageDbm : peakDbm)
    }
}
