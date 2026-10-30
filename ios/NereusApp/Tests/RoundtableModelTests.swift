// NereusSDR for iOS: Roundtable's link model against a fake Core that runs it: rooms, reading, posts and windows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 16, the phone's link model tests, the
// windows tested as tests/tst_roundtable_model.cpp tests the desktop's.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable fix wave: a room is built once per change of its
// records. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

// 2026-10-30: Exact-frequency contexts, scoped reports, paths and native renders.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// R-RT-15 (phone side), R-RT-17: the real app's session against a fake
/// Core that runs Roundtable (``FakeStation/Additions/roundtable``,
/// FakeStation+Roundtable.swift). The phone follows the band list and the
/// slice's room, reads another room through `roundtable.read` and the
/// stream it names, posts and gets the answer, marks mentions read, and
/// from a Core without Roundtable follows and sends nothing. The rooms read
/// as the desktop's window reads them, on a clock the test moves.
@Suite("Roundtable link model", .serialized)
@MainActor
struct RoundtableModelTests {
    private let platform = TestPlatform()
    /// Keeps the app (and so its session) alive for the whole test, used or not.
    private final class Held {
        var model: AppModel?
    }

    private let held = Held()
    private static let usb = "usb-14200000"
    private static let cw = "cw-7030000"
    private static let minute: Int64 = 60 * 1000
    private static let hour: Int64 = 60 * minute

    @Test("Core source controls follow every authoritative Roundtable state", arguments: [
        (FakeStation.RoundtableLink.off, "Start", "spots.connect"),
        (.connecting, "Stop", "spots.disconnect"), (.live, "Stop", "spots.disconnect"),
        (.reconnecting, "Stop", "spots.disconnect"), (.offline, "Stop", "spots.disconnect"),
        (.hidden, "Stop", "spots.disconnect"),
    ])
    func sourceControlFollowsCoreState(state: FakeStation.RoundtableLink, title: String, verb: String) async throws {
        let (_, station, rt, _) = try await connected()
        try await station.deliverRoundtable(.init(rooms: [], link: state))
        #expect(await settle { rt.linkState.rawValue == state.rawValue })
        #expect(rt.sourceControlReason == nil && rt.sourceControlTitle == title)
        #expect(await rt.setRunning(title == "Start") == nil)
        #expect(await sent(station, verb) == [.init(name: "source", value: .utf8("roundtable"))])
        #expect(await settle { rt.sourceControlTitle == (title == "Start" ? "Stop" : "Start") })
    }

    @Test("a refused start keeps the Core's state and words")
    func sourceControlRefusal() async throws {
        let (_, station, rt, _) = try await connected()
        try await station.deliverRoundtable(.init(rooms: [], link: .off))
        #expect(await settle { rt.linkState == .off })
        let reason = "Enter your callsign and grid square in Spot Hub first."
        station.refuseNext("spots.connect", reason: reason)
        #expect(await rt.setRunning(true) == reason)
        #expect(rt.note == reason && rt.linkState == .off && rt.sourceControlTitle == "Start")
    }

    @Test("disconnection and missing source capability send no source controls")
    func unavailableSourceControl() async throws {
        let (model, station, rt, _) = try await connected()
        // Roundtable's normal fake correctly implies record streams. Feed a partial capability set
        // explicitly to exercise a Core that does not offer the source-control verb surface.
        model.mirror.apply(.capabilities(.init(properties: [
            .init(name: "stationRoundtableVersion", value: .i64(1)),
            .init(name: "recordStreamVersion", value: .i64(0)),
        ])))
        #expect(await settle { rt.sourceControlVersion == 0 })
        #expect(rt.available && rt.sourceControlReason == RoundtableModel.olderCoreReason)
        #expect(await rt.setRunning(true) == RoundtableModel.olderCoreReason)
        #expect(await rt.setRunning(false) == RoundtableModel.olderCoreReason)
        await TransmitScreenTests.barrier(model, station)
        await model.disconnect()
        #expect(await settle { !rt.connected })
        #expect(await rt.setRunning(true) == RoundtableModel.notConnectedReason)
        #expect(await rt.setRunning(false) == RoundtableModel.notConnectedReason)
        #expect(!station.messages.compactMap(AccessoryPagesTests.invoke).contains {
            $0.verb == "spots.connect" || $0.verb == "spots.disconnect"
        })
    }

    // MARK: The band list and the slice's room

    @Test("the band list comes from roundtableRooms, by center, and the slice's room reads as the desktop's")
    func roomsAppear() async throws {
        let (model, station, rt, _) = try await connected()
        #expect(rt.version == 1)
        let subscribe = await sent(station, "records.subscribe") { $0.first?.value == .utf8("roundtableRooms") }
        #expect(subscribe == [.init(name: "stream", value: .utf8("roundtableRooms")),
                              .init(name: "backlog", value: .i64(500))])
        try await station.deliverRoundtable()
        #expect(await settle { rt.rooms.count == 2 && rt.room != nil })
        #expect(rt.available)
        // The band list by center frequency: 40 m before 20 m.
        #expect(rt.rooms.map(\.id) == [Self.cw, Self.usb])
        #expect(rt.rooms.last?.members == 2)
        #expect(rt.linkState == .live && rt.linkSince == 1_000)
        #expect(rt.firstRunAnswered && !rt.hidden)
        #expect(rt.mentionCounts == [Self.cw: 1])
        #expect(rt.sliceRooms == [0: Self.usb] && rt.sliceRoomId == Self.usb && rt.readingRoomId == nil)
        // The slice's room is followed, 400 records back.
        let usbStream = "roundtableRoom:\(Self.usb)"
        let room = await sent(station, "records.subscribe") { $0.first?.value == .utf8(usbStream) }
        #expect(room?.last == .init(name: "backlog", value: .i64(400)))
        #expect(model.records.isWanted(usbStream))
        let shown = try #require(rt.room)
        #expect(shown.id == Self.usb && shown.family == "usb" && shown.hz == 14_200_000 && shown.band == "20m")
        #expect(shown.members.map(\.ref) == ["a/1", "b/0"])
        #expect(shown.members.first?.reading == .init(dbm: -73.5, t: 1_000, present: true, turn: nil))
        #expect(shown.members.last?.reading == nil && shown.members.last?.you == true)
        #expect(shown.now.map(\.id) == [11])
        // A turn's names by count, highest first.
        #expect(shown.now.first?.names == [.init(name: "K1ABC", count: 3), .init(name: "W1AW", count: 1)])
        #expect(shown.now.first?.talkers.map(\.callsign) == ["W1AW"])
        // Ended turns newest first; messages oldest first.
        #expect(shown.earlier.map(\.id) == [10, 9])
        #expect(shown.earlier.first?.names == [.init(name: "N0CALL", count: 2)])
        #expect(shown.messages.map(\.id) == [20, 21])
        #expect(shown.messages.last?.text == "good signal")
        #expect(RoundtableModel.bandOf(7_030_000) == "40m" && RoundtableModel.bandOf(14_350_001) == "other")
        // The session ends: every room goes.
        await model.disconnect()
        #expect(await settle { rt.rooms.isEmpty && rt.room == nil && !rt.available })
    }

    @Test("members keep their joining order and a room keeps its last fifty messages")
    func joiningOrderAndFiftyMessages() async throws {
        let (_, station, rt, _) = try await connected()
        try await station.deliverRoundtable()
        #expect(await settle { rt.room?.members.count == 2 })
        // A third station joins, the first leaves: b/0 stays in its place.
        await station.deliverRoundtableRoom(Self.usb, upserts: [
            FakeStation.roundtableMember(ref: "c/2", callsign: "K1ABC", seq: 2),
        ], removes: ["member:a/1", "reading:a/1"])
        #expect(await settle { rt.room?.members.map(\.ref) == ["b/0", "c/2"] })
        let many = (0..<60).map { index in
            FakeStation.roundtableMessage(id: Int64(100 + index), t: Int64(2_000 + index), callsign: "W1AW",
                                          text: "line \(index)")
        }
        await station.deliverRoundtableRoom(Self.usb, upserts: many)
        #expect(await settle { rt.room?.messages.last?.id == 159 })
        let messages = try #require(rt.room?.messages)
        #expect(messages.count == RoundtableModel.roomMessages)
        #expect(messages.first?.id == 110)
    }

    @Test("a room is built once per change of its records, not on every read")
    func roomBuiltOncePerChange() async throws {
        let (_, station, rt, clock) = try await connected()
        let minute = Self.minute
        await clock.advance(by: 100 * minute)
        try await station.deliverRoundtable(Self.scene([
            FakeStation.roundtableMember(ref: "a/1", callsign: "W1AW", seq: 0),
            FakeStation.roundtableTurn(id: 2, start: 70 * minute, end: 75 * minute),
        ]))
        #expect(await settle { rt.room?.earlier.map(\.id) == [2] })
        let first = rt.view(of: Self.usb)
        let builds = rt.roomBuildsForTesting
        // The flag lines, the tags and the sheet ask on every pass.
        for _ in 0..<50 {
            #expect(rt.view(of: Self.usb) == first)
            _ = RoundtableFlagLine.line(rt, sliceId: 0)
        }
        #expect(rt.roomBuildsForTesting == builds)
        // A change to its records builds it anew.
        await station.deliverRoundtableRoom(Self.usb, upserts: [
            FakeStation.roundtableMember(ref: "c/2", callsign: "K1ABC", seq: 2),
        ])
        #expect(await settle { rt.view(of: Self.usb)?.members.map(\.ref) == ["a/1", "c/2"] })
        #expect(rt.roomBuildsForTesting > builds)
        // So does its earlier turn leaving the 30 minutes, with no record changed.
        await clock.advance(by: 5 * minute + 1)
        #expect(rt.view(of: Self.usb)?.earlier.isEmpty == true)
    }

    // MARK: Reading another room

    @Test("reading another room sends roundtable.read, then follows the stream the Core names")
    func readingAnotherRoom() async throws {
        let (model, station, rt, _) = try await connected()
        try await station.deliverRoundtable()
        #expect(await settle { rt.room?.id == Self.usb })
        let stream = "roundtableRoom:\(Self.cw)"
        #expect(!model.records.isWanted(stream))
        #expect(await rt.read(Self.cw) == nil)
        #expect(station.roundtableReads == [Self.cw])
        #expect(rt.readingRoomId == Self.cw)
        #expect(await sent(station, "records.subscribe") { $0.first?.value == .utf8(stream) } != nil)
        // The read went before the stream was asked for.
        let verbs = station.messages.compactMap(AccessoryPagesTests.invoke).map { invoke -> String in
            if case .utf8(let name)? = invoke.args.first?.value, invoke.verb.hasPrefix("records.") {
                return "\(invoke.verb) \(name)"
            }
            return invoke.verb
        }
        let read = try #require(verbs.firstIndex(of: "roundtable.read"))
        let follow = try #require(verbs.firstIndex(of: "records.subscribe \(stream)"))
        #expect(read < follow)
        #expect(model.records.isWanted(stream))
        #expect(await settle { rt.room?.id == Self.cw })
        #expect(rt.room?.members.map(\.callsign) == ["K1ABC"])
        // Back to the slice: the room is let go, and its stream stops.
        await rt.backToSlice()
        #expect(rt.readingRoomId == nil && !model.records.isWanted(stream))
        #expect(await sent(station, "roundtable.unread") == [.init(name: "roomId", value: .utf8(Self.cw))])
        #expect(await settle { rt.room?.id == Self.usb })
        #expect(model.records.isWanted("roundtableRoom:\(Self.usb)"))
        // A room the Core does not list: refused in its words, nothing read or followed.
        #expect(await rt.read("am-3885000") == FakeStation.roundtableNoRoomReason)
        #expect(rt.readingRoomId == nil && rt.note == FakeStation.roundtableNoRoomReason)
        #expect(!model.records.isWanted("roundtableRoom:am-3885000"))
        #expect(rt.room?.id == Self.usb)
    }

    // MARK: Actions

    @Test("a post returns the Core's answer, and its message arrives in the room")
    func posting() async throws {
        let (_, station, rt, _) = try await connected()
        try await station.deliverRoundtable()
        #expect(await settle { rt.room?.id == Self.usb })
        let result = await rt.post("73 all", in: Self.usb, clientId: "phone-1")
        #expect(result == .posted(messageId: "1001"))
        #expect(station.roundtablePosts == [.init(roomId: Self.usb, text: "73 all", clientId: "phone-1",
                                                  messageId: "1001")])
        #expect(await settle { rt.room?.messages.last?.text == "73 all" })
        // Roundtable drops: the Core says why it can't be posted.
        await station.deliverRoundtableState { $0.link = .reconnecting }
        #expect(await settle { rt.linkState == .reconnecting })
        #expect(await rt.post("again", in: Self.usb, clientId: "phone-2")
            == .refused(FakeStation.roundtableNotPostedReason))
        #expect(rt.note == FakeStation.roundtableNotPostedReason)
        #expect(station.roundtablePosts.count == 1)
    }

    @Test("a mention marked read on the phone is sent as roundtable.markMentionRead")
    func markingMentionsRead() async throws {
        let (_, station, rt, _) = try await connected()
        try await station.deliverRoundtable()
        #expect(await settle { rt.mentionCounts == [Self.cw: 1] })
        #expect(await rt.markMentionRead(upToMs: 1_790_000_000_000) == nil)
        #expect(await sent(station, "roundtable.markMentionRead")
            == [.init(name: "upToMs", value: .i64(1_790_000_000_000))])
        #expect(station.roundtableMentionsSeenMs == 1_790_000_000_000)
        // The other verbs, each with its arguments; the Core's echoes move the object.
        #expect(await rt.setHidden(true) == nil)
        #expect(await settle { rt.hidden })
        #expect(await rt.mute("k4abc/p") == nil)
        #expect(station.roundtableMuteList == ["K4ABC"])
        #expect(await rt.unmute("K4ABC") == nil)
        #expect(station.roundtableMuteList.isEmpty)
        #expect(await rt.mute("hello") == FakeStation.roundtableMuteReason)
        #expect(await rt.name(turn: "11", as: "W1AW") == nil)
        #expect(await sent(station, "roundtable.name")
            == [.init(name: "turnId", value: .utf8("11")), .init(name: "name", value: .utf8("W1AW"))])
        #expect(await rt.delete("21") == nil)
        #expect(await rt.report(kind: "message", target: "21", reason: "spam") == nil)
        #expect(await rt.answerFirstRun(live: true) == nil)
        #expect(await rt.webSignIn() == URL(string: FakeStation.roundtableWebLink))
        #expect(await rt.deleteArchive(confirm: "delete it") == FakeStation.roundtableConfirmReason)
        #expect(await rt.deleteArchive(confirm: RoundtableModel.deleteArchiveWords) == nil)
        #expect(station.roundtableAnswered == ["markMentionRead", "setHidden", "mute", "unmute", "mute", "name",
                                               "delete", "report", "answerFirstRun", "webSignIn", "deleteArchive",
                                               "deleteArchive"])
    }

    @Test("from a Core without Roundtable the model is unavailable and nothing is followed or sent")
    func aCoreWithoutRoundtable() async throws {
        let (model, station, rt, _) = try await connected(additions: [.spots])
        #expect(await settle { model.records.available })
        #expect(rt.version == 0 && !rt.available)
        #expect(rt.coreReason == RoundtableModel.olderCoreReason)
        #expect(await rt.setRunning(true) == RoundtableModel.olderCoreReason)
        #expect(await rt.setRunning(false) == RoundtableModel.olderCoreReason)
        #expect(await rt.post("hello", in: Self.usb) == .refused(RoundtableModel.olderCoreReason))
        #expect(await rt.read(Self.usb) == RoundtableModel.olderCoreReason)
        #expect(await rt.markMentionRead(upToMs: 5) == RoundtableModel.olderCoreReason)
        #expect(await rt.webSignIn() == nil)
        #expect(rt.readingRoomId == nil)
        await TransmitScreenTests.barrier(model, station)
        #expect(!model.records.isWanted("roundtableRooms"))
        #expect(!model.records.isWanted("roundtableRoom:\(Self.usb)"))
        let sent = station.messages.compactMap(AccessoryPagesTests.invoke)
        #expect(!sent.contains { $0.verb.hasPrefix("roundtable.")
            || $0.verb == "spots.connect" || $0.verb == "spots.disconnect" })
        #expect(!sent.contains { invoke in
            guard case .utf8(let stream)? = invoke.args.first?.value else {
                return false
            }
            return stream.hasPrefix("roundtableRoom")
        })
        #expect(rt.rooms.isEmpty && rt.room == nil)
    }

    // MARK: The windows (spec 8.6, wire spec 5.4.6)

    @Test("the earlier turns are the last thirty minutes, dropped on arrival, in the readers and by the timer")
    func earlierTurnsAreTheLastThirtyMinutes() async throws {
        let (_, station, rt, clock) = try await connected()
        let minute = Self.minute
        await clock.advance(by: 100 * minute)
        try await station.deliverRoundtable(Self.scene([
            FakeStation.roundtableMember(ref: "a/1", callsign: "W1AW", seq: 0),
            FakeStation.roundtableTurn(id: 1, start: 60 * minute, end: 65 * minute),
            FakeStation.roundtableTurn(id: 2, start: 70 * minute, end: 75 * minute),
            FakeStation.roundtableTurn(id: 3, start: 90 * minute, end: 95 * minute),
        ]))
        // Turn 1 ended 35 minutes ago: dropped on arrival.
        #expect(await settle { rt.room?.earlier.map(\.id) == [3, 2] })
        // Turn 2 leaves the window: the reader stops showing it at once.
        await clock.advance(by: 5 * minute + 1)
        #expect(rt.view(of: Self.usb)?.earlier.map(\.id) == [3])
        // A second past the moment, the timer drops it and the room in view follows.
        await clock.advance(by: 1_000)
        #expect(rt.room?.earlier.map(\.id) == [3])
        rt.pruneEarlierTurns()
        #expect(rt.room?.earlier.map(\.id) == [3])
        // Turn 3 ages out unasked once its time comes.
        await clock.advance(by: 20 * minute)
        #expect(rt.room?.earlier.isEmpty == true)
        #expect(rt.room?.members.map(\.ref) == ["a/1"])
    }

    @Test("the names to name a turn with are the last day's on that frequency, newest first")
    func recentNamesAreTheLastDayOnThatFrequency() async throws {
        let (_, station, rt, clock) = try await connected()
        let minute = Self.minute
        let hour = Self.hour
        await clock.advance(by: 30 * hour)
        let now = 30 * hour
        // K4ABC/P talked 10 minutes ago; an unknown turn 5 minutes ago was
        // named W4XYZ by two and k9awy by one.
        try await station.deliverRoundtable(Self.scene([
            FakeStation.roundtableMember(ref: "a/1", callsign: "W1AW", seq: 0),
            FakeStation.roundtableTurn(id: 1, start: now - 12 * minute, end: now - 10 * minute, talkers: ["K4ABC/P"]),
            FakeStation.roundtableTurn(id: 2, start: now - 6 * minute, end: now - 5 * minute),
            FakeStation.roundtableName(turn: 2, name: "W4XYZ", count: 2),
            FakeStation.roundtableName(turn: 2, name: "k9awy", count: 1),
        ]))
        let firstSeen = ["K9AWY", "W4XYZ", "K4ABC/P"]
        #expect(await settle { rt.recentNames(Self.usb) == firstSeen })
        #expect(rt.recentNames(Self.cw).isEmpty)

        // Two hours on, the earlier turns are gone from the room; their
        // names are not. W1AW talks now: used now, so first. A name given
        // as the base callsign of one already listed replaces it.
        await clock.advance(by: 2 * hour)
        #expect(rt.room?.earlier.isEmpty == true)
        await station.deliverRoundtableRoom(Self.usb, upserts: [
            FakeStation.roundtableTurn(id: 3, start: now + 2 * hour - minute, end: nil, talkers: ["W1AW"]),
            FakeStation.roundtableName(turn: 3, name: "K4ABC", count: 1),
        ])
        let later = ["K4ABC", "W1AW", "K9AWY", "W4XYZ"]
        #expect(await settle { rt.recentNames(Self.usb) == later })

        // The room closes and reopens on the same center: the same room
        // for this, so its names stay.
        await station.deliverRoundtableRoom(Self.usb, upserts: [], removes: [
            "member:a/1", "turn:1", "turn:2", "turn:3", "name:2:W4XYZ", "name:2:k9awy", "name:3:K4ABC",
        ])
        #expect(await settle { rt.room == nil })
        #expect(rt.recentNames(Self.usb) == later)
        await station.deliverRoundtableRoom(Self.usb, upserts: [
            FakeStation.roundtableMember(ref: "a/1", callsign: "W1AW", seq: 0),
        ])
        #expect(await settle { rt.room?.members.count == 1 })
        #expect(rt.recentNames(Self.usb) == later)

        // A day after each was last used, it leaves the list: turn 2's
        // names (used at 30 h less 5 minutes) go first.
        let day = 24 * hour
        await clock.advance(by: (now - 5 * minute + day + 1) - clock.now)
        #expect(rt.recentNames(Self.usb) == ["K4ABC", "W1AW"])
        await clock.advance(by: (now + 2 * hour + day + 1) - clock.now)
        #expect(rt.recentNames(Self.usb).isEmpty)
    }

    @Test("frequency subscriptions follow exact slice Hz without an RF room, and preserve sender context and drafts")
    func frequencyContextWithoutRFMembership() async throws {
        let (model, station, rt, _) = try await connected(additions: [.roundtableFrequency])
        try await MainScreenShotTests.fill(station)
        #expect(await settle { model.main.catalogFeed.catalog != nil && !model.main.slices.entries.isEmpty })
        var message = FakeStation.roundtableMessage(id: 901, t: 1000, callsign: "W1AW", text: "Original frequency")
        message.fields["hz"] = .number(7_236_400)
        message.fields["family"] = .string("lsb")
        var scene = FakeStation.RoundtableScene(rooms: [], roomRecords: ["lsb-7236400": [message]], sliceRooms: [:])
        scene.frequencyPresences = [0: "lsb-7236400"]
        try await station.deliverRoundtable(scene)
        model.mirror.apply(.delta(.init(key: "slice:0", properties: [
            .init(name: "frequency", value: .f64(7_236_400)),
            .init(name: "dspMode", value: .enumeration(0)),
        ])))
        rt.activeSliceId = 0
        let ready = await settle { rt.activeContext?.id == "lsb-7236400" && rt.room?.messages.count == 1 }
        if !ready {
            print("Frequency fixture version=\(rt.version) context=\(String(describing: rt.activeContext)) note=\(rt.note ?? "none") reads=\(station.roundtableReads) followed=\(station.roundtableFollowed)")
        }
        try #require(ready)
        #expect(station.roundtableReads.contains("lsb-7236400"))
        #expect(station.roundtableFollowed.contains("roundtableFrequency:lsb-7236400"))
        rt.setDraft("Keep on the original frequency", for: "lsb-7236400")
        #expect(await rt.post("hello", in: "lsb-7236400", clientId: "frequency-test") == .posted(messageId: "1001"))
        let originalCount = station.roundtablePosts.count
        let otherSlice = RoundtableModel.FrequencyContext(sliceId: 1, hz: 7_236_400, family: "lsb")
        if case .refused = await rt.post("Wrong slice", in: "lsb-7236400", context: otherSlice) {} else {
            Issue.record("An original draft cannot migrate to another slice at the same Hz")
        }
        #expect(station.roundtablePosts.count == originalCount)
        let args = await sent(station, "roundtable.postAt")
        #expect(args?.contains(.init(name: "sliceId", value: .i64(0))) == true)
        #expect(args?.contains(.init(name: "hz", value: .i64(7_236_400))) == true)
        model.mirror.apply(.delta(.init(key: "slice:0", properties: [
            .init(name: "frequency", value: .f64(7_250_001)),
        ])))
        #expect(await settle { rt.activeContext?.hz == 7_250_001 })
        let before = station.roundtablePosts.count
        if case .refused = await rt.post("Keep on the original frequency", in: "lsb-7236400") {} else {
            Issue.record("A stale frequency retry must refuse")
        }
        #expect(station.roundtablePosts.count == before)
        #expect(rt.draft("lsb-7236400") == "Keep on the original frequency")
        #expect(rt.draft("lsb-7250001").isEmpty)
        #expect(rt.receivedMessages.first(where: { $0.id == 901 })?.hz == 7_236_400)
        await model.disconnect()
    }

    private static func historyMessage(_ id: Int64, state: String = "shown", text: String = "Received earlier") -> LinkMessage.RecordBatch.Record {
        var record = FakeStation.roundtableMessage(id: id, t: id, callsign: "W1AW", text: text)
        record.fields["hz"] = .number(7_236_400)
        record.fields["family"] = .string("lsb")
        record.fields["state"] = .string(state)
        if state != "shown" { record.fields["text"] = .null }
        return record
    }

    private func historyConnected(_ messages: [LinkMessage.RecordBatch.Record], replies: HeldCommandReplies? = nil) async throws
        -> (AppModel, FakeStation, RoundtableModel, TestLinkClock) {
        let (model, station, rt, clock) = try await connected(additions: [.roundtableFrequency], replies: replies)
        try await MainScreenShotTests.fill(station)
        let context = "lsb-7236400"
        var scene = FakeStation.RoundtableScene(rooms: [], roomRecords: [context: messages], sliceRooms: [:])
        scene.frequencyPresences = [0: context]
        try await station.deliverRoundtable(scene)
        model.mirror.apply(.delta(.init(key: "slice:0", properties: [
            .init(name: "frequency", value: .f64(7_236_400)), .init(name: "dspMode", value: .enumeration(0))])))
        rt.activeSliceId = 0
        try #require(await settle { rt.receivedMessages.count == min(messages.count, 200)
            && rt.room?.frequencyView == true && rt.historyStatus == nil && !station.roundtableHistoryRequests.isEmpty })
        return (model, station, rt, clock)
    }

    @Test("retuned received IDs refresh deletion, moderation, expiry and real restoration without discovery")
    func retainedHistoryAfterRetuning() async throws {
        let (model, station, rt, clock) = try await historyConnected([Self.historyMessage(901), Self.historyMessage(902), Self.historyMessage(903)])
        let mentions = rt.mentionCounts
        model.mirror.apply(.delta(.init(key: "slice:0", properties: [.init(name: "frequency", value: .f64(7_250_001))])))
        try #require(await settle { rt.activeContext?.hz == 7_250_001
            && !station.roundtableFollowed.contains("roundtableFrequency:lsb-7236400") })
        for (id, state) in [(Int64(901), "deleted"), (902, "removed"), (903, "expired")] {
            station.setRoundtableHistoryState(.init(id: id, state: state, text: nil))
        }
        rt.refreshReceivedHistory()
        #expect(await settle { rt.receivedMessages.map(\.state) == ["deleted", "removed", "expired"] && rt.historyStatus == nil })
        #expect(rt.receivedMessages.allSatisfy { $0.text == nil && $0.hz == 7_236_400 })
        #expect(rt.mentionCounts == mentions)
        station.setRoundtableHistoryState(.init(id: 902, state: "shown", text: "Actually restored by helper"))
        await clock.advance(by: 10_000)
        #expect(await settle { rt.receivedMessages.first(where: { $0.id == 902 })?.text == "Actually restored by helper" })
        #expect(rt.receivedMessages.map(\.id) == [901, 902, 903])
        #expect(station.roundtableHistoryRequests.allSatisfy { $0.count <= 200 && Set($0).count == $0.count })
        let draftContext = try #require(rt.room?.id)
        rt.setDraft("keep this slice draft", for: draftContext)
        let draftKey = rt.draftKey(draftContext)
        await model.disconnect()
        #expect(await settle { rt.historyStatus == "History stale · offline" })
        #expect(rt.room?.messages.count == 3, "Disconnected received history stays visible")
        #expect(rt.room?.messages.first(where: { $0.id == 902 })?.text == "Actually restored by helper")
        #expect(rt.draftKey(draftContext) == draftKey)
        #expect(rt.draft(draftContext) == "keep this slice draft")
        rt.setDraft("edited while offline", for: draftContext)
        #expect(rt.drafts[draftKey] == "edited while offline")
    }

    @Test("history snapshots guard each ID against newer live tombstones and ignore unrelated busy arrivals")
    func retainedHistoryLiveGenerationGuards() async throws {
        let heldReplies = HeldCommandReplies()
        let (model, station, rt, _) = try await historyConnected([Self.historyMessage(901), Self.historyMessage(902)], replies: heldReplies)
        station.setRoundtableHistoryState(.init(id: 902, state: "removed", text: nil))
        heldReplies.hold("roundtable.history")
        rt.refreshReceivedHistory()
        try #require(await settle { heldReplies.count("roundtable.history") >= 1 })
        await station.deliverRoundtableRoom("lsb-7236400", upserts: [
            Self.historyMessage(901, state: "deleted"), Self.historyMessage(904, text: "Unrelated live arrival")])
        try #require(await settle { rt.receivedMessages.first(where: { $0.id == 901 })?.state == "deleted"
            && rt.receivedMessages.contains(where: { $0.id == 904 }) })
        await heldReplies.release("roundtable.history")
        #expect(await settle { rt.receivedMessages.first(where: { $0.id == 902 })?.state == "removed" && rt.historyStatus == nil })
        #expect(rt.receivedMessages.first(where: { $0.id == 901 })?.text == nil)
        // A history tombstone also survives reconstruction caused by a meter
        // or member record while the active stream still holds old shown text.
        await station.deliverRoundtableRoom("lsb-7236400", upserts: [FakeStation.roundtableMember(ref: "m", callsign: "N4XYZ", seq: 0)])
        #expect(await settle { rt.room?.members.count == 1 })
        #expect(rt.room?.messages.first(where: { $0.id == 902 })?.state == "removed")
        #expect(rt.room?.messages.first(where: { $0.id == 902 })?.text == nil)
        await model.disconnect()
    }

    @Test("history cache and metadata stay bounded; malformed replies cannot discover or restore text")
    func retainedHistoryBoundsAndMalformedReplies() async throws {
        let (model, station, rt, _) = try await historyConnected((1...240).map { Self.historyMessage(Int64($0)) })
        #expect(rt.receivedMessages.count == 200 && rt.receivedMessages.first?.id == 41)
        let ids = rt.receivedMessages.map(\.id)
        let mentions = rt.mentionCounts
        station.setRoundtableHistoryReplyOverride("[{\"id\":99999,\"state\":\"shown\",\"text\":\"Unrequested discovery\"}]")
        rt.refreshReceivedHistory()
        #expect(await settle { rt.historyStatus == "History stale · refresh failed" })
        #expect(rt.receivedMessages.map(\.id) == ids && rt.mentionCounts == mentions)
        #expect(!rt.receivedMessages.contains(where: { $0.id == 99999 }))
        station.setRoundtableHistoryReplyOverride("not JSON")
        rt.refreshReceivedHistory()
        #expect(await settle { rt.historyStatus == "History stale · refresh failed" })
        #expect(rt.receivedMessages.map(\.id) == ids)
        station.setRoundtableHistoryReplyOverride(nil)
        rt.refreshReceivedHistory()
        #expect(await settle { rt.historyStatus == nil })
        #expect(station.roundtableHistoryRequests.allSatisfy { $0.count <= 200 })
        await model.disconnect()
    }

    @Test("reconnect validates an old retained ID beyond fifty newer messages and cancels the old session reply")
    func retainedHistoryReconnectBeyondBacklog() async throws {
        let heldReplies = HeldCommandReplies()
        let (model, old, rt, _) = try await historyConnected([Self.historyMessage(901)], replies: heldReplies)
        heldReplies.hold("roundtable.history")
        rt.refreshReceivedHistory()
        try #require(await settle { heldReplies.count("roundtable.history") >= 1 })
        let generation = rt.connectionGeneration
        await model.disconnect()
        #expect(await settle { rt.historyStatus == "History stale · offline" })
        let next = try FakeStation(additions: [.roundtableFrequency])
        // Sixty later stored messages: only the latest fifty are in the view.
        for id in 1000..<1060 { next.setRoundtableHistoryState(.init(id: Int64(id), state: "shown", text: "Newer")) }
        next.setRoundtableHistoryState(.init(id: 901, state: "removed", text: nil))
        var scene = FakeStation.RoundtableScene(rooms: [], roomRecords: ["lsb-7236400": (1010..<1060).map { Self.historyMessage(Int64($0)) }], sliceRooms: [:])
        scene.frequencyPresences = [0: "lsb-7236400"]
        await model.connect(to: next.endpoint, trust: next.trust, authenticator: next.authenticator, transportFactory: next.transportFactory)
        #expect(await next.waitUntilLive())
        try #require(await settle { model.connection == .connected })
        try await MainScreenShotTests.fill(next)
        // Deliver the Core's live object and records after its real session
        // snapshot, as all other Roundtable fixtures do. Pre-session delivery
        // populates the generic fake catalogue without a live record owner.
        try await next.deliverRoundtable(scene)
        let refreshed = await settle { rt.connectionGeneration != generation && rt.receivedMessages.count >= 51
            && rt.receivedMessages.first(where: { $0.id == 901 })?.state == "removed" && rt.historyStatus == nil }
        if !refreshed {
            print("History reconnect version=\(rt.version) connected=\(rt.connected) link=\(rt.linkState) status=\(rt.historyStatus ?? "nil") count=\(rt.receivedMessages.count) reads=\(next.roundtableReads) history=\(next.roundtableHistoryRequests) streams=\(next.roundtableFollowed)")
        }
        try #require(refreshed)
        await heldReplies.release("roundtable.history")
        await TransmitScreenTests.barrier(model, next)
        #expect(rt.receivedMessages.first(where: { $0.id == 901 })?.text == nil)
        #expect(next.roundtableHistoryRequests.contains(where: { $0.contains(901) }))
        await model.disconnect()
    }

    @Test("exact directory preserves duplicate refs and nullable bearings without inventing sender Hz")
    func directoryAndScopedReadings() async throws {
        let (model, station, rt, _) = try await connected()
        func member(_ source: String, _ call: String, _ bearing: LinkJSON) -> LinkMessage.RecordBatch.Record {
            var record = FakeStation.roundtableMember(ref: "same", callsign: call, seq: 0)
            record.id = "member:" + source + "/same"
            record.fields["sourceRoom"] = .string(source)
            record.fields["hz"] = .number(14_200_010)
            record.fields["bearingDeg"] = bearing
            return record
        }
        func reading(_ source: String, _ dbm: Double) -> LinkMessage.RecordBatch.Record {
            var record = FakeStation.roundtableReading(ref: "same", dbm: dbm, t: 1000, turn: 9)
            record.id = "reading:" + source + "/same"
            record.fields["sourceRoom"] = .string(source)
            return record
        }
        var summary = FakeStation.roundtableSummary(id: Self.usb, members: 2)
        summary["stations"] = .array(["usb-14200000", "usb-14204000"].map { source in
            .object(["ref": .string("same"), "sourceRoom": .string(source), "slice": .number(1),
                     "hz": .number(14_200_010), "family": .string("usb"), "callsign": .string("W1AW"),
                     "nth": .number(1), "grid": .null, "keyed": .bool(false), "away": .bool(false),
                     "joining": .bool(false)])
        })
        // The directory retains every slice placement, while one scoped person
        // at the same exact Hz appears only once in a cochannel stack.
        if case .array(var stations)? = summary["stations"], case .object(var same) = stations[0] {
            same["slice"] = .number(2)
            stations.append(.object(same))
            summary["stations"] = .array(stations)
        }
        let scene = FakeStation.RoundtableScene(rooms: [summary], roomRecords: [Self.usb: [
            member("usb-14200000", "W1AW", .number(82)), reading("usb-14200000", -73),
            member("usb-14204000", "K4ABC", .number(82.5)), reading("usb-14204000", -91),
            FakeStation.roundtableMessage(id: 20, t: 1000, callsign: "Legacy", text: "No sender Hz"),
        ]], sliceRooms: [0: Self.usb])
        try await station.deliverRoundtable(scene)
        #expect(await settle { rt.room?.members.count == 2 && rt.directory.count == 3 })
        #expect(Set(rt.directory.map(\.id)).count == 2)
        let members = try #require(rt.room?.members)
        #expect(Set(members.map(\.id)).count == 2)
        #expect(members.first(where: { $0.callsign == "W1AW" })?.reading?.dbm == -73)
        #expect(members.first(where: { $0.callsign == "K4ABC" })?.reading?.dbm == -91)
        #expect(members.first(where: { $0.callsign == "W1AW" })?.bearingDeg == 82)
        #expect(members.first(where: { $0.callsign == "K4ABC" })?.bearingDeg == nil)
        #expect(rt.room?.messages.first?.hz == nil)
        var capabilities = model.mirror.capabilities.mapValues(\.wireValue)
        capabilities["stationRoundtableVersion"] = .i64(2)
        await station.deliver(.capabilities(.init(properties: capabilities.map { .init(name: $0.key, value: $0.value) })))
        await station.deliverRoundtableFrequencyCapability(true)
        #expect(await settle { rt.version == 2 && rt.frequencyAvailable })
        #expect(rt.frequencyRows.count == 1 && rt.frequencyRows.first?.stations.count == 2)
        let tags = RoundtableTagLayout.tags(rt, sliceIds: [])
        #expect(tags.count == 1 && tags.first?.hz == 14_200_010)
        #expect(tags.first?.label == "W1AW\nW1AW")
        let narrow = RoundtableTagLayout.layout([
            RoundtableTag(roomId: "earlier", hz: 10), RoundtableTag(roomId: "neighbor", hz: 20),
            RoundtableTag(roomId: "selected", hz: 90, mine: true)],
            hzPerPoint: 1, leftHz: 0, width: 100, tagWidth: 80, gap: 4)
        #expect(narrow.count == 1 && narrow.first?.roomIds == ["selected"] && narrow.first?.folded == false)
        await model.disconnect()
    }

    @Test("Core v2 uses honest legacy fallback until backend frequency metadata, including an empty directory")
    func backendFrequencyNegotiation() async throws {
        let (model, station, rt, _) = try await connected()
        var capabilities = model.mirror.capabilities.mapValues(\.wireValue)
        capabilities["stationRoundtableVersion"] = .i64(2)
        await station.deliver(.capabilities(.init(properties: capabilities.map { .init(name: $0.key, value: $0.value) })))
        try #require(await settle { rt.version == 2 })
        #expect(!rt.frequencyAvailable && rt.serverFrequencyVersion == 0)
        #expect(rt.frequencyReason?.contains("server") == true)
        _ = await rt.read(Self.usb)
        #expect(station.roundtableAnswered.contains("read"))
        #expect(!station.roundtableAnswered.contains("readAt"))
        await station.deliverRoundtableFrequencyCapability(true)
        try #require(await settle { rt.frequencyAvailable })
        #expect(rt.rooms.isEmpty && rt.frequencyRows.isEmpty)
        #expect(rt.serverFrequencyVersion == 1 && rt.frequencyReason == nil)
        await station.deliverRoundtableFrequencyCapability(false)
        #expect(await settle { !rt.frequencyAvailable })
        #expect(rt.rooms.isEmpty && rt.frequencyReason?.contains("server") == true)
        await model.disconnect()
    }

    @Test("mode families and path preferences preserve protocol meaning")
    func frequencyFamiliesAndPathDefaults() async throws {
        #expect(RoundtableModel.family(ofMode: "SAM") == "am")
        #expect(RoundtableModel.family(ofMode: "CWL") == "cw")
        #expect(RoundtableModel.family(ofMode: "CWU") == "cw")
        #expect(RoundtableModel.family(ofMode: "unrecognised") == nil)
        let (model, _, rt, _) = try await connected()
        #expect(rt.distanceUnits == "mi" && rt.showBearing)
        model.settings.apply(.settingsValue(.init(key: "Roundtable/DistanceUnits", origin: "other", properties: [.init(name: "Roundtable/DistanceUnits", value: .utf8("km"))])))
        model.settings.apply(.settingsValue(.init(key: "Roundtable/ShowBearing", origin: "other", properties: [.init(name: "Roundtable/ShowBearing", value: .utf8("False"))])))
        #expect(await settle { rt.distanceUnits == "km" && !rt.showBearing })
        #expect(RoundtableText.distance(128, units: "mi") == "80")
        #expect(RoundtableText.distance(128, units: "km") == "128")
        #expect(RoundtableText.distance(nil, units: "mi").isEmpty)
        await model.disconnect()
    }

    // MARK: Helpers

    /// The 20 m room alone, with `records`, slice 0 in it.
    private static func scene(_ records: [LinkMessage.RecordBatch.Record]) -> FakeStation.RoundtableScene {
        FakeStation.RoundtableScene(rooms: [FakeStation.roundtableSummary(id: usb, members: 1)],
                                    roomRecords: [usb: records], sliceRooms: [0: usb])
    }

    private func connected(additions: FakeStation.Additions = [.roundtable], replies: HeldCommandReplies? = nil) async throws
        -> (AppModel, FakeStation, RoundtableModel, TestLinkClock) {
        let defaults = try #require(UserDefaults(suiteName: "RoundtableModelTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: { endpoint, trust in
            let inner = station.transportFactory(endpoint, trust)
            return replies.map { ReplyHoldingTransport(inner: inner, replies: $0) } ?? inner
        })
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        let clock = TestLinkClock()
        let rt = RoundtableModel(mirror: model.mirror, commands: model.commands, records: model.records,
                                 clock: clock, settings: model.settings, catalogFeed: model.main.catalogFeed)
        #expect(await settle { rt.connected })
        // The model reads the session through the app's objects; the app lives as long as the test.
        held.model = model
        return (model, station, rt, clock)
    }

    /// The arguments of the next `verb` the app sent (that `matching` takes), or nil if none came.
    private func sent(_ station: FakeStation, _ verb: String,
                      matching: @escaping @Sendable ([LinkMessage.PropertyEntry]) -> Bool = { _ in true }) async
        -> [LinkMessage.PropertyEntry]? {
        await station.waitForMessage(within: .seconds(30)) { message in
            guard let invoke = AccessoryPagesTests.invoke(message) else {
                return false
            }
            return invoke.verb == verb && matching(invoke.args)
        }.flatMap(AccessoryPagesTests.invoke)?.args
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
}
