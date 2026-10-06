// NereusSDR for iOS: the speaker's glyph on the toolbar
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The speaker's glyph: a speaker and two sound waves.
struct SpeakerGlyph: Shape {
    func path(in rect: CGRect) -> Path {
        let unit = min(rect.width, rect.height) / 24
        let origin = CGPoint(x: rect.midX - 12 * unit, y: rect.midY - 12 * unit)
        func point(_ x: CGFloat, _ y: CGFloat) -> CGPoint {
            CGPoint(x: origin.x + x * unit, y: origin.y + y * unit)
        }
        var path = Path()
        path.addLines([point(11, 5), point(6, 9), point(2, 9), point(2, 15), point(6, 15), point(11, 19)])
        path.closeSubpath()
        for radius: CGFloat in [5, 10] {
            path.move(to: point(12 + radius * cos(.pi / 4), 12 - radius * sin(.pi / 4)))
            path.addArc(center: point(12, 12), radius: radius * unit, startAngle: .degrees(-45),
                        endAngle: .degrees(45), clockwise: false)
        }
        return path
    }
}
