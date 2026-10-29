// NereusSDR for iOS: the red TX pill in a tab's title bar while this phone transmits, with its clock and Stop
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-27: Remote transmit hold then cut: the pill reads "Held" in amber
//   while the Core holds RF off for this phone's key.
//   J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import NereusLink
import SwiftUI

/// While a key of this phone's is on, every tab's title bar carries the TX
/// pill (spec section 5.1 item 12, the board's `.txpill`): a red dot, TX
/// and the transmission's clock, and Stop, which ends everything of this
/// phone's on the air in one tap. While the Core holds RF off for the key
/// (spec section 5.3) it reads "Held" in amber; the clock keeps counting
/// from key-down and Stop still ends everything.
struct TxPill: View {
    @ObservedObject var transmit: TransmitModel

    var body: some View {
        if transmit.ptt.transmitting {
            TimelineView(.periodic(from: .now, by: 1)) { _ in
                Button {
                    transmit.stopAll()
                } label: {
                    let held = transmit.holdShown
                    HStack(spacing: 6) {
                        Circle().fill(held ? ChromeColours.buttonOnAmberText : ChromeColours.pttEdge)
                            .frame(width: 8, height: 8)
                        Text(Self.title(held: held, seconds: seconds))
                            .font(.system(size: 12, weight: .bold, design: .monospaced))
                            .foregroundStyle(held ? ChromeColours.buttonOnAmberText : ChromeColours.txPillText)
                        Text("Stop")
                            .font(.system(size: 11, weight: .bold))
                            .foregroundStyle(held ? ChromeColours.buttonOnAmberText : ChromeColours.txPillStop)
                    }
                    .padding(.horizontal, 10)
                    .frame(height: 30)
                    .background(held ? ChromeColours.buttonOnAmber : ChromeColours.txPill, in: Capsule())
                    .overlay(Capsule().strokeBorder(held ? ChromeColours.buttonOnAmberBorder : ChromeColours.txPillBorder,
                                                    lineWidth: 1))
                    .contentShape(Capsule())
                }
                .buttonStyle(.plain)
                .accessibilityLabel(Self.spoken(held: transmit.holdShown, reason: transmit.holdShownReason,
                                                seconds: seconds))
                .accessibilityHint("Stops transmitting")
                .accessibilityIdentifier("txPill")
            }
        }
    }

    /// The pill's words: "TX" or "Held", then the clock from key-down.
    static func title(held: Bool, seconds: Int) -> String {
        (held ? "Held " : "TX ") + PttButton.clock(seconds: seconds)
    }

    /// What VoiceOver says. A hold shown after it ended has no reason, so
    /// it names none.
    static func spoken(held: Bool, reason: String, seconds: Int) -> String {
        let clock = PttButton.clock(seconds: seconds)
        guard held else {
            return "Transmitting, " + clock
        }
        switch reason {
        case TransmitStateReport.linkReason:
            return "Transmitting, RF held while the link is quiet, " + clock
        case TransmitStateReport.micReason:
            return "Transmitting, RF held while no microphone audio arrives, " + clock
        default:
            return "Transmitting, RF held for a moment, " + clock
        }
    }

    private var seconds: Int {
        if case .keyed(let since) = transmit.ptt.state {
            return Int((ContinuousClock.now - since).components.seconds)
        }
        return Int(transmit.keyedForSeconds)
    }
}
