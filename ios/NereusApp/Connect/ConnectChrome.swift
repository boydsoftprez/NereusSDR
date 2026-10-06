// NereusSDR for iOS: the connecting screens' shared parts: the bar, the buttons, the rows and the notices, as the board draws them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The parts every connecting screen shares, drawn from the board's
/// `.nav`, `.list`, `.row`, `.rbtn`, `.wide`, `.code`, `.warnbox` and
/// `.tsheet` styles (docs/architecture/2026-09-23-iphone-app-design/board.html).
enum ConnectChrome {
    // The board's colours for these screens, beyond the toolbar's.
    static let rowFill = rgb(0x0D, 0x1B, 0x28)
    static let rowBorder = rgb(0x20, 0x30, 0x40)
    static let heading = rgb(0x70, 0x80, 0x90)
    static let codeText = rgb(0x00, 0xE5, 0xFF)
    static let fieldFill = rgb(0x0A, 0x0A, 0x18)
    static let nameBorder = rgb(0x2A, 0x3A, 0x4A)
    static let warn = rgb(0xFF, 0xD7, 0x00)
    static let bad = rgb(0xFF, 0x6B, 0x6B)
    static let badBorder = rgb(0xFF, 0x50, 0x50)
    static let badText = rgb(0xFF, 0x8A, 0x8A)
    static let pillOn = rgb(0x39, 0xC1, 0x67)
    static let pillStale = rgb(0xD3, 0x9C, 0x2A)
    static let pillOff = rgb(0xC1, 0x48, 0x48)
    static let pillUnknown = rgb(0x40, 0x48, 0x58)
    static let sheetFill = rgb(0x0A, 0x0A, 0x18)
    static let sheetEdge = rgb(0x30, 0x40, 0x50)
    static let grab = rgb(0x40, 0x50, 0x60)

    static func rgb(_ red: Int, _ green: Int, _ blue: Int) -> Color {
        Color(red: Double(red) / 255, green: Double(green) / 255, blue: Double(blue) / 255)
    }

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
                                .foregroundStyle(ChromeColours.accent)
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

    /// A full-width button: `.go` is the board's blue one.
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
            Button(action: action) {
                HStack(spacing: 8) {
                    if busy {
                        ProgressView().tint(.white)
                    }
                    Text(title)
                }
                .font(.system(size: 14, weight: .bold))
                .foregroundStyle(enabled ? (look == .plain ? ChromeColours.text : Color.white) : ChromeColours.textDim)
                .frame(maxWidth: .infinity)
                .frame(height: 48)
                .background(fill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(border, lineWidth: 1))
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled || busy)
        }

        private var fill: Color {
            guard enabled else {
                return ChromeColours.buttonOff
            }
            switch look {
            case .go:
                return ChromeColours.buttonOnBlue
            case .plain:
                return ChromeColours.button
            case .stop:
                return Self.stopFill
            }
        }

        private var border: Color {
            guard enabled else {
                return ChromeColours.buttonOffBorder
            }
            switch look {
            case .go:
                return ChromeColours.buttonOnBlueBorder
            case .plain:
                return ChromeColours.buttonBorder
            case .stop:
                return Self.stopBorder
            }
        }

        /// The desktop's red take-over button (TakeTransmitDialog: #b02020, edge #ff4444).
        static let stopFill = Color(red: 0xB0 / 255, green: 0x20 / 255, blue: 0x20 / 255)
        static let stopBorder = Color(red: 0xFF / 255, green: 0x44 / 255, blue: 0x44 / 255)
    }

    /// A row's button, the board's `.rbtn`.
    struct RowButton: View {
        let title: String
        var go = false
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                Text(title)
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(go ? Color.white : ChromeColours.text)
                    .padding(.horizontal, 12)
                    .frame(minWidth: 88)
                    .frame(height: 40)
                    .background(go ? ChromeColours.buttonOnBlue : ChromeColours.button,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(go ? ChromeColours.buttonOnBlueBorder : ChromeColours.buttonBorder, lineWidth: 1))
            }
            .buttonStyle(.plain)
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
            .background(tone == .warn ? Color(red: 1, green: 215.0 / 255, blue: 0).opacity(0.08)
                            : Color(red: 1, green: 80.0 / 255, blue: 80.0 / 255).opacity(0.08),
                        in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4)
                .strokeBorder(tone == .warn ? Color(red: 1, green: 215.0 / 255, blue: 0).opacity(0.35)
                                  : Color(red: 1, green: 80.0 / 255, blue: 80.0 / 255).opacity(0.45),
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
