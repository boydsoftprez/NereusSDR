// NereusSDR for iOS: the band column's places for the PTT and the keyed meters, and +RX's reason at the slice limit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
@testable import NereusSDR
import Testing

/// R-IOS-58, DC7, DC8: the PTT keeps just right of whatever of the column
/// reaches its row and comes back when it closes; the keyed meters keep
/// right of the column, or go above the moved PTT where there is no room;
/// +RX at the slice limit says the radio's name and its slices.
@Suite("Band column")
@MainActor
struct BandButtonColumnTests {
    /// The PTT at home on an 11-inch iPad's band.
    static let home = CGRect(x: 14, y: 600, width: 76, height: 76)

    @Test("the PTT stays home while nothing reaches its row, and moves just right of what does")
    func pttMoves() {
        let column = CGRect(x: 4, y: 4, width: BandColumnLayout.columnWidth, height: 258)
        #expect(BandColumnLayout.pttFrame(home: Self.home, avoid: []) == Self.home)
        #expect(BandColumnLayout.pttFrame(home: Self.home, avoid: [column]) == Self.home)
        let pill = CGRect(x: column.maxX + 2, y: 4, width: 340, height: 680)
        let moved = BandColumnLayout.pttFrame(home: Self.home, avoid: [column, pill])
        #expect(moved.minX == pill.maxX + BandColumnLayout.clearance)
        #expect(moved.minY == Self.home.minY && moved.size == Self.home.size)
        #expect(!moved.intersects(pill) && !moved.intersects(column))
        // The phone sideways: the column alone reaches the PTT's row.
        let sidewaysHome = CGRect(x: 76, y: 182, width: 76, height: 76)
        let sidewaysColumn = CGRect(x: 66, y: 4, width: BandColumnLayout.columnWidth, height: 258)
        let beside = BandColumnLayout.pttFrame(home: sidewaysHome, avoid: [sidewaysColumn])
        #expect(beside.minX == sidewaysColumn.maxX + BandColumnLayout.clearance)
    }

    @Test("the keyed meters keep their place, move right of the column, or go above the moved PTT")
    func keyedMetersPlace() {
        let needed: CGFloat = 330
        let untouched = BandColumnLayout.keyedPlace(top: 300, height: 154, bandWidth: 1210, needed: needed,
                                                    avoid: [CGRect(x: 4, y: 4, width: 84, height: 258)], ptt: nil)
        #expect(untouched == .init(top: 300, leading: 0, alignLeading: false))
        let pill = CGRect(x: 90, y: 4, width: 340, height: 680)
        let beside = BandColumnLayout.keyedPlace(top: 300, height: 154, bandWidth: 1210, needed: needed,
                                                 avoid: [pill], ptt: nil)
        #expect(beside.top == 300 && !beside.alignLeading)
        #expect(beside.leading + 10 >= pill.maxX)
        // Too little room beside: above the moved PTT, at its left.
        let ptt = CGRect(x: 438, y: 182, width: 76, height: 76)
        let above = BandColumnLayout.keyedPlace(top: 134, height: 104, bandWidth: 812, needed: needed,
                                                avoid: [pill, ptt], ptt: ptt)
        #expect(above.alignLeading)
        #expect(above.top + 104 <= ptt.minY)
        #expect(above.leading + 10 == ptt.minX)
    }

    @Test("+RX at the slice limit names the radio as the Core reports it")
    func sliceLimitWords() {
        #expect(PanSheetModel.sliceLimitReason(radio: "ANAN-G2", limit: 5) == "The ANAN-G2 runs 5 slices.")
        #expect(PanSheetModel.sliceLimitReason(radio: "Hermes Lite 2", limit: 1) == "The Hermes Lite 2 runs 1 slice.")
        #expect(PanSheetModel.sliceLimitReason(radio: nil, limit: 4) == "The radio runs 4 slices.")
    }
}
