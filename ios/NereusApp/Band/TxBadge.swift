// NereusSDR for iOS: the TX badge on a flag and a folded tag, lit only on the slice this device transmits on
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The TX badge (R-IOS-11): lit only while the slice's `txSlice` is true,
/// which the Core sets only while this device holds transmit. With nobody
/// holding transmit, no slice shows it lit.
struct TxBadge: View {
    let lit: Bool

    var body: some View {
        Text("TX")
            .font(.system(size: 10, weight: .bold))
            .foregroundStyle(lit ? BandColours.txBadgeOnText : BandColours.txBadgeOffText)
            .frame(width: 28, height: 18)
            .background(lit ? BandColours.txBadgeOn : BandColours.txBadgeOff, in: RoundedRectangle(cornerRadius: 3))
            .overlay(RoundedRectangle(cornerRadius: 3)
                .strokeBorder(lit ? BandColours.txBadgeOnBorder : BandColours.txBadgeOffBorder, lineWidth: 1))
            .accessibilityLabel(lit ? "Transmit slice" : "Not the transmit slice")
    }
}
