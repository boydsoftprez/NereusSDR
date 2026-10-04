// NereusSDR for iOS: the S-meter's Classic face: a shallow arc, the S scale outside and the TX Mode's scale inside
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The Classic (flat) face (D86): a shallow 70° arc across the meter, the
/// receive scale outside it and the TX Mode's scale inside, the needle
/// rising from just under the meter's foot, and the readouts along the top.
/// The geometry, colours and sizes are the desktop's (D83); the drawing
/// is the phone's own.
struct ClassicMeterFace: View {
    let display: SMeterDisplay
    /// Where the needle is drawn, 0 to 1.
    let needle: Double

    // The desktop's colours.
    nonisolated static let background = Color(hex: 0x0F0F1A)
    nonisolated static let scale = Color(hex: 0xC8D8E8)
    nonisolated static let red = Color(hex: 0xFF4444)
    nonisolated static let blue = Color(hex: 0x0080D0)
    nonisolated static let caption = Color(hex: 0x8090A0)
    nonisolated static let reading = Color(hex: 0x00B4D8)
    nonisolated static let peak = Color(hex: 0xFFAA00)

    /// The arc's ends, in degrees counterclockwise from the right.
    nonisolated static let arcRightDegrees = 55.0
    nonisolated static let arcLeftDegrees = 125.0
    /// The inner (TX) arc sits this far inside the outer one.
    nonisolated static let arcGap: CGFloat = 6

    var body: some View {
        Canvas { context, size in
            draw(in: &context, size: size)
        }
        .background(Self.background)
    }

    /// The arc for a meter `size`: the centre far below, so the arc is shallow.
    struct Geometry: Equatable {
        let size: CGSize

        var radius: CGFloat { size.width * 0.85 }
        var centre: CGPoint { CGPoint(x: size.width / 2, y: size.height + radius - size.height * 0.65) }
        /// Where the needle turns: just under the meter.
        var pivot: CGPoint { CGPoint(x: size.width / 2, y: size.height + 6) }

        /// The angle for `fraction`, radians counterclockwise from the right.
        func angle(_ fraction: Double) -> Double {
            let left = ClassicMeterFace.arcLeftDegrees * .pi / 180
            let right = ClassicMeterFace.arcRightDegrees * .pi / 180
            return left - fraction * (left - right)
        }

        func point(_ fraction: Double, radius: CGFloat) -> CGPoint {
            let angle = angle(fraction)
            return CGPoint(x: centre.x + radius * cos(angle), y: centre.y - radius * sin(angle))
        }

        /// The unit direction from the pivot through the arc at `fraction`.
        func outward(_ fraction: Double) -> CGVector {
            let onArc = point(fraction, radius: radius)
            let dx = onArc.x - centre.x
            let dy = onArc.y - pivot.y
            let length = max(sqrt(dx * dx + dy * dy), 0.001)
            return CGVector(dx: dx / length, dy: dy / length)
        }

        func arc(radius: CGFloat, from start: Double, to end: Double) -> Path {
            var path = Path()
            let steps = max(Int((end - start) * 80), 1)
            for step in 0 ... steps {
                let at = point(start + (end - start) * Double(step) / Double(steps), radius: radius)
                if step == 0 {
                    path.move(to: at)
                } else {
                    path.addLine(to: at)
                }
            }
            return path
        }
    }

    private func draw(in context: inout GraphicsContext, size: CGSize) {
        let geometry = Geometry(size: size)
        let radius = geometry.radius
        let inner = radius - Self.arcGap
        let tickFont = Font.system(size: max(10, size.height / 10), weight: .bold)

        // The outer arc: the receive scale, red from S9.
        let receiveRed = display.receiveScale?.redFrom ?? 1
        context.stroke(geometry.arc(radius: radius, from: 0, to: receiveRed), with: .color(Self.scale), lineWidth: 3)
        context.stroke(geometry.arc(radius: radius, from: receiveRed, to: 1), with: .color(Self.red), lineWidth: 3)
        // The inner arc: the TX Mode's scale, blue, red from its red point.
        let transmitRed = display.transmitScale?.redFrom ?? 1
        context.stroke(geometry.arc(radius: inner, from: 0, to: transmitRed), with: .color(Self.blue), lineWidth: 3)
        if transmitRed < 1 {
            context.stroke(geometry.arc(radius: inner, from: transmitRed, to: 1), with: .color(Self.red), lineWidth: 3)
        }

        for tick in display.receiveScale?.ticks ?? [] {
            let colour = tick.red ? Self.red : Self.scale
            let out = geometry.outward(tick.fraction)
            let onArc = geometry.point(tick.fraction, radius: radius)
            var line = Path()
            line.move(to: onArc.offset(out, 2))
            line.addLine(to: onArc.offset(out, 14))
            context.stroke(line, with: .color(colour), lineWidth: 1.5)
            if let label = tick.label {
                context.draw(Text(label).font(tickFont).foregroundStyle(colour), at: onArc.offset(out, 26))
            }
        }
        for tick in display.transmitScale?.ticks ?? [] {
            let out = geometry.outward(tick.fraction)
            let onArc = geometry.point(tick.fraction, radius: inner)
            var line = Path()
            line.move(to: onArc.offset(out, -2))
            line.addLine(to: onArc.offset(out, -14))
            context.stroke(line, with: .color(tick.red ? Self.red : Self.blue), lineWidth: 1.5)
            if let label = tick.label {
                context.draw(Text(label).font(tickFont).foregroundStyle(tick.red ? Self.red : Self.scale),
                             at: onArc.offset(out, -26))
            }
        }

        // The needle and its shadow, out to the end of the outer ticks.
        let tip = geometry.point(needle, radius: radius + 14)
        var shadow = Path()
        shadow.move(to: CGPoint(x: geometry.pivot.x + 1, y: geometry.pivot.y + 1))
        shadow.addLine(to: CGPoint(x: tip.x + 1, y: tip.y + 1))
        context.stroke(shadow, with: .color(.black.opacity(80.0 / 255)), lineWidth: 3)
        var pointer = Path()
        pointer.move(to: geometry.pivot)
        pointer.addLine(to: tip)
        context.stroke(pointer, with: .color(.white), lineWidth: 2)

        // Signal Peak's marker: a small triangle on the arc's inside, pointing out.
        if let peak = display.peakMarker {
            let angle = geometry.angle(peak)
            let (cosA, sinA) = (cos(angle), sin(angle))
            let point = geometry.point(peak, radius: radius - 2)
            var triangle = Path()
            triangle.move(to: point)
            triangle.addLine(to: CGPoint(x: point.x - 6 * cosA - 3 * sinA, y: point.y + 6 * sinA + 3 * cosA))
            triangle.addLine(to: CGPoint(x: point.x - 6 * cosA + 3 * sinA, y: point.y + 6 * sinA - 3 * cosA))
            triangle.closeSubpath()
            context.fill(triangle, with: .color(Self.peak))
        }
        // The peak hold line across the outer arc.
        if let hold = display.holdLine {
            var line = Path()
            line.move(to: geometry.point(hold, radius: radius - 4))
            line.addLine(to: geometry.point(hold, radius: radius + 10))
            context.stroke(line, with: .color(Self.red.opacity(0xCC / 255.0)), lineWidth: 2)
        }

        // The readouts along the top, on one baseline under the caption's
        // line: S-units (or TX) on the left, the caption, the value.
        let captionSize = max(9, size.height / 14)
        let valueSize = max(13, size.height / 8)
        let baseline = captionSize * 1.2 + 2
        context.draw(Text(display.caption).font(.system(size: captionSize)).foregroundStyle(Self.caption),
                     at: CGPoint(x: size.width / 2, y: baseline + captionSize * 0.22), anchor: .bottom)
        context.draw(Text(display.left).font(.system(size: valueSize, weight: .bold)).foregroundStyle(Self.reading),
                     at: CGPoint(x: 6, y: baseline + valueSize * 0.22), anchor: .bottomLeading)
        context.draw(Text(display.right).font(.system(size: valueSize, weight: .bold)).foregroundStyle(Self.scale),
                     at: CGPoint(x: size.width - 6, y: baseline + valueSize * 0.22), anchor: .bottomTrailing)
    }
}
