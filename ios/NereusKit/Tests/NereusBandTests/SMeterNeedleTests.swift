// NereusSDR for iOS: the S-meter's needle rises quickly and falls slowly, then settles
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusBand

/// D86, D83: 45 ms rising, 180 ms falling, settled within a thousandth.
@Suite struct SMeterNeedleTests {
    @Test("one time constant covers 63% of the way, rising in 45 ms and falling in 180 ms")
    func ballistics() {
        var needle = SMeterNeedle()
        needle.step(toward: 1, seconds: 0.045)
        #expect(abs(needle.fraction - (1 - exp(-1))) < 1e-9)

        var falling = SMeterNeedle(fraction: 1)
        falling.step(toward: 0, seconds: 0.045)
        #expect(falling.fraction > 0.7)
        falling.step(toward: 0, seconds: 0.135)
        #expect(abs(falling.fraction - exp(-1)) < 1e-9)
    }

    @Test("within a thousandth the needle settles on its target")
    func settles() {
        var needle = SMeterNeedle(fraction: 0.5)
        #expect(!needle.settled(on: 0.6))
        needle.step(toward: 0.5005, seconds: 0.01)
        #expect(needle.fraction == 0.5005)
        #expect(needle.settled(on: 0.5005))
    }
}
