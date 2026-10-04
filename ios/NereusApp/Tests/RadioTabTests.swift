// NereusSDR for iOS: the Radio tab against a fake Core: the radio at a glance, the More list, Manage Radios and Protocol Info
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
import NereusModels
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-18, D41, spec section 5.2 item 5: the Radio tab shows the radio at
/// a glance from what the Core sends, each value the Core does not send (or
/// an older Core does not know, or a reading gone stale) as unavailable
/// with its reason and never as zero, following the Core while the tab is
/// open; the More list follows the Core's own list, hiding Antenna Setup
/// only on a radio without antenna control; Manage Radios lists, chooses,
/// scans and forgets through the Core and shows its refusals in its words;
/// Protocol Info shows the Core's radio.
@Suite("Radio tab", .serialized)
@MainActor
struct RadioTabTests {
    private let platform = TestPlatform()

    // MARK: The radio at a glance

    @Test("each line shows the Core's value, and follows the Core while the tab is open")
    func glanceValuesFollowTheCore() async throws {
        let clock = TestLinkClock()
        let (model, station) = try await connected(additions: [.radioTelemetry], clock: clock)
        let radio = RadioTabModel(app: model)
        try await deliverCatalogue(station, revision: 2)
        await station.deliverRadioTelemetry()
        #expect(await settle { model.main.catalogFeed.revision == 2 && model.mirror.currentTelemetryReceipt != nil })
        #expect(values(radio, clock) == ["Hermes Lite 2", "72", "1", "192 kHz", "1 of 5", "13.8 V", "No overload",
                                         "31%"])
        #expect(radio.glance(nowMilliseconds: clock.now).first { $0.id == "adc" }?.reading
                == .value("No overload", .good))

        // New readings: an ADC in overload, a lower PA voltage, a new rate.
        await station.deliverRadioTelemetry(.init(paVolts: 12.5, sampleRateHz: 384_000, adcOverloads: [
            .init(adc: 0, overloaded: true), .init(adc: 1, overloaded: false),
        ], systemCpuPercent: 64.4))
        #expect(await settle { values(radio, clock)[5] == "12.5 V" })
        #expect(values(radio, clock) == ["Hermes Lite 2", "72", "1", "384 kHz", "1 of 5", "12.5 V",
                                         "Overload on ADC 0", "64%"])
        #expect(reading(radio, clock, "adc") == .value("Overload on ADC 0", .warning))

        // A status not known is not "no overload"; a reading left out is not zero.
        await station.deliverRadioTelemetry(.init(adcOverloads: [.init(adc: 0, overloaded: nil)]))
        #expect(await settle { reading(radio, clock, "pa") == .unavailable(RadioAtAGlance.absentPaReason) })
        #expect(reading(radio, clock, "adc") == .unavailable(RadioAtAGlance.unknownAdcReason))
        #expect(reading(radio, clock, "sampleRate") == .unavailable(RadioAtAGlance.absentSampleRateReason))
        #expect(reading(radio, clock, "coreCpu") == .unavailable(RadioAtAGlance.absentCpuReason))

        // Three seconds without a reading: unavailable, not the last value.
        await station.deliverRadioTelemetry()
        #expect(await settle { values(radio, clock)[5] == "13.8 V" })
        await clock.advance(by: 3_001)
        for id in ["sampleRate", "pa", "adc", "coreCpu"] {
            #expect(reading(radio, clock, id) == .unavailable(RadioAtAGlance.staleReason), "\(id)")
        }
        #expect(values(radio, clock)[0] == "Hermes Lite 2")

        // The Core loses its radio: its words on the radio's lines.
        let words = "The Core is waiting for its radio to appear on the network."
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(name: "connected", value: .bool(false)),
            .init(name: StationRadio.waitingProperty, value: .utf8(words)),
        ])))
        #expect(await settle { reading(radio, clock, "firmware") == .unavailable(words) })
        #expect(reading(radio, clock, "protocol") == .unavailable(words))
        #expect(reading(radio, clock, "pa") == .unavailable(words))
        #expect(radio.noRadio() == words)
        #expect(values(radio, clock)[4] == "1 of 5")

        // Disconnected: nothing old shows as current.
        await model.disconnect()
        #expect(await settle {
            radio.glance(nowMilliseconds: clock.now).allSatisfy {
                $0.reading == .unavailable(RadioAtAGlance.notConnectedReason)
            }
        })
    }

    @Test("an older Core's lines say what it does not report, and a Core yet to sample says so")
    func olderCoreReasons() async throws {
        // A Core before telemetry: no rate, PA, ADC or CPU.
        let (older, _) = try await connected()
        let radio = RadioTabModel(app: older)
        #expect(await settle { reading(radio, older.mirrorClock, "pa") != .unavailable(RadioAtAGlance.notConnectedReason) })
        #expect(reading(radio, older.mirrorClock, "sampleRate") == .unavailable(RadioAtAGlance.olderSampleRateReason))
        #expect(reading(radio, older.mirrorClock, "pa") == .unavailable(RadioAtAGlance.olderPaReason))
        #expect(reading(radio, older.mirrorClock, "adc") == .unavailable(RadioAtAGlance.olderAdcReason))
        #expect(reading(radio, older.mirrorClock, "coreCpu") == .unavailable(RadioAtAGlance.olderCpuReason))
        // Without a catalogue the slice count has no ceiling from it; the Core's own ceiling stands.
        #expect(reading(radio, older.mirrorClock, "slices") == .value("1 of 5"))
        await older.disconnect()

        // A Core at telemetry version 4: PA and rate, and no ADC status yet.
        let clock = TestLinkClock()
        let (four, station) = try await connected(additions: [.setupPanels], clock: clock)
        let fourRadio = RadioTabModel(app: four)
        #expect(await settle { four.mirror.capabilityVersion("stationTelemetryVersion") == 4 })
        #expect(reading(fourRadio, clock, "pa") == .unavailable(RadioAtAGlance.waitingReason))
        await station.deliverRadioTelemetry()
        #expect(await settle { reading(fourRadio, clock, "pa") == .value("13.8 V") })
        #expect(reading(fourRadio, clock, "sampleRate") == .value("192 kHz"))
        #expect(reading(fourRadio, clock, "adc") == .unavailable(RadioAtAGlance.olderAdcReason))
        #expect(reading(fourRadio, clock, "coreCpu") == .value("31%"))
        await four.disconnect()
    }

    // MARK: More

    @Test("More follows the Core's list while the tab is open; an older Core's items are greyed with why")
    func moreFollowsTheCore() async throws {
        let (model, station) = try await connected(additions: [.stationRadios])
        let radio = RadioTabModel(app: model)
        // Before a catalogue: this app's items, Antenna Setup waiting for the Core's word.
        #expect(await settle { radio.menu.map(\.id) == ["antennaSetup", "manageRadios", "protocolInfo"] })
        #expect(radio.menu[0].reason == RadioMenu.olderCoreAntennaReason)
        #expect(radio.menu[1].enabled && radio.menu[2].enabled)
        try await deliverCatalogue(station, revision: 2)
        #expect(await settle { model.main.catalogFeed.revision == 2 })
        // The tab reads the catalogue again on the main queue's next turn.
        #expect(await settle { radio.menu.map(\.id) == ["manageRadios", "antennaSetup", "protocolInfo"] })
        #expect(radio.menu.map(\.title) == ["Manage Radios", "Antenna Setup", "Protocol Info"])
        // The desktop builds Transverters: listed in its place, greyed, since this app has no page for it.
        try await deliverCatalogue(station, revision: 3, radioItems: { items in
            items.map { item in
                var item = item
                if item["id"] as? String == "transverters" { item["offered"] = true }
                return item
            }
        })
        #expect(await settle { radio.menu.contains { $0.id == "transverters" } })
        #expect(radio.menu.map(\.id) == ["manageRadios", "antennaSetup", "transverters", "protocolInfo"])
        let transverters = try #require(radio.menu.first { $0.id == "transverters" })
        #expect(!transverters.enabled && transverters.reason == RadioMenu.unknownItemReason)
        // A radio with one antenna port has no antenna control: Antenna Setup is left out.
        try await deliverCatalogue(station, revision: 4, board: ["rxAntennas": ["ANT1"], "txAntennas": ["ANT1"]])
        #expect(await settle { !radio.menu.contains { $0.id == "antennaSetup" } })
        await model.disconnect()
        #expect(await settle { radio.menu.filter { $0.page != nil }.allSatisfy { $0.reason == RadioMenu.notConnectedReason } })

        // A Core that does not choose its radio: Manage Radios greyed with the Core's words.
        let (older, _) = try await connected()
        let olderRadio = RadioTabModel(app: older)
        #expect(await settle { olderRadio.menu.first { $0.id == "manageRadios" }?.reason
                == RadioMenu.olderCoreRadioReason })
        #expect(olderRadio.menu.first { $0.id == "protocolInfo" }?.enabled == true)
        await older.disconnect()
    }

    @Test("More follows each item's offered: Antenna Setup and Transverters are left out where the Core does not offer them")
    func moreFollowsOffered() async throws {
        let (model, station) = try await connected(additions: [.stationRadios])
        let radio = RadioTabModel(app: model)
        // The Hermes Lite 2's catalogue: no antenna control, and Transverters not built.
        try await deliverCatalogue(station, revision: 2, fixture: "catalog-hermes-lite-2")
        #expect(await settle { radio.menu.map(\.id) == ["manageRadios", "protocolInfo"] })
        #expect(radio.menu.allSatisfy { $0.enabled })
        // The ANAN-G2's offers Antenna Setup; a catalogue that stops offering it leaves it out,
        // even with the board's antennas unchanged.
        try await deliverCatalogue(station, revision: 3)
        #expect(await settle { radio.menu.map(\.id) == ["manageRadios", "antennaSetup", "protocolInfo"] })
        try await deliverCatalogue(station, revision: 4, radioItems: { items in
            items.map { item in
                var item = item
                if item["id"] as? String == "antennaSetup" { item["offered"] = false }
                return item
            }
        })
        #expect(await settle { radio.menu.map(\.id) == ["manageRadios", "protocolInfo"] })
        await model.disconnect()
    }

    @Test("Antenna Setup opens the Core's antenna page when the Core describes it, and is left out when it has none")
    func antennaSetupFromTheCoresDescription() async throws {
        let (model, station) = try await connected(additions: [.setupDescription, .setupPanels])
        let radio = RadioTabModel(app: model)
        #expect(await settle { radio.menu.first { $0.id == "antennaSetup" }?.reason == RadioMenu.antennaWaitingReason })
        await station.deliverSetup([("hardware", Self.antennaPage)])
        #expect(await settle { radio.menu.first { $0.id == "antennaSetup" }?.enabled == true })
        #expect(model.setupPages.page(RadioTabModel.antennaPage, in: RadioTabModel.antennaCategory)?.title
                == "Antenna / ALEX")
        // The Core describes Setup without an antenna page: a board without antenna control.
        await station.publishSetup("hardware", json: "", revision: 2)
        #expect(await settle { !radio.menu.contains { $0.id == "antennaSetup" } })
        await model.disconnect()
    }

    // MARK: Manage Radios

    @Test("Manage Radios lists the Core's radios, chooses, forgets and scans, and shows the Core's refusals")
    func manageRadiosChooseAndRefusal() async throws {
        let (model, station) = try await connected(additions: [.stationRadios])
        await station.setStationRadios(.bench)
        let manage = ManageRadiosModel(mirror: model.mirror, commands: model.commands, records: model.records,
                                       catalogFeed: model.main.catalogFeed, signedInWithDeviceKey: { true })
        #expect(manage.radios.isEmpty)
        manage.open()
        let subscribe = await station.waitForMessage {
            Self.invoke($0)?.verb == "records.subscribe"
                && Self.invoke($0)?.args.contains { $0.value == .utf8(StationRadio.streamName) } == true
        }
        #expect(subscribe.flatMap(Self.invoke)?.args.contains { $0.name == "backlog" && $0.value == .i64(64) } == true)
        #expect(await settle { manage.radios.count == 2 })
        #expect(manage.radios.map(\.name) == ["Bench HL2", "Bench G2"])
        #expect(manage.radios[0].inUse && !manage.radios[1].inUse)
        #expect(manage.radios[1].protocolVersion == 2 && manage.radios[1].address == "192.0.2.22")
        #expect(manage.actionReason == nil && manage.listNote == nil)
        // A Core before radioModelsVersion 1 names no model; the choice is greyed with why.
        #expect(manage.modelName(manage.radios[0]) == nil && manage.modelName(manage.radios[1]) == nil)
        #expect(manage.modelText(manage.radios[1]) == ManageRadiosModel.unknownModelText)
        #expect(manage.modelChoices(manage.radios[1]).isEmpty)
        #expect(manage.changeModelReason(manage.radios[1]) == ManageRadiosModel.olderModelsReason)
        // The catalogue names the model the Core runs its radio as.
        try await deliverCatalogue(station, revision: 2, board: ["model": 14])
        #expect(await settle { manage.modelName(manage.radios[0]) == "ANAN-G2" })
        let g2 = manage.radios[1]
        let hl2 = manage.radios[0]

        // The Core refuses: its words, as sent.
        station.refuseNext(StationRadio.selectVerb, reason: FakeStation.radioOnAirReason)
        manage.choose(g2)
        #expect(await settle { manage.refusal == FakeStation.radioOnAirReason })
        #expect(manage.radios[0].inUse)

        // Chosen: the Core runs the G2, and says it is switching.
        manage.choose(g2)
        #expect(await settle { manage.radios.first { $0.mac == g2.mac }?.inUse == true })
        #expect(manage.refusal == nil && manage.note == ManageRadiosModel.switchingNote("Bench G2"))
        let chosen = station.messages.compactMap(Self.invoke).last { $0.verb == StationRadio.selectVerb }
        #expect(chosen?.args == [LinkMessage.PropertyEntry(name: "mac", value: .utf8(g2.mac))])

        // The Core will not forget the radio it runs; it forgets another.
        manage.forget(g2)
        #expect(await settle { manage.refusal == FakeStation.forgetInUseReason })
        manage.forget(hl2)
        #expect(await settle { manage.radios.map(\.mac) == [g2.mac] })
        #expect(manage.note == ManageRadiosModel.forgottenNote)
        manage.rescan()
        #expect(await settle { manage.note == ManageRadiosModel.scanningNote })

        // A radio the Core cannot see.
        manage.choose(StationRadio(id: "AA:BB:CC:DD:EE:09", name: "Gone"))
        #expect(await settle { manage.refusal == FakeStation.cannotSeeRadioReason })

        // Another device is on the Core: the choice is asked about first, not refused.
        var asking = FakeStation.SceneRadios.bench
        asking.asksFirst = true
        await station.setStationRadios(asking)
        #expect(await settle { manage.radios.count == 2 && manage.radios[0].inUse })
        // The Core holds it for this phone's question sheet: neither a note
        // nor a refusal, so nothing stays on the page once the question
        // closes (R2; the desktop, StationClient.cpp:7308-7317).
        manage.choose(manage.radios[1])
        #expect(manage.busy)
        #expect(await settle { !manage.busy })
        #expect(manage.note == nil)
        #expect(manage.refusal == nil)
        // The Core's question for the held choice opens the sheet; the page
        // still shows nothing, while it is open or once it is closed.
        await station.deliver(SeveralDevicesScreenTests.sharedSetting(id: 31))
        #expect(await settle { model.devices.question?.id == 31 })
        #expect(manage.note == nil && manage.refusal == nil)
        model.devices.cancel()
        #expect(model.devices.question == nil)
        #expect(manage.note == nil && manage.refusal == nil)
        #expect(manage.radios[0].inUse, "a held choice changes nothing until confirmed")

        // Closed: the app stops asking and drops the list.
        manage.close()
        #expect(await station.waitForMessage {
            Self.invoke($0)?.verb == "records.unsubscribe"
                && Self.invoke($0)?.args.contains { $0.value == .utf8(StationRadio.streamName) } == true
        } != nil)
        #expect(await settle { manage.radios.isEmpty })
        await model.disconnect()
    }

    @Test("the Core names each radio's model and the models it can run as; Change model sends station.setRadioModel")
    func manageRadiosModels() async throws {
        let (model, station) = try await connected(additions: [.stationRadios, .radioModels])
        await station.setStationRadios(.bench)
        let manage = ManageRadiosModel(mirror: model.mirror, commands: model.commands, records: model.records,
                                       catalogFeed: model.main.catalogFeed, signedInWithDeviceKey: { true })
        manage.open()
        #expect(await settle { manage.radios.count == 2 && manage.namesModels })
        let hl2 = manage.radios[0]
        let g2 = manage.radios[1]
        // A radio the Core is not running, named by the Core.
        #expect(manage.modelName(g2) == "ANAN-G2 1K" && manage.modelText(g2) == "ANAN-G2 1K")
        #expect(manage.modelName(hl2) == "Hermes Lite 2")
        #expect(manage.modelChoices(g2).map(\.label) == ["ANAN-G2", "ANAN-G2 1K"])
        #expect(manage.changeModelReason(g2) == nil)
        // A board that runs as one model: the choice stays, greyed, with why.
        #expect(manage.modelChoices(hl2).map(\.label) == ["Hermes Lite 2"])
        #expect(manage.changeModelReason(hl2) == ManageRadiosModel.oneModelReason("Hermes Lite 2"))

        // Chosen: the Core saves it, and the record carries the new model and name.
        let anan = try #require(manage.modelChoices(g2).first)
        manage.setModel(g2, anan)
        #expect(await settle { manage.radios.first { $0.mac == g2.mac }?.modelLabel == "ANAN-G2" })
        #expect(manage.modelName(try #require(manage.radios.first { $0.mac == g2.mac })) == "ANAN-G2")
        #expect(manage.note == ManageRadiosModel.modelSavedNote(radio: "Bench G2", model: "ANAN-G2"))
        let sent = station.messages.compactMap(Self.invoke).last { $0.verb == StationRadio.setModelVerb }
        #expect(sent?.args == [LinkMessage.PropertyEntry(name: "mac", value: .utf8(g2.mac)),
                               LinkMessage.PropertyEntry(name: "model", value: .i64(11))])

        // The Core refuses: its words, as sent.
        station.refuseNext(StationRadio.setModelVerb, reason: FakeStation.modelMismatchReason)
        manage.setModel(try #require(manage.radios.first { $0.mac == g2.mac }), try #require(manage.modelChoices(g2).last))
        #expect(await settle { manage.refusal == FakeStation.modelMismatchReason })

        // A model the radio does not list is never sent.
        let before = station.messages.compactMap(Self.invoke).filter { $0.verb == StationRadio.setModelVerb }.count
        manage.setModel(hl2, StationRadio.ModelChoice(model: 11, label: "ANAN-G2"))
        try await Task.sleep(for: .milliseconds(300))
        #expect(station.messages.compactMap(Self.invoke).filter { $0.verb == StationRadio.setModelVerb }.count == before)

        // On the air: greyed with the Core's reason.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(name: "transmitting", value: .bool(true)),
        ])))
        #expect(await settle { manage.changeModelReason(g2) == ManageRadiosModel.onAirReason })
        manage.close()
        await model.disconnect()
    }

    @Test("Manage Radios says why it cannot act: older Core, pairing-token sign-in, on the air, not connected")
    func manageRadiosReasons() async throws {
        let (older, _) = try await connected()
        let olderManage = ManageRadiosModel(app: older)
        olderManage.open()
        #expect(await settle { olderManage.actionReason == ManageRadiosModel.olderCoreReason })
        #expect(olderManage.listNote == ManageRadiosModel.olderCoreReason)
        await older.disconnect()

        // The fake signs in with the pairing token: the Core's own words for that.
        let (model, station) = try await connected(additions: [.stationRadios])
        let tokenManage = ManageRadiosModel(app: model)
        #expect(await settle { tokenManage.actionReason == ManageRadiosModel.pairedDeviceReason })

        let manage = ManageRadiosModel(mirror: model.mirror, commands: model.commands, records: model.records,
                                       catalogFeed: model.main.catalogFeed, signedInWithDeviceKey: { true })
        #expect(await settle { manage.actionReason == nil })
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(name: "transmitting", value: .bool(true)),
        ])))
        #expect(await settle { manage.actionReason == ManageRadiosModel.onAirReason })
        // A request while it cannot act is not sent.
        manage.rescan()
        #expect(!manage.busy)
        await station.setRadioWaiting("The Core can see more than one radio. Choose which one it runs.")
        #expect(await settle { manage.waiting == "The Core can see more than one radio. Choose which one it runs." })
        await model.disconnect()
        #expect(await settle { manage.actionReason == ManageRadiosModel.notConnectedReason && manage.waiting == nil })
    }

    // MARK: Protocol Info

    @Test("Protocol Info shows the Core's radio, its model, protocol, firmware, MAC and address")
    func protocolInfo() async throws {
        let (model, _) = try await connected()
        let radio = RadioTabModel(app: model)
        #expect(await settle { radio.protocolInfo(nowMilliseconds: 0).first?.reading
                != .unavailable(RadioAtAGlance.notConnectedReason) })
        let rows = radio.protocolInfo(nowMilliseconds: 0)
        #expect(rows.map(\.label) == ["Radio", "Model", "Protocol", "Firmware", "MAC", "IP address"])
        #expect(rows.map(\.reading.text) == ["ConnectableRadioModel fake", "Hermes Lite 2", "Protocol 1", "72",
                                             "aa:bb:cc:11:22:33", "127.0.0.1"])
        await model.disconnect()
        #expect(await settle { radio.protocolInfo(nowMilliseconds: 0).allSatisfy {
            $0.reading == .unavailable(RadioAtAGlance.notConnectedReason)
        } })
    }

    @Test("pure reading rules: kilohertz, the slice count and an unnamed radio")
    func readingRules() {
        #expect(RadioAtAGlance.kilohertz(192_000) == "192 kHz")
        #expect(RadioAtAGlance.kilohertz(44_100) == "44.1 kHz")
        let bare = RadioAtAGlance.Inputs(connected: true, ownSlices: 1, otherSlices: 2)
        #expect(RadioAtAGlance.slices(bare) == .value("3 in use"))
        #expect(RadioAtAGlance.model(bare) == .unavailable(RadioAtAGlance.notReportedReason))
        #expect(RadioAtAGlance.protocolNumber(bare) == .unavailable(RadioAtAGlance.notReportedReason))
        // The radio identity entries need agreed minor 11.
        let old = RadioAtAGlance.Inputs(connected: true, capabilities: ["radioProtocol": .int(2)], agreedMinor: 10)
        #expect(RadioAtAGlance.protocolNumber(old) == .unavailable(RadioAtAGlance.notReportedReason))
    }

    // MARK: On screen only

    @Test("the readings tick once a second only while shown, and show current values at once on return")
    func readingsTickOnlyWhileShown() async throws {
        let clock = TestLinkClock()
        let defaults = try #require(UserDefaults(suiteName: "RadioTabTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform, mirrorClock: clock)
        let radio = RadioTabModel(app: model)
        // Hidden: nothing periodic.
        #expect(!radio.isTicking && clock.pendingDueTimes.isEmpty)
        await clock.advance(by: 5_000)
        let hidden = radio.revision
        await clock.advance(by: 5_000)
        #expect(radio.revision == hidden)

        // Shown: read at once, then once a second.
        radio.show()
        #expect(radio.revision == hidden &+ 1)
        #expect(radio.isTicking && clock.pendingDueTimes == [clock.now + 1_000])
        await clock.advance(by: 1_000)
        #expect(radio.revision == hidden &+ 2)
        await clock.advance(by: 2_000)
        #expect(radio.revision == hidden &+ 4)

        // Hidden again: the timer is gone and the count stands still.
        radio.hide()
        #expect(!radio.isTicking && clock.pendingDueTimes.isEmpty)
        let stopped = radio.revision
        await clock.advance(by: 10_000)
        #expect(radio.revision == stopped)

        // Back on screen: current values at once, one timer only.
        radio.show()
        radio.show()
        #expect(radio.revision == stopped &+ 1 && clock.pendingDueTimes.count == 1)
        radio.hide()
    }

    @Test("what runs follows what is on screen: the tab's readings, or Manage Radios' list, or nothing")
    func lifecycleFollowsTheScreen() {
        #expect(RadioView.lifecycle(onScreen: nil) == (false, false))
        #expect(RadioView.lifecycle(onScreen: []) == (true, false))
        #expect(RadioView.lifecycle(onScreen: [.manageRadios]) == (false, true))
        #expect(RadioView.lifecycle(onScreen: [.protocolInfo]) == (false, false))
        #expect(RadioView.lifecycle(onScreen: [.accessory(.page(.powerGenius))]) == (false, false))
    }

    @Test("Manage Radios asks for its list while on screen, stops when it goes, and drops only its own ask")
    func manageRadiosSubscribesWhileOnScreen() async throws {
        let (model, station) = try await connected(additions: [.stationRadios])
        await station.setStationRadios(.bench)
        let manage = ManageRadiosModel(mirror: model.mirror, commands: model.commands, records: model.records,
                                       catalogFeed: model.main.catalogFeed, signedInWithDeviceKey: { true })
        func count(_ verb: String) -> Int {
            station.messages.compactMap(Self.invoke).filter {
                $0.verb == verb && $0.args.contains { $0.value == .utf8(StationRadio.streamName) }
            }.count
        }
        manage.open()
        #expect(await settle { manage.radios.count == 2 && count("records.subscribe") == 1 })
        manage.close()
        #expect(!model.records.isWanted(StationRadio.streamName))
        #expect(await settle { count("records.unsubscribe") == 1 && manage.radios.isEmpty })
        // Back on screen: asked again.
        manage.open()
        #expect(await settle { manage.radios.count == 2 && count("records.subscribe") == 2 })
        manage.close()
        #expect(await settle { count("records.unsubscribe") == 2 })

        // Something else asks for the list first: the page leaves that ask alone.
        model.records.want(StationRadio.streamName, backlog: StationRadio.capacity)
        #expect(await settle { count("records.subscribe") == 3 && manage.radios.count == 2 })
        manage.open()
        manage.close()
        #expect(model.records.isWanted(StationRadio.streamName))
        #expect(manage.radios.count == 2)
        #expect(count("records.unsubscribe") == 2)
        await model.disconnect()
    }

    @Test("the tab's own words are plain operator words")
    func plainWords() {
        let words = [
            RadioAtAGlance.notConnectedReason, RadioAtAGlance.noRadioReason, RadioAtAGlance.notReportedReason,
            RadioAtAGlance.waitingReason, RadioAtAGlance.staleReason, RadioAtAGlance.olderSampleRateReason,
            RadioAtAGlance.olderPaReason, RadioAtAGlance.olderAdcReason, RadioAtAGlance.olderCpuReason,
            RadioAtAGlance.absentSampleRateReason, RadioAtAGlance.absentPaReason, RadioAtAGlance.absentAdcReason,
            RadioAtAGlance.unknownAdcReason, RadioAtAGlance.absentCpuReason, RadioMenu.olderCoreRadioReason,
            RadioMenu.olderCoreAntennaReason, RadioMenu.antennaWaitingReason, RadioMenu.unknownItemReason,
            ManageRadiosModel.listWaitingReason, ManageRadiosModel.scanningNote, ManageRadiosModel.forgottenNote,
            ManageRadiosModel.unknownModelText, ManageRadiosModel.olderModelsReason,
            ManageRadiosModel.oneModelReason("Hermes Lite 2"), ManageRadiosModel.noModelsReason,
            ManageRadiosModel.modelSavedNote(radio: "Bench G2", model: "ANAN-G2"),
            ManageRadiosModel.switchingNote("Bench G2"),
        ]
        for text in words {
            let lower = text.lowercased()
            for word in ["yet", "soon", "station", "capability", "snapshot", "session", "telemetry", "\u{2014}"] {
                #expect(!lower.contains(word), "\(text)")
            }
        }
    }

    // MARK: Pictures

    @Test("pictures of the Radio tab and its pages, portrait and landscape")
    func pictures() async throws {
        let (model, flow, station) = try await connectedThroughYourCores(
            additions: [.radioTelemetry, .stationRadios, .radioModels, .accessories, .setupDescription, .setupPanels])
        try await deliverCatalogue(station, revision: 2)
        await station.deliverAccessories()
        // This fixture's Core starts without its radio: it connects one, as the board shows.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(name: "connected", value: .bool(true)), .init(name: "model", value: .utf8("ANAN-G2")),
            .init(name: "version", value: .utf8("27")),
        ])))
        await station.deliverRadioTelemetry()
        await station.setStationRadios(.bench)
        await station.deliverSetup([("hardware", Self.antennaPage)])
        try await station.deliverSetupPanels()
        #expect(await settle { model.main.catalogFeed.revision == 2 && model.mirror.currentTelemetryReceipt != nil
                && model.setupPages.page(RadioTabModel.antennaPage, in: RadioTabModel.antennaCategory) != nil })
        // A connected, idle Core signed in with this phone's key: Remove Core and the radio choice are open.
        #expect(flow.radioRemoveReason == nil)
        #expect(ManageRadiosModel(app: model).actionReason == nil)
        // The pictures ask for the list first, so each page shows it at once.
        model.records.want(StationRadio.streamName, backlog: StationRadio.capacity)
        #expect(await settle { model.records.records(StationRadio.streamName).count == 2 })
        let pages: [(String, [RadioView.Route])] = [
            ("radio-tab", []),
            ("radio-manage-radios", [.manageRadios]),
            ("radio-protocol-info", [.protocolInfo]),
            ("radio-antenna-setup", [.antennaSetup]),
        ]
        for (name, route) in pages {
            for landscape in [false, true] {
                try await shoot("\(name)-\(landscape ? "landscape" : "portrait")", landscape: landscape) {
                    RadioView(app: model, main: model.main, flow: flow, route: route)
                }
            }
        }
        await model.disconnect()

        // A Core that lists its radios but names no models: Unknown model, Change model greyed with why.
        let (unnamed, unnamedFlow, unnamedStation) = try await connectedThroughYourCores(additions: [.stationRadios])
        await unnamedStation.setStationRadios(.bench)
        unnamed.records.want(StationRadio.streamName, backlog: StationRadio.capacity)
        #expect(await settle { unnamed.records.records(StationRadio.streamName).count == 2 })
        try await shoot("radio-manage-radios-no-models-portrait", landscape: false) {
            RadioView(app: unnamed, main: unnamed.main, flow: unnamedFlow, route: [.manageRadios])
        }
        await unnamed.disconnect()

        // An older Core: the lines it does not report say so.
        let (older, olderStation) = try await connected()
        let olderFlow = Self.flow(older, olderStation)
        try await shoot("radio-tab-older-core-portrait", landscape: false) {
            RadioView(app: older, main: older.main, flow: olderFlow)
        }
        await older.disconnect()
    }

    // MARK: The fake Core

    /// A made-up Hardware category with the antenna page's id, as the Core names it, at the
    /// version the Core projects Hardware to (6).
    static let antennaPage = #"{"version":6,"category":{"id":"hardware","title":"Hardware","where":"station"},"pages":[{"id":"hardware.antennaAlex","title":"Antenna / ALEX","where":"station","sections":[{"title":"Sample antennas","controls":[{"id":"hardware.antennaAlex.sample","label":"Sample switch","tooltip":"Turns the sample on.","kind":"toggle","binding":{"setting":"SampleAntennaOn"},"applies":"live","valueEncoding":{"true":"True","false":"False"}}]}]}]}"#

    private func connected(additions: FakeStation.Additions = [], clock: (any LinkClock)? = nil) async throws
        -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "RadioTabTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform, mirrorClock: clock ?? SystemLinkClock())
        let station = try FakeStation(additions: additions)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected && model.mirror.isSnapshotComplete })
        return (model, station)
    }

    /// The app connected through Your Cores to a saved Core that takes this
    /// phone's device key, as a user connects.
    private func connectedThroughYourCores(additions: FakeStation.Additions) async throws
        -> (AppModel, ConnectionFlow, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "RadioTabTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let station = try FakeStation(fixture: "session-device-sign-in", additions: additions,
                                      stationLabel: "KG4VCF/shack")
        let stations = PairedStationStore(item: InMemorySecretItem())
        try stations.save(PairedStation(identityKey: station.identity.publicKey, label: "KG4VCF/shack",
                                        endpoints: [station.endpoint]))
        let flow = ConnectionFlow(app: model, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()), stations: stations, kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band && model.mirror.isSnapshotComplete })
        #expect(model.session?.signsWithDeviceKey == true)
        return (model, flow, station)
    }

    /// The suite's ANAN-G2 catalogue as a new revision, with its board and Radio menu changed.
    private func deliverCatalogue(_ station: FakeStation, revision: Int64, board edits: [String: Any] = [:],
                                  fixture: String = "catalog-anan-g2",
                                  radioItems change: ([[String: Any]]) -> [[String: Any]] = { $0 }) async throws {
        var object = try #require(ModesTabBindingTests.catalogueObject(fixture))
        object["radioItems"] = change(try #require(object["radioItems"] as? [[String: Any]]))
        var board = try #require(object["board"] as? [String: Any])
        board.merge(edits) { $1 }
        object["board"] = board
        let data = try JSONSerialization.data(withJSONObject: object, options: [.sortedKeys])
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(String(decoding: data, as: UTF8.self))),
            .init(ordinal: 1, name: "revision", value: .i64(revision)),
        ])))
    }

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

    private func values(_ radio: RadioTabModel, _ clock: any LinkClock) -> [String] {
        radio.glance(nowMilliseconds: clock.nowMilliseconds).map(\.reading.text)
    }

    private func reading(_ radio: RadioTabModel, _ clock: any LinkClock, _ id: String) -> RadioAtAGlance.Reading? {
        radio.glance(nowMilliseconds: clock.nowMilliseconds).first { $0.id == id }?.reading
    }

    private func shoot<Content: View>(_ name: String, landscape: Bool,
                                      @ViewBuilder content: () -> Content) async throws {
        let size = landscape ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            content()
            TabBar(selection: .constant(.radio), sideways: landscape)
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
        await ShotWait.laidOut(window)
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

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            await Task.yield()
            try? await Task.sleep(for: .milliseconds(20))
        }
        return condition()
    }
}
