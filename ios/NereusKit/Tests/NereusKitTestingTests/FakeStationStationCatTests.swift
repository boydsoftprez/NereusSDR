// NereusSDR for iOS: the fake Core runs its CAT as the Core does: the stationCat object, the four verbs and the catLog stream
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusKitTesting
import NereusLink
import NereusMirror
import Testing

/// Made with ``FakeStation/Additions/stationCat`` the fake plays a Core at
/// `stationCatVersion` 1: it sends the `stationCat` object, lays a sent
/// channel or global config over its own and sends the result back, answers
/// the tester with a `reply`, and serves the `catLog` stream.
@Suite("FakeStation, the Core's CAT", .serialized)
@MainActor
struct FakeStationStationCatTests {
    struct Rig {
        let station: FakeStation
        let session: StationSession
        let mirror: MirrorStore
        let commands: CommandClient
        let records: RecordStreamClient
        let feeding: Task<Void, Never>
    }

    private func connected(_ additions: FakeStation.Additions = [.stationCat]) async throws -> Rig {
        let station = try FakeStation(additions: additions)
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let commands = CommandClient(session: session)
        let records = RecordStreamClient(mirror: mirror, commands: commands)
        let events = session.events
        let feeding = Task { @MainActor in
            for await event in events {
                mirror.handle(event)
                await commands.handle(event)
                records.handle(event)
            }
        }
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await poll { mirror.isSnapshotComplete })
        return Rig(station: station, session: session, mirror: mirror, commands: commands, records: records,
                   feeding: feeding)
    }

    private func poll(_ condition: @MainActor () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    private func cat(_ rig: Rig) -> StationCat {
        StationCat(values: rig.mirror.object(StationCat.objectKey)?.values ?? [:])
    }

    @Test("it advertises stationCatVersion 1 and sends the scene; not without the addition")
    func scene() async throws {
        let rig = try await connected()
        defer { rig.feeding.cancel() }
        #expect(rig.mirror.capabilityVersion(StationCat.capabilityName) == 1)
        #expect(rig.mirror.capabilityVersion("recordStreamVersion") >= 1)
        try await rig.station.deliverStationCat()
        #expect(await poll { cat(rig).channel(1).received })
        let scene = cat(rig)
        #expect(scene.channel(1).config.tcpEnabled && scene.channel(1).status.tcpClients == 2)
        #expect(scene.channel(1).config.ptyEnabled && scene.channel(1).status.ptyPath == "/dev/ttys004")
        #expect(scene.channel(2).config.serialEnabled && !scene.channel(3).primaryValid)
        #expect(scene.platform.serial && scene.platform.pty && scene.platform.serialDevices.count == 2)
        #expect(scene.platform.ptyDialects.map(\.value) == [FakeStation.catNativeDialect, "Rigctld"])
        #expect(scene.global.config.rigIdentity == "TS-2000")
        await rig.session.disconnect()

        let older = try await connected([])
        defer { older.feeding.cancel() }
        #expect(older.mirror.capabilityVersion(StationCat.capabilityName) == 0)
        await older.session.disconnect()
    }

    @Test("a channel config is laid over the Core's and comes back in a delta; a refusal answers instead")
    func channel() async throws {
        let rig = try await connected()
        defer { rig.feeding.cancel() }
        try await rig.station.deliverStationCat()
        #expect(await poll { cat(rig).channel(4).received })
        var config = cat(rig).channel(4).config
        config.rigctldEnabled = true
        config.rigctldPort = 4600
        let result = try await rig.commands.invoke(StationCat.setChannelVerb, arguments: [
            CommandArgument(name: "channel", value: .int(4)),
            CommandArgument(name: "config", value: .text(config.commandText(primaryRebind: false, secondaryRebind: false))),
        ], timeout: .seconds(5))
        #expect(result.accepted)
        #expect(await poll { cat(rig).channel(4).config.rigctldPort == 4600 })
        #expect(cat(rig).channel(4).status.rigctld == "Listening" && cat(rig).channel(4).status.rigctldBoundPort == 4600)

        var rebound = cat(rig).channel(3).config
        rebound.primarySliceId = 0
        _ = try await rig.commands.invoke(StationCat.setChannelVerb, arguments: [
            CommandArgument(name: "channel", value: .int(3)),
            CommandArgument(name: "config", value: .text(rebound.commandText(primaryRebind: true, secondaryRebind: false))),
        ], timeout: .seconds(5))
        #expect(await poll { cat(rig).channel(3).primaryValid })

        rig.station.refuseNext(StationCat.setChannelVerb, reason: FakeStation.catChannelRefusedReason)
        let refused = try await rig.commands.invoke(StationCat.setChannelVerb, arguments: [
            CommandArgument(name: "channel", value: .int(4)),
            CommandArgument(name: "config", value: .text(config.commandText(primaryRebind: false, secondaryRebind: false))),
        ], timeout: .seconds(5))
        #expect(!refused.accepted && refused.reason == FakeStation.catChannelRefusedReason)
        await rig.session.disconnect()
    }

    @Test("the global settings change, the tester replies, and the log is served on subscribe")
    func globalTesterAndLog() async throws {
        let rig = try await connected()
        defer { rig.feeding.cancel() }
        try await rig.station.deliverStationCat()
        #expect(await poll { cat(rig).global.received })
        var global = cat(rig).global.config
        global.rigIdentity = "TS-480"
        let changed = try await rig.commands.invoke(StationCat.setGlobalVerb, arguments: [
            CommandArgument(name: "config", value: .text(global.text)),
        ], timeout: .seconds(5))
        #expect(changed.accepted)
        #expect(await poll { cat(rig).global.config.rigIdentity == "TS-480" })

        let test = try await rig.commands.invoke(StationCat.testVerb, arguments: [
            CommandArgument(name: "requestId", value: .int(1)), CommandArgument(name: "channel", value: .int(1)),
            CommandArgument(name: "command", value: .text("FA;")),
        ], timeout: .seconds(5))
        #expect(test.accepted && test.values["reply"] == .text("FA00014074000;"))
        #expect(await poll { cat(rig).lastTest.command == "FA;" })

        rig.records.want(StationCat.logStream, backlog: StationCat.logCapacity)
        #expect(await poll { (rig.records.streams[StationCat.logStream]?.records.count ?? 0) == 6 })
        let lines = rig.records.streams[StationCat.logStream]?.records.map(StationCat.LogLine.init(record:)) ?? []
        #expect(lines.first?.text == "FA;" && lines.first?.inbound == true && lines.last?.text == "FA00014074000;")
        await rig.station.deliverCatLog([FakeStation.LogLine(channel: 2, inbound: true, text: "ID;", timeMs: 5)])
        #expect(await poll { (rig.records.streams[StationCat.logStream]?.records.count ?? 0) == 7 })
        await rig.session.disconnect()
    }
}
