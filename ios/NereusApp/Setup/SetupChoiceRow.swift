// NereusSDR for iOS: one choice in a Setup list: a title, a line under it, and a tick on the chosen one
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// A choice among several (the board's ticked rows): the whole row is the
/// button. A choice that can't be made here is greyed and does nothing;
/// its section says why.
struct SetupChoiceRow: View {
    let title: String
    let detail: String
    let chosen: Bool
    var enabled = true
    let id: String
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(title)
                        .font(.body.weight(.semibold))
                        .foregroundStyle(enabled ? Color.primary : Color.secondary)
                    Text(detail)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                Spacer(minLength: 0)
                Image(systemName: "checkmark")
                    .foregroundStyle(enabled ? ChromeColours.accent : Color.secondary)
                    .opacity(chosen ? 1 : 0)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityAddTraits(chosen ? .isSelected : [])
        .accessibilityIdentifier(id)
    }
}
