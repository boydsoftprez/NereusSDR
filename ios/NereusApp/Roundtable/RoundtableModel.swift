// NereusSDR for iOS: Roundtable as the Core runs it: the band list, the room in view, and the operator's actions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 16, the phone's link model, read as the
// desktop's RoundtableModel reads a window's streams. J.J. Boyd (KG4VCF),
// AI-assisted via Claude Code.
// 2026-10-24: Roundtable plan Task 17, the app that owns the model: once it
// is freed the model is no longer connected. J.J. Boyd (KG4VCF),
// AI-assisted via Claude Code.
// 2026-10-25: Roundtable fix wave: a room built by view(of:) is kept until
// its records change or an earlier turn leaves the window, so the flag
// lines, the tags and the sheet do not sort it again on every pass.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

// 2026-10-30: Frequency views, immutable sender contexts and shared path preferences.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import Combine
import Foundation
import NereusLink
import NereusMirror
import os

/// Roundtable on the phone (R-RT-15, R-RT-17, design 10.3 and 11.3). The
/// Core holds the Roundtable connection; the phone reads the `roundtable`
/// object (the link state, the first-run answer, Hide my station, mention
/// counts and the room each slice is in), the band list as the
/// `roundtableRooms` record stream and each room in view as its
/// `roundtableRoom:<id>` stream, and acts through the thirteen
/// `roundtable.*` verbs. It holds the same rooms the desktop's window
/// holds, read the same way (src/models/RoundtableModel.cpp): the band list
/// by center frequency, a room's members in joining order, its turns on the
/// air oldest first, the turns that ended in the last 30 minutes newest
/// first, each turn's names by count then name, and its last 50 messages;
/// and the names used on a room's frequency in the last 24 hours.
///
/// Version 2 follows the selected slice's actual integer Hz and catalogue
/// mode family, or an explicitly browsed frequency, using readAt/unreadAt
/// and the Core's returned frequency stream. The directory preserves exact
/// slice presence; received chat retains immutable sender context through
/// tuning. Version 1 keeps the legacy room path and labels its limitation.
@MainActor
final class RoundtableModel: ObservableObject {
    // MARK: What the screens read

    /// The `roundtable` object's link states (RoundtableClient::LinkState).
    enum LinkState: Int64, Equatable, Sendable {
        case off = 0
        case connecting = 1
        case live = 2
        case reconnecting = 3
        case offline = 4
        case hidden = 5
    }

    /// The last message a band list row shows.
    struct LastMessage: Equatable, Sendable {
        var callsign: String
        var nth: Int64
        var t: Int64
        var state: String
        var text: String?
    }

    struct FrequencyContext: Equatable, Hashable, Sendable {
        var sliceId: Int64
        var hz: Int64
        var family: String
        var id: String { "\(family)-\(hz)" }
    }

    struct Station: Equatable, Identifiable, Sendable {
        var ref: String
        var sourceRoom: String
        var slice: Int64
        var hz: Int64
        var family: String
        var callsign: String
        var nth: Int64
        var grid: String?
        var keyed: Bool
        var away: Bool
        var joining: Bool
        var id: String { sourceRoom + "/" + ref }
    }

    /// One room in the band list (wire spec 5.2, summary).
    struct RoomSummary: Equatable, Identifiable, Sendable {
        var id: String
        var band: String
        var family: String
        var hz: Int64
        var state: String
        var talkers: [String]
        var members: Int64
        var messages: Int64
        var last: Int64
        var folded: Bool
        var start: Int64?
        var heard: Int64
        var readings: [Double]
        var lastMessage: LastMessage?
        var ended: Int64?
        var stations: [Station] = []
    }

    /// A member's newest reading.
    struct Reading: Equatable, Sendable {
        var dbm: Double
        var t: Int64
        var present: Bool
        var turn: Int64?
    }

    struct Member: Equatable, Identifiable, Sendable {
        var ref: String
        var callsign: String
        var nth: Int64
        var grid: String?
        var km: Int64?
        var offsetHz: Int64
        var joining: Bool
        var away: Bool
        var keyed: Bool
        var software: String
        var softwareVersion: String
        var you: Bool
        var reading: Reading?
        var hz: Int64? = nil
        var sourceRoom: String? = nil
        var slice: Int64? = nil
        var bearingDeg: Int64? = nil
        var id: String { (sourceRoom.map { $0 + "/" } ?? "") + ref }
    }

    struct Talker: Equatable, Sendable {
        var ref: String?
        var callsign: String
        var nth: Int64
        var hz: Int64? = nil
        var grid: String? = nil
        var sourceRoom: String? = nil
        var identity: String? { ref.map { (sourceRoom.map { $0 + "/" } ?? "") + $0 } }
    }

    struct NameCount: Equatable, Sendable {
        var name: String
        var count: Int64
    }

    struct Turn: Equatable, Identifiable, Sendable {
        var id: Int64
        var kind: String
        var talkers: [Talker]
        var start: Int64
        /// Nil while the turn is on the air.
        var end: Int64?
        var ending: Bool
        /// By count, highest first, then by name.
        var names: [NameCount]
        var heard: Int64
        var mine: String?
        var hz: Int64? = nil
        var sourceRoom: String? = nil
    }

    struct Message: Equatable, Identifiable, Sendable {
        var id: Int64
        var ref: String?
        var callsign: String
        var nth: Int64
        var t: Int64
        var state: String
        var text: String?
        var dbm: Double?
        var hz: Int64? = nil
        var family: String? = nil
        var sourceRoom: String? = nil
    }

    /// One room in view, as the desktop's room() reads it.
    struct Room: Equatable, Identifiable, Sendable {
        var id: String
        var band: String
        var family: String
        var hz: Int64
        /// In joining order.
        var members: [Member]
        /// On the air, oldest first.
        var now: [Turn]
        /// Ended in the last 30 minutes, newest first.
        var earlier: [Turn]
        /// The last 50, oldest first.
        var messages: [Message]
        /// Exact neighborhoods can contain several independent RF turns.
        var frequencyView = false
    }

    /// A post's answer: the message the Core's Roundtable took, or why not.
    enum PostResult: Equatable, Sendable {
        case posted(messageId: String)
        case refused(String)
    }

    // MARK: Constants

    static let capabilityName = "stationRoundtableVersion"
    static let objectKey = "roundtable"
    static let roomsStream = "roundtableRooms"
    static let roomStreamPrefix = RecordStreamClient.roundtableRoomStreamPrefix
    static let frequencyStreamPrefix = RecordStreamClient.roundtableFrequencyStreamPrefix
    static let roomsCapacity = 500
    static let roomCapacity = RecordStreamClient.roundtableRoomCapacity
    /// Wire spec 5.4.5: a room view holds the last 50 messages.
    static let roomMessages = 50
    /// Wire spec 5.4.6: the earlier turns are those that ended in the last 30 minutes.
    static let earlierWindowMs: Int64 = 30 * 60 * 1000
    /// Design 8.6: the names used on a frequency in the last 24 hours.
    static let recentNamesWindowMs: Int64 = 24 * 60 * 60 * 1000
    static let commandTimeout: Duration = .seconds(5)
    /// A post and the web sign-in wait for Roundtable itself, which the Core gives 30 s.
    static let answerTimeout: Duration = .seconds(35)
    static let deleteArchiveWords = "delete my archive"

    static let olderCoreReason = "This Core does not run Roundtable for this app. Updating the Core may help."
    static let notConnectedReason = SpotsModel.notConnectedReason
    static let noAnswerReason = "The Core did not answer this request."

    // MARK: Published

    /// The Core's `stationRoundtableVersion` (0 for a Core without Roundtable).
    @Published private(set) var version: Int64 = 0
    /// The snapshot is complete and fresh.
    @Published private(set) var connected = false
    /// Changes across disconnect/reconnect, even when a view coalesces those state changes.
    @Published private(set) var connectionGeneration: UInt64 = 0
    /// Start and Stop use the existing spot-source verbs, gated separately by recordStreamVersion.
    @Published private(set) var sourceControlVersion: Int64 = 0
    /// The band list, by center frequency, then id.
    @Published private(set) var rooms: [RoomSummary] = []
    @Published private(set) var sliceContexts: [Int64: FrequencyContext] = [:]
    @Published private(set) var distanceUnits = "mi"
    @Published private(set) var showBearing = true
    @Published private(set) var receivedMessages: [Message] = []
    /// Retained text stays visible while its deletion/moderation status refreshes.
    @Published private(set) var historyStatus: String?
    @Published private(set) var serverFrequencyVersion: Int64 = 0
    var frequencyAvailable: Bool { version >= 2 && serverFrequencyVersion >= 1 }
    var directory: [Station] { rooms.flatMap(\.stations) }
    var frequencyRows: [RoomSummary] {
        guard frequencyAvailable else { return rooms }
        let grouped = Dictionary(grouping: directory) { "\($0.family)-\($0.hz)" }
        return grouped.map { id, placements in
            // A station's two slices at the same exact context are one person.
            // The scoped ref retains distinct stations with duplicate callsigns.
            let people = placements.reduce(into: [Station]()) { people, station in
                if let index = people.firstIndex(where: { $0.id == station.id }) {
                    people[index].keyed = people[index].keyed || station.keyed
                } else { people.append(station) }
            }
            let first = people[0]
            return RoomSummary(id: id, band: Self.bandOf(first.hz), family: first.family, hz: first.hz,
                               state: people.contains(where: \.keyed) ? "talking" : "quiet",
                               talkers: people.map { RoundtableText.withNth($0.callsign, $0.nth) },
                               members: Int64(people.count), messages: 0, last: 0, folded: false,
                               start: nil, heard: 0, readings: [], lastMessage: nil, ended: nil, stations: people)
        }.sorted { ($0.hz, $0.id) < ($1.hz, $1.id) }
    }
    var activeContext: FrequencyContext? {
        (activeSliceId ?? sliceContexts.keys.sorted().first).flatMap { sliceContexts[$0] }
    }
    var frequencyReason: String? {
        if frequencyAvailable { return nil }
        return version >= 2
            ? "The Roundtable server provides legacy room views. Exact-frequency chat requires a newer Roundtable server."
            : "This Core provides legacy room views. Exact-frequency chat requires a newer Core."
    }
    func setDistanceUnits(_ units: String) async {
        guard ["mi", "km"].contains(units), let settings else { return }
        let outcome = await settings.write("Roundtable/DistanceUnits", units)
        note = outcome.propertyOutcome.accepted ? nil : outcome.propertyOutcome.reason
    }
    func setShowBearing(_ show: Bool) async {
        guard let settings else { return }
        let outcome = await settings.write("Roundtable/ShowBearing", show ? "True" : "False")
        note = outcome.propertyOutcome.accepted ? nil : outcome.propertyOutcome.reason
    }

    /// Each slice's room, by slice id; a slice in no room is left out.
    @Published private(set) var sliceRooms: [Int64: String] = [:]
    /// The slice whose room the screen shows; nil follows the lowest slice in a room.
    @Published var activeSliceId: Int64? {
        didSet {
            if activeSliceId != oldValue {
                readAll()
            }
        }
    }
    /// The room the active slice is in, or nil.
    @Published private(set) var sliceRoomId: String?
    /// The room this phone reads (``read(_:)``), or nil.
    @Published private(set) var readingRoomId: String?
    /// The room in view: the one read, else the slice's.
    @Published private(set) var room: Room?
    @Published private(set) var linkState: LinkState = .off
    @Published private(set) var linkSince: Int64 = 0
    @Published private(set) var firstRunAnswered = false
    @Published private(set) var hidden = false
    /// Room id to its mentions not yet read.
    @Published private(set) var mentionCounts: [String: Int] = [:]
    /// What this phone has typed in each room's box (not kept).
    @Published var drafts: [String: String] = [:]
    /// The newest refusal or failure, in the Core's words where it gave them.
    @Published private(set) var note: String?

    /// The actions reach the Core's Roundtable.
    var available: Bool {
        version >= 1 && connected
    }

    /// Why the actions can't reach the Core's Roundtable, or nil.
    var coreReason: String? {
        if !connected {
            return Self.notConnectedReason
        }
        return version >= 1 ? nil : Self.olderCoreReason
    }

    /// As on the desktop, any state other than Off can be stopped, including a reconnect.
    var sourceRunning: Bool { linkState != .off }
    var sourceControlTitle: String { sourceRunning ? "Stop" : "Start" }

    var sourceControlReason: String? {
        coreReason ?? (sourceControlVersion >= 1 ? nil : Self.olderCoreReason)
    }

    /// The same Core source commands as SpotSourceHost::startRoundtable/stopRoundtable.
    /// The mirrored link state remains authoritative; accepting Start is not proof it is live.
    func setRunning(_ running: Bool) async -> String? {
        if let reason = sourceControlReason {
            note = reason
            return reason
        }
        return await act(running ? "spots.connect" : "spots.disconnect",
                         [CommandArgument(name: "source", value: .text("roundtable"))])
    }

    // MARK: Inside

    private static let logger = Logger(subsystem: "NereusSDR", category: "roundtable")

    /// A room as its stream holds it: members with their joining order.
    private struct View {
        var members: [(seq: Int64, member: Member)] = []
        var turns: [Int64: Turn] = [:]
        var messages: [Int64: Message] = [:]
    }

    private struct RecentName {
        var name: String
        var usedMs: Int64
    }

    /// Who wants each room stream in the record client.
    private final class Owner {}

    private let catalogFeed: CatalogFeed?
    private let settings: SettingsProxyClient?
    private let mirror: MirrorStore
    private let commands: CommandClient
    private let records: RecordStreamClient
    private let clock: any LinkClock
    private var watch: ToolMirrorWatch?
    private var cancellables: Set<AnyCancellable> = []
    private let sliceOwner = Owner()
    private let readOwner = Owner()
    static let retainedMessageLimit = 200
    private var retainedMessages: [Int64: Message] = [:]
    private var historyRevisions: [Int64: UInt64] = [:]
    private var historyLiveVersion: UInt64 = 0
    private var historyValidated: [Int64: UInt64] = [:]
    private var historyTask: Task<Void, Never>?
    private var historyTimer: (any LinkTimer)?
    private var historySerial: UInt64 = 0
    private var historyGeneration: UInt64?
    private var historyContext: String?
    private var historySliceId: Int64?

    private struct HistoryState: Decodable {
        let id: Int64
        let state: String
        let text: String?
        enum CodingKeys: String, CodingKey { case id, state, text }
        init(from decoder: any Decoder) throws {
            let fields = try decoder.container(keyedBy: CodingKeys.self)
            id = try fields.decode(Int64.self, forKey: .id)
            state = try fields.decode(String.self, forKey: .state)
            guard fields.contains(.text) else { throw DecodingError.keyNotFound(CodingKeys.text,
                .init(codingPath: decoder.codingPath, debugDescription: "Missing history text state")) }
            text = try fields.decodeIfPresent(String.self, forKey: .text)
        }
    }
    private var views: [String: View] = [:] {
        didSet {
            // Any change to a room's records makes each room built from them anew.
            builtRooms.removeAll(keepingCapacity: true)
        }
    }
    /// A room as ``view(of:)`` last built it, until its records change or
    /// its oldest earlier turn leaves the 30 minutes: the flag lines, the
    /// band's tags and the sheet ask for it on every pass.
    private struct BuiltRoom {
        let builtMs: Int64
        let validUntilMs: Int64
        let room: Room
    }
    private var builtRooms: [String: BuiltRoom] = [:]
    #if DEBUG
    /// Each room ``view(of:)`` built rather than reused (the tests).
    private(set) var roomBuildsForTesting = 0
    #endif
    /// Each room stream's records as last read, to rebuild only what changed.
    private var seen: [String: [LinkMessage.RecordBatch.Record]] = [:]
    private var recentNames: [String: [String: RecentName]] = [:]
    /// The room streams wanted for the slices.
    private var sliceStreams: Set<String> = []
    /// The stream the Core named for the room read, once it answered.
    private var readStream: String?
    /// The read room's stream as wanted now.
    private var readWanted: String?
    private var readSerial = 0
    /// The room was being read when the link went: read it again on the next.
    private var readAgain = false
    private var frequencyWanted: String?
    private var frequencyTask: Task<Void, Never>?
    private var pruneTimer: (any LinkTimer)?

    /// The app whose session the mirror and the clients carry, when one
    /// was named: once it is freed its session is gone, and the model
    /// reads as not connected, whatever the mirror last said.
    private weak var owner: AnyObject?
    private let owned: Bool

    /// Tells the model, on the main actor, when the owner it is tied to is freed.
    private final class OwnerWatch {
        let gone: @Sendable () -> Void
        init(gone: @escaping @Sendable () -> Void) {
            self.gone = gone
        }
        deinit {
            gone()
        }
    }

    private static var ownerWatchKey: UInt8 = 0

    /// `owner` is the app whose session `mirror`, `commands` and `records`
    /// belong to; the model holds it weakly and stops reading as connected
    /// once it is freed.
    init(mirror: MirrorStore, commands: CommandClient, records: RecordStreamClient,
         clock: any LinkClock = RoundtableWallClock(), settings: SettingsProxyClient? = nil, catalogFeed: CatalogFeed? = nil,
         owner: AnyObject? = nil) {
        self.catalogFeed = catalogFeed
        self.settings = settings
        self.mirror = mirror
        self.commands = commands
        self.records = records
        self.clock = clock
        self.owner = owner
        owned = owner != nil
        if let owner {
            let watch = OwnerWatch { [weak self] in
                Task { @MainActor [weak self] in self?.readAll() }
            }
            objc_setAssociatedObject(owner, &Self.ownerWatchKey, watch, .OBJC_ASSOCIATION_RETAIN)
        }
        let watch = ToolMirrorWatch(mirror: mirror) { [weak self] in self?.readAll() }
        watch.watch(records.$streams)
        if let settings { watch.watch(settings.$values) }
        if let catalogFeed { watch.watch(catalogFeed.$catalog) }
        self.watch = watch
        readAll()
    }

    // MARK: Reading

    private func readAll() {
        // An owner freed took its session with it: nothing is followed or sent.
        let alive = !owned || owner != nil
        let next = alive ? mirror.capabilityVersion(Self.capabilityName) : 0
        if next != version {
            if version >= 2 {
                frequencyTask?.cancel()
                frequencyWanted = nil
                readStream = nil
                readingRoomId = nil
                readAgain = false
            }
            builtRooms.removeAll()
            version = next
        }
        let sourceVersion = alive ? mirror.capabilityVersion("recordStreamVersion") : 0
        if sourceVersion != sourceControlVersion {
            sourceControlVersion = sourceVersion
        }
        let up = alive && mirror.isSnapshotComplete && !mirror.isStale
        if up != connected {
            connectionGeneration &+= 1
            if !up {
                // The Core forgets this phone's reading with the session.
                readStream = nil
                frequencyWanted = nil
                frequencyTask?.cancel()
                readAgain = readingRoomId != nil
            }
            connected = up
        }
        if up, next < 1, readingRoomId != nil {
            readingRoomId = nil
        }
        let backendVersion = next >= 2 ? (records.streams[Self.roomsStream]?.records.first(where: {
            $0.id == "frequency-capability" && Self.text($0.fields["kind"]) == "frequency"
        }).flatMap { Self.whole($0.fields["version"]) } ?? 0) : 0
        if backendVersion != serverFrequencyVersion {
            if serverFrequencyVersion >= 1 && backendVersion < 1 {
                frequencyTask?.cancel()
                frequencyWanted = nil
                readStream = nil
                readingRoomId = nil
                readAgain = false
            }
            serverFrequencyVersion = backendVersion
            builtRooms.removeAll()
        }
        let units = settings?.value("Roundtable/DistanceUnits") == "km" ? "km" : "mi"
        if units != distanceUnits { distanceUnits = units }
        let bearing = settings?.value("Roundtable/ShowBearing") != "False"
        if bearing != showBearing { showBearing = bearing }
        readObject(next >= 1 ? watch?.object(Self.objectKey) : nil)
        syncWants()
        readStreams(next >= 1 ? records.streams : [:])
        syncReceivedHistory()
        if available, !frequencyAvailable, readAgain, let roomId = readingRoomId {
            readAgain = false
            Task { [weak self] in await self?.sendRead(roomId) }
        }
    }

    private func readObject(_ object: MirrorObject?) {
        let link = LinkState(rawValue: Self.whole(object?["linkState"]) ?? 0) ?? .off
        if link != linkState {
            linkState = link
        }
        let since = Self.whole(object?["linkSince"]) ?? 0
        if since != linkSince {
            linkSince = since
        }
        let answered = Self.flag(object?["firstRunAnswered"]) ?? false
        if answered != firstRunAnswered {
            firstRunAnswered = answered
        }
        let isHidden = Self.flag(object?["hidden"]) ?? false
        if isHidden != hidden {
            hidden = isHidden
        }
        var counts: [String: Int] = [:]
        if case .object(let entries)? = Self.json(object?["mentionCounts"]) {
            for (roomId, value) in entries {
                if let count = Self.whole(value), count > 0 {
                    counts[roomId] = Int(count)
                }
            }
        }
        if counts != mentionCounts {
            mentionCounts = counts
        }
        var slices: [Int64: String] = [:]
        if case .object(let entries)? = Self.json(object?["sliceRooms"]) {
            for (slice, value) in entries {
                if let id = Int64(slice), case .string(let roomId) = value, !roomId.isEmpty {
                    slices[id] = roomId
                }
            }
        }
        var contexts: [Int64: FrequencyContext] = [:]
        for object in mirror.objects(ofClass: "SliceModel") {
            guard object.key.hasPrefix("slice:"), let id = Int64(object.key.dropFirst("slice:".count)) else { continue }
            _ = watch?.object(object.key)
            guard let hz = ToolValue.number(object["frequency"]), hz.isFinite, hz > 0, hz < 1e12,
                  let mode = ToolValue.number(object["dspMode"]), mode.isFinite, mode >= 0, mode < 1e6,
                  let label = catalogFeed?.catalog?.modeDescription(Int(mode))?.label,
                  let family = Self.family(ofMode: label) else { continue }
            contexts[id] = FrequencyContext(sliceId: id, hz: Int64(hz.rounded()), family: family)
        }
        if sliceContexts != contexts { sliceContexts = contexts }
        if frequencyAvailable { slices = contexts.mapValues(\.id) }
        if slices != sliceRooms { sliceRooms = slices }
        let active: String?
        if let slice = activeSliceId {
            active = slices[slice]
        } else {
            active = slices.keys.sorted().first.flatMap { slices[$0] }
        }
        // Retain the last exact display context while disconnected or
        // unsupported, so received history remains readable and visibly stale.
        let displayed = active ?? (!retainedMessages.isEmpty ? historyContext : nil)
        if displayed != sliceRoomId { sliceRoomId = displayed }
    }

    /// The band list once the Core offers Roundtable, each slice's room and
    /// the room read; nothing from a Core without it.
    private func syncWants() {
        guard version >= 1 else {
            records.unwant(Self.roomsStream, by: self)
            for stream in sliceStreams {
                records.unwant(stream, by: sliceOwner)
            }
            sliceStreams = []
            if let stream = readWanted {
                records.unwant(stream, by: readOwner)
            }
            readWanted = nil
            return
        }
        records.want(Self.roomsStream, backlog: Self.roomsCapacity, by: self)
        if frequencyAvailable {
            for stream in sliceStreams { records.unwant(stream, by: sliceOwner) }
            sliceStreams = []
            let wanted = connected ? (readingRoomId ?? activeContext?.id) : nil
            if wanted != frequencyWanted {
                let previous = frequencyWanted
                frequencyWanted = wanted
                let previousTask = frequencyTask
                readStream = nil
                frequencyTask = Task { [weak self] in
                    await previousTask?.value
                    guard let self else { return }
                    if let previous { _ = await self.send("roundtable.unreadAt", self.contextArguments(previous)) }
                    guard !Task.isCancelled, self.frequencyWanted == wanted else { return }
                    if let wanted { await self.openFrequency(wanted) }
                }
            }
            if readWanted != readStream {
                if let stream = readWanted { records.unwant(stream, by: readOwner) }
                if let stream = readStream { records.want(stream, backlog: Self.roomCapacity, by: readOwner) }
                readWanted = readStream
            }
            return
        }
        let wanted = Set(sliceRooms.values.map(streamName))
        for stream in sliceStreams.subtracting(wanted) {
            records.unwant(stream, by: sliceOwner)
        }
        for stream in wanted.subtracting(sliceStreams).sorted() {
            records.want(stream, backlog: Self.roomCapacity, by: sliceOwner)
        }
        sliceStreams = wanted
        if readWanted != readStream {
            if let stream = readWanted {
                records.unwant(stream, by: readOwner)
            }
            if let stream = readStream {
                records.want(stream, backlog: Self.roomCapacity, by: readOwner)
            }
            readWanted = readStream
        }
    }

    private func readStreams(_ streams: [String: RecordStreamClient.Stream]) {
        let summaries = (streams[Self.roomsStream]?.records ?? []).compactMap(Self.summary)
            .sorted { $0.hz != $1.hz ? $0.hz < $1.hz : $0.id < $1.id }
        if summaries != rooms {
            rooms = summaries
        }
        var changed = false
        let prefix = frequencyAvailable ? Self.frequencyStreamPrefix : Self.roomStreamPrefix
        let roomStreams = streams.filter { $0.key.hasPrefix(prefix) }
        for (stream, _) in seen where roomStreams[stream] == nil {
            seen[stream] = nil
            if views.removeValue(forKey: Self.roomId(ofStream: stream)) != nil {
                changed = true
            }
        }
        for (stream, held) in roomStreams.sorted(by: { $0.key < $1.key }) where seen[stream] != held.records {
            let previousMessages = Self.view(of: seen[stream] ?? []).messages
            seen[stream] = held.records
            let roomId = Self.roomId(ofStream: stream)
            if held.records.isEmpty {
                views[roomId] = nil
            } else {
                var view = Self.view(of: held.records)
                // Rebuilding after an unrelated meter/member delta must not
                // reapply an unchanged old shown record over a history tombstone.
                for message in view.messages.values where previousMessages[message.id] != message {
                    retainedMessages[message.id] = message
                    historyLiveVersion &+= 1
                    historyRevisions[message.id] = historyLiveVersion
                    historyValidated[message.id] = historyLiveVersion
                }
                publishRetainedMessages()

                noteRecentNames(roomId, view)
                Self.prune(&view, before: clock.nowMilliseconds - Self.earlierWindowMs)
                views[roomId] = view
            }
            changed = true
        }
        if changed {
            schedulePrune()
        }
        publishRoom()
    }

    private func publishRoom() {
        let next = (readingRoomId ?? sliceRoomId).flatMap(view(of:))
        if next != room {
            room = next
        }
    }

    // MARK: Readers

    /// A room in view (the desktop's room()), or nil when it is not followed.
    func view(of roomId: String) -> Room? {
        let nowMs = clock.nowMilliseconds
        if let built = builtRooms[roomId], nowMs >= built.builtMs, nowMs < built.validUntilMs {
            return built.room
        }
        let retainedView = !retainedMessages.isEmpty && roomId == historyContext
            && (!connected || !frequencyAvailable)
        guard let view = views[roomId] ?? ((frequencyAvailable && (roomId == activeContext?.id || roomId == readingRoomId))
                                         || retainedView ? View() : nil) else {
            return nil
        }
        #if DEBUG
        roomBuildsForTesting += 1
        #endif
        let dash = roomId.firstIndex(of: "-")
        let family = dash.map { String(roomId[..<$0]) } ?? ""
        let hz = dash.flatMap { Int64(roomId[roomId.index(after: $0)...]) } ?? 0
        let cutoff = nowMs - Self.earlierWindowMs
        var open: [Turn] = []
        var ended: [Turn] = []
        for turn in view.turns.values {
            if let end = turn.end {
                // Wire spec 5.4.6: only the turns that ended in the last 30 minutes,
                // even between the prune timer's runs.
                if end >= cutoff {
                    ended.append(turn)
                }
            } else {
                open.append(turn)
            }
        }
        open.sort { $0.start != $1.start ? $0.start < $1.start : $0.id < $1.id }
        ended.sort { ($0.end ?? 0) != ($1.end ?? 0) ? ($0.end ?? 0) > ($1.end ?? 0) : $0.id > $1.id }
        let messages = (frequencyAvailable || retainedView ? receivedMessages
                        : view.messages.keys.sorted().suffix(Self.roomMessages).compactMap { view.messages[$0] })
        let room = Room(id: roomId, band: Self.bandOf(hz), family: dash == nil ? "" : family, hz: hz,
                        members: view.members.sorted { $0.seq < $1.seq }.map(\.member),
                        now: open.map(Self.withSortedNames), earlier: ended.map(Self.withSortedNames),
                        messages: Array(messages), frequencyView: frequencyAvailable || retainedView)
        // Good until the oldest earlier turn shown leaves the window (an
        // ended turn shows while end >= now - window).
        let validUntil = ended.compactMap(\.end).min().map { $0 + Self.earlierWindowMs + 1 } ?? .max
        builtRooms[roomId] = BuiltRoom(builtMs: nowMs, validUntilMs: validUntil, room: room)
        return room
    }

    /// The names used on a room's frequency in the last 24 hours (design
    /// 8.6), newest first, upper case, one per base callsign; it outlives
    /// the earlier turns and a room that closed and reopened on the same
    /// center. A turn still on the air counts as used now.
    func recentNames(_ roomId: String) -> [String] {
        let cutoff = clock.nowMilliseconds - Self.recentNamesWindowMs
        return (recentNames[roomId] ?? [:]).values.filter { $0.usedMs >= cutoff }
            .sorted { $0.usedMs != $1.usedMs ? $0.usedMs > $1.usedMs : $0.name < $1.name }
            .map(\.name)
    }

    func draft(_ roomId: String) -> String {
        drafts[draftKey(roomId)] ?? ""
    }

    func setDraft(_ text: String, for roomId: String) {
        drafts[draftKey(roomId)] = text.isEmpty ? nil : text
    }

    func draftKey(_ roomId: String) -> String {
        let frequencyContext = frequencyAvailable || room?.frequencyView == true || historyContext == roomId
        let slice = activeSliceId ?? activeContext?.sliceId ?? historySliceId ?? -1
        return frequencyContext ? "\(slice):\(roomId)" : roomId
    }

    /// Drops every ended turn older than 30 minutes from the rooms in view.
    /// Runs on its own when the oldest earlier turn ages out.
    func pruneEarlierTurns() {
        let cutoff = clock.nowMilliseconds - Self.earlierWindowMs
        for roomId in views.keys {
            if var view = views[roomId], Self.prune(&view, before: cutoff) {
                views[roomId] = view
            }
        }
        schedulePrune()
        publishRoom()
    }

    // MARK: Actions

    /// Posts `text` in a room. The answer waits for Roundtable itself.
    func post(_ text: String, in roomId: String, clientId: String = UUID().uuidString,
              context expectedContext: FrequencyContext? = nil) async -> PostResult {
        if let reason = coreReason {
            note = reason
            return .refused(reason)
        }
        if frequencyAvailable {
            guard let context = activeContext, context.id == roomId,
                  expectedContext == nil || context == expectedContext,
                  firstRunAnswered, !hidden, linkState == .live,
                  !((room?.members.first { $0.you && ($0.slice == nil || $0.slice == context.sliceId) })?.away ?? false) else {
                return .refused("The selected slice changed or your station cannot post. Your original draft stays here.")
            }
            let result = await send("roundtable.postAt", [
                CommandArgument(name: "sliceId", value: .int(context.sliceId)),
                CommandArgument(name: "hz", value: .int(context.hz)),
                CommandArgument(name: "family", value: .text(context.family)),
                CommandArgument(name: "text", value: .text(text)),
                CommandArgument(name: "clientId", value: .text(clientId)),
            ], timeout: Self.answerTimeout)
            guard let result, result.accepted else { return .refused(note ?? Self.noAnswerReason) }
            if case .text(let id)? = result.values["messageId"] { return .posted(messageId: id) }
            return .posted(messageId: "")
        }
        let result = await send("roundtable.post", [
            CommandArgument(name: "roomId", value: .text(roomId)),
            CommandArgument(name: "text", value: .text(text)),
            CommandArgument(name: "clientId", value: .text(clientId)),
        ], timeout: Self.answerTimeout)
        guard let result else {
            return .refused(Self.noAnswerReason)
        }
        guard result.accepted else {
            return .refused(result.reason)
        }
        if case .text(let id)? = result.values["messageId"] {
            return .posted(messageId: id)
        }
        return .posted(messageId: "")
    }

    @discardableResult
    func delete(_ messageId: String) async -> String? {
        await act("roundtable.delete", [CommandArgument(name: "messageId", value: .text(messageId))])
    }

    @discardableResult
    func name(turn turnId: String, as name: String) async -> String? {
        await act("roundtable.name", [CommandArgument(name: "turnId", value: .text(turnId)),
                                      CommandArgument(name: "name", value: .text(name))])
    }

    /// Reports a message, a turn or a station (`kind` `message`, `turn` or `station`).
    @discardableResult
    func report(kind: String, target: String, reason: String) async -> String? {
        await act("roundtable.report", [CommandArgument(name: "kind", value: .text(kind)),
                                        CommandArgument(name: "target", value: .text(target)),
                                        CommandArgument(name: "reason", value: .text(reason))])
    }

    /// Reads another room: the Core names its stream, which this phone then
    /// follows. The room read before is let go first. A refused read reads
    /// nothing.
    @discardableResult
    func read(_ roomId: String) async -> String? {
        if roomId == readingRoomId {
            return nil
        }
        if let reason = coreReason {
            note = reason
            return reason
        }
        if readingRoomId != nil {
            await unread()
        }
        readingRoomId = roomId
        publishRoom()
        if frequencyAvailable { syncWants(); return nil }
        return await sendRead(roomId)
    }

    /// Stops reading the room ``read(_:)`` opened.
    func unread() async {
        guard let roomId = readingRoomId else {
            return
        }
        readingRoomId = nil
        readStream = nil
        if frequencyAvailable { syncWants(); publishRoom(); return }
        readSerial += 1
        syncWants()
        publishRoom()
        guard available else {
            return
        }
        _ = await send(frequencyAvailable ? "roundtable.unreadAt" : "roundtable.unread", contextArguments(roomId))
    }

    /// Back to the active slice's room.
    func backToSlice() async {
        await unread()
    }

    @discardableResult
    func setHidden(_ hidden: Bool) async -> String? {
        await act("roundtable.setHidden", [CommandArgument(name: "hidden", value: .bool(hidden))])
    }

    @discardableResult
    func answerFirstRun(live: Bool) async -> String? {
        await act("roundtable.answerFirstRun", [CommandArgument(name: "live", value: .bool(live))])
    }

    @discardableResult
    func mute(_ callsign: String) async -> String? {
        await act("roundtable.mute", [CommandArgument(name: "callsign", value: .text(callsign))])
    }

    @discardableResult
    func unmute(_ callsign: String) async -> String? {
        await act("roundtable.unmute", [CommandArgument(name: "callsign", value: .text(callsign))])
    }

    /// Marks the mentions up to `upToMs` (Unix ms) read, on every device.
    @discardableResult
    func markMentionRead(upToMs: Int64) async -> String? {
        await act("roundtable.markMentionRead", [CommandArgument(name: "upToMs", value: .int(upToMs))])
    }

    /// The link that signs the operator in on Roundtable's website, or nil.
    func webSignIn() async -> URL? {
        if let reason = coreReason {
            note = reason
            return nil
        }
        guard let result = await send("roundtable.webSignIn", [], timeout: Self.answerTimeout), result.accepted,
              case .text(let link)? = result.values["link"] else {
            return nil
        }
        return URL(string: link)
    }

    /// Deletes the operator's archive on Roundtable; `confirm` is the words
    /// "delete my archive" as the operator typed them.
    @discardableResult
    func deleteArchive(confirm: String) async -> String? {
        await act("roundtable.deleteArchive", [CommandArgument(name: "confirm", value: .text(confirm))])
    }

    // MARK: Sending

    private func sendRead(_ roomId: String) async -> String? {
        readSerial += 1
        let serial = readSerial
        let result = await send(frequencyAvailable ? "roundtable.readAt" : "roundtable.read", contextArguments(roomId))
        // Only the room this phone still reads; a later read replaced it.
        guard serial == readSerial, readingRoomId == roomId else {
            return nil
        }
        guard let result, result.accepted else {
            readingRoomId = nil
            publishRoom()
            return result?.reason ?? Self.noAnswerReason
        }
        if case .text(let stream)? = result.values["stream"], stream == streamName(roomId) {
            readStream = stream
            syncWants()
        }
        return nil
    }

    private func openFrequency(_ id: String) async {
        let result = await send("roundtable.readAt", contextArguments(id))
        guard !Task.isCancelled, frequencyWanted == id else {
            // A superseded request may have reached the Core before cancellation.
            if result?.accepted == true { _ = await send("roundtable.unreadAt", contextArguments(id)) }
            return
        }
        guard let result, result.accepted,
              case .text(let stream)? = result.values["stream"], stream == streamName(id) else { return }
        readStream = stream
        syncWants()
    }

    static func family(ofMode mode: String) -> String? {
        // Same names as RoundtableWire::familyOfMode; no DSP numeric assumptions.
        switch mode.uppercased() {
        case "LSB": "lsb"
        case "USB": "usb"
        case "DIGL": "digl"
        case "DIGU": "digu"
        case "AM", "SAM": "am"
        case "FM": "fm"
        case "CWL", "CWU": "cw"
        case "FDVL": "fdvl"
        case "FDVU": "fdvu"
        default: nil
        }
    }

    private func streamName(_ id: String) -> String {
        (frequencyAvailable ? Self.frequencyStreamPrefix : Self.roomStreamPrefix) + id
    }

    private func contextArguments(_ id: String) -> [CommandArgument] {
        guard frequencyAvailable, let dash = id.firstIndex(of: "-"),
              let hz = Int64(id[id.index(after: dash)...]) else {
            return [CommandArgument(name: "roomId", value: .text(id))]
        }
        return [CommandArgument(name: "hz", value: .int(hz)),
                CommandArgument(name: "family", value: .text(String(id[..<dash])))]
    }

    private func act(_ verb: String, _ arguments: [CommandArgument]) async -> String? {
        if let reason = coreReason {
            note = reason
            return reason
        }
        guard let result = await send(verb, arguments) else {
            return Self.noAnswerReason
        }
        return result.accepted ? nil : result.reason
    }

    /// Sends a verb; a refusal or a failure becomes the note.
    private func send(_ verb: String, _ arguments: [CommandArgument],
                      timeout: Duration = RoundtableModel.commandTimeout) async -> CommandResult? {
        do {
            let result = try await commands.invoke(verb, arguments: arguments, timeout: timeout)
            note = result.accepted ? nil : result.reason
            return result
        } catch {
            Self.logger.info("A Roundtable request was not answered: \(String(describing: error), privacy: .public)")
            note = Self.noAnswerReason
            return nil
        }
    }

    // MARK: Bounded retained-message status synchronization

    private func publishRetainedMessages() {
        let retained = Array(retainedMessages.values.sorted { ($0.t, $0.id) < ($1.t, $1.id) }
            .suffix(Self.retainedMessageLimit))
        let ids = Set(retained.map(\.id))
        retainedMessages = retainedMessages.filter { ids.contains($0.key) }
        historyRevisions = historyRevisions.filter { ids.contains($0.key) }
        historyValidated = historyValidated.filter { ids.contains($0.key) }
        if receivedMessages != retained { receivedMessages = retained }
        builtRooms.removeAll()
    }

    private func syncReceivedHistory() {
        guard !retainedMessages.isEmpty else {
            historyTask?.cancel(); historyTask = nil
            historyTimer?.cancel(); historyTimer = nil
            historyStatus = nil
            return
        }
        guard connected, frequencyAvailable, [.live, .hidden, .reconnecting].contains(linkState) else {
            historyTask?.cancel(); historyTask = nil
            historyTimer?.cancel(); historyTimer = nil
            historySerial &+= 1
            historyGeneration = nil
            historyValidated.removeAll()
            historyStatus = !connected || [.off, .offline, .connecting].contains(linkState)
                ? "History stale · offline" : "History stale · refresh unavailable"
            return
        }
        let context = readingRoomId ?? activeContext?.id
        if historyGeneration != connectionGeneration || historyContext != context
            || (historyTask == nil && historyTimer == nil) {
            refreshReceivedHistory()
        }
    }

    /// Called before exposing Earlier/received history as well as on tuning,
    /// reconnect and the bounded periodic timer. No IDs are discovered here.
    func refreshReceivedHistory() {
        guard connected, frequencyAvailable, [.live, .hidden, .reconnecting].contains(linkState),
              !retainedMessages.isEmpty else { syncReceivedHistory(); return }
        historyTask?.cancel()
        historyTimer?.cancel()
        historySerial &+= 1
        let serial = historySerial
        let generation = connectionGeneration
        let ids = retainedMessages.keys.sorted()
        let revisions = historyRevisions
        historyGeneration = generation
        historyContext = readingRoomId ?? activeContext?.id
        historySliceId = activeSliceId ?? activeContext?.sliceId ?? historySliceId
        historyStatus = "History updating…"
        historyTimer = clock.schedule(after: .seconds(10)) { [weak self] in
            await self?.refreshReceivedHistory()
        }
        historyTask = Task { [weak self] in
            guard let self else { return }
            do {
                let encoded = String(decoding: try JSONEncoder().encode(ids), as: UTF8.self)
                let result = try await self.commands.invoke("roundtable.history",
                    arguments: [.init(name: "ids", value: .text(encoded))], timeout: Self.commandTimeout)
                guard !Task.isCancelled, serial == self.historySerial,
                      generation == self.connectionGeneration, self.connected, self.frequencyAvailable else { return }
                guard result.accepted, case .text(let raw)? = result.values["states"],
                      let states = try? JSONDecoder().decode([HistoryState].self, from: Data(raw.utf8)),
                      states.count == ids.count, Set(states.map(\.id)) == Set(ids),
                      Set(states.map(\.id)).count == states.count,
                      states.allSatisfy({ $0.id > 0 && ["shown", "deleted", "removed", "expired"].contains($0.state)
                          && ($0.state == "shown" ? $0.text != nil : $0.text == nil) }) else {
                    self.historyTask = nil
                    self.historyStatus = "History stale · refresh failed"
                    return
                }
                for state in states {
                    // A newer live tombstone protects just its own ID. Busy
                    // unrelated arrivals do not invalidate the whole response.
                    guard var message = self.retainedMessages[state.id],
                          self.historyRevisions[state.id] == revisions[state.id] else { continue }
                    message.state = state.state
                    message.text = state.text
                    self.retainedMessages[state.id] = message
                    self.historyValidated[state.id] = self.historyRevisions[state.id]
                }
                self.publishRetainedMessages()
                self.publishRoom()
                self.historyTask = nil
                self.historyStatus = self.retainedMessages.keys.allSatisfy {
                    self.historyValidated[$0] == self.historyRevisions[$0]
                } ? nil : "History stale · refresh pending"
            } catch {
                guard !Task.isCancelled, serial == self.historySerial,
                      generation == self.connectionGeneration else { return }
                self.historyTask = nil
                self.historyStatus = "History stale · refresh failed"
            }
        }
    }

    // MARK: Earlier turns and names

    @discardableResult
    private static func prune(_ view: inout View, before cutoffMs: Int64) -> Bool {
        let before = view.turns.count
        view.turns = view.turns.filter { _, turn in turn.end.map { $0 >= cutoffMs } ?? true }
        return view.turns.count != before
    }

    private func noteRecentNames(_ roomId: String, _ view: View) {
        let now = clock.nowMilliseconds
        var names = recentNames[roomId] ?? [:]
        func note(_ given: String, _ usedMs: Int64) {
            let name = given.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
            let base = Self.baseCallsign(name)
            guard !base.isEmpty else {
                return
            }
            if usedMs >= names[base]?.usedMs ?? 0 {
                names[base] = RecentName(name: name, usedMs: usedMs)
            }
        }
        for turn in view.turns.values {
            let usedMs = turn.end ?? now
            for talker in turn.talkers {
                note(talker.callsign, usedMs)
            }
            for name in turn.names {
                note(name.name, usedMs)
            }
        }
        let cutoff = now - Self.recentNamesWindowMs
        names = names.filter { $0.value.usedMs >= cutoff }
        recentNames[roomId] = names.isEmpty ? nil : names
    }

    /// Runs the prune a second past the moment the oldest earlier turn
    /// leaves the window, at least a second from now.
    private func schedulePrune() {
        pruneTimer?.cancel()
        pruneTimer = nil
        let oldest = views.values.flatMap { $0.turns.values.compactMap(\.end) }.min()
        guard let oldest else {
            return
        }
        let wait = min(max(oldest + Self.earlierWindowMs - clock.nowMilliseconds + 1000, 1000), Self.earlierWindowMs)
        pruneTimer = clock.schedule(after: .milliseconds(wait)) { [weak self] in
            await self?.pruneEarlierTurns()
        }
    }

    // MARK: Records

    static func roomStreamName(_ roomId: String) -> String {
        roomStreamPrefix + roomId
    }

    static func roomId(ofStream stream: String) -> String {
        if stream.hasPrefix(Self.frequencyStreamPrefix) { return String(stream.dropFirst(Self.frequencyStreamPrefix.count)) }
        return stream.hasPrefix(roomStreamPrefix) ? String(stream.dropFirst(roomStreamPrefix.count)) : ""
    }

    private static func view(of records: [LinkMessage.RecordBatch.Record]) -> View {
        var view = View()
        var readings: [String: Reading] = [:]
        var names: [Int64: [NameCount]] = [:]
        var members: [(seq: Int64, member: Member)] = []
        for record in records {
            let fields = record.fields
            switch text(fields["record"]) {
            case "member":
                if let member = member(fields) {
                    members.append((whole(fields["seq"]) ?? 0, member))
                }
            case "reading":
                if let ref = text(fields["ref"]) {
                    let identity = (text(fields["sourceRoom"]).map { $0 + "/" } ?? "") + ref
                    readings[identity] = Reading(dbm: number(fields["dbm"]) ?? 0, t: whole(fields["t"]) ?? 0,
                                            present: flag(fields["present"]) ?? false, turn: whole(fields["turn"]))
                }
            case "turn":
                if let turn = turn(fields) {
                    view.turns[turn.id] = turn
                }
            case "name":
                if let turn = whole(fields["turn"]), let name = text(fields["name"]) {
                    names[turn, default: []].append(NameCount(name: name, count: whole(fields["count"]) ?? 0))
                }
            case "message":
                if let message = message(fields) {
                    view.messages[message.id] = message
                }
            default:
                break
            }
        }
        view.members = members.map { entry in
            var member = entry.member
            member.reading = readings[member.id]
            return (entry.seq, member)
        }
        for (id, turnNames) in names {
            view.turns[id]?.names = turnNames
        }
        return view
    }

    private static func withSortedNames(_ turn: Turn) -> Turn {
        var sorted = turn
        sorted.names.sort { $0.count != $1.count ? $0.count > $1.count : $0.name < $1.name }
        return sorted
    }

    private static func summary(_ record: LinkMessage.RecordBatch.Record) -> RoomSummary? {
        let fields = record.fields
        guard text(fields["record"]) == "room" else {
            return nil
        }
        let id = text(fields["id"]) ?? record.id
        var last: LastMessage?
        if case .object(let message)? = fields["lastMessage"] {
            last = LastMessage(callsign: text(message["callsign"]) ?? "", nth: whole(message["nth"]) ?? 1,
                               t: whole(message["t"]) ?? 0, state: text(message["state"]) ?? "",
                               text: text(message["text"]))
        }
        return RoomSummary(id: id, band: text(fields["band"]) ?? "", family: text(fields["family"]) ?? "",
                           hz: whole(fields["hz"]) ?? 0, state: text(fields["state"]) ?? "",
                           talkers: texts(fields["talkers"]), members: whole(fields["members"]) ?? 0,
                           messages: whole(fields["messages"]) ?? 0, last: whole(fields["last"]) ?? 0,
                           folded: flag(fields["folded"]) ?? false, start: whole(fields["start"]),
                           heard: whole(fields["heard"]) ?? 0, readings: numbers(fields["readings"]),
                           lastMessage: last, ended: whole(fields["ended"]), stations: stations(fields["stations"]))
    }

    private static func member(_ fields: [String: LinkJSON]) -> Member? {
        guard let ref = text(fields["ref"]) else {
            return nil
        }
        return Member(ref: ref, callsign: text(fields["callsign"]) ?? "", nth: whole(fields["nth"]) ?? 1,
                      grid: text(fields["grid"]), km: whole(fields["km"]), offsetHz: whole(fields["offsetHz"]) ?? 0,
                      joining: flag(fields["joining"]) ?? false, away: flag(fields["away"]) ?? false,
                      keyed: flag(fields["keyed"]) ?? false, software: text(fields["software"]) ?? "",
                      softwareVersion: text(fields["softwareVersion"]) ?? "", you: flag(fields["you"]) ?? false,
                      reading: nil, hz: whole(fields["hz"]), sourceRoom: text(fields["sourceRoom"]),
                      slice: whole(fields["slice"]), bearingDeg: bearing(fields["bearingDeg"]))
    }

    private static func turn(_ fields: [String: LinkJSON]) -> Turn? {
        guard let id = whole(fields["id"]), id > 0 else {
            return nil
        }
        var talkers: [Talker] = []
        if case .array(let entries)? = fields["talkers"] {
            for case .object(let talker) in entries {
                talkers.append(Talker(ref: text(talker["ref"]), callsign: text(talker["callsign"]) ?? "",
                                      nth: whole(talker["nth"]) ?? 1, hz: whole(talker["hz"]),
                                      grid: text(talker["grid"]), sourceRoom: text(talker["sourceRoom"])))
            }
        }
        return Turn(id: id, kind: text(fields["kind"]) ?? "", talkers: talkers, start: whole(fields["start"]) ?? 0,
                    end: whole(fields["end"]), ending: flag(fields["ending"]) ?? false, names: [],
                    heard: whole(fields["heard"]) ?? 0, mine: text(fields["mine"]),
                    hz: whole(fields["hz"]), sourceRoom: text(fields["sourceRoom"]))
    }

    private static func message(_ fields: [String: LinkJSON]) -> Message? {
        guard let id = whole(fields["id"]), id > 0 else {
            return nil
        }
        return Message(id: id, ref: text(fields["ref"]), callsign: text(fields["callsign"]) ?? "",
                       nth: whole(fields["nth"]) ?? 1, t: whole(fields["t"]) ?? 0, state: text(fields["state"]) ?? "",
                       text: text(fields["text"]), dbm: number(fields["dbm"]), hz: whole(fields["hz"]),
                       family: text(fields["family"]), sourceRoom: text(fields["sourceRoom"]))
    }

    private static func bearing(_ value: LinkJSON?) -> Int64? {
        guard let bearing = whole(value), (0...359).contains(bearing) else { return nil }
        return bearing
    }

    private static func stations(_ value: LinkJSON?) -> [Station] {
        guard case .array(let entries)? = value else { return [] }
        return entries.compactMap { entry in
            guard case .object(let fields) = entry, let ref = text(fields["ref"]),
                  let source = text(fields["sourceRoom"]), let slice = whole(fields["slice"]),
                  let hz = whole(fields["hz"]), let family = text(fields["family"]) else { return nil }
            return Station(ref: ref, sourceRoom: source, slice: slice, hz: hz, family: family,
                           callsign: text(fields["callsign"]) ?? "", nth: whole(fields["nth"]) ?? 1,
                           grid: text(fields["grid"]), keyed: flag(fields["keyed"]) ?? false,
                           away: flag(fields["away"]) ?? false, joining: flag(fields["joining"]) ?? false)
        }
    }

    // MARK: Wire words

    /// RoundtableWire::kBands: each band's edges in Hz, both included.
    private static let bands: [(name: String, from: Int64, to: Int64)] = [
        ("2200m", 135_700, 137_800), ("630m", 472_000, 479_000),
        ("160m", 1_800_000, 2_000_000), ("80m", 3_500_000, 4_000_000),
        ("60m", 5_250_000, 5_450_000), ("40m", 7_000_000, 7_300_000),
        ("30m", 10_100_000, 10_150_000), ("20m", 14_000_000, 14_350_000),
        ("17m", 18_068_000, 18_168_000), ("15m", 21_000_000, 21_450_000),
        ("12m", 24_890_000, 24_990_000), ("10m", 28_000_000, 29_700_000),
        ("6m", 50_000_000, 54_000_000), ("4m", 70_000_000, 71_000_000),
        ("2m", 144_000_000, 148_000_000), ("1.25m", 222_000_000, 225_000_000),
        ("70cm", 420_000_000, 450_000_000), ("33cm", 902_000_000, 928_000_000),
        ("23cm", 1_240_000_000, 1_300_000_000),
    ]

    /// The band a center frequency is in (wire spec 6.2), or `other`.
    static func bandOf(_ hz: Int64) -> String {
        bands.first { hz >= $0.from && hz <= $0.to }?.name ?? "other"
    }

    /// The longest `/`-separated part of a callsign, upper case (design 7.1).
    nonisolated static func baseCallsign(_ callsign: String) -> String {
        var longest = ""
        for part in callsign.split(separator: "/", omittingEmptySubsequences: false) where part.count > longest.count {
            longest = String(part)
        }
        return longest.uppercased()
    }

    // MARK: Values

    private static func text(_ value: LinkJSON?) -> String? {
        if case .string(let text)? = value {
            return text
        }
        return nil
    }

    private static func texts(_ value: LinkJSON?) -> [String] {
        guard case .array(let entries)? = value else {
            return []
        }
        return entries.compactMap { text($0) }
    }

    private static func number(_ value: LinkJSON?) -> Double? {
        if case .number(let number)? = value, number.isFinite {
            return number
        }
        return nil
    }

    private static func numbers(_ value: LinkJSON?) -> [Double] {
        guard case .array(let entries)? = value else {
            return []
        }
        return entries.compactMap { number($0) }
    }

    private static func whole(_ value: LinkJSON?) -> Int64? {
        guard let number = number(value), number == number.rounded(), abs(number) <= 9_007_199_254_740_991 else {
            return nil
        }
        return Int64(number)
    }

    private static func flag(_ value: LinkJSON?) -> Bool? {
        if case .bool(let flag)? = value {
            return flag
        }
        return nil
    }

    private static func whole(_ value: MirrorValue?) -> Int64? {
        switch value {
        case .int(let number)?, .enumeration(let number)?:
            return number
        default:
            return nil
        }
    }

    private static func flag(_ value: MirrorValue?) -> Bool? {
        if case .bool(let flag)? = value {
            return flag
        }
        return nil
    }

    private static func json(_ value: MirrorValue?) -> LinkJSON? {
        guard case .text(let text)? = value, !text.isEmpty else {
            return nil
        }
        return try? LinkJSON.parse(text)
    }
}

/// The wall clock Roundtable's times are read against: Unix ms, as the
/// Core's turns and messages carry them.
struct RoundtableWallClock: LinkClock {
    var nowMilliseconds: Int64 {
        Int64((Date().timeIntervalSince1970 * 1000).rounded())
    }

    func schedule(after delay: Duration, _ action: @escaping @Sendable () async -> Void) -> any LinkTimer {
        SystemLinkClock().schedule(after: delay, action)
    }
}
