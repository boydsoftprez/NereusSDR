// NereusSDR for iOS: the colours of the toolbar, the tab bar and the panels, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The colours around the band: the toolbar, the tab bar, the RX panel and
/// the notices, as the board draws them
/// (docs/architecture/2026-09-23-iphone-app-design/board.html: the `.tbar`,
/// `.tabbar`, `.drawer`, `.nbtn`, `.af` and `.toast` styles). NereusSDR
/// ships one dark palette.
enum ChromeColours {
    // The bars.
    static let bar = rgb(0x0A, 0x0A, 0x14)
    static let barBorder = rgb(0x20, 0x30, 0x40)
    static let text = rgb(0xC8, 0xD8, 0xE8)
    static let textBright = rgb(0xE4, 0xEE, 0xF8)
    static let textDim = rgb(0x80, 0x90, 0xA0)
    static let textFaint = rgb(0x60, 0x70, 0x80)
    static let accent = rgb(0x00, 0xB4, 0xD8)
    static let icon = rgb(0x8A, 0xA8, 0xC0)
    static let iconMuted = rgb(0x40, 0x50, 0x60)
    static let iconOpenBackground = rgb(0x1A, 0x2A, 0x3A)
    static let iconOpenBorder = rgb(0x20, 0x50, 0x70)

    // The link chip.
    static let chip = rgb(0x0F, 0x14, 0x20)
    static let linkUp = rgb(0x5F, 0xFF, 0x8A)
    static let linkUpPulse = rgb(0x3F, 0xCF, 0x6A)
    static let linkUpText = rgb(0xA0, 0xD8, 0xA0)
    static let linkLost = rgb(0xC1, 0x48, 0x48)
    static let linkOffDot = rgb(0x40, 0x48, 0x58)

    // The Core's name, sideways, and the Radio tab's Core card (the board's `.stn`).
    static let stationBorder = rgb(0x00, 0xB4, 0xD8).opacity(0.314)
    // A setup page's ground (the board's `.scr`), and its ending button (`.revoke`).
    static let page = rgb(0x0F, 0x0F, 0x1A)
    static let revoke = rgb(0x4A, 0x20, 0x20)
    static let revokeBorder = rgb(0x6A, 0x30, 0x30)
    static let revokeText = rgb(0xC8, 0xA8, 0xA8)

    // The panels.
    static let panel = rgb(0x0A, 0x0A, 0x18)
    static let panelEdge = rgb(0x30, 0x40, 0x50)
    static let scrim = Color.black.opacity(0.35)
    static let titleTop = rgb(0x3A, 0x4A, 0x5A)
    static let titleMiddle = rgb(0x2A, 0x3A, 0x4A)
    static let titleBottom = rgb(0x1A, 0x2A, 0x38)
    static let titleBorder = rgb(0x0A, 0x1A, 0x28)
    static let caption = rgb(0x70, 0x80, 0x90)
    static let button = rgb(0x1A, 0x2A, 0x3A)
    static let buttonBorder = rgb(0x20, 0x50, 0x70)
    static let buttonOnBlue = rgb(0x00, 0x70, 0xC0)
    static let buttonOnBlueBorder = rgb(0x00, 0x90, 0xE0)
    static let buttonOnGreen = rgb(0x1A, 0x60, 0x30)
    static let buttonOnGreenBorder = rgb(0x20, 0xA0, 0x40)
    static let buttonOnGreenText = rgb(0x80, 0xFF, 0x80)
    static let buttonOnAmber = rgb(0x60, 0x40, 0x00)
    static let buttonOnAmberBorder = rgb(0x90, 0x60, 0x00)
    static let buttonOnAmberText = rgb(0xFF, 0xB8, 0x00)
    static let buttonOff = rgb(0x1A, 0x1A, 0x2A)
    static let buttonOffBorder = rgb(0x2A, 0x30, 0x40)
    static let buttonOffText = rgb(0x55, 0x60, 0x70)
    static let sliderTrack = rgb(0x20, 0x30, 0x40)
    static let inset = rgb(0x0A, 0x0A, 0x18)
    static let insetBorder = rgb(0x1E, 0x2E, 0x3E)

    // The Modes tab's slice switch (the board's `.mhead` and `.sliceseg`).
    static let modesHead = rgb(0x0D, 0x1B, 0x28)
    static let sliceChosen = rgb(0x10, 0x28, 0x38)
    static let sectionSubtitle = rgb(0x60, 0x70, 0x80)

    // The notices on the band.
    static let notice = rgb(0x0F, 0x0F, 0x1A).opacity(0.92)
    static let noticeWarn = rgb(0xDD, 0xBB, 0x00)
    static let noticeInfo = rgb(0x5F, 0xA8, 0xFF)

    // The toolbar's Pan and Display sheets (the board's `.dropsheet` and `.tbtn.is-open`).
    static let sheet = rgb(0x0F, 0x1A, 0x26)
    static let sheetBorder = rgb(0x20, 0x30, 0x40)
    static let toolbarOpenText = rgb(0x00, 0xE5, 0xFF)
    static let toolbarOpenBackground = rgb(0x00, 0xB4, 0xD8).opacity(0.14)

    // Transmit (the board's `.ptt`, `.txpill`, `.pan__tx`, `.opbtn` and HGauge colours).
    static let pttGround = rgb(0x3A, 0x1C, 0x20)
    static let pttEdge = rgb(0xFF, 0x44, 0x44)
    static let pttText = rgb(0xFF, 0x80, 0x80)
    static let pttSub = rgb(0xB0, 0x78, 0x80)
    static let txRed = rgb(0xCC, 0x22, 0x22)
    static let pttElsewhereGround = rgb(0x1C, 0x1A, 0x24)
    static let pttElsewhereEdge = rgb(0x6A, 0x40, 0x48)
    static let pttElsewhereText = rgb(0xA0, 0x70, 0x78)
    static let pttElsewhereSub = rgb(0x80, 0x90, 0xA0)
    static let pttElsewhereAirSub = rgb(0xD0, 0x90, 0x98)
    static let txFilterFill = rgb(0xFF, 0x78, 0x3C).opacity(0.18)
    static let txFilterEdge = rgb(0xFF, 0x78, 0x33)
    static let txPill = rgb(0xFF, 0x60, 0x60).opacity(0.2)
    static let txPillBorder = rgb(0xFF, 0x60, 0x60).opacity(0.45)
    static let txPillText = rgb(0xFF, 0x60, 0x60)
    static let txPillStop = rgb(0xFF, 0xD0, 0xD0)
    static let operateOn = rgb(0x00, 0x60, 0x30)
    static let operateOnBorder = rgb(0x00, 0x80, 0x40)
    static let operateOff = rgb(0x1A, 0x3A, 0x5A)
    static let operateOffBorder = rgb(0x20, 0x50, 0x70)
    static let gaugeGround = rgb(0x0A, 0x0A, 0x18)
    static let gaugeBorder = rgb(0x20, 0x30, 0x40)
    static let gaugeNormal = rgb(0x00, 0xB4, 0xD8)
    static let gaugeWarn = rgb(0xDD, 0xBB, 0x00)
    static let gaugeRed = rgb(0xFF, 0x44, 0x44)
    static let refusalText = rgb(0xFF, 0x80, 0x80)

    private static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }
}
