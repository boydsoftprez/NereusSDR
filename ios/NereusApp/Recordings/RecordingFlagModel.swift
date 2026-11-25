// NereusSDR for iOS: what each flag's Record and Play show and do, the playing bar's words and the Kept notice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 16, the phone's flag Record and Play.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: The armed ring drops its AUTO caption. J.J. Boyd (KG4VCF),
//   AI-assisted via Claude Code.
// 2026-11-17: PR 808 design calls, the playing bar's shorter names (the
//   mode, then the frequency go first). J.J. Boyd (KG4VCF), AI-assisted
//   via Claude Code.
// 2026-11-19: A WAV kept by a Core with no MP3 encoder says so; "failed"
//   only where an encoder failed (JJ's phone run of PR 808). J.J. Boyd
//   (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusBand
import NereusMirror
import NereusModels
import Observation

/// Record and Play in each full flag's column, the blue playing bar under
/// the flag and the Kept notice (design 2026-11-15-recording-voice-keyer-dsr
/// sections 3, 6.2, 6.3 and 6.8), on the Core's recordings as
/// ``RecordingsClient`` holds them.
///
/// One recording per slice, made on the Core: every device's Record shows
/// the same state, and any device starts or stops it. Play plays the
/// slice's latest recording on this phone's own speaker; the playing bar's
/// Others picks one of the slice's last eight. A Core too old for
/// recordings leaves both buttons visible and greyed, and a tap on one
/// writes the reason under the flag (V9).
@MainActor
@Observable
final class RecordingFlagModel {
    /// Spec 7, not connected: everything stays in place, dimmed, with this note.
    nonisolated static let notConnectedReason = "Connect to a radio to use recordings."
    /// How long the Kept notice and a reason stay under the flag.
    nonisolated static let noticeSeconds: Double = 6
    /// Others lists this many of the slice's recordings.
    nonisolated static let othersCount = 8

    /// What Record shows.
    enum RecordLook: Equatable {
        case idle
        /// Start by itself when I transmit is on: a hollow red ring.
        case armed
        /// Blinks red with the running time under it.
        case recording(elapsedMs: Int64)
    }

    /// What Play shows.
    enum PlayLook: Equatable {
        case idle
        /// A blue stop square with the position under it.
        case playing(positionMs: Int64)
        /// The same in red: this phone's key sends it on the air.
        case onAir(positionMs: Int64)
    }

    /// One flag's Record and Play.
    struct Look: Equatable {
        var record: RecordLook
        var play: PlayLook
        /// Why Record cannot act now; nil when it can.
        var recordReason: String?
        /// Why Play cannot act now; nil when it can.
        var playReason: String?

        /// A column with nothing to show: the flag pictures.
        static let idle = Look(record: .idle, play: .idle, recordReason: nil, playReason: nil)
    }

    /// The playing bar under the flag.
    struct Bar: Equatable {
        let title: String
        let positionMs: Int64
        let lengthMs: Int64
        let onAir: Bool
        /// Shorter names for the bar when `title` does not fit, most kept
        /// first: the mode goes, then the frequency (the flag shows it).
        var shorter: [String] = []

        /// "0:12 / 3:25 · speaker", or "ON AIR · 0:12 / 3:25" while on the air.
        var detail: String {
            let times = RecordingFlagModel.lengthText(positionMs) + " / " + RecordingFlagModel.lengthText(lengthMs)
            return onAir ? "ON AIR · " + times : times + " · speaker"
        }

        /// How far through, 0 to 1.
        var fraction: Double {
            lengthMs > 0 ? min(1, max(0, Double(positionMs) / Double(lengthMs))) : 0
        }
    }

    /// The notice under the flag: a recording just kept, or why a control
    /// cannot act.
    enum Notice: Equatable {
        /// "Kept in Recordings: <name>, <length>", why it stopped, and Play.
        case kept(id: String, title: String, reason: String)
        /// Why Record or Play cannot act, in the tool pages' reason style.
        case reason(String)
    }

    let client: RecordingsClient
    /// Each slice's last eight Contact recordings, newest first.
    private(set) var recent: [Int: [RecordingEntry]] = [:]
    /// The recording a flag asked to play, and that flag's slice.
    private(set) var playRequest: (id: String, slice: Int)?
    /// The blink pauses for still pictures.
    var blinks = true
    /// The Core's words for a call it refused, shown the way the band shows reasons.
    @ObservationIgnored var showRefusal: (String) -> Void = { _ in }

    /// The notice under one flag, and when it went up.
    private var notices: [Int: (notice: Notice, at: Date)] = [:]
    /// Moves on with every notice, so a view waiting to take one down knows which.
    private(set) var noticeSerial = 0
    /// The Kept `seq` the document carried when it first came, which is
    /// old news; nil before a document.
    @ObservationIgnored private var keptBaseline: Int64?
    @ObservationIgnored private var keptShown: Int64 = 0
    @ObservationIgnored private var listedRevision: [Int: Int64] = [:]

    init(client: RecordingsClient) {
        self.client = client
    }

    // MARK: What the flags show

    /// Why a control that needs `recordingsVersion` `needs` cannot act now.
    func unavailableReason(needs: Int64 = RecordingsClient.recordingsVersion) -> String? {
        if client.state == .offline {
            return Self.notConnectedReason
        }
        return client.unavailableReason(needs: needs)
    }

    /// One slice's Record and Play (0 is Slice A).
    func look(slice: Int) -> Look {
        let reason = unavailableReason()
        var record = RecordLook.idle
        if reason == nil, let state = client.document?.slice(slice) {
            switch state.state {
            case .idle:
                record = .idle
            case .armed:
                record = .armed
            case .recording:
                record = .recording(elapsedMs: max(0, state.elapsedMs))
            }
        }
        var play = PlayLook.idle
        let playback = client.playback
        if reason == nil, playback.playing, shownSlice() == slice {
            play = playback.onAir ? .onAir(positionMs: playback.positionMs) : .playing(positionMs: playback.positionMs)
        }
        var playReason = reason
        if playReason == nil, play == .idle, latest(slice: slice) == nil {
            playReason = "Nothing recorded on \(Self.sliceName(slice))."
        }
        return Look(record: record, play: play, recordReason: reason, playReason: playReason)
    }

    /// The playing bar under `slice`'s flag, only while this phone plays.
    func bar(slice: Int) -> Bar? {
        let playback = client.playback
        guard unavailableReason() == nil, playback.playing, shownSlice() == slice else {
            return nil
        }
        let entry = recent.values.lazy.flatMap { $0 }.first { $0.id == playback.id }
        return Bar(title: entry.map(Self.title(of:)) ?? "Recording", positionMs: playback.positionMs,
                   lengthMs: playback.lengthMs, onAir: playback.onAir,
                   shorter: entry.map(Self.shorterTitles(of:)) ?? [])
    }

    /// The notice under `slice`'s flag. A Kept notice gives way while the
    /// slice records again or this phone plays.
    func notice(slice: Int) -> Notice? {
        guard let held = notices[slice] else {
            return nil
        }
        if case .kept = held.notice {
            if case .recording = look(slice: slice).record {
                return nil
            }
            if bar(slice: slice) != nil {
                return nil
            }
        }
        return held.notice
    }

    /// Others: the slice's last eight, newest first.
    func others(slice: Int) -> [RecordingEntry] {
        Array((recent[slice] ?? []).prefix(Self.othersCount))
    }

    /// The slice's latest recording, which Play plays.
    func latest(slice: Int) -> RecordingEntry? {
        recent[slice]?.first
    }

    // MARK: Taps

    /// Record: start, or stop while it records. Greyed, it writes why.
    func tapRecord(slice: Int) async {
        let look = look(slice: slice)
        if let reason = look.recordReason {
            post(.reason(reason), slice: slice)
            return
        }
        await run {
            if case .recording = look.record {
                try await client.stop(slice: slice)
            } else {
                try await client.start(slice: slice)
            }
        }
    }

    /// Play: the slice's latest recording, or stop while it plays here.
    func tapPlay(slice: Int) async {
        let look = look(slice: slice)
        if look.play != .idle {
            await stop()
            return
        }
        if let reason = look.playReason {
            post(.reason(reason), slice: slice)
            return
        }
        guard let entry = latest(slice: slice) else {
            return
        }
        await play(entry, slice: slice)
    }

    /// Plays one recording on this phone's speaker, its bar under `slice`'s flag.
    func play(_ entry: RecordingEntry, slice: Int) async {
        if let reason = unavailableReason() {
            post(.reason(reason), slice: slice)
            return
        }
        playRequest = (entry.id, slice)
        clearKept(slice: slice)
        await run { try await client.play(id: entry.id) }
    }

    /// The Kept notice's Play.
    func playKept(slice: Int) async {
        guard case .kept(let id, _, _)? = notices[slice]?.notice else {
            return
        }
        if let reason = unavailableReason() {
            post(.reason(reason), slice: slice)
            return
        }
        playRequest = (id, slice)
        clearKept(slice: slice)
        await run { try await client.play(id: id) }
    }

    /// The bar's Stop.
    func stop() async {
        await run { try await client.stopPlay() }
    }

    // MARK: Keeping up

    /// Asks for the slice's last eight when the Core's list has changed
    /// since the last ask.
    func refreshList(slice: Int) async {
        guard unavailableReason() == nil, let revision = client.index?.revision else {
            return
        }
        guard listedRevision[slice] != revision else {
            return
        }
        listedRevision[slice] = revision
        do {
            let page = try await client.list(offset: 0, count: Self.othersCount, slice: slice, kind: .contact)
            received(page, slice: slice)
        } catch {
            listedRevision[slice] = nil
        }
    }

    /// One page of the slice's recordings, newest first.
    func received(_ page: RecordingsPage, slice: Int) {
        recent[slice] = page.entries.filter { $0.kind == .contact }
    }

    /// The document's Kept moved: a recording just stopped, so its flag
    /// shows the notice. The Kept a document already carries when it first
    /// comes is old news. Call it whenever `client.kept` changes.
    func keptChanged(now: Date = Date()) {
        guard let kept = client.kept else {
            keptBaseline = nil
            recent = [:]
            listedRevision = [:]
            return
        }
        guard let baseline = keptBaseline else {
            keptBaseline = kept.seq
            keptShown = kept.seq
            return
        }
        guard kept.seq > baseline, kept.seq > keptShown, !kept.id.isEmpty else {
            return
        }
        keptShown = kept.seq
        let title = "Kept in Recordings: " + kept.name + ", " + Self.lengthText(kept.lengthMs)
        let reason = Self.keptReason(kept, quietMinutes: client.settings?.quietMinutes ?? 2,
                                     noMp3: client.settings?.noMp3 ?? false)
        post(.kept(id: kept.id, title: title, reason: reason), slice: kept.slice, now: now)
    }

    /// Takes down each notice that has been up its six seconds by `now`.
    func expireNotices(now: Date = Date()) {
        notices = notices.filter { now.timeIntervalSince($0.value.at) < Self.noticeSeconds }
    }

    // MARK: Words

    /// "Slice A" for 0.
    nonisolated static func sliceName(_ slice: Int) -> String {
        guard (0..<26).contains(slice), let scalar = UnicodeScalar(65 + slice) else {
            return "Slice \(slice)"
        }
        return "Slice " + String(Character(scalar))
    }

    /// "3:25", or "1:02:03" past an hour.
    nonisolated static func lengthText(_ ms: Int64) -> String {
        let seconds = max(0, ms) / 1000
        let h = seconds / 3600
        let m = (seconds / 60) % 60
        let s = seconds % 60
        let ss = s < 10 ? "0\(s)" : "\(s)"
        if h > 0 {
            return "\(h):" + (m < 10 ? "0\(m)" : "\(m)") + ":" + ss
        }
        return "\(m):" + ss
    }

    /// A recording as the bar and Others name it: "7.236.400 LSB K4ABC",
    /// with the call when the Core knows it.
    nonisolated static func title(of entry: RecordingEntry) -> String {
        var text = BandSlice.frequencyText(hz: Double(entry.frequencyHz))
        if !entry.mode.isEmpty {
            text += " " + entry.mode
        }
        if !entry.call.isEmpty {
            text += " " + entry.call
        }
        return text
    }

    /// Shorter names for the playing bar, most kept first: without the
    /// mode ("7.236.400 K4ABC"), then the call alone ("K4ABC"), or the
    /// frequency alone with no call (PR 808 screen walk).
    nonisolated static func shorterTitles(of entry: RecordingEntry) -> [String] {
        let frequency = BandSlice.frequencyText(hz: Double(entry.frequencyHz))
        guard !entry.call.isEmpty else {
            return entry.mode.isEmpty ? [] : [frequency]
        }
        return (entry.mode.isEmpty ? [] : [frequency + " " + entry.call]) + [entry.call]
    }

    /// Why it stopped, in the desktop flag's words.
    /// `noMp3`: the Core says it has no MP3 encoder, so a WAV was not a failure.
    nonisolated static func keptReason(_ kept: RecordingsDocument.Kept, quietMinutes: Int,
                                       noMp3: Bool = false) -> String {
        switch kept.reason {
        case .device?:
            return stoppedOn(kept.device)
        case .quiet?:
            let minutes = max(1, quietMinutes)
            return minutes == 1 ? "Stopped by itself after one quiet minute"
                : "Stopped by itself after \(numberWord(minutes)) quiet minutes"
        case .diskFull?:
            return "Stopped: the disk is full"
        case .wav?:
            return noMp3 ? "Kept as WAV: this Core has no MP3 encoder" : "Kept as WAV: the MP3 encoder failed"
        case .here?, nil:
            // The Core's own window: on the phone it is never this device.
            return "Stopped on the Core"
        }
    }

    /// "Stopped on the iPhone", "Stopped on JJ's iPad", "Stopped on another
    /// device": a one-word device name reads with "the".
    nonisolated static func stoppedOn(_ device: String) -> String {
        let name = device.trimmingCharacters(in: .whitespacesAndNewlines)
        if name.isEmpty || name == "Another device" {
            return "Stopped on another device"
        }
        if !name.contains(" ") && !name.contains("'") && !name.contains("\u{2019}") {
            return "Stopped on the " + name
        }
        return "Stopped on " + name
    }

    nonisolated private static func numberWord(_ n: Int) -> String {
        let words = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"]
        return (0...10).contains(n) ? words[n] : String(n)
    }

    // MARK: Inside

    /// The slice whose flag carries this phone's playback: the one whose
    /// Play started it, else the recording's own slice, else Slice A.
    private func shownSlice() -> Int {
        let id = client.playback.id
        if let playRequest, playRequest.id == id {
            return playRequest.slice
        }
        for (slice, entries) in recent where entries.contains(where: { $0.id == id }) {
            return slice
        }
        return 0
    }

    private func post(_ notice: Notice, slice: Int, now: Date = Date()) {
        notices[slice] = (notice, now)
        noticeSerial += 1
    }

    private func clearKept(slice: Int) {
        if case .kept? = notices[slice]?.notice {
            notices[slice] = nil
        }
    }

    /// Runs one call, showing the Core's words when it says no.
    private func run(_ call: () async throws -> Void) async {
        do {
            try await call()
        } catch let error as RecordingsError {
            if let reason = error.reason {
                showRefusal(reason)
            }
        } catch {
            return
        }
    }
}
