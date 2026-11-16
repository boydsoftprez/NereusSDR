// NereusSDR for iOS: the Core's recordings and voice keyer: the mirrored document, this device's playback and the verbs
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 15, the phone's recordings and voice keyer.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: Recording plan Task 28, the Digital Spectrum Recorder's verbs
//   (version 4). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: Recording plan fix round: linkDrops, so the recorder's screens
//   drop what they kept across a reconnect. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusLink
import NereusModels
import Observation
import os

/// Why a recordings call did not do what was asked.
public enum RecordingsError: Error, Equatable, Sendable {
    /// The Core does not offer what the call needs (`recordingsVersion`
    /// too low, or none at all): nothing was sent (V9).
    case needsCoreUpdate
    /// No session is ready: nothing was sent.
    case notConnected
    /// The Core said no, in its own words: a refusal of the holder's, a
    /// slot on the air, a recording another device removed, and the rest.
    case refused(String)
    /// The Core gave no final answer: nothing sent, the link lost, or no answer in time.
    case notAnswered(CommandError)
    /// The Core's answer lacked what it should carry.
    case badAnswer
    /// A file on this device could not be read or written.
    case localFile(String)

    /// The words a control shows for it, where the Core or the link gave
    /// some; nil for a failure the app words itself.
    public var reason: String? {
        switch self {
        case .needsCoreUpdate:
            return RecordingsClient.updateReason
        case .refused(let reason):
            return reason
        case .notConnected, .notAnswered, .badAnswer, .localFile:
            return nil
        }
    }
}

/// The Core's recordings and voice keyer as this device works them
/// (design 2026-11-15-recording-voice-keyer-dsr-design.md section 6.9;
/// link document, the notes on the `recordings` key and the recordings and
/// voice keyer verbs). Feed it the session's events with ``handle(_:)``
/// after the mirror has handled them.
///
/// What every device sees comes from the mirrored `recordings` document:
/// each open slice's Record, the last recording kept, the index, the
/// settings and the voice keyer. This device's own playback comes on its
/// own, as `recordings.playback`. Every call needs the version of
/// `recordingsVersion` it names: a Core that offers less leaves the call
/// unsent and throws ``RecordingsError/needsCoreUpdate``, whose words are
/// ``updateReason``; the voice keyer needs 3 and the on-air switches 2.
///
/// The voice keyer keys the radio as the sending device. While the
/// document's `voiceKeyer.sending.deviceId` names this device, the client
/// tells `voiceMessageSending` so, in order, and the app hands that to its
/// PTT (``PttController/setVoiceMessageSending(_:)``) so the transmit
/// keepalives keep the message on the air. Ruling 8.5: only the sender's
/// own Stop and PTT stop its message; any other device's Stop is refused
/// by the Core with the holder's words, which ``stopVoice()`` throws.
@MainActor
@Observable
public final class RecordingsClient {
    /// Where the client is.
    public enum State: Equatable, Sendable {
        /// No session is ready.
        case offline
        /// The Core offers no recordings (`recordingsVersion` 0).
        case needsCoreUpdate
        /// The Core offers recordings at ``RecordingsClient/coreVersion``.
        case ready
    }

    /// The capability that gates the recordings document and the verbs.
    nonisolated public static let capabilityName = "recordingsVersion"
    /// The feature this app declares in its hello for them.
    nonisolated public static let featureName = "recordings"
    /// The first version: recordings, their list and this device's playback.
    nonisolated public static let recordingsVersion: Int64 = 1
    /// A recording goes on the air, and the five on-air switches.
    nonisolated public static let onAirVersion: Int64 = 2
    /// The voice keyer.
    nonisolated public static let voiceKeyerVersion: Int64 = 3
    /// The Digital Spectrum Recorder: rewind, the past picture, saving a
    /// stretch, its settings and erasing what it keeps.
    nonisolated public static let dsrVersion: Int64 = 4
    /// V9: every control a Core too old for it leaves greyed shows this.
    nonisolated public static let updateReason = "This Core needs an update for recordings."
    /// A file moves this many bytes at a time each way, as the Core's
    /// export and voice keyer chunks do (192 KiB, base64 on the wire).
    nonisolated public static let chunkBytes = 196_608
    /// This device's own playback, which the Core sends with id 0.
    nonisolated public static let playbackVerb = "recordings.playback"

    private static let logger = Logger(subsystem: "NereusSDR", category: "mirror.recordings")

    // MARK: What the screens read

    public private(set) var state: State = .offline
    /// The Core's `recordingsVersion` while a session is ready; 0 otherwise.
    public private(set) var coreVersion: Int64 = 0
    /// The Core's document as last mirrored; nil before it publishes.
    public private(set) var document: RecordingsDocument?
    /// This device's own playback.
    public private(set) var playback: RecordingsPlayback = .stopped
    /// The Core's words for a play it refused after answering it (the file
    /// could not be decoded); nil until then, and after the next play.
    public private(set) var playbackRefusal: String?
    /// The Core's voice keyer sends this device's message, or waits to repeat it.
    public private(set) var voiceMessageSending = false
    /// How many times the link has been lost. A screen that keeps
    /// something for this session (a LIVE owed, the past it drew) compares
    /// it to tell a reconnect or another Core from the same session.
    public private(set) var linkDrops = 0

    /// Each open slice's Record.
    public var slices: [RecordingsDocument.Slice] { document?.slices ?? [] }
    /// The last recording kept.
    public var kept: RecordingsDocument.Kept? { document?.kept }
    /// What the Core's recordings folder holds.
    public var index: RecordingsDocument.Index? { document?.index }
    /// The recording settings.
    public var settings: RecordingsDocument.Settings? { document?.settings }
    /// The voice keyer; nil below version 3.
    public var voiceKeyer: VoiceKeyerState? { document?.voiceKeyer }
    /// The Digital Spectrum Recorder; nil below version 4.
    public var dsr: RecordingsDocument.Dsr? { document?.dsr }

    /// V9's words for a control that needs `recordingsVersion` `needs`, or
    /// nil when the Core offers it. Nil too while no session is ready:
    /// ``state`` says so.
    public func unavailableReason(needs: Int64) -> String? {
        guard state != .offline else {
            return nil
        }
        return coreVersion >= needs ? nil : Self.updateReason
    }

    // MARK: Inside state

    @ObservationIgnored private let mirror: MirrorStore
    @ObservationIgnored private let commands: CommandClient
    @ObservationIgnored private let timeout: Duration
    @ObservationIgnored private let importTimeout: Duration
    @ObservationIgnored private let pastTimeout: Duration
    @ObservationIgnored private let thisDeviceId: @MainActor () -> String?
    @ObservationIgnored private let voiceMessageSink: (@Sendable (Bool) async -> Void)?
    /// The Core's name for this device, from the answer to its own send:
    /// a token sign-in cannot work it out for itself.
    @ObservationIgnored private var sentWireId: String?
    @ObservationIgnored private var linkUp = false
    @ObservationIgnored private var lastJson: String?
    @ObservationIgnored private var lastRevision: Int64?
    /// The sink's calls, one after another, so a stop never overtakes a start.
    @ObservationIgnored private var sinkTail: Task<Void, Never>?

    /// - Parameters:
    ///   - thisDeviceId: the id the link names this device by, as
    ///     `voiceKeyer.sending.deviceId` would; nil when the app does not know it.
    ///   - voiceMessageSending: told, in order, each time this device's
    ///     voice message starts or stops going out, and again while it does.
    ///   - timeout: how long a call waits for the Core's answer.
    ///   - importTimeout: how long a voice message's last step waits, while
    ///     the Core converts the file.
    ///   - pastTimeout: how long one chunk of the past picture waits, while
    ///     the Core draws it from its disk.
    public init(mirror: MirrorStore, commands: CommandClient,
                thisDeviceId: @escaping @MainActor () -> String? = { nil },
                voiceMessageSending: (@Sendable (Bool) async -> Void)? = nil,
                timeout: Duration = .seconds(5), importTimeout: Duration = .seconds(60),
                pastTimeout: Duration = .seconds(30)) {
        self.mirror = mirror
        self.commands = commands
        self.thisDeviceId = thisDeviceId
        self.voiceMessageSink = voiceMessageSending
        self.timeout = timeout
        self.importTimeout = importTimeout
        self.pastTimeout = pastTimeout
    }

    /// The sink's calls so far have all run (the tests wait on it).
    public func voiceMessageSinkSettled() async {
        await sinkTail?.value
    }

    // MARK: Feeding

    /// One session event. Feed it after the mirror has handled it.
    public func handle(_ event: StationSession.Event) {
        switch event {
        case .message(.commandResult(let result)) where result.id == 0:
            receiveUnsolicited(result)
        case .message:
            if linkUp {
                refreshDocument()
            }
        case .stateChanged(let next):
            let up = next == .receivingSnapshot || next == .ready
            if linkUp && !up {
                linkLost()
            }
            linkUp = up
            if next == .ready {
                coreVersion = max(0, mirror.capabilityVersion(Self.capabilityName))
                state = coreVersion >= Self.recordingsVersion ? .ready : .needsCoreUpdate
                refreshDocument()
            }
        case .refused:
            break
        }
    }

    /// The Core's own pushes: this device's playback, and a play refused late.
    private func receiveUnsolicited(_ wire: LinkMessage.CommandResult) {
        let result = CommandResult(wire)
        if wire.verb == Self.playbackVerb {
            guard case .text(let id)? = result.values["id"] else {
                return
            }
            playback = RecordingsPlayback(id: id, positionMs: Self.whole(result.values["positionMs"]) ?? 0,
                                          lengthMs: Self.whole(result.values["lengthMs"]) ?? 0,
                                          playing: Self.flag(result.values["playing"]) ?? false,
                                          onAir: Self.flag(result.values["onAir"]) ?? false)
        } else if wire.verb == "recordings.play", !wire.accepted {
            playbackRefusal = wire.reason
        }
    }

    private func refreshDocument() {
        guard let object = mirror.object(RecordingsDocument.objectKey) else {
            if document != nil {
                document = nil
                lastJson = nil
                lastRevision = nil
                followVoiceMessage()
            }
            return
        }
        guard case .text(let json)? = object["json"], case .int(let revision)? = object["revision"] else {
            return
        }
        guard json != lastJson || revision != lastRevision else {
            return
        }
        lastJson = json
        lastRevision = revision
        let next = RecordingsDocument(json: json, revision: revision)
        if next == nil && !json.isEmpty {
            Self.logger.info("A recordings document could not be read")
        }
        if next != document {
            document = next
        }
        followVoiceMessage()
    }

    private func linkLost() {
        linkDrops += 1
        state = .offline
        coreVersion = 0
        document = nil
        lastJson = nil
        lastRevision = nil
        playback = .stopped
        playbackRefusal = nil
        sentWireId = nil
        followVoiceMessage()
    }

    /// Whether `voiceKeyer.sending` is this device's, told to the sink in
    /// order: on every change, and again with each document while it is,
    /// so a PTT that started a new session hears it again.
    private func followVoiceMessage() {
        let mine = linkUp && (voiceKeyer?.isSentBy([thisDeviceId(), sentWireId]) ?? false)
        let changed = mine != voiceMessageSending
        if changed {
            voiceMessageSending = mine
        }
        guard changed || mine, let sink = voiceMessageSink else {
            return
        }
        let previous = sinkTail
        sinkTail = Task {
            await previous?.value
            await sink(mine)
        }
    }

    // MARK: Recordings (version 1)

    /// Starts Record on a slice (0 is Slice A).
    public func start(slice: Int) async throws {
        _ = try await invoke("recordings.start", [.init(name: "slice", value: .int(Int64(slice)))])
    }

    /// Stops Record on a slice; the Core keeps the recording.
    public func stop(slice: Int) async throws {
        _ = try await invoke("recordings.stop", [.init(name: "slice", value: .int(Int64(slice)))])
    }

    /// One page of the Core's recordings, newest first, at most 100 from
    /// `offset`. `slice` and `kind` narrow it; `reader`, the asking list's
    /// own number, comes back in the page, so several lists each get their own.
    public func list(offset: Int, count: Int, slice: Int? = nil, kind: RecordingEntry.Kind? = nil,
                     reader: Int64? = nil) async throws -> RecordingsPage {
        var arguments: [CommandArgument] = [.init(name: "offset", value: .int(Int64(offset))),
                                            .init(name: "count", value: .int(Int64(count)))]
        if let slice {
            arguments.append(.init(name: "slice", value: .int(Int64(slice))))
        }
        if let kind {
            arguments.append(.init(name: "kind", value: .text(kind.rawValue)))
        }
        if let reader {
            arguments.append(.init(name: "reader", value: .int(reader)))
        }
        let result = try await invoke("recordings.list", arguments)
        guard case .text(let text)? = result.values["entries"], let entries = RecordingEntry.list(json: text),
              let at = Self.whole(result.values["offset"]), let total = Self.whole(result.values["total"]) else {
            throw RecordingsError.badAnswer
        }
        return RecordingsPage(entries: entries, offset: Int(at), total: Int(total),
                              reader: Self.whole(result.values["reader"]))
    }

    /// Plays a recording in this device's own speakers; never on the air by itself.
    public func play(id: String) async throws {
        playbackRefusal = nil
        _ = try await invoke("recordings.play", [.init(name: "id", value: .text(id))])
    }

    public func stopPlay() async throws {
        _ = try await invoke("recordings.stopPlay", [])
    }

    /// Moves this device's playback to `ms` from the start.
    public func seek(ms: Int64) async throws {
        _ = try await invoke("recordings.seek", [.init(name: "positionMs", value: .int(ms))])
    }

    public func rename(id: String, name: String) async throws {
        _ = try await invoke("recordings.rename", [.init(name: "id", value: .text(id)),
                                                   .init(name: "name", value: .text(name))])
    }

    /// Deletes recordings; one another device deleted first is skipped by the Core.
    public func delete(ids: [String]) async throws {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.withoutEscapingSlashes]
        guard let data = try? encoder.encode(ids) else {
            throw RecordingsError.badAnswer
        }
        _ = try await invoke("recordings.delete", [.init(name: "ids", value: .text(String(decoding: data, as: UTF8.self)))])
    }

    /// Copies a recording's file from the Core to `url`, a chunk at a time;
    /// `progress` hears the bytes so far and the file's size after each.
    /// A copy that fails leaves no file behind.
    public func download(id: String, to url: URL,
                         progress: (@MainActor (Int64, Int64) -> Void)? = nil) async throws {
        try await copyFile(verb: "recordings.export", needs: Self.recordingsVersion, to: url, progress: progress) { offset in
            [.init(name: "id", value: .text(id)), .init(name: "offset", value: .int(offset))]
        }
    }

    /// Changes the recording settings, which live on the Core. The on-air
    /// switches need version 2 and the recorder's settings version 4. A
    /// change that sets nothing sends nothing.
    public func setSettings(_ change: RecordingsSettingsChange) async throws {
        let needs = change.setsDsr ? Self.dsrVersion
            : change.setsOnAirSwitches ? Self.onAirVersion : Self.recordingsVersion
        try admit(needs: needs)
        guard !change.isEmpty else {
            return
        }
        let arguments: [CommandArgument] = change.fields.map { field in
            switch field.value {
            case .flag(let on):
                return .init(name: field.name, value: .bool(on))
            case .whole(let value):
                return .init(name: field.name, value: .int(value))
            case .text(let text):
                return .init(name: field.name, value: .text(text))
            }
        }
        _ = try await invoke("recordings.setSettings", arguments, needs: needs)
    }

    // MARK: The voice keyer (version 3)

    /// Sends Mn on the air: the one call that keys the radio, as this
    /// device. The Core refuses it while another device holds transmit,
    /// with the holder's words.
    public func sendVoice(_ n: Int) async throws {
        let result = try await invoke("voiceKeyer.send", [.init(name: "n", value: .int(Int64(n)))],
                                      needs: Self.voiceKeyerVersion)
        if case .text(let id)? = result.values["deviceId"], !id.isEmpty {
            sentWireId = id
            followVoiceMessage()
        }
    }

    /// Stops this device's message, or the wait between its repeats, and
    /// unkeys. From any other device the Core refuses it with the holder's
    /// words, which this throws, and the message goes on (ruling 8.5).
    public func stopVoice() async throws {
        _ = try await invoke("voiceKeyer.stop", [], needs: Self.voiceKeyerVersion)
    }

    /// Loads Mn from a WAV or MP3 file at `url`, keeping Mn's name when
    /// `name` is "". Refused while Mn is on the air, here before anything goes.
    public func importVoice(_ n: Int, url: URL, name: String = "") async throws {
        try admit(needs: Self.voiceKeyerVersion)
        try refuseOnAir(n)
        let data: Data
        do {
            data = try Data(contentsOf: url)
        } catch {
            throw RecordingsError.localFile(error.localizedDescription)
        }
        _ = try await invoke("voiceKeyer.beginImport", [.init(name: "n", value: .int(Int64(n))),
                                                        .init(name: "name", value: .text(name)),
                                                        .init(name: "size", value: .int(Int64(data.count)))],
                             needs: Self.voiceKeyerVersion)
        var offset = 0
        while offset < data.count {
            let end = min(data.count, offset + Self.chunkBytes)
            // Each chunk after the one before it is answered.
            _ = try await invoke("voiceKeyer.chunk",
                                 [.init(name: "data", value: .text(data[offset..<end].base64EncodedString()))],
                                 needs: Self.voiceKeyerVersion)
            offset = end
        }
        _ = try await invoke("voiceKeyer.finishImport", [], needs: Self.voiceKeyerVersion, timeout: importTimeout)
    }

    /// Copies Mn's file (48 kHz 16-bit mono WAV) from the Core to `url`.
    public func exportVoice(_ n: Int, to url: URL,
                            progress: (@MainActor (Int64, Int64) -> Void)? = nil) async throws {
        try await copyFile(verb: "voiceKeyer.export", needs: Self.voiceKeyerVersion, to: url, progress: progress) { offset in
            [.init(name: "n", value: .int(Int64(n))), .init(name: "offset", value: .int(offset))]
        }
    }

    /// Renames Mn, at most 32 characters; allowed while it is on the air.
    public func renameVoice(_ n: Int, name: String) async throws {
        _ = try await invoke("voiceKeyer.rename", [.init(name: "n", value: .int(Int64(n))),
                                                   .init(name: "name", value: .text(name))],
                             needs: Self.voiceKeyerVersion)
    }

    /// Empties Mn. Refused while Mn is on the air, here before anything goes.
    public func clearVoice(_ n: Int) async throws {
        try admit(needs: Self.voiceKeyerVersion)
        try refuseOnAir(n)
        _ = try await invoke("voiceKeyer.clear", [.init(name: "n", value: .int(Int64(n)))],
                             needs: Self.voiceKeyerVersion)
    }

    /// Repeat on or off, and the wait between repeats (1 to 60 seconds).
    public func setVoiceRepeat(on: Bool, delaySec: Int) async throws {
        _ = try await invoke("voiceKeyer.setRepeat", [.init(name: "on", value: .bool(on)),
                                                      .init(name: "delaySec", value: .int(Int64(delaySec)))],
                             needs: Self.voiceKeyerVersion)
    }

    /// Plays Mn in this device's own speakers, never on the air.
    public func listenVoice(_ n: Int) async throws {
        _ = try await invoke("voiceKeyer.listen", [.init(name: "n", value: .int(Int64(n)))],
                             needs: Self.voiceKeyerVersion)
    }

    // MARK: The Digital Spectrum Recorder (version 4)

    /// Moves `slice` to `frequencyHz` and plays it from `utcNs` (ns since
    /// 1970 UTC) out of the recorder; the other slices stay live. Never keys.
    public func rewind(slice: Int, utcNs: Int64, frequencyHz: Int64) async throws {
        _ = try await invoke("dsr.rewind", [.init(name: "slice", value: .int(Int64(slice))),
                                            .init(name: "utcNs", value: .int(utcNs)),
                                            .init(name: "frequencyHz", value: .int(frequencyHz))],
                             needs: Self.dsrVersion)
    }

    /// Moves a rewound slice to another moment at the same frequency.
    public func seekPast(slice: Int, utcNs: Int64) async throws {
        _ = try await invoke("dsr.seek", [.init(name: "slice", value: .int(Int64(slice))),
                                          .init(name: "utcNs", value: .int(utcNs))],
                             needs: Self.dsrVersion)
    }

    /// Returns a rewound slice to the radio, at the frequency it had before.
    public func live(slice: Int) async throws {
        _ = try await invoke("dsr.live", [.init(name: "slice", value: .int(Int64(slice)))], needs: Self.dsrVersion)
    }

    /// `rowCount` rows of the past, evenly from `toUtcNs` back to
    /// `fromUtcNs`, newest first, drawn with the pan's own display. The Core
    /// sends them in chunks; `chunk` hears each as it comes, and the next is
    /// asked for until none are left or the task is cancelled.
    public func pastRows(slice: Int, view: DsrPanView, fromUtcNs: Int64, toUtcNs: Int64, rowCount: Int,
                         chunk: @MainActor (DsrPastFrames) -> Void) async throws {
        var to = toUtcNs
        var left = rowCount
        while true {
            let result = try await invoke("dsr.rows", [.init(name: "slice", value: .int(Int64(slice))),
                                                       .init(name: "fromUtcNs", value: .int(fromUtcNs)),
                                                       .init(name: "toUtcNs", value: .int(to)),
                                                       .init(name: "rowCount", value: .int(Int64(left)))]
                                              + Self.viewArguments(view),
                                          needs: Self.dsrVersion, timeout: pastTimeout)
            let frames = try Self.frames(result)
            chunk(frames)
            guard frames.rowsLeft > 0, frames.rowsLeft < left || frames.nextToUtcNs < to, !Task.isCancelled else {
                return
            }
            to = frames.nextToUtcNs
            left = frames.rowsLeft
        }
    }

    /// One spectrum of the past at `utcNs`, drawn with the pan's own display.
    public func pastSpectrum(slice: Int, view: DsrPanView, utcNs: Int64) async throws -> DsrPastFrames {
        let result = try await invoke("dsr.spectrum", [.init(name: "slice", value: .int(Int64(slice))),
                                                       .init(name: "utcNs", value: .int(utcNs))]
                                          + Self.viewArguments(view),
                                      needs: Self.dsrVersion, timeout: pastTimeout)
        return try Self.frames(result)
    }

    /// Every kept hour, newest first, shaded by how busy the band was.
    public func hours() async throws -> [DsrHour] {
        let result = try await invoke("dsr.hours", [], needs: Self.dsrVersion, timeout: pastTimeout)
        guard case .text(let starts)? = result.values["starts"], case .text(let activity)? = result.values["activity"],
              let hours = DsrHour.decode(starts: starts, activity: activity) else {
            throw RecordingsError.badAnswer
        }
        return hours
    }

    /// Saves a stretch the recorder keeps as a recording; returns the
    /// Core's words for what it is saving. The list shows it when done.
    public func saveStretch(fromUtcNs: Int64, toUtcNs: Int64, kind: DsrStretchKind) async throws -> String {
        let result = try await invoke("dsr.saveStretch", [.init(name: "fromUtcNs", value: .int(fromUtcNs)),
                                                          .init(name: "toUtcNs", value: .int(toUtcNs)),
                                                          .init(name: "kind", value: .text(kind.rawValue))],
                                      needs: Self.dsrVersion)
        return Self.note(result)
    }

    /// Keeps the sound of a saved whole-pan stretch as a recording of its own.
    public func saveSoundOnly(id: String) async throws -> String {
        let result = try await invoke("recordings.saveSoundOnly", [.init(name: "id", value: .text(id))],
                                      needs: Self.dsrVersion)
        return Self.note(result)
    }

    /// Opens a saved whole-pan stretch on the waterfall: Slice A plays it like rewind.
    public func openStretch(id: String) async throws {
        _ = try await invoke("recordings.openStretch", [.init(name: "id", value: .text(id))], needs: Self.dsrVersion)
    }

    /// Erases everything the recorder keeps now. It cannot be undone: the
    /// caller asks first. Rewound slices go live first; saved stretches stay.
    public func eraseAll() async throws {
        _ = try await invoke("dsr.eraseAll", [], needs: Self.dsrVersion)
    }

    private static func viewArguments(_ view: DsrPanView) -> [CommandArgument] {
        [.init(name: "fftSize", value: .int(Int64(view.fftSize))),
         .init(name: "windowType", value: .int(Int64(view.windowType))),
         .init(name: "centreHz", value: .int(view.centreHz)),
         .init(name: "spanHz", value: .int(view.spanHz)),
         .init(name: "pixels", value: .int(Int64(view.pixels)))]
    }

    private static func frames(_ result: CommandResult) throws -> DsrPastFrames {
        guard let slice = whole(result.values["slice"]), let binCount = whole(result.values["binCount"]),
              let first = real(result.values["firstBinHz"]), let step = real(result.values["hzPerBin"]),
              case .text(let times)? = result.values["times"], case .text(let bins)? = result.values["bins"] else {
            throw RecordingsError.badAnswer
        }
        var transmit: String?
        if case .text(let marks)? = result.values["transmit"] {
            transmit = marks
        }
        guard let frames = DsrPastFrames.decode(slice: Int(slice), firstBinHz: first, hzPerBin: step,
                                                binCount: Int(binCount), times: times, bins: bins, transmit: transmit,
                                                rowsLeft: Int(whole(result.values["rowsLeft"]) ?? 0),
                                                nextToUtcNs: whole(result.values["nextToUtcNs"]) ?? 0) else {
            throw RecordingsError.badAnswer
        }
        return frames
    }

    private static func note(_ result: CommandResult) -> String {
        if case .text(let note)? = result.values["note"] {
            return note
        }
        return ""
    }

    private static func real(_ value: MirrorValue?) -> Double? {
        switch value {
        case .double(let real)?:
            return real
        case .int(let whole)?:
            return Double(whole)
        default:
            return nil
        }
    }

    // MARK: Inside

    private func admit(needs: Int64) throws {
        switch state {
        case .offline:
            throw RecordingsError.notConnected
        case .needsCoreUpdate, .ready:
            if coreVersion < needs {
                throw RecordingsError.needsCoreUpdate
            }
        }
    }

    private func refuseOnAir(_ n: Int) throws {
        if let reason = voiceKeyer?.onAirReason(n) {
            throw RecordingsError.refused(reason)
        }
    }

    /// Sends one verb the Core answers once and returns its accepted answer.
    private func invoke(_ verb: String, _ arguments: [CommandArgument], needs: Int64 = RecordingsClient.recordingsVersion,
                        timeout: Duration? = nil) async throws -> CommandResult {
        try admit(needs: needs)
        let result: CommandResult
        do {
            result = try await commands.invoke(verb, arguments: arguments, timeout: timeout ?? self.timeout)
        } catch let error as CommandError {
            throw RecordingsError.notAnswered(error)
        }
        guard result.accepted else {
            throw RecordingsError.refused(result.reason)
        }
        return result
    }

    /// Asks for a file a chunk at a time from offset 0 until it is whole,
    /// writing each chunk to `url` as it comes.
    private func copyFile(verb: String, needs: Int64, to url: URL, progress: (@MainActor (Int64, Int64) -> Void)?,
                          arguments: (Int64) -> [CommandArgument]) async throws {
        try admit(needs: needs)
        guard FileManager.default.createFile(atPath: url.path, contents: nil),
              let handle = try? FileHandle(forWritingTo: url) else {
            throw RecordingsError.localFile(url.lastPathComponent)
        }
        var whole = false
        defer {
            try? handle.close()
            if !whole {
                try? FileManager.default.removeItem(at: url)
            }
        }
        var received: Int64 = 0
        while true {
            let result = try await invoke(verb, arguments(received), needs: needs)
            guard Self.whole(result.values["offset"]) == received, let size = Self.whole(result.values["size"]),
                  size >= received, case .text(let text)? = result.values["data"],
                  let chunk = Data(base64Encoded: text) else {
                throw RecordingsError.badAnswer
            }
            if chunk.isEmpty && received < size {
                throw RecordingsError.badAnswer
            }
            do {
                try handle.write(contentsOf: chunk)
            } catch {
                throw RecordingsError.localFile(error.localizedDescription)
            }
            received += Int64(chunk.count)
            guard received <= size else {
                throw RecordingsError.badAnswer
            }
            progress?(received, size)
            if received == size {
                whole = true
                return
            }
        }
    }

    private static func whole(_ value: MirrorValue?) -> Int64? {
        switch value {
        case .int(let whole)?, .enumeration(let whole)?:
            return whole
        default:
            return nil
        }
    }

    private static func flag(_ value: MirrorValue?) -> Bool? {
        if case .bool(let on)? = value {
            return on
        }
        return nil
    }
}
