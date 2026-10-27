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
/// at the right, then the body, scrolling when the band is too short for it,
/// and a footer, when given, under the body (the Display pill's Reset this
/// pan and Setup defaults, as the board's `.dp-foot`): pinned while the
/// body scrolls where the sheet has room for rows above it, else last in
/// the scrolling body.
struct DropSheet<Trailing: View, Content: View>: View {
    let title: String
    let accessibilityIdentifier: String
    /// At the sheet's foot: pinned while the body scrolls, where there is room.
    var footer: AnyView? = nil
    @ViewBuilder let trailing: () -> Trailing
    @ViewBuilder let content: () -> Content
    /// While a Display pill row is held (its peek), the sheet's face and
    /// title clear away so the band shows under the one row.
    @Environment(\.dropSheetClear) private var clear

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
            .opacity(clear ? 0 : 1)
            .padding(.horizontal, 8)
            .frame(maxWidth: .infinity, minHeight: 22, maxHeight: 22)
            .background(LinearGradient(stops: [.init(color: ChromeColours.titleTop, location: 0),
                                               .init(color: ChromeColours.titleMiddle, location: 0.5),
                                               .init(color: ChromeColours.titleBottom, location: 1)],
                                       startPoint: .top, endPoint: .bottom))
            .overlay(alignment: .bottom) {
                Rectangle().fill(ChromeColours.titleBorder).frame(height: 1)
            }
            if let footer {
                // Everything in view when it fits; else the footer pinned
                // while the rows scroll, where the sheet leaves the rows
                // `pinnedFooterMinimumBody`; else, on a short band (the
                // phone held sideways), the footer last in the scrolling
                // rows, so the rows keep the sheet's height.
                ViewThatFits(in: .vertical) {
                    VStack(alignment: .leading, spacing: 0) {
                        body(content)
                        placed(footer)
                    }
                    VStack(alignment: .leading, spacing: 0) {
                        ScrollView {
                            body(content)
                        }
                        .frame(idealHeight: Self.pinnedFooterMinimumBody)
                        placed(footer)
                    }
                    ScrollView {
                        VStack(alignment: .leading, spacing: 0) {
                            body(content)
                            placed(footer)
                        }
                    }
                }
            } else {
                ViewThatFits(in: .vertical) {
                    body(content)
                    ScrollView {
                        body(content)
                    }
                }
            }
        }
        .background(clear ? Color.clear : ChromeColours.sheet)
        .clipShape(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius)
            .strokeBorder(clear ? Color.clear : ChromeColours.sheetBorder, lineWidth: 1))
        .shadow(color: .black.opacity(clear ? 0 : 0.55), radius: 14, y: 10)
        // The whole sheet takes its touches, title row included: none
        // falls through to the band's flags and buttons under it, or to
        // the layer behind that closes the sheet.
        .contentShape(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .accessibilityElement(children: .contain)
        .accessibilityLabel(title)
        .accessibilityIdentifier(accessibilityIdentifier)
    }

    /// The least height the rows keep above a pinned footer: the pill's
    /// tabs and three of its 30-point lines with their 9-point gaps, inside
    /// the body's 10-point padding (30 + 3 x 30 + 3 x 9 + 2 x 10 = 167),
    /// rounded up. Below that the footer would leave a header and no row
    /// (the phone held sideways, its pill about 174 points tall), so it scrolls with
    /// the rows instead; the iPad's pill and the upright phone's sheet keep
    /// it pinned.
    static var pinnedFooterMinimumBody: CGFloat { 170 }

    private func placed(_ footer: AnyView) -> some View {
        footer
            .padding(.horizontal, 10)
            .padding(.bottom, 10)
            .frame(maxWidth: .infinity, alignment: .leading)
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
