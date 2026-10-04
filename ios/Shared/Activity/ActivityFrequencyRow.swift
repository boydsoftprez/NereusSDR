// NereusSDR for iOS: the active slice's frequency, mode and width on the card and the opened island
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The card's main row (the board's `.la__main`): the slice's letter (and
/// TX while keyed) when asked, the frequency in the flag's cyan, and the
/// mode over the width at the right.
struct ActivityFrequencyRow: View {
    let slice: StationActivityAttributes.Slice?
    let keyed: Bool
    let withBadges: Bool

    var body: some View {
        HStack(spacing: 6) {
            if withBadges, let slice {
                ActivityBadge(kind: .slice(letter: slice.letter, colour: slice.colour))
                if keyed {
                    ActivityBadge(kind: .tx)
                }
            }
            Text(slice.map { ActivityWords.frequency($0.frequencyHz) } ?? "")
                .font(.system(size: 30, weight: .bold, design: .monospaced))
                .foregroundStyle(ActivityColours.frequency)
                .lineLimit(1)
                .minimumScaleFactor(0.6)
                .frame(maxWidth: .infinity, alignment: .leading)
            VStack(alignment: .trailing, spacing: 1) {
                Text(slice?.mode ?? "")
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(ActivityColours.text)
                Text(slice?.bandwidth ?? "")
                    .font(.system(size: 12))
                    .foregroundStyle(ActivityColours.bandwidth)
            }
            .lineLimit(1)
            .fixedSize()
        }
        .frame(minHeight: 34)
    }
}
