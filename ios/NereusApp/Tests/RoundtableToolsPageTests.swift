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

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
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
                                         members: 9, mine: true, mentionsMe: false))
        #expect(tags[1].label == "Doubled" && tags[1].glyph == .doubled)
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
            (.off, .hidden), (.connecting, .hidden), (.live, .live), (.reconnecting, .live), (.hidden, .dimmed),
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
        #expect(RoundtableTagLayout.foldHeading(count: 5, sliceLetter: "A") == "5 rooms here \u{00B7} a tap tunes slice A")
        #expect(RoundtableTagLayout.foldWho(RoundtableTag(roomId: "x", hz: 1, glyph: .unknown, label: "Unknown"))
                == "Unknown talker")
        #expect(RoundtableTagLayout.foldWho(Self.tagAt("K4ABC", 1)) == "K4ABC")
    }

    @Test("one width for every tag, the widest label's, between 56 and 128 points")
    func tagWidths() {
        #expect(RoundtableTagLayout.tagWidth([]) == RoundtableTagLayout.minWidth)
        #expect(RoundtableTagLayout.tagWidth([Self.tagAt("Q", 1)]) == RoundtableTagLayout.minWidth)
        let long = RoundtableTag(roomId: "x", hz: 1, label: String(repeating: "W", count: 40), members: 12)
        #expect(RoundtableTagLayout.tagWidth([long]) == RoundtableTagLayout.maxWidth)
        let mid = RoundtableTagLayout.tagWidth([RoundtableTag(roomId: "x", hz: 1, label: "KG4VCF", members: 9)])
        #expect(mid > RoundtableTagLayout.minWidth && mid < RoundtableTagLayout.maxWidth)
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
        #expect(rows[1].who == "Doubled: N4XYZ" && rows[1].sub == "1 station \u{00B7} 1 message")
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
    private func shotStation() async throws -> (AppModel, FakeStation) {
        let (model, station) = try await connected(additions: [.remoteTx, .roundtable])
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
                                          @ViewBuilder content: () -> Content) async throws {
        let root = VStack(spacing: 0) {
            content()
            TabBar(selection: .constant(.tools), sideways: false)
        }
        .background(ChromeColours.page)
        let (window, _) = try host(root)
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        try await Task.sleep(for: .milliseconds(300))
        try write(name, window)
    }

    private func shootBand(_ name: String, model: AppModel) async throws {
        let size = CGSize(width: 402, height: 874)
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
