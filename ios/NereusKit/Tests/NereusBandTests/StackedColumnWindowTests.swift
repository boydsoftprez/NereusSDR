// NereusSDR for iOS: tests for the 3D view's columns: every column the peak of equally many samples
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-25 J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusMedia
import Testing
@testable import NereusBand

/// The 3D stack's rows reduce each waterfall line to its columns by peak.
/// Windows taken from each column's edges were one sample wider at some
/// columns than at the rest, the same columns on every row, and stood as
/// fixed ridges through the stack (first seen and measured on the
/// desktop's 3D, 2026-10-22). Equal windows give every column the same mean.
@Suite struct StackedColumnWindowTests {
    static let coverage = BandCoverage(centerHz: 7_200_000, spanHz: 48_000)

    /// Gaussian levels from a seeded SplitMix64 (Box-Muller), the same on
    /// every run.
    struct NoiseRows {
        var state: UInt64

        mutating func next() -> UInt64 {
            state &+= 0x9E37_79B9_7F4A_7C15
            var z = state
            z = (z ^ (z >> 30)) &* 0xBF58_476D_1CE4_E5B9
            z = (z ^ (z >> 27)) &* 0x94D0_49BB_1331_11EB
            return z ^ (z >> 31)
        }

        /// Open interval (0, 1), so log never sees zero.
        mutating func uniform() -> Double {
            (Double(next() >> 11) + 0.5) / 9_007_199_254_740_992
        }

        mutating func row(_ count: Int, mean: Float, sigma: Float) -> [Float] {
            (0..<count).map { _ in
                let z = (-2 * log(uniform())).squareRoot() * cos(2 * Double.pi * uniform())
                return mean + sigma * Float(z)
            }
        }
    }

    static func frame(_ row: [Float]) -> DisplayFrame {
        DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: 1, producerTimestamp: 1,
                     isKeyframe: true, waterfallAdvance: true, minDbm: -160, maxDbm: 0,
                     traceDbm: row, waterfallDbm: row, wideDbm: [])
    }

    /// The columns over the pan: index, low and high edge in hertz, as
    /// ``StackedTraceHistory/append(frame:coverage:wide:time:)`` lays them.
    static func panColumns(_ history: StackedTraceHistory) -> [(column: Int, low: Double, high: Double)] {
        guard let window = history.window(age: 0) else {
            return []
        }
        let columnHz = window.spanHz / Double(history.columns)
        return (0..<history.columns).compactMap { column in
            let low = window.lowHz + Double(column) * columnHz
            let middle = low + columnHz / 2
            guard middle >= coverage.lowHz, middle < coverage.highHz else {
                return nil
            }
            return (column, low, low + columnHz)
        }
    }

    /// The width of the window the columns took before 2026-10-25: from
    /// the floor of the low edge to the ceiling of the high one.
    static func oldWidth(samples: Int, low: Double, high: Double) -> Int {
        let binHz = coverage.spanHz / Double(samples)
        let first = min(max(Int(((low - coverage.lowHz) / binHz).rounded(.down)), 0), samples - 1)
        let last = min(max(Int(((high - coverage.lowHz) / binHz).rounded(.up)), first + 1), samples)
        return last - first
    }

    @Test(arguments: [1320, 500])
    func aFlatLineStaysFlatInEveryColumn(samples: Int) throws {
        var history = StackedTraceHistory()
        history.append(frame: Self.frame([Float](repeating: -120, count: samples)),
                       coverage: Self.coverage, wide: nil, time: 0)
        let row = try #require(history.row(age: 0))
        let columns = Self.panColumns(history)
        #expect(columns.count > 100)
        for column in columns {
            #expect(row[row.startIndex + column.column] == -120)
        }
    }

    /// 1320 samples is an iPhone 18 Pro Max's width in pixels: 159 columns
    /// over the pan, 8.25 samples each. The old windows were 9 samples wide
    /// at some columns and 10 at the rest; with 3 dB of noise the 10-sample
    /// columns stood 0.15 dB higher (0.40 dB at 500 samples).
    @Test(arguments: [1320, 500])
    func noiseGivesEveryColumnTheSameMean(samples: Int) throws {
        let rows = 2000
        var history = StackedTraceHistory()
        var noise = NoiseRows(state: 20_261_025)
        var sums: [Double] = []
        var columns: [(column: Int, low: Double, high: Double)] = []
        for _ in 0..<rows {
            history.append(frame: Self.frame(noise.row(samples, mean: -120, sigma: 3)),
                           coverage: Self.coverage, wide: nil, time: 0)
            if columns.isEmpty {
                columns = Self.panColumns(history)
                sums = Array(repeating: 0, count: columns.count)
            }
            let row = try #require(history.row(age: 0))
            for (index, column) in columns.enumerated() {
                sums[index] += Double(row[row.startIndex + column.column])
            }
        }
        let widths = columns.map { Self.oldWidth(samples: samples, low: $0.low, high: $0.high) }
        let narrowWidth = try #require(widths.min())
        var narrow: [Double] = []
        var wide: [Double] = []
        for (index, width) in widths.enumerated() {
            let mean = sums[index] / Double(rows)
            if width == narrowWidth {
                narrow.append(mean)
            } else {
                wide.append(mean)
            }
        }
        #expect(narrow.count > 10 && wide.count > 10)
        let rise = wide.reduce(0, +) / Double(wide.count) - narrow.reduce(0, +) / Double(narrow.count)
        #expect(abs(rise) < 0.05, "the columns that had wider windows sit \(rise) dB above the rest")
    }

    /// Where the columns line up with the desktop's (768 columns over the
    /// same samples), every column takes the desktop's window: as many
    /// samples as a column is wide rounded up, from the sample nearest
    /// its low edge, kept inside the line.
    @Test(arguments: [2270, 500])
    func columnsTakeTheDesktopsWindows(samples: Int) {
        let columns = 768
        let columnHz = Self.coverage.spanHz / Double(columns)
        let step = Double(samples) / Double(columns)
        let width = max(1, Int(step.rounded(.up)))
        let rising = (0..<samples).map { Float($0) }
        let falling = rising.map { -$0 }
        var wrong: [Int] = []
        for column in 0..<columns {
            let low = Self.coverage.lowHz + Double(column) * columnHz
            let first = min(max(Int((Double(column) * step).rounded()), 0), samples - width)
            let last = StackedTraceHistory.peak(rising, over: Self.coverage, from: low, to: low + columnHz)
            let start = StackedTraceHistory.peak(falling, over: Self.coverage, from: low, to: low + columnHz)
            if last != Float(first + width - 1) || start != -Float(first) {
                wrong.append(column)
            }
        }
        #expect(wrong.isEmpty, "columns \(wrong) take other samples than the desktop's")
    }

    /// No sample between the middles of the first and last pan columns is
    /// skipped: a one-sample carrier there stands in the row.
    @Test(arguments: [1320, 500])
    func everySampleBetweenTheColumnsReachesOne(samples: Int) throws {
        var history = StackedTraceHistory()
        history.append(frame: Self.frame([Float](repeating: -130, count: samples)),
                       coverage: Self.coverage, wide: nil, time: 0)
        let columns = Self.panColumns(history)
        let binHz = Self.coverage.spanHz / Double(samples)
        let first = try #require(columns.first)
        let last = try #require(columns.last)
        let from = Int((((first.low + first.high) / 2 - Self.coverage.lowHz) / binHz).rounded(.up))
        let to = Int((((last.low + last.high) / 2 - Self.coverage.lowHz) / binHz).rounded(.down))
        #expect(to - from > samples * 9 / 10)
        var lost: [Int] = []
        for carrier in from..<to {
            var line = [Float](repeating: -130, count: samples)
            line[carrier] = -40
            history.append(frame: Self.frame(line), coverage: Self.coverage, wide: nil, time: 0)
            let row = try #require(history.row(age: 0))
            if !columns.contains(where: { row[row.startIndex + $0.column] > -100 }) {
                lost.append(carrier)
            }
        }
        #expect(lost.isEmpty, "carriers lost at samples \(lost)")
    }
}
