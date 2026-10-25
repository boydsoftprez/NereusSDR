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
