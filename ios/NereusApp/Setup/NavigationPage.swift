// NereusSDR for iOS: Setup's Navigation page on this phone: the tuning dial, its haptics and direction, and touch
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 18, the swipes on the panadapter beside
// the tuning dial, with Reset to defaults. J.J. Boyd (KG4VCF), AI-assisted
// via Claude Code.

import NereusBand
import SwiftUI

/// Setup, General, Navigation, kept on this phone (D12, spec section 5.1
/// items 8 and 9, picture 3, the board's navigation page): the tuning dial,
/// Off until someone picks one of the three, its two haptics and its
/// direction; then Touch, which takes the place of the desktop's Mouse
/// section (drag, tap, pinch and the double tap), and Tuning (snap a tap to
/// the step). Each choice is kept on this phone and applies at once.
/// Between the dial and Touch, the swipes on the panadapter (spec 11.3,
/// S1): what each edge and the tab bar open, and Reset to defaults, dimmed
/// until a mapping has changed.
struct NavigationPage: View {
    @ObservedObject var settings: PhoneSettings

    var body: some View {
        SetupPageContainer {
            SetupPanelSection {
                ForEach(DialKind.allCases, id: \.self) { kind in
                    row(kind)
                }
            } header: {
                Text("Tuning dial")
            }
            SetupPanelSection {
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
            SetupPanelSection {
                ForEach(SwipeOrigin.allCases, id: \.self) { origin in
                    swipeRow(origin)
                }
                Button {
                    settings.resetSwipes()
                } label: {
                    VStack(alignment: .leading, spacing: 2) {
                        Text(Self.swipeResetTitle)
                            .font(.body.weight(.semibold))
                            .foregroundStyle(settings.swipesChanged ? Self.resetLit : Self.resetDim)
                        Text(Self.swipeResetDetail)
                            .font(.footnote)
                            .foregroundStyle(WorkspacePalette.textSecondary)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(!settings.swipesChanged)
                .accessibilityIdentifier("swipe.reset")
            } header: {
                Text("Swipes on the panadapter")
            } footer: {
                Text(Self.swipeNote)
            }
            SetupPanelSection {
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
                .setupMenuRow("Double-tap action")
            } header: {
                Text("Touch")
            }
            SetupPanelSection {
                toggle("Snap tap-to-tune to step", "A tap lands on the slice's nearest step",
                       isOn: Binding(get: { settings.snapTapToStep }, set: { settings.snapTapToStep = $0 }),
                       id: "snapTapToStep")
                    .disabled(!settings.tapToTune && settings.doubleTapAction != .tune)
            } header: {
                Text("Tuning")
            }
        }
        .setupPageTitle("Navigation")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                SetupTagBadge(tag: .thisPhone)
            }
        }
    }

    static let swipeResetTitle = "Reset swipes to defaults"
    static let swipeResetDetail = "RX panel, TX panel, Roundtable, my frequency"
    static let swipeNote = "Panadapter tab only. A swipe is another way in: the toolbar's RX and TX buttons stay "
        + "where they are. Saved on this phone."
    /// Reset lit in the accent's text colour (the board's #4a7ba8 reads under
    /// 4.5:1 on the panel), dimmed grey until a mapping has changed.
    private static let resetLit = ChromeColours.accentText
    private static let resetDim = Color(red: 0x84 / 255, green: 0x84 / 255, blue: 0x8B / 255)

    /// Setup's words for where a swipe starts.
    static func title(_ origin: SwipeOrigin) -> String {
        switch origin {
        case .leftEdge:
            return "From the left edge"
        case .rightEdge:
            return "From the right edge"
        case .tabBar:
            return "Up from the tab bar"
        }
    }

    private func swipeRow(_ origin: SwipeOrigin) -> some View {
        Picker(selection: Binding(get: { settings.swipeTarget(origin) },
                                  set: { settings.setSwipeTarget($0, for: origin) })) {
            ForEach(SwipeTarget.allCases, id: \.self) { target in
                Text(target.title).tag(target)
            }
        } label: {
            Text(Self.title(origin))
        }
        .accessibilityIdentifier(PhoneSettings.swipeKey(origin))
        .setupMenuRow(Self.title(origin))
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
                    .foregroundStyle(WorkspacePalette.textSecondary)
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
                        .foregroundStyle(WorkspacePalette.textSecondary)
                }
                Spacer(minLength: 0)
                SetupCheckMark(chosen: chosen)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .setupPicked(chosen)
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
