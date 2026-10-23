// NereusSDR for iOS: the connecting screens' shared parts: the bar, the buttons, the rows and the notices, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The parts every connecting screen shares, drawn from the board's
/// `.nav`, `.list`, `.row`, `.rbtn`, `.wide`, `.code`, `.warnbox` and
/// `.tsheet` styles (docs/architecture/2026-09-23-iphone-app-design/board.html).
enum ConnectChrome {
    // The board's colours for these screens, beyond the toolbar's, in the
    // desktop's workspace look (docs/architecture/2026-10-20-phone-workspace-look-design.md,
    // appendix A and the board's mapping, make_board.py). A red that is not
    // transmit takes the muted danger set (W5).
    private typealias P = WorkspacePalette

    static let rowFill = P.rgb(0x141418)
    static let rowBorder = P.borderSubtle
    static let heading = P.textTertiary
    static let codeText = P.rgb(0x00E5FF) // kept (W2), the frequency cyan
    static let fieldFill = P.panelBg
    static let nameBorder = P.rgb(0x26262B)
    static let warn = P.amberText
    static let bad = P.redBorder
    static let badBorder = P.redBorder
    // Standalone red text on a plain panel takes kRedBorder, as the desktop
    // colors it (src/gui/SliceChooser.cpp:417); kRedText is only for text on a
    // kRedBg fill (src/gui/applets/ModMonitorApplet.cpp:630-640).
    static let badText = P.redBorder
    static let pillOn = P.liveGreen
    static let pillStale = P.noticeWarn
    static let pillOff = P.redBorder
    static let pillUnknown = P.borderMuted
    static let sheetFill = P.panelBg
    static let sheetEdge = P.border
    static let grab = P.textInactive

    /// The bar over a connecting screen: Back on the left, the title in
    /// the middle, and an optional control on the right.
    struct NavBar<Trailing: View>: View {
        let title: String
        var back: String?
        var onBack: () -> Void = {}
        var backIdentifier = "connectBack"
        var backHeight: CGFloat = 40
        @ViewBuilder var trailing: () -> Trailing

        var body: some View {
            ZStack {
                Text(title)
                    .font(.system(size: 16, weight: .bold))
                    .foregroundStyle(ChromeColours.textBright)
                    .lineLimit(1)
                    .accessibilityAddTraits(.isHeader)
                HStack {
                    if let back {
                        Button(action: onBack) {
                            Text("\u{2039} \(back)")
                                .font(.system(size: 15, weight: .semibold))
                                .foregroundStyle(ChromeColours.accentText)
                                .frame(height: backHeight)
                                .padding(.horizontal, 4)
                        }
                        .accessibilityLabel("Back to \(back)")
                        .accessibilityIdentifier(backIdentifier)
                    }
                    Spacer()
                    trailing()
                }
            }
            .padding(.horizontal, 8)
            .frame(height: 44)
            .background(ChromeColours.bar)
            .overlay(alignment: .bottom) {
                Rectangle().fill(ChromeColours.barBorder).frame(height: 1)
            }
        }
    }

    /// A full-width button: `.go` is the glass, `.plain` raised and `.stop`
    /// the danger set (docs/architecture/2026-10-20-phone-workspace-look-design.md,
    /// section 5); greyed, the raised face at 40 percent.
    struct WideButton: View {
        enum Look {
            case go
            case plain
            /// Red, for a step that takes someone off the air (the desktop's
            /// Unkey and take over).
            case stop
        }

        let title: String
        var look: Look = .plain
        var enabled = true
        var busy = false
        let action: () -> Void

        var body: some View {
            let face = Self.face(look, enabled: enabled)
            Button(action: action) {
                HStack(spacing: 8) {
                    if busy {
                        ProgressView().tint(workspaceForeground(face))
                    }
                    Text(title)
                }
                .font(.system(size: 14, weight: .bold))
                .foregroundStyle(workspaceForeground(face))
                .frame(maxWidth: .infinity)
                .frame(height: 48)
                .background { WorkspaceFace(face) }
                .opacity(enabled ? 1 : WorkspacePalette.disabledOpacity)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled || busy)
        }

        /// The treatment a look draws in, raised while greyed.
        static func face(_ look: Look, enabled: Bool) -> WorkspaceLook {
            guard enabled else {
                return .raised
            }
            switch look {
            case .go:
                return .glass
            case .plain:
                return .raised
            case .stop:
                return .danger
            }
        }

        /// The red take-over button (the desktop's TakeTransmitDialog), in
        /// the muted danger set (W5): it takes someone off the air, it is not
        /// itself transmit.
        static let stopFill = WorkspacePalette.redBg
        static let stopBorder = WorkspacePalette.redBorder
    }

    /// A row's button, the board's `.rbtn`: the glass for `go`, else raised.
    struct RowButton: View {
        let title: String
        var go = false
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                Text(title)
                    .font(.system(size: 13, weight: .bold))
                    .padding(.horizontal, 12)
                    .frame(minWidth: 88)
                    .frame(height: 40)
            }
            .buttonStyle(WorkspaceButtonStyle(go ? .glass : .raised))
        }
    }

    /// A section's heading, the board's `.list__head`.
    struct Heading: View {
        let text: String

        var body: some View {
            Text(text.uppercased())
                .font(.system(size: 11, weight: .bold))
                .kerning(1.1)
                .foregroundStyle(ConnectChrome.heading)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, 2)
                .accessibilityAddTraits(.isHeader)
        }
    }

    /// A line of small print under a screen.
    struct Note: View {
        let text: String

        var body: some View {
            Text(text)
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.textFaint)
                .frame(maxWidth: .infinity, alignment: .leading)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal, 2)
        }
    }

    /// A notice box, the board's `.warnbox`: its first line bold in the
    /// notice's colour, then the words, then an optional button.
    struct NoticeBox: View {
        enum Tone {
            case warn
            case bad
        }

        var tone: Tone = .warn
        let title: String?
        let text: String?
        var button: String?
        var action: () -> Void = {}

        var body: some View {
            VStack(alignment: .leading, spacing: 6) {
                if let title {
                    Text(title)
                        .font(.system(size: 13, weight: .bold))
                        .foregroundStyle(tone == .warn ? ConnectChrome.warn : ConnectChrome.bad)
                }
                if let text {
                    Text(text)
                        .font(.system(size: 13))
                        .foregroundStyle(ChromeColours.text)
                }
                if let button {
                    RowButton(title: button, go: true, action: action)
                        .padding(.top, 4)
                }
            }
            .lineSpacing(2)
            .fixedSize(horizontal: false, vertical: true)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(12)
            .background(tone == .warn ? ConnectChrome.warn.opacity(0.08) : ConnectChrome.badBorder.opacity(0.08),
                        in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius, style: .continuous)
                .strokeBorder(tone == .warn ? ConnectChrome.warn.opacity(0.35) : ConnectChrome.badBorder.opacity(0.45),
                              lineWidth: 1))
            .accessibilityElement(children: .combine)
        }
    }

    /// A state dot, the board's `.pill`.
    struct Pill: View {
        let colour: Color

        var body: some View {
            Circle().fill(colour).frame(width: 14, height: 14)
        }
    }
}

extension ConnectChrome.NavBar where Trailing == EmptyView {
    init(title: String, back: String? = nil, onBack: @escaping () -> Void = {}) {
        self.init(title: title, back: back, onBack: onBack, trailing: { EmptyView() })
    }
}
