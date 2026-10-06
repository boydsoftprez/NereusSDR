// NereusSDR for iOS: the accessory pages against a fake Core: the Power Genius, the Tuner Genius, the RF2K-S and the interlock
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-19, D18, spec section 5.4 items 1 to 5: the real app against a
/// fake Core that owns a Power Genius XL, a Tuner Genius XL and an RF2K-S
/// (``FakeStation/Additions/accessories``) and reaches no device. Each
/// control sends its verb and the page follows the Core's report; a refused
/// verb shows the Core's reason; a switch that cannot run is greyed with
/// its reason; and with the amp in STANDBY and the interlock on Block, PTT
/// is refused with the Core's words and "Operate amp", which operates it.
/// With `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the pages are written there for
/// comparing with `11-amps-and-tuner.jpg`.
@Suite("Accessory pages", .serialized)
@MainActor
struct AccessoryPagesTests {
    /// This test's own lock, background time and idle timer.
    private let platform = TestPlatform()
    // MARK: Bounded TX readouts

    private func readoutModel(_ store: MirrorStore) -> AccessoriesModel {
        AccessoriesModel(mirror: store, commands: CommandClient(send: { _ in Issue.record("Readouts must send no command") }),
                         slices: BandSlicesModel(store: store, commands: nil), catalogFeed: CatalogFeed(store: store))
    }

    private func readoutObject(_ store: MirrorStore, tuner: Bool = false,
                               fields: [LinkMessage.PropertyEntry] = []) {
        store.apply(.objectCreate(.init(key: tuner ? "tuner" : "amplifier",
                                       className: tuner ? "TunerModel" : "AmplifierModel", properties: [
            .init(name: "connectionPhase", value: .enumeration(6)),
            .init(name: tuner ? "isPresent" : "present", value: .bool(true)),
        ] + fields)))
    }

    @Test("TX accessory readings retain distinct wire watts, ratios, temperature and relay values with units")
    func txAccessoryReadoutUnits() {
        let store = MirrorStore(send: { _ in Issue.record("Readouts must send no message") })
        let model = readoutModel(store)
        store.apply(.snapshotComplete)
        readoutObject(store, fields: [
            .init(name: "forwardPowerW", value: .f64(1234)), .init(name: "swr", value: .f64(1.23)),
            .init(name: "temperatureC", value: .f64(48.5)),
        ])
        readoutObject(store, tuner: true, fields: [
            .init(name: "fwdPower", value: .f64(87)), .init(name: "swr", value: .f64(1.45)),
            .init(name: "relayC1", value: .i64(12)), .init(name: "relayL", value: .i64(34)),
            .init(name: "relayC2", value: .i64(56)),
        ])
        model.refresh()
        #expect(model.powerGeniusReadings.forwardW == 1234)
        #expect(model.powerGeniusReadings.swr == 1.23)
        #expect(model.powerGeniusReadings.temperatureC == 48.5)
        #expect(model.tunerGeniusReadings.forwardW == 87)
        #expect(model.tunerGeniusReadings.swr == 1.45)
        #expect(model.tunerGeniusReadings.relays == [12, 34, 56])
        #expect(TxAccessoryReadoutText.power(model.powerGeniusReadings.forwardW) == "1234 W")
        #expect(TxAccessoryReadoutText.swr(model.tunerGeniusReadings.swr) == "1.45:1")
        #expect(TxAccessoryReadoutText.temperature(model.powerGeniusReadings.temperatureC) == "48.5 °C")
        #expect(TxAccessoryReadoutText.relay(model.tunerGeniusReadings.relays[1]) == "34")
    }

    @Test("TX accessory missing and invalid fields stay unavailable while genuine idle zero and one remain valid")
    func txAccessoryReadoutValidation() {
        let store = MirrorStore(send: { _ in Issue.record("Readouts must send no message") })
        let model = readoutModel(store)
        store.apply(.snapshotComplete)
        model.refresh()
        #expect(model.powerGeniusReadings == .init())
        #expect(model.tunerGeniusReadings == .init())
        readoutObject(store)
        readoutObject(store, tuner: true)
        model.refresh()
        #expect(model.powerGeniusReadings == .init())
        #expect(model.tunerGeniusReadings == .init())
        #expect(TxAccessoryReadoutText.power(nil) == "--")
        #expect(TxAccessoryReadoutText.swr(nil) == "--")
        #expect(TxAccessoryReadoutText.temperature(nil) == "--")
        #expect(TxAccessoryReadoutText.relay(nil) == "--")
        let invalidValues: [LinkMessage.PropertyValue] = [.utf8("12"), .bool(true), .f64(.nan), .f64(.infinity), .enumeration(12)]
        for invalid in invalidValues {
            readoutObject(store, fields: [
                .init(name: "forwardPowerW", value: invalid), .init(name: "swr", value: invalid),
                .init(name: "temperatureC", value: invalid),
            ])
            readoutObject(store, tuner: true, fields: [
                .init(name: "fwdPower", value: invalid), .init(name: "swr", value: invalid),
                .init(name: "relayC1", value: invalid), .init(name: "relayL", value: .f64(1.5)),
                .init(name: "relayC2", value: .i64(256)),
            ])
            model.refresh()
            #expect(model.powerGeniusReadings == .init())
            #expect(model.tunerGeniusReadings == .init())
        }
        // Celsius may be below zero: Core PgxlStatusGauges.cpp:51-52 imposes no minimum.
        // Preserve the measured number; only the canonical gauge's fill clamps at its scale minimum.
        readoutObject(store, fields: [.init(name: "forwardPowerW", value: .f64(-1)),
                                    .init(name: "swr", value: .f64(-1)), .init(name: "temperatureC", value: .f64(-5))])
        model.refresh()
        #expect(model.powerGeniusReadings.forwardW == nil)
        #expect(model.powerGeniusReadings.swr == nil)
        #expect(model.powerGeniusReadings.temperatureC == -5)
        #expect(model.powerGenius?.temperatureC == -5)
        #expect(TxAccessoryReadoutText.temperature(model.powerGeniusReadings.temperatureC) == "-5.0 °C")
        let coldMeter = LinearGauge(scale: .ampTemperature, value: model.powerGeniusReadings.temperatureC)
        #expect(coldMeter.value == -5)
        #expect(coldMeter.filledFraction == 0)
        #expect(LinearGauge(scale: .ampTemperature, value: nil).filledFraction == nil)
        #expect(LinearGauge(scale: .ampTemperature, value: 0).filledFraction == 0)
        #expect(LinearGauge(scale: .ampTemperature, value: 55).filledFraction == 0.55)
        readoutObject(store, tuner: true, fields: [.init(name: "fwdPower", value: .f64(-1)),
            .init(name: "swr", value: .f64(-1)), .init(name: "relayC1", value: .i64(-1))])
        model.refresh()
        #expect(model.tunerGeniusReadings == .init())
        readoutObject(store, fields: [.init(name: "forwardPowerW", value: .f64(0)),
                                    .init(name: "swr", value: .f64(1)), .init(name: "temperatureC", value: .f64(0))])
        readoutObject(store, tuner: true, fields: [.init(name: "fwdPower", value: .f64(0)),
            .init(name: "swr", value: .f64(0.99)), .init(name: "relayC1", value: .i64(0)),
            .init(name: "relayL", value: .i64(255)), .init(name: "relayC2", value: .i64(0))])
        model.refresh()
        #expect(model.powerGeniusReadings.forwardW == 0)
        #expect(model.powerGeniusReadings.swr == 1)
        #expect(model.powerGeniusReadings.temperatureC == 0)
        #expect(model.tunerGeniusReadings.forwardW == 0)
        #expect(model.tunerGeniusReadings.swr == nil)
        #expect(model.tunerGeniusReadings.relays == [0, 255, 0])
        #expect(TxAccessoryReadoutText.power(0) == "0 W")
        #expect(TxAccessoryReadoutText.swr(1) == "1.00:1")
    }

    @Test("TX accessory meters retain desktop scales and distinguish an unavailable bar from genuine zero")
    func txAccessoryMeterScalesAndAvailability() {
        let emptyPower = LinearGauge(scale: .ampPower, value: nil)
        let zeroPower = LinearGauge(scale: .ampPower, value: 0)
        let emptySwr = LinearGauge(scale: .ampSwr, value: nil)
        let idleSwr = LinearGauge(scale: .ampSwr, value: 1)
        #expect(emptyPower.value == nil)
        #expect(zeroPower.value == 0)
        #expect(emptySwr.value == nil)
        #expect(idleSwr.value == 1)
        #expect(AccessoryChrome.RelayBar(label: "C1", value: nil).value == nil)
        #expect(AccessoryChrome.RelayBar(label: "C1", value: 0).value == 0)
        #expect(TxAccessoryMeterScale.tunerPower(maxWatts: 100, amplifying: false).max == 200)
        #expect(TxAccessoryMeterScale.tunerPower(maxWatts: 100, amplifying: false).redFrom == 125)
        #expect(TxAccessoryMeterScale.tunerPower(maxWatts: 500, amplifying: false).max == 600)
        #expect(TxAccessoryMeterScale.tunerPower(maxWatts: 500, amplifying: false).yellowFrom == 500)
        #expect(TxAccessoryMeterScale.tunerPower(maxWatts: 500, amplifying: true).max == 2000)
        #expect(TxAccessoryMeterScale.tunerPower(maxWatts: 500, amplifying: true).yellowFrom == 1500)
        #expect(TxAccessoryMeterScale.tunerPower(maxWatts: 500, amplifying: true).redFrom == 1500)
        #expect(TxAccessoryMeterScale.tunerSwr.min == 1)
        #expect(TxAccessoryMeterScale.tunerSwr.max == 3)
        #expect(TxAccessoryMeterScale.tunerSwr.yellowFrom == 2.5)
        #expect(TxAccessoryMeterScale.tunerSwr.redFrom == 2.5)
        #expect(TxAccessoryMeterScale.tunerPower(maxWatts: 100, amplifying: false).ticks.isEmpty)
        #expect(TxAccessoryMeterScale.tunerSwr.ticks.isEmpty)
    }

    @Test("TX accessory readings retire on device or Core disconnect, reconnect snapshot, disable and removal")
    func txAccessoryReadoutLifecycle() async {
        let store = MirrorStore(send: { _ in Issue.record("Readouts must send no message") })
        let model = readoutModel(store)
        store.handle(.stateChanged(.receivingSnapshot))
        let ampFields: [LinkMessage.PropertyEntry] = [.init(name: "forwardPowerW", value: .f64(500))]
        let tunerFields: [LinkMessage.PropertyEntry] = [.init(name: "fwdPower", value: .f64(50))]
        readoutObject(store, fields: ampFields)
        readoutObject(store, tuner: true, fields: tunerFields)
        model.refresh()
        #expect(model.powerGeniusReadings.forwardW == nil)
        store.apply(.snapshotComplete)
        #expect(await settle { model.powerGeniusReadings.forwardW == 500 && model.tunerGeniusReadings.forwardW == 50 })
        for (key, present) in [("amplifier", "present"), ("tuner", "isPresent")] {
            for phase in [Int64(5), 0, 6] {
                store.apply(.delta(.init(key: key, properties: [
                    .init(name: present, value: .bool(false)),
                    .init(name: "connectionPhase", value: .enumeration(phase)),
                    .init(name: "configuredHost", value: .utf8("configured.invalid")),
                ])))
                model.refresh()
                #expect(key == "amplifier" ? model.powerGeniusReadings.forwardW == nil : model.tunerGeniusReadings.forwardW == nil)
                #expect(model.standing(key == "amplifier" ? .powerGenius : .tunerGenius) == (phase == 0 ? .switchedOff : .setUp))
            }
        }
        readoutObject(store, fields: ampFields)
        readoutObject(store, tuner: true, fields: tunerFields)
        model.refresh()
        store.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(await settle { model.powerGeniusReadings.forwardW == nil && model.tunerGeniusReadings.forwardW == nil })
        store.apply(.authResult(.init(accepted: true, reason: "")))
        store.handle(.stateChanged(.receivingSnapshot))
        readoutObject(store, fields: [.init(name: "forwardPowerW", value: .f64(600))])
        model.refresh()
        #expect(model.powerGeniusReadings.forwardW == nil)
        store.apply(.snapshotComplete)
        #expect(await settle { model.powerGeniusReadings.forwardW == 600 && model.tunerGenius == nil })
        store.apply(.objectDestroy(.init(key: "amplifier", className: "AmplifierModel")))
        #expect(await settle { model.powerGeniusReadings.forwardW == nil && model.powerGenius == nil })
    }

    // MARK: The Radio tab

    @Test("the Radio tab always lists the three accessories, with where each is or why it is not set up")
    func radioTabRows() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        // Before the Core has said anything of them: listed, not hidden.
        #expect(accessories.listed == [.powerGenius, .tunerGenius, .rfKit])
        #expect(AccessoryStatusLine.status(.powerGenius, in: accessories) == "Not reported by this Core")
        await station.deliverAccessories()
        #expect(await settle { accessories.standing(.rfKit) == .setUp })
        #expect(accessories.listed == [.powerGenius, .tunerGenius, .rfKit])
        #expect(AccessoryStatusLine.status(.powerGenius, in: accessories)
                == "Operating \u{00B7} on the station's network")
        #expect(AccessoryStatusLine.status(.tunerGenius, in: accessories)
                == "Operating \u{00B7} on the station's network")
        #expect(AccessoryStatusLine.status(.rfKit, in: accessories) == "Standby \u{00B7} through the Core")
        // The station switches the RF-Kit off: its row stays and says so.
        await station.deliver(FakeStation.accessoryDelta("rfkit", "RfKitModel", [
            ("connectionPhase", .enumeration(0)), ("present", .bool(false)),
        ]))
        #expect(await settle { accessories.standing(.rfKit) == .switchedOff })
        #expect(accessories.listed == [.powerGenius, .tunerGenius, .rfKit])
        #expect(AccessoryStatusLine.status(.rfKit, in: accessories) == "Switched off at the Core")
        // A Tuner Genius switched on with no address and never seen: not set up.
        await station.deliver(FakeStation.accessoryDelta("tuner", "TunerModel", [
            ("connectionPhase", .enumeration(1)), ("isPresent", .bool(false)), ("configuredHost", .utf8("")),
        ]))
        #expect(await settle { accessories.standing(.tunerGenius) == .notSetUp })
        #expect(AccessoryStatusLine.status(.tunerGenius, in: accessories) == "Not set up")
        await model.disconnect()
    }

    @Test("a switched-off accessory's page switches it on at the Core, and one not set up leads to its setup")
    func switchedOffAndNotSetUp() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        var scene = FakeStation.AccessoryScene.board
        scene.rfKitEnabled = false
        scene.fourO3AEnabled = false
        await station.deliverAccessories(scene)
        await station.deliver(FakeStation.accessoryDelta("rfkit", "RfKitModel", [
            ("connectionPhase", .enumeration(0)), ("present", .bool(false)),
        ]))
        await station.deliver(FakeStation.accessoryDelta("amplifier", "AmplifierModel", [
            ("connectionPhase", .enumeration(0)), ("present", .bool(false)),
        ]))
        #expect(await settle { accessories.standing(.rfKit) == .switchedOff
                && accessories.standing(.powerGenius) == .switchedOff })
        #expect(AccessorySetupCard.headline(.rfKit, .switchedOff)
                == "The Core has the RF-Kit amplifier switched off. Switch it on here, then set it up.")
        #expect(AccessorySetupCard.headline(.tunerGenius, .notSetUp)
                == "The Core has no address for the Tuner Genius. Set it up to connect.")
        let flow = Self.flow(model, station)
        try await shoot("accessories-radio-tab-switched-off", model: model) {
            RadioView(app: model, main: model.main, flow: flow)
        }
        try await shoot("accessories-power-genius-switched-off", model: model) {
            RadioView(app: model, main: model.main, flow: flow, accessoryRoute: [.page(.powerGenius)])
        }
        // Its switch works from here: the RF-Kit's, and the 4O3A switch.
        #expect(accessories.stationSwitchReason(.rfKit) == nil)
        accessories.setStationSwitch(.rfKit, true)
        #expect(await sent(station, "setRfKitEnabled") == [LinkMessage.PropertyEntry(name: "enabled", value: .bool(true))])
        #expect(await settle { accessories.rfKitEnabled })
        accessories.setStationSwitch(.powerGenius, true)
        #expect(await sent(station, "setFourO3AEnabled")
                == [LinkMessage.PropertyEntry(name: "enabled", value: .bool(true))])
        #expect(await settle { accessories.fourO3AEnabled })
        await model.disconnect()

        // On a Core without the 4O3A verb the switch is shown disabled, with its reason.
        let (older, olderStation) = try await connected(additions: [.remoteTx])
        await olderStation.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "amplifier", className: "AmplifierModel",
                                                                         properties: [
            .init(name: "connectionPhase", value: .enumeration(0)), .init(name: "present", value: .bool(false)),
        ])))
        let olderAccessories = older.main.accessories
        #expect(await settle { olderAccessories.standing(.powerGenius) == .switchedOff })
        #expect(olderAccessories.stationSwitchReason(.powerGenius) == AccessoriesModel.olderCoreSettingText)
        await older.disconnect()
    }

    // MARK: The Power Genius XL

    @Test("the Power Genius's OPERATE sends setPgxlOperate and the page follows the amp's report")
    func powerGeniusOperate() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.powerGenius?.operate == true })
        #expect(accessories.switchReason(.powerGenius) == nil)
        accessories.setAmpOperate(false)
        let sent = await station.waitForMessage(within: .seconds(30)) { Self.invoke($0)?.verb == "setPgxlOperate" }
        #expect(sent.flatMap(Self.invoke)?.args == [LinkMessage.PropertyEntry(name: "on", value: .bool(false))])
        #expect(await settle { accessories.powerGenius?.operate == false })
        #expect(accessories.powerGenius?.state == .standby)
        #expect(AccessoryStatusLine.status(.powerGenius, in: accessories) == "Standby \u{00B7} on the station's network")
        #expect(accessories.notes[.powerGenius] == nil)
        await model.disconnect()
    }

    @Test("the Power Genius's band, pairing and TX interlock read as the board writes them")
    func powerGeniusWords() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .accessories, .bands])
        let accessories = model.main.accessories
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(3)),
            .init(ordinal: 12, name: "txSlice", value: .bool(true)),
        ])))
        await station.deliverAccessories()
        #expect(await settle { accessories.records != nil && accessories.bandLabel == "40m" })
        let amp = try #require(accessories.powerGenius)
        #expect(AccessoryStatusLine.ampBand(amp, bandLabel: accessories.bandLabel) == "40m, from the radio")
        #expect(PowerGeniusPage.pairedWith(amp, coreName: "KG4VCF/shack") == "KG4VCF/shack")
        let interlock = try #require(accessories.records?.interlock)
        #expect(AccessoryStatusLine.interlockTitle(interlock) == "TX interlock: Block")
        #expect(AccessoryStatusLine.interlockSummary(interlock)
                == "Transmit is refused while the amp is in STANDBY or FAULT. SWR gate at 3.0, "
                + "with a 3 second grace after OPERATE.")
        var warn = interlock
        warn.mode = 1
        warn.swrGateEnabled = false
        #expect(AccessoryStatusLine.interlockSummary(warn)
                == "Transmit goes ahead with a warning while the amp is in STANDBY or FAULT. No SWR gate.")
        warn.mode = 0
        #expect(AccessoryStatusLine.interlockSummary(warn) == "The interlock never holds back transmit.")
        await model.disconnect()
    }

    @Test("the fault history shows the Core's records, and Clear all sends clearAccessoryFaults")
    func faultHistory() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        var scene = FakeStation.AccessoryScene.board
        let now = Date()
        scene.faultTimes = [now.addingTimeInterval(-3 * 86_400), now.addingTimeInterval(-9 * 86_400)]
        await station.deliverAccessories(scene)
        #expect(await settle { accessories.records?.faults[.powerGenius]?.count == 2 })
        let faults = try #require(accessories.records?.faults[.powerGenius])
        #expect(faults.first?.text == "The Power Genius reported a fault. Likely cause: high SWR.")
        #expect(AccessoryFaultsPage.summary(faults, now: now) == "2 entries \u{00B7} last 3 days ago")
        #expect(AccessoryFaultsPage.summary([], now: now) == "No faults recorded")
        accessories.clearFaults(.powerGenius)
        let sent = await station.waitForMessage(within: .seconds(30)) {
            Self.invoke($0)?.verb == "clearAccessoryFaults"
        }
        #expect(sent.flatMap(Self.invoke)?.args == [LinkMessage.PropertyEntry(name: "device", value: .utf8("pgxl"))])
        #expect(await settle { accessories.records?.faults[.powerGenius]?.isEmpty == true })
        await model.disconnect()
    }

    // MARK: The Tuner Genius XL

    @Test("the Tuner Genius: OPERATE and the antennas send their verbs, TUNE waits with its reason")
    func tunerGenius() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.tunerGenius?.antenna == 1 && accessories.records != nil })
        let records = try #require(accessories.records)
        #expect(records.tunerLabels == ["Beam", "Vertical", "Dipole"])
        #expect(records.tuneMemory.count == 12)
        #expect(TuneMemoryPage.summary(records.tuneMemory) == "12 stored tunes, by antenna and band")
        let tuner = try #require(accessories.tunerGenius)
        #expect(TunerGeniusPage.recalledLine(tuner, memory: records.tuneMemory)
                == "Recalled from tune memory for ANT 1 on 40m.")
        #expect(TunerGeniusPage.buttonLabel(tuner) == "OPERATE")
        // TUNE keys the tuner's tune carrier through the PTT (tx.tunerTune
        // at remoteTxVersion 2); this fake's Core is at 1, so it is greyed
        // with the reason and sends nothing.
        #expect(model.main.transmit.tunerTuneReason == TransmitModel.tunerTuneOlderCoreText)
        model.main.transmit.toggleTunerTune()
        await model.main.transmit.lastTuneOperationForTesting?.value

        accessories.setTunerAntenna(2)
        let antenna = await station.waitForMessage(within: .seconds(30)) { Self.invoke($0)?.verb == "setTgxlAntenna" }
        #expect(antenna.flatMap(Self.invoke)?.args == [LinkMessage.PropertyEntry(name: "port", value: .i64(2))])
        #expect(await settle { accessories.tunerGenius?.antenna == 2 })
        let moved = try #require(accessories.tunerGenius)
        #expect(TunerGeniusPage.recalledLine(moved, memory: records.tuneMemory) == nil)

        accessories.setTunerOperate(false)
        let operate = await station.waitForMessage(within: .seconds(30)) { Self.invoke($0)?.verb == "setTgxlOperate" }
        #expect(operate.flatMap(Self.invoke)?.args == [LinkMessage.PropertyEntry(name: "on", value: .bool(false))])
        #expect(await settle { accessories.tunerGenius?.operate == false })
        #expect(TunerGeniusPage.buttonLabel(try #require(accessories.tunerGenius)) == "STANDBY")
        #expect(!station.messages.compactMap(Self.invoke).contains { $0.verb == "tx.tune" || $0.verb == "tx.tunerTune" })
        await model.disconnect()
    }

    @Test("a refused verb shows the Core's reason on its page, and the page keeps the device's report")
    func refusedVerb() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.tunerGenius?.antenna == 1 })
        let words = "The radio is on the air. Try again when it stops."
        station.refuseNext("setTgxlAntenna", reason: words)
        accessories.setTunerAntenna(3)
        #expect(await settle { accessories.notes[.tunerGenius] == words })
        #expect(accessories.tunerGenius?.antenna == 1)
        accessories.dismissNote(.tunerGenius)
        #expect(accessories.notes[.tunerGenius] == nil)
        await model.disconnect()
    }

    @Test("a change the Core holds for this phone's question is not a refusal, by verb or by setting (A1)")
    func heldForQuestionIsNotARefusal() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.tunerGenius?.antenna == 1 && accessories.records != nil })
        // The Core holds the antenna change for a question (the
        // several-devices design, section 7.3): no red refusal on the page.
        station.refuseNext("setTgxlAntenna", reason: SeveralDevices.waitingReason)
        let accepted = await accessories.invoke(.tunerGenius, "setTgxlAntenna",
                                                [CommandArgument(name: "port", value: .int(3))])
        #expect(!accepted)
        #expect(accessories.notes[.tunerGenius] == nil)
        // Likewise a held setting; the poll interval written after it is
        // answered after it, so its answer is in by then.
        station.refuseNext("TGXL_Ant1_Label", reason: SeveralDevices.waitingReason)
        accessories.setAntennaLabel(.tunerGenius, port: 1, "Dipole")
        accessories.setRfKitPollInterval(2000)
        #expect(await settingWritten(station, "RfKit_PollIntervalMs") == "2000")
        #expect(await settle { accessories.rfKitConnectionSettings.pollMs == 2000 })
        #expect(accessories.notes[.tunerGenius] == nil)
        await model.disconnect()
    }

    @Test("the Tuner Genius lights the antenna the Core reports, which counts from 0")
    func tunerAntennaFromCore() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.tunerGenius != nil && accessories.records != nil })
        // The Core reports ANT 3 as 2.
        await station.deliver(FakeStation.accessoryDelta("tuner", "TunerModel", [("antennaA", .i64(2))]))
        #expect(await settle { accessories.tunerGenius?.antenna == 3 })
        let three = try #require(accessories.tunerGenius)
        #expect((1...3).filter { TunerGeniusPage.antennaLit(three, port: $0) } == [3])
        // And ANT 1 as 0.
        await station.deliver(FakeStation.accessoryDelta("tuner", "TunerModel", [("antennaA", .i64(0))]))
        #expect(await settle { accessories.tunerGenius?.antenna == 1 })
        let one = try #require(accessories.tunerGenius)
        #expect((1...3).filter { TunerGeniusPage.antennaLit(one, port: $0) } == [1])
        // The tune-memory line reads the same numbering: ANT 1's stored 40m tune.
        #expect(TunerGeniusPage.recalledLine(one, memory: try #require(accessories.records).tuneMemory)
                == "Recalled from tune memory for ANT 1 on 40m.")
        await model.disconnect()
    }

    // MARK: The RF2K-S

    @Test("the RF2K-S lights the antenna the Core reports, which counts from 1 as the amp does")
    func rfKitAntennaFromCore() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.rfKit != nil })
        await station.deliver(FakeStation.accessoryDelta("rfkit", "RfKitModel", [
            ("activeAntennaNumber", .i64(3)), ("activeAntennaExternal", .bool(false)),
        ]))
        #expect(await settle { accessories.rfKit?.activeAntenna == 3 })
        let three = try #require(accessories.rfKit)
        #expect((1...4).filter { Rf2ksPage.antennaLit(three, port: $0) } == [3])
        // 0 is no antenna: none lit.
        await station.deliver(FakeStation.accessoryDelta("rfkit", "RfKitModel", [("activeAntennaNumber", .i64(0))]))
        #expect(await settle { accessories.rfKit?.activeAntenna == 0 })
        let none = try #require(accessories.rfKit)
        #expect((1...4).filter { Rf2ksPage.antennaLit(none, port: $0) }.isEmpty)
        await model.disconnect()
    }

    @Test("the RF2K-S: OPERATE and its antennas send their verbs; an antenna it does not list is greyed")
    func rfKit() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.rfKit?.activeAntenna == 1 })
        let rfKit = try #require(accessories.rfKit)
        #expect(AccessoryStatusLine.rfKitHeader(rfKit) == "Through the Core \u{00B7} band from the radio")
        #expect(Rf2ksPage.readings(rfKit) == "Fwd 0 W \u{00B7} SWR 1.00 \u{00B7} 53.2 V \u{00B7} 0.0 A")
        #expect(Rf2ksPage.tunerWord(rfKit.tunerMode) == "BYPASS")
        #expect(AccessoriesModel.rfKitTunerNote
                == "Use the amp's front panel for these. Its firmware doesn't accept tuner commands.")
        #expect(accessories.rfKitAntennaReason(3) == nil)
        #expect(accessories.rfKitAntennaReason(4) == AccessoriesModel.rfKitAntennaUnavailable)

        accessories.setRfKitOperate(true)
        let operate = await station.waitForMessage(within: .seconds(30)) { Self.invoke($0)?.verb == "setRfKitOperate" }
        #expect(operate.flatMap(Self.invoke)?.args == [LinkMessage.PropertyEntry(name: "on", value: .bool(true))])
        #expect(await settle { accessories.rfKit?.operate == true })

        accessories.setRfKitAntenna(3)
        let antenna = await station.waitForMessage(within: .seconds(30)) { Self.invoke($0)?.verb == "setRfKitAntenna" }
        #expect(antenna.flatMap(Self.invoke)?.args == [LinkMessage.PropertyEntry(name: "port", value: .i64(3))])
        #expect(await settle { accessories.rfKit?.activeAntenna == 3 })

        // ANT 4 is not one the amp lists: nothing is sent, and the page says why.
        accessories.setRfKitAntenna(4)
        #expect(accessories.notes[.rfKit] == AccessoriesModel.rfKitAntennaUnavailable)
        #expect(station.messages.compactMap(Self.invoke).filter { $0.verb == "setRfKitAntenna" }.count == 1)
        await model.disconnect()
    }

    // MARK: Greyed with a reason

    @Test("a switch that cannot run is greyed with its reason and sends nothing")
    func greyedSwitches() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.powerGenius?.link.connected == true })

        // The Core loses the amp.
        await station.deliver(FakeStation.accessoryDelta("amplifier", "AmplifierModel", [
            ("connectionPhase", .enumeration(5)), ("present", .bool(false)),
        ]))
        #expect(await settle { accessories.switchReason(.powerGenius) == AccessoriesModel.ampNotConnectedReason })
        #expect(AccessoryStatusLine.status(.powerGenius, in: accessories)
                == "Trying again \u{00B7} on the station's network")
        accessories.setAmpOperate(true)
        #expect(accessories.notes[.powerGenius] == AccessoriesModel.ampNotConnectedReason)

        // The radio goes on the air: the tuner and the RF-Kit wait.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(name: "transmitting", value: .bool(true)),
        ])))
        #expect(await settle { accessories.onAir })
        #expect(accessories.switchReason(.tunerGenius) == AccessoriesModel.onAirReason)
        #expect(accessories.rfKitAntennaReason(2) == AccessoriesModel.onAirReason)
        accessories.setTunerAntenna(2)
        #expect(accessories.notes[.tunerGenius] == AccessoriesModel.onAirReason)
        let sent = station.messages.compactMap(Self.invoke).map(\.verb)
        #expect(!sent.contains("setPgxlOperate"))
        #expect(!sent.contains("setTgxlAntenna"))
        await model.disconnect()
    }

    @Test("a Core without the accessory verbs greys each switch with its reason")
    func olderCore() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let accessories = model.main.accessories
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "amplifier", className: "AmplifierModel",
                                                                    properties: [
            .init(name: "connectionPhase", value: .enumeration(6)), .init(name: "present", value: .bool(true)),
            .init(name: "operate", value: .bool(true)),
        ])))
        #expect(await settle { accessories.powerGenius?.present == true })
        #expect(accessories.records == nil)
        #expect(accessories.switchReason(.powerGenius) == TransmitModel.ampOlderCoreText)
        #expect(accessories.switchReason(.tunerGenius) == TransmitModel.tunerOlderCoreText)
        accessories.clearFaults(.powerGenius)
        #expect(accessories.notes[.powerGenius] == AccessoriesModel.noRecordsText)
        #expect(!station.messages.compactMap(Self.invoke).contains { $0.verb == "clearAccessoryFaults" })
        await model.disconnect()
    }

    // MARK: The devices' own settings (fix round 1: what a remote window changes)

    @Test("the name, hardware and network go to the device through the Core, and the page follows its answer")
    func deviceSettings() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.deviceSettingsReason(.powerGenius) == nil
                && accessories.deviceSettings["pgxlBiasMode"] == .text("ClassAB") })

        accessories.setName(.powerGenius, "Shack_amp")
        #expect(await sent(station, "setPgxlName") == [.init(name: "name", value: .utf8("Shack_amp"))])
        #expect(await settle { accessories.deviceSettings["pgxlNickname"] == .text("Shack_amp") })
        accessories.setName(.tunerGenius, "Shack_tuner")
        #expect(await sent(station, "setTgxlName") == [.init(name: "name", value: .utf8("Shack_tuner"))])
        #expect(await settle { accessories.deviceSettings["tgxlNickname"] == .text("Shack_tuner") })

        accessories.setPgxlHardware("biasMode", .text("ClassA"))
        #expect(await sent(station, "setPgxlHardware") == [.init(name: "biasMode", value: .utf8("ClassA"))])
        #expect(await settle { accessories.deviceSettings["pgxlBiasMode"] == .text("ClassA") })
        accessories.setPgxlHardware("ledIntensity", .int(50))
        #expect(await settle { accessories.deviceSettings["pgxlLedIntensity"] == .int(50) })

        accessories.setNetwork(.tunerGenius, dhcp: false, address: "192.168.1.50", netmask: "255.255.255.0",
                               gateway: "192.168.1.1")
        #expect(await sent(station, "setTgxlNetwork") == [
            .init(name: "dhcp", value: .bool(false)), .init(name: "address", value: .utf8("192.168.1.50")),
            .init(name: "netmask", value: .utf8("255.255.255.0")), .init(name: "gateway", value: .utf8("192.168.1.1")),
        ])
        #expect(await settle { accessories.deviceSettings["tgxlDhcp"] == .bool(false) })

        accessories.saveDeviceSettings(.powerGenius)
        #expect(await sent(station, "savePgxlSettings") == [])
        accessories.revertDeviceSettings(.tunerGenius)
        #expect(await sent(station, "readTgxlSettings") == [])

        // The Core's refusal shows on the page, in its words.
        let words = "The request to rename the Power Genius was not understood."
        station.refuseNext("setPgxlName", reason: words)
        accessories.setName(.powerGenius, "Other")
        #expect(await settle { accessories.notes[.powerGenius] == words })
        #expect(accessories.deviceSettings["pgxlNickname"] == .text("Shack_amp"))
        await model.disconnect()
    }

    /// The desktop's Advanced pages take the amp's and tuner's name as one
    /// word (PgxlAdvancedPage.cpp:393-399, TgxlAdvancedPage.cpp:464-470):
    /// its validator refuses a space or "=" at the keyboard, and the Core
    /// refuses them too ("Enter a name without spaces or equals signs.").
    @Test("the Name rows take one word: a space or = typed or pasted changes nothing, and Set sends the word")
    func nameIsOneWord() async throws {
        #expect(AccessoryFields.nameTip == "One word: no spaces or equals signs.")
        #expect(AccessoryFields.nameEdit(from: "Shack", to: "Shack_") == "Shack_")
        #expect(AccessoryFields.nameEdit(from: "Shack", to: "Shack ") == "Shack")
        #expect(AccessoryFields.nameEdit(from: "Shack", to: "Shack=") == "Shack")
        #expect(AccessoryFields.nameEdit(from: "Shack", to: "Shack\t") == "Shack")
        #expect(AccessoryFields.nameEdit(from: "Shack", to: "Shack\u{00A0}") == "Shack")
        #expect(AccessoryFields.nameEdit(from: "Shack", to: "Shack\u{0007}") == "Shack")
        // A paste with a space in it is refused whole, as the desktop's validator refuses it.
        #expect(AccessoryFields.nameEdit(from: "", to: "Shack amp") == "")
        #expect(AccessoryFields.nameEdit(from: "", to: "Shack_amp") == "Shack_amp")

        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.deviceSettingsReason(.powerGenius) == nil
                && accessories.deviceSettingsReason(.tunerGenius) == nil })
        for (device, identifier, verb) in [(AccessoriesModel.Device.powerGenius, "pgxlName", "setPgxlName"),
                                           (.tunerGenius, "tgxlName", "setTgxlName")] {
            let window = try BandFlagShotTests.window(size: CGSize(width: 402, height: 1_600))
            let host = UIHostingController(rootView: ScrollView {
                AccessoryAdvancedPage(model: accessories, device: device, coreName: "Shack")
            }.preferredColorScheme(.dark))
            host.view.frame = window.bounds
            let wasOn = SetupTypedEntryTests.applicationAccessibility()
            SetupTypedEntryTests.setApplicationAccessibility(true)
            window.rootViewController = host
            window.isHidden = false
            defer {
                window.isHidden = true
                window.rootViewController = nil
                SetupTypedEntryTests.setApplicationAccessibility(wasOn)
            }
            await ShotWait.laidOut(window)
            // The page's own identifier covers its rows' (SwiftUI hands a
            // container's identifier to every element in it), so the row's
            // elements are found by their labels, and its text field is the
            // one under the Name element.
            let fields = Self.textFields(in: window)
            func field(under element: NSObject) -> UITextField? {
                let centre = CGPoint(x: element.accessibilityFrame.midX, y: element.accessibilityFrame.midY)
                return fields.first { $0.convert($0.bounds, to: nil).contains(centre) }
            }
            let named = Self.elements(labelled: "Name", in: window)
            let element = try #require(named.first { field(under: $0) != nil },
                                       "no \(identifier) field among \(named.map(\.accessibilityFrame))")
            #expect(element.accessibilityHint == AccessoryFields.nameTip)
            let field = try #require(field(under: element))
            #expect(field.placeholder == (device == .powerGenius ? "e.g. Shack_PGXL" : "e.g. Shack_TGXL"))
            #expect(field.becomeFirstResponder())
            field.text = ""
            field.sendActions(for: .editingChanged)
            for keystroke in ["Shack", " ", "=", "_", "amp", "\t", "=x"] {
                field.insertText(keystroke)
                await ShotWait.laidOut(window)
            }
            #expect(await settle { field.text == "Shack_amp" }, "\(identifier) reads \(field.text ?? "")")
            field.resignFirstResponder()
            let set = try #require(Self.elements(labelled: "Set Name", in: window).first, "no Set Name button")
            #expect(set.accessibilityActivate())
            #expect(await sent(station, verb) == [.init(name: "name", value: .utf8("Shack_amp"))])
        }
        await model.disconnect()
    }

    /// The accessibility elements under `root` whose label is `label`.
    static func elements(labelled label: String, in root: NSObject) -> [NSObject] {
        var found: [NSObject] = []
        var queue: [NSObject] = [root]
        var visited = 0
        while !queue.isEmpty, visited < 20_000 {
            let node = queue.removeFirst()
            visited += 1
            if node.isAccessibilityElement, node.accessibilityLabel == label {
                found.append(node)
            }
            if let elements = node.accessibilityElements as? [NSObject] {
                queue += elements
            } else {
                let count = node.accessibilityElementCount()
                if count != NSNotFound, count > 0 {
                    queue += (0..<count).compactMap { node.accessibilityElement(at: $0) as? NSObject }
                }
            }
            if let view = node as? UIView {
                queue += view.subviews
            }
        }
        return found
    }

    private static func textFields(in view: UIView) -> [UITextField] {
        var found: [UITextField] = []
        if let field = view as? UITextField {
            found.append(field)
        }
        for child in view.subviews {
            found += textFields(in: child)
        }
        return found
    }

    @Test("the output limit goes to the Core with setPgxlPowerCap and the page follows its record")
    func outputLimit() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.records?.powerCapEnabled == false })
        accessories.setPowerCap(enabled: true, watts: 1200)
        #expect(await sent(station, "setPgxlPowerCap") == [.init(name: "enabled", value: .bool(true)),
                                                          .init(name: "watts", value: .i64(1200))])
        #expect(await settle { accessories.records?.powerCapEnabled == true && accessories.records?.powerCapW == 1200 })
        await model.disconnect()
    }

    // MARK: The Core's connection to each device

    @Test("the address, Connect, Disconnect, the LAN scan and the switches go to the Core for each device")
    func connections() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.fourO3AEnabled && accessories.rfKitEnabled
                && accessories.pgxlConnectionSettings.pingSec == 10 })

        accessories.saveAddress(.powerGenius, host: "192.0.2.9", port: 9008)
        #expect(await sent(station, "setPgxlAddress") == [.init(name: "host", value: .utf8("192.0.2.9")),
                                                         .init(name: "port", value: .i64(9008))])
        #expect(await settle { accessories.powerGenius?.link.host == "192.0.2.9" })
        accessories.saveAddress(.rfKit, host: "192.0.2.9", port: 0)
        #expect(await settle { accessories.notes[.rfKit]
                == "Enter the RF-Kit amplifier's IP address or host name, and a port from 1 to 65535." })
        accessories.connect(.tunerGenius, host: "192.0.2.10", port: 9010)
        #expect(await sent(station, "configureTgxl") == [.init(name: "host", value: .utf8("192.0.2.10")),
                                                        .init(name: "port", value: .i64(9010))])
        #expect(await settle { accessories.tunerGenius?.link.phase == .connecting })
        accessories.disconnect(.rfKit)
        #expect(await sent(station, "disconnectRfKit") == [])
        #expect(await settle { accessories.rfKit?.link.phase == .disconnected })

        accessories.scan(.powerGenius)
        #expect(await sent(station, "scanPgxlLan") == [])
        #expect(await settle { accessories.scans[.powerGenius]?.first?.address == "192.168.109.235" })
        #expect(accessories.scans[.powerGenius]?.first?.port == 9008)

        accessories.setFourO3AEnabled(false, from: .tunerGenius)
        #expect(await sent(station, "setFourO3AEnabled") == [.init(name: "enabled", value: .bool(false))])
        #expect(await settle { !accessories.fourO3AEnabled })
        accessories.setRfKitEnabled(false)
        #expect(await sent(station, "setRfKitEnabled") == [.init(name: "enabled", value: .bool(false))])
        #expect(await settle { !accessories.rfKitEnabled })

        accessories.setPgxlConnectionSettings(autoReconnect: false, keepaliveSec: 45, pingSec: 0)
        #expect(await sent(station, "setPgxlConnectionSettings") == [
            .init(name: "autoReconnect", value: .bool(false)), .init(name: "keepaliveSec", value: .i64(45)),
            .init(name: "pingSec", value: .i64(0)),
        ])
        #expect(await settle { accessories.pgxlConnectionSettings == (false, 45, 0) })
        await model.disconnect()
    }

    // MARK: The Tuner Genius's bypass and relays, the RF2K-S's reset and TCI mode

    @Test("BYPASS and the relay nudges go to the tuner, and the RF2K-S's reset and TCI mode to the amp")
    func tunerAndRfKitActions() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.tunerGenius?.relays == [112, 47, 186] })

        accessories.setTunerBypass(true)
        #expect(await sent(station, "setTgxlBypass") == [.init(name: "on", value: .bool(true))])
        #expect(await settle { accessories.tunerGenius?.bypass == true })
        #expect(TunerGeniusPage.buttonLabel(try #require(accessories.tunerGenius)) == "BYPASS")

        accessories.moveTunerRelay(1, direction: 1)
        #expect(await sent(station, "moveTgxlRelay") == [.init(name: "relay", value: .i64(1)),
                                                        .init(name: "direction", value: .i64(1))])
        #expect(await settle { accessories.tunerGenius?.relays == [112, 48, 186] })

        accessories.resetRfKitError()
        #expect(await sent(station, "resetRfKitError") == [])
        accessories.setRfKitTciMode()
        #expect(await sent(station, "setRfKitTciMode") == [])

        // On the air, the relays and TCI mode wait; nothing is sent.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(name: "transmitting", value: .bool(true)),
        ])))
        #expect(await settle { accessories.onAir })
        #expect(accessories.relayReason == AccessoriesModel.onAirReason)
        #expect(accessories.rfKitActionReason(tciMode: true) == AccessoriesModel.onAirReason)
        #expect(accessories.rfKitActionReason(tciMode: false) == nil)
        let before = station.messages.compactMap(Self.invoke).filter { $0.verb == "moveTgxlRelay" }.count
        accessories.moveTunerRelay(0, direction: -1)
        #expect(station.messages.compactMap(Self.invoke).filter { $0.verb == "moveTgxlRelay" }.count == before)
        await model.disconnect()
    }

    // MARK: Station settings the pages write

    @Test("antenna names, the tune memory recall and the RF-Kit's reading settings are the Core's station settings")
    func stationSettings() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.records?.tunerLabels == ["Beam", "Vertical", "Dipole"]
                && accessories.rfKitConnectionSettings.pollMs == 1000 })

        accessories.setAntennaLabel(.tunerGenius, port: 2, "Loop")
        #expect(await settingWritten(station, "TGXL_Ant2_Label") == "Loop")
        #expect(await settle { accessories.records?.tunerLabels == ["Beam", "Loop", "Dipole"] })
        accessories.setAntennaLabel(.rfKit, port: 4, "Yagi")
        #expect(await settingWritten(station, "RfKit_Ant4_Label") == "Yagi")
        #expect(await settle { accessories.records?.rfKitLabels[3] == "Yagi" })

        accessories.setAutoRecall(false)
        #expect(await settingWritten(station, "TGXL_AutoTuneMemoryRecall") == "False")
        #expect(await settle { accessories.records?.autoRecall == false })

        accessories.setRfKitAutoReconnect(false)
        #expect(await settingWritten(station, "RfKit_AutoReconnect") == "False")
        accessories.setRfKitPollInterval(2000)
        #expect(await settingWritten(station, "RfKit_PollIntervalMs") == "2000")
        #expect(await settle { accessories.rfKitConnectionSettings == (false, 2000) })

        // The Core's refusal of a setting shows its words.
        let words = "The Core could not save this antenna name."
        station.refuseNext("TGXL_Ant1_Label", reason: words)
        accessories.setAntennaLabel(.tunerGenius, port: 1, "Bad")
        #expect(await settle { accessories.notes[.tunerGenius] == words })
        #expect(accessories.records?.tunerLabels.first == "Beam")
        await model.disconnect()
    }

    @Test("Clear forgets a stored tune at the Core, and the tune memory page's recall switch writes the Core's setting")
    func tuneMemoryClearAndRecall() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.records?.tuneMemory.count == 12 })
        let tune = try #require(accessories.records?.tuneMemory.first { $0.band == "40m" && $0.antenna == 1 })
        #expect(AccessoriesModel.storedTuneKey(tune) == "TGXL_TuneMemory_Ant1_Band40m")
        accessories.clearStoredTune(tune)
        #expect(await settingWritten(station, "TGXL_TuneMemory_Ant1_Band40m") == "")
        #expect(await settle { accessories.records?.tuneMemory.count == 11 })
        #expect(accessories.records?.tuneMemory.contains(tune) == false)
        accessories.setAutoRecall(false)
        #expect(await settingWritten(station, "TGXL_AutoTuneMemoryRecall") == "False")
        #expect(await settle { accessories.records?.autoRecall == false })
        await model.disconnect()

        // A Core that does not share its records: Clear and the switch say why and send nothing.
        let (older, olderStation) = try await connected(additions: [.remoteTx])
        older.main.accessories.clearStoredTune(tune)
        #expect(older.main.accessories.notes[.tunerGenius] == AccessoriesModel.noRecordsText)
        #expect(!olderStation.messages.contains {
            if case .settingsWrite(let write) = $0 { return write.key.hasPrefix("TGXL_TuneMemory_") }
            return false
        })
        await older.disconnect()
    }

    @Test("while the Power Genius is over its output limit its page and the TX panel show the Core's words")
    func powerCapAlert() async throws {
        let (model, station) = try await connected()
        let accessories = model.main.accessories
        await station.deliverAccessories()
        #expect(await settle { accessories.records != nil })
        #expect(accessories.powerCapAlert == nil)
        let words = "Power Genius output 1612 W is above the 1500 W limit."
        await station.deliver(FakeStation.accessoryDelta("accessoryData", "AccessoryDataModel", [
            ("powerCapEnabled", .bool(true)), ("powerCapExceeded", .bool(true)),
            ("powerCapAlertText", .utf8(words)), ("powerCapAlertCount", .i64(1)),
        ]))
        #expect(await settle { accessories.powerCapAlert == words })
        #expect(accessories.records?.powerCapAlertCount == 1)
        // Back under the limit, the Core clears it and the words go.
        await station.deliver(FakeStation.accessoryDelta("accessoryData", "AccessoryDataModel", [
            ("powerCapExceeded", .bool(false)),
        ]))
        #expect(await settle { accessories.powerCapAlert == nil })
        await model.disconnect()
    }

    @Test("the TX panel's SWR Prot row reads the Core's SWR protection setting and says why it never lights")
    func swrProtection() async throws {
        #expect(TransmitModel.swrProtection([:]) == nil)
        #expect(TransmitModel.swrProtection(["DisplayFftSize": "4096"]) == .init(on: false, limit: nil))
        #expect(TransmitModel.swrProtection(["SwrProtectionEnabled": "True", "SwrProtectionLimit": "2.5"])
                == .init(on: true, limit: 2.5))
        #expect(TxPanel.swrProtectionText(nil) == "SWR protection: not known.")
        #expect(TxPanel.swrProtectionText(.init(on: false, limit: 2)) == "SWR protection: off at the Core.")
        #expect(TxPanel.swrProtectionText(.init(on: true, limit: 2.5)) == "SWR protection: on at the Core, limit 2.5.")
        #expect(TxPanel.swrProtectionText(.init(on: true, limit: nil)) == "SWR protection: on at the Core.")

        let (model, station) = try await connected()
        await station.deliver(.settingsValue(LinkMessage.SettingsValue(key: "SwrProtectionEnabled", origin: "",
                                                                         properties: [.init(name: "SwrProtectionEnabled", value: .utf8("True"))])))
        await station.deliver(.settingsValue(LinkMessage.SettingsValue(key: "SwrProtectionLimit", origin: "",
                                                                         properties: [.init(name: "SwrProtectionLimit", value: .utf8("3.0"))])))
        #expect(await settle { model.main.transmit.swrProtection == .init(on: true, limit: 3) })
        await model.disconnect()
    }

    @Test("on an older Core, or with the device not connected, a setting says why and sends nothing")
    func settingsGates() async throws {
        let (model, station) = try await connected(additions: [.remoteTx])
        let accessories = model.main.accessories
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "amplifier", className: "AmplifierModel",
                                                                    properties: [
            .init(name: "connectionPhase", value: .enumeration(6)), .init(name: "present", value: .bool(true)),
        ])))
        #expect(await settle { accessories.powerGenius != nil })
        #expect(accessories.deviceSettingsReason(.powerGenius) == AccessoriesModel.olderCoreSettingText)
        #expect(accessories.savedAddressReason(.rfKit) == AccessoriesModel.olderCoreSettingText)
        accessories.setName(.powerGenius, "Shack_amp")
        #expect(accessories.notes[.powerGenius] == AccessoriesModel.olderCoreSettingText)
        accessories.setAntennaLabel(.tunerGenius, port: 1, "Beam")
        #expect(accessories.notes[.tunerGenius] == AccessoriesModel.noRecordsText)
        #expect(!station.messages.compactMap(Self.invoke).contains { $0.verb == "setPgxlName" })
        await model.disconnect()

        let (newer, newerStation) = try await connected()
        var scene = FakeStation.AccessoryScene.board
        scene.tunerConnected = false
        await newerStation.deliverAccessories(scene)
        let newerAccessories = newer.main.accessories
        #expect(await settle { newerAccessories.tunerGenius?.link.phase == .disconnected })
        #expect(newerAccessories.deviceSettingsReason(.tunerGenius) == AccessoriesModel.tunerNotConnectedReason)
        #expect(newerAccessories.savedAddressReason(.tunerGenius) == nil)
        await newer.disconnect()
    }

    // MARK: Interlock says no

    @Test("with the amp in STANDBY and the interlock on Block, PTT is refused with Operate amp, which operates it")
    func interlockSaysNo() async throws {
        let (model, station) = try await connected()
        let transmit = model.main.transmit
        var scene = FakeStation.AccessoryScene.board
        scene.ampOperate = false
        await station.deliverAccessories(scene)
        #expect(await settle { transmit.amp?.operate == false && transmit.ampOperateAvailable })

        transmit.tapPtt()
        let refusal = TxRefusalInfo(reason: FakeStation.ampStandbyReason, code: "ampStandby",
                                    fix: TxRefusalInfo.operateAmp)
        #expect(await settle { transmit.ptt.state == .refused(refusal) })
        #expect(TxNoticeCard.content(ptt: transmit.ptt, heldNote: nil) == .refusal(refusal))
        #expect(!transmit.ptt.transmitting)
        #expect(!station.keyed)

        transmit.applyFix(refusal.fix)
        let operate = await station.waitForMessage(within: .seconds(30)) { Self.invoke($0)?.verb == "setPgxlOperate" }
        #expect(operate.flatMap(Self.invoke)?.args == [LinkMessage.PropertyEntry(name: "on", value: .bool(true))])
        #expect(await settle { transmit.amp?.operate == true && model.main.accessories.powerGenius?.operate == true })
        // Amp values can be optimistic; dismissal reaches this snapshot through
        // its own actor task and stream, so observe that independent completion.
        #expect(await settle { TxNoticeCard.content(ptt: transmit.ptt, heldNote: nil) == nil })
        #expect(TxNoticeCard.content(ptt: transmit.ptt, heldNote: nil) == nil)

        // Operating, the next tap keys.
        transmit.tapPtt()
        #expect(await settle { transmit.ptt.state.isKeyed })
        #expect(station.keyed)
        transmit.tapPtt()
        #expect(await settle { transmit.ptt.state == .idle })
        await model.disconnect()
    }

    // MARK: Pictures

    @Test("the Radio tab, the three pages and the interlock's refusal, for picture 11")
    func accessoryShots() async throws {
        let (model, station) = try await connected(additions: [.remoteTx, .accessories, .bands])
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(3)),
            .init(ordinal: 12, name: "txSlice", value: .bool(true)),
        ])))
        await station.deliverAccessories()
        let accessories = model.main.accessories
        #expect(await settle { accessories.standing(.rfKit) == .setUp && accessories.bandLabel == "40m"
                && model.main.coreName == "KG4VCF/shack" })
        let flow = Self.flow(model, station)

        try await shoot("accessories-radio-tab", model: model) {
            RadioView(app: model, main: model.main, flow: flow)
        }
        let pages: [(String, [AccessoriesSection.Route])] = [
            ("accessories-power-genius", [.page(.powerGenius)]),
            ("accessories-tuner-genius", [.page(.tunerGenius)]),
            ("accessories-rf2ks", [.page(.rfKit)]),
            ("accessories-power-genius-faults", [.page(.powerGenius), .faults(.powerGenius)]),
            ("accessories-power-genius-advanced", [.page(.powerGenius), .advanced(.powerGenius)]),
            ("accessories-tuner-genius-memory", [.page(.tunerGenius), .tuneMemory]),
            ("accessories-rf2ks-settings", [.page(.rfKit), .advanced(.rfKit)]),
            ("accessories-tuner-genius-advanced", [.page(.tunerGenius), .advanced(.tunerGenius)]),
        ]
        for (name, stack) in pages {
            try await shoot(name, model: model) {
                RadioView(app: model, main: model.main, flow: flow, accessoryRoute: stack)
            }
        }

        // Interlock says no: the amp in STANDBY, PTT tapped.
        var standby = FakeStation.AccessoryScene.board
        standby.ampOperate = false
        await station.deliverAccessories(standby)
        let transmit = model.main.transmit
        #expect(await settle { transmit.amp?.operate == false })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        transmit.tapPtt()
        #expect(await settle { if case .refused = transmit.ptt.state { return true } else { return false } })
        try await shoot("accessories-interlock-says-no", model: model, feedsBand: true) {
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, txPanelOpen: false)
                TabBar(selection: .constant(.panadapter), sideways: false)
            }
        }
        await model.disconnect()
    }

    // MARK: The fake Core

    /// The app connected to a fake Core that owns its accessories and lets
    /// this phone transmit.
    private func connected(additions: FakeStation.Additions = [.remoteTx, .accessories]) async throws
        -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "AccessoryPagesTests-\(UUID().uuidString)"))
        let model = AppModel(displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "txState", className: "TransmitState",
                                                                    properties: TransmitScreenTests.holder(
                                                                        "", short: "", keyed: false)
                                                                        + [.init(ordinal: 17, name: "stopSerial",
                                                                                 value: .i64(0))])))
        #expect(await settle { model.main.transmit.permitted && model.connection == .connected })
        return (model, station)
    }

    /// A connection flow for the Radio tab's pictures; the app is already connected.
    private static func flow(_ model: AppModel, _ station: FakeStation) -> ConnectionFlow {
        ConnectionFlow(app: model, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()),
            stations: PairedStationStore(item: InMemorySecretItem()), kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
    }

    nonisolated static func invoke(_ message: LinkMessage) -> LinkMessage.CommandInvoke? {
        TransmitScreenTests.invoke(message)
    }

    /// The arguments of the next `verb` the app sent, or nil if none came.
    private func sent(_ station: FakeStation, _ verb: String) async -> [LinkMessage.PropertyEntry]? {
        await station.waitForMessage(within: .seconds(30)) { Self.invoke($0)?.verb == verb }.flatMap(Self.invoke)?.args
    }

    /// The value the app wrote to the Core's station setting `key`, or nil.
    private func settingWritten(_ station: FakeStation, _ key: String) async -> String? {
        let message = await station.waitForMessage(within: .seconds(30)) {
            if case .settingsWrite(let write) = $0 {
                return write.key == key
            }
            return false
        }
        guard case .settingsWrite(let write)? = message, case .utf8(let value)? = write.properties.first?.value else {
            return nil
        }
        return value
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(50))
        }
        return condition()
    }

    /// Draws `content` over the tab bar on an upright phone and, with
    /// `NEREUS_MAIN_SHOTS` set, writes it there.
    private func shoot<Content: View>(_ name: String, model: AppModel, feedsBand: Bool = false,
                                      @ViewBuilder content: () -> Content) async throws {
        let size = CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            content()
            if !feedsBand {
                TabBar(selection: .constant(.radio), sideways: false)
            }
        }
        .background(ChromeColours.page)
        .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw: ShotWait.BandDrawing?
        if feedsBand {
            bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        } else {
            bandDraw = nil
            await ShotWait.laidOut(window)
        }
        if let bandDraw {
            let band = model.main.band
            BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
            try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}
