// NereusSDR for iOS: Tools > Voice Keyer: record M1 to M8 from this phone's microphone, send, listen, rename, import, save and repeat
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 18, the phone's voice keyer page and its model.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: JJ's ruling on PR 797: an imported file is sent as imported (a fixed level
//   on the air), a clip from this phone's microphone is not. J.J. Boyd (KG4VCF),
//   AI-assisted via Claude Code.
// 2026-11-17: The switch that takes VOICE off the main screen (JJ's request),
//   kept on this device. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-17: Screen walk for PR 808, Record's "Recording 0:03" keeps to one
//   line on a small phone. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Combine
import Foundation
import NereusLink
import NereusMirror
import NereusModels
import Observation
import os
import SwiftUI
import UniformTypeIdentifiers

/// The voice keyer on the phone (design 2026-11-15-recording-voice-keyer-dsr
/// sections 3, 6.6 and 6.8): eight messages kept on the Core, sent on the
/// air by the Core's voice keyer as this phone. Tools > Voice Keyer and the
/// round VOICE beside PTT both work it.
///
/// A tap on a message sends it: the one call that keys the radio, and only
/// when the operator asks. The Core keys as this phone, so while the Core
/// sends this phone's message the app keeps its transmit keepalives going
/// (``RecordingsClient/voiceMessageSending``, wired in ``AppModel``).
/// Tapping the message again, Stop, VOICE or PTT stops it (ruling 8.5:
/// only this phone's own controls stop its message; another device's Stop
/// is refused in the Core's words, and that device acts only by Take
/// transmit). While another station holds transmit the messages dim and
/// Take transmit is offered; listening and recording still work.
///
/// A message is recorded from this phone's own microphone before any
/// processing, 48 kHz mono, at most two minutes (``PhoneMicClipRecorder``),
/// then loaded into the Core's message, as Import WAV loads a file. Every
/// control stays shown: a Core too old for the voice keyer, or no Core,
/// leaves it all greyed with the reason (V9).
@MainActor
@Observable
final class VoiceKeyerModel {
    /// What a tap on a message did.
    enum TapResult: Equatable {
        case sent
        case stopped
        /// The message is empty: the page records or imports into it.
        case empty
        /// It could not act; ``note`` says why.
        case refused
    }

    /// The message going out, as it stands now.
    struct Progress: Equatable {
        let n: Int
        let positionMs: Int64
        let lengthMs: Int64
        /// Above 0 between repeats, counting down.
        let waitingMs: Int64
        /// This phone sent it.
        let mine: Bool
        /// The sender in the Core's words.
        let by: String

        var waiting: Bool { waitingMs > 0 }
        /// How far through, 0 to 1; 0 while waiting.
        var fraction: Double {
            guard !waiting, lengthMs > 0 else {
                return 0
            }
            return min(1, max(0, Double(positionMs) / Double(lengthMs)))
        }
        /// Whole seconds before the next repeat, at least 1 while waiting.
        var waitSeconds: Int64 { max(1, (waitingMs + 999) / 1000) }
    }

    /// One line under the page's cards: what happened, or why not.
    struct Note: Equatable {
        let text: String
        /// The Core or the phone refused; drawn as a refusal.
        let refusal: Bool
    }

    // MARK: Words

    nonisolated static let slotNumbers = Array(VoiceKeyerState.slotRange)
    nonisolated static let pageNote = "Eight messages kept on the Core. Tap one to send it on the air."
    nonisolated static let footNote = "Sending keys the radio and goes back to receive at the end. Stop, PTT or "
        + "tapping the message again ends it early. Your voice is kept as it comes from the mic, and the transmit "
        + "processing is applied each time it is sent."
    nonisolated static let repeatDetail = "Calls CQ until someone answers"
    nonisolated static let allFullText = "All eight messages are full. Use ··· on a message to replace it."
    nonisolated static let listenDetail = "On this phone only, not on the air"
    nonisolated static let renameMessage = "A name for this message, up to 32 characters. Every phone and computer shows it."
    nonisolated static func emptyText(_ n: Int) -> String {
        "M\(n) is empty. Tap Record, speak, then Stop, or Import WAV."
    }
    nonisolated static func emptyReason(_ n: Int) -> String { "M\(n) is empty." }
    nonisolated static func firstEmptyText(_ n: Int) -> String { "Fills the first empty message, M\(n)" }

    // MARK: State

    let client: RecordingsClient
    let recorder: PhoneMicClipRecorder
    /// Why this phone may not transmit now, in the Core's words with its
    /// fix; nil when it may. Fed from ``TransmitModel``'s permission.
    var held: TxRefusalInfo?
    /// The message Record records into.
    var recordInto = 1
    /// What happened last, or why a tap did nothing.
    private(set) var note: Note?
    /// The message being renamed, and the name typed so far.
    var renaming: Int?
    var renameText = ""
    /// The message the file picker loads into.
    var importing: Int?
    /// The clear waiting for its confirm.
    var pendingClear: Int?
    /// A saved message for the share sheet.
    var shared: RecordingsListModel.SharedFiles?
    /// The Core's words for a refusal on the pan, which shows them under the flag.
    @ObservationIgnored var showRefusal: (String) -> Void = { _ in }
    /// The transmit permission's feed, kept while the model lives.
    @ObservationIgnored var heldFeed: AnyCancellable?

    @ObservationIgnored private var stamp: (sending: VoiceKeyerState.Sending, at: Date)?
    @ObservationIgnored private let folder: URL
    private static let logger = Logger(subsystem: "NereusSDR", category: "recordings.voiceKeyer")

    init(client: RecordingsClient, recorder: PhoneMicClipRecorder,
         folder: URL = FileManager.default.temporaryDirectory.appendingPathComponent("VoiceKeyerFiles",
                                                                                     isDirectory: true)) {
        self.client = client
        self.recorder = recorder
        self.folder = folder
    }

    // MARK: What the screens read

    /// Why nothing here can act now: no Core, or one too old for the voice keyer.
    var reason: String? {
        if client.state == .offline {
            return RecordingFlagModel.notConnectedReason
        }
        return client.unavailableReason(needs: RecordingsClient.voiceKeyerVersion)
    }

    /// M1 to M8, empty where the Core sends none.
    var slots: [VoiceKeyerState.Slot] {
        Self.slotNumbers.map { slot($0) }
    }

    func slot(_ n: Int) -> VoiceKeyerState.Slot {
        client.voiceKeyer?.slot(n) ?? VoiceKeyerState.Slot(n: n, name: "", lengthMs: 0, empty: true)
    }

    /// "M2" for a message with no name, else its name.
    func label(_ n: Int) -> String {
        let name = slot(n).name
        return name.isEmpty ? "M\(n)" : name
    }

    /// The first empty message, which Import WAV fills.
    var firstEmpty: Int? {
        slots.first { $0.empty }?.n
    }

    /// Why a message cannot be sent now: no Core, or transmit held elsewhere.
    var sendReason: String? {
        reason ?? held?.reason
    }

    /// Take transmit is the fix for ``held``.
    var offersTake: Bool {
        reason == nil && held?.fix == TxRefusalInfo.takeTransmit
    }

    var repeatOn: Bool { client.voiceKeyer?.repeat ?? false }
    var repeatDelaySec: Int { client.voiceKeyer?.repeatDelaySec ?? 5 }

    /// The message going out at `now`: the Core's position moved on by the
    /// time since the document said it, as the desktop's VOICE counts.
    func progress(now: Date = Date()) -> Progress? {
        guard reason == nil, let sending = client.voiceKeyer?.sending else {
            stamp = nil
            return nil
        }
        if stamp?.sending != sending {
            stamp = (sending, now)
        }
        let elapsed = Int64(max(0, now.timeIntervalSince(stamp?.at ?? now)) * 1000)
        let position = sending.waiting ? sending.lengthMs
            : (sending.lengthMs > 0 ? min(sending.lengthMs, sending.positionMs + elapsed) : sending.positionMs)
        let waiting = sending.waiting ? max(1, sending.waitingMs - elapsed) : 0
        return Progress(n: sending.n, positionMs: position, lengthMs: sending.lengthMs, waitingMs: waiting,
                        mine: client.voiceMessageSending, by: sending.by)
    }

    // MARK: Words for a message

    /// "0:06", a message's length rounded up, as the desktop shows it.
    nonisolated static func lengthText(_ ms: Int64) -> String {
        RecordingFlagModel.lengthText(ms > 0 ? ms + 999 : 0)
    }

    /// A row's line: the time going out, the wait, its length, or what an empty one takes.
    func rowDetail(_ n: Int, progress: Progress?) -> String {
        let slot = slot(n)
        if let progress, progress.n == n {
            if progress.waiting {
                return "Sends again in \(progress.waitSeconds) s"
            }
            return "On the air " + RecordingFlagModel.lengthText(progress.positionMs) + " / "
                + Self.lengthText(progress.lengthMs) + (repeatOn ? " · repeats every \(repeatDelaySec) s" : "")
        }
        if slot.empty {
            return "Tap to record or import"
        }
        return Self.lengthText(slot.lengthMs) + (repeatOn ? " · repeats" : "")
    }

    /// The foot bar's two lines while a message goes out.
    func airText(_ progress: Progress) -> (title: String, detail: String) {
        let from = progress.mine || progress.by.isEmpty ? "" : " from " + progress.by
        let what = "M\(progress.n) " + label(progress.n)
        if progress.waiting {
            return ("Waiting to repeat" + from + ": " + what, "Sends again in \(progress.waitSeconds) s")
        }
        return ("On the air" + from + ": " + what,
                RecordingFlagModel.lengthText(progress.positionMs) + " / " + Self.lengthText(progress.lengthMs))
    }

    /// The panel's Repeat line.
    var repeatSummary: String {
        repeatOn ? "Sends again every \(repeatDelaySec) s until you stop it" : "Off"
    }

    /// "M5 · CQ" or "M5 · empty", Record into's choice.
    func intoLabel(_ n: Int) -> String {
        let slot = slot(n)
        return "M\(n) · " + (slot.empty ? "empty" : label(n))
    }

    /// Import WAV…'s line.
    var importDetail: String {
        firstEmpty.map(Self.firstEmptyText) ?? Self.allFullText
    }

    // MARK: Sending

    /// A tap on a message: sends it, stops it while it goes out, or, for an
    /// empty one, points Record at it.
    @discardableResult
    func tapMessage(_ n: Int) async -> TapResult {
        if let reason {
            say(reason, refusal: true)
            return .refused
        }
        if slot(n).empty {
            recordInto = n
            say(Self.emptyText(n), refusal: false)
            return .empty
        }
        if client.voiceKeyer?.sending?.n == n {
            return await stop() ? .stopped : .refused
        }
        if let held {
            say(held.reason, refusal: true)
            return .refused
        }
        return await send(n) ? .sent : .refused
    }

    /// Sends Mn on the air.
    @discardableResult
    func send(_ n: Int) async -> Bool {
        if let why = sendReason {
            say(why, refusal: true)
            return false
        }
        note = nil
        return await run { try await client.sendVoice(n) }
    }

    /// Stops the message going out, or the wait between its repeats.
    @discardableResult
    func stop() async -> Bool {
        if let reason {
            say(reason, refusal: true)
            return false
        }
        note = nil
        return await run { try await client.stopVoice() }
    }

    /// PTT tapped: while the Core sends this phone's message it stops it and
    /// keys nothing (true); otherwise it leaves the PTT alone (false).
    func pttTapped() -> Bool {
        guard client.voiceMessageSending else {
            return false
        }
        Task { await stop() }
        return true
    }

    /// The round VOICE: stops a message going out; otherwise false, and the
    /// messages open over the pan.
    func voiceTapped() async -> Bool {
        guard progress() != nil else {
            return false
        }
        await stop()
        return true
    }

    // MARK: Recording

    /// Record: records this phone's microphone into ``recordInto``. The
    /// first time, iOS asks for the microphone here.
    func record() async {
        if let reason {
            say(reason, refusal: true)
            return
        }
        guard !recorder.isRecording else {
            return
        }
        let n = recordInto
        if let onAir = client.voiceKeyer?.onAirReason(n) {
            say(onAir, refusal: true)
            return
        }
        note = nil
        if let why = await recorder.start(slot: n) {
            say(why, refusal: true)
        }
    }

    /// Stop: ends the recording and loads it, or stops the message going out.
    func stopTapped() async {
        if recorder.isRecording {
            if let clip = recorder.finish() {
                await load(clip)
            }
            return
        }
        if progress() != nil {
            await stop()
        }
    }

    /// The recording's clock and level; a recording that reached two
    /// minutes stops by itself and loads.
    func tick() async {
        if let clip = recorder.tick() {
            await load(clip)
        }
    }

    /// Loads a finished clip into its message, as the desktop does.
    func load(_ clip: PhoneMicClipRecorder.Clip) async {
        defer { try? FileManager.default.removeItem(at: clip.url) }
        let n = clip.slot
        say(clip.reachedLimit ? "Recorded M\(n) (2:00, the most a message holds). Sending it to the Core."
                : "Recorded M\(n) (" + String(format: "%.1f s", Double(clip.lengthMs) / 1000) + "). Sending it to the Core.",
            refusal: false)
        // Recorded on the microphone: it goes out at the mic gain.
        if await upload(n, url: clip.url, name: "", imported: false) {
            // The next empty message, not counting this one: the Core's
            // document saying it is full may not have arrived yet.
            if let next = slots.first(where: { $0.empty && $0.n != n })?.n {
                recordInto = next
            }
        }
    }

    // MARK: Import, save, rename, clear, listen

    /// Import WAV…: the first empty message, or why there is none.
    func importFirstEmpty() {
        guard let n = firstEmpty else {
            say(reason ?? Self.allFullText, refusal: reason != nil)
            return
        }
        askImport(n)
    }

    /// Opens the file picker for Mn, unless Mn is on the air.
    func askImport(_ n: Int) {
        if let reason {
            say(reason, refusal: true)
            return
        }
        if let onAir = client.voiceKeyer?.onAirReason(n) {
            say(onAir, refusal: true)
            return
        }
        importing = n
    }

    /// The picker's file: copied from where it is, then loaded into Mn.
    /// A message with a name keeps it; one without takes the file's.
    func importFile(_ source: URL, into n: Int) async {
        let scoped = source.startAccessingSecurityScopedResource()
        defer {
            if scoped {
                source.stopAccessingSecurityScopedResource()
            }
        }
        let copy = folder.appendingPathComponent(UUID().uuidString).appendingPathComponent(source.lastPathComponent)
        do {
            try FileManager.default.createDirectory(at: copy.deletingLastPathComponent(), withIntermediateDirectories: true)
            try FileManager.default.copyItem(at: source, to: copy)
        } catch {
            say("M\(n) was not saved: that file could not be read on this phone.", refusal: true)
            return
        }
        defer { try? FileManager.default.removeItem(at: copy.deletingLastPathComponent()) }
        let name = slot(n).name.isEmpty
            ? String(source.deletingPathExtension().lastPathComponent.prefix(VoiceKeyerState.nameMaxCharacters)) : ""
        say("Loading " + source.lastPathComponent + " into M\(n).", refusal: false)
        // A chosen file: the Core sends it at a fixed level (JJ, 2026-11-16).
        _ = await upload(n, url: copy, name: name, imported: true)
    }

    /// Loads a file into Mn on the Core: "Saved M5.", or why not.
    private func upload(_ n: Int, url: URL, name: String, imported: Bool) async -> Bool {
        do {
            try await client.importVoice(n, url: url, name: name, imported: imported)
            say("Saved M\(n).", refusal: false)
            return true
        } catch let error as RecordingsError {
            say("M\(n) was not saved: " + Self.words(error), refusal: true)
        } catch {
            say("M\(n) was not saved: " + error.localizedDescription, refusal: true)
        }
        return false
    }

    /// Plays Mn on this phone only.
    func listen(_ n: Int) async {
        if let why = reason ?? (slot(n).empty ? Self.emptyReason(n) : nil) {
            say(why, refusal: true)
            return
        }
        note = nil
        if await run({ try await client.listenVoice(n) }) {
            say("Listening to M\(n) on this phone, not on the air.", refusal: false)
        }
    }

    /// Record again: points Record at Mn.
    func recordAgain(_ n: Int) {
        recordInto = n
        say(slot(n).empty ? "Tap Record, speak, then Stop." : "Tap Record, speak, then Stop. M\(n) is replaced.",
            refusal: false)
    }

    func startRename(_ n: Int) {
        if let why = reason ?? (slot(n).empty ? Self.emptyReason(n) : nil) {
            say(why, refusal: true)
            return
        }
        renaming = n
        renameText = slot(n).name
    }

    /// The rename's Save: at most 32 characters, which every device then shows.
    func commitRename() async {
        guard let n = renaming else {
            return
        }
        renaming = nil
        let name = String(renameText.trimmingCharacters(in: .whitespacesAndNewlines)
            .prefix(VoiceKeyerState.nameMaxCharacters))
        guard !name.isEmpty, name != slot(n).name else {
            return
        }
        note = nil
        await run { try await client.renameVoice(n, name: name) }
    }

    /// Clear: asks first; Mn on the air is refused in the Core's words.
    func askClear(_ n: Int) {
        if let why = reason ?? (slot(n).empty ? Self.emptyReason(n) : nil) ?? client.voiceKeyer?.onAirReason(n) {
            say(why, refusal: true)
            return
        }
        pendingClear = n
    }

    func confirmClear() async {
        guard let n = pendingClear else {
            return
        }
        pendingClear = nil
        note = nil
        if await run({ try await client.clearVoice(n) }) {
            say("M\(n) cleared.", refusal: false)
        }
    }

    /// Save as WAV…: copies Mn from the Core, then opens the share sheet.
    func saveAs(_ n: Int) async {
        if let why = reason ?? (slot(n).empty ? Self.emptyReason(n) : nil) {
            say(why, refusal: true)
            return
        }
        let directory = folder.appendingPathComponent(UUID().uuidString, isDirectory: true)
        let safe = label(n).replacingOccurrences(of: "/", with: "-").replacingOccurrences(of: ":", with: "-")
        let url = directory.appendingPathComponent(safe + ".wav")
        do {
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            try await client.exportVoice(n, to: url)
        } catch {
            try? FileManager.default.removeItem(at: directory)
            let words = (error as? RecordingsError).map(Self.words) ?? error.localizedDescription
            say("M\(n) was not saved as a WAV file: " + words, refusal: true)
            return
        }
        note = nil
        shared = RecordingsListModel.SharedFiles(urls: [url])
    }

    /// The share sheet closed: the copy on this phone goes.
    func shareClosed(_ files: RecordingsListModel.SharedFiles) {
        for url in files.urls {
            try? FileManager.default.removeItem(at: url.deletingLastPathComponent())
        }
    }

    // MARK: Repeat

    func setRepeat(_ on: Bool) async {
        await run { try await client.setVoiceRepeat(on: on, delaySec: repeatDelaySec) }
    }

    /// The wait between repeats, 1 to 60 seconds.
    func setDelay(_ seconds: Int) async {
        let clamped = min(VoiceKeyerState.repeatDelayRange.upperBound,
                          max(VoiceKeyerState.repeatDelayRange.lowerBound, seconds))
        guard clamped != repeatDelaySec else {
            return
        }
        await run { try await client.setVoiceRepeat(on: repeatOn, delaySec: clamped) }
    }

    // MARK: Inside

    private func say(_ text: String, refusal: Bool) {
        note = Note(text: text, refusal: refusal)
        if refusal {
            showRefusal(text)
        }
    }

    /// Runs one call; false, with the Core's words shown, when it says no.
    @discardableResult
    private func run(_ call: () async throws -> Void) async -> Bool {
        do {
            try await call()
            return true
        } catch let error as RecordingsError {
            say(Self.words(error), refusal: true)
        } catch {
            say(error.localizedDescription, refusal: true)
        }
        return false
    }

    /// The words for a failed call.
    nonisolated static func words(_ error: RecordingsError) -> String {
        if let reason = error.reason, !reason.isEmpty {
            return reason
        }
        switch error {
        case .notConnected:
            return RecordingFlagModel.notConnectedReason
        case .localFile:
            return "the file could not be read or written on this phone."
        case .notAnswered, .badAnswer, .needsCoreUpdate, .refused:
            return "The Core did not answer. Try again."
        }
    }
}

/// The page itself, under the Tools tab's bar.
struct VoiceKeyerPage: View {
    @Bindable var model: VoiceKeyerModel
    /// Take transmit, while another station holds it.
    var take: TransmitTakeModel?
    /// This device's own settings: whether VOICE shows on the main screen.
    var settings: PhoneSettings?

    var body: some View {
        // The clock runs only while a message goes out.
        TimelineView(.animation(minimumInterval: 0.2, paused: model.client.voiceKeyer?.sending == nil)) { timeline in
            let progress = model.progress(now: timeline.date)
            ZStack(alignment: .bottom) {
                ScrollView {
                    content(progress: progress)
                        .padding(12)
                        .padding(.bottom, progress == nil ? 8 : 80)
                }
                if let progress {
                    AirFoot(model: model, progress: progress)
                }
            }
        }
        .task(id: model.recorder.isRecording) {
            // The recording's clock and level, ten times a second.
            while model.recorder.isRecording, !Task.isCancelled {
                try? await Task.sleep(for: .milliseconds(100))
                await model.tick()
            }
        }
        .fileImporter(isPresented: Binding(get: { model.importing != nil }, set: { if !$0 { model.importing = nil } }),
                      allowedContentTypes: [.wav, .mp3]) { result in
            let n = model.importing
            model.importing = nil
            if let n, case .success(let url) = result {
                Task { await model.importFile(url, into: n) }
            }
        }
        .alert("Rename M\(model.renaming ?? 1)", isPresented: Binding(
            get: { model.renaming != nil },
            set: { if !$0 { model.renaming = nil } })) {
            TextField("Name (CQ, Call, 73...)", text: $model.renameText)
                .accessibilityIdentifier("voiceKeyer.renameField")
            Button("Save") {
                Task { await model.commitRename() }
            }
            Button("Cancel", role: .cancel) {
                model.renaming = nil
            }
        } message: {
            Text(VoiceKeyerModel.renameMessage)
        }
        .confirmationDialog("Clear M\(model.pendingClear ?? 1)?", isPresented: Binding(
            get: { model.pendingClear != nil },
            set: { if !$0 { model.pendingClear = nil } }), titleVisibility: .visible) {
            Button("Clear M\(model.pendingClear ?? 1)", role: .destructive) {
                Task { await model.confirmClear() }
            }
            .accessibilityIdentifier("voiceKeyer.confirmClear")
            Button("Cancel", role: .cancel) {
                model.pendingClear = nil
            }
        } message: {
            Text("The recording is erased from the Core for every phone and computer.")
        }
        .sheet(item: $model.shared, onDismiss: nil) { files in
            ShareSheet(urls: files.urls)
                .onDisappear { model.shareClosed(files) }
        }
    }

    @ViewBuilder
    private func content(progress: VoiceKeyerModel.Progress?) -> some View {
        let enabled = model.reason == nil
        VStack(alignment: .leading, spacing: 8) {
            if let reason = model.reason {
                ConnectChrome.NoticeBox(tone: .warn, title: nil, text: reason)
                    .accessibilityIdentifier("voiceKeyer.reason")
            }
            ConnectChrome.Note(text: VoiceKeyerModel.pageNote)
            SpotHubPage.Heading(text: "Record", tag: .core).padding(.top, 4)
            RecordCard(model: model, enabled: enabled)
                .opacity(enabled ? 1 : WorkspacePalette.disabledOpacity)
            SpotHubPage.Heading(text: "Messages", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                ForEach(VoiceKeyerModel.slotNumbers, id: \.self) { n in
                    if n > 1 {
                        SpotHubPage.Line()
                    }
                    MessageRow(model: model, n: n, progress: progress, enabled: enabled)
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
            .opacity(enabled ? 1 : WorkspacePalette.disabledOpacity)
            if let held = model.held, enabled {
                ToolPageParts.Reason(text: held.reason, identifier: "voiceKeyer.held")
                if let take, model.offersTake {
                    TakeTransmitButton(take: take, identifier: "voiceKeyer.takeTransmit")
                }
            }
            if let note = model.note {
                if note.refusal {
                    ToolPageParts.Refusal(text: note.text, identifier: "voiceKeyer.note")
                } else {
                    ConnectChrome.Note(text: note.text).accessibilityIdentifier("voiceKeyer.note")
                }
            }
            SpotHubPage.Card {
                Button {
                    model.importFirstEmpty()
                } label: {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Import WAV…")
                            .font(.system(size: 15))
                            .foregroundStyle(enabled ? ChromeColours.accentText : ChromeColours.buttonOffText)
                        Text(model.importDetail)
                            .font(.system(size: 12))
                            .foregroundStyle(ChromeColours.textDim)
                    }
                    .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 6)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(!enabled)
                .accessibilityIdentifier("voiceKeyer.import")
            }
            .padding(.top, 2)
            .opacity(enabled ? 1 : WorkspacePalette.disabledOpacity)
            SpotHubPage.Heading(text: "Repeat", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                SpotHubPage.SettingRow(title: "Repeat the message", detail: VoiceKeyerModel.repeatDetail) {
                    SpotHubPage.OnOff(isOn: model.repeatOn, enabled: enabled, identifier: "voiceKeyer.repeat") {
                        Task { await model.setRepeat(!model.repeatOn) }
                    }
                }
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Wait between") {
                    DelayStepper(model: model, enabled: enabled && model.repeatOn)
                }
                .opacity(model.repeatOn ? 1 : WorkspacePalette.disabledOpacity)
            }
            .opacity(enabled ? 1 : WorkspacePalette.disabledOpacity)
            if let settings {
                VoiceButtonSwitch(settings: settings).padding(.top, 8)
            }
            ConnectChrome.Note(text: VoiceKeyerModel.footNote).padding(.top, 4)
        }
    }
}

/// Whether the round VOICE shows beside PTT on the main screen: this
/// device's own choice, on by default, live with no Core and with an older
/// one. Off, the voice keyer still works from this page.
struct VoiceButtonSwitch: View {
    @ObservedObject var settings: PhoneSettings

    nonisolated static let title = "Show the VOICE button beside PTT"
    nonisolated static let detail = "Off, the messages are still here in Tools > Voice Keyer."

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: settings.isPad ? "On this iPad" : "On this phone", tag: .thisPhone)
            SpotHubPage.Card {
                SpotHubPage.SettingRow(title: Self.title, detail: Self.detail) {
                    SpotHubPage.OnOff(isOn: settings.voiceButtonShown, identifier: "voiceKeyer.showVoiceButton") {
                        settings.voiceButtonShown.toggle()
                    }
                }
            }
        }
    }
}

/// Record into M#, Record and Stop, and this phone's microphone level.
private struct RecordCard: View {
    let model: VoiceKeyerModel
    let enabled: Bool

    var body: some View {
        let recording = model.recorder.isRecording
        SpotHubPage.Card {
            SpotHubPage.SettingRow(title: "Record into") {
                Menu {
                    ForEach(VoiceKeyerModel.slotNumbers, id: \.self) { n in
                        Button(model.intoLabel(n)) {
                            model.recordInto = n
                        }
                    }
                } label: {
                    HStack(spacing: 4) {
                        Text(model.intoLabel(model.recordInto))
                            .lineLimit(1)
                        Image(systemName: "chevron.down").font(.system(size: 10, weight: .semibold))
                    }
                    .font(.system(size: 14))
                    .foregroundStyle(enabled && !recording ? ChromeColours.text : ChromeColours.buttonOffText)
                    .padding(.horizontal, 10)
                    .frame(height: 34)
                    .workspaceInset()
                }
                .disabled(!enabled || recording)
                .accessibilityLabel("Record into " + model.intoLabel(model.recordInto))
                .accessibilityIdentifier("voiceKeyer.recordInto")
            }
            SpotHubPage.Line()
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 8) {
                    Button {
                        Task { await model.record() }
                    } label: {
                        HStack(spacing: 7) {
                            Image(systemName: "circle.fill").font(.system(size: 12))
                            Text(recording ? "Recording " + RecordingFlagModel.lengthText(model.recorder.elapsedMs)
                                 : "Record")
                                .monospacedDigit()
                                // One line on a small phone, where "0:03" wrapped under "Recording".
                                .lineLimit(1)
                                .minimumScaleFactor(0.7)
                        }
                        .font(.system(size: 16, weight: .semibold))
                        .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(SetupButtonStyle(recording ? .keyed : .danger, fill: true))
                    .disabled(!enabled)
                    .accessibilityIdentifier("voiceKeyer.record")
                    Button {
                        Task { await model.stopTapped() }
                    } label: {
                        HStack(spacing: 7) {
                            Image(systemName: "stop.fill").font(.system(size: 12))
                            Text("Stop")
                        }
                        .font(.system(size: 16, weight: .semibold))
                        .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(SetupButtonStyle(.raised, fill: true))
                    .disabled(!enabled)
                    .accessibilityIdentifier("voiceKeyer.stop")
                }
                HStack(spacing: 8) {
                    Text("This phone's mic")
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .fixedSize()
                    LevelBar(level: Double(model.recorder.level))
                }
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("This phone's microphone level")
                .accessibilityValue("\(Int(model.recorder.level * 100)) percent")
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 10)
        }
    }
}

/// The microphone's level: green, then amber, then red near full scale.
struct LevelBar: View {
    let level: Double

    var body: some View {
        GeometryReader { proxy in
            ZStack(alignment: .leading) {
                Capsule().fill(ChromeColours.sliderTrack)
                Capsule()
                    .fill(LinearGradient(stops: [
                        .init(color: ChromeColours.gaugeNormal, location: 0),
                        .init(color: ChromeColours.gaugeNormal, location: 0.7),
                        .init(color: ChromeColours.gaugeWarn, location: 0.7),
                        .init(color: ChromeColours.gaugeWarn, location: 0.88),
                        .init(color: ChromeColours.gaugeRed, location: 0.88),
                    ], startPoint: .leading, endPoint: .trailing))
                    .mask(alignment: .leading) {
                        Rectangle().frame(width: proxy.size.width * min(1, max(0, level)))
                    }
            }
        }
        .frame(height: 8)
    }
}

/// One message: its number, its name and line, and the round ··· button.
/// A tap sends it (or stops it while it goes out); an empty one points
/// Record at it.
private struct MessageRow: View {
    let model: VoiceKeyerModel
    let n: Int
    let progress: VoiceKeyerModel.Progress?
    let enabled: Bool

    var body: some View {
        let slot = model.slot(n)
        let sending = progress?.n == n
        let target = model.recorder.isRecording && model.recorder.slot == n
        let dim = enabled && !slot.empty && model.held != nil && !sending
        HStack(spacing: 10) {
            Button {
                Task { await model.tapMessage(n) }
            } label: {
                HStack(spacing: 10) {
                    Text("M\(n)")
                        .font(.system(size: 14, weight: .bold))
                        .foregroundStyle(ChromeColours.textBright)
                        .frame(width: 30, alignment: .leading)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(slot.empty ? "Empty" : model.label(n))
                            .font(.system(size: 15, weight: slot.empty ? .regular : .semibold))
                            .foregroundStyle(sending ? ChromeColours.revokeText
                                             : slot.empty ? ChromeColours.textFaint : ChromeColours.textBright)
                            .lineLimit(1)
                        Text(model.rowDetail(n, progress: progress))
                            .font(.system(size: 12.5).monospacedDigit())
                            .foregroundStyle(ChromeColours.textDim)
                            .lineLimit(1)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
                .opacity(dim ? WorkspacePalette.disabledOpacity : 1)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .accessibilityLabel(slot.empty ? "M\(n), empty" : "M\(n), " + model.label(n))
            .accessibilityValue(model.rowDetail(n, progress: progress))
            .accessibilityHint(sending ? "Stops it" : slot.empty ? "Records or imports into it"
                               : model.held?.reason ?? "Sends it on the air")
            .accessibilityIdentifier("voiceKeyer.message\(n)")
            MoreMenu(model: model, n: n, enabled: enabled)
        }
        .padding(.leading, 12)
        .padding(.trailing, 8)
        .frame(minHeight: 52)
        .background(sending ? ChromeColours.revoke : Color.clear)
        .overlay(alignment: .leading) {
            if target {
                Rectangle().fill(ChromeColours.txRed).frame(width: 3)
            }
        }
        .overlay(alignment: .bottomLeading) {
            if sending, let progress {
                GeometryReader { proxy in
                    Rectangle().fill(ChromeColours.txRed)
                        .frame(width: proxy.size.width * progress.fraction, height: 3)
                        .frame(maxHeight: .infinity, alignment: .bottom)
                }
            }
        }
    }
}

/// A message's ···: Send, Listen, Record again, Rename, Import WAV, Save
/// as WAV and Clear; what cannot act now is greyed with why.
private struct MoreMenu: View {
    let model: VoiceKeyerModel
    let n: Int
    let enabled: Bool

    var body: some View {
        let slot = model.slot(n)
        let empty = VoiceKeyerModel.emptyReason(n)
        let onAir = model.client.voiceKeyer?.onAirReason(n)
        return Menu {
            Section("M\(n) · " + (slot.empty ? "empty" : model.label(n) + " · " + VoiceKeyerModel.lengthText(slot.lengthMs))) {
                item("Send on the air", reason: slot.empty ? empty : model.held?.reason) {
                    Task { await model.send(n) }
                }
                item("Listen", detail: VoiceKeyerModel.listenDetail, reason: slot.empty ? empty : nil) {
                    Task { await model.listen(n) }
                }
                item(slot.empty ? "Record into M\(n)" : "Record again", reason: onAir) {
                    model.recordAgain(n)
                }
                item("Rename…", reason: slot.empty ? empty : nil) {
                    model.startRename(n)
                }
                item(slot.empty ? "Import a WAV into M\(n)…" : "Import a WAV…",
                     detail: slot.empty ? nil : "Replaces what M\(n) holds", reason: onAir) {
                    model.askImport(n)
                }
                item("Save as WAV…", reason: slot.empty ? empty : nil) {
                    Task { await model.saveAs(n) }
                }
                if let why = slot.empty ? empty : onAir {
                    Button(role: .destructive) {
                    } label: {
                        Text("Clear…")
                        Text(why)
                    }
                    .disabled(true)
                } else {
                    Button("Clear…", role: .destructive) {
                        model.askClear(n)
                    }
                }
            }
        } label: {
            Text("···")
                .font(.system(size: 16, weight: .bold))
                .kerning(1)
                .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
                .frame(width: 34, height: 34)
                .background(ChromeColours.button, in: Circle())
                .overlay(Circle().strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
                .contentShape(Circle())
        }
        .disabled(!enabled)
        .accessibilityLabel("More for M\(n)")
        .accessibilityIdentifier("voiceKeyer.more\(n)")
    }

    @ViewBuilder
    private func item(_ title: String, detail: String? = nil, reason: String?,
                      perform: @escaping () -> Void) -> some View {
        Button(action: perform) {
            Text(title)
            if let line = reason ?? detail {
                Text(line)
            }
        }
        .disabled(reason != nil)
    }
}

/// The wait between repeats: − 5 s +.
private struct DelayStepper: View {
    let model: VoiceKeyerModel
    let enabled: Bool

    var body: some View {
        HStack(spacing: 0) {
            step("minus", label: "Shorter wait", by: -1)
            Text("\(model.repeatDelaySec) s")
                .font(.system(size: 15).monospacedDigit())
                .foregroundStyle(ChromeColours.text)
                .frame(minWidth: 46)
                .accessibilityIdentifier("voiceKeyer.delay")
            step("plus", label: "Longer wait", by: 1)
        }
        .workspaceInset()
    }

    private func step(_ symbol: String, label: String, by: Int) -> some View {
        Button {
            Task { await model.setDelay(model.repeatDelaySec + by) }
        } label: {
            Image(systemName: symbol)
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
                .frame(width: 40, height: 34)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityLabel(label)
    }
}

/// The message going out, at the foot of the page, with Stop.
private struct AirFoot: View {
    let model: VoiceKeyerModel
    let progress: VoiceKeyerModel.Progress

    var body: some View {
        let text = model.airText(progress)
        HStack(spacing: 10) {
            VStack(alignment: .leading, spacing: 3) {
                Text(text.title)
                    .font(.system(size: 14, weight: .bold))
                    .foregroundStyle(ChromeColours.textBright)
                    .lineLimit(1)
                Text(text.detail)
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundStyle(ChromeColours.revokeText)
                    .accessibilityIdentifier("voiceKeyer.airDetail")
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            Button("Stop") {
                Task { await model.stop() }
            }
            .font(.system(size: 16, weight: .bold))
            .buttonStyle(SetupButtonStyle(.keyed))
            .accessibilityIdentifier("voiceKeyer.airStop")
        }
        .padding(.leading, 14)
        .padding(.trailing, 10)
        .frame(height: 64)
        .background(ChromeColours.revoke, in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius)
            .strokeBorder(ChromeColours.txRed, lineWidth: 1))
        .padding(.horizontal, 8)
        .padding(.bottom, 6)
    }
}
