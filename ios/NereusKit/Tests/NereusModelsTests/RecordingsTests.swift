// NereusSDR for iOS: decoding the Core's recordings document, its list's entries and the voice keyer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels
import Testing

/// The document in the shape RecordingsFacade builds it
/// (src/core/session/RecordingsFacade.cpp, rebuild, and VoiceKeyer's
/// documentJson), with the values tst_recordings_link asserts of a fresh
/// Core's: version 3, Slice A idle, no recording kept, an empty folder,
/// WAV, and eight empty voice messages with nothing going out.
@Suite struct RecordingsTests {
    static func slots(_ change: (inout [[String: Any]]) -> Void = { _ in }) -> [[String: Any]] {
        var slots: [[String: Any]] = (1...8).map { ["n": $0, "name": "", "lengthMs": 0, "empty": true] }
        change(&slots)
        return slots
    }

    static func document(_ change: (inout [String: Any]) -> Void = { _ in }) -> [String: Any] {
        var document: [String: Any] = [
            "version": 3,
            "slices": [["slice": 0, "state": "idle", "startUtc": "", "startedBy": "", "elapsedMs": 0,
                        "myVoice": false]],
            "kept": ["seq": 0, "slice": -1, "id": "", "name": "", "lengthMs": 0, "reason": "", "device": ""],
            "index": ["revision": 0, "count": 0, "usedBytes": 0, "freeBytes": 1_000_000_000,
                      "folder": "/srv/nereus/Recordings"],
            "settings": ["keepMyVoice": true, "sidesApart": false, "autoStart": false, "autoStop": true,
                         "quietMinutes": 5, "format": "wav", "disk": "",
                         "disks": [["root": "/", "name": "Core", "free": 1_000_000_000, "total": 2_000_000_000,
                                    "sd": false, "own": true]],
                         "onAirUseTxEq": false, "onAirUseCompressor": true, "onAirUseLeveler": false,
                         "onAirUseCfc": false, "onAirUsePhaseRotator": false],
            "voiceKeyer": ["slots": slots(), "repeat": false, "repeatDelaySec": 5, "sending": NSNull()],
        ]
        change(&document)
        return document
    }

    static func text(_ object: Any) -> String {
        String(decoding: (try? JSONSerialization.data(withJSONObject: object)) ?? Data(), as: UTF8.self)
    }

    static func decoded(revision: Int64 = 1, _ change: (inout [String: Any]) -> Void = { _ in }) -> RecordingsDocument? {
        RecordingsDocument(json: text(document(change)), revision: revision)
    }

    @Test func aFreshCoresDocumentDecodes() throws {
        let document = try #require(Self.decoded(revision: 7))
        #expect(document.version == 3)
        #expect(document.revision == 7)
        #expect(document.slices == [RecordingsDocument.Slice(slice: 0, state: .idle, startUtc: "", startedBy: "",
                                                              elapsedMs: 0, myVoice: false)])
        #expect(document.slice(0)?.state == .idle)
        #expect(document.slice(1) == nil)
        #expect(document.kept == RecordingsDocument.Kept(seq: 0, slice: -1, id: "", name: "", lengthMs: 0, reason: nil,
                                                         device: ""))
        #expect(document.index == RecordingsDocument.Index(revision: 0, count: 0, usedBytes: 0,
                                                           freeBytes: 1_000_000_000, folder: "/srv/nereus/Recordings"))
        #expect(document.settings.format == .wav)
        #expect(document.settings.keepMyVoice)
        #expect(document.settings.autoStop)
        #expect(document.settings.quietMinutes == 5)
        #expect(document.settings.disk.isEmpty)
        #expect(document.settings.disks == [RecordingsDocument.Disk(root: "/", name: "Core", free: 1_000_000_000,
                                                                     total: 2_000_000_000, sd: false, own: true)])
        #expect(document.settings.onAirUseCompressor)
        #expect(!document.settings.onAirUseTxEq)
        let voice = try #require(document.voiceKeyer)
        #expect(voice.slots.map(\.n) == Array(1...8))
        #expect(voice.slots.allSatisfy { $0.empty })
        #expect(!voice.repeat)
        #expect(voice.repeatDelaySec == 5)
        #expect(voice.sending == nil)
    }

    @Test func aRecordingSliceAndAKeptRecordingRead() throws {
        let document = try #require(Self.decoded { document in
            document["slices"] = [["slice": 0, "state": "recording", "startUtc": "2026-11-16T14:02:03.000Z",
                                   "startedBy": "Shack phone", "elapsedMs": 12_000, "myVoice": true],
                                  ["slice": 1, "state": "armed", "startUtc": "", "startedBy": "", "elapsedMs": 0,
                                   "myVoice": false]]
            document["kept"] = ["seq": 4, "slice": 0, "id": "r-4", "name": "W1AW 20m", "lengthMs": 65_000,
                                "reason": "device", "device": "Shack phone"]
        })
        #expect(document.slice(0) == RecordingsDocument.Slice(slice: 0, state: .recording,
                                                              startUtc: "2026-11-16T14:02:03.000Z",
                                                              startedBy: "Shack phone", elapsedMs: 12_000,
                                                              myVoice: true))
        #expect(document.slice(1)?.state == .armed)
        #expect(document.kept == RecordingsDocument.Kept(seq: 4, slice: 0, id: "r-4", name: "W1AW 20m",
                                                         lengthMs: 65_000, reason: .device, device: "Shack phone"))
        for reason in ["here", "device", "quiet", "diskFull", "wav"] {
            let kept = try #require(Self.decoded { document in
                var kept = document["kept"] as? [String: Any] ?? [:]
                kept["reason"] = reason
                document["kept"] = kept
            })
            #expect(kept.kept.reason?.rawValue == reason)
        }
    }

    /// A version 1 Core sends neither the on-air switches nor the voice keyer;
    /// a Core without a voice keyer sends an empty object.
    @Test func olderDocumentsReadWithoutTheirLaterParts() throws {
        let first = try #require(Self.decoded { document in
            document["version"] = 1
            var settings = document["settings"] as? [String: Any] ?? [:]
            for name in ["onAirUseTxEq", "onAirUseCompressor", "onAirUseLeveler", "onAirUseCfc", "onAirUsePhaseRotator"] {
                settings[name] = nil
            }
            document["settings"] = settings
            document["voiceKeyer"] = nil
        })
        #expect(first.version == 1)
        #expect(first.voiceKeyer == nil)
        #expect(!first.settings.onAirUseCompressor)
        let none = try #require(Self.decoded { $0["voiceKeyer"] = [String: Any]() })
        #expect(none.voiceKeyer == nil)
    }

    @Test func theMessageGoingOutNamesItsSender() throws {
        let document = try #require(Self.decoded { document in
            document["voiceKeyer"] = ["slots": Self.slots { $0[0] = ["n": 1, "name": "CQ", "lengthMs": 4_000,
                                                                     "empty": false] },
                                      "repeat": true, "repeatDelaySec": 10,
                                      "sending": ["n": 1, "positionMs": 0, "lengthMs": 4_000, "waitingMs": 7_000,
                                                  "by": "Shack phone", "deviceId": "phone-id"]]
        })
        let voice = try #require(document.voiceKeyer)
        #expect(voice.slot(1) == VoiceKeyerState.Slot(n: 1, name: "CQ", lengthMs: 4_000, empty: false))
        #expect(voice.repeat)
        #expect(voice.repeatDelaySec == 10)
        let sending = try #require(voice.sending)
        #expect(sending == VoiceKeyerState.Sending(n: 1, positionMs: 0, lengthMs: 4_000, waitingMs: 7_000,
                                                   by: "Shack phone", deviceId: "phone-id"))
        // Waiting between repeats is still on the air.
        #expect(sending.waiting)
        #expect(voice.isSentBy(["phone-id"]))
        #expect(voice.isSentBy([nil, "phone-id"]))
        #expect(!voice.isSentBy(["tablet-id", nil]))
        #expect(voice.onAirReason(1) == "M1 is on the air.")
        #expect(voice.onAirReason(2) == nil)
        // Nothing going out is no one's.
        let idle = try #require(Self.decoded()?.voiceKeyer)
        #expect(!idle.isSentBy(["phone-id"]))
        #expect(idle.onAirReason(1) == nil)
    }

    @Test func unknownKeysAreIgnored() throws {
        let plain = try #require(Self.decoded())
        let extended = try #require(Self.decoded { document in
            document["somethingNew"] = ["a": 1]
            var index = document["index"] as? [String: Any] ?? [:]
            index["somethingNew"] = true
            document["index"] = index
        })
        #expect(extended == plain)
    }

    @Test func aBadDocumentOrRevisionDecodesToNil() {
        let good = Self.text(Self.document())
        #expect(RecordingsDocument(json: good, revision: 0) != nil)
        #expect(RecordingsDocument(json: good, revision: Int64(UInt32.max)) != nil)
        #expect(RecordingsDocument(json: good, revision: -1) == nil)
        #expect(RecordingsDocument(json: good, revision: Int64(UInt32.max) + 1) == nil)
        // What a Core sends before it publishes.
        #expect(RecordingsDocument(json: "", revision: 0) == nil)
        #expect(RecordingsDocument(json: "{", revision: 1) == nil)
        #expect(RecordingsDocument(json: "[]", revision: 1) == nil)
        #expect(Self.decoded { $0["version"] = 0 } == nil)
        #expect(Self.decoded { $0["kept"] = nil } == nil)
        #expect(Self.decoded { $0["slices"] = [["slice": 0, "state": "paused", "startUtc": "", "startedBy": "",
                                                 "elapsedMs": 0, "myVoice": false]] } == nil)
        #expect(Self.decoded { document in
            var settings = document["settings"] as? [String: Any] ?? [:]
            settings["format"] = "ogg"
            document["settings"] = settings
        } == nil)
        #expect(Self.decoded { $0["voiceKeyer"] = ["slots": "none"] } == nil)
    }

    /// `recordings.list`'s `entries`, as RecordingsFacade::entryJson writes each.
    @Test func theListsEntriesDecodeAndUnknownKindsAreLeftOut() throws {
        let entries: [[String: Any]] = [
            ["id": "r-2", "kind": "Contact", "slice": 0, "startUtc": "2026-11-16T14:02:03.000Z", "lengthMs": 65_000,
             "frequencyHz": 14_250_000, "mode": "USB", "call": "W1AW", "name": "W1AW 20m",
             "fileName": "2026-11-16 1402 W1AW.mp3", "sizeBytes": 520_000],
            ["id": "r-1", "kind": "WholePan", "slice": -1, "startUtc": "2026-11-15T10:00:00.000Z", "lengthMs": 1_000,
             "frequencyHz": 7_100_000, "mode": "", "call": "", "name": "40m", "fileName": "pan.wav",
             "sizeBytes": 96_000],
            ["id": "r-0", "kind": "Hologram", "slice": 0, "startUtc": "", "lengthMs": 0, "frequencyHz": 0, "mode": "",
             "call": "", "name": "", "fileName": "", "sizeBytes": 0],
            ["id": "", "kind": "Sound", "slice": -1, "startUtc": "", "lengthMs": 0, "frequencyHz": 0, "mode": "",
             "call": "", "name": "", "fileName": "", "sizeBytes": 0],
        ]
        let list = try #require(RecordingEntry.list(json: Self.text(entries)))
        #expect(list == [
            RecordingEntry(id: "r-2", kind: .contact, slice: 0, startUtc: "2026-11-16T14:02:03.000Z", lengthMs: 65_000,
                           frequencyHz: 14_250_000, mode: "USB", call: "W1AW", name: "W1AW 20m",
                           fileName: "2026-11-16 1402 W1AW.mp3", sizeBytes: 520_000),
            RecordingEntry(id: "r-1", kind: .wholePan, slice: -1, startUtc: "2026-11-15T10:00:00.000Z", lengthMs: 1_000,
                           frequencyHz: 7_100_000, mode: "", call: "", name: "40m", fileName: "pan.wav",
                           sizeBytes: 96_000),
        ])
        #expect(RecordingEntry.list(json: "[]") == [])
        #expect(RecordingEntry.list(json: "{}") == nil)
    }

    /// Only the fields set go, in the link's order.
    @Test func aSettingsChangeSendsOnlyWhatItSets() {
        #expect(RecordingsSettingsChange().isEmpty)
        let change = RecordingsSettingsChange(keepMyVoice: true, quietMinutes: 10, format: .mp3, disk: "",
                                              onAirUseCfc: false)
        #expect(change.fields.map(\.name) == ["keepMyVoice", "quietMinutes", "format", "disk", "onAirUseCfc"])
        #expect(change.fields.map(\.value) == [.flag(true), .whole(10), .text("mp3"), .text(""), .flag(false)])
        #expect(change.setsOnAirSwitches)
        #expect(!RecordingsSettingsChange(sidesApart: true).setsOnAirSwitches)
    }
}
