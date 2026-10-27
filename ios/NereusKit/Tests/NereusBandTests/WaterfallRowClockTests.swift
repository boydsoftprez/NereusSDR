// NereusSDR for iOS: the row clock's grid, held to the desktop clock's own cases
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

@testable import NereusBand
import Testing

/// The grid the waterfall's lines go in on (``WaterfallRowClock``), run
/// against the cases the desktop's clock is held to
/// (`tests/tst_remote_waterfall_cadence.cpp`): lines handed over about 200 ms
/// before they are due, or (as the phone's Core sends them) due as they
/// come, a line every 66.67 ms, frames at 60 Hz, the audio delay stepping,
/// the Core's clock drifting against this one.
@Suite("Waterfall row clock")
struct WaterfallRowClockTests {
    static let ms: Int64 = 1_000_000
    static let frameNs: Int64 = 1_000_000_000 / 60
    static let twoFrameRowNs: Int64 = 2 * 1_000_000_000 / 30

    /// One line: when it is due on screen and when it was handed over.
    struct GridRow {
        var dueNs: Int64 = 0
        var handedNs: Int64 = 0
        /// The due time without its jitter.
        var nominalDueNs: Int64 = 0
    }

    /// What the clock did over a run of frames.
    struct GridRun {
        var pushed = 0
        var framesWithTwoRows = 0
        var maxRowsInAFrame = 0
        /// Frames after the first line went in where the glide did not
        /// move, and the last of them.
        var stillFrames = 0
        var lastStillNs: Int64 = -1
        /// Lines that went in later than the first frame of their slide.
        var rowsInLate = 0
        /// Frames that put lines in with the glide already at the full line.
        var framesPutWithoutGlide = 0
        /// The largest step of the glide on one frame, in lines.
        var maxGlideStepRows = 0.0
        /// Each line put in alone on its frame: its index, slot and frame.
        struct Put {
            var row: Int
            var slotNs: Int64
            var atNs: Int64
        }
        var puts: [Put] = []
    }

    /// Lines (in hand-off order) reach the queue at `handedNs`; a frame every
    /// ``frameNs`` from 0 to `endNs` advances the clock. Frames inside
    /// `stallFromNs..<stallToNs` do not happen.
    static func runGrid(_ clock: inout WaterfallRowClock, _ rows: [GridRow], endNs: Int64,
                        stallFromNs: Int64 = -1, stallToNs: Int64 = -1) -> GridRun {
        var run = GridRun()
        var queue: [Int] = []
        var next = 0
        var lastPosition = -1.0
        let frameRows = Double(frameNs) / Double(clock.periodNs)
        var now: Int64 = 0
        while now <= endNs {
            defer { now += frameNs }
            while next < rows.count && rows[next].handedNs <= now {
                queue.append(next)
                next += 1
            }
            if now >= stallFromNs && now < stallToNs {
                continue
            }
            let put = queue.isEmpty
                ? clock.advance(nowNs: now, queuedRows: 0, frontDueNs: now, frontQueuedNs: now)
                : clock.advance(nowNs: now, queuedRows: queue.count, frontDueNs: rows[queue[0]].dueNs,
                                frontQueuedNs: rows[queue[0]].handedNs)
            run.maxRowsInAFrame = max(run.maxRowsInAFrame, put)
            if put >= 2 {
                run.framesWithTwoRows += 1
            }
            if put > 0 && clock.progress >= 1 {
                run.framesPutWithoutGlide += 1
            }
            if put == 1 {
                run.puts.append(.init(row: queue[0], slotNs: clock.lastSlotNs, atNs: now))
                // In on the first frame of its slide.
                if clock.progress > frameRows * 1.05 {
                    run.rowsInLate += 1
                }
            }
            queue.removeFirst(put)
            run.pushed += put
            let position = Double(run.pushed) + clock.progress
            if run.pushed > 0 && lastPosition >= 0 {
                if position <= lastPosition {
                    run.stillFrames += 1
                    run.lastStillNs = now
                }
                run.maxGlideStepRows = max(run.maxGlideStepRows, position - lastPosition)
            }
            if run.pushed > 0 {
                lastPosition = position
            }
        }
        return run
    }

    /// A fixed pseudo-random offset in `-spanNs...spanNs` for line `k`, the
    /// desktop test's own.
    static func jitterNs(_ k: Int64, _ spanNs: Int64) -> Int64 {
        guard spanNs > 0 else {
            return 0
        }
        var seed = UInt32(truncatingIfNeeded: 12_345 &+ UInt32(truncatingIfNeeded: k) &* 2_654_435_761)
        seed = seed &* 1_664_525 &+ 1_013_904_223
        seed ^= seed >> 13
        return Int64(seed % UInt32(2 * spanNs + 1)) - spanNs
    }

    /// Lines due from 1 s every `periodNs`, each due time moved by up to
    /// `dueJitterNs` either way, handed over `leadNs` before it is due.
    static func gridRows(_ periodNs: Int64, _ dueJitterNs: Int64, _ leadNs: Int64, endNs: Int64) -> [GridRow] {
        var rows: [GridRow] = []
        var k: Int64 = 0
        while true {
            var row = GridRow()
            row.nominalDueNs = 1_000 * ms + k * periodNs
            row.dueNs = row.nominalDueNs + jitterNs(k, dueJitterNs)
            row.handedNs = row.dueNs - leadNs
            if row.handedNs > endNs {
                break
            }
            rows.append(row)
            k += 1
        }
        // Handed over in order: the hand-off jitter never reorders lines.
        for index in rows.indices.dropFirst() {
            rows[index].handedNs = max(rows[index].handedNs, rows[index - 1].handedNs)
        }
        return rows
    }

    /// The largest |slot - due| of the lines put in alone with slots from
    /// `fromNs` on; `nominal`: against the due time without its jitter.
    static func maxSlotErrorMs(_ run: GridRun, _ rows: [GridRow], from fromNs: Int64, nominal: Bool = false) -> Double {
        var worst = 0.0
        for put in run.puts where put.slotNs >= fromNs {
            let due = nominal ? rows[put.row].nominalDueNs : rows[put.row].dueNs
            worst = max(worst, abs(Double(put.slotNs - due)) / Double(ms))
        }
        return worst
    }

    static func handOffOrder(_ rows: [GridRow]) -> [GridRow] {
        rows.sorted { $0.handedNs < $1.handedNs }
    }

    @Test("a steady stream goes in one line a slot")
    func steadyStream() {
        var clock = WaterfallRowClock()
        clock.setPeriodNs(Self.twoFrameRowNs)
        let endNs = 30_000 * Self.ms
        let rows = Self.gridRows(Self.twoFrameRowNs, 7_500_000, 200 * Self.ms, endNs: endNs)
        let run = Self.runGrid(&clock, rows, endNs: endNs)
        #expect(run.rowsInLate == 0)
        #expect(run.framesWithTwoRows == 0)
        #expect(run.framesPutWithoutGlide == 0)
        #expect(run.stillFrames == 0)
        #expect(clock.starvedSlots == 0)
        #expect(clock.reanchors == 0)
        #expect(run.puts.first?.slotNs == rows.first?.dueNs)
        let worst = Self.maxSlotErrorMs(run, rows, from: rows[0].dueNs + 2_000 * Self.ms)
        #expect(worst < 10, "\(worst)")
        #expect(run.pushed >= rows.count - 4)
    }

    @Test("a step of the audio delay is followed over many lines, never as a jump", arguments: [20, -20])
    func followsAStep(stepMs: Int64) {
        let stepNs = stepMs * Self.ms
        var clock = WaterfallRowClock()
        clock.setPeriodNs(Self.twoFrameRowNs)
        let endNs = 30_000 * Self.ms
        var rows = Self.gridRows(Self.twoFrameRowNs, 7_500_000, 200 * Self.ms, endNs: endNs)
        for index in 150 ..< rows.count {
            rows[index].dueNs += stepNs
            rows[index].nominalDueNs += stepNs
            rows[index].handedNs += stepNs
        }
        rows = Self.handOffOrder(rows)
        let run = Self.runGrid(&clock, rows, endNs: endNs)
        let steadyStep = Double(Self.frameNs) / Double(Self.twoFrameRowNs)
        #expect(run.maxGlideStepRows <= 2 * steadyStep, "\(run.maxGlideStepRows)")
        #expect(run.stillFrames == 0)
        #expect(run.framesWithTwoRows == 0)
        #expect(clock.reanchors == 0)
        let worst = Self.maxSlotErrorMs(run, rows, from: rows[150].nominalDueNs + 5_000 * Self.ms, nominal: true)
        #expect(worst < 5, "\(worst)")
    }

    @Test("the rate takes up the Core's clock drifting against this one", arguments: [0.005, -0.005])
    func followsDrift(drift: Double) {
        var clock = WaterfallRowClock()
        clock.setPeriodNs(Self.twoFrameRowNs)
        let endNs = 120_000 * Self.ms
        let spacing = Int64(Double(Self.twoFrameRowNs) * (1 + drift))
        let rows = Self.gridRows(spacing, 7_500_000, 200 * Self.ms, endNs: endNs)
        let run = Self.runGrid(&clock, rows, endNs: endNs)
        #expect(clock.starvedSlots == 0)
        #expect(run.framesWithTwoRows == 0)
        #expect(run.stillFrames == 0)
        let worst = Self.maxSlotErrorMs(run, rows, from: rows[0].dueNs + 10_000 * Self.ms)
        #expect(worst < 10, "\(worst)")
        #expect(drift > 0 ? clock.rate < 1 : clock.rate > 1)
    }

    @Test("a late hand-off rests the glide and keeps the rate learnt")
    func keepsItsRateThroughLateHandOffs() {
        var clock = WaterfallRowClock()
        clock.setPeriodNs(Self.twoFrameRowNs)
        let endNs = 300_000 * Self.ms
        let spacing = Int64(Double(Self.twoFrameRowNs) * 1.005)
        var rows = Self.gridRows(spacing, 5_500_000, 0, endNs: endNs)
        for index in stride(from: 149, to: rows.count, by: 150) {
            rows[index].dueNs += 20 * Self.ms
            rows[index].handedNs += 20 * Self.ms
        }
        rows = Self.handOffOrder(rows)
        var queue: [Int] = []
        var next = 0
        var pushed = 0
        var starvedSeen = 0
        var resting = false
        var starvedRowsIn = 0
        var worstRateStep = 0.0
        var now: Int64 = 0
        while now <= endNs {
            defer { now += Self.frameNs }
            while next < rows.count && rows[next].handedNs <= now {
                queue.append(next)
                next += 1
            }
            let rateBefore = clock.rate
            let put = queue.isEmpty
                ? clock.advance(nowNs: now, queuedRows: 0, frontDueNs: now, frontQueuedNs: now)
                : clock.advance(nowNs: now, queuedRows: queue.count, frontDueNs: rows[queue[0]].dueNs,
                                frontQueuedNs: rows[queue[0]].handedNs)
            if resting && put > 0 {
                starvedRowsIn += 1
                worstRateStep = max(worstRateStep, abs(clock.rate - rateBefore))
                resting = false
            }
            if clock.starvedSlots != starvedSeen {
                starvedSeen = clock.starvedSlots
                resting = true
            }
            queue.removeFirst(put)
            pushed += put
        }
        #expect(starvedRowsIn >= 3, "\(starvedRowsIn)")
        #expect(clock.reanchors == 0)
        #expect(worstRateStep < 0.001, "\(worstRateStep)")
        #expect(clock.rate < 0.997)
        #expect(pushed >= rows.count - 3)
    }

    @Test("a hold that grows starts the grid again at its line")
    func startsAgainWhenTheHoldGrows() throws {
        var clock = WaterfallRowClock()
        clock.setPeriodNs(Self.twoFrameRowNs)
        let endNs = 20_000 * Self.ms
        var rows = Self.gridRows(Self.twoFrameRowNs, 7_500_000, 200 * Self.ms, endNs: endNs)
        for index in 150 ..< rows.count {
            rows[index].dueNs += 400 * Self.ms
            rows[index].nominalDueNs += 400 * Self.ms
            rows[index].handedNs += 400 * Self.ms
        }
        let run = Self.runGrid(&clock, rows, endNs: endNs)
        #expect(clock.reanchors == 1)
        #expect(run.framesWithTwoRows == 0)
        for put in run.puts {
            #expect(put.slotNs >= rows[put.row].dueNs - 10 * Self.ms, "line \(put.row)")
        }
        let first = try #require(run.puts.first { $0.row == 150 })
        #expect(first.slotNs == rows[150].dueNs)
        let worst = Self.maxSlotErrorMs(run, rows, from: rows[150].dueNs)
        #expect(worst < 10, "\(worst)")
    }

    @Test("lines with no due time run a line period and the margin behind their arrival, steadily")
    func runsARowBehindRowsWithNoDueTime() {
        var clock = WaterfallRowClock()
        clock.setPeriodNs(Self.twoFrameRowNs)
        let endNs = 30_000 * Self.ms
        let rows = Self.gridRows(Self.twoFrameRowNs, 5_500_000, 0, endNs: endNs)
        let run = Self.runGrid(&clock, rows, endNs: endNs)
        #expect(clock.starvedSlots <= 1)
        #expect(run.framesWithTwoRows == 0)
        #expect(clock.reanchors == 0)
        #expect(run.lastStillNs < 3_000 * Self.ms, "\(run.lastStillNs)")
        var least = 1e9
        var most = 0.0
        for put in run.puts where put.atNs >= 3_000 * Self.ms {
            let late = Double(put.slotNs - rows[put.row].dueNs) / Double(Self.ms)
            least = min(least, late)
            most = max(most, late)
        }
        #expect(least >= 66 && most < 66.67 + 30, "\(least) \(most)")
        #expect(most - least < 11 + 16.7, "\(least) \(most)")
    }

    @Test("lines bound by their hand-off still teach the rate the Core's clock",
          arguments: [(0.005, Int64(0)), (-0.005, Int64(0)), (0.005, Int64(20)), (-0.005, Int64(20))])
    func followsDriftForRowsBoundByTheirHandOff(drift: Double, leadMs: Int64) {
        var clock = WaterfallRowClock()
        clock.setPeriodNs(Self.twoFrameRowNs)
        let endNs = 300_000 * Self.ms
        let spacing = Int64(Double(Self.twoFrameRowNs) * (1 + drift))
        let rows = Self.gridRows(spacing, 5_500_000, leadMs * Self.ms, endNs: endNs)
        let run = Self.runGrid(&clock, rows, endNs: endNs)
        #expect(clock.reanchors == 0)
        #expect(run.framesWithTwoRows == 0)
        #expect(clock.starvedSlots <= 1)
        #expect(run.lastStillNs < 3_000 * Self.ms, "\(run.lastStillNs)")
        #expect(run.pushed >= rows.count - 3)
        #expect(drift > 0 ? clock.rate < 1 : clock.rate > 1, "\(clock.rate)")
        var least = 1e9
        var most = 0.0
        for put in run.puts where put.atNs >= 10_000 * Self.ms {
            let late = Double(put.slotNs - rows[put.row].handedNs) / Double(Self.ms)
            least = min(least, late)
            most = max(most, late)
        }
        #expect(least >= 66 && most - least < 11 + 16.7, "\(least) \(most)")
    }

    @Test("lines that come late rest the glide once, then lock back onto their due times")
    func restsOnceForRowsThatComeLate() {
        var clock = WaterfallRowClock()
        clock.setPeriodNs(Self.twoFrameRowNs)
        let endNs = 20_000 * Self.ms
        var rows = Self.gridRows(Self.twoFrameRowNs, 0, 200 * Self.ms, endNs: endNs)
        for index in rows.indices where rows[index].handedNs > 5_000 * Self.ms && rows[index].handedNs < 5_300 * Self.ms {
            rows[index].handedNs = 5_300 * Self.ms
        }
        let run = Self.runGrid(&clock, rows, endNs: endNs)
        #expect(clock.starvedSlots == 1)
        #expect(run.stillFrames > 0 && run.stillFrames < 12, "\(run.stillFrames)")
        #expect(run.framesWithTwoRows == 0)
        #expect(clock.reanchors == 0)
        let worst = Self.maxSlotErrorMs(run, rows, from: 12_000 * Self.ms)
        #expect(worst < 5, "\(worst)")
    }

    @Test("lines behind a stall of the screen play out two a frame, gliding")
    func playsAStallOutTwoRowsAFrame() {
        var clock = WaterfallRowClock()
        clock.setPeriodNs(Self.twoFrameRowNs)
        let endNs = 8_000 * Self.ms
        let rows = Self.gridRows(Self.twoFrameRowNs, 0, 200 * Self.ms, endNs: endNs)
        let run = Self.runGrid(&clock, rows, endNs: endNs, stallFromNs: 3_000 * Self.ms, stallToNs: 3_400 * Self.ms)
        #expect(run.maxRowsInAFrame == 2)
        #expect(run.framesWithTwoRows >= 1 && run.framesWithTwoRows <= 3)
        #expect(run.framesPutWithoutGlide == 0)
        #expect(run.maxGlideStepRows <= 2, "\(run.maxGlideStepRows)")
        #expect(clock.starvedSlots == 0)
        #expect(clock.reanchors == 0)
        let worst = Self.maxSlotErrorMs(run, rows, from: 4_000 * Self.ms)
        #expect(worst < 1, "\(worst)")
    }

    @Test("stopped, nothing goes in; a reset or a new period starts the grid again")
    func stopsAndStartsAgain() {
        var clock = WaterfallRowClock()
        #expect(clock.advance(nowNs: 10 * Self.twoFrameRowNs, queuedRows: 5, frontDueNs: 0, frontQueuedNs: 0) == 0)
        clock.setPeriodNs(Self.twoFrameRowNs)
        let due = 500 * Self.ms
        #expect(clock.advance(nowNs: due - Self.twoFrameRowNs - Self.ms, queuedRows: 1, frontDueNs: due,
                              frontQueuedNs: 0) == 0)
        #expect(clock.progress == 1)
        #expect(clock.advance(nowNs: due - Self.twoFrameRowNs, queuedRows: 1, frontDueNs: due, frontQueuedNs: 0) == 1)
        #expect(clock.lastSlotNs == due)
        #expect(clock.progress == 0)
        clock.reset()
        #expect(!clock.anchored)
        #expect(clock.advance(nowNs: 2 * due - Self.twoFrameRowNs, queuedRows: 1, frontDueNs: 2 * due,
                              frontQueuedNs: 0) == 1)
        #expect(clock.lastSlotNs == 2 * due)
        clock.setPeriodNs(1_000_000_000 / 30)
        #expect(!clock.anchored)
    }
}
