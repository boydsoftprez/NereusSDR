// NereusSDR for iOS: Tools > Recordings: the Core's recordings to filter, search, sort, play, rename, share and delete
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 17, the phone's Recordings page.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: Task 17 fixes: dark whatever the phone's setting, an empty
//   list with no count or totals under a reason, Delete in red.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusBand
import NereusMirror
import NereusModels
import Observation
import SwiftUI
import UIKit

/// Tools > Recordings (design 2026-11-15-recording-voice-keyer-dsr
/// sections 6.4 and 6.8): every recording the Core keeps, with the
/// desktop's filter (All, Contact, Whole pan, Sound), its search on name
/// and call, and a sort. A tap plays a recording on this phone's own
/// speaker; each row's round ··· button holds Play, Rename, Save sound
/// only, Share a copy and Delete; a swipe left shows Share and Delete; and
/// Select picks several to share or delete. Delete always asks first and
/// says it cannot be undone. Sharing copies the file from the Core to this
/// phone, then opens the share sheet; a big whole-pan copy says how big it
/// is first. Not connected, or a Core too old for recordings, leaves it all
/// in place and greyed with the reason (V9).
@MainActor
@Observable
final class RecordingsListModel {
    /// The desktop window's filter.
    enum Filter: Int64, CaseIterable, Sendable {
        case all, contact, wholePan, sound

        var title: String {
            switch self {
            case .all: return "All"
            case .contact: return "Contact"
            case .wholePan: return "Whole pan"
            case .sound: return "Sound"
            }
        }

        func admits(_ kind: RecordingEntry.Kind) -> Bool {
            switch self {
            case .all: return true
            case .contact: return kind == .contact
            case .wholePan: return kind == .wholePan
            case .sound: return kind == .sound
            }
        }
    }

    /// The sort the list is in.
    enum Sort: Int, CaseIterable, Sendable {
        case newest, oldest, frequency, longest, largest

        var title: String {
            switch self {
            case .newest: return "Newest first"
            case .oldest: return "Oldest first"
            case .frequency: return "Frequency"
            case .longest: return "Longest first"
            case .largest: return "Largest first"
            }
        }
    }

    /// One delete waiting for its confirm.
    struct PendingDelete: Equatable {
        let ids: [String]
        let title: String
        let message: String
        let button: String
    }

    /// A copy from the Core under way.
    struct Copying: Equatable {
        let title: String
        let number: Int
        let count: Int
        var received: Int64
        var size: Int64

        var text: String {
            let percent = size > 0 ? Int(received * 100 / size) : 0
            let which = count > 1 ? "Copying \(number) of \(count): " : "Copying "
            return which + title + " (\(percent)%)"
        }
    }

    /// Files copied to this phone, for the share sheet.
    struct SharedFiles: Identifiable {
        let id = UUID()
        let urls: [URL]
    }

    // MARK: Words

    /// The recordings list asks this many at a time (the Core's most).
    nonisolated static let pageCount = 100
    /// This list's own number on the Core's answers.
    nonisolated static let reader: Int64 = 17
    /// A whole-pan copy this big or bigger says so before it starts.
    nonisolated static let bigShareBytes: Int64 = 100_000_000
    /// The desktop's words while the Digital Spectrum Recorder is not there
    /// to open a whole-pan stretch.
    nonisolated static let wholePanReason = "Available when the Digital Spectrum Recorder is built."
    nonisolated static let recorderNote = "The Digital Spectrum Recorder's loop is separate and not counted here."
    nonisolated static let searchPrompt = "Search call or name"
    nonisolated static let copyFailedText = "The copy did not finish. Try again."

    // MARK: State

    let client: RecordingsClient
    private(set) var entries: [RecordingEntry] = []
    var filter: Filter = .all
    var search = ""
    var sort: Sort = .newest
    private(set) var selecting = false
    private(set) var selected: Set<String> = []
    /// The delete the confirm is asking about.
    var pendingDelete: PendingDelete?
    /// The recording being renamed, and the name typed so far.
    var renaming: RecordingEntry?
    var renameText = ""
    /// A big whole-pan copy waiting for its go-ahead, and the words that say so.
    var pendingShare: [RecordingEntry]?
    private(set) var copying: Copying?
    /// The files the share sheet offers.
    var shared: SharedFiles?
    /// The Core's words for something it refused, or why a tap did nothing.
    private(set) var note: String?

    @ObservationIgnored private var listedRevision: Int64?
    @ObservationIgnored private let folder: URL

    init(client: RecordingsClient,
         folder: URL = FileManager.default.temporaryDirectory.appendingPathComponent("Recordings", isDirectory: true)) {
        self.client = client
        self.folder = folder
    }

    /// Why nothing here can act now, or nil.
    var reason: String? {
        if client.state == .offline {
            return RecordingFlagModel.notConnectedReason
        }
        return client.unavailableReason(needs: RecordingsClient.recordingsVersion)
    }

    /// What the list redraws on: the session and the Core's list.
    var listKey: String {
        "\(client.state)-\(client.index?.revision ?? -1)"
    }

    /// The rows shown: the filter, the search and the sort.
    var visible: [RecordingEntry] {
        let query = search.trimmingCharacters(in: .whitespacesAndNewlines)
        let shown = entries.filter { entry in
            filter.admits(entry.kind)
                && (query.isEmpty || entry.name.localizedCaseInsensitiveContains(query)
                    || entry.call.localizedCaseInsensitiveContains(query))
        }
        // Ties keep the Core's order, newest first.
        let indexed = Array(shown.enumerated())
        return indexed.sorted { a, b in
            let order = Self.compare(a.element, b.element, by: sort)
            return order == 0 ? a.offset < b.offset : order < 0
        }.map(\.element)
    }

    /// The recording this phone plays now, if any.
    var playingId: String? {
        let playback = client.playback
        return playback.playing && !playback.id.isEmpty ? playback.id : nil
    }

    /// The bar at the foot of the page while this phone plays.
    var bar: RecordingFlagModel.Bar? {
        guard let id = playingId else {
            return nil
        }
        let playback = client.playback
        let title = entries.first { $0.id == id }.map(Self.title(of:)) ?? "Recording"
        return RecordingFlagModel.Bar(title: title, positionMs: playback.positionMs, lengthMs: playback.lengthMs,
                                      onAir: playback.onAir)
    }

    // MARK: The list

    /// Asks for the whole list again when the Core's has changed since the last ask.
    func refresh() async {
        guard reason == nil, let revision = client.index?.revision, revision != listedRevision else {
            return
        }
        listedRevision = revision
        var all: [RecordingEntry] = []
        do {
            var offset = 0
            while true {
                let page = try await client.list(offset: offset, count: Self.pageCount, reader: Self.reader)
                all += page.entries
                offset = page.offset + page.entries.count
                if page.entries.isEmpty || offset >= page.total {
                    break
                }
            }
        } catch {
            listedRevision = nil
            show(error)
            return
        }
        received(all)
    }

    /// The Core's whole list, newest first.
    func received(_ all: [RecordingEntry]) {
        entries = all
        let ids = Set(all.map(\.id))
        selected = selected.intersection(ids)
    }

    // MARK: Taps

    /// A row's tap: picks it while selecting; otherwise plays it on this
    /// phone's speaker, or stops it while it plays.
    func tap(_ entry: RecordingEntry) async {
        if let reason {
            note = reason
            return
        }
        if selecting {
            toggle(entry.id)
            return
        }
        if playingId == entry.id {
            await stop()
            return
        }
        await play(entry)
    }

    func play(_ entry: RecordingEntry) async {
        if let reason {
            note = reason
            return
        }
        guard entry.kind != .wholePan else {
            note = Self.wholePanReason
            return
        }
        note = nil
        await run { try await client.play(id: entry.id) }
    }

    func stop() async {
        await run { try await client.stopPlay() }
    }

    /// Select, or Done.
    func toggleSelecting() {
        selecting.toggle()
        selected = []
    }

    func toggle(_ id: String) {
        if selected.contains(id) {
            selected.remove(id)
        } else {
            selected.insert(id)
        }
    }

    /// The chosen rows in the list's order.
    var chosen: [RecordingEntry] {
        visible.filter { selected.contains($0.id) } + entries.filter { selected.contains($0.id) && !visibleIds.contains($0.id) }
    }

    private var visibleIds: Set<String> { Set(visible.map(\.id)) }

    // MARK: Delete

    /// Asks before deleting: nothing goes to the Core until the confirm.
    func askDelete(_ ids: [String]) {
        if let reason {
            note = reason
            return
        }
        let doomed = entries.filter { ids.contains($0.id) }
        guard !doomed.isEmpty else {
            return
        }
        let bytes = doomed.reduce(Int64(0)) { $0 + $1.sizeBytes }
        let one = doomed.count == 1
        let title = one ? "Delete this recording from the Core?" : "Delete \(doomed.count) recordings from the Core?"
        let message = "It is removed for every computer and phone. This frees \(Self.sizeText(bytes)) and cannot be "
            + "undone. Copies already saved on a phone or computer stay."
        pendingDelete = PendingDelete(ids: doomed.map(\.id), title: title, message: message,
                                      button: one ? "Delete Recording" : "Delete \(doomed.count) Recordings")
    }

    /// The confirm's Delete.
    func confirmDelete() async {
        guard let pending = pendingDelete else {
            return
        }
        pendingDelete = nil
        let ok = await run { try await client.delete(ids: pending.ids) }
        guard ok else {
            return
        }
        entries.removeAll { pending.ids.contains($0.id) }
        selected.subtract(pending.ids)
        if selecting && entries.isEmpty {
            selecting = false
        }
    }

    // MARK: Rename

    func startRename(_ entry: RecordingEntry) {
        if let reason {
            note = reason
            return
        }
        renaming = entry
        renameText = entry.name
    }

    /// The rename's Save: the new name goes to the Core, which every device then shows.
    func commitRename() async {
        guard let entry = renaming else {
            return
        }
        renaming = nil
        let name = renameText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !name.isEmpty, name != entry.name else {
            return
        }
        let ok = await run { try await client.rename(id: entry.id, name: name) }
        if ok, let index = entries.firstIndex(where: { $0.id == entry.id }) {
            entries[index] = Self.renamed(entry, to: name)
        }
    }

    // MARK: Share

    /// Share a copy: a big whole-pan stretch says how big it is first.
    func share(_ chosen: [RecordingEntry]) async {
        if let reason {
            note = reason
            return
        }
        guard !chosen.isEmpty, copying == nil else {
            return
        }
        if chosen.contains(where: { $0.kind == .wholePan && $0.sizeBytes >= Self.bigShareBytes }) {
            pendingShare = chosen
            return
        }
        await copy(chosen)
    }

    /// The big copy's warning.
    static func shareWarning(_ chosen: [RecordingEntry]) -> String {
        let bytes = chosen.reduce(Int64(0)) { $0 + $1.sizeBytes }
        let first = chosen.count == 1 && chosen[0].kind == .wholePan
            ? "This is a whole-pan stretch of \(sizeText(bytes))."
            : "These include a whole-pan stretch, \(sizeText(bytes)) in all."
        return first + " It is copied from the Core to this phone first, which can take a while on mobile data."
    }

    /// The warning's go-ahead.
    func confirmShare() async {
        guard let chosen = pendingShare else {
            return
        }
        pendingShare = nil
        await copy(chosen)
    }

    /// Copies each file from the Core, then opens the share sheet.
    func copy(_ chosen: [RecordingEntry]) async {
        note = nil
        var urls: [URL] = []
        for (index, entry) in chosen.enumerated() {
            copying = Copying(title: Self.title(of: entry), number: index + 1, count: chosen.count, received: 0,
                              size: entry.sizeBytes)
            let directory = folder.appendingPathComponent(entry.id, isDirectory: true)
            let url = directory.appendingPathComponent(Self.fileName(of: entry))
            do {
                try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
                try await client.download(id: entry.id, to: url) { [weak self] received, size in
                    self?.copying?.received = received
                    self?.copying?.size = size
                }
            } catch {
                copying = nil
                show(error, otherwise: Self.copyFailedText)
                removeCopies(urls)
                return
            }
            urls.append(url)
        }
        copying = nil
        shared = SharedFiles(urls: urls)
    }

    /// The share sheet closed: the copies on this phone go.
    func shareClosed(_ files: SharedFiles) {
        removeCopies(files.urls)
    }

    private func removeCopies(_ urls: [URL]) {
        for url in urls {
            try? FileManager.default.removeItem(at: url.deletingLastPathComponent())
        }
    }

    // MARK: Inside

    /// Runs one call; false, with the Core's words shown, when it says no.
    @discardableResult
    private func run(_ call: () async throws -> Void) async -> Bool {
        do {
            try await call()
            return true
        } catch {
            show(error)
            return false
        }
    }

    private func show(_ error: Error, otherwise: String? = nil) {
        if let error = error as? RecordingsError, let reason = error.reason {
            note = reason
        } else if let otherwise {
            note = otherwise
        }
    }

    // MARK: Words

    /// The desktop's size words: whole GB from 10 GB, one decimal from 1
    /// to 10 GB, whole MB from 10 MB, one decimal below.
    nonisolated static func sizeText(_ bytes: Int64) -> String {
        let mb = Double(max(0, bytes)) / 1.0e6
        if mb >= 10_000 {
            return "\(Int64((mb / 1000).rounded())) GB"
        }
        if mb >= 1000 {
            return String(format: "%.1f GB", mb / 1000)
        }
        if mb >= 10 {
            return "\(Int64(mb.rounded())) MB"
        }
        return String(format: "%.1f MB", mb)
    }

    nonisolated static func kindText(_ kind: RecordingEntry.Kind) -> String {
        switch kind {
        case .contact: return "Contact"
        case .wholePan: return "Whole pan"
        case .sound: return "Sound"
        }
    }

    /// The start as a date; nil when the Core's text is not ISO 8601.
    nonisolated static func date(_ text: String) -> Date? {
        let fractional = ISO8601DateFormatter()
        fractional.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        if let date = fractional.date(from: text) {
            return date
        }
        return ISO8601DateFormatter().date(from: text)
    }

    private nonisolated static func utc(_ format: String) -> DateFormatter {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(identifier: "UTC")
        formatter.dateFormat = format
        return formatter
    }

    /// "16 Nov 14:32", in UTC, as the desktop's When column.
    nonisolated static func whenText(_ entry: RecordingEntry) -> String {
        date(entry.startUtc).map { utc("d MMM HH:mm").string(from: $0) } ?? ""
    }

    /// The name the Core gives a recording it made (design section 6.4):
    /// `2026-11-16 1432Z 7.236.400 LSB K4ABC`, the call only when known.
    nonisolated static func madeName(_ entry: RecordingEntry) -> String {
        var parts: [String] = []
        if let start = date(entry.startUtc) {
            parts.append(utc("yyyy-MM-dd HHmm").string(from: start) + "Z")
        }
        parts.append(BandSlice.frequencyText(hz: Double(entry.frequencyHz)))
        let mode = entry.mode.trimmingCharacters(in: .whitespaces)
        if !mode.isEmpty {
            parts.append(mode)
        }
        let call = entry.call.trimmingCharacters(in: .whitespaces)
        if !call.isEmpty {
            parts.append(call)
        }
        return parts.joined(separator: " ")
    }

    /// A row's title: the name the operator gave it, or, for a name the
    /// Core made, its frequency, mode and call (the desktop's way).
    nonisolated static func title(of entry: RecordingEntry) -> String {
        entry.name == madeName(entry) || entry.name.isEmpty ? RecordingFlagModel.title(of: entry) : entry.name
    }

    /// "16 Nov 14:32 · 7.236.400 LSB".
    nonisolated static func whereText(_ entry: RecordingEntry) -> String {
        var text = BandSlice.frequencyText(hz: Double(entry.frequencyHz))
        if !entry.mode.isEmpty {
            text += " " + entry.mode
        }
        let when = whenText(entry)
        return when.isEmpty ? text : when + " · " + text
    }

    /// "Contact · 3:25 · 1.4 MB", with the call when the title does not show it.
    nonisolated static func detailText(_ entry: RecordingEntry) -> String {
        var parts = [kindText(entry.kind), RecordingFlagModel.lengthText(entry.lengthMs), sizeText(entry.sizeBytes)]
        if !entry.call.isEmpty, title(of: entry) == entry.name {
            parts.append(entry.call)
        }
        return parts.joined(separator: " · ")
    }

    /// "8 recordings".
    nonisolated static func countText(_ count: Int) -> String {
        count == 1 ? "1 recording" : "\(count) recordings"
    }

    /// The Core's totals: "1.4 GB saved in 12 recordings. 812 GB free on
    /// Samsung T7." and that the recorder's loop is not counted; nil while
    /// the Core's totals cannot be read, where the reason says why.
    var summary: String? {
        guard reason == nil, let index = client.index else {
            return nil
        }
        let saved = Self.sizeText(index.usedBytes) + " saved in "
            + (index.count == 1 ? "one recording" : "\(index.count) recordings") + "."
        var free = Self.sizeText(index.freeBytes) + " free"
        // The chosen disk's own free space and name, when the Core lists it.
        if let settings = client.settings,
           let disk = settings.disks.first(where: { settings.disk.isEmpty ? $0.own : $0.root == settings.disk }) {
            free = Self.sizeText(disk.free) + " free" + (disk.name.isEmpty ? "" : " on " + disk.name)
        }
        return saved + " " + free + ". " + Self.recorderNote
    }

    /// The file's name on this phone: the Core's file name, or the shown name.
    nonisolated static func fileName(of entry: RecordingEntry) -> String {
        let core = (entry.fileName as NSString).lastPathComponent
        if !core.isEmpty {
            return core
        }
        return entry.name.isEmpty ? entry.id : entry.name
    }

    nonisolated static func compare(_ a: RecordingEntry, _ b: RecordingEntry, by sort: Sort) -> Int {
        func three<T: Comparable>(_ x: T, _ y: T) -> Int { x < y ? -1 : (y < x ? 1 : 0) }
        switch sort {
        case .newest:
            return three(date(b.startUtc) ?? .distantPast, date(a.startUtc) ?? .distantPast)
        case .oldest:
            return three(date(a.startUtc) ?? .distantPast, date(b.startUtc) ?? .distantPast)
        case .frequency:
            return three(a.frequencyHz, b.frequencyHz)
        case .longest:
            return three(b.lengthMs, a.lengthMs)
        case .largest:
            return three(b.sizeBytes, a.sizeBytes)
        }
    }

    nonisolated static func renamed(_ entry: RecordingEntry, to name: String) -> RecordingEntry {
        RecordingEntry(id: entry.id, kind: entry.kind, slice: entry.slice, startUtc: entry.startUtc,
                       lengthMs: entry.lengthMs, frequencyHz: entry.frequencyHz, mode: entry.mode, call: entry.call,
                       name: name, fileName: entry.fileName, sizeBytes: entry.sizeBytes)
    }
}

/// The page itself, under the Tools tab's bar.
struct RecordingsListPage: View {
    @Bindable var model: RecordingsListModel

    var body: some View {
        ZStack(alignment: .bottom) {
            ScrollView {
                content
                    .padding(12)
                    .padding(.bottom, footHeight)
            }
            VStack(spacing: 0) {
                if let bar = model.bar {
                    PlayingFoot(bar: bar) {
                        Task { await model.stop() }
                    }
                }
                if model.selecting {
                    SelectFoot(model: model)
                }
            }
        }
        .task(id: model.listKey) { await model.refresh() }
        .confirmationDialog(model.pendingDelete?.title ?? "", isPresented: Binding(
            get: { model.pendingDelete != nil },
            set: { if !$0 { model.pendingDelete = nil } }), titleVisibility: .visible) {
            Button(model.pendingDelete?.button ?? "Delete", role: .destructive) {
                Task { await model.confirmDelete() }
            }
            .accessibilityIdentifier("recordings.confirmDelete")
            Button("Cancel", role: .cancel) {
                model.pendingDelete = nil
            }
        } message: {
            Text(model.pendingDelete?.message ?? "")
        }
        .confirmationDialog("Share a copy", isPresented: Binding(
            get: { model.pendingShare != nil },
            set: { if !$0 { model.pendingShare = nil } }), titleVisibility: .visible) {
            Button("Copy and share") {
                Task { await model.confirmShare() }
            }
            .accessibilityIdentifier("recordings.confirmShare")
            Button("Cancel", role: .cancel) {
                model.pendingShare = nil
            }
        } message: {
            Text(RecordingsListModel.shareWarning(model.pendingShare ?? []))
        }
        .alert("Rename", isPresented: Binding(
            get: { model.renaming != nil },
            set: { if !$0 { model.renaming = nil } })) {
            TextField("Name", text: $model.renameText)
                .accessibilityIdentifier("recordings.renameField")
            Button("Save") {
                Task { await model.commitRename() }
            }
            Button("Cancel", role: .cancel) {
                model.renaming = nil
            }
        } message: {
            Text("A name for this recording. Every phone and computer shows it.")
        }
        .sheet(item: $model.shared, onDismiss: nil) { files in
            ShareSheet(urls: files.urls)
                .onDisappear { model.shareClosed(files) }
        }
        // The app draws dark whatever the phone's own setting (NereusSDRApp),
        // so the search field and the menus keep to the dark workspace.
        .environment(\.colorScheme, .dark)
    }

    private var footHeight: CGFloat {
        (model.bar != nil ? 64 : 0) + (model.selecting ? 52 : 0)
    }

    @ViewBuilder
    private var content: some View {
        let enabled = model.reason == nil
        VStack(alignment: .leading, spacing: 8) {
            if let reason = model.reason {
                ConnectChrome.NoticeBox(tone: .warn, title: nil, text: reason)
                    .accessibilityIdentifier("recordings.reason")
            }
            if let summary = model.summary {
                Text(summary)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("recordings.summary")
            }
            ToolPageParts.Choices(options: RecordingsListModel.Filter.allCases.map { ($0.rawValue, $0.title) },
                                  selected: model.filter.rawValue, enabled: enabled,
                                  identifier: "recordings.filter") { id in
                model.filter = RecordingsListModel.Filter(rawValue: id) ?? .all
            }
            searchField(enabled: enabled)
            HStack {
                // No count while the Core's list cannot be read.
                if enabled {
                    Text(RecordingsListModel.countText(model.visible.count))
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .accessibilityIdentifier("recordings.count")
                }
                Spacer()
                Menu {
                    ForEach(RecordingsListModel.Sort.allCases, id: \.self) { sort in
                        Button {
                            model.sort = sort
                        } label: {
                            if sort == model.sort {
                                Label(sort.title, systemImage: "checkmark")
                            } else {
                                Text(sort.title)
                            }
                        }
                    }
                } label: {
                    HStack(spacing: 3) {
                        Text(model.sort.title)
                        Image(systemName: "chevron.down").font(.system(size: 10, weight: .semibold))
                    }
                    .font(.system(size: 13))
                    .foregroundStyle(enabled ? ChromeColours.accentText : ChromeColours.buttonOffText)
                }
                .disabled(!enabled)
                .accessibilityLabel("Sort: " + model.sort.title)
                .accessibilityIdentifier("recordings.sort")
            }
            .padding(.horizontal, 4)
            if let copying = model.copying {
                ToolPageParts.Reading(title: copying.text, value: "", identifier: "recordings.copying")
            }
            if let note = model.note {
                ToolPageParts.Refusal(text: note, identifier: "recordings.note")
            }
            SpotHubPage.Card {
                // Not connected, or a Core without recordings: an empty list
                // under the reason, never rows kept from before.
                let rows = enabled ? model.visible : []
                if !enabled {
                    Color.clear
                        .frame(maxWidth: .infinity)
                        .frame(height: 64)
                        .accessibilityHidden(true)
                } else if rows.isEmpty {
                    Text(model.entries.isEmpty ? "No recordings on the Core." : "Nothing matches.")
                        .font(.system(size: 13))
                        .foregroundStyle(ChromeColours.textDim)
                        .frame(maxWidth: .infinity)
                        .padding(22)
                        .accessibilityIdentifier("recordings.empty")
                }
                ForEach(Array(rows.enumerated()), id: \.element.id) { index, entry in
                    if index > 0 {
                        SpotHubPage.Line()
                    }
                    RecordingRow(model: model, entry: entry, enabled: enabled)
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
            .opacity(enabled ? 1 : WorkspacePalette.disabledOpacity)
        }
    }

    private func searchField(enabled: Bool) -> some View {
        HStack(spacing: 6) {
            Image(systemName: "magnifyingglass")
                .font(.system(size: 13))
                .foregroundStyle(ChromeColours.textFaint)
            TextField(RecordingsListModel.searchPrompt, text: $model.search)
                .font(.system(size: 15))
                .foregroundStyle(ChromeColours.text)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .submitLabel(.search)
                .accessibilityIdentifier("recordings.search")
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .workspaceInset()
        .disabled(!enabled)
        .opacity(enabled ? 1 : WorkspacePalette.disabledOpacity)
    }

    /// The nav bar's right: the Core chip, and Select or Done.
    struct Trailing: View {
        let model: RecordingsListModel

        var body: some View {
            let enabled = model.reason == nil && (!model.entries.isEmpty || model.selecting)
            HStack(spacing: 10) {
                SetupTagBadge(tag: .core)
                Button(model.selecting ? "Done" : "Select") {
                    model.toggleSelecting()
                }
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(enabled ? ChromeColours.accentText : ChromeColours.buttonOffText)
                .disabled(!enabled)
                .accessibilityIdentifier("recordings.select")
            }
        }
    }
}

/// One recording: its kind's mark, its title and lines, and the round ···
/// button. A tap plays it (or picks it while selecting); a swipe left shows
/// Share and Delete, which the ··· button and VoiceOver's actions also hold.
private struct RecordingRow: View {
    let model: RecordingsListModel
    let entry: RecordingEntry
    let enabled: Bool
    @State private var offset: CGFloat = 0
    @State private var open = false

    static let actionWidth: CGFloat = 74
    static let openDistance: CGFloat = 60

    var body: some View {
        let playing = model.playingId == entry.id
        ZStack(alignment: .trailing) {
            if open || offset < 0 {
                HStack(spacing: 0) {
                    action("Share", look: .glass, identifier: "share") {
                        Task { await model.share([entry]) }
                    }
                    action("Delete", look: .danger, identifier: "delete") {
                        model.askDelete([entry.id])
                    }
                }
            }
            face(playing: playing)
                .background(ConnectChrome.rowFill)
                .offset(x: offset)
                .overlay {
                    if open {
                        // A tap on the shifted row puts it back.
                        Color.clear
                            .contentShape(Rectangle())
                            .padding(.trailing, Self.actionWidth * 2)
                            .onTapGesture { close() }
                            .accessibilityHidden(true)
                    }
                }
        }
        .simultaneousGesture(DragGesture(minimumDistance: 20).onChanged { drag in
            guard enabled, !model.selecting, abs(drag.translation.width) > abs(drag.translation.height) else {
                return
            }
            let base = open ? -Self.actionWidth * 2 : 0
            offset = min(0, max(-Self.actionWidth * 3, base + drag.translation.width))
        }.onEnded { drag in
            guard enabled, !model.selecting else {
                return
            }
            withAnimation(.snappy(duration: 0.2)) {
                if abs(drag.translation.width) > abs(drag.translation.height) {
                    open = (open ? -Self.actionWidth * 2 : 0) + drag.translation.width < -Self.openDistance
                }
                offset = open ? -Self.actionWidth * 2 : 0
            }
        })
        .onChange(of: model.selecting) { _, _ in close() }
        .accessibilityElement(children: .contain)
        .accessibilityAction(named: "Share a copy") {
            Task { await model.share([entry]) }
        }
        .accessibilityAction(named: "Delete") {
            model.askDelete([entry.id])
        }
        .accessibilityIdentifier("recordings.row.\(entry.id)")
    }

    private func face(playing: Bool) -> some View {
        HStack(spacing: 10) {
            if model.selecting {
                let on = model.selected.contains(entry.id)
                Image(systemName: on ? "checkmark.circle.fill" : "circle")
                    .font(.system(size: 22))
                    .foregroundStyle(on ? ChromeColours.accentText : ChromeColours.textFaint)
                    .accessibilityHidden(true)
            }
            KindMark(kind: entry.kind)
            Button {
                close()
                Task { await model.tap(entry) }
            } label: {
                VStack(alignment: .leading, spacing: 2) {
                    Text(RecordingsListModel.title(of: entry))
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(playing ? ChromeColours.accentText : ChromeColours.textBright)
                        .lineLimit(1)
                    Text(RecordingsListModel.whereText(entry))
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .lineLimit(1)
                    Text(RecordingsListModel.detailText(entry))
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .lineLimit(1)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityAddTraits(model.selecting && model.selected.contains(entry.id) ? .isSelected : [])
            .accessibilityHint(model.selecting ? "Picks it" : playing ? "Stops it" : "Plays it on this phone")
            .accessibilityIdentifier("recordings.play.\(entry.id)")
            more
        }
        .padding(.leading, 12)
        .padding(.trailing, 8)
        .padding(.vertical, 9)
    }

    /// The round ··· button: everything a row can do, without a swipe.
    private var more: some View {
        let playing = model.playingId == entry.id
        return Menu {
            Section(RecordingsListModel.kindText(entry.kind) + " · "
                    + RecordingFlagModel.lengthText(entry.lengthMs) + " · "
                    + RecordingsListModel.sizeText(entry.sizeBytes)) {
                if entry.kind == .wholePan {
                    Button {
                    } label: {
                        Text("Open on the pan")
                        Text(RecordingsListModel.wholePanReason)
                    }
                    .disabled(true)
                } else {
                    Button(playing ? "Stop" : "Play") {
                        Task { await model.tap(entry) }
                    }
                }
                Button("Rename…") {
                    model.startRename(entry)
                }
                if entry.kind == .wholePan {
                    Button {
                    } label: {
                        Text("Save sound only…")
                        Text(RecordingsListModel.wholePanReason)
                    }
                    .disabled(true)
                }
                Button("Share a copy…") {
                    Task { await model.share([entry]) }
                }
                Button("Delete…", role: .destructive) {
                    model.askDelete([entry.id])
                }
            }
        } label: {
            Text("···")
                .font(.system(size: 16, weight: .bold))
                .kerning(1)
                .foregroundStyle(enabled && !model.selecting ? ChromeColours.text : ChromeColours.buttonOffText)
                .frame(width: 34, height: 34)
                .background(ChromeColours.button, in: Circle())
                .overlay(Circle().strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
                .contentShape(Circle())
        }
        .disabled(!enabled || model.selecting)
        .accessibilityLabel("More")
        .accessibilityIdentifier("recordings.more.\(entry.id)")
    }

    private func action(_ title: String, look: WorkspaceLook, identifier: String,
                        perform: @escaping () -> Void) -> some View {
        Button {
            close()
            perform()
        } label: {
            Text(title)
                .font(.system(size: 14, weight: .semibold))
                .frame(width: Self.actionWidth)
                .frame(maxHeight: .infinity)
        }
        .buttonStyle(SetupButtonStyle(look))
        .accessibilityIdentifier("recordings.\(identifier).\(entry.id)")
    }

    private func close() {
        guard open || offset != 0 else {
            return
        }
        withAnimation(.snappy(duration: 0.2)) {
            open = false
            offset = 0
        }
    }
}

/// A recording's kind as a small square: red dot for a contact, blue
/// pan for a whole-pan stretch, green wave for sound.
private struct KindMark: View {
    let kind: RecordingEntry.Kind

    var body: some View {
        let (symbol, ground, ink): (String, Color, Color) = {
            switch kind {
            case .contact: return ("record.circle", ChromeColours.revoke, ChromeColours.revokeText)
            case .wholePan: return ("waveform.path.ecg.rectangle", ChromeColours.buttonOnBlue, ChromeColours.accentText)
            case .sound: return ("waveform", ChromeColours.buttonOnGreen, ChromeColours.buttonOnGreenText)
            }
        }()
        Image(systemName: symbol)
            .font(.system(size: 18, weight: .medium))
            .foregroundStyle(ink)
            .frame(width: 36, height: 36)
            .background(ground, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
            .accessibilityLabel(RecordingsListModel.kindText(kind))
    }
}

/// What this phone plays, at the foot of the page, with Stop.
private struct PlayingFoot: View {
    let bar: RecordingFlagModel.Bar
    let stop: () -> Void

    var body: some View {
        HStack(spacing: 10) {
            VStack(alignment: .leading, spacing: 4) {
                Text(bar.title)
                    .font(.system(size: 13.5, weight: .semibold))
                    .foregroundStyle(ChromeColours.textBright)
                    .lineLimit(1)
                GeometryReader { proxy in
                    ZStack(alignment: .leading) {
                        Capsule().fill(ChromeColours.sliderTrack)
                        Capsule().fill(bar.onAir ? ChromeColours.txRed : ChromeColours.accentText)
                            .frame(width: proxy.size.width * bar.fraction)
                    }
                }
                .frame(height: 4)
                Text(bar.detail)
                    .font(.system(size: 11).monospacedDigit())
                    .foregroundStyle(bar.onAir ? ChromeColours.txRed : ChromeColours.textDim)
                    .accessibilityIdentifier("recordings.playingDetail")
            }
            Button("Stop", action: stop)
                .font(.system(size: 15, weight: .semibold))
                .buttonStyle(SetupButtonStyle(bar.onAir ? .danger : .raised))
                .accessibilityIdentifier("recordings.stop")
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(ChromeColours.sheet, in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius)
            .strokeBorder(bar.onAir ? ChromeColours.txRed : ChromeColours.sheetBorder, lineWidth: 1))
        .padding(.horizontal, 8)
        .padding(.bottom, 6)
    }
}

/// Select's bar: Share, how many are picked, and Delete.
private struct SelectFoot: View {
    let model: RecordingsListModel

    var body: some View {
        let count = model.selected.count
        HStack {
            Button("Share") {
                Task { await model.share(model.chosen) }
            }
            .foregroundStyle(count > 0 ? ChromeColours.accentText : ChromeColours.buttonOffText)
            .disabled(count == 0)
            .accessibilityIdentifier("recordings.shareSelected")
            Spacer()
            Text(count == 0 ? "None selected" : "\(count) selected")
                .font(.system(size: 13))
                .foregroundStyle(ChromeColours.textDim)
                .accessibilityIdentifier("recordings.selectedCount")
            Spacer()
            Button(count > 1 ? "Delete \(count)" : "Delete") {
                model.askDelete(model.chosen.map(\.id))
            }
            // The app's standalone red words for a delete (W5), as Setup > Devices has them.
            .foregroundStyle(count > 0 ? ConnectChrome.badText : ChromeColours.buttonOffText)
            .disabled(count == 0)
            .accessibilityIdentifier("recordings.deleteSelected")
        }
        .font(.system(size: 17))
        .padding(.horizontal, 18)
        .frame(height: 48)
        .background(ChromeColours.bar)
        .overlay(alignment: .top) {
            Rectangle().fill(ChromeColours.barBorder).frame(height: 1)
        }
    }
}

/// The system share sheet for files copied to this phone.
struct ShareSheet: UIViewControllerRepresentable {
    let urls: [URL]

    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: urls, applicationActivities: nil)
    }

    func updateUIViewController(_ controller: UIActivityViewController, context: Context) {}
}
