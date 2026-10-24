// NereusSDR for iOS: one switch in a Setup list, with a line under its title
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// An on/off setting with a title and a line under it, drawn by the Setup
/// switch (``WorkspaceSwitchStyle``, W7). A switch that can't be used here
/// is greyed as a whole row at 0.4, as a number row is, showing what this
/// build does; its section or the line under it says why.
struct SetupSwitchRow: View {
    let title: String
    let detail: String
    @Binding var isOn: Bool
    var enabled = true
    let id: String

    var body: some View {
        Toggle(isOn: $isOn) {
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .font(.body.weight(.semibold))
                Text(detail)
                    .font(.footnote)
                    .foregroundStyle(WorkspacePalette.textSecondary)
            }
        }
        // The Setup switch greys the whole row at 0.4 when it cannot act.
        .disabled(!enabled)
        .accessibilityIdentifier(id)
    }
}
