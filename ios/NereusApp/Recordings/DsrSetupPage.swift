// NereusSDR for iOS: Setup > Digital Spectrum Recorder: on or off, the disk, the size, what is kept, erase
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 28, the phone's and iPad's Digital Spectrum
//   Recorder Setup page. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: Recording plan fix round: the Size slider starts at the Core's
//   1 GB, and the erase note clears on a reconnect or another Core.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusMirror
import NereusModels
import Observation
import SwiftUI

/// Setup > Digital Spectrum Recorder (design 2026-11-15-recording-voice-
/// keyer-dsr sections 3, 6.7 and A16, and the phone mockup's Setup screen):
/// the Core's recorder, which every device sees the same. Each change sends
/// its one field. A Core whose only disk is an SD card shows the red
/// warning and keeps the switch off. Not connected or an older Core, every
/// group stays in place, greyed, with the reason.
@MainActor
@Observable
final class DsrSetupModel {
    nonisolated static let title = "Digital Spectrum Recorder"
    nonisolated static let intro = "These settings live on the Core, so every phone and computer sees the same ones."
    nonisolated static let notConnectedTitle = "Not connected to a Core."
    nonisolated static let notConnectedText = "Connect to change the recorder. Everything here stays visible."
    nonisolated static let sdTitle = "This Core needs a proper storage device for the recorder."
    nonisolated static let sdText = "It only has an SD card, which is too slow for this and wears out under constant "
        + "writing. Connect a USB SSD or NVMe drive to the Core, then pick it below. A thumb drive is not enough."
    /// The Core's words when its only disk is an SD card (RecordingService::dsrReasonWords).
    nonisolated static let onlySdReason = "The only disk is an SD card."
    nonisolated static let refusedText = "The Core did not take the change."
    /// The Size slider's reach, in GB: from the Core's least
    /// (RecordingSettings kDsrSizeLimitGbMin, 1) to the phone mockup's 200;
    /// a size the Core holds beyond it widens the slider to it.
    nonisolated static let sizeRange = 1...200
    /// The Core's default size (design section 6.7).
    nonisolated static let defaultSizeGb = 10

    let client: RecordingsClient
    private(set) var refusal: String?
    /// Changes sent and not yet answered, shown meanwhile.
    private(set) var pendingEnabled: Bool?
    private(set) var pendingDisk: String?
    private(set) var pendingSizeGb: Int?
    /// The size while the slider moves; sent when it lets go.
    var draggedSizeGb: Double?
    /// The erase question is open.
    var askingErase = false
    /// The Core's answer to an erase, shown under the button until the
    /// link drops (a reconnect or another Core).
    var erasedNote: String? {
        guard let erased, client.state != .offline, erased.drops == client.linkDrops else {
            return nil
        }
        return erased.text
    }

    private var erased: (text: String, drops: Int)?

    init(client: RecordingsClient) {
        self.client = client
    }

    // MARK: Reasons

    var notConnected: Bool { client.state == .offline }

    /// Why nothing here can change now, or nil.
    var reason: String? {
        if notConnected {
            return Self.notConnectedTitle
        }
        return client.unavailableReason(needs: RecordingsClient.dsrVersion)
    }

    var dsr: RecordingsDocument.Dsr? { client.dsr }

    /// The Core has only an SD card for the recorder (A14, A16).
    var onlySdCard: Bool {
        guard reason == nil else {
            return false
        }
        if dsr?.reason == Self.onlySdReason {
            return true
        }
        let disks = client.settings?.disks ?? []
        return !disks.isEmpty && disks.allSatisfy(\.sd)
    }

    /// The Core's words for why the recorder is on but not recording,
    /// other than the SD card the warning already says.
    var stoppedReason: String? {
        guard reason == nil, let dsr, dsr.enabled, !dsr.recording, !dsr.reason.isEmpty, !onlySdCard else {
            return nil
        }
        return dsr.reason
    }

    // MARK: What is shown

    var enabled: Bool { pendingEnabled ?? dsr?.enabled ?? false }
    var disk: String { pendingDisk ?? dsr?.disk ?? "" }
    var sizeGb: Int { pendingSizeGb ?? dsr?.sizeLimitGb ?? Self.defaultSizeGb }
    var shownSizeGb: Int { draggedSizeGb.map { Int($0.rounded()) } ?? sizeGb }

    var sliderRange: ClosedRange<Double> {
        Double(Self.sizeRange.lowerBound)...Double(max(Self.sizeRange.upperBound, sizeGb))
    }

    /// The switch's line: what the recorder is doing.
    var statusText: String {
        guard reason == nil, let dsr else {
            return "Unknown while not connected."
        }
        if onlySdCard {
            return "Off. This Core has no proper storage device."
        }
        if !enabled {
            return "Off. Nothing new is kept."
        }
        if dsr.recording {
            return "Recording. Reaches back " + DsrPanModel.duration(dsr.hoursHeld * 3600) + "."
        }
        return dsr.reason.isEmpty ? "On. Starts when the radio runs." : dsr.reason
    }

    /// "Receiver 1, the whole 192 kHz it hears. About 2.8 GB an hour."
    var recordsText: String {
        guard let dsr, !dsr.receivers.isEmpty else {
            return "Each receiver the pan shows, the whole width it hears."
        }
        let names = dsr.receivers.map { String($0.receiver + 1) }
        let rates = dsr.receivers.map { Self.kilohertz($0.rateHz) }
        let who: String
        if dsr.receivers.count == 1 {
            who = "Receiver \(names[0]), the whole \(rates[0]) kHz it hears."
        } else {
            who = "Receivers \(Self.listed(names)), the whole \(Self.listed(rates)) kHz they hear."
        }
        guard dsr.bytesPerHour > 0 else {
            return who
        }
        return who + " About " + Self.gigabytes(Double(dsr.bytesPerHour)) + " an hour."
    }

    /// "Holds about 3 h 34 min at this pan width."
    var holdsText: String {
        guard let perHour = dsr?.bytesPerHour, perHour > 0 else {
            return "How long it holds shows once it records."
        }
        let seconds = Double(shownSizeGb) * 1.0e9 / Double(perHour) * 3600
        return "Holds about " + DsrPanModel.duration(seconds) + " at this pan width."
    }

    /// The in-use bar: kept, the write head and the part erased next, each 0 to 1.
    var meter: (kept: Double, head: Double, erasedNext: Double) {
        guard reason == nil, let dsr, dsr.sizeLimitBytes > 0 else {
            return (0, 0, 0)
        }
        let limit = Double(dsr.sizeLimitBytes)
        let kept = min(1, Double(dsr.usedBytes) / limit)
        let head = min(1, Double(dsr.writePosition) / limit)
        let full = dsr.usedBytes >= dsr.sizeLimitBytes
        return (kept, head, full ? min(0.04, 1 - head) : 0)
    }

    var fullText: String {
        guard reason == nil, let dsr else {
            return ""
        }
        if onlySdCard {
            return "Nothing is kept on the SD card."
        }
        if dsr.sizeLimitBytes > 0, dsr.usedBytes >= dsr.sizeLimitBytes {
            return "Full. The oldest part is erased as each new second comes in."
        }
        return Self.gigabytes(Double(dsr.usedBytes)) + " of " + Self.gigabytes(Double(dsr.sizeLimitBytes))
            + " used. When it is full, the oldest part is erased as each new second comes in."
    }

    /// "The last 3 h 34 min of the pan goes. ..."
    var eraseQuestion: String {
        let span = (dsr?.hoursHeld ?? 0) * 3600
        let what = span > 0 ? "The last " + DsrPanModel.duration(span) + " of the pan goes. " : ""
        return what + "Saved stretches in Recordings stay. Recording carries on from now."
    }

    var canErase: Bool { reason == nil && !onlySdCard }

    var format: RecordingsDocument.Format { client.settings?.format ?? .mp3 }

    /// The disk picker's choices: the Core's disks, and the chosen one when it is not there now.
    var diskChoices: [(root: String, text: String)] {
        let disks = client.settings?.disks ?? []
        var choices = disks.map { (root: $0.own ? "" : $0.root, text: Self.diskText($0)) }
        let chosen = disk
        if !choices.contains(where: { $0.root == chosen }) {
            choices.append((root: chosen, text: chosen.isEmpty ? "The Core's own disk"
                                : "\(chosen) (not on the Core now)"))
        }
        return choices
    }

    // MARK: Changes

    func setEnabled(_ on: Bool) async {
        pendingEnabled = on
        await send(RecordingsSettingsChange(dsrEnabled: on))
        pendingEnabled = nil
    }

    func setDisk(_ root: String) async {
        pendingDisk = root
        await send(RecordingsSettingsChange(dsrDisk: root))
        pendingDisk = nil
    }

    /// Sends the slider's size when it lets go.
    func commitSize() async {
        guard let dragged = draggedSizeGb else {
            return
        }
        draggedSizeGb = nil
        let gb = Int(dragged.rounded())
        guard gb != sizeGb else {
            return
        }
        pendingSizeGb = gb
        await send(RecordingsSettingsChange(dsrSizeLimitGb: gb))
        pendingSizeGb = nil
    }

    func setFormat(_ format: RecordingsDocument.Format) async {
        await send(RecordingsSettingsChange(format: format))
    }

    /// After the question: `dsr.eraseAll`.
    func eraseAll() async {
        guard canErase else {
            return
        }
        refusal = nil
        do {
            try await client.eraseAll()
            erased = ("Erased. The recorder starts again from now.", client.linkDrops)
        } catch {
            refusal = (error as? RecordingsError)?.reason ?? Self.refusedText
        }
    }

    private func send(_ change: RecordingsSettingsChange) async {
        guard reason == nil else {
            return
        }
        refusal = nil
        do {
            try await client.setSettings(change)
        } catch {
            refusal = (error as? RecordingsError)?.reason ?? Self.refusedText
        }
    }

    // MARK: Words

    nonisolated static func diskText(_ disk: RecordingsDocument.Disk) -> String {
        guard disk.sd else {
            return RecordingSettingsModel.diskText(disk)
        }
        // Short enough for the menu at phone width; the card is what matters here.
        return "SD card (\(RecordingsListModel.sizeText(disk.free)) free)"
    }

    nonisolated static func kilohertz(_ hz: Int64) -> String {
        let khz = Double(hz) / 1000
        return khz == khz.rounded() ? String(Int(khz)) : String(format: "%.1f", khz)
    }

    nonisolated static func gigabytes(_ bytes: Double) -> String {
        let gb = bytes / 1.0e9
        if gb >= 10 {
            return "\(Int(gb.rounded())) GB"
        }
        return String(format: "%.1f GB", gb)
    }

    /// "1", "1 and 2", "1, 2 and 3".
    nonisolated static func listed(_ items: [String]) -> String {
        guard items.count > 1 else {
            return items.first ?? ""
        }
        return items.dropLast().joined(separator: ", ") + " and " + (items.last ?? "")
    }
}

/// The page, in Setup's own look.
struct DsrSetupPage: View {
    @Bindable var model: DsrSetupModel
    /// Tools > Recordings.
    var openRecordings: () -> Void = {}
    /// Setup > Recording settings.
    var openRecordingSettings: () -> Void = {}

    var body: some View {
        let usable = model.reason == nil
        SetupPageContainer {
            Text(DsrSetupModel.intro)
                .font(.footnote)
                .foregroundStyle(WorkspacePalette.textSecondary)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal, 4)
            if model.onlySdCard {
                ConnectChrome.NoticeBox(tone: .bad, title: DsrSetupModel.sdTitle, text: DsrSetupModel.sdText)
                    .accessibilityIdentifier("dsrSetup.sdWarning")
            }
            if model.notConnected {
                ConnectChrome.NoticeBox(tone: .warn, title: DsrSetupModel.notConnectedTitle,
                                        text: DsrSetupModel.notConnectedText)
                    .accessibilityIdentifier("dsrSetup.reason")
            } else if let reason = model.reason {
                ConnectChrome.NoticeBox(tone: .warn, title: nil, text: reason)
                    .accessibilityIdentifier("dsrSetup.reason")
            }
            if let refusal = model.refusal {
                ToolPageParts.Refusal(text: refusal, identifier: "dsrSetup.refusal")
            }
            recording
                .disabled(!usable)
                .setupGreyed(!usable)
            whereKept
                .disabled(!usable)
                .setupGreyed(!usable)
            saved
                .disabled(!usable)
                .setupGreyed(!usable)
            erase
        }
        .setupPageTitle(DsrSetupModel.title)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .core)
            }
        }
        .toolbarColorScheme(.dark, for: .navigationBar)
        .environment(\.colorScheme, .dark)
        .alert("Erase what is kept now?", isPresented: $model.askingErase) {
            Button("Cancel", role: .cancel) {}
            Button("Erase", role: .destructive) {
                Task { await model.eraseAll() }
            }
        } message: {
            Text(model.eraseQuestion)
        }
    }

    private var recording: some View {
        SetupPanelSection("Recording", tag: .core) {
            SetupSwitchRow(title: "Keep recording the pan", detail: model.statusText,
                           isOn: Binding(get: { model.enabled && !model.onlySdCard },
                                         set: { on in Task { await model.setEnabled(on) } }),
                           enabled: !model.onlySdCard, id: "dsrSetup.enabled")
            if let stopped = model.stoppedReason {
                Text(stopped)
                    .font(.footnote)
                    .foregroundStyle(ConnectChrome.warn)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("dsrSetup.stopped")
            }
            VStack(alignment: .leading, spacing: 2) {
                Text("What it records")
                    .font(.body.weight(.semibold))
                Text(model.recordsText)
                    .font(.footnote)
                    .foregroundStyle(WorkspacePalette.textSecondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("dsrSetup.records")
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var whereKept: some View {
        SetupPanelSection("Where it is kept", tag: .core,
                          note: "It is always erasing. Whatever size you pick, the recorder keeps only the newest "
                              + "stretch and erases the oldest as it goes. To keep something, save it from the "
                              + "time strip.") {
            Picker("Disk", selection: Binding(get: { model.disk }, set: { root in
                Task { await model.setDisk(root) }
            })) {
                ForEach(model.diskChoices, id: \.root) { choice in
                    Text(choice.text).tag(choice.root)
                }
            }
            .setupMenuRow("Disk")
            .accessibilityIdentifier("dsrSetup.disk")
            VStack(alignment: .leading, spacing: 4) {
                HStack(alignment: .firstTextBaseline) {
                    Text("Size")
                        .font(.body.weight(.semibold))
                    Spacer()
                    Text("\(model.shownSizeGb) GB")
                        .font(.body.monospacedDigit())
                        .foregroundStyle(WorkspacePalette.textSecondary)
                }
                Slider(value: Binding(get: { model.draggedSizeGb ?? Double(model.sizeGb) },
                                      set: { model.draggedSizeGb = $0 }),
                       in: model.sliderRange, step: 1) { editing in
                    if !editing {
                        Task { await model.commitSize() }
                    }
                }
                .tint(WorkspacePalette.accentFill)
                .accessibilityIdentifier("dsrSetup.size")
                .accessibilityLabel("Size")
                .accessibilityValue("\(model.shownSizeGb) GB")
                Text(model.holdsText)
                    .font(.footnote)
                    .foregroundStyle(WorkspacePalette.textSecondary)
                    .accessibilityIdentifier("dsrSetup.holds")
            }
            VStack(alignment: .leading, spacing: 6) {
                Text("In use")
                    .font(.body.weight(.semibold))
                DsrInUseBar(meter: model.meter)
                    .frame(height: 12)
                    .accessibilityElement()
                    .accessibilityIdentifier("dsrSetup.inUse")
                    .accessibilityLabel("In use")
                    .accessibilityValue(model.fullText)
                HStack(spacing: 12) {
                    key("Kept", WorkspacePalette.accentFill)
                    key("Writing now", .white)
                    key("Erased next", DsrHoursMap.erasedNext)
                }
                if !model.fullText.isEmpty {
                    Text(model.fullText)
                        .font(.footnote)
                        .foregroundStyle(WorkspacePalette.textSecondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
            .opacity(model.onlySdCard ? WorkspacePalette.disabledOpacity : 1)
        }
    }

    private func key(_ text: String, _ colour: Color) -> some View {
        HStack(spacing: 4) {
            Rectangle().fill(colour).frame(width: 8, height: 8)
            Text(text)
                .font(.caption)
                .foregroundStyle(WorkspacePalette.textSecondary)
        }
    }

    private var saved: some View {
        SetupPanelSection("Saved stretches", tag: .core) {
            Button(action: openRecordings) {
                SetupLinkLabel {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Recordings")
                            .font(.body.weight(.semibold))
                        Text("Saved stretches are never erased by the recorder.")
                            .font(.footnote)
                            .foregroundStyle(WorkspacePalette.textSecondary)
                    }
                }
            }
            .buttonStyle(SetupLinkStyle())
            .accessibilityIdentifier("dsrSetup.recordings")
            Button(action: openRecordingSettings) {
                SetupLinkLabel {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Recording settings")
                            .font(.body.weight(.semibold))
                        Text("The Record button, file type and names.")
                            .font(.footnote)
                            .foregroundStyle(WorkspacePalette.textSecondary)
                    }
                }
            }
            .buttonStyle(SetupLinkStyle())
            .accessibilityIdentifier("dsrSetup.recordingSettings")
            Picker("Save sound only as", selection: Binding(get: { model.format }, set: { format in
                Task { await model.setFormat(format) }
            })) {
                Text("MP3").tag(RecordingsDocument.Format.mp3)
                Text("WAV").tag(RecordingsDocument.Format.wav)
            }
            .setupMenuRow("Save sound only as")
            .accessibilityIdentifier("dsrSetup.format")
        }
    }

    private var erase: some View {
        VStack(alignment: .leading, spacing: 6) {
            Button {
                model.askingErase = true
            } label: {
                Text("Erase what is kept now\u{2026}")
                    .font(.body.weight(.semibold))
                    .frame(maxWidth: .infinity, minHeight: 44)
            }
            .buttonStyle(WorkspaceButtonStyle(.danger))
            .disabled(!model.canErase)
            .accessibilityIdentifier("dsrSetup.erase")
            if let note = model.erasedNote {
                Text(note)
                    .font(.footnote)
                    .foregroundStyle(WorkspacePalette.textSecondary)
                    .accessibilityIdentifier("dsrSetup.erased")
            }
        }
    }
}

/// The in-use bar: what is kept in the workspace blue, the part erased
/// next in red, and a white line where the recorder writes now.
struct DsrInUseBar: View {
    let meter: (kept: Double, head: Double, erasedNext: Double)

    var body: some View {
        GeometryReader { proxy in
            let width = proxy.size.width
            ZStack(alignment: .leading) {
                Rectangle().fill(WorkspacePalette.insetBg)
                Rectangle().fill(WorkspacePalette.accentFill).frame(width: width * meter.kept)
                if meter.erasedNext > 0 {
                    Rectangle().fill(DsrHoursMap.erasedNext)
                        .frame(width: width * meter.erasedNext)
                        .offset(x: width * meter.head)
                }
                if meter.kept > 0 {
                    Rectangle().fill(Color.white)
                        .frame(width: 2)
                        .offset(x: max(0, width * meter.head - 1))
                }
            }
            .clipShape(Rectangle())
            .overlay(Rectangle().stroke(WorkspacePalette.insetBorder))
        }
    }
}
