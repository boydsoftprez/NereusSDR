// NereusSDR for iOS: the 3D view's rows smoothed as they go in, so the stack's surface reads calm
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Smooths each 3D row as it joins the stack, in three steps, so the
/// stack's surface reads as calm as the desktop's 3D does:
///
/// 1. Each column takes the median of its level in the newest three rows
///    as they came, once three of them are held, so a burst on one row
///    alone never stands up. Until then the row is taken as it came.
/// 2. Each column takes half of itself and a quarter of each neighbour.
/// 3. The row is 0.6 of itself and 0.4 of the row before it, as smoothed.
///
/// A column's neighbours count only when they come from the same source as
/// it (the frame's own line, or the Core's wide row beside it) and hold a
/// level; a neighbour that does not count, and the one missing past each
/// end of the row, is the column's own level. A row covering other
/// frequencies than the one before (a pan, a zoom, a new band), from
/// another capture or of another width starts over: no median until it
/// has three rows of its own, and no share of the row before. So does the
/// first row after a key or an unkey (``StackedTraceHistory/resetSmoothing()``).
struct StackedRowSmoother: Equatable, Sendable {
    /// The new row's share of each smoothed row; the rest is the row before.
    static let newShare: Float = 0.6

    /// Where a column's level came from.
    enum Source: UInt8, Equatable, Sendable {
        case none = 0
        case line = 1
        case wide = 2
    }

    /// What a row covered and from which capture, so rows from another
    /// view or capture are never mixed.
    struct Origin: Equatable, Sendable {
        let endpointId: UInt32
        let contextGeneration: UInt32
        let window: BandCoverage
        let wide: BandCoverage?
    }

    /// The last three rows as they came, three slots of `width` levels one
    /// after another, each new row in the slot after the last one's.
    private var recent: [Float] = []
    private var width = 0
    private var slot = 0
    /// Rows seen since the smoothing last started over, up to 3.
    private var seen = 0
    private var origin: Origin?
    /// The next row takes no share of the row before it.
    private var fresh = true

    /// The smoothed `raw` row, whose columns came from `sources`, from
    /// `origin`, with `previous` the row before it as smoothed (nil when
    /// there is none).
    mutating func smooth(_ raw: [Float], sources: [Source], origin: Origin,
                         previous: ArraySlice<Float>?) -> [Float] {
        var row = [Float](repeating: 0, count: raw.count)
        row.withUnsafeMutableBufferPointer { out in
            raw.withUnsafeBufferPointer { levels in
                sources.withUnsafeBufferPointer { from in
                    if let previous {
                        previous.withUnsafeBufferPointer { before in
                            smooth(levels, sources: from, origin: origin, previous: before, into: out)
                        }
                    } else {
                        smooth(levels, sources: from, origin: origin, previous: nil, into: out)
                    }
                }
            }
        }
        return row
    }

    /// Smooths `raw` into `out` (as many levels as `raw`), as
    /// ``smooth(_:sources:origin:previous:)`` does, with no list made on
    /// the way. `previous` may be the stack's own storage: it is only read,
    /// and never overlaps `out`.
    mutating func smooth(_ raw: UnsafeBufferPointer<Float>, sources: UnsafeBufferPointer<Source>, origin: Origin,
                         previous: UnsafeBufferPointer<Float>?, into out: UnsafeMutableBufferPointer<Float>) {
        let count = raw.count
        let continues = self.origin == origin && width == count
        if !continues {
            seen = 0
        }
        if width != count {
            width = count
            recent = [Float](repeating: .nan, count: 3 * count)
        }
        recent.withUnsafeMutableBufferPointer { rows in
            let start = slot * count
            for column in 0..<count {
                rows[start + column] = raw[column]
            }
            // Step 1, into `out`: the median of the three rows held, or the row as it came.
            if seen >= 2 {
                for column in 0..<count {
                    out[column] = Self.median(rows[column], rows[count + column], rows[2 * count + column],
                                              newest: raw[column])
                }
            } else {
                for column in 0..<count {
                    out[column] = raw[column]
                }
            }
        }
        slot = (slot + 1) % 3
        seen = min(seen + 1, 3)

        // Step 2, in place: `left` keeps the level the column to the left
        // had before its own spread.
        var left = Float.nan
        for column in 0..<count {
            let level = out[column]
            defer { left = level }
            guard level.isFinite else {
                continue
            }
            let source = sources[column]
            let leftCounts = column > 0 && sources[column - 1] == source && left.isFinite
            let right = column + 1 < count ? out[column + 1] : .nan
            let rightCounts = column + 1 < count && sources[column + 1] == source && right.isFinite
            out[column] = 0.25 * (leftCounts ? left : level) + 0.5 * level + 0.25 * (rightCounts ? right : level)
        }

        // Step 3: a share of the row before, where both hold a level.
        if continues, !fresh, let previous, previous.count == count {
            let rest = 1 - Self.newShare
            for column in 0..<count where out[column].isFinite && previous[column].isFinite {
                out[column] = Self.newShare * out[column] + rest * previous[column]
            }
        }
        fresh = false
        self.origin = origin
    }

    /// Takes the median of the rows as they came afresh and no share of
    /// the row before for the next row: a key or an unkey.
    mutating func restart() {
        seen = 0
        fresh = true
    }

    /// Forgets everything: the stack has cleared.
    mutating func clear() {
        self = StackedRowSmoother()
    }

    /// The median of three levels, or `newest` alone when any of them holds
    /// no level (not a number or infinite).
    static func median(_ a: Float, _ b: Float, _ c: Float, newest: Float) -> Float {
        guard a.isFinite, b.isFinite, c.isFinite else {
            return newest
        }
        let low = min(a, b)
        let high = max(a, b)
        return c < low ? low : (c > high ? high : c)
    }

    static func == (lhs: Self, rhs: Self) -> Bool {
        lhs.seen == rhs.seen && lhs.slot == rhs.slot && lhs.width == rhs.width && lhs.origin == rhs.origin && lhs.fresh == rhs.fresh
            && lhs.recent.map(\.bitPattern) == rhs.recent.map(\.bitPattern)
    }
}
