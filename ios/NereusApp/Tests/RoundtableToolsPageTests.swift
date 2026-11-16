// NereusSDR for iOS: Roundtable's tags on the band and its page on the Tools tab, against the desktop's layout cases and a fake Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 18, the phone's tag layout run through
// the desktop's cases, and the Tools page that reads and never tunes.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: JJ's answer: hidden, the tags show every room dimmed.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-25: the room page found by its read bar, not by an identifier on
// a ScrollView, which another suite's accessibility walk can blank.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

// 2026-10-31: Capture quiet and joining people with canonical frequency records.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-11-03: Pins follow the overlay redesign (design D7, D8).
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-11-04: One sheet on iPhone and iPad: toggle, swap, Back in place, and a tap never tunes
// (design D22, D24 to D27, D36, D38). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Spec 11.3 (Tags, Tools) and U1. The tag layout runs the desktop's own
/// cases (tests/tst_roundtable_tag_layout.cpp): a group folds only when its
/// tags cannot slide apart. The Tools page lists every room by band, marks
/// this phone's slices, and a row reads its room without tuning; only a tag
/// on the band (or Tune A here) tunes. With `NEREUS_MAIN_SHOTS` set, the
/// page, a room and the crowded band are written there for the board.
@Suite("Roundtable tags and Tools page", .serialized)
@MainActor
struct RoundtableToolsPageTests {
    private let platform = TestPlatform()

    // MARK: The desktop's layout cases

    private static let tagWidth = 80.0
    private static let gap = 4.0

    private static func tagAt(_ id: String, _ hz: Int64, mine: Bool = false, mentionsMe: Bool = false)
        -> RoundtableTag {
        RoundtableTag(roomId: id, hz: hz, glyph: .talking, label: id, members: 3, mine: mine, mentionsMe: mentionsMe)
    }

    /// Five rooms 50 Hz apart on 7.185 MHz; `special` (0 to 4) is mine or names you, -1 for none.
    private static func fiveClose(_ special: Int = -1, mentions: Bool = false) -> [RoundtableTag] {
        (0..<5).map { i in
            let pinned = i == special
            return tagAt("r\(i)", 7_185_000 + Int64(i) * 50, mine: pinned && !mentions, mentionsMe: pinned && mentions)
        }
    }

    private static func anyOverlap(_ placements: [RoundtableTagPlacement]) -> Bool {
        for i in placements.indices {
            for j in placements.indices where j > i {
                if placements[i].left < placements[j].right && placements[j].left < placements[i].right {
                    return true
                }
            }
        }
        return false
    }

    private static func layout(_ tags: [RoundtableTag], _ hzPerPoint: Double, _ leftHz: Int64, _ width: Double,
                               tagWidth: Double = tagWidth, keepOut: [RoundtableKeepOut] = [])
        -> [RoundtableTagPlacement] {
        RoundtableTagLayout.layout(tags, hzPerPoint: hzPerPoint, leftHz: leftHz, width: width, tagWidth: tagWidth,
                                   gap: gap, keepOut: keepOut)
    }

    @Test("five tags 50 Hz apart fold into one, on the rooms")
    func fiveTagsFiftyHertzApartFoldIntoOne() {
        // 100 Hz per point: the five sit within 2 points of each other.
        let placements = Self.layout(Self.fiveClose(), 100, 7_150_000, 800)
        #expect(placements.count == 1)
        #expect(placements.first?.folded == true)
        #expect(placements.first?.roomIds == ["r0", "r1", "r2", "r3", "r4"])
        // The fold sits on the rooms, 350 points across.
        #expect(abs((placements.first?.centerX ?? 0) - 351) < 2)
        #expect(placements.first?.width == Self.tagWidth)
    }

    @Test("zooming in opens the fold, each tag on its own frequency")
    func zoomingInOpensTheFold() {
        // 1 Hz per point: 50 points apart, room for 40-point tags.
        let placements = Self.layout(Self.fiveClose(), 1, 7_184_800, 800, tagWidth: 40)
        #expect(placements.count == 5)
        for (i, placement) in placements.enumerated() {
            #expect(!placement.folded)
            #expect(placement.roomIds == ["r\(i)"])
            #expect(placement.centerX == 200 + Double(i) * 50)
        }
        #expect(!Self.anyOverlap(placements))
        // Between the two, a middle zoom opens part of it.
        #expect(Self.layout(Self.fiveClose(), 4, 7_184_000, 800, tagWidth: 40).count > 1)
    }

    @Test("your slice's room stays out of the fold")
    func yourSlicesRoomStaysOutOfTheFold() {
        let placements = Self.layout(Self.fiveClose(2), 100, 7_150_000, 800)
        #expect(placements.count == 2)
        #expect(placements.filter(\.folded).count == 1)
        for placement in placements {
            #expect(placement.roomIds == (placement.folded ? ["r0", "r1", "r3", "r4"] : ["r2"]))
        }
        #expect(!Self.anyOverlap(placements))
    }

    @Test("a room that names you stays out of the fold")
    func aRoomThatNamesYouStaysOutOfTheFold() {
        let placements = Self.layout(Self.fiveClose(0, mentions: true), 100, 7_150_000, 800)
        #expect(placements.count == 2)
        #expect(placements.first?.folded == false)
        #expect(placements.first?.roomIds == ["r0"])
        #expect(placements.last?.folded == true)
        #expect(placements.last?.roomIds.count == 4)
    }

    @Test("two overlapping tags move apart instead of folding")
    func twoOverlappingTagsMoveApartInsteadOfFolding() throws {
        // 10 points apart with 80-point tags and the pan nearly empty.
        let placements = Self.layout([Self.tagAt("a", 7_185_000), Self.tagAt("b", 7_186_000)], 100, 7_150_000, 800)
        try #require(placements.count == 2)
        #expect(!placements[0].folded && !placements[1].folded)
        #expect(!Self.anyOverlap(placements))
        #expect(placements[1].left - placements[0].right >= Self.gap - 1e-9)
        // Each stays within a tag's width of its own frequency.
        #expect(abs(placements[0].centerX - 350) <= Self.tagWidth)
        #expect(abs(placements[1].centerX - 360) <= Self.tagWidth)
    }

    @Test("a lone neighbor of your room moves aside")
    func aLoneNeighborOfYourRoomMovesAside() {
        // A fold holds two or more: your room and one other sit side by side.
        let placements = Self.layout([Self.tagAt("mine", 7_185_000, mine: true), Self.tagAt("other", 7_185_050)],
                                     100, 7_150_000, 800)
        #expect(placements.count == 2)
        #expect(placements.allSatisfy { !$0.folded })
        #expect(!Self.anyOverlap(placements))
    }

    @Test("folds widen only when the tags do not fit")
    func foldsWidenOnlyWhenTheTagsDoNotFit() {
        // Four pairs, each pair 10 points apart. Across 800 points the eight fit side by side.
        var wide: [RoundtableTag] = []
        for pair in 0..<4 {
            let hz = 7_150_000 + Int64(100 + pair * 200) * 100
            wide += [Self.tagAt("w\(pair)a", hz), Self.tagAt("w\(pair)b", hz + 1000)]
        }
        let open = Self.layout(wide, 100, 7_150_000, 800)
        #expect(open.count == 8)
        #expect(open.allSatisfy { !$0.folded })
        #expect(!Self.anyOverlap(open))
        // Across 400 points eight tags (676) do not fit, four (340) do: each pair folds.
        var narrow: [RoundtableTag] = []
        for pair in 0..<4 {
            let hz = 7_150_000 + Int64(50 + pair * 100) * 100
            narrow += [Self.tagAt("n\(pair)a", hz), Self.tagAt("n\(pair)b", hz + 1000)]
        }
        let folded = Self.layout(narrow, 100, 7_150_000, 400)
        #expect(folded.count == 4)
        #expect(folded.allSatisfy { $0.folded && $0.roomIds.count == 2 })
        #expect(!Self.anyOverlap(folded))
    }

    @Test("placements stay on the pan and rooms off it are left out")
    func placementsStayOnThePanAndOffPanRoomsAreLeftOut() {
        let tags = [Self.tagAt("left", 7_150_100), Self.tagAt("right", 7_229_900), Self.tagAt("off", 7_240_000),
                    Self.tagAt("below", 7_140_000)]
        let placements = Self.layout(tags, 100, 7_150_000, 800)
        #expect(placements.map(\.roomIds) == [["left"], ["right"]])
        for placement in placements {
            #expect(placement.left >= Self.gap - 1e-9)
            #expect(placement.right <= 800 - Self.gap + 1e-9)
        }
        #expect(Self.layout(tags, 0, 7_150_000, 800).isEmpty)
        #expect(Self.layout(tags, 100, 7_150_000, 0).isEmpty)
    }

    @Test("tags keep clear of a flag over their row")
    func tagsKeepClearOfAFlagOverTheirRow() throws {
        // A flag over 300 to 560: the room under its left half goes left of it, the right half's right.
        let tags = [Self.tagAt("left", 7_150_000 + 340 * 100), Self.tagAt("right", 7_150_000 + 540 * 100),
                    Self.tagAt("far", 7_150_000 + 700 * 100)]
        let flag = [RoundtableKeepOut(left: 300, right: 560)]
        let placements = Self.layout(tags, 100, 7_150_000, 800, keepOut: flag)
        try #require(placements.count == 3)
        #expect(placements[0].right <= 300 - Self.gap + 1e-9)
        #expect(placements[1].left >= 560 + Self.gap - 1e-9)
        #expect(!Self.anyOverlap(placements))
        #expect(placements.allSatisfy { $0.right <= 300 || $0.left >= 560 })
        // Only the stretches beside the flag count: with 600 of 800 kept out the five fold,
        // and with the whole pan kept out nothing is placed.
        let crowded = Self.layout(Self.fiveClose(), 1, 7_184_800, 800, tagWidth: 40,
                                  keepOut: [RoundtableKeepOut(left: 100, right: 700)])
        #expect(crowded.count < 5)
        #expect(!Self.anyOverlap(crowded))
        #expect(crowded.allSatisfy { $0.right <= 100 || $0.left >= 700 })
        #expect(Self.layout(tags, 100, 7_150_000, 800, keepOut: [RoundtableKeepOut(left: 0, right: 800)]).isEmpty)
    }

    private static func summary(_ id: String, _ hz: Int64, _ state: String, talkers: [String] = [],
                                members: Int64 = 3, messages: Int64 = 0, last: Int64 = 0,
                                band: String = "40m") -> RoundtableModel.RoomSummary {
        RoundtableModel.RoomSummary(id: id, band: band, family: "lsb", hz: hz, state: state, talkers: talkers,
                                    members: members, messages: messages, last: last, folded: false, start: nil,
                                    heard: 0, readings: [], lastMessage: nil, ended: nil)
    }

    @Test("the model's rooms become tags: who, state, members, mine, names you, and You on your own turn")
    func theModelsRoomsBecomeTags() throws {
        let rooms = [Self.summary("lsb-7185000", 7_185_000, "talking", talkers: ["W1TLK"], members: 9),
                     Self.summary("lsb-7200000", 7_200_000, "doubled", talkers: ["K4ABC", "N3XYZ"], members: 4),
                     Self.summary("lsb-7212000", 7_212_000, "unknown"),
                     Self.summary("lsb-7230000", 7_230_000, "quiet", members: 2)]
        let tags = RoundtableTagLayout.tags(rooms: rooms, mine: ["lsb-7185000"],
                                            mentionCounts: ["lsb-7212000": 2], room: { _ in nil })
        try #require(tags.count == 4)
        #expect(tags[0] == RoundtableTag(roomId: "lsb-7185000", hz: 7_185_000, glyph: .talking, label: "W1TLK",
                                         members: 9, mine: true, mentionsMe: false, keyed: ["W1TLK"]))
        #expect(tags[1].label == "Double" && tags[1].glyph == .doubled)
        #expect(tags[2].label == "Unknown" && tags[2].glyph == .unknown && tags[2].mentionsMe && !tags[2].mine)
        #expect(tags[3].label == "Quiet" && tags[3].glyph == .quiet)
        // Your own turn in your slice's room: You, in cyan (U8).
        let you = RoundtableModel.Member(ref: "me", callsign: "KG4VCF", nth: 1, grid: nil, km: nil, offsetHz: 0,
                                         joining: false, away: false, keyed: true, software: "NereusSDR",
                                         softwareVersion: "26.10.3", you: true, reading: nil)
        let turn = RoundtableModel.Turn(id: 40, kind: "talking", talkers: [.init(ref: "me", callsign: "KG4VCF", nth: 1)],
                                        start: 1, end: nil, ending: false, names: [], heard: 0, mine: nil)
        let view = RoundtableModel.Room(id: "lsb-7185000", band: "40m", family: "lsb", hz: 7_185_000, members: [you],
                                        now: [turn], earlier: [], messages: [])
        let keyed = RoundtableTagLayout.tags(rooms: rooms, mine: ["lsb-7185000"], mentionCounts: [:]) { id in
            id == view.id ? view : nil
        }
        #expect(keyed[0].glyph == .you && keyed[0].label == "You")
        // Another station's room never reads You.
        let notMine = RoundtableTagLayout.tags(rooms: rooms, mine: [], mentionCounts: [:]) { id in
            id == view.id ? view : nil
        }
        #expect(notMine[0].glyph == .talking)
    }

    @Test("offline greys the tags as last known, connecting and off hide them, hidden dims them")
    func offlineGreysConnectingHides() {
        #expect(RoundtableTagLayout.display(available: false, link: .live) == .hidden)
        #expect(RoundtableTagLayout.display(available: false, link: .hidden) == .hidden)
        // You are hidden: every room as when live, its marking dimmed.
        let cases: [(RoundtableModel.LinkState, RoundtableTagDisplay)] = [
            (.off, .hidden), (.connecting, .hidden), (.live, .live), (.reconnecting, .stale), (.hidden, .dimmed),
            (.offline, .stale),
        ]
        for (link, display) in cases {
            #expect(RoundtableTagLayout.display(available: true, link: link) == display)
        }
        // The dimmed border keeps its state's colour at the dimmed opacity;
        // live keeps it whole.
        for glyph in [RoundtableFlagLine.Glyph.talking, .doubled, .unknown, .you] {
            var live: (CGFloat, CGFloat, CGFloat, CGFloat) = (0, 0, 0, 0)
            var dim: (CGFloat, CGFloat, CGFloat, CGFloat) = (0, 0, 0, 0)
            #expect(UIColor(RoundtableTagLayer.border(glyph)).getRed(&live.0, green: &live.1, blue: &live.2,
                                                                     alpha: &live.3))
            #expect(UIColor(RoundtableTagLayer.border(glyph, dimmed: true)).getRed(&dim.0, green: &dim.1,
                                                                                   blue: &dim.2, alpha: &dim.3))
            #expect(abs(dim.0 - live.0) < 0.01 && abs(dim.1 - live.1) < 0.01 && abs(dim.2 - live.2) < 0.01)
            #expect(live.3 > 0.99 && abs(dim.3 - CGFloat(RoundtableColours.dimmedOpacity)) < 0.01)
        }
    }

    @Test("frequencies as the fold list shows them, its heading and its rows' words")
    func frequenciesAsTheFoldListShowsThem() {
        #expect(RoundtableTagLayout.frequencyText(7_185_000) == "7.185")
        #expect(RoundtableTagLayout.frequencyText(7_030_500) == "7.0305")
        #expect(RoundtableTagLayout.frequencyText(14_074_150) == "14.07415")
        #expect(RoundtableTagLayout.frequencyText(14_074_151) == "14.074151")
        #expect(RoundtableTagLayout.foldHeading(count: 5, sliceLetter: "A") == "5 frequencies here \u{00B7} a tap tunes slice A")
        #expect(RoundtableTagLayout.foldWho(RoundtableTag(roomId: "x", hz: 1, glyph: .unknown, label: "Unknown"))
                == "Unknown talker")
        #expect(RoundtableTagLayout.foldWho(Self.tagAt("K4ABC", 1)) == "K4ABC")
    }

    @Test("each tag is as wide as its own words, up to 128 points, and a fold holds three glyphs and +99")
    func tagWidths() {
        let short = RoundtableTagLayout.itemWidth(RoundtableTag(roomId: "x", hz: 1, label: "Q", members: 1))
        let call = RoundtableTagLayout.itemWidth(RoundtableTag(roomId: "x", hz: 1, label: "KG4VCF", members: 9))
        #expect(short < call && call < RoundtableTagLayout.maxWidth)
        let long = RoundtableTag(roomId: "x", hz: 1, label: String(repeating: "W", count: 40), members: 12)
        #expect(RoundtableTagLayout.itemWidth(long) == RoundtableTagLayout.maxWidth)
        #expect(RoundtableTagLayout.foldWidth() >= RoundtableTagLayout.minWidth)
    }

    @Test("a flag in the tag row keeps the tags off its span; one that only meets the row's edge does not")
    func keepOut() {
        let top: CGFloat = 345
        let inRow = CGRect(x: 240, y: 330, width: 150, height: 30)
        let touching = CGRect(x: 240, y: 315, width: 150, height: 30.6)
        let below = CGRect(x: 10, y: 366.5, width: 40, height: 20)
        let spans = RoundtableTagLayer.keepOut([inRow, touching, below], top: top, width: 402)
        #expect(spans == [RoundtableKeepOut(left: 240, right: 390)])
    }

    // MARK: The Tools page's rows

    @Test("chips: All, then each band with a room, lowest first, with counts; the slice's band first shown")
    func chips() {
        let rooms = [Self.summary("usb-14250000", 14_250_000, "talking", band: "other"),
                     Self.summary("lsb-7236400", 7_236_400, "talking"),
                     Self.summary("lsb-7185000", 7_185_000, "quiet"),
                     Self.summary("lsb-3850000", 3_850_000, "talking", band: "")]
        let chips = RoundtableToolsList.chips(rooms)
        #expect(chips == [.init(id: "all", title: "All", count: 4), .init(id: "80m", title: "80M", count: 1),
                          .init(id: "40m", title: "40M", count: 2), .init(id: "20m", title: "20M", count: 1)])
        #expect(RoundtableToolsList.defaultChip(rooms, sliceRoomId: "usb-14250000", sliceHz: 7_236_400) == "20m")
        #expect(RoundtableToolsList.defaultChip(rooms, sliceRoomId: nil, sliceHz: 7_100_000) == "40m")
        #expect(RoundtableToolsList.defaultChip(rooms, sliceRoomId: nil, sliceHz: 28_400_000) == "all")
        #expect(RoundtableToolsList.chips([]) == [.init(id: "all", title: "All", count: 0)])
    }

    @Test("rows: frequency, who, stations and messages, the slice's letter, @N, and a quiet room's last talker")
    func rows() throws {
        let now: Int64 = 10_000_000
        let rooms = [Self.summary("lsb-7236400", 7_236_400, "talking", talkers: ["K4ABC"], members: 6, messages: 4),
                     Self.summary("lsb-7185000", 7_185_000, "quiet", talkers: ["W2ABC"], members: 2,
                                  last: now - 18 * 60_000),
                     Self.summary("lsb-7200000", 7_200_000, "doubled", talkers: ["N4XYZ"], members: 1, messages: 1),
                     Self.summary("lsb-7212000", 7_212_000, "unknown", members: 4),
                     Self.summary("usb-14250000", 14_250_000, "talking", talkers: ["K1ABC"], band: "20m")]
        let rows = RoundtableToolsList.rows(rooms, chip: "40m", sliceRooms: [0: "lsb-7236400", 1: "usb-14250000"],
                                            letters: [0: "A", 1: "B"], mentionCounts: ["lsb-7236400": 1], now: now)
        try #require(rows.count == 4)
        #expect(rows.map(\.id) == ["lsb-7185000", "lsb-7200000", "lsb-7212000", "lsb-7236400"])
        #expect(rows[0].who == "Quiet" && rows[0].sub == "last W2ABC, 18 min ago" && rows[0].quiet)
        #expect(rows[1].who == "Double: N4XYZ" && rows[1].sub == "1 station \u{00B7} 1 message")
        #expect(rows[2].who == "Unknown talker" && rows[2].sub == "4 stations")
        #expect(rows[3] == RoundtableToolsList.Row(id: "lsb-7236400", hz: 7_236_400, frequency: "7.236",
                                                   glyph: .talking, who: "K4ABC",
                                                   sub: "6 stations \u{00B7} 4 messages", letters: ["A"],
                                                   mentions: 1, quiet: false))
        #expect(rows[3].mine && !rows[0].mine)
        let all = RoundtableToolsList.rows(rooms, chip: "all", sliceRooms: [:], letters: [:], mentionCounts: [:],
                                           now: now)
        #expect(all.count == 5 && all.last?.id == "usb-14250000")
        #expect(RoundtableToolsList.note(sliceLetter: "A").hasPrefix("A tap on a row reads that frequency; slice A"))
    }

    @Test("Tools lists Roundtable after FreeDV Reporter, greyed with the Core's reason, once")
    func toolsRow() {
        let entries = StationToolList.entries(tools: nil, connected: false, olderCore: false)
        let ids = StationToolList.withRoundtable(entries, reason: nil).map(\.id)
        let reporter = ids.firstIndex(of: "freedvReporter")
        #expect(reporter.map { ids[$0 + 1] } == "roundtable")
        #expect(ids.filter { $0 == "roundtable" }.count == 1)
        let open = StationToolList.withRoundtable(entries, reason: nil).first { $0.id == "roundtable" }
        #expect(open?.title == "Roundtable" && open?.detail == StationToolList.roundtableDetail && open?.enabled == true)
        #expect(open?.page == .roundtable && open?.tag == .core)
        let greyed = StationToolList.withRoundtable(entries, reason: RoundtableModel.notConnectedReason)
            .first { $0.id == "roundtable" }
        #expect(greyed?.enabled == false && greyed?.detail == RoundtableModel.notConnectedReason)
        // A Core that lists Roundtable among its tools still shows one row, this app's.
        let listed = entries + [StationToolList.Entry(id: "roundtable", title: "Roundtable", detail: "x", tag: .core,
                                                      page: nil, reason: "x")]
        let once = StationToolList.withRoundtable(listed, reason: nil)
        #expect(once.filter { $0.id == "roundtable" }.count == 1)
        #expect(once.first { $0.id == "roundtable" }?.page == .roundtable)
        // With no FreeDV Reporter, after Spot Hub.
        let noReporter = entries.filter { $0.id != "freedvReporter" }
        #expect(StationToolList.withRoundtable(noReporter, reason: nil).map(\.id).prefix(2) == ["spotHub", "roundtable"])
    }

    // MARK: Against a fake Core

    @Test("the Tools control starts and stops Roundtable on the Core")
    func controlStartsAndStopsCore() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await shotStation()
        try await station.deliverRoundtable(.init(rooms: [], link: .off))
        #expect(await settle { model.roundtable.linkState == .off && model.roundtable.available })
        let (window, _) = try host(RoundtableToolsPage(roundtable: model.roundtable,
                                                      slices: model.main.slices, open: { _ in }))
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        var button: NSObject?
        #expect(await settle(seconds: 3) {
            button = SetupTypedEntryTests.element("roundtableTools.control", in: window)
            return button != nil
        })
        let control = try #require(button)
        #expect(control.accessibilityLabel == "Start")
        #expect(control.accessibilityActivate())
        #expect(await settle { model.roundtable.linkState == .live })
        #expect(await settle {
            SetupTypedEntryTests.element("roundtableTools.control", in: window)?.accessibilityLabel == "Stop"
        })
        #expect(SetupTypedEntryTests.element("roundtableTools.control", in: window)?.accessibilityActivate() == true)
        #expect(await settle { model.roundtable.linkState == .off })
        let commands = station.messages.compactMap(AccessoryPagesTests.invoke)
            .filter { $0.verb == "spots.connect" || $0.verb == "spots.disconnect" }
        #expect(commands.map(\.verb) == ["spots.connect", "spots.disconnect"])
        #expect(commands.allSatisfy { $0.args == [.init(name: "source", value: .utf8("roundtable"))] })
    }

    @Test("starting from Tools offers the existing first-run choice here without going live")
    func startOffersFirstRunHere() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await shotStation()
        try await station.deliverRoundtable(.init(rooms: [], link: .off, firstRunAnswered: false))
        #expect(await settle { model.roundtable.available && model.roundtable.linkState == .off })
        let flow = Self.flow(model, station)
        let (window, _) = try host(ToolsTab(app: model, flow: flow, spots: model.spots, route: [.roundtable]))
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        var control: NSObject?
        #expect(await settle {
            control = SetupTypedEntryTests.element("roundtableTools.control", in: window)
            return control != nil
        })
        #expect(try #require(control).accessibilityActivate())
        #expect(await settle { model.roundtable.askingFirstRun })
        var stayHidden: NSObject?
        #expect(await settle {
            stayHidden = SetupTypedEntryTests.element("roundtableStayHidden", in: window)
            return stayHidden != nil
        })
        #expect(!station.roundtableAnswered.contains("answerFirstRun"))
        #expect(try #require(stayHidden).accessibilityActivate())
        #expect(await settle { model.roundtable.firstRunAnswered && model.roundtable.hidden })
        #expect(station.roundtableAnswered.filter { $0 == "answerFirstRun" }.count == 1)
        let answer = station.messages.compactMap(AccessoryPagesTests.invoke).last { $0.verb == "roundtable.answerFirstRun" }
        #expect(answer?.args == [.init(name: "live", value: .bool(false))])
        #expect(await settle { SetupTypedEntryTests.element("roundtableStayHidden", in: window) == nil })
    }

    @Test("a Tools first-run form closes with its Core and does not follow a replacement session")
    func firstRunFormRetiresWithCore() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await shotStation()
        try await station.deliverRoundtable(.init(rooms: [], link: .off, firstRunAnswered: false))
        #expect(await settle { model.roundtable.available && model.roundtable.linkState == .off })
        let (window, _) = try host(RoundtableToolsPage(roundtable: model.roundtable,
                                                      slices: model.main.slices, open: { _ in }))
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        var control: NSObject?
        #expect(await settle {
            control = SetupTypedEntryTests.element("roundtableTools.control", in: window)
            return control != nil
        })
        #expect(try #require(control).accessibilityActivate())
        #expect(await settle { SetupTypedEntryTests.element("roundtableStayHidden", in: window) != nil })
        await model.disconnect()
        #expect(await settle {
            !model.roundtable.connected && SetupTypedEntryTests.element("roundtableStayHidden", in: window) == nil
        })
        let replacement = try FakeStation(additions: [.roundtable])
        await model.connect(to: replacement.endpoint, trust: replacement.trust, authenticator: replacement.authenticator,
                            transportFactory: replacement.transportFactory)
        #expect(await replacement.waitUntilLive())
        try await replacement.deliverRoundtable(.init(rooms: [], link: .live, firstRunAnswered: false))
        #expect(await settle {
            model.roundtable.askingFirstRun
                && SetupTypedEntryTests.element("roundtableFirstRunChoose", in: window) != nil
        })
        #expect(SetupTypedEntryTests.element("roundtableStayHidden", in: window) == nil)
        #expect(!station.roundtableAnswered.contains("answerFirstRun"))
        #expect(!replacement.roundtableAnswered.contains("answerFirstRun"))
        await model.disconnect()
    }

    @Test("a row on the Tools page reads its room and opens it; no slice moves")
    func aRowReadsAndNeverTunes() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await shotStation()
        try await station.deliverRoundtable(Self.bandScene())
        #expect(await settle { model.roundtable.rooms.count == Self.bandRooms.count })
        let before = try #require(model.main.slices.active?.slice.frequencyHz)
        let flow = Self.flow(model, station)
        let (window, _) = try host(ToolsTab(app: model, flow: flow, spots: model.spots, route: [.roundtable]))
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        let id = "usb-7250000"
        var row: NSObject?
        #expect(await settle { row = SetupTypedEntryTests.element("roundtableTools.row.\(id)", in: window); return row != nil })
        #expect(row?.accessibilityActivate() == true)
        // The room page is open: the list's row is gone and the room's read bar is there. Not found by
        // "roundtableTools.room": that identifier sits on a ScrollView, and once an earlier suite has walked
        // a hosted ScrollView's accessibility tree (BandButtonColumnTests) with app accessibility switched on
        // mid-run, SwiftUI's platform scroll views report no identifier for the rest of the process.
        #expect(await settle {
            SetupTypedEntryTests.element("roundtableTools.row.\(id)", in: window) == nil
                && SetupTypedEntryTests.element("roundtableBackToSlice", in: window) != nil
        })
        #expect(await settle { station.roundtableReads == [id] })
        #expect(await settle { model.roundtable.room?.id == id })
        try await Task.sleep(for: .milliseconds(300))
        #expect(model.main.slices.active?.slice.frequencyHz == before)
        #expect(model.roundtable.sliceRooms[0] == Self.sliceRoom)
        await model.disconnect()
    }

    @Test("a tag tunes on tap, +N lists its rooms and a row there tunes, and keyed nothing tunes")
    func tagsTuneAndFold() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await shotStation()
        try await station.deliverRoundtable(Self.bandScene())
        #expect(await settle { model.roundtable.rooms.count == Self.bandRooms.count })
        let tuned = Tuned()
        let layer = { (keyed: Bool) in
            RoundtableTagLayer(roundtable: model.roundtable, sliceIds: [0], geometryWidth: 402,
                               lowHz: BandFlagShotTests.centre - BandFlagShotTests.span / 2,
                               hzPerPoint: BandFlagShotTests.span / 402, top: 300, tickFoot: 330, flagRects: [],
                               keyed: keyed, sliceLetter: "A") { room in tuned.ids.append(room.id) }
                .frame(width: 402, height: 874)
                .background(Color.black)
        }
        do {
            let (window, _) = try host(layer(false))
            defer {
                window.isHidden = true
                window.rootViewController = nil
            }
            await ShotWait.laidOut(window)
            let tag = try #require(SetupTypedEntryTests.element("roundtableTag.usb-7250000", in: window))
            #expect(tag.accessibilityActivate())
            #expect(tuned.ids == ["usb-7250000"])
            let fold = try #require(SetupTypedEntryTests.element("roundtableFold", in: window))
            #expect(fold.accessibilityActivate())
            #expect(await settle { SetupTypedEntryTests.element("roundtableFoldList", in: window) != nil })
            let row = try #require(SetupTypedEntryTests.element("roundtableFoldRow.lsb-7258100", in: window))
            #expect(row.accessibilityActivate())
            #expect(tuned.ids == ["usb-7250000", "lsb-7258100"])
            #expect(await settle { SetupTypedEntryTests.element("roundtableFoldList", in: window) == nil })
        }
        do {
            let (window, _) = try host(layer(true))
            defer {
                window.isHidden = true
                window.rootViewController = nil
            }
            await ShotWait.laidOut(window)
            let tag = try #require(SetupTypedEntryTests.element("roundtableTag.usb-7250000", in: window))
            _ = tag.accessibilityActivate()
            #expect(tuned.ids == ["usb-7250000", "lsb-7258100"])
        }
        await model.disconnect()
    }

    // MARK: Shots

    @Test("shots: the Tools list, the Roundtable page, a room read, and the crowded band")
    func shots() async throws {
        let (model, station) = try await shotStation()
        model.phoneSettings.roundtableFirstRunNoteSeen = true
        try await station.deliverRoundtable(Self.bandScene())
        #expect(await settle { model.roundtable.rooms.count == Self.bandRooms.count && model.roundtable.room != nil })
        let flow = Self.flow(model, station)
        try await shootPage("roundtable-tools-entry", model: model) {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [])
        }
        try await shootPage("roundtable-tools-list", model: model) {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [.roundtable])
        }
        _ = await model.roundtable.read("usb-7250000")
        #expect(await settle { model.roundtable.room?.id == "usb-7250000" })
        try await shootPage("roundtable-tools-room", model: model) {
            ToolsTab(app: model, flow: flow, spots: model.spots,
                     route: [.roundtable, .roundtableRoom(id: "usb-7250000", title: "7.250.000 USB")])
        }
        await model.roundtable.backToSlice()
        try await shootBand("roundtable-band-tags", model: model)
        await model.disconnect()
    }

    @Test("card accessibility format and read actions never tune, including while keyed")
    func reportCardActions() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await shotStation(frequency: true)
        var scene = Self.bandScene()
        var summary = FakeStation.roundtableSummary(id: Self.sliceRoom, members: 2, state: "talking", talkers: ["K4ABC"])
        summary["stations"] = .array(["K4ABC", "N4XYZ"].map { call in .object([
            "ref": .string(call), "sourceRoom": .string(Self.sliceRoom), "slice": .number(0),
            "hz": .number(7_236_400), "family": .string("lsb"), "callsign": .string(call),
            "nth": .number(1), "grid": .string("EM73"), "keyed": .bool(false),
            "away": .bool(false), "joining": .bool(false),
        ]) })
        scene.roomRecords[Self.sliceRoom] = scene.roomRecords[Self.sliceRoom]?.map { original in
            var record = original
            if record.fields["record"] == .string("member") || record.fields["record"] == .string("reading") {
                record.fields["sourceRoom"] = .string(Self.sliceRoom)
            }
            if record.fields["record"] == .string("turn") {
                record.fields["sourceRoom"] = .string(Self.sliceRoom)
                record.fields["hz"] = .number(7_236_400)
            }
            if case .array(let talkers)? = record.fields["talkers"] {
                record.fields["talkers"] = .array(talkers.map { value in
                    guard case .object(var talker) = value else { return value }
                    talker["sourceRoom"] = .string(Self.sliceRoom)
                    return .object(talker)
                })
            }
            return record
        }
        model.roundtable.activeSliceId = 0
        scene.rooms = [summary]
        scene.frequencyPresences = [0: Self.sliceRoom]
        try await station.deliverRoundtable(scene)
        #expect(await settle { model.roundtable.frequencyRows.first?.stations.count == 2 })
        #expect(await settle { model.roundtable.room?.now.first?.hz == 7_236_400 })
        let tuned = Tuned(), read = Tuned()
        let layer = RoundtableTagLayer(roundtable: model.roundtable, sliceIds: [0], geometryWidth: 402,
            lowHz: 7_220_000, hzPerPoint: 100, top: 200, tickFoot: 230, flagRects: [],
            keyed: true, sliceLetter: "A", readReports: { read.ids.append($0) }) { room in tuned.ids.append(room.id) }
        let (window, _) = try host(layer.frame(width: 402, height: 400))
        defer { window.isHidden = true; window.rootViewController = nil }
        await ShotWait.laidOut(window)
        let tag = try #require(SetupTypedEntryTests.element("roundtableTag.\(Self.sliceRoom)", in: window))
        await MainQueue.drained()
        let messages = station.messages.count
        let initialFrame = tag.accessibilityFrame
        #expect(SetupTypedEntryTests.perform("Change signal display", on: tag))
        #expect(model.roundtable.signalFormat == .sUnits)
        await ShotWait.laidOut(window)
        // A native screenshot needs a completed TimelineView update, beyond
        // the accessibility action's immediate layout transaction.
        let sLabel = SetupTypedEntryTests.element("roundtableTag.\(Self.sliceRoom)", in: window)?.accessibilityLabel
        try #require(await settle {
            let label = SetupTypedEntryTests.element("roundtableTag.\(Self.sliceRoom)", in: window)?.accessibilityLabel
            return label != nil && label != sLabel && label?.contains("Signal display: S-units") == true
        })
        await ShotWait.laidOut(window)
        try write("roundtable-overlay-card-s-units", window)
        #expect(SetupTypedEntryTests.perform("Change signal display", on: tag))
        #expect(model.roundtable.signalFormat == .both)
        await ShotWait.laidOut(window)
        let bothTag = try #require(SetupTypedEntryTests.element("roundtableTag.\(Self.sliceRoom)", in: window))
        // S and dBm share one line (D11): nothing shifts when the format changes (D14).
        #expect(bothTag.accessibilityFrame == initialFrame)
        #expect(bothTag.accessibilityLabel?.contains("N4XYZ heard K4ABC") == true)
        #expect(bothTag.accessibilityLabel?.contains("-43 dBm") == true)
        try write("roundtable-overlay-card-both", window)
        #expect(SetupTypedEntryTests.perform("Read reports", on: tag))
        #expect(read.ids == [Self.sliceRoom] && tuned.ids.isEmpty)
        #expect(station.messages.count == messages)
        #expect(tag.accessibilityLabel?.contains("K4ABC") == true)
        #expect(tag.accessibilityLabel?.contains("N4XYZ") == true)
        #expect(tag.accessibilityFrame.width <= 177 && tag.accessibilityFrame.height > 40)
        await model.disconnect()
    }

    /// The active frequency's scene for the card's pictures, with a double, a
    /// talker and a quiet frequency elsewhere on the band, and a frequency slice B's flag speaks for.
    private static func cardScene(double: Bool, link: FakeStation.RoundtableLink = .live,
                                  wideNames: Bool = false) -> FakeStation.RoundtableScene {
        let now = Int64(Date().timeIntervalSince1970 * 1000)
        let source = sliceRoom
        let listeners: [(String, String, Double, Int64, Double, Int64, Bool)] = [
            ("n/2", "N4RDR", 179, 39, -67, 1_000, false), ("y/4", "KG4VCF", 212, 135, -73, 2_000, true),
            ("d/5", "KD8LMN", 496, 12, -78, 3_000, false), ("a/6", "AA4QQ", 565, 50, -110, 1_000, false),
            ("s/7", "N0SPM", 872, 333, -78, 26_000, false), ("w/8", "W5ABC", 901, 281, -88, 2_000, false),
        ]
        var records: [LinkMessage.RecordBatch.Record] = []
        func member(_ ref: String, _ call: String, km: Double?, keyed: Bool, you: Bool, seq: Int) {
            records.append(.init(id: "member:\(source)/\(ref)", fields: [
                "record": .string("member"), "ref": .string(ref), "sourceRoom": .string(source), "hz": .number(7_236_400),
                "callsign": .string(call), "nth": .number(1), "grid": .string("EM73"), "km": km.map(LinkJSON.number) ?? .null,
                "offsetHz": .number(0), "joining": .bool(false), "away": .bool(false), "keyed": .bool(keyed),
                "software": .string("NereusSDR"), "softwareVersion": .string("26.10.3"), "you": .bool(you),
                "seq": .number(Double(seq)),
            ]))
        }
        let talkers = wideNames ? [("k/1", "VE3WIDE/MM"), ("n/9", "KG4WIDE/QRP")]
            : double ? [("k/1", "K4ABC"), ("n/9", "N4XYZ")] : [("k/1", "K4ABC")]
        for (index, talker) in talkers.enumerated() {
            member(talker.0, talker.1, km: nil, keyed: true, you: false, seq: index)
        }
        for (index, listener) in listeners.enumerated() {
            member(listener.0, listener.1, km: listener.2, keyed: false, you: listener.6, seq: 10 + index)
            var reading = FakeStation.roundtableReading(ref: listener.0, dbm: listener.4, t: now - listener.5, turn: 12)
            reading.id = "reading:\(source)/\(listener.0)"
            reading.fields["sourceRoom"] = .string(source)
            records.append(reading)
        }
        records.append(.init(id: "turn:12", fields: [
            "record": .string("turn"), "id": .number(12), "kind": .string(double ? "doubled" : "talking"),
            "talkers": .array(talkers.map { .object(["ref": .string($0.0), "callsign": .string($0.1), "nth": .number(1),
                                                     "hz": .number(7_236_400), "sourceRoom": .string(source)]) }),
            "start": .number(Double(now - 83_000)), "end": .null, "ending": .bool(false), "heard": .number(6),
            "mine": .null, "sourceRoom": .string(source), "hz": .number(7_236_400),
        ]))
        func room(_ id: String, hz: Int64, state: String, _ people: [(String, Bool)]) -> [String: LinkJSON] {
            var summary = FakeStation.roundtableSummary(id: id, members: people.count, state: state,
                                                        talkers: people.filter(\.1).map(\.0))
            summary["band"] = .string("40m")
            summary["stations"] = .array(people.map {
                FakeStation.roundtableStation(ref: $0.0, callsign: $0.0, sourceRoom: id, hz: hz, keyed: $0.1)
            })
            return summary
        }
        var mine = room(source, hz: 7_236_400, state: double ? "doubled" : "talking",
                        talkers.map { ($0.1, true) } + listeners.map { ($0.1, false) })
        mine["stations"] = .array(talkers.map {
            FakeStation.roundtableStation(ref: $0.0, callsign: $0.1, sourceRoom: source, hz: 7_236_400, keyed: true)
        } + listeners.map {
            FakeStation.roundtableStation(ref: $0.0, callsign: $0.1, sourceRoom: source, hz: 7_236_400)
        })
        var scene = FakeStation.RoundtableScene(rooms: [
            mine,
            room("lsb-7225000", hz: 7_225_000, state: "doubled", [("AA5ABC", true), ("K0ABC", true), ("W2ABC", false)]),
            room("lsb-7249000", hz: 7_249_000, state: "talking", [("K1SAID", true)]),
            room("lsb-7254000", hz: 7_254_000, state: "talking", [("W6ABC", true), ("KC2ABC", false)]),
            room("lsb-7261000", hz: 7_261_000, state: "quiet", [("W2DEF", false), ("N5ABC", false), ("K7ABC", false)]),
        ], roomRecords: [source: records], link: link, linkSince: now - 12_000, sliceRooms: [0: source])
        scene.frequencyPresences = [0: source, 1: "lsb-7249000"]
        return scene
    }

    // MARK: Viewing a frequency (overlay design D21 to D29)

    /// The frequency tapped. A phone's short pan has room for one tag, past its card.
    private static var viewedHz: Int64 { UIDevice.current.userInterfaceIdiom == .pad ? 7_225_000 : 7_249_000 }
    private static var viewedRoom: String { "lsb-\(viewedHz)" }

    /// The card's scene, plus a viewed double with readings, and messages here and elsewhere.
    private static func viewScene(double: Bool = true, link: FakeStation.RoundtableLink = .live) -> FakeStation.RoundtableScene {
        var scene = cardScene(double: double, link: link)
        let now = Int64(Date().timeIntervalSince1970 * 1000)
        let source = viewedRoom
        let viewed = Double(viewedHz)
        var summary = FakeStation.roundtableSummary(id: source, members: 3, state: "doubled", talkers: ["AA5ABC", "K0ABC"])
        summary["band"] = .string("40m")
        summary["stations"] = .array([("AA5ABC", true), ("K0ABC", true), ("W2ABC", false)].map {
            FakeStation.roundtableStation(ref: $0.0, callsign: $0.0, sourceRoom: source, hz: Int64(viewed), keyed: $0.1)
        })
        scene.rooms = scene.rooms.filter { $0["id"] != .string(source) } + [summary]
        var records: [LinkMessage.RecordBatch.Record] = []
        func member(_ ref: String, _ call: String, hz: Double, km: Double?, keyed: Bool, seq: Int) {
            records.append(.init(id: "member:\(source)/\(ref)", fields: [
                "record": .string("member"), "ref": .string(ref), "sourceRoom": .string(source), "hz": .number(hz),
                "callsign": .string(call), "nth": .number(1), "grid": .string("EM73"), "km": km.map(LinkJSON.number) ?? .null,
                "bearingDeg": .number(39), "offsetHz": .number(0), "joining": .bool(false), "away": .bool(false),
                "keyed": .bool(keyed), "software": .string("NereusSDR"), "softwareVersion": .string("26.10.3"),
                "you": .bool(false), "seq": .number(Double(seq)),
            ]))
        }
        member("a/1", "AA5ABC", hz: viewed, km: nil, keyed: true, seq: 0)
        member("k/2", "K0ABC", hz: viewed + 20, km: nil, keyed: true, seq: 1)
        member("w/3", "W2ABC", hz: viewed - 40, km: 340, keyed: false, seq: 2)
        var reading = FakeStation.roundtableReading(ref: "w/3", dbm: -71, t: now - 1_000, turn: 40)
        reading.id = "reading:\(source)/w/3"
        reading.fields["sourceRoom"] = .string(source)
        records.append(reading)
        records.append(.init(id: "turn:40", fields: [
            "record": .string("turn"), "id": .number(40), "kind": .string("doubled"),
            "talkers": .array([("a/1", "AA5ABC"), ("k/2", "K0ABC")].map {
                .object(["ref": .string($0.0), "callsign": .string($0.1), "nth": .number(1),
                         "hz": .number(viewed), "sourceRoom": .string(source)])
            }),
            "start": .number(Double(now - 95_000)), "end": .null, "ending": .bool(false), "heard": .number(1),
            "mine": .null, "sourceRoom": .string(source), "hz": .number(viewed),
        ]))
        var here = FakeStation.roundtableMessage(id: 501, t: now - 30_000, callsign: "W2ABC", text: "Good signal into Albany.")
        here.fields["hz"] = .number(viewed)
        here.fields["family"] = .string("lsb")
        records.append(here)
        scene.roomRecords[source] = records
        var elsewhere = FakeStation.roundtableMessage(id: 502, t: now - 20_000, callsign: "N4RDR", text: "Good copy into Knoxville.")
        elsewhere.fields["hz"] = .number(7_236_400)
        elsewhere.fields["family"] = .string("lsb")
        scene.roomRecords[sliceRoom, default: []].append(elsewhere)
        return scene
    }

    private func viewStation(double: Bool = true) async throws -> (AppModel, FakeStation) {
        let (model, station) = try await shotStation(frequency: true)
        model.phoneSettings.roundtableFirstRunNoteSeen = true
        model.roundtable.activeSliceId = 0
        try await station.deliverRoundtable(Self.viewScene(double: double))
        #expect(await settle { model.roundtable.room?.id == Self.sliceRoom
            && model.roundtable.cardData(context: Self.sliceRoom).rows.count == 6 })
        return (model, station)
    }

    /// Tunes 100 Hz under your own frequency's open view: it keeps its typed name and
    /// its scroll, comes up once, and only the model's one refresh for a new frequency asks the Core.
    private func tuneUnderOwnFrequency(_ model: AppModel, _ station: FakeStation, window: UIWindow, box: String,
                                       toFull: Bool) async throws {
        func element(_ id: String) -> NSObject? { SetupTypedEntryTests.element(id, in: window) }
        func activate(_ id: String) async -> Bool {
            var found: NSObject?
            let ok = await settle { found = element(id); return found != nil }
            return ok && found?.accessibilityActivate() == true
        }
        func views<V: UIView>(_ type: V.Type, in view: UIView) -> [V] {
            (view as? V).map { [$0] } ?? [] + view.subviews.flatMap { views(type, in: $0) }
        }
        #expect(await activate("roundtableTag.\(Self.sliceRoom)"))
        #expect(await settle { element(box) != nil })
        if toFull {
            #expect(await activate("roundtableHandle"))
        }
        #expect(await activate("roundtableName-77"))
        var found: UITextField?
        #expect(await settle { found = views(UITextField.self, in: window).first; return found != nil })
        let field = try #require(found)
        field.insertText("W1A")
        let sheet = try #require(element(box)).accessibilityFrame
        var scroller: UIScrollView?
        func longScroller() -> UIScrollView? {
            views(UIScrollView.self, in: window).first {
                $0.contentSize.height > $0.bounds.height + 100 && $0.convert($0.bounds, to: nil).intersects(sheet)
                    && !$0.isKind(of: UITextView.self)
            }
        }
        #expect(await settle { scroller = longScroller(); return scroller != nil })
        let scroll = try #require(scroller)
        scroll.setContentOffset(CGPoint(x: 0, y: 40), animated: false)
        try await Task.sleep(for: .milliseconds(300))

        let appears = RoundtableRoomView.appearsForTesting
        let asks = station.roundtableHistoryRequests.count
        let next = "lsb-7236500"
        await station.deliver(.delta(.init(key: "slice:0", properties: [.init(name: "frequency", value: .f64(7_236_500))])))
        #expect(await settle { model.roundtable.room?.id == next })
        try await Task.sleep(for: .milliseconds(400))
        #expect(model.roundtable.readingRoomId == nil)
        #expect(RoundtableRoomView.appearsForTesting == appears, "the tune brought the view up new")
        #expect(station.roundtableHistoryRequests.count - asks <= 1, "more than one history ask after the tune")
        #expect(views(UITextField.self, in: window).first?.text == "W1A", "the tune lost a half-typed name")
        let after = longScroller()
        #expect(after === scroll, "the tune made a new scroll view")
        #expect((after?.contentOffset.y ?? 0) > 0, "the tune lost the scroll")
    }

    /// Both frequencies carry the turn to name, as a nearby turn does, and the messages to scroll.
    private func nameableStation() async throws -> (AppModel, FakeStation) {
        let (model, station) = try await viewStation()
        let now = Int64(Date().timeIntervalSince1970 * 1000)
        let turn = FakeStation.roundtableTurn(id: 77, start: now - 120_000, end: now - 60_000)
        let messages = (0 ..< 30).map { index -> LinkMessage.RecordBatch.Record in
            var record = FakeStation.roundtableMessage(id: Int64(600 + index), t: now - 200_000 + Int64(index),
                                                       callsign: "W1AW", text: "Message \(index)")
            record.fields["hz"] = .number(7_236_400)
            record.fields["family"] = .string("lsb")
            return record
        }
        await station.deliverRoundtableRoom(Self.sliceRoom, upserts: [turn] + messages)
        await station.deliverRoundtableRoom("lsb-7236500", upserts: [turn])
        #expect(await settle { model.roundtable.receivedMessages.count >= 30 })
        return (model, station)
    }

    @Test("D27, D38: tuning under your own sheet keeps its scroll and typed name and adds no history ask")
    func tuneKeepsOwnSheetState() async throws {
        guard UIDevice.current.userInterfaceIdiom == .phone else { return }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await nameableStation()
        let (window, _) = try host(BandShotRoot(model: model))
        defer { window.isHidden = true; window.rootViewController = nil }
        _ = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        try await tuneUnderOwnFrequency(model, station, window: window, box: "roundtableSheet", toFull: true)
    }

    @Test("the real iPhone 17 band on its side: your card stands clear of the flag, its round buttons and the column")
    func sidewaysCardClearsTheRealBand() async throws {
        guard UIDevice.current.userInterfaceIdiom == .phone else { return }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, _) = try await viewStation()
        let (window, _) = try host(BandShotRoot(model: model), size: CGSize(width: 874, height: 402))
        defer { window.isHidden = true; window.rootViewController = nil }
        _ = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        func frame(_ id: String) -> CGRect? { SetupTypedEntryTests.element(id, in: window)?.accessibilityFrame }
        let tag = "roundtableTag.\(Self.sliceRoom)"
        #expect(await settle { frame(tag) != nil && frame("flagA") != nil && frame("bandColumn") != nil })
        let card = try #require(frame(tag))
        #expect(card.width > 0 && card.height > 0)
        for id in ["flagA", "flagCloseA", "flagLockA", "flagMoreA", "bandColumn"] {
            let block = try #require(frame(id), "\(id) is not on the band")
            #expect(!card.intersects(block), "\(card) meets \(id) \(block)")
        }
    }

    @Test("D38: tuning under your own panel on an iPad keeps its scroll and typed name too")
    func tuneKeepsOwnPanelState() async throws {
        guard UIDevice.current.userInterfaceIdiom == .pad else { return }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await nameableStation()
        let (window, _) = try host(ViewShotRoot(model: model, columnShown: false, wide: false),
                                   size: CGSize(width: 1376, height: 1032))
        defer { window.isHidden = true; window.rootViewController = nil }
        _ = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        try await tuneUnderOwnFrequency(model, station, window: window, box: "roundtableFrequencyPopover", toFull: false)
    }

    private final class Viewed {
        var ids: [String] = []
        var tuned: [Int64] = []
    }

    @Test("a tag's tap views on a capable server, your own as well, and a legacy server keeps today's")
    func tagTapRules() {
        typealias Layer = RoundtableTagLayer
        #expect(Layer.tagTap(roomId: "x", canView: true, activeContext: "me", hasDetails: true) == .view("x"))
        #expect(Layer.tagTap(roomId: "x", canView: true, activeContext: "me", hasDetails: false) == .view("x"))
        #expect(Layer.tagTap(roomId: "me", canView: true, activeContext: "me", hasDetails: false) == .view("me"))
        #expect(Layer.tagTap(roomId: "x", canView: false, activeContext: "me", hasDetails: true) == .legacyDetails("x"))
        #expect(Layer.tagTap(roomId: "x", canView: false, activeContext: "me", hasDetails: false) == .tune("x"))
        #expect(Layer.foldRowTap(roomId: "x", canView: true) == .view("x"))
        #expect(Layer.foldRowTap(roomId: "x", canView: false) == .tune("x"))
    }

    @Test("the same target closes, another swaps, and the iPad's column returns to your own and stays")
    func toggleSteps() {
        typealias Screen = MainScreen
        let viewed = Screen.RoundtableTarget.viewed("x")
        let other = Screen.RoundtableTarget.viewed("y")
        #expect(Screen.roundtableStep(target: .own, shown: nil, inColumn: false) == .show(.own))
        #expect(Screen.roundtableStep(target: viewed, shown: nil, inColumn: false) == .show(viewed))
        #expect(Screen.roundtableStep(target: .own, shown: .own, inColumn: false) == .close)
        #expect(Screen.roundtableStep(target: viewed, shown: viewed, inColumn: false) == .close)
        #expect(Screen.roundtableStep(target: other, shown: viewed, inColumn: false) == .show(other))
        #expect(Screen.roundtableStep(target: .own, shown: viewed, inColumn: false) == .show(.own))
        #expect(Screen.roundtableStep(target: viewed, shown: .own, inColumn: false) == .show(viewed))
        #expect(Screen.roundtableStep(target: viewed, shown: viewed, inColumn: true) == .show(.own))
        #expect(Screen.roundtableStep(target: .own, shown: .own, inColumn: true) == .show(.own))
        #expect(Screen.roundtableStep(target: other, shown: viewed, inColumn: true) == .show(other))
    }

    @Test("a hosted tag's tap reads and no slice moves; your own card asks for your own and reads nothing")
    func hostedTagViews() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await viewStation()
        let before = model.main.slices.active?.slice.frequencyHz
        let viewed = Viewed(), legacy = Tuned()
        let layer = RoundtableTagLayer(roundtable: model.roundtable, sliceIds: [0], geometryWidth: 402,
            lowHz: BandFlagShotTests.centre - BandFlagShotTests.span / 2, hzPerPoint: BandFlagShotTests.span / 402,
            top: 300, tickFoot: 330, flagRects: [], keyed: false, sliceLetter: "A", compact: true, readReports: { _ in },
            viewFrequency: { viewed.ids.append($0) }) { room in legacy.ids.append(room.id) }
        let (window, _) = try host(layer.frame(width: 402, height: 400).background(Color.black))
        defer { window.isHidden = true; window.rootViewController = nil }
        await ShotWait.laidOut(window)
        var tag: NSObject?
        #expect(await settle { tag = SetupTypedEntryTests.element("roundtableTag.lsb-7225000", in: window); return tag != nil })
        let sent = station.messages.count
        #expect(tag?.accessibilityActivate() == true)
        #expect(viewed.ids == ["lsb-7225000"])
        var card: NSObject?
        #expect(await settle { card = SetupTypedEntryTests.element("roundtableTag.\(Self.sliceRoom)", in: window); return card != nil })
        #expect(card?.accessibilityActivate() == true)
        #expect(viewed.ids == ["lsb-7225000", Self.sliceRoom])
        #expect(legacy.ids.isEmpty)
        #expect(model.main.slices.active?.slice.frequencyHz == before)
        #expect(station.messages.count == sent)
        await model.disconnect()
    }

    /// The real main screen on a phone: tap, look, Back, Tune; the slice moves only on Tune.
    @Test("on a phone a tag opens the sheet, Back stays up on your own, the same tag closes, and Tune tunes and closes")
    func phoneViewFlow() async throws {
        guard UIDevice.current.userInterfaceIdiom == .phone else { return }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, _) = try await viewStation()
        let (window, _) = try host(BandShotRoot(model: model))
        defer { window.isHidden = true; window.rootViewController = nil }
        _ = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let before = try #require(model.main.slices.active?.slice.frequencyHz)
        func activate(_ id: String) async -> Bool {
            var found: NSObject?
            let ok = await settle { found = SetupTypedEntryTests.element(id, in: window); return found != nil }
            return ok && found?.accessibilityActivate() == true
        }
        #expect(await activate("roundtableTag.\(Self.viewedRoom)"))
        #expect(await settle { model.roundtable.readingRoomId == Self.viewedRoom })
        #expect(await settle { SetupTypedEntryTests.element("roundtableViewHead", in: window) != nil })
        #expect(model.main.slices.active?.slice.frequencyHz == before)
        // D22: read only, and only this frequency's messages.
        #expect(await settle { model.roundtable.room?.messages.map(\.id) == [501] })
        // Back to your own, in place: the sheet stays up, on your frequency.
        #expect(await activate("roundtableBackToSlice"))
        #expect(await settle { model.roundtable.readingRoomId == nil })
        #expect(await settle { SetupTypedEntryTests.element("roundtableViewTuneReason", in: window) != nil })
        #expect(SetupTypedEntryTests.element("roundtableSheet", in: window) != nil)
        #expect(SetupTypedEntryTests.element("roundtableViewHead", in: window) != nil)
        #expect(model.main.slices.active?.slice.frequencyHz == before)
        // The same tag twice: opens, then closes.
        #expect(await activate("roundtableTag.\(Self.viewedRoom)"))
        #expect(await settle { model.roundtable.readingRoomId == Self.viewedRoom })
        #expect(await activate("roundtableTag.\(Self.viewedRoom)"))
        #expect(await settle { model.roundtable.readingRoomId == nil })
        #expect(await settle { SetupTypedEntryTests.element("roundtableSheet", in: window) == nil })
        #expect(model.main.slices.active?.slice.frequencyHz == before)
        #expect(await activate("roundtableTag.\(Self.viewedRoom)"))
        #expect(await settle { SetupTypedEntryTests.element("roundtableViewTune", in: window) != nil })
        #expect(await settle { model.roundtable.readingRoomId == Self.viewedRoom })
        #expect(await activate("roundtableViewTune"))
        #expect(await settle { model.main.slices.active?.slice.frequencyHz == Double(Self.viewedHz) })
        #expect(await settle { model.roundtable.readingRoomId == nil })
        #expect(await settle { SetupTypedEntryTests.element("roundtableViewHead", in: window) == nil })
        await model.disconnect()
    }

    /// Spec D38 and the rule that a tap never tunes: the slice's frequency and mode hold
    /// across a toggle, a swap and Back, and a swap keeps the height the sheet has.
    @Test("one sheet on a phone: your card opens it with Tune greyed and the box kept; a swap keeps its height")
    func phoneOneSheet() async throws {
        guard UIDevice.current.userInterfaceIdiom == .phone else { return }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await viewStation()
        let (window, _) = try host(BandShotRoot(model: model))
        defer { window.isHidden = true; window.rootViewController = nil }
        _ = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let hz = try #require(model.main.slices.active?.slice.frequencyHz)
        let mode = try #require(model.main.slices.active?.modeLabel)
        func unmoved() -> Bool {
            model.main.slices.active?.slice.frequencyHz == hz && model.main.slices.active?.modeLabel == mode
        }
        func activate(_ id: String) async -> Bool {
            var found: NSObject?
            let ok = await settle { found = SetupTypedEntryTests.element(id, in: window); return found != nil }
            return ok && found?.accessibilityActivate() == true
        }
        func exists(_ id: String) async -> Bool {
            await settle { SetupTypedEntryTests.element(id, in: window) != nil }
        }
        func sheetTop() -> CGFloat {
            SetupTypedEntryTests.element("roundtableSheet", in: window)?.accessibilityFrame.minY ?? -1
        }
        let writes = station.messages.count
        let own = "roundtableTag.\(Self.sliceRoom)", tag = "roundtableTag.\(Self.viewedRoom)"
        // Your own card opens your own frequency: the same view, Tune greyed with the reason, and the box.
        #expect(await activate(own))
        #expect(await exists("roundtableViewHead"))
        #expect(await exists("roundtableViewTuneReason"))
        #expect(await exists("roundtableBox"))
        #expect(await exists("roundtableMore"))
        #expect(model.roundtable.readingRoomId == nil)
        let tune = try #require(SetupTypedEntryTests.element("roundtableViewTune", in: window))
        #expect(tune.accessibilityTraits.contains(.notEnabled))
        #expect(tune.accessibilityActivate() == false || unmoved())
        #expect(unmoved())
        // The same card closes it.
        #expect(await activate(own))
        #expect(await settle { SetupTypedEntryTests.element("roundtableSheet", in: window) == nil })
        // Full height, then a swap each way: the height stays.
        #expect(await activate(own))
        #expect(await exists("roundtableHandle"))
        #expect(await activate("roundtableHandle"))
        await ShotWait.laidOut(window)
        #expect(await settle { sheetTop() >= 0 })
        try await Task.sleep(for: .milliseconds(400))
        let full = sheetTop()
        #expect(await activate(tag))
        #expect(await settle { model.roundtable.readingRoomId == Self.viewedRoom })
        try await Task.sleep(for: .milliseconds(400))
        #expect(abs(sheetTop() - full) < 2, "\(sheetTop()) was \(full)")
        #expect(model.roundtable.lostReads == 0)
        #expect(unmoved())
        #expect(await activate("roundtableBackToSlice"))
        #expect(await settle { model.roundtable.readingRoomId == nil })
        try await Task.sleep(for: .milliseconds(400))
        #expect(abs(sheetTop() - full) < 2, "\(sheetTop()) was \(full)")
        #expect(SetupTypedEntryTests.element("roundtableSheet", in: window) != nil)
        #expect(unmoved())
        // Nothing wrote to the slice: the frequency's reads are the only traffic.
        let sliceWrites = station.messages.dropFirst(writes).filter { "\($0)".contains("slice:0") }
        #expect(sliceWrites.isEmpty, "\(sliceWrites)")
        await model.disconnect()
    }

    /// D24, D38 on an iPad: the panel with the column hidden, the applet with it.
    @Test("an iPad's panel takes your own frequency and toggles; its column views and returns, in the applet")
    func iPadOneInspector() async throws {
        guard UIDevice.current.userInterfaceIdiom == .pad else { return }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, _) = try await viewStation()
        let hz = try #require(model.main.slices.active?.slice.frequencyHz)
        let mode = try #require(model.main.slices.active?.modeLabel)
        func unmoved() -> Bool {
            model.main.slices.active?.slice.frequencyHz == hz && model.main.slices.active?.modeLabel == mode
        }
        let size = CGSize(width: 1376, height: 1032)
        let own = "roundtableTag.\(Self.sliceRoom)", tag = "roundtableTag.\(Self.viewedRoom)"
        do {
            let (window, _) = try host(ViewShotRoot(model: model, columnShown: false, wide: false), size: size)
            defer { window.isHidden = true; window.rootViewController = nil }
            _ = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
            func activate(_ id: String) async -> Bool {
                var found: NSObject?
                let ok = await settle { found = SetupTypedEntryTests.element(id, in: window); return found != nil }
                return ok && found?.accessibilityActivate() == true
            }
            func panel() -> NSObject? { SetupTypedEntryTests.element("roundtableFrequencyPopover", in: window) }
            #expect(await activate(own))
            #expect(await settle { panel() != nil })
            #expect(SetupTypedEntryTests.element("roundtableSheet", in: window) == nil)
            #expect(await settle { SetupTypedEntryTests.element("roundtableViewTuneReason", in: window) != nil })
            #expect(await settle { SetupTypedEntryTests.element("roundtableBox", in: window) != nil })
            #expect(await settle { SetupTypedEntryTests.element("roundtableMore", in: window) != nil })
            // Your card again closes; a tag swaps in; Back stays up; the close button closes.
            #expect(await activate(own))
            #expect(await settle { panel() == nil })
            #expect(await activate(own))
            #expect(await settle { panel() != nil })
            #expect(await activate(tag))
            #expect(await settle { model.roundtable.readingRoomId == Self.viewedRoom })
            #expect(panel() != nil)
            #expect(await activate("roundtableBackToSlice"))
            #expect(await settle { model.roundtable.readingRoomId == nil })
            try await Task.sleep(for: .milliseconds(300))
            #expect(panel() != nil)
            #expect(await activate(tag))
            #expect(await settle { model.roundtable.readingRoomId == Self.viewedRoom })
            #expect(await activate(tag))
            #expect(await settle { model.roundtable.readingRoomId == nil })
            #expect(await settle { panel() == nil })
            #expect(await activate(own))
            #expect(await settle { panel() != nil })
            #expect(await activate("roundtableViewClose"))
            #expect(await settle { panel() == nil })
            #expect(unmoved())
        }
        do {
            let (window, _) = try host(ViewShotRoot(model: model, columnShown: true, wide: false), size: size)
            defer { window.isHidden = true; window.rootViewController = nil }
            _ = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
            func activate(_ id: String) async -> Bool {
                var found: NSObject?
                let ok = await settle { found = SetupTypedEntryTests.element(id, in: window); return found != nil }
                return ok && found?.accessibilityActivate() == true
            }
            #expect(await activate(tag))
            #expect(await settle { model.roundtable.readingRoomId == Self.viewedRoom })
            // The same tag returns to your own, in the applet, with no sheet or panel.
            #expect(await activate(tag))
            #expect(await settle { model.roundtable.readingRoomId == nil })
            #expect(SetupTypedEntryTests.element("roundtableSheet", in: window) == nil)
            #expect(SetupTypedEntryTests.element("roundtableFrequencyPopover", in: window) == nil)
            #expect(await settle { SetupTypedEntryTests.element("roundtableViewTuneReason", in: window) != nil })
            #expect(await activate(tag))
            #expect(await settle { model.roundtable.readingRoomId == Self.viewedRoom })
            // Your own card returns as well.
            #expect(await activate(own))
            #expect(await settle { model.roundtable.readingRoomId == nil })
            #expect(SetupTypedEntryTests.element("roundtableSheet", in: window) == nil)
            #expect(unmoved())
        }
        await model.disconnect()
    }

    @Test("at half height Tune is pinned above the composer, on the screen, however long the readings")
    func tuneIsPinnedAtHalfHeight() async throws {
        guard UIDevice.current.userInterfaceIdiom == .phone else { return }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, _) = try await viewStation()
        let (window, _) = try host(BandShotRoot(model: model))
        defer { window.isHidden = true; window.rootViewController = nil }
        _ = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let tag = try await requireElement("roundtableTag.\(Self.viewedRoom)", in: window)
        #expect(tag.accessibilityActivate())
        _ = try await requireElement("roundtableViewHead", in: window)
        // Shorter, so half height leaves the readings far less than they need.
        window.frame.size.height = 640
        window.layoutIfNeeded()
        let tune = try await requireElement("roundtableViewTune", in: window)
        let composer = try await requireElement("roundtableBox", in: window)
        let head = try await requireElement("roundtableViewHead", in: window)
        try await Task.sleep(for: .milliseconds(500))
        func frame(_ element: NSObject) -> CGRect { (element as? UIAccessibilityElement)?.accessibilityFrame ?? element.accessibilityFrame }
        let tuneFrame = frame(tune), composerFrame = frame(composer), headFrame = frame(head)
        #expect(tuneFrame.maxY <= composerFrame.minY + 1, "Tune \(tuneFrame) composer \(composerFrame)")
        #expect(tuneFrame.minY >= headFrame.minY, "Tune \(tuneFrame) head \(headFrame)")
        #expect(window.bounds.contains(tuneFrame))
        await model.disconnect()
    }

    @Test("another slice's flag line views its frequency and leaves the active slice; your own opens your room")
    func flagLines() async throws {
        // An iPad's slice B has a full flag, with its line; a phone's short pan folds it.
        guard UIDevice.current.userInterfaceIdiom == .pad else { return }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, _) = try await viewStation()
        let (window, _) = try host(ViewShotRoot(model: model, columnShown: true, wide: false),
                                   size: CGSize(width: 1376, height: 1032))
        defer { window.isHidden = true; window.rootViewController = nil }
        _ = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let active = model.main.slices.activeSliceId
        var line: NSObject?
        #expect(await settle { line = SetupTypedEntryTests.element("roundtableLineB", in: window); return line != nil })
        #expect(line?.accessibilityActivate() == true)
        #expect(await settle { model.roundtable.readingRoomId == "lsb-7249000" })
        #expect(model.main.slices.activeSliceId == active)
        line = nil
        #expect(await settle { line = SetupTypedEntryTests.element("roundtableLineA", in: window); return line != nil })
        #expect(line?.accessibilityActivate() == true)
        #expect(await settle { model.roundtable.readingRoomId == nil })
        #expect(model.main.slices.activeSliceId == active)
        await model.disconnect()
    }

    @Test("a band tune leaves the view open, landing on it makes it your own, and a lost read closes it with the usual status line")
    func viewEnds() async throws {
        guard UIDevice.current.userInterfaceIdiom == .phone else { return }
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await viewStation()
        let (window, _) = try host(BandShotRoot(model: model))
        defer { window.isHidden = true; window.rootViewController = nil }
        _ = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        var tag: NSObject?
        #expect(await settle { tag = SetupTypedEntryTests.element("roundtableTag.\(Self.viewedRoom)", in: window); return tag != nil })
        #expect(tag?.accessibilityActivate() == true)
        #expect(await settle { model.roundtable.isViewing })
        // D27, D38: a band drag tunes your slice elsewhere. The view stays on what it shows.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_240_000)),
        ])))
        #expect(await settle { model.main.slices.active?.slice.frequencyHz == 7_240_000 })
        try await Task.sleep(for: .milliseconds(500))
        #expect(model.roundtable.readingRoomId == Self.viewedRoom)
        #expect(SetupTypedEntryTests.element("roundtableSheet", in: window) != nil)
        // Your slice lands on the viewed frequency: the sheet becomes your own, in place.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(Double(Self.viewedHz))),
        ])))
        #expect(await settle { !model.roundtable.isViewing })
        #expect(await settle { SetupTypedEntryTests.element("roundtableViewTuneReason", in: window) != nil })
        #expect(SetupTypedEntryTests.element("roundtableSheet", in: window) != nil)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(7_236_400)),
        ])))
        #expect(await settle { model.roundtable.activeContext?.id == Self.sliceRoom
            && model.main.slices.active?.slice.frequencyHz == 7_236_400 })
        await ShotWait.laidOut(window)
        tag = nil
        #expect(await settle { tag = SetupTypedEntryTests.element("roundtableTag.\(Self.viewedRoom)", in: window); return tag != nil })
        #expect(tag?.accessibilityActivate() == true)
        #expect(await settle { model.roundtable.isViewing })
        // The server stops offering frequency views.
        await station.deliverRoundtableFrequencyCapability(false)
        #expect(await settle { model.roundtable.readingRoomId == nil })
        #expect(model.roundtable.lostReads == 1)
        #expect(await settle { SetupTypedEntryTests.element("roundtableViewHead", in: window) == nil })
        #expect(await settle { SetupTypedEntryTests.element("roundtableSheet", in: window) == nil })
        await model.disconnect()
    }

    @Test("Widen's rows say Double with its names, and a row reads and never tunes")
    func wideRows() throws {
        let base = RoundtableToolsList.Row(id: "lsb-7225000", hz: 7_225_000, frequency: "7.225", glyph: .doubled, who: "",
                                           sub: "3 stations", letters: [], mentions: 0, quiet: false)
        let double = RoundtableToolsList.wideRow(base, keyedNames: ["AA5ABC", "K0ABC"])
        #expect(double.who == "Double" && double.sub == "AA5ABC + K0ABC")
        var mine = base
        mine.glyph = .talking
        mine.letters = ["A"]
        let talking = RoundtableToolsList.wideRow(mine, keyedNames: ["K4ABC"])
        #expect(talking.who == "K4ABC" && talking.sub == "3 stations \u{00B7} you")
        var quiet = base
        quiet.glyph = .quiet
        #expect(RoundtableToolsList.wideRow(quiet, keyedNames: []).who == "Quiet")
        quiet.glyph = .unknown
        #expect(RoundtableToolsList.wideRow(quiet, keyedNames: []).who == "Unknown talker")
    }

    @Test("Your pan: the peak in the sideband window, nil off pan, before a frame and while transmitting")
    func panLevel() throws {
        let band = BandModel()
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        let hz: Int64 = 7_240_000
        #expect(band.panLevel(hz: hz, family: "lsb") == nil) // before a frame
        let samples = 1_206
        let coverage = BandCoverage(centerHz: BandFlagShotTests.centre, spanHz: BandFlagShotTests.span)
        var trace = [Float](repeating: -120, count: samples)
        func put(_ at: Double, _ dbm: Float) {
            if let index = coverage.sample(forHz: at, samples: samples) { trace[index] = dbm }
        }
        put(Double(hz) - 1_500, -60) // lower sideband's window
        put(Double(hz) + 1_500, -50) // upper sideband's window
        put(Double(hz) + 8_000, -20) // in neither
        band.receive(.displayFrame(DisplayFrame(endpointId: 1, contextGeneration: 1, encoderSequence: 1,
                                                producerTimestamp: 1, isKeyframe: true, waterfallAdvance: true,
                                                minDbm: -160, maxDbm: 0, traceDbm: trace, waterfallDbm: trace, wideDbm: [])))
        #expect(band.panLevel(hz: hz, family: "lsb") == -60)
        #expect(band.panLevel(hz: hz, family: "usb") == -50)
        #expect(band.panLevel(hz: hz, family: "am") == -120) // 1.35 kHz each side misses both
        #expect(band.panLevel(hz: 14_200_000, family: "usb") == nil) // off this pan
        band.transmit.keyedHere = true
        #expect(band.panLevel(hz: hz, family: "lsb") == nil)
    }

    @Test("the view's words: offsets, the path, the head, and Your pan only for now")
    func viewWords() {
        typealias Words = RoundtableFrequencyText
        #expect(Words.sub(hz: 7_225_000, family: "lsb", sliceHz: 7_236_400, sliceLetter: "A")
                == "Viewing, not tuned \u{00B7} \u{2212}11.4 kHz from slice A \u{00B7} LSB")
        #expect(Words.sub(hz: 7_225_000, family: "lsb", sliceHz: 7_225_000, sliceLetter: "A")
                == "Viewing, not tuned \u{00B7} LSB")
        #expect(Words.alreadyHere(sliceLetter: "A") == "Slice A is already here")
        #expect(Words.offset(0) == "on freq" && Words.offset(20) == "+20 Hz" && Words.offset(-40) == "\u{2212}40 Hz")
        #expect(Words.path(km: 340, bearing: 39, units: "mi", showBearing: true) == "211 mi 039\u{00B0}")
        #expect(Words.path(km: 340, bearing: 39, units: "km", showBearing: false) == "340 km")
        #expect(Words.path(km: nil, bearing: nil, units: "mi", showBearing: true).isEmpty)
        var card = RoundtableCardData(hz: 7_225_000)
        #expect(Words.head(card) == "Nobody talking")
        card.kind = .lastTalker
        #expect(Words.head(card) == "Nobody talking")
        card.kind = .unknown
        #expect(Words.head(card) == "Unknown signal")
        #expect(!Words.showsPath(card))
        card.kind = .double
        #expect(!Words.showsPath(card))
        // Your pan comes first when someone is on the air.
        #expect(Words.rows(card, panDbm: -70, panPeak: -68).first?.label == "Your pan")
        #expect(Words.rows(card, panDbm: nil, panPeak: nil).isEmpty)
        card.kind = .quiet
        #expect(Words.rows(card, panDbm: -70, panPeak: -68).isEmpty)
        card.kind = .double
        card.frozen = true
        #expect(Words.rows(card, panDbm: -70, panPeak: -68).isEmpty)
        // Tune.
        func can(hz: Int64 = 1, family: String = "lsb", sliceHz: Int64? = 2, sliceFamily: String? = "lsb",
                 keyed: Bool = false, tunable: Bool = true, linkUp: Bool = true) -> Bool {
            Words.canTune(hz: hz, family: family, sliceHz: sliceHz, sliceFamily: sliceFamily, keyed: keyed,
                          tunable: tunable, linkUp: linkUp)
        }
        #expect(can())
        #expect(!can(keyed: true))
        #expect(!can(sliceHz: 1))
        #expect(!can(tunable: false))
        #expect(!can(linkUp: false))
        // The same frequency in another family is not "already there".
        #expect(can(sliceHz: 1, sliceFamily: "usb"))
        #expect(!can(sliceHz: nil, sliceFamily: nil))
        // A FreeDV view writes no mode: from any family on this frequency, nothing to do.
        #expect(!can(family: "fdvu", sliceHz: 1, sliceFamily: "usb"))
        #expect(!can(family: "fdvl", sliceHz: 1, sliceFamily: "lsb"))
        #expect(!can(family: "fdvl", sliceHz: 1, sliceFamily: "fdvl"))
        #expect(can(family: "fdvu", sliceHz: 2, sliceFamily: "usb"))
        // With no mode to reach the family, off.
        #expect(!Words.canTune(hz: 1, family: "lsb", sliceHz: 2, sliceFamily: "usb", keyed: false, tunable: true,
                               linkUp: true, familyReachable: false))
    }

    @Test("a RADE slice has a frequency context, and Tune is off when it is already there")
    func radeSliceHasAContext() async throws {
        let (model, station) = try await viewStation()
        for (mode, family) in [(Int64(14), "fdvu"), (Int64(15), "fdvl")] {
            await station.deliver(.delta(LinkMessage.Delta(key: "slice:1", properties: [
                .init(ordinal: 2, name: "dspMode", value: .enumeration(mode)),
            ])))
            #expect(await settle { model.roundtable.sliceContexts[1]?.family == family })
        }
        let context = try #require(model.roundtable.sliceContexts[1])
        let entry = try #require(model.main.slices.entries.first { $0.id == 1 })
        var data = RoundtableCardData(hz: context.hz)
        data.family = context.family
        #expect(!RoundtableFrequencyHost.canTune(data, active: entry, keyed: false, tunable: true, linkUp: true,
                                                 modes: model.main.slices.catalog?.offeredModes ?? []))
        await model.disconnect()
    }

    @Test("an unviewed slice's flag names who is on the air within 5 kHz, through the model")
    func unviewedSliceFlagFollowsTheNearestKeyed() async throws {
        let (model, station) = try await viewStation()
        let context = try #require(model.roundtable.sliceContexts[1])
        #expect(model.roundtable.view(of: context.id) == nil)
        var scene = Self.viewScene()
        let source = "lsb-\(context.hz + 3_000)"
        var summary = FakeStation.roundtableSummary(id: source, members: 1, state: "talking", talkers: ["W9XYZ"])
        summary["stations"] = .array([FakeStation.roundtableStation(ref: "x/1", callsign: "W9XYZ", sourceRoom: source,
                                                                    hz: context.hz + 3_000, keyed: true)])
        // Only this neighbor: on iPad the scene's own rooms sit nearer slice B.
        scene.rooms = [summary]
        scene.roomRecords = [:]
        try await station.deliverRoundtable(scene)
        #expect(await settle { RoundtableFlagLine.line(model.roundtable, sliceId: 1)?.text.hasSuffix("W9XYZ") == true })
        await model.disconnect()
    }

    /// Set NEREUS_ROUNDTABLE_VIEW_SHOTS to a folder to run this and write its pictures there.
    @Test("shots: a view before its stream beside a double, and a FreeDV view with Tune off",
          .enabled(if: !(ProcessInfo.processInfo.environment["NEREUS_ROUNDTABLE_VIEW_SHOTS"] ?? "").isEmpty))
    func secondRoundViewShots() async throws {
        let directory = try #require(ProcessInfo.processInfo.environment["NEREUS_ROUNDTABLE_VIEW_SHOTS"])
        func station(_ ref: String, _ source: String, _ hz: Int64, _ keyed: Bool) -> RoundtableModel.Station {
            .init(ref: ref, sourceRoom: source, slice: 0, hz: hz, family: "lsb", callsign: ref, nth: 1, keyed: keyed,
                  away: false, joining: false)
        }
        func summary(_ id: String, _ hz: Int64, _ stations: [RoundtableModel.Station]) -> RoundtableModel.RoomSummary {
            .init(id: id, band: "40m", family: "lsb", hz: hz, state: "quiet", talkers: [], members: Int64(stations.count),
                  messages: 0, last: 0, folded: false, heard: 0, readings: [], stations: stations)
        }
        let directoryStations = [station("W2QQQ", "lsb-7236000", 7_236_000, false)]
        let doubled = [station("W1AW", "lsb-7239000", 7_239_000, true), station("K4ABC", "lsb-7239000", 7_239_040, true)]
        let rows = RoundtableModel.frequencyRows(of: [summary("lsb-7236000", 7_236_000, directoryStations),
                                                      summary("lsb-7239000", 7_239_000, doubled)])
        var pending = RoundtableTagLayout.cardData(summary: try #require(rows.first { $0.hz == 7_236_000 }), view: nil, now: 0,
                                                   link: .live, linkSince: 0, directory: directoryStations + doubled, rows: rows)
        pending.family = "lsb"
        #expect(pending.kind == .double)
        var freedv = pending
        freedv.family = "fdvu"
        let cannot = !RoundtableFrequencyText.canTune(hz: 7_236_000, family: "fdvu", sliceHz: 7_236_000, sliceFamily: "usb",
                                                      keyed: false, tunable: true, linkUp: true)
        #expect(cannot)
        for (name, card, canTune, sliceHz) in [("pending-next-to-double", pending, true, Int64(7_224_600)),
                                               ("freedv-same-frequency", freedv, false, Int64(7_236_000))] {
            let view = RoundtableFrequencyView(data: card, viewing: true, canTune: canTune, sliceHz: sliceHz, sliceLetter: "A",
                                               format: .dbm, units: "mi", showBearing: true, panDbm: nil, panPeak: nil,
                                               reconnecting: false, showsBack: false, onTune: {}, onBack: {})
                .background(Color.black)
            let (window, _) = try host(view, size: CGSize(width: 402, height: 420))
            defer {
                window.isHidden = true
                window.rootViewController = nil
            }
            try await Task.sleep(for: .milliseconds(600))
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            try #require(image.pngData()).write(to: URL(fileURLWithPath: directory).appendingPathComponent("view-\(name).png"))
        }
    }

    @Test("a live double's view has a clock and blank paths; a frozen one has no clock")
    func viewClockAndPaths() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        var card = RoundtableCardData(hz: 7_225_000)
        card.kind = .double
        card.keyed = ["AA5ABC", "K0ABC"]
        card.clock = 95_000
        card.family = "lsb"
        card.rows = [.init(call: "W2ABC", dbm: -71, age: 1_000, km: nil, bearing: nil, offsetHz: 0, you: false, old: false,
                           peak: -70)]
        card.onFrequency = [.init(name: "AA5ABC", keyed: true, you: false, offsetHz: 0),
                            .init(name: "K0ABC", keyed: true, you: false, offsetHz: 20)]
        func clockShown(_ data: RoundtableCardData) async throws -> Bool {
            let view = RoundtableFrequencyView(data: data, viewing: true, canTune: false, sliceHz: 7_236_400,
                                               onTune: {}, onBack: {})
            let (window, _) = try host(view.frame(width: 300).background(Color.black))
            defer { window.isHidden = true; window.rootViewController = nil }
            await ShotWait.laidOut(window)
            #expect(SetupTypedEntryTests.element("roundtableViewTune", in: window) != nil)
            let head = SetupTypedEntryTests.element("roundtableViewHead", in: window)
            return head?.accessibilityLabel?.contains("1:35") == true
        }
        #expect(try await clockShown(card))
        card.frozen = true
        card.note = "Reconnecting"
        #expect(try await !clockShown(card))
    }

    @Test("a pending view and pan card show no clock until a turn start is known")
    func pendingViewHasNoClock() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        func clockShown(_ data: RoundtableCardData) async throws -> Bool {
            let view = RoundtableFrequencyView(data: data, viewing: true, canTune: false, sliceHz: 7_236_400,
                                               onTune: {}, onBack: {})
            let (window, _) = try host(view.frame(width: 300).background(Color.black))
            defer { window.isHidden = true; window.rootViewController = nil }
            await ShotWait.laidOut(window)
            return SetupTypedEntryTests.element("roundtableViewClock", in: window) != nil
                || SetupTypedEntryTests.element("roundtableViewHead", in: window)?.accessibilityLabel?.contains("0:00") == true
        }
        // Known only from the directory: Double and Talking, with nothing to time.
        var pending = RoundtableCardData(hz: 7_225_000)
        pending.family = "lsb"
        pending.keyed = ["AA5ABC", "K0ABC"]
        pending.kind = .double
        #expect(RoundtableTagLayout.cardClockText(pending) == nil)
        #expect(try await !clockShown(pending))
        pending.kind = .talking
        pending.keyed = ["AA5ABC"]
        #expect(RoundtableTagLayout.cardClockText(pending) == nil)
        #expect(try await !clockShown(pending))
        // The turn's start arrives with the stream.
        pending.clock = 95_000
        #expect(RoundtableTagLayout.cardClockText(pending) == "1:35")
    }

    /// Set NEREUS_ROUNDTABLE_VIEW_SHOTS to a folder to run this and write its pictures there.
    @Test("shots: the frequency view on the phone sheet, the iPad applet, Widen, the popover and portrait",
          .enabled(if: !(ProcessInfo.processInfo.environment["NEREUS_ROUNDTABLE_VIEW_SHOTS"] ?? "").isEmpty))
    func viewShots() async throws {
        let directory = try #require(ProcessInfo.processInfo.environment["NEREUS_ROUNDTABLE_VIEW_SHOTS"])
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let (model, station) = try await viewStation()
        let pad = UIDevice.current.userInterfaceIdiom == .pad
        let landscape = CGSize(width: 1376, height: 1032)
        let portrait = pad ? CGSize(width: 1032, height: 1376) : CGSize(width: 402, height: 874)
        func shoot(_ name: String, size: CGSize, columnShown: Bool = true, wide: Bool = false, tag: Bool = true,
                   own: Bool = false, release: Bool = true, check: ((UIWindow) -> Void)? = nil) async throws {
            let (window, _) = try host(ViewShotRoot(model: model, columnShown: columnShown, wide: wide), size: size)
            defer {
                window.isHidden = true
                window.rootViewController = nil
            }
            if wide {
                await ShotWait.laidOut(window)
            } else {
                let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
                BandFlagShotTests.feed(model.main.band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
                try await ShotWait.requireBandShown(model.main.band, in: window, after: bandDraw)
            }
            if tag {
                if wide {
                    let row = try await requireElement("roundtableWide.row.\(Self.viewedRoom)", in: window)
                    #expect(row.accessibilityActivate())
                } else {
                    let found = try await requireElement("roundtableTag.\(Self.viewedRoom)", in: window)
                    #expect(found.accessibilityActivate())
                }
                #expect(await settle { model.roundtable.readingRoomId == Self.viewedRoom })
            }
            if own {
                let found = try await requireElement("roundtableTag.\(Self.sliceRoom)", in: window)
                #expect(found.accessibilityActivate())
            }
            try await Task.sleep(for: .milliseconds(900))
            check?(window)
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            let url = URL(fileURLWithPath: directory).appendingPathComponent("view-\(name).png")
            try #require(image.pngData()).write(to: url)
            print("View shot \(url.lastPathComponent) reading=\(model.roundtable.readingRoomId ?? "none")")
            if release {
                await model.roundtable.backToSlice()
                #expect(await settle { model.roundtable.readingRoomId == nil })
            }
        }
        if pad {
            try await shoot("ipad-applet", size: landscape) { window in
                #expect(SetupTypedEntryTests.element("roundtableFrequencyPopover", in: window) == nil)
                #expect(SetupTypedEntryTests.element("roundtableSheet", in: window) == nil)
                #expect(SetupTypedEntryTests.element("roundtableViewHead", in: window) != nil)
            }
            try await shoot("ipad-applet-own", size: landscape, tag: false)
            try await shoot("ipad-widen", size: landscape, wide: true) { window in
                #expect(SetupTypedEntryTests.element("roundtableFrequencyPopover", in: window) == nil)
            }
            try await shoot("ipad-popover", size: landscape, columnShown: false) { window in
                #expect(SetupTypedEntryTests.element("roundtableFrequencyPopover", in: window) != nil)
            }
            try await shoot("ipad-portrait", size: portrait) { window in
                #expect(SetupTypedEntryTests.element("roundtableFrequencyPopover", in: window) != nil)
            }
            try await shoot("ipad-portrait-own", size: portrait, tag: false)
            // D24: your own frequency in the panel, with the column hidden.
            try await shoot("ipad-panel-own", size: landscape, columnShown: false, tag: false, own: true) { window in
                #expect(SetupTypedEntryTests.element("roundtableFrequencyPopover", in: window) != nil)
                #expect(SetupTypedEntryTests.element("roundtableViewTuneReason", in: window) != nil)
            }
        } else {
            // The sheet opens at half height, with Tune pinned above the composer.
            try await shoot("iphone-sheet-double", size: portrait) { window in
                #expect(SetupTypedEntryTests.element("roundtableSheet", in: window) != nil)
                #expect(SetupTypedEntryTests.element("roundtableViewTune", in: window) != nil)
            }
            // One sheet: your own frequency, Tune greyed with its reason, and the box.
            try await shoot("iphone-sheet-own", size: portrait, tag: false, own: true) { window in
                #expect(SetupTypedEntryTests.element("roundtableViewTuneReason", in: window) != nil)
            }
            try await shoot("iphone-sideways-own", size: CGSize(width: 874, height: 402), tag: false, own: true)
            try await shoot("iphone-sideways-closed", size: CGSize(width: 874, height: 402), tag: false)
        }
        _ = station
        await model.disconnect()
    }

    private func requireElement(_ id: String, in window: UIWindow) async throws -> NSObject {
        var found: NSObject?
        _ = await settle { found = SetupTypedEntryTests.element(id, in: window); return found != nil }
        return try #require(found, "no \(id)")
    }

    /// Set NEREUS_ROUNDTABLE_CARD_SHOTS to a folder to run this and write its pictures there.
    @Test("shots: the card and tags on the real band",
          .enabled(if: !(ProcessInfo.processInfo.environment["NEREUS_ROUNDTABLE_CARD_SHOTS"] ?? "").isEmpty))
    func cardShots() async throws {
        let directory = try #require(ProcessInfo.processInfo.environment["NEREUS_ROUNDTABLE_CARD_SHOTS"])
        let (model, station) = try await shotStation(frequency: true)
        model.phoneSettings.roundtableFirstRunNoteSeen = true
        model.roundtable.activeSliceId = 0
        let pad = UIDevice.current.userInterfaceIdiom == .pad
        let portrait = pad ? CGSize(width: 1032, height: 1376) : CGSize(width: 402, height: 874)
        let landscape = pad ? CGSize(width: 1376, height: 1032) : CGSize(width: 874, height: 402)
        func shoot(_ name: String, size: CGSize) async throws {
            let (window, _) = try host(BandShotRoot(model: model), size: size)
            defer {
                window.isHidden = true
                window.rootViewController = nil
            }
            let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
            BandFlagShotTests.feed(model.main.band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
            try await ShotWait.requireBandShown(model.main.band, in: window, after: bandDraw)
            try await Task.sleep(for: .milliseconds(600))
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(pad ? "ipad" : "iphone")-\(name).png")
            try #require(image.pngData()).write(to: url)
            let summary = model.roundtable.cardData(context: Self.sliceRoom)
            print("Card shot \(url.lastPathComponent) kind=\(summary.kind) keyed=\(summary.keyed) rows=\(summary.rows.count) frozen=\(summary.frozen)")
        }
        try await station.deliverRoundtable(Self.cardScene(double: false))
        #expect(await settle { model.roundtable.cardData(context: Self.sliceRoom).kind == .talking
            && model.roundtable.cardData(context: Self.sliceRoom).rows.count == 6 })
        try await shoot(pad ? "landscape-talking" : "short-talking", size: pad ? landscape : portrait)
        try await station.deliverRoundtable(Self.cardScene(double: true))
        #expect(await settle { model.roundtable.cardData(context: Self.sliceRoom).kind == .double
            && model.roundtable.cardData(context: Self.sliceRoom).rows.count == 6 })
        try await shoot(pad ? "landscape-double" : "short-double", size: pad ? landscape : portrait)
        if !pad {
            // Names too wide to share a line: stacked, the double still has a reading.
            try await station.deliverRoundtable(Self.cardScene(double: true, wideNames: true))
            #expect(await settle { model.roundtable.cardData(context: Self.sliceRoom).keyed.contains("VE3WIDE/MM") })
            try await shoot("short-double-wide", size: portrait)
            try await station.deliverRoundtable(Self.cardScene(double: true))
            #expect(await settle { !model.roundtable.cardData(context: Self.sliceRoom).keyed.contains("VE3WIDE/MM") })
        }
        if pad {
            try await shoot("portrait-double", size: portrait)
        } else {
            // A taller pan: the display sheet's Spectrum height.
            model.main.display.setSpectrumHeight(65)
            try await shoot("tall-double", size: portrait)
            try await station.deliverRoundtable(Self.cardScene(double: false))
            #expect(await settle { model.roundtable.cardData(context: Self.sliceRoom).kind == .talking })
            try await shoot("tall-talking", size: portrait)
            model.main.display.setSpectrumHeight(40)
            try await station.deliverRoundtable(Self.cardScene(double: true))
            try await shoot("sideways-double", size: landscape)
        }
        try await station.deliverRoundtable(Self.cardScene(double: true, link: .reconnecting))
        #expect(await settle { model.roundtable.cardData(context: Self.sliceRoom).frozen })
        try await shoot(pad ? "landscape-frozen" : "short-frozen", size: pad ? landscape : portrait)
        await model.disconnect()
    }

    @Test("canonical frequency capability renders quiet people and omits empty rooms")
    func canonicalFrequencyShots() async throws {
        let (model, station) = try await shotStation(frequency: true)
        model.phoneSettings.roundtableFirstRunNoteSeen = true
        var scene = RoundtableSheetTests.boardScene()
        scene.rooms = [(Self.sliceRoom, "KG4VCF", Int64(7_236_400), 1, false),
                       ("lsb-7240000", "W4KZO", Int64(7_242_800), 0, true)].map { id, call, hz, members, joining in
            var summary = FakeStation.roundtableSummary(id: id, members: members, state: "quiet", talkers: [])
            summary["stations"] = .array([.object([
                "ref": .string(call), "sourceRoom": .string(id), "slice": .number(0),
                "hz": .number(Double(hz)), "family": .string("lsb"), "callsign": .string(call),
                "nth": .number(1), "grid": .string("EM73"), "keyed": .bool(false),
                "away": .bool(false), "joining": .bool(joining),
            ])])
            return summary
        }
        scene.rooms.append(FakeStation.roundtableSummary(id: "lsb-7250000", members: 0,
                                                          state: "quiet", talkers: []))
        scene.frequencyPresences = [0: Self.sliceRoom]
        try await station.deliverRoundtable(scene)
        // Use the producer's literal record contract independently of the helper.
        await station.deliver(.recordBatch(.init(stream: "roundtableRooms", generation: 1, reset: false,
            upserts: [.init(id: "frequency-capability", fields: ["record": .string("frequency"), "version": .number(1)])],
            removes: [])))
        try #require(await settle { model.roundtable.frequencyAvailable && model.roundtable.rooms.count == 3 })
        #expect(model.roundtable.frequencyRows.map(\.members) == [1, 1])
        #expect(RoundtableTagLayout.tags(model.roundtable, sliceIds: [0]).map(\.label) == ["KG4VCF", "W4KZO"])
        let flow = Self.flow(model, station)
        try await shootPage("roundtable-canonical-people-tools-portrait", model: model) {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [.roundtable])
        }
        try await shootPage("roundtable-canonical-people-tools-landscape", model: model,
                            size: CGSize(width: 874, height: 402)) {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [.roundtable])
        }
        try await shootBand("roundtable-canonical-people-band-portrait", model: model)
        try await shootBand("roundtable-canonical-people-band-landscape", model: model,
                            size: CGSize(width: 874, height: 402))
        await model.disconnect()
    }

    // MARK: Helpers

    static let sliceRoom = "lsb-7236400"

    /// The crowded band of the board (2d): slice A's room, a quiet one, a
    /// doubled one, five 50 Hz apart that fold, and an unknown talker.
    static let bandRooms: [(id: String, state: String, talkers: [String], members: Int, messages: Int)] = [
        ("lsb-7228000", "quiet", [], 2, 0),
        (sliceRoom, "talking", ["K4ABC"], 5, 2),
        ("usb-7250000", "doubled", ["N4XYZ", "K4QRS"], 4, 1),
        ("lsb-7258000", "talking", ["W1AW"], 3, 0),
        ("lsb-7258050", "talking", ["K1ABC"], 2, 0),
        ("lsb-7258100", "unknown", [], 3, 0),
        ("lsb-7258150", "talking", ["N0CALL"], 2, 3),
        ("lsb-7258200", "talking", ["W9XYZ"], 6, 0),
        ("usb-7264000", "unknown", [], 3, 0),
    ]

    static func bandScene() -> FakeStation.RoundtableScene {
        var scene = RoundtableSheetTests.boardScene()
        let now = Int64(Date().timeIntervalSince1970 * 1000)
        scene.rooms = bandRooms.map { room in
            var summary = FakeStation.roundtableSummary(id: room.id, members: room.members, state: room.state,
                                                        talkers: room.talkers, messages: room.messages,
                                                        last: now - 12 * 60_000)
            summary["band"] = .string("40m")
            return summary
        }
        scene.roomRecords["usb-7250000"] = [FakeStation.roundtableMember(ref: "n/9", callsign: "N4XYZ", seq: 0),
                                            FakeStation.roundtableMember(ref: "q/8", callsign: "K4QRS", seq: 1)]
        scene.mentionCounts = [sliceRoom: 1, "usb-7250000": 1]
        return scene
    }

    private func connected(additions: FakeStation.Additions) async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "RoundtableToolsPageTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        return (model, station)
    }

    /// A fake Core with Roundtable, the main screen's two slices and a band to draw them on.
    private func shotStation(frequency: Bool = false) async throws -> (AppModel, FakeStation) {
        let (model, station) = try await connected(additions: frequency ? [.remoteTx, .roundtableFrequency] : [.remoteTx, .roundtable])
        try await MainScreenShotTests.fill(station)
        #expect(await settle { model.main.slices.entries.count == 2 })
        model.main.band.endpointId = 1
        model.main.band.receive(.context(try #require(BandFlagShotTests.context())))
        return (model, station)
    }

    static func flow(_ model: AppModel, _ station: FakeStation) -> ConnectionFlow {
        ConnectionFlow(app: model, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()),
            stations: PairedStationStore(item: InMemorySecretItem()), kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    private func host<Content: View>(_ content: Content,
                                     size: CGSize = CGSize(width: 402, height: 874)) throws -> (UIWindow, UIView) {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let host = ShotHosting.host(content.preferredColorScheme(.dark))
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        return (window, host.view)
    }

    private func write(_ name: String, _ window: UIWindow) throws {
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    private func shootPage<Content: View>(_ name: String, model: AppModel,
                                          size: CGSize = CGSize(width: 402, height: 874),
                                          @ViewBuilder content: () -> Content) async throws {
        let root = VStack(spacing: 0) {
            content()
            TabBar(selection: .constant(.tools), sideways: false)
        }
        .background(ChromeColours.page)
        let (window, _) = try host(root, size: size)
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        try await Task.sleep(for: .milliseconds(300))
        try write(name, window)
    }

    private func shootBand(_ name: String, model: AppModel,
                           size: CGSize = CGSize(width: 402, height: 874)) async throws {
        let (window, _) = try host(BandShotRoot(model: model), size: size)
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        try await Task.sleep(for: .milliseconds(300))
        try write(name, window)
    }
}

/// The rooms a tag or fold row tuned to, in order.
@MainActor
private final class Tuned {
    var ids: [String] = []
}

/// The app's root as `RootView` lays it out on the Panadapter tab.
private struct BandShotRoot: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}

private struct ViewShotRoot: View {
    @ObservedObject var model: AppModel
    let columnShown: Bool
    let wide: Bool

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, columnShown: columnShown, roundtableWide: wide)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
