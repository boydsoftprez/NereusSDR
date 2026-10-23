// NereusSDR for iOS: tests of the workspace tokens, the treatments and the kept data colors
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-49 (tokens), R-IOS-50 (treatments) and R-IOS-54 (kept colors),
/// docs/architecture/2026-10-20-phone-workspace-look-design.md sections 4, 5
/// and 10. The desktop tokens' hex values are checked against the desktop's
/// header by tests/compliance/test_ios_workspace_palette.py.
@Suite("Workspace palette", .serialized)
@MainActor
struct WorkspacePaletteTests {
    /// A color's 0xRRGGBB in sRGB.
    static func hex(_ color: Color) -> UInt32 {
        var red: CGFloat = 0, green: CGFloat = 0, blue: CGFloat = 0, alpha: CGFloat = 0
        UIColor(color).getRed(&red, green: &green, blue: &blue, alpha: &alpha)
        func byte(_ value: CGFloat) -> UInt32 { UInt32((min(max(value, 0), 1) * 255).rounded()) }
        return byte(red) << 16 | byte(green) << 8 | byte(blue)
    }

    static func shown(_ value: UInt32) -> String { String(format: "#%06x", value) }

    @Test func txRedResolvesToTheDesktopsRed() {
        var red: CGFloat = 0, green: CGFloat = 0, blue: CGFloat = 0, alpha: CGFloat = 0
        UIColor(WorkspacePalette.txRed).getRed(&red, green: &green, blue: &blue, alpha: &alpha)
        #expect(Int((red * 255).rounded()) == 0xC2)
        #expect(Int((green * 255).rounded()) == 0x5A)
        #expect(Int((blue * 255).rounded()) == 0x5C)
        #expect(alpha == 1)
    }

    @Test func computedValuesEqualTheDesktopsComputation() {
        let expected: [(String, Color, UInt32)] = [
            ("litGreenTop", WorkspacePalette.litGreenTop, 0x306247),
            ("litGreenMid", WorkspacePalette.litGreenMid, 0x254D38),
            ("litGreenBottom", WorkspacePalette.litGreenBottom, 0x1D3D2C),
            ("litGreenText", WorkspacePalette.litGreenText, 0xBBE1C6),
            ("litGreenRim", WorkspacePalette.litGreenRim, 0x3E7E5C),
            ("litGreenRimTop", WorkspacePalette.litGreenRimTop, 0x48946B),
            ("litAmberTop", WorkspacePalette.litAmberTop, 0x4C3B16),
            ("litAmberMid", WorkspacePalette.litAmberMid, 0x33280F),
            ("litAmberBottom", WorkspacePalette.litAmberBottom, 0x20190A),
            ("litAmberRimTop", WorkspacePalette.litAmberRimTop, 0x83672F),
            ("glassSelBorderTop", WorkspacePalette.glassSelBorderTop, 0x4D89C1),
        ]
        for (name, color, value) in expected {
            #expect(Self.shown(Self.hex(color)) == Self.shown(value), "\(name)")
        }
    }

    @Test func phoneStepsAreTheSpecsValues() {
        let expected: [(String, Color, UInt32)] = [
            ("textBright", WorkspacePalette.textBright, 0xECECF0),
            ("accentFill", WorkspacePalette.accentFill, 0x4A7BA8),
            ("accentText", WorkspacePalette.accentText, 0x5E8DB8),
            ("tabChosen", WorkspacePalette.tabChosen, 0x8FB4D9),
            ("noticeWarn", WorkspacePalette.noticeWarn, 0xA8853F),
            ("keyedRim", WorkspacePalette.keyedRim, 0xE0A3A4),
        ]
        for (name, color, value) in expected {
            #expect(Self.shown(Self.hex(color)) == Self.shown(value), "\(name)")
        }
    }

    @Test func shapesAndSteps() {
        #expect(WorkspacePalette.chipRadius == 7)
        #expect(WorkspacePalette.panelRadius == 10)
        #expect(WorkspacePalette.raisedMidStop == 0.55)
        #expect(WorkspacePalette.disabledOpacity == 0.4)
        #expect(WorkspacePalette.disabledOpacity == SetupNumberRow.greyedOpacity)
    }

    /// R-IOS-50: each PanelButton style lights in its treatment; at rest and
    /// greyed it is raised (plan call 2), and a greyed lit button is raised too.
    @Test func panelButtonsDrawTheirTreatments() {
        let lit: [(PanelButton.Style, WorkspaceLook)] = [(.blue, .glass), (.dsp, .litGreen),
                                                         (.amber, .litAmber), (.red, .danger),
                                                         (.txAntenna, .keyed)]
        for (style, look) in lit {
            #expect(PanelButton.look(style: style, lit: true, disabled: false) == look)
            #expect(PanelButton.look(style: style, lit: false, disabled: false) == .raised)
            #expect(PanelButton.look(style: style, lit: true, disabled: true) == .raised)
            #expect(PanelButton.look(style: style, lit: false, disabled: true) == .raised)
        }
    }

    /// The other shared buttons: the connecting screens' wide button, the
    /// performance page's buttons and the WIDE chip.
    @Test func sharedButtonsDrawTheirTreatments() {
        typealias Wide = ConnectChrome.WideButton
        #expect(Wide.face(.go, enabled: true) == .glass)
        #expect(Wide.face(.plain, enabled: true) == .raised)
        #expect(Wide.face(.stop, enabled: true) == .danger)
        for look in [Wide.Look.go, .plain, .stop] {
            #expect(Wide.face(look, enabled: false) == .raised)
        }
        #expect(PerformanceButtonStyle.look == .raised)
        #expect(WideChip.look == .litAmber)
    }

    @Test func lookForegrounds() {
        #expect(Self.hex(workspaceForeground(.raised)) == Self.hex(WorkspacePalette.textPrimary))
        #expect(Self.hex(workspaceForeground(.glass)) == Self.hex(WorkspacePalette.glassSelText))
        #expect(Self.hex(workspaceForeground(.litGreen)) == 0xBBE1C6)
        #expect(Self.hex(workspaceForeground(.litAmber)) == 0xD8A55F)
        #expect(Self.hex(workspaceForeground(.keyed)) == 0xFFFFFF)
        #expect(Self.hex(workspaceForeground(.danger)) == 0xF0DCDC)
    }

    // MARK: Kept colors (W2, R-IOS-54)

    @Test func keptColorsDoNotChange() {
        let expected: [(String, Color, UInt32)] = [
            ("BandColours.frequency", BandColours.frequency, 0x00E5FF),
            ("BandColours.bandwidth", BandColours.bandwidth, 0x00C8FF),
            ("BandColours.rxAntenna", BandColours.rxAntenna, 0x4488FF),
            ("BandColours.timestamp", BandColours.timestamp, 0xC8DCFF),
            ("DialColours.indexGlow", DialColours.indexGlow, 0x00E5FF),
            ("DialColours.indexTick", DialColours.indexTick, 0xC8F8FF),
            ("ChromeColours.txFilterEdge", ChromeColours.txFilterEdge, 0xFF7833),
        ]
        for (name, color, value) in expected {
            #expect(Self.shown(Self.hex(color)) == Self.shown(value), "\(name)")
        }
    }

    // MARK: The role files (R-IOS-49, appendix A)

    /// The roles the plan names, each the token itself.
    @Test func rolesPointAtTheTokens() {
        typealias P = WorkspacePalette
        let expected: [(String, Color, Color)] = [
            ("ChromeColours.bar", ChromeColours.bar, P.statusBarBg),
            ("ChromeColours.page", ChromeColours.page, P.appBg),
            ("ChromeColours.text", ChromeColours.text, P.textPrimary),
            ("ChromeColours.textDim", ChromeColours.textDim, P.textSecondary),
            ("ChromeColours.accent", ChromeColours.accent, P.accentFill),
            ("ChromeColours.linkLost", ChromeColours.linkLost, P.redBorder),
            ("ChromeColours.gaugeRed", ChromeColours.gaugeRed, P.gaugeDanger),
            ("ChromeColours.pttEdge", ChromeColours.pttEdge, P.txRed),
            ("ChromeColours.noticeInfo", ChromeColours.noticeInfo, P.accentText),
            ("BandColours.txAntenna", BandColours.txAntenna, P.txRed),
            ("BandColours.levelHigh", BandColours.levelHigh, P.liveGreen),
            ("DialColours.index", DialColours.index, P.accentFill),
            ("ConnectChrome.pillOn", ConnectChrome.pillOn, P.liveGreen),
            ("ConnectChrome.pillStale", ConnectChrome.pillStale, P.noticeWarn),
            ("ConnectChrome.pillOff", ConnectChrome.pillOff, P.redBorder),
            ("ConnectChrome.pillUnknown", ConnectChrome.pillUnknown, P.borderMuted),
            // A treatment's role holds its flat middle color.
            ("ChromeColours.buttonOnBlue", ChromeColours.buttonOnBlue, P.glassSelBot),
            ("ChromeColours.iconOpenBackground", ChromeColours.iconOpenBackground, P.glassSelBot),
            ("ChromeColours.sliceChosen", ChromeColours.sliceChosen, P.glassSelBot),
            ("ChromeColours.buttonOnGreen", ChromeColours.buttonOnGreen, P.litGreenMid),
            ("ChromeColours.buttonOnAmber", ChromeColours.buttonOnAmber, P.litAmberMid),
            ("ChromeColours.button", ChromeColours.button, P.glassBtnBot),
            ("ChromeColours.txRed", ChromeColours.txRed, P.txRed),
        ]
        for (name, role, token) in expected {
            #expect(role == token, "\(name)")
        }
    }

    /// Every role appendix A gives, at the appendix's workspace value (or a
    /// treatment's flat middle color).
    @Test func rolesHoldTheAppendixValues() {
        let expected: [(String, Color, UInt32)] = [
            ("ChromeColours.bar", ChromeColours.bar, 0x0A0A0C),
            ("ChromeColours.barBorder", ChromeColours.barBorder, 0x1F1F23),
            ("ChromeColours.text", ChromeColours.text, 0xDCDCE1),
            ("ChromeColours.textBright", ChromeColours.textBright, 0xECECF0),
            ("ChromeColours.textDim", ChromeColours.textDim, 0xA6A6AC),
            ("ChromeColours.textFaint", ChromeColours.textFaint, 0x7E7E85),
            ("ChromeColours.accent", ChromeColours.accent, 0x4A7BA8),
            ("ChromeColours.accentText", ChromeColours.accentText, 0x5E8DB8),
            ("ChromeColours.icon", ChromeColours.icon, 0xC4C4CA),
            ("ChromeColours.iconMuted", ChromeColours.iconMuted, 0x58585E),
            ("ChromeColours.chip", ChromeColours.chip, 0x050507),
            ("ChromeColours.linkUp", ChromeColours.linkUp, 0x4CAF6A),
            ("ChromeColours.linkUpPulse", ChromeColours.linkUpPulse, 0x4CAF6A),
            ("ChromeColours.linkUpText", ChromeColours.linkUpText, 0x6FA384),
            ("ChromeColours.linkLost", ChromeColours.linkLost, 0xA86B6D),
            ("ChromeColours.linkOffDot", ChromeColours.linkOffDot, 0x3D3D41),
            ("ChromeColours.stationBorder", ChromeColours.stationBorder, 0x4A7BA8),
            ("ChromeColours.page", ChromeColours.page, 0x08080A),
            ("ChromeColours.revoke", ChromeColours.revoke, 0x3F2224),
            ("ChromeColours.revokeBorder", ChromeColours.revokeBorder, 0x7A2C2E),
            ("ChromeColours.revokeText", ChromeColours.revokeText, 0xF0DCDC),
            ("ChromeColours.panelEdge", ChromeColours.panelEdge, 0x2C2C31),
            ("ChromeColours.titleTop", ChromeColours.titleTop, 0x26262B),
            ("ChromeColours.titleMiddle", ChromeColours.titleMiddle, 0x26262B),
            ("ChromeColours.titleBottom", ChromeColours.titleBottom, 0x141417),
            ("ChromeColours.titleBorder", ChromeColours.titleBorder, 0x0D0D0F),
            ("ChromeColours.caption", ChromeColours.caption, 0x909096),
            ("ChromeColours.button", ChromeColours.button, 0x141417),
            ("ChromeColours.buttonBorder", ChromeColours.buttonBorder, 0x2C2C31),
            ("ChromeColours.buttonOnBlue", ChromeColours.buttonOnBlue, 0x1E3D5F),
            ("ChromeColours.buttonOnBlueBorder", ChromeColours.buttonOnBlueBorder, 0x3D78B0),
            ("ChromeColours.buttonOnGreen", ChromeColours.buttonOnGreen, 0x254D38),
            ("ChromeColours.buttonOnGreenBorder", ChromeColours.buttonOnGreenBorder, 0x3E7E5C),
            ("ChromeColours.buttonOnGreenText", ChromeColours.buttonOnGreenText, 0xBBE1C6),
            ("ChromeColours.buttonOnAmber", ChromeColours.buttonOnAmber, 0x33280F),
            ("ChromeColours.buttonOnAmberBorder", ChromeColours.buttonOnAmberBorder, 0x6B5426),
            ("ChromeColours.buttonOnAmberText", ChromeColours.buttonOnAmberText, 0xD8A55F),
            ("ChromeColours.buttonOff", ChromeColours.buttonOff, 0x141417),
            ("ChromeColours.buttonOffBorder", ChromeColours.buttonOffBorder, 0x232327),
            ("ChromeColours.buttonOffText", ChromeColours.buttonOffText, 0x4E4E53),
            ("ChromeColours.sliderTrack", ChromeColours.sliderTrack, 0x1F1F23),
            ("ChromeColours.inset", ChromeColours.inset, 0x050507),
            ("ChromeColours.insetBorder", ChromeColours.insetBorder, 0x232329),
            ("ChromeColours.modesHead", ChromeColours.modesHead, 0x141418),
            ("ChromeColours.sectionSubtitle", ChromeColours.sectionSubtitle, 0x7E7E85),
            ("ChromeColours.notice", ChromeColours.notice, 0x08080A),
            ("ChromeColours.noticeWarn", ChromeColours.noticeWarn, 0xA8853F),
            ("ChromeColours.noticeInfo", ChromeColours.noticeInfo, 0x5E8DB8),
            ("ChromeColours.sheet", ChromeColours.sheet, 0x121215),
            ("ChromeColours.sheetBorder", ChromeColours.sheetBorder, 0x1F1F23),
            ("ChromeColours.pttGround", ChromeColours.pttGround, 0x3F2224),
            ("ChromeColours.pttEdge", ChromeColours.pttEdge, 0xC25A5C),
            ("ChromeColours.pttText", ChromeColours.pttText, 0xE0A3A4),
            ("ChromeColours.pttSub", ChromeColours.pttSub, 0xA86B6D),
            ("ChromeColours.txRed", ChromeColours.txRed, 0xC25A5C),
            ("ChromeColours.pttElsewhereGround", ChromeColours.pttElsewhereGround, 0x18181B),
            ("ChromeColours.pttElsewhereEdge", ChromeColours.pttElsewhereEdge, 0x5A3A3C),
            ("ChromeColours.pttElsewhereText", ChromeColours.pttElsewhereText, 0xA86B6D),
            ("ChromeColours.pttElsewhereSub", ChromeColours.pttElsewhereSub, 0xA6A6AC),
            ("ChromeColours.pttElsewhereAirSub", ChromeColours.pttElsewhereAirSub, 0xE0A3A4),
            ("ChromeColours.txFilterFill", ChromeColours.txFilterFill, 0xFF783C),
            ("ChromeColours.txPill", ChromeColours.txPill, 0xC25A5C),
            ("ChromeColours.txPillBorder", ChromeColours.txPillBorder, 0xC25A5C),
            ("ChromeColours.txPillText", ChromeColours.txPillText, 0xC25A5C),
            ("ChromeColours.txPillStop", ChromeColours.txPillStop, 0xF0DCDC),
            ("ChromeColours.gaugeNormal", ChromeColours.gaugeNormal, 0x4A7BA8),
            ("ChromeColours.gaugeWarn", ChromeColours.gaugeWarn, 0xA8853F),
            ("ChromeColours.gaugeRed", ChromeColours.gaugeRed, 0xA86B6D),
            ("ChromeColours.refusalText", ChromeColours.refusalText, 0xE0A3A4),
            ("BandColours.flagBackground", BandColours.flagBackground, 0x0A0A0C),
            ("BandColours.text", BandColours.text, 0xDCDCE1),
            ("BandColours.txAntenna", BandColours.txAntenna, 0xC25A5C),
            ("BandColours.tab", BandColours.tab, 0x828288),
            ("BandColours.txBadgeOff", BandColours.txBadgeOff, 0x1A1A1E),
            ("BandColours.txBadgeOffBorder", BandColours.txBadgeOffBorder, 0x2C2C31),
            ("BandColours.txBadgeOffText", BandColours.txBadgeOffText, 0x828288),
            ("BandColours.txBadgeOn", BandColours.txBadgeOn, 0x3F2224),
            ("BandColours.txBadgeOnBorder", BandColours.txBadgeOnBorder, 0xC25A5C),
            ("BandColours.txBadgeOnText", BandColours.txBadgeOnText, 0xE0A3A4),
            ("BandColours.buttonBorder", BandColours.buttonBorder, 0x69696D),
            ("BandColours.levelTicks", BandColours.levelTicks, 0x828288),
            ("BandColours.levelBackground", BandColours.levelBackground, 0x050507),
            ("BandColours.levelBorder", BandColours.levelBorder, 0x2C2C31),
            ("BandColours.levelLow", BandColours.levelLow, 0x4A7BA8),
            ("BandColours.levelHigh", BandColours.levelHigh, 0x4CAF6A),
            ("BandColours.zoomBackground", BandColours.zoomBackground, 0x202024),
            ("BandColours.radeRow", BandColours.radeRow, 0x4A7BA8),
            ("BandColours.radeRowText", BandColours.radeRowText, 0x5E8DB8),
            ("BandColours.radeGood", BandColours.radeGood, 0x6FA384),
            ("BandColours.radeOff", BandColours.radeOff, 0x828288),
            ("BandColours.radeMarginal", BandColours.radeMarginal, 0xE0E040),
            ("BandColours.radeMode", BandColours.radeMode, 0xA78BFA),
            ("BandColours.readoutBackground", BandColours.readoutBackground, 0x18181C),
            ("BandColours.cursorBackground", BandColours.cursorBackground, 0x18181C),
            ("BandColours.awayMarkBackground", BandColours.awayMarkBackground, 0x0A0A0C),
            ("BandColours.awayMarkEdge", BandColours.awayMarkEdge, 0x58585E),
            ("BandColours.awayMarkText", BandColours.awayMarkText, 0xA6A6AC),
            ("DialColours.knob", DialColours.knob, 0x0A0A0C),
            ("DialColours.gripLight", DialColours.gripLight, 0x46464A),
            ("DialColours.gripMiddle", DialColours.gripMiddle, 0x2A2A2E),
            ("DialColours.gripDark", DialColours.gripDark, 0x1C1C20),
            ("DialColours.index", DialColours.index, 0x4A7BA8),
            ("DialColours.buttonOn", DialColours.buttonOn, 0x1E3D5F),
            ("DialColours.stepBackground", DialColours.stepBackground, 0x050507),
            ("DialColours.sheet", DialColours.sheet, 0x0C0C0E),
            // W5 wins over the board's table for a red that is not transmit.
            ("ConnectChrome.bad", ConnectChrome.bad, 0xA86B6D),
            ("ConnectChrome.badBorder", ConnectChrome.badBorder, 0xA86B6D),
            ("ConnectChrome.badText", ConnectChrome.badText, 0xA86B6D),
            ("ConnectChrome.WideButton.stopFill", ConnectChrome.WideButton.stopFill, 0x7A2C2E),
            ("ConnectChrome.WideButton.stopBorder", ConnectChrome.WideButton.stopBorder, 0xA86B6D),
            ("ConnectChrome.rowFill", ConnectChrome.rowFill, 0x141418),
        ]
        for (name, color, value) in expected {
            #expect(Self.shown(Self.hex(color)) == Self.shown(value), "\(name)")
        }
    }

    /// No role outside the transmit set is the transmit red (W5).
    @Test func onlyTransmitRolesAreTheTransmitRed() {
        let transmit: Set<String> = ["ChromeColours.pttEdge", "ChromeColours.txRed", "ChromeColours.txPill",
                                     "ChromeColours.txPillBorder", "ChromeColours.txPillText",
                                     "BandColours.txAntenna", "BandColours.txBadgeOnBorder"]
        let roles: [(String, Color)] = [
            ("ChromeColours.pttEdge", ChromeColours.pttEdge), ("ChromeColours.txRed", ChromeColours.txRed),
            ("ChromeColours.txPill", ChromeColours.txPill), ("ChromeColours.txPillBorder", ChromeColours.txPillBorder),
            ("ChromeColours.txPillText", ChromeColours.txPillText), ("BandColours.txAntenna", BandColours.txAntenna),
            ("BandColours.txBadgeOnBorder", BandColours.txBadgeOnBorder),
            ("ChromeColours.linkLost", ChromeColours.linkLost), ("ChromeColours.gaugeRed", ChromeColours.gaugeRed),
            ("ChromeColours.refusalText", ChromeColours.refusalText), ("ChromeColours.revoke", ChromeColours.revoke),
            ("ConnectChrome.bad", ConnectChrome.bad), ("ConnectChrome.badBorder", ConnectChrome.badBorder),
            ("ConnectChrome.badText", ConnectChrome.badText), ("ConnectChrome.pillOff", ConnectChrome.pillOff),
            ("ConnectChrome.WideButton.stopFill", ConnectChrome.WideButton.stopFill),
            ("ConnectChrome.WideButton.stopBorder", ConnectChrome.WideButton.stopBorder),
            ("ConfirmationSheetChrome.refused", ConfirmationSheetChrome.refused),
            ("ModMonitorView.redLamp.ground", ModMonitorView.redLamp.ground),
            ("ModMonitorView.redLamp.border", ModMonitorView.redLamp.border),
            ("ModMonitorView.redLamp.text", ModMonitorView.redLamp.text),
        ]
        let red = Self.hex(WorkspacePalette.txRed)
        for (name, color) in roles {
            #expect((Self.hex(color) == red) == transmit.contains(name), "\(name)")
        }
        // The Mod Monitor's over-range lamps are the muted red set
        // (src/gui/applets/ModMonitorApplet.cpp:630, :634, :640).
        #expect(Self.hex(ModMonitorView.redLamp.ground) == 0x7A2C2E)
        #expect(Self.hex(ModMonitorView.redLamp.border) == 0xA86B6D)
        #expect(Self.hex(ModMonitorView.redLamp.text) == 0xF0DCDC)
    }

    /// Each slice letter's color as the Core sends it today (ControlRanges
    /// kSliceColours, src/core/ControlRanges.h:1041-1050), and the neutral
    /// grey for a slice the catalogue has no color for.
    @Test func sliceColorsParseAsToday() {
        let today: [(String, UInt32)] = [
            ("A", 0x00D4FF), ("B", 0xFF40FF), ("C", 0x40FF40), ("D", 0xFFFF00),
            ("E", 0xFFA000), ("F", 0x00E0C0), ("G", 0xFF6080), ("H", 0xB080FF),
        ]
        let colours = today.map { "#" + String(format: "%06X", $0.1) }
        for (index, (letter, value)) in today.enumerated() {
            #expect(BandSlice.letter(forIndex: index) == letter)
            let hex = colours[index]
            #expect(BandSlice.colour(forIndex: index, in: colours) == hex)
            #expect(Self.shown(Self.hex(BandColours.slice(hex))) == Self.shown(value), "slice \(letter)")
        }
        #expect(Self.hex(BandColours.slice(BandSlice.colourUnknown)) == 0x8090A0)
        #expect(Self.hex(BandColours.slice("not a colour")) == 0x8090A0)
    }

    // MARK: The switch

    /// The switch's binding, changed from outside the view.
    @MainActor final class SwitchState: ObservableObject {
        @Published var isOn = false
    }

    private struct SwitchRoot: View {
        @ObservedObject var state: SwitchState
        let style: Bool

        var body: some View {
            Group {
                if style {
                    Toggle("Mute", isOn: $state.isOn).toggleStyle(WorkspaceSwitchStyle())
                } else {
                    Toggle("Mute", isOn: $state.isOn).toggleStyle(.switch)
                }
            }
            .accessibilityIdentifier("workspaceSwitch")
            .padding(.horizontal, 16)
            .frame(width: 320, height: 60)
            .background(WorkspacePalette.panelBg)
        }
    }

    /// Hosts the switch in a window and runs `body` with the window.
    private func hosted(style: Bool, state: SwitchState, _ body: (UIWindow) async throws -> Void) async throws {
        let size = CGSize(width: 320, height: 60)
        let window = try BandFlagShotTests.window(size: size)
        let host = UIHostingController(rootView: SwitchRoot(state: state, style: style).preferredColorScheme(.dark))
        // The window is shorter than the phone's safe area; lay out in all of it.
        host.safeAreaRegions = []
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        try await body(window)
    }

    /// How many of the window's drawn pixels are the inset's ground and how
    /// many lie in the glass, eight bits a channel at one pixel a point.
    private func faceCounts(_ window: UIWindow) -> (inset: Int, glass: Int) {
        let format = UIGraphicsImageRendererFormat()
        format.preferredRange = .standard
        format.scale = 1
        let image = UIGraphicsImageRenderer(bounds: window.bounds, format: format).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        guard let cgImage = image.cgImage else {
            return (0, 0)
        }
        let width = cgImage.width, height = cgImage.height
        var bytes = [UInt8](repeating: 0, count: width * height * 4)
        guard let context = CGContext(data: &bytes, width: width, height: height, bitsPerComponent: 8,
                                      bytesPerRow: width * 4,
                                      space: CGColorSpace(name: CGColorSpace.sRGB) ?? CGColorSpaceCreateDeviceRGB(),
                                      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
            return (0, 0)
        }
        context.draw(cgImage, in: CGRect(x: 0, y: 0, width: width, height: height))
        let inset = Self.hex(WorkspacePalette.insetBg)
        var counts = (inset: 0, glass: 0)
        for index in stride(from: 0, to: bytes.count, by: 4) {
            let value = UInt32(bytes[index]) << 16 | UInt32(bytes[index + 1]) << 8 | UInt32(bytes[index + 2])
            if value == inset {
                counts.inset += 1
            } else if Self.isGlass(value) {
                counts.glass += 1
            }
        }
        return counts
    }

    /// The switch reads to VoiceOver as the stock switch does: the same
    /// traits and the same on and off value.
    @Test func switchKeepsTheStockSwitchsAccessibility() async throws {
        // SwiftUI builds its accessibility elements only while application
        // accessibility is on; turn it on as SetupTypedEntryTests.onScreen
        // does, and put it back as found (it outlives the test process).
        let wasOn = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(wasOn) }
        for isOn in [false, true] {
            let stockState = SwitchState(), styledState = SwitchState()
            stockState.isOn = isOn
            styledState.isOn = isOn
            var stock: (UIAccessibilityTraits, String?) = ([], nil)
            try await hosted(style: false, state: stockState) { window in
                let element = try #require(SetupTypedEntryTests.element("workspaceSwitch", in: window))
                stock = (element.accessibilityTraits, element.accessibilityValue)
            }
            try await hosted(style: true, state: styledState) { window in
                let element = try #require(SetupTypedEntryTests.element("workspaceSwitch", in: window))
                #expect(element.accessibilityTraits == stock.0, "on \(isOn)")
                #expect(element.accessibilityValue == stock.1, "on \(isOn)")
                #expect(element.accessibilityLabel == "Mute")
                #expect(element.accessibilityTraits.contains(.toggleButton) ==
                        stock.0.contains(.toggleButton))
            }
            #expect(stock.1 == (isOn ? "1" : "0"))
        }
    }

    /// Flipping the binding moves the face between the inset (off) and the
    /// glass (on), counted over the whole window: the ground is kPanelBg
    /// and the label kTextPrimary, so nothing else is drawn in either.
    @Test func switchFaceFollowsTheBinding() async throws {
        let state = SwitchState()
        // A track of 51 by 31 less its knob and rim leaves well over 200 pixels of face.
        let face = 200
        try await hosted(style: true, state: state) { window in
            let off = faceCounts(window)
            #expect(off.inset > face && off.glass == 0, "off: \(off)")

            // The knob slides over 0.2 s; wait for the face to settle.
            state.isOn = true
            // A few edge pixels on the glass's dark end may round to the
            // inset's ground; the inset's face is gone.
            _ = await ShotWait.until(seconds: 3) { faceCounts(window).glass > face }
            let on = faceCounts(window)
            #expect(on.glass > face && on.inset * 10 < off.inset, "on: \(on), off: \(off)")

            state.isOn = false
            _ = await ShotWait.until(seconds: 3) { faceCounts(window).glass == 0 }
            let offAgain = faceCounts(window)
            #expect(offAgain.inset > face && offAgain.glass == 0, "off again: \(offAgain)")
        }
    }

    /// Between the glass's two ends, channel by channel.
    static func isGlass(_ value: UInt32) -> Bool {
        let top = hex(WorkspacePalette.glassSelTop), bottom = hex(WorkspacePalette.glassSelBot)
        return [16, 8, 0].allSatisfy { shift in
            let channel = (value >> UInt32(shift)) & 0xFF
            let a = (top >> UInt32(shift)) & 0xFF, b = (bottom >> UInt32(shift)) & 0xFF
            return channel >= min(a, b) && channel <= max(a, b)
        }
    }
}
