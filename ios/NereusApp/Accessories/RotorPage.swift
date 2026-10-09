// NereusSDR for iOS: the Rotor page: the dial, the heading, Turn, Stop and the nudge holds, the paths and the presets
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Rotor page (design, iPhone; the rotor mockup's phone): the status
/// line, the dial, the heading in amber and the target with its "to go"
/// under it, Turn while a selection waits, CCW, STOP and CW, Down and Up on
/// an az/el rotor, short and long path, and the preset chips. The rotor's
/// setup and its presets list are in Setup, CAT & Network, Rotor
/// (``RotorSetupPage``), where the desktop keeps them. The Accessories row
/// on the Radio tab and the Rotor row on the Tools tab open this same page.
///
/// A drag on the dial only selects; Turn sends it. Presets and Stop are one
/// tap; CCW, CW, Down and Up turn while held. Leaving the page or the app
/// going to the background ends a hold. With no rotor, a Core too old, or
/// the rotor not connected, every control stays shown and greyed with the
/// reason.
struct RotorPage: View {
    @ObservedObject var model: RotorModel
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        let reason = model.turnReason
        VStack(alignment: .leading, spacing: 10) {
            if let note = model.note {
                AccessoryChrome.Refusal(text: note) { model.dismissNote() }
            }
            status
            RotorDial(state: RotorDialState(model), enabled: reason == nil) { model.select(azimuth: $0) }
            readout
            if let selection = model.selection {
                turnButton(selection)
            }
            controls(reason: reason)
            if let reason {
                AccessoryChrome.Note(text: reason)
                    .accessibilityIdentifier("rotor.reason")
            }
            if let state = model.state, !state.fault.isEmpty {
                AccessoryChrome.Note(text: state.fault)
                    .accessibilityIdentifier("rotor.fault")
            }
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("rotorPage")
        .onDisappear { model.pageLeft() }
        .onChange(of: scenePhase) { _, phase in
            if phase != .active {
                model.sceneLeft()
            }
        }
    }

    private var status: some View {
        HStack(spacing: 6) {
            Circle().fill(dotColour).frame(width: 8, height: 8)
            Text(model.statusLine)
                .font(.system(size: 11))
                .foregroundStyle(ChromeColours.textDim)
                .lineLimit(2)
                .accessibilityIdentifier("rotor.status")
        }
        .frame(maxWidth: .infinity)
    }

    private var dotColour: Color {
        guard let state = model.state, state.connected, model.turnReason == nil else {
            return ChromeColours.linkOffDot
        }
        return state.motion == .stopped ? ChromeColours.linkUp : RotorColours.amber
    }

    private var readout: some View {
        VStack(spacing: 2) {
            let live = model.state?.heading != nil && model.state?.positionFresh == true
            Text(RotorModel.headingText(model.state?.heading))
                .font(.system(size: 34, weight: .semibold, design: .monospaced))
                .foregroundStyle(live ? RotorColours.amber : RotorColours.staleHeading)
                .accessibilityLabel("Heading")
                .accessibilityValue(RotorModel.headingText(model.state?.heading))
                .accessibilityIdentifier("rotor.heading")
            if model.staleSinceMs != nil {
                TimelineView(.periodic(from: .now, by: 1)) { _ in
                    if let words = model.staleText(nowMs: model.clock.nowMilliseconds) {
                        Text(words)
                            .font(.system(size: 11))
                            .foregroundStyle(ChromeColours.textFaint)
                            .accessibilityIdentifier("rotor.lastHeard")
                    }
                }
            }
            Text(model.targetLine)
                .font(.system(size: 12, design: .monospaced))
                .foregroundStyle(ChromeColours.textDim)
                .multilineTextAlignment(.center)
                .accessibilityIdentifier("rotor.target")
            if let line = model.elevationLine {
                Text(line)
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundStyle(ChromeColours.textDim)
                    .accessibilityIdentifier("rotor.elevation")
            }
        }
        .frame(maxWidth: .infinity)
    }

    private func turnButton(_ selection: Double) -> some View {
        Button {
            model.turn()
        } label: {
            Text("Turn to \(RotorModel.headingText(selection))")
                .font(.system(size: 14, weight: .semibold))
                .foregroundStyle(ChromeColours.accent)
                .frame(maxWidth: .infinity, minHeight: 40)
                .background(ChromeColours.accent.opacity(0.13), in: RoundedRectangle(cornerRadius: 8))
                .overlay(RoundedRectangle(cornerRadius: 8).strokeBorder(ChromeColours.accent, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityIdentifier("rotor.turn")
    }

    @ViewBuilder
    private func controls(reason: String?) -> some View {
        let enabled = reason == nil
        HStack(spacing: 8) {
            RotorHoldButton(label: "\u{25C0}", accessibility: "Turn counter-clockwise", direction: .counterClockwise,
                            enabled: enabled, model: model)
                .frame(maxWidth: .infinity)
            stopButton(enabled: enabled)
                .frame(maxWidth: .infinity)
                .layoutPriority(1)
            RotorHoldButton(label: "\u{25B6}", accessibility: "Turn clockwise", direction: .clockwise,
                            enabled: enabled, model: model)
                .frame(maxWidth: .infinity)
        }
        if model.state?.axes == .azimuthElevation {
            let elevation = model.elevationReason == nil
            HStack(spacing: 8) {
                RotorHoldButton(label: "\u{25BC} Down", accessibility: "Lower the elevation", direction: .down,
                                enabled: elevation, model: model)
                RotorHoldButton(label: "\u{25B2} Up", accessibility: "Raise the elevation", direction: .up,
                                enabled: elevation, model: model)
            }
        }
        HStack(spacing: 8) {
            AccessoryChrome.ChoiceButton(label: "Short path", lit: !model.longPath, enabled: true) {
                model.setLongPath(false)
            }
            .accessibilityIdentifier("rotor.shortPath")
            AccessoryChrome.ChoiceButton(label: "Long path", lit: model.longPath, enabled: true) {
                model.setLongPath(true)
            }
            .accessibilityIdentifier("rotor.longPath")
        }
        if !model.presets.isEmpty {
            LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 6), count: 4), spacing: 6) {
                ForEach(model.presets) { preset in
                    presetChip(preset, enabled: enabled)
                }
            }
        }
    }

    private func stopButton(enabled: Bool) -> some View {
        Button {
            model.stop()
        } label: {
            Text("STOP")
                .font(.system(size: 14, weight: .bold))
                .tracking(1)
                .foregroundStyle(enabled ? RotorColours.stopText : ChromeColours.buttonOffText)
                .frame(maxWidth: .infinity, minHeight: 46)
                .background(enabled ? RotorColours.stop : ChromeColours.buttonOff, in: RoundedRectangle(cornerRadius: 8))
                .overlay(RoundedRectangle(cornerRadius: 8)
                    .strokeBorder(enabled ? RotorColours.stopBorder : ChromeColours.buttonOffBorder, lineWidth: 1))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityLabel("Stop the rotor")
        .accessibilityIdentifier("rotor.stop")
    }

    private func presetChip(_ preset: RotorModel.Preset, enabled: Bool) -> some View {
        Button {
            model.turn(to: preset)
        } label: {
            Text("\(preset.name) \(Int(preset.degrees.rounded()))\u{00B0}")
                .font(.system(size: 11, weight: .semibold))
                .lineLimit(1)
                .minimumScaleFactor(0.7)
                .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
                .frame(maxWidth: .infinity, minHeight: 32)
                .background(enabled ? ChromeColours.button : ChromeColours.buttonOff, in: Capsule())
                .overlay(Capsule().strokeBorder(enabled ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder,
                                                lineWidth: 1))
                .contentShape(Capsule())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityLabel("Turn to \(preset.name), \(Int(preset.degrees.rounded())) degrees")
        .accessibilityIdentifier("rotor.preset.\(preset.id)")
    }
}

/// A nudge button: the rotor turns while it is held, and stops when it is
/// let go (the Core also stops it if the repeats lapse). A cancelled touch
/// ends the hold too.
struct RotorHoldButton: View {
    let label: String
    let accessibility: String
    let direction: RotorModel.Direction
    let enabled: Bool
    @ObservedObject var model: RotorModel
    @GestureState private var pressed = false

    var body: some View {
        let held = model.holding == direction
        Text(label)
            .font(.system(size: 14, weight: .semibold))
            .foregroundStyle(enabled ? (held ? Color.white : ChromeColours.text) : ChromeColours.buttonOffText)
            .frame(maxWidth: .infinity, minHeight: 46)
            .background(enabled ? (held ? ChromeColours.buttonOnBlue : ChromeColours.button) : ChromeColours.buttonOff,
                        in: RoundedRectangle(cornerRadius: 8))
            .overlay(RoundedRectangle(cornerRadius: 8)
                .strokeBorder(enabled ? (held ? ChromeColours.buttonOnBlueBorder : ChromeColours.buttonBorder)
                              : ChromeColours.buttonOffBorder, lineWidth: 1))
            .contentShape(Rectangle())
            .gesture(DragGesture(minimumDistance: 0).updating($pressed) { _, state, _ in
                state = true
            }, isEnabled: enabled)
            .onChange(of: pressed) { _, now in
                if now {
                    model.startHold(direction)
                } else if model.holding == direction {
                    model.endHold()
                }
            }
            .accessibilityElement()
            .accessibilityLabel(accessibility)
            .accessibilityHint(enabled ? "Touch and hold to turn" : "")
            .accessibilityAddTraits(.isButton)
            .accessibilityValue(enabled ? (held ? "Turning" : "") : "Not available")
            .accessibilityIdentifier("rotor.nudge.\(direction.rawValue)")
    }
}

/// The rotor's row among the Radio tab's accessories: its name and one-line
/// status, opening the Rotor page.
struct RotorAccessoryRow: View {
    @ObservedObject var model: RotorModel
    let open: () -> Void

    var body: some View {
        Button(action: open) {
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Rotor")
                        .font(.system(size: 14, weight: .bold))
                        .foregroundStyle(ChromeColours.textBright)
                    Text(model.statusLine)
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textDim)
                        .lineLimit(2)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                Image(systemName: "chevron.right")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(ChromeColours.textFaint)
            }
            .padding(12)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityIdentifier("accessory.rotor")
    }
}
