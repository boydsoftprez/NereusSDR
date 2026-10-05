// NereusSDR for iOS: the fake Core serves a newer Core's band grid, band select, notch at a slice and Re-tune only when made with them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import NereusKitTesting
import NereusLink
import NereusMirror
import Testing

/// The fake plays a newer Core (catalogue `bands`, `slice.selectBand`,
/// `notch.addAtSlice`, `clarity-retune`) only behind the capabilities that
/// Core advertises; without them it answers as a Core before them, lowering
/// what the suite's Core advertises.
@Suite("FakeStation, a newer Core", .serialized)
@MainActor
struct FakeStationAdditionsTests {
    /// The media control operations the Core sent, in order.
    final class MediaSeen {
        var payloads: [[String: LinkJSON]] = []
    }

    struct Rig {
        let media: MediaSeen
        let station: FakeStation
        let session: StationSession
        let mirror: MirrorStore
        let commands: CommandClient
        let feeding: Task<Void, Never>
    }

    private func connected(_ additions: FakeStation.Additions) async throws -> Rig {
        let station = try FakeStation(additions: additions)
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let commands = CommandClient(session: session)
        let events = session.events
        let media = MediaSeen()
        let feeding = Task { @MainActor in
            for await event in events {
                mirror.handle(event)
                await commands.handle(event)
                if case .message(.mediaControl(let control)) = event {
                    media.payloads.append(control.payload)
                }
            }
        }
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await poll { mirror.isSnapshotComplete })
        return Rig(media: media, station: station, session: session, mirror: mirror, commands: commands,
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

    private func invoke(_ rig: Rig, _ verb: String, _ arguments: [CommandArgument]) async throws -> CommandResult {
        try await rig.commands.invoke(verb, arguments: arguments, timeout: .seconds(10))
    }

    @Test("without the additions the capabilities are a Core's before them and the verbs are unknown")
    func olderCore() async throws {
        let rig = try await connected([])
        defer { rig.feeding.cancel() }
        #expect(rig.mirror.capabilityVersion("bandSelectVersion") == 0)
        #expect(rig.mirror.capabilityVersion("notchControlVersion") == 1)
        #expect(rig.mirror.capabilityVersion("displayExtrasVersion") == 0)
        #expect(rig.station.catalogue(#"{"modes":[]}"#) == #"{"modes":[]}"#)
        #expect(rig.station.catalogue(#"{"bands":[],"modes":[]}"#) == #"{"modes":[]}"#)
        for verb in [FakeStation.selectBandVerb, FakeStation.addNotchAtSliceVerb] {
            let result = try await invoke(rig, verb, [CommandArgument(name: "sliceId", value: .int(0)),
                                                      CommandArgument(name: "band", value: .int(3))])
            #expect(!result.accepted)
            #expect(result.reason == FakeStation.unknownVerbReason)
        }
        await rig.session.disconnect()
    }

    @Test("the fake's band grid is the one the suite's catalogues carry")
    func theGridIsTheSuites() throws {
        for file in ["sessions/catalog-anan-g2.json", "sessions/catalog-hermes-lite-2.json"] {
            let fixture = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(file))
            let steps = try #require(fixture["steps"] as? [[String: Any]])
            let found = steps.compactMap { step -> String? in
                guard let message = step["message"] as? [String: Any], message["key"] as? String == "catalog",
                      let properties = message["properties"] as? [[String: Any]] else {
                    return nil
                }
                return properties.first { $0["name"] as? String == "json" }?["value"] as? String
            }
            let json = try #require(found.first)
            let catalogue = try #require(try JSONSerialization.jsonObject(with: Data(json.utf8)) as? [String: Any])
            let bands = try #require(catalogue["bands"] as? [[String: Any]])
            #expect(bands.map { $0["id"] as? Int } == FakeStation.bands.map(\.id), "\(file)")
            #expect(bands.map { $0["label"] as? String } == FakeStation.bands.map(\.label), "\(file)")
        }
    }

    @Test("with the additions the Core advertises them, serves the grid and answers the verbs")
    func newerCore() async throws {
        let rig = try await connected(.all)
        defer { rig.feeding.cancel() }
        #expect(rig.mirror.capabilityVersion("bandSelectVersion") == 1)
        #expect(rig.mirror.capabilityVersion("notchControlVersion") == 2)
        #expect(rig.mirror.capabilityVersion("displayExtrasVersion") == 2)
        let catalogue = try #require(try JSONSerialization.jsonObject(
            with: Data(rig.station.catalogue(#"{"modes":[]}"#).utf8)) as? [String: Any])
        let bands = try #require(catalogue["bands"] as? [[String: Any]])
        #expect(bands.map { $0["label"] as? String } == FakeStation.bands.map(\.label))

        // Band select: the Core reports the slice's new band.
        let select = try await invoke(rig, FakeStation.selectBandVerb, [CommandArgument(name: "sliceId", value: .int(0)),
                                                                         CommandArgument(name: "band", value: .int(3))])
        #expect(select.accepted)
        #expect(await poll { rig.mirror.object("slice:0")?["band"] == .enumeration(3) })
        let unknownBand = try await invoke(rig, FakeStation.selectBandVerb,
                                           [CommandArgument(name: "sliceId", value: .int(0)),
                                            CommandArgument(name: "band", value: .int(11))])
        #expect(unknownBand.reason == FakeStation.unknownBandReason)
        let unknownSlice = try await invoke(rig, FakeStation.addNotchAtSliceVerb,
                                            [CommandArgument(name: "sliceId", value: .int(7))])
        #expect(unknownSlice.reason == FakeStation.unknownSliceReason)
        let notch = try await invoke(rig, FakeStation.addNotchAtSliceVerb, [CommandArgument(name: "sliceId", value: .int(0))])
        #expect(notch.accepted)
        #expect(notch.affectedKeys == ["notches"])

        // A queued refusal, with the Core's words.
        rig.station.refuseNext(FakeStation.selectBandVerb, reason: "Not while transmitting.")
        let refused = try await invoke(rig, FakeStation.selectBandVerb, [CommandArgument(name: "sliceId", value: .int(0)),
                                                                          CommandArgument(name: "band", value: .int(5))])
        #expect(!refused.accepted)
        #expect(refused.reason == "Not while transmitting.")
        await rig.session.disconnect()
    }

    @Test("with the Core's addresses the fake sends coreAddressesVersion 1, the list in devices, and each change as a delta")
    func coreAddresses() async throws {
        let first = #"{"addresses":["[2001:db8:467f:66e7:ec1f:31ff:fe8e:15f2]:50055"]}"#
        let station = try FakeStation(additions: [.coreAddresses])
        station.setCoreAddresses(first)
        let session = station.makeSession()
        let mirror = MirrorStore(session: session)
        let events = session.events
        let feeding = Task { @MainActor in
            for await event in events {
                mirror.handle(event)
            }
        }
        defer { feeding.cancel() }
        await session.connect()
        #expect(await station.waitUntilLive())
        #expect(await poll { mirror.isSnapshotComplete })
        #expect(mirror.capabilityVersion("coreAddressesVersion") == 1)
        #expect(mirror.object("devices")?["coreAddresses"] == .text(first))
        #expect(mirror.propertyNames(ofClass: FakeStation.devicesClass)?.last == "coreAddresses")
        let renumbered = #"{"addresses":["[2001:db8:467f:1:ec1f:31ff:fe8e:15f2]:50055"]}"#
        await station.deliverCoreAddresses(renumbered)
        #expect(await poll { mirror.object("devices")?["coreAddresses"] == .text(renumbered) })
        await session.disconnect()

        // Without it, an older Core: neither the capability nor the property.
        let older = try await connected([])
        defer { older.feeding.cancel() }
        #expect(older.mirror.capabilityVersion("coreAddressesVersion") == 0)
        #expect(older.mirror.object("devices")?["coreAddresses"] == nil)
        await older.session.disconnect()
    }

    @Test("with 2 m the fake sends band2mVersion, 2 m after 6 m in its grid, and selects band 27")
    func twoMetres() async throws {
        let rig = try await connected([.bands, .bandSelect, .band2m])
        defer { rig.feeding.cancel() }
        #expect(rig.mirror.capabilityVersion("band2mVersion") == 1)
        let catalogue = try #require(try JSONSerialization.jsonObject(
            with: Data(rig.station.catalogue(#"{"modes":[]}"#).utf8)) as? [String: Any])
        let bands = try #require(catalogue["bands"] as? [[String: Any]])
        #expect(bands.map { $0["label"] as? String }
                == ["160", "80", "60", "40", "30", "20", "17", "15", "12", "10", "6", "2", "WWV"])
        #expect(bands.map { $0["id"] as? Int }.dropLast().last == 27)
        let select = try await invoke(rig, FakeStation.selectBandVerb, [CommandArgument(name: "sliceId", value: .int(0)),
                                                                         CommandArgument(name: "band", value: .int(27))])
        #expect(select.accepted)
        #expect(await poll { rig.mirror.object("slice:0")?["band"] == .enumeration(27) })
        await rig.session.disconnect()

        // Without it, the grid and the verb are a Core's before 2 m.
        let older = try await connected([.bands, .bandSelect])
        defer { older.feeding.cancel() }
        #expect(older.mirror.capabilityVersion("band2mVersion") == 0)
        let refused = try await invoke(older, FakeStation.selectBandVerb, [CommandArgument(name: "sliceId", value: .int(0)),
                                                                            CommandArgument(name: "band", value: .int(27))])
        #expect(refused.reason == FakeStation.unknownBandReason)
        await older.session.disconnect()
    }

    @Test("Re-tune is refused in the refused-display shape only by a Core that offers it")
    func retune() async throws {
        let payload: [String: LinkJSON] = ["op": .string(FakeStation.clarityRetuneOp), "connectionId": .string("c"),
                                           "endpointId": .number(1)]
        for additions in [FakeStation.Additions(), .clarityRetune] {
            let rig = try await connected(additions)
            rig.station.refuseNext(FakeStation.clarityRetuneOp, reason: "This display is not in Clarity.")
            try await rig.session.send(.mediaControl(LinkMessage.MediaControl(payload: payload)))
            #expect(await rig.station.waitForMessage { $0 == .mediaControl(LinkMessage.MediaControl(payload: payload)) } != nil)
            if additions.isEmpty {
                // An older Core sends nothing back.
                for _ in 0..<5_000 {
                    await Task.yield()
                }
                #expect(rig.media.payloads.isEmpty)
            } else {
                #expect(await poll { !rig.media.payloads.isEmpty })
                let refusal = try #require(rig.media.payloads.first)
                #expect(refusal["op"] == .string("rejected"))
                #expect(refusal["endpointId"] == .number(1))
                #expect(refusal["reason"] == .string("This display is not in Clarity."))
                #expect(Set(refusal.keys) == ["op", "connectionId", "endpointId", "revision", "reason"])
            }
            rig.feeding.cancel()
            await rig.session.disconnect()
        }
    }

    @Test("with remote transmit the fake keys one key per device as the Core's RemoteKeying does")
    func keyingAsTheCoreDoes() async throws {
        let rig = try await connected([.remoteTx])
        defer { rig.feeding.cancel() }
        #expect(rig.mirror.capabilityVersion("remoteTxVersion") == 1)
        #expect(rig.mirror.capabilities["txPermitted"] == .bool(true))
        func key(_ verb: String, _ arguments: [CommandArgument]) async throws -> CommandResult {
            try await rig.commands.invoke(verb, arguments: arguments, copies: 3, timeout: .seconds(10))
        }
        let screen = [CommandArgument(name: "trigger", value: .text("screen"))]
        // An unkey with nothing on is accepted and changes nothing; the key after it keys.
        #expect(try await key("tx.unkey", [CommandArgument(name: "epoch", value: .int(4_294_967_295))]).accepted)
        #expect(!rig.station.keyed)
        let first = try await key("tx.key", screen)
        #expect(first.values["epoch"] == .int(1))
        #expect(rig.station.keyed)
        // TUNE while the key is on answers that key's epoch: one key per device.
        #expect(try await key("tx.tune", [CommandArgument(name: "on", value: .bool(true))]).values["epoch"] == .int(1))
        // An unkey older than the live key is ignored.
        #expect(try await key("tx.unkey", [CommandArgument(name: "epoch", value: .int(0))]).accepted)
        #expect(rig.station.keyed)
        #expect(try await key("tx.unkey", [CommandArgument(name: "epoch", value: .int(1))]).accepted)
        #expect(!rig.station.keyed)
        // TUNE keys with a new epoch, and a PTT unkey at it ends TUNE too.
        #expect(try await key("tx.tune", [CommandArgument(name: "on", value: .bool(true))]).values["epoch"] == .int(2))
        #expect(try await key("tx.unkey", [CommandArgument(name: "epoch", value: .int(2))]).accepted)
        #expect(!rig.station.keyed)
        // TUNE off ends TUNE; TUNE off while the PTT's key is on leaves that key.
        #expect(try await key("tx.tune", [CommandArgument(name: "on", value: .bool(true))]).values["epoch"] == .int(3))
        #expect(try await key("tx.tune", [CommandArgument(name: "on", value: .bool(false))]).accepted)
        #expect(!rig.station.keyed)
        #expect(try await key("tx.key", screen).values["epoch"] == .int(4))
        #expect(try await key("tx.tune", [CommandArgument(name: "on", value: .bool(false))]).accepted)
        #expect(rig.station.keyed)
        // Another device takes transmit: this device's key ends, and its
        // releases with nothing on are refused otherDeviceHolds, as
        // RemoteKeying::stopFrom refuses them (RemoteKeying.cpp:549-556).
        rig.station.otherDeviceTakes("MacBook Pro")
        #expect(!rig.station.keyed)
        for (verb, arguments) in [("tx.unkey", [CommandArgument(name: "epoch", value: .int(4))]),
                                  ("tx.tune", [CommandArgument(name: "on", value: .bool(false))]),
                                  ("tx.twoTone", [CommandArgument(name: "on", value: .bool(false))])] {
            let refused = try await key(verb, arguments)
            #expect(!refused.accepted)
            #expect(refused.reason == "MacBook Pro has the transmitter. Take it to stop the transmission.")
            #expect(refused.values["refusalCode"] == .text("otherDeviceHolds"))
        }
        rig.station.otherDeviceReleases()
        #expect(try await key("tx.unkey", [CommandArgument(name: "epoch", value: .int(4))]).accepted)
        await rig.session.disconnect()
    }

    @Test("with the tuner's tune the fake advertises remoteTxVersion 2 and keys tx.tunerTune as RemoteKeying does")
    func tunerTuneAsTheCoreDoes() async throws {
        let rig = try await connected([.remoteTx, .tunerTune])
        defer { rig.feeding.cancel() }
        #expect(rig.mirror.capabilityVersion("remoteTxVersion") == 2)
        func key(_ verb: String, _ arguments: [CommandArgument]) async throws -> CommandResult {
            try await rig.commands.invoke(verb, arguments: arguments, copies: 3, timeout: .seconds(10))
        }
        let on = [CommandArgument(name: "on", value: .bool(true))]
        let off = [CommandArgument(name: "on", value: .bool(false))]
        #expect(try await key("tx.tunerTune", on).values["epoch"] == .int(1))
        #expect(rig.station.keyedVerb == "tx.tunerTune")
        #expect(try await key("tx.tunerTune", off).accepted)
        #expect(!rig.station.keyed)
        // Never started on the air, this device's own key included.
        #expect(try await key("tx.key", [CommandArgument(name: "trigger", value: .text("screen"))]).accepted)
        let refused = try await key("tx.tunerTune", on)
        #expect(!refused.accepted)
        #expect(refused.reason == "The radio is on the air. Try again when it stops.")
        #expect(refused.values["refusalCode"] == .text("holderOnAir"))
        #expect(rig.station.keyedVerb == "tx.key")
        await rig.session.disconnect()
        // Without the addition the verb is unknown and the version stays 1.
        let older = try await connected([.remoteTx])
        defer { older.feeding.cancel() }
        #expect(older.mirror.capabilityVersion("remoteTxVersion") == 1)
        let unknown = try await older.commands.invoke("tx.tunerTune", arguments: on, copies: 3, timeout: .seconds(10))
        #expect(!unknown.accepted)
        await older.session.disconnect()
    }

    @Test("with the transmit stage readings the fake advertises txReadingsVersion 3; without them it sends none")
    func transmitStageReadings() async throws {
        let rig = try await connected([.remoteTx, .txStageReadings])
        #expect(rig.mirror.capabilityVersion("txStateVersion") == 2)
        #expect(rig.mirror.capabilityVersion("txReadingsVersion") == 3)
        rig.feeding.cancel()
        await rig.session.disconnect()

        // An older Core: remote transmit without the stage readings.
        let older = try await connected([.remoteTx])
        #expect(older.mirror.capabilityVersion("txStateVersion") == 2)
        #expect(older.mirror.capabilities["txReadingsVersion"] == nil)
        older.feeding.cancel()
        await older.session.disconnect()
    }
}
