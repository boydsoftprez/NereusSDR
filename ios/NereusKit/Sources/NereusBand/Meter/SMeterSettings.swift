// NereusSDR for iOS: the S-meter's menu choices, kept on this device
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Everything the S-meter's menu chooses (D86): RX Mode, TX Mode, the
/// peak hold line and its decay, and the face; and the Multimeter page's
/// Display units and Show decimal point (description V12), which print the
/// signal level wherever the phone shows it (``SMeterReadout``). Each
/// device keeps its own, once for every pan as the desktop does; nothing
/// here goes to the Core. The defaults are the desktop's: Signal, Power,
/// peak hold on at Medium, the Aged Cream face, dBm with the decimal point.
public struct SMeterSettings: Equatable, Sendable, Codable {
    public var rxMode: SMeterRxMode
    public var txMode: SMeterTxMode
    public var peakHold: Bool
    public var peakDecay: SMeterPeakDecay
    public var face: SMeterFace
    /// Display units (`MultimeterUnitMode`).
    public var unit: SMeterUnit
    /// Show decimal point in readouts (`MultimeterShowDecimal`).
    public var showDecimal: Bool

    public init(rxMode: SMeterRxMode = .signal, txMode: SMeterTxMode = .power, peakHold: Bool = true,
                peakDecay: SMeterPeakDecay = .medium, face: SMeterFace = .agedCream,
                unit: SMeterUnit = SMeterReadout.desktopDefaults.unit,
                showDecimal: Bool = SMeterReadout.desktopDefaults.showDecimal) {
        self.rxMode = rxMode
        self.txMode = txMode
        self.peakHold = peakHold
        self.peakDecay = peakDecay
        self.face = face
        self.unit = unit
        self.showDecimal = showDecimal
    }

    /// How the signal level is printed.
    public var readout: SMeterReadout {
        get { SMeterReadout(unit: unit, showDecimal: showDecimal) }
        set {
            unit = newValue.unit
            showDecimal = newValue.showDecimal
        }
    }

    /// The desktop's defaults.
    public static let desktopDefaults = SMeterSettings()
}
