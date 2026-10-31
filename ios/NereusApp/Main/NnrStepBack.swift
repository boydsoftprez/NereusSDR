// NereusSDR for iOS: measured NNR warning choices and legacy Core limit/retry
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

// 2026-10-31: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import SwiftUI

/// Shared by RX and Modes: a measured warning leaves NNR running until an
/// explicit model/off choice. Actual older-Core limits retain Try again.
struct NnrStepBack: View {
    @ObservedObject var rx: RxPanelModel
    let identifier: String

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            if let reason = rx.nnrPriorityReason {
                Text("NNR: " + reason)
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textFaint)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier(identifier + "Reason")
            }
            if let nnr = rx.nnr, nnr.limit != 0, let text = nnr.limitText {
                VStack(alignment: .leading, spacing: 6) {
                    Text(text)
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.noticeWarn)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier(identifier + "Limit")
                    HStack(spacing: 6) {
                        PanelButton(label: "Try again", lit: false, style: .blue, disabled: nnr.tryAgainReason != nil) {
                            rx.tryNnrAgain()
                        }
                        .frame(width: 96)
                        .accessibilityHint(nnr.tryAgainReason ?? "Runs the saved noise reduction choice again")
                        .accessibilityIdentifier(identifier + "TryAgain")
                        Spacer(minLength: 0)
                    }
                    if let reason = nnr.tryAgainReason {
                        Text("Try again: " + reason)
                            .font(.system(size: 11))
                            .foregroundStyle(ChromeColours.textFaint)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
            }
            if let nnr = rx.nnr, let warning = nnr.loadWarning {
                VStack(alignment: .leading, spacing: 6) {
                    Text(warning)
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.noticeWarn)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier(identifier + "LoadWarning")
                    if nnr.modelSlot == 1 && nnr.standardReady {
                        PanelButton(label: "Use Standard", lit: false, style: .blue, disabled: !rx.nnrCanUseStandard) {
                            rx.useStandardForNnrWarning()
                        }
                        .accessibilityLabel("Use Standard NNR model")
                        .accessibilityIdentifier(identifier + "UseStandard")
                    }
                    PanelButton(label: "Turn NNR off", lit: false, style: .blue, disabled: !rx.nnrCanTurnOff) {
                        rx.turnNnrOffForWarning()
                    }
                    .accessibilityHint("Retains your model choice")
                    .accessibilityIdentifier(identifier + "TurnOff")
                    Text("Leave the warning alone to keep NNR running.")
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textFaint)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
        }
    }
}
