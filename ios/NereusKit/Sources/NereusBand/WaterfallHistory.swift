// NereusSDR for iOS: the waterfall's painted lines, newest first, each coloured against its own levels
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMedia

/// The waterfall's history (R-IOS-11): one line for each frame the Core
/// marks as a waterfall advance, newest first, up to `capacity` lines.
///
/// Each line is coloured when it arrives, against the levels the Core's
/// display extras carry for that frame (the Core runs Clarity and the other
/// level modes; the app adds nothing). A frame without its datagram keeps
/// the levels last carried; before the Core has sent any (a Core without
/// display extras, or Clarity not yet spoken) the manual levels apply.
///
/// A line is stored as palette positions, 0 at the low level and 255 at the
/// high one, so the renderer only looks colours up. Each line also keeps the
/// frequencies it covers (D74), so after a pan or a zoom the older lines are
/// drawn where their frequencies now fall, not stretched to the new view.
public struct WaterfallHistory: Equatable, Sendable {
    /// A waterfall's low and high levels, in dBm.
    public struct Levels: Equatable, Sendable {
        public var lowDbm: Float
        public var highDbm: Float

        public init(lowDbm: Float, highDbm: Float) {
            self.lowDbm = lowDbm
            self.highDbm = highDbm
        }

        /// These levels as a line is coloured against them after Color
        /// Gain and Black Level, as the desktop's waterfall applies them in
        /// every level mode (`SpectrumWidget.cpp` `dbmToRgb`, receive
        /// side): the low level rises 0.4 dB for each step Black Level is
        /// under 125, the high level falls 0.3 dB for each step of Color
        /// Gain, and the high level stays at least 1 dB over the low one.
        public func adjusted(by adjustment: Adjustment) -> Levels {
            let black = Float(min(max(adjustment.blackLevel, 0), Adjustment.blackLevelTop))
            let gain = Float(min(max(adjustment.colorGain, 0), Adjustment.colorGainTop))
            let low = lowDbm + (Float(Adjustment.blackLevelTop) - black) * 0.4
            var high = highDbm - gain * 0.3
            if high <= low {
                high = low + 1
            }
            return Levels(lowDbm: low, highDbm: high)
        }
    }

    /// The waterfall's Color Gain (0 to 100) and Black Level (0 to 125).
    public struct Adjustment: Equatable, Sendable {
        public var colorGain: Int
        public var blackLevel: Int

        public init(colorGain: Int, blackLevel: Int) {
            self.colorGain = colorGain
            self.blackLevel = blackLevel
        }

        static let colorGainTop = 100
        static let blackLevelTop = 125

        /// No change to the levels: Color Gain 0, Black Level 125.
        public static let none = Adjustment(colorGain: 0, blackLevel: blackLevelTop)
    }

    /// How many samples every line has; 0 before the first line.
    public private(set) var columns = 0
    /// The most lines kept.
    public private(set) var capacity: Int
    /// How many lines are held, at most `capacity`.
    public private(set) var count = 0
    /// Every line ever appended since the history last started over; the
    /// renderer uploads only the lines it has not seen.
    public private(set) var linesAppended: UInt64 = 0
    /// Counts the times the history's layout changed (a new line width, a
    /// new capacity or a clear), so the renderer knows to upload it all.
    public private(set) var layoutGeneration: UInt64 = 0
    /// The levels the last line was coloured against.
    public private(set) var currentLevels: Levels?
    /// The levels the Core last sent, held for frames without a datagram.
    public private(set) var heldLevels: Levels?

    /// Each storage row's coverage, nil where it is not known.
    private var coverages: [BandCoverage?] = []
    /// Each storage row's arrival time, 0 where it is not known.
    private var times: [Double] = []
    private var storage: [UInt8] = []
    /// Where the next line goes.
    private var head = 0

    public init(capacity: Int) {
        self.capacity = max(1, capacity)
    }

    /// Adds `frame`'s waterfall line when the frame advances the waterfall,
    /// coloured against the levels in `extras` when it is this frame's
    /// datagram, else the held levels, else `manualLevels`, each after
    /// `adjustment`. Returns whether a line was added.
    /// With `phoneLevels` the phone sets the levels itself (Use spectrum
    /// min/max): `manualLevels` colour the line whatever the Core sent.
    /// `time` is when the line arrived, seconds since 1970. Without
    /// `addsLine` only the Core's levels are noted (the waterfall held).
    @discardableResult
    public mutating func append(frame: DisplayFrame, extras: DisplayExtras?, manualLevels: Levels,
                                coverage: BandCoverage? = nil, adjustment: Adjustment = .none,
                                phoneLevels: Bool = false, time: Double = 0, addsLine: Bool = true) -> Bool {
        if let extras, Self.belongs(extras, to: frame), let levels = extras.waterfallLevels {
            heldLevels = Levels(lowDbm: levels.lowDbm, highDbm: levels.highDbm)
        }
        guard addsLine, frame.waterfallAdvance, !frame.waterfallDbm.isEmpty else {
            return false
        }
        let levels = phoneLevels ? manualLevels : (heldLevels ?? manualLevels)
        append(line: frame.waterfallDbm, levels: levels.adjusted(by: adjustment), coverage: coverage, time: time)
        return true
    }

    /// Adds one line of dBm values coloured against `levels`. A line of a
    /// different width than the held ones (the band turned between upright
    /// and sideways, so the Core now sends it wider or narrower) re-maps the
    /// held lines to the new width, keeping the history and the frequencies
    /// each line covers.
    public mutating func append(line dbm: [Float], levels: Levels, coverage: BandCoverage? = nil,
                                time: Double = 0) {
        guard !dbm.isEmpty else {
            return
        }
        if dbm.count != columns {
            remap(columns: dbm.count)
        }
        let start = head * columns
        for (offset, value) in dbm.enumerated() {
            storage[start + offset] = Self.paletteIndex(dbm: value, levels: levels)
        }
        coverages[head] = coverage
        times[head] = time
        head = (head + 1) % capacity
        count = min(count + 1, capacity)
        linesAppended &+= 1
        currentLevels = levels
    }

    /// When the line `age` lines old arrived, seconds since 1970; nil past
    /// the held ones or when not known.
    public func time(age: Int) -> Double? {
        guard age >= 0, age < count else {
            return nil
        }
        let time = times[storageRow(age: age)]
        return time > 0 ? time : nil
    }

    /// The line `age` lines old (0 the newest), or nil past the held ones.
    public func line(age: Int) -> ArraySlice<UInt8>? {
        guard age >= 0, age < count else {
            return nil
        }
        let row = storageRow(age: age)
        return storage[(row * columns)..<((row + 1) * columns)]
    }

    /// The storage row `age` lines old: the renderer's texture keeps the
    /// same rows, so a new line uploads into one row and nothing moves.
    public func storageRow(age: Int) -> Int {
        ((head - 1 - age) % capacity + capacity) % capacity
    }

    /// Where the next line goes in storage.
    public var nextStorageRow: Int { head }

    /// The line in storage row `row`, whether or not it holds one yet.
    public func storageLine(_ row: Int) -> ArraySlice<UInt8> {
        storage[(row * columns)..<((row + 1) * columns)]
    }

    /// The frequencies storage row `row` covers, nil when not known.
    public func coverage(storageRow row: Int) -> BandCoverage? {
        row >= 0 && row < coverages.count ? coverages[row] : nil
    }

    /// Keeps up to `newCapacity` lines, the newest ones.
    public mutating func resize(capacity newCapacity: Int) {
        let newCapacity = max(1, newCapacity)
        guard newCapacity != capacity else {
            return
        }
        let kept = min(count, newCapacity)
        var lines: [(ArraySlice<UInt8>, BandCoverage?, Double)] = []
        for age in stride(from: kept - 1, through: 0, by: -1) {
            if let line = line(age: age) {
                let row = storageRow(age: age)
                lines.append((line, coverages[row], times[row]))
            }
        }
        capacity = newCapacity
        restart()
        for (line, coverage, time) in lines {
            storage.replaceSubrange((head * columns)..<((head + 1) * columns), with: line)
            coverages[head] = coverage
            times[head] = time
            head = (head + 1) % capacity
            count += 1
        }
        linesAppended = UInt64(count)
    }

    /// Re-maps every held line to `newColumns` samples. Each new sample
    /// takes the strongest of the old samples it covers, so a narrower
    /// line keeps its signals; a wider one repeats them. The rows stay
    /// where they are in storage, with their coverage.
    private mutating func remap(columns newColumns: Int) {
        let oldColumns = columns
        guard oldColumns > 0, count > 0 else {
            columns = newColumns
            restart()
            return
        }
        var remapped = [UInt8](repeating: 0, count: capacity * newColumns)
        for age in 0..<count {
            let row = storageRow(age: age)
            let source = row * oldColumns
            let target = row * newColumns
            for column in 0..<newColumns {
                let start = column * oldColumns / newColumns
                let end = max(start + 1, (column + 1) * oldColumns / newColumns)
                var strongest: UInt8 = 0
                for index in start..<min(end, oldColumns) {
                    strongest = max(strongest, storage[source + index])
                }
                remapped[target + column] = strongest
            }
        }
        columns = newColumns
        storage = remapped
        layoutGeneration &+= 1
    }

    /// Forgets the held levels but keeps the lines: the next levels the Core
    /// sends are taken afresh, as the desktop resets its waterfall AGC at a
    /// key and an unkey.
    public mutating func forgetLevels() {
        heldLevels = nil
    }

    /// Forgets every line and the held levels.
    public mutating func clear() {
        heldLevels = nil
        currentLevels = nil
        restart()
    }

    /// `dbm`'s place in the palette against `levels`: 0 at or below the
    /// low level, 255 at or above the high one.
    public static func paletteIndex(dbm: Float, levels: Levels) -> UInt8 {
        let range = levels.highDbm - levels.lowDbm
        guard range > 0, dbm.isFinite else {
            return dbm.isFinite && dbm >= levels.highDbm ? 255 : 0
        }
        let fraction = min(max((dbm - levels.lowDbm) / range, 0), 1)
        return UInt8((fraction * 255).rounded())
    }

    /// Whether `extras` is the datagram of `frame`.
    public static func belongs(_ extras: DisplayExtras, to frame: DisplayFrame) -> Bool {
        extras.endpointId == frame.endpointId && extras.contextGeneration == frame.contextGeneration
            && extras.encoderSequence == frame.encoderSequence
    }

    private mutating func restart() {
        storage = [UInt8](repeating: 0, count: capacity * columns)
        coverages = [BandCoverage?](repeating: nil, count: capacity)
        times = [Double](repeating: 0, count: capacity)
        head = 0
        count = 0
        linesAppended = 0
        layoutGeneration &+= 1
    }
}
