// NereusSDR for iOS: the band-plan strip, the Core's band plan laid out under the spectrum as the desktop lays it out
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import NereusModels

/// The band-plan strip (R-IOS-11, spec section 5.1 items 4 and 19, D79):
/// the segments of the Core's band plan that fall inside the span, each
/// placed at its frequencies, filled with its colour dimmed by its licence
/// class, and named at the middle of its visible part where the name fits;
/// and the plan's spots as dots. The plans are the Core's catalogue's; the
/// app carries none of its own (D4). The strip ends at `rightX`, the dBm
/// scale's left edge, as the desktop's does.
public struct BandPlanStrip: Equatable, Sendable {
    /// One visible segment.
    public struct Piece: Equatable, Sendable {
        public let lowX: Double
        public let highX: Double
        public let label: String
        /// The segment's licence classes (`E,G`), or empty.
        public let licence: String
        /// The segment's lowest licence class (`Extra`), or empty.
        public let lowestClass: String
        /// `#RRGGBB`, as the catalogue gives it.
        public let colour: String

        public init(lowX: Double, highX: Double, label: String, licence: String = "", lowestClass: String = "",
                    colour: String) {
            self.lowX = lowX
            self.highX = highX
            self.label = label
            self.licence = licence
            self.lowestClass = lowestClass
            self.colour = colour
        }

        /// Where the label is centred.
        public var labelX: Double { (lowX + highX) / 2 }
        public var width: Double { highX - lowX }

        /// The texts the strip may show, longest first, each with the
        /// segment width in points it needs more than: the label with its
        /// lowest class after it (`CW Extra`) in a segment wider than 60
        /// points, the label alone in one wider than 20, as the desktop
        /// names them.
        public var texts: [(text: String, widerThanPoints: Double)] {
            var texts: [(text: String, widerThanPoints: Double)] = []
            if !lowestClass.isEmpty {
                texts.append(("\(label) \(lowestClass)", BandPlanStrip.classedWidthPoints))
            }
            texts.append((label, BandPlanStrip.labelWidthPoints))
            return texts
        }

        /// The first of ``texts`` whose segment is wide enough at `scale`
        /// pixels per point and that `fits`, or nil when none does: a name
        /// that would overflow its segment is left out (on the phone the
        /// width is measured, where the desktop's name can run over).
        public func text(scale: CGFloat, fitting fits: (String) -> Bool) -> String? {
            texts.first { width > $0.widerThanPoints * Double(scale) && fits($0.text) }?.text
        }
    }

    /// The segment widths, in points, a name needs more than, as the
    /// desktop's strip needs them in pixels: the label and its class, and
    /// the label alone.
    public static let classedWidthPoints = 60.0
    public static let labelWidthPoints = 20.0

    /// How much of a segment's colour shows over the band's background,
    /// by its licence classes (D79, as the desktop's strip dims them):
    /// Extra only 0.2, Extra and General 0.4, any with Technician 0.6, none
    /// 0.5, anything else 0.6.
    public static func colourShare(licence: String) -> Double {
        if licence == "E" {
            return 0.20
        }
        if licence == "E,G" {
            return 0.40
        }
        if licence.contains("T") {
            return 0.60
        }
        if licence.isEmpty {
            return 0.50
        }
        return 0.60
    }

    /// The background a segment's colour is blended into, `#0A0A14`.
    public static let background: (red: Int, green: Int, blue: Int) = (0x0A, 0x0A, 0x14)
    /// The separator at each segment's left edge: `#0F0F1A` at 200 of 255.
    public static let separator: (red: Int, green: Int, blue: Int, alpha: Int) = (0x0F, 0x0F, 0x1A, 200)
    /// The spot dots' radius, in points.
    public static let spotRadiusPoints = 4.0

    /// A segment's fill, opaque, each channel 0 to 255: its colour blended
    /// into ``background`` by ``colourShare(licence:)``, rounded down; nil
    /// when `colour` is not `#RRGGBB`.
    public static func fill(colour: String, licence: String) -> (red: Int, green: Int, blue: Int)? {
        let digits = colour.dropFirst()
        guard colour.hasPrefix("#"), digits.count == 6, digits.allSatisfy(\.isHexDigit),
              let value = Int(digits, radix: 16) else {
            return nil
        }
        let share = colourShare(licence: licence)
        func blend(_ channel: Int, _ under: Int) -> Int {
            Int(Double(channel) * share + Double(under) * (1 - share))
        }
        return (blend((value >> 16) & 0xFF, background.red), blend((value >> 8) & 0xFF, background.green),
                blend(value & 0xFF, background.blue))
    }

    public let pieces: [Piece]
    /// Where each of the plan's spots in the strip sits across, lowest first.
    public let spotXs: [Double]

    /// Lays out `plan`'s segments and spots that fall in `geometry`'s span,
    /// clipped to it and to `rightX` (the dBm scale's left edge; the span's
    /// right end when nil), lowest frequency first.
    public init(plan: StationCatalog.BandPlan?, geometry: BandGeometry, rightX: Double? = nil) {
        guard let plan else {
            pieces = []
            spotXs = []
            return
        }
        let right = min(Double(geometry.size.width), rightX ?? .infinity)
        pieces = plan.segments
            .filter { $0.highHz > geometry.lowHz && $0.lowHz < geometry.highHz && $0.highHz > $0.lowHz }
            .sorted { $0.lowHz < $1.lowHz }
            .compactMap { segment in
                let low = max(0, geometry.x(forHz: segment.lowHz))
                let high = min(right, geometry.x(forHz: segment.highHz))
                guard high > low else {
                    return nil
                }
                return Piece(lowX: low, highX: high, label: segment.label, licence: segment.licence,
                             lowestClass: segment.lowestClass, colour: segment.colour)
            }
        spotXs = plan.spots
            .filter { $0.hz >= geometry.lowHz && $0.hz <= geometry.highHz }
            .map { geometry.x(forHz: $0.hz) }
            .filter { $0 >= 0 && $0 <= right }
            .sorted()
    }

    /// The plan the strip shows (D79): the plan the Core marks `active`;
    /// from a Core that marks none, the plan its `BandPlanName` setting
    /// names (`stationPlanName`); otherwise the catalogue's default (ARRL
    /// (US) today); with no plans, none.
    public static func plan(in plans: [StationCatalog.BandPlan], stationPlanName: String?)
        -> StationCatalog.BandPlan? {
        if let active = plans.first(where: \.isActive) {
            return active
        }
        if let stationPlanName, let named = plans.first(where: { $0.name == stationPlanName }) {
            return named
        }
        return plans.first(where: \.isDefault)
    }
}
