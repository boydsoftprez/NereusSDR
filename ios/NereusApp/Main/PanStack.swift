// NereusSDR for iOS: the iPad's open pans stacked in the band's place, Pan 1 on top, each sharing the height equally
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The iPad's pans in the band's place (R-IOS-59, DC9, spec section 6.3,
/// the board's "Two pans, two pills"): every open pan gets an equal share
/// of the height, Pan 1 on top, a hairline between them. Each pan draws
/// with its own band, column and pill (`content`); any touch on a pan makes
/// it the pan last touched, which the toolbar follows.
struct PanStack<Content: View>: View {
    @ObservedObject var stack: PanStackModel
    @ViewBuilder let content: (PanIdentity) -> Content

    var body: some View {
        VStack(spacing: 0) {
            ForEach(Array(stack.open.enumerated()), id: \.element) { index, pan in
                if index > 0 {
                    Rectangle()
                        .fill(ChromeColours.barBorder)
                        .frame(height: 1)
                        .accessibilityHidden(true)
                }
                content(pan)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .clipped()
                    .contentShape(Rectangle())
                    .simultaneousGesture(TapGesture().onEnded { touch(pan) })
                    .simultaneousGesture(DragGesture(minimumDistance: 0).onChanged { _ in touch(pan) })
                    .accessibilityElement(children: .contain)
                    .accessibilityIdentifier("pan.\(pan.number)")
            }
        }
    }

    private func touch(_ pan: PanIdentity) {
        if stack.lastTouched != pan {
            stack.lastTouched = pan
        }
    }
}
