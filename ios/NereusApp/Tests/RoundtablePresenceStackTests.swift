// NereusSDR for iOS: exact-frequency presence remains independent of loaded RF reports.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-30: Bounded presence-stack acceptance correction.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

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
        #expect(doubled.stateCaption == "Last overlap" && doubled.stateTitle == "Double")
        let captionWidth = ("Last overlap" as NSString).size(withAttributes: [
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
        for i in room.members.indices { room.members[i].reading?.turn = 999 }
        #expect(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200)?.state == .waiting)
        room = overlapRoom
        room.now[0].sourceRoom = "lsb-7240000"
        #expect(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200)?.state == .waiting)
        room = overlapRoom
        room.family = "usb"
        #expect(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200)?.state == .waiting)
        room = overlapRoom
        room.now[0].kind = "talking"
        room.now[0].talkers = Array(room.now[0].talkers.prefix(1))
        #expect(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200)?.state == .waiting)
        room.now[0].kind = "unknown"
        room.now[0].talkers = []
        #expect(RoundtableTagLayout.card(for: overlapSummary, view: room, now: 1200)?.state == .waiting)
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
}
