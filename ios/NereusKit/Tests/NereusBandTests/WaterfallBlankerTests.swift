// NereusSDR for iOS: tests for NB Blank's row decision on the phone
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-23: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code (Claude Opus 5.5):
//   new. The cases of the desktop's tst_waterfall_impulse_blanker (PR 625),
//   and the reset when transmit ends, in the blanker and in BandState.

import Testing
@testable import NereusBand

/// WaterfallBlanker on rows of 800 bins with a floor of -120 dB and +/-2 dB
/// of noise, threshold 3 dB unless stated, as the desktop's helper is tested:
/// a fence pulse is blanked alone and as a train; a loud station on 5 % or
/// 25 % of the bins blanks nothing; a lasting rise is blanked for two rows
/// and then followed; nothing is a burst before 8 rows; a row with no finite
/// bin is not blanked and leaves the baseline as it was.
@Suite struct WaterfallBlankerTests {
    typealias Verdict = WaterfallBlanker.Verdict

    static let bins = 800
    static let floorDb: Float = -120
    static let noiseDb: Float = 2

    /// Rows of the floor with deterministic uniform noise (SplitMix64).
    struct Rows {
        private var state: UInt64 = 20_261_022

        private mutating func noise() -> Float {
            state &+= 0x9E37_79B9_7F4A_7C15
            var z = state
            z = (z ^ (z >> 30)) &* 0xBF58_476D_1CE4_E5B9
            z = (z ^ (z >> 27)) &* 0x94D0_49BB_1331_11EB
            z ^= z >> 31
            let unit = Float(z >> 40) / Float(1 << 24)
            return (unit * 2 - 1) * WaterfallBlankerTests.noiseDb
        }

        /// Every bin `riseDb` up, and bins [signalFirst, signalFirst +
        /// signalBins) `signalDb` over the floor.
        mutating func row(_ riseDb: Float = 0, signalFirst: Int = 0, signalBins: Int = 0,
                          signalDb: Float = 0) -> [Float] {
            (0..<WaterfallBlankerTests.bins).map { index in
                var value = WaterfallBlankerTests.floorDb + riseDb + noise()
                if index >= signalFirst && index < signalFirst + signalBins {
                    value += signalDb
                }
                return value
            }
        }
    }

    // A fence charger's click: one row, every bin up.
    @Test func fencePulseIsBlankedAndTheNextRowIsNot() {
        var blanker = WaterfallBlanker()
        var rows = Rows()
        for index in 0..<32 {
            #expect(blanker.judge(rows.row(), thresholdDb: 3) == .accepted, "row \(index)")
        }
        #expect(blanker.judge(rows.row(10), thresholdDb: 3) == .blanked)
        #expect(blanker.judge(rows.row(), thresholdDb: 3) == .accepted)
    }

    // About once a second: a pulse every 20 rows.
    @Test func fenceTrainBlanksEveryPulseAndNoOtherRow() {
        var blanker = WaterfallBlanker()
        var rows = Rows()
        var pulses = 0
        for index in 0..<400 {
            let pulse = index % 20 == 19
            let verdict = blanker.judge(rows.row(pulse ? 10 : 0), thresholdDb: 3)
            #expect(verdict == (pulse ? .blanked : .accepted), "row \(index)")
            pulses += pulse ? 1 : 0
        }
        #expect(pulses == 20)
    }

    // A loud SSB station on 5 % of the bins (the 75 m freeze).
    @Test func loudTalkerOnFivePercentBlanksNothing() {
        var blanker = WaterfallBlanker()
        var rows = Rows()
        for _ in 0..<32 {
            _ = blanker.judge(rows.row(), thresholdDb: 3)
        }
        var blanked = 0
        for _ in 0..<200 {
            blanked += blanker.judge(rows.row(0, signalFirst: 380, signalBins: 40, signalDb: 40),
                                     thresholdDb: 3) == .accepted ? 0 : 1
        }
        #expect(blanked == 0)
    }

    // A wide loud signal on 25 % of the bins.
    @Test func wideLoudSignalOnAQuarterBlanksNothing() {
        var blanker = WaterfallBlanker()
        var rows = Rows()
        for _ in 0..<32 {
            _ = blanker.judge(rows.row(), thresholdDb: 3)
        }
        var blanked = 0
        for _ in 0..<200 {
            blanked += blanker.judge(rows.row(0, signalFirst: 300, signalBins: 200, signalDb: 30),
                                     thresholdDb: 3) == .accepted ? 0 : 1
        }
        #expect(blanked == 0)
    }

    // The floor rises 10 dB and stays. Rows 40 and 41 are blanked; rows 42
    // to 58 are accepted over the hold while the baseline follows (the
    // ring's median reaches the new floor once 17 of its 32 rows are risen
    // rows); row 59 on is judged against the new floor and is no burst.
    @Test func sustainedRiseBlanksTwoRowsThenIsFollowed() {
        var blanker = WaterfallBlanker()
        var rows = Rows()
        for index in 0..<40 {
            #expect(blanker.judge(rows.row(), thresholdDb: 3) == .accepted, "row \(index)")
        }
        var blanked = 0
        for index in 40..<240 {
            let verdict = blanker.judge(rows.row(10), thresholdDb: 3)
            let expected: Verdict = index < 42 ? .blanked : index <= 58 ? .acceptedOverHold : .accepted
            #expect(verdict == expected, "row \(index)")
            blanked += verdict == .blanked ? 1 : 0
        }
        #expect(blanked == 2)
    }

    // At most two rows in a row; a row that is no burst ends the run.
    @Test func holdLimitIsTwoRowsAndARunEnds() {
        var blanker = WaterfallBlanker()
        var rows = Rows()
        for _ in 0..<32 {
            _ = blanker.judge(rows.row(), thresholdDb: 3)
        }
        #expect(WaterfallBlanker.maxConsecutiveRows == 2)
        #expect(blanker.judge(rows.row(10), thresholdDb: 3) == .blanked)
        #expect(blanker.judge(rows.row(10), thresholdDb: 3) == .blanked)
        #expect(blanker.consecutiveBurstRows == 2)
        #expect(blanker.judge(rows.row(), thresholdDb: 3) == .accepted)
        #expect(blanker.consecutiveBurstRows == 0)
        // A fresh run: two more blanked, the third accepted.
        #expect(blanker.judge(rows.row(10), thresholdDb: 3) == .blanked)
        #expect(blanker.judge(rows.row(10), thresholdDb: 3) == .blanked)
        #expect(blanker.judge(rows.row(10), thresholdDb: 3) == .acceptedOverHold)
        // Reset forgets the run and the history.
        blanker.reset()
        #expect(blanker.consecutiveBurstRows == 0)
        #expect(blanker.historyRows == 0)
    }

    // Nothing is a burst before 8 rows of history.
    @Test func noBurstBeforeEightRowsOfHistory() {
        #expect(WaterfallBlanker.rowsBeforeDetection == 8)
        for history in 0..<8 {
            var blanker = WaterfallBlanker()
            var rows = Rows()
            for _ in 0..<history {
                _ = blanker.judge(rows.row(), thresholdDb: 3)
            }
            #expect(blanker.judge(rows.row(10), thresholdDb: 3) == .accepted, "history \(history)")
        }
        var blanker = WaterfallBlanker()
        var rows = Rows()
        for _ in 0..<8 {
            _ = blanker.judge(rows.row(), thresholdDb: 3)
        }
        #expect(blanker.judge(rows.row(10), thresholdDb: 3) == .blanked)
    }

    // A row with no finite bin.
    @Test func nonFiniteRowIsNotBlankedAndLeavesTheBaseline() {
        var blanker = WaterfallBlanker()
        var rows = Rows()
        for _ in 0..<20 {
            _ = blanker.judge(rows.row(), thresholdDb: 3)
        }
        let nan = [Float](repeating: .nan, count: Self.bins)
        let negativeInfinity = [Float](repeating: -.infinity, count: Self.bins)
        #expect(blanker.judge(nan, thresholdDb: 3) == .noFiniteBins)
        #expect(blanker.judge(negativeInfinity, thresholdDb: 3) == .noFiniteBins)
        #expect(blanker.historyRows == 20)
        #expect(blanker.judge(rows.row(), thresholdDb: 3) == .accepted)
        // Still judged against the floor: a pulse is blanked.
        #expect(blanker.judge(rows.row(10), thresholdDb: 3) == .blanked)
        #expect(blanker.judge(rows.row(), thresholdDb: 3) == .accepted)
        // An empty row counts as no finite bin.
        #expect(blanker.judge([], thresholdDb: 3) == .noFiniteBins)
    }

    // A +10 dB pulse with -inf in 500 of its 800 bins is still blanked:
    // were the -inf bins counted, the row's median would be -inf.
    @Test func mostlyNonFinitePulseIsJudgedOnTheRest() {
        var blanker = WaterfallBlanker()
        var rows = Rows()
        for _ in 0..<32 {
            _ = blanker.judge(rows.row(), thresholdDb: 3)
        }
        var pulse = rows.row(10)
        var emptied = 0
        for index in 0..<Self.bins where index % 8 < 5 {
            pulse[index] = -.infinity
            emptied += 1
        }
        #expect(emptied == 500)
        #expect(blanker.judge(pulse, thresholdDb: 3) == .blanked)
        #expect(blanker.judge(rows.row(), thresholdDb: 3) == .accepted)
    }

    // The threshold is the edge in dB.
    @Test func thresholdSixPassesFiveAndBlanksSeven() {
        var blanker = WaterfallBlanker()
        var rows = Rows()
        for _ in 0..<32 {
            _ = blanker.judge(rows.row(), thresholdDb: 6)
        }
        #expect(blanker.judge(rows.row(5), thresholdDb: 6) == .accepted)
        for _ in 0..<32 {
            _ = blanker.judge(rows.row(), thresholdDb: 6)
        }
        #expect(blanker.judge(rows.row(7), thresholdDb: 6) == .blanked)
    }

    // What the waterfall draws: a blanked row is the last accepted row, the
    // row over the hold is drawn as is and becomes the last accepted row,
    // nothing is judged while transmitting, and when transmit ends the
    // blanker starts over as the desktop's does: a zero run and no
    // detection for 8 rows.
    @Test func filteredSubstitutesTheLastAcceptedRowAndStartsOverWhenTransmitEnds() {
        var blanker = WaterfallBlanker()
        var rows = Rows()
        var last: [Float] = []
        for _ in 0..<32 {
            last = rows.row()
            #expect(blanker.filtered(last, enabled: true, transmitting: false, thresholdDb: 3) == last)
        }
        let first = rows.row(10)
        let second = rows.row(10)
        let third = rows.row(10)
        #expect(blanker.filtered(first, enabled: true, transmitting: false, thresholdDb: 3) == last)
        #expect(blanker.filtered(second, enabled: true, transmitting: false, thresholdDb: 3) == last)
        #expect(blanker.filtered(third, enabled: true, transmitting: false, thresholdDb: 3) == third)
        // A burst while transmitting is drawn and changes nothing.
        let history = blanker.historyRows
        let keyed = rows.row(30)
        #expect(blanker.filtered(keyed, enabled: true, transmitting: true, thresholdDb: 3) == keyed)
        #expect(blanker.historyRows == history)
        #expect(blanker.consecutiveBurstRows == 2)
        // Transmit ends: the first 8 receive rows are drawn as they are,
        // however loud, and build a new baseline from a zero run.
        let firstReceive = rows.row(10)
        #expect(blanker.filtered(firstReceive, enabled: true, transmitting: false, thresholdDb: 3) == firstReceive)
        #expect(blanker.consecutiveBurstRows == 0)
        #expect(blanker.historyRows == 1)
        for index in 1..<8 {
            let row = rows.row(index % 2 == 0 ? 10 : 0)
            #expect(blanker.filtered(row, enabled: true, transmitting: false, thresholdDb: 3) == row, "row \(index)")
        }
        #expect(blanker.historyRows == 8)
        #expect(blanker.consecutiveBurstRows == 0)
    }

    // BandState learns that transmit ended when transmittingWaterfall
    // falls, and the blanker starts over there, even when no row reached
    // it while keyed (Stop on TX holds the waterfall).
    @Test func bandStateResetsTheBlankerWhenTransmitEnds() throws {
        let levels = WaterfallHistory.Levels(lowDbm: -130, highDbm: -60)
        let quiet = [Float](repeating: -100, count: 16)
        let impulse = [Float](repeating: -80, count: 16)
        func band() -> BandState {
            var state = BandState(waterfallLines: 64)
            state.waterfallBlanking = true
            state.waterfallBlankingThresholdDb = 6
            return state
        }
        var sequence: UInt32 = 0
        func send(_ row: [Float], to state: inout BandState) -> [UInt8] {
            sequence += 1
            state.receive(frame: BandFixtures.frame(trace: row, sequence: sequence), manualLevels: levels)
            return Array(state.history.line(age: 0) ?? [])
        }

        // Without a transmission, the impulse after 8 quiet rows is blanked.
        var steady = band()
        var quietLine: [UInt8] = []
        for _ in 0..<8 {
            quietLine = send(quiet, to: &steady)
        }
        let impulseBlanked = send(impulse, to: &steady)
        #expect(impulseBlanked == quietLine)

        // Keyed and back, with no row offered while keyed: the impulse is
        // the first receive row, drawn as it is.
        var keyed = band()
        for _ in 0..<8 {
            _ = send(quiet, to: &keyed)
        }
        keyed.transmittingWaterfall = true
        keyed.transmittingWaterfall = false
        let impulseDrawn = send(impulse, to: &keyed)
        #expect(impulseDrawn != quietLine)
        // Re-asserting receive is no transmit end and resets nothing.
        for _ in 0..<8 {
            _ = send(quiet, to: &keyed)
        }
        keyed.transmittingWaterfall = false
        #expect(send(impulse, to: &keyed) == quietLine)
    }

    // The selection agrees with a sort, odd and even counts, with ties.
    @Test func medianMatchesASortedMedian() {
        var rows = Rows()
        for count in [1, 2, 3, 4, 7, 8, 31, 32, 799, 800] {
            var values = Array(rows.row().prefix(count))
            if count > 4 {
                values[1] = values[0]
                values[3] = values[0]
            }
            let sorted = values.sorted()
            let expected = count % 2 == 1
                ? sorted[count / 2]
                : 0.5 * (sorted[count / 2 - 1] + sorted[count / 2])
            let median = values.withUnsafeMutableBufferPointer { WaterfallBlanker.medianInPlace($0) }
            #expect(median == expected, "count \(count)")
        }
    }
}
