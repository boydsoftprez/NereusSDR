// NereusSDR for iOS: reading the Core's TCI clients and options, its log lines and PureSignal's status for the Tools pages
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-18, spec section 5.2 item 4: the TCI Server page lists the
/// Core's `tciClients` records and the PureSignal page shows the Core's
/// `statusJson`. A record reads tolerantly; a status that is not schema 1,
/// or lacks a field the page shows, reads as none.
@Suite struct StationToolsDecodingTests {
    @Test("a tciClients record reads every field; missing or odd fields read as empty")
    @MainActor
    func tciClient() {
        let full = StationTciClient(record: LinkMessage.RecordBatch.Record(id: "7", fields: [
            "id": .string("7"), "name": .string("WSJT-X"), "address": .string("192.0.2.20:51234"),
            "subscriptions": .array([.string("audio"), .number(3), .string("sensors")]),
            "transmitting": .bool(true), "lastCommand": .string("vfo:0,0;"),
        ]))
        #expect(full == StationTciClient(id: "7", name: "WSJT-X", address: "192.0.2.20:51234",
                                         subscriptions: ["audio", "sensors"], transmitting: true,
                                         lastCommand: "vfo:0,0;"))
        let bare = StationTciClient(record: LinkMessage.RecordBatch.Record(id: "8", fields: [
            "name": .number(1), "transmitting": .string("yes"), "subscriptions": .string("audio"),
        ]))
        #expect(bare == StationTciClient(id: "8"))
        #expect(RecordStreamClient.capacity(of: StationTciClient.streamName) == 64)
    }

    @Test("PureSignal's status reads from schema 1 and gives calibrating, feedback and correcting")
    func pureSignalStatus() throws {
        let json = """
        {"schema":1,"psEnabled":true,"mox":true,"engineState":4,"feedbackLevel":153,\
        "successfulCalibrations":12,"attemptedCalibrations":15,"correctionsApplied":true,"raw":[1,2]}
        """
        let status = try #require(PureSignalStatus.parse(json))
        #expect(status.enabled && status.mox && status.correctionsApplied)
        #expect(status.engineState == 4 && status.calibrating)
        #expect(status.feedbackLevel == 153 && status.hearingFeedback)
        #expect(abs(status.feedbackShare - 0.6) < 0.000_1)
        #expect(status.successfulCalibrations == 12 && status.attemptedCalibrations == 15)
        let idle = try #require(PureSignalStatus.parse("""
        {"schema":1,"psEnabled":false,"mox":false,"engineState":0,"feedbackLevel":0,\
        "successfulCalibrations":0,"attemptedCalibrations":0,"correctionsApplied":false}
        """))
        #expect(!idle.calibrating && !idle.hearingFeedback && idle.feedbackShare == 0)
    }

    @Test("a status of another schema, with a missing field, a wrong kind or too long reads as none")
    func unreadableStatus() {
        #expect(PureSignalStatus.parse("") == nil)
        #expect(PureSignalStatus.parse("not json") == nil)
        let fields = "\"psEnabled\":true,\"mox\":false,\"engineState\":1,\"feedbackLevel\":0," +
            "\"successfulCalibrations\":0,\"attemptedCalibrations\":0,\"correctionsApplied\":false"
        #expect(PureSignalStatus.parse("{\"schema\":1,\(fields)}") != nil)
        #expect(PureSignalStatus.parse("{\"schema\":2,\(fields)}") == nil)
        #expect(PureSignalStatus.parse("{\"schema\":1,\(fields.replacingOccurrences(of: "\"mox\":false,", with: ""))}")
                    == nil)
        // A number where a true or false belongs, and a fraction where a whole number belongs.
        #expect(PureSignalStatus.parse("{\"schema\":1,\(fields.replacingOccurrences(of: "\"mox\":false", with: "\"mox\":0"))}")
                    == nil)
        #expect(PureSignalStatus.parse("{\"schema\":1,\(fields.replacingOccurrences(of: "\"engineState\":1", with: "\"engineState\":1.5"))}")
                    == nil)
        let long = "{\"schema\":1,\(fields),\"pad\":\"\(String(repeating: "x", count: 8_200))\"}"
        #expect(PureSignalStatus.parse(long) == nil)
    }

    @Test("the TCI server's four options read only when all four arrive as flags, and go back whole in order")
    func tciOptions() {
        let values: [String: MirrorValue] = ["enabled": .bool(true), "emulateExpertSdr3": .bool(true),
                                             "emulateSunSdr2Pro": .bool(false), "cwluBecomesCw": .bool(true),
                                             "sendInitialState": .bool(false)]
        let options = StationTciOptions(values: values)
        #expect(options == StationTciOptions(emulateExpertSdr3: true, emulateSunSdr2Pro: false, cwluBecomesCw: true,
                                             sendInitialState: false))
        #expect(options?.arguments == [
            CommandArgument(name: "emulateExpertSdr3", value: .bool(true)),
            CommandArgument(name: "emulateSunSdr2Pro", value: .bool(false)),
            CommandArgument(name: "cwluBecomesCw", value: .bool(true)),
            CommandArgument(name: "sendInitialState", value: .bool(false)),
        ])
        // A Core before version 2 sends none; a wrong kind reads as none.
        #expect(StationTciOptions(values: ["enabled": .bool(true)]) == nil)
        var odd = values
        odd["cwluBecomesCw"] = .int(1)
        #expect(StationTciOptions(values: odd) == nil)
    }

    @Test("a coreLog record reads its line; the categories split on commas and drop blanks")
    @MainActor
    func coreLog() {
        let line = CoreLogLine(record: LinkMessage.RecordBatch.Record(id: "41", fields: [
            "line": .string("[18:34:00.120] INF: Core started"),
        ]))
        #expect(line == CoreLogLine(id: "41", line: "[18:34:00.120] INF: Core started"))
        #expect(CoreLogLine(record: LinkMessage.RecordBatch.Record(id: "42", fields: ["line": .number(3)]))
                    == CoreLogLine(id: "42", line: ""))
        #expect(RecordStreamClient.capacity(of: CoreLogLine.streamName) == 200)
        #expect(CoreLogLine.categories("nereus.discovery, nereus.tci,,") == ["nereus.discovery", "nereus.tci"])
        #expect(CoreLogLine.categories("").isEmpty)
    }
}
