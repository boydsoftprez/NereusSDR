// NereusSDR for iOS: reading the Core's freedvStations records, tolerant of missing and newer fields
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-26, link document section 7.7: a `freedvStations` record reads
/// into a station with every one of its fields; a missing or odd field reads
/// as "not known", and the optional `band` reads only as a whole number.
@Suite struct FreeDVStationTests {
    private func record(_ fields: [String: LinkJSON], id: String = "sid-1") -> LinkMessage.RecordBatch.Record {
        LinkMessage.RecordBatch.Record(id: id, fields: fields)
    }

    @Test("every field of a record reads into the station")
    func everyField() throws {
        let station = FreeDVStation(record: record([
            "callsign": .string("K4XYZ"), "gridSquare": .string("EM73"), "distanceKm": .number(812.4),
            "headingDeg": .number(45), "headingCardinal": .string("NE"), "version": .string("freedv-gui 2.0"),
            "frequencyHz": .number(7_177_000), "txMode": .string("RADE"), "status": .string("Active"),
            "userMessage": .string("QRV"), "lastTxUtc": .string("2026-09-28T19:31:00Z"),
            "lastRxCallsign": .string("W1ABC"), "lastRxMode": .string("RADE"), "snrDb": .number(6),
            "lastUpdateUtc": .string("2026-09-28T19:32:10.250Z"), "transmitting": .bool(false),
            "receivingFrom": .string("W1ABC"), "messageChangedAtMs": .number(1_790_000_000_000),
            "lastRxUtc": .string("2026-09-28T19:32:00Z"),
        ]))
        #expect(station.id == "sid-1" && station.callsign == "K4XYZ" && station.gridSquare == "EM73")
        #expect(station.distanceKm == 812.4 && station.headingDeg == 45 && station.headingCardinal == "NE")
        #expect(station.hasBearing)
        #expect(station.version == "freedv-gui 2.0" && station.frequencyHz == 7_177_000 && station.txMode == "RADE")
        #expect(station.status == "Active" && station.userMessage == "QRV")
        #expect(station.lastTx == FreeDVStation.time("2026-09-28T19:31:00Z"))
        #expect(station.lastRxCallsign == "W1ABC" && station.lastRxMode == "RADE" && station.snrDb == 6)
        #expect(station.lastUpdate != nil && station.lastRx != nil)
        #expect(!station.transmitting && station.receivingFrom == "W1ABC")
        #expect(station.messageChangedAtMs == 1_790_000_000_000)
        #expect(station.band == nil)
    }

    @Test("missing and odd fields read as not known, never as a failure")
    func tolerant() {
        let station = FreeDVStation(record: record([
            "callsign": .number(12), "distanceKm": .number(-4), "snrDb": .number(-99), "frequencyHz": .string("7.1"),
            "lastTxUtc": .string(""), "lastUpdateUtc": .string("yesterday"), "transmitting": .number(1),
            "messageChangedAtMs": .number(-5), "unheardOf": .string("from a newer Core"),
        ]))
        #expect(station.callsign == "" && station.distanceKm == 0 && station.snrDb == nil)
        #expect(station.frequencyHz == 0 && station.lastTx == nil && station.lastUpdate == nil)
        #expect(!station.transmitting && station.messageChangedAtMs == 0 && !station.hasBearing)
    }

    @Test("the band reads only as a whole number the catalogue could name")
    func band() {
        #expect(FreeDVStation(record: record(["band": .number(3)])).band == 3)
        #expect(FreeDVStation(record: record(["band": .number(13)])).band == 13)
        #expect(FreeDVStation(record: record(["band": .number(3.5)])).band == nil)
        #expect(FreeDVStation(record: record(["band": .number(-1)])).band == nil)
        #expect(FreeDVStation(record: record(["band": .string("40m")])).band == nil)
        #expect(FreeDVStation(record: record([:])).band == nil)
    }

    /// The suite's `control-station-freedv-stations-band` batch, from a
    /// Core at `stationFreedvVersion` 2: each station with a known
    /// frequency names its band as the catalogue numbers it (2 m is GEN,
    /// 11), and one with no frequency names none.
    @Test("the Core's band batch from the suite names each station's band")
    func theCoresBandBatch() throws {
        let fixture = try #require(try LinkFixtureLoader.manifest().first {
            $0.id == "control-station-freedv-stations-band"
        })
        let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(fixture.file))
        let message = try LinkCodec.decode(try LinkJSON(foundation: try #require(object["wire"])).compactText)
        guard case .recordBatch(let batch) = message else {
            Issue.record("not a record.batch")
            return
        }
        #expect(batch.stream == FreeDVStation.streamName)
        let stations = batch.upserts.map(FreeDVStation.init(record:))
        let bands = Dictionary(uniqueKeysWithValues: stations.map { ($0.callsign, $0.band) })
        #expect(bands["W1AW"] == .some(5))
        #expect(bands["VK5DGR"] == .some(3))
        #expect(bands["K6AQ"] == .some(11))
        #expect(bands["G4ABC"] == .some(nil))
        #expect(stations.first { $0.callsign == "G4ABC" }?.frequencyHz == 0)
    }

    @MainActor
    @Test("the station list holds the Core's thousand records")
    func capacity() {
        #expect(RecordStreamClient.capacity(of: FreeDVStation.streamName) == 1000)
        #expect(RecordStreamClient.capacity(of: "spots") == 500)
        #expect(RecordStreamClient.capacity(of: "spotConsole:freedvReporter") == 200)
    }
}
