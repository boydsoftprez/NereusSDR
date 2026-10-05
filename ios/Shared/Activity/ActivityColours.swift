// NereusSDR for iOS: the Live Activity's colours, the board's lock-screen card and island in NereusSDR's palette
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The lock-screen card's and the Dynamic Island's colours, as the board
/// draws them (`board.html`, the Lock screen and In another app sections).
/// The widget can't reach the app's own palette, so the few it shares with
/// the band are repeated here with the same values.
enum ActivityColours {
    static let card = Color(red: 15 / 255, green: 15 / 255, blue: 26 / 255).opacity(0.9)
    static let cardKeyed = Color(red: 32 / 255, green: 12 / 255, blue: 16 / 255).opacity(0.93)
    static let cardEdge = Color.white.opacity(0.07)
    static let cardEdgeKeyed = Color(red: 1, green: 68 / 255, blue: 68 / 255).opacity(0.6)
    static let cardEdgeLost = Color(red: 193 / 255, green: 72 / 255, blue: 72 / 255).opacity(0.6)
    /// StandBy draws the card's colour edge to edge.
    static let standBy = rgb(0x0F, 0x0F, 0x1A)

    static let text = rgb(0xC8, 0xD8, 0xE8)
    static let textBright = rgb(0xE4, 0xEE, 0xF8)
    static let textDim = rgb(0x80, 0x90, 0xA0)
    static let frequency = rgb(0x00, 0xE5, 0xFF)
    static let bandwidth = rgb(0x00, 0xC8, 0xFF)
    static let timeOut = rgb(0xFF, 0xD7, 0x00)
    static let good = rgb(0x39, 0xC1, 0x67)

    // The link chip.
    static let chip = rgb(0x0F, 0x14, 0x20)
    static let linkUp = rgb(0x5F, 0xFF, 0x8A)
    static let linkLost = rgb(0xC1, 0x48, 0x48)
    static let linkOff = rgb(0x40, 0x48, 0x58)

    // The TX badge and clock (the flag's TX badge, checked).
    static let txFill = rgb(0x6A, 0x30, 0x30)
    static let txEdge = rgb(0xFF, 0x44, 0x44)
    static let txText = rgb(0xFF, 0x80, 0x80)
    static let txClockText = rgb(0xFF, 0xB0, 0xB0)

    // UNKEY: the MOX colours.
    static let unkey = rgb(0xCC, 0x22, 0x22)
    static let unkeyEdge = rgb(0xFF, 0x44, 0x44)

    // The speaker button.
    static let button = rgb(0x1A, 0x2A, 0x3A)
    static let buttonEdge = rgb(0x30, 0x40, 0x50)
    static let mutedFill = rgb(0x3A, 0x30, 0x10)
    static let mutedEdge = rgb(0xFF, 0xD7, 0x00)

    // Cancel and Reconnect on the lost card.
    static let cancel = rgb(0x4A, 0x20, 0x20)
    static let cancelEdge = rgb(0x6A, 0x30, 0x30)
    static let cancelText = rgb(0xC8, 0xA8, 0xA8)
    static let reconnect = rgb(0x00, 0x70, 0xC0)
    static let reconnectEdge = rgb(0x00, 0x90, 0xE0)

    // The signal bar (the flag's).
    static let levelTicks = rgb(0x68, 0x88, 0xA0)
    static let levelBackground = rgb(0x10, 0x10, 0x1C)
    static let levelEdge = rgb(0x30, 0x40, 0x50)
    static let levelLow = rgb(0x00, 0xB4, 0xD8)
    static let levelHigh = rgb(0x00, 0xD8, 0x60)

    /// The slice's colour from the Core's `#RRGGBB`, the band's grey when it can't be read.
    static func slice(_ hex: String) -> Color {
        let digits = hex.hasPrefix("#") ? String(hex.dropFirst()) : hex
        guard digits.count == 6, let value = UInt32(digits, radix: 16) else {
            return rgb(0x80, 0x90, 0xA0)
        }
        return rgb(Double((value >> 16) & 0xFF), Double((value >> 8) & 0xFF), Double(value & 0xFF))
    }

    static func rgb(_ red: Double, _ green: Double, _ blue: Double) -> Color {
        Color(red: red / 255, green: green / 255, blue: blue / 255)
    }
}
