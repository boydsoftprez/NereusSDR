// NereusSDR for iOS: the Live Activity's color roles, the lock-screen card and island in the desktop's workspace look
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The lock-screen card's and the Dynamic Island's color roles
/// (docs/architecture/2026-10-20-phone-workspace-look-design.md, section 8;
/// `lock-board.html`). `ios/Shared` builds into the widget too, so each role
/// reads the app's own token from ``WorkspacePalette`` rather than repeating
/// it; the colors written here are the kept data colors (W2).
enum ActivityColours {
    typealias P = WorkspacePalette

    /// The card's ground: iOS's tint takes one color, so it carries the
    /// panel's bottom (kGlassPanelBot) at 0.92 and the card lays the panel's
    /// top over it with ``cardLift``, giving the panel gradient.
    static let card = P.glassPanelBot.opacity(0.92)
    /// The panel's top (kGlassPanelTop) at 0.92, fading down the card.
    static let cardLift = P.glassPanelTop.opacity(0.92)
    /// Keyed: the TX badge's red under the whole card, set by hand (W10).
    static let cardKeyed = P.badgeTxBg.opacity(0.93)
    static let cardEdge = P.border
    static let cardEdgeKeyed = P.redBorder
    static let cardEdgeLost = P.redBorder.opacity(0.7)
    /// StandBy draws the card's ground edge to edge: the panel color.
    static let standBy = P.panelBg

    static let text = P.textPrimary
    static let textBright = P.textBright
    static let textDim = P.textSecondary
    /// The flag's frequency cyan, kept (W2).
    static let frequency = rgb(0x00, 0xE5, 0xFF)
    /// The bandwidth cyan, kept (W2).
    static let bandwidth = rgb(0x00, 0xC8, 0xFF)
    static let timeOut = P.amberText
    static let good = P.liveGreen

    // The link chip: an inset, its dot.
    static let chip = P.insetBg
    static let chipEdge = P.insetBorder
    static let chipRadius = P.chipRadius
    static let linkUp = P.liveGreen
    /// Not transmit, so the muted red (W5).
    static let linkLost = P.redBorder
    static let linkOff = P.borderMuted

    // The TX badge and clock (transmit, so kTxRed on the edge).
    static let txFill = P.badgeTxBg
    static let txEdge = P.txRed
    static let txText = P.keyedRim
    static let txClockText = P.keyedRim

    // The buttons, each one of the desktop's faces.
    /// UNKEY: the keyed face, kTxRed (W5).
    static let unkey = WorkspaceLook.keyed
    /// The speaker at rest.
    static let button = WorkspaceLook.raised
    /// The speaker while this phone is muted.
    static let muted = WorkspaceLook.litAmber
    /// Cancel on the lost card: the muted danger set (W5).
    static let cancel = WorkspaceLook.danger
    /// Reconnect on the lost card.
    static let reconnect = WorkspaceLook.glass

    // The signal bar (the flag's).
    static let levelTicks = P.textScale
    static let levelBackground = P.insetBg
    static let levelEdge = P.border
    /// The fill up to S9 (a fill, never small text).
    static let levelLow = P.accentFill
    /// The reading's words while under S9.
    static let levelLowText = P.accentText
    static let levelHigh = P.liveGreen

    /// The slice's color from the Core's `#RRGGBB`, the band's grey when it can't be read.
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
