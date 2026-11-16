// NereusSDR for iOS: decoding the Core's band stack document, and reading it from the mirror
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// The document is the one `tst_bandstack_link` holds the Core's facade to.
@Suite struct BandStackTests {
    private func sample() throws -> [String: Any] {
        let data = Data(try LinkFixtureLoader.bandStackDocument().utf8)
        return try #require(try JSONSerialization.jsonObject(with: data) as? [String: Any])
    }

    private func text(_ document: [String: Any]) throws -> String {
        String(decoding: try JSONSerialization.data(withJSONObject: document), as: UTF8.self)
    }

    private func decoded(_ change: (inout [String: Any]) -> Void) throws -> BandStack? {
        var document = try sample()
        change(&document)
        return BandStack(json: try text(document), revision: 1)
    }

    @Test func theFacadesDocumentDecodes() throws {
        let stack = try #require(BandStack(json: try LinkFixtureLoader.bandStackDocument(), revision: 7))
        #expect(stack.revision == 7)
        #expect(stack.region == "United States")
        #expect(stack.dupeProtection)
        #expect(stack.expiryMinutes == 15)
        #expect(stack.dwellSeconds == 30)
        #expect(stack.lastExpiryBatchId.isEmpty)
        #expect(stack.lastExpiryCount == 0)
        #expect(stack.lastRecall == BandStack.LastRecall(seq: 0, sliceId: -1, applyView: false, centerHz: 0, spanHz: 0))
        #expect(stack.entries.map(\.id) == ["80m-low", "40m-net", "40m-ssb", "20m-ft8"])

        let net = try #require(stack.entries(band: "40m").first)
        #expect(net == BandStack.Entry(id: "40m-net", band: "40m", frequencyHz: 7_185_000, mode: "LSB",
                                       filterLowHz: -2800, filterHighHz: -100, centerHz: 7_188_000, spanHz: 192_000,
                                       locked: true, seeded: false, name: "Evening net",
                                       lastUsedMs: 1_762_300_000_123))
    }

    @Test func aBandsEntriesComeInTheCoresOrder() throws {
        let stack = try #require(try decoded { document in
            var bands = document["bands"] as? [String: Any] ?? [:]
            bands["40m"] = ["order": ["40m-ssb", "40m-net"], "returnMode": "current", "specificId": ""]
            document["bands"] = bands
        })
        #expect(stack.entries(band: "40m").map(\.id) == ["40m-ssb", "40m-net"])
        #expect(stack.entries(band: "20m").map(\.id) == ["20m-ft8"])
        #expect(stack.entries(band: "160m").isEmpty)
    }

    @Test func placesAndReturnsReadByBand() throws {
        let stack = try #require(BandStack(json: try LinkFixtureLoader.bandStackDocument(), revision: 1))
        #expect(stack.place(slice: "A", band: "40m") == "40m-net")
        #expect(stack.place(slice: "B", band: "20m") == "20m-ft8")
        #expect(stack.place(slice: "A", band: "20m") == nil)
        #expect(stack.place(slice: "C", band: "40m") == nil)
        #expect(stack.returnMode(band: "40m") == .specific)
        #expect(stack.specificId(band: "40m") == "40m-net")
        #expect(stack.returnMode(band: "20m") == .lastUsed)
        #expect(stack.specificId(band: "20m") == nil)
        #expect(stack.returnMode(band: "80m") == .current)
        // A band the document does not mention.
        #expect(stack.returnMode(band: "160m") == .current)
        #expect(stack.specificId(band: "160m") == nil)
    }

    @Test func aPlaceOnAnEntryThatHasGoneIsNoPlace() throws {
        let stack = try #require(try decoded { $0["places"] = ["A": ["40m": "gone"]] })
        #expect(stack.place(slice: "A", band: "40m") == nil)
    }

    @Test func theRecallAndExpiryNotesAreRead() throws {
        let stack = try #require(try decoded { document in
            document["lastRecall"] = ["seq": 4, "sliceId": 1, "applyView": true, "centerHz": 7_188_000, "spanHz": 192_000]
            document["lastExpiry"] = ["batchId": "batch-1", "count": 2]
        })
        #expect(stack.lastRecall == BandStack.LastRecall(seq: 4, sliceId: 1, applyView: true, centerHz: 7_188_000,
                                                         spanHz: 192_000))
        #expect(stack.lastExpiryBatchId == "batch-1")
        #expect(stack.lastExpiryCount == 2)
    }

    @Test func unknownKeysAreIgnored() throws {
        let plain = try #require(BandStack(json: try LinkFixtureLoader.bandStackDocument(), revision: 1))
        let extended = try #require(try decoded { document in
            document["somethingNew"] = ["a": 1]
            var entries = document["entries"] as? [[String: Any]] ?? []
            entries[0]["somethingNew"] = true
            document["entries"] = entries
            var options = document["options"] as? [String: Any] ?? [:]
            options["somethingNew"] = 1
            document["options"] = options
        })
        #expect(extended == plain)
    }

    @Test func aBadDocumentOrRevisionDecodesToNil() throws {
        let good = try LinkFixtureLoader.bandStackDocument()
        #expect(BandStack(json: good, revision: 0) != nil)
        #expect(BandStack(json: good, revision: Int64(UInt32.max)) != nil)
        #expect(BandStack(json: good, revision: -1) == nil)
        #expect(BandStack(json: good, revision: Int64(UInt32.max) + 1) == nil)
        // What a Core with no radio sends.
        #expect(BandStack(json: "", revision: 1) == nil)
        #expect(BandStack(json: "{", revision: 1) == nil)
        #expect(BandStack(json: "[]", revision: 1) == nil)
        #expect(try decoded { $0["schema"] = 2 } == nil)
        #expect(try decoded { $0["region"] = nil } == nil)
        #expect(try decoded { $0["options"] = "none" } == nil)
        #expect(try decoded { $0["lastRecall"] = nil } == nil)
        #expect(try decoded { document in
            var bands = document["bands"] as? [String: [String: Any]] ?? [:]
            bands["40m"]?["returnMode"] = "elsewhere"
            document["bands"] = bands
        } == nil)
        #expect(try decoded { document in
            var entries = document["entries"] as? [[String: Any]] ?? []
            entries[0]["frequencyHz"] = "3573000"
            document["entries"] = entries
        } == nil)
        // Two entries with one id, and an entry with none.
        #expect(try decoded { document in
            var entries = document["entries"] as? [[String: Any]] ?? []
            entries[1]["id"] = entries[0]["id"]
            document["entries"] = entries
        } == nil)
        #expect(try decoded { document in
            var entries = document["entries"] as? [[String: Any]] ?? []
            entries[0]["id"] = ""
            document["entries"] = entries
        } == nil)
    }

    /// The object's place in the Core's snapshot does not matter, nor do its
    /// neighbours: the mirror reads it by key.
    @MainActor @Test func theMirrorReadsTheObjectByKeyWhereverItCame() throws {
        let json = try LinkFixtureLoader.bandStackDocument()
        let schema = LinkMessage.Schema(className: "BandStackFacade", fields: [
            .init(ordinal: 0, name: "json", kind: .utf8), .init(ordinal: 1, name: "revision", kind: .i64),
        ])
        let other = LinkMessage.Schema(className: "Other", fields: [.init(ordinal: 0, name: "value", kind: .i64)])
        let stack = LinkMessage.ObjectCreate(key: BandStack.objectKey, className: "BandStackFacade", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)), .init(ordinal: 1, name: "revision", value: .i64(3)),
        ])
        let neighbour = LinkMessage.ObjectCreate(key: "other", className: "Other", properties: [
            .init(ordinal: 0, name: "value", value: .i64(1)),
        ])
        for creates in [[stack, neighbour], [neighbour, stack]] {
            let store = MirrorStore(send: { _ in })
            #expect(store.bandStack == nil)
            store.apply(.schema(schema))
            store.apply(.schema(other))
            for create in creates {
                store.apply(.objectCreate(create))
            }
            #expect(store.objectKeys == creates.map(\.key))
            #expect(store.bandStack == BandStack(json: json, revision: 3))

            store.apply(.delta(LinkMessage.Delta(key: BandStack.objectKey, properties: [
                .init(ordinal: 0, name: "json", value: .utf8("")), .init(ordinal: 1, name: "revision", value: .i64(4)),
            ])))
            #expect(store.bandStack == nil)
        }
    }
}
