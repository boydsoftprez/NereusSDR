// NereusSDR for iOS: the Core's VAX channels (the vax object) and their meters (the vaxLevels record), as read
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-18 (link document sections 6.3, 7.1 and 7.7, `vaxVersion` 1): the
/// `vax` object's four channels and its transmit row read by name, and the
/// one `vaxLevels` record's five meters.
@MainActor
@Suite struct StationVaxTests {
    @Test("the names the Core sends, and the one-record stream")
    func names() {
        #expect(StationVax.featureName == "vax" && StationVax.capabilityName == "vaxVersion")
        #expect(StationVax.objectKey == "vax" && StationVax.levelsStream == "vaxLevels")
        #expect(StationVax.levelsCapacity == 1)
        #expect(RecordStreamClient.capacity(of: StationVax.levelsStream) == 1)
        #expect(StationVax.rxGainProperty(2) == "ch2RxGain" && StationVax.mutedProperty(4) == "ch4Muted")
        #expect(StationVax.txGainProperty == "txGain")
    }

    @Test("each channel's slices, level, mute and device, then the transmit slice and level")
    func channels() {
        let vax = StationVax(values: [
            "ch1Slices": .text("AB"), "ch2Slices": .text(""), "ch3Slices": .text("C"), "ch4Slices": .text(""),
            "ch1RxGain": .double(0.5), "ch2RxGain": .double(1), "ch3RxGain": .double(0.125), "ch4RxGain": .double(0),
            "ch1Muted": .bool(false), "ch2Muted": .bool(true), "ch3Muted": .bool(false), "ch4Muted": .bool(false),
            "ch1Device": .text("NereusSDR VAX 1"), "ch2Device": .text("NereusSDR VAX 2"),
            "ch3Device": .text("NereusSDR VAX 3"), "ch4Device": .text(""),
            "txSlice": .text("A"), "txGain": .double(0.75),
        ])
        #expect(vax.channels.map(\.number) == [1, 2, 3, 4])
        #expect(vax.channels.map(\.slices) == ["AB", "", "C", ""])
        #expect(vax.channels.map(\.rxGain) == [0.5, 1, 0.125, 0])
        #expect(vax.channels.map(\.muted) == [false, true, false, false])
        #expect(vax.channels.map(\.device) == ["NereusSDR VAX 1", "NereusSDR VAX 2", "NereusSDR VAX 3", ""])
        #expect(vax.txSlice == "A" && vax.txGain == 0.75)
    }

    @Test("a property missing or of another kind reads as not sent")
    func missing() {
        let vax = StationVax(values: ["ch1RxGain": .text("loud"), "ch2Muted": .int(1), "txGain": .double(.nan)])
        #expect(vax.channels.count == 4)
        #expect(vax.channels.allSatisfy { $0.rxGain == nil && $0.muted == nil && $0.slices.isEmpty })
        #expect(vax.txGain == nil && vax.txSlice.isEmpty)
    }

    @Test("the meters' record: four channels and transmit, 0 to 1, and when the Core read them")
    func levels() {
        let record = LinkMessage.RecordBatch.Record(id: "0", fields: [
            "ch1Level": .number(0.412), "ch2Level": .number(0), "ch3Level": .number(1.2),
            "ch4Level": .string("x"), "txLevel": .number(0.05), "atMs": .number(1_790_000_000_500),
        ])
        let levels = StationVax.Levels(record: record)
        #expect(levels.channels == [0.412, 0, 1, nil])
        #expect(levels.tx == 0.05)
        #expect(levels.atMs == 1_790_000_000_500)
    }
}
