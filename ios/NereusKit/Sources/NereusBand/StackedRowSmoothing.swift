// NereusSDR for iOS: the 3D view's rows smoothed as they go in, so the stack's surface reads calm
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Smooths each 3D row as it joins the stack, in three steps, as the
/// desktop's 3D smooths its rows: each column takes the middle of its last
/// three levels, so a burst on one row alone never stands up; then each
/// column takes a quarter of each neighbour and half of itself; then the
/// row is 0.6 of itself and 0.4 of the row before it, as smoothed. The
/// steps run in that order, on the levels as the row's columns hold them.
///
/// A column's neighbours count only when they come from the same source as
/// it (the frame's own line, or the Core's wide row beside it) and hold a
/// level; past them the column stands in for its missing neighbour, as at
/// the row's ends. A row covering other frequencies than the one before
/// (a pan, a zoom, a new band) or from another capture starts over: no
/// middle of three until it has two rows of its own, and no share of the
/// row before. So does the first row after a key or an unkey
/// (``StackedTraceHistory/resetSmoothing()``).
struct StackedRowSmoother: Equatable, Sendable {
    /// The share of the new row in each row; the rest is the row before.
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

    /// The last two rows as they came, newest first, and how many are held.
    private var lastRaw: [Float] = []
    private var rawBefore: [Float] = []
    private var rawHeld = 0
    private var origin: Origin?
    /// The next row takes no share of the row before it.
    private var startsFresh = true

    /// The smoothed `raw` row, whose columns came from `sources`, from
    /// `origin`, with `previous` the row before it as smoothed (nil when
    /// there is none).
    mutating func smooth(_ raw: [Float], sources: [Source], origin: Origin,
                         previous: ArraySlice<Float>?) -> [Float] {
        let sameOrigin = self.origin == origin && lastRaw.count == raw.count
        if !sameOrigin {
            rawHeld = 0
        }
        var row = raw
        if rawHeld >= 2 {
            for index in row.indices {
                row[index] = Self.middle(raw[index], lastRaw[index], rawBefore[index])
            }
        }
        rawBefore = lastRaw
        lastRaw = raw
        rawHeld = min(rawHeld + 1, 2)

        var smoothed = Self.spread(row, sources: sources)
        if sameOrigin, !startsFresh, let previous, previous.count == smoothed.count {
            for (index, before) in zip(smoothed.indices, previous) where smoothed[index].isFinite && before.isFinite {
                smoothed[index] = Self.newShare * smoothed[index] + (1 - Self.newShare) * before
            }
        }
        startsFresh = false
        self.origin = origin
        return smoothed
    }

    /// Forgets the rows as they came and takes no share of the row before
    /// for the next row: a key or an unkey.
    mutating func restart() {
        rawHeld = 0
        startsFresh = true
    }

    /// Forgets everything: the stack has cleared or changed its width.
    mutating func clear() {
        self = StackedRowSmoother()
    }

    /// The middle of three levels; the newest alone when any has none.
    static func middle(_ a: Float, _ b: Float, _ c: Float) -> Float {
        guard a.isFinite, b.isFinite, c.isFinite else {
            return a
        }
        return max(min(a, b), min(max(a, b), c))
    }

    /// Each column a quarter of each neighbour and half of itself. A
    /// neighbour from another source or with no level counts as the
    /// column itself.
    static func spread(_ row: [Float], sources: [Source]) -> [Float] {
        var spread = row
        for index in row.indices where row[index].isFinite {
            func neighbour(_ other: Int) -> Float {
                guard other >= 0, other < row.count, sources[other] == sources[index], row[other].isFinite else {
                    return row[index]
                }
                return row[other]
            }
            spread[index] = 0.25 * neighbour(index - 1) + 0.5 * row[index] + 0.25 * neighbour(index + 1)
        }
        return spread
    }

    static func == (lhs: Self, rhs: Self) -> Bool {
        lhs.rawHeld == rhs.rawHeld && lhs.origin == rhs.origin && lhs.startsFresh == rhs.startsFresh
            && lhs.lastRaw.map(\.bitPattern) == rhs.lastRaw.map(\.bitPattern)
            && lhs.rawBefore.map(\.bitPattern) == rhs.rawBefore.map(\.bitPattern)
    }
}
