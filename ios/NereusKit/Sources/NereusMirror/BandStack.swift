// NereusSDR for iOS: the Core's band stack document, read from its mirrored bandStack object
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The Core's band stack, as its `bandStack` object sends it (the `json` and
/// `revision` properties, feature `bandStack` 1). The phone only reads it:
/// seeds, cycling, wrap and duplicates are the Core's, and the `bandStack.*`
/// verbs change it. Bands are named by the Core's band keys ("40m"), slices
/// by their letters ("A"). Keys the phone does not know are ignored; a
/// document whose known keys have the wrong shape decodes to nil.
public struct BandStack: Equatable, Sendable {
    public static let objectKey = "bandStack"
    public static let capability = "bandStackVersion"
    /// The one document schema this reader knows.
    private static let schema = 1

    public struct Entry: Equatable, Sendable, Identifiable {
        public let id: String
        public let band: String
        public let frequencyHz: Int64
        public let mode: String
        public let filterLowHz: Int
        public let filterHighHz: Int
        public let centerHz: Int64
        /// 0 means no saved zoom.
        public let spanHz: Int64
        public let locked: Bool
        public let seeded: Bool
        public let name: String
        public let lastUsedMs: Int64
    }

    public enum ReturnMode: String, Sendable {
        case current, lastUsed, specific
    }

    /// The Core's answer to a recall. Only the device controlling `sliceId`
    /// applies it to its own view; every other device ignores it.
    public struct LastRecall: Equatable, Sendable {
        public let seq: Int
        public let sliceId: Int
        public let applyView: Bool
        public let centerHz: Int64
        public let spanHz: Int64
    }

    public let revision: Int64
    public let region: String
    public let entries: [Entry]
    public let dupeProtection: Bool
    public let expiryMinutes: Int
    public let dwellSeconds: Int
    public let lastExpiryBatchId: String
    public let lastExpiryCount: Int
    public let lastRecall: LastRecall

    private let bandStates: [String: BandState]
    private let places: [String: [String: String]]
    private let entriesByBand: [String: [Entry]]

    /// A band's entries in the Core's order.
    public func entries(band: String) -> [Entry] {
        entriesByBand[band] ?? []
    }

    /// The entry a slice stands on in a band; nil when it has none, or the entry has gone.
    public func place(slice: String, band: String) -> String? {
        guard let id = places[slice]?[band], entries(band: band).contains(where: { $0.id == id }) else {
            return nil
        }
        return id
    }

    /// Where a band returns a slice. A band the document does not mention is `current`.
    public func returnMode(band: String) -> ReturnMode {
        bandStates[band]?.returnMode ?? .current
    }

    /// The entry a band's `specific` return names, nil when it names none.
    public func specificId(band: String) -> String? {
        guard let id = bandStates[band]?.specificId, !id.isEmpty else {
            return nil
        }
        return id
    }

    /// Nil for a revision outside the link's range (an unsigned 32 bit
    /// count), or a document that is empty, as a Core with no radio sends,
    /// malformed, or of another schema.
    public init?(json: String, revision: Int64) {
        guard (0...Int64(UInt32.max)).contains(revision),
              let document = try? JSONDecoder().decode(Document.self, from: Data(json.utf8)),
              document.schema == Self.schema,
              !document.entries.contains(where: { $0.id.isEmpty }),
              Set(document.entries.map(\.id)).count == document.entries.count else {
            return nil
        }
        self.revision = revision
        region = document.region
        entries = document.entries
        dupeProtection = document.options.dupeProtection
        expiryMinutes = document.options.expiryMinutes
        dwellSeconds = document.options.dwellSeconds
        lastExpiryBatchId = document.lastExpiry.batchId
        lastExpiryCount = document.lastExpiry.count
        lastRecall = document.lastRecall
        bandStates = document.bands
        places = document.places

        // Each band in its order. The Core's orders name all its entries and
        // only those; anything else in a document is shown, not reasoned about.
        var byBand: [String: [Entry]] = [:]
        for band in Set(document.entries.map(\.band)) {
            let onBand = document.entries.filter { $0.band == band }
            var named: [Entry] = []
            for id in document.bands[band]?.order ?? [] where !named.contains(where: { $0.id == id }) {
                if let entry = onBand.first(where: { $0.id == id }) {
                    named.append(entry)
                }
            }
            byBand[band] = named + onBand.filter { entry in !named.contains { $0.id == entry.id } }
        }
        entriesByBand = byBand
    }

    private struct BandState: Equatable, Sendable, Decodable {
        let order: [String]
        let returnMode: ReturnMode
        let specificId: String
    }

    private struct Options: Decodable {
        let dupeProtection: Bool
        let expiryMinutes: Int
        let dwellSeconds: Int
    }

    private struct LastExpiry: Decodable {
        let batchId: String
        let count: Int
    }

    private struct Document: Decodable {
        let schema: Int
        let region: String
        let options: Options
        let bands: [String: BandState]
        let entries: [Entry]
        let places: [String: [String: String]]
        let lastExpiry: LastExpiry
        let lastRecall: LastRecall
    }
}

extension BandStack.Entry: Decodable {}
extension BandStack.ReturnMode: Decodable {}
extension BandStack.LastRecall: Decodable {}

extension MirrorStore {
    /// The Core's band stack; nil on a Core that has none, or until it knows a radio.
    public var bandStack: BandStack? {
        guard let object = object(BandStack.objectKey), case .text(let json)? = object["json"],
              case .int(let revision)? = object["revision"] else {
            return nil
        }
        return BandStack(json: json, revision: revision)
    }
}
