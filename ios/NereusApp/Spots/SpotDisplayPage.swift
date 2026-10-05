// NereusSDR for iOS: how spots look on this phone's band, which sources show, and the Core's lifetime and Clear
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// Spot Hub's Display (spec section 5.7 item 3, picture 20's third phone):
/// the desktop's knobs with its defaults and ranges, kept on this phone as
/// the desktop keeps them on each computer (spots on, Auto mode, levels,
/// the start position, the font size, override colours with the spot
/// colour, override background with its colour and opacity), the sources
/// shown on this phone's band, then what belongs to the Core: the spot
/// lifetime and Clear all spots, which clears them for every device. Last,
/// a count of what the Core holds. The desktop's Memories row is left off,
/// as the desktop leaves it off: memories are not built (D41).
///
/// Auto mode sets the slice's mode from the mode the Core works out for
/// each spot (a Core at `recordStreamVersion` 2); the phone does not work
/// one out itself (D4). On an older Core it is shown greyed with its
/// reason, and the setting is kept for a Core that names the mode.
struct SpotDisplayPage: View {
    @ObservedObject var spots: SpotsModel
    /// The lifetime step while the slider is held; sent when it is let go.
    @State private var lifetimeDraft: Double?
    @State private var askingToClear = false

    static let flagNote = "On the phone, spots never start behind a flag, whatever the position."
    static let autoModeDetail = "A tap on a spot also sets the slice's mode"
    static let clearDetail = "For every device on this Core"

    var body: some View {
        let display = spots.display
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: "On the band", tag: .thisPhone)
            SpotHubPage.Card {
                SpotHubPage.SettingRow(title: "Spots") {
                    SpotHubPage.OnOff(isOn: display.enabled, identifier: "spotDisplay.spots") {
                        spots.changeDisplay { $0.enabled.toggle() }
                    }
                }
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Auto mode", detail: Self.autoModeDetail) {
                    SpotHubPage.OnOff(isOn: display.autoMode, enabled: spots.autoModeReason == nil,
                                      identifier: "spotDisplay.autoMode") {
                        spots.changeDisplay { $0.autoMode.toggle() }
                    }
                }
                if let reason = spots.autoModeReason {
                    AccessoryChrome.Note(text: reason)
                        .padding(.horizontal, 10)
                        .padding(.bottom, 8)
                }
                SpotHubPage.Line()
                slider("Levels", value: display.maxLevels, range: SpotDisplaySettings.levelsRange, shown: "\(display.maxLevels)",
                       identifier: "spotDisplay.levels") { value in
                    spots.changeDisplay { $0.maxLevels = value }
                }
                SpotHubPage.Line()
                slider("Position", value: display.startPercent, range: SpotDisplaySettings.startPercentRange,
                       shown: "\(display.startPercent)%", identifier: "spotDisplay.position") { value in
                    spots.changeDisplay { $0.startPercent = value }
                }
                SpotHubPage.Line()
                slider("Font size", value: display.fontSize, range: SpotDisplaySettings.fontSizeRange,
                       shown: "\(display.fontSize)", identifier: "spotDisplay.fontSize") { value in
                    spots.changeDisplay { $0.fontSize = value }
                }
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Override colors", detail: "Off: colors come from your log") {
                    SpotHubPage.OnOff(isOn: display.overrideColours, identifier: "spotDisplay.overrideColours") {
                        spots.changeDisplay { $0.overrideColours.toggle() }
                    }
                }
                SpotHubPage.Line()
                colourRow("Spot color", hex: display.overrideColour, identifier: "spotDisplay.overrideColour") { hex in
                    spots.changeDisplay { $0.overrideColour = hex }
                }
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Override background", detail: "Off: no background behind a callsign") {
                    SpotHubPage.OnOff(isOn: display.overrideBackground, identifier: "spotDisplay.overrideBackground") {
                        spots.changeDisplay { $0.overrideBackground.toggle() }
                    }
                }
                SpotHubPage.Line()
                colourRow("Background color", hex: display.backgroundColour,
                          identifier: "spotDisplay.backgroundColour") { hex in
                    spots.changeDisplay { $0.backgroundColour = hex }
                }
                SpotHubPage.Line()
                slider("Opacity", value: display.backgroundOpacity, range: SpotDisplaySettings.backgroundOpacityRange,
                       shown: "\(display.backgroundOpacity)%", identifier: "spotDisplay.backgroundOpacity") { value in
                    spots.changeDisplay { $0.backgroundOpacity = value }
                }
            }
            ConnectChrome.Note(text: Self.flagNote)
            SpotHubPage.Heading(text: "Show on the band", tag: .thisPhone).padding(.top, 8)
            SpotHubPage.Pills(isOn: spots.showsOnBand, enabled: { SpotsModel.pillReason($0) == nil },
                              prefix: "spotDisplay.pill", toggle: spots.toggleBandSource)
            SpotHubPage.Heading(text: "At the Core", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                lifetime
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Clear all spots", detail: Self.clearDetail) {
                    Button {
                        askingToClear = true
                    } label: {
                        Text("Clear")
                            .font(.system(size: 13, weight: .bold))
                            .foregroundStyle(spots.clearReason == nil ? ChromeColours.revokeText : ChromeColours.buttonOffText)
                            .padding(.horizontal, 12)
                            .frame(height: 34)
                            .background(spots.clearReason == nil ? ChromeColours.revoke : ChromeColours.buttonOff,
                                        in: RoundedRectangle(cornerRadius: 4))
                            .overlay(RoundedRectangle(cornerRadius: 4)
                                .strokeBorder(spots.clearReason == nil ? ChromeColours.revokeBorder
                                                                       : ChromeColours.buttonOffBorder, lineWidth: 1))
                            .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .disabled(spots.clearReason != nil)
                    .accessibilityIdentifier("spotDisplay.clear")
                }
            }
            .confirmationDialog("Clear all spots?", isPresented: $askingToClear, titleVisibility: .visible) {
                Button("Clear all spots", role: .destructive) {
                    spots.clearAll()
                }
            } message: {
                Text("This clears the Core's spots for every device.")
            }
            if let reason = spots.clearReason {
                AccessoryChrome.Note(text: reason)
            }
            if let note = spots.displayNote {
                AccessoryChrome.Refusal(text: note) { spots.displayNote = nil }
            }
            SpotHubPage.Heading(text: "Statistics").padding(.top, 8)
            statistics
        }
    }

    private var lifetime: some View {
        let steps = SpotsModel.lifetimeSteps
        let step = lifetimeDraft.map { Int($0.rounded()) } ?? spots.lifetimeStep
        let enabled = spots.clearReason == nil
        return HStack(spacing: 10) {
            Text("Lifetime")
                .font(.system(size: 13))
                .foregroundStyle(ChromeColours.text)
                .frame(width: 72, alignment: .leading)
            Slider(value: Binding(get: { Double(step) }, set: { lifetimeDraft = $0 }),
                   in: 0...Double(steps.count - 1), step: 1) { editing in
                if !editing, let draft = lifetimeDraft {
                    spots.setLifetimeStep(Int(draft.rounded()))
                    lifetimeDraft = nil
                }
            }
            .tint(ChromeColours.accent)
            .disabled(!enabled)
            .accessibilityLabel("Lifetime")
            .accessibilityValue(SpotsModel.lifetimeWords(steps[min(max(step, 0), steps.count - 1)]))
            .accessibilityIdentifier("spotDisplay.lifetime")
            valueBox(SpotsModel.lifetimeWords(steps[min(max(step, 0), steps.count - 1)]), width: 74)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 9)
        .frame(minHeight: 48)
    }

    private var statistics: some View {
        let all = spots.spots
        let calls = Set(all.map(\.call)).count
        let running = SpotsModel.Source.allCases.filter { spots.status($0).state == .connected }.count
        let rows: [(String, String, Color)] = [
            ("Total spots", "\(all.count)", ChromeColours.text),
            ("Unique callsigns", "\(calls)", ChromeColours.text),
            ("Active sources", "\(running)", ChromeColours.text),
            ("New DXCC in feed", "\(all.filter { $0.dxccPriority == 4 }.count)", SpotColours.hex("#FF3030")),
            ("New bands in feed", "\(all.filter { $0.dxccPriority == 3 }.count)", SpotColours.hex("#FF8C00")),
        ]
        return Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 6) {
            ForEach(rows, id: \.0) { row in
                GridRow {
                    Text(row.0)
                        .foregroundStyle(ChromeColours.textDim)
                        .frame(maxWidth: .infinity, alignment: .leading)
                    Text(row.1)
                        .fontWeight(.bold)
                        .foregroundStyle(row.2)
                        .monospacedDigit()
                }
            }
        }
        .font(.system(size: 13))
        .padding(12)
        .background(ChromeColours.modesHead, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("spotDisplay.statistics")
    }

    private func slider(_ title: String, value: Int, range: ClosedRange<Int>, shown: String, identifier: String,
                        change: @escaping (Int) -> Void) -> some View {
        HStack(spacing: 10) {
            Text(title)
                .font(.system(size: 13))
                .foregroundStyle(ChromeColours.text)
                .frame(width: 72, alignment: .leading)
            Slider(value: Binding(get: { Double(value) }, set: { change(Int($0.rounded())) }),
                   in: Double(range.lowerBound)...Double(range.upperBound), step: 1)
                .tint(ChromeColours.accent)
                .accessibilityLabel(title)
                .accessibilityValue(shown)
                .accessibilityIdentifier(identifier)
            valueBox(shown, width: 44)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 9)
        .frame(minHeight: 48)
    }

    /// A colour the settings keep as `#RRGGBB`, picked with the system's colour picker.
    private func colourRow(_ title: String, hex: String, identifier: String,
                           change: @escaping (String) -> Void) -> some View {
        ColorPicker(selection: Binding(get: { SpotColours.hex(hex) },
                                       set: { change(SpotColours.hexText($0)) }), supportsOpacity: false) {
            Text(title)
                .font(.system(size: 14))
                .foregroundStyle(ChromeColours.text)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 9)
        .frame(minHeight: 48)
        .accessibilityIdentifier(identifier)
    }

    private func valueBox(_ text: String, width: CGFloat) -> some View {
        Text(text)
            .font(.system(size: 12, weight: .semibold, design: .monospaced))
            .foregroundStyle(ChromeColours.text)
            .lineLimit(1)
            .frame(width: width, height: 26)
            .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
            .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
            .accessibilityHidden(true)
    }
}
