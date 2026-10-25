// NereusSDR for iOS: Roundtable's link model against a fake Core that runs it: rooms, reading, posts and windows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 16, the phone's link model tests, the
// windows tested as tests/tst_roundtable_model.cpp tests the desktop's.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

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
        #expect(await rt.post("hello", in: Self.usb) == .refused(RoundtableModel.olderCoreReason))
        #expect(await rt.read(Self.usb) == RoundtableModel.olderCoreReason)
        #expect(await rt.markMentionRead(upToMs: 5) == RoundtableModel.olderCoreReason)
        #expect(await rt.webSignIn() == nil)
        #expect(rt.readingRoomId == nil)
        await TransmitScreenTests.barrier(model, station)
        #expect(!model.records.isWanted("roundtableRooms"))
        #expect(!model.records.isWanted("roundtableRoom:\(Self.usb)"))
        let sent = station.messages.compactMap(AccessoryPagesTests.invoke)
        #expect(!sent.contains { $0.verb.hasPrefix("roundtable.") })
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

    // MARK: Helpers

    /// The 20 m room alone, with `records`, slice 0 in it.
    private static func scene(_ records: [LinkMessage.RecordBatch.Record]) -> FakeStation.RoundtableScene {
        FakeStation.RoundtableScene(rooms: [FakeStation.roundtableSummary(id: usb, members: 1)],
                                    roomRecords: [usb: records], sliceRooms: [0: usb])
    }

    private func connected(additions: FakeStation.Additions = [.roundtable]) async throws
        -> (AppModel, FakeStation, RoundtableModel, TestLinkClock) {
        let defaults = try #require(UserDefaults(suiteName: "RoundtableModelTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        let clock = TestLinkClock()
        let rt = RoundtableModel(mirror: model.mirror, commands: model.commands, records: model.records,
                                 clock: clock)
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
