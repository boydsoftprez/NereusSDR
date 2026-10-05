// NereusSDR for iOS: the fold rule: full flags where they fit, one-line tags where they would land on another
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Testing
@testable import NereusBand

/// D9, R-IOS-11: every slice keeps its full flag unless it would land on
/// another flag; then it folds. The active slice is always full. The
/// colours here are synthetic (D4).
@Suite struct FlagLayoutTests {
    static let phoneWidth: CGFloat = 390
    static let sidewaysWidth: CGFloat = 844
    /// The board's sideways band, 874 points across.
    static let boardSidewaysWidth: CGFloat = 874
    static let iPadWidth: CGFloat = 1194

    static func geometry(width: CGFloat, centerHz: Double = 7_245_000, spanHz: Double = 48_000) -> BandGeometry {
        BandGeometry(centerHz: centerHz, spanHz: spanHz, size: CGSize(width: width, height: 300),
                     dbmRange: -140 ... -40)
    }

    static func slice(_ id: Int, _ hz: Double, lower: Bool = true, tx: Bool = false) -> BandSlice {
        BandSlice(id: id, frequencyHz: hz, filterLowHz: lower ? -3000 : 100, filterHighHz: lower ? -100 : 3000,
                  colour: "#102030", lowerSideband: lower, txSlice: tx)
    }

    // MARK: The brief's case: two slices two kilohertz apart

    @Test func twoSlicesTwoKilohertzApartFoldTheInactiveOneOnAPhone() {
        // A 10 kHz span, where 2 kHz is 78 points on the phone and 239 on the iPad.
        let slices = [Self.slice(0, 7_236_000), Self.slice(1, 7_238_000)]
        let placements = FlagLayout.layout(slices: slices, activeSliceId: 0,
                                           geometry: Self.geometry(width: Self.phoneWidth, centerHz: 7_237_000,
                                                                   spanHz: 10_000))
        #expect(placements.count == 2)
        #expect(!placements[0].isFolded)
        #expect(placements[1].isFolded)
        #expect(placements[1].rect.height == FlagLayout.foldedHeight)
    }

    @Test func twoSlicesThatFoldOnAPhoneStayFullOnAnIPad() {
        // 2.5 kHz on a 10 kHz span: 97 points on the phone, 298 on the
        // iPad, past the flag and its button column (246).
        let slices = [Self.slice(0, 7_236_000), Self.slice(1, 7_238_500)]
        let placements = FlagLayout.layout(slices: slices, activeSliceId: 0,
                                           geometry: Self.geometry(width: Self.iPadWidth, centerHz: 7_237_000,
                                                                   spanHz: 10_000))
        #expect(placements.allSatisfy { !$0.isFolded })
        #expect(!FlagLayout.overlaps(placements[0].rect, placements[1].rect))
        let phone = FlagLayout.layout(slices: slices, activeSliceId: 0,
                                      geometry: Self.geometry(width: Self.phoneWidth, centerHz: 7_237_000,
                                                              spanHz: 10_000))
        #expect(phone[1].isFolded)
    }

    @Test func oneTapOnAFoldedTagSwaps() {
        let slices = [Self.slice(0, 7_236_000), Self.slice(1, 7_238_000)]
        let geometry = Self.geometry(width: Self.phoneWidth, centerHz: 7_237_000, spanHz: 10_000)
        let before = FlagLayout.layout(slices: slices, activeSliceId: 0, geometry: geometry)
        #expect(!before[0].isFolded && before[1].isFolded)
        // The tap makes slice B active: B is full, and A folds.
        let after = FlagLayout.layout(slices: slices, activeSliceId: 1, geometry: geometry)
        #expect(after[0].isFolded)
        #expect(!after[1].isFolded)
        #expect(after[1].rect.minY == 0)
    }

    // MARK: The board's case: 7.2364 and 7.249 MHz on a 48 kHz span

    @Test func theBoardsTwoSlicesFoldUprightAndSidewaysWithTheButtonColumn() {
        // 12.6 kHz apart: 222 points on the 874-point band sideways, closer
        // than the flag, the gap and the button column (246), so B folds
        // sideways too, as the board draws it.
        let slices = [Self.slice(0, 7_236_400), Self.slice(1, 7_249_000)]
        let upright = FlagLayout.layout(slices: slices, activeSliceId: 0, geometry: Self.geometry(width: Self.phoneWidth))
        #expect(!upright[0].isFolded)
        #expect(upright[1].isFolded)
        // The tag cannot sit beside A's flag, so it drops below it.
        #expect(upright[1].rect.minY == upright[0].rect.maxY + FlagLayout.stackGap)
        let sideways = FlagLayout.layout(slices: slices, activeSliceId: 0,
                                         geometry: Self.geometry(width: Self.boardSidewaysWidth))
        #expect(!sideways[0].isFolded)
        #expect(sideways[1].isFolded)
    }

    @Test func theBoardsWidePairStaysFullSidewaysInBothTypeSizes() {
        // B moved to 7.256.000: 19.6 kHz, 357 points apart.
        let slices = [Self.slice(0, 7_236_400), Self.slice(1, 7_256_000)]
        let geometry = Self.geometry(width: Self.boardSidewaysWidth)
        let plain = FlagLayout.layout(slices: slices, activeSliceId: 0, geometry: geometry)
        #expect(plain.allSatisfy { !$0.isFolded })
        #expect(plain.allSatisfy { $0.rect.minY == 0 && $0.rect.size == FlagLayout.flagSize })
        let large = FlagLayout.layout(slices: slices, activeSliceId: 0, geometry: geometry,
                                      flagHeight: { _ in FlagLayout.largeFlagHeight },
                                      sideColumn: FlagLayout.sideColumnSize(button: FlagLayout.largeSideButton))
        #expect(large.allSatisfy { !$0.isFolded && $0.rect.height == FlagLayout.largeFlagHeight })
    }

    // MARK: The fold threshold: the flag, the gap and the button column

    /// Two lower-sideband slices `points` apart on the sideways band,
    /// both hanging right of their lines, A active.
    static func pair(pointsApart points: Double, large: Bool) -> [FlagPlacement] {
        let geometry = geometry(width: boardSidewaysWidth)
        let perPoint = geometry.spanHz / Double(boardSidewaysWidth)
        let aHz = geometry.lowHz + 100 * perPoint
        let slices = [slice(0, aHz), slice(1, aHz + points * perPoint)]
        return FlagLayout.layout(slices: slices, activeSliceId: 0, geometry: geometry,
                                 flagHeight: { _ in large ? FlagLayout.largeFlagHeight : FlagLayout.flagSize.height },
                                 sideColumn: FlagLayout.sideColumnSize(button: large ? FlagLayout.largeSideButton
                                                                                     : FlagLayout.sideButton))
    }

    @Test func theSecondFlagFoldsWhenTheLinesAreCloserThan246Points() {
        #expect(FlagLayout.flagSize.width + FlagLayout.sideGap + FlagLayout.sideButton == 246)
        #expect(Self.pair(pointsApart: 245.5, large: false)[1].isFolded)
        #expect(!Self.pair(pointsApart: 246.5, large: false)[1].isFolded)
    }

    /// The redrawn flag (JJ, 2026-09-30): one layout at every text size.
    @Test func theRedrawnFlagIsOneSizeAtEveryTextSize() {
        #expect(FlagLayout.flagSize == CGSize(width: 200, height: 158))
        #expect(FlagLayout.largeFlagHeight == FlagLayout.flagSize.height)
        #expect(FlagLayout.radeFlagHeight == 176)
        #expect(FlagLayout.radeFlagHeight + FlagLayout.ownerRowHeight == 220)
        #expect(FlagLayout.sideButton == FlagLayout.largeSideButton)
    }

    @Test func flagsStartBelowTheBackBar() {
        let slices = [Self.slice(0, 7_236_400), Self.slice(1, 7_249_000)]
        let placements = FlagLayout.layout(slices: slices, activeSliceId: 0,
                                           geometry: Self.geometry(width: Self.phoneWidth), top: 50)
        #expect(placements[0].rect.minY == 50)
        #expect(placements[1].rect.minY == placements[0].rect.maxY + FlagLayout.stackGap)
    }

    @Test func inLargeTypeTheThresholdIs246Points() {
        #expect(FlagLayout.flagSize.width + FlagLayout.sideGap + FlagLayout.largeSideButton == 246)
        #expect(Self.pair(pointsApart: 245.5, large: true)[1].isFolded)
        #expect(!Self.pair(pointsApart: 246.5, large: true)[1].isFolded)
    }

    @Test func theButtonColumnIsThreeFingerSizedButtons() {
        #expect(FlagLayout.sideColumnSize() == CGSize(width: 44, height: 136))
        #expect(FlagLayout.sideColumnSize(button: FlagLayout.largeSideButton) == CGSize(width: 44, height: 136))
    }

    @Test func theButtonColumnNeverLandsOnItsOwnFlagOrOffTheBand() {
        let width = Double(Self.phoneWidth)
        // A flag hanging right from a line near the left edge: the column
        // cannot sit left of it on the band, so it sits on the far side.
        let atLeft = CGRect(x: 10, y: 0, width: 200, height: 151)
        let left = FlagLayout.sideColumnRect(flag: atLeft, lineX: 10, bandWidth: width)
        #expect(!FlagLayout.overlaps(left, atLeft))
        #expect(left.minX >= 0 && left.maxX <= Self.phoneWidth)
        #expect(left.minX == atLeft.maxX + FlagLayout.sideGap)
        // Hanging left from a line near the right edge: the far side again.
        let atRight = CGRect(x: 180, y: 0, width: 200, height: 151)
        let right = FlagLayout.sideColumnRect(flag: atRight, lineX: 380, bandWidth: width)
        #expect(!FlagLayout.overlaps(right, atRight))
        #expect(right.maxX == atRight.minX - FlagLayout.sideGap)
        // With room, on the line's side, as the desktop's floating buttons.
        let middle = CGRect(x: 150, y: 0, width: 200, height: 151)
        let usual = FlagLayout.sideColumnRect(flag: middle, lineX: 150, bandWidth: width)
        #expect(usual.maxX == middle.minX - FlagLayout.sideGap)
    }

    @Test func threeSlicesStackTheirTagsBelowEachOther() {
        let slices = [Self.slice(0, 7_240_000), Self.slice(1, 7_241_000), Self.slice(2, 7_242_000)]
        let placements = FlagLayout.layout(slices: slices, activeSliceId: 1,
                                           geometry: Self.geometry(width: Self.phoneWidth))
        #expect(!placements[1].isFolded)
        #expect(placements[1].rect.minY == 0)
        #expect(placements[0].isFolded && placements[2].isFolded)
        for (a, b) in [(0, 1), (0, 2), (1, 2)] {
            #expect(!FlagLayout.overlaps(placements[a].rect, placements[b].rect))
        }
        // A's tag is placed before C's, so it sits higher.
        #expect(placements[0].rect.minY < placements[2].rect.minY)
    }

    // MARK: Sides and edges

    @Test func aFlagHangsAwayFromItsPassband() {
        let geometry = Self.geometry(width: Self.sidewaysWidth)
        let lineX = CGFloat(geometry.x(forHz: 7_245_000))
        let lower = FlagLayout.layout(slices: [Self.slice(0, 7_245_000, lower: true)], activeSliceId: 0,
                                      geometry: geometry)
        #expect(lower[0].rect.minX == lineX)
        let upper = FlagLayout.layout(slices: [Self.slice(0, 7_245_000, lower: false)], activeSliceId: 0,
                                      geometry: geometry)
        #expect(upper[0].rect.maxX == lineX)
    }

    @Test func aFlagNearAnEdgeFlipsAndStaysOnTheBand() {
        let geometry = Self.geometry(width: Self.phoneWidth)
        // Lower sideband near the right edge: it cannot hang right, so it hangs left.
        let right = FlagLayout.layout(slices: [Self.slice(0, geometry.highHz - 1_000)], activeSliceId: 0,
                                      geometry: geometry)
        #expect(right[0].rect.maxX == CGFloat(geometry.x(forHz: geometry.highHz - 1_000)))
        // Upper sideband near the left edge: it hangs right.
        let left = FlagLayout.layout(slices: [Self.slice(0, geometry.lowHz + 500, lower: false)], activeSliceId: 0,
                                     geometry: geometry)
        #expect(left[0].rect.minX == CGFloat(geometry.x(forHz: geometry.lowHz + 500)))
        // Off the band altogether: the flag goes with its line (D74).
        let off = FlagLayout.layout(slices: [Self.slice(0, geometry.highHz + 50_000)], activeSliceId: 0,
                                    geometry: geometry)
        #expect(off[0].rect.minX >= Self.phoneWidth)
    }

    @Test func aSliceOffTheBandTakesItsFlagWithIt() {
        // D74: the band panned away from slice A, whose line is now 50
        // points past the right edge; its flag goes with it, not pinned
        // to the edge.
        let geometry = Self.geometry(width: Self.phoneWidth)
        let perPoint = geometry.spanHz / Double(Self.phoneWidth)
        let off = Self.slice(0, geometry.highHz + 50 * perPoint, lower: false)
        let placed = FlagLayout.layout(slices: [off], activeSliceId: 0, geometry: geometry)
        #expect(abs(placed[0].rect.maxX - (Self.phoneWidth + 50)) < 0.01)
        // Past the left edge, the lower-sideband flag hangs right of its
        // line and slides in as the line comes back.
        let left = Self.slice(0, geometry.lowHz - 20 * perPoint)
        let slid = FlagLayout.layout(slices: [left], activeSliceId: 0, geometry: geometry)
        #expect(abs(slid[0].rect.minX - -20) < 0.01)
        // A line on the band still keeps its flag on the band.
        let edge = Self.slice(0, geometry.highHz - 1 * perPoint)
        let kept = FlagLayout.layout(slices: [edge], activeSliceId: 0, geometry: geometry)
        #expect(kept[0].rect.maxX <= Self.phoneWidth)
    }

    @Test func withNoActiveSliceTheFirstSliceKeepsItsFlag() {
        let slices = [Self.slice(1, 7_238_000), Self.slice(0, 7_236_000)]
        let placements = FlagLayout.layout(slices: slices, activeSliceId: nil,
                                           geometry: Self.geometry(width: Self.phoneWidth))
        // Slice order decides: slice 0 is placed first and keeps its flag.
        #expect(!placements[1].isFolded)
        #expect(placements[0].isFolded)
    }

    @Test func theFoldedTagFitsItsFrequency() {
        #expect(FlagLayout.foldedSize(frequencyText: "7.249.000").width == 151)
        #expect(FlagLayout.foldedSize(frequencyText: "14.249.000").width == 160)
        #expect(BandSlice.frequencyText(hz: 7_249_000) == "7.249.000")
        #expect(BandSlice.frequencyText(hz: 14_074_005) == "14.074.005")
        #expect(BandSlice.frequencyText(hz: 475_000) == "0.475.000")
    }

    @Test func slicesAreLetteredAndColouredFromTheCatalogue() {
        #expect(BandSlice.letter(forIndex: 0) == "A")
        #expect(BandSlice.letter(forIndex: 4) == "E")
        let colours = ["#111111", "#222222"]
        #expect(BandSlice.colour(forIndex: 1, in: colours) == "#222222")
        // A slice the catalogue has no colour for is grey, not another slice's colour.
        #expect(BandSlice.colour(forIndex: 2, in: colours) == BandSlice.colourUnknown)
        let slice = BandSlice(id: 0, frequencyHz: 7_236_400, filterLowHz: -3000, filterHighHz: -100, colour: "#111111",
                              lowerSideband: true)
        #expect(slice.bandwidthText == "2.9K")
        #expect(slice.passbandHz == 7_233_400...7_236_300)
    }

    // MARK: A taller flag (the RADE row, or large type)

    @Test func aTallerFlagPushesTheTagBelowItsOwnHeight() {
        // The board's two slices upright: B folds and drops below A's flag,
        // which is taller while it carries the RADE row.
        let slices = [Self.slice(0, 7_236_400), Self.slice(1, 7_249_000)]
        let placements = FlagLayout.layout(slices: slices, activeSliceId: 0,
                                           geometry: Self.geometry(width: Self.phoneWidth),
                                           flagHeight: {
                                               $0.id == 0 ? FlagLayout.radeFlagHeight : FlagLayout.flagSize.height
                                           })
        #expect(placements[0].rect.height == FlagLayout.radeFlagHeight)
        #expect(placements[0].rect.width == FlagLayout.flagSize.width)
        #expect(placements[1].isFolded)
        #expect(placements[1].rect.minY == FlagLayout.radeFlagHeight + FlagLayout.stackGap)
    }

    @Test func eachFullFlagKeepsItsOwnHeight() {
        // Sideways both flags fit side by side; each flag's own height is
        // used for its fold test and for where it sits.
        let slices = [Self.slice(0, 7_236_400), Self.slice(1, 7_256_000)]
        let placements = FlagLayout.layout(slices: slices, activeSliceId: 0,
                                           geometry: Self.geometry(width: Self.sidewaysWidth),
                                           flagHeight: { $0.id == 1 ? 150 : FlagLayout.flagSize.height })
        #expect(placements.allSatisfy { !$0.isFolded })
        #expect(placements[0].rect.height == FlagLayout.flagSize.height)
        #expect(placements[1].rect.height == 150)
    }
}
