// NereusSDR for iOS: DSP > CFC's band editor: its row on the page and the editor it opens, as the desktop's CFC dialog lays it out
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The CFC band editor's row (Setup description 19): the Core's label and
/// help, opening the editor. Greyed, with the Core's reason, while the
/// profile cannot change; the Core's last refusal shows in its own words.
struct CfcBandsPanel: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher

    var body: some View {
        let panel = dispatcher.cfcPanel(control, in: category)
        VStack(alignment: .leading, spacing: 4) {
            NavigationLink(value: SetupTree.Route.cfcBands(category: category, control: control.id)) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(control.label)
                        .font(.body.weight(.semibold))
                    if let profile = panel.profile {
                        Text(CfcBandsEditor.summary(profile))
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                            .monospacedDigit()
                    }
                }
            }
            .disabled(!panel.editable)
            .accessibilityIdentifier(control.id)
            if !control.tooltip.isEmpty {
                Text(control.tooltip)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
            if let reason = panel.reason {
                Text(reason)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .accessibilityIdentifier("\(control.id).reason")
            }
            if let problem = panel.problem {
                Text(problem)
                    .font(.footnote)
                    .foregroundStyle(ConnectChrome.badText)
                    .accessibilityIdentifier("\(control.id).problem")
            }
        }
    }
}

/// The editor (the desktop's TxCfcDialog): a 5, 10 or 18 band choice,
/// Low and High, Use Q Factors, Pre-Comp and Post-EQ, then each band's
/// frequency, compression and post-EQ gain, and their Qs, which change
/// only while Use Q Factors is on and are greyed with the reason under
/// the band while it is off. The end bands sit on Low and High and are
/// read-only.
/// Each change shows at once and the whole profile goes to the Core after
/// a pause in editing; the Core's refusal shows in its own words.
struct CfcBandsEditor: View {
    @ObservedObject var pages: SetupDescribedPages
    @ObservedObject var dispatcher: SetupControlDispatcher
    let category: String
    let controlId: String

    /// The desktop's group titles.
    static let bandsTitle = "Bands"
    static let rangeTitle = "Freq Range"
    static let settingsTitle = "Compression and Post-EQ"
    static let sendingText = "Sending to the Core."
    static let goneText = DescribedPage.goneText

    static func bandCountLabel(_ count: Int) -> String { "\(count)-band" }
    static func bandTitle(_ number: Int) -> String { "Band \(number)" }
    /// Why a band's Qs are greyed: they change only with Use Q Factors on.
    static let qFactorsOffReason = "Turn on Use Q Factors to change the Qs."
    /// The columns that apply only while Use Q Factors is on.
    static let qColumns: Set<String> = ["compressionQ", "postEqQ"]
    /// A band field's pad title: its band's group title, then the Core's
    /// label for the field ("Band 2 Freq"). The desktop's editor names the
    /// band the same way, its "#" box beside the field
    /// (src/gui/applets/TxCfcDialog.cpp:298-307).
    static func bandFieldTitle(_ number: Int, _ label: String) -> String { "\(bandTitle(number)) \(label)" }

    /// The row's line under its label: the band count and the range.
    static func summary(_ profile: CfcProfile) -> String {
        "\(bandCountLabel(profile.bands.count)), \(Int(profile.minHz.rounded()))\u{2013}\(Int(profile.maxHz.rounded())) Hz"
    }

    private var control: SetupDescription.Control? {
        pages.categories[category]?.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == controlId }
    }

    var body: some View {
        if let control, case .cfcProfile(let editor)? = control.binding {
            let panel = dispatcher.cfcPanel(control, in: category)
            List {
                if let profile = panel.profile {
                    content(control, editor, profile, editable: panel.editable)
                }
                Section {
                    if let reason = panel.reason {
                        Text(reason)
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                            .accessibilityIdentifier("\(controlId).reason")
                    }
                    if let problem = panel.problem {
                        Text(problem)
                            .font(.footnote)
                            .foregroundStyle(ConnectChrome.badText)
                            .accessibilityIdentifier("\(controlId).problem")
                    }
                    if panel.sending {
                        Text(Self.sendingText)
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                } footer: {
                    Text(control.tooltip)
                }
            }
            .navigationTitle(control.label)
            .accessibilityIdentifier("\(controlId).editor")
        } else {
            List {
                Text(Self.goneText)
                    .foregroundStyle(.secondary)
            }
        }
    }

    @ViewBuilder
    private func content(_ control: SetupDescription.Control, _ editor: SetupDescription.CfcEditor,
                         _ profile: CfcProfile, editable: Bool) -> some View {
        Section(Self.bandsTitle) {
            Picker(Self.bandsTitle, selection: Binding(
                get: { profile.bands.count },
                set: { count in edit(control) { $0 = $0.withBandCount(count) } })) {
                ForEach(editor.bandCounts, id: \.self) { count in
                    Text(Self.bandCountLabel(count)).tag(count)
                }
            }
            .pickerStyle(.segmented)
            .disabled(!editable)
            .accessibilityIdentifier("\(controlId).bandCount")
        }
        Section(Self.rangeTitle) {
            ForEach(editor.fields.filter { $0.id == "minHz" || $0.id == "maxHz" }, id: \.id) { field in
                headerRow(control, editor, field, profile, editable: editable)
            }
        }
        Section(Self.settingsTitle) {
            ForEach(editor.fields.filter { $0.id != "minHz" && $0.id != "maxHz" }, id: \.id) { field in
                headerRow(control, editor, field, profile, editable: editable)
            }
        }
        ForEach(Array(profile.bands.enumerated()), id: \.offset) { index, band in
            Section {
                ForEach(editor.columns, id: \.id) { column in
                    bandRow(control, column, index: index, band: band, count: profile.bands.count,
                            parametric: profile.parametric, editable: editable)
                }
            } header: {
                Text(Self.bandTitle(index + 1))
            } footer: {
                // The Q columns apply only while Use Q Factors is on
                // (setup description 19): greyed, never hidden.
                if !profile.parametric, editor.columns.contains(where: { Self.qColumns.contains($0.id) }) {
                    Text(Self.qFactorsOffReason)
                        .accessibilityIdentifier("\(controlId).band\(index + 1).reason")
                }
            }
        }
    }

    @ViewBuilder
    private func headerRow(_ control: SetupDescription.Control, _ editor: SetupDescription.CfcEditor,
                           _ field: SetupDescription.CfcField, _ profile: CfcProfile, editable: Bool) -> some View {
        let id = "\(controlId).\(field.id)"
        switch field.id {
        case "parametric":
            Toggle(field.label, isOn: Binding(get: { profile.parametric },
                                              set: { on in edit(control) { $0.parametric = on } }))
                .disabled(!editable)
                .accessibilityIdentifier(id)
        case "minHz":
            // Low stays the Core's span below High, as the desktop's does.
            number(field, value: profile.minHz, id: id, editable: editable,
                   upper: profile.maxHz - editor.minSpanHz) { low in
                edit(control) { $0 = $0.withRange(minHz: low, maxHz: $0.maxHz) }
            }
        case "maxHz":
            number(field, value: profile.maxHz, id: id, editable: editable,
                   lower: profile.minHz + editor.minSpanHz) { high in
                edit(control) { $0 = $0.withRange(minHz: $0.minHz, maxHz: high) }
            }
        case "precompDb":
            number(field, value: profile.precompDb, id: id, editable: editable) { value in
                edit(control) { $0.precompDb = value }
            }
        case "postEqGainDb":
            number(field, value: profile.postEqGainDb, id: id, editable: editable) { value in
                edit(control) { $0.postEqGainDb = value }
            }
        default:
            EmptyView()
        }
    }

    @ViewBuilder
    private func bandRow(_ control: SetupDescription.Control, _ column: SetupDescription.CfcField, index: Int,
                         band: CfcProfile.Band, count: Int, parametric: Bool, editable: Bool) -> some View {
        let id = "\(controlId).band\(index + 1).\(column.id)"
        let padTitle = Self.bandFieldTitle(index + 1, column.label)
        switch column.id {
        case "frequencyHz":
            if index == 0 || index == count - 1 {
                // The end bands sit on Low and High.
                LabeledContent(column.label) {
                    Text(SetupNumberRow.text(band.frequencyHz, decimals: column.decimals, unit: column.unit))
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                }
                .accessibilityIdentifier(id)
            } else {
                number(column, value: band.frequencyHz, id: id, editable: editable, padTitle: padTitle) { value in
                    edit(control) { $0.bands[index].frequencyHz = value }
                }
            }
        case "compressionDb":
            number(column, value: band.compressionDb, id: id, editable: editable, padTitle: padTitle) { value in
                edit(control) { $0.bands[index].compressionDb = value }
            }
        case "compressionQ":
            number(column, value: band.compressionQ, id: id, editable: editable && parametric,
                   padTitle: padTitle) { value in
                edit(control) { $0.bands[index].compressionQ = value }
            }
        case "postEqGainDb":
            number(column, value: band.postEqGainDb, id: id, editable: editable, padTitle: padTitle) { value in
                edit(control) { $0.bands[index].postEqGainDb = value }
            }
        case "postEqQ":
            number(column, value: band.postEqQ, id: id, editable: editable && parametric,
                   padTitle: padTitle) { value in
                edit(control) { $0.bands[index].postEqQ = value }
            }
        default:
            EmptyView()
        }
    }

    /// One value stepped within the Core's range (narrowed for Low and
    /// High), or typed on the number pad within it, rounded to the Core's
    /// precision. A typed value goes into the profile as a step does, and
    /// the whole profile to the Core after the pause; the Core's refusal
    /// shows at the foot of the editor, as it does for a step.
    private func number(_ field: SetupDescription.CfcField, value: Double, id: String, editable: Bool,
                        lower: Double? = nil, upper: Double? = nil, padTitle: String? = nil,
                        set: @escaping (Double) -> Void) -> some View {
        let range = field.range ?? SetupDescription.Range(minimum: value, maximum: value, step: 1)
        let low = max(range.minimum, lower ?? range.minimum)
        let high = max(low, min(range.maximum, upper ?? range.maximum))
        return SetupNumberRow(title: field.label, value: value, range: low...high, step: range.step,
                              unit: field.unit, decimals: field.decimals, enabled: editable, id: id,
                              padTitle: padTitle, enter: { typed in
                                  set(DescribedControl.rounded(typed, decimals: field.decimals))
                                  return SetupNumberRow.kept
                              }) { next in
            set(DescribedControl.rounded(next, decimals: field.decimals))
        }
    }

    private func edit(_ control: SetupDescription.Control, _ change: (inout CfcProfile) -> Void) {
        dispatcher.editCfc(control, in: category, change)
    }
}
