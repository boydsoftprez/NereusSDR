// NereusSDR for iOS: the catalogue feed over the mirror, its revisions, sessions and older Cores
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import NereusModels
import Testing
@testable import NereusMirror

/// Link document section 7.4 (R-IOS-06, R-IOS-27, D23). Every catalogue is
/// the Core's own, read from its fixtures at run time (D4).
@MainActor
@Suite struct CatalogFeedTests {
    private let sent = SentMessages()

    /// A store with a feed on it, after `messages`, once the feed has read them.
    private func feed(after messages: [LinkMessage]) async -> (MirrorStore, CatalogFeed) {
        let store = MirrorStore(send: sent.sender)
        let feed = CatalogFeed(store: store)
        await apply(messages, to: store)
        return (store, feed)
    }

    /// Applies `messages`, then lets the feed read the store.
    private func apply(_ messages: [LinkMessage], to store: MirrorStore) async {
        for message in messages {
            store.apply(message)
        }
        // The feed reads on the main actor's next turn; two turns cover a
        // refresh that queues after another.
        for _ in 0..<4 {
            await Task.yield()
        }
    }

    private static func json(_ fixture: String) throws -> String {
        for message in try FixtureReplay.stationMessages("session-\(fixture)") {
            if case .objectCreate(let create) = message, create.key == CatalogFeed.objectKey,
               case .utf8(let json)? = create.properties.first(where: { $0.name == "json" })?.value {
                return json
            }
        }
        throw LinkFixtureLoader.Malformed(description: "\(fixture) has no catalogue")
    }

    private static func opening(minor: UInt16 = 11, catalogVersion: Int64 = 1) -> [LinkMessage] {
        [FixtureReplay.stationHello(minor: minor), FixtureReplay.accepted,
         FixtureReplay.capabilities(["stationCatalogVersion": .i64(catalogVersion)])]
    }

    private static func create(json: String, revision: Int64) -> LinkMessage {
        .objectCreate(LinkMessage.ObjectCreate(key: CatalogFeed.objectKey, className: CatalogFeed.className,
                                               properties: [
                                                   LinkMessage.PropertyEntry(ordinal: 0, name: "json", value: .utf8(json)),
                                                   LinkMessage.PropertyEntry(ordinal: 1, name: "revision",
                                                                             value: .i64(revision)),
                                               ]))
    }

    private static func delta(json: String, revision: Int64) -> LinkMessage {
        .delta(LinkMessage.Delta(key: CatalogFeed.objectKey, properties: [
            LinkMessage.PropertyEntry(ordinal: 0, name: "json", value: .utf8(json)),
            LinkMessage.PropertyEntry(ordinal: 1, name: "revision", value: .i64(revision)),
        ]))
    }

    // MARK: Tests

    @Test(arguments: ["catalog-anan-g2", "catalog-hermes-lite-2"])
    func theCoresCatalogueReachesTheFeed(_ fixture: String) async throws {
        let (_, feed) = await feed(after: try FixtureReplay.stationMessages("session-\(fixture)"))
        let expected = try #require(StationCatalog.parse(json: try Self.json(fixture)))
        #expect(feed.catalog == expected)
        #expect(feed.revision == 1)
        #expect(!feed.needsNewerCore)
    }

    @Test func theSessionFixturesStandInIsNoCatalogueYet() async throws {
        let (store, feed) = await feed(after: try FixtureReplay.stationMessages("session-connect-connectable"))
        #expect(store.object(CatalogFeed.objectKey) != nil)
        #expect(feed.catalog == nil)
        #expect(!feed.needsNewerCore)
    }

    @Test func aNewerRevisionReplacesTheCatalogueAcrossTheWrapAndAnOlderOneIsIgnored() async throws {
        let g2 = try Self.json("catalog-anan-g2")
        let hl2 = try Self.json("catalog-hermes-lite-2")
        let first = try #require(StationCatalog.parse(json: g2))
        let second = try #require(StationCatalog.parse(json: hl2))
        let (store, feed) = await feed(after: Self.opening() + [Self.create(json: g2, revision: 4_294_967_295),
                                                                 .snapshotComplete])
        #expect(feed.catalog == first)
        #expect(feed.revision == 4_294_967_295)

        // 5 comes after 4294967295 when the u32 wraps.
        await apply([Self.delta(json: hl2, revision: 5)], to: store)
        #expect(feed.catalog == second)
        #expect(feed.revision == 5)

        // An older revision, and the same one again, change nothing.
        await apply([Self.delta(json: g2, revision: 4)], to: store)
        #expect(feed.catalog == second)
        #expect(feed.revision == 5)
        await apply([Self.delta(json: g2, revision: 5)], to: store)
        #expect(feed.catalog == second)
        await apply([Self.delta(json: g2, revision: 4_294_967_295)], to: store)
        #expect(feed.catalog == second)
        #expect(feed.revision == 5)
    }

    @Test func anUnreadableCatalogueChangesNothing() async throws {
        let g2 = try Self.json("catalog-anan-g2")
        let (store, feed) = await feed(after: Self.opening() + [Self.create(json: g2, revision: 1), .snapshotComplete])
        let held = feed.catalog
        #expect(held != nil)
        await apply([Self.delta(json: "{", revision: 2)], to: store)
        #expect(feed.catalog == held)
        #expect(feed.revision == 1)
        // A revision out of the u32 range is not one the Core sends.
        await apply([Self.delta(json: "", revision: -1)], to: store)
        await apply([Self.delta(json: "", revision: 4_294_967_296)], to: store)
        #expect(feed.catalog == held)
        // An empty catalogue at a newer revision is none yet.
        await apply([Self.delta(json: "", revision: 3)], to: store)
        #expect(feed.catalog == nil)
        #expect(feed.revision == 3)
    }

    @Test func aNewSessionTakesItsFirstValueWhateverItsRevision() async throws {
        let g2 = try Self.json("catalog-anan-g2")
        let hl2 = try Self.json("catalog-hermes-lite-2")
        let (store, feed) = await feed(after: Self.opening() + [Self.create(json: g2, revision: 40), .snapshotComplete])
        #expect(feed.revision == 40)

        // The link drops; the values stay readable meanwhile.
        store.handle(.stateChanged(.receivingSnapshot))
        store.handle(.stateChanged(.ready))
        await apply([], to: store)
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        await apply([], to: store)
        #expect(feed.catalog == StationCatalog.parse(json: g2))

        // A restarted Core counts again from 1.
        await apply(Self.opening() + [Self.create(json: hl2, revision: 1)], to: store)
        #expect(feed.catalog == StationCatalog.parse(json: hl2))
        #expect(feed.revision == 1)
        await apply([.snapshotComplete], to: store)
        #expect(feed.revision == 1)
        // From here revisions are ordered again.
        await apply([Self.delta(json: g2, revision: 0)], to: store)
        #expect(feed.revision == 1)
        await apply([Self.delta(json: g2, revision: 2)], to: store)
        #expect(feed.catalog == StationCatalog.parse(json: g2))
    }

    @Test func aSnapshotWithoutTheObjectLeavesNoCatalogue() async throws {
        let g2 = try Self.json("catalog-anan-g2")
        let (store, feed) = await feed(after: Self.opening() + [Self.create(json: g2, revision: 1), .snapshotComplete])
        #expect(feed.catalog != nil)
        await apply(Self.opening() + [.snapshotComplete], to: store)
        #expect(store.object(CatalogFeed.objectKey) == nil)
        #expect(feed.catalog == nil)
        #expect(feed.revision == nil)
    }

    @Test(arguments: [(UInt16(10), Int64(1)), (UInt16(11), Int64(0))])
    func aCoreWithoutACatalogueNeedsANewerCore(minor: UInt16, version: Int64) async throws {
        let g2 = try Self.json("catalog-anan-g2")
        let (_, feed) = await feed(after: Self.opening(minor: minor, catalogVersion: version)
            + [Self.create(json: g2, revision: 1), .snapshotComplete])
        #expect(feed.needsNewerCore)
        #expect(feed.catalog == nil)
        #expect(CatalogFeed.needsNewerCoreText == "Needs a newer Core")
    }

    @Test func beforeAnyCoreHasSpokenNothingIsClaimed() async {
        let (_, feed) = await feed(after: [])
        #expect(feed.catalog == nil)
        #expect(!feed.needsNewerCore)
    }
}
