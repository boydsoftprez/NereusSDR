// NereusSDR for iOS: FreeDV Reporter's colours, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The FreeDV Reporter page's colours (the board's `.fdrow`, `.fdtop` and
/// `.fdseg` styles). The three tints are the desktop dialog's dim rust,
/// slate and mauve.
enum FreeDVColours {
    static let transmitting = rgb(0x5E, 0x39, 0x33)
    static let hearing = rgb(0x2E, 0x47, 0x50)
    static let message = rgb(0x4D, 0x3D, 0x4D)
    static let top = rgb(0x0A, 0x0A, 0x14)
    static let topBorder = rgb(0x20, 0x30, 0x40)
    static let rowDivider = rgb(0x1A, 0x2A, 0x3A)
    static let callsign = rgb(0xE4, 0xEE, 0xF8)
    static let frequency = rgb(0x00, 0xE5, 0xFF)
    static let modeChip = rgb(0x5F, 0xA8, 0xFF).opacity(0.1)
    static let modeText = rgb(0x5F, 0xA8, 0xFF)
    static let segmentBorder = rgb(0x30, 0x40, 0x50)

    /// A tint's colour, or clear for none.
    static func colour(_ tint: FreeDVReporterModel.Tint?) -> Color {
        switch tint {
        case .message?: return message
        case .transmitting?: return transmitting
        case .hearing?: return hearing
        case nil: return .clear
        }
    }

    private static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }
}
