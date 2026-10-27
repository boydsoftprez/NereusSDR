// NereusSDR for iOS: a pan whose display the Core refused says so in the Core's words
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// Over a pan's band, where the 3D notice sits (``StackNotice``): when the
/// Core refused this pan's display, its words in the house card, so the pan
/// says why it is blank rather than staying so. It holds no control; the
/// band's own gestures pass through it, and closing the pan clears it.
struct PanDisplayRefusalNotice: View {
    @ObservedObject var stack: PanStackModel
    @ObservedObject var band: BandModel
    let pan: PanIdentity
    var sideways = false
    /// The band's edges the column and the dB scale cover.
    var leadingInset: CGFloat = 0
    var trailingInset: CGFloat = 0

    var body: some View {
        if let words = stack.displayRefusals[pan] {
            GeometryReader { proxy in
                let top = band.pointGeometry(size: proxy.size).map { $0.layout.waterfall.minY + 8 } ?? 8
                Text(words)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.text)
                    .lineSpacing(2)
                    .fixedSize(horizontal: false, vertical: true)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 10)
                    .background(StackNotice.card, in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
                    .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius)
                        .strokeBorder(StackNotice.cardEdge, lineWidth: 1))
                    .accessibilityElement(children: .combine)
                    .accessibilityIdentifier("panDisplayRefusal")
                    .frame(maxWidth: sideways ? 546 : .infinity)
                    // Clear of the band's button column, which sits over the band's leading edge.
                    .padding(.leading, leadingInset + BandColumnLayout.origin
                             + BandColumnLayout.columnWidth(columns: BandColumnLayout.columns(bandHeight: proxy.size.height))
                             + BandColumnLayout.clearance)
                    .padding(.trailing, trailingInset + 10)
                    .offset(y: top)
                    .allowsHitTesting(false)
            }
        }
    }
}
