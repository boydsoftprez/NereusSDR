// NereusSDR for iOS: the Roundtable card and tags on the band: meter, placement and bounded measuring
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-11-03: Overlay redesign D10, D11, D13 to D15, D17 to D20.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import SwiftUI
import Testing
@testable import NereusSDR

@Suite("Roundtable card and tags on the band")
@MainActor
struct RoundtableCardLayoutTests {
    private func row(_ call: String, dbm: Double = -70, old: Bool = false, you: Bool = false,
                     peak: Double? = -66) -> RoundtableCardData.Row {
        .init(call: call, dbm: dbm, age: old ? 30_000 : 2_000, km: nil, bearing: nil, offsetHz: 0, you: you, old: old,
              peak: old ? nil : peak)
    }

    private func rows(_ count: Int) -> [RoundtableCardData.Row] {
        (0..<count).map { row("W\($0)ABC") }
    }

    private func talking(_ count: Int = 5) -> RoundtableCardData {
        RoundtableCardData(kind: .talking, hz: 7_236_400, keyed: ["K4ABC"], rows: rows(count))
    }

    private func double(_ count: Int = 5, names: [String] = ["K4ABC", "N4XYZ"]) -> RoundtableCardData {
        RoundtableCardData(kind: .double, hz: 7_236_400, keyed: names, clock: 83_000, rows: rows(count))
    }

    // MARK: The meter

    @Test("the meter's width is the fraction; a double is striped; stale is grey with no tick")
    func meter() {
        let live = RoundtableMeterBar(fraction: 0.5, peak: 0.75, style: .talking)
        #expect(live.fillFraction == 0.5 && live.tickFraction == 0.75 && !live.isStriped)
        #expect(RoundtableMeterBar(fraction: 1.4, peak: -1, style: .you).fillFraction == 1)
        #expect(RoundtableMeterBar(fraction: -1, peak: -1, style: .you).tickFraction == 0)
        #expect(RoundtableMeterBar(fraction: 0.5, peak: nil, style: .talking).tickFraction == nil)
        #expect(RoundtableMeterBar(fraction: 0.5, peak: 0.6, style: .double).isStriped)
        #expect(RoundtableMeterBar(fraction: 0.5, peak: 0.6, style: .youInDouble).isStriped)
        let stale = RoundtableMeterBar(fraction: 0.5, peak: 0.75, style: .stale)
        #expect(stale.tickFraction == nil && !stale.isStriped && stale.fillFraction == 0.5)
        #expect(RoundtableMeterBar.marks == [0.136, 0.273, 0.409, 0.545, 0.773])
    }

    @Test("a row's meter style follows the state, your own row and old readings")
    func meterStyles() {
        let plain = row("N4XYZ")
        let you = row("KG4VCF", you: true)
        let old = row("W9ABC", old: true)
        #expect(RoundtableTagLayout.meterStyle(kind: .talking, row: plain, frozen: false) == .talking)
        #expect(RoundtableTagLayout.meterStyle(kind: .talking, row: you, frozen: false) == .you)
        #expect(RoundtableTagLayout.meterStyle(kind: .you, row: plain, frozen: false) == .you)
        #expect(RoundtableTagLayout.meterStyle(kind: .unknown, row: plain, frozen: false) == .unknown)
        #expect(RoundtableTagLayout.meterStyle(kind: .double, row: plain, frozen: false) == .double)
        #expect(RoundtableTagLayout.meterStyle(kind: .double, row: you, frozen: false) == .youInDouble)
        #expect(RoundtableTagLayout.meterStyle(kind: .lastDouble, row: plain, frozen: false) == .ended)
        #expect(RoundtableTagLayout.meterStyle(kind: .double, row: old, frozen: false) == .stale)
        #expect(RoundtableTagLayout.meterStyle(kind: .double, row: plain, frozen: true) == .stale)
    }

    // MARK: Placement

    /// The iPhone 17's real short pan: 402 wide, the flag and its buttons taking
    /// most of the height right of the dial.
    private let phoneFoot: CGFloat = 220
    private let phoneWidth: CGFloat = 362

    @Test("iPhone on a short pan: beside the flag and narrow, its rows with meters")
    func phoneShortPanBeside() {
        let flag = CGRect(x: 140, y: 0, width: 220, height: 170)
        let placed = RoundtableTagLayout.placeCard(flags: [flag], dialX: 150, foot: phoneFoot, panWidth: phoneWidth,
                                                   compact: true, card: double())
        #expect(placed.placement == .beside)
        #expect(placed.rect.width >= 104 && placed.rect.width < 176)
        #expect(placed.rect.maxY == phoneFoot)
        #expect(placed.rect.maxX <= flag.minX)
        #expect(placed.rows > 0)
        #expect(placed.rows + placed.moreRows == 5)
    }

    @Test("the real iPhone 17 pan: the card starts under the round buttons, beside the flag, clear of both slices' flags")
    func realPhoneGeometry() {
        let buttons = CGRect(x: 87, y: 0, width: 44, height: 135)
        let flagA = CGRect(x: 132, y: 0, width: 200, height: 181)
        let flagB = CGRect(x: 240, y: 190, width: 150, height: 25)
        let flags = [flagA, flagB, buttons]
        let talkingCard = RoundtableTagLayout.placeCard(flags: flags, dialX: 133, foot: 239, panWidth: 362, compact: true,
                                                        card: talking())
        #expect(talkingCard.placement == .beside)
        #expect(talkingCard.rect.width >= 104 && talkingCard.rect.width < 176)
        #expect(talkingCard.rows >= 2 && talkingCard.rows + talkingCard.moreRows == 5)
        for flag in flags { #expect(!talkingCard.rect.intersects(flag)) }
        let doubleCard = RoundtableTagLayout.placedCard(flags: flags, dialX: 133, foot: 239, panWidth: 362, compact: true,
                                                        card: double())
        #expect(doubleCard?.placement == .beside && doubleCard?.plan.names == 2)
    }

    @Test("under 122 wide no age and no frequency in the head; under 150 S units for Both")
    func narrowRules() throws {
        let tight = try #require(RoundtableTagLayout.cardPlan(talking(), width: 121, room: 300))
        #expect(tight.tight && tight.narrow)
        let at122 = try #require(RoundtableTagLayout.cardPlan(talking(), width: 122, room: 300))
        #expect(!at122.tight && at122.narrow)
        let wide = try #require(RoundtableTagLayout.cardPlan(talking(), width: 150, room: 300))
        #expect(!wide.narrow)
        #expect(RoundtableTagLayout.cardSignal(-79, format: .both, narrow: true) == "S8")
        #expect(RoundtableTagLayout.cardSignal(-79, format: .both, narrow: false) == "S8 \u{2212}79")
        #expect(RoundtableTagLayout.cardHead(RoundtableCardData(kind: .you, hz: 1), narrow: true) == "You")
        #expect(RoundtableTagLayout.cardHead(RoundtableCardData(kind: .you, hz: 1), narrow: false)
                == "You \u{00B7} on the air")
    }

    @Test("iPhone on a taller pan: 176 under the flag, centered on the dial")
    func phoneTallPanUnder() {
        let flag = CGRect(x: 140, y: 0, width: 220, height: 170)
        let placed = RoundtableTagLayout.placeCard(flags: [flag], dialX: 150, foot: 420, panWidth: phoneWidth,
                                                   compact: true, card: talking())
        #expect(placed.placement == .under)
        #expect(placed.rect.width == 176)
        #expect(placed.rect.minY >= flag.maxY)
        #expect(placed.rect.minX == 62)
        #expect(placed.rows == 5 && placed.moreRows == 0)
    }

    @Test("iPad: 232 under the flag from the dial plus 4, clamped to the pan")
    func iPadUnder() {
        let flag = CGRect(x: 400, y: 0, width: 240, height: 180)
        let placed = RoundtableTagLayout.placeCard(flags: [flag], dialX: 410, foot: 480, panWidth: 1000,
                                                   compact: false, card: double())
        #expect(placed.placement == .under)
        #expect(placed.rect.width == 232)
        #expect(placed.rect.minX == 414)
        let clamped = RoundtableTagLayout.placeCard(flags: [flag], dialX: 900, foot: 480, panWidth: 1000,
                                                    compact: false, card: double())
        #expect(clamped.rect.maxX == 996)
    }

    @Test("the card never crosses a flag or leaves the pan")
    func neverCrosses() {
        let cards = [talking(1), talking(7), double(0), double(7), double(3, names: ["A", "B", "C", "D"])]
        for card in cards {
            for compact in [true, false] {
                for foot in stride(from: CGFloat(60), through: 500, by: 40) {
                    for dial in stride(from: CGFloat(0), through: 400, by: 30) {
                        for flagX in stride(from: CGFloat(0), through: 300, by: 50) {
                            let flag = CGRect(x: flagX, y: 0, width: 130, height: min(foot - 10, 170))
                            let keep = CGRect(x: 0, y: 0, width: 40, height: 60)
                            let placed = RoundtableTagLayout.placeCard(flags: [flag], dialX: dial, foot: foot,
                                                                       panWidth: 400, compact: compact, card: card,
                                                                       keepOut: [keep])
                            guard !placed.rect.isNull else { continue }
                            #expect(!placed.rect.intersects(flag), "\(placed.rect) crosses \(flag)")
                            #expect(!placed.rect.intersects(keep), "\(placed.rect) crosses \(keep)")
                            #expect(placed.rect.minX >= 0 && placed.rect.maxX <= 400 && placed.rect.minY >= 0)
                            #expect(placed.rect.maxY == foot)
                        }
                    }
                }
            }
        }
    }

    @Test("the iPad's button column: the card stands clear of it, folded or not")
    func clearOfButtonColumn() {
        // An iPad held upright: the dial left of center, with the flag and its round buttons right of it.
        let flag = CGRect(x: 342, y: 0, width: 200, height: 182)
        let buttons = CGRect(x: 296, y: 0, width: 44, height: 150)
        for folded in [false, true] {
            let column = BandColumnLayout.frame(bandHeight: 344, leadingInset: 0, folded: folded)
            #expect(column.minX == BandColumnLayout.origin && column.minY == BandColumnLayout.origin)
            #expect(folded ? column.height < 60 : column.height > 200)
            let placed = RoundtableTagLayout.placeCard(flags: [flag, buttons], dialX: 342, foot: 330,
                                                       panWidth: 1032, compact: false, card: double(5),
                                                       keepOut: [column])
            #expect(!placed.rect.isNull)
            #expect(!placed.rect.intersects(column), "\(placed.rect) crosses \(column)")
        }
        let inset = BandColumnLayout.frame(bandHeight: 344, leadingInset: 47, folded: false)
        #expect(inset.minX == 47 + BandColumnLayout.origin)
    }

    @Test("the real short iPhone pan: a double's two names share a line when they fit, and a reading row shows")
    func shortPanDoubleKeepsARow() throws {
        let flags = [CGRect(x: 87, y: 0, width: 44, height: 135), CGRect(x: 132, y: 0, width: 200, height: 181),
                     CGRect(x: 240, y: 190, width: 150, height: 25)]
        func placed(_ names: [String]) throws -> RoundtablePlacedCard {
            try #require(RoundtableTagLayout.placedCard(flags: flags, dialX: 133, foot: 239, panWidth: 362, compact: true,
                                                        card: double(7, names: names)))
        }
        let typical = try placed(["K4ABC", "N4XYZ"])
        #expect(typical.rect.width < 150 && typical.plan.narrow)
        #expect(typical.plan.namesInRow && typical.plan.names == 2 && typical.plan.namesGap == 6)
        #expect(typical.plan.rows >= 1 && typical.plan.rows + typical.plan.moreRows == 7 && typical.plan.showsMoreRows)
        for flag in flags { #expect(!typical.rect.intersects(flag)) }

        // Two long callsigns measure too wide: they stack, the names outlast the rows, and none may fit.
        let long = try placed(["VE3ABCD/P", "KH6XYZ/MM"])
        #expect(!long.plan.namesInRow && long.plan.names == 2)
        #expect(long.plan.rows == 0 && long.plan.moreRows == 7)

        // With no height at all the rows still give way first, and the head and names stay.
        let none = try #require(RoundtableTagLayout.cardPlan(double(7), width: 124, room: 60))
        #expect(none.rows == 0 && none.names == 2 && none.namesInRow)
    }

    @Test("two names on one line at a wide card, as before; three still stack")
    func wideDoubleResultsStay() throws {
        let wide = try #require(RoundtableTagLayout.cardPlan(double(5), width: 176, room: 400))
        #expect(wide.namesInRow && wide.namesGap == 10 && wide.rows == 5)
        let three = try #require(RoundtableTagLayout.cardPlan(double(5, names: ["K4A", "N4B", "W1C"]), width: 124, room: 400))
        #expect(!three.namesInRow && three.names == 3)
    }

    @Test("a short pan loses rows first with +N more, and the Double head and names stay")
    func shortPanKeepsDoubleHead() throws {
        let card = double(5)
        let roomy = try #require(RoundtableTagLayout.cardPlan(card, width: 176, room: 400))
        #expect(roomy.rows == 5 && roomy.moreRows == 0 && roomy.names == 2 && roomy.hasCaption)
        let fewer = try #require(RoundtableTagLayout.cardPlan(card, width: 176, room: roomy.height - 4))
        #expect(fewer.rows == 4 && fewer.moreRows == 1 && fewer.showsMoreRows && fewer.names == 2)
        // Only the head and the names.
        let least = try #require(RoundtableTagLayout.cardPlan(card, width: 120, room: 66))
        #expect(least.rows == 0 && !least.hasCaption && least.names == 2)
        #expect(least.height <= 66)
        // Then names, one at a time, with +N keyed.
        let one = try #require(RoundtableTagLayout.cardPlan(card, width: 120, room: 50))
        #expect(one.names < 2 && one.moreKeyed == 2 - one.names && one.height <= 50)
        // The head is last.
        let head = try #require(RoundtableTagLayout.cardPlan(card, width: 120, room: 29))
        #expect(head.names == 0 && !head.showsMoreKeyed)
        #expect(RoundtableTagLayout.cardPlan(card, width: 120, room: 28) == nil)
    }

    @Test("a dropped link keeps Last known longer than +N more")
    func frozenKeepsCaption() throws {
        var card = double(5)
        card.frozen = true
        card.note = "Reconnecting"
        // The names share a line, so the real short pan's 100 points keep every line but the rows.
        let roomy = try #require(RoundtableTagLayout.cardPlan(card, width: 125, room: 100))
        #expect(roomy.rows == 0 && roomy.hasCaption && roomy.showsMoreRows && roomy.namesInRow && roomy.hasNote)
        let plan = try #require(RoundtableTagLayout.cardPlan(card, width: 125, room: 85))
        #expect(plan.rows == 0 && plan.hasCaption && !plan.showsMoreRows && plan.names == 2 && plan.hasNote)
    }

    @Test("a double's two names sit side by side whenever they fit, at any width")
    func namesSideBySide() throws {
        #expect(try #require(RoundtableTagLayout.cardPlan(double(), width: 176, room: 300)).namesInRow)
        let narrow = try #require(RoundtableTagLayout.cardPlan(double(), width: 149, room: 300))
        #expect(narrow.namesInRow && narrow.namesGap == 10)
        #expect(try !(#require(RoundtableTagLayout.cardPlan(double(), width: 104, room: 300)).namesInRow))
        let long = double(names: ["EA8/KG4VCF/MM/QRP", "VP2E/KG4VCF/QRP"])
        #expect(try !(#require(RoundtableTagLayout.cardPlan(long, width: 176, room: 300)).namesInRow))
        #expect(try !(#require(RoundtableTagLayout.cardPlan(double(names: ["A", "B", "C"]), width: 232, room: 300)).namesInRow))
    }

    @Test("the card's words: caption, clock, age, frozen and last")
    func words() {
        var card = double()
        #expect(RoundtableTagLayout.cardCaption(card, narrow: false) == "Combined RF")
        #expect(RoundtableTagLayout.cardClock(card.clock) == "1:23")
        card.frozen = true
        #expect(RoundtableTagLayout.cardCaption(card, narrow: false) == "Last known \u{00B7} combined")
        #expect(RoundtableTagLayout.cardCaption(card, narrow: true) == "Last known")
        #expect(RoundtableTagLayout.cardCaption(RoundtableCardData(kind: .lastTalker, hz: 1, endedAgo: 6_000),
                                                narrow: false) == "Last heard 6s ago")
        #expect(RoundtableTagLayout.cardCaption(RoundtableCardData(kind: .you, hz: 1), narrow: false) == "Hearing you")
        #expect(RoundtableTagLayout.cardFormatTitle(.both) == "S / dBm")
        #expect(RoundtableTagLayout.cardAge(61_000) == "1m")
        #expect(RoundtableTagLayout.cardFrequency(7_236_400) == "7.2364")
    }

    // MARK: Tags

    private func stationTag(_ id: String, hz: Int64, glyph: RoundtableFlagLine.Glyph, keyed: [String] = [],
                            members: Int64 = 1, source: String? = nil, mine: Bool = false) -> RoundtableTag {
        RoundtableTag(roomId: id, hz: hz, glyph: glyph, label: id, members: members, mine: mine, keyed: keyed,
                      stationed: true, source: source)
    }

    @Test("a double's tag says Double and two names; a talker and a quiet tag say a name and +N")
    func faces() {
        let double = stationTag("d", hz: 1, glyph: .doubled, keyed: ["AA5ABC", "K0ABC", "W2ABC"], members: 3)
        #expect(RoundtableTagLayout.face(double) == RoundtableTagFace(head: "Double", count: nil, names: ["AA5ABC", "K0ABC"]))
        #expect(RoundtableTagLayout.tagHeight(double) == 54)
        let talker = stationTag("t", hz: 1, glyph: .talking, keyed: ["W6ABC"], members: 2)
        #expect(RoundtableTagLayout.face(talker) == RoundtableTagFace(head: "W6ABC", count: "+1", names: []))
        var quiet = stationTag("W2DEF\nN5ABC", hz: 1, glyph: .quiet, members: 3)
        quiet.label = "W2DEF\nN5ABC\nK7ABC"
        #expect(RoundtableTagLayout.face(quiet) == RoundtableTagFace(head: "W2DEF", count: "+2", names: []))
        #expect(RoundtableTagLayout.tagHeight(quiet) == RoundtableTagLayout.height)
        let alone = stationTag("W2DEF", hz: 1, glyph: .quiet)
        #expect(RoundtableTagLayout.face(alone).count == nil)
        // Tags are sized per item, no shared width.
        #expect(RoundtableTagLayout.itemWidth(double) != RoundtableTagLayout.itemWidth(alone))
    }

    @Test("one double across several frequencies in one source room is one tag")
    func mergedDouble() {
        let merged = RoundtableTagLayout.mergeDoubleTags([
            stationTag("a", hz: 100, glyph: .doubled, keyed: ["A", "B"], members: 2, source: "lsb-1"),
            stationTag("b", hz: 200, glyph: .doubled, keyed: ["B", "A"], members: 1, source: "lsb-1"),
            stationTag("c", hz: 300, glyph: .doubled, keyed: ["C", "D"], members: 2, source: "lsb-1"),
            stationTag("e", hz: 400, glyph: .doubled, keyed: ["A", "B"], members: 2, source: "lsb-2"),
        ])
        #expect(merged.map(\.roomId) == ["a", "c", "e"])
        #expect(merged[0].mergedHz == [200] && merged[0].members == 3)
    }

    private func arrange(_ tags: [RoundtableTag], card: RoundtableCardData? = nil, spoken: Set<Int64> = [],
                         flags: [CGRect] = [], foot: CGFloat = 220) -> RoundtableTagArrangement {
        RoundtableTagLayout.arrange(tags: tags, card: card, cardRoom: "active", format: .dbm, spokenHz: spoken,
                                    foot: foot, panWidth: 362, hzPerPoint: 100, lowHz: 7_220_000, flags: flags,
                                    keepOut: [], compact: true)
    }

    @Test("a frequency another slice's flag speaks for has no tag, and the card's own has none")
    func noTagForWhatIsSaid() {
        let other = stationTag("other", hz: 7_240_000, glyph: .talking, keyed: ["W6ABC"], members: 2)
        let active = stationTag("active", hz: 7_236_400, glyph: .talking, keyed: ["K4ABC"], members: 2, mine: true)
        #expect(arrange([other]).placements.count == 1)
        #expect(arrange([other], spoken: [7_240_000]).placements.isEmpty)
        var merged = other
        merged.mergedHz = [7_241_000]
        #expect(arrange([merged], spoken: [7_241_000]).placements.isEmpty)
        let withCard = arrange([active, other], card: talking())
        #expect(withCard.card != nil && withCard.placements.map(\.roomIds) == [["other"]])
        // Nobody talking draws no card, only the compact tag.
        let quiet = arrange([active, other], card: RoundtableCardData(kind: .quiet, hz: 7_236_400))
        #expect(quiet.card == nil && quiet.placements.flatMap(\.roomIds).contains("active"))
    }

    @Test("a tag with no clear spot is dropped; tags stay off the card and the flags")
    func dropped() throws {
        let tag = stationTag("t", hz: 7_240_000, glyph: .talking, keyed: ["W6ABC"], members: 2)
        #expect(arrange([tag], flags: [CGRect(x: 0, y: 0, width: 362, height: 300)]).placements.isEmpty)
        let flag = CGRect(x: 150, y: 0, width: 150, height: 300)
        let beside = try #require(arrange([tag], flags: [flag]).placements.first)
        #expect(beside.right <= Double(flag.minX) || beside.left >= Double(flag.maxX))
        let arranged = arrange([tag], card: talking(), flags: [flag])
        let card = try #require(arranged.card)
        for placement in arranged.placements {
            #expect(placement.right <= Double(card.rect.minX) || placement.left >= Double(card.rect.maxX))
        }
        // The double's taller stack also keeps off a flag that ends above a one line tag.
        let lowFlag = CGRect(x: 150, y: 0, width: 150, height: 180)
        let stacked = stationTag("d", hz: 7_240_000, glyph: .doubled, keyed: ["AA5ABC", "K0ABC"], members: 2)
        let placed = try #require(arrange([stacked], flags: [lowFlag]).placements.first)
        #expect(placed.right <= Double(lowFlag.minX) || placed.left >= Double(lowFlag.maxX))
        // A double taller than the pan's room above the foot has no place.
        #expect(arrange([stacked], foot: 40).placements.isEmpty)
    }

    @Test("folds stay: tags too close to sit apart fold into one +N of their own width")
    func foldsStay() throws {
        let tags = (0..<5).map { stationTag("r\($0)", hz: 7_240_000 + Int64($0) * 50, glyph: .talking, keyed: ["W\($0)ABC"]) }
        let arranged = arrange(tags)
        let fold = try #require(arranged.placements.first { $0.folded })
        #expect(fold.roomIds.count >= 2)
        #expect(fold.width == RoundtableTagLayout.foldWidth())
    }

    @Test("text is measured once per string: a thousand ticks of new readings measure nothing new")
    func measuringIsBounded() {
        let tags = [stationTag("a", hz: 7_230_000, glyph: .doubled, keyed: ["AA5ABC", "K0ABC"], members: 2),
                    stationTag("b", hz: 7_240_000, glyph: .talking, keyed: ["W6ABC"], members: 2)]
        var card = double(5)
        let flag = CGRect(x: 150, y: 0, width: 150, height: 150)
        _ = arrange(tags, card: card, flags: [flag])
        let measured = RoundtableTagLayout.textMeasurements
        let planned = RoundtableTagLayout.planComputations
        for tick in 0..<1000 {
            card.clock += 1000
            for index in card.rows.indices {
                card.rows[index].dbm = -70 - Double(tick % 20)
                card.rows[index].age = Int64(tick) * 1000
            }
            _ = arrange(tags, card: card, flags: [flag])
        }
        #expect(RoundtableTagLayout.textMeasurements == measured)
        #expect(RoundtableTagLayout.planComputations == planned)
    }

    @Test("a double names which keyed station is you")
    func keyedYou() {
        let source = "lsb-7236000"
        let member = { (ref: String, you: Bool, keyed: Bool) -> RoundtableModel.Member in
            .init(ref: ref, callsign: ref.uppercased(), nth: 1, offsetHz: 0, joining: false, away: false, keyed: keyed,
                  software: "NereusSDR", softwareVersion: "", you: you, reading: nil, hz: 7_236_400, sourceRoom: source)
        }
        let view = RoundtableModel.Room(
            id: "lsb-7236400", band: "40m", family: "lsb", hz: 7_236_400,
            members: [member("me", true, true), member("k4abc", false, true)],
            now: [.init(id: 12, kind: "doubled", talkers: [
                .init(ref: "me", callsign: "ME", nth: 1, hz: 7_236_400, sourceRoom: source),
                .init(ref: "k4abc", callsign: "K4ABC", nth: 1, hz: 7_236_400, sourceRoom: source),
            ], start: 900, ending: false, names: [], heard: 0, hz: 7_236_000, sourceRoom: source)],
            earlier: [], messages: [], frequencyView: true)
        let summary = RoundtableModel.RoomSummary(id: "lsb-7236400", band: "40m", family: "lsb", hz: 7_236_400,
                                                  state: "doubled", talkers: [], members: 2, messages: 0, last: 0,
                                                  folded: false, start: nil, heard: 0, readings: [], lastMessage: nil,
                                                  ended: nil)
        let card = RoundtableTagLayout.cardData(summary: summary, view: view, now: 1000, link: .live, linkSince: 0)
        #expect(card.kind == .double && card.keyedYou == "ME")
        #expect(RoundtableTagLayout.cardName("ME", in: card) == "You")
        #expect(RoundtableTagLayout.cardName("K4ABC", in: card) == "K4ABC")
    }
}
