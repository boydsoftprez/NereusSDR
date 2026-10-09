// NereusSDR for iOS: the rotor's control dial: the compass rose, the heading needle, the target and the route, and the elevation gauge
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The rotor page's colours, as the rotor mockup draws them
/// (docs/architecture/2026-10-07-rotor-control-mockup.html): the amber
/// heading, the cyan target, green on arrival, the rose's rings and
/// words, and Stop, the only red button.
enum RotorColours {
    static let amber = rgb(0xFF, 0xB8, 0x00)
    static let target = rgb(0x00, 0xB4, 0xD8)
    static let arrived = rgb(0x5F, 0xFF, 0x8A)
    static let ring = rgb(0x7E, 0x8A, 0x96)
    static let inner = rgb(0x2C, 0x3A, 0x48)
    static let muted = rgb(0xA6, 0xB0, 0xBC)
    static let endStop = rgb(0xC1, 0x48, 0x48)
    static let stop = rgb(0x7A, 0x1C, 0x1C)
    static let stopBorder = rgb(0xC1, 0x48, 0x48)
    static let stopText = rgb(0xFF, 0xD0, 0xD0)
    static let staleHeading = rgb(0x50, 0x60, 0x70)
    /// The spot sheet's Turn beam button: amber, as the mockup's.
    static let beam = rgb(0x60, 0x40, 0x00)
    static let beamBorder = rgb(0x90, 0x60, 0x00)

    private static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }
}

/// What the dial draws, read from ``RotorModel`` (kept apart so a picture
/// test can draw any state).
struct RotorDialState: Equatable {
    /// The compass heading; nil when not known.
    var heading: Double?
    /// The heading is the last one heard, not live: drawn muted, never as a live needle.
    var stale = false
    /// The target; nil when none.
    var target: Double?
    /// The target is a selection not yet sent: drawn in the target colour.
    var selected = false
    /// The signed route to the target; nil draws no route.
    var travel: Double?
    /// The rotor is at its target.
    var arrived = false
    /// The rotor is turning to its target.
    var turning = false
    /// The compass heading of the end stop; nil with none.
    var endStop: Double?
    /// The elevation gauge beside the rose on an az/el rotor; nil on an azimuth rotor.
    var elevation: Elevation?

    struct Elevation: Equatable {
        var degrees: Double?
        var target: Double?
    }

    @MainActor
    init(_ model: RotorModel) {
        let state = model.state
        heading = state?.heading
        stale = state?.positionFresh == false
        target = model.dialTarget
        selected = model.selection != nil
        travel = model.dialTravel
        arrived = model.arrived
        turning = model.selection == nil && state?.motion == .turning
        // The stop is where the controller's own reading stops; the dial
        // shows headings after the offset, so it sits the offset round.
        let offset = state?.offsetDeg ?? 0
        switch state?.endStop {
        case .north?: endStop = RotorModel.compass(0 + offset)
        case .south?: endStop = RotorModel.compass(180 + offset)
        default: endStop = nil
        }
        if let state, state.axes == .azimuthElevation {
            elevation = Elevation(degrees: state.elevation, target: state.targetElevation)
        }
    }

    init(heading: Double?, target: Double? = nil) {
        self.heading = heading
        self.target = target
    }
}

/// The control dial (design, iPhone; geometry after the mockup's
/// Longpath-style dial): rings and ticks, the cardinal letters and the
/// 30-degree numbers; the amber beam wedge and tapered needle at the
/// heading; the dashed target line with a rim triangle; the travel sector
/// along the route the rotor will take; the end stop marked on the rim; and
/// on an az/el rotor the elevation quarter gauge beside the rose.
///
/// A drag only selects (``onSelect``); it never turns the rotor.
struct RotorDial: View {
    let state: RotorDialState
    let enabled: Bool
    let onSelect: (Double) -> Void

    /// The drawing's own units: the rose is 200 high; an az/el rotor adds the gauge to make it 300 wide.
    static let unitHeight: CGFloat = 200
    static let roseCentre = CGPoint(x: 100, y: 100)
    static let radius: CGFloat = 88
    /// The dial's height on the page, as the mockup's phone draws it.
    static let maximumHeight: CGFloat = 250

    private var unitWidth: CGFloat { state.elevation == nil ? 200 : 300 }

    var body: some View {
        Canvas { context, size in
            let scale = size.height / Self.unitHeight
            context.scaleBy(x: scale, y: scale)
            Self.drawRose(&context, state)
            if let elevation = state.elevation {
                Self.drawElevation(&context, elevation)
            }
        }
        .aspectRatio(unitWidth / Self.unitHeight, contentMode: .fit)
        // The touch is read in the drawing's own frame, before it is centred in the row.
        .onGeometryChange(for: CGSize.self) { $0.size } action: { size = $0 }
        .contentShape(Rectangle())
        .gesture(drag)
        .frame(maxWidth: .infinity, maxHeight: Self.maximumHeight)
        .opacity(enabled || state.heading != nil ? 1 : 0.45)
        .accessibilityElement()
        .accessibilityLabel("Rotor dial")
        .accessibilityValue(accessibilityValue)
        .accessibilityHint(enabled ? "Drag on the dial to choose a heading, then tap Turn" : "")
        .accessibilityAdjustableAction { direction in
            guard enabled else {
                return
            }
            let from = state.target ?? state.heading ?? 0
            onSelect(direction == .increment ? from + 5 : from - 5)
        }
        .accessibilityIdentifier("rotor.dial")
    }

    private var accessibilityValue: String {
        var words = "Heading " + RotorModel.headingText(state.heading)
        if let target = state.target {
            words += state.selected ? ", selected " : ", target "
            words += RotorModel.headingText(target)
        }
        return words
    }

    private var drag: some Gesture {
        DragGesture(minimumDistance: 0)
            .onChanged { value in
                guard enabled, let heading = heading(at: value.location, start: value.startLocation) else {
                    return
                }
                onSelect(heading)
            }
    }

    /// The compass heading under a touch, in the dial's own units; nil for
    /// a drag that did not start on the rose.
    private func heading(at point: CGPoint, start: CGPoint) -> Double? {
        // The view is laid out at the drawing's aspect ratio, so one scale fits both axes.
        guard let scale = lastScale else {
            return nil
        }
        let centre = CGPoint(x: Self.roseCentre.x * scale, y: Self.roseCentre.y * scale)
        let startDistance = hypot(start.x - centre.x, start.y - centre.y)
        guard startDistance <= Self.radius * 1.15 * scale else {
            return nil
        }
        let dx = point.x - centre.x
        let dy = point.y - centre.y
        guard hypot(dx, dy) > 4 else {
            return nil
        }
        return RotorModel.compass(atan2(Double(dx), Double(-dy)) * 180 / .pi)
    }

    @State private var size: CGSize = .zero
    private var lastScale: CGFloat? {
        size.height > 0 ? size.height / Self.unitHeight : nil
    }

    // MARK: Drawing

    private static func point(_ centre: CGPoint, _ radius: CGFloat, _ degrees: Double) -> CGPoint {
        let angle = (degrees - 90) * .pi / 180
        return CGPoint(x: centre.x + radius * CGFloat(cos(angle)), y: centre.y + radius * CGFloat(sin(angle)))
    }

    private static func direction(_ degrees: Double) -> CGVector {
        let angle = (degrees - 90) * .pi / 180
        return CGVector(dx: cos(angle), dy: sin(angle))
    }

    private static func pie(_ centre: CGPoint, _ radius: CGFloat, from start: Double, span: Double) -> Path {
        var path = Path()
        path.move(to: centre)
        path.addArc(center: centre, radius: radius, startAngle: .degrees(start - 90),
                    endAngle: .degrees(start + span - 90), clockwise: span < 0)
        path.closeSubpath()
        return path
    }

    private static func text(_ context: inout GraphicsContext, _ words: String, at point: CGPoint, size: CGFloat,
                             colour: Color) {
        context.draw(Text(words).font(.system(size: size, design: .monospaced)).foregroundColor(colour),
                     at: point, anchor: .center)
    }

    /// The tapered needle with its counterweight, as one closed shape.
    private static func needle(_ centre: CGPoint, length: CGFloat, tail: CGFloat, width: CGFloat,
                               direction: CGVector) -> Path {
        let normal = CGVector(dx: -direction.dy, dy: direction.dx)
        let half = width * 0.9
        let tip = CGPoint(x: centre.x + length * direction.dx, y: centre.y + length * direction.dy)
        let back = CGPoint(x: centre.x - tail * direction.dx, y: centre.y - tail * direction.dy)
        var path = Path()
        path.move(to: tip)
        path.addLine(to: CGPoint(x: centre.x + normal.dx * half, y: centre.y + normal.dy * half))
        path.addLine(to: CGPoint(x: back.x + normal.dx * half * 0.7, y: back.y + normal.dy * half * 0.7))
        path.addLine(to: CGPoint(x: back.x - normal.dx * half * 0.7, y: back.y - normal.dy * half * 0.7))
        path.addLine(to: CGPoint(x: centre.x - normal.dx * half, y: centre.y - normal.dy * half))
        path.closeSubpath()
        return path
    }

    static func drawRose(_ context: inout GraphicsContext, _ state: RotorDialState) {
        let centre = roseCentre
        let radius = radius
        // The lamp behind the rose.
        let lamp = Path(ellipseIn: CGRect(x: centre.x - radius * 1.05, y: centre.y - radius * 1.05,
                                          width: radius * 2.1, height: radius * 2.1))
        context.fill(lamp, with: .radialGradient(Gradient(colors: [RotorColours.amber.opacity(0.13),
                                                                   RotorColours.amber.opacity(0)]),
                                                 center: centre, startRadius: 0, endRadius: radius * 1.05))
        // Rings.
        context.stroke(Path(ellipseIn: CGRect(x: centre.x - radius, y: centre.y - radius, width: radius * 2,
                                              height: radius * 2)),
                       with: .color(RotorColours.ring), lineWidth: 0.9)
        let innerRadius = radius * 0.83
        context.stroke(Path(ellipseIn: CGRect(x: centre.x - innerRadius, y: centre.y - innerRadius,
                                              width: innerRadius * 2, height: innerRadius * 2)),
                       with: .color(RotorColours.inner), lineWidth: 0.9)
        // Ticks every 10 degrees: longer at 30, longest at the cardinals.
        for degrees in stride(from: 0, to: 360, by: 10) {
            let cardinal = degrees % 90 == 0
            let major = degrees % 30 == 0
            let inner = radius * (cardinal ? 0.86 : major ? 0.90 : 0.94)
            var tick = Path()
            tick.move(to: point(centre, inner, Double(degrees)))
            tick.addLine(to: point(centre, radius, Double(degrees)))
            context.stroke(tick, with: .color(cardinal ? RotorColours.muted : RotorColours.ring),
                           lineWidth: cardinal ? 1.5 : major ? 1.1 : 0.6)
        }
        for degrees in stride(from: 30, to: 360, by: 30) where degrees % 90 != 0 {
            text(&context, String(degrees), at: point(centre, radius * 0.755, Double(degrees)), size: 8,
                 colour: RotorColours.ring.opacity(0.7))
        }
        for (letter, degrees) in [("N", 0.0), ("E", 90.0), ("S", 180.0), ("W", 270.0)] {
            text(&context, letter, at: point(centre, radius * 0.745, degrees), size: 10, colour: RotorColours.muted)
        }
        // The end stop on the rim.
        if let stop = state.endStop {
            var mark = Path()
            mark.move(to: point(centre, radius * 0.97, stop))
            mark.addLine(to: point(centre, radius * 1.07, stop))
            context.stroke(mark, with: .color(RotorColours.endStop), style: StrokeStyle(lineWidth: 3, lineCap: .round))
        }
        let live = state.heading != nil && !state.stale
        let headColour = state.arrived ? RotorColours.arrived : live ? RotorColours.amber : RotorColours.staleHeading
        // The route to the target.
        if let heading = state.heading, state.target != nil, let travel = state.travel, live {
            let fill = state.arrived ? RotorColours.arrived : state.turning ? RotorColours.amber : RotorColours.target
            let alpha = (state.turning || state.arrived ? 30.0 : 26.0) / 255
            context.fill(pie(centre, radius, from: heading, span: travel), with: .color(fill.opacity(alpha)))
        }
        // The beam wedge.
        if let heading = state.heading, live {
            context.fill(pie(centre, radius * 0.95, from: heading - 20, span: 40),
                         with: .color(RotorColours.amber.opacity(16.0 / 255)))
        }
        // The target: a dashed line with its halo, and a triangle on the rim.
        if let target = state.target {
            let way = direction(target)
            let normal = CGVector(dx: -way.dy, dy: way.dx)
            let rim = CGPoint(x: centre.x + radius * way.dx, y: centre.y + radius * way.dy)
            let base = CGPoint(x: centre.x + (radius - 9) * way.dx, y: centre.y + (radius - 9) * way.dy)
            var triangle = Path()
            triangle.move(to: rim)
            triangle.addLine(to: CGPoint(x: base.x + normal.dx * 4.5, y: base.y + normal.dy * 4.5))
            triangle.addLine(to: CGPoint(x: base.x - normal.dx * 4.5, y: base.y - normal.dy * 4.5))
            triangle.closeSubpath()
            context.fill(triangle, with: .color(state.arrived ? RotorColours.arrived : RotorColours.target))
            if !state.arrived {
                var line = Path()
                line.move(to: centre)
                line.addLine(to: CGPoint(x: centre.x + radius * 0.9 * way.dx, y: centre.y + radius * 0.9 * way.dy))
                context.stroke(line, with: .color(RotorColours.target.opacity(70.0 / 255)),
                               style: StrokeStyle(lineWidth: 5, lineCap: .round))
                context.stroke(line, with: .color(RotorColours.target),
                               style: StrokeStyle(lineWidth: 2.4, lineCap: .round, dash: [2.2 * 2.4, 1.6 * 2.4]))
            }
        }
        // The heading needle, with its halo; muted when the heading is the last heard.
        if let heading = state.heading {
            let shape = needle(centre, length: radius * 0.9, tail: radius * 0.16, width: 2.6,
                               direction: direction(heading))
            context.stroke(shape, with: .color(headColour.opacity(60.0 / 255)),
                           style: StrokeStyle(lineWidth: 3, lineJoin: .round))
            context.fill(shape, with: .color(headColour))
        }
        // The hub.
        let hub = Path(ellipseIn: CGRect(x: centre.x - 9, y: centre.y - 9, width: 18, height: 18))
        context.fill(hub, with: .radialGradient(Gradient(colors: [headColour, headColour.opacity(0)]),
                                                center: centre, startRadius: 0, endRadius: 9))
        context.fill(Path(ellipseIn: CGRect(x: centre.x - 3.6, y: centre.y - 3.6, width: 7.2, height: 7.2)),
                     with: .color(headColour))
    }

    /// The elevation quarter gauge beside the rose: 0 at the right, 90 straight up.
    static func drawElevation(_ context: inout GraphicsContext, _ elevation: RotorDialState.Elevation) {
        let corner = CGPoint(x: 208, y: 178)
        let radius: CGFloat = 84
        func at(_ distance: CGFloat, _ degrees: Double) -> CGPoint {
            let angle = degrees * .pi / 180
            return CGPoint(x: corner.x + distance * CGFloat(cos(angle)), y: corner.y - distance * CGFloat(sin(angle)))
        }
        func arc(_ distance: CGFloat) -> Path {
            var path = Path()
            path.addArc(center: corner, radius: distance, startAngle: .degrees(0), endAngle: .degrees(-90),
                        clockwise: true)
            return path
        }
        var glow = arc(radius)
        glow.addLine(to: corner)
        glow.closeSubpath()
        context.fill(glow, with: .color(RotorColours.amber.opacity(0.05)))
        context.stroke(arc(radius), with: .color(RotorColours.ring), lineWidth: 0.9)
        context.stroke(arc(radius * 0.83), with: .color(RotorColours.inner), lineWidth: 0.9)
        var axes = Path()
        axes.move(to: at(radius, 0))
        axes.addLine(to: corner)
        axes.addLine(to: at(radius, 90))
        context.stroke(axes, with: .color(RotorColours.inner), lineWidth: 1)
        for degrees in stride(from: 0, through: 90, by: 5) {
            let major = degrees % 30 == 0
            let middle = degrees % 10 == 0
            var tick = Path()
            tick.move(to: at(radius * (major ? 0.86 : middle ? 0.90 : 0.94), Double(degrees)))
            tick.addLine(to: at(radius, Double(degrees)))
            context.stroke(tick, with: .color(major ? RotorColours.muted : RotorColours.ring),
                           lineWidth: major ? 1.4 : middle ? 1 : 0.6)
        }
        for degrees in [30.0, 60.0] {
            text(&context, String(Int(degrees)), at: at(radius * 0.72, degrees), size: 8,
                 colour: RotorColours.ring.opacity(0.7))
        }
        text(&context, "0", at: CGPoint(x: corner.x + radius, y: corner.y + 9), size: 8, colour: RotorColours.ring)
        text(&context, "90", at: CGPoint(x: corner.x, y: corner.y - radius - 6), size: 8, colour: RotorColours.ring)
        if let target = elevation.target {
            var line = Path()
            line.move(to: corner)
            line.addLine(to: at(radius * 0.97, target))
            context.stroke(line, with: .color(RotorColours.target), style: StrokeStyle(lineWidth: 1.6, dash: [5, 3]))
            if let degrees = elevation.degrees {
                var sector = Path()
                sector.move(to: corner)
                sector.addArc(center: corner, radius: radius, startAngle: .degrees(-min(degrees, target)),
                              endAngle: .degrees(-max(degrees, target)), clockwise: true)
                sector.closeSubpath()
                context.fill(sector, with: .color(RotorColours.amber.opacity(30.0 / 255)))
            }
        }
        if let degrees = elevation.degrees {
            let angle = degrees * .pi / 180
            let way = CGVector(dx: cos(angle), dy: -sin(angle))
            let shape = needle(corner, length: radius * 0.9, tail: radius * 0.10, width: 2.6, direction: way)
            context.stroke(shape, with: .color(RotorColours.amber.opacity(60.0 / 255)),
                           style: StrokeStyle(lineWidth: 3, lineJoin: .round))
            context.fill(shape, with: .color(RotorColours.amber))
            context.fill(Path(ellipseIn: CGRect(x: corner.x - 7, y: corner.y - 7, width: 14, height: 14)),
                         with: .color(RotorColours.amber.opacity(0.25)))
            context.fill(Path(ellipseIn: CGRect(x: corner.x - 3.6, y: corner.y - 3.6, width: 7.2, height: 7.2)),
                         with: .color(RotorColours.amber))
        }
        let words = "EL " + (elevation.degrees.map { "\(Int($0.rounded()))\u{00B0}" } ?? "--\u{00B0}")
        text(&context, words, at: CGPoint(x: corner.x + radius * 0.5, y: corner.y - 10), size: 9,
             colour: RotorColours.muted)
    }
}
