// NereusSDR for iOS: the lock-screen card (and StandBy's, at twice the size)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The lock-screen card (spec section 5.5 items 1, 2 and 13; picture 13):
/// NereusSDR's icon, the Core's name and the link; the active slice's frequency,
/// mode and width; the signal (or a message) and the speaker. Keyed, the
/// TX clock joins the head and the foot shows the readings and UNKEY, on
/// red. Lost, LINK LOST with the retries. It stays under iOS's 160-point
/// limit. StandBy shows this same card at twice the size.
struct ActivityCard: View {
    let state: StationActivityAttributes.ContentState

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 8) {
                ActivityAppIcon()
                Text(state.stationName)
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(ActivityColours.textBright)
                    .lineLimit(1)
                    .truncationMode(.tail)
                    .frame(maxWidth: .infinity, alignment: .leading)
                if state.keyed {
                    ActivityTxClock(since: state.keyedSince)
                }
                ActivityLinkChip(link: state.link, roundTripMs: state.roundTripMs)
            }
            .frame(minHeight: 26)
            if state.link == .lost {
                ActivityLostBlock(state: state)
            } else {
                ActivityFrequencyRow(slice: state.slice, keyed: state.keyed, withBadges: true)
                ActivityFoot(state: state)
            }
        }
        .padding(.top, 12)
        .padding(.horizontal, 15)
        .padding(.bottom, 13)
        .overlay(RoundedRectangle(cornerRadius: 24, style: .continuous).strokeBorder(edge, lineWidth: 1))
    }

    /// The card's colour: red while keyed.
    static func background(_ state: StationActivityAttributes.ContentState) -> Color {
        state.keyed ? ActivityColours.cardKeyed : ActivityColours.card
    }

    private var edge: Color {
        if state.keyed {
            return ActivityColours.cardEdgeKeyed
        }
        return state.link == .lost ? ActivityColours.cardEdgeLost : ActivityColours.cardEdge
    }
}
