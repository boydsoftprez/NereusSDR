// NereusSDR for iOS: the desktop's workspace tokens, the one place the app's chrome colors are written
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The desktop's workspace look on the phone
/// (docs/architecture/2026-10-20-phone-workspace-look-design.md, sections
/// 4 and 5). The desktop is the source of every value here: each token
/// carries the desktop's name, its line in `src/gui/styles/WorkspacePalette.h`
/// and the desktop's own cite for where the value came from, exactly as the
/// desktop writes it (JJ's licence ruling, 2026-10-21, spec section 11).
/// `tests/compliance/test_ios_workspace_palette.py` reads the token lines in
/// this form and fails when one drifts from the desktop.
///
/// This file and `WorkspaceStyles.swift` build into both the app and the
/// Live Activity extension (`ios/project.yml`, `Shared`). The role files
/// (`ChromeColours` and the rest) point at these values; a view reads a
/// role or a token and never repeats a value.
enum WorkspacePalette {
    // MARK: The desktop's tokens

    static let appBg = rgb(0x08080A) // kAppBg, WorkspacePalette.h:105; From Longpath src/gui/StyleConstants.h:90 [@551576e3]
    static let panelBg = rgb(0x0C0C0E) // kPanelBg, WorkspacePalette.h:107; From Longpath src/gui/StyleConstants.h:91 [@551576e3]
    static let statusBarBg = rgb(0x0A0A0C) // kStatusBarBg, WorkspacePalette.h:122; From Longpath src/gui/StyleConstants.h:640 [@551576e3]
    static let statusBarBorder = rgb(0x1F1F23) // kStatusBarBorder, WorkspacePalette.h:123; From Longpath src/gui/StyleConstants.h:641 [@551576e3]
    static let textPrimary = rgb(0xDCDCE1) // kTextPrimary, WorkspacePalette.h:150; From Longpath src/gui/StyleConstants.h:117 [@551576e3]
    static let textSecondary = rgb(0xA6A6AC) // kTextSecondary, WorkspacePalette.h:151; From Longpath src/gui/StyleConstants.h:118 [@551576e3]
    static let textTertiary = rgb(0x909096) // kTextTertiary, WorkspacePalette.h:152; From Longpath src/gui/StyleConstants.h:119 [@551576e3]
    static let textScale = rgb(0x7E7E85) // kTextScale, WorkspacePalette.h:153; From Longpath src/gui/StyleConstants.h:182 [@551576e3]
    static let textInactive = rgb(0x58585E) // kTextInactive, WorkspacePalette.h:154; From Longpath src/gui/StyleConstants.h:183 [@551576e3]
    static let titleText = rgb(0xC4C4CA) // kTitleText, WorkspacePalette.h:163; From Longpath src/gui/StyleConstants.h:192 [@551576e3]
    static let buttonBg = rgb(0x1A1A1E) // kButtonBg, WorkspacePalette.h:173; From Longpath src/gui/StyleConstants.h:200 [@551576e3]
    static let border = rgb(0x2C2C31) // kBorder, WorkspacePalette.h:179; From Longpath src/gui/StyleConstants.h:203 [@551576e3]
    static let borderSubtle = rgb(0x1F1F23) // kBorderSubtle, WorkspacePalette.h:181; From Longpath src/gui/StyleConstants.h:204 [@551576e3]
    static let borderMuted = rgb(0x3D3D41) // kBorderMuted, WorkspacePalette.h:185; From Longpath src/gui/StyleConstants.h:208 [@551576e3]
    static let insetBg = rgb(0x050507) // kInsetBg, WorkspacePalette.h:209; From Longpath src/gui/StyleConstants.h:245 [@551576e3]
    static let insetBorder = rgb(0x232329) // kInsetBorder, WorkspacePalette.h:214; From Longpath src/gui/StyleConstants.h:249 [@551576e3]
    static let greenBg = rgb(0x1C3A2A) // kGreenBg, WorkspacePalette.h:227; From Longpath src/gui/StyleConstants.h:259 [@551576e3]
    static let liveGreen = rgb(0x4CAF6A) // kLiveGreen, WorkspacePalette.h:233; From Longpath src/gui/StyleConstants.h:265 [@551576e3]
    static let amberBg = rgb(0x33280F) // kAmberBg, WorkspacePalette.h:263; From Longpath src/gui/StyleConstants.h:297 [@551576e3]
    static let amberText = rgb(0xD8A55F) // kAmberText, WorkspacePalette.h:264; From Longpath src/gui/StyleConstants.h:298 [@551576e3]
    static let amberBorder = rgb(0x6B5426) // kAmberBorder, WorkspacePalette.h:271; From Longpath src/gui/StyleConstants.h:305 [@551576e3]
    static let dspToggleBorder = rgb(0x33684C) // kDspToggleBorder, WorkspacePalette.h:412; From Longpath src/gui/StyleConstants.h:887 [@551576e3]
    static let badgeTxBg = rgb(0x3F2224) // kBadgeTxBg, WorkspacePalette.h:318; From Longpath src/gui/StyleConstants.h:354 [@551576e3]
    static let txRed = rgb(0xC25A5C) // kTxRed, WorkspacePalette.h:324; From Longpath src/gui/StyleConstants.h:359 [@551576e3]
    static let redBg = rgb(0x7A2C2E) // kRedBg, WorkspacePalette.h:325; From Longpath src/gui/StyleConstants.h:361 [@551576e3]
    static let redText = rgb(0xF0DCDC) // kRedText, WorkspacePalette.h:326; From Longpath src/gui/StyleConstants.h:362 [@551576e3]
    static let redBorder = rgb(0xA86B6D) // kRedBorder, WorkspacePalette.h:327; From Longpath src/gui/StyleConstants.h:363 [@551576e3]
    static let gaugeDanger = rgb(0xA86B6D) // kGaugeDanger, WorkspacePalette.h:328; From Longpath src/gui/StyleConstants.h:506 [@551576e3]
    static let glassPanelTop = rgb(0x17171B) // kGlassPanelTop, WorkspacePalette.h:348; From Longpath src/gui/StyleConstants.h:521 [@551576e3]
    static let glassPanelBot = rgb(0x0E0E10) // kGlassPanelBot, WorkspacePalette.h:350; From Longpath src/gui/StyleConstants.h:522 [@551576e3]
    static let glassBtnTop = rgb(0x222227) // kGlassBtnTop, WorkspacePalette.h:352; From Longpath src/gui/StyleConstants.h:523 [@551576e3]
    static let glassBtnBot = rgb(0x141417) // kGlassBtnBot, WorkspacePalette.h:354; From Longpath src/gui/StyleConstants.h:524 [@551576e3]
    static let glassSelTop = rgb(0x2F5F92) // kGlassSelTop, WorkspacePalette.h:356; From Longpath src/gui/StyleConstants.h:525 [@551576e3]
    static let glassSelBot = rgb(0x1E3D5F) // kGlassSelBot, WorkspacePalette.h:358; From Longpath src/gui/StyleConstants.h:526 [@551576e3]
    static let glassSelBorder = rgb(0x3D78B0) // kGlassSelBorder, WorkspacePalette.h:359; From Longpath src/gui/StyleConstants.h:527 [@551576e3]
    static let glassSelText = rgb(0xDBE9F8) // kGlassSelText, WorkspacePalette.h:360; From Longpath src/gui/StyleConstants.h:528 [@551576e3]

    /// The light line along a raised face's top: white at kGlassLightAlpha
    /// 18 of 255 (WorkspacePalette.h:362; From Longpath src/gui/StyleConstants.h:529 [@551576e3]).
    static let glassLight = Color.white.opacity(18.0 / 255)

    // MARK: Computed by the desktop

    // The desktop computes these with Qt's HSL arithmetic, shiftL(base, delta)
    // (src/gui/styles/WorkspaceStyle.cpp:211; From Longpath src/gui/StyleConstants.h:697-703 [@551576e3]),
    // and the raised fill raisedFillOf(base): top shiftL(base, +16), base at
    // 55 percent, bottom shiftL(base, -12) (WorkspaceComponents.cpp:1710-1726;
    // From Longpath src/gui/StyleConstants.h:705-718 [@551576e3]). The lit
    // buttons take their roles from greenCheckedStyle, dspToggleStyle and
    // amberCheckedStyle at Longpath src/gui/StyleConstants.h:799-804, :890-895
    // and :844-849 [@551576e3] (WorkspaceComponents.cpp:192-204); their raised
    // forms are JJ's. Written here as the literals the desktop computes.

    static let litGreenTop = rgb(0x306247) // shiftL(shiftL(kGreenBg, +14), +16), litGreenFill, WorkspaceComponents.cpp:243
    static let litGreenMid = rgb(0x254D38) // shiftL(kGreenBg, +14), litGreenFill, WorkspaceComponents.cpp:243
    static let litGreenBottom = rgb(0x1D3D2C) // shiftL(shiftL(kGreenBg, +14), -12), litGreenFill, WorkspaceComponents.cpp:243
    static let litGreenText = rgb(0xBBE1C6) // shiftL(kLiveGreen, +80), litGreenText, WorkspaceComponents.cpp:248
    static let litGreenRim = rgb(0x3E7E5C) // shiftL(kDspToggleBorder, +16), litGreenRim, WorkspaceComponents.cpp:253
    static let litGreenRimTop = rgb(0x48946B) // shiftL(litGreenRim, +16), litTreatment, WorkspaceComponents.cpp:1299
    static let litAmberTop = rgb(0x4C3B16) // shiftL(kAmberBg, +16), litAmberFill, WorkspaceComponents.cpp:258
    static let litAmberMid = rgb(0x33280F) // kAmberBg, litAmberFill, WorkspaceComponents.cpp:258
    static let litAmberBottom = rgb(0x20190A) // shiftL(kAmberBg, -12), litAmberFill, WorkspaceComponents.cpp:258
    static let litAmberRimTop = rgb(0x83672F) // shiftL(kAmberBorder, +16), litTreatment, WorkspaceComponents.cpp:1304
    // shiftL(kGlassSelBorder, +16), glassTreatment, WorkspaceComponents.cpp:1027; From Longpath src/gui/StyleConstants.h:815-823 [@551576e3]
    static let glassSelBorderTop = rgb(0x4D89C1)

    // MARK: The phone's own steps

    static let textBright = rgb(0xECECF0) // one step above kTextPrimary for headlines, spec section 13 point 1
    static let accentFill = rgb(0x4A7BA8) // fills and marks only (4.4 on kPanelBg), spec section 10 "Contrast"
    static let accentText = rgb(0x5E8DB8) // small text and notice information blue, spec sections 10 and 13 point 3
    static let tabChosen = rgb(0x8FB4D9) // the chosen tab and the Core chip's text, spec section 13 point 5
    static let noticeWarn = rgb(0xA8853F) // notice warning, spec section 13 point 3
    static let keyedRim = rgb(0xE0A3A4) // the keyed PTT's and UNKEY's rim, spec section 5 "Keyed"
    /// The keyed PTT's and UNKEY's text, spec section 5 "Keyed".
    static let keyedText = Color.white

    // MARK: Shapes and steps

    /// kGlassChipRadius (WorkspacePalette.h:368; From Longpath src/gui/StyleConstants.h:533 [@551576e3]).
    static let chipRadius: CGFloat = 7
    /// kGlassPanelRadius (WorkspacePalette.h:369; From Longpath src/gui/StyleConstants.h:534 [@551576e3]).
    static let panelRadius: CGFloat = 10
    /// kRaisedMidStopPercent, where raisedFillOf puts its base (WorkspaceComponents.cpp:190).
    static let raisedMidStop: CGFloat = 0.55
    /// A control that cannot act now, in place (SetupNumberRow.greyedOpacity).
    static let disabledOpacity = 0.4

    /// A color from `0xRRGGBB` in sRGB.
    static func rgb(_ value: UInt32) -> Color {
        Color(.sRGB,
              red: Double((value >> 16) & 0xFF) / 255,
              green: Double((value >> 8) & 0xFF) / 255,
              blue: Double(value & 0xFF) / 255,
              opacity: 1)
    }
}
