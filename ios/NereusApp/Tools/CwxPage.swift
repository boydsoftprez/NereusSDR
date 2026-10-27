// NereusSDR for iOS: CWX, one Tools page: the text to send, Send and Stop, and the Core's eight memories
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-26: CW transmit, the CWX page (R-CW-50). J.J. Boyd (KG4VCF),
//   AI-assisted via Claude Code.

import NereusLink
import SwiftUI

/// CWX on the phone (R-CW-50, C13): the operator's text with Send and
/// Stop, and the Core's memories M1 to M8, each showing what it holds.
/// The Core's sender keys the radio; this page only asks it to. Every
/// control stays shown; greyed, the reason sits beneath.
struct CwxPage: View {
    @ObservedObject var model: CwxModel
    /// Take transmit, under a refusal whose fix is to take it.
    var take: TransmitTakeModel?

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: "Send", tag: .core)
            SpotHubPage.Card {
                VStack(alignment: .leading, spacing: 8) {
                    TextField("Text to send", text: $model.text, axis: .vertical)
                        .lineLimit(2...6)
                        .font(.system(size: 14, design: .monospaced))
                        .foregroundStyle(ChromeColours.text)
                        .textInputAutocapitalization(.characters)
                        .autocorrectionDisabled()
                        .padding(8)
                        .frame(minHeight: 44)
                        .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius)
                            .strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                        .disabled(model.reason != nil)
                        .accessibilityLabel("Text to send")
                        .accessibilityIdentifier("cwx.text")
                    HStack(spacing: 6) {
                        PanelButton(label: "Send", lit: false, style: .dsp, disabled: model.sendReason != nil) {
                            model.send()
                        }
                        .accessibilityHint(model.sendReason ?? "")
                        .accessibilityIdentifier("cwx.send")
                        PanelButton(label: "Stop", lit: false, style: .red, disabled: model.reason != nil) {
                            model.stop()
                        }
                        .accessibilityHint(model.reason ?? "")
                        .accessibilityIdentifier("cwx.stop")
                    }
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 9)
                if let reason = model.reason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "cwx.reason")
                }
            }
            SpotHubPage.Heading(text: "Memories", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                ForEach(CwxModel.memoryNumbers, id: \.self) { number in
                    if number > 1 {
                        SpotHubPage.Line()
                    }
                    memoryRow(number)
                }
            }
            if let refusal = model.refusal {
                ToolPageParts.Refusal(text: refusal.reason, identifier: "cwx.refusal").padding(.top, 4)
                if let take, refusal.fix == TxRefusalInfo.takeTransmit {
                    TakeTransmitButton(take: take, identifier: "cwx.takeTransmit") { model.takeStarted() }
                        .padding(.top, 4)
                }
            }
            ConnectChrome.Note(text: CwxModel.atCoreNote).padding(.top, 4)
        }
        .environment(\.panelButtonHeight, 44)
    }

    /// One memory: its number, what it holds, and a tap to send it.
    private func memoryRow(_ number: Int) -> some View {
        let held = model.memories[number - 1]
        let reason = model.memoryReason(number)
        return Button {
            model.sendMemory(number)
        } label: {
            HStack(spacing: 10) {
                Text("M\(number)")
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(reason == nil ? ChromeColours.textBright : ChromeColours.textDim)
                    .frame(width: 32, alignment: .leading)
                Text(held.map { $0.isEmpty ? "Empty" : $0 } ?? "Not sent by the Core")
                    .font(.system(size: 13, design: .monospaced))
                    .foregroundStyle(reason == nil ? ChromeColours.text : ChromeColours.textDim)
                    .lineLimit(2)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .padding(.horizontal, 10)
            .frame(minHeight: 44)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(reason != nil)
        .opacity(reason == nil ? 1 : WorkspacePalette.disabledOpacity)
        .accessibilityLabel("Memory \(number)")
        .accessibilityValue(held ?? "")
        .accessibilityHint(reason ?? "Sends this memory")
        .accessibilityIdentifier("cwx.memory\(number)")
    }
}
