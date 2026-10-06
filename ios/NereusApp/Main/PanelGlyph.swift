// NereusSDR for iOS: the RX and TX panels' glyph on the toolbar
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The RX or TX panel's glyph: a box with a bar down the panel's side.
struct PanelGlyph: Shape {
    enum Side {
        case leading
        case trailing
    }

    let side: Side

    func path(in rect: CGRect) -> Path {
        let unit = min(rect.width, rect.height) / 24
        let origin = CGPoint(x: rect.midX - 12 * unit, y: rect.midY - 12 * unit)
        func point(_ x: CGFloat, _ y: CGFloat) -> CGPoint {
            CGPoint(x: origin.x + x * unit, y: origin.y + y * unit)
        }
        var path = Path(roundedRect: CGRect(origin: point(3, 4), size: CGSize(width: 18 * unit, height: 16 * unit)),
                        cornerRadius: 2 * unit)
        let x: CGFloat = side == .leading ? 9 : 15
        path.move(to: point(x, 4))
        path.addLine(to: point(x, 20))
        return path
    }
}
