// NereusSDR for iOS: the Core's stationRadios records read as radios, tolerant of missing and odd fields
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMirror

/// Link document section 7.7, stream `stationRadios` at
/// `stationRadiosVersion` 1: each record reads as one radio; a field the
/// Core left out reads as empty or nil, never as a made-up value.
@Suite struct StationRadioTests {
    @Test("a full record reads every field as the Core sent it")
    func fullRecord() {
        let radio = StationRadio(record: .init(id: "AA:BB:CC:DD:EE:02", fields: [
            "id": .string("AA:BB:CC:DD:EE:02"), "mac": .string("AA:BB:CC:DD:EE:02"), "name": .string("Bench G2"),
            "model": .number(12), "address": .string("192.0.2.22"), "protocol": .number(2), "inUse": .bool(false),
        ]))
        #expect(radio == StationRadio(id: "AA:BB:CC:DD:EE:02", name: "Bench G2", model: 12, address: "192.0.2.22",
                                      protocolVersion: 2, inUse: false))
    }

    @Test("missing or odd fields read as empty or nil, not as zero")
    func tolerant() {
        let radio = StationRadio(record: .init(id: "AA:BB:CC:DD:EE:09", fields: [
            "model": .string("12"), "protocol": .number(1.5), "inUse": .string("yes"),
        ]))
        #expect(radio.mac == "AA:BB:CC:DD:EE:09")
        #expect(radio.name.isEmpty && radio.address.isEmpty)
        #expect(radio.model == nil && radio.protocolVersion == nil && !radio.inUse)
    }

    @Test("with radioModelsVersion 1 each record names its model and lists the models its board can run as")
    func models() throws {
        var radios: [StationRadio] = []
        for message in try FixtureReplay.stationMessages("session-station-radio-models") {
            if case .recordBatch(let batch) = message, batch.stream == StationRadio.streamName {
                radios = batch.upserts.map(StationRadio.init(record:))
            }
        }
        #expect(radios.map(\.modelLabel) == ["Hermes Lite 2", "ANAN-G2"])
        #expect(radios.first?.models == [StationRadio.ModelChoice(model: 14, label: "Hermes Lite 2")])
        #expect(radios.last?.models == [StationRadio.ModelChoice(model: 11, label: "ANAN-G2"),
                                        StationRadio.ModelChoice(model: 12, label: "ANAN-G2 1K")])
        #expect(StationRadio.modelsFeatureName == "radioModels")
        #expect(StationRadio.modelsCapabilityName == "radioModelsVersion")
    }

    @Test("an older Core's record has no model name and no model list; an odd entry in the list is skipped")
    func modelsMissing() {
        let older = StationRadio(record: .init(id: "AA:BB:CC:DD:EE:02", fields: ["model": .number(12)]))
        #expect(older.modelLabel.isEmpty && older.models == nil)
        let odd = StationRadio(record: .init(id: "AA:BB:CC:DD:EE:02", fields: [
            "modelLabel": .number(3),
            "models": .array([.object(["model": .number(11), "label": .string("ANAN-G2")]),
                              .object(["model": .string("12"), "label": .string("ANAN-G2 1K")]),
                              .object(["model": .number(1.5), "label": .string("Half")]),
                              .object(["model": .number(12)]), .string("odd"),
                              .object(["model": .number(12), "label": .string("ANAN-G2 1K"), "extra": .bool(true)])]),
        ]))
        #expect(odd.modelLabel.isEmpty)
        // Only a whole model number with its label is a choice the phone can offer.
        #expect(odd.models == [StationRadio.ModelChoice(model: 11, label: "ANAN-G2"),
                               StationRadio.ModelChoice(model: 12, label: "ANAN-G2 1K")])
        let notAList = StationRadio(record: .init(id: "X", fields: ["models": .string("11,12")]))
        #expect(notAList.models == nil)
    }

    @Test("the stream keeps at most 64 radios")
    @MainActor
    func capacity() {
        #expect(RecordStreamClient.capacity(of: StationRadio.streamName) == 64)
    }
}
