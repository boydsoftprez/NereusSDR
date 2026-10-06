// NereusSDR for iOS: a waterfall palette from the Core's catalogue, as 256 colours, and colour text as numbers
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels

/// The waterfall's colours (R-IOS-11): the chosen palette of the Core's
/// catalogue, found by the desktop's palette number and spread over 256
/// entries, each stop's colour at its place and straight lines between.
/// The palettes are the Core's; the app carries none (D4). With no palette
/// (no catalogue yet) the waterfall is drawn in greys.
public enum BandPalette {
    public static let entries = 256

    /// The palette with `id` in `palettes`, else the first one, else nil.
    public static func palette(id: Int, in palettes: [StationCatalog.Palette]) -> StationCatalog.Palette? {
        palettes.first(where: { $0.id == id }) ?? palettes.first
    }

    /// The palette number of this phone's own gradient
    /// (``BandDisplaySettings/customPalette``): never one of the Core's.
    public static let customPaletteId = -1
    public static let customPaletteName = "Custom (this phone)"

    /// 256 colours, RGBA bytes, for `palette`.
    public static func table(for palette: StationCatalog.Palette?) -> [UInt8] {
        table(stops: (palette?.stops ?? []).map { (at: $0.at, colour: $0.colour) })
    }

    /// `table` with its first colour, what a line at or under its low
    /// level is drawn in, replaced by `lowColour` (`#RRGGBBAA`) when given.
    public static func table(_ table: [UInt8], lowColour: String?) -> [UInt8] {
        let unread = SIMD4<Float>(repeating: -1)
        guard let lowColour, table.count >= 4 else {
            return table
        }
        let colour = rgbaWithAlpha(lowColour, fallback: unread)
        guard colour != unread else {
            return table
        }
        var result = table
        for channel in 0..<4 {
            result[channel] = UInt8((min(max(colour[channel], 0), 1) * 255).rounded())
        }
        return result
    }

    /// 256 colours, RGBA bytes, for this phone's own gradient.
    public static func table(custom stops: [PaletteStop]) -> [UInt8] {
        table(stops: stops.map { (at: $0.at, colour: $0.colour) })
    }

    private static func table(stops given: [(at: Double, colour: String)]) -> [UInt8] {
        var table = [UInt8](repeating: 255, count: entries * 4)
        let stops = given
            .compactMap { stop in rgba(stop.colour).map { (at: min(max(stop.at, 0), 1), colour: $0) } }
            .sorted { $0.at < $1.at }
        for index in 0..<entries {
            let position = Double(index) / Double(entries - 1)
            let colour: SIMD4<Float>
            if stops.isEmpty {
                colour = SIMD4(Float(position), Float(position), Float(position), 1)
            } else if position <= stops[0].at {
                colour = stops[0].colour
            } else if position >= stops[stops.count - 1].at {
                colour = stops[stops.count - 1].colour
            } else {
                let upper = stops.firstIndex(where: { $0.at >= position }) ?? stops.count - 1
                let low = stops[upper - 1]
                let high = stops[upper]
                let span = high.at - low.at
                let t = Float(span > 0 ? (position - low.at) / span : 0)
                colour = low.colour + (high.colour - low.colour) * t
            }
            for channel in 0..<4 {
                table[index * 4 + channel] = UInt8((min(max(colour[channel], 0), 1) * 255).rounded())
            }
        }
        return table
    }

    /// `#RRGGBBAA` (or `#RRGGBB`, opaque) as red, green, blue and alpha
    /// from 0 to 1, or `fallback` when it cannot be read.
    public static func rgbaWithAlpha(_ text: String, fallback: SIMD4<Float> = SIMD4(1, 1, 1, 1)) -> SIMD4<Float> {
        let digits = text.hasPrefix("#") ? text.dropFirst() : Substring(text)
        if digits.count == 8, let value = UInt32(digits, radix: 16) {
            return SIMD4(Float((value >> 24) & 0xFF) / 255, Float((value >> 16) & 0xFF) / 255,
                         Float((value >> 8) & 0xFF) / 255, Float(value & 0xFF) / 255)
        }
        return rgba(text) ?? fallback
    }

    /// Red, green, blue and alpha from 0 to 1 as `#RRGGBBAA`.
    public static func hex(_ colour: SIMD4<Float>) -> String {
        func byte(_ part: Float) -> Int { Int((min(max(part, 0), 1) * 255).rounded()) }
        return String(format: "#%02X%02X%02X%02X", byte(colour.x), byte(colour.y), byte(colour.z), byte(colour.w))
    }

    /// `#RRGGBB` as red, green, blue and alpha from 0 to 1, or nil.
    public static func rgba(_ text: String, alpha: Float = 1) -> SIMD4<Float>? {
        let digits = text.hasPrefix("#") ? text.dropFirst() : Substring(text)
        guard digits.count == 6, let value = UInt32(digits, radix: 16) else {
            return nil
        }
        return SIMD4(Float((value >> 16) & 0xFF) / 255, Float((value >> 8) & 0xFF) / 255,
                     Float(value & 0xFF) / 255, alpha)
    }
}
