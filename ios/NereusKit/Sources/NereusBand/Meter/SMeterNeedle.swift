// NereusSDR for iOS: how the S-meter's needle moves: quick up, slower down, as the desktop's needle does
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The needle's swing (D86, D83: the desktop's timings): it closes on its
/// target exponentially, with a 45 ms time constant rising and 180 ms
/// falling, and settles on the target once within a thousandth of it.
public struct SMeterNeedle: Equatable, Sendable {
    public static let riseSeconds = 0.045
    public static let fallSeconds = 0.180
    public static let settle = 0.001
    /// How often the meter redraws while the needle moves: 30 times a second.
    public static let frameSeconds = 0.033

    /// Where the needle is, 0 to 1 along the scale.
    public private(set) var fraction: Double

    public init(fraction: Double = 0) {
        self.fraction = fraction
    }

    /// Whether the needle rests on `target`.
    public func settled(on target: Double) -> Bool {
        abs(target - fraction) <= Self.settle
    }

    /// Moves the needle `seconds` toward `target`.
    public mutating func step(toward target: Double, seconds: Double) {
        let delta = target - fraction
        guard abs(delta) > Self.settle, seconds > 0 else {
            fraction = target
            return
        }
        let constant = delta >= 0 ? Self.riseSeconds : Self.fallSeconds
        fraction += delta * (1 - exp(-seconds / constant))
    }
}
