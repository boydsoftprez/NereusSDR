// NereusSDR for iOS: the S-meter's faces, the desktop meter menu's Meter Face
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The S-meter's face (D86): six vintage panel-meter faces and the flat
/// Classic face, in the desktop menu's order.
public enum SMeterFace: String, Equatable, Sendable, Codable, CaseIterable {
    case agedCream
    case vuAmber
    case collinsWhite
    case blackface
    case carbon
    case ice
    case classic

    /// The vintage faces, in the menu's order.
    public static let vintage: [SMeterFace] = [.agedCream, .vuAmber, .collinsWhite, .blackface, .carbon, .ice]

    /// The menu's words, the desktop's.
    public var label: String {
        theme?.name ?? "Classic (flat)"
    }

    /// A vintage face's colours; nil for Classic.
    public var theme: VintageFaceTheme? {
        switch self {
        case .agedCream:
            return .agedCream
        case .vuAmber:
            return .vuAmber
        case .collinsWhite:
            return .collinsWhite
        case .blackface:
            return .blackface
        case .carbon:
            return .carbon
        case .ice:
            return .ice
        case .classic:
            return nil
        }
    }
}
