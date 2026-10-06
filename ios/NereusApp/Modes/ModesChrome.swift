// NereusSDR for iOS: the Modes tab's shared pieces: a section's title bar, captions, notes, grids and value boxes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusModels
import SwiftUI

/// The pieces every Modes section draws with, in the board's styles
/// (`.atitle`, `.abody`, `.acap`, `.agrid`, `.arow`, `.inset` and the
/// `.nbtn.tri` arrows).
@MainActor
enum ModesChrome {
    /// A section: its title bar over its body.
    static func section(_ title: String, subtitle: String? = nil,
                        @ViewBuilder content: () -> some View) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: 6) {
                Text(title)
                    .font(.system(size: 11, weight: .bold))
                    .foregroundStyle(ChromeColours.icon)
                if let subtitle {
                    Text(subtitle)
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.sectionSubtitle)
                }
            }
            .padding(.horizontal, 8)
            .frame(maxWidth: .infinity, minHeight: 22, maxHeight: 22, alignment: .leading)
            .background(LinearGradient(stops: [.init(color: ChromeColours.titleTop, location: 0),
                                               .init(color: ChromeColours.titleMiddle, location: 0.5),
                                               .init(color: ChromeColours.titleBottom, location: 1)],
                                       startPoint: .top, endPoint: .bottom))
            .overlay(alignment: .bottom) {
                Rectangle().fill(ChromeColours.titleBorder).frame(height: 1)
            }
            .accessibilityElement(children: .combine)
            .accessibilityAddTraits(.isHeader)
            VStack(alignment: .leading, spacing: 9) {
                content()
            }
            .padding(10)
        }
    }

    /// A caption over a row of buttons, `Preamp:`.
    static func caption(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(ChromeColours.caption)
            .padding(.bottom, -4)
    }

    /// Why a control is greyed, or the Core's words for a refused change.
    static func note(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(ChromeColours.textFaint)
            .fixedSize(horizontal: false, vertical: true)
    }

    /// A row's label, 62 points wide as the board's `.lbl`.
    static func label(_ text: String) -> some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(ChromeColours.textDim)
            .frame(width: 62, alignment: .leading)
    }

    /// Buttons in equal columns.
    static func grid(columns: Int, @ViewBuilder content: () -> some View) -> some View {
        LazyVGrid(columns: Array(repeating: GridItem(.flexible(minimum: 0), spacing: 4), count: columns), spacing: 4) {
            content()
        }
    }

    /// A value in its inset box; it reads, it does not take touches.
    static func inset(_ text: String, minWidth: CGFloat = 34, grow: Bool = false) -> some View {
        Text(text)
            .font(.system(size: 12).monospacedDigit())
            .foregroundStyle(ChromeColours.text)
            .lineLimit(1)
            .padding(.horizontal, 4)
            .padding(.vertical, 2)
            .frame(minWidth: minWidth, maxWidth: grow ? .infinity : nil)
            .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
            .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
    }

    /// One of a stepper's arrows, the board's `.nbtn.tri`.
    static func arrow(left: Bool, accessibility: String, disabled: Bool,
                      action: @escaping () -> Void) -> some View {
        PanelButton(label: left ? "\u{25C0}" : "\u{25B6}", lit: false, style: .blue, disabled: disabled,
                    action: action)
            .frame(width: 40)
            .accessibilityLabel(accessibility)
    }

    /// Hertz as the board writes them, with a true minus sign: `−3000 Hz`.
    static func hertz(_ hz: Int64?, signed: Bool = false) -> String {
        guard let hz else {
            return ""
        }
        if hz < 0 {
            return "\u{2212}\(-hz) Hz"
        }
        return signed ? "+\(hz) Hz" : "\(hz) Hz"
    }
}
