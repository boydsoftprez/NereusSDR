// NereusSDR for iOS: the app's five tabs, in their order on the tab bar
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The tab bar's tabs, left to right (spec section 5.1 item 2).
enum AppTab: String, CaseIterable, Identifiable, Sendable {
    case panadapter
    case modes
    case tools
    case radio
    case setup

    var id: String { rawValue }

    /// The tab's name on the tab bar.
    var title: String {
        switch self {
        case .panadapter:
            return "Panadapter"
        case .modes:
            return "Modes"
        case .tools:
            return "Tools"
        case .radio:
            return "Radio"
        case .setup:
            return "Setup"
        }
    }
}
