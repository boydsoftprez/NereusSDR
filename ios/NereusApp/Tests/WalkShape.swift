// NereusSDR for iOS: the phone shapes the recording screens are drawn at for the screen walk: small and large, upright and sideways, two text sizes
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI
import UIKit

/// One phone shape for the screen walk of the recording screens (PR 808):
/// the smallest iPhone (an iPhone SE's 375 by 667 points) and the largest
/// (an iPhone 17 Pro Max's 440 by 956), upright and sideways, at the
/// default text size and one step larger. Shots taken at a shape are named
/// `<picture>-<shape>.png`, for example `dsr-phone-past-small-upright-larger.png`.
struct WalkShape: Sendable, CustomStringConvertible {
    let small: Bool
    let sideways: Bool
    let larger: Bool

    static let all: [WalkShape] = [false, true].flatMap { small in
        [false, true].flatMap { sideways in
            [false, true].map { larger in WalkShape(small: small, sideways: sideways, larger: larger) }
        }
    }

    var description: String {
        "\(small ? "small" : "big")-\(sideways ? "sideways" : "upright")-\(larger ? "larger" : "default")"
    }

    /// The whole screen, in points.
    var screen: CGSize {
        let upright = small ? CGSize(width: 375, height: 667) : CGSize(width: 440, height: 956)
        return sideways ? CGSize(width: upright.height, height: upright.width) : upright
    }

    var type: DynamicTypeSize { larger ? .xLarge : .large }

    /// The screen's safe area as the phone has it: the Pro Max's island and
    /// home bar; the SE's status bar upright and nothing sideways.
    var safeArea: UIEdgeInsets {
        switch (small, sideways) {
        case (true, false): return UIEdgeInsets(top: 20, left: 0, bottom: 0, right: 0)
        case (true, true): return .zero
        case (false, false): return UIEdgeInsets(top: 62, left: 0, bottom: 34, right: 0)
        case (false, true): return UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        }
    }

    /// The pan as the main screen leaves it: the screen less its safe area,
    /// the tab bar and the readouts and controls above the band. An
    /// approximation for drawing the band alone; the main screen's own
    /// shots (DialShotTests) draw the real arrangement.
    var pan: CGSize {
        let inset = safeArea
        let width = screen.width - inset.left - inset.right
        let height = screen.height - inset.top - inset.bottom
        return sideways ? CGSize(width: width, height: height - 56)
            : CGSize(width: width, height: height - 56 - 180)
    }
}
