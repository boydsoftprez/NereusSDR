// NereusSDR for iOS: the fold rule: where each slice's flag sits, and which flags fold to a one-line tag
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics

/// The one flag rule for the phone and the iPad (D9, R-IOS-11): every slice
/// keeps its full flag unless it would land on another flag, and then it
/// folds to a one-line tag (letter, frequency, TX badge). The active slice
/// is always full and is placed first, at the top edge.
///
/// Each flag hangs on the side of its line away from the passband, as the
/// desktop's flag does: to the right for a lower-sideband mode, to the left
/// otherwise, and on the other side when that side would leave the band. A
/// folded tag that would land on a flag or a tag already placed drops below
/// it, as the board draws the stack.
///
/// A full flag's footprint is the flag and the column of round buttons
/// (close, lock and more) beside it on its line's side, as JJ's board
/// "Flag controls at finger size" places them: two flags hanging the same
/// way fold the second when their lines are closer than the flag, the gap
/// and the column, 246 points at every text size (the redrawn flag of
/// JJ's 2026-09-30 approval: one layout at every size, the round buttons
/// 44 points across).
public enum FlagLayout {
    /// The full flag in the desktop's rows (the board's `#flagbtn-review`,
    /// approved 2026-09-30), 200 points wide: the 44-point header, the
    /// 40-point frequency, a 4-point gap, the 24-point level row and the
    /// 44-point tabs, 158 points in all with its border.
    public static let flagSize = CGSize(width: 200, height: 158)
    /// The same flag in large type: one layout at every text size, where
    /// only the words grow, so the same height.
    public static let largeFlagHeight: CGFloat = 158
    /// The flag with the RADE row (16 points and its 2-point gap).
    public static let radeFlagHeight: CGFloat = 176
    /// The listening flag's owner row, as tall as the header.
    public static let ownerRowHeight: CGFloat = 44
    /// The round buttons beside a full flag: 44 points across at every
    /// text size, three of them 2 points apart, the column 2 points from
    /// the flag.
    public static let sideButton: CGFloat = 44
    public static let largeSideButton: CGFloat = 44
    public static let sideGap: CGFloat = 2
    public static let sideButtonCount = 3
    /// The folded tag's height.
    public static let foldedHeight: CGFloat = 28
    /// The gap between a tag and the flag or tag above it.
    public static let stackGap: CGFloat = 6
    /// The folded tag's frequency: bold monospaced at 15 points, whose
    /// characters are 0.6 of the size wide.
    public static let foldedFrequencyPoints: CGFloat = 15
    public static let foldedCharacterWidth: CGFloat = 9
    /// The tag's parts beside its frequency: the padding and border (12),
    /// the letter badge (18), two gaps (12) and the TX badge (28).
    public static let foldedFixedWidth: CGFloat = 70

    /// The column of round buttons for buttons `button` points across.
    public static func sideColumnSize(button: CGFloat = sideButton) -> CGSize {
        CGSize(width: button,
               height: CGFloat(sideButtonCount) * button + CGFloat(sideButtonCount - 1) * sideGap)
    }

    /// Where a full flag's column of round buttons sits: beside the flag
    /// on its line's side, and on the flag's far side when the near side
    /// would leave the band, so it never lands on its own flag. A flag the
    /// band has moved away takes its column along.
    public static func sideColumnRect(flag: CGRect, lineX: Double, bandWidth: Double,
                                      column: CGSize = sideColumnSize()) -> CGRect {
        let flagOnLeft = Double(flag.maxX) <= lineX + 0.5
        let right = flag.maxX + sideGap
        let left = flag.minX - sideGap - column.width
        var x = flagOnLeft ? right : left
        if lineX >= 0, lineX <= bandWidth {
            if x < 0 {
                x = right
            } else if Double(x + column.width) > bandWidth {
                x = left
            }
        }
        return CGRect(x: x, y: flag.minY, width: column.width, height: column.height)
    }

    /// The folded tag's size for `frequencyText`.
    public static func foldedSize(frequencyText: String) -> CGSize {
        CGSize(width: foldedFixedWidth + CGFloat(frequencyText.count) * foldedCharacterWidth, height: foldedHeight)
    }

    /// Each slice's placement, in the order of `slices`. `geometry` is the
    /// band's frequency axis in points (its width is the band's width).
    /// `foldedSize` gives a slice's tag size; the default fits its
    /// frequency. `flagHeight` gives a slice's full flag height, which
    /// grows while the flag carries the RADE row or large type; the flag
    /// keeps `flagSize`'s width. `sideColumn` is the round buttons' column
    /// beside each full flag, part of the flag's footprint. `top` is where
    /// the flags start: below the Back to your band bar while the band
    /// shows another slice's display.
    public static func layout(slices: [BandSlice], activeSliceId: Int?, geometry: BandGeometry,
                              flagSize: CGSize = FlagLayout.flagSize,
                              foldedSize: (BandSlice) -> CGSize = { FlagLayout.foldedSize(frequencyText: $0.frequencyText) },
                              flagHeight: (BandSlice) -> CGFloat = { _ in FlagLayout.flagSize.height },
                              sideColumn: CGSize = FlagLayout.sideColumnSize(), top: CGFloat = 0)
        -> [FlagPlacement] {
        let width = Double(geometry.size.width)
        // The active slice first, then the others in slice order.
        let order = slices.indices.sorted { a, b in
            let aActive = slices[a].id == activeSliceId
            let bActive = slices[b].id == activeSliceId
            if aActive != bActive {
                return aActive
            }
            return slices[a].id < slices[b].id
        }
        var placed: [CGRect] = []
        var result = [FlagPlacement?](repeating: nil, count: slices.count)
        for index in order {
            let slice = slices[index]
            let lineX = geometry.x(forHz: slice.frequencyHz)
            let isActive = slice.id == activeSliceId
            let full = CGRect(x: sideX(lineX: lineX, width: Double(flagSize.width), bandWidth: width,
                                       lowerSideband: slice.lowerSideband),
                              y: top, width: flagSize.width, height: flagHeight(slice))
            let footprint = full.union(sideColumnRect(flag: full, lineX: lineX, bandWidth: width, column: sideColumn))
            if isActive || !placed.contains(where: { overlaps($0, footprint) }) {
                // A full flag always sits at the top; one that would land on another folds instead.
                placed.append(footprint)
                result[index] = .full(full)
                continue
            }
            let size = foldedSize(slice)
            var rect = CGRect(x: sideX(lineX: lineX, width: Double(size.width), bandWidth: width,
                                       lowerSideband: slice.lowerSideband),
                              y: top, width: size.width, height: size.height)
            var moved = true
            while moved {
                moved = false
                for other in placed where overlaps(other, rect) {
                    rect.origin.y = other.maxY + stackGap
                    moved = true
                }
            }
            placed.append(rect)
            result[index] = .folded(rect)
        }
        return result.compactMap { $0 }
    }

    /// The flag's left edge for a line at `lineX`: away from the passband,
    /// flipped when that side would leave the band, then kept on the band.
    /// A line off the band (the band moved away from the slice, D74) takes
    /// its flag with it, so the flag slides off and comes back with the line.
    static func sideX(lineX: Double, width: Double, bandWidth: Double, lowerSideband: Bool) -> Double {
        var x: Double
        if lowerSideband {
            x = lineX
            if x + width > bandWidth {
                x = lineX - width
            }
        } else {
            x = lineX - width
            if x < 0 {
                x = lineX
            }
        }
        guard lineX >= 0, lineX <= bandWidth else {
            return x
        }
        return min(max(x, 0), max(0, bandWidth - width))
    }

    /// True when the two rectangles share some area (touching edges do not).
    public static func overlaps(_ a: CGRect, _ b: CGRect) -> Bool {
        a.minX < b.maxX && b.minX < a.maxX && a.minY < b.maxY && b.minY < a.maxY
    }
}
