// NereusSDR for iOS: FreeDV Reporter's colours, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The FreeDV Reporter page's colours (the board's `.fdrow`, `.fdtop` and
/// `.fdseg` styles). The three tints are the desktop dialog's dim rust,
/// slate and mauve.
enum FreeDVColours {
    // The row tints are kept (W2); the rest is the desktop's workspace look
    // (docs/architecture/2026-10-20-phone-workspace-look-design.md, appendix A
    // and the board's mapping, make_board.py).
    static let transmitting = WorkspacePalette.rgb(0x5E3933)
    static let hearing = WorkspacePalette.rgb(0x2E4750)
    static let message = WorkspacePalette.rgb(0x4D3D4D)
    static let top = WorkspacePalette.statusBarBg
    static let topBorder = WorkspacePalette.borderSubtle
    static let rowDivider = WorkspacePalette.rgb(0x1A1A1E)
    static let callsign = WorkspacePalette.textBright
    static let frequency = WorkspacePalette.rgb(0x00E5FF) // kept (W2)
    static let modeChip = WorkspacePalette.accentText.opacity(0.1)
    static let modeText = WorkspacePalette.accentText
    static let segmentBorder = WorkspacePalette.border

    /// A tint's colour, or clear for none.
    static func colour(_ tint: FreeDVReporterModel.Tint?) -> Color {
        switch tint {
        case .message?: return message
        case .transmitting?: return transmitting
        case .hearing?: return hearing
        case nil: return .clear
        }
    }
}
