// NereusSDR for iOS: the iPad's Recordings panel in the applet column: the latest recordings with play and the row menu
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 19, the iPad's Recordings panel.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: Recording plan fix round: a whole-pan row's button opens it on
//   the pan. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusBand
import NereusMirror
import NereusModels
import Observation
import SwiftUI

/// What the iPad's Recordings panel shows and does (design
/// 2026-11-15-recording-voice-keyer-dsr sections 3, 6.4 and 6.8; the
/// desktop's RecordingsPanel): each slice recording now in a red row at the
/// top, then the Core's latest recordings, six rows in all, each with a
/// play button and the Recordings page's row menu, and how many the Core
/// keeps. A not connected or older Core leaves the panel in place with
/// the reason (V9).
@MainActor
@Observable
final class RecordingsAppletModel {
    /// The desktop panel's six rows.
    nonisolated static let rowCount = 6
    /// The panel's own number on the Core's list answers.
    nonisolated static let reader: Int64 = 18
    nonisolated static let emptyText = "Nothing recorded. Record on a slice's flag."

    /// How a recording's row looks.
    enum Look: Equatable {
        case idle
        /// Plays on this iPad's speaker.
        case playing(positionMs: Int64)
        /// This iPad's key sends it on the air.
        case onAir(positionMs: Int64)
    }

    /// One row of the panel.
    enum Row: Equatable, Identifiable {
        /// A slice recording now: red, with a stop square.
        case recording(slice: Int, elapsedMs: Int64)
        case entry(RecordingEntry, look: Look)

        var id: String {
            switch self {
            case .recording(let slice, _): return "slice:\(slice)"
            case .entry(let entry, _): return entry.id
            }
        }
    }

    let client: RecordingsClient
    /// The rows' menu, their plays, the confirms and the share sheet: the
    /// Recordings page's own, over the panel's six.
    let actions: RecordingsListModel
    /// Why a stop the Core refused did nothing.
    private(set) var stopNote: String?
    @ObservationIgnored private var listedRevision: Int64?

    init(client: RecordingsClient) {
        self.client = client
        actions = RecordingsListModel(client: client)
    }

    /// Why nothing here can act now, or nil.
    var reason: String? { actions.reason }

    /// The note under the rows: the Core's words for a refusal, or why a tap did nothing.
    var note: String? { stopNote ?? actions.note }

    /// The recording slices first, then the latest, six in all.
    var rows: [Row] {
        guard reason == nil else {
            return []
        }
        var rows: [Row] = []
        for slice in client.slices where slice.state == .recording && rows.count < Self.rowCount {
            rows.append(.recording(slice: slice.slice, elapsedMs: max(0, slice.elapsedMs)))
        }
        let playback = client.playback
        for entry in actions.entries {
            guard rows.count < Self.rowCount else {
                break
            }
            var look = Look.idle
            if playback.playing, playback.id == entry.id {
                look = playback.onAir ? .onAir(positionMs: playback.positionMs)
                    : .playing(positionMs: playback.positionMs)
            }
            rows.append(.entry(entry, look: look))
        }
        return rows
    }

    /// A recording row's second line: "what you hear and your voice" while
    /// the Record button keeps your voice, "what you hear" otherwise.
    var recordingDetail: String {
        client.settings?.keepMyVoice ?? true ? "what you hear and your voice" : "what you hear"
    }

    /// "5 on the Core, 1.4 GB", the desktop panel's words; nil while the reason shows.
    var footer: String? {
        guard reason == nil, let index = client.index else {
            return nil
        }
        return "\(index.count) on the Core, " + RecordingsListModel.sizeText(index.usedBytes)
    }

    // MARK: Keeping up

    /// Asks for the latest six when the Core's list has changed since the last ask.
    func refresh() async {
        guard reason == nil else {
            listedRevision = nil
            return
        }
        guard let revision = client.index?.revision, revision != listedRevision else {
            return
        }
        listedRevision = revision
        do {
            let page = try await client.list(offset: 0, count: Self.rowCount, reader: Self.reader)
            actions.received(page.entries)
        } catch {
            listedRevision = nil
        }
    }

    // MARK: Taps

    /// A row's play button: plays it on this iPad's speaker, or stops it.
    func tapPlay(_ entry: RecordingEntry) async {
        stopNote = nil
        await actions.tap(entry)
    }

    /// A recording row's stop square: the Core keeps what it has.
    func stopRecording(slice: Int) async {
        stopNote = nil
        do {
            try await client.stop(slice: slice)
        } catch {
            if let error = error as? RecordingsError, let reason = error.reason {
                stopNote = reason
            }
        }
    }

    // MARK: Words

    /// "today 1432Z", "yesterday 0915Z", "3 days ago 2210Z", in UTC as the desktop panel.
    nonisolated static func dayText(_ entry: RecordingEntry, now: Date) -> String {
        guard let start = RecordingsListModel.date(entry.startUtc) else {
            return ""
        }
        var calendar = Calendar(identifier: .gregorian)
        calendar.timeZone = TimeZone(identifier: "UTC") ?? calendar.timeZone
        let days = calendar.dateComponents([.day], from: calendar.startOfDay(for: start),
                                           to: calendar.startOfDay(for: now)).day ?? 0
        let day = days <= 0 ? "today" : days == 1 ? "yesterday" : "\(days) days ago"
        let parts = calendar.dateComponents([.hour, .minute], from: start)
        let hhmm = String(format: "%02d%02d", parts.hour ?? 0, parts.minute ?? 0)
        return day + " " + hhmm + "Z"
    }

    /// "3:25", or "0:12/3:25" while it plays.
    nonisolated static func lengthText(_ entry: RecordingEntry, look: Look) -> String {
        let length = RecordingFlagModel.lengthText(entry.lengthMs)
        switch look {
        case .idle:
            return length
        case .playing(let position), .onAir(let position):
            return RecordingFlagModel.lengthText(position) + "/" + length
        }
    }
}

/// What the column needs for its Recordings panel.
struct RecordingsAppletConfig {
    let model: RecordingsAppletModel
    /// This device's settings: whether the panel shows.
    let settings: PhoneSettings
    /// Tools > Recordings.
    let openAll: () -> Void
    /// Setup > Recording settings.
    let openSettings: () -> Void
}

/// The Recordings panel in the iPad's column (design section 3, Recordings
/// panel; 6.8; the iPad mockup): a title bar like the S-Meter's with its
/// menu, the rows, and a foot with how many the Core keeps and All
/// recordings. Shown only while this iPad's Setup > Recording settings has
/// it on; off by default. In the column's one scroll, so it pushes the
/// applets under it down.
struct RecordingsApplet: View {
    let model: RecordingsAppletModel
    @ObservedObject var settings: PhoneSettings
    let openAll: () -> Void
    let openSettings: () -> Void

    /// The panel's place in the column's scroll.
    static let id = "recordingsApplet"
    static let rowHeight: CGFloat = 42
    static let playSize: CGFloat = 28

    init(_ config: RecordingsAppletConfig) {
        model = config.model
        settings = config.settings
        openAll = config.openAll
        openSettings = config.openSettings
    }

    var body: some View {
        if settings.recordingsAppletShown {
            panel
        }
    }

    private var panel: some View {
        VStack(alignment: .leading, spacing: 0) {
            title
            if let reason = model.reason {
                Text(reason)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 10)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .accessibilityIdentifier("recordingsApplet.reason")
            } else {
                rows
            }
            if let note = model.note {
                Text(note)
                    .font(.system(size: 11.5))
                    .foregroundStyle(ChromeColours.txRed)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.horizontal, 10)
                    .padding(.top, 4)
                    .accessibilityIdentifier("recordingsApplet.note")
            }
            foot
        }
        .background(ChromeColours.panel)
        .task(id: model.actions.listKey) { await model.refresh() }
        .modifier(RecordingsListDialogs(model: model.actions))
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier(Self.id)
    }

    private var title: some View {
        HStack(spacing: 0) {
            Text("Recordings")
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(ChromeColours.icon)
            Spacer(minLength: 0)
            Menu {
                Button("All recordings…", action: openAll)
                Button("Recording settings…", action: openSettings)
                Divider()
                Button("Hide this panel") {
                    settings.recordingsAppletShown = false
                }
            } label: {
                Image(systemName: "line.3.horizontal")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(ChromeColours.icon)
                    .frame(width: 44, height: AnalogSMeter.titleHeight)
                    .contentShape(Rectangle())
            }
            .accessibilityLabel("Recordings menu")
            .accessibilityIdentifier("recordingsApplet.menu")
        }
        .padding(.leading, 8)
        .frame(maxWidth: .infinity, minHeight: AnalogSMeter.titleHeight, maxHeight: AnalogSMeter.titleHeight)
        .background(LinearGradient(stops: [.init(color: ChromeColours.titleTop, location: 0),
                                           .init(color: ChromeColours.titleMiddle, location: 0.5),
                                           .init(color: ChromeColours.titleBottom, location: 1)],
                                   startPoint: .top, endPoint: .bottom))
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.titleBorder).frame(height: 1)
        }
    }

    @ViewBuilder
    private var rows: some View {
        let rows = model.rows
        if rows.isEmpty {
            Text(RecordingsAppletModel.emptyText)
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.textDim)
                .padding(.horizontal, 10)
                .padding(.vertical, 10)
                .frame(maxWidth: .infinity, alignment: .leading)
                .accessibilityIdentifier("recordingsApplet.empty")
        } else {
            TimelineView(.everyMinute) { context in
                VStack(spacing: 0) {
                    ForEach(Array(rows.enumerated()), id: \.element.id) { index, row in
                        RecordingsAppletRow(model: model, row: row, odd: index % 2 == 0, now: context.date)
                    }
                }
                .padding(.horizontal, 6)
                .padding(.top, 4)
            }
        }
    }

    private var foot: some View {
        HStack(spacing: 6) {
            if let footer = model.footer {
                Text(footer)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .lineLimit(1)
                    .minimumScaleFactor(0.8)
                    .accessibilityIdentifier("recordingsApplet.footer")
            }
            Spacer(minLength: 0)
            Button(action: openAll) {
                Text("All recordings \u{203A}")
                    .font(.system(size: 12, weight: .medium))
                    .foregroundStyle(ChromeColours.accentText)
                    .frame(minHeight: 30)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityIdentifier("recordingsApplet.all")
        }
        .padding(.horizontal, 10)
        .padding(.top, 2)
        .padding(.bottom, 4)
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.panelEdge).frame(height: 1)
        }
    }
}

/// One row: the play button (a stop square while it plays or records),
/// the title over the kind and when, the length, and the ··· menu.
private struct RecordingsAppletRow: View {
    let model: RecordingsAppletModel
    let row: RecordingsAppletModel.Row
    let odd: Bool
    let now: Date

    private static let oddGround = WorkspacePalette.rgb(0x0A0A0C)
    private static let playingGround = WorkspacePalette.rgb(0x12202F)
    private static let airGround = WorkspacePalette.rgb(0x2A0F0D)
    private static let recordingGround = WorkspacePalette.rgb(0x1A0D0E)
    private static let buttonGround = WorkspacePalette.rgb(0x222226)
    private static let buttonEdge = WorkspacePalette.rgb(0x333338)
    private static let buttonInk = WorkspacePalette.rgb(0xDDDDDD)
    private static let playingButton = WorkspacePalette.rgb(0x1E3D5F)
    private static let playingEdge = WorkspacePalette.rgb(0x4A7BA8)
    private static let redButton = WorkspacePalette.rgb(0x7A2C2E)
    private static let airEdge = WorkspacePalette.rgb(0xFF453A)
    private static let recordingEdge = WorkspacePalette.rgb(0xC25A5C)
    private static let recordingInk = WorkspacePalette.rgb(0xFFD9D9)
    private static let titleInk = WorkspacePalette.rgb(0xE8E8EC)
    private static let dimInk = WorkspacePalette.rgb(0x7E7E85)
    private static let panInk = WorkspacePalette.rgb(0x5E5E66)

    var body: some View {
        switch row {
        case .recording(let slice, let elapsedMs):
            recording(slice: slice, elapsedMs: elapsedMs)
        case .entry(let entry, let look):
            recordingEntry(entry, look: look)
        }
    }

    private func recording(slice: Int, elapsedMs: Int64) -> some View {
        let name = RecordingFlagModel.sliceName(slice)
        return HStack(spacing: 8) {
            Button {
                Task { await model.stopRecording(slice: slice) }
            } label: {
                face(symbol: "stop.fill", ground: Self.redButton, edge: Self.recordingEdge, ink: Self.recordingInk)
            }
            .buttonStyle(.plain)
            .disabled(model.reason != nil)
            .accessibilityLabel("Stop recording " + name)
            .accessibilityIdentifier("recordingsApplet.stop.\(slice)")
            VStack(alignment: .leading, spacing: 1) {
                (Text("\u{25CF} ").foregroundStyle(Self.airEdge) + Text("Recording " + name))
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(Self.recordingInk)
                    .lineLimit(1)
                Text(model.recordingDetail)
                    .font(.system(size: 11))
                    .foregroundStyle(Self.dimInk)
                    .lineLimit(1)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            length(RecordingFlagModel.lengthText(elapsedMs))
            // The ··· button's room, so the lengths line up.
            Color.clear.frame(width: 24, height: 1)
        }
        .modifier(RowFrame(ground: Self.recordingGround))
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("recordingsApplet.row.slice:\(slice)")
    }

    private func recordingEntry(_ entry: RecordingEntry, look: RecordingsAppletModel.Look) -> some View {
        let enabled = model.reason == nil
        let ground: Color
        let button: AnyView
        switch look {
        case .idle:
            ground = odd ? Self.oddGround : .clear
            button = entry.kind == .wholePan
                ? AnyView(face(symbol: "waveform.path.ecg", ground: Self.buttonGround, edge: Self.buttonEdge,
                               ink: Self.panInk))
                : AnyView(face(symbol: "play.fill", ground: Self.buttonGround, edge: Self.buttonEdge,
                               ink: Self.buttonInk))
        case .playing:
            ground = Self.playingGround
            button = AnyView(face(symbol: "stop.fill", ground: Self.playingButton, edge: Self.playingEdge,
                                  ink: .white))
        case .onAir:
            ground = Self.airGround
            button = AnyView(face(symbol: "stop.fill", ground: Self.redButton, edge: Self.airEdge, ink: .white))
        }
        let playing = look != .idle
        return HStack(spacing: 8) {
            Button {
                Task { await model.tapPlay(entry) }
            } label: {
                button
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .accessibilityLabel(entry.kind == .wholePan ? "Open on the pan" : playing ? "Stop" : "Play")
            .accessibilityHint(entry.kind == .wholePan
                               ? model.actions.stretchReason ?? RecordingsListModel.openStretchHint
                               : playing ? "" : "Plays it on this iPad. Transmit while it plays to send it on the air.")
            .accessibilityIdentifier("recordingsApplet.play.\(entry.id)")
            VStack(alignment: .leading, spacing: 1) {
                Text(RecordingsListModel.title(of: entry))
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(Self.titleInk)
                    .lineLimit(1)
                    .truncationMode(.tail)
                HStack(spacing: 4) {
                    KindTag(kind: entry.kind)
                    Text(RecordingsAppletModel.dayText(entry, now: now))
                        .font(.system(size: 11))
                        .foregroundStyle(Self.dimInk)
                        .lineLimit(1)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            length(RecordingsAppletModel.lengthText(entry, look: look))
            Menu {
                RecordingRowMenuItems(model: model.actions, entry: entry, playing: playing)
            } label: {
                Text("···")
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
                    .frame(width: 24, height: Self.rowHeightForTaps)
                    .contentShape(Rectangle())
            }
            .disabled(!enabled)
            .accessibilityLabel("More")
            .accessibilityIdentifier("recordingsApplet.more.\(entry.id)")
        }
        .modifier(RowFrame(ground: ground))
        .contextMenu {
            if enabled {
                RecordingRowMenuItems(model: model.actions, entry: entry, playing: playing)
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("recordingsApplet.row.\(entry.id)")
    }

    private static let rowHeightForTaps: CGFloat = 40

    private func face(symbol: String, ground: Color, edge: Color, ink: Color) -> some View {
        Image(systemName: symbol)
            .font(.system(size: 11, weight: .semibold))
            .foregroundStyle(ink)
            .frame(width: RecordingsApplet.playSize, height: RecordingsApplet.playSize)
            .background(ground, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
            .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).strokeBorder(edge, lineWidth: 1))
            .frame(width: RecordingsApplet.rowHeight, height: RecordingsApplet.rowHeight)
            .contentShape(Rectangle())
            .padding(.horizontal, -7)
    }

    private func length(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11.5).monospacedDigit())
            .foregroundStyle(Self.dimInk)
            .lineLimit(1)
            .fixedSize()
    }

    private struct RowFrame: ViewModifier {
        let ground: Color

        func body(content: Content) -> some View {
            content
                .padding(.horizontal, 4)
                .frame(height: RecordingsApplet.rowHeight)
                .background(ground, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
        }
    }
}

/// A recording's kind as the desktop panel's small tag: Contact green,
/// Whole pan blue, Sound amber.
private struct KindTag: View {
    let kind: RecordingEntry.Kind

    var body: some View {
        let (ground, ink): (Color, Color) = {
            switch kind {
            case .contact: return (WorkspacePalette.rgb(0x1F2A1F), WorkspacePalette.rgb(0x8FC79C))
            case .wholePan: return (WorkspacePalette.rgb(0x1E2A3A), WorkspacePalette.rgb(0x8CC4FF))
            case .sound: return (WorkspacePalette.rgb(0x2A2418), WorkspacePalette.rgb(0xD8A55F))
            }
        }()
        Text(RecordingsListModel.kindText(kind))
            .font(.system(size: 9.5, weight: .semibold))
            .foregroundStyle(ink)
            .padding(.horizontal, 4)
            .frame(height: 14)
            .background(ground, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
    }
}
