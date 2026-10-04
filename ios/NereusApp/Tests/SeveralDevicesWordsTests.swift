// NereusSDR for iOS: another device's name inside the phone's own several-devices sentences
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// R-IOS-17: every device name reads right in the sentences the phone writes
/// itself. A plain short name takes "the" as the board writes it ("the
/// MacBook"); a name that already says whose it is does not ("Grant's
/// iPhone", "Chris' iPad").
@Suite("Several devices: device names in sentences")
@MainActor
struct SeveralDevicesWordsTests {
    /// A device name, and how a sentence names it mid-sentence and at its start.
    static let names: [(name: String, the: String, The: String)] = [
        ("MacBook", "the MacBook", "The MacBook"),
        ("Grant's iPhone", "Grant's iPhone", "Grant's iPhone"),
        ("Chris' iPad", "Chris' iPad", "Chris' iPad"),
    ]

    @Test("whose a name says it is")
    func possessive() {
        #expect(!SeveralDevicesWords.isPossessive("MacBook"))
        #expect(!SeveralDevicesWords.isPossessive("iPad Pro"))
        #expect(!SeveralDevicesWords.isPossessive("Chris iPad"))
        #expect(SeveralDevicesWords.isPossessive("Grant's iPhone"))
        #expect(SeveralDevicesWords.isPossessive("Grant\u{2019}s iPhone"))
        #expect(SeveralDevicesWords.isPossessive("Chris' iPad"))
        #expect(SeveralDevicesWords.isPossessive("Chris\u{2019} iPad"))
        #expect(SeveralDevicesWords.isPossessive("GRANT'S IPHONE"))
    }

    /// A marker's slice whose owner came as the Core sends it: `kind` alone, or a name.
    static func entry(kind: String, name: String = "", shortName: String = "") -> ForeignSliceMarkers.Slice {
        ForeignSliceMarkers.Slice(id: 1, frequencyHz: 7_249_000, filterLowHz: -3000, filterHighHz: -1000,
                                  colour: "#FF40FF", ownerName: name, ownerShortName: shortName, ownerKind: kind)
    }

    /// The note says what the Core says when it refuses a change to
    /// another device's slice from this app: "That slice belongs to <holder>.
    /// It can be changed only there." (StationServer::ownedElsewhereReason,
    /// src/core/session/StationServer.cpp:9085-9093 at c13abe564). The
    /// holder is in the Core's words (sliceHolderWords, 9059-9083): "the
    /// Core" for kind `station`, which the Core sends with no name for a
    /// slice it holds itself or no device holds; "a" and the kind for a
    /// device sent with no name; "another device" when neither is known.
    @Test("an owner with no name", arguments: [
        ("station", "the Core", "The Core\u{2019}s slice B",
         "That slice belongs to the Core. It can be changed only there.", "Slice B, the Core\u{2019}s. Tap for details."),
        ("phone", "a phone", "A phone\u{2019}s slice B",
         "That slice belongs to a phone. It can be changed only there.", "Slice B, a phone\u{2019}s. Tap for details."),
        ("tablet", "a tablet", "A tablet\u{2019}s slice B",
         "That slice belongs to a tablet. It can be changed only there.", "Slice B, a tablet\u{2019}s. Tap for details."),
        ("computer", "a computer", "A computer\u{2019}s slice B",
         "That slice belongs to a computer. It can be changed only there.",
         "Slice B, a computer\u{2019}s. Tap for details."),
        ("", "another device", "Another device\u{2019}s slice B",
         "That slice belongs to another device. It can be changed only there.",
         "Slice B, another device\u{2019}s. Tap for details."),
        // The Core reads any other kind as a computer.
        ("watch", "a computer", "A computer\u{2019}s slice B",
         "That slice belongs to a computer. It can be changed only there.",
         "Slice B, a computer\u{2019}s. Tap for details."),
    ])
    func ownerWithNoName(_ words: (String, String, String, String, String)) {
        let (kind, label, title, fine, spoken) = words
        let entry = Self.entry(kind: kind)
        #expect(entry.labelName == label)
        #expect(ForeignSliceLabel.Note.title(entry) == title)
        #expect(ForeignSliceLabel.Note.fine(entry) == fine)
        #expect("Slice B, \(ForeignSliceLabel.whose(entry)). Tap for details." == spoken)
        for text in [entry.labelName, ForeignSliceLabel.Note.title(entry), ForeignSliceLabel.Note.fine(entry)] {
            #expect(!text.isEmpty)
            #expect(!text.contains("  "))
            #expect(!text.hasPrefix("\u{2019}"))
        }
    }

    /// A named owner is named as the Core sends it, never with "the" put
    /// before it or its case changed: the label its short name, the note
    /// its name.
    @Test("a named owner, as sent")
    func namedOwner() {
        let mac = Self.entry(kind: "computer", name: "MacBook Pro", shortName: "MacBook")
        #expect(mac.labelName == "MacBook")
        #expect(ForeignSliceLabel.Note.title(mac) == "MacBook Pro\u{2019}s slice B")
        #expect(ForeignSliceLabel.Note.fine(mac) == "That slice belongs to MacBook Pro. It can be changed only there.")
        #expect(ForeignSliceLabel.whose(mac) == "MacBook\u{2019}s")
        let grant = Self.entry(kind: "phone", name: "Grant's iPhone")
        #expect(ForeignSliceLabel.Note.fine(grant) == "That slice belongs to Grant's iPhone. It can be changed only there.")
        let lower = Self.entry(kind: "tablet", name: "ipad in the shack")
        #expect(ForeignSliceLabel.Note.title(lower) == "ipad in the shack\u{2019}s slice B")
        #expect(SeveralDevicesWords.sliceOwner(lower, capitalised: true) == "ipad in the shack")
        // A station owner is the Core's even when a name came with it.
        #expect(ForeignSliceLabel.Note.fine(Self.entry(kind: "station", name: "Shack"))
            == "That slice belongs to the Core. It can be changed only there.")
    }

    @Test("moving a receiver another device shares", arguments: [0, 1, 2])
    func moveSheet(_ index: Int) {
        let name = Self.names[index]
        let closes = Self.question(kind: "panMove", shortName: name.name, effect: "closes")
        #expect(MoveSharedReceiverSheet.kicker(closes) == "\(name.The) shares this receiver")
        #expect(MoveSharedReceiverSheet.note(closes)
            == "Going to 20 m moves the receiver off 40 m. No other receiver is free, so \(name.the)\u{2019}s "
            + "slice B closes, and \(name.the) is told who moved it.")
        let moves = Self.question(kind: "panMove", shortName: name.name, effect: "moves")
        #expect(MoveSharedReceiverSheet.note(moves)
            == "Going to 20 m moves the receiver off 40 m. \(name.The)\u{2019}s slice B moves to a free receiver, "
            + "and \(name.the) is told who moved it.")
    }

    @Test("a change another device hears", arguments: [0, 1, 2])
    func sharedChangeSheet(_ index: Int) {
        let name = Self.names[index]
        let question = Self.question(kind: "sharedSetting", shortName: name.name, effect: "changes")
        #expect(SharedChangeSheet.kicker(question) == "This changes what \(name.the) hears")
        #expect(SharedChangeSheet.note(question) == "\(name.The) is told you changed it.")
    }

    @Test("taking another device's receiver or slice", arguments: [0, 1, 2])
    func takeSheet(_ index: Int) throws {
        let name = Self.names[index]
        let question = SeveralDevices.Question(LinkMessage.ConfirmRequest(
            id: 1, kind: "takeReceiver", reason: SeveralDevices.waitingReason, affected: [], expiresInMs: 60_000,
            choices: [.object([
                "choice": .number(0), "streamIndex": .number(1), "adc": .number(0), "centreHz": .number(14_230_000),
                "rateHz": .number(192_000), "anchorName": .string(name.name),
                "slices": .array([.object(["sliceId": .number(2), "letter": .string("C"), "deviceId": .string("d"),
                                           "deviceName": .string(name.name), "frequencyHz": .number(14_230_000),
                                           "mode": .number(1), "band": .number(5), "txSlice": .bool(false)])]),
                "devices": .array([.object(["deviceId": .string("d"), "name": .string(name.name),
                                            "shortName": .string(name.name), "state": .string("listening"),
                                            "lastActivitySeconds": .number(60)])]),
                "takeable": .bool(true), "why": .string(""),
            ])],
            forCommandId: 900))
        let choice = try #require(question.choices.first)
        #expect(TakeReceiverSheet.go(choice, slices: false) == "Take \(name.the)\u{2019}s receiver")
        #expect(TakeReceiverSheet.go(choice, slices: true) == "Take \(name.the)\u{2019}s slice")
    }

    /// A question naming one device, by `shortName`, whose slice B the change reaches with `effect`.
    static func question(kind: String, shortName: String, effect: String) -> SeveralDevices.Question {
        let device = LinkJSON.object([
            "deviceId": .string("d"), "deviceName": .string(shortName + " Pro"), "deviceShortName": .string(shortName),
            "state": .string("listening"), "holdsTransmit": .bool(false),
            "slices": .array([.object(["sliceId": .number(1), "letter": .string("B"), "frequencyHz": .number(7_249_000),
                                       "band": .number(3), "mode": .number(0), "adc": .number(0),
                                       "streamIndex": .number(0), "effect": .string(effect)])]),
        ])
        let change: [String: LinkJSON] = kind == "panMove"
            ? ["label": .string("Receiver 1"), "from": .string("40 m"), "to": .string("20 m")]
            // An asking row since ruling 7.1a: the receive antenna relay, as the
            // shared-setting-confirm session asks it (the attenuator is a notice now).
            : ["label": .string("Receive on the transmit antenna"), "from": .string("Off"), "to": .string("On")]
        return SeveralDevices.Question(LinkMessage.ConfirmRequest(
            id: 1, kind: kind, reason: SeveralDevices.waitingReason, affected: [device], expiresInMs: 60_000,
            change: change, forWriteId: 900))
    }
}
