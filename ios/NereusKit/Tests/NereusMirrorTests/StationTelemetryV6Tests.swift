// NereusSDR for iOS: version 6 Core radio diagnostics decoding
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import Testing
@testable import NereusMirror

@Suite struct StationTelemetryV6Tests {
    private func sample(_ sequence: Int = 1, _ elapsed: Int = 1_000,
                        radio: [String: LinkJSON] = ["connected": .bool(true)]) -> LinkMessage.StationMetrics {
        LinkMessage.StationMetrics(payload: ["sequence": .number(Double(sequence)),
                                             "sampledElapsedMs": .number(Double(elapsed)),
                                             "radio": .object(radio)])
    }

    private func adc(_ index: Double, _ count: Double, status: Double? = nil,
                     overloaded: Bool? = nil, last: Double? = nil) -> LinkJSON {
        var entry: [String: LinkJSON] = ["adc": .number(index), "eventsSinceConnection": .number(count)]
        if let status { entry["statusAgeMs"] = .number(status) }
        if let overloaded { entry["overloaded"] = .bool(overloaded) }
        if let last { entry["lastOverloadAgeMs"] = .number(last) }
        return .object(entry)
    }

    @Test func exactCoreShapeAndNegotiationGates() throws {
        let radio: [String: LinkJSON] = [
            "connected": .bool(true), "connectionAgeMs": .number(123_456),
            "radioUdpBasePort": .number(42_000),
            "adcOverloads": .array([adc(0, 0, status: 25, overloaded: false),
                                    adc(1, 2, status: 100, overloaded: true, last: 100),
                                    adc(2, 3, status: 4_100, last: 4_200)]),
        ]
        var decoder = StationTelemetryDecoder()
        let read = try #require(decoder.decode(sample(radio: radio), agreedMinor: 11, telemetryVersion: 6)?.radio)
        #expect(read.connectionAgeMs == 123_456)
        #expect(read.radioUdpBasePort == 42_000)
        let entries = try #require(read.adcOverloads)
        #expect(entries.count == 3)
        #expect(entries[0].adc == 0 && entries[0].eventsSinceConnection == 0 && entries[0].overloaded == false)
        #expect(entries[1].overloaded == true && entries[1].lastOverloadAgeMs == 100)
        #expect(entries[2].statusAgeMs == 4_100 && entries[2].overloaded == nil)
        for (minor, version) in [(UInt16(10), Int64(6)), (11, 5), (11, 4), (11, 3), (10, 1)] {
            var older = StationTelemetryDecoder()
            let result = try #require(older.decode(sample(radio: radio), agreedMinor: minor,
                                                   telemetryVersion: version)?.radio)
            #expect(result.connectionAgeMs == nil && result.radioUdpBasePort == nil && result.adcOverloads == nil)
            let legacy = sample(2, 2_000, radio: ["connected": .bool(true),
                                                      "connectionAgeMs": .number(-1),
                                                      "adcOverloads": .string("unknown future field")])
            #expect(older.decode(legacy, agreedMinor: minor, telemetryVersion: version) != nil)
        }
    }

    @Test func validBoundsAndUnknownKeysRemainAvailable() throws {
        var decoder = StationTelemetryDecoder()
        let radio: [String: LinkJSON] = ["connected": .bool(true),
                                         "connectionAgeMs": .number(9_007_199_254_740_991),
                                         "radioUdpBasePort": .number(65_535),
                                         "adcOverloads": .array([adc(0, 9_007_199_254_740_991,
                                                                     status: 3_000, overloaded: false,
                                                                     last: 3_000)]),
                                         "futureRadioField": .string("ignored")]
        let read = try #require(decoder.decode(sample(radio: radio), agreedMinor: 11, telemetryVersion: 6)?.radio)
        #expect(read.connectionAgeMs == 9_007_199_254_740_991)
        #expect(read.radioUdpBasePort == 65_535)
        #expect(read.adcOverloads?.first?.eventsSinceConnection == 9_007_199_254_740_991)
        #expect(read.adcOverloads?.first?.overloaded == false)
    }

    @Test func absenceEmptyAndDisconnectedDiffer() throws {
        var decoder = StationTelemetryDecoder()
        let absent = try #require(decoder.decode(sample(), agreedMinor: 11, telemetryVersion: 6)?.radio)
        #expect(absent.connectionAgeMs == nil && absent.radioUdpBasePort == nil && absent.adcOverloads == nil)
        let empty = try #require(decoder.decode(sample(2, 2_000, radio: ["connected": .bool(true),
                                                    "adcOverloads": .array([])]),
                                                agreedMinor: 11, telemetryVersion: 6)?.radio)
        #expect(empty.adcOverloads == [])
        #expect(decoder.decode(sample(3, 3_000, radio: ["connected": .bool(false),
                                                     "adcOverloads": .array([])]),
                               agreedMinor: 11, telemetryVersion: 6) == nil)
        #expect(decoder.decode(sample(3, 3_000, radio: ["connectionAgeMs": .number(1)]),
                               agreedMinor: 11, telemetryVersion: 6) == nil)
        let disconnected = try #require(decoder.decode(sample(3, 3_000, radio: ["connected": .bool(false)]),
                                                     agreedMinor: 11, telemetryVersion: 6)?.radio)
        #expect(disconnected.adcOverloads == nil && disconnected.connectionAgeMs == nil)
    }

    @Test func invalidPresentV6DoesNotPoisonOrder() throws {
        let clear = adc(0, 0, status: 3_000, overloaded: false)
        let bad: [[String: LinkJSON]] = [
            ["connectionAgeMs": .number(-1)], ["connectionAgeMs": .number(1.5)],
            ["connectionAgeMs": .number(9_007_199_254_740_992)], ["connectionAgeMs": .string("1")],
            ["connectionAgeMs": .null],
            ["radioUdpBasePort": .number(0)], ["radioUdpBasePort": .number(65_536)],
            ["radioUdpBasePort": .number(1.5)], ["radioUdpBasePort": .string("42000")],
            ["radioUdpBasePort": .null],
            ["adcOverloads": .bool(false)], ["adcOverloads": .array([clear, clear])],
            ["adcOverloads": .null],
            ["adcOverloads": .array([clear, adc(1, 0), adc(2, 0), adc(0, 0)])],
            ["adcOverloads": .array([.number(1)])],
            ["adcOverloads": .array([.object(["adc": .number(0)])])],
            ["adcOverloads": .array([adc(3, 0)])], ["adcOverloads": .array([adc(0.5, 0)])],
            ["adcOverloads": .array([adc(0, -1)])], ["adcOverloads": .array([adc(0, 1.5)])],
            ["adcOverloads": .array([adc(0, 9_007_199_254_740_992)])],
            ["adcOverloads": .array([adc(0, 0, status: -1)])],
            ["adcOverloads": .array([adc(0, 1, status: 0, last: -1)])],
            ["adcOverloads": .array([adc(0, 1, overloaded: true)])],
            ["adcOverloads": .array([adc(0, 1, status: 3_001, overloaded: true)])],
            ["adcOverloads": .array([adc(0, 0, status: 1, overloaded: true)])],
            ["adcOverloads": .array([adc(0, 0, last: 1)])],
            ["adcOverloads": .array([adc(0, 1, status: 100, last: 50)])],
            ["adcOverloads": .array([.object(["adc": .number(0), "eventsSinceConnection": .number(0),
                                                      "statusAgeMs": .number(1), "overloaded": .number(0)])])],
        ]
        for fields in bad {
            var decoder = StationTelemetryDecoder()
            #expect(decoder.decode(sample(), agreedMinor: 11, telemetryVersion: 6) != nil)
            let radio = ["connected": LinkJSON.bool(true)].merging(fields) { _, new in new }
            #expect(decoder.decode(sample(9, 9_000, radio: radio), agreedMinor: 11, telemetryVersion: 6) == nil,
                    "\(fields)")
            #expect(decoder.decode(sample(2, 2_000), agreedMinor: 11, telemetryVersion: 6) != nil)
        }
    }

    @Test func staleStatusPreservesHistoryAndResetStartsNewSession() throws {
        var decoder = StationTelemetryDecoder()
        let stale = sample(7, 7_000, radio: ["connected": .bool(true),
                                               "adcOverloads": .array([adc(2, 3, status: 4_100, last: 4_200)])])
        let entry = try #require(decoder.decode(stale, agreedMinor: 11, telemetryVersion: 6)?.radio?.adcOverloads?.first)
        #expect(entry.overloaded == nil && entry.eventsSinceConnection == 3 && entry.lastOverloadAgeMs == 4_200)
        #expect(decoder.decode(sample(6, 7_000), agreedMinor: 11, telemetryVersion: 6) == nil)
        #expect(decoder.decode(sample(8, 6_999), agreedMinor: 11, telemetryVersion: 6) == nil)
        decoder.reset()
        #expect(decoder.decode(sample(1, 100), agreedMinor: 11, telemetryVersion: 6) != nil)
    }
}
