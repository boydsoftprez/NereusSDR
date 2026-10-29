// NereusSDR for iOS: golden values for the 3D view's row smoothing, bit for bit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-26 J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import NereusMedia
import Testing
@testable import NereusBand

/// The smoothed rows of two seeded runs, hashed bit for bit. The hashes
/// were taken from the smoother as it stood at 531d3b65b, before it was
/// written again in this app's own form (2026-10-26), so the new form is
/// shown to give every level exactly as the old one did. The runs go
/// through every path: levels missing and infinite, columns of no source,
/// the line and the wide row side by side, a pan, a new capture, a key, a
/// clear and a row of another width.
@Suite struct StackedRowSmoothingGoldenTests {
    /// Seeded SplitMix64, the same on every run.
    struct Seeded {
        var state: UInt64

        mutating func next() -> UInt64 {
            state &+= 0x9E37_79B9_7F4A_7C15
            var z = state
            z = (z ^ (z >> 30)) &* 0xBF58_476D_1CE4_E5B9
            z = (z ^ (z >> 27)) &* 0x94D0_49BB_1331_11EB
            return z ^ (z >> 31)
        }

        /// A level about -120 dBm, now and then a carrier, a missing level
        /// or an infinite one.
        mutating func level() -> Float {
            let roll = next() % 100
            if roll == 0 {
                return .nan
            }
            if roll == 1 {
                return -.infinity
            }
            let noise = Float(next() % 2_000) / 100 - 130
            return roll < 6 ? noise + 70 : noise
        }
    }

    /// FNV-1a over 32-bit words.
    struct Hash {
        var value: UInt64 = 0xCBF2_9CE4_8422_2325

        mutating func add(_ word: UInt32) {
            for shift in stride(from: 0, to: 32, by: 8) {
                value ^= UInt64((word >> UInt32(shift)) & 0xFF)
                value &*= 0x0000_0100_0000_01B3
            }
        }

        mutating func add<S: Sequence>(_ levels: S) where S.Element == Float {
            for level in levels {
                add(level.bitPattern)
            }
        }
    }

    static let goldenSmoother: UInt64 = 8_193_790_362_527_061_017
    static let goldenStack: UInt64 = 1_311_529_272_060_540_682

    @Test func theSmootherGivesTheGoldenLevels() {
        var seeded = Seeded(state: 0x5EED_0001)
        var smoother = StackedRowSmoother()
        var hash = Hash()
        func origin(window centerHz: Double, generation: UInt32) -> StackedRowSmoother.Origin {
            .init(endpointId: 1, contextGeneration: generation,
                  window: BandCoverage(centerHz: centerHz, spanHz: 100_000), wide: nil)
        }
        var current = origin(window: 7_200_000, generation: 1)
        var previous: [Float]?
        for index in 0..<220 {
            switch index {
            case 40: current = origin(window: 7_210_000, generation: 1)
            case 80: smoother.restart()
            case 120: current = origin(window: 7_210_000, generation: 2)
            case 150:
                smoother.clear()
                previous = nil
            default: break
            }
            let width = (170..<174).contains(index) ? 48 : 64
            let raw = (0..<width).map { _ in seeded.level() }
            let sources = (0..<width).map { column -> StackedRowSmoother.Source in
                column < 6 ? .none : column < width - 14 ? .line : .wide
            }
            let row = smoother.smooth(raw, sources: sources, origin: current, previous: previous?[...])
            hash.add(UInt32(row.count))
            hash.add(row)
            previous = row
        }
        #expect(hash.value == Self.goldenSmoother, "smoother hash \(hash.value)")
    }

    /// Through the stack as the band feeds it: 768 samples across the pan
    /// and the Core's 768 across the widest row beside it, so the rows are
    /// 2195 columns wide before and after the stack kept its width fixed.
    @Test func theStackGivesTheGoldenLevels() throws {
        var seeded = Seeded(state: 0x5EED_0002)
        var stack = StackedTraceHistory()
        var hash = Hash()
        var pan = BandCoverage(centerHz: 7_200_000, spanHz: 48_000)
        for index in 0..<150 {
            if index == 60 {
                stack.resetSmoothing()
            }
            if index == 100 {
                pan = BandCoverage(centerHz: 7_212_000, spanHz: 48_000)
            }
            let line = (0..<768).map { _ in seeded.level() }
            let wideLine = (0..<768).map { _ in seeded.level() }
            let frame = DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: UInt32(index + 1),
                                     producerTimestamp: UInt64(index + 1), isKeyframe: index == 0,
                                     waterfallAdvance: true, minDbm: -160, maxDbm: 0, traceDbm: line,
                                     waterfallDbm: line, wideDbm: wideLine)
            let wide = BandCoverage(centerHz: pan.centerHz, spanHz: pan.spanHz * StackedTrace.widestRowSpan)
            stack.append(frame: frame, coverage: pan, wide: wide, time: Double(index))
            let row = try #require(stack.row(age: 0))
            hash.add(UInt32(row.count))
            hash.add(row)
        }
        #expect(stack.columns == 2195)
        #expect(hash.value == Self.goldenStack, "stack hash \(hash.value)")
    }
}
