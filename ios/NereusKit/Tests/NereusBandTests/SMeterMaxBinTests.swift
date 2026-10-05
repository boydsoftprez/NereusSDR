// NereusSDR for iOS: Max Bin reads the active slice's passband on its displayed pan trace
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusBand

@Suite struct SMeterMaxBinTests {
    @Test("the strongest displayed point inside the slice passband wins")
    func passband() {
        let trace: [Float] = [-120, -90, -45, -82, -30, -110]
        #expect(SMeterMaxBin.measure(trace: trace, centerHz: 100, spanHz: 50,
                                     lowHz: 91, highHz: 107) == -45)
        #expect(SMeterMaxBin.measure(trace: trace, centerHz: 100, spanHz: 50,
                                     lowHz: 113, highHz: 122) == -30)
    }

    @Test("off-span, missing and malformed displayed traces have no reading")
    func noReading() {
        let trace: [Float] = [-100, -70, -90]
        #expect(SMeterMaxBin.measure(trace: [], centerHz: 100, spanHz: 50, lowHz: 95, highHz: 105) == nil)
        #expect(SMeterMaxBin.measure(trace: [-70], centerHz: 100, spanHz: 50, lowHz: 95, highHz: 105) == nil)
        #expect(SMeterMaxBin.measure(trace: trace, centerHz: 100, spanHz: 0, lowHz: 95, highHz: 105) == nil)
        #expect(SMeterMaxBin.measure(trace: trace, centerHz: 100, spanHz: 50, lowHz: 126, highHz: 130) == nil)
        #expect(SMeterMaxBin.measure(trace: trace, centerHz: 100, spanHz: 50, lowHz: 105, highHz: 105) == nil)
        #expect(SMeterMaxBin.measure(trace: trace, centerHz: .nan, spanHz: 50, lowHz: 95, highHz: 105) == nil)
        #expect(SMeterMaxBin.measure(trace: trace, centerHz: 100, spanHz: 50, lowHz: .nan, highHz: 105) == nil)
        #expect(SMeterMaxBin.measure(trace: [-100, .nan, -90], centerHz: 100, spanHz: 50,
                                     lowHz: 95, highHz: 105) == nil)
    }
}
