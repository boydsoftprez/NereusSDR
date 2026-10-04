// NereusSDR for iOS: the slice's letter and the TX badge, as the flag draws them, on the card and the island
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The flag's small badges: the slice's letter in the slice's colour, and
/// TX lit red while this phone is on the air.
struct ActivityBadge: View {
    enum Kind: Equatable {
        case slice(letter: String, colour: String)
        case tx
    }

    let kind: Kind

    var body: some View {
        switch kind {
        case .slice(let letter, let colour):
            Text(letter)
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(Color.white)
                .frame(width: 18, height: 18)
                .background(ActivityColours.slice(colour), in: RoundedRectangle(cornerRadius: 3))
                .accessibilityLabel("Slice \(letter)")
        case .tx:
            Text(ActivityWords.tx)
                .font(.system(size: 10, weight: .bold))
                .foregroundStyle(ActivityColours.txText)
                .frame(width: 28, height: 18)
                .background(ActivityColours.txFill, in: RoundedRectangle(cornerRadius: 3))
                .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ActivityColours.txEdge, lineWidth: 1))
                .accessibilityLabel("Transmitting")
        }
    }
}
