// NereusSDR for iOS: the TX clock on the card and the opened island, counting up from the key
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// "TX 0:47" in the TX badge's colours. iOS counts it on between the
/// app's updates, so the clock runs every second though the card can't.
struct ActivityTxClock: View {
    let since: Date?

    var body: some View {
        HStack(spacing: 4) {
            Text(ActivityWords.tx)
            if let since {
                Text(since, style: .timer)
                    .monospacedDigit()
            }
        }
        .font(.system(size: 11, weight: .bold, design: .monospaced))
        .foregroundStyle(ActivityColours.txClockText)
        .lineLimit(1)
        .padding(.horizontal, 7)
        .frame(height: 22)
        .background(ActivityColours.txFill, in: RoundedRectangle(cornerRadius: 3))
        .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ActivityColours.txEdge, lineWidth: 1))
        .fixedSize()
    }
}
