// NereusSDR for iOS: a Roundtable reading's meter, on the flag's S scale, and the stripes a double wears
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-11-03: Overlay redesign design D11, D13 and D15.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import SwiftUI

/// Diagonal stripes, as the board's repeating 135 degree gradients: a double's
/// rail, its tag's top edge and its meters.
struct RoundtableStripes: View {
    let on: Color
    let off: Color
    /// Each stripe's width across the stripe.
    var onWidth: CGFloat = 3
    var offWidth: CGFloat = 3

    var body: some View {
        Canvas { context, size in
            Self.draw(&context, in: CGRect(origin: .zero, size: size), on: on, off: off,
                      onWidth: onWidth, offWidth: offWidth)
        }
        .accessibilityHidden(true)
    }

    static func draw(_ context: inout GraphicsContext, in rect: CGRect, on: Color, off: Color,
                     onWidth: CGFloat, offWidth: CGFloat) {
        guard rect.width > 0, rect.height > 0 else { return }
        context.fill(Path(rect), with: .color(off))
        let root2 = CGFloat(2).squareRoot()
        let period = (onWidth + offWidth) * root2
        let stripe = onWidth * root2
        var x = rect.minX - rect.height
        while x < rect.maxX {
            var path = Path()
            path.move(to: CGPoint(x: x, y: rect.maxY))
            path.addLine(to: CGPoint(x: x + stripe, y: rect.maxY))
            path.addLine(to: CGPoint(x: x + stripe + rect.height, y: rect.minY))
            path.addLine(to: CGPoint(x: x + rect.height, y: rect.minY))
            path.closeSubpath()
            context.fill(path, with: .color(on))
            x += period
        }
    }
}

/// One reading's 3 point meter. `fraction` and `peak` are places on the bar, 0 to
/// 1 (``RoundtableTagLayout/meterFraction(_:)``). It draws in place: nothing here
/// measures text or lays anything out.
struct RoundtableMeterBar: View, Equatable {
    enum Style: Equatable {
        case talking, you, unknown, double, youInDouble
        /// A turn that ended, not yet old.
        case ended
        /// An old reading or a dropped link: grey, and no tick.
        case stale
    }

    static let height: CGFloat = 3
    /// D11: the division marks, as fractions of the bar.
    static let marks = [0.136, 0.273, 0.409, 0.545, 0.773]

    private static let trackColour = Color(red: 0x26 / 255, green: 0x26 / 255, blue: 0x2A / 255)
    private static let tickColour = Color(red: 0xF4 / 255, green: 0xF4 / 255, blue: 0xF6 / 255).opacity(0.85)
    private static let endedColour = Color(red: 0x7E / 255, green: 0x7E / 255, blue: 0x85 / 255)
    private static let staleColour = Color(red: 0x60 / 255, green: 0x60 / 255, blue: 0x66 / 255)
    private static let doubleOff = Color(red: 0x5D / 255, green: 0x2C / 255, blue: 0x2E / 255)
    private static let youOff = Color(red: 0x0D / 255, green: 0x4A / 255, blue: 0x58 / 255)

    let fraction: Double
    let peak: Double?
    let style: Style

    /// How much of the bar is filled.
    var fillFraction: Double { min(1, max(0, fraction)) }
    /// Where the peak tick sits; nil when there is none.
    var tickFraction: Double? {
        style == .stale ? nil : peak.map { min(1, max(0, $0)) }
    }
    var isStriped: Bool { style == .double || style == .youInDouble }

    /// A solid fill's colour; a striped one's first stripe.
    var fillColour: Color {
        switch style {
        case .talking: RoundtableColours.talking
        case .you, .youInDouble: RoundtableColours.you
        case .unknown: RoundtableColours.unknown
        case .double: RoundtableColours.doubled
        case .ended: Self.endedColour
        case .stale: Self.staleColour
        }
    }

    var body: some View {
        let fill = fillFraction
        let tick = tickFraction
        let striped = isStriped
        let colour = fillColour
        let off = style == .youInDouble ? Self.youOff : Self.doubleOff
        Canvas { context, size in
            let bounds = CGRect(origin: .zero, size: size)
            context.clip(to: Path(roundedRect: bounds, cornerRadius: 1)) // kept-radius: roundtable-tag
            context.fill(Path(bounds), with: .color(Self.trackColour))
            for mark in Self.marks {
                context.fill(Path(CGRect(x: size.width * mark - 0.5, y: 0, width: 1, height: size.height)),
                             with: .color(.black))
            }
            let filled = CGRect(x: 0, y: 0, width: size.width * fill, height: size.height)
            if striped {
                var inner = context
                inner.clip(to: Path(filled))
                RoundtableStripes.draw(&inner, in: bounds, on: colour, off: off, onWidth: 3, offWidth: 3)
            } else {
                context.fill(Path(filled), with: .color(colour))
            }
            if let tick {
                context.fill(Path(CGRect(x: size.width * tick, y: 0, width: 1, height: size.height)),
                             with: .color(Self.tickColour))
            }
        }
        .frame(height: Self.height)
        .accessibilityHidden(true)
    }
}
