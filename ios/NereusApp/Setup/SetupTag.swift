// NereusSDR for iOS: where a Setup category's or page's settings live: the Core, this phone, or both
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Each Setup category and page is marked with where its settings live
/// (D16, spec section 5.2 item 6): Core settings are shared by every device
/// paired with the Core, This phone settings stay on the phone, and Both
/// holds some of each.
enum SetupTag: String, Equatable, Sendable, CaseIterable {
    case core
    case thisPhone
    case both

    /// The mark's words.
    var label: String {
        switch self {
        case .core:
            return "Core"
        case .thisPhone:
            return "This phone"
        case .both:
            return "Both"
        }
    }
}
