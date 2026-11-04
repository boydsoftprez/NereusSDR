// NereusSDR for iOS: exact-frequency presence remains independent of loaded RF reports.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-30: Bounded presence-stack acceptance correction.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-11-03: Overlay redesign truth rules and card data (design D1 to D9).
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import Testing
import UIKit
@testable import NereusSDR

@Suite("Roundtable presence stack")
struct RoundtablePresenceStackTests {
    private var tag: RoundtableTag {
        RoundtableTag(roomId: "frequency:lsb-7236400", hz: 7_236_400,
            label: "W1AW\nK4ABC (2)\nN3XYZ\nW4KZO\nKG4VCF", card: .init(
                state: .known("W4KZO"), reports: [
                    .init(id: "a/n", reporter: "N4XYZ", reading: .init(dbm: -43, t: 1000, present: true, turn: 12), age: "4 s", stale: false),
                    .init(id: "a/k", reporter: "K9XYZ", reading: .init(dbm: -61, t: 3000, present: true, turn: 12), age: "2 s", stale: false),
                ]), members: 5)
    }

    @Test("active reports take priority and disclose omitted rows")
    func reportsTakePriority() {
        var value = tag
        value.card?.reports.append(.init(id: "a/z", reporter: "W3XYZ", reading: .init(dbm: -70, t: 3000, present: true, turn: 12), age: "2 s", stale: false))
        let plan = RoundtableTagLayout.cardPlan(value, width: 176, maxRows: 9)
        #expect(plan.stateTitle == "W4KZO")
        #expect(!plan.reports.isEmpty)
        #expect(plan.omittedReports == 3 - plan.reports.count)
        #expect(plan.people.isEmpty)
    }

    @Test("a narrow card discloses reports without splitting its state into words")
    func narrowCardDisclosesReports() {
        var value = tag
        value.card?.state = .unknown
        value.card?.reports = []
        for (width, innerWidth) in [(76.33333333333333, 68.33333333333333), (80.0, 72.0), (100.0, 87.0)] {
            let plan = RoundtableTagLayout.cardPlan(value, width: width, maxRows: 18)
            #expect(plan.stateTitle == "Unknown signal")
            #expect(plan.stateCaption == nil && plan.reports.isEmpty)
            #expect(plan.showsDetails)
            #expect(plan.height <= 105)
            let text = ("Unknown signal" as NSString).size(withAttributes: [
                .font: UIFont.systemFont(ofSize: plan.stateFontSize, weight: .medium)]).width
            #expect(text <= innerWidth, "The complete state must fit inside the actual card padding")
        }
    }

    @Test("long portable identities and suffixes retain readable text and their full accessible identity")
    func longIdentityKeepsReadableFloor() throws {
        let identity = "EA8/KG4VCF/MM/QRP (2)"
        var value = tag
        value.card?.state = .known(identity)
        for width in [80.0, 100.0] {
            let plan = RoundtableTagLayout.cardPlan(value, width: width)
            #expect(plan.stateFontSize >= 8 && plan.stateFontSize <= 10)
            #expect(plan.stateTitle == identity)
            #expect(value.card?.accessibilityText.contains(identity) == true)
        }
        let floorWidth = (identity as NSString).size(withAttributes: [
            .font: UIFont.systemFont(ofSize: 8, weight: .medium)]).width
        #expect(floorWidth > 72, "This identity needs honest ellipsis at 80 points, not smaller text")
        value.card?.state = .unattributed
        #expect(RoundtableTagLayout.cardPlan(value, width: 80).stateFontSize >= 8)
    }

    @Test("ended unknown and combined observations say last heard without changing measurement ages")
    func endedObservationIsVisiblyHistorical() {
        var value = tag
        value.card?.state = .unknown
        let live = RoundtableTagLayout.cardPlan(value, width: 176)
        value.card?.ended = true
        let ended = RoundtableTagLayout.cardPlan(value, width: 176)
        #expect(live.stateCaption == nil && ended.stateCaption == "Last heard")
        #expect(ended.stateTitle == "Unknown signal")
        #expect(ended.reports.map(\.age) == ["4 s", "2 s"])
        #expect(value.card?.accessibilityText.contains("Turn ended") == true)
        value.card?.state = .known("W4KZO")
        #expect(RoundtableTagLayout.cardPlan(value, width: 80).stateCaption == "Last talker")
        value.card?.state = .doubled
        value.card?.reports = []
        let doubled = RoundtableTagLayout.cardPlan(value, width: 80)
        #expect(doubled.stateCaption == "Last double" && doubled.stateTitle == "Double")
        let captionWidth = ("Last double" as NSString).size(withAttributes: [
            .font: UIFont.systemFont(ofSize: 8)]).width
        #expect(captionWidth <= 67, "Historical combined state must remain readable in the 80-point strip")
    }

    @Test("loaded and unloaded reports preserve the same capped exact-frequency people")
    func loadedReportsKeepPresence() {
        let loaded = RoundtableTagLayout.cardPlan(tag, width: 80, maxRows: 18)
        var unloaded = tag
        unloaded.card = .init()
        let waiting = RoundtableTagLayout.cardPlan(unloaded, width: 80, maxRows: 18)
        #expect(loaded.people.isEmpty && loaded.showsDetails)
        #expect(waiting.people == ["W1AW", "K4ABC (2)"] && waiting.omittedPeople == 3)
        #expect(waiting.reports.isEmpty && waiting.showsDetails)
        #expect(loaded.stateTitle == "W4KZO")
        #expect(waiting.stateTitle == nil)
    }

    @Test("short legal height keeps whole rows and explicit reports disclosure")
    func shortHeightUsesHonestDetails() {
        for limit in 0...18 {
            let plan = RoundtableTagLayout.cardPlan(tag, width: 176, maxRows: limit)
            #expect(plan.height <= CGFloat(limit * 13 + 8))
            if limit >= 9 {
                #expect(plan.people.isEmpty && plan.showsDetails)
            }
            if !plan.reports.isEmpty {
                #expect(plan.stateTitle == "W4KZO")
                #expect(plan.reports.first?.reporter == "N4XYZ")
            }
        }
    }

    @Test("unknown and doubled signal descriptions do not replace presence or infer a known talker")
    func unattributedSignalsKeepPresence() {
        for state in [RoundtableTagCard.State.unknown, .doubled] {
            var value = tag
            value.card?.state = state
            value.card?.reports = []
            let plan = RoundtableTagLayout.cardPlan(value, width: 176, maxRows: 18)
            #expect(plan.people.isEmpty)
            #expect(plan.stateCaption != "Last talker" && plan.reports.isEmpty)
            #expect(plan.stateTitle == (state == .unknown ? "Unknown signal" : "Double"))
        }
    }

    @Test("Both retains aligned semantic reporter rows and enough height for both signal lines")
    func selectedSignalFitsWholeRows() {
        var value = tag
        let dbm = RoundtableTagLayout.cardPlan(value, width: 176)
        #expect(dbm.reports.map(\.signal) == [["-43"], ["-61"]])
        value.card?.format = .sUnits
        let sUnits = RoundtableTagLayout.cardPlan(value, width: 176)
        #expect(sUnits.reports.map(\.signal) == [["S9 +30"], ["S9 +12"]])
        value.card?.format = .both
        let both = RoundtableTagLayout.cardPlan(value, width: 176)
        #expect(both.reports.map(\.signal) == [["S9 +30", "-43 dBm"], ["S9 +12", "-61 dBm"]])
        #expect(both.reports.map(\.reporter) == ["N4XYZ", "K9XYZ"])
        #expect(both.reports.map(\.age) == ["4 s", "2 s"])
        #expect(both.reportHeight == 30 && both.height == dbm.height + 24)
        let constrained = RoundtableTagLayout.cardPlan(value, width: 176, maxRows: 14)
        #expect(constrained.reports.count == 2 && constrained.reports.first?.signal.count == 2)
        #expect(constrained.height <= 190)
    }

    private func overlapMember(_ ref: String, hz: Int64 = 7_236_400, keyed: Bool = false,
                               source: String = "lsb-7236000", turn: Int64? = 12) -> RoundtableModel.Member {
        .init(ref: ref, callsign: ref.uppercased(), nth: 1, offsetHz: 0, joining: false,
              away: false, keyed: keyed, software: "NereusSDR", softwareVersion: "", you: false,
              reading: .init(dbm: -73, t: 1000, present: true, turn: turn), hz: hz, sourceRoom: source)
    }

    private var overlapSummary: RoundtableModel.RoomSummary {
        .init(id: "lsb-7236400", band: "40m", family: "lsb", hz: 7_236_400,
              state: "talking", talkers: [], members: 1, messages: 0, last: 1000, folded: false,
              heard: 0, readings: [], stations: [
                .init(ref: "a", sourceRoom: "lsb-7236000", slice: 0, hz: 7_236_400, family: "lsb",
                      callsign: "A", nth: 1, keyed: true, away: false, joining: false),
              ])
    }

    private var overlapRoom: RoundtableModel.Room {
        .init(id: "lsb-7236400", band: "40m", family: "lsb", hz: 7_236_400,
              members: [overlapMember("a", keyed: true), overlapMember("b", hz: 7_236_700, keyed: true),
                        overlapMember("c", keyed: true), overlapMember("listener")],
              now: [.init(id: 12, kind: "doubled", talkers: [
                .init(ref: "a", callsign: "A", nth: 1, hz: 7_236_400, sourceRoom: "lsb-7236000"),
                .init(ref: "b", callsign: "B", nth: 2, hz: 7_236_700, sourceRoom: "lsb-7236000"),
                .init(ref: "c", callsign: "C", nth: 1, hz: 7_236_400, sourceRoom: "lsb-7236000"),
              ], start: 900, ending: false, names: [], heard: 1, hz: 7_236_000, sourceRoom: "lsb-7236000")],
              earlier: [], messages: [], frequencyView: true)
    }

    @Test("a Double at a room center belongs to its exact endpoint or explicitly associated exact listener")
    func overlapUsesExactAssociation() throws {
        var room = overlapRoom
        let endpoint = try #require(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200))
        #expect(endpoint.state == .doubled && endpoint.participants == ["A", "B (2)", "C"])
        for i in room.now[0].talkers.indices { room.now[0].talkers[i].hz = 7_236_700 }
        let listener = try #require(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200))
        #expect(listener.state == .doubled && listener.reports.map(\.reporter) == ["LISTENER"])
        // Within the window it is still the turn to show (D3); only its
        // readings say who heard it.
        for i in room.members.indices { room.members[i].reading?.turn = 999 }
        let unread = try #require(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200))
        #expect(unread.state == .doubled && unread.reports.isEmpty)
        room = overlapRoom
        room.now[0].sourceRoom = "lsb-7240000"
        let elsewhere = try #require(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200))
        #expect(elsewhere.state == .doubled && elsewhere.participants.isEmpty && elsewhere.reports.isEmpty)
        room = overlapRoom
        room.now[0].hz = 7_242_000
        for i in room.now[0].talkers.indices { room.now[0].talkers[i].hz = 7_242_000 }
        for i in room.members.indices { room.members[i].hz = 7_242_000 }
        #expect(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200)?.state == .waiting)
        room = overlapRoom
        room.family = "usb"
        #expect(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200)?.state == .waiting)
        room = overlapRoom
        room.now[0].kind = "talking"
        room.now[0].talkers = Array(room.now[0].talkers.prefix(1))
        #expect(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200)?.state == .known("A"))
        room.now[0].kind = "unknown"
        room.now[0].talkers = []
        #expect(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200)?.state == .unknown)
    }

    @Test("a live Double keeps current keyed identities and lets an unkeyed former talker report combined RF")
    func overlapThreeToTwoKeepsCurrentParticipants() throws {
        var room = overlapRoom
        room.members[2].keyed = false
        room.members.append(overlapMember("listener", source: "lsb-7240000"))
        let live = try #require(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200))
        #expect(live.state == .doubled && live.participants == ["A", "B (2)"])
        #expect(live.reports.map(\.reporter) == ["C", "LISTENER"])
        #expect(live.reports.allSatisfy { $0.id.hasPrefix("lsb-7236000/") })
        room.now[0].end = 1100
        room.earlier = room.now
        room.now = []
        for i in room.members.indices { room.members[i].keyed = false }
        let ended = try #require(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200))
        #expect(ended.ended && ended.participants == ["A", "B (2)", "C"])
        #expect(ended.reports.map(\.reporter) == ["LISTENER"])
        room.now = [.init(id: 13, kind: "talking", talkers: [room.earlier[0].talkers[0]], start: 1100,
                          ending: false, names: [], heard: 0, hz: 7_236_400, sourceRoom: "lsb-7236000")]
        let single = try #require(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200))
        #expect(single.state == .known("A") && single.reports.isEmpty)
    }

    // MARK: Overlay truth rules (overlay design D1 to D9)

    private func singleTurn(id: Int64 = 20, hz: Int64?, start: Int64 = 900, kind: String = "talking",
                            talkers: [RoundtableModel.Talker]? = nil) -> RoundtableModel.Turn {
        .init(id: id, kind: kind,
              talkers: talkers ?? (kind == "unknown" ? [] : [.init(ref: "a", callsign: "A", nth: 1, hz: hz, sourceRoom: "lsb-7236000")]),
              start: start, ending: false, names: [], heard: 1, hz: hz, sourceRoom: "lsb-7236000")
    }

    private func room(now: [RoundtableModel.Turn] = [], earlier: [RoundtableModel.Turn] = [],
                      members: [RoundtableModel.Member] = []) -> RoundtableModel.Room {
        .init(id: "lsb-7236400", band: "40m", family: "lsb", hz: 7_236_400, members: members,
              now: now, earlier: earlier, messages: [], frequencyView: true)
    }

    @Test("D3: a talker 7 Hz off the dial and an unknown off the room's center both match")
    func nearbyTurnsMatchTheCard() throws {
        let talker = room(now: [singleTurn(hz: 7_236_407)])
        #expect(try #require(RoundtableTagLayout.card(for: overlapSummary, view: talker, now: 1200)).state == .known("A"))
        let unknown = room(now: [singleTurn(hz: 7_236_000, kind: "unknown")])
        #expect(try #require(RoundtableTagLayout.card(for: overlapSummary, view: unknown, now: 1200)).state == .unknown)
    }

    @Test("D5: a departed participant is dropped from a live double and kept once it has ended")
    func departedParticipantDroppedLive() throws {
        var live = overlapRoom
        live.members.removeAll { $0.ref == "c" }
        #expect(try #require(RoundtableTagLayout.card(for: overlapSummary, view: live, now: 1200)).participants == ["A", "B (2)"])
        var ended = live
        ended.now[0].end = 1100
        ended.earlier = ended.now
        ended.now = []
        #expect(try #require(RoundtableTagLayout.card(for: overlapSummary, view: ended, now: 1200)).participants == ["A", "B (2)", "C"])
    }

    @Test("D6: a live talker has no Last talker caption, and an ended one keeps it")
    func liveTalkerNeverCaptionedAsPast() {
        var card = RoundtableTagCard(state: .known("A"))
        #expect(card.caption == nil)
        card.ended = true
        #expect(card.caption == "Last talker")
    }

    @Test("D2: a keyed station that is not a talker never appears among the listeners, and turn ids stay in their source room")
    func listenersAreNeverTransmitters() throws {
        let turn = singleTurn(id: 12, hz: 7_236_400)
        let view = room(now: [turn], members: [
            overlapMember("a", keyed: true), overlapMember("other", keyed: true), overlapMember("listener"),
            overlapMember("elsewhere", source: "lsb-7240000"),
        ])
        let card = try #require(RoundtableTagLayout.card(for: overlapSummary, view: view, now: 1200))
        #expect(card.reports.map(\.reporter) == ["LISTENER"])
    }

    @Test("D3: of two live turns the nearest shows, then the one that started last, and past 5 kHz none")
    func liveTurnChoosesNearestThenLatest() {
        let far = singleTurn(id: 1, hz: 7_238_000, start: 100)
        let near = singleTurn(id: 2, hz: 7_236_407, start: 50)
        let tied = singleTurn(id: 3, hz: 7_236_393, start: 70)
        let summary = overlapSummary
        #expect(RoundtableTagLayout.liveTurn(summary: summary, view: room(now: [far, near]))?.id == 2)
        #expect(RoundtableTagLayout.liveTurn(summary: summary, view: room(now: [near, tied]))?.id == 3)
        #expect(RoundtableTagLayout.liveTurn(summary: summary, view: room(now: [tied, near]))?.id == 3)
        let exact = singleTurn(id: 4, hz: 7_236_400, start: 10)
        #expect(RoundtableTagLayout.liveTurn(summary: summary, view: room(now: [near, tied, exact]))?.id == 4)
        #expect(RoundtableTagLayout.liveTurn(summary: summary, view: room(now: [singleTurn(hz: 7_241_400)]))?.id == 20)
        #expect(RoundtableTagLayout.liveTurn(summary: summary, view: room(now: [singleTurn(hz: 7_241_401)])) == nil)
        var usb = room(now: [near])
        usb.family = "usb"
        #expect(RoundtableTagLayout.liveTurn(summary: summary, view: usb) == nil)
        var ended = exact
        ended.end = 500
        #expect(RoundtableTagLayout.liveTurn(summary: summary, view: room(now: [ended])) == nil)
    }

    @Test("D11: the meter runs from -121 dBm to -33 dBm, clamped outside")
    func meterFractionIsClamped() {
        #expect(RoundtableTagLayout.meterFraction(-121) == 0)
        #expect(RoundtableTagLayout.meterFraction(-33) == 1)
        #expect(RoundtableTagLayout.meterFraction(-77) == 0.5)
        #expect(RoundtableTagLayout.meterFraction(-140) == 0)
        #expect(RoundtableTagLayout.meterFraction(10) == 1)
    }

    private func station(_ ref: String, source: String, hz: Int64, keyed: Bool, family: String = "lsb") -> RoundtableModel.Station {
        .init(ref: ref, sourceRoom: source, slice: 0, hz: hz, family: family, callsign: ref.uppercased(), nth: 1,
              keyed: keyed, away: false, joining: false)
    }

    private func source(_ id: String, _ stations: [RoundtableModel.Station], state: String = "quiet",
                        talkers: [String] = []) -> RoundtableModel.RoomSummary {
        .init(id: id, band: "40m", family: String(id.prefix(3)), hz: Int64(id.dropFirst(4)) ?? 0, state: state,
              talkers: talkers, members: Int64(stations.count), messages: 0, last: 0, folded: false, heard: 0,
              readings: [], stations: stations)
    }

    @Test("D1: two keyed in one source room is a double with both names; neighbors in other rooms are not")
    func directoryDoubleNeedsOneSourceRoom() throws {
        let double = RoundtableModel.frequencyRows(of: [source("lsb-7236000", [
            station("a", source: "lsb-7236000", hz: 7_236_400, keyed: true),
            station("b", source: "lsb-7236000", hz: 7_236_700, keyed: true),
            station("q", source: "lsb-7236000", hz: 7_236_900, keyed: false),
        ])])
        #expect(double.map(\.state) == ["doubled", "doubled", "doubled"])
        #expect(double[0].keyedNames == ["A", "B"] && double[1].keyedNames == ["B", "A"])

        let neighbors = RoundtableModel.frequencyRows(of: [
            source("lsb-7236000", [station("a", source: "lsb-7236000", hz: 7_236_400, keyed: true)]),
            source("lsb-7240000", [station("b", source: "lsb-7240000", hz: 7_239_000, keyed: true)]),
        ])
        #expect(neighbors.map(\.state) == ["talking", "talking"])
        #expect(neighbors.map(\.keyedNames) == [["A"], ["B"]])

        // A double in the next room within the window is not a double here.
        let nextDoor = RoundtableModel.frequencyRows(of: [
            source("lsb-7236000", [station("a", source: "lsb-7236000", hz: 7_236_400, keyed: true),
                                   station("b", source: "lsb-7236000", hz: 7_236_700, keyed: true)]),
            source("lsb-7240000", [station("c", source: "lsb-7240000", hz: 7_239_000, keyed: false)]),
        ])
        #expect(nextDoor.map(\.state) == ["doubled", "doubled", "quiet"])

        // One person's two slices are one station, never a double.
        let twoSlices = RoundtableModel.frequencyRows(of: [source("lsb-7236000", [
            station("a", source: "lsb-7236000", hz: 7_236_400, keyed: true),
            station("a", source: "lsb-7236000", hz: 7_236_500, keyed: true),
        ])])
        #expect(twoSlices.allSatisfy { $0.state == "talking" })

        // Another family in the same window says nothing about this one.
        let families = RoundtableModel.frequencyRows(of: [source("lsb-7236000", [
            station("a", source: "lsb-7236000", hz: 7_236_400, keyed: true),
            station("b", source: "lsb-7236000", hz: 7_236_400, keyed: true, family: "usb"),
        ])])
        #expect(families.allSatisfy { $0.state == "talking" })
    }

    @Test("D1: the source room's summary carries through: doubled with its names, unknown, a named talker, else quiet")
    func summaryStateCarriesThrough() {
        let doubled = RoundtableModel.frequencyRows(of: [source("lsb-7236000", [
            station("q", source: "lsb-7236000", hz: 7_236_400, keyed: false)], state: "doubled", talkers: ["K4ABC", "N3XYZ"])])
        #expect(doubled[0].state == "doubled" && doubled[0].keyedNames == ["K4ABC", "N3XYZ"])
        let unknown = RoundtableModel.frequencyRows(of: [source("lsb-7236000", [
            station("q", source: "lsb-7236000", hz: 7_236_400, keyed: false)], state: "unknown")])
        #expect(unknown[0].state == "unknown" && unknown[0].keyedNames.isEmpty)
        let talker = RoundtableModel.frequencyRows(of: [source("lsb-7236000", [
            station("a", source: "lsb-7236000", hz: 7_236_400, keyed: false),
            station("q", source: "lsb-7236000", hz: 7_236_900, keyed: false)], state: "talking", talkers: ["A"])])
        #expect(talker.map(\.state) == ["talking", "quiet"] && talker[0].keyedNames == ["A"])
    }

    @Test("a non-active double's tag and list row say Double with both names")
    @MainActor
    func nonActiveDoubleShowsBothNames() throws {
        let rows = RoundtableModel.frequencyRows(of: [source("lsb-7236000", [
            station("a", source: "lsb-7236000", hz: 7_236_400, keyed: true),
            station("b", source: "lsb-7236000", hz: 7_236_700, keyed: true),
        ])])
        let tags = RoundtableTagLayout.tags(rooms: rows, mine: [], mentionCounts: [:], room: { _ in nil })
        #expect(tags[0].glyph == .doubled && tags[0].label == "Double\nA\nB" && tags[0].card == nil)
        #expect(RoundtableTagLayout.foldWho(tags[0]) == "Double \u{00B7} A \u{00B7} B")
        let list = RoundtableToolsList.rows(rows, chip: "40m", sliceRooms: [:], letters: [:], mentionCounts: [:], now: 0)
        #expect(list[0].who == "Double: A, B")
    }

    @Test("D7: a dropped link freezes the card and its clock, while ages keep counting and no tick shows")
    func droppedLinkFreezes() {
        var turn = singleTurn(id: 12, hz: 7_236_407, start: 1_000)
        turn.talkers = [.init(ref: "a", callsign: "A", nth: 1, hz: 7_236_407, sourceRoom: "lsb-7236000")]
        let view = room(now: [turn], members: [overlapMember("a", keyed: true), overlapMember("listener")])
        let peaks = RoundtablePeakTracker()
        func data(_ link: RoundtableModel.LinkState, since: Int64 = 0) -> RoundtableCardData {
            RoundtableTagLayout.cardData(summary: overlapSummary, view: view, now: 30_000, link: link, linkSince: since, peaks: peaks)
        }
        let live = data(.live)
        #expect(!live.frozen && live.note == nil && live.clock == 29_000 && live.kind == .talking)
        #expect(live.liveHz == 7_236_407 && live.hz == 7_236_400)
        #expect(data(.hidden).note == "You are hidden" && !data(.hidden).frozen)
        let reconnecting = data(.reconnecting, since: 20_000)
        #expect(reconnecting.frozen && reconnecting.note == "Reconnecting" && reconnecting.clock == 19_000)
        #expect(reconnecting.rows[0].age == 29_000 && reconnecting.rows[0].peak == nil)
        let offline = data(.offline, since: 3_600_000)
        #expect(offline.frozen && offline.note == "Offline since 01:00 UTC")
        #expect(data(.offline).note == "Offline")
        #expect(RoundtableTagLayout.display(available: true, link: .reconnecting) == .stale)
        var frozen = RoundtableTagCard(state: .known("A"), frozen: true)
        #expect(frozen.caption == "Last known")
        frozen.state = .waiting
        #expect(frozen.caption == nil, "A card with no state line has no caption to draw")
    }

    @Test("D2: rows in a double have no distance or bearing; D13: a row past 20 seconds is old with no tick; D9: not present is filtered")
    func cardRowsFollowTheTruthRules() throws {
        var listener = overlapMember("listener")
        listener.km = 400
        listener.bearingDeg = 82
        listener.reading = .init(dbm: -60, t: 10_000, present: true, turn: 12)
        var old = overlapMember("old")
        old.km = 900
        old.reading = .init(dbm: -70, t: 5_000, present: true, turn: 12)
        var absent = overlapMember("absent")
        absent.reading = .init(dbm: -70, t: 10_000, present: false, turn: 12)
        var view = overlapRoom
        view.members = [overlapMember("a", keyed: true), overlapMember("b", hz: 7_236_700, keyed: true), listener, old, absent]
        view.now[0].talkers.removeLast()
        let double = RoundtableTagLayout.cardData(summary: overlapSummary, view: view, now: 26_000, link: .live, linkSince: 0,
                                                  peaks: RoundtablePeakTracker())
        #expect(double.kind == .double && double.keyed == ["A", "B (2)"] && double.liveHz == 7_236_400)
        #expect(double.rows.map(\.call) == ["LISTENER", "OLD"])
        #expect(double.rows.allSatisfy { $0.km == nil && $0.bearing == nil })
        #expect(double.rows[0].peak == -60 && !double.rows[0].old)
        #expect(double.rows[1].old && double.rows[1].peak == nil)

        view.now[0].kind = "talking"
        view.now[0].talkers = [view.now[0].talkers[0]]
        let single = RoundtableTagLayout.cardData(summary: overlapSummary, view: view, now: 26_000, link: .live, linkSince: 0)
        #expect(single.kind == .talking && single.rows[0].km == 400 && single.rows[0].bearing == 82 && single.rows[0].peak == nil)
    }

    @Test("D4, D5: an ended double keeps every name, then the remaining talker starts with no readings and a note for 8 seconds")
    func afterADouble() throws {
        var ended = overlapRoom
        ended.members.removeAll { $0.ref == "c" }
        ended.now[0].end = 10_000
        ended.earlier = ended.now
        ended.now = []
        let last = RoundtableTagLayout.cardData(summary: overlapSummary, view: ended, now: 13_000, link: .live, linkSince: 0)
        #expect(last.kind == .lastDouble && last.keyed == ["A", "B (2)", "C"] && last.endedAgo == 3_000)
        #expect(last.liveHz == nil && last.clock == 0 && last.note == nil)
        #expect(last.rows.allSatisfy { $0.km == nil && $0.peak == nil })

        var remaining = ended
        remaining.now = [singleTurn(id: 13, hz: 7_236_400, start: 10_000)]
        let talker = RoundtableTagLayout.cardData(summary: overlapSummary, view: remaining, now: 13_400, link: .live, linkSince: 0)
        #expect(talker.kind == .talking && talker.keyed == ["A"] && talker.rows.isEmpty)
        #expect(talker.note == "Double ended 3s ago")
        let later = RoundtableTagLayout.cardData(summary: overlapSummary, view: remaining, now: 18_000, link: .live, linkSince: 0)
        #expect(later.note == nil)
        let reconnecting = RoundtableTagLayout.cardData(summary: overlapSummary, view: remaining, now: 13_400,
                                                        link: .reconnecting, linkSince: 13_000)
        #expect(reconnecting.note == "Reconnecting", "A link note takes the one line")
    }

    @Test("a frequency known only from the directory says who is keyed, with no readings; a quiet one has no card state")
    func cardFromTheDirectory() throws {
        let rows = RoundtableModel.frequencyRows(of: [source("lsb-7236000", [
            station("a", source: "lsb-7236000", hz: 7_236_400, keyed: true),
            station("b", source: "lsb-7236000", hz: 7_236_700, keyed: true),
        ])])
        let double = RoundtableTagLayout.cardData(summary: rows[0], view: nil, now: 1, link: .live, linkSince: 0)
        #expect(double.kind == .double && double.keyed == ["A", "B"] && double.rows.isEmpty && double.liveHz == nil)
        #expect(double.people == ["A"])
        #expect(RoundtableTagLayout.card(for: rows[0], view: nil, now: 1)?.participants == ["A", "B"])
        let quiet = RoundtableModel.frequencyRows(of: [source("lsb-7236000", [
            station("q", source: "lsb-7236000", hz: 7_236_400, keyed: false)])])
        #expect(RoundtableTagLayout.cardData(summary: quiet[0], view: room(), now: 1, link: .live, linkSince: 0).kind == .quiet)
    }

    @Test("D12: a held peak rises at once, falls 2 dB a second, and resets on a new turn, per source room and listener")
    func peakHold() {
        let peaks = RoundtablePeakTracker()
        #expect(peaks.update(sourceRoom: "s", ref: "a", turn: 1, dbm: -80, now: 0) == -80)
        #expect(peaks.update(sourceRoom: "s", ref: "a", turn: 1, dbm: -60, now: 100) == -60)
        #expect(peaks.update(sourceRoom: "s", ref: "a", turn: 1, dbm: -90, now: 2_100) == -64)
        #expect(peaks.update(sourceRoom: "s", ref: "a", turn: 1, dbm: -63, now: 2_100) == -63)
        #expect(peaks.update(sourceRoom: "s", ref: "a", turn: 2, dbm: -90, now: 2_200) == -90)
        #expect(peaks.update(sourceRoom: "t", ref: "a", turn: 1, dbm: -50, now: 2_200) == -50)
        #expect(peaks.update(sourceRoom: "s", ref: "b", turn: 2, dbm: -70, now: 2_200) == -70)
        // A long session cannot grow it without bound, and comes straight back.
        for index in 0..<2_000 {
            _ = peaks.update(sourceRoom: "s", ref: "r\(index)", turn: 1, dbm: -80, now: 3_000)
        }
        #expect(peaks.update(sourceRoom: "s", ref: "r1999", turn: 1, dbm: -90, now: 4_000) == -82)
    }

    @Test("the room's readings and title follow the same turn, and drop a transmitter from the listeners")
    func roomReadingsFollowTheTruthRules() {
        let view = room(now: [singleTurn(id: 12, hz: 7_236_407)], members: [
            overlapMember("a", keyed: true), overlapMember("other", keyed: true), overlapMember("listener")])
        #expect(RoundtableReadings.rows(room: view, keyed: false).map(\.member.ref) == ["listener"])
        // D6, D8: a live talker is never captioned as past.
        #expect(RoundtableReadings.talkerTitle(room: view, keyed: false) == "Talking: A")
        #expect(RoundtableReadings.talkerTitle(room: view, keyed: true) == "Talking: You")
        #expect(RoundtableReadings.talkerTitle(room: overlapRoom, keyed: false) == "Double \u{00B7} combined RF")
        var past = singleTurn(id: 12, hz: 7_236_400)
        past.end = 1000
        #expect(RoundtableReadings.talkerTitle(room: room(earlier: [past]), keyed: false) == "Last talker: A")
        var pastDouble = overlapRoom.now[0]
        pastDouble.end = 1000
        #expect(RoundtableReadings.talkerTitle(room: room(earlier: [pastDouble]), keyed: false) == "Last double \u{00B7} combined RF")
        let ended = RoundtableText.lastTurnText(room: overlapRoom, turn: overlapRoom.now[0], now: 900)
        #expect(ended.hasPrefix("Last double \u{00B7} combined RF (A + B (2) + C)"))
    }
}
