// NereusSDR for iOS: LINK LOST on the card and the opened island, with the retries and Cancel or Reconnect
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AppIntents
import SwiftUI

/// The link lost (spec section 5.5 items 2 and 4): LINK LOST, "The Core
/// unkeyed itself." when this phone was keyed, then the try and its
/// countdown with Cancel; after Cancel, Reconnect.
struct ActivityLostBlock: View {
    let state: StationActivityAttributes.ContentState

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(ActivityWords.linkLost)
                .font(.system(size: 20, weight: .heavy))
                .kerning(4)
                .foregroundStyle(ActivityColours.linkLost)
            if state.lostWhileKeyed {
                Text(ActivityWords.unkeyedItself)
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(ActivityColours.textBright)
            }
            HStack(spacing: 8) {
                retryText
                    .font(.system(size: 12).monospacedDigit())
                    .foregroundStyle(ActivityColours.text)
                    .lineLimit(1)
                    .frame(maxWidth: .infinity, alignment: .leading)
                if state.retry?.stopped == true {
                    Button(intent: ReconnectIntent()) {
                        stripLabel(ActivityWords.reconnect, go: true)
                    }
                    .buttonStyle(.plain)
                } else {
                    Button(intent: CancelReconnectingIntent()) {
                        stripLabel(ActivityWords.cancel, go: false)
                    }
                    .buttonStyle(.plain)
                }
            }
        }
    }

    private var retryText: Text {
        if !state.message.isEmpty {
            // Only a stale card carries words here (the app sends a lost
            // card none): the countdown would be old news.
            return Text(state.message)
        }
        guard let retry = state.retry else {
            return Text(ActivityWords.reconnecting(attempt: 1) + ActivityWords.tryingNow)
        }
        if retry.stopped {
            return Text(ActivityWords.stoppedReconnecting)
        }
        let lead = Text(ActivityWords.reconnecting(attempt: retry.attempt))
        guard let at = retry.at else {
            return lead + Text(ActivityWords.tryingNow)
        }
        return lead + Text(ActivityWords.nextIn)
            + Text(timerInterval: at.addingTimeInterval(-3_600)...at, countsDown: true)
    }

    private func stripLabel(_ title: String, go: Bool) -> some View {
        Text(title)
            .font(.system(size: 13, weight: .bold))
            .foregroundStyle(go ? Color.white : ActivityColours.cancelText)
            .padding(.horizontal, 14)
            .frame(height: 34)
            .background(go ? ActivityColours.reconnect : ActivityColours.cancel, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4)
                .strokeBorder(go ? ActivityColours.reconnectEdge : ActivityColours.cancelEdge, lineWidth: 1))
    }
}
