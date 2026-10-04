// NereusSDR for iOS: the speaker on the card and the opened island: mute this phone
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AppIntents
import SwiftUI

/// The card's speaker (the board's `.la__btn`): lit amber while this phone
/// is muted; a tap asks for the other state.
struct ActivityMuteButton: View {
    let muted: Bool

    var body: some View {
        Button(intent: MuteIntent(muted: !muted)) {
            Image(systemName: muted ? "speaker.slash" : "speaker.wave.1")
                .font(.system(size: 17, weight: .semibold))
                .foregroundStyle(muted ? ActivityColours.mutedEdge : ActivityColours.text)
                .frame(width: 46, height: 36)
                .background(muted ? ActivityColours.mutedFill : ActivityColours.button,
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(muted ? ActivityColours.mutedEdge : ActivityColours.buttonEdge, lineWidth: 1))
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Mute this phone")
        .accessibilityValue(muted ? "Muted" : "Sound on")
    }
}
