// NereusSDR for iOS: Roundtable's first-run question, "Roundtable is public", and the note when another device answered
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, the phone's first-run sheet, in the
// words of spec 11.2 and the desktop's RoundtableFirstRunDialog.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import SwiftUI

/// The first time the station opens Roundtable (spec 11.2 and 11.3, D21):
/// what goes public, Hide my station, and Stay hidden or Go live, the
/// switch moving the main button between them; either button answers.
/// Nothing of the station is sent before the answer: the Core holds the
/// question, asked once per station on any of its devices.
struct RoundtableFirstRunSheet: View {
    @ObservedObject var roundtable: RoundtableModel
    /// The station's callsign and grid; empty leaves them out.
    let callsign: String
    let grid: String
    /// The answer went in.
    let answered: () -> Void

    @State private var hide = false
    @State private var sent = false
    @State private var error: String?

    static let title = "Roundtable is public"
    static let intro = "Roundtable shows who hears whom, live, to everyone on it and on its web page. "
        + "With your station on, they see:"
    static let readingsItem = "Your radio's reading of each talker, and how far you are from them"
    static let youItem = "Every station's reading of you while you talk"
    static let messagesItem = "Messages you post on your frequency"
    static let archive = "Your archive of who heard you stays private to you."
    static let hideTitle = "Hide my station"
    static let hideText = "You still see everything. Your station and your readings are not sent. "
        + "Others can still hear you, and may name you when you talk."
    static let stayHidden = "Stay hidden"
    static let goLive = "Go live"
    static let foot = "Asked once for this station, on any of its devices. Change it any time in "
        + "Roundtable settings."
    static let notRunning = "Roundtable is not running here."

    /// The first item: "Your callsign CALL and grid GRID", or what is known of them.
    static func whoItem(callsign: String, grid: String) -> (lead: String, call: String?, middle: String?,
                                                            grid: String?) {
        let call = callsign.uppercased()
        if call.isEmpty {
            return ("Your callsign and grid", nil, nil, nil)
        }
        if grid.isEmpty {
            return ("Your callsign ", call, nil, nil)
        }
        return ("Your callsign ", call, " and grid ", grid)
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 10) {
                Text(Self.title)
                    .font(.system(size: 16, weight: .bold))
                    .foregroundStyle(RoundtableColours.bright)
                    .accessibilityAddTraits(.isHeader)
                    .accessibilityIdentifier("roundtableFirstRunTitle")
                Text(Self.intro)
                    .font(.system(size: 12))
                    .foregroundStyle(RoundtableColours.text)
                    .fixedSize(horizontal: false, vertical: true)
                VStack(alignment: .leading, spacing: 0) {
                    item(dot: RoundtableColours.you, ring: RoundtableColours.you) { whoText }
                    item(dot: RoundtableColours.talking, ring: RoundtableColours.talking) {
                        Text(Self.readingsItem)
                    }
                    item(dot: Color(red: 0x33 / 255, green: 0x28 / 255, blue: 0x0F / 255),
                         ring: RoundtableColours.unknown) { Text(Self.youItem) }
                    item(dot: .clear, ring: Color(red: 0x90 / 255, green: 0x90 / 255, blue: 0x96 / 255)) {
                        Text(Self.messagesItem)
                    }
                }
                Text(Self.archive)
                    .font(.system(size: 12))
                    .foregroundStyle(RoundtableColours.dim)
                hideRow
                HStack(spacing: 10) {
                    Button(Self.stayHidden) { answer(live: false) }
                        .buttonStyle(FirstRunButtonStyle(main: hide))
                        .accessibilityIdentifier("roundtableStayHidden")
                    Button(Self.goLive) { answer(live: true) }
                        .buttonStyle(FirstRunButtonStyle(main: !hide))
                        .accessibilityIdentifier("roundtableGoLiveFirstRun")
                }
                .disabled(sent)
                if let error {
                    Text(error)
                        .font(.system(size: 12))
                        .foregroundStyle(RoundtableColours.unknown)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier("roundtableFirstRunError")
                }
                Text(Self.foot)
                    .font(.system(size: 11))
                    .foregroundStyle(RoundtableColours.dim)
                    .multilineTextAlignment(.center)
                    .frame(maxWidth: .infinity)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .padding(.horizontal, 16)
            .padding(.bottom, 16)
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableFirstRun")
    }

    private var whoText: Text {
        let who = Self.whoItem(callsign: callsign, grid: grid)
        var text = Text(who.lead)
        if let call = who.call {
            text = text + Text(call).font(.system(size: 12, weight: .bold).monospaced())
                .foregroundColor(RoundtableColours.you)
        }
        if let middle = who.middle, let grid = who.grid {
            text = text + Text(middle) + Text(grid).font(.system(size: 12, weight: .bold).monospaced())
                .foregroundColor(RoundtableColours.you)
        }
        return text
    }

    private func item(dot: Color, ring: Color, @ViewBuilder text: () -> Text) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            Circle().fill(dot).overlay(Circle().strokeBorder(ring, lineWidth: 2))
                .frame(width: 9, height: 9)
                .accessibilityHidden(true)
            text()
                .font(.system(size: 12))
                .foregroundStyle(RoundtableColours.text)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
        .padding(.vertical, 6)
        .overlay(alignment: .bottom) { Rectangle().fill(RoundtableColours.rule).frame(height: 1) }
    }

    private var hideRow: some View {
        HStack(alignment: .center, spacing: 10) {
            VStack(alignment: .leading, spacing: 3) {
                Text(Self.hideTitle).font(.system(size: 13, weight: .bold)).foregroundStyle(RoundtableColours.bright)
                Text(Self.hideText).font(.system(size: 11)).foregroundStyle(RoundtableColours.dim)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 4)
            Button(hide ? "On" : "Off") { hide.toggle() }
                .buttonStyle(RoundtableButtonStyle(main: hide))
                .frame(minWidth: 52, minHeight: 44)
                .accessibilityLabel(Self.hideTitle)
                .accessibilityValue(hide ? "On" : "Off")
                .accessibilityIdentifier("roundtableHideSwitch")
        }
        .padding(10)
        .background(RoundtableColours.field, in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius).strokeBorder(RoundtableColours.rule, lineWidth: 1))
    }

    private func answer(live: Bool) {
        guard !sent else {
            return
        }
        guard roundtable.available else {
            error = roundtable.coreReason ?? Self.notRunning
            return
        }
        sent = true
        error = nil
        Task {
            if let reason = await roundtable.answerFirstRun(live: live) {
                sent = false
                error = reason
            } else {
                answered()
            }
        }
    }
}

/// The one-time note on a device that opens Roundtable after another
/// device answered (spec 11.2).
struct RoundtableFirstRunNote: View {
    let hidden: Bool
    let ok: () -> Void

    static func head(hidden: Bool) -> String {
        hidden ? "Your station chose Stay hidden on another device."
            : "Your station went live from another device."
    }

    static let body = "Asked once per station, so this phone doesn't ask again. Hide my station is in "
        + "Roundtable settings."

    var body: some View {
        HStack(alignment: .top, spacing: 10) {
            (Text(Self.head(hidden: hidden)).bold().foregroundColor(RoundtableColours.bright)
                + Text(" " + Self.body).foregroundColor(RoundtableColours.text))
                .font(.system(size: 12))
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 4)
            Button("OK", action: ok)
                .buttonStyle(RoundtableButtonStyle(main: true))
                .accessibilityIdentifier("roundtableFirstRunNoteOk")
        }
        .padding(10)
        .background(RoundtableColours.field, in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius).strokeBorder(RoundtableColours.rule, lineWidth: 1))
        .padding(.horizontal, 10)
        .padding(.bottom, 6)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("roundtableFirstRunNote")
    }
}

/// The first-run sheet's two buttons: the main one blue, full width.
private struct FirstRunButtonStyle: ButtonStyle {
    let main: Bool

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 14, weight: .bold))
            .foregroundStyle(main ? Color.white : RoundtableColours.text)
            .frame(maxWidth: .infinity, minHeight: 44)
            .background(main ? RoundtableColours.mainButton : RoundtableColours.field,
                        in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
            .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).strokeBorder(
                main ? RoundtableColours.accent : RoundtableColours.fieldEdge, lineWidth: 1))
            .opacity(configuration.isPressed ? 0.7 : 1)
            .contentShape(Rectangle())
    }
}
