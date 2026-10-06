// NereusSDR for iOS: the red TX pill in a tab's title bar while this phone transmits, with its clock and Stop
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import SwiftUI

/// While a key of this phone's is on, every tab's title bar carries the TX
/// pill (spec section 5.1 item 12, the board's `.txpill`): a red dot, TX
/// and the transmission's clock, and Stop, which ends everything of this
/// phone's on the air in one tap.
struct TxPill: View {
    @ObservedObject var transmit: TransmitModel

    var body: some View {
        if transmit.ptt.transmitting {
            TimelineView(.periodic(from: .now, by: 1)) { _ in
                Button {
                    transmit.stopAll()
                } label: {
                    HStack(spacing: 6) {
                        Circle().fill(ChromeColours.pttEdge).frame(width: 8, height: 8)
                        Text("TX " + PttButton.clock(seconds: seconds))
                            .font(.system(size: 12, weight: .bold, design: .monospaced))
                            .foregroundStyle(ChromeColours.txPillText)
                        Text("Stop")
                            .font(.system(size: 11, weight: .bold))
                            .foregroundStyle(ChromeColours.txPillStop)
                    }
                    .padding(.horizontal, 10)
                    .frame(height: 30)
                    .background(ChromeColours.txPill, in: Capsule())
                    .overlay(Capsule().strokeBorder(ChromeColours.txPillBorder, lineWidth: 1))
                    .contentShape(Capsule())
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Transmitting, " + PttButton.clock(seconds: seconds))
                .accessibilityHint("Stops transmitting")
                .accessibilityIdentifier("txPill")
            }
        }
    }

    private var seconds: Int {
        if case .keyed(let since) = transmit.ptt.state {
            return Int((ContinuousClock.now - since).components.seconds)
        }
        return Int(transmit.keyedForSeconds)
    }
}
