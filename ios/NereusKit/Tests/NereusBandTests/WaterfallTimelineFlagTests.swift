// NereusSDR for iOS: the waterfall's timeline check, against a walk over every row
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-11-15: J.J. Boyd (KG4VCF), AI-assisted via Claude Code (Opus 5.5):
//   the running broken-pair count must give the same answer as walking the rows.
import Foundation
import Testing
@testable import NereusBand

@Suite struct WaterfallTimelineFlagTests {
    private let levels = WaterfallHistory.Levels(lowDbm: -100, highDbm: 0)

    /// The check as it was before the running count: every held row has a
    /// known time, each strictly older than the row above it.
    private func walked(_ history: WaterfallHistory) -> Bool {
        guard history.count > 0, var newer = history.presentationTime(age: 0) else { return false }
        for age in 1..<history.count {
            guard let older = history.presentationTime(age: age), older < newer else { return false }
            newer = older
        }
        return true
    }

    private func append(_ time: Double?, to history: inout WaterfallHistory, width: Int = 4) {
        // A nil time with no arrival time leaves the row's time unknown.
        history.append(line: [Float](repeating: -50, count: width), levels: levels,
                       time: 0, presentationTime: time)
    }

    private func check(_ history: WaterfallHistory, _ expected: Bool,
                       sourceLocation: SourceLocation = #_sourceLocation) {
        #expect(history.hasPresentationTimeline == walked(history), sourceLocation: sourceLocation)
        #expect(history.hasPresentationTimeline == expected, sourceLocation: sourceLocation)
    }

    @Test func emptyHistoryHasNoTimeline() {
        check(WaterfallHistory(capacity: 8), false)
    }

    @Test func inOrderAppendsKeepTheTimeline() {
        var history = WaterfallHistory(capacity: 8)
        for time in [10.0, 11, 12, 13, 14] {
            append(time, to: &history)
            check(history, true)
        }
    }

    @Test func outOfOrderAppendBreaksItUntilTheRingDropsIt() {
        var history = WaterfallHistory(capacity: 4)
        append(10, to: &history)
        append(9, to: &history)  // older than the row below it
        check(history, false)
        append(11, to: &history)
        append(12, to: &history)
        check(history, false)
        // The ring is full; the next line drops the 10, the pair (10, 9) goes.
        append(13, to: &history)
        check(history, true)
    }

    @Test func equalTimesBreakTheTimeline() {
        var history = WaterfallHistory(capacity: 4)
        append(10, to: &history)
        append(10, to: &history)
        check(history, false)
    }

    @Test func unknownTimeBreaksItUntilTheRingDropsIt() {
        var history = WaterfallHistory(capacity: 3)
        append(nil, to: &history)
        check(history, false)
        append(1, to: &history)
        check(history, false)
        append(2, to: &history)
        check(history, false)
        append(3, to: &history)  // drops the unknown row
        check(history, true)
        append(nil, to: &history)  // newest unknown
        check(history, false)
        append(5, to: &history)
        append(6, to: &history)
        check(history, false)
        append(7, to: &history)
        check(history, true)
    }

    @Test func singleLineRingFollowsItsOneRow() {
        var history = WaterfallHistory(capacity: 1)
        append(5, to: &history)
        check(history, true)
        append(4, to: &history)
        check(history, true)
        append(nil, to: &history)
        check(history, false)
        append(3, to: &history)
        check(history, true)
    }

    @Test func resizeKeepsTheAnswer() {
        var history = WaterfallHistory(capacity: 6)
        for time in [1.0, 0.5, 2, 3, 4, 5] { append(time, to: &history) }
        check(history, false)
        history.resize(capacity: 8)
        check(history, false)
        // Keeping only the newest four drops the broken pair.
        history.resize(capacity: 4)
        check(history, true)
        history.resize(capacity: 10)
        check(history, true)
        append(4.5, to: &history)
        check(history, false)
    }

    @Test func clearStartsOver() {
        var history = WaterfallHistory(capacity: 4)
        append(2, to: &history)
        append(1, to: &history)
        check(history, false)
        history.clear()
        check(history, false)
        append(3, to: &history)
        check(history, true)
        append(4, to: &history)
        check(history, true)
    }

    @Test func widthChangeKeepsTheAnswer() {
        var history = WaterfallHistory(capacity: 4)
        append(1, to: &history)
        append(0, to: &history)
        check(history, false)
        append(2, to: &history, width: 6)
        check(history, false)
        append(3, to: &history, width: 6)
        append(4, to: &history, width: 6)
        check(history, true)
    }

    /// A long seeded run of appends, resizes and clears, compared against the
    /// walk after every step.
    @Test(arguments: [1, 2, 3, 7, 32])
    func seededRunMatchesTheWalk(capacity: Int) {
        var seed: UInt64 = 0x9E37_79B9_7F4A_7C15 &+ UInt64(capacity)
        func next() -> UInt64 {
            seed = seed &* 6_364_136_223_846_793_005 &+ 1_442_695_040_888_963_407
            return seed >> 33
        }
        var history = WaterfallHistory(capacity: capacity)
        var time = 1000.0
        for _ in 0..<2000 {
            switch next() % 100 {
            case 0:
                history.clear()
            case 1...2:
                history.resize(capacity: Int(next() % 40) + 1)
            case 3...6:
                append(nil, to: &history)
            case 7...12:
                append(time - Double(next() % 3), to: &history)  // out of order or equal
            default:
                time += 0.01
                append(time, to: &history)
            }
            #expect(history.hasPresentationTimeline == walked(history))
        }
    }
}
