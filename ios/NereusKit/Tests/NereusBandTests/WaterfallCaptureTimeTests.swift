// NereusSDR for iOS: capture-time geometry must not turn delivery jitter into missing RF rows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-28: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import NereusMedia
import Testing
@testable import NereusBand

@Suite struct WaterfallCaptureTimeTests {
    private let levels = WaterfallHistory.Levels(lowDbm: -130, highDbm: -30)
    private let coverage = BandCoverage(centerHz: 7_100_000, spanHz: 48_000)

    private func frame(_ stamp: UInt64, sequence: UInt32 = 1, generation: UInt32 = 1,
                       advance: Bool = true, traceCarried: Bool = false) -> DisplayFrame {
        DisplayFrame(endpointId: 1, contextGeneration: generation, encoderSequence: sequence,
                     producerTimestamp: stamp, isKeyframe: false, waterfallAdvance: advance,
                     minDbm: -160, maxDbm: 0, traceDbm: [-80], waterfallDbm: [-90], wideDbm: [],
                     traceCarried: traceCarried)
    }

    private func state() -> BandState {
        var state = BandState(waterfallLines: 32)
        state.note(context: 1, coverage: coverage)
        state.linePeriodSeconds = 1 / 30
        return state
    }

    // Reverting capture geometry to commit time must fail this test even
    // when all frames are delivered, including an extras wait and a batch.
    @Test(arguments: [false, true])
    func continuousCapturesSurviveCommitStallsAndBatches(paced: Bool) throws {
        for extras in [false, true] {
            var state = state()
            state.pacesLines = paced
            state.expectsExtras = extras
            state.tracesLines = true
            state.clock = { 1_700_000_000 }
            let captures: [UInt64] = [8_000_000_000_000_000_000, 8_000_000_000_033_333_333,
                                      8_000_000_000_066_666_666]
            let commits = [1000.0, 1000.15, 1000.15]
            for index in captures.indices {
                let frame = frame(captures[index], sequence: UInt32(index + 1))
                state.receive(frame: frame, manualLevels: levels, at: commits[index] - (extras ? 0.01 : 0))
                if extras {
                    state.receive(extras: BandFixtures.extras(for: frame), manualLevels: levels, at: commits[index])
                }
            }
            state.flushLines(at: 1001)
            let newest = try #require(state.history.presentationTime(age: 0))
            #expect(abs(newest - 1000.066666666) < 0.0000001)
            #expect(state.history.hasPresentationTimeline)
            #expect(state.history.count == 3 && state.history.linesAppended == 3)
            #expect(state.history.coverage(storageRow: state.history.storageRow(age: 0)) == coverage)
            #expect(state.history.time(age: 0) == 1_700_000_000)
            #expect(state.takePutLines().map(\.arrivedAt) == commits)
            let axis = WaterfallTimeAxis(topTime: newest, periodMs: 10)
            for scale in [1.0, 2.0, 3.0] {
                var rows: [UInt32] = []
                axis.fillAges(history: state.history, heightPixels: Int(6 * scale), scale: scale, into: &rows)
                #expect(rows.allSatisfy { $0 < 3 })
            }
        }
    }

    @Test func timestampSpacingAloneDoesNotDeclareAnInterruption() throws {
        var state = state()
        state.receive(frame: frame(9_000_000_000), manualLevels: levels, at: 1000)
        state.receive(frame: frame(9_150_000_000, sequence: 2), manualLevels: levels, at: 1000.01)
        let axis = WaterfallTimeAxis(topTime: try #require(state.history.presentationTime(age: 0)), periodMs: 10)
        #expect(abs(axis.topTime - 1000.15) < 0.0000001)
        #expect(axis.age(at: 1000.075, history: state.history) != nil)
        #expect(axis.age(at: 1000.15, history: state.history) == 0)
        #expect(axis.age(at: 1000, history: state.history) == 1)
    }

    @Test(arguments: [false, true])
    func explicitInterruptionStartsAtTheNextCommittedWaterfallRow(paced: Bool) throws {
        var state = state()
        state.pacesLines = paced
        state.expectsExtras = true
        state.receive(frame: frame(1_000_000_000), manualLevels: levels, at: 1000)
        state.commit(manualLevels: levels, at: 1000)
        state.flushLines(at: 1000)
        state.noteInterruption()
        state.receive(frame: frame(1_010_000_000, sequence: 2, advance: false), manualLevels: levels, at: 1000.01)
        state.commit(manualLevels: levels, at: 1000.01)
        #expect(state.history.count == 1)
        let resumed = frame(1_150_000_000, sequence: 3)
        state.receive(frame: resumed, manualLevels: levels, at: 1000.02)
        state.receive(extras: BandFixtures.extras(for: resumed), manualLevels: levels, at: 1000.03)
        state.flushLines(at: 1000.04)
        #expect(state.history.presentationBreakBefore(age: 0))
        state.resizeWaterfall(lines: 8)
        let axis = WaterfallTimeAxis(topTime: try #require(state.history.presentationTime(age: 0)), periodMs: 10)
        #expect(axis.age(at: 1000.075, history: state.history) == nil)
        #expect(WaterfallTimeAxis.liveTop(history: state.history, progress: 0) == axis.topTime)
        state.receive(frame: frame(1_183_333_333, sequence: 4), manualLevels: levels, at: 1000.05)
        state.commit(manualLevels: levels, at: 1000.05)
        state.flushLines(at: 1000.06)
        #expect(!state.history.presentationBreakBefore(age: 0))
        #expect(state.history.presentationBreakBefore(age: 1))
        state.clear()
        #expect(state.history.count == 0)
    }

    @Test func quantizedFFTSlotsDoNotBecomeThinBlackBands() throws {
        var state = state()
        // 4096/768k source hops sampled at 30 fps: six or seven hops,
        // plus the sample clock's permitted submillisecond anchor creep.
        let stamps: [UInt64] = [1_000_000_000, 1_032_050_000, 1_069_433_333, 1_101_483_333]
        for (index, stamp) in stamps.enumerated() {
            state.receive(frame: frame(stamp, sequence: UInt32(index)), manualLevels: levels, at: 1000 + Double(index))
        }
        // Ring resizing retains the pair metadata.
        state.resizeWaterfall(lines: 8)
        let axis = WaterfallTimeAxis(topTime: try #require(state.history.presentationTime(age: 0)), periodMs: 10)
        for scale in [1.0, 2.0, 3.0] {
            var rows: [UInt32] = []
            axis.fillAges(history: state.history, heightPixels: Int(10 * scale), scale: scale, into: &rows)
            #expect(rows.allSatisfy { $0 < 4 })
        }
    }

    @Test func missingEncoderSequenceDoesNotDeclareAnInterruption() {
        var state = state()
        state.receive(frame: frame(1_000_000_000, sequence: 10), manualLevels: levels, at: 1000)
        // A released partial frame without its waterfall must not establish
        // a fresh row or prove continuity to the next advancing frame.
        state.receive(frame: frame(1_010_000_000, sequence: 11, advance: false), manualLevels: levels, at: 1000.01)
        state.receive(frame: frame(1_037_333_333, sequence: 12), manualLevels: levels, at: 1000.02)
        let axis = WaterfallTimeAxis(topTime: 1000.037333333, periodMs: 10)
        #expect(state.history.count == 2)
        #expect(axis.age(at: 1000.0186666665, history: state.history) != nil)
    }

    @Test func repeatedPanContextsAndSequenceResetsDoNotPaintThinBands() throws {
        var state = state()
        let stamps: [UInt64] = [1_000_000_000, 1_037_333_333, 1_069_333_333, 1_106_666_666, 1_144_000_000]
        for (index, stamp) in stamps.enumerated() {
            let generation = UInt32(index + 1)
            state.note(context: generation, coverage: BandCoverage(centerHz: 7_100_000 + Double(index * 100), spanHz: 48_000))
            state.receive(frame: frame(stamp, sequence: 0, generation: generation), manualLevels: levels, at: 1000 + Double(index) / 30)
        }
        state.resizeWaterfall(lines: 8)
        let axis = WaterfallTimeAxis(topTime: try #require(state.history.presentationTime(age: 0)), periodMs: 10)
        for scale in [1.0, 2.0, 3.0] {
            var rows: [UInt32] = []
            axis.fillAges(history: state.history, heightPixels: Int(14 * scale), scale: scale, into: &rows)
            #expect(rows.allSatisfy { $0 < 5 })

        }
    }

    @Test func frameDividerAndSequenceWrapRespectWaterfallParts() {
        var state = state()
        state.linePeriodSeconds = 0.1
        state.receive(frame: frame(1_000_000_000, sequence: UInt32.max - 1), manualLevels: levels, at: 1000)
        state.receive(frame: frame(1_033_000_000, sequence: UInt32.max, advance: false), manualLevels: levels, at: 1000.01)
        state.receive(frame: frame(1_066_000_000, sequence: 0, advance: false), manualLevels: levels, at: 1000.02)
        // Missing trace alone does not invalidate a carried waterfall plane.
        state.receive(frame: frame(1_105_000_000, sequence: 1, traceCarried: true), manualLevels: levels, at: 1000.15)
        let axis = WaterfallTimeAxis(topTime: 1000.105, periodMs: 10)
        #expect(state.history.count == 2)
        #expect(axis.age(at: 1000.0525, history: state.history) != nil)
    }

    @Test func retuneAndCadenceChangesKeepTheCaptureAnchor() throws {
        var state = state()
        state.receive(frame: frame(9_000_000_000), manualLevels: levels, at: 1000)
        state.note(context: 2, coverage: coverage)
        state.linePeriodSeconds = 0.1
        state.receive(frame: frame(9_100_000_000, sequence: 2, generation: 2), manualLevels: levels, at: 1001)
        let axis = WaterfallTimeAxis(topTime: try #require(state.history.presentationTime(age: 0)), periodMs: 10)
        #expect(abs(axis.topTime - 1000.1) < 0.0000001)
        #expect(state.history.presentationTime(age: 1) == 1000)
        for scale in [1.0, 2.0, 3.0] {
            var rows: [UInt32] = []
            axis.fillAges(history: state.history, heightPixels: Int(10 * scale), scale: scale, into: &rows)
            #expect(rows.allSatisfy { $0 < 2 })
        }
        // A fixed rewind coordinate continues to name the original row.
        #expect(axis.age(at: 1000, history: state.history) == 1)
    }

    @Test func aSenderResetAndClearedHistoryStartNewAnchors() throws {
        var state = state()
        let epoch = UInt64.max - 1_000_000_000
        state.receive(frame: frame(epoch), manualLevels: levels, at: 1000)
        state.receive(frame: frame(epoch + 33_333_333, sequence: 2), manualLevels: levels, at: 1000.15)
        #expect(abs(try #require(state.history.presentationTime(age: 0)) - 1000.033333333) < 0.0000001)
        state.discardPending() // The existing endpoint/media-lifetime boundary.
        state.receive(frame: frame(1_000_000), manualLevels: levels, at: 1002)
        #expect(state.history.presentationTime(age: 0) == 1002)
        #expect(state.history.presentationTime(age: 2) == 1000)
        state.clear()
        state.receive(frame: frame(50_000_000), manualLevels: levels, at: 50)
        #expect(state.history.count == 1)
        #expect(state.history.presentationTime(age: 0) == 50)
    }

    @Test(arguments: [UInt64(0), 8_000_000_000, 7_000_000_000])
    func missingDuplicateOrBackwardCaptureMetadataUsesRowProjection(stamp: UInt64) {
        var state = state()
        state.receive(frame: frame(8_000_000_000), manualLevels: levels, at: 1000)
        state.receive(frame: frame(stamp, sequence: 2), manualLevels: levels, at: 1000.15)
        #expect(state.history.presentationTime(age: 0) == nil)
        #expect(!state.history.hasPresentationTimeline)
        var rows: [UInt32] = []
        WaterfallTimeAxis(topTime: 1000.15, periodMs: 10)
            .fillAges(history: state.history, heightPixels: 3, scale: 3, into: &rows)
        #expect(rows == [0, 1, WaterfallTimeAxis.blank])
    }
}
