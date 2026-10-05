// NereusSDR for iOS: NereusSDR's own icon at the head of the card and the lost island
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// NereusSDR's icon as the card shows it (the board's `.appicon`), 22 points with rounded corners.
struct ActivityAppIcon: View {
    var body: some View {
        Image("ActivityIcon")
            .resizable()
            .interpolation(.high)
            .frame(width: 22, height: 22)
            .clipShape(RoundedRectangle(cornerRadius: 6))
            .accessibilityHidden(true)
    }
}
