// NereusSDR for iOS: the tab bar's glyphs: the panadapter trace, the RX/TX box, the wrench, the radio and the gear
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// A tab's glyph, drawn as the board draws it in a 36 by 28 box with round
/// 1.6 strokes (spec section 5.2, board `.tab svg`): the panadapter's trace
/// in a screen, the Modes tab's RX over TX box, a wrench, a radio with its
/// dial and antenna, and a gear. The trace, the box and the radio follow
/// the board's own drawing; the wrench and the gear are drawn here for the
/// app (the board's two come from an icon set the app does not carry).
struct TabGlyph: View {
    let tab: AppTab

    /// The drawing's box, in its own units.
    static let box = CGSize(width: 36, height: 28)
    static let lineWidth: CGFloat = 1.6

    var body: some View {
        Canvas { context, size in
            let scale = min(size.width / Self.box.width, size.height / Self.box.height)
            let origin = CGPoint(x: (size.width - Self.box.width * scale) / 2,
                                 y: (size.height - Self.box.height * scale) / 2)
            context.translateBy(x: origin.x, y: origin.y)
            context.scaleBy(x: scale, y: scale)
            let style = StrokeStyle(lineWidth: Self.lineWidth, lineCap: .round, lineJoin: .round)
            context.stroke(Self.path(for: tab), with: .foreground, style: style)
            if tab == .modes {
                for (word, y) in [("RX", 7.5), ("TX", 20.0)] {
                    context.draw(Text(word).font(.system(size: 8.5, weight: .heavy)).foregroundStyle(.foreground),
                                 at: CGPoint(x: 18, y: y), anchor: .center)
                }
            }
        }
        .accessibilityHidden(true)
    }

    /// The glyph's strokes in the 36 by 28 box.
    static func path(for tab: AppTab) -> Path {
        var path = Path()
        switch tab {
        case .panadapter:
            path.addRoundedRect(in: CGRect(x: 1.5, y: 1.5, width: 33, height: 25), cornerSize: CGSize(width: 3, height: 3))
            let trace: [(CGFloat, CGFloat)] = [
                (4, 21), (5, 20), (6, 21), (7, 18), (8, 21), (9, 20), (10, 17), (11, 11), (12, 16), (13, 21),
                (14, 20), (15, 21), (16, 15), (17, 21), (18, 20), (19, 18), (20, 21), (21, 19), (22, 11), (23, 5),
                (24, 10), (25, 20), (26, 21), (27, 20), (28, 16), (29, 21), (30, 20), (32, 21),
            ]
            path.addLines(trace.map { CGPoint(x: $0.0, y: $0.1) })
        case .modes:
            path.addRoundedRect(in: CGRect(x: 7, y: 1.5, width: 22, height: 25), cornerSize: CGSize(width: 3, height: 3))
            path.move(to: CGPoint(x: 7, y: 14))
            path.addLine(to: CGPoint(x: 29, y: 14))
        case .tools:
            wrench(&path)
        case .radio:
            path.addRoundedRect(in: CGRect(x: 3, y: 8, width: 30, height: 16), cornerSize: CGSize(width: 2.5, height: 2.5))
            path.addEllipse(in: CGRect(x: 21, y: 12, width: 8, height: 8))
            for (y, length) in [(13.0, 9.0), (16.5, 9.0), (20.0, 6.0)] {
                path.move(to: CGPoint(x: 8, y: y))
                path.addLine(to: CGPoint(x: 8 + length, y: y))
            }
            path.move(to: CGPoint(x: 9, y: 8))
            path.addLine(to: CGPoint(x: 23, y: 2))
        case .setup:
            gear(&path)
        }
        return path
    }

    /// A wrench across the box, lower left to upper right: an open jaw at
    /// the top end and a rounded handle.
    private static func wrench(_ path: inout Path) {
        let centre = CGPoint(x: 23.5, y: 8.5)
        let radius: CGFloat = 5.5
        let axis = CGFloat.pi * 3 / 4   // from the head down the handle
        let half: CGFloat = 1.9         // half the handle's width
        let jaw: CGFloat = 1.8          // half the jaw's opening
        let direction = CGPoint(x: cos(axis), y: sin(axis))
        let normal = CGPoint(x: -direction.y, y: direction.x)
        let tail = CGPoint(x: 9, y: 23)
        func along(_ point: CGPoint, _ distance: CGFloat, _ side: CGFloat) -> CGPoint {
            CGPoint(x: point.x + direction.x * distance + normal.x * side,
                    y: point.y + direction.y * distance + normal.y * side)
        }
        // Where the handle's edges meet the head's circle.
        let meet = sqrt(radius * radius - half * half)
        let leftJoin = along(centre, meet, half)
        let rightJoin = along(centre, meet, -half)
        let angleLeft = atan2(leftJoin.y - centre.y, leftJoin.x - centre.x)
        let angleRight = atan2(rightJoin.y - centre.y, rightJoin.x - centre.x)
        // The jaw opens away from the handle.
        let open = axis + .pi
        let jawAngle = asin(jaw / radius)
        path.move(to: along(tail, 0, half))
        path.addLine(to: leftJoin)
        path.addArc(center: centre, radius: radius, startAngle: .radians(Double(angleLeft)),
                    endAngle: .radians(Double(open - jawAngle)), clockwise: false)
        let jawDepth: CGFloat = 3.2
        let jawLeft = CGPoint(x: centre.x + cos(open - jawAngle) * radius, y: centre.y + sin(open - jawAngle) * radius)
        let jawRight = CGPoint(x: centre.x + cos(open + jawAngle) * radius, y: centre.y + sin(open + jawAngle) * radius)
        path.addLine(to: CGPoint(x: jawLeft.x + direction.x * jawDepth, y: jawLeft.y + direction.y * jawDepth))
        path.addLine(to: CGPoint(x: jawRight.x + direction.x * jawDepth, y: jawRight.y + direction.y * jawDepth))
        path.addLine(to: jawRight)
        path.addArc(center: centre, radius: radius, startAngle: .radians(Double(open + jawAngle)),
                    endAngle: .radians(Double(angleRight) + 2 * .pi), clockwise: false)
        path.addLine(to: along(tail, 0, -half))
        path.addArc(center: tail, radius: half, startAngle: .radians(Double(axis) - .pi / 2),
                    endAngle: .radians(Double(axis) + .pi / 2), clockwise: false)
        path.closeSubpath()
    }

    /// A gear of eight teeth round a hub.
    private static func gear(_ path: inout Path) {
        let centre = CGPoint(x: 18, y: 14)
        let outer: CGFloat = 10.5
        let inner: CGFloat = 8
        let teeth = 8
        let step = 2 * CGFloat.pi / CGFloat(teeth)
        let half = step * 0.22
        for index in 0..<teeth {
            let middle = CGFloat(index) * step
            if index == 0 {
                path.move(to: point(centre, inner, middle - half))
            }
            path.addLine(to: point(centre, outer, middle - half * 0.7))
            path.addLine(to: point(centre, outer, middle + half * 0.7))
            path.addLine(to: point(centre, inner, middle + half))
            path.addArc(center: centre, radius: inner, startAngle: .radians(Double(middle + half)),
                        endAngle: .radians(Double(middle + step - half)), clockwise: false)
        }
        path.closeSubpath()
        path.addEllipse(in: CGRect(x: centre.x - 3, y: centre.y - 3, width: 6, height: 6))
    }

    private static func point(_ centre: CGPoint, _ radius: CGFloat, _ angle: CGFloat) -> CGPoint {
        CGPoint(x: centre.x + cos(angle) * radius, y: centre.y + sin(angle) * radius)
    }
}
