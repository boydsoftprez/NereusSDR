// NereusSDR for iOS: the chrome the toolbar's Pan and Display sheets share: dropped under the toolbar over the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Which of the toolbar's sheets is open.
enum OpenSheet: String, Equatable {
    case pan
    case display
    /// Every slice on the Core, from the Slice button (R-IOS-42).
    case slices
}

/// A sheet dropped under the toolbar over the band, as the board's
/// `.dropsheet` draws it (picture 25): the applet title row with its words
/// at the right, then the body, scrolling when the band is too short for it.
struct DropSheet<Trailing: View, Content: View>: View {
    let title: String
    let accessibilityIdentifier: String
    @ViewBuilder let trailing: () -> Trailing
    @ViewBuilder let content: () -> Content

    /// Sideways, the sheet keeps to this width.
    static var sidewaysWidth: CGFloat { 420 }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: 6) {
                Text(title)
                    .font(.system(size: 11, weight: .bold))
                    .foregroundStyle(ChromeColours.icon)
                Spacer(minLength: 0)
                trailing()
            }
            .padding(.horizontal, 8)
            .frame(maxWidth: .infinity, minHeight: 22, maxHeight: 22)
            .background(LinearGradient(stops: [.init(color: ChromeColours.titleTop, location: 0),
                                               .init(color: ChromeColours.titleMiddle, location: 0.5),
                                               .init(color: ChromeColours.titleBottom, location: 1)],
                                       startPoint: .top, endPoint: .bottom))
            .overlay(alignment: .bottom) {
                Rectangle().fill(ChromeColours.titleBorder).frame(height: 1)
            }
            ViewThatFits(in: .vertical) {
                body(content)
                ScrollView {
                    body(content)
                }
            }
        }
        .background(ChromeColours.sheet)
        .clipShape(RoundedRectangle(cornerRadius: 6))
        .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(ChromeColours.sheetBorder, lineWidth: 1))
        .shadow(color: .black.opacity(0.55), radius: 14, y: 10)
        // The whole sheet takes its touches, title row included: none
        // falls through to the band's flags and buttons under it, or to
        // the layer behind that closes the sheet.
        .contentShape(RoundedRectangle(cornerRadius: 6))
        .accessibilityElement(children: .contain)
        .accessibilityLabel(title)
        .accessibilityIdentifier(accessibilityIdentifier)
    }

    private func body(_ content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 9) {
            content()
        }
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

/// The small grey caption over a group (`.acap`).
struct SheetCaption: View {
    let text: String

    init(_ text: String) {
        self.text = text
    }

    var body: some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(ChromeColours.caption)
            .padding(.bottom, -4)
    }
}

/// A note under a group (`.dropsheet__note`): the board's words, a greyed
/// control's reason, or the Core's refusal.
struct SheetNote: View {
    let text: String

    init(_ text: String) {
        self.text = text
    }

    var body: some View {
        Text(text)
            .font(.system(size: 11))
            .foregroundStyle(ChromeColours.textDim)
            .lineSpacing(2)
            .fixedSize(horizontal: false, vertical: true)
            .padding(.horizontal, 2)
    }
}

/// Buttons in equal columns (`.agrid`).
struct SheetGrid<Content: View>: View {
    let columns: Int
    @ViewBuilder let content: () -> Content

    var body: some View {
        LazyVGrid(columns: Array(repeating: GridItem(.flexible(minimum: 0), spacing: 4), count: columns),
                  spacing: 4) {
            content()
        }
    }
}
