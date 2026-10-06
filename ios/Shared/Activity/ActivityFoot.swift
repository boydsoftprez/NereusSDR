// NereusSDR for iOS: the bottom row of the card and the opened island: the signal or a message with the speaker, or the readings with UNKEY
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The card's foot (the board's `.la__foot`). Listening: the slice's signal,
/// or a message in its place (which keeps the card under iOS's height
/// limit), then the speaker; while the radio is on the air for someone
/// else, "On the air from <holder>" in red takes the signal's place. Keyed:
/// forward power and SWR over the time left before the Core's time-out,
/// then UNKEY.
struct ActivityFoot: View {
    let state: StationActivityAttributes.ContentState

    var body: some View {
        HStack(spacing: 10) {
            if state.staleKeyed {
                Text(state.message)
                    .font(.system(size: 12))
                    .foregroundStyle(ActivityColours.timeOut)
                    .lineLimit(2)
                    .fixedSize(horizontal: false, vertical: true)
                    .frame(maxWidth: .infinity, alignment: .leading)
                ActivityUnkeyButton()
            } else if state.keyed {
                VStack(alignment: .leading, spacing: 2) {
                    Text(ActivityWords.telemetry(watts: state.forwardWatts, swr: state.swr))
                        .foregroundStyle(ActivityColours.text)
                    if let end = state.timeOutAt {
                        (Text(ActivityWords.timeOutIn)
                            + Text(timerInterval: min(state.keyedSince ?? end.addingTimeInterval(-86_400), end)...end,
                                   countsDown: true))
                            .foregroundStyle(ActivityColours.timeOut)
                    }
                }
                .font(.system(size: 12).monospacedDigit())
                .lineLimit(1)
                .frame(maxWidth: .infinity, alignment: .leading)
                ActivityUnkeyButton()
            } else {
                if let holder = state.onAirFrom {
                    HStack(spacing: 7) {
                        Circle().fill(ActivityColours.txText).frame(width: 9, height: 9)
                        Text(ActivityWords.onAirFrom(holder))
                            .font(.system(size: 12, weight: .semibold))
                            .foregroundStyle(ActivityColours.txText)
                            .lineLimit(1)
                            .truncationMode(.tail)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .accessibilityElement(children: .combine)
                } else if state.message.isEmpty {
                    ActivityLevelBar(dbm: state.slice?.signalDbm, text: state.slice?.signalText,
                                     spoken: state.slice?.signalSpoken, meter: state.meter)
                        .frame(maxWidth: 262)
                    Spacer(minLength: 0)
                } else {
                    HStack(spacing: 7) {
                        if state.messageGood {
                            Circle().fill(ActivityColours.good).frame(width: 9, height: 9)
                        }
                        Text(state.message)
                            .font(.system(size: 12))
                            .foregroundStyle(ActivityColours.textDim)
                            .lineLimit(2)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
                ActivityMuteButton(muted: state.muted)
            }
        }
        .frame(minHeight: 36)
    }
}
