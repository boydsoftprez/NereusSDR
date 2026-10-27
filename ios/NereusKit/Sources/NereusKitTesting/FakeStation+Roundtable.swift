// NereusSDR for iOS: the fake Core's Roundtable: its roundtable object, room streams and verbs
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 16, the fake Core's Roundtable for the
// phone's tests. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusLink

/// The fake Core's Roundtable (link document, the Roundtable sections).
/// With ``Additions/roundtable`` it plays a Core that runs Roundtable, at
/// `stationRoundtableVersion` 1: ``deliverRoundtable(_:)`` sends the
/// `RoundtableState` schema and the `roundtable` object, then the band
/// list and the rooms to a device that follows them. It answers
/// `records.subscribe` and `records.unsubscribe` for `roundtableRooms` and
/// `roundtableRoom:<id>` with a reset of the newest records, and the
/// thirteen `roundtable.*` verbs by the Core's rules: a post is taken and
/// its message sent on the room's stream, a read names the room's stream,
/// a web sign-in answers with a link, and each refusal is in the Core's
/// words. Without the addition it plays a Core before Roundtable: no
/// capability, and each `roundtable.*` verb unknown. Nothing reaches a
/// Roundtable server.
extension FakeStation {
    public static let roundtableCapability = "stationRoundtableVersion"
    public static let roundtableClass = "RoundtableState"
    public static let roundtableKey = "roundtable"
    public static let roundtableRoomsStream = "roundtableRooms"
    public static let roundtableRoomStreamPrefix = "roundtableRoom:"
    /// The link the fake's `roundtable.webSignIn` answers with.
    public static let roundtableWebLink = "https://roundtable.invalid/sign-in/fake"

    // The Core's words (src/core/roundtable/RoundtableState.cpp, StationServer.cpp).
    public static let roundtableUnreadableReason = "The Core could not read this request."
    public static let roundtableNotConnectedReason = "Roundtable is not connected, so this can't be sent."
    public static let roundtableNotPostedReason = "Roundtable is not connected, so this can't be posted."
    public static let roundtableNoRoomReason = "Roundtable has no room there right now."
    public static let roundtableOffReason = "Roundtable is off on this Core."
    public static let roundtableMuteReason = "Roundtable can only mute a callsign."
    public static let roundtableNameReason = "Name a turn with the callsign you heard."
    public static let roundtableConfirmReason = "Type delete my archive to confirm."
    public static let roundtableDeleteArchiveWords = "delete my archive"

    /// The `roundtable` object's link states (RoundtableClient::LinkState).
    public enum RoundtableLink: Int64, Sendable {
        case off = 0
        case connecting = 1
        case live = 2
        case reconnecting = 3
        case offline = 4
        case hidden = 5
    }

    /// What the fake's Roundtable shows.
    public struct RoundtableScene: Sendable {
        /// The band list's summaries, each a `roundtableRooms` record's fields (without `record`).
        public var rooms: [[String: LinkJSON]]
        /// Each room's `roundtableRoom:<id>` records, oldest first, by room id.
        public var roomRecords: [String: [LinkMessage.RecordBatch.Record]]
        public var link: RoundtableLink
        public var linkSince: Int64
        public var firstRunAnswered: Bool
        public var hidden: Bool
        /// Room id to the mentions there not yet read.
        public var mentionCounts: [String: Int]
        /// Slice id to the room it is in, or nil.
        public var sliceRooms: [Int: String?]

        public init(rooms: [[String: LinkJSON]], roomRecords: [String: [LinkMessage.RecordBatch.Record]] = [:],
                    link: RoundtableLink = .live, linkSince: Int64 = 1_000, firstRunAnswered: Bool = true,
                    hidden: Bool = false, mentionCounts: [String: Int] = [:], sliceRooms: [Int: String?] = [0: nil]) {
            self.rooms = rooms
            self.roomRecords = roomRecords
            self.link = link
            self.linkSince = linkSince
            self.firstRunAnswered = firstRunAnswered
            self.hidden = hidden
            self.mentionCounts = mentionCounts
            self.sliceRooms = sliceRooms
        }

        /// Two rooms, 40 m CW and 20 m USB, slice 0 in the 20 m room with
        /// two members, a turn on the air, two ended turns and two messages,
        /// and a mention in the CW room.
        public static var board: RoundtableScene {
            let usb = "usb-14200000"
            let records: [LinkMessage.RecordBatch.Record] = [
                FakeStation.roundtableMember(ref: "a/1", callsign: "W1AW", seq: 0),
                FakeStation.roundtableReading(ref: "a/1", dbm: -73.5, t: 1_000),
                FakeStation.roundtableMember(ref: "b/0", callsign: "KG4VCF", seq: 1, you: true),
                FakeStation.roundtableTurn(id: 11, start: 900, end: nil, talkers: ["W1AW"]),
                FakeStation.roundtableName(turn: 11, name: "W1AW", count: 1),
                FakeStation.roundtableName(turn: 11, name: "K1ABC", count: 3),
                FakeStation.roundtableTurn(id: 9, start: 100, end: 400),
                FakeStation.roundtableTurn(id: 10, start: 450, end: 800),
                FakeStation.roundtableName(turn: 10, name: "N0CALL", count: 2),
                FakeStation.roundtableMessage(id: 21, t: 950, callsign: "W1AW", text: "good signal"),
                FakeStation.roundtableMessage(id: 20, t: 940, callsign: "KG4VCF", text: "hello all"),
            ]
            return RoundtableScene(
                rooms: [FakeStation.roundtableSummary(id: usb, members: 2),
                        FakeStation.roundtableSummary(id: "cw-7030000", members: 1)],
                roomRecords: [usb: records, "cw-7030000": [
                    FakeStation.roundtableMember(ref: "c/2", callsign: "K1ABC", seq: 0),
                ]],
                mentionCounts: ["cw-7030000": 1], sliceRooms: [0: usb])
        }
    }

    // MARK: Records, as the Core's streams carry them

    /// A `roundtableRooms` summary: the room's family and center come from its id.
    public static func roundtableSummary(id: String, members: Int, state: String = "talking",
                                         talkers: [String] = [], messages: Int = 0,
                                         last: Int64 = 1_000) -> [String: LinkJSON] {
        let parts = id.split(separator: "-", maxSplits: 1).map(String.init)
        let hz = Double(parts.count == 2 ? parts[1] : "0") ?? 0
        return ["id": .string(id), "band": .string("other"), "family": .string(parts.first ?? ""),
                "hz": .number(hz), "state": .string(state), "talkers": .array(talkers.map(LinkJSON.string)),
                "members": .number(Double(members)), "messages": .number(Double(messages)),
                "last": .number(Double(last)), "folded": .bool(false), "start": .null, "heard": .number(0),
                "readings": .array([]), "lastMessage": .null, "ended": .null]
    }

    public static func roundtableMember(ref: String, callsign: String, seq: Int64, you: Bool = false)
        -> LinkMessage.RecordBatch.Record {
        .init(id: "member:\(ref)", fields: [
            "record": .string("member"), "ref": .string(ref), "callsign": .string(callsign), "nth": .number(1),
            "grid": .string("EM73"), "km": .null, "offsetHz": .number(0), "joining": .bool(false),
            "away": .bool(false), "keyed": .bool(false), "software": .string("NereusSDR"),
            "softwareVersion": .string("26.10.3"), "you": .bool(you), "seq": .number(Double(seq)),
        ])
    }

    public static func roundtableReading(ref: String, dbm: Double, t: Int64, turn: Int64? = nil)
        -> LinkMessage.RecordBatch.Record {
        .init(id: "reading:\(ref)", fields: [
            "record": .string("reading"), "ref": .string(ref), "dbm": .number(dbm), "t": .number(Double(t)),
            "present": .bool(true), "turn": turn.map { .number(Double($0)) } ?? .null,
        ])
    }

    /// A turn without its names (each is a record of its own); `end` nil while it is open.
    public static func roundtableTurn(id: Int64, start: Int64, end: Int64?, talkers: [String] = [])
        -> LinkMessage.RecordBatch.Record {
        let talking: [LinkJSON] = talkers.map { callsign in
            .object(["ref": .null, "callsign": .string(callsign), "nth": .number(1)])
        }
        return .init(id: "turn:\(id)", fields: [
            "record": .string("turn"), "id": .number(Double(id)), "kind": .string("talking"),
            "talkers": .array(talking), "start": .number(Double(start)),
            "end": end.map { .number(Double($0)) } ?? .null, "ending": .bool(false), "heard": .number(0),
            "mine": .null,
        ])
    }

    public static func roundtableName(turn: Int64, name: String, count: Int64) -> LinkMessage.RecordBatch.Record {
        .init(id: "name:\(turn):\(name)", fields: [
            "record": .string("name"), "turn": .number(Double(turn)), "name": .string(name),
            "count": .number(Double(count)),
        ])
    }

    public static func roundtableMessage(id: Int64, t: Int64, callsign: String, text: String)
        -> LinkMessage.RecordBatch.Record {
        .init(id: "message:\(id)", fields: [
            "record": .string("message"), "id": .number(Double(id)), "ref": .null, "callsign": .string(callsign),
            "nth": .number(1), "t": .number(Double(t)), "state": .string("shown"), "text": .string(text),
            "dbm": .null,
        ])
    }

    // MARK: State

    /// One `roundtable.post` the fake took.
    public struct RoundtablePost: Sendable, Equatable {
        public let roomId: String
        public let text: String
        public let clientId: String
        public let messageId: String

        public init(roomId: String, text: String, clientId: String, messageId: String) {
            self.roomId = roomId
            self.text = text
            self.clientId = clientId
            self.messageId = messageId
        }
    }

    /// What the fake's Roundtable is doing now, behind its own lock.
    final class RoundtableFakeState: @unchecked Sendable {
        struct Inside {
            var scene = RoundtableScene(rooms: [])
            var delivered = false
            var followed: Set<String> = []
            var read: [String] = []
            var posts: [RoundtablePost] = []
            var nextMessageId: Int64 = 1_000
            var muteList: [String] = []
            var mentionsSeenMs: Int64 = 0
            var answered: [String] = []
        }

        private let lock = NSLock()
        private var inside = Inside()

        func read<Result>(_ body: (inout Inside) -> Result) -> Result {
            lock.withLock { body(&inside) }
        }
    }

    static func roundtableCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        additions.contains(.roundtable) ? [(roundtableCapability, 1)] : []
    }

    /// The streams the app follows now, sorted.
    public var roundtableFollowed: [String] {
        roundtableState.read { $0.followed.sorted() }
    }

    /// Each room the app asked to read, oldest first.
    public var roundtableReads: [String] {
        roundtableState.read { $0.read }
    }

    /// Each post the fake took, oldest first.
    public var roundtablePosts: [RoundtablePost] {
        roundtableState.read { $0.posts }
    }

    /// The base callsigns muted at the fake Core, in the order they were muted.
    public var roundtableMuteList: [String] {
        roundtableState.read { $0.muteList }
    }

    /// The mentions read up to (Unix ms), as the Core keeps it: it only moves forward.
    public var roundtableMentionsSeenMs: Int64 {
        roundtableState.read { $0.mentionsSeenMs }
    }

    /// Each `roundtable.*` verb the fake answered, oldest first, without its prefix.
    public var roundtableAnswered: [String] {
        roundtableState.read { $0.answered }
    }

    /// Sets the scene and sends it as a Core at `stationRoundtableVersion` 1
    /// would: the class's schema and the `roundtable` object, then a reset
    /// of each stream the app follows.
    public func deliverRoundtable(_ scene: RoundtableScene = .board) async throws {
        guard additions.contains(.roundtable) else {
            return
        }
        let resets = roundtableState.read { inside -> [LinkMessage] in
            inside.scene = scene
            inside.delivered = true
            return inside.followed.sorted().map { Self.roundtableReset(inside.scene, stream: $0) }
        }
        await deliver(.schema(try Self.schema(ofClass: Self.roundtableClass)))
        await deliver(.objectCreate(try Self.objectCreate(key: Self.roundtableKey, className: Self.roundtableClass,
                                                          values: Self.roundtableValues(scene))))
        for reset in resets {
            await deliver(reset)
        }
    }

    /// The `roundtable` object changes as `change` says, sent as the Core's delta.
    public func deliverRoundtableState(_ change: @Sendable (inout RoundtableScene) -> Void) async {
        let values = roundtableState.read { inside -> [String: LinkMessage.PropertyValue] in
            change(&inside.scene)
            return Self.roundtableValues(inside.scene)
        }
        await deliver(Self.roundtableDelta(values))
    }

    /// News on one room's stream: its records upserted (a record of the same
    /// id replaced) and removed, sent to an app that follows it.
    public func deliverRoundtableRoom(_ roomId: String, upserts: [LinkMessage.RecordBatch.Record],
                                      removes: [String] = []) async {
        let stream = Self.roundtableRoomStreamPrefix + roomId
        let batch = roundtableState.read { inside -> LinkMessage? in
            var records = inside.scene.roomRecords[roomId] ?? []
            for record in upserts {
                if let index = records.firstIndex(where: { $0.id == record.id }) {
                    records[index] = record
                } else {
                    records.append(record)
                }
            }
            records.removeAll { removes.contains($0.id) }
            inside.scene.roomRecords[roomId] = records
            guard inside.followed.contains(stream) else {
                return nil
            }
            return .recordBatch(LinkMessage.RecordBatch(stream: stream, generation: 1, reset: false,
                                                        upserts: upserts, removes: removes))
        }
        if let batch {
            await deliver(batch)
        }
    }

    /// The band list's news: summaries upserted by room id, and rooms removed.
    public func deliverRoundtableRooms(upserts: [[String: LinkJSON]], removes: [String] = []) async {
        let batch = roundtableState.read { inside -> LinkMessage? in
            for summary in upserts {
                let id = Self.roomId(of: summary)
                if let index = inside.scene.rooms.firstIndex(where: { Self.roomId(of: $0) == id }) {
                    inside.scene.rooms[index] = summary
                } else {
                    inside.scene.rooms.append(summary)
                }
            }
            inside.scene.rooms.removeAll { removes.contains(Self.roomId(of: $0)) }
            guard inside.followed.contains(Self.roundtableRoomsStream) else {
                return nil
            }
            return .recordBatch(LinkMessage.RecordBatch(stream: Self.roundtableRoomsStream, generation: 1,
                                                        reset: false, upserts: upserts.map(Self.roomRecord),
                                                        removes: removes))
        }
        if let batch {
            await deliver(batch)
        }
    }

    // MARK: Answers

    /// The fake's answer to a Roundtable verb or stream; nil leaves it to the rest of the fake.
    func roundtableReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        func text(_ name: String) -> String? {
            if case .utf8(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        func flag(_ name: String) -> Bool? {
            if case .bool(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        func whole(_ name: String) -> Int64? {
            if case .i64(let value)? = invoke.args.first(where: { $0.name == name })?.value {
                return value
            }
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "",
                    values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: [], values: values))
        }
        let ownVerb = invoke.verb.hasPrefix("roundtable.")
        let stream = text("stream") ?? ""
        let ownStream = ["records.subscribe", "records.unsubscribe"].contains(invoke.verb)
            && (stream == Self.roundtableRoomsStream || stream.hasPrefix(Self.roundtableRoomStreamPrefix))
        guard ownVerb || ownStream else {
            return nil
        }
        guard additions.contains(.roundtable) else {
            // A Core before Roundtable knows none of its verbs; its streams are another part's.
            return ownVerb ? [result(false, Self.unknownVerbReason)] : nil
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        return roundtableState.read { inside -> [LinkMessage] in
            if ownVerb {
                inside.answered.append(String(invoke.verb.dropFirst("roundtable.".count)))
            }
            let live = inside.scene.link == .live
            let listed = Set(inside.scene.rooms.map(Self.roomId(of:)))
            switch invoke.verb {
            case "records.subscribe":
                inside.followed.insert(stream)
                return [result(true), Self.roundtableReset(inside.scene, stream: stream)]
            case "records.unsubscribe":
                inside.followed.remove(stream)
                return [result(true)]
            case "roundtable.post":
                guard let roomId = text("roomId"), let message = text("text"), let clientId = text("clientId"),
                      !roomId.isEmpty, !message.isEmpty, Self.isClientId(clientId) else {
                    return [result(false, Self.roundtableUnreadableReason)]
                }
                guard live else {
                    return [result(false, Self.roundtableNotPostedReason)]
                }
                inside.nextMessageId += 1
                let id = inside.nextMessageId
                inside.posts.append(RoundtablePost(roomId: roomId, text: message, clientId: clientId,
                                                   messageId: String(id)))
                let record = Self.roundtableMessage(id: id, t: inside.scene.linkSince, callsign: "KG4VCF",
                                                    text: message)
                inside.scene.roomRecords[roomId, default: []].append(record)
                var replies = [result(true, values: [.init(name: "messageId", value: .utf8(String(id)))])]
                let roomStream = Self.roundtableRoomStreamPrefix + roomId
                if inside.followed.contains(roomStream) {
                    replies.append(.recordBatch(LinkMessage.RecordBatch(stream: roomStream, generation: 1,
                                                                        reset: false, upserts: [record],
                                                                        removes: [])))
                }
                return replies
            case "roundtable.read":
                guard let roomId = text("roomId") else {
                    return [result(false, Self.roundtableUnreadableReason)]
                }
                guard listed.contains(roomId) else {
                    return [result(false, Self.roundtableNoRoomReason)]
                }
                inside.read.append(roomId)
                return [result(true, values: [.init(name: "stream",
                                                    value: .utf8(Self.roundtableRoomStreamPrefix + roomId))])]
            case "roundtable.unread":
                guard text("roomId") != nil else {
                    return [result(false, Self.roundtableUnreadableReason)]
                }
                return [result(true)]
            case "roundtable.delete":
                guard let id = text("messageId"), Self.isWireId(id) else {
                    return [result(false, Self.roundtableUnreadableReason)]
                }
                return [result(live, live ? "" : Self.roundtableNotConnectedReason)]
            case "roundtable.name":
                guard let turn = text("turnId"), Self.isWireId(turn) else {
                    return [result(false, Self.roundtableUnreadableReason)]
                }
                guard let name = text("name"), Self.looksLikeCallsign(name) else {
                    return [result(false, Self.roundtableNameReason)]
                }
                return [result(live, live ? "" : Self.roundtableNotConnectedReason)]
            case "roundtable.report":
                guard let kind = text("kind"), let target = text("target"), text("reason") != nil,
                      ["message", "turn", "station"].contains(kind),
                      kind == "station" ? target.contains("/") : Self.isWireId(target) else {
                    return [result(false, Self.roundtableUnreadableReason)]
                }
                return [result(live, live ? "" : Self.roundtableNotConnectedReason)]
            case "roundtable.setHidden":
                guard let hidden = flag("hidden") else {
                    return [result(false, Self.roundtableUnreadableReason)]
                }
                inside.scene.hidden = hidden
                return [result(true), Self.roundtableDelta(Self.roundtableValues(inside.scene))]
            case "roundtable.answerFirstRun":
                guard flag("live") != nil else {
                    return [result(false, Self.roundtableUnreadableReason)]
                }
                inside.scene.firstRunAnswered = true
                return [result(true), Self.roundtableDelta(Self.roundtableValues(inside.scene))]
            case "roundtable.mute", "roundtable.unmute":
                guard let callsign = text("callsign"), Self.looksLikeCallsign(callsign) else {
                    return [result(false, Self.roundtableMuteReason)]
                }
                let base = Self.baseCallsign(callsign)
                if invoke.verb == "roundtable.mute" {
                    if !inside.muteList.contains(base) {
                        inside.muteList.append(base)
                    }
                } else {
                    inside.muteList.removeAll { $0 == base }
                }
                return [result(true)]
            case "roundtable.markMentionRead":
                guard let upTo = whole("upToMs"), upTo >= 0 else {
                    return [result(false, Self.roundtableUnreadableReason)]
                }
                // The mark only moves forward: read on one device is read on all.
                inside.mentionsSeenMs = max(inside.mentionsSeenMs, upTo)
                return [result(true)]
            case "roundtable.webSignIn":
                guard inside.scene.link != .off else {
                    return [result(false, Self.roundtableOffReason)]
                }
                return [result(true, values: [.init(name: "link", value: .utf8(Self.roundtableWebLink))])]
            case "roundtable.deleteArchive":
                guard text("confirm") == Self.roundtableDeleteArchiveWords else {
                    return [result(false, Self.roundtableConfirmReason)]
                }
                guard inside.scene.link != .off else {
                    return [result(false, Self.roundtableOffReason)]
                }
                return [result(true)]
            default:
                return [result(false, Self.unknownVerbReason)]
            }
        }
    }

    // MARK: Inside

    private static func roomId(of summary: [String: LinkJSON]) -> String {
        if case .string(let id)? = summary["id"] {
            return id
        }
        return ""
    }

    private static func roomRecord(_ summary: [String: LinkJSON]) -> LinkMessage.RecordBatch.Record {
        var fields = summary
        fields["record"] = .string("room")
        return .init(id: roomId(of: summary), fields: fields)
    }

    private static func roundtableReset(_ scene: RoundtableScene, stream: String) -> LinkMessage {
        let records: [LinkMessage.RecordBatch.Record]
        if stream == roundtableRoomsStream {
            records = scene.rooms.map(roomRecord)
        } else {
            records = scene.roomRecords[String(stream.dropFirst(roundtableRoomStreamPrefix.count))] ?? []
        }
        return .recordBatch(LinkMessage.RecordBatch(stream: stream, generation: 1, reset: true, upserts: records,
                                                    removes: []))
    }

    static func roundtableValues(_ scene: RoundtableScene) -> [String: LinkMessage.PropertyValue] {
        let mentions = LinkJSON.object(scene.mentionCounts.mapValues { .number(Double($0)) }).compactText
        var slices: [String: LinkJSON] = [:]
        for (slice, room) in scene.sliceRooms {
            slices[String(slice)] = room.map(LinkJSON.string) ?? .null
        }
        return [
            "linkState": .enumeration(scene.link.rawValue), "linkSince": .i64(scene.linkSince),
            "firstRunAnswered": .bool(scene.firstRunAnswered), "hidden": .bool(scene.hidden),
            "mentionCounts": .utf8(mentions), "sliceRooms": .utf8(LinkJSON.object(slices).compactText),
        ]
    }

    private static func roundtableDelta(_ values: [String: LinkMessage.PropertyValue]) -> LinkMessage {
        let order = ["linkState", "linkSince", "firstRunAnswered", "hidden", "mentionCounts", "sliceRooms"]
        return .delta(LinkMessage.Delta(key: roundtableKey, properties: order.enumerated().compactMap { ordinal, name in
            values[name].map { LinkMessage.PropertyEntry(ordinal: UInt16(ordinal), name: name, value: $0) }
        }))
    }

    private static func isClientId(_ text: String) -> Bool {
        (1...64).contains(text.utf8.count) && text.utf8.allSatisfy { (0x21...0x7E).contains($0) }
    }

    private static func isWireId(_ text: String) -> Bool {
        guard let value = Int64(text), String(value) == text else {
            return false
        }
        return value >= 1
    }

    private static func looksLikeCallsign(_ text: String) -> Bool {
        let trimmed = text.trimmingCharacters(in: .whitespaces)
        return !trimmed.isEmpty && trimmed.allSatisfy { $0.isASCII && ($0.isLetter || $0.isNumber || $0 == "/") }
            && trimmed.contains(where: \.isNumber)
    }

    private static func baseCallsign(_ callsign: String) -> String {
        var longest = ""
        for part in callsign.trimmingCharacters(in: .whitespaces).split(separator: "/", omittingEmptySubsequences: false)
            where part.count > longest.count {
            longest = String(part)
        }
        return longest.uppercased()
    }
}

extension FakeStation.Additions {
    /// Roundtable at `stationRoundtableVersion` 1 (FakeStation+Roundtable.swift). Not in ``all``.
    public static let roundtable = FakeStation.Additions(rawValue: 1 << 54)
}
