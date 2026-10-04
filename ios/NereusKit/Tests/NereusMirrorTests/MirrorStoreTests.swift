// NereusSDR for iOS: the mirror of the Core's objects, its writes, staleness, capabilities and telemetry
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@MainActor
@Suite struct MirrorStoreTests {
    private let sent = SentMessages()

    private func store(clock: any LinkClock = SystemLinkClock()) -> MirrorStore {
        MirrorStore(send: sent.sender, clock: clock)
    }

    /// A store that has taken the whole connect sequence of `fixture`.
    private func replayed(_ fixture: String = "session-connect-connectable") throws -> MirrorStore {
        let store = store()
        for message in try FixtureReplay.stationMessages(fixture) {
            store.apply(message)
        }
        return store
    }

    @Test func transmitSwrFreshnessAndRepeatedSamples() async throws {
        let clock = ManualLinkClock()
        let store = MirrorStore(send: sent.sender, clock: clock)
        for message in try FixtureReplay.stationMessages("session-connect-connectable") { store.apply(message) }
        func state(_ keyed: Bool, swr: Double? = nil) {
            var fields: [LinkMessage.PropertyEntry] = [.init(ordinal: 0, name: "keyed", value: .bool(keyed))]
            if let swr { fields.append(.init(ordinal: 11, name: "swr", value: .f64(swr))) }
            if store.object("txState") == nil {
                store.apply(.objectCreate(.init(key: "txState", className: "TransmitState", properties: fields)))
            } else { store.apply(.delta(.init(key: "txState", properties: fields))) }
        }
        #expect(store.currentTxSwr == nil)
        state(false, swr: 3.2)
        #expect(store.currentTxSwr == nil, "receive samples never warn")
        state(true)
        #expect(store.currentTxSwr == nil, "keying must not revive a receive sample")
        state(true, swr: 3.2)
        #expect(store.currentTxSwr == 3.2)
        let serial = store.txSwrSampleSerial
        await clock.advance(by: 1500)
        state(true, swr: 3.2)
        #expect(store.txSwrSampleSerial > serial, "equal-valued samples renew freshness")
        await clock.advance(by: 1500)
        #expect(store.currentTxSwr == 3.2)
        await clock.advance(by: 501)
        #expect(store.currentTxSwr == nil, "a sample expires after two seconds without readings")
        for invalid in [0.0, 0.5, -400, Double.infinity, Double.nan] {
            state(true, swr: invalid)
            #expect(store.currentTxSwr == nil)
        }
        state(true, swr: 3.2)
        #expect(store.currentTxSwr == 3.2)
        state(false)
        #expect(store.currentTxSwr == nil)
        state(true)
        #expect(store.currentTxSwr == nil, "a prior key's reading must not survive the next key")
        state(true, swr: 3.2)
        store.handle(.stateChanged(.ready))
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(store.currentTxSwr == nil, "a stale session never warns")
    }

    // MARK: The snapshot

    @Test func aReplayedConnectSequenceBuildsTheObjectsClassesAndValues() throws {
        let messages = try FixtureReplay.stationMessages("session-connect-connectable")
        let store = store()
        for message in messages {
            store.apply(message)
        }
        #expect(store.isSnapshotComplete)
        #expect(!store.isStale)
        #expect(store.agreedMinor == 11)

        let creates = messages.compactMap { message -> LinkMessage.ObjectCreate? in
            if case .objectCreate(let create) = message {
                return create
            }
            return nil
        }
        // With the Core's spot sources (SpotSourceHost, parity Task 19).
        #expect(creates.count == 10)
        #expect(creates.contains { $0.key == "spotSources" && $0.className == "SpotSourceHost" })
        #expect(store.objectKeys == creates.map(\.key))
        for create in creates {
            let object = try #require(store.object(create.key))
            #expect(object.className == create.className)
            #expect(object.values.count == create.properties.count, "\(create.key)")
            for entry in create.properties {
                #expect(object[entry.name] == MirrorValue(entry.value), "\(create.key).\(entry.name)")
            }
        }
        #expect(store.objects(ofClass: "SliceModel").map(\.key) == ["slice:0"])
        #expect(store.objects(ofClass: "RadioModel").map(\.key) == ["radio"])
        #expect(store.propertyNames(ofClass: "SliceModel")?.count == 150)

        // Enum values are numbers, as the Core sent them.
        let slice = try #require(store.object("slice:0"))
        #expect(slice["dspMode"] == .enumeration(1))
        #expect(slice["band"] == .enumeration(5))
        #expect(store.object("tuner")?["connectionPhase"] == .enumeration(1))
        // Capabilities arrive as a set.
        #expect(store.capabilities["propertyResultVersion"] == .int(1))
        #expect(store.propertyResultsAvailable)
    }

    @Test func currentSessionSchemaIdentityRetiresAtNewSnapshot() {
        let store = store()
        let setupSchema = LinkMessage.schema(LinkMessage.Schema(className: "SetupDescription", fields: [
            .init(ordinal: 0, name: "general", kind: .utf8),
            .init(ordinal: 1, name: "revision", kind: .i64),
        ]))
        store.apply(FixtureReplay.accepted)
        store.apply(setupSchema)
        let first = store.currentSessionSchemaIdentity(ofClass: "SetupDescription")
        #expect(first != nil)
        #expect(store.currentSessionPropertyNames(ofClass: "SetupDescription") == ["general", "revision"])

        store.apply(FixtureReplay.accepted)
        #expect(store.currentSessionSchemaIdentity(ofClass: "SetupDescription") == nil)
        #expect(store.currentSessionPropertyNames(ofClass: "SetupDescription") == nil)
        #expect(store.propertyNames(ofClass: "SetupDescription") == ["general", "revision"])

        store.apply(setupSchema)
        #expect(store.currentSessionSchemaIdentity(ofClass: "SetupDescription") != first)
    }

    @Test func aDeltaChangesOnlyTheNamedProperties() throws {
        let store = try replayed()
        let slice = try #require(store.object("slice:0"))
        let before = slice.values
        store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(7_074_000)),
            LinkMessage.PropertyEntry(ordinal: 142, name: "snrDb", value: .f64(.nan)),
        ])))
        #expect(slice["frequency"] == .double(7_074_000))
        #expect(slice["snrDb"] == .double(.nan))
        for (name, value) in before where name != "frequency" && name != "snrDb" {
            #expect(slice[name] == value, "\(name)")
        }
        // A change to an object the Core never created is dropped.
        store.apply(.delta(LinkMessage.Delta(key: "slice:9", properties: [
            LinkMessage.PropertyEntry(name: "frequency", value: .f64(1)),
        ])))
        #expect(store.object("slice:9") == nil)
    }

    @Test func objectDestroyRemovesTheObject() throws {
        let store = try replayed()
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:1", className: "SliceModel", properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(14_074_000)),
        ])))
        #expect(store.objects(ofClass: "SliceModel").map(\.key) == ["slice:0", "slice:1"])
        store.apply(.objectDestroy(LinkMessage.ObjectDestroy(key: "slice:1", className: "SliceModel")))
        #expect(store.object("slice:1") == nil)
        #expect(store.objects(ofClass: "SliceModel").map(\.key) == ["slice:0"])
        #expect(!store.objectKeys.contains("slice:1"))
    }

    // MARK: Staleness

    @Test func aLostLinkLeavesTheValuesReadableUntilTheNextSnapshotCompletes() throws {
        let store = store()
        store.handle(.stateChanged(.connecting))
        store.handle(.stateChanged(.authenticating))
        store.handle(.stateChanged(.receivingSnapshot))
        for message in try FixtureReplay.stationMessages("session-connect-connectable") {
            store.handle(.message(message))
        }
        store.handle(.stateChanged(.ready))
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:1", className: "SliceModel", properties: [])))
        let slice = try #require(store.object("slice:0"))
        let frequency = slice["frequency"]
        #expect(!store.isStale)

        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(store.isStale)
        #expect(!store.isSnapshotComplete)
        #expect(store.object("slice:0")?["frequency"] == frequency)
        #expect(store.object("slice:1") != nil)

        // The next session: still stale through its snapshot, clear at its end.
        store.handle(.stateChanged(.connecting))
        store.handle(.stateChanged(.authenticating))
        store.handle(.message(FixtureReplay.stationHello()))
        store.handle(.message(FixtureReplay.accepted))
        store.handle(.stateChanged(.receivingSnapshot))
        store.handle(.message(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel",
                                                                    properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(3_573_000)),
        ]))))
        #expect(store.isStale)
        // The object kept its identity, so a screen watching it stays attached.
        #expect(store.object("slice:0") === slice)
        #expect(slice["frequency"] == .double(3_573_000))
        store.handle(.message(.snapshotComplete))
        store.handle(.stateChanged(.ready))
        #expect(!store.isStale)
        #expect(store.isSnapshotComplete)
        // What the new snapshot did not create is gone.
        #expect(store.object("slice:1") == nil)
        #expect(store.object("radio") == nil)
        #expect(store.objectKeys == ["slice:0"])
    }

    @Test func aConnectionThatNeverReachedTheCoreIsNotStale() {
        let store = store()
        store.handle(.stateChanged(.connecting))
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(!store.isStale)
    }

    // MARK: Capabilities

    @Test func capabilitiesAfterTheSnapshotReplaceTheWholeSetAndClearNothing() throws {
        let store = try replayed()
        let keys = store.objectKeys
        #expect(store.capabilities["displayBudgetGeneration"] == nil)
        #expect(store.capabilities["stationName"] == .text("ConnectableRadioModel fake"))

        // A changed display allowance.
        store.apply(FixtureReplay.capabilities([
            "stationName": .utf8("Shack"),
            "propertyResultVersion": .i64(1),
            "displayBudgetGeneration": .i64(2),
        ]))
        #expect(store.capabilities == [
            "stationName": .text("Shack"),
            "propertyResultVersion": .int(1),
            "displayBudgetGeneration": .int(2),
        ])
        #expect(store.objectKeys == keys)
        #expect(store.isSnapshotComplete)
        #expect(!store.isStale)

        // A radio that arrived late: capabilities, then a settings snapshot.
        store.apply(FixtureReplay.capabilities(["radioConnected": .bool(true)]))
        store.apply(.settingsSnapshot(LinkMessage.SettingsSnapshot(properties: [
            LinkMessage.PropertyEntry(name: "DisplaySpectrumFps", value: .utf8("30")),
        ])))
        #expect(store.capabilities == ["radioConnected": .bool(true)])
        #expect(store.objectKeys == keys)
        #expect(store.isSnapshotComplete)
    }

    // MARK: Writes

    /// Starts a write and waits for it to leave; returns the task and the message.
    private func startWrite(_ store: MirrorStore, _ key: String, _ property: String, _ value: MirrorValue)
        async throws -> (Task<PropertyWriteOutcome, Never>, LinkMessage.PropertyWrite) {
        let before = sent.count
        let task = Task { await store.write(key, property: property, value: value) }
        #expect(await sent.settle(untilCount: before + 1))
        guard case .propertyWrite(let write)? = sent.messages.last else {
            Issue.record("no property.write was sent")
            throw CancellationError()
        }
        return (task, write)
    }

    @Test func aWriteCarriesAFreshWriteIdAndResolvesOnItsResult() async throws {
        let clock = ManualLinkClock()
        let (handoffs, handedOff) = AsyncStream<LinkMessage>.makeStream(bufferingPolicy: .unbounded)
        let store = MirrorStore(send: { [sent] message in
            try await sent.sender(message)
            handedOff.yield(message)
        }, clock: clock)
        for message in try FixtureReplay.stationMessages("session-connect-connectable") {
            store.apply(message)
        }
        var writes = handoffs.makeAsyncIterator()
        let first = Task { await store.write("slice:0", property: "frequency", value: .double(7_074_000)) }
        guard case .propertyWrite(let write)? = await writes.next() else {
            Issue.record("no property.write was sent")
            throw CancellationError()
        }
        #expect(clock.pendingDueTimes.contains(5_000))
        #expect(write.key == "slice:0")
        let writeId = try #require(write.writeId)
        #expect(writeId != 0)
        #expect(write.properties == [LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(7_074_000))])

        // A second write while the first waits gets its own id.
        let second = Task { await store.write("slice:0", property: "afGain", value: .int(40)) }
        guard case .propertyWrite(let other)? = await writes.next() else {
            Issue.record("no property.write was sent")
            throw CancellationError()
        }
        #expect(other.writeId != writeId)
        #expect(other.writeId != 0)
        #expect(other.properties.first?.ordinal == 7)

        // An answer to a write this app did not send changes nothing.
        store.apply(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: 1_002, results: [
            LinkMessage.PropertyResult.Result(property: "frequency", accepted: true, reason: "",
                                              value: LinkMessage.PropertyEntry(ordinal: 1, name: "frequency",
                                                                               value: .f64(1))),
        ])))
        #expect(store.object("slice:0")?["frequency"] != .double(1))

        store.apply(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: writeId, results: [
            LinkMessage.PropertyResult.Result(property: "frequency", accepted: true, reason: "",
                                              value: LinkMessage.PropertyEntry(ordinal: 1, name: "frequency",
                                                                               value: .f64(7_074_000))),
        ])))
        let outcome = await first.value
        #expect(outcome == PropertyWriteOutcome(accepted: true, reason: "", value: .double(7_074_000)))
        #expect(store.object("slice:0")?["frequency"] == .double(7_074_000))

        store.apply(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: other.writeId ?? 0,
                                                               results: [
            LinkMessage.PropertyResult.Result(property: "afGain", accepted: true, reason: "",
                                              value: LinkMessage.PropertyEntry(ordinal: 7, name: "afGain",
                                                                               value: .i64(40))),
        ])))
        #expect(await second.value.accepted)
        #expect(clock.now == 0)
    }

    /// Both sides of the property-result gate: a negotiated session sends a
    /// nonzero writeId and resolves on property.result; below minor 5, or
    /// without propertyResultVersion, the write carries no writeId and
    /// resolves from the next delta.
    @Test(arguments: [(minor: UInt16(11), version: Int64(1), negotiated: true),
                      (minor: UInt16(5), version: Int64(1), negotiated: true),
                      (minor: UInt16(4), version: Int64(1), negotiated: false),
                      (minor: UInt16(11), version: Int64(0), negotiated: false)])
    func aWriteIdTravelsOnlyWhenResultsWereNegotiated(minor: UInt16, version: Int64, negotiated: Bool) async throws {
        let clock = ManualLinkClock()
        let store = store(clock: clock)
        store.apply(FixtureReplay.stationHello(minor: minor))
        store.apply(FixtureReplay.accepted)
        store.apply(FixtureReplay.capabilities(["propertyResultVersion": .i64(version)]))
        store.apply(.schema(LinkMessage.Schema(className: "SliceModel", fields: [
            LinkMessage.SchemaField(ordinal: 7, name: "afGain", kind: .i64),
        ])))
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel", properties: [
            LinkMessage.PropertyEntry(ordinal: 7, name: "afGain", value: .i64(50)),
        ])))
        store.apply(.snapshotComplete)
        #expect(store.propertyResultsAvailable == negotiated)

        let (task, write) = try await startWrite(store, "slice:0", "afGain", .int(40))
        // The wire form, as the Core reads it.
        let wire = try LinkJSON.parse(LinkCodec.encode(.propertyWrite(write)))
        guard case .object(let object) = wire else {
            Issue.record("not an object")
            return
        }
        if negotiated {
            let writeId = try #require(write.writeId)
            #expect(writeId != 0)
            #expect(object["writeId"] == .number(Double(writeId)))
            // A delta does not answer a negotiated write; its result does.
            store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
                LinkMessage.PropertyEntry(ordinal: 7, name: "afGain", value: .i64(40)),
            ])))
            store.apply(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: writeId, results: [
                LinkMessage.PropertyResult.Result(property: "afGain", accepted: false, reason: "Kept at 45.",
                                                  value: LinkMessage.PropertyEntry(ordinal: 7, name: "afGain",
                                                                                   value: .i64(45))),
            ])))
            #expect(await task.value == PropertyWriteOutcome(accepted: false, reason: "Kept at 45.", value: .int(45)))
        } else {
            #expect(write.writeId == nil)
            #expect(object["writeId"] == nil)
            store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
                LinkMessage.PropertyEntry(ordinal: 7, name: "afGain", value: .i64(40)),
            ])))
            #expect(await task.value == PropertyWriteOutcome(accepted: true, reason: "", value: .int(40)))
        }
        #expect(store.object("slice:0")?["afGain"] == (negotiated ? .int(45) : .int(40)))
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func aRefusedWriteKeepsTheCoresValueAndReason() async throws {
        let store = try replayed()
        let slice = try #require(store.object("slice:0"))
        let (task, write) = try await startWrite(store, "slice:0", "signalStrengthDbm", .double(-50))
        // The store sends it; the Core decides.
        #expect(write.properties.first?.name == "signalStrengthDbm")
        let reason = "The station sets this itself; it cannot be changed from here."
        store.apply(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: write.writeId ?? 0, results: [
            LinkMessage.PropertyResult.Result(property: "signalStrengthDbm", accepted: false, reason: reason,
                                              value: LinkMessage.PropertyEntry(ordinal: 15, name: "signalStrengthDbm",
                                                                               value: .f64(-140))),
        ])))
        let outcome = await task.value
        #expect(!outcome.accepted)
        #expect(outcome.reason == reason)
        #expect(outcome.value == .double(-140))
        #expect(outcome.answeredByCore)
        #expect(slice["signalStrengthDbm"] == .double(-140))

        // A property the Core does not have: refused without a value.
        let (unknown, unknownWrite) = try await startWrite(store, "slice:0", "noSuchProperty", .int(1))
        #expect(unknownWrite.properties.first == LinkMessage.PropertyEntry(name: "noSuchProperty", value: .i64(1)))
        store.apply(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: unknownWrite.writeId ?? 0,
                                                               results: [
            LinkMessage.PropertyResult.Result(property: "noSuchProperty", accepted: false,
                                              reason: "The Core does not have this setting, or not in this form.",
                                              value: nil),
        ])))
        let refused = await unknown.value
        #expect(!refused.accepted)
        #expect(refused.value == nil)
        #expect(slice["noSuchProperty"] == nil)
    }

    @Test(arguments: [(minor: UInt16(4), version: Int64(1)), (minor: UInt16(11), version: Int64(0))])
    func withoutResultsAWriteResolvesFromTheNextDelta(minor: UInt16, version: Int64) async throws {
        let clock = ManualLinkClock()
        let store = store(clock: clock)
        store.apply(FixtureReplay.stationHello(minor: minor))
        store.apply(FixtureReplay.accepted)
        store.apply(FixtureReplay.capabilities(["propertyResultVersion": .i64(version)]))
        store.apply(.schema(LinkMessage.Schema(className: "SliceModel", fields: [
            LinkMessage.SchemaField(ordinal: 1, name: "frequency", kind: .f64),
            LinkMessage.SchemaField(ordinal: 7, name: "afGain", kind: .i64),
        ])))
        store.apply(.objectCreate(LinkMessage.ObjectCreate(key: "slice:0", className: "SliceModel", properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(14_074_000)),
            LinkMessage.PropertyEntry(ordinal: 7, name: "afGain", value: .i64(50)),
        ])))
        store.apply(.snapshotComplete)
        #expect(!store.propertyResultsAvailable)

        let (task, write) = try await startWrite(store, "slice:0", "afGain", .int(40))
        #expect(write.writeId == nil)
        // A result is not read without the two keys; the delta decides.
        store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(14_075_000)),
        ])))
        store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            LinkMessage.PropertyEntry(ordinal: 7, name: "afGain", value: .i64(40)),
        ])))
        #expect(await task.value == PropertyWriteOutcome(accepted: true, reason: "", value: .int(40)))

        // A whole number to an f64 property travels as an f64; kept at another value, it is not accepted.
        let (other, otherWrite) = try await startWrite(store, "slice:0", "frequency", .int(7_074_000))
        #expect(otherWrite.properties.first?.value == .f64(7_074_000))
        store.apply(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(7_000_000)),
        ])))
        let kept = await other.value
        #expect(!kept.accepted)
        #expect(kept.value == .double(7_000_000))
        #expect(store.object("slice:0")?["frequency"] == .double(7_000_000))
        #expect(clock.now == 0 && clock.pendingDueTimes.isEmpty)
    }

    @Test func aWriteThatCannotBeSentOrLosesItsLinkResolvesWithoutTheCore() async throws {
        let store = store()
        store.handle(.stateChanged(.receivingSnapshot))
        for message in try FixtureReplay.stationMessages("session-connect-connectable") {
            store.handle(.message(message))
        }
        store.handle(.stateChanged(.ready))

        sent.refuseAll()
        let unsent = await store.write("slice:0", property: "afGain", value: .int(10))
        #expect(!unsent.accepted)
        #expect(!unsent.answeredByCore)
        #expect(!unsent.reason.isEmpty)
        sent.refuseAll(false)

        let (task, _) = try await startWrite(store, "slice:0", "afGain", .int(10))
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        let lost = await task.value
        #expect(!lost.accepted)
        #expect(!lost.answeredByCore)
    }

    @Test func writeIdsWrapWithinTheirRangeAndSkipZero() async throws {
        let store = try replayed()
        var ids: Set<UInt32> = []
        for _ in 0..<3 {
            let (task, write) = try await startWrite(store, "slice:0", "afGain", .int(1))
            let id = try #require(write.writeId)
            #expect(id != 0)
            #expect(ids.insert(id).inserted)
            store.apply(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: id, results: [
                LinkMessage.PropertyResult.Result(property: "afGain", accepted: true, reason: "", value: nil),
            ])))
            _ = await task.value
        }
        #expect(ids == [1, 2, 3])
    }

    // MARK: Telemetry

    private static func metrics(sequence: Int, elapsed: Int, extra: [String: LinkJSON] = [:]) -> LinkMessage {
        var payload: [String: LinkJSON] = [
            "sequence": .number(Double(sequence)),
            "sampledElapsedMs": .number(Double(elapsed)),
            "radio": .object(["connected": .bool(true), "rxMbps": .number(12.5)]),
            "host": .object(["systemCpuPercent": .number(21.5), "hottestZoneName": .string("cpu-thermal")]),
            "receivers": .array([.object(["sliceId": .number(0), "loadPercent": .number(35.5)])]),
        ]
        payload.merge(extra) { _, new in new }
        return .stationMetrics(LinkMessage.StationMetrics(payload: payload))
    }

    private func telemetryStore(minor: UInt16, version: Int64) -> MirrorStore {
        let store = store()
        store.apply(FixtureReplay.stationHello(minor: minor))
        store.apply(FixtureReplay.accepted)
        store.apply(FixtureReplay.capabilities(["stationTelemetryVersion": .i64(version)]))
        store.apply(.snapshotComplete)
        return store
    }

    @Test func telemetryReceiptUsesPhoneArrivalAndOnlyAcceptedSamplesAdvanceIt() async throws {
        let clock = ManualLinkClock()
        let store = MirrorStore(send: sent.sender, clock: clock)
        store.apply(FixtureReplay.stationHello(minor: 11))
        store.apply(FixtureReplay.accepted)
        store.apply(FixtureReplay.capabilities(["stationTelemetryVersion": .i64(6)]))
        store.apply(.snapshotComplete)
        #expect(store.currentTelemetryReceipt == nil)

        await clock.advance(by: 1_234)
        store.apply(Self.metrics(sequence: 5, elapsed: 5_000,
                                 extra: ["radio": .object(["connected": .bool(true), "rxMbps": .number(0)])]))
        let first = try #require(store.currentTelemetryReceipt)
        #expect(first.metrics.sequence == 5)
        #expect(first.metrics.radio?.rxMbps == 0)
        #expect(first.observedAtMilliseconds == 1_234)
        #expect(first.snapshotIdentity == store.snapshotIdentity)

        await clock.advance(by: 500)
        store.apply(Self.metrics(sequence: 5, elapsed: 6_000))
        store.apply(Self.metrics(sequence: 4, elapsed: 7_000))
        store.apply(Self.metrics(sequence: 6, elapsed: 4_999))
        store.apply(Self.metrics(sequence: 6, elapsed: 6_000,
                                 extra: ["radio": .object(["connected": .bool(true),
                                                            "connectionAgeMs": .number(-1)])]))
        store.apply(.schema(LinkMessage.Schema(className: "RadioModel", fields: [])))
        #expect(store.currentTelemetryReceipt?.observedAtMilliseconds == 1_234)
        #expect(store.currentTelemetryReceipt?.metrics.sequence == 5)

        store.apply(Self.metrics(sequence: 6, elapsed: 6_000))
        #expect(store.currentTelemetryReceipt?.observedAtMilliseconds == 1_734)
        #expect(store.currentTelemetryReceipt?.metrics.sequence == 6)
    }

    @Test func telemetryReceiptRetiresAcrossDisconnectAndEmptyReconnect() throws {
        let clock = ManualLinkClock()
        let store = MirrorStore(send: sent.sender, clock: clock)
        store.handle(.stateChanged(.receivingSnapshot))
        store.handle(.message(FixtureReplay.stationHello(minor: 11)))
        store.handle(.message(FixtureReplay.accepted))
        store.handle(.message(FixtureReplay.capabilities(["stationTelemetryVersion": .i64(1)])))
        store.handle(.message(.snapshotComplete))
        store.handle(.stateChanged(.ready))
        store.handle(.message(Self.metrics(sequence: 1, elapsed: 100)))
        let old = try #require(store.currentTelemetryReceipt)
        #expect(!store.isStale)
        #expect(store.objectKeys.isEmpty)

        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(store.currentTelemetryReceipt == nil)
        #expect(store.latestTelemetryReceipt?.snapshotIdentity == old.snapshotIdentity)
        #expect(store.metrics?.sequence == 1)

        store.handle(.message(FixtureReplay.accepted))
        #expect(store.currentTelemetryReceipt == nil)
        #expect(store.snapshotIdentity != old.snapshotIdentity)
        store.handle(.message(.snapshotComplete))
        #expect(store.currentTelemetryReceipt == nil)
        store.handle(.stateChanged(.ready))
        store.handle(.message(Self.metrics(sequence: 1, elapsed: 50)))
        let new = try #require(store.currentTelemetryReceipt)
        #expect(new.snapshotIdentity == store.snapshotIdentity)
        #expect(new.snapshotIdentity != old.snapshotIdentity)
        #expect(new.metrics.sequence == 1)
    }

    @Test func telemetryReceiptIsRetiredBeforeIncompleteSnapshotPublication() {
        let store = store()
        store.handle(.stateChanged(.receivingSnapshot))
        store.handle(.message(FixtureReplay.stationHello(minor: 11)))
        store.handle(.message(FixtureReplay.accepted))
        store.handle(.message(FixtureReplay.capabilities(["stationTelemetryVersion": .i64(1)])))
        store.handle(.message(.snapshotComplete))
        store.handle(.stateChanged(.ready))
        store.handle(.message(Self.metrics(sequence: 1, elapsed: 100)))

        var retiredAtPublication: [Bool] = []
        let subscription = store.$isSnapshotComplete.sink { complete in
            if !complete {
                retiredAtPublication.append(store.currentTelemetryReceipt == nil)
            }
        }
        store.handle(.message(FixtureReplay.accepted))
        store.handle(.message(.snapshotComplete))
        store.handle(.message(Self.metrics(sequence: 1, elapsed: 50)))
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(retiredAtPublication == [true, true])
        _ = subscription
    }

    @Test func telemetryOutOfSequenceOrGoingBackIsDropped() {
        let store = telemetryStore(minor: 11, version: 3)
        store.apply(Self.metrics(sequence: 5, elapsed: 5_000))
        #expect(store.metrics?.sequence == 5)
        store.apply(Self.metrics(sequence: 5, elapsed: 6_000))
        #expect(store.metrics?.sampledElapsedMs == 5_000)
        store.apply(Self.metrics(sequence: 4, elapsed: 7_000))
        #expect(store.metrics?.sequence == 5)
        store.apply(Self.metrics(sequence: 6, elapsed: 4_999))
        #expect(store.metrics?.sequence == 5)
        store.apply(Self.metrics(sequence: 6, elapsed: 5_000))
        #expect(store.metrics?.sequence == 6)
        store.apply(Self.metrics(sequence: 7, elapsed: 6_000))
        #expect(store.metrics?.sequence == 7)
        #expect(store.metrics?.radio?.rxMbps == 12.5)
        #expect(store.metrics?.host?.hottestZoneName == "cpu-thermal")
        #expect(store.metrics?.receivers?.first?.loadPercent == 35.5)

        // A new session counts from its own start.
        store.apply(FixtureReplay.accepted)
        store.apply(Self.metrics(sequence: 1, elapsed: 1_000))
        #expect(store.metrics?.sequence == 1)
    }

    @Test func telemetryFieldsBeyondTheAgreedVersionAreNotRead() {
        let none = telemetryStore(minor: 11, version: 0)
        none.apply(Self.metrics(sequence: 1, elapsed: 1_000))
        #expect(none.metrics == nil)

        let old = telemetryStore(minor: 2, version: 3)
        old.apply(Self.metrics(sequence: 1, elapsed: 1_000))
        #expect(old.metrics == nil)

        let first = telemetryStore(minor: 11, version: 1)
        first.apply(Self.metrics(sequence: 1, elapsed: 1_000))
        #expect(first.metrics?.radio?.connected == true)
        #expect(first.metrics?.host == nil)
        #expect(first.metrics?.receivers == nil)

        let hostByVersion = telemetryStore(minor: 11, version: 2)
        hostByVersion.apply(Self.metrics(sequence: 1, elapsed: 1_000))
        #expect(hostByVersion.metrics?.host?.systemCpuPercent == 21.5)
        #expect(hostByVersion.metrics?.receivers == nil)

        let hostByMinor = telemetryStore(minor: 9, version: 3)
        hostByMinor.apply(Self.metrics(sequence: 1, elapsed: 1_000))
        #expect(hostByMinor.metrics?.radio != nil)
        #expect(hostByMinor.metrics?.host == nil)
        #expect(hostByMinor.metrics?.receivers == nil)

        let receiversByMinor = telemetryStore(minor: 10, version: 3)
        receiversByMinor.apply(Self.metrics(sequence: 1, elapsed: 1_000))
        #expect(receiversByMinor.metrics?.host != nil)
        #expect(receiversByMinor.metrics?.receivers == nil)
    }

    /// Link section 10, version 4: the radio's PA readings and link quality
    /// need minor 11 and version 4; a reading out of range is unavailable.
    @Test func telemetryVersionFourAddsTheRadiosReadings() throws {
        let v4: [String: LinkJSON] = [
            "connected": .bool(true), "paVolts": .number(49.5), "supplyVolts": .number(13.8),
            "paCurrentAmps": .number(1.25), "paTemperatureCelsius": .number(-10),
            "packetLossPercent": .number(0.5), "jitterMs": .number(0.75), "packetGapMs": .number(2.5),
            "sampleRateHz": .number(192_000), "udpPacketsSeen": .number(123_456_789_012),
        ]
        var decoder = StationTelemetryDecoder()
        guard case .stationMetrics(let sample) = Self.metrics(sequence: 1, elapsed: 1_000,
                                                                extra: ["radio": .object(v4)]) else {
            Issue.record("not metrics")
            return
        }
        let read = try #require(decoder.decode(sample, agreedMinor: 11, telemetryVersion: 4)?.radio)
        #expect(read == StationMetrics.Radio(connected: true, paVolts: 49.5, supplyVolts: 13.8, paCurrentAmps: 1.25,
                                             paTemperatureCelsius: -10, packetLossPercent: 0.5, jitterMs: 0.75,
                                             packetGapMs: 2.5, sampleRateHz: 192_000,
                                             udpPacketsSeen: 123_456_789_012))

        // Every radio field the surface names for version 4 is read.
        let fields = try Self.radioTelemetryFields(version: 4)
        #expect(Set(fields).subtracting(["rxMbps", "txMbps", "rttMs", "rttAgeMs"]) == Set(v4.keys))
        #expect(Set(fields).isSubset(of: Set(Mirror(reflecting: read).children.compactMap(\.label))))

        // Not before version 4, nor below minor 11.
        var older = StationTelemetryDecoder()
        let three = try #require(older.decode(sample, agreedMinor: 11, telemetryVersion: 3)?.radio)
        #expect(three == StationMetrics.Radio(connected: true))
        var lowMinor = StationTelemetryDecoder()
        let ten = try #require(lowMinor.decode(sample, agreedMinor: 10, telemetryVersion: 4)?.radio)
        #expect(ten == StationMetrics.Radio(connected: true))

        // Out of range reads as unavailable, never as 0.
        let wrong: [String: LinkJSON] = [
            "paVolts": .number(-1), "supplyVolts": .string("13.8"), "paCurrentAmps": .number(-0.1),
            "paTemperatureCelsius": .number(-300), "packetLossPercent": .number(100.5), "jitterMs": .number(-2),
            "packetGapMs": .number(-1), "sampleRateHz": .number(192_000.5), "udpPacketsSeen": .number(-1),
        ]
        var strict = StationTelemetryDecoder()
        guard case .stationMetrics(let bad) = Self.metrics(sequence: 1, elapsed: 1_000,
                                                             extra: ["radio": .object(wrong)]) else {
            Issue.record("not metrics")
            return
        }
        let unread = try #require(strict.decode(bad, agreedMinor: 11, telemetryVersion: 4)?.radio)
        #expect(unread == StationMetrics.Radio())
    }

    /// The radio fields the surface lists for telemetry `version`, without
    /// their `radio.` prefix.
    private static func radioTelemetryFields(version: Int) throws -> [String] {
        let surface = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
        let telemetry = try #require(surface["telemetry"] as? [String: Any])
        let versions = try #require(telemetry["versions"] as? [[String: Any]])
        let entry = try #require(versions.first { $0["version"] as? Int == version })
        return (entry["fields"] as? [String] ?? []).filter { $0.hasPrefix("radio.") }.map { String($0.dropFirst(6)) }
    }

    /// Link section 10, version 5: the Hermes Lite 2 link needs minor 11 and
    /// version 5; a reading out of range is unavailable.
    @Test func telemetryVersionFiveAddsTheHermesLiteLink() throws {
        let v5: [String: LinkJSON] = [
            "connected": .bool(true), "hl2RxBytesPerSecond": .number(1_536_000.5),
            "hl2TxBytesPerSecond": .number(64_000), "hl2Throttled": .bool(false), "hl2SequenceGaps": .number(3),
        ]
        guard case .stationMetrics(let sample) = Self.metrics(sequence: 1, elapsed: 1_000,
                                                                extra: ["radio": .object(v5)]) else {
            Issue.record("not metrics")
            return
        }
        var decoder = StationTelemetryDecoder()
        let read = try #require(decoder.decode(sample, agreedMinor: 11, telemetryVersion: 5)?.radio)
        #expect(read == StationMetrics.Radio(connected: true, hl2RxBytesPerSecond: 1_536_000.5,
                                             hl2TxBytesPerSecond: 64_000, hl2Throttled: false, hl2SequenceGaps: 3))

        // The surface now lists through version 6; every version 5 radio
        // field remains covered by this decoder check.
        let surface = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent("surface.json"))
        let telemetry = try #require(surface["telemetry"] as? [String: Any])
        let versions = try #require(telemetry["versions"] as? [[String: Any]])
        #expect(versions.compactMap { $0["version"] as? Int }.max() == 6)
        let added = Set(try Self.radioTelemetryFields(version: 5)).subtracting(try Self.radioTelemetryFields(version: 4))
        #expect(added == Set(v5.keys).subtracting(["connected"]))
        let labels = Set(Mirror(reflecting: read).children.compactMap(\.label))
        #expect(Set(try Self.radioTelemetryFields(version: 5)).isSubset(of: labels))

        // Not at version 4, nor below minor 11.
        var older = StationTelemetryDecoder()
        #expect(try #require(older.decode(sample, agreedMinor: 11, telemetryVersion: 4)?.radio)
                    == StationMetrics.Radio(connected: true))
        var lowMinor = StationTelemetryDecoder()
        #expect(try #require(lowMinor.decode(sample, agreedMinor: 10, telemetryVersion: 5)?.radio)
                    == StationMetrics.Radio(connected: true))

        // Out of range reads as unavailable, never as 0.
        let wrong: [String: LinkJSON] = [
            "hl2RxBytesPerSecond": .number(-1), "hl2TxBytesPerSecond": .string("64000"),
            "hl2Throttled": .number(1), "hl2SequenceGaps": .number(2.5),
        ]
        guard case .stationMetrics(let bad) = Self.metrics(sequence: 1, elapsed: 1_000,
                                                             extra: ["radio": .object(wrong)]) else {
            Issue.record("not metrics")
            return
        }
        var strict = StationTelemetryDecoder()
        #expect(try #require(strict.decode(bad, agreedMinor: 11, telemetryVersion: 5)?.radio) == StationMetrics.Radio())
    }

    @Test func telemetryOverSixteenKiBIsRefused() throws {
        var decoder = StationTelemetryDecoder()
        let padding = String(repeating: "x", count: StationTelemetryDecoder.capBytes)
        guard case .stationMetrics(let big) = Self.metrics(sequence: 1, elapsed: 1_000,
                                                             extra: ["padding": .string(padding)]) else {
            Issue.record("not metrics")
            return
        }
        #expect(decoder.decode(big, agreedMinor: 11, telemetryVersion: 3) == nil)

        // The control fixture's sample, as the Core sends it, reads in full.
        let control = try LinkFixtureLoader.jsonObject(
            at: LinkFixtureLoader.root.appendingPathComponent("control/station-metrics.json"))
        let raw = try #require(control["wire"])
        let wire = try LinkJSON(foundation: raw)
        guard case .stationMetrics(let sample) = try LinkCodec.decode(wire.compactText) else {
            Issue.record("the control fixture is not metrics")
            return
        }
        let decoded = decoder.decode(sample, agreedMinor: 11, telemetryVersion: 3)
        let read = try #require(decoded)
        #expect(read.sequence == 42)
        #expect(read.radio == StationMetrics.Radio(connected: true, rxMbps: 12.5, txMbps: 0.5, rttMs: 2, rttAgeMs: 900))
        #expect(read.audio?.contextGeneration == 3)
        #expect(read.host?.memoryTotalKiB == 4_000_000)
        #expect(read.receivers == [StationMetrics.Receiver(sliceId: 0, inputDelayMs: 4, skippedInputMs: 0,
                                                           loadPercent: 35.5)])
    }
}
