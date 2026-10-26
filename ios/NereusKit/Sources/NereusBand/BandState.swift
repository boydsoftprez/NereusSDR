// NereusSDR for iOS: one band's latest frame, its extras and its waterfall, as the Core's display events arrive
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-23: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code (Claude Opus 5.5):
//   NB Blank is reset when transmit ends, as the desktop's is.
// 2026-10-26: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code (Claude Opus 5.5):
//   while the screen drives the band, each waterfall line waits for the row clock
//   (WaterfallRowClock) and goes in on its steady grid, as the desktop's remote pans do.

import Foundation
import NereusMedia
import QuartzCore

/// One band's state between the Core's display events and the renderer.
///
/// The Core sends a frame's display extras right after the frame, so a
/// frame waits for its datagram before its waterfall line is coloured: it
/// is committed when its datagram arrives, when the next frame arrives, or
/// when the band is drawn, whichever comes first. With no extras expected
/// (an older Core, or nothing asked) a frame is committed at once.
///
/// While the screen drives the band's draws (``pacesLines``) and the Core
/// has said its line period, a committed frame's waterfall line waits for
/// the row clock (``WaterfallRowClock``) and goes into the waterfall and the
/// 3D stack on its steady grid (``advanceLines(at:frameSeconds:)``); the
/// frame's trace, extras and peak hold are still committed at once.
public struct BandState: Equatable, Sendable {
    public private(set) var history: WaterfallHistory
    /// The latest committed frame.
    public private(set) var frame: DisplayFrame?
    /// Advances only when a frame is committed, including after its
    /// matching extras arrive. A viewer can tell a fresh frame from a
    /// picture retained across a media connection.
    public private(set) var committedSerial: UInt64 = 0
    /// The committed frame's own datagram, or nil when it came without one.
    public private(set) var frameExtras: DisplayExtras?
    /// The frequencies the committed frame covers, when its context is known.
    public private(set) var frameCoverage: BandCoverage?
    /// Whether the Core sends extras beside this band's frames.
    public var expectsExtras: Bool
    /// The waterfall's Color Gain and Black Level, applied to each line's
    /// levels as it is coloured; the pan's settings set it.
    public var levelAdjustment: WaterfallHistory.Adjustment = .none
    public var waterfallBlanking = false {
        didSet { if !waterfallBlanking { blanker.reset() } }
    }
    public var waterfallBlankingThresholdDb = 6
    public var transmittingWaterfall = false {
        // Transmit has ended: the blanker forgets its baseline, its burst
        // run and its last accepted row, as the desktop's does, so the first
        // receive rows are drawn as they are and build a new baseline.
        didSet { if oldValue && !transmittingWaterfall { blanker.reset() } }
    }
    private var blanker = WaterfallBlanker()
    public var stackRowDivider = 0
    /// The waterfall height in points that Match counts, independent of
    /// the retained rewind capacity. Upright it is the visible height
    /// (``BandLayout/waterfallPoints``); sideways it is the upright height
    /// the band last had, or an estimate of it (``noteVisibleWaterfall(_:)``).
    public var stackVisibleWaterfallRows = 0
    /// The waterfall's height in points the last time the band stood
    /// upright; nil before it has.
    public private(set) var stackUprightWaterfallPoints: Int?
    private var stackFold = StackTraceFold()
    /// Native Match is round(visible/96), clamped1...64; explicit1...10
    /// keeps its selected cadence (SpectrumWidget.cpp:5533–5548). The
    /// phone's visible height stays the upright one when it turns
    /// (``noteVisibleWaterfall(_:)``).
    public var effectiveStackRowDivider: Int {
        if stackRowDivider > 0 { return min(stackRowDivider, 10) }
        return min(max(Int((Double(max(0, stackVisibleWaterfallRows)) / Double(StackedTrace.visibleRows)).rounded()), 1), 64)
    }

    /// The band's layout before a draw: Match's rows follow the waterfall's
    /// height in points while the band stands upright (taller than wide).
    /// Lying sideways it keeps the last upright height, so turning the
    /// phone or iPad keeps the 3D's rows a second: on an iPhone 18 Pro Max
    /// the sideways waterfall is about 93 points against 393 upright, and
    /// Match went from 4 lines a row to 1, the 3D four times as fast (JJ,
    /// 2026-10-25: smooth both ways with no setting changed). The desktop
    /// follows its pane's height as it is.
    ///
    /// Before the band has stood upright, Match estimates the upright height
    /// as the sideways waterfall's share of the band's height, taken over
    /// the band's width, which is its height upright.
    public mutating func noteVisibleWaterfall(_ layout: BandLayout) {
        // A band not laid out yet (no size, or no waterfall) says nothing.
        if layout.size.width > 0, layout.size.height >= layout.size.width, layout.waterfallPoints > 0 {
            stackUprightWaterfallPoints = layout.waterfallPoints
        }
        stackVisibleWaterfallRows = stackUprightWaterfallPoints ?? Self.uprightEstimate(layout)
    }

    private static func uprightEstimate(_ layout: BandLayout) -> Int {
        guard layout.size.height > 0, layout.scale > 0 else {
            return layout.waterfallPoints
        }
        let points = layout.waterfall.height / layout.size.height * layout.size.width / layout.scale
        return points.isFinite ? max(0, Int(points.rounded())) : layout.waterfallPoints
    }

    /// The phone sets the waterfall's levels itself (Use spectrum min/max):
    /// the manual levels given colour each line, whatever the Core sent.
    public var phoneSetsLevels = false
    /// The waterfall stands still: no line is added (Stop on TX while keyed).
    /// Lines that came before the hold go in at once when it starts.
    public var waterfallHeld = false {
        didSet { if waterfallHeld && !oldValue { flushLines() } }
    }
    /// The classic peak hold's reset period in milliseconds, or nil while
    /// it is off (``BandDisplaySettings/peakHold``).
    public var peakHoldDelayMs: Int? {
        didSet {
            if peakHoldDelayMs == nil {
                peakHold = nil
            }
        }
    }
    /// The classic peak hold: the highest level at each trace sample since
    /// it last started afresh; nil while off or before a frame.
    public private(set) var peakHold: [Float]?
    /// The producer time, in nanoseconds, the peak hold last started afresh.
    private var peakHoldStart: UInt64 = 0
    /// The 3D view's rows, kept only while ``stacksRows`` is on.
    public private(set) var stack = StackedTraceHistory()
    /// The pan has 3D chosen: each waterfall line also becomes a row of the
    /// stack. Off clears the rows.
    public var stacksRows = false {
        didSet {
            if !stacksRows {
                stack.clear()
                stackFold.clear()
                linesSinceStackRow = 0
            }
        }
    }
    /// Now, in seconds since 1970, for each line's time.
    public var clock: @Sendable () -> Double = { Date().timeIntervalSince1970 }

    // MARK: Lines on the row clock

    /// The screen drives the band's draws on its display's frames: each
    /// waterfall line waits for the row clock. Off, every waiting line goes
    /// in at once and lines go in as they come.
    public var pacesLines = false {
        didSet { if !pacesLines { flushLines() } }
    }
    /// The Core's seconds between two waterfall lines; 0 before it has
    /// said, when lines go in as they come. A new period starts the grid again.
    public var linePeriodSeconds = 0.0 {
        didSet {
            guard linePeriodSeconds != oldValue else {
                return
            }
            let period = linePeriodSeconds.isFinite && linePeriodSeconds > 0 ? linePeriodSeconds : 0
            if period == 0 {
                flushLines()
            }
            rowClock.setPeriodNs(Self.nanoseconds(period))
            frameLock.reset()
        }
    }
    /// The clock the waiting lines go in by.
    public private(set) var rowClock = WaterfallRowClock()
    /// Holds the clock's slide starts clear of the screen's frame edges.
    public private(set) var frameLock = WaterfallFrameLock()
    /// The lines committed and not yet in, oldest first.
    private var waitingLines: [WaitingLine] = []
    /// How many lines wait for the clock.
    public var linesWaiting: Int { waitingLines.count }
    /// The most lines that wait (the desktop's row queue without a hold,
    /// `kMaxRemoteRowQueue`). The desktop drops the oldest past it; the
    /// phone puts it in at once, so no line the Core sent is lost (a phone
    /// back from the background catches up rather than leaves a gap).
    public static let mostLinesWaiting = 32
    /// The frame time the clock last advanced at, so two views of one band
    /// advance it once a frame.
    private var lastAdvanceNs: Int64?
    /// Waterfall lines put in since the 3D stack's newest row.
    public private(set) var linesSinceStackRow = 0
    /// Lines wait for the clock now: the screen drives the band and the
    /// Core has said its line period.
    public var pacingLines: Bool { pacesLines && rowClock.running }

    /// One line put into the waterfall, for the phone's timing trace.
    public struct PutLine: Equatable, Sendable {
        /// The Core's frame sequence the line came in.
        public let sequence: UInt32
        /// When it was committed and when it went in, on the steady clock.
        public let arrivedAt: Double
        public let putAt: Double
    }
    /// Keeps each line put in for the timing trace (``takePutLines()``).
    public var tracesLines = false {
        didSet { if !tracesLines { putLines.removeAll() } }
    }
    private var putLines: [PutLine] = []

    /// The lines put in since the last call, for the timing trace.
    public mutating func takePutLines() -> [PutLine] {
        defer { putLines.removeAll(keepingCapacity: true) }
        return putLines
    }

    /// How far the 3D stack has glided toward its next row while lines go
    /// in by the clock: the lines put in since the newest row and the
    /// clock's glide toward the last, over the lines a row, 0 to 1. Nil
    /// while lines go in as they come (the glide is then timed from the
    /// newest row, ``StackedTrace/glide(since:rowPeriod:divider:)``).
    public var pacedStackGlide: Double? {
        guard pacingLines else {
            return nil
        }
        return StackedTrace.glide(lines: linesSinceStackRow, progress: rowClock.progress,
                                  divider: effectiveStackRowDivider)
    }

    /// Advances the clock to the screen frame at `now` (steady seconds,
    /// the display link's) of `frameSeconds` length, and puts in the lines
    /// it says, oldest first. Returns how many went in. A second call for
    /// the same frame does nothing.
    @discardableResult
    public mutating func advanceLines(at now: Double, frameSeconds: Double) -> Int {
        guard pacingLines, now.isFinite else {
            return 0
        }
        let nowNs = Self.nanoseconds(now)
        if let last = lastAdvanceNs, nowNs <= last {
            return 0
        }
        lastAdvanceNs = nowNs
        let read = frameLock.clockTime(frameNs: nowNs)
        let rows: Int
        if let front = waitingLines.first {
            // The phone's Core sends no due times: each line is due as it was committed.
            let committed = Self.nanoseconds(front.arrivedAt)
            rows = rowClock.advance(nowNs: read, queuedRows: waitingLines.count, frontDueNs: committed,
                                    frontQueuedNs: committed)
        } else {
            rows = rowClock.advance(nowNs: read, queuedRows: 0, frontDueNs: read, frontQueuedNs: read)
        }
        frameLock.follow(rowClock, frameNs: nowNs,
                         frameLengthNs: frameSeconds.isFinite ? Self.nanoseconds(max(frameSeconds, 0)) : 0)
        let count = min(rows, waitingLines.count)
        for _ in 0 ..< count {
            put(waitingLines.removeFirst(), at: now)
        }
        return count
    }

    /// Puts every waiting line in at once, in order, and starts the grid again.
    public mutating func flushLines() {
        guard !waitingLines.isEmpty || rowClock.anchored else {
            return
        }
        let lines = waitingLines
        waitingLines.removeAll()
        let now = lines.isEmpty ? 0 : Self.steadyNow()
        for line in lines {
            put(line, at: now)
        }
        rowClock.reset()
        frameLock.reset()
    }

    /// A committed frame's waterfall line, waiting for the clock.
    private struct WaitingLine: Equatable, Sendable {
        /// The frame as it came, for the 3D stack's fold.
        let frame: DisplayFrame
        /// Its line after NB Blank.
        let line: [Float]
        /// The levels it is coloured against, as they were when it came.
        let levels: WaterfallHistory.Levels
        let coverage: BandCoverage?
        let wide: BandCoverage?
        let transmitting: Bool
        /// When it was committed: on the steady clock, and in seconds since
        /// 1970 for the waterfall's time readout.
        let arrivedAt: Double
        let arrivedWall: Double
    }

    /// One line into the waterfall and, with 3D chosen, the stack's fold.
    private mutating func put(_ line: WaitingLine, at steadyNow: Double) {
        history.append(line: line.line, levels: line.levels, coverage: line.coverage, time: line.arrivedWall)
        foldIntoStack(line.frame, coverage: line.coverage, wide: line.wide, transmitting: line.transmitting)
        if tracesLines {
            putLines.append(PutLine(sequence: line.frame.encoderSequence, arrivedAt: line.arrivedAt,
                                    putAt: steadyNow))
        }
    }

    /// Every advancing tick contributes its exact and wide peaks to the
    /// 3D stack's fold; only a complete capture enters the painted history.
    private mutating func foldIntoStack(_ frame: DisplayFrame, coverage: BandCoverage?, wide: BandCoverage?,
                                        transmitting: Bool) {
        guard stacksRows && frame.waterfallAdvance else {
            return
        }
        linesSinceStackRow += 1
        if let folded = stackFold.append(frame, coverage: coverage, wide: wide,
            transmitting: transmitting, divider: effectiveStackRowDivider) {
            stack.append(frame: folded, coverage: coverage, wide: transmitting ? nil : wide, time: clock())
            linesSinceStackRow = 0
        }
    }

    static func nanoseconds(_ seconds: Double) -> Int64 {
        Int64((seconds * 1_000_000_000).rounded())
    }

    /// Now on the steady clock the display link keeps, in seconds.
    public static func steadyNow() -> Double {
        CACurrentMediaTime()
    }

    public static func == (lhs: BandState, rhs: BandState) -> Bool {
        lhs.history == rhs.history && lhs.frame == rhs.frame && lhs.committedSerial == rhs.committedSerial
            && lhs.frameExtras == rhs.frameExtras
            && lhs.frameCoverage == rhs.frameCoverage && lhs.expectsExtras == rhs.expectsExtras
            && lhs.levelAdjustment == rhs.levelAdjustment && lhs.phoneSetsLevels == rhs.phoneSetsLevels
            && lhs.waterfallHeld == rhs.waterfallHeld && lhs.peakHold == rhs.peakHold
            && lhs.contexts == rhs.contexts && lhs.pending == rhs.pending && lhs.pendingExtras == rhs.pendingExtras
            && lhs.blanker == rhs.blanker && lhs.waterfallBlanking == rhs.waterfallBlanking
            && lhs.waterfallBlankingThresholdDb == rhs.waterfallBlankingThresholdDb
            && lhs.transmittingWaterfall == rhs.transmittingWaterfall
            && lhs.stackRowDivider == rhs.stackRowDivider && lhs.stackFold == rhs.stackFold
            && lhs.stackVisibleWaterfallRows == rhs.stackVisibleWaterfallRows
            && lhs.stackUprightWaterfallPoints == rhs.stackUprightWaterfallPoints
            && lhs.stack == rhs.stack && lhs.stacksRows == rhs.stacksRows && lhs.wideContexts == rhs.wideContexts
            && lhs.pacesLines == rhs.pacesLines && lhs.linePeriodSeconds == rhs.linePeriodSeconds
            && lhs.rowClock == rhs.rowClock && lhs.frameLock == rhs.frameLock
            && lhs.waitingLines == rhs.waitingLines && lhs.lastAdvanceNs == rhs.lastAdvanceNs
            && lhs.linesSinceStackRow == rhs.linesSinceStackRow && lhs.tracesLines == rhs.tracesLines
            && lhs.putLines == rhs.putLines
    }

    /// The latest contexts' coverage, by context generation.
    private var contexts: [UInt32: BandCoverage] = [:]
    /// What each context's wide row covers, where it has one.
    private var wideContexts: [UInt32: BandCoverage] = [:]
    private var contextOrder: [UInt32] = []
    /// How many recent contexts are remembered for frames still arriving.
    static let rememberedContexts = 8

    private var pending: DisplayFrame?
    private var pendingExtras: DisplayExtras?

    public init(waterfallLines: Int, expectsExtras: Bool = false) {
        history = WaterfallHistory(capacity: waterfallLines)
        self.expectsExtras = expectsExtras
    }

    /// The Core's context `generation` covers `coverage`: its frames are
    /// drawn there.
    /// `wide` is what its frames' wide row covers, nil for none.
    public mutating func note(context generation: UInt32, coverage: BandCoverage, wide: BandCoverage? = nil) {
        if contexts[generation] == nil {
            contextOrder.append(generation)
        }
        contexts[generation] = coverage
        wideContexts[generation] = wide.flatMap { $0.spanHz > 0 ? $0 : nil }
        while contextOrder.count > Self.rememberedContexts {
            let gone = contextOrder.removeFirst()
            contexts[gone] = nil
            wideContexts[gone] = nil
        }
    }

    /// A frame for this band, at `now` on the steady clock.
    public mutating func receive(frame: DisplayFrame, manualLevels: WaterfallHistory.Levels,
                                 at now: Double = BandState.steadyNow()) {
        commit(manualLevels: manualLevels, at: now)
        pending = frame
        pendingExtras = nil
        if !expectsExtras {
            commit(manualLevels: manualLevels, at: now)
        }
    }

    /// A datagram for this band: it goes with the waiting frame when it is
    /// that frame's.
    public mutating func receive(extras: DisplayExtras, manualLevels: WaterfallHistory.Levels,
                                 at now: Double = BandState.steadyNow()) {
        guard let waiting = pending, WaterfallHistory.belongs(extras, to: waiting) else {
            return
        }
        pendingExtras = extras
        commit(manualLevels: manualLevels, at: now)
    }

    /// Commits the waiting frame, with its datagram if it came, at `steady`
    /// on the steady clock. Its waterfall line goes in now, or waits for
    /// the row clock while lines are paced (``pacingLines``).
    public mutating func commit(manualLevels: WaterfallHistory.Levels, at steady: Double = BandState.steadyNow()) {
        guard let waiting = pending else {
            return
        }
        let coverage = contexts[waiting.contextGeneration]
        if waterfallHeld {
            // The line is dropped, but the Core's levels are still noted.
            history.append(frame: waiting, extras: pendingExtras, manualLevels: manualLevels, coverage: coverage,
                           addsLine: false)
        } else if pacingLines && waiting.waterfallAdvance && !waiting.waterfallDbm.isEmpty {
            // The Core's levels are noted now; the line, coloured against
            // them, waits for the clock.
            let waterfall = blanker.filtered(waiting.waterfallDbm, enabled: waterfallBlanking,
                transmitting: transmittingWaterfall, thresholdDb: waterfallBlankingThresholdDb)
            history.append(frame: waiting, extras: pendingExtras, manualLevels: manualLevels, coverage: coverage,
                           addsLine: false)
            let levels = (phoneSetsLevels ? manualLevels : (history.heldLevels ?? manualLevels))
                .adjusted(by: levelAdjustment)
            waitingLines.append(WaitingLine(frame: waiting, line: waterfall, levels: levels, coverage: coverage,
                wide: wideContexts[waiting.contextGeneration], transmitting: transmittingWaterfall,
                arrivedAt: steady, arrivedWall: clock()))
            while waitingLines.count > Self.mostLinesWaiting {
                put(waitingLines.removeFirst(), at: steady)
            }
        } else {
            if !pacingLines || waiting.waterfallAdvance {
                // Lines still waiting go in first, so the waterfall keeps its order.
                flushLines()
            }
            let now = clock()
            let waterfall = waiting.waterfallAdvance
                ? blanker.filtered(waiting.waterfallDbm, enabled: waterfallBlanking,
                    transmitting: transmittingWaterfall, thresholdDb: waterfallBlankingThresholdDb)
                : waiting.waterfallDbm
            let painted = DisplayFrame(endpointId: waiting.endpointId, contextGeneration: waiting.contextGeneration,
                encoderSequence: waiting.encoderSequence, producerTimestamp: waiting.producerTimestamp,
                isKeyframe: waiting.isKeyframe, waterfallAdvance: waiting.waterfallAdvance,
                minDbm: waiting.minDbm, maxDbm: waiting.maxDbm, traceDbm: waiting.traceDbm,
                waterfallDbm: waterfall, wideDbm: waiting.wideDbm)
            let added = history.append(frame: painted, extras: pendingExtras, manualLevels: manualLevels,
                                       coverage: coverage, adjustment: levelAdjustment, phoneLevels: phoneSetsLevels,
                                       time: now)
            foldIntoStack(waiting, coverage: coverage, wide: wideContexts[waiting.contextGeneration],
                          transmitting: transmittingWaterfall)
            if added && tracesLines {
                putLines.append(PutLine(sequence: waiting.encoderSequence, arrivedAt: steady, putAt: steady))
            }
        }
        holdPeaks(waiting)
        frame = waiting
        committedSerial &+= 1
        frameCoverage = coverage
        frameExtras = pendingExtras
        pending = nil
        pendingExtras = nil
    }

    /// Retires a frame waiting for extras when its media or endpoint ends.
    /// The already committed picture and waterfall remain visible.
    public mutating func discardPending() {
        // Lines already committed stay: they go in now.
        flushLines()
        stackFold.clear()
        pending = nil
        pendingExtras = nil
    }

    /// The classic peak hold after `frame`: each sample's highest level,
    /// started afresh from this frame's trace every ``peakHoldDelayMs`` and
    /// whenever the trace's width or context changes.
    private mutating func holdPeaks(_ frame: DisplayFrame) {
        guard let delay = peakHoldDelayMs, !frame.traceDbm.isEmpty else {
            return
        }
        let elapsed = frame.producerTimestamp &- peakHoldStart
        let fresh = peakHold?.count != frame.traceDbm.count || self.frame?.contextGeneration != frame.contextGeneration
            || frame.producerTimestamp < peakHoldStart || elapsed >= UInt64(max(delay, 1)) * 1_000_000
        if fresh || peakHold == nil {
            peakHold = frame.traceDbm
            peakHoldStart = frame.producerTimestamp
            return
        }
        for index in frame.traceDbm.indices where frame.traceDbm[index] > peakHold![index] {
            peakHold![index] = frame.traceDbm[index]
        }
    }

    /// Keeps as many waterfall lines as the band has pixel rows.
    public mutating func resizeWaterfall(lines: Int) {
        history.resize(capacity: lines)
    }

    /// A changed local level mode/endpoint must not reuse its prior Core pair.
    /// Painted history remains in its original colours and frequencies.
    public mutating func forgetWaterfallLevels() {
        history.forgetLevels()
    }

    /// At a key and an unkey on this band: the peak hold starts afresh and
    /// the waterfall's held levels are forgotten, so nothing blends from
    /// the receiver into the transmit display or back (the desktop clears
    /// its averaging and resets its waterfall AGC at both edges). The
    /// waterfall's lines stay.
    public mutating func resetSmoothing() {
        // The lines from before the edge go in before it, as they came.
        flushLines()
        stackFold.clear()
        peakHold = nil
        history.forgetLevels()
    }

    /// Starts over: a new session or a new endpoint.
    public mutating func clear() {
        waitingLines.removeAll()
        rowClock.reset()
        frameLock.reset()
        lastAdvanceNs = nil
        linesSinceStackRow = 0
        putLines.removeAll()
        history.clear()
        blanker.reset()
        stackFold.clear()
        stack.clear()
        frame = nil
        peakHold = nil
        frameCoverage = nil
        frameExtras = nil
        pending = nil
        pendingExtras = nil
    }
}
