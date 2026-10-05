// NereusSDR for iOS: the Core's readings the S-meter can show, and why a mode it cannot show is off
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// What the Core sends the S-meter (D86): the active slice's receive
/// readings and the radio's transmit readings, and which of them this Core
/// sends at all. A mode whose reading the Core does not send is shown in
/// the menu disabled, with its reason.
public struct SMeterReadings: Equatable, Sendable {
    /// The Core's value for no reading (the link document's slice readings).
    public static let noReadingDbm = -400.0

    /// The receiver's peak meter (`signalPeakDbm`), which Signal and
    /// Signal Peak read; nil for none.
    public var peakDbm: Double?
    /// The receiver's average meter (`signalAverageDbm`), Sig Avg's.
    public var averageDbm: Double?
    /// The strongest displayed trace point in this slice's passband.
    public var maxBinDbm: Double?
    /// This slice's own pan display is subscribed and has a current frame.
    public var maxBinAvailable: Bool
    /// The radio is on the air.
    public var transmitting: Bool
    /// The Core sends this device its transmit readings (`txState`).
    public var transmitSent: Bool
    /// The Core's `txReadingsVersion`, 0 without it.
    public var txReadingsVersion: Int64
    public var forwardWatts: Double
    public var swr: Double
    /// The mic level in dB; nil for none.
    public var micLevelDb: Double?
    /// The speech compressor's gain reduction in dB; nil for none.
    public var compressionDb: Double?
    /// The Core sends the compression reading at all.
    public var compressionSent: Bool

    public init(peakDbm: Double? = nil, averageDbm: Double? = nil, maxBinDbm: Double? = nil,
                maxBinAvailable: Bool = false, transmitting: Bool = false, transmitSent: Bool = false,
                txReadingsVersion: Int64 = 0, forwardWatts: Double = 0, swr: Double = 1,
                micLevelDb: Double? = nil, compressionDb: Double? = nil, compressionSent: Bool = false) {
        self.peakDbm = peakDbm
        self.averageDbm = averageDbm
        self.maxBinDbm = maxBinDbm
        self.maxBinAvailable = maxBinAvailable
        self.transmitting = transmitting
        self.transmitSent = transmitSent
        self.txReadingsVersion = txReadingsVersion
        self.forwardWatts = forwardWatts
        self.swr = swr
        self.micLevelDb = micLevelDb
        self.compressionDb = compressionDb
        self.compressionSent = compressionSent
    }

    /// A reading, or nil for the Core's no-reading value or a value that is not a number.
    public static func reading(_ value: Double?) -> Double? {
        guard let value, value.isFinite, value > noReadingDbm else {
            return nil
        }
        return value
    }

    /// The reading `mode` shows; nil for none.
    public func level(for mode: SMeterRxMode) -> Double? {
        switch mode {
        case .signal, .signalPeak:
            return Self.reading(peakDbm)
        case .signalAverage:
            return Self.reading(averageDbm)
        case .maxBin:
            return maxBinAvailable ? Self.reading(maxBinDbm) : nil
        }
    }

    /// The reading `mode` shows while on the air; nil for none.
    public func value(for mode: SMeterTxMode) -> Double? {
        guard reason(for: mode) == nil else {
            return nil
        }
        switch mode {
        case .power:
            return forwardWatts
        case .swr:
            return swr
        case .level:
            return Self.reading(micLevelDb)
        case .compression:
            guard let raw = Self.reading(compressionDb) else {
                return nil
            }
            // The Core sends COMPPEAK in dBFS. The desktop's silence gate
            // rests the compression needle at zero below -30 dB.
            return raw > -30 ? min(max(raw, -25), 0) : 0
        }
    }

    // MARK: Why a mode is off

    /// Max Bin depends on this slice's live pan trace, which this device draws.
    public static let maxBinUnavailableText = "Max Bin reads this slice's panadapter. Open its display to see it."
    /// A Core that sends this device no transmit readings.
    public static let transmitNotSentText = "This Core does not send its transmit readings to this device."
    /// A Core that does not send the compression reading.
    public static let compressionNotSentText =
        "This Core does not send the compression reading. Updating the Core may help."

    /// Why `mode` cannot be shown, or nil when it can.
    public func reason(for mode: SMeterRxMode) -> String? {
        mode == .maxBin && !maxBinAvailable ? Self.maxBinUnavailableText : nil
    }

    /// Why `mode` cannot be shown, or nil when it can. Power, SWR and
    /// Level read the transmit state every Core with remote transmit
    /// sends; compression needs the transmit readings of
    /// `txReadingsVersion` 1 and the reading itself.
    public func reason(for mode: SMeterTxMode) -> String? {
        guard transmitSent else {
            return Self.transmitNotSentText
        }
        if mode == .compression && (txReadingsVersion < 1 || !compressionSent) {
            return Self.compressionNotSentText
        }
        return nil
    }
}
