// NereusSDR for iOS: the S-meter's six vintage faces: a bezel, a graduated card, a lance pointer and a pivot cap
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// A vintage panel-meter face (D86): a three-ring bezel round a card with
/// its colour gradient, a mirror strip on some faces, the one scale the
/// needle is on (the S scale receiving, the TX Mode's scale on the air)
/// with its red band, a title over the scale and a legend under it, the
/// readouts either side of the pivot, a lance pointer with its shadow,
/// and the pivot's cap and screw. The proportions are the desktop's,
/// in units where the scale's radius is 250 (D83); the drawing is the
/// phone's own.
struct VintageMeterFace: View {
    let display: SMeterDisplay
    /// Where the needle is drawn, 0 to 1.
    let needle: Double
    let theme: VintageFaceTheme

    /// The scale's sweep, centred on straight up.
    nonisolated static let sweepDegrees = 110.0
    /// The original design's sizes, in units of a 250-unit radius.
    nonisolated static let arcRadiusUnits = 250.0
    nonisolated static let needleUnits = 262.0
    nonisolated static let tailUnits = 18.0
    nonisolated static let arcWidthUnits = 3.0
    nonisolated static let bandWidthUnits = 9.0
    nonisolated static let majorUnits = 22.0
    nonisolated static let majorWidthUnits = 3.0
    nonisolated static let minorUnits = 12.0
    nonisolated static let minorWidthUnits = 2.0
    nonisolated static let labelPadUnits = 8.0
    nonisolated static let mirrorUnits = 6.0
    nonisolated static let mirrorLineUnits = 12.0
    nonisolated static let capUnits = 26.0
    nonisolated static let screwUnits = 11.0
    nonisolated static let surround = Color(hex: 0x0F0F1A)
    /// The lettering's typeface, a geometric sans like the desktop's.
    nonisolated static let typeface = "Futura-Medium"
    nonisolated static let boldTypeface = "Futura-Bold"

    var body: some View {
        Canvas { context, size in
            draw(in: &context, size: size)
        }
        .background(Self.surround)
    }

    /// The card, the pivot and the scale's radius for a meter `size`.
    struct Geometry: Equatable {
        let bounds: CGRect
        let bezel: CGFloat
        let face: CGRect
        let radius: CGFloat
        let unit: CGFloat
        let pivot: CGPoint

        init(size: CGSize) {
            bounds = CGRect(origin: .zero, size: size).insetBy(dx: 1, dy: 1)
            bezel = min(max(bounds.height * 0.06, 5), 14)
            face = bounds.insetBy(dx: bezel, dy: bezel)
            let halfSine = sin(VintageMeterFace.sweepDegrees / 2 * .pi / 180)
            let outer = (VintageMeterFace.arcRadiusUnits + VintageMeterFace.mirrorLineUnits + 4)
                / VintageMeterFace.arcRadiusUnits
            let byHeight = 0.72 * face.height
            let byWidth = (0.5 * face.width - 4) / (halfSine * outer)
            radius = max(20, min(byHeight, byWidth))
            unit = radius / VintageMeterFace.arcRadiusUnits
            pivot = CGPoint(x: face.midX, y: face.minY + 0.915 * face.height)
        }

        /// The angle for `fraction` in degrees from straight up, left negative.
        static func degrees(_ fraction: Double) -> Double {
            -VintageMeterFace.sweepDegrees / 2 + min(max(fraction, 0), 1) * VintageMeterFace.sweepDegrees
        }

        func point(_ fraction: Double, radius r: CGFloat) -> CGPoint {
            point(degrees: Self.degrees(fraction), radius: r)
        }

        func point(degrees: Double, radius r: CGFloat) -> CGPoint {
            let angle = degrees * .pi / 180
            return CGPoint(x: pivot.x + r * sin(angle), y: pivot.y - r * cos(angle))
        }

        func arc(radius r: CGFloat, fromDegrees start: Double, toDegrees end: Double) -> Path {
            var path = Path()
            let steps = max(Int(abs(end - start)), 2)
            for step in 0 ... steps {
                let at = point(degrees: start + (end - start) * Double(step) / Double(steps), radius: r)
                if step == 0 {
                    path.move(to: at)
                } else {
                    path.addLine(to: at)
                }
            }
            return path
        }

        func arc(radius r: CGFloat, from start: Double, to end: Double) -> Path {
            arc(radius: r, fromDegrees: Self.degrees(start), toDegrees: Self.degrees(end))
        }
    }

    private func colour(_ hex: UInt32, _ alpha: Double = 1) -> Color {
        Color(hex: hex, opacity: alpha)
    }

    private func font(_ size: CGFloat, bold: Bool) -> Font {
        .custom(bold ? Self.boldTypeface : Self.typeface, fixedSize: max(6, size.rounded()))
    }

    // MARK: Drawing

    private func draw(in context: inout GraphicsContext, size: CGSize) {
        let g = Geometry(size: size)
        drawBezelAndCard(in: &context, g)
        var card = context
        let cardPath = Path(roundedRect: g.face, cornerRadius: max(4, g.bezel * 1.1))
        card.clip(to: cardPath)
        drawShadowAndMirror(in: &card, g, cardPath: cardPath)
        drawScale(in: &card, g)
        drawLettering(in: &card, g)
        if theme.glare {
            card.fill(cardPath, with: .linearGradient(
                Gradient(colors: [.white.opacity(60.0 / 255), .white.opacity(0)]),
                startPoint: g.face.origin,
                endPoint: CGPoint(x: g.face.minX + g.face.width * 0.25, y: g.face.minY + g.face.height * 0.55)))
        }
        drawReadouts(in: &card, g)
        drawPeaks(in: &card, g)
        drawPointer(in: &card, g)
        drawHub(in: &card, g)
    }

    private func drawBezelAndCard(in context: inout GraphicsContext, _ g: Geometry) {
        let corner = max(4, g.bezel * 1.1)
        // Three rings give the bezel a turned bevel.
        let rings: [(CGFloat, UInt32)] = [(0, theme.bezelDark), (g.bezel * 16 / 29, theme.bezelMid),
                                          (g.bezel * 22 / 29, theme.bezelLight), (g.bezel * 26 / 29, theme.bezelDark)]
        for (inset, hex) in rings {
            let ringCorner = max(corner, corner + g.bezel - inset)
            context.fill(Path(roundedRect: g.bounds.insetBy(dx: inset, dy: inset), cornerRadius: ringCorner),
                         with: .color(colour(hex)))
        }
        context.fill(Path(roundedRect: g.face, cornerRadius: corner),
                     with: .linearGradient(Gradient(colors: [colour(theme.faceTop), colour(theme.faceBottom)]),
                                           startPoint: CGPoint(x: g.face.minX, y: g.face.minY),
                                           endPoint: CGPoint(x: g.face.minX, y: g.face.maxY)))
    }

    private func drawShadowAndMirror(in context: inout GraphicsContext, _ g: Geometry, cardPath: Path) {
        // The bezel stands proud of the card: a soft shadow round its inside edge.
        let depth = max(6, g.face.height * 0.09)
        let steps = 14
        for step in 0 ..< steps {
            context.stroke(cardPath, with: .color(.black.opacity(4.0 / 255)),
                           lineWidth: depth * CGFloat(steps - step) / CGFloat(steps) * 2)
        }
        guard theme.mirrorOpacity > 0 else {
            return
        }
        let strip = Self.mirrorUnits * g.unit
        let over = 2.0
        let start = -Self.sweepDegrees / 2 - over
        let end = Self.sweepDegrees / 2 + over
        context.stroke(g.arc(radius: g.radius + strip / 2, fromDegrees: start, toDegrees: end),
                       with: .color(colour(theme.mirror, Double(theme.mirrorOpacity) / 255)),
                       style: StrokeStyle(lineWidth: strip, lineCap: .butt))
        context.stroke(g.arc(radius: g.radius + Self.mirrorLineUnits * g.unit, fromDegrees: start, toDegrees: end),
                       with: .color(colour(theme.inkSoft, 153.0 / 255)),
                       style: StrokeStyle(lineWidth: max(0.7, g.unit), lineCap: .butt))
    }

    private func drawScale(in context: inout GraphicsContext, _ g: Geometry) {
        guard let scale = display.activeScale else {
            return
        }
        let redFrom = scale.redFrom
        let inRed: (Double) -> Bool = { fraction in
            redFrom.map { fraction >= $0 - 0.0005 } ?? false
        }
        let arcWidth = max(1.3, Self.arcWidthUnits * g.unit)
        let bandWidth = max(3, Self.bandWidthUnits * g.unit)
        context.stroke(g.arc(radius: g.radius - arcWidth / 2, from: 0, to: 1), with: .color(colour(theme.ink)),
                       style: StrokeStyle(lineWidth: arcWidth, lineCap: .butt))
        if let redFrom {
            context.stroke(g.arc(radius: g.radius - bandWidth / 2, from: redFrom, to: 1),
                           with: .color(colour(theme.red)), style: StrokeStyle(lineWidth: bandWidth, lineCap: .butt))
        }
        let major = Self.majorUnits * g.unit
        let minor = Self.minorUnits * g.unit
        let labelSize = max(9, 30 * g.unit)
        let labelRadius = g.radius - major - Self.labelPadUnits * g.unit - labelSize * 0.7 * 0.75
        for tick in scale.ticks {
            let red = inRed(tick.fraction)
            let ink = red ? colour(theme.red) : colour(tick.major ? theme.ink : theme.inkSoft)
            let width = tick.major ? max(1.3, Self.majorWidthUnits * g.unit) : max(0.9, Self.minorWidthUnits * g.unit)
            var line = Path()
            line.move(to: g.point(tick.fraction, radius: g.radius))
            line.addLine(to: g.point(tick.fraction, radius: g.radius - (tick.major ? major : minor)))
            context.stroke(line, with: .color(ink), style: StrokeStyle(lineWidth: width, lineCap: .butt))
            if let label = tick.label {
                context.draw(Text(label).font(font(labelSize, bold: false))
                                .foregroundStyle(red ? colour(theme.red) : colour(theme.ink)),
                             at: g.point(tick.fraction, radius: labelRadius))
            }
        }
    }

    private func drawLettering(in context: inout GraphicsContext, _ g: Geometry) {
        let ink = colour(theme.ink)
        let arcTop = g.pivot.y - g.radius - Self.mirrorLineUnits * g.unit
        let title = fitted(display.title, size: max(9, g.face.height * 0.105), bold: true, spacing: 0.14,
                           width: g.face.width * 0.9, smallest: 7, in: context, ink: ink)
        context.draw(title, at: CGPoint(x: g.face.midX, y: (g.face.minY + arcTop) / 2 + 1))
        let legend = fitted(display.legend, size: max(8, 25 * g.unit), bold: false, spacing: 0.12,
                            width: g.radius * 1.1, smallest: 6, in: context, ink: ink)
        context.draw(legend, at: CGPoint(x: g.pivot.x, y: g.pivot.y - 0.5 * g.radius))
    }

    /// Lettering at `size`, shrunk a point at a time until it fits `width`.
    private func fitted(_ text: String, size: CGFloat, bold: Bool, spacing: CGFloat, width: CGFloat,
                        smallest: CGFloat, in context: GraphicsContext, ink: Color) -> GraphicsContext.ResolvedText {
        var points = size.rounded()
        while true {
            let resolved = context.resolve(Text(text).font(font(points, bold: bold)).tracking(points * spacing * 0.5)
                                            .foregroundStyle(ink))
            let measured = resolved.measure(in: CGSize(width: 10_000, height: 1_000))
            if measured.width <= width || points <= smallest {
                return resolved
            }
            points -= 1
        }
    }

    private func drawReadouts(in context: inout GraphicsContext, _ g: Geometry) {
        let gap = max(12, 44 * g.unit)
        let size = max(11, g.face.height * 0.115)
        let middle = g.pivot.y - size * 0.18
        let leftInk = display.transmitting ? colour(theme.red) : colour(theme.ink)
        context.draw(Text(display.left).font(font(size, bold: true)).foregroundStyle(leftInk),
                     at: CGPoint(x: g.pivot.x - gap, y: middle), anchor: .trailing)
        context.draw(Text(display.right).font(font(size * 0.86, bold: false)).foregroundStyle(colour(theme.ink)),
                     at: CGPoint(x: g.pivot.x + gap, y: middle), anchor: .leading)
    }

    private func drawPeaks(in context: inout GraphicsContext, _ g: Geometry) {
        if let peak = display.peakMarker {
            // A wedge riding outside the arc, pointing in at the held peak.
            let angle = Geometry.degrees(peak) * .pi / 180
            let radial = CGVector(dx: sin(angle), dy: -cos(angle))
            let across = CGVector(dx: cos(angle), dy: sin(angle))
            let tip = g.point(peak, radius: g.radius + g.unit)
            let base = tip.offset(radial, max(5, 13 * g.unit))
            let half = max(2.5, 5.5 * g.unit)
            var wedge = Path()
            wedge.move(to: tip)
            wedge.addLine(to: base.offset(across, half))
            wedge.addLine(to: base.offset(across, -half))
            wedge.closeSubpath()
            context.fill(wedge, with: .color(colour(theme.yellow)))
            context.stroke(wedge, with: .color(colour(theme.ink)), lineWidth: 0.6)
        }
        if let hold = display.holdLine {
            var line = Path()
            line.move(to: g.point(hold, radius: g.radius + 1.5 * g.unit))
            line.addLine(to: g.point(hold, radius: g.radius + 15 * g.unit))
            context.stroke(line, with: .color(colour(theme.red)),
                           style: StrokeStyle(lineWidth: max(1.6, 3 * g.unit), lineCap: .butt))
        }
    }

    private func drawPointer(in context: inout GraphicsContext, _ g: Geometry) {
        // The lance: a fine shaft to 40% of its length carrying a spear that
        // opens from 28%, is widest at 56% and runs out to the tip. Drawn
        // along +x, then turned about the pivot.
        let length = Self.needleUnits * g.unit
        let tail = Self.tailUnits * g.unit
        let shaft = max(0.6, g.unit)
        let spear = max(2.2, 6 * g.unit)
        var lance = Path(CGRect(x: -tail, y: -shaft, width: tail + 0.40 * length, height: 2 * shaft))
        lance.move(to: CGPoint(x: 0.28 * length, y: 0))
        lance.addLine(to: CGPoint(x: 0.56 * length, y: -spear))
        lance.addLine(to: CGPoint(x: length, y: 0))
        lance.addLine(to: CGPoint(x: 0.56 * length, y: spear))
        lance.closeSubpath()
        let turn = Angle.degrees(Geometry.degrees(needle) - 90)
        let shadowOffset = CGSize(width: max(1.2, 3 * g.unit), height: max(2, 5 * g.unit))
        let shadow = lance.applying(CGAffineTransform(rotationAngle: turn.radians)
            .concatenating(CGAffineTransform(translationX: g.pivot.x + shadowOffset.width,
                                             y: g.pivot.y + shadowOffset.height)))
        context.fill(shadow, with: .color(.black.opacity(51.0 / 255)))
        let pointer = lance.applying(CGAffineTransform(rotationAngle: turn.radians)
            .concatenating(CGAffineTransform(translationX: g.pivot.x, y: g.pivot.y)))
        context.fill(pointer, with: .color(colour(theme.pointer)))
    }

    private func drawHub(in context: inout GraphicsContext, _ g: Geometry) {
        let cap = max(6, Self.capUnits * g.unit)
        let screw = max(2.6, Self.screwUnits * g.unit)
        let shadowCentre = CGPoint(x: g.pivot.x, y: g.pivot.y + cap * 0.12)
        context.fill(Path(ellipseIn: CGRect(x: shadowCentre.x - cap * 1.04, y: shadowCentre.y - cap * 1.04,
                                            width: cap * 2.08, height: cap * 2.08)),
                     with: .color(.black.opacity(45.0 / 255)))
        context.fill(Path(ellipseIn: CGRect(x: g.pivot.x - cap, y: g.pivot.y - cap, width: cap * 2, height: cap * 2)),
                     with: .color(colour(theme.cap)))
        context.fill(Path(ellipseIn: CGRect(x: g.pivot.x - screw, y: g.pivot.y - screw,
                                            width: screw * 2, height: screw * 2)),
                     with: .linearGradient(Gradient(colors: [colour(theme.screwLight), colour(theme.screwDark)]),
                                           startPoint: CGPoint(x: g.pivot.x, y: g.pivot.y - screw),
                                           endPoint: CGPoint(x: g.pivot.x, y: g.pivot.y + screw)))
    }
}
