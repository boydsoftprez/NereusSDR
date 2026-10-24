// NereusSDR for iOS: the colours the tuning dials are drawn in, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The tuning dials' colours, as the board draws them
/// (docs/architecture/2026-09-23-iphone-app-design/board.html, the `.knob`,
/// `.dsheet` and `.twheel` styles): a machined knob in the workspace greys
/// with its index mark at the top.
enum DialColours {
    // The desktop's workspace look (docs/architecture/2026-10-20-phone-workspace-look-design.md,
    // appendix A): the body greys by the board's neutral fallback (make_board.py
    // `neutral`, the grey's lightness kept and its blue cast dropped), the
    // index in the workspace accent, the lit button glass, the step box an
    // inset and the sheet a panel. The index tick and its glow are kept.
    private typealias P = WorkspacePalette

    static let knob = P.statusBarBg.opacity(0.78)
    static let ringTick = P.rgb(0x4B4B4F)
    static let index = P.accentFill
    /// The index mark as it flashes on a detent (kept, W2).
    static let indexTick = P.rgb(0xC8F8FF)
    static let indexGlow = P.rgb(0x00E5FF)
    static let gripLight = P.rgb(0x46464A)
    static let gripMiddle = P.rgb(0x2A2A2E)
    static let gripDark = P.rgb(0x1C1C20)
    static let knurlDark = P.glassPanelTop // #0e1620 by the fallback
    static let knurlLight = P.rgb(0x26262B)
    static let dimpleDark = P.rgb(0x101014)
    static let dimpleLight = P.rgb(0x28282C)
    static let stepBackground = P.insetBg // inset
    static let stepBorder = P.insetBorder // inset
    static let stepText = P.textPrimary
    static let stepCaption = P.textScale
    static let sheet = P.panelBg // panel
    static let sheetBorder = P.border // panel
    static let grab = P.textInactive
    static let frequency = P.rgb(0x00E5FF) // kept (W2)
    static let button = P.rgb(0x1A1A1E)
    static let buttonBorder = P.border
    static let buttonOn = P.glassSelBot // glass
    static let buttonOnBorder = P.glassSelBorder // glass
    static let trackEdge = P.rgb(0x26262B)
    static let trackMiddle = P.rgb(0x18181C)
    static let wheelTick = P.rgb(0x5D5D61)
    static let wheelFade = P.statusBarBg.opacity(0.95)
}
