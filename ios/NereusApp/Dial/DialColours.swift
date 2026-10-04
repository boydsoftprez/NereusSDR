// NereusSDR for iOS: the colours the tuning dials are drawn in, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The tuning dials' colours, as the board draws them
/// (docs/architecture/2026-09-23-iphone-app-design/board.html, the `.knob`,
/// `.dsheet` and `.twheel` styles): a machined knob in NereusSDR's navy
/// with its index mark at the top.
enum DialColours {
    static let knob = rgb(0x0A, 0x0A, 0x14).opacity(0.78)
    static let ringTick = rgb(0x3A, 0x4A, 0x5C)
    static let index = rgb(0x00, 0xB4, 0xD8)
    /// The index mark as it flashes on a detent.
    static let indexTick = rgb(0xC8, 0xF8, 0xFF)
    static let indexGlow = rgb(0x00, 0xE5, 0xFF)
    static let gripLight = rgb(0x33, 0x46, 0x5A)
    static let gripMiddle = rgb(0x1C, 0x29, 0x38)
    static let gripDark = rgb(0x12, 0x1B, 0x26)
    static let knurlDark = rgb(0x0E, 0x16, 0x20)
    static let knurlLight = rgb(0x2A, 0x3A, 0x4C)
    static let dimpleDark = rgb(0x0A, 0x0F, 0x16)
    static let dimpleLight = rgb(0x1B, 0x28, 0x36)
    static let stepBackground = rgb(0x0F, 0x14, 0x20)
    static let stepBorder = rgb(0x30, 0x40, 0x50)
    static let stepText = rgb(0xC8, 0xD8, 0xE8)
    static let stepCaption = rgb(0x60, 0x70, 0x80)
    static let sheet = rgb(0x0A, 0x0A, 0x18)
    static let sheetBorder = rgb(0x30, 0x40, 0x50)
    static let grab = rgb(0x40, 0x50, 0x60)
    static let frequency = rgb(0x00, 0xE5, 0xFF)
    static let button = rgb(0x1A, 0x2A, 0x3A)
    static let buttonBorder = rgb(0x30, 0x40, 0x50)
    static let buttonOn = rgb(0x00, 0x70, 0xC0)
    static let buttonOnBorder = rgb(0x00, 0x90, 0xE0)
    static let trackEdge = rgb(0x23, 0x32, 0x44)
    static let trackMiddle = rgb(0x0F, 0x16, 0x20)
    static let wheelTick = rgb(0x4A, 0x5C, 0x70)
    static let wheelFade = rgb(0x0A, 0x0A, 0x14).opacity(0.95)

    private static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }
}
