// NereusSDR for iOS: Setup feed availability across live mirror sessions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@MainActor
@Suite struct SetupDescriptionFeedTests {
    private func json(_ page: String, version: Int = 1) -> String {
        """
        {"version":\(version),"category":{"id":"general","title":"General","where":"mixed"},"pages":[{"id":"general.\(page)","title":"\(page)","where":"station","sections":[{"title":"Choices","controls":[{"id":"general.\(page).one","label":"One","tooltip":"","kind":"toggle","binding":{"property":{"object":"radio","name":"enabled"}},"applies":"live"}]}]}]}
        """
    }
    private func opening(_ version: Int64 = 1) -> [LinkMessage] {
        [FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
         FixtureReplay.capabilities(["setupDescriptionVersion": .i64(version)]),
         schema()]
    }
    private func schema(paFirst: Bool = false) -> LinkMessage {
        .schema(LinkMessage.Schema(className: "SetupDescription", fields: [
            .init(ordinal: paFirst ? 2 : 0, name: "general", kind: .utf8),
            .init(ordinal: 1, name: "revision", kind: .i64),
            .init(ordinal: paFirst ? 0 : 2, name: "pa", kind: .utf8),
        ]))
    }
    private func create(_ general: String, pa: String = "", revision: Int64 = 1) -> LinkMessage {
        .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
            .init(ordinal: 0, name: "general", value: .utf8(general)),
            .init(ordinal: 1, name: "revision", value: .i64(revision)),
            .init(ordinal: 2, name: "pa", value: .utf8(pa)),
        ]))
    }
    private func apply(_ messages: [LinkMessage], to store: MirrorStore) async {
        for message in messages { store.apply(message) }
        for _ in 0..<5 { await Task.yield() }
    }
    private func setup() -> (MirrorStore, SetupDescriptionFeed) {
        let store = MirrorStore(send: { _ in })
        return (store, SetupDescriptionFeed(store: store))
    }

    @Test func onlyCurrentCompleteSnapshotIsUsable() async {
        let (store, feed) = setup()
        await apply(opening() + [create(json("first"))], to: store)
        #expect(feed.description(for: "general") == nil)
        await apply([.snapshotComplete], to: store)
        #expect(feed.description(for: "general")?.pages[0].id == "general.first")
        let generation = feed.generation
        store.handle(.stateChanged(.receivingSnapshot))
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(feed.description(for: "general") == nil)
        await apply(opening() + [create(json("second"))], to: store)
        #expect(feed.description(for: "general") == nil)
        await apply([.snapshotComplete], to: store)
        #expect(feed.description(for: "general")?.pages[0].id == "general.second")
        #expect(feed.generation > generation)
    }

    @Test func emptyMalformedAndVersionMismatchStayDistinct() async {
        let (store, feed) = setup()
        await apply(opening() + [create("") , .snapshotComplete], to: store)
        #expect(feed.status(for: "general") == .unpublished)
        await apply([.delta(.init(key: "setup", properties: [.init(name: "general", value: .utf8("{")),
                                                             .init(name: "revision", value: .i64(2))]))], to: store)
        if case .unavailable(let reason) = feed.status(for: "general") { #expect(!reason.isEmpty) }
        else { Issue.record("Malformed category was not unavailable") }
        await apply([.delta(.init(key: "setup", properties: [.init(name: "general", value: .utf8(json("future", version: 2))),
                                                             .init(name: "revision", value: .i64(3))]))], to: store)
        #expect(feed.description(for: "general") == nil)
        if case .unavailable = feed.status(for: "general") {} else { Issue.record("Version mismatch was not unavailable") }
    }

    @Test func replacementRevisionCapabilityAndObjectRemovalInvalidate() async {
        let (store, feed) = setup()
        await apply(opening() + [create(json("first")), .snapshotComplete], to: store)
        let first = feed.generation
        await apply([.delta(.init(key: "setup", properties: [.init(name: "general", value: .utf8(json("later"))),
                                                             .init(name: "revision", value: .i64(2))]))], to: store)
        #expect(feed.description(for: "general")?.pages[0].id == "general.later")
        #expect(feed.generation > first)
        await apply([.capabilities(.init(properties: []))], to: store)
        #expect(feed.description(for: "general") == nil)
        await apply([FixtureReplay.capabilities(["setupDescriptionVersion": .i64(1)])], to: store)
        #expect(feed.description(for: "general") != nil)
        await apply([.objectDestroy(.init(key: "setup", className: "SetupDescription"))], to: store)
        #expect(feed.description(for: "general") == nil)
    }

    @Test func schemaAfterRevisionIncludesPaAndCanGainNewPage() async {
        let (store, feed) = setup()
        await apply(opening() + [create(json("one")), .snapshotComplete], to: store)
        #expect(feed.categoryOrder == ["general", "pa"])
        let pa = json("values").replacingOccurrences(of: "general", with: "pa")
        await apply([.delta(.init(key: "setup", properties: [.init(name: "pa", value: .utf8(pa)),
                                                             .init(name: "revision", value: .i64(2))]))], to: store)
        #expect(feed.description(for: "pa")?.pages[0].id == "pa.values")
        await apply([.delta(.init(key: "setup", properties: [.init(name: "general", value: .utf8(json("newPage"))),
                                                             .init(name: "revision", value: .i64(3))]))], to: store)
        #expect(feed.description(for: "general")?.pages[0].id == "general.newPage")
    }

    @Test func objectReplacementAndUnsupportedCapabilityRetireOldDescriptions() async {
        let (store, feed) = setup()
        await apply(opening() + [create(json("one")), .snapshotComplete], to: store)
        let first = feed.generation
        #expect(feed.isCurrent(first, category: "general"))
        store.apply(.delta(.init(key: "setup", properties: [.init(name: "revision", value: .i64(2))])))
        #expect(!feed.isCurrent(first, category: "general"))
        await apply([], to: store)
        await apply([.objectDestroy(.init(key: "setup", className: "SetupDescription"))], to: store)
        #expect(feed.description(for: "general") == nil)
        await apply([create(json("replacement"), revision: 1)], to: store)
        #expect(feed.description(for: "general")?.pages[0].id == "general.replacement")
        #expect(feed.generation > first)
        await apply([FixtureReplay.capabilities(["setupDescriptionVersion": .i64(25)])], to: store)
        #expect(feed.description(for: "general") == nil)
        if case .unavailable = feed.status(for: "general") {} else { Issue.record("Unsupported version was usable") }
    }

    @Test func secondSessionNeedsItsOwnSetupSchemaAndLateSchemaRecovers() async {
        let (store, feed) = setup()
        await apply(opening() + [create(json("first")), .snapshotComplete], to: store)
        #expect(feed.description(for: "general") != nil)
        store.handle(.stateChanged(.receivingSnapshot))
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        await apply(Array(opening().dropLast()) + [create(json("second")), .snapshotComplete], to: store)
        #expect(feed.description(for: "general") == nil)
        if case .unavailable(let reason) = feed.status(for: "general") {
            #expect(reason.contains("schema"))
        } else { Issue.record("Missing current Setup schema was usable") }
        await apply([schema()], to: store)
        #expect(feed.description(for: "general")?.pages[0].id == "general.second")
    }

    @Test func categoryProjectionUsesExactVersionAtGlobalMaximum() async {
        let (store, feed) = setup()
        let general = json("main", version: 3)
        let pa = json("values", version: 5).replacingOccurrences(of: "general", with: "pa")
        await apply(opening(8) + [create(general, pa: pa), .snapshotComplete], to: store)
        #expect(feed.description(for: "general")?.version == 3)
        #expect(feed.description(for: "pa")?.version == 5)
        let first = feed.generation
        let oldPa = json("values", version: 3).replacingOccurrences(of: "general", with: "pa")
        store.apply(.delta(.init(key: "setup", properties: [.init(name: "pa", value: .utf8(oldPa)),
                                                        .init(name: "revision", value: .i64(2))])))
        #expect(!feed.isCurrent(first, category: "pa"))
        await apply([], to: store)
        #expect(feed.description(for: "pa") == nil)
        #expect(feed.description(for: "general")?.version == 3)
        await apply([.delta(.init(key: "setup", properties: [.init(name: "pa", value: .utf8(pa)),
                                                     .init(name: "revision", value: .i64(3))]))], to: store)
        #expect(feed.description(for: "pa")?.version == 5)
        await apply([FixtureReplay.capabilities(["setupDescriptionVersion": .i64(25)])], to: store)
        #expect(feed.description(for: "pa") == nil)
    }

    @Test func appearanceAndDisplayHoldPriorShapeUntilTheirNextVersion() async {
        let categories = ["hardware", "pa", "display", "appearance", "general"]
        for maximum in [6, 7, 8, 9, 10, 11, 12] {
            let (store, feed) = setup()
            let versions = ["hardware": 6, "pa": 5, "display": maximum < 8 ? 4 : maximum,
                            "appearance": maximum < 7 ? 4 : maximum < 12 ? 7 : 12, "general": 3]
            let fields = [LinkMessage.SchemaField(ordinal: 0, name: "revision", kind: .i64)]
                + categories.enumerated().map { index, name in
                    LinkMessage.SchemaField(ordinal: UInt16(index + 1), name: name, kind: .utf8)
                }
            let properties = [LinkMessage.PropertyEntry(ordinal: 0, name: "revision", value: .i64(1))]
                + categories.enumerated().map { index, name in
                    LinkMessage.PropertyEntry(ordinal: UInt16(index + 1), name: name,
                        value: .utf8(json("shape", version: versions[name]!)
                            .replacingOccurrences(of: "general", with: name)))
                }
            await apply([FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                         FixtureReplay.capabilities(["setupDescriptionVersion": .i64(Int64(maximum))]),
                         .schema(.init(className: "SetupDescription", fields: fields)),
                         .objectCreate(.init(key: "setup", className: "SetupDescription", properties: properties)),
                         .snapshotComplete], to: store)
            for name in categories { #expect(feed.description(for: name)?.version == versions[name]) }
        }
    }

    @Test func schemaOnlyReplacementRetiresGestureAndRefreshesOrder() async {
        let (store, feed) = setup()
        await apply(opening() + [create(json("first")), .snapshotComplete], to: store)
        let before = feed.generation
        #expect(feed.categoryOrder == ["general", "pa"])
        store.apply(schema(paFirst: true))
        #expect(!feed.isCurrent(before, category: "general"))
        await apply([], to: store)
        #expect(feed.categoryOrder == ["pa", "general"])
        #expect(feed.description(for: "general") != nil)
        #expect(feed.generation > before)
        let reorderedGeneration = feed.generation
        store.apply(schema(paFirst: true))
        #expect(!feed.isCurrent(reorderedGeneration, category: "general"))
        await apply([], to: store)
        #expect(feed.categoryOrder == ["pa", "general"])
        #expect(feed.generation > reorderedGeneration)
    }
}
