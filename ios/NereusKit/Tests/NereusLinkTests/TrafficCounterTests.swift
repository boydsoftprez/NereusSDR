// NereusSDR for iOS: tests of the counter of every byte the app sends to or takes from a Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import Testing

/// R-IOS-23: the data use counters read these totals.
@Suite("Traffic counter")
struct TrafficCounterTests {
    @Test("bytes in and out add up apart, and nothing or less than nothing counts for nothing")
    func counts() {
        let counter = TrafficCounter()
        counter.received(1200)
        counter.sent(80)
        counter.received(300)
        counter.received(0)
        counter.sent(-5)
        #expect(counter.reading == TrafficCounter.Totals(bytesIn: 1500, bytesOut: 80))
    }

    @Test("each kind is counted apart, the totals are their sum, and a difference never goes below nothing")
    func kinds() {
        let counter = TrafficCounter()
        counter.received(1200, as: .control)
        counter.sent(80, as: .control)
        counter.received(40_000, as: .display)
        counter.received(3_000, as: .audio)
        counter.sent(2_000, as: .audio)
        counter.sent(16, as: .transmit)
        counter.received(5)
        counter.received(0, as: .display)
        counter.sent(-5, as: .audio)
        let reading = counter.readingByKind
        #expect(reading[.control] == TrafficCounter.Totals(bytesIn: 1200, bytesOut: 80))
        #expect(reading[.display] == TrafficCounter.Totals(bytesIn: 40_000, bytesOut: 0))
        #expect(reading[.audio] == TrafficCounter.Totals(bytesIn: 3_000, bytesOut: 2_000))
        #expect(reading[.transmit] == TrafficCounter.Totals(bytesIn: 0, bytesOut: 16))
        #expect(reading[.other] == TrafficCounter.Totals(bytesIn: 5, bytesOut: 0))
        #expect(counter.reading == TrafficCounter.Totals(bytesIn: 44_205, bytesOut: 2_096))
        #expect(reading.sum == counter.reading)

        counter.received(500, as: .audio)
        let since = counter.readingByKind.since(reading)
        #expect(since[.audio] == TrafficCounter.Totals(bytesIn: 500, bytesOut: 0))
        #expect(since.sum == TrafficCounter.Totals(bytesIn: 500, bytesOut: 0))
        #expect(reading.since(counter.readingByKind).sum == TrafficCounter.Totals())
    }

    @Test("counting from many threads at once loses nothing")
    func concurrent() async {
        let counter = TrafficCounter()
        await withTaskGroup(of: Void.self) { group in
            for _ in 0..<8 {
                group.addTask {
                    for _ in 0..<1000 {
                        counter.received(2)
                        counter.sent(1)
                    }
                }
            }
        }
        #expect(counter.reading == TrafficCounter.Totals(bytesIn: 16_000, bytesOut: 8000))
    }
}
