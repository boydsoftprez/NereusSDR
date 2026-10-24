// NereusSDR for iOS: the desktop's workspace treatments as SwiftUI faces and styles
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// One of the desktop's button treatments
/// (docs/architecture/2026-10-20-phone-workspace-look-design.md, section 5).
enum WorkspaceLook {
    /// Every button at rest: raisedFill, kGlassBtnTop to kGlassBtnBot, rim
    /// kBorder, a light line on top (WorkspaceComponents.cpp:1698).
    case raised
    /// A picked choice, a switch when on, a primary action: glassFill and
    /// glassTreatment (WorkspaceComponents.cpp:1010, :1735).
    case glass
    /// A running function (WorkspaceComponents.cpp:243-256, :1292).
    case litGreen
    /// A measured or caution state (WorkspaceComponents.cpp:258, :1292).
    case litAmber
    /// The keyed PTT and UNKEY: kTxRed with a lighter top (WorkspacePalette.h:324).
    case keyed
    /// Cancel, Revoke, a refusal: the muted danger set (WorkspacePalette.h:325-327).
    case danger
}

/// The colors one look is drawn in.
private struct WorkspaceLookColors {
    /// The fill from top to bottom, with each stop's place.
    let stops: [Gradient.Stop]
    let rim: Color
    let topEdge: Color
    /// Laid over the fill's top half to lift a flat color (the keyed face).
    let lift: Color?

    init(_ look: WorkspaceLook) {
        typealias P = WorkspacePalette
        switch look {
        case .raised:
            stops = [.init(color: P.glassBtnTop, location: 0), .init(color: P.glassBtnBot, location: 1)]
            rim = P.border
            topEdge = P.border
            lift = nil
        case .glass:
            stops = [.init(color: P.glassSelTop, location: 0), .init(color: P.glassSelBot, location: 1)]
            rim = P.glassSelBorder
            topEdge = P.glassSelBorderTop
            lift = nil
        case .litGreen:
            stops = [.init(color: P.litGreenTop, location: 0),
                     .init(color: P.litGreenMid, location: P.raisedMidStop),
                     .init(color: P.litGreenBottom, location: 1)]
            rim = P.litGreenRim
            topEdge = P.litGreenRimTop
            lift = nil
        case .litAmber:
            stops = [.init(color: P.litAmberTop, location: 0),
                     .init(color: P.litAmberMid, location: P.raisedMidStop),
                     .init(color: P.litAmberBottom, location: 1)]
            rim = P.amberBorder
            topEdge = P.litAmberRimTop
            lift = nil
        case .keyed:
            stops = [.init(color: P.txRed, location: 0), .init(color: P.txRed, location: 1)]
            rim = P.keyedRim
            topEdge = P.keyedRim
            lift = P.glassLight
        case .danger:
            stops = [.init(color: P.redBg, location: 0), .init(color: P.redBg, location: 1)]
            rim = P.redBorder
            topEdge = P.redBorder
            lift = nil
        }
    }

    /// The fill as it shows pressed: the gradient turned over, as the
    /// desktop's pressedFill turns raisedFill over (WorkspaceComponents.cpp:1728).
    var pressedStops: [Gradient.Stop] {
        stops.reversed().map { .init(color: $0.color, location: 1 - $0.location) }
    }
}

/// The text color of a look.
func workspaceForeground(_ look: WorkspaceLook) -> Color {
    switch look {
    case .raised: WorkspacePalette.textPrimary
    case .glass: WorkspacePalette.glassSelText
    case .litGreen: WorkspacePalette.litGreenText
    case .litAmber: WorkspacePalette.amberText
    case .keyed: WorkspacePalette.keyedText
    case .danger: WorkspacePalette.redText
    }
}

/// A look's face: its fill, its 1 point rim and the rim's lighter top edge.
struct WorkspaceFace: View {
    private let look: WorkspaceLook
    private let radius: CGFloat
    private let pressed: Bool

    init(_ look: WorkspaceLook, radius: CGFloat = WorkspacePalette.chipRadius, pressed: Bool = false) {
        self.look = look
        self.radius = radius
        self.pressed = pressed
    }

    var body: some View {
        let colors = WorkspaceLookColors(look)
        let shape = RoundedRectangle(cornerRadius: radius, style: .continuous)
        shape
            .fill(LinearGradient(stops: pressed ? colors.pressedStops : colors.stops,
                                 startPoint: .top, endPoint: .bottom))
            .overlay {
                if let lift = colors.lift {
                    shape.fill(LinearGradient(colors: pressed ? [.clear, lift] : [lift, .clear],
                                              startPoint: .top, endPoint: .bottom))
                }
            }
            .overlay { shape.strokeBorder(colors.rim, lineWidth: 1) }
            .overlay {
                // The top edge: the rim's upper band in the lighter color, and on
                // the raised face the light line laid over the rim.
                ZStack {
                    shape.strokeBorder(colors.topEdge, lineWidth: 1)
                    if look == .raised {
                        shape.strokeBorder(WorkspacePalette.glassLight, lineWidth: 1)
                    }
                }
                .mask(alignment: .top) {
                    Rectangle().frame(height: max(radius, 1))
                }
            }
            .accessibilityHidden(true)
    }
}

/// A button drawn in one of the desktop's treatments: the label over the
/// look's face in its text color, the face turned over while pressed, and
/// the same face at 40 percent when the button cannot act.
struct WorkspaceButtonStyle: ButtonStyle {
    private let look: WorkspaceLook
    private let radius: CGFloat
    @Environment(\.isEnabled) private var isEnabled

    init(_ look: WorkspaceLook, radius: CGFloat = WorkspacePalette.chipRadius) {
        self.look = look
        self.radius = radius
    }

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .foregroundStyle(workspaceForeground(look))
            .background {
                WorkspaceFace(look, radius: radius, pressed: configuration.isPressed)
            }
            .contentShape(RoundedRectangle(cornerRadius: radius, style: .continuous))
            .opacity(isEnabled ? 1 : WorkspacePalette.disabledOpacity)
    }
}

/// The Setup switch: the iOS switch's shape, a 51 by 31 point capsule with a
/// round knob, on in the glass and off in the inset; the label on the
/// leading side as a stock `Toggle` lays it out. VoiceOver reads it as the
/// stock switch it stands for.
struct WorkspaceSwitchStyle: ToggleStyle {
    static let trackSize = CGSize(width: 51, height: 31)
    static let knobInset: CGFloat = 2

    @Environment(\.isEnabled) private var isEnabled

    func makeBody(configuration: Configuration) -> some View {
        HStack {
            configuration.label
            Spacer(minLength: 8)
            Track(isOn: configuration.isOn)
                .contentShape(Capsule())
                .onTapGesture {
                    withAnimation(.snappy(duration: 0.2)) {
                        configuration.isOn.toggle()
                    }
                }
        }
        .opacity(isEnabled ? 1 : WorkspacePalette.disabledOpacity)
        .accessibilityRepresentation {
            Toggle(isOn: configuration.$isOn) {
                configuration.label
            }
            .toggleStyle(.switch)
        }
    }

    /// The capsule and its knob.
    private struct Track: View {
        let isOn: Bool

        var body: some View {
            let size = WorkspaceSwitchStyle.trackSize
            let knob = size.height - 2 * WorkspaceSwitchStyle.knobInset
            ZStack(alignment: isOn ? .trailing : .leading) {
                if isOn {
                    WorkspaceFace(.glass, radius: size.height / 2)
                } else {
                    Capsule()
                        .fill(WorkspacePalette.insetBg)
                        .overlay { Capsule().strokeBorder(WorkspacePalette.insetBorder, lineWidth: 1) }
                }
                Circle()
                    .fill(WorkspacePalette.textPrimary)
                    .frame(width: knob, height: knob)
                    .padding(WorkspaceSwitchStyle.knobInset)
            }
            .frame(width: size.width, height: size.height)
        }
    }
}

extension View {
    /// The root of everything presented: the app's root, and the content of
    /// every sheet, popover and full-screen cover, which do not inherit the
    /// root's environment. Every switch draws as the workspace switch (W7),
    /// and the workspace accent for text is the tint, so a control that reads
    /// the tint never falls back to the system blue (W6). The tint is
    /// accentText, not accentFill: the tint reaches text buttons (a toolbar's
    /// Done), and accentFill is for fills and marks only (spec section 10).
    /// Every fill that wants accentFill sets it itself.
    func workspacePresentationRoot() -> some View {
        toggleStyle(WorkspaceSwitchStyle())
            .tint(WorkspacePalette.accentText)
    }

    /// A panel: kGlassPanelTop to kGlassPanelBot, rim kBorder, radius 10
    /// (WorkspacePalette.h:348-350, :369).
    func workspacePanel(radius: CGFloat = WorkspacePalette.panelRadius) -> some View {
        background {
            let shape = RoundedRectangle(cornerRadius: radius, style: .continuous)
            shape
                .fill(LinearGradient(colors: [WorkspacePalette.glassPanelTop, WorkspacePalette.glassPanelBot],
                                     startPoint: .top, endPoint: .bottom))
                .overlay { shape.strokeBorder(WorkspacePalette.border, lineWidth: 1) }
        }
    }

    /// An inset field or trough: kInsetBg, rim kInsetBorder (WorkspacePalette.h:209, :214).
    func workspaceInset(radius: CGFloat = WorkspacePalette.chipRadius) -> some View {
        background {
            let shape = RoundedRectangle(cornerRadius: radius, style: .continuous)
            shape
                .fill(WorkspacePalette.insetBg)
                .overlay { shape.strokeBorder(WorkspacePalette.insetBorder, lineWidth: 1) }
        }
    }
}
