// NereusSDR for iOS: the Core's record streams: asked for when the Core is ready, kept in step batch by batch
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-25, link document section 7.7: the app asks for the streams it
/// wants with `records.subscribe` once the snapshot is complete on a Core
/// with `recordStreamVersion` 1, and after every reconnect; each batch
/// resets, changes, adds or removes records as the Core sends them.
@MainActor
@Suite struct RecordStreamClientTests {
    private let sent = SentMessages()

    private func rig(recordStreamVersion: Int64 = 1, clock: any LinkClock = SystemLinkClock()) -> (RecordStreamClient, MirrorStore, CommandClient) {
        let mirror = MirrorStore(send: sent.sender)
        let commands = CommandClient(clock: clock, send: sent.sender)
        mirror.apply(FixtureReplay.stationHello())
        mirror.apply(FixtureReplay.capabilities([RecordStreamClient.capabilityName: .i64(recordStreamVersion)]))
        return (RecordStreamClient(mirror: mirror, commands: commands), mirror, commands)
    }

    private func ready(_ client: RecordStreamClient, _ commands: CommandClient) async {
        await commands.handle(.stateChanged(.receivingSnapshot))
        client.handle(.stateChanged(.receivingSnapshot))
        await commands.handle(.stateChanged(.ready))
        client.handle(.stateChanged(.ready))
    }

    private func invokes() -> [LinkMessage.CommandInvoke] {
        sent.messages.compactMap {
            if case .commandInvoke(let invoke) = $0 {
                return invoke
            }
            return nil
        }
    }

    private static func record(_ id: String, _ call: String) -> LinkMessage.RecordBatch.Record {
        .init(id: id, fields: ["call": .string(call)])
    }

    private static func batch(_ stream: String = "spots", generation: Int64 = 1, reset: Bool = false,
                              upserts: [LinkMessage.RecordBatch.Record] = [], removes: [String] = [])
        -> LinkMessage.RecordBatch {
        LinkMessage.RecordBatch(stream: stream, generation: generation, reset: reset, upserts: upserts,
                                removes: removes)
    }

    private func calls(_ client: RecordStreamClient, _ stream: String = "spots") -> [String] {
        client.records(stream).compactMap {
            if case .string(let call)? = $0.fields["call"] {
                return call
            }
            return nil
        }
    }

    @Test func wantedStreamsAreAskedForOnceTheCoreIsReadyWithTheirBacklogs() async {
        let (client, _, commands) = rig()
        client.want(RecordStreamClient.spotsStream, backlog: 500)
        client.want(RecordStreamClient.consoleStream("dxCluster"), backlog: 200)
        // Nothing goes before the snapshot is complete.
        #expect(sent.count == 0)
        await ready(client, commands)
        #expect(client.available)
        #expect(await sent.settle(untilCount: 2))
        let asked = invokes()
        #expect(asked.map(\.verb) == ["records.subscribe", "records.subscribe"])
        #expect(asked.map(\.args) == [
            [.init(name: "stream", value: .utf8("spotConsole:dxCluster")), .init(name: "backlog", value: .i64(200))],
            [.init(name: "stream", value: .utf8("spots")), .init(name: "backlog", value: .i64(500))],
        ])
        // Asking again for the same changes nothing.
        client.want(RecordStreamClient.spotsStream, backlog: 500)
        await Task.yield()
        #expect(sent.count == 2)
    }

    @Test func aBacklogIsBoundedByTheStreamsCapacity() async {
        let (client, _, commands) = rig()
        await ready(client, commands)
        client.want(RecordStreamClient.consoleStream("rbn"), backlog: 10_000)
        #expect(await sent.settle(untilCount: 1))
        #expect(invokes().first?.args.last == .init(name: "backlog", value: .i64(200)))
    }

    @Test func anOlderCoreIsNeverAsked() async {
        let (client, _, commands) = rig(recordStreamVersion: 0)
        client.want(RecordStreamClient.spotsStream, backlog: 500)
        await ready(client, commands)
        #expect(!client.available)
        for _ in 0..<20 {
            await Task.yield()
        }
        #expect(sent.count == 0)
    }

    @Test func aResetReplacesTheCopyAndChangesArriveByIdOldestFirst() async {
        let (client, _, commands) = rig()
        client.want("spots", backlog: 500)
        await ready(client, commands)
        client.apply(Self.batch(reset: true, upserts: [Self.record("1", "VE3ABC"), Self.record("2", "K1ABC")]))
        #expect(calls(client) == ["VE3ABC", "K1ABC"])
        // A changed record stays where it was; a new one goes last; an unknown remove is ignored.
        client.apply(Self.batch(upserts: [Self.record("1", "VE3ABC/P"), Self.record("3", "EA8XYZ")], removes: ["9"]))
        #expect(calls(client) == ["VE3ABC/P", "K1ABC", "EA8XYZ"])
        client.apply(Self.batch(removes: ["2"]))
        #expect(calls(client) == ["VE3ABC/P", "EA8XYZ"])
        // Clear all spots: a new generation's reset leaves nothing old.
        client.apply(Self.batch(generation: 2, reset: true))
        #expect(calls(client).isEmpty)
        #expect(client.streams["spots"]?.generation == 2)
    }

    @Test func aStreamNeverHoldsMoreThanItsCapacityTheOldestGoFirst() async {
        let (client, _, commands) = rig()
        let console = RecordStreamClient.consoleStream("dxCluster")
        client.want(console, backlog: 200)
        await ready(client, commands)
        client.apply(Self.batch(console, reset: true, upserts: (1...200).map { Self.record("\($0)", "line \($0)") }))
        client.apply(Self.batch(console, upserts: [Self.record("201", "line 201"), Self.record("202", "line 202")]))
        let lines = calls(client, console)
        #expect(lines.count == 200)
        #expect(lines.first == "line 3")
        #expect(lines.last == "line 202")
    }

    @Test func theCopiesGoWhenTheLinkDropsAndTheStreamsAreAskedForAgainOnReconnect() async {
        let (client, _, commands) = rig()
        client.want("spots", backlog: 500)
        await ready(client, commands)
        #expect(await sent.settle(untilCount: 1))
        client.apply(Self.batch(reset: true, upserts: [Self.record("1", "VE3ABC")]))
        #expect(calls(client) == ["VE3ABC"])
        await commands.handle(.stateChanged(.waitingToRetry(seconds: 5)))
        client.handle(.stateChanged(.waitingToRetry(seconds: 5)))
        #expect(client.streams.isEmpty)
        #expect(!client.available)
        await ready(client, commands)
        #expect(await sent.settle(untilCount: 2))
        #expect(invokes().map(\.verb) == ["records.subscribe", "records.subscribe"])
    }

    @Test func theCoresRefusalIsKeptForItsStream() async throws {
        let (client, _, commands) = rig(clock: ManualLinkClock())
        client.want("notAStream", backlog: 10)
        await ready(client, commands)
        #expect(await sent.settle(untilCount: 1))
        let invoke = try #require(invokes().first)
        let answered = try #require(client.recordCommandTask)
        await commands.receive(.commandResult(LinkMessage.CommandResult(
            verb: invoke.verb, id: invoke.id, accepted: false, reason: "The Core does not keep that list.",
            affected: [], values: nil)))
        let consumed = TestPhase<Void>()
        let observer = Task { await answered.value; consumed.finish(.success(())) }
        defer { observer.cancel() }
        do { try await consumed.wait(until: ContinuousClock.now + .seconds(10)) }
        catch {
            answered.cancel()
            await commands.handle(.stateChanged(.stopped))
            client.handle(.stateChanged(.stopped))
            throw error
        }
        #expect(client.refusals["notAStream"] == "The Core does not keep that list.")
    }

    @Test func aStreamNoLongerWantedIsStoppedAndItsLateBatchesAreDropped() async {
        let (client, _, commands) = rig()
        let console = RecordStreamClient.consoleStream("pota")
        client.want(console, backlog: 200)
        await ready(client, commands)
        #expect(await sent.settle(untilCount: 1))
        client.apply(Self.batch(console, reset: true, upserts: [Self.record("1", "Polling")]))
        client.unwant(console)
        #expect(await sent.settle(untilCount: 2))
        #expect(invokes().last?.verb == "records.unsubscribe")
        #expect(invokes().last?.args == [.init(name: "stream", value: .utf8(console))])
        #expect(client.streams[console] == nil)
        client.apply(Self.batch(console, upserts: [Self.record("2", "late")]))
        #expect(client.streams[console] == nil)
    }

    @Test func aWantedStreamIsAskedForAgainWithoutStoppingAndAnUnwantedOneIsNot() async {
        let (client, _, commands) = rig()
        client.want(CoreLogLine.streamName, backlog: 200)
        await ready(client, commands)
        #expect(await sent.settle(untilCount: 1))
        client.resubscribe(CoreLogLine.streamName)
        #expect(await sent.settle(untilCount: 2))
        #expect(invokes().map(\.verb) == ["records.subscribe", "records.subscribe"])
        #expect(invokes().last?.args == [.init(name: "stream", value: .utf8("coreLog")),
                                         .init(name: "backlog", value: .i64(200))])
        client.resubscribe("spots")
        for _ in 0..<20 {
            await Task.yield()
        }
        #expect(sent.count == 2)
    }

    private final class User {}

    /// Two screens that read the same stream: it is asked for once on the
    /// wire, and each screen's copy is the same one.
    @Test func twoUsersOfAStreamAskForItOnce() async {
        let (client, _, commands) = rig()
        let logs = User()
        let tools = User()
        client.want(CoreLogLine.streamName, backlog: 200, by: logs)
        client.want(CoreLogLine.streamName, backlog: 200, by: tools)
        await ready(client, commands)
        #expect(await sent.settle(untilCount: 1))
        for _ in 0..<20 {
            await Task.yield()
        }
        #expect(invokes().map(\.verb) == ["records.subscribe"])
        // A second user arriving on a running stream sends nothing.
        let meters = User()
        client.want(CoreLogLine.streamName, backlog: 200, by: meters)
        for _ in 0..<20 {
            await Task.yield()
        }
        #expect(sent.count == 1)
        #expect(client.isWanted(CoreLogLine.streamName, by: meters))
    }

    /// One user leaving leaves the stream running for the other; the last
    /// one out stops it.
    @Test func aUserLeavingDoesNotCutTheOtherAndTheLastOneOutStopsTheStream() async {
        let (client, _, commands) = rig()
        let spots = User()
        let map = User()
        client.want(RecordStreamClient.spotsStream, backlog: 500, by: spots)
        client.want(RecordStreamClient.spotsStream, backlog: 500, by: map)
        await ready(client, commands)
        #expect(await sent.settle(untilCount: 1))
        client.apply(Self.batch(RecordStreamClient.spotsStream, reset: true, upserts: [Self.record("1", "VE3ABC")]))
        client.unwant(RecordStreamClient.spotsStream, by: spots)
        for _ in 0..<20 {
            await Task.yield()
        }
        #expect(sent.count == 1)
        #expect(client.isWanted(RecordStreamClient.spotsStream))
        #expect(!client.isWanted(RecordStreamClient.spotsStream, by: spots))
        #expect(calls(client, RecordStreamClient.spotsStream) == ["VE3ABC"])
        client.apply(Self.batch(RecordStreamClient.spotsStream, upserts: [Self.record("2", "K1ABC")]))
        #expect(calls(client, RecordStreamClient.spotsStream) == ["VE3ABC", "K1ABC"])
        client.unwant(RecordStreamClient.spotsStream, by: map)
        #expect(await sent.settle(untilCount: 2))
        #expect(invokes().map(\.verb) == ["records.subscribe", "records.unsubscribe"])
        #expect(invokes().last?.args == [.init(name: "stream", value: .utf8(RecordStreamClient.spotsStream))])
        #expect(client.streams[RecordStreamClient.spotsStream] == nil)
        #expect(!client.isWanted(RecordStreamClient.spotsStream))
    }

    /// A screen that never asked for a stream cannot stop it for the one
    /// that did.
    @Test func aUserThatNeverAskedCannotStopAStream() async {
        let (client, _, commands) = rig()
        let tci = User()
        let reader = User()
        client.want(StationTciClient.streamName, backlog: StationTciClient.capacity, by: reader)
        await ready(client, commands)
        #expect(await sent.settle(untilCount: 1))
        client.unwant(StationTciClient.streamName, by: tci)
        client.unwant(StationTciClient.streamName)
        for _ in 0..<20 {
            await Task.yield()
        }
        #expect(sent.count == 1)
        #expect(client.isWanted(StationTciClient.streamName))
    }

    /// The stream carries the most backlog any user asks for: a user
    /// asking for more asks again; one leaving asks nothing.
    @Test func theStreamCarriesTheMostBacklogAnyUserAsksFor() async {
        let (client, _, commands) = rig()
        let small = User()
        let large = User()
        let console = RecordStreamClient.consoleStream("dxCluster")
        client.want(console, backlog: 20, by: small)
        await ready(client, commands)
        #expect(await sent.settle(untilCount: 1))
        client.want(console, backlog: 200, by: large)
        #expect(await sent.settle(untilCount: 2))
        #expect(invokes().last?.args.last == .init(name: "backlog", value: .i64(200)))
        client.unwant(console, by: large)
        for _ in 0..<20 {
            await Task.yield()
        }
        #expect(sent.count == 2)
        #expect(client.isWanted(console))
    }

    /// The Core's own batch from the suite (`control-station-record-batch`):
    /// a spot added with every field the link document lists, and a remove
    /// of one this app does not hold, which is ignored.
    @Test func theCoresBatchFromTheSuiteAddsItsSpot() async throws {
        let fixture = try #require(try LinkFixtureLoader.manifest().first { $0.id == "control-station-record-batch" })
        let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(fixture.file))
        let message = try LinkCodec.decode(try LinkJSON(foundation: try #require(object["wire"])).compactText)
        guard case .recordBatch(let batch) = message else {
            Issue.record("not a record.batch")
            return
        }
        let (client, _, commands) = rig()
        client.want("spots", backlog: 500)
        await ready(client, commands)
        client.handle(.message(.recordBatch(batch)))
        let record = try #require(client.records("spots").first)
        #expect(client.records("spots").count == 1)
        #expect(record.id == "12")
        #expect(record.fields["call"] == .string("JA1ABC"))
        #expect(record.fields["frequencyHz"] == .number(14_025_000))
        #expect(record.fields["timeUtc"] == .string("2026-09-26T18:24:00Z"))
        // A version 1 record names no mode.
        #expect(RecordStreamClient.resolvedMode(of: record) == nil)
    }

    /// The Core's `recordStreamVersion` is known once the snapshot is
    /// complete, and forgotten when the link drops.
    @Test func theCoresVersionIsKnownWhileTheLinkIsUp() async {
        let (client, _, commands) = rig(recordStreamVersion: 2)
        #expect(client.version == 0)
        await ready(client, commands)
        #expect(client.version == 2 && client.available)
        client.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(client.version == 0 && !client.available)
        let (older, _, olderCommands) = rig(recordStreamVersion: 1)
        await ready(older, olderCommands)
        #expect(older.version == 1 && older.version < RecordStreamClient.resolvedModeVersion)
    }

    /// The suite's `control-station-record-batch-resolved-mode` batch, from a
    /// Core at `recordStreamVersion` 2: a CW spot on 20 m names CWU (4), a
    /// FreeDV spot on 40 m RADE_L (13), and a spot at 0.5 MHz names none.
    @Test func theCoresResolvedModeBatchNamesEachSpotsMode() async throws {
        let fixture = try #require(try LinkFixtureLoader.manifest().first {
            $0.id == "control-station-record-batch-resolved-mode"
        })
        let object = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(fixture.file))
        let message = try LinkCodec.decode(try LinkJSON(foundation: try #require(object["wire"])).compactText)
        guard case .recordBatch(let batch) = message else {
            Issue.record("not a record.batch")
            return
        }
        let (client, _, commands) = rig(recordStreamVersion: 2)
        client.want("spots", backlog: 500)
        await ready(client, commands)
        client.handle(.message(.recordBatch(batch)))
        let modes = Dictionary(uniqueKeysWithValues: client.records("spots").map {
            ($0.id, RecordStreamClient.resolvedMode(of: $0))
        })
        let expected: [String: Int?] = ["12": 4, "13": 13, "14": nil]
        #expect(modes == expected)
    }

    /// `resolvedMode` reads only as a whole, non-negative number.
    @Test func aResolvedModeReadsOnlyAsAWholeNumber() {
        func mode(_ value: LinkJSON) -> Int? {
            RecordStreamClient.resolvedMode(of: .init(id: "1", fields: ["resolvedMode": value]))
        }
        #expect(mode(.number(0)) == 0)
        #expect(mode(.number(7)) == 7)
        #expect(mode(.number(4.5)) == nil)
        #expect(mode(.number(-1)) == nil)
        #expect(mode(.string("CWU")) == nil)
        #expect(mode(.bool(true)) == nil)
    }
}
