// NereusSDR for iOS: the colours of the toolbar, the tab bar and the panels, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The colours around the band: the toolbar, the tab bar, the RX panel and
/// the notices, as the board draws them
/// (docs/architecture/2026-09-23-iphone-app-design/board.html: the `.tbar`,
/// `.tabbar`, `.drawer`, `.nbtn`, `.af` and `.toast` styles). NereusSDR
/// ships one dark palette.
enum ChromeColours {
    // The desktop's workspace look (docs/architecture/2026-10-20-phone-workspace-look-design.md,
    // appendix A): each role points at a WorkspacePalette token; a role whose
    // workspace value is a treatment holds that treatment's flat middle color
    // for a caller still drawing it flat. A value the palette does not hold is
    // the board's mapping (make_board.py MAP) through WorkspacePalette.rgb.
    private typealias P = WorkspacePalette

    // The bars.
    static let bar = P.statusBarBg
    static let barBorder = P.statusBarBorder
    static let text = P.textPrimary
    static let textBright = P.textBright
    static let textDim = P.textSecondary
    static let textFaint = P.textScale
    /// The accent for fills, marks and icons (4.4 on kPanelBg; spec section 10).
    static let accent = P.accentFill
    /// The accent for words: small text and a text button's title (5.6 on kPanelBg; spec section 10).
    static let accentText = P.accentText
    static let icon = P.titleText
    static let iconMuted = P.textInactive
    static let iconOpenBackground = P.glassSelBot // glass
    static let iconOpenBorder = P.glassSelBorder // glass

    // The link chip.
    static let chip = P.insetBg // inset
    static let linkUp = P.liveGreen
    static let linkUpPulse = P.liveGreen
    static let linkUpText = P.rgb(0x6FA384)
    static let linkLost = P.redBorder // W5: not transmit
    static let linkOffDot = P.borderMuted

    // The Core's name, sideways, and the Radio tab's Core card (the board's `.stn`).
    static let stationBorder = P.accentFill.opacity(0.314)
    // A setup page's ground (the board's `.scr`), and its ending button (`.revoke`).
    static let page = P.appBg
    static let revoke = P.badgeTxBg // danger
    static let revokeBorder = P.redBg // danger
    static let revokeText = P.redText // danger

    // The panels.
    static let panel = P.panelBg // panel treatment's flat ground
    static let panelEdge = P.border
    static let scrim = Color.black.opacity(0.35)
    /// The cover over a band that cannot be used now (link lost, receiver taken).
    static let cover = P.statusBarBg.opacity(0.784)
    /// A reason or note box on a panel, and its edge.
    static let reason = P.glassBtnBot
    static let reasonEdge = P.border
    static let titleTop = P.rgb(0x26262B)
    static let titleMiddle = P.rgb(0x26262B)
    static let titleBottom = P.glassBtnBot
    static let titleBorder = P.rgb(0x0D0D0F)
    static let caption = P.textTertiary
    static let button = P.glassBtnBot // raised
    static let buttonBorder = P.border // raised
    static let buttonOnBlue = P.glassSelBot // glass
    static let buttonOnBlueBorder = P.glassSelBorder // glass
    static let buttonOnGreen = P.litGreenMid // lit green
    static let buttonOnGreenBorder = P.litGreenRim // lit green
    static let buttonOnGreenText = P.litGreenText // lit green
    static let buttonOnAmber = P.litAmberMid // lit amber
    static let buttonOnAmberBorder = P.amberBorder // lit amber
    static let buttonOnAmberText = P.amberText // lit amber
    static let buttonOff = P.glassBtnBot
    static let buttonOffBorder = P.rgb(0x232327)
    static let buttonOffText = P.rgb(0x4E4E53)
    static let sliderTrack = P.borderSubtle
    static let inset = P.insetBg
    static let insetBorder = P.insetBorder

    // The Modes tab's slice switch (the board's `.mhead` and `.sliceseg`).
    static let modesHead = P.rgb(0x141418)
    static let sliceChosen = P.glassSelBot // glass, a picked choice (W3)
    static let sectionSubtitle = P.textScale

    // The notices on the band.
    static let notice = P.appBg.opacity(0.92)
    static let noticeWarn = P.noticeWarn
    static let noticeInfo = P.accentText

    // The toolbar's Pan and Display sheets (the board's `.dropsheet` and `.tbtn.is-open`).
    static let sheet = P.rgb(0x121215)
    static let sheetBorder = P.borderSubtle
    static let toolbarOpenText = P.glassSelText // glass
    static let toolbarOpenBackground = P.glassSelBot // glass

    // Transmit (the board's `.ptt`, `.txpill`, `.pan__tx`, `.opbtn` and HGauge colours).
    static let pttGround = P.badgeTxBg
    static let pttEdge = P.txRed
    static let pttText = P.keyedRim
    static let pttSub = P.redBorder
    static let txRed = P.txRed // keyed
    static let pttElsewhereGround = P.rgb(0x18181B)
    static let pttElsewhereEdge = P.rgb(0x5A3A3C)
    static let pttElsewhereText = P.redBorder
    static let pttElsewhereSub = P.textSecondary
    static let pttElsewhereAirSub = P.keyedRim
    // Kept (W2): the TX filter's orange, a band mark.
    static let txFilterFill = P.rgb(0xFF783C).opacity(0.18)
    static let txFilterEdge = P.rgb(0xFF7833)
    static let txPill = P.txRed.opacity(0.2)
    static let txPillBorder = P.txRed.opacity(0.45)
    static let txPillText = P.txRed
    static let txPillStop = P.redText
    static let operateOn = P.litGreenMid // lit green
    static let operateOnBorder = P.litGreenRim // lit green
    static let operateOff = P.glassBtnBot // raised
    static let operateOffBorder = P.border // raised
    static let gaugeGround = P.insetBg // inset
    static let gaugeBorder = P.insetBorder // inset
    static let gaugeNormal = P.accentFill
    static let gaugeWarn = P.noticeWarn
    static let gaugeRed = P.gaugeDanger // W5: over range is not transmit
    /// Kept (W2), a meter zone: the dark red ground of a stage meter past its middle.
    static let gaugeRedZone = P.rgb(0x1E0E12)
    static let refusalText = P.keyedRim
}
