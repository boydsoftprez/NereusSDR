// NereusSDR for iOS: a spot's bearing, Turn beam on its details, and the auto-turn setting, against the real command client
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// Spots and the rotor (remote rotor control, Bearings on spots; design,
/// Turn to a spot): each Core-served spot carries `bearingDeg`, the short
/// path, -1 when not known; the long path is its reciprocal. Turn beam on
/// a spot's details is one tap and sends one `setRotorTarget`; it is greyed
/// with its reason with no rotor or no bearing. "Turn the beam when I tune
/// to a spot" is this phone's own setting, off by default; with it off,
/// tuning never moves the rotor.
@Suite("Spots and the rotor", .serialized)
@MainActor
struct SpotTurnBeamTests {
    @MainActor final class Recorded {
        var commands: [LinkMessage.CommandInvoke] = []
        var writes: [LinkMessage.PropertyWrite] = []
    }

    @MainActor struct Rig {
        let mirror: MirrorStore
        let recorded: Recorded
        let commands: CommandClient
        let records: RecordStreamClient
        let main: MainScreenModel
        let phone: PhoneSettings
        let spots: SpotsModel

        init(version: Int64 = 1, driver: Int64 = 2, defaults: UserDefaults? = nil) async {
            let recorded = Recorded()
            self.recorded = recorded
            mirror = MirrorStore(send: { message in
                if case .propertyWrite(let write) = message {
                    await MainActor.run { recorded.writes.append(write) }
                }
            })
            commands = CommandClient(send: { message in
                if case .commandInvoke(let command) = message {
                    await MainActor.run { recorded.commands.append(command) }
                }
            })
            await commands.handle(.stateChanged(.ready))
            mirror.handle(.stateChanged(.receivingSnapshot))
            mirror.apply(.authResult(.init(accepted: true, reason: "", retryable: false)))
            mirror.apply(.hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")))
            mirror.apply(.capabilities(.init(properties: version > 0
                ? [.init(name: RotorModel.capability, value: .i64(version))] : [])))
            if version > 0 {
                mirror.apply(.objectCreate(.init(key: RotorModel.objectKey, className: "RotorModel", properties: [
                    .init(name: "connectionPhase", value: .enumeration(driver == 0 ? 0 : 6)),
                    .init(name: "driver", value: .enumeration(driver)),
                    .init(name: "positionFresh", value: .bool(true)),
                    .init(name: "azimuthDeg", value: .f64(47)),
                ])))
            }
            mirror.apply(.objectCreate(.init(key: "slice:0", className: "SliceModel", properties: [
                .init(ordinal: 1, name: "frequency", value: .f64(7_236_400)),
                .init(ordinal: 2, name: "dspMode", value: .enumeration(0)),
                .init(ordinal: 11, name: "active", value: .bool(true)),
                .init(ordinal: 13, name: "sliceIndex", value: .i64(0)),
                .init(ordinal: 35, name: "locked", value: .bool(false)),
            ])))
            mirror.apply(.snapshotComplete)
            mirror.handle(.stateChanged(.ready))
            records = RecordStreamClient(mirror: mirror, commands: commands)
            main = MainScreenModel(mirror: mirror, settings: SettingsProxyClient(send: { _ in }), commands: commands,
                                   operations: BandSubscriber.Operations(subscribe: { _ in }, unsubscribe: { _ in }))
            main.rotor.refresh()
            phone = PhoneSettings(defaults: defaults
                ?? UserDefaults(suiteName: "SpotTurnBeamTests-\(UUID().uuidString)") ?? .standard)
            spots = SpotsModel(records: records, mirror: mirror, commands: commands, settings: nil,
                               slices: main.slices, catalogFeed: main.catalogFeed, phone: phone, rotor: main.rotor)
            records.apply(LinkMessage.RecordBatch(stream: RecordStreamClient.spotsStream, generation: 1, reset: true,
                                                  upserts: [Rig.record("1", 7_250_000, "JA1ABC", 330),
                                                            Rig.record("2", 7_260_000, "W1AW", -1)],
                                                  removes: []))
        }

        static func record(_ id: String, _ hz: Double, _ call: String, _ bearing: Double?)
            -> LinkMessage.RecordBatch.Record {
            var fields: [String: LinkJSON] = [
                "frequencyHz": .number(hz), "call": .string(call), "mode": .string("CW"),
                "source": .string("Cluster"), "spotter": .string("K1TTT"), "band": .number(3),
            ]
            if let bearing {
                fields["bearingDeg"] = .number(bearing)
            }
            return LinkMessage.RecordBatch.Record(id: id, fields: fields)
        }

        func spot(_ call: String) -> SpotsModel.Spot? {
            spots.spots.first { $0.call == call }
        }

        func sent(_ verb: String) -> [LinkMessage.CommandInvoke] {
            recorded.commands.filter { $0.verb == verb }
        }

        var frequencyWrites: [LinkMessage.PropertyWrite] {
            recorded.writes.filter { $0.key == "slice:0" && $0.properties.first?.name == "frequency" }
        }
    }

    private func turns(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<20_000 {
            if condition() { return true }
            await Task.yield()
        }
        return condition()
    }

    /// Lets queued sends run, so a check that nothing was sent can fail.
    private func drain() async {
        for _ in 0..<2_000 {
            await Task.yield()
        }
    }

    private func argument(_ command: LinkMessage.CommandInvoke, _ name: String) -> LinkMessage.PropertyValue? {
        command.args.first { $0.name == name }?.value
    }

    // MARK: The bearing on a spot

    @Test("a spot's bearingDeg is read from a Core that advertises remote rotor control; long path is its reciprocal")
    func bearingIsRead() async throws {
        let rig = await Rig()
        #expect(await turns { rig.spots.spots.count == 2 })
        let ja = try #require(rig.spot("JA1ABC"))
        #expect(ja.bearingDeg == 330 && ja.shortPathDeg == 330 && ja.longPathDeg == 150)
        let w1 = try #require(rig.spot("W1AW"))
        #expect(w1.bearingDeg == -1 && w1.shortPathDeg == nil && w1.longPathDeg == nil)

        // The details: the bearing after Source, short and long; none when not known.
        #expect(SpotDetailsSheet.rows(ja).map(\.name) == ["Mode", "Source", "Bearing", "Spotter"])
        #expect(SpotDetailsSheet.bearingWords(ja) == "330\u{00B0} short \u{00B7} 150\u{00B0} long")
        #expect(SpotDetailsSheet.rows(w1).map(\.name) == ["Mode", "Source", "Spotter"])
        #expect(SpotDetailsSheet.turnBeamTitle(ja) == "Turn beam 330\u{00B0}")
        #expect(SpotDetailsSheet.turnBeamTitle(w1) == "Turn beam")

        // A record with no bearing, or one from a Core without the capability, reads -1.
        #expect(SpotsModel.spot(Rig.record("3", 7_000_000, "K1ABC", nil), bearings: true).bearingDeg == -1)
        #expect(SpotsModel.spot(Rig.record("4", 7_000_000, "K1ABC", 45), bearings: false).bearingDeg == -1)
        #expect(SpotsModel.spot(Rig.record("5", 7_000_000, "K1ABC", 45), bearings: true).bearingDeg == 45)
        // Long path wraps: 200.4 short is 20.4 long.
        let wrapped = SpotsModel.spot(Rig.record("6", 7_000_000, "VK2ABC", 200.4), bearings: true)
        #expect(wrapped.longPathDeg.map { abs($0 - 20.4) < 1e-9 } == true)
        #expect(SpotDetailsSheet.bearingWords(wrapped) == "200\u{00B0} short \u{00B7} 20\u{00B0} long")
    }

    @Test("a Core that does not advertise remote rotor control: its spots' bearings read -1")
    func olderCoreHasNoBearings() async throws {
        let rig = await Rig(version: 0)
        #expect(await turns { rig.spots.spots.count == 2 })
        let ja = try #require(rig.spot("JA1ABC"))
        #expect(ja.bearingDeg == -1)
        #expect(rig.main.rotor.beamReason(bearing: ja.shortPathDeg) == RotorModel.olderCoreReason)
    }

    // MARK: Turn beam

    @Test("Turn beam on a spot sends one setRotorTarget with its short path and elevation -1")
    func turnBeamSendsOnce() async throws {
        let rig = await Rig()
        #expect(await turns { rig.spots.spots.count == 2 })
        let ja = try #require(rig.spot("JA1ABC"))
        let rotor = rig.main.rotor
        #expect(rotor.beamReason(bearing: ja.shortPathDeg) == nil)
        rotor.turnBeam(toBearing: ja.shortPathDeg)
        #expect(await turns { rig.recorded.commands.count == 1 })
        await drain()
        #expect(rig.recorded.commands.count == 1)
        let command = try #require(rig.recorded.commands.first)
        #expect(command.verb == "setRotorTarget")
        #expect(argument(command, "azimuthDeg") == .f64(330))
        #expect(argument(command, "elevationDeg") == .f64(-1))
        #expect(rotor.awaitingTarget == 330)
    }

    @Test("Turn beam is greyed with its reason with no bearing or no rotor, and sends nothing")
    func turnBeamGreyed() async throws {
        let rig = await Rig()
        #expect(await turns { rig.spots.spots.count == 2 })
        let w1 = try #require(rig.spot("W1AW"))
        rig.main.rotor.turnBeam(toBearing: w1.shortPathDeg)
        #expect(rig.main.rotor.beamReason(bearing: w1.shortPathDeg) == RotorModel.noBearingReason)
        #expect(rig.main.rotor.note == RotorModel.noBearingReason)

        let none = await Rig(driver: 0)
        #expect(await turns { none.spots.spots.count == 2 })
        let ja = try #require(none.spot("JA1ABC"))
        #expect(ja.shortPathDeg == 330)
        #expect(none.main.rotor.beamReason(bearing: ja.shortPathDeg) == RotorModel.noRotorReason)
        none.main.rotor.turnBeam(toBearing: ja.shortPathDeg)
        await drain()
        #expect(rig.recorded.commands.isEmpty)
        #expect(none.recorded.commands.isEmpty)
    }

    // MARK: Auto-turn

    @Test("auto-turn is off by default: tuning to a spot tunes the slice and never moves the rotor")
    func tuningNeverTurnsWhenOff() async throws {
        let rig = await Rig()
        #expect(await turns { rig.spots.spots.count == 2 && rig.main.slices.active != nil })
        #expect(!rig.spots.turnBeamOnTune)
        #expect(rig.spots.tuneReason == nil)
        let ja = try #require(rig.spot("JA1ABC"))
        rig.spots.tune(ja, onBand: true)
        rig.spots.tune(ja, onBand: false)
        // The slice was tuned, so the check below can fail. (The second write
        // of the same frequency waits on the first, which nothing answers here.)
        #expect(await turns { !rig.frequencyWrites.isEmpty })
        await drain()
        #expect(rig.recorded.commands.isEmpty)
    }

    @Test("with auto-turn on, tuning to a spot also turns the beam to its short path; it is kept on this phone")
    func tuningTurnsWhenOn() async throws {
        let defaults = try #require(UserDefaults(suiteName: "SpotTurnBeamTests-\(UUID().uuidString)"))
        let rig = await Rig(defaults: defaults)
        #expect(await turns { rig.spots.spots.count == 2 && rig.main.slices.active != nil })
        rig.spots.setTurnBeamOnTune(true)
        #expect(rig.spots.turnBeamOnTune)
        let ja = try #require(rig.spot("JA1ABC"))
        rig.spots.tune(ja, onBand: false)
        #expect(await turns { rig.sent("setRotorTarget").count == 1 })
        let command = try #require(rig.sent("setRotorTarget").first)
        #expect(argument(command, "azimuthDeg") == .f64(330))
        #expect(await turns { rig.frequencyWrites.count == 1 })

        // A spot with no bearing sends nothing to the rotor.
        let w1 = try #require(rig.spot("W1AW"))
        rig.spots.tune(w1, onBand: false)
        await drain()
        #expect(rig.recorded.commands.count == 1)

        // Read back by the next model on this phone; turned off, it stays off.
        let again = await Rig(defaults: defaults)
        #expect(again.spots.turnBeamOnTune)
        again.spots.setTurnBeamOnTune(false)
        let third = await Rig(defaults: defaults)
        #expect(!third.spots.turnBeamOnTune)
    }
}
