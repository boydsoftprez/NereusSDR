// NereusSDR for iOS: zoom minus and plus, at the bottom right of the waterfall
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Zoom minus and plus (spec section 5.1 item 3), side by side in one box
/// at the bottom right of the waterfall, as the board draws them. Minus
/// widens the band's view, plus closes in; the bottom left stays clear for
/// PTT.
struct ZoomButtons: View {
    let zoomOut: () -> Void
    let zoomIn: () -> Void

    /// Each button's size, and the box's inset from the waterfall's corner.
    static let buttonSize = CGSize(width: 50, height: 42)
    static let inset: CGFloat = 12
    static var size: CGSize { CGSize(width: buttonSize.width * 2 + 1, height: buttonSize.height) }

    var body: some View {
        HStack(spacing: 0) {
            button("\u{2212}", label: "Zoom out", action: zoomOut)
            Rectangle().fill(BandColours.zoomBorder).frame(width: 1)
            button("+", label: "Zoom in", action: zoomIn)
        }
        .frame(height: Self.buttonSize.height)
        .background(BandColours.zoomBackground, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(BandColours.zoomBorder, lineWidth: 1))
        .clipShape(RoundedRectangle(cornerRadius: 4))
    }

    private func button(_ glyph: String, label: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(glyph)
                .font(.system(size: 22, weight: .bold))
                .foregroundStyle(BandColours.text)
                .frame(width: Self.buttonSize.width, height: Self.buttonSize.height)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(label)
    }
}
