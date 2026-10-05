// NereusSDR for iOS: UNKEY on the card and the opened island, in the MOX colours
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AppIntents
import SwiftUI

/// UNKEY (the board's `.la__unkey`): one tap ends this phone's
/// transmission, on the lock screen too, with no Face ID first (JJ,
/// 2026-09-26; spec sections 4.7 and 5.5 item 8).
struct ActivityUnkeyButton: View {
    var body: some View {
        Button(intent: UnkeyIntent()) {
            Text(ActivityWords.unkey)
                .font(.system(size: 14, weight: .heavy))
                .kerning(1.1)
            .foregroundStyle(Color.white)
            .padding(.horizontal, 14)
            .frame(minWidth: 128, minHeight: 44)
            .background(ActivityColours.unkey, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ActivityColours.unkeyEdge, lineWidth: 1))
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Unkey")
    }
}
