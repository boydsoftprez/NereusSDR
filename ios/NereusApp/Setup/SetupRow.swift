// NereusSDR for iOS: one row of the Setup tree: a category or page name, the line under it, and its mark
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// One row of the tree: a name, the line under it, and its mark.
struct SetupRow: View {
    let title: String
    let detail: String?
    let tag: SetupTag
    /// A page this Core cannot open: greyed, its reason in full under it.
    var unavailable = false

    var body: some View {
        HStack(spacing: 10) {
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .font(.body.weight(.semibold))
                    .foregroundStyle(unavailable ? .secondary : .primary)
                if let detail {
                    Text(detail)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                        .lineLimit(unavailable ? nil : 1)
                        .fixedSize(horizontal: false, vertical: unavailable)
                }
            }
            Spacer(minLength: 8)
            SetupTagBadge(tag: tag)
                .opacity(unavailable ? 0.5 : 1)
        }
        .accessibilityElement(children: .combine)
        .accessibilityAddTraits(unavailable ? .isStaticText : [])
    }
}
