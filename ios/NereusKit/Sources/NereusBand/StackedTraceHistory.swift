// NereusSDR for iOS: the 3D view's rows: each waterfall line with the spectrum beside the pan, newest first
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMedia

/// The stacked trace's rows (the 3D view): one for each waterfall line the
/// band keeps, so the stack and the waterfall move and stop together (Stop
/// on TX holds both). Each row is the line's levels in dBm across a window
/// ``StackedTrace/widestRowSpan`` times the pan's width about its centre:
/// the line where the frame covers, the Core's wide row beside it where
/// the frame carries one, and nothing (not a number) where neither reaches.
/// Each row keeps its window, so after a pan or a zoom the older rows are
/// drawn where their frequencies now fall.
public struct StackedTraceHistory: Equatable, Sendable {
    /// Samples in every row; 0 before the first.
    public private(set) var columns = 0
    public let capacity = StackedTrace.keptRows
    /// Rows held, at most ``capacity``.
    public private(set) var count = 0
    /// Every row ever added since the history last started over.
    public private(set) var rowsAppended: UInt64 = 0
    /// Counts the times the rows' width changed or the history cleared.
    public private(set) var layoutGeneration: UInt64 = 0

    private var storage: [Float] = []
    private var windows: [BandCoverage?] = []
    private var times: [Double] = []
    private var head = 0

    public init() {}

    /// Rows with no level hold not-a-number, which never equals itself, so
    /// two histories compare by their levels' bits.
    public static func == (lhs: StackedTraceHistory, rhs: StackedTraceHistory) -> Bool {
        lhs.columns == rhs.columns && lhs.count == rhs.count && lhs.rowsAppended == rhs.rowsAppended
            && lhs.layoutGeneration == rhs.layoutGeneration && lhs.head == rhs.head && lhs.windows == rhs.windows
            && lhs.times == rhs.times && lhs.storage.map(\.bitPattern) == rhs.storage.map(\.bitPattern)
    }

    /// Adds `frame`'s waterfall line as a row when the frame advances the
    /// waterfall. `coverage` is what the frame covers (its context),
    /// `wide` what its wide row covers (nil when the frame has none), and
    /// `time` when the line arrived. Returns whether a row was added.
    @discardableResult
    public mutating func append(frame: DisplayFrame, coverage: BandCoverage?, wide: BandCoverage?,
                                time: Double) -> Bool {
        guard frame.waterfallAdvance, !frame.waterfallDbm.isEmpty, let coverage, coverage.spanHz > 0 else {
            return false
        }
        let wanted = min(StackedTrace.maxColumns,
                         Int((Double(frame.waterfallDbm.count) * StackedTrace.widestRowSpan).rounded(.up)))
        if wanted != columns || storage.isEmpty {
            columns = max(1, wanted)
            storage = Array(repeating: .nan, count: columns * capacity)
            windows = Array(repeating: nil, count: capacity)
            times = Array(repeating: 0, count: capacity)
            head = 0
            count = 0
            layoutGeneration &+= 1
        }
        let window = BandCoverage(centerHz: coverage.centerHz, spanHz: coverage.spanHz * StackedTrace.widestRowSpan)
        let wideLine = wide.flatMap { $0.spanHz > 0 && !frame.wideDbm.isEmpty ? $0 : nil }
        let start = head * columns
        let columnHz = window.spanHz / Double(columns)
        for column in 0..<columns {
            let low = window.lowHz + Double(column) * columnHz
            let middle = low + columnHz / 2
            var value = Float.nan
            if middle >= coverage.lowHz, middle < coverage.highHz {
                value = Self.peak(frame.waterfallDbm, over: coverage, from: low, to: low + columnHz)
            } else if let wideLine, middle >= wideLine.lowHz, middle < wideLine.highHz {
                value = Self.peak(frame.wideDbm, over: wideLine, from: low, to: low + columnHz)
            }
            storage[start + column] = value
        }
        windows[head] = window
        times[head] = time
        head = (head + 1) % capacity
        count = min(count + 1, capacity)
        rowsAppended &+= 1
        return true
    }

    /// The highest level of `samples` (covering `coverage`) from `low` to
    /// `high` hertz: at least the one sample under the middle.
    static func peak(_ samples: [Float], over coverage: BandCoverage, from low: Double, to high: Double) -> Float {
        let binHz = coverage.spanHz / Double(samples.count)
        let first = min(max(Int(((low - coverage.lowHz) / binHz).rounded(.down)), 0), samples.count - 1)
        let last = min(max(Int(((high - coverage.lowHz) / binHz).rounded(.down)), first + 1), samples.count)
        var best = -Float.infinity
        for index in first..<last where samples[index].isFinite && samples[index] > best {
            best = samples[index]
        }
        return best.isFinite ? best : .nan
    }

    /// The storage row of the row `age` lines old (0 the newest).
    public func storageRow(age: Int) -> Int {
        (head - 1 - age + capacity * 2) % capacity
    }

    /// The levels of storage row `row`.
    public func storageLine(_ row: Int) -> ArraySlice<Float> {
        storage[(row * columns)..<((row + 1) * columns)]
    }

    /// The row `age` lines old: its levels, nil when there is none.
    public func row(age: Int) -> ArraySlice<Float>? {
        guard age >= 0, age < count else {
            return nil
        }
        return storageLine(storageRow(age: age))
    }

    /// The frequencies storage row `row` covers.
    public func window(storageRow row: Int) -> BandCoverage? {
        row >= 0 && row < windows.count ? windows[row] : nil
    }

    /// The window of the row `age` lines old.
    public func window(age: Int) -> BandCoverage? {
        guard age >= 0, age < count else {
            return nil
        }
        return windows[storageRow(age: age)]
    }

    /// When the newest row arrived; nil before any.
    public var newestTime: Double? {
        count > 0 ? times[storageRow(age: 0)] : nil
    }

    /// Starts over: 3D turned off, a new session or a new endpoint.
    public mutating func clear() {
        guard count > 0 || !storage.isEmpty else {
            return
        }
        storage = []
        windows = []
        times = []
        columns = 0
        head = 0
        count = 0
        layoutGeneration &+= 1
    }
}
