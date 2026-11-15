// NereusSDR for iOS: the RXL tag on the flag: how far RXL turns the station up or down
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-15: New, the phone's copy of the desktop flag's RXL tag. J.J. Boyd
// (KG4VCF), AI-assisted via Claude Code.

import SwiftUI

/// "RXL +12 dB" in a thin box, as the desktop flag shows it at the end of
/// its "You control" line (JJ, 2026-11-15): whole dB with a sign, a real
/// minus sign for a cut; green while RXL turns the station up, amber while
/// it turns it down, dim at 0. Shown while RXL is on in a mode it levels
/// (``BandSlicesModel/Entry/rxlLiftDb``).
struct RxlLiftTag: View {
    let liftDb: Double
    var points: CGFloat = 10

    static let lift = WorkspacePalette.liveGreen
    static let cut = WorkspacePalette.amberText
    static let level = ChromeColours.textDim

    /// The whole dB the tag shows.
    static func whole(_ dB: Double) -> Int {
        dB.isFinite ? Int(dB.rounded()) : 0
    }

    /// "RXL +12 dB", "RXL −5 dB" (U+2212), "RXL 0 dB".
    static func text(_ dB: Double) -> String {
        let value = whole(dB)
        let sign = value > 0 ? "+" : value < 0 ? "\u{2212}" : ""
        return "RXL \(sign)\(abs(value)) dB"
    }

    /// What VoiceOver says.
    static func spoken(_ dB: Double) -> String {
        let value = whole(dB)
        return value > 0 ? "RXL turning this station up \(value) dB"
            : value < 0 ? "RXL turning this station down \(-value) dB"
            : "RXL leaving this station's level as it is"
    }

    static func ink(_ dB: Double) -> Color {
        let value = whole(dB)
        return value > 0 ? lift : value < 0 ? cut : level
    }

    var body: some View {
        let ink = Self.ink(liftDb)
        Text(Self.text(liftDb))
            .font(.system(size: points, weight: .bold).monospacedDigit())
            .foregroundStyle(ink)
            .lineLimit(1)
            .fixedSize()
            .padding(.horizontal, 4)
            .padding(.vertical, 1)
            .overlay(RoundedRectangle(cornerRadius: 3) // kept-radius: flag
                .strokeBorder(ink.opacity(Self.whole(liftDb) == 0 ? 0.4 : 0.6), lineWidth: 1))
            .accessibilityElement()
            .accessibilityLabel(Self.spoken(liftDb))
            .accessibilityIdentifier("flagRxl")
    }
}
