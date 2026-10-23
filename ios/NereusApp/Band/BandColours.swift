// NereusSDR for iOS: the colours the flags, tags and zoom buttons are drawn in, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The colours of the band's flags, folded tags and zoom buttons, as the
/// board draws them (docs/architecture/2026-09-23-iphone-app-design/board.html,
/// the `.vfo`, `.vfo-fold` and `.zoom` styles). A slice's own colour is the
/// Core's, read from the catalogue; these are the parts around it.
enum BandColours {
    // The desktop's workspace look (docs/architecture/2026-10-20-phone-workspace-look-design.md,
    // appendix A): chrome roles point at WorkspacePalette tokens, a raised role
    // holds the raised treatment's flat bottom, a value the palette does not
    // hold is the board's mapping (make_board.py), and the kept data colors
    // (W2) stay as they were.
    private typealias P = WorkspacePalette

    static let flagBackground = P.statusBarBg.opacity(0.902)
    static let flagBorder = Color.white.opacity(0.118)
    static let text = P.textPrimary
    static let rxAntenna = P.rgb(0x4488FF) // kept (W2)
    static let txAntenna = P.txRed
    static let bandwidth = P.rgb(0x00C8FF) // kept (W2)
    static let frequency = P.rgb(0x00E5FF) // kept (W2)
    static let frequencyBorder = Color.white.opacity(0.196)
    /// The flag's step button (the board's `.vfo__step`).
    static let stepBackground = P.glassBtnBot // raised
    static let stepBorder = P.border // raised
    static let tab = P.rgb(0x828288)
    static let tabDivider = Color.white.opacity(0.314)
    static let txBadgeOff = P.rgb(0x1A1A1E)
    static let txBadgeOffBorder = P.border
    static let txBadgeOffText = tab
    static let txBadgeOn = P.badgeTxBg
    static let txBadgeOnBorder = P.txRed
    static let txBadgeOnText = P.keyedRim
    static let buttonBackground = P.glassBtnBot.opacity(0.902) // raised
    static let buttonBorder = P.rgb(0x69696D).opacity(0.706)
    static let levelTicks = tab
    static let levelBackground = P.insetBg
    static let levelBorder = P.border
    static let levelLow = P.accentFill
    static let levelHigh = P.liveGreen
    static let zoomBackground = P.rgb(0x202024).opacity(0.941)
    static let zoomBorder = Color.white.opacity(0.157)
    /// The RADE row's accent for fills and marks (the board's `.rf-row`, the
    /// desktop's SNR row). Nothing draws in it today: the row's words take
    /// radeRowText, and its dots and mode tab have their own roles. Kept, and
    /// pinned, as the row's fill accent.
    static let radeRow = P.accentFill
    /// The RADE row's words, in the accent for text (spec section 10).
    static let radeRowText = P.accentText
    static let radeGood = P.rgb(0x6FA384)
    static let radeMarginal = P.rgb(0xE0E040) // kept (W2)
    static let radeHollow = P.rgb(0x505054)
    static let radeOff = tab
    static let radeMode = P.rgb(0xA78BFA) // kept (W2)

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
    static let readoutBackground = P.rgb(0x18181C).opacity(0.706)
    /// The finger's frequency box, the desktop's cursor readout at 200 of
    /// 255 (value from src/gui/SpectrumWidget.cpp:6609).
    static let cursorBackground = P.rgb(0x18181C).opacity(200.0 / 255)
    /// The waterfall's time, the desktop's `#C8DCFF` at 10 points (values
    /// from src/gui/SpectrumWidget.cpp:4642-4644).
    static let timestamp = P.rgb(0xC8DCFF) // kept (W2)
    /// The mark of a stretch the band was not sent (the board's `.wfgap`).
    static let awayMarkBackground = P.statusBarBg.opacity(0.88)
    static let awayMarkEdge = P.textInactive
    static let awayMarkText = P.textSecondary
    static let timestampPoints: CGFloat = 10
    /// The transmit zero line down the spectrum: dashes 4 and gaps 2, a
    /// point wide (values from src/gui/SpectrumWidget.cpp:4286).
    static let txZeroLineDash: [CGFloat] = [4, 2]
}
