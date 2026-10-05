// NereusSDR for iOS: producer-pinned full TX Profile snapshot and shared flow regressions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

// Source-only proposal: the initial producer corpus is unreleased and its
// correlated edits must fail isolatedEdit until the reviewed replacement lands.
@MainActor
@Suite("Producer TX Profile full60", .serialized)
struct TxProfileWatch60Tests {
    private struct Mapping: Decodable {
        let id: String
        let kind: String
        let ordinal: UInt16
        let saved_keys: [String]
    }
    private struct Corpus {
        let initial: [LinkMessage]
        let audio: String
        let cases: [[String: Any]]
        let mapping: [Mapping]
        let baseline: [String: LinkMessage.PropertyEntry]

        init() throws {
            let root = try #require(Bundle.module.url(forResource: "TxProfileWatch60", withExtension: nil))
            let data = try Data(contentsOf: root.appendingPathComponent("tx-profile-watch-60.json"))
            let json = try #require(JSONSerialization.jsonObject(with: data) as? [String: Any])
            initial = try #require(json["initial"] as? [Any]).map(Self.decode)
            audio = try LinkJSON(foundation: #require(json["audio"])).compactText
            cases = try #require(json["cases"] as? [[String: Any]])
            mapping = try JSONDecoder().decode([Mapping].self, from: Data(contentsOf: root.appendingPathComponent("typed60-mapping.json")))
            let transmit = try #require(initial.compactMap { message -> LinkMessage.ObjectCreate? in
                if case .objectCreate(let object) = message, object.key == "transmit" { return object }
                return nil
            }.first)
            baseline = Dictionary(uniqueKeysWithValues: transmit.properties.map { ($0.name, $0) })
        }
        static func decode(_ raw: Any) throws -> LinkMessage {
            // Preserve the producer's kinds, ordinals and raw string values.
            // The corpus contains parsed SessionMessages JSON objects.
            try LinkCodec.decode(LinkJSON(foundation: raw).compactText)
        }
        func delta(_ index: Int, key: String = "delta") throws -> LinkMessage.Delta {
            guard case .delta(let delta) = try Self.decode(#require(cases[index][key])) else {
                throw CorpusError.notDelta
            }
            return delta
        }
        func isolatedEdit(_ index: Int) throws -> LinkMessage.Delta {
            let row = mapping[index]
            try #require(cases[index]["field"] as? String == row.id)
            let edit = try delta(index)
            try #require(edit.key == "transmit")
            try #require(!edit.properties.contains { $0.name == "activeTxProfile" },
                         "Producer edit must not contain an active-profile event: \(row.id)")
            let target = try #require(edit.properties.first { $0.name == row.id })
            let original = try #require(baseline[row.id])
            try #require(target.ordinal == row.ordinal && target.kind.rawValue == row.kind)
            try #require(original.ordinal == target.ordinal && original.kind == target.kind)
            let watch = Set(mapping.map(\.id))
            let changed = edit.properties.filter {
                watch.contains($0.name) && baseline[$0.name]?.value != $0.value
            }.map(\.name)
            try #require(changed == [row.id],
                         "Exactly one watched field must differ from the full baseline; \(row.id): \(changed)")
            try #require(Set(edit.properties.map(\.name)).count == edit.properties.count)
            let restored = try delta(index, key: "restored")
            try #require(restored.properties.first { $0.name == row.id } == target,
                         "Actual profile restore must retain the exact edited wire entry")
            let saved = try #require(cases[index]["saved"] as? [String: String])
            try #require(Set(saved.keys) == Set(row.saved_keys))
            return edit
        }
    }
    private enum CorpusError: Error { case notDelta }

    @MainActor private final class Rig {
        let station: ScriptedStation
        let clock: ManualLinkClock
        let session: StationSession
        let mirror: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let dispatcher: SetupControlDispatcher
        let recorder: EventRecorder
        var flow: SetupTxProfileFlow { dispatcher.txProfiles }

        init() {
            let station = ScriptedStation(certificateSHA256: Data(repeating: 1, count: 32))
            let clock = ManualLinkClock()
            self.station = station; self.clock = clock
            let session = StationSession(endpoint: StationEndpoint(host: "fixture.invalid"),
                trust: station.trust, authenticator: TokenAuthenticator(token: "fixture-token"),
                clock: clock, transportFactory: station.factory)
            self.session = session
            let mirror = MirrorStore(session: session)
            let settings = SettingsProxyClient(session: session)
            let commands = CommandClient(session: session, clock: clock)
            self.mirror = mirror; self.settings = settings; self.commands = commands
            let feed = SetupDescriptionFeed(store: mirror)
            self.feed = feed
            dispatcher = SetupControlDispatcher(store: mirror, feed: feed, settings: settings, commands: commands,
                captureSender: { { message, permit in try await session.send(message, permit: permit) } },
                selectedSlice: { nil }, selectionChanges: Just<Int?>(nil).eraseToAnyPublisher(), phone: nil)
            recorder = EventRecorder(session, forward: { event in
                await MainActor.run {
                    mirror.handle(event); settings.handle(event)
                }
                await commands.handle(event)
            })
        }
        var transport: ScriptedTransport { get throws { try #require(station.latest) } }
        func deliver(_ message: LinkMessage) async throws {
            let wire = LinkCodec.encode(message)
            let expected = try LinkCodec.decode(wire)
            let startingCount = recorder.messages.count
            try await transport.deliver(wire)
            // Keep the predicate on MainActor and wait for this exact wire
            // message after the captured position. Earlier identical messages
            // and unrelated queued events cannot satisfy the forwarding barrier.
            try #require(await Self.until {
                self.recorder.messages.dropFirst(startingCount).contains(expected)
            })
        }
        func connect(_ corpus: Corpus, complete: Bool = true) async throws {
            await session.connect()
            try await deliver(FixtureReplay.stationHello())
            try await deliver(FixtureReplay.accepted)
            try await deliver(FixtureReplay.capabilities([
                "setupDescriptionVersion": .i64(15), "transmitSettingsVersion": .i64(3), "txPermitted": .bool(true)]))
            try await deliver(.schema(.init(className: "SetupDescription", fields: [
                .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "audio", kind: .utf8)])))
            try await deliver(.objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                .init(name: "revision", value: .i64(1)), .init(name: "audio", value: .utf8(corpus.audio))])))
            // Surrounding session gates are the established phone flow rig's
            // envelope; every TransmitModel entry/schema comes from the Core.
            try await deliver(.objectCreate(.init(key: "txState", className: "TransmitState", properties: [
                .init(name: "keyed", value: .bool(false)), .init(name: "tuning", value: .bool(false)),
                .init(name: "twoTone", value: .bool(false)), .init(name: "txEnding", value: .bool(false))])))
            try await deliver(.settingsSnapshot(.init(properties: [])))
            for message in corpus.initial where message != .snapshotComplete { try await deliver(message) }
            if complete { try await finishSnapshot() }
        }
        func finishSnapshot() async throws {
            try await deliver(.snapshotComplete)
            try #require(await Self.until { self.recorder.states.contains(.ready) })
            try #require(await Self.until { self.feed.description(for: "audio") != nil })
        }
        func active() throws -> SetupDescription.Control {
            let description = try #require(feed.description(for: "audio"))
            return try #require(description.pages.flatMap(\.sections).flatMap(\.controls)
                .first { $0.id == "audio.txProfile.activeProfile" })
        }
        func invokes() throws -> [LinkMessage.CommandInvoke] {
            try transport.pending.map(LinkCodec.decode).compactMap {
                if case .commandInvoke(let invoke) = $0 { return invoke }; return nil
            }
        }
        func invoke(_ index: Int) async throws -> LinkMessage.CommandInvoke {
            try #require(await Self.until { ((try? self.invokes().count) ?? 0) > index })
            return try invokes()[index]
        }
        func answer(_ invoke: LinkMessage.CommandInvoke, accepted: Bool = true) async throws {
            try await deliver(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: accepted,
                reason: accepted ? "" : "Exact Core TX refusal", affected: [], values: [])))
        }
        func echo(_ name: String) async throws {
            try await deliver(.delta(.init(key: "transmit", properties: [.init(name: "activeTxProfile", value: .utf8(name))])))
        }
        static func until(_ condition: () -> Bool) async -> Bool {
            for _ in 0..<2_000 {
                if condition() { return true }
                await Task.yield()
            }
            return condition()
        }
        func stop() async { await session.disconnect() }
    }

    @Test func completeSnapshotRetainsEveryTypedValueBeforeFlowAttaches() async throws {
        let corpus = try Corpus(), rig = Rig()
        try #require(corpus.mapping.count == 60 && Set(corpus.mapping.map(\.id)).count == 60)
        try #require(corpus.cases.count == 60)
        try await rig.connect(corpus, complete: false)
        #expect(!rig.mirror.isSnapshotComplete && rig.feed.description(for: "audio") == nil)
        let schema = try #require(corpus.initial.compactMap { message -> LinkMessage.Schema? in
            if case .schema(let schema) = message, schema.className == "TransmitModel" { return schema }
            return nil
        }.first)
        let fields = Dictionary(uniqueKeysWithValues: schema.fields.map { ($0.name, $0) })
        let transmit = try #require(rig.mirror.object("transmit"))
        #expect(transmit.values == corpus.baseline.mapValues { MirrorValue($0.value) })
        for row in corpus.mapping {
            let property = try #require(corpus.baseline[row.id])
            #expect(property.ordinal == row.ordinal && property.kind.rawValue == row.kind)
            #expect(fields[row.id]?.ordinal == row.ordinal && fields[row.id]?.kind.rawValue == row.kind)
            #expect(transmit[row.id] == MirrorValue(property.value))
        }
        #expect(corpus.baseline["micGainDb"]?.kind == .i64)
        #expect(corpus.baseline["filterLow"]?.kind == .i64 && corpus.baseline["filterHigh"]?.kind == .i64)
        #expect(corpus.baseline["filterLowHz"] == nil)
        let doubles = ["dexpDetectorTauMs", "dexpAttackTimeMs", "dexpReleaseTimeMs", "dexpExpansionRatioDb",
            "dexpHysteresisRatioDb", "dexpLookAheadMs", "dexpLowCutHz", "dexpHighCutHz"]
        #expect(doubles.allSatisfy { corpus.baseline[$0]?.kind == .f64 })
        for id in ["txEqBandsJson", "txEqFreqsJson", "cfcEqFreqJson", "cfcCompressionJson", "cfcPostEqBandGainJson"] {
            guard case .utf8(let raw)? = corpus.baseline[id]?.value else { Issue.record("missing array \(id)"); continue }
            let array = try #require(JSONSerialization.jsonObject(with: Data(raw.utf8)) as? [NSNumber])
            #expect(array.count == 10 && array.allSatisfy { $0.doubleValue == Double($0.int64Value) })
            #expect(raw == "[" + array.map { String($0.int64Value) }.joined(separator: ",") + "]")
            #expect(transmit[id] == .text(raw))
        }
        for id in ["txEqParaEqData", "cfcParaEqData"] {
            guard case .utf8(let raw)? = corpus.baseline[id]?.value else { Issue.record("missing raw \(id)"); continue }
            #expect(transmit[id] == .text(raw))
        }
        try await rig.finishSnapshot()
        let active = try rig.active()
        #expect(active.modern?.profileUnsavedChanges?.watch == corpus.mapping.map(\.id))
        rig.flow.open(active, in: "audio")
        #expect(!rig.flow.dirty && rig.flow.currentName == "Alpha")
        // Repeat actual getter output as a delta, preserving exact wire kinds.
        try await rig.deliver(.delta(.init(key: "transmit", properties: Array(corpus.baseline.values))))
        #expect(!rig.flow.dirty)
        #expect(try rig.invokes().isEmpty)
        await rig.stop()
    }

    @Test(arguments: Array(0..<60))
    func isolatedActualEditsLatchDirtyAndCancelRetainsHistory(_ index: Int) async throws {
        let corpus = try Corpus()
        let edit = try corpus.isolatedEdit(index)
        let rig = Rig(); try await rig.connect(corpus)
        let flow = rig.flow; flow.open(try rig.active(), in: "audio")
        try #require(!flow.dirty)
        try await rig.deliver(.delta(edit))
        let field = corpus.mapping[index].id
        let target = try #require(edit.properties.first { $0.name == field })
        #expect(rig.mirror.object("transmit")?[field] == MirrorValue(target.value))
        #expect(flow.dirty, "Actual isolated \(field) change must be observed")
        // Return only this field to its captured opening value, without a
        // fabricated profile-selection echo that would clear dirty history.
        try await rig.deliver(.delta(.init(key: "transmit", properties: [#require(corpus.baseline[field])])))
        #expect(flow.dirty)
        flow.choose("Beta")
        #expect(flow.question == .unsaved(title: "Unsaved Profile Changes",
            text: "The current profile (\"Alpha\") has been modified.\nSave before switching?"))
        #expect(try rig.invokes().isEmpty)
        flow.cancel()
        #expect(flow.question == nil && flow.dirty && flow.currentName == "Alpha")
        #expect(try rig.invokes().isEmpty)
        // Actual producer profile restore retains its raw string/scalar target
        // through the session and mirror too; no local normalization is used.
        try await rig.deliver(.delta(corpus.delta(index, key: "restored")))
        #expect(rig.mirror.object("transmit")?[field] == MirrorValue(target.value))
        #expect(flow.dirty && flow.currentName == "Alpha")
        await rig.stop()
    }

    @Test(arguments: ["discard", "save", "refusal", "timeout", "owner", "session"])
    func fullSnapshotUsesExistingSharedDecisionContract(_ decision: String) async throws {
        let corpus = try Corpus(), rig = Rig()
        let edit = try corpus.isolatedEdit(0)
        try await rig.connect(corpus)
        let flow = rig.flow; flow.open(try rig.active(), in: "audio")
        try await rig.deliver(.delta(edit))
        let owner = UUID(); flow.choose("Beta", owner: owner)
        try #require(flow.question != nil && flow.dirty)
        let task = Task {
            if decision == "discard" { await flow.discardAndSwitch() }
            else { await flow.saveAndSwitch() }
        }
        let first = try await rig.invoke(0)
        #expect(first.verb == (decision == "discard" ? "txProfile.select" : "txProfile.save"))
        #expect(first.args == [.init(name: "name", value: .utf8(decision == "discard" ? "Beta" : "Alpha"))])
        let countBeforeAnswer = try rig.invokes().count
        #expect(flow.currentName == "Alpha" && countBeforeAnswer == 1)
        switch decision {
        case "timeout":
            try #require(await Rig.until { rig.clock.pendingDueTimes.contains(5_000) })
            await rig.clock.advance(by: 5_000)
        case "refusal": try await rig.answer(first, accepted: false)
        case "owner": flow.retire(owner: owner); try await rig.answer(first)
        case "session": await rig.stop()
        default: try await rig.answer(first)
        }
        if decision == "save" {
            let select = try await rig.invoke(1)
            #expect(select.verb == "txProfile.select" && select.args == [.init(name: "name", value: .utf8("Beta"))])
            try await rig.answer(first); try await rig.answer(select)
        }
        await task.value
        if decision == "save" || decision == "discard" {
            #expect(flow.currentName == "Alpha")
            try await rig.echo("Beta")
            #expect(flow.currentName == "Beta" && !flow.dirty)
            #expect(try rig.invokes().count == (decision == "save" ? 2 : 1))
        } else {
            #expect(flow.currentName == "Alpha" && flow.dirty && !flow.busy && flow.question == nil)
            if decision == "timeout" || decision == "refusal" {
                #expect(flow.problem == (decision == "timeout" ? SetupControlDispatcher.noAnswerReason : "Exact Core TX refusal"))
                try await rig.answer(first)
            }
            #expect(try rig.invokes().count == 1, "Retired/refused/timed out save cannot resume select")
        }
        await rig.stop()
    }

    @Test func setupRevisionAndDerivedEditorChangesDoNotDirtyRawProfile() async throws {
        let corpus = try Corpus(), rig = Rig(); try await rig.connect(corpus)
        let flow = rig.flow; flow.open(try rig.active(), in: "audio")
        let before = try #require(rig.mirror.object("transmit")).values
        try await rig.deliver(.delta(.init(key: "setup", properties: [.init(name: "revision", value: .i64(2))])))
        // These are non-watch editor projections; they do not replace the raw
        // saved para-EQ/array strings. The test needs no guessed editor schema.
        try await rig.deliver(.delta(.init(key: "transmit", properties: [
            .init(name: "txEqCurve", value: .utf8("{\"revision\":2}")),
            .init(name: "cfcProfile", value: .utf8("{\"revision\":2}"))])))
        try #require(await Rig.until { rig.feed.revision == 2 })
        #expect(!flow.dirty && flow.currentName == "Alpha")
        for row in corpus.mapping { #expect(rig.mirror.object("transmit")?[row.id] == before[row.id]) }
        #expect(try rig.invokes().isEmpty)
        await rig.stop()
    }
}
