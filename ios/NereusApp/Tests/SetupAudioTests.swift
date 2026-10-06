// NereusSDR for iOS: Setup > Audio > TX Input at description 24 on each radio, and HL2 Swap audio channels open
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-18: the phone reads Setup description 24 (the Core's radio codec
/// lane). Audio > TX Input shows the radio mic group the Core sends for the
/// connected radio: Line In Gain in the radio's 1.5 dB steps from -34.5
/// with one decimal, typed on the decimal pad; the Saturn G2's Mic Tip-Ring
/// second in its group; the Hermes Lite 2's group under its own title with
/// the Core's add-on note on each row; the Red Pitaya's Orion rows greyed
/// as whole rows with the Core's reason. A Core at 23 keeps whole decibels
/// and no Saturn Mic Tip-Ring. HL2 Swap audio channels is a live switch when
/// the Core sends it open, and greyed with an older Core's reason when it
/// sends the closed row. With `NEREUS_MAIN_SHOTS` set, the pictures are
/// written there.
@Suite("Setup Audio, description 24", .serialized)
@MainActor
struct SetupAudioTests {
    typealias Pages = SetupDescribedPagesTests
    typealias Typed = SetupTypedEntryTests

    static let txInputPage = "audio.txInput"
    static let lineInGainId = "audio.txInput.hermesLineInGain"
    static let saturnTipRingId = "audio.txInput.saturnMicTipRing"
    static let swapId = "hardware.hl2Io.swapAudioChannels"
    /// The Core's words (RadioModel::radioMicAddOnNote, orionMicPanelUnavailableReason).
    static let addOnNote = "Needs the Hermes Lite 2 audio add-on board. A stock Hermes Lite 2 sends no mic audio."
    static let redPitayaReason = "These mic settings do not apply to the Red Pitaya."
    /// The reason an older Core sent with its closed Swap audio channels row.
    static let swapClosedReason = "NereusSDR does not send the radio audio of its own, so there is nothing to swap."
    static let txInputPath: [SetupTree.Route] = [.category("Audio"), .described(category: "audio", page: txInputPage)]
    static let hl2IoPath: [SetupTree.Route] = [.category("Hardware"),
                                               .described(category: "hardware", page: Pages.clockPage)]

    /// The radios whose TX Input differs, as the Core's
    /// SetupDescriptionV15 keepSectionForRadio and projectForRadio send it.
    enum Radio {
        case anan7000, g2, hl2, redPitaya
    }

    // MARK: The Core's description for a radio

    /// The Core's categories at `peer`, with Audio as the Core sends it for
    /// `radio` and, with `swapClosed`, Hardware's Swap audio channels as an
    /// older Core sent it.
    static func categories(peer: Int, radio: Radio, swapClosed: Bool = false) throws -> [(id: String, json: String)] {
        try Pages.coreCategories(peer: peer).map { entry in
            if entry.id == "audio" {
                return (entry.id, try audio(entry.json, radio: radio))
            }
            if entry.id == "hardware", swapClosed {
                return (entry.id, try closingSwap(entry.json))
            }
            return entry
        }
    }

    /// One radio's TX Input: its board family's group only; the Hermes group
    /// retitled with the add-on note on each row for the Hermes Lite 2; the
    /// Orion group's rows closed with the Core's reason on the Red Pitaya.
    static func audio(_ json: String, radio: Radio) throws -> String {
        var root = try #require(JSONSerialization.jsonObject(with: Data(json.utf8)) as? [String: Any])
        var pages: [[String: Any]] = []
        for var page in root["pages"] as? [[String: Any]] ?? [] {
            var sections: [[String: Any]] = []
            for var section in page["sections"] as? [[String: Any]] ?? [] {
                guard let family = section.removeValue(forKey: "boardFamily") as? String else {
                    sections.append(section)
                    continue
                }
                var controls = section["controls"] as? [[String: Any]] ?? []
                switch (family, radio) {
                case ("hermes", .hl2):
                    section["title"] = "Radio Mic (Hermes Lite 2)"
                    controls = controls.map { control in
                        var control = control
                        control["tooltip"] = addOnNote
                        return control
                    }
                case ("orion", .anan7000):
                    break
                case ("orion", .redPitaya):
                    controls = controls.map { control in
                        var control = control
                        control["availability"] = ["enabled": false, "reason": redPitayaReason]
                        return control
                    }
                case ("saturn", .g2):
                    break
                default:
                    continue
                }
                section["controls"] = controls
                sections.append(section)
            }
            page["sections"] = sections
            pages.append(page)
        }
        root["pages"] = pages
        return String(decoding: try JSONSerialization.data(withJSONObject: root), as: UTF8.self)
    }

    /// Hardware with Swap audio channels as a Core before the radio codec
    /// lane sent it: no tooltip, closed with its reason.
    static func closingSwap(_ json: String) throws -> String {
        var root = try #require(JSONSerialization.jsonObject(with: Data(json.utf8)) as? [String: Any])
        var pages: [[String: Any]] = []
        for var page in root["pages"] as? [[String: Any]] ?? [] {
            var sections: [[String: Any]] = []
            for var section in page["sections"] as? [[String: Any]] ?? [] {
                section["controls"] = (section["controls"] as? [[String: Any]] ?? []).map { control in
                    guard control["id"] as? String == swapId else {
                        return control
                    }
                    var closed = control
                    closed["tooltip"] = ""
                    closed["availability"] = ["enabled": false, "reason": swapClosedReason]
                    return closed
                }
                sections.append(section)
            }
            page["sections"] = sections
            pages.append(page)
        }
        root["pages"] = pages
        return String(decoding: try JSONSerialization.data(withJSONObject: root), as: UTF8.self)
    }

    // MARK: The rig

    /// Connected at `peer` with transmit settings and TX Input's values as
    /// the Core holds them.
    static func connected(peer: Int, radio: Radio, swapClosed: Bool = false) async throws -> Pages.Rig {
        let rig = try await Pages.connected()
        let app = rig.app
        Pages.asV12Core(app, setupDescription: Int64(peer))
        Pages.withCapabilities(app, ["transmitSettingsVersion": .i64(15), "txPermitted": .bool(true),
                                     "radioHardwareVersion": .i64(peer >= 24 ? 13 : 12)])
        await rig.station.deliverSetup(try categories(peer: peer, radio: radio, swapClosed: swapClosed))
        let audioVersion = peer >= 24 ? 24 : 15
        #expect(await Pages.settle { app.setupPages.isCurrent && app.setupPages.categories["audio"]?.version == audioVersion })
        let values: [LinkMessage.PropertyEntry] = [
            .init(name: "micGainDb", value: .i64(-6)),
            .init(name: "lineIn", value: .bool(false)),
            .init(name: "micBoost", value: .bool(true)),
            .init(name: "lineInBoost", value: .f64(peer >= 24 ? -1.5 : -2)),
            .init(name: "micXlr", value: .bool(true)),
            .init(name: "micTipRing", value: .bool(true)),
            .init(name: "micBias", value: .bool(false)),
            .init(name: "micPttDisabled", value: .bool(false)),
        ]
        if app.mirror.object("transmit") == nil {
            await rig.station.deliver(.objectCreate(.init(key: "transmit", className: "TransmitModel", properties: values)))
        } else {
            await rig.station.deliver(.delta(.init(key: "transmit", properties: values)))
        }
        #expect(await Pages.settle { app.mirror.object("transmit")?["micPttDisabled"] != nil })
        // The radio's catalogue, which carries Mic Gain's range (link 7.4).
        let fixture = radio == .hl2 ? "catalog-hermes-lite-2" : "catalog-anan-g2"
        let json = try #require(ModesTabBindingTests.catalogueJSON(fixture))
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(json)),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        #expect(await Pages.settle { app.main.catalogFeed.catalog?.board.transmit?.micGainDb != nil })
        return rig
    }

    static func txInput(_ app: AppModel) throws -> SetupDescription.Page {
        try #require(app.setupPages.categories["audio"]?.pages.first { $0.id == txInputPage })
    }

    static func control(_ id: String, in page: SetupDescription.Page) throws -> SetupDescription.Control {
        try #require(page.sections.flatMap(\.controls).first { $0.id == id }, "\(id)")
    }

    static func propertyWrite(_ name: String, _ station: FakeStation) async throws -> LinkMessage.PropertyWrite {
        let sent = await station.waitForMessage { message in
            if case .propertyWrite(let write) = message {
                return write.key == "transmit" && write.properties.contains { $0.name == name }
            }
            return false
        }
        guard case .propertyWrite(let write)? = sent else { throw SetupRenderingTests.SetupTestError.notSent(name) }
        return write
    }

    /// The Core keeps `value` for the write, as it answers one.
    static func keep(_ write: LinkMessage.PropertyWrite, _ value: LinkMessage.PropertyValue,
                     station: FakeStation) async throws {
        let name = try #require(write.properties.first?.name)
        await station.deliver(.propertyResult(.init(key: write.key, writeId: write.writeId ?? 0, results: [
            .init(property: name, accepted: true, reason: "", value: .init(name: name, value: value)),
        ])))
        await station.deliver(.delta(.init(key: "transmit", properties: [.init(name: name, value: value)])))
    }

    // MARK: The ANAN-G2: the Saturn group

    @Test("V24 on an ANAN-G2: the Saturn group with Mic Tip-Ring second, every row live; its switch writes micTipRing")
    func g2SaturnGroup() async throws {
        let rig = try await Self.connected(peer: 24, radio: .g2)
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        let page = try Self.txInput(app)
        #expect(page.sections.map(\.title) == ["PC Mic", "Radio Mic (Saturn G2)"])
        let saturn = try #require(page.sections.last)
        #expect(saturn.controls.map(\.id) == ["audio.txInput.saturnMicXlr", Self.saturnTipRingId,
                                              "audio.txInput.saturnMicPttDisabled", "audio.txInput.saturnMicBias",
                                              "audio.txInput.saturnMicBoost"])
        let controls = app.setupControls
        for control in page.sections.flatMap(\.controls) {
            #expect(control.metadataIssue == nil, "\(control.id)")
            #expect(await Pages.settle { controls.state(of: control, in: "audio").editable }, "\(control.id)")
        }
        let tipRing = try Self.control(Self.saturnTipRingId, in: page)
        #expect(tipRing.label == "Mic Tip-Ring (Tip is Mic)" && tipRing.tooltip.isEmpty)
        #expect(DescribedControl.row(for: tipRing) == .switchRow)
        #expect(controls.state(of: tipRing, in: "audio").value == .bool(true))
        try await Typed.shootAll("trunk-dc238c-tx-input-g2", rig: rig, path: Self.txInputPath)

        let turning = Task { await controls.edit(tipRing, in: "audio", to: .bool(false)) }
        let write = try await Self.propertyWrite("micTipRing", rig.station)
        #expect(write.properties.first?.value == .bool(false))
        try await Self.keep(write, .bool(false), station: rig.station)
        #expect(await turning.value == .applied)
        #expect(await Pages.settle { controls.state(of: tipRing, in: "audio").value == .bool(false) })
        await rig.app.disconnect()
    }

    // MARK: The Hermes Lite 2: Line In Gain in 1.5 dB steps

    @Test("V24 on the Hermes Lite 2: its own group title and the add-on note; Line In Gain steps 1.5 dB and is typed on the decimal pad")
    func hl2LineInGain() async throws {
        let rig = try await Self.connected(peer: 24, radio: .hl2)
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        let page = try Self.txInput(app)
        #expect(page.sections.map(\.title) == ["PC Mic", "Radio Mic (Hermes Lite 2)"])
        let hermes = try #require(page.sections.last)
        #expect(hermes.controls.map(\.id) == ["audio.txInput.hermesLineIn", "audio.txInput.hermesMicBoost",
                                              Self.lineInGainId])
        #expect(hermes.controls.allSatisfy { $0.tooltip == Self.addOnNote })
        let gain = try Self.control(Self.lineInGainId, in: page)
        #expect(gain.label == "Line In Gain:" && gain.kind == .decimal && gain.unit == "dB")
        #expect(gain.range == .init(minimum: -34.5, maximum: 12, step: 1.5) && gain.decimals == 1)
        #expect(DescribedControl.row(for: gain) == .numberRow)
        let controls = app.setupControls
        #expect(await Pages.settle { controls.state(of: gain, in: "audio").value == .decimal(-1.5) })
        #expect(controls.state(of: gain, in: "audio").editable)
        try await Typed.shootAll("trunk-dc238c-tx-input-hl2", rig: rig, path: Self.txInputPath)

        // Enter value opens the decimal pad with the Core's range, unit and place.
        try await Typed.onScreen(rig, path: Self.txInputPath) { window in
            let row = try #require(Typed.element(gain.id, in: window))
            #expect(row.accessibilityTraits.contains(.adjustable))
            #expect(Typed.perform(SetupNumberRow.enterValueAction, on: row))
        }
        let pad = try #require(rig.router.pads.pad)
        #expect(pad.title == "Line In Gain:" && pad.unit == "dB" && pad.decimals == 1 && pad.takesDecimals)
        #expect(pad.numberRange == -34.5...12 && pad.signed)
        // The pad opens on -1.5; the minus key turns the typed 4.5 above zero.
        Typed.type("4.5", on: pad)
        pad.press(.minus)
        #expect(pad.enterLabel == "Set to +4.5 dB" && pad.canEnter)
        try await Typed.shootAll("trunk-dc238c-line-in-gain-pad", rig: rig, path: Self.txInputPath)
        let entering = Task { await pad.enter() }
        let write = try await Self.propertyWrite("lineInBoost", rig.station)
        #expect(write.properties.first?.value == .f64(4.5))
        try await Self.keep(write, .f64(4.5), station: rig.station)
        #expect(await entering.value)
        #expect(rig.router.pads.pad == nil)
        #expect(await Pages.settle { controls.state(of: gain, in: "audio").value == .decimal(4.5) })

        // Beyond the Core's range either side of zero, the pad turns it down.
        try await Typed.onScreen(rig, path: Self.txInputPath) { window in
            let row = try #require(Typed.element(gain.id, in: window))
            #expect(Typed.perform(SetupNumberRow.enterValueAction, on: row))
        }
        let again = try #require(rig.router.pads.pad)
        Typed.type("40", on: again)
        #expect(!again.canEnter && again.problem != nil)
        again.press(.minus)
        #expect(!again.canEnter && again.problem != nil)
        again.cancel()
        await rig.app.disconnect()
    }

    // MARK: The Red Pitaya: the Orion rows greyed

    @Test("V24 on the Red Pitaya: the Orion group's four rows greyed as whole rows with the Core's reason")
    func redPitayaOrionGreyed() async throws {
        let rig = try await Self.connected(peer: 24, radio: .redPitaya)
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        let page = try Self.txInput(app)
        #expect(page.sections.map(\.title) == ["PC Mic", "Radio Mic (Orion-MkII)"])
        let orion = try #require(page.sections.last)
        #expect(orion.controls.count == 4)
        let controls = app.setupControls
        for control in orion.controls {
            #expect(control.metadataIssue == nil, "\(control.id)")
            let state = controls.state(of: control, in: "audio")
            #expect(!state.editable && state.reason == Self.redPitayaReason, "\(control.id)")
        }
        try await Typed.shootAll("trunk-dc238c-tx-input-red-pitaya", rig: rig, path: Self.txInputPath)
        await rig.app.disconnect()
    }

    // MARK: A Core at 23

    @Test("A Core at 23: Line In Gain in whole decibels from -34 and no Saturn Mic Tip-Ring")
    func olderCoreKeepsVersionFifteen() async throws {
        let rig = try await Self.connected(peer: 23, radio: .hl2)
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let gain = try Self.control(Self.lineInGainId, in: try Self.txInput(rig.app))
        #expect(gain.range == .init(minimum: -34, maximum: 12, step: 1) && gain.decimals == nil)
        #expect(rig.app.setupControls.state(of: gain, in: "audio").editable)
        try await Pages.shoot("trunk-dc238c-tx-input-hl2-core-23", rig: rig, path: Self.txInputPath)
        await rig.station.deliverSetup(try Self.categories(peer: 23, radio: .g2))
        #expect(await Pages.settle {
            (try? Self.txInput(rig.app))?.sections.last?.title == "Radio Mic (Saturn G2)"
        })
        let saturn = try #require(try Self.txInput(rig.app).sections.last)
        #expect(!saturn.controls.contains { $0.id == Self.saturnTipRingId })
        #expect(saturn.controls.count == 4)
        await rig.app.disconnect()
    }

    // MARK: HL2 Swap audio channels

    @Test("HL2 Swap audio channels: a live switch with the Core's tooltip when sent open; greyed with the reason in the older closed shape")
    func hl2SwapAudioChannels() async throws {
        let rig = try await Self.connected(peer: 24, radio: .hl2)
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        #expect(await Pages.settle { app.setupPages.categories["hardware"]?.version == 23 })
        func swap() throws -> SetupDescription.Control {
            let page = try #require(app.setupPages.categories["hardware"]?.pages.first { $0.id == Pages.clockPage })
            return try Self.control(Self.swapId, in: page)
        }
        let open = try swap()
        #expect(open.availability == nil && open.metadataIssue == nil)
        #expect(open.tooltip == "Swap the audio channels sent to the HL2")
        #expect(DescribedControl.row(for: open) == .switchRow)
        let controls = app.setupControls
        #expect(controls.state(of: open, in: "hardware") == .init(value: .bool(false), editable: true, reason: nil))
        try await Self.shootSwap("trunk-dc238c-hl2-swap-open", rig: rig)
        try await Pages.shoot("trunk-dc238c-hl2-swap-open-large-type", rig: rig, path: Self.hl2IoPath, height: 2_400,
                              dynamicType: .accessibility2, atBottom: true)

        let key = "hardware/\(FakeStation.setupPanelsRadioMac)/hl2/swapAudioChannels"
        let turning = Task { await controls.edit(open, in: "hardware", to: .bool(true)) }
        let write = try await Pages.settingsWrite(key, rig.station)
        #expect(write.properties.first?.value == .utf8("True"))
        await rig.station.deliver(.settingsValue(.init(key: write.key, origin: write.origin, properties: write.properties)))
        #expect(await turning.value == .applied)
        #expect(await Pages.settle { controls.state(of: open, in: "hardware").value == .bool(true) })
        try await Pages.shoot("trunk-dc238c-hl2-swap-on-portrait-dark", rig: rig, path: Self.hl2IoPath, height: 1_500)

        // An older Core's closed row: greyed, with that Core's reason, never hidden.
        await rig.station.deliverSetup(try Self.categories(peer: 24, radio: .hl2, swapClosed: true))
        #expect(await Pages.settle { (try? swap())?.availability != nil })
        let closed = try swap()
        #expect(closed.availability?.enabled == false && closed.availability?.reason == Self.swapClosedReason)
        #expect(await Pages.settle {
            controls.state(of: closed, in: "hardware").reason == Self.swapClosedReason
                && !controls.state(of: closed, in: "hardware").editable
        })
        try await Self.shootSwap("trunk-dc238c-hl2-swap-closed", rig: rig)
        await rig.app.disconnect()
    }

    /// HL2 I/O down to Swap audio channels, its last row: upright in one
    /// tall picture and sideways in parts, light and dark.
    static func shootSwap(_ name: String, rig: Pages.Rig) async throws {
        for scheme in [ColorScheme.light, .dark] {
            let look = scheme == .dark ? "dark" : "light"
            try await Pages.shoot("\(name)-portrait-\(look)", rig: rig, path: hl2IoPath, height: 1_500,
                                  scheme: scheme)
            try await Pages.shoot("\(name)-landscape-\(look)", rig: rig, path: hl2IoPath, height: 402, width: 874,
                                  sideways: true, parts: 4, scheme: scheme)
        }
    }
}
