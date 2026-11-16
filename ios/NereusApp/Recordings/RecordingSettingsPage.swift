// NereusSDR for iOS: Setup > Recording settings: the Core's Record button, files, disk and on-air choices
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 17, the phone's Recording settings page.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusMirror
import NereusModels
import Observation
import SwiftUI

/// Setup > Recording settings (design 2026-11-15-recording-voice-keyer-dsr
/// section 6.5): the desktop's Setup > Cores > Recordings page at phone
/// width, in its five groups. The settings live on the Core, so every
/// device shows the same ones; each change sends that one field and no
/// other. Not connected, every group dims with a note; a Core too old for
/// recordings, or for the on-air switches, greys what it lacks with V9's
/// words.
@MainActor
@Observable
final class RecordingSettingsModel {
    nonisolated static let notConnectedNote =
        "Not connected to a Core. Connect to change these. Everything here stays visible."
    nonisolated static let refusedText = "The Core did not take the change."
    /// The Core's defaults (section 6.5), shown before it sends its own.
    nonisolated static let defaults = RecordingsDocument.Settings(
        keepMyVoice: true, sidesApart: false, autoStart: false, autoStop: true, quietMinutes: 2, format: .mp3,
        disk: "", disks: [])
    /// The desktop's quiet time, in minutes (RecordingSettings::kQuietMinutesMin and Max).
    nonisolated static let quietRange = 1...60
    /// The file the Record button writes: 24 kHz, 16-bit WAV, or MP3 at 64
    /// kbit/s alone and 128 together apart (ContactRecorder, spec V2).
    nonisolated static let fileRateHz = 24_000
    nonisolated static let wavBytesPerSample = 2
    nonisolated static let monoKbitPerSec = 64
    nonisolated static let stereoKbitPerSec = 128
    /// Beyond this, the room left reads "well over a thousand hours".
    nonisolated static let manyHours = 1000.0

    /// The five on-air switches: the field and the word beside it, as the desktop has them.
    enum OnAirSwitch: String, CaseIterable, Sendable {
        case txEq = "onAirUseTxEq"
        case compressor = "onAirUseCompressor"
        case leveler = "onAirUseLeveler"
        case cfc = "onAirUseCfc"
        case phaseRotator = "onAirUsePhaseRotator"

        var title: String {
            switch self {
            case .txEq: return "EQ"
            case .compressor: return "Compressor"
            case .leveler: return "Leveler"
            case .cfc: return "CFC"
            case .phaseRotator: return "Phase rotator"
            }
        }

        func value(_ settings: RecordingsDocument.Settings) -> Bool {
            switch self {
            case .txEq: return settings.onAirUseTxEq
            case .compressor: return settings.onAirUseCompressor
            case .leveler: return settings.onAirUseLeveler
            case .cfc: return settings.onAirUseCfc
            case .phaseRotator: return settings.onAirUsePhaseRotator
            }
        }

        func change(_ on: Bool) -> RecordingsSettingsChange {
            switch self {
            case .txEq: return RecordingsSettingsChange(onAirUseTxEq: on)
            case .compressor: return RecordingsSettingsChange(onAirUseCompressor: on)
            case .leveler: return RecordingsSettingsChange(onAirUseLeveler: on)
            case .cfc: return RecordingsSettingsChange(onAirUseCfc: on)
            case .phaseRotator: return RecordingsSettingsChange(onAirUsePhaseRotator: on)
            }
        }
    }

    let client: RecordingsClient
    /// The Core's words for a change it refused.
    private(set) var refusal: String?
    /// Changes sent and not yet answered, shown meanwhile.
    private(set) var pending = RecordingsSettingsChange()

    init(client: RecordingsClient) {
        self.client = client
    }

    /// Why no group can change now, or nil.
    var reason: String? {
        if client.state == .offline {
            return Self.notConnectedNote
        }
        return client.unavailableReason(needs: RecordingsClient.recordingsVersion)
    }

    /// Why the on-air switches cannot change now, or nil.
    var onAirReason: String? {
        reason ?? client.unavailableReason(needs: RecordingsClient.onAirVersion)
    }

    /// The settings shown: the Core's, with any change on its way.
    var settings: RecordingsDocument.Settings {
        Self.merged(client.settings ?? Self.defaults, pending)
    }

    // MARK: Changes, one field each

    func setKeepMyVoice(_ on: Bool) async { await send(RecordingsSettingsChange(keepMyVoice: on)) }
    func setSidesApart(_ apart: Bool) async { await send(RecordingsSettingsChange(sidesApart: apart)) }
    func setAutoStart(_ on: Bool) async { await send(RecordingsSettingsChange(autoStart: on)) }
    func setAutoStop(_ on: Bool) async { await send(RecordingsSettingsChange(autoStop: on)) }
    func setFormat(_ format: RecordingsDocument.Format) async { await send(RecordingsSettingsChange(format: format)) }
    func setDisk(_ root: String) async { await send(RecordingsSettingsChange(disk: root)) }
    func setOnAir(_ which: OnAirSwitch, _ on: Bool) async { await send(which.change(on)) }

    func setQuietMinutes(_ minutes: Int) async {
        let held = min(Self.quietRange.upperBound, max(Self.quietRange.lowerBound, minutes))
        guard held != settings.quietMinutes else {
            return
        }
        await send(RecordingsSettingsChange(quietMinutes: held))
    }

    /// Sends one change; it shows at once, and the Core's document follows.
    func send(_ change: RecordingsSettingsChange) async {
        guard reason == nil, !change.setsOnAirSwitches || onAirReason == nil, !change.isEmpty else {
            return
        }
        refusal = nil
        pending = Self.merged(pending, change)
        do {
            try await client.setSettings(change)
        } catch {
            refusal = (error as? RecordingsError)?.reason ?? Self.refusedText
        }
        pending = Self.without(pending, change)
    }

    // MARK: Words

    /// About how many megabytes an hour the Record button writes.
    nonisolated static func megabytesPerHour(wav: Bool, apart: Bool) -> Int {
        let channels = apart ? 2 : 1
        let bytesPerSecond = wav ? Double(fileRateHz * wavBytesPerSample * channels)
            : Double(apart ? stereoKbitPerSec : monoKbitPerSec) * 1000 / 8
        return Int((bytesPerSecond * 3600 / 1.0e6).rounded())
    }

    func formatDetail(_ format: RecordingsDocument.Format) -> String {
        let shown = settings
        let mb = Self.megabytesPerHour(wav: format == .wav, apart: shown.keepMyVoice && shown.sidesApart)
        return format == .wav ? "Full quality, bigger. About \(mb) MB an hour."
            : "Small, plays anywhere. About \(mb) MB an hour."
    }

    /// The spec's example name, with the chosen file type.
    var nameExample: String {
        "2026-11-15 1432Z 7.236.400 LSB K4ABC." + (settings.format == .wav ? "wav" : "mp3")
    }

    var myVoiceDetail: String {
        settings.keepMyVoice ? "So the recording has both sides of the contact."
            : "Off. Only what you hear on the slice is kept."
    }

    var autoStartDetail: String {
        settings.autoStart ? "Every contact you answer or call is recorded without tapping Record."
            : "Off. Recording starts only when you tap Record."
    }

    var autoStopDetail: String {
        settings.autoStop ? "A recording left running ends \(settings.quietMinutes) min after the band goes quiet."
            : "Off. A recording runs until you tap Record again."
    }

    /// The disk the settings name, from the Core's list.
    var chosenDisk: RecordingsDocument.Disk? {
        let shown = settings
        return shown.disks.first { shown.disk.isEmpty ? $0.own : $0.root == shown.disk }
    }

    /// A disk as the picker lists it.
    nonisolated static func diskText(_ disk: RecordingsDocument.Disk) -> String {
        let free = RecordingsListModel.sizeText(disk.free)
        if disk.own {
            return disk.name.isEmpty ? "The Core's own disk (\(free) free)" : "The Core's own disk (\(disk.name), \(free) free)"
        }
        return "\(disk.name.isEmpty ? disk.root : disk.name) (\(free) free)"
    }

    /// The picker's choices: each disk the Core lists, by the root the
    /// settings name it with ("" for its own), and the chosen one even when
    /// it is not there now.
    var diskChoices: [(root: String, text: String)] {
        let shown = settings
        var choices = shown.disks.map { (root: $0.own ? "" : $0.root, text: Self.diskText($0)) }
        if !choices.contains(where: { $0.root == shown.disk }) {
            choices.append((root: shown.disk, text: shown.disk.isEmpty ? "The Core's own disk"
                                : "\(shown.disk) (not on the Core now)"))
        }
        return choices
    }

    /// "12 recordings, 1.4 GB. 812 GB free on this disk, room for about 28 hours more."
    var usageText: String {
        guard reason == nil, let index = client.index else {
            return "Not known."
        }
        let shown = settings
        let mb = Self.megabytesPerHour(wav: shown.format == .wav, apart: shown.keepMyVoice && shown.sidesApart)
        let free = chosenDisk?.free ?? index.freeBytes
        let hours = Double(free) / (Double(mb) * 1.0e6)
        let room = hours > Self.manyHours ? "room for well over a thousand hours more."
            : "room for about \(Int(hours.rounded(.down))) hours more."
        return RecordingsListModel.countText(index.count) + ", " + RecordingsListModel.sizeText(index.usedBytes) + ". "
            + RecordingsListModel.sizeText(free) + " free on this disk, " + room
    }

    // MARK: Inside

    nonisolated static func merged(_ base: RecordingsDocument.Settings,
                                   _ change: RecordingsSettingsChange) -> RecordingsDocument.Settings {
        RecordingsDocument.Settings(
            keepMyVoice: change.keepMyVoice ?? base.keepMyVoice, sidesApart: change.sidesApart ?? base.sidesApart,
            autoStart: change.autoStart ?? base.autoStart, autoStop: change.autoStop ?? base.autoStop,
            quietMinutes: change.quietMinutes ?? base.quietMinutes, format: change.format ?? base.format,
            disk: change.disk ?? base.disk, disks: base.disks,
            onAirUseTxEq: change.onAirUseTxEq ?? base.onAirUseTxEq,
            onAirUseCompressor: change.onAirUseCompressor ?? base.onAirUseCompressor,
            onAirUseLeveler: change.onAirUseLeveler ?? base.onAirUseLeveler,
            onAirUseCfc: change.onAirUseCfc ?? base.onAirUseCfc,
            onAirUsePhaseRotator: change.onAirUsePhaseRotator ?? base.onAirUsePhaseRotator)
    }

    nonisolated static func merged(_ base: RecordingsSettingsChange,
                                   _ change: RecordingsSettingsChange) -> RecordingsSettingsChange {
        RecordingsSettingsChange(
            keepMyVoice: change.keepMyVoice ?? base.keepMyVoice, sidesApart: change.sidesApart ?? base.sidesApart,
            autoStart: change.autoStart ?? base.autoStart, autoStop: change.autoStop ?? base.autoStop,
            quietMinutes: change.quietMinutes ?? base.quietMinutes, format: change.format ?? base.format,
            disk: change.disk ?? base.disk, onAirUseTxEq: change.onAirUseTxEq ?? base.onAirUseTxEq,
            onAirUseCompressor: change.onAirUseCompressor ?? base.onAirUseCompressor,
            onAirUseLeveler: change.onAirUseLeveler ?? base.onAirUseLeveler,
            onAirUseCfc: change.onAirUseCfc ?? base.onAirUseCfc,
            onAirUsePhaseRotator: change.onAirUsePhaseRotator ?? base.onAirUsePhaseRotator)
    }

    /// `base` without the fields `change` set to the same values.
    nonisolated static func without(_ base: RecordingsSettingsChange,
                                    _ change: RecordingsSettingsChange) -> RecordingsSettingsChange {
        func keep<T: Equatable>(_ a: T?, _ b: T?) -> T? { b != nil && a == b ? nil : a }
        return RecordingsSettingsChange(
            keepMyVoice: keep(base.keepMyVoice, change.keepMyVoice), sidesApart: keep(base.sidesApart, change.sidesApart),
            autoStart: keep(base.autoStart, change.autoStart), autoStop: keep(base.autoStop, change.autoStop),
            quietMinutes: keep(base.quietMinutes, change.quietMinutes), format: keep(base.format, change.format),
            disk: keep(base.disk, change.disk), onAirUseTxEq: keep(base.onAirUseTxEq, change.onAirUseTxEq),
            onAirUseCompressor: keep(base.onAirUseCompressor, change.onAirUseCompressor),
            onAirUseLeveler: keep(base.onAirUseLeveler, change.onAirUseLeveler),
            onAirUseCfc: keep(base.onAirUseCfc, change.onAirUseCfc),
            onAirUsePhaseRotator: keep(base.onAirUsePhaseRotator, change.onAirUsePhaseRotator))
    }
}

/// The page, in Setup's own look.
struct RecordingSettingsPage: View {
    let model: RecordingSettingsModel
    /// Tools > Recordings.
    var openRecordings: () -> Void = {}

    nonisolated static let title = "Recording settings"
    nonisolated static let intro = "Settings for the Record button beside the slice flag and for sound you save. "
        + "They live on the Core, so every phone and computer sees the same ones."

    var body: some View {
        let usable = model.reason == nil
        let shown = model.settings
        SetupPageContainer {
            Text(Self.intro)
                .font(.footnote)
                .foregroundStyle(WorkspacePalette.textSecondary)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal, 4)
            if let reason = model.reason {
                ConnectChrome.NoticeBox(tone: .warn, title: nil, text: reason)
                    .accessibilityIdentifier("recordingSettings.reason")
            }
            if let refusal = model.refusal {
                ToolPageParts.Refusal(text: refusal, identifier: "recordingSettings.refusal")
            }
            keeps(shown)
                .disabled(!usable)
                .setupGreyed(!usable)
            startStop(shown)
                .disabled(!usable)
                .setupGreyed(!usable)
            files(shown)
                .disabled(!usable)
                .setupGreyed(!usable)
            whereKept(shown)
                .disabled(!usable)
                .setupGreyed(!usable)
            playingBack(shown)
                .disabled(!usable)
                .setupGreyed(!usable)
        }
        .setupPageTitle(Self.title)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .core)
            }
        }
    }

    private func flag(_ value: Bool, _ set: @escaping (Bool) async -> Void) -> Binding<Bool> {
        Binding(get: { value }, set: { next in Task { await set(next) } })
    }

    private func keeps(_ shown: RecordingsDocument.Settings) -> some View {
        SetupPanelSection("What the Record button keeps", tag: .core,
                          note: shown.keepMyVoice ? "The round Record button beside the slice flag records the contact on that slice."
                              : "Only what you hear is kept while your own voice is off.") {
            SetupSwitchRow(title: "What you hear on the slice", detail: "Always kept.", isOn: .constant(true),
                           enabled: false, id: "recordingSettings.hear")
            SetupSwitchRow(title: "Your own voice when you transmit", detail: model.myVoiceDetail,
                           isOn: flag(shown.keepMyVoice) { await model.setKeepMyVoice($0) },
                           id: "recordingSettings.myVoice")
            SetupChoiceRow(title: "Together, one after the other", detail: "As you heard them.",
                           chosen: !shown.sidesApart, enabled: shown.keepMyVoice, id: "recordingSettings.together") {
                Task { await model.setSidesApart(false) }
            }
            SetupChoiceRow(title: "Apart: them on the left, you on the right",
                           detail: "So either side can be turned down.", chosen: shown.sidesApart,
                           enabled: shown.keepMyVoice, id: "recordingSettings.apart") {
                Task { await model.setSidesApart(true) }
            }
        }
    }

    private func startStop(_ shown: RecordingsDocument.Settings) -> some View {
        SetupPanelSection("Starting and stopping", tag: .core) {
            SetupSwitchRow(title: "Start by itself when I transmit", detail: model.autoStartDetail,
                           isOn: flag(shown.autoStart) { await model.setAutoStart($0) },
                           id: "recordingSettings.autoStart")
            SetupSwitchRow(title: "Stop by itself when it goes quiet", detail: model.autoStopDetail,
                           isOn: flag(shown.autoStop) { await model.setAutoStop($0) },
                           id: "recordingSettings.autoStop")
            SetupStepperRow("Quiet for", valueText: "\(shown.quietMinutes) min", id: "recordingSettings.quiet",
                            increment: { Task { await model.setQuietMinutes(shown.quietMinutes + 1) } },
                            decrement: { Task { await model.setQuietMinutes(shown.quietMinutes - 1) } })
                .disabled(!shown.autoStop)
                .accessibilityIdentifier("recordingSettings.quiet")
        }
    }

    private func files(_ shown: RecordingsDocument.Settings) -> some View {
        SetupPanelSection("Files", tag: .core,
                          note: "Also used for \"Save sound only\" from the Digital Spectrum Recorder. "
                              + "Rename any recording in Tools, Recordings.") {
            SetupChoiceRow(title: "MP3", detail: model.formatDetail(.mp3), chosen: shown.format == .mp3,
                           id: "recordingSettings.mp3") {
                Task { await model.setFormat(.mp3) }
            }
            SetupChoiceRow(title: "WAV", detail: model.formatDetail(.wav), chosen: shown.format == .wav,
                           id: "recordingSettings.wav") {
                Task { await model.setFormat(.wav) }
            }
            VStack(alignment: .leading, spacing: 6) {
                Text("Names")
                    .font(.body.weight(.semibold))
                Text("When and where it was made, with the other station's call when the log knows it:")
                    .font(.footnote)
                    .foregroundStyle(WorkspacePalette.textSecondary)
                    .fixedSize(horizontal: false, vertical: true)
                Text(model.nameExample)
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundStyle(WorkspacePalette.textPrimary)
                    .padding(.horizontal, 8)
                    .padding(.vertical, 6)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .workspaceInset()
                    .accessibilityIdentifier("recordingSettings.nameExample")
            }
        }
    }

    private func whereKept(_ shown: RecordingsDocument.Settings) -> some View {
        SetupPanelSection("Where they are kept", tag: .core,
                          note: "Recordings are never erased by themselves. Only you delete them.") {
            Picker("Disk", selection: Binding(get: { shown.disk }, set: { root in
                Task { await model.setDisk(root) }
            })) {
                ForEach(model.diskChoices, id: \.root) { choice in
                    Text(choice.text).tag(choice.root)
                }
            }
            .setupMenuRow("Disk")
            .accessibilityIdentifier("recordingSettings.disk")
            if model.chosenDisk?.sd == true {
                Text("The SD card is fine for these. Recordings are only written while one is running, so they "
                     + "do not wear the card the way the Digital Spectrum Recorder would.")
                    .font(.footnote)
                    .foregroundStyle(WorkspacePalette.textSecondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("recordingSettings.sdNote")
            }
            Button(action: openRecordings) {
                SetupLinkLabel {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Recordings")
                            .font(.body.weight(.semibold))
                        Text(model.usageText)
                            .font(.footnote)
                            .foregroundStyle(WorkspacePalette.textSecondary)
                            .fixedSize(horizontal: false, vertical: true)
                            .accessibilityIdentifier("recordingSettings.usage")
                    }
                }
            }
            .buttonStyle(SetupLinkStyle())
            .accessibilityIdentifier("recordingSettings.recordings")
        }
    }

    private func playingBack(_ shown: RecordingsDocument.Settings) -> some View {
        let onAirUsable = model.onAirReason == nil
        return SetupPanelSection("Playing back", tag: .core,
                                 note: model.reason == nil ? model.onAirReason
                                     ?? "They return to your normal settings when it stops going out." : nil) {
            VStack(alignment: .leading, spacing: 4) {
                Text("Play comes out of this phone's speaker or headphones.")
                Text("Keying while a recording plays sends it on the air.")
                Text("Use while it goes out:")
                    .foregroundStyle(WorkspacePalette.textSecondary)
            }
            .font(.footnote)
            .fixedSize(horizontal: false, vertical: true)
            ForEach(RecordingSettingsModel.OnAirSwitch.allCases, id: \.self) { which in
                Toggle(which.title, isOn: flag(which.value(shown)) { await model.setOnAir(which, $0) })
                    .font(.body.weight(.semibold))
                    .disabled(!onAirUsable)
                    .accessibilityIdentifier("recordingSettings.\(which.rawValue)")
            }
        }
    }
}
