// NereusSDR for iOS: which layout the main screen takes: the phone's, the iPad's applet column, or its front panel
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import SwiftUI

/// The main screen's arrangement (spec section 5.6, D30, D31, R-IOS-24).
/// A phone, or an iPad sharing its screen with another app in a narrow
/// window, takes the phone's layout. A full iPad window on its side takes
/// the applet column, like the desktop's main window: the band on the left
/// and the S-meter, RX and TX in one column on the right. Upright it takes
/// the front panel: the band keeps the whole width, with the S-meter, RX and
/// TX in three columns below it.
enum IPadLayout: Equatable {
    case phone
    case column
    case frontPanel

    /// The applet column's width on its side (the board's column).
    static let columnWidth: CGFloat = 320
    /// Upright, the share of the room under the toolbar the three columns
    /// take; the band keeps the rest (the board's front panel).
    static let frontPanelShare: CGFloat = 0.45
    /// The iPad's layouts need an iPad's room: a window whose shorter side
    /// is at least this, and a regular width. Every iPhone is narrower, and
    /// so is an iPad window shared narrowly with another app.
    static let minimumShorterSide: CGFloat = 600

    /// The arrangement for a screen of `size` (the room under the status
    /// bar, over the tab bar) with the window's width class.
    static func arrangement(horizontal: UserInterfaceSizeClass?, size: CGSize) -> IPadLayout {
        guard horizontal == .regular, min(size.width, size.height) >= minimumShorterSide else {
            return .phone
        }
        return size.width > size.height ? .column : .frontPanel
    }

    /// The band's width in a screen `width` wide: on its side, less the
    /// column while it shows.
    func bandWidth(screenWidth width: CGFloat, columnShown: Bool) -> CGFloat {
        switch self {
        case .column:
            return columnShown ? max(width - Self.columnWidth, 0) : width
        case .phone, .frontPanel:
            return width
        }
    }

    /// Upright, the three columns' height under a band and toolbar `height` tall.
    static func frontPanelHeight(underToolbar height: CGFloat) -> CGFloat {
        (max(height, 0) * frontPanelShare).rounded()
    }

    /// The toolbar's corner button that hides the column shows only on its side.
    var hasColumnButton: Bool {
        self == .column
    }

    /// The Core's name sits in the toolbar's middle on an iPad either way up.
    var isIPad: Bool {
        self != .phone
    }
}
