// NereusSDR for iOS: the band's width with the phone upright, for sizing a dial the same way sideways
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

extension EnvironmentValues {
    /// How wide the band is with the phone upright: the window's shorter
    /// side. Sideways the thumbwheel keeps the width it has upright rather
    /// than stretching across the band (``DialLayer/wheelFrame(waterfall:sideways:uprightWidth:trailingInset:)``).
    /// Nil where no window is known (tests that draw a screen alone), which
    /// takes the band's own width.
    @Entry var uprightBandWidth: CGFloat?
}

extension GeometryProxy {
    /// The window's shorter side, from a reader that fills the window less
    /// its safe area: the band's width with the phone upright.
    var uprightBandWidth: CGFloat {
        let insets = safeAreaInsets
        return min(size.width + insets.leading + insets.trailing, size.height + insets.top + insets.bottom)
    }
}
