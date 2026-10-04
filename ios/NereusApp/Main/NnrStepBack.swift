// NereusSDR for iOS: NNR held back by the Core: its reason in the warning colour, and Try again
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// NNR held back by the Core (C3): the Core's reason in the warning colour
/// and Try again, as the desktop's NNR controls show them, on the RX panel
/// and in the Modes tab's Noise section.
struct NnrStepBack: View {
    @ObservedObject var rx: RxPanelModel
    let identifier: String

    var body: some View {
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
    }
}
