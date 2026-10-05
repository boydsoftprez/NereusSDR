// NereusSDR for iOS: the AM Mod Monitor's settings sheet: this phone's source, flashers and style, and the Core's shared feedback receiver
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The sheet the monitor's Settings button opens, and nothing else opens
/// (D102): "On this phone" (the source, both flasher thresholds and the
/// meter style, kept on this phone) and "On the Core, for every device"
/// (the PA feedback receiver, `ModMon/FbStream`, which every device shares).
struct ModMonitorSettingsSheet: View {
    @ObservedObject var model: ModMonitorModel
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
                Text("AM Mod Monitor settings")
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
                .accessibilityIdentifier("modMonitorSettingsDone")
            }
            .padding(.leading, 16)
            .padding(.trailing, 8)
            .padding(.top, 8)
            .overlay(alignment: .bottom) {
                Rectangle().fill(ChromeColours.sheetBorder).frame(height: 1)
            }
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    phoneGroup
                    coreGroup
                }
                .padding(.horizontal, 14)
                .padding(.top, 10)
                .padding(.bottom, 40)
            }
        }
        .background(ChromeColours.sheet.ignoresSafeArea())
        .presentationDetents([.large])
        .presentationDragIndicator(.visible)
        .accessibilityIdentifier("modMonitorSettingsSheet")
    }

    private var phoneGroup: some View {
        group("On this phone", tag: .thisPhone) {
            VStack(alignment: .leading, spacing: 6) {
                label("Source")
                ModMonitorSourcePicker(model: model, height: control, font: text)
                if !model.pureSignalRunning {
                    note(ModMonitorModel.pureSignalOffText)
                }
            }
            ModMonitorThresholdRow(title: "Positive peak flasher", range: ModMonitorModel.posFlashRange,
                                   value: model.posFlashPct, identifier: "modMonitorPositiveThreshold",
                                   font: text, small: small, control: control) { model.setPosFlash($0) }
            ModMonitorThresholdRow(title: "Negative peak flasher", range: ModMonitorModel.negFlashRange,
                                   value: model.negFlashPct, identifier: "modMonitorNegativeThreshold",
                                   font: text, small: small, control: control) { model.setNegFlash($0) }
            VStack(alignment: .leading, spacing: 6) {
                label("Meter style")
                HStack(spacing: 4) {
                    ForEach(ModMonitorModel.MeterStyle.allCases, id: \.self) { style in
                        ModMonitorSegment(label: style.label, chosen: model.meterStyle == style, height: control,
                                          font: text) {
                            model.setMeterStyle(style)
                        }
                        .accessibilityIdentifier("modMonitorStyle" + style.label)
                    }
                }
                .accessibilityElement(children: .contain)
                .accessibilityLabel("Meter style")
            }
            note(ModMonitorModel.defaultsText)
        }
    }

    private var coreGroup: some View {
        group("On the Core, for every device", tag: .core) {
            VStack(alignment: .leading, spacing: 6) {
                label("PA feedback receiver")
                HStack(spacing: 4) {
                    ForEach(Array(AmModulationReading.feedbackReceivers), id: \.self) { receiver in
                        ModMonitorSegment(label: "rx\(receiver)", chosen: model.feedbackReceiver == receiver,
                                          height: control, font: text, disabled: !model.enabled) {
                            model.setFeedbackReceiver(receiver)
                        }
                        .accessibilityLabel("Receiver \(receiver)")
                        .accessibilityIdentifier("modMonitorReceiver\(receiver)")
                    }
                }
                .accessibilityElement(children: .contain)
                .accessibilityLabel("PA feedback receiver, shared by every device")
                note(ModMonitorModel.receiverText)
                if let reason = model.receiverNote {
                    Text(reason)
                        .font(.system(size: small))
                        .foregroundStyle(ChromeColours.refusalText)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier("modMonitorReceiverNote")
                }
            }
        }
    }

    private func group(_ title: String, tag: SetupTag, @ViewBuilder content: () -> some View) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 8) {
                Text(title)
                    .font(.system(size: text * 0.92, weight: .bold))
                    .foregroundStyle(Color(red: 0xB5 / 255, green: 0xC9 / 255, blue: 0xDA / 255))
                    .accessibilityAddTraits(.isHeader)
                Spacer(minLength: 4)
                SetupTagBadge(tag: tag)
            }
            content()
        }
        .padding(10)
        .background(Color(red: 0x0D / 255, green: 0x13 / 255, blue: 0x20 / 255), in: RoundedRectangle(cornerRadius: 10))
        .overlay(RoundedRectangle(cornerRadius: 10).strokeBorder(ChromeColours.sheetBorder, lineWidth: 1))
    }

    private func label(_ words: String) -> some View {
        Text(words)
            .font(.system(size: text))
            .foregroundStyle(ChromeColours.text)
    }

    private func note(_ words: String) -> some View {
        Text(words)
            .font(.system(size: small))
            .foregroundStyle(Color(red: 0x8F / 255, green: 0xA2 / 255, blue: 0xB4 / 255))
            .fixedSize(horizontal: false, vertical: true)
    }
}

/// A flasher threshold: minus, the percent typed in, plus. A typed number
/// outside the range is held to its nearest end.
struct ModMonitorThresholdRow: View {
    let title: String
    let range: ClosedRange<Int>
    let value: Int
    let identifier: String
    var font: CGFloat = 13
    var small: CGFloat = 11
    var control: CGFloat = 44
    let change: (Int) -> Void

    @State private var typed = ""
    @FocusState private var typing: Bool
    @Environment(\.dynamicTypeSize) private var typeSize

    var body: some View {
        let layout = typeSize >= .accessibility1 ? AnyLayout(VStackLayout(alignment: .leading, spacing: 6))
            : AnyLayout(HStackLayout(spacing: 10))
        layout {
            VStack(alignment: .leading, spacing: 1) {
                Text(title)
                    .font(.system(size: font))
                    .foregroundStyle(ChromeColours.text)
                Text("\(range.lowerBound) to \(range.upperBound)%")
                    .font(.system(size: small))
                    .foregroundStyle(ChromeColours.textDim)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            HStack(spacing: 4) {
                step("\u{2212}", by: -1, disabled: value <= range.lowerBound, name: "Lower")
                TextField("", text: $typed)
                    .keyboardType(.numberPad)
                    .multilineTextAlignment(.trailing)
                    .font(.system(size: font + 2, weight: .semibold).monospacedDigit())
                    .foregroundStyle(ChromeColours.textBright)
                    .focused($typing)
                    .padding(.horizontal, 4)
                    .frame(width: control * 1.05, height: control * 0.9)
                    .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
                    .accessibilityLabel(title + ", percent")
                    .accessibilityIdentifier(identifier)
                    .onChange(of: typing) { _, now in
                        if !now {
                            commit()
                        }
                    }
                    .onSubmit(commit)
                Text("%")
                    .font(.system(size: font))
                    .foregroundStyle(ChromeColours.textDim)
                step("+", by: 1, disabled: value >= range.upperBound, name: "Raise")
            }
        }
        .onAppear { typed = "\(value)" }
        .onChange(of: value) { _, now in
            if !typing {
                typed = "\(now)"
            }
        }
        .onDisappear(perform: commit)
    }

    private func commit() {
        guard let number = Int(typed.trimmingCharacters(in: .whitespaces)) else {
            typed = "\(value)"
            return
        }
        let held = min(max(number, range.lowerBound), range.upperBound)
        typed = "\(held)"
        if held != value {
            change(held)
        }
    }

    private func step(_ glyph: String, by delta: Int, disabled: Bool, name: String) -> some View {
        Button {
            typing = false
            change(min(max(value + delta, range.lowerBound), range.upperBound))
        } label: {
            Text(glyph)
                .font(.system(size: font + 7, weight: .bold))
                .foregroundStyle(disabled ? Color(red: 0x40 / 255, green: 0x50 / 255, blue: 0x60 / 255) : ChromeColours.text)
                .frame(width: control, height: control)
                .background(ChromeColours.button, in: RoundedRectangle(cornerRadius: 6))
                .overlay(RoundedRectangle(cornerRadius: 6)
                    .strokeBorder(disabled ? ChromeColours.insetBorder : ChromeColours.buttonBorder, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(disabled)
        .accessibilityLabel(name + " " + title.lowercased())
        .accessibilityIdentifier(identifier + (delta < 0 ? "Lower" : "Raise"))
    }
}
