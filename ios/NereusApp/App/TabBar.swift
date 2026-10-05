// NereusSDR for iOS: the tab bar as the board draws it: NereusSDR's flat bar with its own glyphs
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The tab bar (spec section 5.1 item 2, section 5.2, board `.tabbar`):
/// a flat bar the colour of the toolbar across the bottom, the five tabs in
/// equal columns, each its glyph over its name, the chosen one in the
/// accent. Sideways it takes iOS's compact form, each glyph beside its
/// name, the five in a centred row. On an iPad each glyph sits over its
/// name, the five in a centred row either way up.
struct TabBar: View {
    @Binding var selection: AppTab
    let sideways: Bool
    /// An iPad either way up (spec section 5.6): each glyph over its name,
    /// the five a fixed width apart in the middle of the bar.
    var iPad = false

    static let accessibilityIdentifier = "Sections"
    /// Each tab's width on an iPad.
    static let iPadItemWidth: CGFloat = 132

    var body: some View {
        Group {
            if iPad {
                HStack(alignment: .top, spacing: 0) {
                    ForEach(AppTab.allCases) { tab in
                        item(tab)
                            .frame(width: Self.iPadItemWidth)
                    }
                }
                .padding(.top, 9)
                .frame(maxWidth: .infinity)
            } else if sideways {
                HStack(alignment: .center, spacing: 34) {
                    ForEach(AppTab.allCases) { tab in
                        item(tab)
                    }
                }
                .padding(.top, 5)
                .frame(maxWidth: .infinity, alignment: .top)
            } else {
                HStack(alignment: .top, spacing: 0) {
                    ForEach(AppTab.allCases) { tab in
                        item(tab)
                            .frame(maxWidth: .infinity)
                    }
                }
                .padding(.top, 7)
            }
        }
        .padding(.bottom, sideways && !iPad ? 4 : 6)
        .background(ChromeColours.bar.ignoresSafeArea(edges: [.bottom, .horizontal]))
        .overlay(alignment: .top) {
            Rectangle().fill(ChromeColours.barBorder).frame(height: 1).ignoresSafeArea(edges: .horizontal)
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Sections")
        .accessibilityIdentifier(Self.accessibilityIdentifier)
    }

    private func item(_ tab: AppTab) -> some View {
        let chosen = tab == selection
        return Button {
            selection = tab
        } label: {
            Group {
                if iPad {
                    VStack(spacing: 4) {
                        TabGlyph(tab: tab).frame(width: 32, height: 26)
                        Text(tab.title).font(.system(size: 13, weight: .semibold))
                    }
                } else if sideways {
                    HStack(spacing: 6) {
                        TabGlyph(tab: tab).frame(width: 27, height: 21)
                        Text(tab.title).font(.system(size: 12, weight: .semibold))
                    }
                    .frame(height: 26)
                } else {
                    VStack(spacing: 3) {
                        TabGlyph(tab: tab).frame(width: 32, height: 26)
                        Text(tab.title).font(.system(size: 10, weight: .semibold))
                    }
                }
            }
            .foregroundStyle(chosen ? ChromeColours.accent : ChromeColours.textFaint)
            .lineLimit(1)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(tab.title)
        .accessibilityAddTraits(chosen ? [.isSelected, .isButton] : .isButton)
        .accessibilityIdentifier("tab." + tab.rawValue)
    }
}
