// NereusSDR for iOS: the SSB Mod Monitor's record as the Core sends it on txSsbModulation and txSsbModulationFeedback
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMirror

/// Link document sections 6.3, 7.7, 9.1: the Core's SSB Mod Monitor readings
/// arrive as one record, `id` "0"; each field is read as sent, crest may be
/// absent, the trace is non-negative little-endian int16 tenths in base64, and
/// a record with a field missing, mistyped or out of range is refused.
@MainActor
@Suite struct SsbModulationReadingTests {
    static func pack(_ tenths: [Int16]) -> String {
        var bytes: [UInt8] = []
        for value in tenths {
            let raw = UInt16(bitPattern: value)
            bytes.append(UInt8(raw & 0xff))
            bytes.append(UInt8(raw >> 8))
        }
        return Data(bytes).base64EncodedString()
    }

    static func fields() -> [String: LinkJSON] {
        [
            "atMs": .number(1_790_000_000_123),
            "peakPct": .number(61.5),
            "avgPct": .number(22.25),
            "peakHoldPct": .number(70.8),
            "peakDbfs": .number(-4.2),
            "avgDbfs": .number(-13.1),
            "peakHoldDbfs": .number(-3),
            "crestDb": .number(9.1),
            "signalPresent": .bool(true),
            "scopeRateHz": .number(1500),
            "scopePctTenths": .string(pack([615, 0, 5])),
        ]
    }

    private func read(_ fields: [String: LinkJSON]) -> SsbModulationReading? {
        SsbModulationReading(record: .init(id: "0", fields: fields))
    }

    @Test func everyFieldIsReadAsSent() throws {
        let reading = try #require(read(Self.fields()))
        #expect(reading == SsbModulationReading(
            atMs: 1_790_000_000_123, peakPct: 61.5, avgPct: 22.25, peakHoldPct: 70.8, peakDbfs: -4.2, avgDbfs: -13.1,
            peakHoldDbfs: -3, crestDb: 9.1, signalPresent: true, scopeRateHz: 1500, scopePct: [61.5, 0, 0.5]))
    }

    @Test func crestMayBeAbsentAndThePhoneNeverWorksItOut() throws {
        var fields = Self.fields()
        fields["crestDb"] = nil
        let reading = try #require(read(fields))
        #expect(reading.crestDb == nil)
    }

    @Test func aMissingRequiredFieldRefusesTheRecord() {
        for name in ["atMs", "peakPct", "avgPct", "peakHoldPct", "peakDbfs", "avgDbfs", "peakHoldDbfs", "signalPresent",
                     "scopeRateHz", "scopePctTenths"] {
            var fields = Self.fields()
            fields[name] = nil
            #expect(read(fields) == nil, "without \(name)")
        }
    }

    @Test func aMistypedOrOutOfRangeFieldRefusesTheRecord() {
        let bad: [(String, LinkJSON)] = [
            ("atMs", .number(-1)), ("atMs", .string("1")),
            ("peakPct", .number(-0.1)), ("peakPct", .number(3276.8)), ("peakPct", .string("1")),
            ("avgPct", .number(-1)), ("avgPct", .number(4000)),
            ("peakHoldPct", .number(-1)), ("peakHoldPct", .number(4000)),
            ("peakDbfs", .number(-120.1)), ("peakDbfs", .number(71.1)),
            ("avgDbfs", .number(-121)), ("avgDbfs", .number(72)),
            ("peakHoldDbfs", .number(-121)), ("peakHoldDbfs", .number(72)),
            ("crestDb", .number(200.5)), ("crestDb", .number(-200.5)), ("crestDb", .string("3")),
            ("scopeRateHz", .number(1500.5)), ("scopeRateHz", .number(1_536_001)), ("scopeRateHz", .number(-1)),
            ("signalPresent", .number(1)), ("scopePctTenths", .number(1)),
            ("scopePctTenths", .string("%%%")),
        ]
        for (name, value) in bad {
            var fields = Self.fields()
            fields[name] = value
            #expect(read(fields) == nil, "\(name) \(value)")
        }
    }

    @Test func theTraceIsReadAsTheCorePacksItOrRefused() throws {
        #expect(SsbModulationReading.scopePercent(Self.pack([1200, 32767])) == [120, 3276.7])
        #expect(SsbModulationReading.scopePercent("") == [])
        #expect(SsbModulationReading.scopePercent(Self.pack([5, -1])) == nil)
        #expect(SsbModulationReading.scopePercent(Data([0x10, 0x00, 0x7f]).base64EncodedString()) == nil)
        #expect(SsbModulationReading.scopePercent("not base64!") == nil)
        let full = [Int16](repeating: 1, count: 512)
        #expect(SsbModulationReading.scopePercent(Self.pack(full))?.count == 512)
        #expect(SsbModulationReading.scopePercent(Self.pack(full + [1])) == nil)
    }

    @Test func theLinkNamesAndTheStreamsHoldOneRecord() {
        #expect(SsbModulationReading.capabilityName == "txSsbModMonitorVersion")
        #expect(SsbModulationReading.resetVerb == "txSsbModMonitor.reset")
        #expect(SsbModulationReading.Source.txIq.stream == "txSsbModulation")
        #expect(SsbModulationReading.Source.paFeedback.stream == "txSsbModulationFeedback")
        #expect(SsbModulationReading.Source.txIq.rawValue == 0)
        #expect(SsbModulationReading.Source.paFeedback.rawValue == 1)
        #expect(RecordStreamClient.capacity(of: "txSsbModulation") == 1)
        #expect(RecordStreamClient.capacity(of: "txSsbModulationFeedback") == 1)
    }
}
