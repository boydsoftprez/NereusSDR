// NereusSDR for iOS: the Radio tab's More list: Antenna Setup, Transverters, Manage Radios and Protocol Info as the Core offers them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The rest of the desktop's Radio menu (spec section 5.2 item 5, the
/// board's "More" block), one row each from ``RadioMenu``: a row that opens
/// its page has a chevron; one that cannot open now is greyed with its
/// reason on its line.
struct RadioMoreSection: View {
    let entries: [RadioMenu.Entry]
    let open: (RadioMenu.Page) -> Void

    var body: some View {
        if !entries.isEmpty {
            VStack(alignment: .leading, spacing: 6) {
                AccessoryChrome.Caption(text: "More")
                AccessoryChrome.Rows {
                    ForEach(Array(entries.enumerated()), id: \.element.id) { index, entry in
                        if index > 0 {
                            AccessoryChrome.RowDivider()
                        }
                        row(entry)
                    }
                }
            }
            .accessibilityElement(children: .contain)
            .accessibilityIdentifier("radioMore")
        }
    }

    private func row(_ entry: RadioMenu.Entry) -> some View {
        Button {
            if let page = entry.page {
                open(page)
            }
        } label: {
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(entry.title)
                        .font(.system(size: 14, weight: .bold))
                        .foregroundStyle(entry.enabled ? ChromeColours.textBright : ChromeColours.buttonOffText)
                    Text(entry.detail)
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                Image(systemName: "chevron.right")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(entry.enabled ? ChromeColours.textFaint : ChromeColours.buttonOffBorder)
            }
            .padding(12)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!entry.enabled)
        .accessibilityHint(entry.reason ?? "")
        .accessibilityIdentifier("radio.\(entry.id)")
    }
}
