// NereusSDR for iOS: the Digital Spectrum Recorder's document part, past rows and hours as the Core packs them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels
import Testing

/// Recording plan Task 28 (design sections 6.7, 6.8): the `dsr` part of
/// the recordings document, the rows and spectrum `dsr.rows` and
/// `dsr.spectrum` answer and the hours `dsr.hours` answers, decoded the way
/// the Core's DsrPastRenderer packs them (little-endian 64-bit times,
/// little-endian signed 16-bit tenths of a dB, one transmit byte per row).
@Suite struct DsrTests {
    static let dsrJSON = """
    {"enabled":true,"disk":"","sizeLimitGb":10,"recording":true,"reason":"","sizeLimitBytes":10000000000,\
    "usedBytes":6300000000,"writePosition":6300000000,"oldestUtcMs":1763290800000,"newestUtcMs":1763298720000,\
    "hoursHeld":2.2,"overflows":0,"rewound":[{"slice":0,"utcNs":1763297000000000000,"frequencyHz":7240500,\
    "liveFrequencyHz":7236400}],"stretch":null,"saving":1,"receivers":[{"receiver":0,"rateHz":192000}],\
    "bytesPerHour":2764800000}
    """

    static func document(dsr: String?) -> String {
        """
        {"version":\(dsr == nil ? 3 : 4),"slices":[],"kept":{"seq":0,"slice":-1,"id":"","name":"","lengthMs":0,\
        "reason":"","device":""},"index":{"revision":1,"count":0,"usedBytes":0,"freeBytes":0,"folder":"/r"},\
        "settings":{"keepMyVoice":true,"sidesApart":false,"autoStart":false,"autoStop":true,"quietMinutes":2,\
        "format":"mp3","disk":"","disks":[]}\(dsr.map { ",\"dsr\":" + $0 } ?? "")}
        """
    }

    @Test func theDocumentsDsrPartIsRead() throws {
        let document = try #require(RecordingsDocument(json: Self.document(dsr: Self.dsrJSON), revision: 4))
        let dsr = try #require(document.dsr)
        #expect(dsr.enabled && dsr.recording && dsr.reason.isEmpty && dsr.sizeLimitGb == 10)
        #expect(dsr.usedBytes == 6_300_000_000 && dsr.sizeLimitBytes == 10_000_000_000)
        #expect(dsr.oldestUtcMs == 1_763_290_800_000 && dsr.newestUtcMs == 1_763_298_720_000)
        #expect(dsr.rewound(0) == .init(slice: 0, utcNs: 1_763_297_000_000_000_000, frequencyHz: 7_240_500,
                                        liveFrequencyHz: 7_236_400))
        #expect(dsr.rewound(1) == nil && dsr.stretch == nil && dsr.saving == 1)
        #expect(dsr.receivers == [.init(receiver: 0, rateHz: 192_000)] && dsr.bytesPerHour == 2_764_800_000)
        // An older Core sends no dsr part.
        #expect(try #require(RecordingsDocument(json: Self.document(dsr: nil), revision: 3)).dsr == nil)
    }

    @Test func aCoreWithoutTheLaterFieldsStillReads() throws {
        let plain = """
        {"enabled":false,"disk":"/media/ssd","sizeLimitGb":20,"recording":false,"reason":"The only disk is an SD card.",\
        "sizeLimitBytes":0,"usedBytes":0,"writePosition":0,"oldestUtcMs":0,"newestUtcMs":0,"hoursHeld":0,"overflows":0,\
        "stretch":{"id":"r-3","fromUtcNs":10,"toUtcNs":20}}
        """
        let dsr = try #require(RecordingsDocument(json: Self.document(dsr: plain), revision: 2)?.dsr)
        #expect(!dsr.enabled && dsr.disk == "/media/ssd" && dsr.reason == "The only disk is an SD card.")
        #expect(dsr.rewound.isEmpty && dsr.receivers.isEmpty && dsr.bytesPerHour == 0 && dsr.saving == 0)
        #expect(dsr.stretch == .init(id: "r-3", fromUtcNs: 10, toUtcNs: 20))
    }

    /// The rows come back as the very bins the Core packed, newest first,
    /// with the rows' times and transmit marks.
    @Test func rowsDecodeToTheSameBins() throws {
        let times: [Int64] = [1_763_298_000_000_000_000, 1_763_297_999_500_000_000, -1]
        let bins: [Int16] = [-1_200, -1_195, -400, Int16.min, -2_000, 0, 32_767, -1, 5]
        let frames = try #require(DsrPastFrames.decode(
            slice: 1, firstBinHz: 7_100_000.5, hzPerBin: 46.875, binCount: 3,
            times: DsrPastFrames.packTimes(times).base64EncodedString(),
            bins: DsrPastFrames.packBins(bins).base64EncodedString(),
            transmit: Data([0, 1, 0]).base64EncodedString(), rowsLeft: 7, nextToUtcNs: 42))
        #expect(frames.utcNs == times && frames.tenthsDb == bins && frames.transmit == [false, true, false])
        #expect(frames.rowCount == 3 && frames.binCount == 3 && frames.slice == 1)
        #expect(frames.firstBinHz == 7_100_000.5 && frames.hzPerBin == 46.875)
        #expect(frames.rowsLeft == 7 && frames.nextToUtcNs == 42)
        #expect(Array(frames.row(1)) == [Int16.min, -2_000, 0])
        #expect(frames.dbm(row: 0, bin: 1) == -119.5)
        // The Core's own packing, byte for byte: -1200 is 0x50 0xFB.
        #expect(Array(DsrPastFrames.packBins([-1_200])) == [0x50, 0xFB])
        #expect(Array(DsrPastFrames.packTimes([1])) == [1, 0, 0, 0, 0, 0, 0, 0])
        // An answer from before the transmit marks reads as none marked.
        let unmarked = try #require(DsrPastFrames.decode(
            slice: 0, firstBinHz: 0, hzPerBin: 1, binCount: 3, times: DsrPastFrames.packTimes(times).base64EncodedString(),
            bins: DsrPastFrames.packBins(bins).base64EncodedString(), transmit: nil, rowsLeft: 0, nextToUtcNs: 0))
        #expect(unmarked.transmit == [false, false, false])
    }

    @Test func rowsThatDoNotAgreeAreNotRead() {
        let times = DsrPastFrames.packTimes([1, 2]).base64EncodedString()
        let five = DsrPastFrames.packBins([1, 2, 3, 4, 5]).base64EncodedString()
        #expect(DsrPastFrames.decode(slice: 0, firstBinHz: 0, hzPerBin: 1, binCount: 3, times: times, bins: five,
                                     transmit: nil, rowsLeft: 0, nextToUtcNs: 0) == nil)
        let six = DsrPastFrames.packBins([1, 2, 3, 4, 5, 6]).base64EncodedString()
        #expect(DsrPastFrames.decode(slice: 0, firstBinHz: 0, hzPerBin: 1, binCount: 3, times: times, bins: six,
                                     transmit: Data([1]).base64EncodedString(), rowsLeft: 0, nextToUtcNs: 0) == nil)
        #expect(DsrPastFrames.decode(slice: 0, firstBinHz: 0, hzPerBin: 1, binCount: 3, times: "not base64", bins: six,
                                     transmit: nil, rowsLeft: 0, nextToUtcNs: 0) == nil)
    }

    @Test func theHoursComeNewestFirstWithTheirActivity() throws {
        let starts: [Int64] = [1_763_294_400_000_000_000, 1_763_290_800_000_000_000]
        let hours = try #require(DsrHour.decode(starts: DsrPastFrames.packTimes(starts).base64EncodedString(),
                                                activity: "[0.734,0.05]"))
        #expect(hours == [.init(startUtcNs: starts[0], activity: 0.734), .init(startUtcNs: starts[1], activity: 0.05)])
        #expect(DsrHour.decode(starts: "", activity: "[]") == [])
        #expect(DsrHour.decode(starts: DsrPastFrames.packTimes(starts).base64EncodedString(), activity: "[1]") == nil)
    }

    /// The recorder's three settings go after the others, in the link's order.
    @Test func theRecordersSettingsGoAsTheirOwnFields() {
        let change = RecordingsSettingsChange(format: .wav, dsrEnabled: false, dsrDisk: "", dsrSizeLimitGb: 25)
        #expect(change.fields.map(\.name) == ["format", "dsrEnabled", "dsrDisk", "dsrSizeLimitGb"])
        #expect(change.fields.last?.value == .whole(25))
        #expect(change.setsDsr && !change.setsOnAirSwitches)
        #expect(!RecordingsSettingsChange(format: .mp3).setsDsr)
    }
}
