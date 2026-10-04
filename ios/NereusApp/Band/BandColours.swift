// NereusSDR for iOS: the colours the flags, tags and zoom buttons are drawn in, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The colours of the band's flags, folded tags and zoom buttons, as the
/// board draws them (docs/architecture/2026-09-23-iphone-app-design/board.html,
/// the `.vfo`, `.vfo-fold` and `.zoom` styles). A slice's own colour is the
/// Core's, read from the catalogue; these are the parts around it.
enum BandColours {
    static let flagBackground = rgb(0x0A, 0x0A, 0x14).opacity(0.902)
    static let flagBorder = Color.white.opacity(0.118)
    static let text = rgb(0xC8, 0xD8, 0xE8)
    static let rxAntenna = rgb(0x44, 0x88, 0xFF)
    static let txAntenna = rgb(0xFF, 0x44, 0x44)
    static let bandwidth = rgb(0x00, 0xC8, 0xFF)
    static let frequency = rgb(0x00, 0xE5, 0xFF)
    static let frequencyBorder = Color.white.opacity(0.196)
    /// The flag's step button (the board's `.vfo__step`).
    static let stepBackground = rgb(0x0D, 0x1B, 0x28)
    static let stepBorder = rgb(0x2A, 0x4A, 0x60)
    static let tab = rgb(0x68, 0x88, 0xA0)
    static let tabDivider = Color.white.opacity(0.314)
    static let txBadgeOff = rgb(0x1A, 0x2A, 0x3A)
    static let txBadgeOffBorder = rgb(0x30, 0x40, 0x50)
    static let txBadgeOffText = rgb(0x68, 0x88, 0xA0)
    static let txBadgeOn = rgb(0x6A, 0x30, 0x30)
    static let txBadgeOnBorder = rgb(0xFF, 0x44, 0x44)
    static let txBadgeOnText = rgb(0xFF, 0x80, 0x80)
    static let buttonBackground = rgb(0x14, 0x1E, 0x32).opacity(0.902)
    static let buttonBorder = rgb(0x50, 0x64, 0x82).opacity(0.706)
    static let levelTicks = rgb(0x68, 0x88, 0xA0)
    static let levelBackground = rgb(0x10, 0x10, 0x1C)
    static let levelBorder = rgb(0x30, 0x40, 0x50)
    static let levelLow = rgb(0x00, 0xB4, 0xD8)
    static let levelHigh = rgb(0x00, 0xD8, 0x60)
    static let zoomBackground = rgb(0x14, 0x1E, 0x2D).opacity(0.941)
    static let zoomBorder = Color.white.opacity(0.157)
    /// The flag's RADE row (the board's `.rf-row`, the desktop's SNR row):
    /// the words, the locked-on dot at 5 dB or more, the dot below 5 dB,
    /// the not-locked dot, the greyed older-Core row, and the mode tab in RADE.
    static let radeRow = rgb(0x00, 0xB4, 0xD8)
    static let radeGood = rgb(0x00, 0xFF, 0x88)
    static let radeMarginal = rgb(0xE0, 0xE0, 0x40)
    static let radeHollow = rgb(0x50, 0x50, 0x50)
    static let radeOff = rgb(0x68, 0x88, 0xA0)
    static let radeMode = rgb(0xA7, 0x8B, 0xFA)

    /// A slice's colour from the Core's `#RRGGBB`.
    static func slice(_ hex: String) -> Color {
        let rgba = BandPalette.rgba(hex) ?? BandPalette.rgba(BandSlice.colourUnknown) ?? SIMD4(0.5, 0.56, 0.63, 1)
        return Color(red: Double(rgba.x), green: Double(rgba.y), blue: Double(rgba.z))
    }

    /// A display setting's colour, `#RRGGBBAA` or `#RRGGBB`, with its opacity.
    static func withAlpha(_ hex: String) -> Color {
        let rgba = BandPalette.rgbaWithAlpha(hex)
        return Color(red: Double(rgba.x), green: Double(rgba.y), blue: Double(rgba.z), opacity: Double(rgba.w))
    }

    /// The readouts' inset box on the band (the desktop's text overlays).
    static let readoutBackground = rgb(0x10, 0x15, 0x20).opacity(0.706)
    /// The finger's frequency box, the desktop's cursor readout at 200 of
    /// 255 (value from src/gui/SpectrumWidget.cpp:6609).
    static let cursorBackground = rgb(0x10, 0x15, 0x20).opacity(200.0 / 255)
    /// The waterfall's time, the desktop's `#C8DCFF` at 10 points (values
    /// from src/gui/SpectrumWidget.cpp:4642-4644).
    static let timestamp = rgb(0xC8, 0xDC, 0xFF)
    /// The mark of a stretch the band was not sent (the board's `.wfgap`).
    static let awayMarkBackground = rgb(0x0A, 0x0A, 0x14).opacity(0.88)
    static let awayMarkEdge = rgb(0x40, 0x50, 0x60)
    static let awayMarkText = rgb(0x80, 0x90, 0xA0)
    static let timestampPoints: CGFloat = 10
    /// The transmit zero line down the spectrum: dashes 4 and gaps 2, a
    /// point wide (values from src/gui/SpectrumWidget.cpp:4286).
    static let txZeroLineDash: [CGFloat] = [4, 2]

    private static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }
}
