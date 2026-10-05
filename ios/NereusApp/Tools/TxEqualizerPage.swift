// NereusSDR for iOS: TX Equalizer, one Tools page: the Core's transmit equalizer as the desktop's editor offers it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// TX Equalizer on the phone (spec section 5.2 item 4, R-IOS-18): the
/// desktop's TX EQ dialog as one page. The switch and Legacy EQ, the TX
/// profile the EQ is saved with and its Save, the parametric curve with its
/// point editor and values (plan Task 59a), the preamp and the ten bands
/// with their centres, then the filter. Everything lives at the Core; a
/// setting that cannot change now is greyed with its reason under it.
struct TxEqualizerPage: View {
    @ObservedObject var model: TxEqualizerModel
    /// False on a phone turned sideways, where the curve is drawn beside the page.
    var showsCurve = true
    @State var profileOwner = UUID()

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: "Equalizer", tag: .core)
            SpotHubPage.Card {
                SpotHubPage.SettingRow(title: "TX EQ", detail: "The equalizer on the transmitted audio") {
                    SpotHubPage.OnOff(isOn: model.enabled ?? false,
                                      enabled: model.switchReason == nil && model.enabled != nil,
                                      identifier: "txEq.enabled") {
                        model.setEnabled(!(model.enabled ?? false))
                    }
                }
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Legacy EQ",
                                       detail: "On, the ten bands shape the audio. Off, the parametric curve does.") {
                    SpotHubPage.OnOff(isOn: model.legacy ?? false,
                                      enabled: model.editorReason == nil && model.legacy != nil,
                                      identifier: "txEq.legacy") {
                        model.setLegacy(!(model.legacy ?? false))
                    }
                }
                if let reason = model.switchReason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "txEq.switchReason")
                }
            }
            SpotHubPage.Heading(text: "Profile", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                SpotHubPage.SettingRow(title: "TX profile", detail: "The EQ is saved with the Core's TX profile") {
                    profileMenu
                }
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Save",
                                       detail: "Saves the \(model.activeProfile ?? "active") profile, this curve with it.") {
                    TxEqCurveParts.PlainButton(title: "Save", enabled: model.saveReason == nil && model.activeProfile != nil,
                                               large: false, identifier: "txEq.save") { model.saveProfile() }
                }
                if let reason = model.profileReason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "txEq.profileReason")
                }
            }
            if showsCurve {
                TxEqCurveParts.CurveCard(model: model).padding(.top, 8)
            }
            TxEqCurveParts.PointCard(model: model).padding(.top, 8)
            TxEqCurveParts.ValuesCard(model: model).padding(.top, 8)
            SpotHubPage.Heading(text: "Bands", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                ToolPageParts.SliderRow(title: "Preamp", value: model.preamp.map(Double.init),
                                        range: Self.range(TxEqualizerModel.preampRange), shown: Self.db,
                                        enabled: model.editorReason == nil, titleWidth: 56,
                                        identifier: "txEq.preamp", notConfirmed: model.isUnconfirmed("txEqPreamp")) { model.setPreamp(Int($0.rounded())) }
                ForEach(0..<TxEqualizerModel.bandCount, id: \.self) { index in
                    SpotHubPage.Line()
                    bandRow(index)
                }
                if let reason = model.editorReason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "txEq.editorReason")
                }
            }
            SpotHubPage.Heading(text: "Filter", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                SpotHubPage.SettingRow(title: "Filter size", detail: "More taps shape the audio more finely") {
                    ValueField(text: model.size.map { "\($0)" } ?? "--", accessibility: "Filter size",
                               disabled: model.editorReason != nil || model.size == nil, minWidth: 56) {
                        model.openSizePad()
                    }
                    .accessibilityIdentifier("txEq.size")
                }
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Minimum phase", detail: "Off keeps the phase linear") {
                    SpotHubPage.OnOff(isOn: model.minimumPhase ?? false,
                                      enabled: model.editorReason == nil && model.minimumPhase != nil,
                                      identifier: "txEq.minimumPhase") {
                        model.setMinimumPhase(!(model.minimumPhase ?? false))
                    }
                }
                SpotHubPage.Line()
                choiceRow("Cutoff", TxEqualizerModel.cutoffChoices, model.cutoff, identifier: "txEq.cutoff") {
                    model.setCutoff($0)
                }
                SpotHubPage.Line()
                choiceRow("Window", TxEqualizerModel.windowChoices, model.window, identifier: "txEq.window") {
                    model.setWindow($0)
                }
            }
            if let note = model.profileFlow?.problem(for: profileOwner) {
                ToolPageParts.Refusal(text: note, identifier: "txEq.profileNote").padding(.top, 4)
            }
            if let note = model.note {
                ToolPageParts.Refusal(text: note, identifier: "txEq.note").padding(.top, 4)
            }
        }
        .background {
            if let flow = model.profileFlow { TxProfileSurfaceQuestion(flow: flow, owner: profileOwner, identifier: "txEq.profile") }
        }
    }

    private var profileMenu: some View {
        let enabled = model.profileReason == nil && model.profileFlow?.busy != true && !model.profiles.isEmpty
        return Menu {
            ForEach(model.profiles, id: \.self) { name in
                Button {
                    model.selectProfile(name, owner: profileOwner)
                } label: {
                    if name == model.activeProfile {
                        Label(name, systemImage: "checkmark")
                    } else {
                        Text(name)
                    }
                }
            }
        } label: {
            HStack(spacing: 4) {
                Text(model.activeProfile ?? "--")
                    .font(.system(size: 12, weight: .semibold))
                    .lineLimit(1)
                Image(systemName: "chevron.up.chevron.down")
                    .font(.system(size: 9, weight: .semibold))
            }
            .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
            .padding(.horizontal, 8)
            .frame(minWidth: 96, minHeight: 30)
            .background(enabled ? ChromeColours.button : ChromeColours.buttonOff, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4)
                .strokeBorder(enabled ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder, lineWidth: 1))
        }
        .disabled(!enabled)
        .accessibilityLabel("TX profile")
        .accessibilityValue(model.activeProfile ?? "None")
        .accessibilityIdentifier("txEq.profile")
    }

    private func bandRow(_ index: Int) -> some View {
        let gain = model.bands.map { Double($0[index]) }
        let hz = model.frequencies?[index]
        let enabled = model.editorReason == nil
        return HStack(spacing: 6) {
            ToolPageParts.SliderRow(title: "B\(index + 1)", value: gain, range: Self.range(TxEqualizerModel.bandRange),
                                    shown: Self.db, enabled: enabled, titleWidth: 28,
                                    identifier: "txEq.band\(index)", notConfirmed: model.isUnconfirmed("txEqBandsJson")) { model.setBand(index, Int($0.rounded())) }
                .padding(.trailing, -10)
            ValueField(text: hz.map { "\($0) Hz" } ?? "--", accessibility: "Band \(index + 1) center",
                       disabled: !enabled || hz == nil, minWidth: 84) {
                model.openFrequencyPad(index)
            }
            .accessibilityIdentifier("txEq.frequency\(index)")
            .padding(.trailing, 10)
        }
    }

    private func choiceRow(_ title: String, _ options: [(id: Int64, label: String)], _ selected: Int64?,
                           identifier: String, pick: @escaping (Int64) -> Void) -> some View {
        HStack(spacing: 10) {
            Text(title)
                .font(.system(size: 14))
                .foregroundStyle(ChromeColours.text)
                .frame(width: 64, alignment: .leading)
            ToolPageParts.Choices(options: options, selected: selected,
                                  enabled: model.editorReason == nil && selected != nil, identifier: identifier,
                                  pick: pick)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
    }

    static func range(_ range: ClosedRange<Int>) -> ClosedRange<Double> {
        Double(range.lowerBound)...Double(range.upperBound)
    }

    /// A gain as the desktop's spinboxes show it, signed: "+3 dB", "0 dB", "−6 dB".
    static func db(_ value: Double) -> String {
        let whole = Int(value.rounded())
        return whole > 0 ? "+\(whole) dB" : whole < 0 ? "\u{2212}\(-whole) dB" : "0 dB"
    }
}

/// The TX Equalizer page on the Tools tab, with its model and number pad:
/// upright, one scrolling page; turned sideways, the curve on the left
/// (364 points wide) and the rest of the page scrolling on the right, so
/// the curve stays in view while a point changes.
struct TxEqualizerScreen: View {
    @StateObject private var model: TxEqualizerModel

    init(model: @autoclosure @escaping () -> TxEqualizerModel) {
        _model = StateObject(wrappedValue: model())
    }

    /// The curve's column when sideways (the board's `.eqc-cols`).
    static let sidewaysCurveWidth: CGFloat = 364

    var body: some View {
        GeometryReader { geometry in
            ZStack(alignment: .bottom) {
                if geometry.size.width > geometry.size.height {
                    HStack(alignment: .top, spacing: 12) {
                        ScrollView {
                            TxEqCurveParts.CurveCard(model: model, sideways: true)
                                .padding(.vertical, 12)
                        }
                        .frame(width: Self.sidewaysCurveWidth)
                        ScrollView {
                            TxEqualizerPage(model: model, showsCurve: false)
                                .padding(.vertical, 12)
                                .padding(.bottom, 8)
                        }
                    }
                    .padding(.horizontal, 12)
                } else {
                    ScrollView {
                        TxEqualizerPage(model: model)
                            .padding(12)
                            .padding(.bottom, 8)
                    }
                }
                ValuePadLayer(pad: model.pad)
            }
        }
    }
}
