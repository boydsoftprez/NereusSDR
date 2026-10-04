// NereusSDR for iOS: the Core’s live Diversity context, guarded active-slice Use, and eight memories
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The live Core context is independent of the band’s active slice. Use targets the active slice.
struct DiversityPage: View {
    @ObservedObject var model: DiversityModel

    var body: some View {
        let enabled = model.reason == nil
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: model.contextTitle, tag: .core)
                .accessibilityIdentifier("diversity.slice")
            if let frequency = model.frequencyText {
                Text(frequency).font(.system(size: 14).monospacedDigit())
                    .foregroundStyle(ChromeColours.textDim)
                    .accessibilityIdentifier("diversity.frequency")
            }
            SpotHubPage.Card {
                SpotHubPage.SettingRow(title: "Diversity", detail: model.contextTitle) {
                    HStack(spacing: 8) {
                        if model.coordinated {
                            Button("Use" + (model.activeTargetLetter.map { " " + $0 } ?? "")) { model.useActiveSlice() }
                                .font(.system(size: 14, weight: .bold))
                                .frame(minWidth: 60, minHeight: 44)
                                .buttonStyle(.bordered)
                                .disabled(!model.canUse)
                                .accessibilityIdentifier("diversity.use")
                        }
                        DiversityOnOff(isOn: model.enabled ?? false, enabled: model.canSwitch,
                                          identifier: "diversity.enabled") {
                            model.setEnabled(!(model.enabled ?? false))
                        }
                    }
                }
                SpotHubPage.Line()
                ToolPageParts.SliderRow(title: "Phase", value: model.phaseDeg, range: DiversityModel.phaseRange,
                                        step: DiversityModel.step, shown: Self.degrees, enabled: enabled,
                                        titleWidth: 48, identifier: "diversity.phase", notConfirmed: model.isUnconfirmed("diversityPhaseDeg"),
                                        editingChanged: { blendEditing("diversityPhaseDeg", $0) }, observesPhysicalLifetime: true,
                                        interactionGeneration: model.blendGeneration, captureCommit: model.capturePhaseCommit) { model.setPhase($0) }
                SpotHubPage.Line()
                ToolPageParts.SliderRow(title: "Gain", value: model.gainDb, range: DiversityModel.gainRange,
                                        step: DiversityModel.step, shown: Self.db, enabled: enabled, titleWidth: 48,
                                        identifier: "diversity.gain", notConfirmed: model.isUnconfirmed("diversityGainDb"),
                                        editingChanged: { blendEditing("diversityGainDb", $0) }, observesPhysicalLifetime: true,
                                        interactionGeneration: model.blendGeneration, captureCommit: model.captureGainCommit) { model.setGain($0) }
                if let reason = model.reason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "diversity.reason")
                }
                if let reason = model.useReason, model.coordinated, reason != model.reason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "diversity.useReason")
                }
                if let reason = model.moveUpdateReason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "diversity.updateReason")
                }
                if model.paused {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: model.pauseReason ?? DiversityModel.pausedText, identifier: "diversity.paused")
                }
            }
            SpotHubPage.Heading(text: "Sensitivity pattern", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                DiversityRadar(sensitivity: model.sensitivity, enabled: enabled && model.enabled == true,
                               phaseDeg: model.phaseDeg)
                    .padding(10)
                if let reason = model.patternReason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "diversity.patternReason")
                }
                SpotHubPage.Line()
                valueRow("Phase", model.phaseDeg.map(Self.degrees), identifier: "diversity.patternPhase")
                SpotHubPage.Line()
                valueRow("Gain", model.gainDb.map(Self.db), identifier: "diversity.patternGain")
            }
            SpotHubPage.Heading(text: "Memories", tag: .thisPhone).padding(.top, 8)
            SpotHubPage.Card {
                SpotHubPage.SettingRow(title: "Store", detail: model.storing ? "Tap a memory to keep this phase and gain"
                                                                              : "Tap a memory to set its phase and gain") {
                    DiversityOnOff(isOn: model.storing, enabled: enabled, identifier: "diversity.store") {
                        model.storing.toggle()
                    }
                }
                SpotHubPage.Line()
                LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 5), count: 4), spacing: 5) {
                    ForEach(0..<DiversityModel.memoryCount, id: \.self) { slot in
                        memoryButton(slot, enabled: enabled)
                    }
                }
                .padding(10)
            }
            ConnectChrome.Note(text: DiversityModel.memoryNote).padding(.top, 4)
            if let note = model.note {
                ToolPageParts.Refusal(text: note, identifier: "diversity.note").padding(.top, 4)
            }
        }
    }

    private func blendEditing(_ property: String, _ editing: Bool) {
        model.blendEditing(property, editing)
        #if DEBUG
        UITestDiversityFixture.sliderLifecycle.record("\(property).\(editing ? "begin" : "end")", model: model)
        #endif
    }

    /// A value the Core sends, shown as it is: the phase or the gain the pattern is made from.
    private func valueRow(_ title: String, _ value: String?, identifier: String) -> some View {
        HStack(spacing: 10) {
            Text(title)
                .font(.system(size: 14))
                .foregroundStyle(ChromeColours.text)
                .frame(maxWidth: .infinity, alignment: .leading)
            Text(value ?? "Not sent")
                .font(.system(size: 14).monospacedDigit())
                .foregroundStyle(value == nil ? ChromeColours.textDim : ChromeColours.textBright)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 9)
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier(identifier)
    }

    private func memoryButton(_ slot: Int, enabled: Bool) -> some View {
        let memory = model.memories[slot]
        // Recalling needs a kept memory; storing takes any.
        let usable = enabled && (model.storing || memory != nil)
        return Button {
            model.tapMemory(slot)
        } label: {
            VStack(spacing: 1) {
                Text("M\(slot + 1)")
                    .font(.system(size: 12, weight: .bold))
                Text(memory.map { "\(Self.degrees($0.phaseDeg)) \(Self.db($0.gainDb))" } ?? "Empty")
                    .font(.system(size: 9).monospacedDigit())
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
            }
            .foregroundStyle(usable ? (memory != nil ? ChromeColours.accent : ChromeColours.text)
                                    : ChromeColours.buttonOffText)
            .frame(maxWidth: .infinity, minHeight: 44)
            .background(model.storing && enabled ? ChromeColours.buttonOnAmber
                                                 : (usable ? ChromeColours.button : ChromeColours.buttonOff),
                        in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4)
                .strokeBorder(model.storing && enabled ? ChromeColours.buttonOnAmberBorder
                                                       : (usable ? ChromeColours.buttonBorder
                                                                 : ChromeColours.buttonOffBorder), lineWidth: 1))
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!usable)
        .accessibilityLabel("Memory \(slot + 1)")
        .accessibilityValue(memory.map { "\(Self.degrees($0.phaseDeg)), \(Self.db($0.gainDb))" } ?? "Empty")
        .accessibilityIdentifier("diversity.memory\(slot)")
    }

    static func degrees(_ value: Double) -> String {
        String(format: "%.1f\u{00B0}", value)
    }

    static func db(_ value: Double) -> String {
        let text = String(format: "%.1f dB", abs(value))
        return value < 0 ? "\u{2212}" + text : text
    }
}

/// The visual switch remains compact, with a real 44-point target around it.
struct DiversityOnOff: View {
    let isOn: Bool
    var enabled = true
    let identifier: String
    let toggle: () -> Void
    var body: some View {
        Button(action: toggle) {
            Text(isOn ? "On" : "Off").font(.system(size: 12, weight: .bold))
                .foregroundStyle(isOn ? ChromeColours.buttonOnGreenText : ChromeColours.text)
                .frame(width: 44, height: 30)
                .background(isOn ? ChromeColours.buttonOnGreen : ChromeColours.button,
                            in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(isOn ? ChromeColours.buttonOnGreenBorder : ChromeColours.buttonBorder, lineWidth: 1))
                .frame(width: 44, height: 44).contentShape(Rectangle())
        }
        .buttonStyle(.plain).disabled(!enabled).opacity(enabled ? 1 : 0.5)
        .accessibilityValue(isOn ? "On" : "Off").accessibilityIdentifier(identifier)
    }
}
