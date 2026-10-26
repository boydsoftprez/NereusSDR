// NereusSDR for iOS: the band column's places for the PTT and the keyed meters, its scroll to an opened menu's button, and +RX's reason at the slice limit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import NereusBand
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-58, DC7, DC8: the PTT keeps just right of whatever of the column
/// reaches its row and comes back when it closes; the keyed meters keep
/// right of the column, or go above the moved PTT where there is no room;
/// +RX at the slice limit says the radio's name and its slices.
@Suite("Band column", .serialized)
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

    @Test("DC14: on a band too short for the column, two columns of four side by side, all in view")
    func twoColumnsOfFour() {
        // The column: eight 30 point buttons, 2 point gaps and padding.
        #expect(BandColumnLayout.columnHeight(rows: 8) == 258)
        #expect(BandColumnLayout.columnHeight(rows: 4) == 130)
        #expect(BandColumnLayout.columnWidth(columns: 1) == BandColumnLayout.columnWidth)
        #expect(BandColumnLayout.columnWidth(columns: 2) == 166)
        // The iPad's band: one column. The phone sideways (about 182 points): two of four, which fit.
        #expect(BandColumnLayout.columns(bandHeight: 700) == 1)
        #expect(BandColumnLayout.columns(bandHeight: 266) == 1)
        #expect(BandColumnLayout.columns(bandHeight: 265) == 2)
        #expect(BandColumnLayout.columns(bandHeight: 182) == 2)
        #expect(BandColumnLayout.columnHeight(rows: 4) <= BandColumnLayout.menuMaxHeight(bandHeight: 182))
    }

    @Test("the PTT moves right of a full flag it would cover, past the column first")
    func pttKeepsOffAFlag() {
        let home = CGRect(x: 76, y: 94, width: 76, height: 76)
        let column = CGRect(x: 66, y: 4, width: BandColumnLayout.columnWidth(columns: 2), height: 130)
        // Slice A's flag, open, reaching down past the PTT's row, just right of the column.
        let flag = CGRect(x: 238, y: 0, width: 252, height: 157)
        let moved = BandColumnLayout.pttFrame(home: home, avoid: [column], flags: [flag])
        #expect(moved.minX == flag.maxX + BandColumnLayout.clearance)
        #expect(!moved.intersects(flag) && !moved.intersects(column))
        // A second flag beside the first: past that one too.
        let next = CGRect(x: 500, y: 0, width: 120, height: 120)
        let past = BandColumnLayout.pttFrame(home: home, avoid: [column], flags: [flag, next])
        #expect(past.minX == next.maxX + BandColumnLayout.clearance)
        // A flag above the PTT's row, or folded away, leaves it where the column put it.
        let short = CGRect(x: 238, y: 0, width: 252, height: 60)
        #expect(BandColumnLayout.pttFrame(home: home, avoid: [column], flags: [short]).minX
                == column.maxX + BandColumnLayout.clearance)
    }

    @Test("DC15: while keyed the PTT stays home over a flag in its row, beside the column if it reaches")
    func keyedPttStaysHome() {
        let home = CGRect(x: 76, y: 94, width: 76, height: 76)
        let flag = CGRect(x: 20, y: 0, width: 252, height: 157)
        #expect(BandColumnLayout.pttFrame(home: home, avoid: [], flags: [flag], keyed: true) == home)
        // Receiving, the same flag moves it.
        #expect(BandColumnLayout.pttFrame(home: home, avoid: [], flags: [flag]).minX
                == flag.maxX + BandColumnLayout.clearance)
        // The column still reaches its row while keyed: just right of it, over the flag.
        let column = CGRect(x: 66, y: 4, width: BandColumnLayout.columnWidth(columns: 2), height: 130)
        let beside = BandColumnLayout.pttFrame(home: home, avoid: [column], flags: [flag], keyed: true)
        #expect(beside.minX == column.maxX + BandColumnLayout.clearance)
        #expect(beside.minY == home.minY && beside.size == home.size)
    }

    @Test("DC15 amended: keyed, the PTT stays where it was when transmitting began until unkey")
    func keyedPttHeldWhereKeyed() {
        let home = CGRect(x: 76, y: 94, width: 76, height: 76)
        let maxX: CGFloat = 874 - 62 - PttButton.inset
        let flag = CGRect(x: 20, y: 0, width: 252, height: 157)
        func inputs(keyed: Bool, flags: [CGRect], home: CGRect = home, maxX: CGFloat = maxX) -> PttLatch.Inputs {
            PttLatch.Inputs(keyed: keyed, home: home, maxX: maxX,
                            receive: BandColumnLayout.pttFrame(home: home, avoid: [], flags: flags, maxX: maxX),
                            keyedRule: BandColumnLayout.pttFrame(home: home, avoid: [], flags: flags, keyed: true,
                                                                 maxX: maxX))
        }
        // Receiving: past the flag, and the latch follows.
        let receiving = inputs(keyed: false, flags: [flag])
        let past = PttLatch.shown(nil, receiving)
        #expect(past.minX == flag.maxX + BandColumnLayout.clearance)
        var latch = PttLatch.next(nil, receiving)
        #expect(latch.frame == past)
        // Keyed: it stays there, not at home over the flag.
        let keyed = inputs(keyed: true, flags: [flag])
        #expect(PttLatch.shown(latch, keyed) == past)
        latch = PttLatch.next(latch, keyed)
        // The flag moves while keyed: still there.
        let moved = inputs(keyed: true, flags: [flag.offsetBy(dx: -60, dy: 0)])
        #expect(PttLatch.shown(latch, moved) == past)
        latch = PttLatch.next(latch, moved)
        #expect(latch.frame == past)
        // Unkeyed: its usual place again, past the flag where it is now.
        let after = inputs(keyed: false, flags: [flag.offsetBy(dx: -60, dy: 0)])
        #expect(PttLatch.shown(latch, after).minX == flag.maxX - 60 + BandColumnLayout.clearance)
        // Keyed with nothing held (the screen came up keyed): DC15's place.
        #expect(PttLatch.shown(nil, keyed) == home)
        // The phone turned with the held spot still on the band: there, on the new home's row.
        let turned = CGRect(x: 14, y: 786, width: 76, height: 76)
        let turnedKeyed = inputs(keyed: true, flags: [], home: turned, maxX: 388)
        #expect(PttLatch.shown(latch, turnedKeyed) == CGRect(x: past.minX, y: turned.minY, width: 76, height: 76))
        // A band too narrow for the held spot, or a home right of it (a
        // rounded edge coming in): DC15's place, held from then.
        let narrow = inputs(keyed: true, flags: [flag], maxX: past.maxX - 1)
        #expect(PttLatch.shown(latch, narrow) == home)
        #expect(PttLatch.next(latch, narrow) == PttLatch(frame: home))
        let inset = CGRect(x: past.minX + 1, y: 94, width: 76, height: 76)
        let insetKeyed = inputs(keyed: true, flags: [], home: inset)
        #expect(PttLatch.shown(latch, insetKeyed) == inset)
    }

    /// The PR 653 re-review, Minor 2: a band that changes height while keyed
    /// (Stage Manager, a row coming or going) moved the PTT's home and so
    /// dropped the hold: the PTT jumped to DC15's place over the flag, and a
    /// second tap where it had been hit the flag and left the radio keyed.
    @Test("keyed, a band that changes height keeps the held PTT's x and follows its home's y")
    func keyedPttHeldThroughABandHeightChange() {
        let home = CGRect(x: 76, y: 94, width: 76, height: 76)
        let maxX: CGFloat = 874 - 62 - PttButton.inset
        let flag = CGRect(x: 20, y: 0, width: 252, height: 157)
        func inputs(keyed: Bool, home: CGRect) -> PttLatch.Inputs {
            PttLatch.Inputs(keyed: keyed, home: home, maxX: maxX,
                            receive: BandColumnLayout.pttFrame(home: home, avoid: [], flags: [flag], maxX: maxX),
                            keyedRule: BandColumnLayout.pttFrame(home: home, avoid: [], flags: [flag], keyed: true,
                                                                 maxX: maxX))
        }
        // Keyed where it had moved past the flag.
        var latch = PttLatch.next(nil, inputs(keyed: false, home: home))
        let past = latch.frame
        #expect(past.minX == flag.maxX + BandColumnLayout.clearance)
        latch = PttLatch.next(latch, inputs(keyed: true, home: home))
        // The band 40 points shorter: its home 40 points higher, the flag still in its row.
        let shorter = inputs(keyed: true, home: home.offsetBy(dx: 0, dy: -40))
        #expect(shorter.keyedRule.minX == home.minX, "the flag no longer reaches the PTT's row")
        let shown = PttLatch.shown(latch, shorter)
        #expect(shown == past.offsetBy(dx: 0, dy: -40))
        latch = PttLatch.next(latch, shorter)
        #expect(latch.frame == shown)
        // A tap at the held x on its new row lands on the PTT, not on the flag beside it.
        let tap = CGPoint(x: past.midX, y: shown.midY)
        #expect(shown.contains(tap) && !flag.contains(tap))
        // Taller again: still there, on the new row.
        let taller = inputs(keyed: true, home: home.offsetBy(dx: 0, dy: 30))
        #expect(PttLatch.shown(latch, taller) == past.offsetBy(dx: 0, dy: 30))
        // Unkeyed: its usual place on the band as it now stands.
        #expect(PttLatch.shown(latch, inputs(keyed: false, home: home.offsetBy(dx: 0, dy: -40)))
                == shorter.receive)
    }

    @Test("the PTT moving past flags stops at the band's right edge")
    func pttStaysOnTheBand() {
        let home = CGRect(x: 76, y: 94, width: 76, height: 76)
        let flags = [CGRect(x: 60, y: 0, width: 252, height: 157), CGRect(x: 320, y: 0, width: 252, height: 157),
                     CGRect(x: 580, y: 0, width: 252, height: 157)]
        let unbounded = BandColumnLayout.pttFrame(home: home, avoid: [], flags: flags)
        #expect(unbounded.minX == 832 + BandColumnLayout.clearance)
        let maxX: CGFloat = 874 - 62 - PttButton.inset
        let bounded = BandColumnLayout.pttFrame(home: home, avoid: [], flags: flags, maxX: maxX)
        #expect(bounded.maxX == maxX)
        #expect(bounded.minY == home.minY && bounded.size == home.size)
        // Room enough: the bound changes nothing.
        #expect(BandColumnLayout.pttFrame(home: home, avoid: [], flags: [flags[0]], maxX: maxX).minX
                == flags[0].maxX + BandColumnLayout.clearance)
        // A band too narrow for even home: the PTT stays home rather than left of it.
        #expect(BandColumnLayout.pttFrame(home: home, avoid: [], flags: flags, maxX: 100) == home)
    }

    @Test("the column's note sits at the band's lower right, clear of the pill, the flags and the PTT")
    func notePlace() {
        let band = CGSize(width: 874, height: 182)
        let free = BandColumnLayout.notePlace(bandSize: band, trailingInset: 62, height: 40, maxWidth: 360, avoid: [])
        #expect(free.maxX == 874 - 62 - BandColumnLayout.noteTrailingInset)
        #expect(free.width == 360)
        #expect(free.maxY == 182 - BandColumnLayout.noteBottomInset)
        // The zoom buttons stay clear below it.
        #expect(free.maxY + ZoomButtons.buttonSize.height + ZoomButtons.inset <= band.height)
        // The pill beside the two columns reaches down to it: the note keeps right of the pill.
        let pill = CGRect(x: 66 + 166 + 2, y: 4, width: 340, height: 174)
        let beside = BandColumnLayout.notePlace(bandSize: band, trailingInset: 62, height: 40, maxWidth: 360,
                                                avoid: [pill])
        #expect(beside.minX >= pill.maxX + BandColumnLayout.clearance)
        #expect(beside.maxX == free.maxX)
        // A flag at the right reaching its row: the note takes the gap left of it, if wide enough.
        let flag = CGRect(x: 700, y: 0, width: 120, height: 150)
        let left = BandColumnLayout.notePlace(bandSize: band, trailingInset: 62, height: 40, maxWidth: 360,
                                              avoid: [pill, flag])
        #expect(!left.intersects(flag) && !left.intersects(pill))
        #expect(left.width >= BandColumnLayout.noteMinWidth)
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
        // A PTT held far right (PttLatch) with room between it and the column: there, clear of both.
        let column = CGRect(x: 66, y: 4, width: BandColumnLayout.columnWidth(columns: 2), height: 130)
        let held = CGRect(x: 720, y: 94, width: 76, height: 76)
        let between = BandColumnLayout.keyedPlace(top: 14, height: 148, bandWidth: 874, needed: needed,
                                                  avoid: [column, held], ptt: held, held: true,
                                                  narrowest: KeyedOverlay.gaugesNarrowest,
                                                  leadingInset: 62, trailingInset: 62)
        #expect(between.top == 14 && !between.alignLeading)
        #expect(between.leading + 10 >= column.maxX + BandColumnLayout.clearance)
        #expect(874 - between.trailing - 10 <= held.minX - BandColumnLayout.clearance)
        #expect(874 - between.trailing - between.leading - 20 >= needed)
    }

    @Test("held mid-band sideways with no room for the keyed meters beside it: narrowed on the wider side")
    func keyedMetersNarrowBesideAHeldPtt() {
        let needed = KeyedOverlay.gaugesWidth
        let narrowest = KeyedOverlay.gaugesNarrowest
        // Four tabs keep 44 points each inside the narrowest gauges.
        #expect((narrowest - 2 * 8 - 3 * 4) / 4 >= 44)
        let width: CGFloat = 874
        let column = CGRect(x: 66, y: 4, width: BandColumnLayout.columnWidth(columns: 2), height: 130)
        func place(_ ptt: CGRect, held: Bool = true) -> BandColumnLayout.KeyedPlace {
            BandColumnLayout.keyedPlace(top: 34, height: 148, bandWidth: width, needed: needed,
                                        avoid: [column, ptt], ptt: ptt, held: held, narrowest: narrowest,
                                        leadingInset: 62, trailingInset: 62)
        }
        func region(_ place: BandColumnLayout.KeyedPlace) -> (start: CGFloat, end: CGFloat) {
            (place.leading + 10, width - place.trailing - 10)
        }
        // More room left of the PTT: there, between the column and the PTT, narrowed.
        let leftOf = CGRect(x: 498, y: 94, width: 76, height: 76)
        let left = region(place(leftOf))
        #expect(left.start >= column.maxX + BandColumnLayout.clearance)
        #expect(left.end <= leftOf.minX - BandColumnLayout.clearance)
        #expect(left.end - left.start >= narrowest && left.end - left.start < needed)
        #expect(place(leftOf).top == 34 && !place(leftOf).alignLeading)
        // More room right of it: there, inside the phone's rounded edge.
        let rightOf = CGRect(x: 420, y: 94, width: 76, height: 76)
        let right = region(place(rightOf))
        #expect(right.start >= rightOf.maxX + BandColumnLayout.clearance)
        #expect(right.end <= width - 62)
        #expect(right.end - right.start >= narrowest && right.end - right.start < needed)
        // Not held (DC15's keyed place, or receiving): unchanged.
        #expect(place(leftOf, held: false) == BandColumnLayout.keyedPlace(
            top: 34, height: 148, bandWidth: width, needed: needed, avoid: [column, leftOf], ptt: leftOf))
        #expect(place(leftOf, held: false).trailing == 0)
    }

    /// The PR 653 re-review, Minor 1: with no room for 44 point targets on
    /// either side of a held PTT (the iPhone SE sideways, held near x 384,
    /// about 198 points each side), the meters went above it at its left and
    /// ran on to the band's edge, narrower than the floor.
    @Test("held with no room either side: the keyed meters go above the PTT at their full width")
    func keyedMetersAboveAHeldPttKeepTheirWidth() {
        let needed = KeyedOverlay.gaugesWidth
        let width: CGFloat = 667
        let column = CGRect(x: 4, y: 4, width: BandColumnLayout.columnWidth(columns: 2), height: 300)
        let ptt = CGRect(x: 384, y: 162, width: 76, height: 76)
        let place = BandColumnLayout.keyedPlace(top: 60, height: 104, bandWidth: width, needed: needed,
                                                avoid: [column, ptt], ptt: ptt, held: true,
                                                narrowest: KeyedOverlay.gaugesNarrowest)
        // Neither side has the narrowest gauges' room.
        #expect(ptt.minX - BandColumnLayout.clearance - (column.maxX + BandColumnLayout.clearance)
                < KeyedOverlay.gaugesNarrowest)
        #expect(width - (ptt.maxX + BandColumnLayout.clearance) < KeyedOverlay.gaugesNarrowest)
        #expect(place.alignLeading)
        #expect(place.top + 104 <= ptt.minY - 6 + 0.5)
        // Moved left from the PTT's left edge just far enough for their full width, clear of the column.
        #expect(width - place.trailing - place.leading - 20 >= needed)
        #expect(place.leading + 10 < ptt.minX)
        #expect(place.leading + 10 >= column.maxX + BandColumnLayout.clearance)
        // Never onto the column: where the band is too narrow even for that, they start right of it.
        let tight = BandColumnLayout.keyedPlace(top: 60, height: 104, bandWidth: 500, needed: needed,
                                                avoid: [column, ptt], ptt: ptt, held: true,
                                                narrowest: KeyedOverlay.gaugesNarrowest)
        #expect(tight.leading + 10 == column.maxX + BandColumnLayout.clearance)
    }

    /// The PR 653 re-review, Minor 4: right of a held PTT the meters ran to
    /// the band's right inset, over zoom and the tuning dial in their rows.
    @Test("beside a held PTT the keyed meters keep clear of zoom and the tuning dial")
    func keyedMetersBesideAHeldPttKeepOffTheBandsControls() {
        let needed = KeyedOverlay.gaugesWidth
        let width: CGFloat = 874
        let column = CGRect(x: 66, y: 4, width: BandColumnLayout.columnWidth(columns: 2), height: 300)
        let ptt = CGRect(x: 300, y: 250, width: 76, height: 76)
        let zoom = CGRect(x: 600, y: 260, width: ZoomButtons.size.width, height: ZoomButtons.size.height)
        func place(_ controls: [CGRect]) -> BandColumnLayout.KeyedPlace {
            BandColumnLayout.keyedPlace(top: 200, height: 104, bandWidth: width, needed: needed,
                                        avoid: [column, ptt], ptt: ptt, held: true,
                                        narrowest: KeyedOverlay.gaugesNarrowest, leadingInset: 62,
                                        trailingInset: 62, bandControls: controls)
        }
        // Without the band's controls: full width right of the PTT, over zoom.
        let over = place([])
        #expect(over.leading + 10 >= ptt.maxX + BandColumnLayout.clearance)
        #expect(width - over.trailing - 10 > zoom.minX)
        // With them: right of the PTT, narrowed to end before zoom.
        let clear = place([zoom])
        #expect(clear.leading + 10 >= ptt.maxX + BandColumnLayout.clearance)
        #expect(width - clear.trailing - 10 == zoom.minX - BandColumnLayout.clearance)
        #expect(width - clear.trailing - clear.leading - 20 >= KeyedOverlay.gaugesNarrowest)
        // A control out of the meters' rows changes nothing.
        #expect(place([zoom.offsetBy(dx: 0, dy: 200)]) == over)
        // The controls are where BandGestureLayer draws them: zoom, and the knob or the wheel.
        let waterfall = CGRect(x: 0, y: 150, width: width, height: 230)
        #expect(DialLayer.controlFrames(kind: .off, waterfall: waterfall, sideways: true, trailingInset: 62)
                == [CGRect(origin: DialLayer.zoomOrigin(kind: .off, waterfall: waterfall, sideways: true,
                                                        trailingInset: 62), size: ZoomButtons.size)])
        let knob = DialLayer.controlFrames(kind: .waterfallKnob, waterfall: waterfall, sideways: true,
                                           trailingInset: 62)
        #expect(knob.count == 2
                && knob[1] == DialLayer.knobFrame(waterfall: waterfall, sideways: true, trailingInset: 62))
        let wheel = DialLayer.controlFrames(kind: .thumbwheel, waterfall: waterfall, sideways: true,
                                            trailingInset: 62, uprightWidth: 402)
        #expect(wheel.count == 2 && wheel[1] == DialLayer.wheelFrame(waterfall: waterfall, sideways: true,
                                                                      uprightWidth: 402, trailingInset: 62))
    }

    @Test("a stacked pan starts down the band where the pans' equal shares and hairlines put it")
    func panTop() {
        #expect(BandColumnLayout.panTop(index: 0, count: 1, bandHeight: 600) == 0)
        #expect(BandColumnLayout.panTop(index: 0, count: 2, bandHeight: 601) == 0)
        // Two pans on 601 points: 300 each and the hairline, Pan 2 from 301.
        #expect(BandColumnLayout.panTop(index: 1, count: 2, bandHeight: 601) == 301)
        #expect(BandColumnLayout.panTop(index: 2, count: 3, bandHeight: 902) == 602)
        // Pan 2's flag in the PTT's row moves the PTT on the band, at the foot pan's top.
        let home = CGRect(x: 14, y: 500, width: 76, height: 76)
        let flag = CGRect(x: 20, y: 150, width: 252, height: 157).offsetBy(
            dx: 0, dy: BandColumnLayout.panTop(index: 1, count: 2, bandHeight: 601))
        #expect(BandColumnLayout.pttFrame(home: home, avoid: [], flags: [flag]).minX
                == flag.maxX + BandColumnLayout.clearance)
    }

    /// L2: a band too short for even two columns of four scrolls the
    /// column, and opening a menu scrolls its button into view.
    @Test("on a band too short for the column, opening ATT scrolls its button into view",
          .timeLimit(.minutes(2)))
    func openingRevealsItsButton() async throws {
        let suite = "BandButtonColumnTests-reveal-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        // 80 points: the menus' 72-point cap holds neither one column (258) nor two of four (130).
        let band = CGSize(width: 400, height: 80)
        #expect(BandColumnLayout.columns(bandHeight: band.height) == 2)
        #expect(BandColumnLayout.columnHeight(rows: 4) > BandColumnLayout.menuMaxHeight(bandHeight: band.height))
        let state = RevealState()
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: 200), size: CGSize(width: band.width, height: 300))
        window.windowLevel = .alert + 1
        let host = ShotHosting.host(RevealHost(main: model.main, state: state, band: band)
            .preferredColorScheme(.dark))
        host.view.frame = CGRect(origin: .zero, size: window.frame.size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        // Laid out before its accessibility tree is walked: a walk of a hosted scroll view before its
        // first layout, with application accessibility just switched on, leaves SwiftUI's platform
        // scroll views reporting no identifier for the rest of the test process.
        await ShotWait.laidOut(window)
        // What of the column shows: the band's top, from the column's margin to the cap.
        let top = window.frame.minY + BandColumnLayout.origin
        let shown = top...(top + BandColumnLayout.menuMaxHeight(bandHeight: band.height))
        func attFrame() async -> CGRect? {
            for _ in 0..<100 {
                if let att = Self.element("bandColumn.attenuator", in: window) {
                    return att.accessibilityFrame
                }
                try? await Task.sleep(for: .milliseconds(50))
            }
            return nil
        }
        let before = try #require(await attFrame())
        // ATT is in the second column's last row, below what shows.
        #expect(before.minY > shown.upperBound)
        // The column's scroll view, scrolled to its top. ATT's accessibility
        // element can be there before the scroll view has its content laid
        // out (a busy simulator), so it is waited for as ATT is.
        func overflowingColumn() async -> UIScrollView? {
            for _ in 0..<100 {
                window.layoutIfNeeded()
                if let scroll = Self.scrollViews(in: window).first(where: { $0.contentSize.height > $0.bounds.height + 1 }) {
                    return scroll
                }
                try? await Task.sleep(for: .milliseconds(50))
            }
            return nil
        }
        let column = try #require(await overflowingColumn())
        #expect(column.contentOffset.y == 0)
        let columnTop = column.convert(column.bounds, to: nil).minY + window.frame.minY
        let att = (before.minY - columnTop)...(before.maxY - columnTop)
        state.open = .attenuator
        func inView() -> Bool {
            column.contentOffset.y <= att.lowerBound + 0.5
                && column.contentOffset.y + column.bounds.height >= att.upperBound - 0.5
        }
        for _ in 0..<100 where !inView() {
            try? await Task.sleep(for: .milliseconds(50))
        }
        #expect(inView(), "ATT at \(att) in the column; it shows \(column.contentOffset.y) + \(column.bounds.height)")
    }

    private static func scrollViews(in view: UIView) -> [UIScrollView] {
        var found: [UIScrollView] = []
        if let scroll = view as? UIScrollView {
            found.append(scroll)
        }
        for subview in view.subviews {
            found += scrollViews(in: subview)
        }
        return found
    }

    /// SetupTypedEntryTests' accessibility search, for a small window.
    private static func element(_ id: String, in root: NSObject) -> NSObject? {
        var queue: [NSObject] = [root]
        var next = 0
        while next < queue.count, next < 50_000 {
            let node = queue[next]
            next += 1
            if SetupTypedEntryTests.identifier(node) == id {
                return node
            }
            if let elements = node.accessibilityElements as? [NSObject] {
                queue += elements
            } else {
                let count = node.accessibilityElementCount()
                if count != NSNotFound, count > 0 {
                    queue += (0..<count).compactMap { node.accessibilityElement(at: $0) as? NSObject }
                }
            }
            if let view = node as? UIView {
                queue += view.subviews
            }
        }
        return nil
    }

    @Test("+RX at the slice limit names the radio as the Core reports it")
    func sliceLimitWords() {
        #expect(PanSheetModel.sliceLimitReason(radio: "ANAN-G2", limit: 5) == "The ANAN-G2 runs five slices.")
        #expect(PanSheetModel.sliceLimitReason(radio: "Hermes Lite 2", limit: 1) == "The Hermes Lite 2 runs one slice.")
        #expect(PanSheetModel.sliceLimitReason(radio: nil, limit: 4) == "The radio runs four slices.")
    }
}

/// The column's open menu, set by the test.
@MainActor
private final class RevealState: ObservableObject {
    @Published var open: BandColumnMenu?
}

/// Pan 1's band column alone on a band `band` tall.
private struct RevealHost: View {
    let main: MainScreenModel
    @ObservedObject var state: RevealState
    let band: CGSize
    @State private var folded = false
    @State private var tab: DisplayPillTab = .spectrum

    var body: some View {
        BandButtonColumn(pan: .first, open: $state.open, folded: $folded, main: main, displayTab: $tab,
                         openSetup: {}, onSetupDefaults: { _, _ in }, onOpenPage: { _ in }, onOpen: { _ in })
            .frame(width: band.width, height: band.height)
            .coordinateSpace(name: BandColumnLayout.space)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .background(ChromeColours.bar)
    }
}
