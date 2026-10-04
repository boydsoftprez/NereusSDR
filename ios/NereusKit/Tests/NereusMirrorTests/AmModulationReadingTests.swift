// NereusSDR for iOS: the AM Mod Monitor's record as the Core sends it on txAmModulation and txAmModulationFeedback
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-13, R-IOS-18, D102 (link document sections 6.3, 7.7, 9.1): the
/// Core's AM Mod Monitor readings arrive as one record, `id` "0", on the
/// `txAmModulation` and `txAmModulationFeedback` streams; each field is read
/// as sent, the envelope trace is little-endian int16 tenths of a percent in
/// base64, and each stream holds one record.
@MainActor
@Suite struct AmModulationReadingTests {
    /// Little-endian int16 tenths, base64, as the Core packs its trace.
    static func pack(_ tenths: [Int16]) -> String {
        var bytes: [UInt8] = []
        for value in tenths {
            let raw = UInt16(bitPattern: value)
            bytes.append(UInt8(raw & 0xff))
            bytes.append(UInt8(raw >> 8))
        }
        return Data(bytes).base64EncodedString()
    }

    static func fields(scope: String = pack([1120, -940, 0, 5])) -> [String: LinkJSON] {
        [
            "atMs": .number(1_790_000_000_123),
            "posPeakPct": .number(104.4),
            "negPeakPct": .number(88.2),
            "posHoldPct": .number(112.3),
            "negHoldPct": .number(94.4),
            "carrierLevel": .number(0.49),
            "carrierDbfs": .number(-6.2),
            "carrierPresent": .bool(true),
            "carrierLow": .bool(false),
            "carrierHigh": .bool(false),
            "scopeRateHz": .number(1500),
            "scopePctTenths": .string(scope),
        ]
    }

    @Test func everyFieldIsReadAsSent() throws {
        let reading = try #require(AmModulationReading(record: .init(id: "0", fields: Self.fields())))
        #expect(reading.atMs == 1_790_000_000_123)
        #expect(reading.posPeakPct == 104.4)
        #expect(reading.negPeakPct == 88.2)
        #expect(reading.posHoldPct == 112.3)
        #expect(reading.negHoldPct == 94.4)
        #expect(reading.carrierLevel == 0.49)
        #expect(reading.carrierDbfs == -6.2)
        #expect(reading.carrierPresent)
        #expect(!reading.carrierLow)
        #expect(!reading.carrierHigh)
        #expect(reading.scopeRateHz == 1500)
        #expect(reading.scopePct == [112.0, -94.0, 0.0, 0.5])
        #expect(abs(reading.asymmetryPct - 17.9) < 1e-9)
    }

    @Test func aRecordMissingAReadingIsNotShown() {
        for name in ["posPeakPct", "negHoldPct", "carrierDbfs", "carrierPresent", "carrierLow", "carrierHigh"] {
            var fields = Self.fields()
            fields[name] = nil
            #expect(AmModulationReading(record: .init(id: "0", fields: fields)) == nil, "without \(name)")
        }
        var wrongKind = Self.fields()
        wrongKind["posHoldPct"] = .string("112")
        #expect(AmModulationReading(record: .init(id: "0", fields: wrongKind)) == nil)
    }

    @Test func theTraceIsLittleEndianTenthsAndAtMost512Points() {
        #expect(AmModulationReading.scopePercent(Self.pack([-1000, 1600, 32767, -32768]))
                == [-100, 160, 3276.7, -3276.8])
        // Text that is not base64, or an odd byte, leaves no trace or drops the odd byte.
        #expect(AmModulationReading.scopePercent("not base64!").isEmpty)
        #expect(AmModulationReading.scopePercent(Data([0x10, 0x00, 0x7f]).base64EncodedString()) == [1.6])
        #expect(AmModulationReading.scopePercent("").isEmpty)
        // Never more than the Core's 512 points; the newest are kept.
        let long = (0..<600).map { Int16($0) }
        let points = AmModulationReading.scopePercent(Self.pack(long))
        #expect(points.count == 512)
        #expect(points.first == 8.8)
        #expect(points.last == 59.9)
        // A record whose trace cannot be read still shows its readings.
        let reading = AmModulationReading(record: .init(id: "0", fields: Self.fields(scope: "%%%")))
        #expect(reading?.scopePct == [])
        #expect(reading?.posHoldPct == 112.3)
    }

    @Test func theWireNamesAreTheLinkDocuments() {
        #expect(AmModulationReading.capabilityName == "txModMonitorVersion")
        #expect(AmModulationReading.resetVerb == "txModMonitor.reset")
        #expect(AmModulationReading.feedbackReceiverKey == "ModMon/FbStream")
        #expect(AmModulationReading.Source.txIq.stream == "txAmModulation")
        #expect(AmModulationReading.Source.paFeedback.stream == "txAmModulationFeedback")
        #expect(AmModulationReading.Source.txIq.rawValue == 0)
        #expect(AmModulationReading.Source.paFeedback.rawValue == 1)
        #expect(RecordStreamClient.capacity(of: "txAmModulation") == 1)
        #expect(RecordStreamClient.capacity(of: "txAmModulationFeedback") == 1)
        #expect(SettingsScope.of(AmModulationReading.feedbackReceiverKey) == .station)
    }
}
