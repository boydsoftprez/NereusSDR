// NereusSDR for iOS: the band's waterfall lines go in on the row clock's steady grid, not as they arrive
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
@testable import NereusBand
import NereusMedia
import Testing

/// JJ's build 7950 recording (2026-10-25): the waterfall moved in 16.7 and
/// 50 ms pairs, because each line went in as it came over Wi-Fi and a few
/// milliseconds of jitter moved it across a 60 Hz refresh. These run the
/// band's state the way the screen does: lines committed at their arrival
/// times, the display link reading the clock every 16.7 ms, and note the
/// frame each line went in on.
@Suite("Waterfall lines on the row clock")
struct WaterfallPacingTests {
    static let frame = 1.0 / 60
    static let period = 1.0 / 30
    /// The steady clock's time at the first frame, as the display link's.
    static let base = 1_000.0
    static let manual = WaterfallHistory.Levels(lowDbm: -130, highDbm: -30)

    /// Wall-clock seconds the state stamps on its rows, set by each frame.
    final class Wall: @unchecked Sendable {
        var now = 0.0
    }

    struct Run {
        /// The frame each line went in on, in order.
        var putFrames: [Int] = []
        var state: BandState
        /// The 3D stack's position (rows in plus the glide) after each frame.
        var stackPositions: [Double] = []
        /// The wall time each stack row was stamped with, and the frame time it went in.
        var rowStamps: [(stamped: Double, frame: Double)] = []
    }

    /// Lines arriving every line period from 200 ms, `phase` later, each
    /// moved up to `jitter` seconds either way, never reordered.
    static func arrivals(lines: Int, phase: Double, jitter: Double) -> [Double] {
        var times = (0 ..< lines).map { k in
            base + 0.2 + phase + Double(k) * period
                + Double(WaterfallRowClockTests.jitterNs(Int64(k), Int64(jitter * 1e9))) / 1e9
        }
        for index in times.indices.dropFirst() {
            times[index] = max(times[index], times[index - 1])
        }
        return times
    }

    static func state(paced: Bool, stacks: Bool = false, divider: Int = 1, wall: Wall = Wall()) -> BandState {
        var state = BandState(waterfallLines: 2_000)
        state.note(context: 1, coverage: BandCoverage(centerHz: 7_100_000, spanHz: 48_000))
        state.clock = { wall.now }
        state.stacksRows = stacks
        state.stackRowDivider = divider
        state.linePeriodSeconds = period
        state.pacesLines = paced
        return state
    }

    /// The lines arrive at `arrivals`; a frame every 1/60 s advances the
    /// clock, except inside `stall` (seconds after the first frame).
    static func run(_ arrivals: [Double], paced: Bool, stall: Range<Double>? = nil, stacks: Bool = false,
                    divider: Int = 1, beforeFrame: (Int, inout BandState) -> Void = { _, _ in }) -> Run {
        let wall = Wall()
        var run = Run(state: state(paced: paced, stacks: stacks, divider: divider, wall: wall))
        var next = 0
        let end = (arrivals.last ?? base) + 2
        var number = 0
        while base + Double(number) * frame <= end {
            defer { number += 1 }
            let now = base + Double(number) * frame
            // Lines that go in as they come show on the frame after they came.
            let lines = run.state.history.linesAppended
            while next < arrivals.count && arrivals[next] <= now {
                // Each line is stamped with its own arrival time, as it comes.
                wall.now = arrivals[next]
                run.state.receive(frame: BandFixtures.frame(trace: [-80, -70], sequence: UInt32(next + 1)),
                                  manualLevels: manual, at: arrivals[next])
                next += 1
            }
            if let stall, stall.contains(now - base) {
                // The screen shows nothing new: what came waits for the next frame.
                if run.state.history.linesAppended != lines {
                    Issue.record("a line went in during a screen stall")
                }
                continue
            }
            wall.now = now
            beforeFrame(number, &run.state)
            let rows = run.state.stack.rowsAppended
            run.state.advanceLines(at: now, frameSeconds: frame)
            for _ in 0 ..< Int(run.state.history.linesAppended - lines) {
                run.putFrames.append(number)
            }
            if run.state.stack.rowsAppended != rows, let stamped = run.state.stack.newestTime {
                run.rowStamps.append((stamped, now))
            }
            if stacks {
                run.stackPositions.append(Double(run.state.stack.rowsAppended) + (run.state.pacedStackGlide ?? 0))
            }
        }
        return run
    }

    /// The gaps, in frames, between lines that went in from `fromFrame` on.
    static func gaps(_ frames: [Int], from fromFrame: Int) -> [Int: Int] {
        var counts: [Int: Int] = [:]
        for (earlier, later) in zip(frames, frames.dropFirst()) where earlier >= fromFrame {
            counts[later - earlier, default: 0] += 1
        }
        return counts
    }

    static func mostInAFrame(_ frames: [Int]) -> Int {
        Dictionary(grouping: frames, by: { $0 }).values.map(\.count).max() ?? 0
    }

    @Test("lines arriving ±8 ms about 33.3 ms go in every second 60 Hz frame, at every phase against the screen",
          arguments: 0 ... 16)
    func jitteredArrivalsGoInEvenly(phaseMs: Int) {
        let arrivals = Self.arrivals(lines: 900, phase: Double(phaseMs) / 1000, jitter: 0.008)
        let paced = Self.run(arrivals, paced: true)
        #expect(paced.putFrames.count == arrivals.count)
        // After two seconds every line is 33.3 ms after the one before.
        let steady = Self.gaps(paced.putFrames, from: 120)
        #expect(steady == [2: steady[2] ?? 0], "\(steady)")
        #expect(paced.state.rowClock.reanchors == 0)
        #expect(paced.state.rowClock.starvedSlots <= 1)
    }

    @Test("the same lines going in as they arrive land 16.7 and 50 ms apart, the recording's pairs")
    func arrivalDrivenLinesGoInUnevenly() {
        // Where the jitter fits inside one refresh (phase 8 ms) arrival
        // happens to be even; across the phases about one gap in five is not.
        var uneven = 0
        var all = 0
        for phaseMs in 0 ... 16 {
            let arrivals = Self.arrivals(lines: 900, phase: Double(phaseMs) / 1000, jitter: 0.008)
            let arrived = Self.gaps(Self.run(arrivals, paced: false).putFrames, from: 120)
            uneven += arrived.filter { $0.key != 2 }.values.reduce(0, +)
            all += arrived.values.reduce(0, +)
        }
        #expect(uneven * 10 > all, "\(uneven) of \(all)")
    }

    @Test("a bunched burst of three rests the glide once and is back on the grid within two seconds")
    func burstOfThree() {
        var arrivals = Self.arrivals(lines: 900, phase: 0.004, jitter: 0.008)
        // Lines 300 and 301 held back, to come with 302.
        arrivals[300] = arrivals[302]
        arrivals[301] = arrivals[302]
        let run = Self.run(arrivals, paced: true)
        #expect(run.putFrames.count == arrivals.count)
        #expect(Self.mostInAFrame(run.putFrames) <= WaterfallRowClock.maxRowsPerFrame)
        #expect(run.state.rowClock.reanchors == 0)
        // Once at the start (the hand-off margin) and once for the burst.
        #expect(run.state.rowClock.starvedSlots <= 2)
        let burstFrame = Int(((arrivals[302] - Self.base) / Self.frame).rounded(.up))
        let after = Self.gaps(run.putFrames, from: burstFrame + 120)
        #expect(after == [2: after[2] ?? 0], "\(after)")
    }

    @Test("200 ms with no line: the glide rests, the lines play out at most two a frame, then steady again")
    func arrivalStall() {
        var arrivals = Self.arrivals(lines: 900, phase: 0.004, jitter: 0.008)
        let from = Self.base + 10
        let to = from + 0.2
        for index in arrivals.indices where arrivals[index] >= from && arrivals[index] < to {
            arrivals[index] = to
        }
        let run = Self.run(arrivals, paced: true)
        #expect(run.putFrames.count == arrivals.count)
        #expect(Self.mostInAFrame(run.putFrames) <= WaterfallRowClock.maxRowsPerFrame)
        #expect(run.state.rowClock.starvedSlots <= 2)
        #expect(run.state.rowClock.reanchors <= 1)
        let after = Self.gaps(run.putFrames, from: Int((to - Self.base) / Self.frame) + 120)
        #expect(after == [2: after[2] ?? 0], "\(after)")
    }

    @Test("200 ms with no screen frame: the lines behind play out two a frame, then steady again")
    func screenStall() {
        let arrivals = Self.arrivals(lines: 900, phase: 0.004, jitter: 0.008)
        let run = Self.run(arrivals, paced: true, stall: 20 ..< 20.2)
        #expect(run.putFrames.count == arrivals.count)
        #expect(Self.mostInAFrame(run.putFrames) == 2)
        #expect(run.state.rowClock.reanchors == 0)
        #expect(run.state.rowClock.starvedSlots <= 1)
        let after = Self.gaps(run.putFrames, from: Int(20.2 / Self.frame) + 60)
        #expect(after == [2: after[2] ?? 0], "\(after)")
    }

    @Test("the 3D glide reads the clock: it moves on every frame, and a row's fold is stamped when it went in")
    func glideAndFoldReadTheClock() {
        let arrivals = Self.arrivals(lines: 300, phase: 0.004, jitter: 0.008)
        let run = Self.run(arrivals, paced: true, stacks: true, divider: 4)
        #expect(run.state.stack.rowsAppended == 75)
        // Every frame after the first rows moves the stack on: no rest and
        // no jump as a row completes (a row is 8 frames at Match 4).
        let positions = Array(run.stackPositions.dropFirst(120).prefix(run.putFrames.count * 2 - 240))
        let steps = zip(positions, positions.dropFirst()).map { $1 - $0 }
        let steady = 1.0 / 8
        #expect(steps.allSatisfy { $0 > 0 && abs($0 - steady) < steady * 0.25 },
                "\(steps.map { ($0 * 1000).rounded() / 1000 })")
        // Each row was stamped with the frame that put its last line in,
        // not with when that line arrived.
        for stamp in run.rowStamps {
            #expect(stamp.stamped == stamp.frame)
        }
    }

    @Test("a key edge mid-row: the glide goes on to the full row and rests until the next, never stepping back")
    func keyEdgeMidRowRestsTheGlide() {
        // As the desktop's: its glide starts again only when a row goes in
        // (pushDssRow), and a fold restarted at a key edge leaves it.
        let arrivals = Self.arrivals(lines: 300, phase: 0.004, jitter: 0.008)
        var edgeFrame: Int?
        let run = Self.run(arrivals, paced: true, stacks: true, divider: 4) { number, state in
            if edgeFrame == nil, number >= 240, state.linesSinceStackRow == 2 {
                edgeFrame = number
                state.resetSmoothing()
            }
        }
        #expect(edgeFrame != nil, "the edge came mid-row")
        guard let edge = edgeFrame else {
            return
        }
        let positions = Array(run.stackPositions.dropFirst(120).prefix(run.putFrames.count * 2 - 240))
        let steps = zip(positions, positions.dropFirst()).map { $1 - $0 }
        #expect(steps.allSatisfy { $0 >= 0 }, "\(steps.map { ($0 * 1000).rounded() / 1000 })")
        // The stack rested a while after the edge, and two seconds on it
        // moves an eighth of a row a frame again.
        let after = Array(steps.dropFirst(edge - 120))
        #expect(after.prefix(24).contains(0))
        let steady = 1.0 / 8
        #expect(after.dropFirst(120).allSatisfy { $0 > 0 && abs($0 - steady) < steady * 0.25 },
                "\(after.dropFirst(120).map { ($0 * 1000).rounded() / 1000 })")
    }

    @Test("two views of one band advance the clock once a frame: a drawn frame takes at most two lines")
    func twoViewsTakeAtMostTwoLinesAFrame() {
        // The desktop's aDrawnFrameTakesAtMostTwoRows: there the ticker and
        // the drawn frame share a frame's two rows; here two views of one
        // band (two display links on one screen) do.
        var state = Self.state(paced: true)
        for sequence in 0 ..< 20 {
            state.receive(frame: BandFixtures.frame(trace: [-80], sequence: UInt32(sequence + 1)),
                          manualLevels: Self.manual, at: Self.base + 0.1 + Double(sequence) * Self.period)
        }
        let start = Self.base + 0.1 + Self.period
        let first = state.advanceLines(at: start, frameSeconds: Self.frame)
        #expect(first >= 1)
        // The other view, on the same frame or a stale one, takes none.
        #expect(state.advanceLines(at: start, frameSeconds: Self.frame) == 0)
        #expect(state.advanceLines(at: start - Self.frame, frameSeconds: Self.frame) == 0)
        // Six line periods with no frame: the lines are far behind the grid,
        // and the first view's frame takes two.
        let behind = start + 6 * Self.period
        #expect(state.advanceLines(at: behind, frameSeconds: Self.frame) == 2)
        #expect(state.advanceLines(at: behind, frameSeconds: Self.frame) == 0)
        // The frame after takes its own two.
        #expect(state.advanceLines(at: behind + Self.frame, frameSeconds: Self.frame) == 2)
        #expect(state.advanceLines(at: behind + Self.frame, frameSeconds: Self.frame) == 0)
    }

    /// Runs the band's draws as the view's display link does in 2D (the
    /// link's step: the clock first, then ``BandLineDraws``), with a change
    /// the screens show on every `urgentEvery`th frame. Returns the frame
    /// each line went in on, the frame each was first drawn on, and for each
    /// frame's trace the seconds from its arrival to the first draw after it.
    static func drawn(_ arrivals: [Double], urgentEvery: Int,
                      draws decide: (inout BandLineDraws, Double, Int, Bool, Bool) -> Bool)
        -> (put: [Int], shown: [Int], traceDelays: [Double]) {
        var state = Self.state(paced: true)
        var draws = BandLineDraws()
        var linkAwake = false
        var dirty = false
        var put: [Int] = []
        var shown: [Int] = []
        var shownLines = state.history.linesAppended
        var traceDelays: [Double] = []
        var tracesWaiting: [Double] = []
        var next = 0
        var number = 0
        let end = (arrivals.last ?? base) + 2
        while base + Double(number) * frame <= end {
            defer { number += 1 }
            let now = base + Double(number) * frame
            // Each arrival and each change asks for a draw; the first wakes the link.
            var asked = false
            while next < arrivals.count && arrivals[next] <= now {
                state.receive(frame: BandFixtures.frame(trace: [-80, -70], sequence: UInt32(next + 1)),
                              manualLevels: manual, at: arrivals[next])
                tracesWaiting.append(arrivals[next])
                next += 1
                asked = true
            }
            let urgent = number % urgentEvery == urgentEvery / 2
            if asked || urgent {
                dirty = true
                if !linkAwake {
                    linkAwake = true
                    draws.reset()
                }
            }
            guard linkAwake else {
                continue
            }
            let before = state.history.linesAppended
            let lines = state.advanceLines(at: now, frameSeconds: frame)
            for _ in 0 ..< Int(state.history.linesAppended - before) {
                put.append(number)
            }
            if lines > 0 {
                dirty = true
            }
            let flowing = state.linesWaiting > 0 || (state.rowClock.anchored && !state.rowClock.starved)
            if decide(&draws, now, lines, dirty, flowing) {
                dirty = false
                for _ in 0 ..< Int(state.history.linesAppended - shownLines) {
                    shown.append(number)
                }
                shownLines = state.history.linesAppended
                traceDelays += tracesWaiting.map { now - $0 }
                tracesWaiting.removeAll()
            }
        }
        return (put, shown, traceDelays)
    }

    @Test("2D draws each line on the frame it went in, 33.3 ms apart, with spots, markers and the grid drawn between",
          arguments: 0 ... 16)
    func drawnLinesStayEven(phaseMs: Int) {
        let arrivals = Self.arrivals(lines: 600, phase: Double(phaseMs) / 1000, jitter: 0.008)
        for urgentEvery in [3, 7, 11] {
            let run = Self.drawn(arrivals, urgentEvery: urgentEvery) { draws, now, lines, dirty, flowing in
                draws.draws(at: now, frame: Self.frame, interval: Self.period, stacked: false, linesPut: lines,
                            dirty: dirty, flowing: flowing)
            }
            #expect(run.put.count == arrivals.count)
            // No line waits a frame for the draws' cadence, nor shows early.
            #expect(run.shown == run.put, "phase \(phaseMs) ms, a change every \(urgentEvery) frames")
            let steady = Self.gaps(run.shown, from: 120)
            #expect(steady == [2: steady[2] ?? 0], "phase \(phaseMs) ms, a change every \(urgentEvery): \(steady)")
        }
    }

    @Test("2D draws each frame's trace on the first screen frame after it comes, not on the next line's frame",
          arguments: 0 ... 16)
    func tracesDrawOnTheNextFrame(phaseMs: Int) {
        let arrivals = Self.arrivals(lines: 600, phase: Double(phaseMs) / 1000, jitter: 0.008)
        // A change the screens show every 3 or 7 frames, or once only.
        for urgentEvery in [3, 7, 1_000_000] {
            let run = Self.drawn(arrivals, urgentEvery: urgentEvery) { draws, now, lines, dirty, flowing in
                draws.draws(at: now, frame: Self.frame, interval: Self.period, stacked: false, linesPut: lines,
                            dirty: dirty, flowing: flowing)
            }
            #expect(run.traceDelays.count == arrivals.count)
            // As before the clock: a trace waits at most for the next screen frame.
            let late = run.traceDelays.filter { $0 > Self.frame + 1e-9 }
            #expect(late.isEmpty, """
                phase \(phaseMs) ms, a change every \(urgentEvery) frames: \(late.count) traces waited past \
                the next frame, the longest \((late.max() ?? 0) * 1000) ms
                """)
            // The lines still show on the frames the clock put them in.
            #expect(run.shown == run.put, "phase \(phaseMs) ms, a change every \(urgentEvery) frames")
        }
    }

    @Test("the draws' cadence: every second frame at 30 a second, and again after a rest")
    func frameDrawCadence() {
        var cadence = FrameDrawCadence()
        let frames = (0 ..< 8).map { Self.base + Double($0) * Self.frame }
        let drawn = frames.indices.filter { cadence.take(at: frames[$0], interval: Self.period, frame: Self.frame) }
        #expect(drawn == [0, 2, 4, 6])
        // A frame long after the last (the link rested) draws and starts the cadence from itself.
        let rested = (40 ... 42).map {
            cadence.take(at: Self.base + Double($0) * Self.frame, interval: Self.period, frame: Self.frame)
        }
        #expect(rested == [true, false, true])
        cadence.reset()
        let woke = cadence.take(at: Self.base + 43 * Self.frame, interval: Self.period, frame: Self.frame)
        #expect(woke)
    }

    @Test("lines put in at once outside a screen frame are stamped by the state's steady clock")
    func flushedLinesTakeTheSteadyClock() {
        var state = Self.state(paced: true)
        state.tracesLines = true
        state.steadyClock = { 4_242 }
        state.receive(frame: BandFixtures.frame(trace: [-80], sequence: 1), manualLevels: Self.manual, at: Self.base)
        state.flushLines()
        #expect(state.takePutLines().map(\.putAt) == [4_242])
        state.receive(frame: BandFixtures.frame(trace: [-80], sequence: 2), manualLevels: Self.manual, at: Self.base)
        state.flushLines(at: 7)
        #expect(state.takePutLines().map(\.putAt) == [7])
    }

    @Test("lines waiting go in at once, in order, when pacing stops, at a hold, and at a key edge")
    func waitingLinesGoInAtOnce() {
        for edge in 0 ..< 3 {
            var state = Self.state(paced: true)
            for sequence in 1 ... 3 {
                state.receive(frame: BandFixtures.frame(trace: [-80, Float(-100 + sequence)], sequence: UInt32(sequence)),
                              manualLevels: Self.manual, at: Self.base + Double(sequence) * Self.period)
            }
            #expect(state.history.count == 0 && state.linesWaiting == 3)
            switch edge {
            case 0: state.pacesLines = false
            case 1: state.waterfallHeld = true
            default: state.resetSmoothing()
            }
            #expect(state.history.count == 3 && state.linesWaiting == 0, "edge \(edge)")
            #expect(!state.rowClock.anchored)
        }
    }

    @Test("past the most lines that wait, the oldest goes in at once: none is lost")
    func overflowPutsTheOldestIn() {
        var state = Self.state(paced: true)
        let lines = 40
        for sequence in 1 ... lines {
            state.receive(frame: BandFixtures.frame(trace: [-80], sequence: UInt32(sequence)), manualLevels: Self.manual,
                          at: Self.base)
        }
        #expect(state.linesWaiting == BandState.mostLinesWaiting)
        #expect(state.history.count == lines - BandState.mostLinesWaiting)
    }

    @Test("a line waiting is coloured against the Core's levels when it came")
    func waitingLineKeepsItsLevels() {
        var state = Self.state(paced: true)
        state.expectsExtras = true
        let first = BandFixtures.frame(trace: [-80], sequence: 1)
        state.receive(frame: first, manualLevels: Self.manual, at: Self.base)
        state.receive(extras: BandFixtures.extras(for: first, levels: (-100, 0)), manualLevels: Self.manual,
                      at: Self.base)
        let second = BandFixtures.frame(trace: [-80], sequence: 2)
        state.receive(frame: second, manualLevels: Self.manual, at: Self.base + Self.period)
        state.receive(extras: BandFixtures.extras(for: second, levels: (-60, -20)), manualLevels: Self.manual,
                      at: Self.base + Self.period)
        #expect(state.history.heldLevels == WaterfallHistory.Levels(lowDbm: -60, highDbm: -20))
        state.flushLines()
        // Each line keeps the levels of its own datagram.
        let own = WaterfallHistory.paletteIndex(dbm: -80, levels: .init(lowDbm: -100, highDbm: 0))
        let later = WaterfallHistory.paletteIndex(dbm: -80, levels: .init(lowDbm: -60, highDbm: -20))
        #expect(own != later)
        #expect(state.history.line(age: 1)?.first == own)
        #expect(state.history.line(age: 0)?.first == later)
    }

    @Test("without the Core's line period, or with no view driving, lines go in as they come")
    func unpacedLinesGoInAtOnce() {
        var state = Self.state(paced: false)
        state.receive(frame: BandFixtures.frame(trace: [-80], sequence: 1), manualLevels: Self.manual, at: Self.base)
        #expect(state.history.count == 1 && state.pacedStackGlide == nil)
        state.pacesLines = true
        state.linePeriodSeconds = 0
        state.receive(frame: BandFixtures.frame(trace: [-80], sequence: 2), manualLevels: Self.manual, at: Self.base)
        #expect(state.history.count == 2 && !state.pacingLines)
    }
}
