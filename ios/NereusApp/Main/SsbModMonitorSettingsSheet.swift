// NereusSDR for iOS: the SSB Mod Monitor's settings sheet: this phone's source, OVER threshold and style
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The sheet the monitor's Settings button opens, and nothing else opens:
/// "On this phone" (the source, the OVER threshold and the meter style, kept
/// on this phone). Unlike the AM monitor's sheet it has no Core group: PA FB
/// reads PureSignal's feedback path, so there is no receiver to choose.
struct SsbModMonitorSettingsSheet: View {
    @ObservedObject var model: SsbModMonitorModel
    /// The sheet closes itself; a preview or a screenshot can leave it open.
    var close: (() -> Void)?
    @Environment(\.dismiss) private var dismiss

    @ScaledMetric(relativeTo: .caption2) private var small: CGFloat = 11
    @ScaledMetric(relativeTo: .caption) private var text: CGFloat = 13
    @ScaledMetric(relativeTo: .headline) private var heading: CGFloat = 15
    @ScaledMetric(relativeTo: .body) private var control: CGFloat = 44

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                Text("SSB Mod Monitor settings")
                    .font(.system(size: heading, weight: .bold))
                    .foregroundStyle(ChromeColours.textBright)
                    .accessibilityAddTraits(.isHeader)
                Spacer(minLength: 4)
                Button {
                    if let close {
                        close()
                    } else {
                        dismiss()
                    }
                } label: {
                    Text("Done")
                        .font(.system(size: text, weight: .bold))
                        .foregroundStyle(ChromeColours.toolbarOpenText)
                        .frame(minWidth: 64, minHeight: 44)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .accessibilityIdentifier("ssbModMonitorSettingsDone")
            }
            .padding(.leading, 16)
            .padding(.trailing, 8)
            .padding(.top, 8)
            .overlay(alignment: .bottom) {
                Rectangle().fill(ChromeColours.sheetBorder).frame(height: 1)
            }
            ScrollView {
                phoneGroup
                    .padding(.horizontal, 14)
                    .padding(.top, 10)
                    .padding(.bottom, 40)
            }
        }
        .background(ChromeColours.sheet.ignoresSafeArea())
        .presentationDetents([.large])
        .presentationDragIndicator(.visible)
        .accessibilityIdentifier("ssbModMonitorSettingsSheet")
    }

    private var phoneGroup: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 8) {
                Text("On this phone")
                    .font(.system(size: text * 0.92, weight: .bold))
                    .foregroundStyle(WorkspacePalette.titleText)
                    .accessibilityAddTraits(.isHeader)
                Spacer(minLength: 4)
                SetupTagBadge(tag: .thisPhone)
            }
            VStack(alignment: .leading, spacing: 6) {
                label("Source")
                SsbModMonitorSourcePicker(model: model, height: control, font: text)
                if let reason = model.paFeedbackReason {
                    note(reason)
                }
            }
            ModMonitorThresholdRow(title: "OVER lamp", range: SsbModMonitorModel.overRange, value: model.overDbfs,
                                   identifier: "ssbModMonitorOverThreshold", font: text, small: small, control: control,
                                   unit: "dBFS", unitName: "dB full scale") { model.setOver($0) }
            VStack(alignment: .leading, spacing: 6) {
                label("Meter style")
                HStack(spacing: 4) {
                    ForEach(SsbModMonitorModel.styles, id: \.self) { style in
                        ModMonitorSegment(label: style.label, chosen: model.meterStyle == style, height: control,
                                          font: text) {
                            model.setMeterStyle(style)
                        }
                        .accessibilityIdentifier("ssbModMonitorStyle" + style.label)
                    }
                }
                .accessibilityElement(children: .contain)
                .accessibilityLabel("Meter style")
            }
            note(SsbModMonitorModel.defaultsText)
        }
        .padding(10)
        .background(ChromeColours.panel, in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius).strokeBorder(ChromeColours.sheetBorder, lineWidth: 1))
    }

    private func label(_ words: String) -> some View {
        Text(words)
            .font(.system(size: text))
            .foregroundStyle(ChromeColours.text)
    }

    private func note(_ words: String) -> some View {
        Text(words)
            .font(.system(size: small))
            .foregroundStyle(ChromeColours.caption)
            .fixedSize(horizontal: false, vertical: true)
    }
}
