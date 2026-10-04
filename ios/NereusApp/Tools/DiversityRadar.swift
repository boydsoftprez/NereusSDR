// NereusSDR for iOS: the Diversity page's sensitivity pattern, drawn only from what the Core sends
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import SwiftUI

/// The sensitivity pattern of slice A's diversity: how strongly each
/// direction is heard, as a share of the strongest, 0 to 1 (not dB), at
/// `stepDeg` steps around the circle from north, clockwise. It is the
/// display the desktop's Diversity dialog shows. The phone never works the
/// pattern out itself (D4): the antennas' layout and the formula live at the
/// Core, which sends the samples (`diversityPattern`) that ``DiversityModel``
/// reads into this. Without them the page greys the drawing and says why.
struct DiversitySensitivity: Equatable, Sendable {
    /// Each direction's share of the strongest, clamped to 0 to 1.
    let shares: [Double]
    /// Degrees between neighbouring shares.
    let stepDeg: Double

    /// `stepDeg` as the Core sends it; without one, the shares go evenly round the circle.
    init(shares: [Double], stepDeg: Double? = nil) {
        self.shares = shares.map { $0.isFinite ? min(max($0, 0), 1) : 0 }
        if let stepDeg, stepDeg.isFinite, stepDeg > 0 {
            self.stepDeg = stepDeg
        } else {
            self.stepDeg = shares.isEmpty ? 0 : 360 / Double(shares.count)
        }
    }

    /// The compass bearing, in whole degrees, heard best, or nil with no directions.
    var strongestBearing: Int? {
        guard let best = shares.indices.max(by: { shares[$0] < shares[$1] }) else {
            return nil
        }
        return Int((stepDeg * Double(best)).rounded()) % 360
    }

    /// Where each direction's share sits on a plot of `radius` around `centre`, north up.
    func points(centre: CGPoint, radius: CGFloat) -> [CGPoint] {
        shares.enumerated().map { index, share in
            Self.point(bearingDeg: stepDeg * Double(index), reach: Double(radius) * share, centre: centre)
        }
    }

    /// Where the steering handle sits: at the phase's bearing, on the plot's `radius`.
    static func handle(phaseDeg: Double, centre: CGPoint, radius: CGFloat) -> CGPoint {
        point(bearingDeg: phaseDeg, reach: Double(radius), centre: centre)
    }

    private static func point(bearingDeg: Double, reach: Double, centre: CGPoint) -> CGPoint {
        let azimuth = bearingDeg * Double.pi / 180
        return CGPoint(x: Double(centre.x) + reach * sin(azimuth), y: Double(centre.y) - reach * cos(azimuth))
    }
}

/// The sensitivity pattern on the Diversity page: a round plot with north
/// up and the Core's pattern filled, each share at 0.85 of the plot's
/// radius at its peak, with the steering handle at the phase on that same
/// circle, as the desktop's radar draws them. With no pattern the face is
/// drawn greyed and empty. It only shows: nothing on it can be touched, and
/// the phase and gain change with the sliders above it.
struct DiversityRadar: View {
    let sensitivity: DiversitySensitivity?
    var enabled = true
    /// Slice A's phase, where the handle is drawn.
    var phaseDeg: Double?

    var body: some View {
        Canvas { context, size in
            let side = min(size.width, size.height)
            let centre = CGPoint(x: size.width / 2, y: size.height / 2)
            let radius = side / 2 - 2
            let face = Path(ellipseIn: CGRect(x: centre.x - radius, y: centre.y - radius,
                                              width: radius * 2, height: radius * 2))
            let drawn = sensitivity != nil
            context.fill(face, with: .color(drawn ? ChromeColours.inset : ChromeColours.buttonOff))
            context.stroke(face, with: .color(drawn ? ChromeColours.insetBorder : ChromeColours.buttonOffBorder),
                           lineWidth: 1)
            let lines = drawn ? ChromeColours.textFaint : ChromeColours.buttonOffBorder
            for share in [1.0 / 3, 2.0 / 3] {
                let ring = radius * 0.85 * share
                context.stroke(Path(ellipseIn: CGRect(x: centre.x - ring, y: centre.y - ring,
                                                      width: ring * 2, height: ring * 2)),
                               with: .color(lines), style: StrokeStyle(lineWidth: 0.8, dash: [3, 3]))
            }
            var cross = Path()
            cross.move(to: CGPoint(x: centre.x - radius, y: centre.y))
            cross.addLine(to: CGPoint(x: centre.x + radius, y: centre.y))
            cross.move(to: CGPoint(x: centre.x, y: centre.y - radius))
            cross.addLine(to: CGPoint(x: centre.x, y: centre.y + radius))
            context.stroke(cross, with: .color(lines), lineWidth: 0.8)
            let labels: [(String, CGPoint)] = [
                ("N", CGPoint(x: centre.x, y: centre.y - radius + 9)),
                ("E", CGPoint(x: centre.x + radius - 9, y: centre.y)),
                ("S", CGPoint(x: centre.x, y: centre.y + radius - 9)),
                ("W", CGPoint(x: centre.x - radius + 9, y: centre.y)),
            ]
            for (label, point) in labels {
                context.draw(Text(label).font(.system(size: 10, weight: .semibold))
                    .foregroundStyle(drawn ? ChromeColours.textDim : ChromeColours.buttonOffText), at: point)
            }
            guard let sensitivity, !sensitivity.shares.isEmpty else {
                return
            }
            let colour = enabled ? ChromeColours.accent : ChromeColours.textDim
            let points = sensitivity.points(centre: centre, radius: radius * 0.85)
            var lobe = Path()
            lobe.addLines(points)
            lobe.closeSubpath()
            context.fill(lobe, with: .color(colour.opacity(0.25)))
            context.stroke(lobe, with: .color(colour), lineWidth: 1.5)
            if let phaseDeg, phaseDeg.isFinite {
                let handle = DiversitySensitivity.handle(phaseDeg: phaseDeg, centre: centre, radius: radius * 0.85)
                let dot = Path(ellipseIn: CGRect(x: handle.x - 4, y: handle.y - 4, width: 8, height: 8))
                context.fill(dot, with: .color(colour))
                context.stroke(dot, with: .color(ChromeColours.inset), lineWidth: 1)
            }
        }
        .aspectRatio(1, contentMode: .fit)
        .frame(maxWidth: 220)
        .frame(maxWidth: .infinity)
        .accessibilityElement()
        .accessibilityLabel("Sensitivity pattern")
        .accessibilityValue(accessibilityValue)
        .accessibilityIdentifier("diversity.radar")
    }

    private var accessibilityValue: String {
        guard let bearing = sensitivity?.strongestBearing else {
            return "Not sent by this Core"
        }
        return "Strongest from \(bearing) degrees"
    }
}
