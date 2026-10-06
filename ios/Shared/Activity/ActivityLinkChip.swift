// NereusSDR for iOS: the link chip on the lock-screen card and the island: the dot, the round trip or Lost
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The band's link chip, at card size (the board's `.conn`): the dot, then
/// the round trip while up, Lost or Offline.
struct ActivityLinkChip: View {
    let link: StationActivityAttributes.Link
    let roundTripMs: Int?

    var body: some View {
        HStack(spacing: 6) {
            Circle().fill(dot).frame(width: 10, height: 10)
            if let words {
                Text(words).foregroundStyle(words == ActivityWords.offline ? ActivityColours.textDim : dot)
            }
        }
        .font(.system(size: 10, weight: .semibold, design: .monospaced))
        .lineLimit(1)
        .padding(.leading, 7)
        .padding(.trailing, 8)
        .frame(height: 26)
        .background(ActivityColours.chip, in: RoundedRectangle(cornerRadius: 3))
        .accessibilityElement(children: .combine)
    }

    private var dot: Color {
        switch link {
        case .up:
            return ActivityColours.linkUp
        case .lost:
            return ActivityColours.linkLost
        case .connecting, .offline:
            return ActivityColours.linkOff
        }
    }

    private var words: String? {
        switch link {
        case .up:
            return roundTripMs.map(ActivityWords.roundTrip)
        case .lost:
            return ActivityWords.lost
        case .offline:
            return ActivityWords.offline
        case .connecting:
            return nil
        }
    }
}
