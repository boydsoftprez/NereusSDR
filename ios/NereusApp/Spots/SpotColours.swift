// NereusSDR for iOS: the colours of spots on the band and of Spot Hub, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI
import UIKit

/// The spots' colours (the board's `.spot`, `.spot-badge`, `.spot-sheet`,
/// `.spot-card` and `.slrow` styles; values as the desktop draws them, D83).
enum SpotColours {
    /// A +N badge: blue-grey at 200 of 255, with amber text.
    static let badge = rgb(0x30, 0x50, 0x70).opacity(200.0 / 255)
    static let badgeText = rgb(0xFF, 0xC0, 0x40)
    /// A spot's tick down to the foot of the spectrum: its colour at 120 of 255.
    static let tickOpacity = 120.0 / 255
    /// The hidden spots' sheet and the details card.
    static let sheet = rgb(0x0F, 0x0F, 0x1A)
    static let sheetBorder = rgb(0x30, 0x50, 0x70)
    static let sheetHead = rgb(0x8A, 0xA8, 0xC0)
    static let rowDivider = rgb(0x1A, 0x2A, 0x3A)
    static let dim = Color.black.opacity(0.45)
    /// A frequency, the flag's cyan.
    static let frequency = rgb(0x00, 0xE5, 0xFF)
    /// The Spot List's source tag.
    static let sourceTag = rgb(0x5F, 0xA8, 0xFF)
    /// The console.
    static let console = rgb(0x07, 0x07, 0x0D)
    static let consoleText = rgb(0xA8, 0xB8, 0xC8)
    static let consoleBorder = rgb(0x20, 0x30, 0x40)

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

    private static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }
}
