// NereusSDR for iOS: the colours of spots on the band and of Spot Hub, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI
import UIKit

/// The spots' colours (the board's `.spot`, `.spot-badge`, `.spot-sheet`,
/// `.spot-card` and `.slrow` styles; values as the desktop draws them, D83),
/// the chrome in the desktop's workspace look
/// (docs/architecture/2026-10-20-phone-workspace-look-design.md, appendix A
/// and the board's mapping, make_board.py).
enum SpotColours {
    /// A +N badge: blue-grey at 200 of 255, with amber text.
    static let badge = WorkspacePalette.border.opacity(200.0 / 255)
    static let badgeText = WorkspacePalette.amberText
    /// A spot's tick down to the foot of the spectrum: its colour at 120 of 255.
    static let tickOpacity = 120.0 / 255
    /// The hidden spots' sheet and the details card.
    static let sheet = WorkspacePalette.appBg
    static let sheetBorder = WorkspacePalette.border
    static let sheetHead = WorkspacePalette.titleText
    static let rowDivider = WorkspacePalette.buttonBg
    static let dim = Color.black.opacity(0.45)
    /// A frequency, the flag's cyan.
    static let frequency = WorkspacePalette.rgb(0x00E5FF) // kept (W2)
    /// The Spot List's source tag (a source color, kept, W2).
    static let sourceTag = WorkspacePalette.rgb(0x5FA8FF)
    /// The console.
    static let console = WorkspacePalette.insetBg
    static let consoleText = WorkspacePalette.titleText
    static let consoleBorder = WorkspacePalette.borderSubtle

    /// A `#RRGGBB` colour.
    static func hex(_ text: String) -> Color {
        BandColours.slice(text)
    }

    /// The label's background on the band: its colour at its opacity while
    /// it is overridden, else none.
    static func background(_ settings: SpotDisplaySettings) -> Color {
        guard let background = settings.labelBackground else {
            return .clear
        }
        return hex(background.colour).opacity(background.opacity)
    }

    /// A picked colour as `#RRGGBB`, as the settings keep it.
    static func hexText(_ colour: Color) -> String {
        var red: CGFloat = 0
        var green: CGFloat = 0
        var blue: CGFloat = 0
        var alpha: CGFloat = 0
        UIColor(colour).getRed(&red, green: &green, blue: &blue, alpha: &alpha)
        func byte(_ part: CGFloat) -> Int {
            Int((min(max(part, 0), 1) * 255).rounded())
        }
        return String(format: "#%02X%02X%02X", byte(red), byte(green), byte(blue))
    }
}
