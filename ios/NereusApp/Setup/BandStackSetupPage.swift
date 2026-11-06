// NereusSDR for iOS: Setup, General, Band Stack: the Core's stack options and defaults, and the question after a region edit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusMirror
import SwiftUI

/// Setup, General, Band Stack (docs/architecture/2026-11-05-band-stack-design.md
/// section 8): Auto-save, Auto-expiry, duplicate protection, Reset to region
/// defaults and Clear unlocked entries, all the Core's. It only reads the
/// mirrored document and sends the `bandStack.*` verbs; each row shows what
/// the Core has, not what was last tapped. A Core without band stacks, or no
/// radio, leaves the whole page greyed with the reason.
struct BandStackSetupPage: View {
    @ObservedObject var model: BandStackModel
    @State private var asking: Question?

    /// A choice's minutes or seconds, its words and the line under them.
    struct Choice: Equatable {
        let value: Int
        let title: String
        var detail = ""
    }

    private enum Question: Equatable {
        case reset(region: String)
        case clear
    }

    // The values the Core takes (docs/architecture/2026-11-05-band-stack-design.md section 6.2).
    static let autoSave = [
        Choice(value: 0, title: "Off", detail: "Only save what you add yourself"),
        Choice(value: 10, title: "After 10 seconds", detail: "Saves a spot you stay on"),
        Choice(value: 30, title: "After 30 seconds"),
        Choice(value: 60, title: "After 60 seconds"),
    ]
    static let autoExpiry = [
        Choice(value: 0, title: "Off", detail: "Saved spots stay until you delete them"),
        Choice(value: 5, title: "After 5 minutes unused"),
        Choice(value: 15, title: "After 15 minutes unused"),
        Choice(value: 30, title: "After 30 minutes unused"),
        Choice(value: 60, title: "After 60 minutes unused"),
    ]
    static let expiryNote = "Locked and default spots never expire, nor does a spot a slice is on."
    static let footNote = "These settings are the Core's, so every device sees the same ones. To show saved spots on the band, use Display on each device."

    // From src/gui/bandstack/BandStackPanel.cpp:431-432, the desktop's question.
    static func resetQuestion(region: String) -> String {
        "Replace the unlocked entries with the defaults for \(region)? Your locked entries stay."
    }

    // From src/gui/bandstack/BandStackPanel.cpp:440-441, the desktop's question.
    static let clearQuestion = "Clear every unlocked entry on every band? Locked entries stay."

    /// The line under Reset to region defaults.
    static func resetDetail(region: String?) -> String {
        let rule = "Locked entries you made stay."
        guard let region, !region.isEmpty else {
            return rule
        }
        return "\(region). \(rule)"
    }

    var body: some View {
        let stack = model.stack
        let enabled = model.unavailableReason == nil && stack != nil
        SetupPageContainer {
            if let reason = model.unavailableReason {
                SetupPanelSection {
                    Text(reason)
                        .accessibilityIdentifier("bandStackSetup.reason")
                }
            }
            if let note = model.note {
                noteRow(note)
            }
            SetupPanelSection {
                ForEach(Self.autoSave, id: \.value) { choice in
                    SetupChoiceRow(title: choice.title, detail: choice.detail,
                                   chosen: enabled && stack?.dwellSeconds == choice.value, enabled: enabled,
                                   id: "bandStackSetup.autoSave.\(choice.value)") {
                        model.options(dupeProtection: nil, expiryMinutes: nil, dwellSeconds: choice.value)
                    }
                }
            } header: {
                Text("Auto-save")
            }
            SetupPanelSection {
                ForEach(Self.autoExpiry, id: \.value) { choice in
                    SetupChoiceRow(title: choice.title, detail: choice.detail,
                                   chosen: enabled && stack?.expiryMinutes == choice.value, enabled: enabled,
                                   id: "bandStackSetup.autoExpiry.\(choice.value)") {
                        model.options(dupeProtection: nil, expiryMinutes: choice.value, dwellSeconds: nil)
                    }
                }
            } header: {
                Text("Auto-expiry")
            } footer: {
                Text(Self.expiryNote)
            }
            SetupPanelSection {
                SetupSwitchRow(title: "Don't save over a frequency already here",
                               detail: "Keeps one entry per frequency on a band",
                               isOn: Binding(get: { enabled && stack?.dupeProtection == true },
                                             set: { model.options(dupeProtection: $0, expiryMinutes: nil, dwellSeconds: nil) }),
                               enabled: enabled, id: "bandStackSetup.duplicates")
            } header: {
                Text("Saving")
            }
            SetupPanelSection {
                defaultsRow("Reset to region defaults", detail: Self.resetDetail(region: stack?.region), danger: false,
                            enabled: enabled, id: "bandStackSetup.reset") {
                    asking = .reset(region: stack?.region ?? "")
                }
                defaultsRow("Clear unlocked entries", detail: nil, danger: true, enabled: enabled,
                            id: "bandStackSetup.clear") {
                    asking = .clear
                }
            } header: {
                Text("Defaults")
            } footer: {
                Text(Self.footNote)
            }
        }
        .accessibilityIdentifier("bandStackSetup")
        .setupPageTitle("Band Stack")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .core)
            }
        }
        .alert(questionText, isPresented: Binding(get: { asking != nil }, set: { if !$0 { asking = nil } })) {
            Button("Cancel", role: .cancel) {}
                .accessibilityIdentifier("bandStackSetup.cancel")
            switch asking {
            case .reset:
                Button("Reset", role: .destructive) { model.reseed() }
                    .accessibilityIdentifier("bandStackSetup.confirmReset")
            case .clear:
                Button("Clear", role: .destructive) { model.clear(bandKey: nil) }
                    .accessibilityIdentifier("bandStackSetup.confirmClear")
            case nil:
                EmptyView()
            }
        }
    }

    private var questionText: String {
        switch asking {
        case .reset(let region):
            return Self.resetQuestion(region: region)
        case .clear:
            return Self.clearQuestion
        case nil:
            return ""
        }
    }

    private func noteRow(_ note: String) -> some View {
        SetupPanelSection {
            Button {
                model.note = nil
            } label: {
                HStack(alignment: .top, spacing: 8) {
                    Text(note)
                        .foregroundStyle(ChromeColours.refusalText)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .accessibilityIdentifier("bandStackSetup.note")
                    Image(systemName: "xmark")
                        .foregroundStyle(WorkspacePalette.textSecondary)
                }
            }
            .buttonStyle(.plain)
            .accessibilityHint("Clears this message")
            .accessibilityIdentifier("bandStackSetup.noteClose")
        }
    }

    private func defaultsRow(_ title: String, detail: String?, danger: Bool, enabled: Bool, id: String,
                             action: @escaping () -> Void) -> some View {
        Button(action: action) {
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .font(.body.weight(.semibold))
                    .foregroundStyle(danger ? ChromeColours.refusalText : WorkspacePalette.textPrimary)
                if let detail {
                    Text(detail)
                        .font(.footnote)
                        .foregroundStyle(WorkspacePalette.textSecondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .setupGreyed(!enabled)
        .accessibilityIdentifier(id)
    }
}

/// The question after this device's own region edit (spec V1). The edit is
/// marked as it goes out; the document's next new region within the window
/// asks, whichever device's change it turns out to be, as on the desktop. A
/// refused edit takes its mark back, an older Core takes none, and another
/// device's change with no mark here asks nothing. The region has already
/// changed either way; Replace only loads the new region's defaults, on the
/// Core. The question lapses with the link, as the Core's own do.
@MainActor
final class BandStackRegionPrompt: ObservableObject {
    /// The described control whose edit asks.
    static let controlId = "general.options.region"
    /// 60 seconds for the question a region edit can raise, and 5 more.
    // From src/gui/MainWindow.h:504 (kBandStackRegionEditWindow).
    static let editWindow: Duration = .seconds(65)

    struct Asked: Equatable {
        let id: UInt64
        let region: String
    }

    /// The question on show.
    @Published private(set) var asked: Asked?
    /// Replace has gone to the Core, with no answer yet.
    @Published private(set) var replacing = false
    /// The Core's words for why it did not replace them.
    @Published private(set) var refusal: String?

    // From src/gui/MainWindow.cpp:2898-2899, the desktop's words.
    static func text(region: String) -> String {
        "You changed the region to \(region). Replace the unlocked entries with the defaults for \(region)? Your locked entries stay."
    }

    private struct Edit {
        let id: UInt64
        let deadline: ContinuousClock.Instant
    }

    private let model: BandStackModel
    private let now: () -> ContinuousClock.Instant
    private var region: String?
    private var edit: Edit?
    private var serial: UInt64 = 0
    private var watch: AnyCancellable?

    init(model: BandStackModel, now: @escaping () -> ContinuousClock.Instant = { ContinuousClock.now }) {
        self.model = model
        self.now = now
        // A published value reaches the sink before the model holds it, so
        // this reads what the sink is handed.
        watch = model.$stack.combineLatest(model.$availability).sink { [weak self] stack, availability in
            MainActor.assumeIsolated {
                self?.follow(stack?.region, availability: availability)
            }
        }
    }

    /// This device is sending a region edit. The mark it returns goes back to
    /// ``editEnded(_:outcome:)``; nil on a Core with nothing to ask about.
    func noteEdit() -> UInt64? {
        guard model.availability == .ready else {
            edit = nil
            return nil
        }
        serial &+= 1
        edit = Edit(id: serial, deadline: now().advanced(by: Self.editWindow))
        return serial
    }

    /// The edit's answer: a refusal, or one never sent, changed nothing to ask about.
    func editEnded(_ id: UInt64, outcome: SetupEditOutcome) {
        switch outcome {
        case .refused, .notSent:
            if edit?.id == id {
                edit = nil
            }
        case .applied, .awaitingConfirmation:
            break
        }
    }

    /// Keep my entries, and a tap outside the question: nothing is sent.
    func keep() {
        guard !replacing else {
            return
        }
        close()
    }

    func replace() {
        guard let asked, !replacing else {
            return
        }
        replacing = true
        refusal = nil
        model.reseed { [weak self] reason in
            guard let self, self.asked?.id == asked.id else {
                return
            }
            replacing = false
            if let reason {
                refusal = reason
            } else {
                close()
            }
        }
    }

    private func follow(_ next: String?, availability: BandStackModel.Availability) {
        guard availability == .ready, let next, !next.isEmpty else {
            region = nil
            edit = nil
            close()
            return
        }
        let before = region
        region = next
        if let edit, now() >= edit.deadline {
            self.edit = nil
        }
        guard let before, next != before else {
            return
        }
        let edited = edit != nil
        edit = nil
        if edited {
            serial &+= 1
            replacing = false
            refusal = nil
            asked = Asked(id: serial, region: next)
        } else if asked != nil {
            // The region it names has gone, and Replace would load another's.
            close()
        }
    }

    private func close() {
        asked = nil
        replacing = false
        refusal = nil
    }
}

/// The region question, in the sheet the Core's own questions use, over
/// whatever screen is showing (``ConfirmationWindowAnchor``). Keep my entries
/// is the default.
struct BandStackRegionSheet: View {
    @ObservedObject var prompt: BandStackRegionPrompt
    let region: String

    var body: some View {
        ConfirmationSheetChrome.Sheet(kicker: "Band stack") {
            ConfirmationSheetChrome.Note(text: BandStackRegionPrompt.text(region: region), lead: true)
                .accessibilityIdentifier("bandStackRegionText")
            if let refusal = prompt.refusal {
                ConfirmationSheetChrome.Refusal(text: refusal)
            }
            VStack(spacing: 8) {
                ConnectChrome.WideButton(title: "Keep my entries", look: .go, enabled: !prompt.replacing) {
                    prompt.keep()
                }
                .keyboardShortcut(.defaultAction)
                .accessibilityIdentifier("bandStackRegionKeep")
                ConnectChrome.WideButton(title: prompt.replacing ? "Replacing\u{2026}" : "Replace", look: .stop,
                                         enabled: !prompt.replacing, busy: prompt.replacing) {
                    prompt.replace()
                }
                .accessibilityIdentifier("bandStackRegionReplace")
            }
        }
    }
}
