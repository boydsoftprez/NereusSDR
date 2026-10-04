// NereusSDR for iOS: the Noise section's settings: each noise reduction's settings as the Core describes them, NNR's model
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import NereusModels
import SwiftUI

/// The Noise section's Settings row (I10, D78: a visible row, no press
/// and hold) and NNR's settings group, as the desktop's NNR controls show
/// them: the Standard or Premium model, Suppression, and the advanced
/// settings, each on the slice; and NR1 to MNR's settings, each slot's
/// controls as the Core's catalogue describes them (`noiseReduction`: the
/// desktop flag's popups, their ranges, readouts and Reset). NNR takes the
/// catalogue's ranges too where the Core sends them, else the ones the Core
/// accepts. A slot a Core does not describe is greyed with its reason.
struct NnrSettingsSection: View {
    /// One NNR setting on the slice.
    struct Setting: Identifiable, Equatable {
        enum Kind: Equatable {
            /// A slider over `range`, its value shown to `decimals` places.
            case number(decimals: Int)
            /// A choice between `labels`, their values 0, 1, ...
            case choice(labels: [String])
        }

        let property: String
        let label: String
        let unit: String
        let range: StationCatalog.Range
        let kind: Kind

        var id: String { property }
    }

    /// NNR's settings in the desktop NNR controls' order, with the ranges
    /// the Core accepts for them.
    static let settings: [Setting] = [
        Setting(property: "nnrMaskFloorDb", label: "Suppression", unit: "dB",
                range: StationCatalog.Range(min: -50, max: -10, step: 0.1), kind: .number(decimals: 1)),
        Setting(property: "nnrPosition", label: "Position", unit: "",
                range: StationCatalog.Range(min: 0, max: 1, step: 1), kind: .choice(labels: ["Pre-AGC", "Post-AGC"])),
        Setting(property: "nnrAlpha", label: "Alpha", unit: "",
                range: StationCatalog.Range(min: 0, max: 4, step: 0.01), kind: .number(decimals: 2)),
        Setting(property: "nnrAlphaKneeDb", label: "Alpha knee", unit: "dB",
                range: StationCatalog.Range(min: 0, max: 40, step: 0.1), kind: .number(decimals: 1)),
        Setting(property: "nnrTauSeconds", label: "Noise time", unit: "s",
                range: StationCatalog.Range(min: 0.05, max: 30, step: 0.05), kind: .number(decimals: 2)),
        Setting(property: "nnrMaxGainDb", label: "Maximum gain", unit: "dB",
                range: StationCatalog.Range(min: 0, max: 24, step: 0.1), kind: .number(decimals: 1)),
        Setting(property: "nnrAttackMs", label: "Attack", unit: "ms",
                range: StationCatalog.Range(min: 0, max: 500, step: 0.1), kind: .number(decimals: 1)),
        Setting(property: "nnrReleaseMs", label: "Release", unit: "ms",
                range: StationCatalog.Range(min: 0, max: 500, step: 0.1), kind: .number(decimals: 1)),
    ]

    /// The reducers whose settings a Core without the catalogue's
    /// `noiseReduction` does not describe to a phone.
    static let undescribed = ["NR1", "NR2", "NR3", "NR4", "DFNR", "MNR"]
    /// Their catalogue keys, in the same order.
    static let undescribedSlots = ["nr1", "nr2", "nr3", "nr4", "dfnr", "mnr"]
    static let nnrSlot = "nnr"
    static let undescribedText =
        "This Core does not describe these settings to a phone. A desktop connected to the Core changes them."
    /// The two NNR models (`nnrModelSlot`).
    static let models = ["Standard", "Premium"]
    /// NNR's model property: the model buttons above its settings write it.
    static let nnrModelProperty = "nnrModelSlot"

    @ObservedObject var model: ModesTabModel
    @ObservedObject var rx: RxPanelModel
    /// The slot whose settings are open, by its catalogue key.
    @State private var open: String?

    /// `open` names a slot to show open at first (pictures); nil shows none.
    init(model: ModesTabModel, rx: RxPanelModel, open: String? = nil) {
        self.model = model
        self.rx = rx
        _open = State(initialValue: open)
    }

    var body: some View {
        ModesChrome.caption("Settings:")
        ModesChrome.grid(columns: 4) {
            PanelButton(label: "NNR", lit: open == Self.nnrSlot, style: .blue, disabled: rx.nnr == nil) {
                toggle(Self.nnrSlot)
            }
            .accessibilityLabel("NNR settings")
            .accessibilityIdentifier("modesNnrSettings")
            ForEach(Array(zip(Self.undescribed, Self.undescribedSlots)), id: \.1) { name, slot in
                PanelButton(label: name, lit: open == slot, style: .blue, disabled: !described(slot)) {
                    toggle(slot)
                }
                .accessibilityLabel("\(name) settings")
                .accessibilityHint(model.noiseReduction?[slot] == nil ? Self.undescribedText : "")
                .accessibilityIdentifier("modes\(name)Settings")
            }
        }
        let missing = Self.undescribed.enumerated().filter { model.noiseReduction?[Self.undescribedSlots[$0.offset]] == nil }
            .map(\.element)
        if !missing.isEmpty {
            ModesChrome.note(Self.namesText(missing) + " settings: " + Self.undescribedText)
        }
        if open == Self.nnrSlot, let nnr = rx.nnr {
            nnrGroup(nnr)
        } else if let slot = open, let controls = model.noiseReduction?[slot], model.slice != nil {
            slotGroup(slot, controls: controls)
        }
    }

    /// The Core describes this slot's settings and there is a slice to set.
    private func described(_ slot: String) -> Bool {
        model.noiseReduction?[slot] != nil && model.slice != nil
    }

    private func toggle(_ slot: String) {
        open = open == slot ? nil : slot
    }

    /// "NR1 to MNR" for the six in a row, else the names with "and".
    static func namesText(_ names: [String]) -> String {
        if names == undescribed {
            return "NR1 to MNR"
        }
        guard let last = names.last else {
            return ""
        }
        return names.count == 1 ? last : names.dropLast().joined(separator: ", ") + " and " + last
    }

    /// One slot's settings as the Core describes them, and its Reset where
    /// the desktop's popup has one.
    private func slotGroup(_ slot: String, controls: [StationCatalog.NrControl]) -> some View {
        let name = slot.uppercased()
        return VStack(alignment: .leading, spacing: 9) {
            ModesChrome.caption("\(name) settings:")
            ForEach(controls) { control in
                describedRow(control, name: name)
            }
            footer(slot, controls: controls, name: name)
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("modes\(name)SettingsGroup")
    }

    private func nnrGroup(_ nnr: RxPanelModel.Nnr) -> some View {
        VStack(alignment: .leading, spacing: 9) {
            ModesChrome.caption("NNR model:")
            ModesChrome.grid(columns: 2) {
                ForEach(Self.models.indices, id: \.self) { index in
                    PanelButton(label: Self.models[index], lit: nnr.modelSlot == Int64(index), style: .blue,
                                disabled: nnr.modelSlot == nil) {
                        rx.selectNnrModel(Int64(index))
                    }
                    .accessibilityIdentifier("modesNnrModel\(index)")
                }
            }
            if let slot = nnr.modelSlot, !(slot == 0 ? nnr.standardReady : nnr.premiumReady) {
                ModesChrome.note(RxPanelModel.nnrModelNotReadyText)
                    .accessibilityIdentifier("modesNnrModelNotReady")
            }
            if let controls = model.noiseReduction?[Self.nnrSlot] {
                // The Core's own ranges; the model is the buttons above.
                let tuning = controls.filter { $0.property != Self.nnrModelProperty }
                ForEach(tuning) { control in
                    describedRow(control, name: "NNR")
                }
                footer(Self.nnrSlot, controls: tuning, name: "NNR")
            } else {
                ForEach(Self.settings) { setting in
                    row(setting)
                }
            }
        }
    }

    /// Reset, where any of the slot's controls has one, and which settings
    /// the slice does not carry.
    @ViewBuilder
    private func footer(_ slot: String, controls: [StationCatalog.NrControl], name: String) -> some View {
        let absent = controls.filter { model.nrValues[$0.property] == nil }.map(\.label)
        if controls.contains(where: Self.hasReset) {
            PanelButton(label: "Reset", lit: false, style: .blue, disabled: absent.count == controls.count) {
                model.resetNrSlot(slot)
            }
            .frame(width: 96)
            .accessibilityLabel("Reset the \(name) settings")
            .accessibilityIdentifier("modes\(name)Reset")
        }
        if !absent.isEmpty {
            ModesChrome.note(Self.namesText(absent) + ": " + CatalogFeed.needsNewerCoreText)
        }
    }

    static func hasReset(_ control: StationCatalog.NrControl) -> Bool {
        switch control.kind {
        case .slider(let slider):
            return slider.reset != nil
        case .choice(let choices):
            return choices.reset != nil
        case .toggle:
            return false
        }
    }

    /// One control as the Core describes it.
    @ViewBuilder
    private func describedRow(_ control: StationCatalog.NrControl, name: String) -> some View {
        let value = model.nrValues[control.property]
        switch control.kind {
        case .slider(let slider):
            PanelSliderRow(label: control.label, value: RxPanelModel.number(value).map(slider.controlValue),
                           range: value == nil ? nil : slider.range,
                           accessibility: "\(name) \(control.label.lowercased())", format: slider.text, notConfirmed: model.isUnconfirmed(control.property)) {
                model.setNrControl(control, $0)
            }
            .accessibilityIdentifier("modes\(name)\(control.property)")
        case .toggle:
            let on = RxPanelModel.flag(value)
            HStack(spacing: 6) {
                Text(control.label)
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                    .frame(maxWidth: .infinity, alignment: .leading)
                PanelButton(label: on == true ? "On" : "Off", lit: on == true, style: .blue, disabled: on == nil) {
                    model.toggleNrControl(control)
                }
                .frame(width: 72)
                .accessibilityLabel("\(name) \(control.label)")
                .accessibilityValue(on == true ? "On" : "Off")
                .accessibilityIdentifier("modes\(name)\(control.property)")
            }
        case .choice(let choices):
            let current = RxPanelModel.whole(value)
            HStack(spacing: 6) {
                ModesChrome.label(control.label)
                ModesChrome.grid(columns: max(choices.options.count, 1)) {
                    ForEach(choices.options, id: \.id) { option in
                        PanelButton(label: option.label, lit: current == Int64(option.id), style: .blue,
                                    disabled: value == nil) {
                            model.setNrControl(control, Double(option.id))
                        }
                        .accessibilityLabel("\(name) \(control.label) \(option.label)")
                    }
                }
            }
        }
    }

    @ViewBuilder
    private func row(_ setting: Setting) -> some View {
        let value = model.nnrSettings[setting.property]
        switch setting.kind {
        case .number(let decimals):
            PanelSliderRow(label: setting.label, value: value, range: value == nil ? nil : setting.range,
                           accessibility: "NNR \(setting.label.lowercased())",
                           format: { Self.text($0, decimals: decimals, unit: setting.unit) }, notConfirmed: model.isUnconfirmed(setting.property)) {
                model.setNnrSetting(setting, $0)
            }
        case .choice(let labels):
            HStack(spacing: 6) {
                ModesChrome.label(setting.label)
                ModesChrome.grid(columns: labels.count) {
                    ForEach(labels.indices, id: \.self) { index in
                        PanelButton(label: labels[index], lit: value.map { Int($0.rounded()) == index } ?? false,
                                    style: .blue, disabled: value == nil) {
                            model.setNnrSetting(setting, Double(index))
                        }
                    }
                }
            }
        }
    }

    static func text(_ value: Double, decimals: Int, unit: String) -> String {
        let number = String(format: "%.\(decimals)f", value).replacingOccurrences(of: "-", with: "\u{2212}")
        return unit.isEmpty ? number : number + " " + unit
    }
}
