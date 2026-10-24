// NereusSDR for iOS: suppresses isolated high-energy waterfall rows on the phone
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-23: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code (Claude Opus 5.5):
//   the detector follows NereusSDR's desktop WaterfallImpulseBlanker (PR 625):
//   a row's median in dB against the median of accepted rows, at most two rows
//   blanked in a row, and only accepted rows in the baseline. The blanker is
//   reset when transmit ends, as the desktop's is.

import Foundation

/// NB Blank: replaces a wideband impulse row (an electric fence charger's
/// click draws one thin line across every bin) with the last accepted row.
///
/// A row's level is the median of its finite bins, in dB. The baseline is
/// the median of the last ``historyLimit`` accepted rows' levels, and a row
/// is a burst when its level is more than the operator's threshold (dB)
/// over it. An impulse lifts every bin, so the median jumps with it; a loud
/// station on a minority of the bins barely moves it, where a mean of
/// linear power would rise by tens of dB and hold the waterfall on every
/// syllable.
///
/// At most ``maxConsecutiveRows`` rows in a row are blanked. Further burst
/// rows of the same run are accepted as real (a band opening, a preamp or
/// attenuator change) and enter the baseline until a row that is no burst
/// arrives, so the baseline follows a lasting rise within a few rows.
/// Only accepted rows enter the baseline. The history pauses during
/// transmit, and when transmit ends the baseline, the burst run and the last
/// accepted row are forgotten, as the desktop does: the first receive rows
/// are drawn as they are and build a new baseline.
public struct WaterfallBlanker: Equatable, Sendable {
    /// Accepted rows' levels the baseline is taken over.
    public static let historyLimit = 32
    /// Rows of history needed before a row can be called a burst.
    public static let rowsBeforeDetection = 8
    /// The most rows in a row that are blanked. A fence click is one row;
    /// a third burst row in a row is a real change of level.
    public static let maxConsecutiveRows = 2

    public enum Verdict: Equatable, Sendable {
        /// Not a burst: drawn, enters the baseline, the new last accepted row.
        case accepted
        /// A burst: replaced with the last accepted row; not in the baseline.
        case blanked
        /// A burst after ``maxConsecutiveRows`` blanked rows: drawn as is,
        /// enters the baseline and is the new last accepted row.
        case acceptedOverHold
        /// No finite bin: drawn as is, never a burst, never in the baseline,
        /// not a last accepted row. A burst run it falls in goes on counting.
        case noFiniteBins
    }

    /// Accepted rows' levels, dB, a ring of ``historyLimit``.
    private var ring = [Float](repeating: 0, count: WaterfallBlanker.historyLimit)
    private var ringIndex = 0
    private var ringCount = 0
    private var burstRun = 0
    /// A row was offered while transmitting and receive has not resumed.
    private var keyed = false
    /// The baseline of the latest judged row, dB; the fill when a burst
    /// has no last accepted row of its width.
    private var lastBaselineDb: Float = 0
    private var lastAcceptedRow: [Float] = []
    /// Scratch for the selections: the row's finite bins, and the ring.
    private var rowScratch: [Float] = []
    private var ringScratch: [Float] = []

    public init() {}

    /// Rows in the baseline.
    public var historyRows: Int { ringCount }
    /// Burst rows blanked in the current run.
    public var consecutiveBurstRows: Int { burstRun }

    public static func == (lhs: WaterfallBlanker, rhs: WaterfallBlanker) -> Bool {
        lhs.ring[0..<lhs.ringCount] == rhs.ring[0..<rhs.ringCount] && lhs.ringIndex == rhs.ringIndex && lhs.ringCount == rhs.ringCount
            && lhs.burstRun == rhs.burstRun && lhs.keyed == rhs.keyed && lhs.lastBaselineDb == rhs.lastBaselineDb
            && lhs.lastAcceptedRow == rhs.lastAcceptedRow
    }

    /// Forgets the baseline, the burst run and the last accepted row.
    public mutating func reset() {
        ringIndex = 0
        ringCount = 0
        burstRun = 0
        keyed = false
        lastBaselineDb = 0
        lastAcceptedRow.removeAll(keepingCapacity: true)
    }

    public mutating func filtered(_ row: [Float], enabled: Bool, transmitting: Bool,
                                  thresholdDb: Int) -> [Float] {
        if transmitting {
            keyed = true
            return row
        }
        if keyed {
            // Transmit has ended: receive starts a new baseline.
            reset()
        }
        guard enabled else {
            return row
        }
        switch judge(row, thresholdDb: Float(thresholdDb)) {
        case .blanked:
            return lastAcceptedRow.count == row.count
                ? lastAcceptedRow
                : [Float](repeating: lastBaselineDb, count: row.count)
        case .accepted, .acceptedOverHold:
            lastAcceptedRow = row
            return row
        case .noFiniteBins:
            return row
        }
    }

    /// Judges one row in dB against `thresholdDb` dB over the baseline and
    /// updates the baseline and the burst run. The last accepted row is
    /// kept by ``filtered(_:enabled:transmitting:thresholdDb:)``.
    public mutating func judge(_ row: [Float], thresholdDb: Float) -> Verdict {
        rowScratch.removeAll(keepingCapacity: true)
        for value in row where value.isFinite {
            rowScratch.append(value)
        }
        guard !rowScratch.isEmpty else {
            return .noFiniteBins
        }
        let rowDb = rowScratch.withUnsafeMutableBufferPointer { Self.medianInPlace($0) }

        var burst = false
        if ringCount >= Self.rowsBeforeDetection {
            ringScratch.removeAll(keepingCapacity: true)
            ringScratch.append(contentsOf: ring[0..<ringCount])
            let baselineDb = ringScratch.withUnsafeMutableBufferPointer { Self.medianInPlace($0) }
            lastBaselineDb = baselineDb
            burst = rowDb - baselineDb > thresholdDb
        }

        guard burst else {
            burstRun = 0
            accept(rowDb)
            return .accepted
        }
        if burstRun < Self.maxConsecutiveRows {
            burstRun += 1
            return .blanked
        }
        // The run stays at the limit until a row that is no burst, so every
        // further row of a lasting rise is accepted while the baseline follows.
        accept(rowDb)
        return .acceptedOverHold
    }

    private mutating func accept(_ rowDb: Float) {
        ring[ringIndex] = rowDb
        ringIndex = (ringIndex + 1) % Self.historyLimit
        if ringCount < Self.historyLimit {
            ringCount += 1
        }
    }

    /// The median of a non-empty buffer of finite values, reordering it. An
    /// even count gives the mean of the two middle values. O(n): a
    /// selection, then the largest value of the lower half.
    static func medianInPlace(_ values: UnsafeMutableBufferPointer<Float>) -> Float {
        let count = values.count
        let middle = count / 2
        select(values, middle)
        let upper = values[middle]
        if count % 2 == 1 {
            return upper
        }
        var lower = values[0]
        for index in 1..<middle where values[index] > lower {
            lower = values[index]
        }
        return 0.5 * (lower + upper)
    }

    /// Puts the `k`th smallest value at index `k`, with nothing larger
    /// before it and nothing smaller after it (quickselect, median-of-three
    /// pivot, Hoare partition).
    private static func select(_ values: UnsafeMutableBufferPointer<Float>, _ k: Int) {
        var low = 0
        var high = values.count - 1
        while low < high {
            let mid = low + (high - low) / 2
            if values[mid] < values[low] { values.swapAt(mid, low) }
            if values[high] < values[low] { values.swapAt(high, low) }
            if values[high] < values[mid] { values.swapAt(high, mid) }
            let pivot = values[mid]
            var i = low
            var j = high
            while i <= j {
                while values[i] < pivot { i += 1 }
                while pivot < values[j] { j -= 1 }
                if i <= j {
                    values.swapAt(i, j)
                    i += 1
                    j -= 1
                }
            }
            if k <= j {
                high = j
            } else if k >= i {
                low = i
            } else {
                return
            }
        }
    }
}
