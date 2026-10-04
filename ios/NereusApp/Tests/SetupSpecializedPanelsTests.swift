// NereusSDR for iOS: Setup's closed panels against the fake Core: the notch table, the settings check, the antenna rows and the PA readings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing

/// R-IOS-18 (Task 58, step 1): the four closed controls are drawn by their
/// own panels, not greyed rows, and each reaches the fake Core with the
/// exact command the contract names; the app's hello asks for them. With
/// `NEREUS_MAIN_SHOTS` set to a directory, the panels are drawn there.
@Suite("Setup's closed panels", .serialized)
@MainActor
struct SetupSpecializedPanelsTests {
    typealias Rig = SetupDescribedPagesTests.Rig

    /// The app connected to a fake Core with the Core's own descriptions
    /// and the objects and verbs the panels use.
    static func connected(band2m: Bool = false, mirrorClock: any LinkClock = SystemLinkClock()) async throws -> Rig {
        let suite = "SetupSpecializedPanelsTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let app = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                           displaySettings: BandDisplaySettingsStore(defaults: defaults),
                           meterSettings: SMeterSettingsStore(defaults: defaults), mirrorClock: mirrorClock)
        let station = try FakeStation(fixture: "session-device-sign-in",
                                      additions: FakeStation.Additions([.bands, .notchAtSlice, .setupDescription,
                                                                        .setupPanels]).union(band2m ? .band2m : []),
                                      stationLabel: "KG4VCF/shack")
        let stations = PairedStationStore(item: InMemorySecretItem())
        try stations.save(PairedStation(identityKey: station.identity.publicKey, label: "KG4VCF/shack",
                                        endpoints: [station.endpoint]))
        let flow = ConnectionFlow(app: app, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()), stations: stations, kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        await station.deliverSetup(try SetupDescribedPagesTests.coreCategories())
        try await station.deliverSetupPanels()
        #expect(await settle { app.setupPages.isCurrent && app.setupFeed.description(for: "pa") != nil
            && app.mirror.object("alexAntennas") != nil && app.mirror.object("txState") != nil })
        return Rig(app: app, flow: flow, station: station, router: SetupRouter(), defaults: defaults, suite: suite)
    }

    static func settle(_ condition: () -> Bool) async -> Bool {
        await SetupDescribedPagesTests.settle(condition)
    }

    private func settle(_ condition: () -> Bool) async -> Bool {
        await Self.settle(condition)
    }

    static func control(_ app: AppModel, _ category: String, _ id: String) throws -> SetupDescription.Control {
        let description = try #require(app.setupFeed.description(for: category))
        return try #require(description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == id })
    }

    private func sent(_ station: FakeStation, _ verb: String) -> [LinkMessage.CommandInvoke] {
        station.messages.compactMap { message in
            if case .commandInvoke(let invoke) = message, invoke.verb == verb { return invoke }
            return nil
        }
    }

    // MARK: What the app asks for

    @Test("the app's hello asks for the Setup description at 12 and for the two panels' features; a V11 Core stays 11")
    func helloDeclaresThePanels() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let hello = rig.station.messages.compactMap { message -> LinkMessage.Hello? in
            if case .hello(let hello) = message { return hello }
            return nil
        }.first
        let features = hello?.features ?? [:]
        #expect(features == LinkFeatures.app)
        #expect(features["setupDescription"] == 24)
        #expect(features["settingsHygiene"] == 2 && features["radioAntennaRows"] == 1)
        #expect(features["band2m"] == 1)
        #expect(rig.app.mirror.capabilityVersion("setupDescriptionVersion") == 11)
        #expect(rig.app.session?.signsWithDeviceKey == true)
        // Each closed control has its panel; none is left as a greyed row.
        let panels = SetupSpecializedPanels.live(rig.app)
        let closed: [(SetupSpecialized, String, String)] = [
            (.notchTable, "dsp", "dsp.tnf.list"), (.settingsHygiene, "diagnostics", "diagnostics.settingsValidation.health"),
            (.antennaRows, "hardware", "hardware.antenna.txRows"), (.antennaRows, "hardware", "hardware.antenna.rxRows"),
            (.paTelemetry, "pa", "pa.values.paCurrent"), (.paTelemetry, "pa", "pa.values.dcVoltage"),
        ]
        for (kind, category, id) in closed {
            let control = try Self.control(rig.app, category, id)
            #expect(control.specialized == kind, "\(id)")
            #expect(panels.make(kind, control, category) != nil, "\(id)")
        }
        await rig.app.disconnect()
    }

    // MARK: The notch table

    @Test("the notch table edits one notch with the described command, and a changed list cancels an edit")
    func notchTable() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let controls = rig.app.setupControls
        let table = try Self.control(rig.app, "dsp", "dsp.tnf.list")
        #expect(await settle { controls.notchTable(table, in: "dsp").list?.rows.count == 2 })
        #expect(controls.notchTable(table, in: "dsp").reason == nil)

        let move = try controls.admitNotch(table, in: "dsp", row: 1).get()
        #expect(await controls.performNotch(move, edit: .move(centreHz: 7_075_000, widthHz: 150)) == .applied)
        let moved = try #require(sent(rig.station, "notch.move").first)
        #expect(moved.args == [.init(name: "centreHz", value: .f64(7_075_000)), .init(name: "id", value: .i64(1)),
                               .init(name: "widthHz", value: .f64(150))])
        #expect(await settle { controls.notchTable(table, in: "dsp").list?.row(1)?.centreHz == 7_075_000 })

        let active = try controls.admitNotch(table, in: "dsp", row: 2).get()
        #expect(await controls.performNotch(active, edit: .setActive(true)) == .applied)
        #expect(sent(rig.station, "notch.setActive").first?.args
                == [.init(name: "active", value: .bool(true)), .init(name: "id", value: .i64(2))])
        #expect(await settle { controls.notchTable(table, in: "dsp").list?.row(2)?.active == true })

        // Another window changes the list while this edit is open: it is cancelled, nothing sent.
        let stale = try controls.admitNotch(table, in: "dsp", row: 2).get()
        await rig.station.setSetupNotches([.init(id: 2, centreHz: 14_074_000, widthHz: 250, active: true)])
        #expect(await settle { stale.isRevoked })
        #expect(await controls.performNotch(stale, edit: .delete) == .notSent(SetupControlDispatcher.notchChangedReason))
        #expect(sent(rig.station, "notch.delete").isEmpty)
        await rig.station.setSetupNotches([.init(id: 1, centreHz: 7_074_000, widthHz: 100, active: true),
                                           .init(id: 2, centreHz: 14_074_000, widthHz: 250, active: false),
                                           .init(id: 4, centreHz: 10_136_000, widthHz: 50, active: true)])
        #expect(await settle { controls.notchTable(table, in: "dsp").list?.rows.count == 3 })
        try await SetupDescribedPagesTests.shoot("setup-panel-notches", rig: rig,
                                                 path: [.category("DSP"), .described(category: "dsp", page: "dsp.tnf")],
                                                 height: 1_500)
        // A list the Core sent that breaks the contract is not shown in part.
        await rig.station.deliver(.delta(.init(key: "notches", properties: [
            .init(name: "listJson", value: .utf8(#"[{"id":1,"centreHz":7074000,"widthHz":100,"active":true},{"id":1,"centreHz":7075000,"widthHz":100,"active":true}]"#)),
            .init(name: "revision", value: .i64(99)),
        ])))
        #expect(await settle { controls.notchTable(table, in: "dsp").list == nil })
        #expect(controls.notchTable(table, in: "dsp").listReason == SetupControlDispatcher.notchListDuplicateReason)
        try await SetupDescribedPagesTests.shoot("setup-panel-notches-refused", rig: rig,
                                                 path: [.category("DSP"), .described(category: "dsp", page: "dsp.tnf")])
        await rig.app.disconnect()
    }

    // MARK: The settings check

    @Test("Re-validate shows the Core's problems, Repair and Forget send this radio, and a refusal is shown in the Core's words")
    func settingsCheck() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let controls = rig.app.setupControls
        let health = try Self.control(rig.app, "diagnostics", "diagnostics.settingsValidation.health")
        var panel = controls.hygiene(health, in: "diagnostics")
        #expect(panel.validateReason == nil && panel.forgetReason == nil && panel.repairReason == nil)
        #expect(panel.repairLabel == "Repair Invalid Settings" && panel.resetLabel.isEmpty)
        #expect(panel.reportReason == SetupControlDispatcher.hygieneNotCheckedText)
        try await SetupDescribedPagesTests.shoot("setup-panel-settings-validation-unchecked", rig: rig,
                                                 path: [.category("Diagnostics"),
                                                        .described(category: "diagnostics", page: "diagnostics.settingsValidation")])

        rig.station.setSettingsIssues(#"[{"severity":"critical","key":"hardware/AA:BB:CC:DD:EE:01/sampleRate","summary":"Sample rate","detail":"The saved rate is more than this radio can run.","fixActionId":"clampSampleRate"},{"severity":"info","key":"hardware/AA:BB:CC:DD:EE:01/preamp","summary":"Preamp","detail":"The saved preamp step is not one this radio has.","fixActionId":""}]"#)
        let check = try controls.admitHygiene(health, in: "diagnostics", action: .validate).get()
        #expect(await controls.performHygiene(check) == .applied)
        #expect(sent(rig.station, "station.validateSettings").first?.args
                == [.init(name: "mac", value: .utf8(FakeStation.setupPanelsRadioMac))])
        panel = controls.hygiene(health, in: "diagnostics")
        #expect(panel.report?.issues.map(\.severity) == [.critical, .info])
        try await SetupDescribedPagesTests.shoot("setup-panel-settings-validation", rig: rig,
                                                 path: [.category("Diagnostics"),
                                                        .described(category: "diagnostics", page: "diagnostics.settingsValidation")])

        // Repair, after its question, sends only this radio's address.
        let repair = try controls.admitHygiene(health, in: "diagnostics", action: .repair).get()
        #expect(await controls.performHygiene(repair) == .applied)
        #expect(sent(rig.station, "station.repairSettings").first?.args
                == [.init(name: "mac", value: .utf8(FakeStation.setupPanelsRadioMac))])

        // Forget, after its question, sends only this radio's address.
        let forget = try controls.admitHygiene(health, in: "diagnostics", action: .forget).get()
        #expect(await controls.performHygiene(forget) == .applied)
        #expect(sent(rig.station, "station.forgetSettings").first?.args
                == [.init(name: "mac", value: .utf8(FakeStation.setupPanelsRadioMac))])

        // A refusal is the Core's words, never an empty list.
        rig.station.refuseNext("station.validateSettings", reason: "The Core could not read this radio's settings.")
        let refused = try controls.admitHygiene(health, in: "diagnostics", action: .validate).get()
        #expect(await controls.performHygiene(refused) == .refused("The Core could not read this radio's settings."))
        panel = controls.hygiene(health, in: "diagnostics")
        #expect(panel.report == nil && panel.reportReason == "The Core could not read this radio's settings.")

        // On the air, Forget waits and says why.
        await rig.station.setSetupKeyed(true)
        #expect(await settle { controls.hygiene(health, in: "diagnostics").forgetReason == SetupControlDispatcher.onAirReason })
        #expect(controls.hygiene(health, in: "diagnostics").repairReason == SetupControlDispatcher.onAirReason)
        try await SetupDescribedPagesTests.shoot("setup-panel-settings-validation-on-air", rig: rig,
                                                 path: [.category("Diagnostics"),
                                                        .described(category: "diagnostics", page: "diagnostics.settingsValidation")])
        #expect(!rig.station.messages.contains { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb.lowercased().contains("reset") }
            return false
        })
        await rig.app.disconnect()
    }

    // MARK: The antenna rows

    @Test("an antenna cell chooses that band's antenna on this radio; transmit rows lock on the air, receive rows do not")
    func antennaRows() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let controls = rig.app.setupControls
        let tx = try Self.control(rig.app, "hardware", "hardware.antenna.txRows")
        let rx = try Self.control(rig.app, "hardware", "hardware.antenna.rxRows")
        #expect(await settle { controls.antennaTable(tx, in: "hardware").grid != nil })
        #expect(controls.antennaTable(tx, in: "hardware").grid?.blocked == [false, false, true])

        let choose = try controls.admitAntenna(tx, in: "hardware").get()
        #expect(await controls.performAntenna(choose, band: 3, column: "tx2") == .applied)
        #expect(sent(rig.station, "setAlexTxAntennaForRadio").first?.args
                == [.init(name: "mac", value: .utf8(FakeStation.setupPanelsRadioMac)), .init(name: "band", value: .i64(3)),
                    .init(name: "antenna", value: .i64(2))])
        #expect(await settle { controls.antennaTable(tx, in: "hardware").grid?.selected[3] == [false, true, false] })
        let receive = try controls.admitAntenna(rx, in: "hardware").get()
        #expect(await controls.performAntenna(receive, band: 13, column: "rxOnly2") == .applied)
        #expect(sent(rig.station, "setAlexRxAntennaForRadio").first?.args.last == .init(name: "rxOnly", value: .bool(true)))
        try await SetupDescribedPagesTests.shoot("setup-panel-antennas", rig: rig,
                                                 path: [.category("Hardware"),
                                                        .described(category: "hardware", page: "hardware.antennaAlex")],
                                                 height: 2_700)

        await rig.station.setSetupKeyed(true)
        #expect(await settle { controls.antennaTable(tx, in: "hardware").reason == SetupControlDispatcher.onAirReason })
        #expect(controls.antennaTable(rx, in: "hardware").reason == nil)
        try await SetupDescribedPagesTests.shoot("setup-panel-antennas-on-air", rig: rig,
                                                 path: [.category("Hardware"),
                                                        .described(category: "hardware", page: "hardware.antennaAlex")],
                                                 height: 2_700)
        await rig.app.disconnect()
    }

    @Test("on a Core with 2 m, 2 m is the tables' last row and a tap on it names band 27")
    func antennaRowsWithTwoMetres() async throws {
        let rig = try await Self.connected(band2m: true)
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let controls = rig.app.setupControls
        let tx = try Self.control(rig.app, "hardware", "hardware.antenna.txRows")
        let rx = try Self.control(rig.app, "hardware", "hardware.antenna.rxRows")
        #expect(await settle { controls.antennaTable(tx, in: "hardware").grid != nil })
        if case .antennaRows(let rows)? = tx.binding {
            #expect(rows.rows.count == 15 && rows.rows.last?.band == 27 && rows.rows.last?.label == "2m")
        } else { Issue.record("Antenna rows were not typed") }
        let choose = try controls.admitAntenna(tx, in: "hardware").get()
        #expect(await controls.performAntenna(choose, band: 27, column: "tx2") == .applied)
        #expect(sent(rig.station, "setAlexTxAntennaForRadio").first?.args
                == [.init(name: "mac", value: .utf8(FakeStation.setupPanelsRadioMac)), .init(name: "band", value: .i64(27)),
                    .init(name: "antenna", value: .i64(2))])
        #expect(await settle { controls.antennaTable(tx, in: "hardware").grid?.selected[14] == [false, true, false] })
        // XVTR's row, the fourteenth, is untouched.
        #expect(controls.antennaTable(tx, in: "hardware").grid?.selected[13] == [true, false, false])
        let receive = try controls.admitAntenna(rx, in: "hardware").get()
        #expect(await controls.performAntenna(receive, band: 27, column: "rxOnly1") == .applied)
        #expect(await settle { controls.antennaTable(rx, in: "hardware").grid?.selected[14]
            == [true, false, false, true, false, false] })
        try await SetupDescribedPagesTests.shoot("setup-panel-antennas-2m", rig: rig,
                                                 path: [.category("Hardware"),
                                                        .described(category: "hardware", page: "hardware.antennaAlex")],
                                                 height: 2_730)
        await rig.app.disconnect()
    }

    // MARK: The PA readings

    @Test("the PA readings show the Core's fresh values, a measured zero as zero, and a missing one as unavailable")
    func paReadings() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let controls = rig.app.setupControls
        let amps = try Self.control(rig.app, "pa", "pa.values.paCurrent")
        let volts = try Self.control(rig.app, "pa", "pa.values.dcVoltage")
        func now() -> Int64 { rig.app.mirrorClock.nowMilliseconds }
        await rig.station.deliverPaTelemetry(sequence: 1, paCurrentAmps: 0, supplyVolts: nil)
        #expect(await settle { controls.telemetryReading(amps, in: "pa", nowMilliseconds: now()).value == 0 })
        #expect(controls.telemetryReading(volts, in: "pa", nowMilliseconds: now()).reason
                == SetupControlDispatcher.telemetryAbsentReason)
        #expect(PaTelemetryPanel.text(controls.telemetryReading(amps, in: "pa", nowMilliseconds: now()), control: amps)
                == "0.00 A")
        await rig.station.deliverPaTelemetry(sequence: 2, paCurrentAmps: 1.25, supplyVolts: 13.8)
        #expect(await settle { controls.telemetryReading(volts, in: "pa", nowMilliseconds: now()).value == 13.8 })
        #expect(PaTelemetryPanel.text(controls.telemetryReading(volts, in: "pa", nowMilliseconds: now()), control: volts)
                == "13.8 V")
        #expect(PaTelemetryPanel.age(0) == "Measured just now" && PaTelemetryPanel.age(2_400) == "Measured 2 s ago")
        try await SetupDescribedPagesTests.shoot("setup-panel-pa-values", rig: rig,
                                                 path: [.category("PA"), .described(category: "pa", page: "pa.values")],
                                                 height: 1_300)
        // Away from the Core the readings are unavailable, never the last value.
        await rig.app.disconnect()
        #expect(await settle { controls.telemetryReading(volts, in: "pa", nowMilliseconds: now()).value == nil })
    }

    @Test("the PA temperature reads in this phone's own unit, chosen on a visible C/F choice beside it; Celsius until chosen")
    func paTemperatureUnit() async throws {
        UserDefaults.standard.removeObject(forKey: PhoneSetupKeys.paTempUnitDefault)
        defer { UserDefaults.standard.removeObject(forKey: PhoneSetupKeys.paTempUnitDefault) }
        // Unit conversion is independent of elapsed telemetry freshness.
        // One clock stamps the receipt and reads its age across UI layout.
        let clock = TestLinkClock()
        let rig = try await Self.connected(mirrorClock: clock)
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let controls = rig.app.setupControls
        controls.telemetryNow = { clock.nowMilliseconds }
        // The PA Temperature row arrived at description 13: a Core at 22 sends it.
        SetupDescribedPagesTests.asV12Core(rig.app, setupDescription: 22)
        await rig.station.deliverSetup(try SetupDescribedPagesTests.coreCategories(peer: 22))
        #expect(await settle { rig.app.setupPages.isCurrent && rig.app.setupPages.categories["pa"]?.version == 20 })
        let temperature = try Self.control(rig.app, "pa", "pa.values.paTemperature")
        await rig.station.deliverPaTelemetry(sequence: 1, paCurrentAmps: 1.25, supplyVolts: 13.8,
                                             paTemperatureCelsius: 25)
        #expect(await settle { controls.state(of: temperature, in: "pa").value == .text("25.0 \u{00B0}C") })
        #expect(controls.temperatureInFahrenheit(temperature) == false)
        let path: [SetupTree.Route] = [.category("PA"), .described(category: "pa", page: "pa.values")]
        try await SetupTypedEntryTests.onScreen(rig, path: path) { window in
            let unit = try #require(SetupTypedEntryTests.element("pa.values.paTemperature.unit", in: window))
            #expect(!unit.accessibilityTraits.contains(.notEnabled))
        }

        #expect(controls.setTemperatureInFahrenheit(true, for: temperature))
        #expect(await settle { controls.state(of: temperature, in: "pa").value == .text("77.0 \u{00B0}F") })
        #expect(UserDefaults.standard.string(forKey: PhoneSetupKeys.paTempUnitDefault) == "F")
        for scheme in [ColorScheme.light, .dark] {
            try await SetupDescribedPagesTests.shoot("fix-setup-pa-temperature-unit-f-portrait-\(scheme == .dark ? "dark" : "light")",
                                                     rig: rig, path: path, height: 1_300, scheme: scheme)
        }
        // Back to Celsius; nothing went to the Core.
        #expect(controls.setTemperatureInFahrenheit(false, for: temperature))
        #expect(await settle { controls.state(of: temperature, in: "pa").value == .text("25.0 \u{00B0}C") })
        await rig.app.disconnect()
    }
}
