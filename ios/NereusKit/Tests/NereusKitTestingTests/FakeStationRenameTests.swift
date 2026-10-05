// NereusSDR for iOS: the fake Core keeps a name and answers station.rename as the suite's devices fixture shows the Core does
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import NereusKitTesting
import NereusLink
import NereusMirror
import Testing

/// D76, R-IOS-08: the fake Core the app's rename tests run against. The
/// suite's `devices` fixture runs on the station alone (its client steps
/// include a self-revoke and raw settings writes no app sends), so its
/// rename steps are played here instead: each `station.rename` the fixture
/// sends goes through the app's command client to the fake, and the fake's
/// answers must be the fixture's station messages.
@Suite("FakeStation, the Core's name", .serialized)
@MainActor
struct FakeStationRenameTests {
    struct Rig {
        let station: FakeStation
        let session: StationSession
        let mirror: MirrorStore
        let commands: CommandClient
        let feeding: Task<Void, Never>
    }

    private func connected(_ station: FakeStation) async throws -> Rig {
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let commands = CommandClient(session: session)
        let events = session.events
        let feeding = Task { @MainActor in
            for await event in events {
                mirror.handle(event)
                await commands.handle(event)
            }
        }
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await poll { mirror.isSnapshotComplete })
        return Rig(station: station, session: session, mirror: mirror, commands: commands, feeding: feeding)
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

    /// The fixture's `station.rename` requests, each with the station
    /// messages that answer it (up to the next client step).
    private static func renameSteps() throws -> [(args: [[String: Any]], answers: [[String: Any]])] {
        let fixture = try #require(try SessionFixtures.all().first { $0.id == "session-devices" })
        #expect(fixture.runs == ["station"])
        var found: [(args: [[String: Any]], answers: [[String: Any]])] = []
        for (index, step) in fixture.steps.enumerated() {
            guard step["from"] as? String == "client", let message = step["message"] as? [String: Any],
                  message["verb"] as? String == FakeStation.renameVerb,
                  let args = message["args"] as? [[String: Any]] else {
                continue
            }
            var answers: [[String: Any]] = []
            for next in fixture.steps[(index + 1)...] {
                guard next["from"] as? String == "station", let answer = next["message"] as? [String: Any] else {
                    break
                }
                answers.append(answer)
            }
            found.append((args, answers))
        }
        return found
    }

    @Test("the devices fixture's renames get the fixture's answers: not understood, the rule, then stored")
    func fixtureRenames() async throws {
        let steps = try Self.renameSteps()
        #expect(steps.count == 3)
        let rig = try await connected(try FakeStation(fixture: "session-device-sign-in", stationLabel: ""))
        defer { rig.feeding.cancel() }
        #expect(rig.mirror.object("devices")?["stationLabel"] == .text(""))
        for step in steps {
            let arguments = try step.args.map { arg -> CommandArgument in
                let name = try #require(arg["name"] as? String)
                let value = try #require(arg["value"] as? String)
                #expect(arg["kind"] as? String == "utf8")
                return CommandArgument(name: name, value: .text(value))
            }
            let result = try await rig.commands.invoke(FakeStation.renameVerb, arguments: arguments,
                                                       timeout: .seconds(10))
            let expected = try #require(step.answers.first { $0["type"] as? String == "command.result" })
            #expect(result.accepted == (expected["accepted"] as? Bool))
            #expect(result.reason == (expected["reason"] as? String))
            #expect(result.affectedKeys == (expected["affected"] as? [String]))
            if let setting = step.answers.first(where: { $0["type"] as? String == "settings.value" }),
               let entry = (setting["properties"] as? [[String: Any]])?.first,
               let value = entry["value"] as? String {
                // Stored: the object's next delta carries the new name.
                #expect(rig.station.stationLabel == value)
                #expect(await poll { rig.mirror.object("devices")?["stationLabel"] == .text(value) })
            } else {
                #expect(rig.station.stationLabel == "", "a refused rename stores nothing")
            }
        }
        #expect(rig.station.stationLabel == "KG4VCF/shack")
        await rig.session.disconnect()
    }

    @Test("the fake's refusals are the Core's words from the fixture")
    func refusalWords() throws {
        let answers = try Self.renameSteps().flatMap(\.answers).compactMap { $0["reason"] as? String }
        #expect(answers.contains(FakeStation.renameNotUnderstoodReason))
        #expect(answers.contains(FakeStation.renameRuleReason))
    }

    @Test("the next connection's snapshot carries the new name, as the Core keeps it")
    func nameKept() async throws {
        let station = try FakeStation(fixture: "session-device-sign-in", stationLabel: "")
        let first = try await connected(station)
        _ = try await first.commands.invoke(FakeStation.renameVerb, arguments: [
            CommandArgument(name: "label", value: .text("  KG4VCF/attic ")),
        ], timeout: .seconds(10))
        await first.session.disconnect()
        first.feeding.cancel()
        let second = try await connected(station)
        defer { second.feeding.cancel() }
        #expect(second.mirror.object("devices")?["stationLabel"] == .text("KG4VCF/attic"))
        await second.session.disconnect()
    }

    @Test("a Core without deviceAdminVersion neither advertises nor takes a rename")
    func olderCore() async throws {
        let rig = try await connected(try FakeStation(fixture: "session-device-sign-in",
                                                      withoutCapabilities: ["deviceAdminVersion"]))
        defer { rig.feeding.cancel() }
        #expect(rig.mirror.capabilities["deviceAdminVersion"] == nil)
        #expect(rig.mirror.object("devices") == nil)
        let result = try await rig.commands.invoke(FakeStation.renameVerb, arguments: [
            CommandArgument(name: "label", value: .text("KG4VCF")),
        ], timeout: .seconds(10))
        #expect(!result.accepted)
        #expect(result.reason == FakeStation.unknownVerbReason)
        await rig.session.disconnect()
    }
}
