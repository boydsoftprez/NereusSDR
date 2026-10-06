// NereusSDR for iOS: tests for the frequency value type
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusModels

@Suite struct FrequencyTests {
    @Test func megahertzTextHasSixDecimals() {
        #expect(Frequency(hz: 14_074_000).megahertzText == "14.074000")
        #expect(Frequency(hz: 7_000_001).megahertzText == "7.000001")
        #expect(Frequency(hz: 0).megahertzText == "0.000000")
        #expect(Frequency(hz: 1_296_100_050).megahertzText == "1296.100050")
    }

    @Test func ordersByHertz() {
        let low = Frequency(hz: 7_074_000)
        let high = Frequency(hz: 14_074_000)
        #expect(low < high)
        #expect(!(high < low))
        #expect([high, low].sorted() == [low, high])
        #expect(Frequency(hz: 14_074_000) == high)
    }

    @Test func hertzRoundTrips() {
        for value: Int64 in [0, 1, 14_074_000, 10_489_550_000, Int64.max] {
            #expect(Frequency(hz: value).hz == value)
        }
        var frequency = Frequency(hz: 3_573_000)
        frequency.hz = 3_574_000
        #expect(frequency.hz == 3_574_000)
    }
}
