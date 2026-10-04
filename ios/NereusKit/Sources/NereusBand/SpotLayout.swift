// NereusSDR for iOS: where each spot's callsign sits on the band, and which spots go into +N badges
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import CoreText
import Foundation

/// The spots on the band (R-IOS-25, D13, spec section 5.1 item 10). Each
/// spot's callsign is a label centred on its frequency. Labels start at
/// the lower of the start setting (halfway down by default) and just under
/// the lowest flag, so a spot never starts behind a flag; a label that
/// would land on one already placed drops a row. Only the rows that fit
/// above the band-plan strip are used, with room left under them for a row
/// of badges, and never more than the levels setting. A spot with no row
/// left goes into a +N badge with the others near it (the spots within
/// each 40 points across), placed under the rows at their average
/// frequency. Upright, with the flags at the top, that is halfway, as on
/// the desktop. Where the flags reach so low that no row fits under them,
/// every spot goes into a badge, and the badges keep to the spectrum's
/// foot: nothing is drawn on the waterfall.
///
/// Everything is in the band's points from the spectrum's top left;
/// `geometry` is the spectrum above the band-plan strip.
public enum SpotLayout {
    /// One spot to place.
    public struct Spot: Equatable, Sendable {
        public let id: String
        public let frequencyHz: Double
        public let call: String
        /// The source's label as the Core names it, for the settings' hidden sources.
        public let source: String

        public init(id: String, frequencyHz: Double, call: String, source: String) {
            self.id = id
            self.frequencyHz = frequencyHz
            self.call = call
            self.source = source
        }
    }

    /// A placed callsign: its box, and the spot's line across.
    public struct Label: Equatable, Sendable {
        public let spotId: String
        public let rect: CGRect
        public let lineX: CGFloat
    }

    /// A +N badge: the spots it holds and its box.
    public struct Badge: Equatable, Sendable {
        public let spotIds: [String]
        public let rect: CGRect

        public var text: String { "+\(spotIds.count)" }
    }

    public struct Result: Equatable, Sendable {
        public var labels: [Label] = []
        public var badges: [Badge] = []
        /// Where the first row starts, and how many rows are used.
        public var startY: CGFloat = 0
        public var levels = 0
        public var rowHeight: CGFloat = 0
    }

    /// Spots within this many points across share a badge.
    public static let badgeBinPoints: CGFloat = 40
    /// The gap under the lowest flag before the first row.
    public static let belowFlagsGap: CGFloat = 4
    /// A label's width beyond its text.
    public static let labelPadding: CGFloat = 6
    /// A badge's width beyond its text, and its text's size under the labels'.
    public static let badgePadding: CGFloat = 10
    public static let badgeFontReduction: CGFloat = 2

    /// A row's height for text of `fontSize` points.
    public static func rowHeight(fontSize: Int) -> CGFloat {
        CGFloat(fontSize) + 5
    }

    /// Places `spots` (every source's, in any order) under `flags` on the
    /// spectrum `geometry` describes. `textWidth` measures bold text of a
    /// size; the default measures the system font's bold face.
    public static func layout(spots: [Spot], flags: [FlagPlacement], geometry: BandGeometry,
                              settings: SpotDisplaySettings,
                              textWidth: (String, CGFloat) -> CGFloat = SpotLayout.boldTextWidth) -> Result {
        let settings = settings.clamped
        let height = geometry.size.height
        let width = CGFloat(geometry.size.width)
        let row = rowHeight(fontSize: settings.fontSize)
        var result = Result(rowHeight: row)
        guard settings.enabled, height >= row, width > 0 else {
            return result
        }
        var startY = (height * CGFloat(settings.startPercent) / 100).rounded()
        if let flagsBottom = flags.map(\.rect.maxY).max() {
            startY = max(startY, flagsBottom.rounded(.up) + belowFlagsGap)
        }
        let fitting = Int(((height - startY - row - 2) / row).rounded(.down))
        let levels = max(0, min(settings.maxLevels, fitting))
        result.startY = startY
        result.levels = levels
        let maxBottom = startY + row * CGFloat(levels)

        let shown = spots.filter { settings.shows(source: $0.source) }
            .sorted { $0.frequencyHz != $1.frequencyHz ? $0.frequencyHz < $1.frequencyHz : $0.id < $1.id }
        var placed: [CGRect] = []
        var overflow: [Int: [(id: String, x: CGFloat)]] = [:]
        for spot in shown {
            let x = CGFloat(geometry.x(forHz: spot.frequencyHz)).rounded()
            guard x >= 0, x <= width else {
                continue
            }
            let labelWidth = textWidth(spot.call, CGFloat(settings.fontSize)).rounded(.up) + labelPadding
            var rect = CGRect(x: x - (labelWidth / 2).rounded(.down), y: startY, width: labelWidth, height: row)
            var moved = true
            while moved {
                moved = false
                for other in placed where FlagLayout.overlaps(other, rect) {
                    rect.origin.y = other.maxY
                    moved = true
                    break
                }
            }
            if rect.maxY - 1 > maxBottom {
                overflow[Int((x / badgeBinPoints).rounded(.down)), default: []].append((spot.id, x))
                continue
            }
            placed.append(rect)
            result.labels.append(Label(spotId: spot.id, rect: rect, lineX: x))
        }
        let badgeSize = CGFloat(settings.fontSize) - badgeFontReduction
        for bin in overflow.keys.sorted() {
            guard let members = overflow[bin], !members.isEmpty else {
                continue
            }
            let average = (members.map(\.x).reduce(0, +) / CGFloat(members.count)).rounded()
            let text = "+\(members.count)"
            let badgeWidth = textWidth(text, badgeSize).rounded(.up) + badgePadding
            var rect = CGRect(x: average - (badgeWidth / 2).rounded(.down), y: min(maxBottom + 2, height - row),
                              width: badgeWidth,
                              height: row)
            for other in placed where FlagLayout.overlaps(other, rect) {
                rect.origin.x = other.maxX + 2
            }
            placed.append(rect)
            result.badges.append(Badge(spotIds: members.map(\.id), rect: rect))
        }
        return result
    }

    /// The width of `text` in the system font's bold face at `size` points.
    public static func boldTextWidth(_ text: String, _ size: CGFloat) -> CGFloat {
        guard let font = CTFontCreateUIFontForLanguage(.emphasizedSystem, size, nil) else {
            return CGFloat(text.count) * size * 0.62
        }
        let attributed = NSAttributedString(string: text,
                                            attributes: [NSAttributedString.Key(kCTFontAttributeName as String): font])
        let line = CTLineCreateWithAttributedString(attributed)
        return CGFloat(CTLineGetTypographicBounds(line, nil, nil, nil))
    }
}
