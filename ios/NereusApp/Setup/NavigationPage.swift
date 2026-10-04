// NereusSDR for iOS: Setup's Navigation page on this phone: the tuning dial, its haptics and direction, and touch
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// Setup, General, Navigation, kept on this phone (D12, spec section 5.1
/// items 8 and 9, picture 3, the board's navigation page): the tuning dial,
/// Off until someone picks one of the three, its two haptics and its
/// direction; then Touch, which takes the place of the desktop's Mouse
/// section (drag, tap, pinch and the double tap), and Tuning (snap a tap to
/// the step). Each choice is kept on this phone and applies at once.
struct NavigationPage: View {
    @ObservedObject var settings: PhoneSettings

    var body: some View {
        List {
            Section {
                ForEach(DialKind.allCases, id: \.self) { kind in
                    row(kind)
                }
            } header: {
                Text("Tuning dial")
            }
            Section {
                Toggle("Haptic tick on each detent", isOn: Binding(get: { settings.dialDetentTicks },
                                                                  set: { settings.dialDetentTicks = $0 }))
                    .disabled(settings.dialKind == .off)
                    .accessibilityIdentifier("dialDetentTicks")
                Toggle("Firmer bump on each whole kHz", isOn: Binding(get: { settings.dialKilohertzBumps },
                                                                     set: { settings.dialKilohertzBumps = $0 }))
                    .disabled(settings.dialKind == .off)
                    .accessibilityIdentifier("dialKilohertzBumps")
                Toggle("Reverse dial direction", isOn: Binding(get: { settings.dialReversed },
                                                               set: { settings.dialReversed = $0 }))
                    .disabled(settings.dialKind == .off)
                    .accessibilityIdentifier("dialReversed")
            } footer: {
                Text(settings.dialKind == .off ? Self.dialOffNote : Self.stepNote)
            }
            Section {
                toggle(Self.dragTitle, Self.dragDetail, isOn: Binding(get: { settings.dragToTune },
                                                                      set: { settings.dragToTune = $0 }),
                       id: "dragToTune")
                toggle("Tap to tune on the band", "A tap on the band tunes the active slice there",
                       isOn: Binding(get: { settings.tapToTune }, set: { settings.tapToTune = $0 }), id: "tapToTune")
                toggle("Pinch to zoom the band", "Zoom minus and plus stay on the waterfall",
                       isOn: Binding(get: { settings.pinchToZoom }, set: { settings.pinchToZoom = $0 }),
                       id: "pinchToZoom")
                Picker(selection: Binding(get: { settings.doubleTapAction },
                                          set: { settings.doubleTapAction = $0 })) {
                    ForEach(TuneGestures.DoubleTapAction.allCases, id: \.self) { action in
                        Text(Self.title(action)).tag(action)
                    }
                } label: {
                    Text("Double-tap action")
                }
                .accessibilityIdentifier("doubleTapAction")
            } header: {
                Text("Touch")
            }
            Section {
                toggle("Snap tap-to-tune to step", "A tap lands on the slice's nearest step",
                       isOn: Binding(get: { settings.snapTapToStep }, set: { settings.snapTapToStep = $0 }),
                       id: "snapTapToStep")
                    .disabled(!settings.tapToTune && settings.doubleTapAction != .tune)
            } header: {
                Text("Tuning")
            }
        }
        .navigationTitle("Navigation")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .thisPhone)
            }
        }
    }

    static let stepNote = "The step is set on the dial itself. Each slice keeps its own step on each band."
    static let dialOffNote = "Pick a dial above to use its haptics and direction."
    static let dragTitle = "Drag a flag to tune"
    static let dragDetail = "A drag on a slice's flag or passband tunes it in its step. A drag on the band moves the band."

    /// The double tap's three choices, in Setup's words.
    static func title(_ action: TuneGestures.DoubleTapAction) -> String {
        switch action {
        case .tune:
            return "Tune"
        case .center:
            return "Center"
        case .none:
            return "None"
        }
    }

    private func toggle(_ title: String, _ detail: String, isOn: Binding<Bool>, id: String) -> some View {
        Toggle(isOn: isOn) {
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                Text(detail)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
        }
        .accessibilityIdentifier(id)
    }

    private func row(_ kind: DialKind) -> some View {
        let chosen = settings.dialKind == kind
        return Button {
            settings.dialKind = kind
        } label: {
            HStack(spacing: 12) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(Self.title(kind))
                        .font(.body.weight(.semibold))
                        .foregroundStyle(.primary)
                    Text(Self.detail(kind))
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                Spacer(minLength: 0)
                Image(systemName: "checkmark")
                    .foregroundStyle(ChromeColours.accent)
                    .opacity(chosen ? 1 : 0)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(chosen ? .isSelected : [])
        .accessibilityIdentifier("dialKind.\(kind.rawValue)")
    }

    /// The board's words for each choice.
    static func title(_ kind: DialKind) -> String {
        switch kind {
        case .off:
            return "Off"
        case .waterfallKnob:
            return "Knob on the waterfall"
        case .sheetKnob:
            return "Pop-up knob"
        case .thumbwheel:
            return "Thumbwheel"
        }
    }

    static func detail(_ kind: DialKind) -> String {
        switch kind {
        case .off:
            return "Tune by dragging or tapping the band"
        case .waterfallKnob:
            return "Opposite PTT, always on screen"
        case .sheetKnob:
            return "A big knob when you tap the frequency"
        case .thumbwheel:
            return "A flat wheel along the bottom of the waterfall"
        }
    }
}
