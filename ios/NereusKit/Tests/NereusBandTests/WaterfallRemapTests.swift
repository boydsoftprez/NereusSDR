// NereusSDR for iOS: tests that the waterfall re-maps a new line width exactly as its plain loop did
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusBand

/// R-IOS-11: turning the phone re-maps the held lines through pointers, and
/// every byte comes out as the plain loop over array subscripts made it.
@Suite struct WaterfallRemapTests {
    /// Levels that store a dBm value of 0 to 255 as that palette position.
    static let levels = WaterfallHistory.Levels(lowDbm: 0, highDbm: 255)
    static let capacity = 37

    /// The re-map as it was written before it read through pointers, kept
    /// as the reference: each new sample takes the strongest of the old
    /// samples it covers, and only the held rows are written.
    static func reference(storage: [UInt8], heldRows: [Int], capacity: Int,
                          oldColumns: Int, newColumns: Int) -> [UInt8] {
        var remapped = [UInt8](repeating: 0, count: capacity * newColumns)
        for row in heldRows {
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
        return remapped
    }

    /// A small fixed-seed generator, so every run sees the same lines.
    struct Lines {
        var state: UInt64

        mutating func next() -> UInt64 {
            state &+= 0x9E37_79B9_7F4A_7C15
            var value = state
            value = (value ^ (value >> 30)) &* 0xBF58_476D_1CE4_E5B9
            value = (value ^ (value >> 27)) &* 0x94D0_49BB_1331_11EB
            return value ^ (value >> 31)
        }

        /// A line of `width` dBm values from 0 to 255.
        mutating func line(width: Int) -> [Float] {
            (0..<width).map { _ in Float(next() % 256) }
        }
    }

    /// A history of `capacity` rows holding `appended` lines of `width`,
    /// each covering its own frequencies.
    static func history(width: Int, appended: Int, seed: UInt64) -> WaterfallHistory {
        var history = WaterfallHistory(capacity: capacity)
        var lines = Lines(state: seed)
        for index in 0..<appended {
            let coverage = BandCoverage(centerHz: 7_000_000 + Double(index) * 1_000, spanHz: 48_000)
            history.append(line: lines.line(width: width), levels: levels, coverage: coverage)
        }
        return history
    }

    @Test(arguments: zip([1320, 2868, 1000, 999, 2868, 97, 1, 5, 4, 8],
                         [2868, 1320, 999, 1000, 97, 2868, 5, 1, 8, 4]))
    func aNewWidthReMapsEveryByteAsThePlainLoopDid(oldColumns: Int, newColumns: Int) {
        // Partly full: 23 of 37 rows, the next line due in row 23. Full:
        // 37 + 11 lines, so the newest sits in row 10 and the history has
        // wrapped round.
        for appended in [23, Self.capacity + 11] {
            var history = Self.history(width: oldColumns, appended: appended,
                                       seed: UInt64(oldColumns * 10_007 + newColumns * 31 + appended))
            #expect(history.columns == oldColumns)
            #expect(history.count == min(appended, Self.capacity))
            #expect(history.nextStorageRow == appended % Self.capacity)

            var storage: [UInt8] = []
            for row in 0..<Self.capacity {
                storage += history.storageLine(row)
            }
            let heldRows = (0..<history.count).map { history.storageRow(age: $0) }
            let coverages = (0..<Self.capacity).map { history.coverage(storageRow: $0) }
            let expected = Self.reference(storage: storage, heldRows: heldRows, capacity: Self.capacity,
                                          oldColumns: oldColumns, newColumns: newColumns)
            let countBefore = history.count
            let layout = history.layoutGeneration
            let appendedBefore = history.linesAppended

            // A line of the new width re-maps the held ones, then takes the
            // next row (in a full history, the oldest line's).
            let newRow = history.nextStorageRow
            var lines = Lines(state: UInt64(appended))
            let newLine = lines.line(width: newColumns)
            history.append(line: newLine, levels: Self.levels)

            #expect(history.columns == newColumns)
            #expect(history.count == min(countBefore + 1, Self.capacity))
            #expect(history.layoutGeneration == layout &+ 1)
            #expect(history.linesAppended == appendedBefore + 1)
            #expect(history.storageRow(age: 0) == newRow)
            #expect(Array(history.storageLine(newRow))
                == newLine.map { WaterfallHistory.paletteIndex(dbm: $0, levels: Self.levels) })
            #expect(history.coverage(storageRow: newRow) == nil)
            for row in 0..<Self.capacity where row != newRow {
                let want = expected[(row * newColumns)..<((row + 1) * newColumns)]
                #expect(history.storageLine(row) == want,
                        "row \(row), \(oldColumns) to \(newColumns), \(appended) lines appended")
                #expect(history.coverage(storageRow: row) == coverages[row])
            }
        }
    }
}
