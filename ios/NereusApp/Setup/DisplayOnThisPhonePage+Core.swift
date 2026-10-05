// NereusSDR for iOS: Setup's Display page, the Core's display settings as its catalogue describes them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import NereusModels
import SwiftUI

/// The Core's display settings on Setup's Display page (R-IOS-06, R-IOS-18,
/// R-IOS-27; link 7.4, `display`): each group the desktop's Setup > Display
/// pages have, in their order and words, marked with where its settings
/// live. FFT size (with the live bin width), window, Hz/bin target and FPS
/// are the Core's; the detectors, averaging and decimation are kept for
/// this pan on this phone and asked of the Core in the subscription.
extension DisplayOnThisPhonePage {
    static let coreNote = "These come from the Core, in the desktop's words. The Core's own settings are shared by every device; the rest are kept on this phone for this pan."

    /// The Core's groups, or, on a Core that does not describe them, its
    /// four settings greyed with the reason.
    @ViewBuilder
    var coreDisplay: some View {
        if core.available {
            ForEach(Array(core.groups.enumerated()), id: \.offset) { index, group in
                coreGroup(group.title, group.controls, first: index == 0)
            }
        } else {
            olderCoreGroup
        }
    }

    private func coreGroup(_ title: String, _ controls: [StationCatalog.Display.Control], first: Bool) -> some View {
        Section {
            ForEach(controls, id: \.label) { control in
                CoreControlRow(core: core, control: control)
                if control.settingsKey == CoreDisplayModel.fftSizeKey {
                    binWidthRow
                }
            }
        } header: {
            HStack(spacing: 6) {
                Text(title)
                SetupTagBadge(tag: CoreDisplayModel.tag(of: controls))
            }
        } footer: {
            VStack(alignment: .leading, spacing: 4) {
                if first {
                    Text(Self.coreNote)
                }
                ForEach(controls, id: \.label) { control in
                    if let reason = core.reason(control) {
                        Text("\(control.label): \(reason)")
                    }
                    let unsent = control.options.filter { !core.canSend($0, of: control) }
                    if !unsent.isEmpty {
                        Text("\(control.label), \(unsent.map(\.label).joined(separator: ", ")): \(CoreDisplayModel.choiceNotSentText)")
                    }
                }
                if first, let note = core.note {
                    Text(note)
                        .accessibilityIdentifier("coreDisplayNote")
                }
            }
        }
        .accessibilityIdentifier("coreGroup\(title)")
    }

    private var binWidthRow: some View {
        LabeledContent(core.binWidthLabel) {
            Text(core.binWidthText ?? "")
                .monospacedDigit()
                .foregroundStyle(.secondary)
        }
        .accessibilityIdentifier("coreBinWidth")
    }

    /// A Core without the description: its four settings shown greyed.
    private var olderCoreGroup: some View {
        Section {
            ForEach(Self.olderCoreRows, id: \.key) { row in
                LabeledContent(row.title) {
                    Text(core.settingValueText(row.key))
                        .monospacedDigit()
                }
                .foregroundStyle(.secondary)
                .accessibilityIdentifier("core.\(row.key)")
            }
            LabeledContent(core.binWidthLabel) {
                Text(core.binWidthText ?? "")
                    .monospacedDigit()
            }
            .foregroundStyle(.secondary)
            .accessibilityIdentifier("coreBinWidth")
        } header: {
            HStack(spacing: 6) {
                Text("FFT")
                SetupTagBadge(tag: .core)
            }
        } footer: {
            Text(CoreDisplayModel.olderCoreText)
                .accessibilityIdentifier("coreDisplayOlder")
        }
    }

    /// The Core's four settings by the phone's own names, for a Core that
    /// does not describe them.
    static let olderCoreRows: [(key: String, title: String)] = [
        (CoreDisplayModel.fftSizeKey, "FFT size"), (CoreDisplayModel.windowKey, "Window"),
        (CoreDisplayModel.hzPerBinKey, "Hz per bin target"), (CoreDisplayModel.fpsKey, "Frames a second"),
    ]
}

/// One of the Core's display controls, drawn as its kind says.
struct CoreControlRow: View {
    @ObservedObject var core: CoreDisplayModel
    let control: StationCatalog.Display.Control

    private var enabled: Bool { core.reason(control) == nil }
    private var id: String { "core.\(control.settingsKey ?? control.subscribe)" }

    var body: some View {
        Group {
            switch control.kind {
            case .choice:
                choice
            case .slider where !control.options.isEmpty:
                steps
            case .slider:
                slider
            case .toggle:
                Toggle(control.label, isOn: Binding(get: { (core.value(control) ?? 0) != 0 },
                                                    set: { core.set(control, to: $0 ? 1 : 0) }))
            case .other:
                LabeledContent(control.label, value: core.value(control).map(control.text) ?? "")
            }
        }
        .disabled(!enabled)
        .accessibilityIdentifier(id)
    }

    /// A choice among the Core's options; one this app can't send is named
    /// in the group's footer.
    private var choice: some View {
        Picker(control.label, selection: Binding(get: { core.value(control) ?? control.defaultValue ?? 0 },
                                                 set: { core.set(control, to: $0) })) {
            ForEach(control.options.filter { core.canSend($0, of: control) }, id: \.value) { option in
                Text(option.label).tag(option.value)
            }
        }
    }

    /// A slider over the Core's options (the FFT size): one step a tap.
    private var steps: some View {
        let value = core.value(control).map(control.clamped)
        let index = value.flatMap { value in control.options.firstIndex { $0.value == value } }
        return Stepper {
            HStack {
                Text(control.label)
                Spacer(minLength: 8)
                Text(value.map(control.text) ?? "")
                    .monospacedDigit()
                    .foregroundStyle(.secondary)
            }
        } onIncrement: {
            if let index, index + 1 < control.options.count {
                core.set(control, to: control.options[index + 1].value)
            }
        } onDecrement: {
            if let index, index > 0 {
                core.set(control, to: control.options[index - 1].value)
            }
        }
        .accessibilityValue(value.map(control.text) ?? "")
    }

    /// A number within the Core's range: minus and plus for a step, and a
    /// slider under them where the range has many steps.
    private var slider: some View {
        let value = core.value(control)
        let range = control.range
        return VStack(alignment: .leading, spacing: 4) {
            Stepper {
                HStack {
                    Text(control.label)
                    Spacer(minLength: 8)
                    Text(value.map(control.text) ?? "")
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                }
            } onIncrement: {
                if let value, let range {
                    core.set(control, to: min(value + range.step, range.max))
                }
            } onDecrement: {
                if let value, let range {
                    core.set(control, to: max(value - range.step, range.min))
                }
            }
            if let range, (range.max - range.min) / range.step > 50 {
                Slider(value: Binding(get: { value ?? range.min }, set: { core.set(control, to: $0) }),
                       in: range.min...range.max)
                    .accessibilityLabel(control.label)
            }
        }
        .accessibilityValue(value.map(control.text) ?? "")
    }
}
