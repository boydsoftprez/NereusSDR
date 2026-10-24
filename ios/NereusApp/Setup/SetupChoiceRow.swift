// NereusSDR for iOS: one choice in a Setup list: a title, a line under it, and a tick on the chosen one
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// A choice among several (the board's ticked rows): the whole row is the
/// button, and the picked row is in the glass with its tick kept (spec
/// section 7.1). A choice that can't be made here is greyed at 0.4 and does
/// nothing; its section says why.
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
                        .foregroundStyle(.primary)
                    Text(detail)
                        .font(.footnote)
                        .foregroundStyle(WorkspacePalette.textSecondary)
                }
                Spacer(minLength: 0)
                SetupCheckMark(chosen: chosen)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .setupPicked(chosen)
        .disabled(!enabled)
        .setupGreyed(!enabled)
        .accessibilityAddTraits(chosen ? .isSelected : [])
        .accessibilityIdentifier(id)
    }
}
