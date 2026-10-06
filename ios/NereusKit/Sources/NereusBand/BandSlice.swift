// NereusSDR for iOS: one of this device's slices as the band draws it: its flag, its marker and its tuning
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusModels

/// One slice on the band (R-IOS-11, D9, D10): what its flag, its folded tag
/// and its marker need, read from the Core's slice and the Core's
/// catalogue. Nothing here is the app's own: the colour is the catalogue's
/// entry for the slice, the sideband the catalogue's for the slice's mode.
public struct BandSlice: Equatable, Sendable, Identifiable {
    /// The slice's number on the Core (`sliceIndex`, the `slice:<id>` key).
    public var id: Int
    public var frequencyHz: Double
    /// The passband's edges from the centre, signed as the slice's
    /// `filterLow` and `filterHigh`.
    public var filterLowHz: Double
    public var filterHighHz: Double
    /// `#RRGGBB`, the catalogue's colour for this slice.
    public var colour: String
    /// True for a lower-sideband mode: the flag then hangs to the right of
    /// the line, away from the passband, as on the desktop.
    public var lowerSideband: Bool
    /// The Core marks the slice this device transmits on; false while
    /// nobody holds transmit.
    public var txSlice: Bool

    public init(id: Int, frequencyHz: Double, filterLowHz: Double, filterHighHz: Double, colour: String,
                lowerSideband: Bool, txSlice: Bool = false) {
        self.id = id
        self.frequencyHz = frequencyHz
        self.filterLowHz = filterLowHz
        self.filterHighHz = filterHighHz
        self.colour = colour
        self.lowerSideband = lowerSideband
        self.txSlice = txSlice
    }

    /// What a slice is drawn in when the catalogue has no colour for it:
    /// the band's neutral grey, never another slice's colour.
    public static let colourUnknown = "#8090A0"

    /// The slice's letter: A for slice 0, B for slice 1, and so on.
    public var letter: String { Self.letter(forIndex: id) }

    public static func letter(forIndex index: Int) -> String {
        guard index >= 0, index < 26, let scalar = Unicode.Scalar(UInt32(65 + index)) else {
            return "?"
        }
        return String(Character(scalar))
    }

    /// The slice's colour in `colours` (the catalogue's `sliceColours`,
    /// slice A's first), or ``colourUnknown`` when it has none.
    public static func colour(forIndex index: Int, in colours: [String]) -> String {
        guard index >= 0, index < colours.count else {
            return colourUnknown
        }
        return colours[index]
    }

    /// Whether the catalogue's `mode` is a lower-sideband mode.
    public static func isLowerSideband(mode id: Int, in modes: [StationCatalog.Mode]) -> Bool {
        modes.first(where: { $0.id == id })?.sideband == .lower
    }

    /// The passband's low and high frequencies, low first.
    public var passbandHz: ClosedRange<Double> {
        let a = frequencyHz + filterLowHz
        let b = frequencyHz + filterHighHz
        return min(a, b)...max(a, b)
    }

    /// The frequency as the flag shows it: megahertz, kilohertz and hertz
    /// in groups of three, `7.236.400`.
    public var frequencyText: String { Self.frequencyText(hz: frequencyHz) }

    public static func frequencyText(hz: Double) -> String {
        let whole = hz.isFinite ? Int64(max(0, hz.rounded())) : 0
        let mhz = whole / 1_000_000
        let khz = (whole / 1_000) % 1_000
        let rest = whole % 1_000
        return "\(mhz)." + pad(khz) + "." + pad(rest)
    }

    /// The passband's width as the flag's header shows it: `2.9K`, or hertz below a kilohertz.
    public var bandwidthText: String {
        Self.widthText(hz: passbandHz.upperBound - passbandHz.lowerBound)
    }

    /// A passband width as the flag and the filter buttons show it: `2.9K`,
    /// or hertz below a kilohertz.
    public static func widthText(hz: Double) -> String {
        let width = hz.isFinite ? Int(abs(hz).rounded()) : 0
        if width >= 1000 {
            return String(format: "%.1fK", Double(width) / 1000)
        }
        return String(width)
    }

    private static func pad(_ value: Int64) -> String {
        let text = String(value)
        return String(repeating: "0", count: max(0, 3 - text.count)) + text
    }
}
