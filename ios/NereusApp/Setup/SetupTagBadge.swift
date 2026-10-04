// NereusSDR for iOS: the small mark beside a Setup category, page or group saying where its settings live
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The board's mark: "Core" in the accent colour, "This phone" and "Both"
/// in grey, small and monospaced on a dark chip.
struct SetupTagBadge: View {
    let tag: SetupTag

    var body: some View {
        Text(tag.label)
            .font(.caption2.monospaced().weight(.semibold))
            .lineLimit(1)
            .minimumScaleFactor(0.5)
            .foregroundStyle(tag == .core ? ChromeColours.accent : Color.secondary)
            .padding(.horizontal, 5)
            .padding(.vertical, 1)
            .background(Color.primary.opacity(0.08), in: RoundedRectangle(cornerRadius: 3))
            .accessibilityLabel(tag.label)
    }
}
