// NereusSDR for iOS: a horizontal gauge in the desktop's colours: RF power, SWR and mic level
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusModels
import SwiftUI

/// A horizontal bar gauge as the board draws the desktop's (spec section
/// 5.1 item 7, D14): the bar fills in blue up to its yellow mark, yellow up
/// to its red mark and red beyond, with its title inside the bar and its
/// scale's marks under it.
struct LinearGauge: View {
    /// A gauge's scale.
    struct Scale: Equatable {
        let title: String
        let min: Double
        let max: Double
        let yellowFrom: Double
        let redFrom: Double
        /// Marks under the bar: the value each sits at and its words.
        let ticks: [(value: Double, label: String)]

        static func == (lhs: Scale, rhs: Scale) -> Bool {
            lhs.title == rhs.title && lhs.min == rhs.min && lhs.max == rhs.max && lhs.yellowFrom == rhs.yellowFrom
                && lhs.redFrom == rhs.redFrom && lhs.ticks.map(\.value) == rhs.ticks.map(\.value)
                && lhs.ticks.map(\.label) == rhs.ticks.map(\.label)
        }

        /// The microphone's level, -40 to +10 dB, yellow from -10 and red from 0.
        static let micLevel = Scale(title: "Mic level", min: -40, max: 10, yellowFrom: -10, redFrom: 0,
                                    ticks: [(-40, "-40dB"), (-30, "-30"), (-20, "-20"), (-10, "-10"), (0, "0"),
                                            (5, "+5"), (10, "+10")])

        /// RF power, from the catalogue's meter for this radio, or the
        /// desktop's 100 W scale without one.
        static func rfPower(_ meter: StationCatalog.Meters.RfPower?) -> Scale {
            let maxW = meter?.maxW ?? 120
            let red = meter?.redFromW ?? 100
            let minW = meter?.minW ?? 0
            return Scale(title: "RF Pwr", min: minW, max: maxW, yellowFrom: red, redFrom: red,
                         ticks: [(minW, whole(minW)), (red / 3, whole(red / 3)), (red * 2 / 3, whole(red * 2 / 3)),
                                 (red, whole(red)), (maxW, whole(maxW))])
        }

        /// SWR, from the catalogue's meter, or 1 to 3 red from 2.5 without one.
        static func swr(_ meter: StationCatalog.Meters.Swr?) -> Scale {
            let low = meter?.min ?? 1
            let high = meter?.max ?? 3
            let red = meter?.redFrom ?? 2.5
            return Scale(title: "SWR", min: low, max: high, yellowFrom: red, redFrom: red,
                         ticks: [(low, trimmed(low)), (1.5, "1.5"), (red, trimmed(red)), (high, trimmed(high))])
        }

        private static func whole(_ value: Double) -> String {
            String(Int(value.rounded()))
        }

        private static func trimmed(_ value: Double) -> String {
            value == value.rounded() ? String(Int(value)) : String(format: "%.1f", value)
        }
    }

    let scale: Scale
    let value: Double?

    /// Nil is unavailable; finite values outside the scale clamp only the meter fill.
    var filledFraction: Double? { value.map(fraction) }

    static let height: CGFloat = 26

    var body: some View {
        Canvas { context, size in
            draw(in: &context, size: size)
        }
        .frame(height: Self.height)
        .accessibilityElement()
        .accessibilityLabel(scale.title)
        .accessibilityValue(value.map(Self.spoken) ?? "--")
    }

    private func fraction(_ number: Double) -> Double {
        guard scale.max > scale.min else {
            return 0
        }
        return Swift.min(Swift.max((number - scale.min) / (scale.max - scale.min), 0), 1)
    }

    private func draw(in context: inout GraphicsContext, size: CGSize) {
        let barHeight: CGFloat = 13
        let bar = CGRect(x: 1, y: 1, width: size.width - 2, height: barHeight)
        context.fill(Path(roundedRect: bar, cornerRadius: 2), with: .color(ChromeColours.gaugeGround))
        context.stroke(Path(roundedRect: bar.insetBy(dx: 0.5, dy: 0.5), cornerRadius: 2),
                       with: .color(ChromeColours.gaugeBorder), lineWidth: 1)
        let inner = bar.insetBy(dx: 1, dy: 1)
        let fill = filledFraction.map { inner.width * $0 } ?? 0
        let yellowX = inner.width * fraction(scale.yellowFrom)
        let redX = inner.width * fraction(scale.redFrom)
        if fill > 0 {
            let normal = Swift.min(fill, yellowX)
            if normal > 0 {
                context.fill(Path(CGRect(x: inner.minX, y: inner.minY, width: normal, height: inner.height)),
                             with: .color(ChromeColours.gaugeNormal))
            }
            if fill > yellowX, redX > yellowX {
                let warn = Swift.min(fill, redX) - yellowX
                context.fill(Path(CGRect(x: inner.minX + yellowX, y: inner.minY, width: warn, height: inner.height)),
                             with: .color(ChromeColours.gaugeWarn))
            }
            if fill > redX {
                context.fill(Path(CGRect(x: inner.minX + redX, y: inner.minY, width: fill - redX,
                                         height: inner.height)),
                             with: .color(ChromeColours.gaugeRed))
            }
        }
        // The title inside the bar.
        let title = Text(value == nil ? scale.title + " --" : scale.title).font(.system(size: 8, weight: .bold)).foregroundStyle(ChromeColours.textBright)
        var shadowed = context
        shadowed.addFilter(.shadow(color: .black, radius: 1.5))
        shadowed.draw(title, at: CGPoint(x: bar.midX, y: bar.midY), anchor: .center)
        // The marks under it, coloured as the zone they sit in.
        for tick in scale.ticks {
            let x = inner.minX + inner.width * fraction(tick.value)
            let colour: Color = tick.value >= scale.redFrom ? ChromeColours.gaugeRed
                : tick.value >= scale.yellowFrom ? ChromeColours.gaugeWarn : ChromeColours.gaugeNormal
            let anchor: UnitPoint = tick.value <= scale.min ? .topLeading
                : tick.value >= scale.max ? .topTrailing : .top
            context.draw(Text(tick.label).font(.system(size: 7, weight: .semibold)).foregroundStyle(colour),
                         at: CGPoint(x: x, y: bar.maxY + 1), anchor: anchor)
        }
    }

    private static func spoken(_ value: Double) -> String {
        value.isFinite ? String(format: "%.1f", value) : ""
    }
}
