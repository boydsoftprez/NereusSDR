// NereusSDR for iOS: the S-meter's state: its menu choices, the Core's readings and the peaks, turned into what it shows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels

/// The S-meter (D86) without its drawing: the menu's choices, the Core's
/// latest readings and the peaks, on a clock the caller supplies in
/// seconds. Each menu item changes what ``display(at:meters:)`` returns as
/// the same item changes the desktop's meter.
public struct SMeterState: Equatable, Sendable {
    public private(set) var settings: SMeterSettings
    public private(set) var readings = SMeterReadings()
    public private(set) var peaks: SMeterPeaks

    public init(settings: SMeterSettings = .desktopDefaults) {
        self.settings = settings
        peaks = SMeterPeaks(holdOn: settings.peakHold, decay: settings.peakDecay)
    }

    // MARK: The Core's readings

    /// The Core's latest readings at `now`.
    public mutating func apply(_ readings: SMeterReadings, at now: Double) {
        self.readings = readings
        peaks.update(level: readings.level(for: settings.rxMode), at: now)
    }

    // MARK: The menu

    /// RX Mode: the needle follows that reading from now on.
    public mutating func chooseRx(_ mode: SMeterRxMode, at now: Double) {
        settings.rxMode = mode
        peaks.update(level: readings.level(for: mode), at: now)
    }

    /// TX Mode: on the air the needle reads that on its scale.
    public mutating func chooseTx(_ mode: SMeterTxMode) {
        settings.txMode = mode
    }

    /// Peak Hold's Enabled.
    public mutating func setPeakHold(_ on: Bool) {
        settings.peakHold = on
        peaks.setHold(on)
    }

    /// Peak Hold's Decay.
    public mutating func choosePeakDecay(_ decay: SMeterPeakDecay) {
        settings.peakDecay = decay
        peaks.setDecay(decay)
    }

    /// Peak Hold's Reset.
    public mutating func resetPeak() {
        peaks.resetHold()
    }

    /// Meter Face.
    public mutating func chooseFace(_ face: SMeterFace) {
        settings.face = face
    }

    /// Display units, from the Multimeter page: the readouts print the level in it from now on.
    public mutating func chooseUnit(_ unit: SMeterUnit) {
        settings.unit = unit
    }

    /// Show decimal point in readouts, from the Multimeter page.
    public mutating func setShowDecimal(_ on: Bool) {
        settings.showDecimal = on
    }

    /// Runs the peaks' clock to `now`.
    public mutating func advance(to now: Double) {
        peaks.advance(to: now)
    }

    // MARK: What it shows

    /// The meter at `now`, on the Core's scales in `meters`.
    public func display(at now: Double, meters: StationCatalog.Meters?) -> SMeterDisplay {
        var peaks = peaks
        peaks.advance(to: now)
        let style: SMeterScale.Style = settings.face == .classic ? .classic : .vintage
        let sMeter = meters?.sMeter
        let receiveScale = sMeter.map { SMeterScale.receive($0, style: style) }
        let transmitScale = SMeterScale.transmit(settings.txMode, power: meters?.rfPower, style: style)
        if readings.transmitting {
            let mode = settings.txMode
            let value = readings.value(for: mode)
            let right = Self.transmitText(value, mode: mode, style: style)
            return SMeterDisplay(needle: value.map { SMeterScale.transmitFraction($0, mode: mode, power: meters?.rfPower) } ?? 0,
                                 transmitting: true, caption: mode.label, left: "TX", right: right,
                                 peakMarker: nil, holdLine: nil, receiveScale: receiveScale,
                                 transmitScale: transmitScale, title: mode.title, legend: mode.legend,
                                 spoken: Self.transmitSpoken(value, mode: mode))
        }
        let mode = settings.rxMode
        let level = peaks.level
        let shown = mode == .signalPeak ? peaks.peak : level
        let fraction: (Double) -> Double = { dbm in
            sMeter.map { SMeterScale.receiveFraction(dbm: dbm, meter: $0) } ?? 0
        }
        let needle = shown.map(fraction) ?? 0
        var marker: Double?
        if mode == .signalPeak, let level, let peak = peaks.peak, peak > level + 1 {
            marker = fraction(peak)
        }
        var holdLine: Double?
        if let level, let hold = peaks.hold(at: now), let sMeter, hold > sMeter.minDbm + 1 {
            holdLine = hold <= level + 0.01 ? needle : max(fraction(hold), needle)
        }
        // The left readout is the S-units; the right one the level in the
        // chosen unit, empty in S where the left one already says it.
        let readout = settings.readout
        let left = shown.flatMap { dbm in sMeter.map { readout.sUnits(dbm: dbm, meter: $0) } } ?? "--"
        let right: String
        let spoken: String
        if readout.unit == .sUnits {
            right = ""
            spoken = shown == nil ? "No signal reading" : left
        } else {
            right = shown.flatMap { readout.text(dbm: $0, meter: sMeter) } ?? readout.noReadingText
            spoken = shown.flatMap { dbm in readout.spoken(dbm: dbm, meter: sMeter).map { "\(left), \($0)" } }
                ?? "No signal reading"
        }
        return SMeterDisplay(needle: needle, transmitting: false, caption: mode.caption, left: left, right: right,
                             peakMarker: marker, holdLine: holdLine, receiveScale: receiveScale,
                             transmitScale: transmitScale, title: "SIGNAL STRENGTH", legend: mode.legend,
                             spoken: spoken)
    }

    /// The desktop's S-unit readout without the decimal point: `S0` at or
    /// below the floor, `S7` up to S9 (rounded to the nearest unit), `S9+13`
    /// above it.
    public static func sUnits(dbm: Double, meter: StationCatalog.Meters.SMeter) -> String {
        SMeterReadout(unit: .sUnits, showDecimal: false).sUnits(dbm: dbm, meter: meter)
    }

    /// A transmit readout: `5 W`, `1.5` (`1.5 : 1` on a vintage card), `-10 dB`.
    static func transmitText(_ value: Double?, mode: SMeterTxMode, style: SMeterScale.Style) -> String {
        switch mode {
        case .power:
            return value.map { String(format: "%.0f W", $0) } ?? "-- W"
        case .swr:
            let text = value.map { String(format: "%.1f", $0) } ?? "--"
            return style == .vintage ? "\(text) : 1" : text
        case .level, .compression:
            return value.map { String(format: "%.0f dB", $0) } ?? "-- dB"
        }
    }

    static func transmitSpoken(_ value: Double?, mode: SMeterTxMode) -> String {
        guard let value else {
            return "\(mode.label), no reading"
        }
        switch mode {
        case .power:
            return "Forward power \(Int(value.rounded())) watts"
        case .swr:
            return String(format: "SWR %.1f", value)
        case .level:
            return "Mic level \(Int(value.rounded())) dB"
        case .compression:
            return "Compression \(Int(value.rounded())) dB"
        }
    }
}
