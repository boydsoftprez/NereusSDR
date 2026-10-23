// NereusSDR for iOS: the small mark beside a Setup category, page or group saying where its settings live
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The board's mark, a chip rounded at 7 (spec section 7.1): "Core" in the
/// Core chip's blue on a glass edge, "This phone" and "Both" in
/// `kTextSecondary` on a `kBorder` edge, small and monospaced.
struct SetupTagBadge: View {
    let tag: SetupTag

    var body: some View {
        let core = tag == .core
        let shape = RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius, style: .continuous)
        Text(tag.label)
            .font(.caption2.monospaced().weight(.semibold))
            .lineLimit(1)
            .minimumScaleFactor(0.5)
            .foregroundStyle(core ? WorkspacePalette.tabChosen : WorkspacePalette.textSecondary)
            .padding(.horizontal, 7)
            .padding(.vertical, 3)
            .background(WorkspacePalette.glassPanelTop, in: shape)
            .overlay {
                shape.strokeBorder(core ? WorkspacePalette.glassSelBorder : WorkspacePalette.border, lineWidth: 1)
            }
            .accessibilityLabel(tag.label)
    }
}
