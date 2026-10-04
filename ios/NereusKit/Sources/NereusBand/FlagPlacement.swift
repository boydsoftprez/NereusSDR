// NereusSDR for iOS: where one slice's flag sits on the band, full or folded to a one-line tag
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics

/// One slice's flag on the band (D9), in the band's points from its top
/// left: the full flag, or the one-line tag (letter, frequency, TX badge)
/// it folds to when the full flag would land on another flag.
public enum FlagPlacement: Equatable, Sendable {
    case full(CGRect)
    case folded(CGRect)

    public var rect: CGRect {
        switch self {
        case .full(let rect), .folded(let rect):
            return rect
        }
    }

    public var isFolded: Bool {
        if case .folded = self {
            return true
        }
        return false
    }
}
