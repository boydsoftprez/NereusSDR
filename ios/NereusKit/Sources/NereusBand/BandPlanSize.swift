// NereusSDR for iOS: the band-plan strip's sizes, the desktop's View > Band Plan sizes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics

/// The band-plan strip's size (D79, spec section 5.1 item 19): the sizes
/// the desktop's View > Band Plan menu offers, by its label's point size.
/// The strip is its label's size plus 4 points high, as the desktop's is.
public enum BandPlanSize: String, Equatable, Sendable, Codable, CaseIterable {
    case off
    case small
    case medium
    case large
    case huge

    /// The label's size in points; 0 for Off. The desktop's menu sizes
    /// (`MainWindow.cpp`, View > Band Plan: 0, 6, 10, 12 and 16 points).
    public var labelPoints: CGFloat {
        switch self {
        case .off:
            return 0
        case .small:
            return 6
        case .medium:
            return 10
        case .large:
            return 12
        case .huge:
            return 16
        }
    }

    /// The strip's height in points: the label's size plus 4, or 0 for Off.
    public var stripHeightPoints: CGFloat {
        self == .off ? 0 : labelPoints + 4
    }

    /// The name the Display sheet shows.
    public var label: String {
        switch self {
        case .off:
            return "Off"
        case .small:
            return "Small"
        case .medium:
            return "Medium"
        case .large:
            return "Large"
        case .huge:
            return "Huge"
        }
    }
}
