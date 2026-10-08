// NereusSDR for iOS: the Rotor page's model against the real command client and a clock moved by the test
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMirror
@testable import NereusSDR
import Testing

/// The Rotor page's touch rules (design, Touch safety): a drag on the dial
/// only selects; Turn sends one `setRotorTarget`; an unsent selection drops
/// after 15 s and a sent target never lapses; a held nudge repeats every
/// 250 ms and ends with `active` false when let go, when the page goes and
/// when the app leaves the foreground. With no rotor, a Core too old, or the
/// rotor not connected, nothing is sent and the reason shows.
@Suite("Rotor page model", .serialized)
@MainActor
struct RotorPageModelTests {
    @MainActor final class Recorded {
        var commands: [LinkMessage.CommandInvoke] = []
    }

    @MainActor struct Rig {
        let clock: TestLinkClock
        let mirror: MirrorStore
        let recorded: Recorded
        let commands: CommandClient
        let model: RotorModel

        init(version: Int64 = 1, rotor: [LinkMessage.PropertyEntry]? = Rig.connectedRotor) async {
            let clock = TestLinkClock()
            let recorded = Recorded()
            self.clock = clock
            self.recorded = recorded
            mirror = MirrorStore(send: { _ in }, clock: clock)
            commands = CommandClient(clock: clock, send: { message in
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
            if let rotor {
                mirror.apply(.objectCreate(.init(key: RotorModel.objectKey, className: "RotorModel",
                                                 properties: rotor)))
            }
            mirror.apply(.snapshotComplete)
            mirror.handle(.stateChanged(.ready))
            model = RotorModel(mirror: mirror, commands: commands, clock: clock)
            model.refresh()
        }

        /// A GS-232B rotor on COM4, connected, at 47 degrees, south stop, 450 degrees.
        static let connectedRotor: [LinkMessage.PropertyEntry] = rotor()

        static func rotor(phase: Int64 = 6, driver: Int64 = 2, axes: Int64 = 0) -> [LinkMessage.PropertyEntry] {
            [
                .init(name: "connectionPhase", value: .enumeration(phase)),
                .init(name: "driver", value: .enumeration(driver)),
                .init(name: "label", value: .utf8("Easy Rotor Control on COM4")),
                .init(name: "serialPort", value: .utf8("COM4")),
                .init(name: "serialPorts", value: .utf8("COM3\nCOM4")),
                .init(name: "axes", value: .enumeration(axes)),
                .init(name: "rangeDeg", value: .i64(450)),
                .init(name: "endStop", value: .enumeration(2)),
                .init(name: "spanPositionDeg", value: .f64(227)),
                .init(name: "positionFresh", value: .bool(true)),
                .init(name: "azimuthDeg", value: .f64(47)),
                .init(name: "elevationDeg", value: .f64(axes == 1 ? 10 : -1)),
                .init(name: "targetAzimuthDeg", value: .f64(-1)),
                .init(name: "targetElevationDeg", value: .f64(-1)),
                .init(name: "motion", value: .enumeration(0)),
                .init(name: "presets", value: .utf8("EU\t45\nJA\t330\nVK\t250")),
            ]
        }

        func delta(_ properties: [LinkMessage.PropertyEntry]) {
            mirror.apply(.delta(.init(key: RotorModel.objectKey, properties: properties)))
            model.refresh()
        }

        func answer(_ command: LinkMessage.CommandInvoke, accepted: Bool = true, reason: String = "") async {
            await commands.receive(.commandResult(.init(verb: command.verb, id: command.id,
                                                        accepted: accepted, reason: reason,
                                                        affected: [], values: nil)))
        }

        func sent(_ verb: String) -> [LinkMessage.CommandInvoke] {
            recorded.commands.filter { $0.verb == verb }
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

    // MARK: Select, then Turn

    @Test("a drag on the dial only selects, rounded to a whole degree; nothing is sent")
    func dragSendsNothing() async {
        let rig = await Rig()
        #expect(rig.model.turnReason == nil)
        rig.model.select(azimuth: 123.4)
        rig.model.select(azimuth: 361.2)
        await drain()
        #expect(rig.model.selection == 1)
        #expect(rig.model.dialTarget == 1)
        #expect(rig.recorded.commands.isEmpty)
    }

    @Test("Turn sends one setRotorTarget with the selection and elevation -1, then the selection is gone")
    func turnSendsOnce() async throws {
        let rig = await Rig()
        rig.model.select(azimuth: 120)
        rig.model.turn()
        #expect(await turns { rig.recorded.commands.count == 1 })
        let command = try #require(rig.recorded.commands.first)
        #expect(command.verb == "setRotorTarget")
        #expect(argument(command, "azimuthDeg") == .f64(120))
        #expect(argument(command, "elevationDeg") == .f64(-1))
        #expect(rig.model.selection == nil && rig.model.awaitingTarget == 120 && rig.model.dialTarget == 120)
        // A second Turn has nothing to send.
        rig.model.turn()
        await drain()
        #expect(rig.recorded.commands.count == 1)
        await rig.answer(command)
        #expect(await turns { rig.model.awaitingTarget == nil })
        #expect(rig.model.note == nil)
    }

    @Test("an unsent selection drops at 15 s, not before")
    func selectionLapses() async {
        let rig = await Rig()
        rig.model.select(azimuth: 200)
        await rig.clock.advance(by: 14_999)
        #expect(rig.model.selection == 200)
        // A new drag starts the 15 s again.
        rig.model.select(azimuth: 210)
        await rig.clock.advance(by: 14_999)
        #expect(rig.model.selection == 210)
        await rig.clock.advance(by: 1)
        #expect(rig.model.selection == nil)
        #expect(rig.recorded.commands.isEmpty)
    }

    @Test("a sent target never lapses: the Core's target stays drawn and nothing more is sent")
    func sentTargetNeverLapses() async throws {
        let rig = await Rig()
        rig.model.select(azimuth: 120)
        rig.model.turn()
        #expect(await turns { rig.recorded.commands.count == 1 })
        await rig.answer(try #require(rig.recorded.commands.first))
        #expect(await turns { rig.model.awaitingTarget == nil })
        rig.delta([.init(name: "targetAzimuthDeg", value: .f64(120)), .init(name: "motion", value: .enumeration(1))])
        await rig.clock.advance(by: 60_000)
        #expect(rig.model.dialTarget == 120)
        #expect(rig.model.state?.target == 120)
        #expect(rig.clock.pendingDueTimes.isEmpty)
        #expect(rig.recorded.commands.count == 1)
    }

    @Test("a preset is one tap; the long path turns to its reciprocal")
    func presets() async throws {
        let rig = await Rig()
        #expect(rig.model.presets.map(\.name) == ["EU", "JA", "VK"])
        #expect(rig.model.presets.map(\.degrees) == [45, 330, 250])
        rig.model.turn(to: rig.model.presets[1])
        #expect(await turns { rig.recorded.commands.count == 1 })
        #expect(argument(try #require(rig.recorded.commands.last), "azimuthDeg") == .f64(330))
        rig.model.setLongPath(true)
        // Toggling the path selects the other way round; it still needs Turn.
        #expect(rig.model.selection == 150)
        await drain()
        #expect(rig.recorded.commands.count == 1)
        rig.model.turn(to: rig.model.presets[0])
        #expect(await turns { rig.recorded.commands.count == 2 })
        #expect(argument(try #require(rig.recorded.commands.last), "azimuthDeg") == .f64(225))
        #expect(rig.model.selection == nil)
    }

    @Test("Stop is one tap and drops a selection")
    func stop() async {
        let rig = await Rig()
        rig.model.select(azimuth: 90)
        rig.model.stop()
        #expect(await turns { rig.sent("stopRotor").count == 1 })
        #expect(rig.model.selection == nil)
    }

    // MARK: The nudge hold

    @Test("a held nudge repeats every 250 ms and ends with active false when let go")
    func holdRepeats() async throws {
        let rig = await Rig()
        rig.model.startHold(.clockwise)
        #expect(await turns { rig.sent("nudgeRotor").count == 1 })
        let first = try #require(rig.sent("nudgeRotor").first)
        #expect(argument(first, "direction") == .enumeration(1) && argument(first, "active") == .bool(true))
        await rig.answer(first)
        await rig.clock.advance(by: 249)
        await drain()
        #expect(rig.sent("nudgeRotor").count == 1)
        await rig.clock.advance(by: 1)
        #expect(await turns { rig.sent("nudgeRotor").count == 2 })
        await rig.clock.advance(by: 250)
        #expect(await turns { rig.sent("nudgeRotor").count == 3 })
        #expect(rig.sent("nudgeRotor").allSatisfy { argument($0, "active") == .bool(true) })
        rig.model.endHold()
        #expect(await turns { rig.sent("nudgeRotor").count == 4 })
        let last = try #require(rig.sent("nudgeRotor").last)
        #expect(argument(last, "direction") == .enumeration(1) && argument(last, "active") == .bool(false))
        #expect(rig.model.holding == nil)
        await rig.clock.advance(by: 2_000)
        await drain()
        #expect(rig.sent("nudgeRotor").count == 4)
    }

    @Test("leaving the page ends the hold and drops the selection; the app leaving the foreground ends the hold")
    func leavingEndsHold() async throws {
        let rig = await Rig()
        rig.model.startHold(.counterClockwise)
        #expect(await turns { rig.sent("nudgeRotor").count == 1 })
        await rig.answer(try #require(rig.sent("nudgeRotor").first))
        rig.model.pageLeft()
        #expect(await turns { rig.sent("nudgeRotor").count == 2 })
        #expect(argument(try #require(rig.sent("nudgeRotor").last), "active") == .bool(false))
        await rig.clock.advance(by: 1_000)
        await drain()
        #expect(rig.sent("nudgeRotor").count == 2)

        rig.model.select(azimuth: 300)
        rig.model.startHold(.clockwise)
        // A nudge drops the selection: the rotor moves by hand now.
        #expect(rig.model.selection == nil)
        #expect(await turns { rig.sent("nudgeRotor").count == 3 })
        rig.model.sceneLeft()
        #expect(await turns { rig.sent("nudgeRotor").count == 4 })
        #expect(argument(try #require(rig.sent("nudgeRotor").last), "active") == .bool(false))
        #expect(rig.model.holding == nil)
    }

    @Test("Up and Down are for an az/el rotor; on an azimuth rotor they send nothing and say why")
    func elevation() async {
        let rig = await Rig()
        #expect(rig.model.elevationReason == RotorModel.azimuthOnlyReason)
        rig.model.startHold(.up)
        await drain()
        #expect(rig.recorded.commands.isEmpty)
        #expect(rig.model.note == RotorModel.azimuthOnlyReason)
        let azEl = await Rig(rotor: Rig.rotor(axes: 1))
        #expect(azEl.model.elevationReason == nil)
        azEl.model.startHold(.up)
        #expect(await turns { azEl.sent("nudgeRotor").count == 1 })
        #expect(argument(azEl.sent("nudgeRotor")[0], "direction") == .enumeration(3))
        azEl.model.endHold()
    }

    // MARK: Disabled, never hidden

    @Test("no rotor, a Core too old, the rotor not connected, no Core: each control says why and sends nothing")
    func reasons() async {
        let older = await Rig(version: 0, rotor: nil)
        #expect(older.model.turnReason == RotorModel.olderCoreReason)
        #expect(older.model.setupReason == RotorModel.olderCoreReason)
        #expect(older.model.statusLine == RotorModel.olderCoreReason)

        let none = await Rig(rotor: Rig.rotor(phase: 0, driver: 0))
        #expect(none.model.turnReason == RotorModel.noRotorReason)
        #expect(none.model.setupReason == nil)
        #expect(none.model.toolLine == RotorModel.noRotorReason)

        let away = await Rig(rotor: Rig.rotor(phase: 5))
        #expect(away.model.turnReason == RotorModel.notConnectedReason)
        away.model.select(azimuth: 90)
        #expect(away.model.selection == nil)
        away.model.stop()
        await drain()
        #expect(away.recorded.commands.isEmpty)
        #expect(away.model.note == RotorModel.notConnectedReason)

        // The rotor drops while a selection waits: the selection goes.
        let live = await Rig()
        live.model.select(azimuth: 90)
        live.delta([.init(name: "connectionPhase", value: .enumeration(5))])
        #expect(live.model.selection == nil)
    }

    @Test("the setup card's Save sends configureRotor with every field")
    func configure() async throws {
        let rig = await Rig()
        let state = try #require(rig.model.state)
        var setup = RotorModel.Setup(state)
        setup.serialPort = "COM3"
        rig.model.configure(setup)
        #expect(await turns { rig.sent("configureRotor").count == 1 })
        let command = try #require(rig.sent("configureRotor").first)
        #expect(command.args.map(\.name) == ["driver", "serialPort", "baud", "host", "port", "hamlibModel",
                                                  "axes", "endStop", "rangeDeg", "offsetDeg"])
        #expect(argument(command, "serialPort") == .utf8("COM3"))
        #expect(argument(command, "driver") == .enumeration(2))
        #expect(argument(command, "endStop") == .enumeration(2))
        #expect(argument(command, "rangeDeg") == .i64(450))
    }

    @Test("the setup card asks the Core for its serial ports when shown and every 20 s, quietly, until hidden")
    func setupAsksForPorts() async {
        let rig = await Rig()
        rig.model.setupShown()
        #expect(await turns { rig.sent("refreshRotorPorts").count == 1 })
        #expect(rig.sent("refreshRotorPorts").first?.args.isEmpty == true)
        await rig.clock.advance(by: 19_999)
        await drain()
        #expect(rig.sent("refreshRotorPorts").count == 1)
        await rig.clock.advance(by: 1)
        #expect(await turns { rig.sent("refreshRotorPorts").count == 2 })
        rig.model.setupHidden()
        await rig.clock.advance(by: 60_000)
        await drain()
        #expect(rig.sent("refreshRotorPorts").count == 2)
        #expect(rig.model.note == nil)

        // A Core too old is not asked, and nothing shows.
        let older = await Rig(version: 0, rotor: nil)
        older.model.setupShown()
        await drain()
        #expect(older.recorded.commands.isEmpty)
        #expect(older.model.note == nil)
        older.model.setupHidden()
    }

    // MARK: The route

    @Test("the predicted route follows the end stop and the overlap, as the bench capture turned")
    func route() {
        // South stop, 450 degrees: the span runs from 180 clockwise.
        #expect(RotorRoute.travel(heading: 183, spanDeg: 3, target: 10, endStop: .south, rangeDeg: 450) == 187)
        #expect(RotorRoute.travel(heading: 292, spanDeg: 112, target: 0, endStop: .south, rangeDeg: 450) == 68)
        #expect(RotorRoute.travel(heading: 301, spanDeg: 121, target: 180, endStop: .south, rangeDeg: 450) == -121)
        // No end stop: the shortest way.
        #expect(RotorRoute.travel(heading: 350, spanDeg: -1, target: 10, endStop: .none, rangeDeg: 360) == 20)
        // An end stop with the span unknown: no prediction.
        #expect(RotorRoute.travel(heading: 350, spanDeg: -1, target: 10, endStop: .north, rangeDeg: 360) == nil)
        #expect(RotorModel.headingText(47) == "047\u{00B0}")
        #expect(RotorModel.headingText(nil) == "---\u{00B0}")
    }

    @Test("the predicted route answers the shared route vectors as the Core's planner does")
    func sharedRouteVectors() throws {
        // tests/data/rotor/route-vectors.json is also read by the Core's
        // tst_rotor_route, so the phone and the Core agree case by case.
        let file = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
            .appendingPathComponent("tests/data/rotor/route-vectors.json")
        let object = try #require(try JSONSerialization.jsonObject(with: Data(contentsOf: file)) as? [String: Any])
        let cases = try #require(object["cases"] as? [[String: Any]])
        #expect(cases.count >= 10)
        for vector in cases {
            let name = vector["name"] as? String ?? "?"
            let endStop: RotorModel.EndStop
            switch vector["endStop"] as? String {
            case "north": endStop = .north
            case "south": endStop = .south
            default: endStop = .none
            }
            func number(_ key: String) -> Double { (vector[key] as? NSNumber)?.doubleValue ?? .nan }
            let travel = RotorRoute.travel(heading: number("headingDeg"), spanDeg: number("spanDeg"),
                                           target: number("targetDeg"), endStop: endStop,
                                           rangeDeg: Int64(number("rangeDeg")), offsetDeg: number("offsetDeg"))
            if vector["travelDeg"] is NSNull {
                #expect(travel == nil, "\(name)")
            } else {
                let expected = number("travelDeg")
                #expect(travel.map { abs($0 - expected) < 1e-9 } == true, "\(name): \(String(describing: travel))")
            }
        }
    }
}
