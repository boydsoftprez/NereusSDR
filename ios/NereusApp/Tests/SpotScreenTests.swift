// NereusSDR for iOS: spots on the band and Spot Hub against a fake Core that runs its spot sources
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

/// R-IOS-25, D13, D32, spec section 5.1 items 10 and 11 and section 5.7
/// items 1 to 4: the real app against a fake Core that runs its spot
/// sources (``FakeStation/Additions/spots``) and reaches no cluster. The
/// app asks for the Core's spots once connected, shows them newest first
/// in the Spot List and on the band under the flags, tunes to one on a
/// tap, lists a badge's spots, runs the Core's sources and their consoles,
/// and writes the lifetime and Clear all spots to the Core. With
/// `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the screens are written there for
/// comparing with `04-spots.jpg` and `20-spot-hub.jpg`.
@Suite("Spots and Spot Hub", .serialized)
@MainActor
struct SpotScreenTests {
    /// This test's own lock, background time and idle timer.
    private let platform = TestPlatform()
    // MARK: The Core's spots

    @Test("the app asks for the Core's spots once connected and lists them newest first, in their DXCC colours")
    func spotsArriveNewestFirst() async throws {
        let (model, station) = try await connected()
        // FreeDV Reporter's stations are asked for too, on this Core; the spots with their own backlog.
        #expect(await sent(station, "records.subscribe", [.init(name: "stream", value: .utf8("spots")),
                                                          .init(name: "backlog", value: .i64(500))]))
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.spots.count == 13 })
        #expect(spots.available)
        #expect(spots.spots.first?.call == "ZL2ABC")
        #expect(spots.spots.last?.call == "N3ABC")
        let vk = try #require(spots.spots.first { $0.call == "VK2XYZ" })
        #expect(vk.mode == "SSB" && vk.source == "Cluster" && vk.spotter == "W2ABC" && vk.comment == "CQ DX, 5/9")
        #expect(vk.frequencyHz == 7_233_000 && vk.band == 3)
        #expect(spots.colour(vk) == "#FF3030")
        #expect(SpotsModel.logWords(vk.dxccPriority) == "New country")
        #expect(SpotDetailsSheet.rows(vk).map(\.name) == ["Mode", "Source", "Spotter", "Comment", "Spotted", "Log"])
        #expect(SpotDetailsSheet.rows(vk).first { $0.name == "Spotted" }?.value == "19:42:10 UTC")
        #expect(SpotDetailsSheet.megahertz(vk.frequencyHz) == "7.2330 MHz")
        // With override colours on, every spot takes the override.
        spots.changeDisplay { $0.overrideColours = true }
        #expect(spots.colour(vk) == "#FFFF00")
        await model.disconnect()
        // The session ends: the Core's spots go with it.
        #expect(await settle { spots.spots.isEmpty && !spots.available })
    }

    @Test("a tap on a spot tunes the active slice to it; a locked slice is not tuned, and says why")
    func aTapTunes() async throws {
        let (model, station) = try await connected()
        try await MainScreenShotTests.fill(station)
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.spots.count == 13 && model.main.slices.active != nil })
        let vk = try #require(spots.spots.first { $0.call == "VK2XYZ" })
        spots.showDetails(vk)
        #expect(spots.openDetails == vk)
        spots.tune(vk, onBand: true)
        #expect(spots.openDetails == nil)
        let write = await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.key == "slice:0" && write.properties.first?.name == "frequency"
            }
            return false
        }
        guard case .propertyWrite(let sent)? = write else {
            Issue.record("no frequency write")
            return
        }
        #expect(sent.properties.first?.value == .f64(7_233_000))
        // Locked, the slice stays and the details say why.
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 35, name: "locked", value: .bool(true)),
        ])))
        if await settle(seconds: 3, { model.main.slices.active?.locked == true }) {
            #expect(spots.tuneReason == SpotsModel.lockedReason)
        }
        await model.disconnect()
    }

    @Test("a +N badge lists the spots it hides, lowest first, and closes when one is tuned")
    func aBadgeListsItsSpots() async throws {
        let (model, station) = try await connected()
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.spots.count == 13 })
        let ids = spots.spots.filter { ["F5ABC", "DL1ABC", "ON4ABC"].contains($0.call) }.map(\.id)
        spots.showHidden(ids)
        #expect(spots.openHidden?.map(\.call) == ["DL1ABC", "F5ABC", "ON4ABC"])
        #expect(HiddenSpotsList.heading(3) == "3 spots at this frequency")
        #expect(HiddenSpotsList.kilohertz(7_246_200) == "7246.2 kHz")
        spots.closeBandPopups()
        #expect(spots.openHidden == nil)
        await model.disconnect()
    }

    // MARK: Spot Hub

    @Test("Spot Hub lists the Core's sources with their state, and greys those the Core does not run")
    func spotHubLines() async throws {
        let (model, station) = try await connected(additions: [.spots, .bands])
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(3)),
        ])))
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.spots.count == 13 && spots.status(.dxCluster).state == .connected
                && spots.activeBand?.label == "40m" })
        #expect(spots.listSummary == "13 on 40m \u{00B7} 13 in all \u{00B7} newest 19:43")
        #expect(spots.displaySummary == "On \u{00B7} 3 levels \u{00B7} halfway \u{00B7} 16 pt")
        #expect(spots.sourceLine(.dxCluster) == "dxc.nc7j.com:7300 \u{00B7} connected")
        #expect(spots.sourceLine(.pota) == "Polling api.pota.app")
        #expect(spots.sourceLine(.rbn) == "Off")
        #expect(spots.sourceLine(.wsjtx) == SpotsModel.listensOnEachComputerReason)
        // The suite's Core runs FreeDV Reporter and sends its state: stopped.
        #expect(spots.sourceLine(.freeDv) == "Stopped")
        #expect(spots.sourceReason(.dxCluster) == nil)
        #expect(spots.identitySummary == "Callsign not set \u{00B7} grid not set \u{00B7} used by every source")
        // The Spot List's pills and band filter.
        #expect(spots.listed.count == 13)
        spots.toggleListSource(.pota)
        #expect(spots.listed.count == 11)
        spots.toggleListSource(.pota)
        #expect(SpotsModel.pillReason(.wsjtx) != nil && SpotsModel.pillReason(.dxCluster) == nil)
        #expect(SpotListPage.detailLine(spots.spots[0]) == "19:43 UTC \u{00B7} K6ABC \u{00B7} grey line")
        await model.disconnect()
    }

    @Test("a source's page asks for its console, runs it at the Core and types into it")
    func aSourcePage() async throws {
        let (model, station) = try await connected()
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.status(.dxCluster).state == .connected })
        spots.pageOpened(.dxCluster)
        #expect(await sent(station, "records.subscribe", [.init(name: "stream", value: .utf8("spotConsole:dxCluster")),
                                                           .init(name: "backlog", value: .i64(200))]))
        #expect(await settle { spots.consoles[.dxCluster]?.count == 4 })
        spots.sendCommand(.dxCluster, "sh/dx 40")
        #expect(await sent(station, "spots.sendCommand")
                == [.init(name: "source", value: .utf8("dxCluster")), .init(name: "text", value: .utf8("sh/dx 40"))])
        #expect(await settle { spots.consoles[.dxCluster]?.last == "> sh/dx 40" })
        // POTA takes no typed commands: the phone says so and sends nothing.
        #expect(spots.commandReason(.pota) == SpotsModel.noCommandsReason)
        // The Reverse Beacon Network runs at the Core from its page.
        spots.setRunning(.rbn, true)
        #expect(await sent(station, "spots.connect") == [.init(name: "source", value: .utf8("rbn"))])
        #expect(await settle { spots.status(.rbn).state == .connected })
        // The Core's refusal shows on the page.
        station.refuseNext("spots.disconnect", reason: "The Core could not read this request.")
        spots.setRunning(.rbn, false)
        #expect(await settle { spots.notes[.rbn] == "The Core could not read this request." })
        spots.pageClosed(.dxCluster)
        #expect(await sent(station, "records.unsubscribe") == [.init(name: "stream", value: .utf8("spotConsole:dxCluster"))])
        await model.disconnect()
    }

    @Test("the lifetime and Clear all spots are the Core's; how spots look is this phone's")
    func whatLivesWhere() async throws {
        let defaults = try #require(UserDefaults(suiteName: "SpotScreenTests-\(UUID().uuidString)"))
        let (model, station) = try await connected(phone: PhoneSettings(defaults: defaults))
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.spots.count == 13 })
        // The desktop's lifetime, 30 minutes, until the Core says otherwise.
        #expect(spots.lifetimeSeconds == 1800)
        #expect(SpotsModel.lifetimeWords(SpotsModel.lifetimeSteps[spots.lifetimeStep]) == "30 mins")
        #expect(SpotsModel.lifetimeSteps.count == 45)
        #expect(SpotsModel.lifetimeWords(45) == "45 sec" && SpotsModel.lifetimeWords(3600) == "1 hr")
        #expect(SpotsModel.lifetimeWords(86_400) == "1 day")
        spots.setLifetimeStep(21)
        #expect(await settingWritten(station, "DxClusterSpotLifetimeSec") == "3600")
        spots.clearAll()
        #expect(await sent(station, "spots.clearAll") == [])
        #expect(await settle { spots.spots.isEmpty })
        // This phone's look is kept on the phone, and read back by the next model.
        spots.changeDisplay { $0.maxLevels = 5 }
        spots.toggleBandSource(.pota)
        #expect(!spots.showsOnBand(.pota))
        let again = SpotsModel(records: nil, mirror: model.mirror, commands: nil, settings: nil,
                               slices: model.main.slices, catalogFeed: model.main.catalogFeed,
                               phone: PhoneSettings(defaults: defaults))
        #expect(again.display.maxLevels == 5 && again.display.hiddenSources == ["POTA"])
        await model.disconnect()
    }

    @Test("an older Core sends no spots: the app asks for none, and the sources say why")
    func anOlderCore() async throws {
        let (model, station) = try await connected(additions: [])
        let spots = model.spots
        #expect(await settle { spots.connected })
        #expect(!spots.available)
        #expect(spots.sourceReason(.dxCluster) == SpotsModel.olderCoreReason)
        #expect(spots.listSummary == SpotsModel.olderCoreReason)
        #expect(spots.clearReason == SpotsModel.olderCoreReason)
        let asked = await station.waitForMessage(within: .seconds(1)) { AccessoryPagesTests.invoke($0)?.verb == "records.subscribe" }
        #expect(asked == nil)
        await model.disconnect()
    }

    // MARK: Task 62 Step 3: Auto mode, the Display page's colours, FreeDV Reporter's state

    @Test("on a Core that names no spot's mode, Auto mode is greyed with its reason; a tap tunes and sets no mode")
    func autoMode() async throws {
        let defaults = try #require(UserDefaults(suiteName: "SpotScreenTests-\(UUID().uuidString)"))
        let (model, station) = try await connected(phone: PhoneSettings(defaults: defaults))
        try await MainScreenShotTests.fill(station)
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.spots.count == 13 && model.main.slices.active != nil })
        // On by default, as on the desktop, and greyed: a Core at recordStreamVersion 1 names no mode on its spots.
        #expect(spots.display.autoMode)
        #expect(spots.autoModeReason == SpotsModel.noSpotModeReason)
        let vk = try #require(spots.spots.first { $0.call == "VK2XYZ" })
        let jp = try #require(spots.spots.first { $0.call == "JA1ABC" })
        for (on, spot) in [(true, vk), (false, jp)] {
            spots.changeDisplay { $0.autoMode = on }
            #expect(spots.display.autoMode == on)
            spots.tune(spot, onBand: true)
            let tuned = await station.waitForMessage(within: .seconds(30)) { message in
                if case .propertyWrite(let write) = message {
                    return write.key == "slice:0" && write.properties.contains {
                        $0.name == "frequency" && $0.value == .f64(spot.frequencyHz)
                    }
                }
                return false
            }
            // The Core takes the frequency and answers the write by its id, so the next tap goes at once.
            guard case .propertyWrite(let request)? = tuned, let writeId = request.writeId else {
                Issue.record("the tap's write did not reach the Core with a write id")
                await model.disconnect()
                return
            }
            let taken = LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(spot.frequencyHz))
            await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: writeId, results: [
                LinkMessage.PropertyResult.Result(property: "frequency", accepted: true, reason: "", value: taken),
            ])))
            await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [taken])))
        }
        // Neither tap set a mode: the Core names none for the phone to set.
        let moded = await station.waitForMessage(within: .seconds(1)) { message in
            if case .propertyWrite(let write) = message {
                return write.properties.contains { $0.name == "dspMode" }
            }
            return false
        }
        #expect(moded == nil)
        // Kept on this phone, and read back by the next model.
        let again = SpotsModel(records: nil, mirror: model.mirror, commands: nil, settings: nil,
                               slices: model.main.slices, catalogFeed: model.main.catalogFeed,
                               phone: PhoneSettings(defaults: defaults))
        #expect(!again.display.autoMode)
        await model.disconnect()
    }

    /// Three spots from a Core at `recordStreamVersion` 2: one the Core
    /// works out as CWU (4), one as LSB (0), and one it names no mode for.
    static let modeScene = FakeStation.SpotScene(spots: [
        FakeStation.SceneSpot(call: "JA1ABC", frequencyHz: 14_025_000, mode: "CW", spotter: "W3LPL",
                              timeUtc: "2026-09-26T18:24:00Z", band: 5, resolvedMode: 4),
        FakeStation.SceneSpot(call: "VK2XYZ", frequencyHz: 7_233_000, spotter: "W2ABC",
                              timeUtc: "2026-09-26T18:25:00Z", band: 3, resolvedMode: 0),
        FakeStation.SceneSpot(call: "AB0NO", frequencyHz: 500_000, mode: "", spotter: "W3LPL",
                              timeUtc: "2026-09-26T18:26:00Z", band: 11),
    ])

    @Test("on a Core that names each spot's mode, a tap also sets the slice's mode when it differs, with Auto mode on")
    func autoModeSetsTheMode() async throws {
        let (model, station) = try await connected(additions: [.spots, .spotModes])
        try await MainScreenShotTests.fill(station)
        await station.deliverSpots(Self.modeScene)
        let spots = model.spots
        #expect(await settle { spots.spots.count == 3 && model.main.slices.active?.mode == 0 })
        #expect(spots.recordVersion == 2 && spots.autoModeReason == nil && spots.display.autoMode)
        let cw = try #require(spots.spots.first { $0.call == "JA1ABC" })
        let lsb = try #require(spots.spots.first { $0.call == "VK2XYZ" })
        let none = try #require(spots.spots.first { $0.call == "AB0NO" })
        #expect(cw.resolvedMode == 4 && lsb.resolvedMode == 0 && none.resolvedMode == nil)
        // The Display page with Auto mode free to change.
        try await shoot("spot-display-auto-mode", height: 1500) {
            ToolsTab(app: model, flow: Self.flow(model, station), spots: spots,
                     route: [.spotHub, .spotHubPage(.display)])
        }
        func modeWrites() -> [LinkMessage.PropertyEntry] {
            station.messages.flatMap { message -> [LinkMessage.PropertyEntry] in
                if case .propertyWrite(let write) = message, write.key == "slice:0" {
                    return write.properties.filter { $0.name == "dspMode" }
                }
                return []
            }
        }
        // The slice is in LSB already, and the Core names no mode for the last: each only tunes.
        for spot in [lsb, none] {
            #expect(await tuneAnswered(spot, spots, station))
        }
        #expect(modeWrites().isEmpty)
        // CWU differs: the slice takes it after the frequency.
        #expect(await tuneAnswered(cw, spots, station))
        let moded = await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.properties.contains { $0.name == "dspMode" }
            }
            return false
        }
        #expect(moded != nil)
        #expect(modeWrites().map(\.value) == [.enumeration(4)])
        // The Core takes CWU: the slice's mode is the Core's again, so its
        // next change shows (the liveui rule, StationClient.cpp:1040-1068).
        if case .propertyWrite(let write)? = moded, let writeId = write.writeId,
           let entry = write.properties.first(where: { $0.name == "dspMode" }) {
            await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: write.key, writeId: writeId, results: [
                LinkMessage.PropertyResult.Result(property: "dspMode", accepted: true, reason: "", value: entry),
            ])))
        }
        // Auto mode off: a tap only tunes.
        spots.changeDisplay { $0.autoMode = false }
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 2, name: "dspMode", value: .enumeration(0)),
        ])))
        #expect(await settle { model.main.slices.active?.mode == 0 })
        #expect(await tuneAnswered(cw, spots, station))
        #expect(modeWrites().count == 1)
        // Away from the Core the switch is this phone's own, so it is free to change.
        await model.disconnect()
        #expect(await settle { !spots.connected })
        #expect(spots.autoModeReason == nil)
    }

    /// Tunes to `spot`, and answers its frequency write as the Core does, so the next tap goes at once.
    private func tuneAnswered(_ spot: SpotsModel.Spot, _ spots: SpotsModel, _ station: FakeStation) async -> Bool {
        let before = station.messages.count
        spots.tune(spot, onBand: true)
        func request() -> LinkMessage.PropertyWrite? {
            for message in station.messages.dropFirst(before) {
                if case .propertyWrite(let write) = message, write.key == "slice:0", write.properties.contains(where: {
                    $0.name == "frequency" && $0.value == .f64(spot.frequencyHz)
                }) {
                    return write
                }
            }
            return nil
        }
        guard await settle(seconds: 30, { request() != nil }), let write = request(), let writeId = write.writeId else {
            return false
        }
        let taken = LinkMessage.PropertyEntry(ordinal: 1, name: "frequency", value: .f64(spot.frequencyHz))
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "slice:0", writeId: writeId, results: [
            LinkMessage.PropertyResult.Result(property: "frequency", accepted: true, reason: "", value: taken),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [taken])))
        return true
    }

    @Test("the spot colour, the background colour and its opacity are this phone's and drive the band's labels")
    func displayColours() async throws {
        let defaults = try #require(UserDefaults(suiteName: "SpotScreenTests-\(UUID().uuidString)"))
        let (model, station) = try await connected(phone: PhoneSettings(defaults: defaults))
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.spots.count == 13 })
        let vk = try #require(spots.spots.first { $0.call == "VK2XYZ" })
        // Override colours off: the DXCC colour, whatever the spot colour.
        spots.changeDisplay { $0.overrideColour = SpotColours.hexText(Color(red: 0, green: 1, blue: 0)) }
        #expect(spots.display.overrideColour == "#00FF00")
        #expect(spots.colour(vk) == "#FF3030")
        spots.changeDisplay { $0.overrideColours = true }
        #expect(spots.colour(vk) == "#00FF00")
        // The background: black at 48 percent until changed, none while off.
        #expect(spots.display.labelBackground?.colour == "#000000")
        spots.changeDisplay { $0.backgroundColour = SpotColours.hexText(Color(red: 0x20 / 255, green: 0x30 / 255,
                                                                                blue: 0x40 / 255)) }
        spots.changeDisplay { $0.backgroundOpacity = 80 }
        #expect(spots.display.labelBackground?.colour == "#203040")
        #expect(spots.display.labelBackground?.opacity == 0.8)
        #expect(SpotColours.background(spots.display) == SpotColours.hex("#203040").opacity(0.8))
        spots.changeDisplay { $0.backgroundOpacity = 140 }
        #expect(spots.display.backgroundOpacity == 100)
        spots.changeDisplay { $0.overrideBackground = false }
        #expect(spots.display.labelBackground == nil && SpotColours.background(spots.display) == .clear)
        // Nothing goes to the Core: the desktop keeps these on each computer too.
        let written = await station.waitForMessage(within: .seconds(1)) { message in
            if case .settingsWrite = message {
                return true
            }
            return false
        }
        #expect(written == nil)
        let again = SpotsModel(records: nil, mirror: model.mirror, commands: nil, settings: nil,
                               slices: model.main.slices, catalogFeed: model.main.catalogFeed,
                               phone: PhoneSettings(defaults: defaults))
        #expect(again.display.overrideColours && again.display.overrideColour == "#00FF00")
        #expect(!again.display.overrideBackground && again.display.backgroundColour == "#203040")
        #expect(again.display.backgroundOpacity == 100)
        await model.disconnect()
    }

    @Test("FreeDV Reporter's state from the Core: connected, off with its reason, in error, hidden")
    func freeDvState() async throws {
        let (model, station) = try await connected()
        let spots = model.spots
        var scene = FakeStation.SpotScene.board
        scene.freedv = ("connected", "", false)
        await station.deliverSpots(scene)
        #expect(await settle { spots.freeDvStatus?.state == .connected })
        #expect(spots.sourceLine(.freeDv) == "Connected")
        #expect(SpotHubPage.dot(spots, .freeDv) == ConnectChrome.pillOn)
        #expect(spots.sourceReason(.freeDv) == nil)
        scene.freedv = ("connected", "", true)
        await station.deliverSpots(scene)
        #expect(await settle { spots.freeDvStatus?.hidden == true })
        #expect(spots.sourceLine(.freeDv) == "Connected \u{00B7} hidden from the dashboard")
        let reason = "Enter your callsign and grid square in Spot Hub first."
        scene.freedv = ("off", reason, false)
        await station.deliverSpots(scene)
        #expect(await settle { spots.freeDvStatus?.text == reason })
        #expect(spots.sourceLine(.freeDv) == reason)
        #expect(SpotHubPage.dot(spots, .freeDv) == ConnectChrome.pillStale)
        scene.freedv = ("error", "The server closed the connection.", false)
        await station.deliverSpots(scene)
        #expect(await settle { spots.freeDvStatus?.state == .error })
        #expect(spots.sourceLine(.freeDv) == "Error: The server closed the connection.")
        #expect(SpotHubPage.dot(spots, .freeDv) == ConnectChrome.pillOff)
        scene.freedv = ("connecting", "Lost the connection; trying again in 5 s", false)
        await station.deliverSpots(scene)
        #expect(await settle { spots.freeDvStatus?.state == .connecting })
        #expect(spots.sourceLine(.freeDv) == "Connecting")
        // Spot Hub does not start FreeDV Reporter: nothing is sent for it.
        spots.setRunning(.freeDv, true)
        let asked = await station.waitForMessage(within: .seconds(1)) {
            AccessoryPagesTests.invoke($0)?.verb == "spots.connect"
        }
        #expect(asked == nil)
        await model.disconnect()
    }

    @Test("a Core before FreeDV Reporter sends no state for it, and its row says so")
    func freeDvStateAbsent() async throws {
        let (model, station) = try await connected(without: [FakeStation.freedvCapability])
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.status(.dxCluster).state == .connected })
        #expect(spots.freeDvStatus == nil)
        #expect(spots.sourceLine(.freeDv) == SpotsModel.freeDvReason)
        #expect(spots.sourceReason(.freeDv) == SpotsModel.freeDvReason)
        #expect(SpotHubPage.dot(spots, .freeDv) == ConnectChrome.pillUnknown)
        await model.disconnect()
    }

    @Test("the Display page, FreeDV Reporter's row and the band with override colours")
    func parityShots() async throws {
        let (model, station) = try await connected(additions: [.spots, .bands])
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(3)),
        ])))
        var scene = FakeStation.SpotScene.board
        scene.freedv = ("connected", "", true)
        await station.deliverSpots(scene)
        let spots = model.spots
        #expect(await settle { spots.spots.count == 13 && spots.freeDvStatus?.hidden == true
                && model.main.slices.entries.count == 2 })
        let flow = Self.flow(model, station)
        try await shoot("spot-parity-index") {
            ToolsTab(app: model, flow: flow, spots: spots, route: [.spotHub])
        }
        spots.changeDisplay { settings in
            settings.overrideColours = true
            settings.overrideColour = "#00E5FF"
            settings.backgroundColour = "#402000"
            settings.backgroundOpacity = 80
        }
        // Tall enough for the whole page.
        try await shoot("spot-parity-display", height: 1500) {
            ToolsTab(app: model, flow: flow, spots: spots, route: [.spotHub, .spotHubPage(.display)])
        }
        let band = model.main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        try await shootBand("spot-parity-band-override", model: model, sideways: false)
        await model.disconnect()
    }

    // MARK: Pictures

    @Test("spots on the band, a badge's list and a spot's details, upright and sideways")
    func bandShots() async throws {
        let (model, station) = try await connected(additions: [.spots, .bands])
        try await MainScreenShotTests.fill(station)
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.spots.count == 13 && model.main.slices.entries.count == 2 })
        let band = model.main.band
        band.endpointId = 1
        let context = try #require(BandFlagShotTests.context())
        band.receive(.context(context))
        try await shootBand("spots-upright", model: model, sideways: false)
        // The biggest badge's spots, and VK2XYZ's details.
        let geometry = BandGeometry(centerHz: BandFlagShotTests.centre, spanHz: BandFlagShotTests.span,
                                    size: CGSize(width: 402, height: 280), dbmRange: -140 ... -40)
        let layout = SpotLayout.layout(spots: spots.bandSpots, flags: [], geometry: geometry, settings: spots.display)
        let biggest = try #require(layout.badges.max { $0.spotIds.count < $1.spotIds.count })
        spots.showHidden(biggest.spotIds)
        try await shootBand("spots-hidden-list", model: model, sideways: false)
        let vk = try #require(spots.spots.first { $0.call == "VK2XYZ" })
        spots.showDetails(vk)
        try await shootBand("spots-details", model: model, sideways: false)
        spots.closeBandPopups()
        try await shootBand("spots-sideways", model: model, sideways: true)
        await model.disconnect()
    }

    @Test("Spot Hub, the Spot List, Display and the cluster's page")
    func spotHubShots() async throws {
        let (model, station) = try await connected(additions: [.spots, .bands])
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(3)),
        ])))
        await station.deliverSpots()
        let spots = model.spots
        #expect(await settle { spots.spots.count == 13 && spots.activeBand?.label == "40m" })
        let flow = Self.flow(model, station)
        let pages: [(String, [ToolsTab.Page])] = [
            ("spothub-tools", []),
            ("spothub-index", [.spotHub]),
            ("spothub-list", [.spotHub, .spotHubPage(.list)]),
            ("spothub-display", [.spotHub, .spotHubPage(.display)]),
            ("spothub-cluster", [.spotHub, .spotHubPage(.source(.dxCluster))]),
            ("spothub-identity", [.spotHub, .spotHubPage(.identity)]),
        ]
        for (name, route) in pages {
            try await shoot(name) {
                ToolsTab(app: model, flow: flow, spots: spots, route: route)
            }
        }
        await model.disconnect()
    }

    // MARK: Inside

    private func connected(additions: FakeStation.Additions = [.spots], phone: PhoneSettings? = nil,
                           without: Set<String> = []) async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "SpotScreenTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: phone ?? PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let station = try FakeStation(additions: additions, withoutCapabilities: without)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected })
        return (model, station)
    }

    private static func flow(_ model: AppModel, _ station: FakeStation) -> ConnectionFlow {
        ConnectionFlow(app: model, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()),
            stations: PairedStationStore(item: InMemorySecretItem()), kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
    }

    /// The arguments of the next `verb` the app sent, or nil if none came.
    private func sent(_ station: FakeStation, _ verb: String) async -> [LinkMessage.PropertyEntry]? {
        await station.waitForMessage(within: .seconds(30)) { AccessoryPagesTests.invoke($0)?.verb == verb }
            .flatMap(AccessoryPagesTests.invoke)?.args
    }

    /// Whether the app sent `verb` with exactly `arguments`, waiting up to 5 s.
    private func sent(_ station: FakeStation, _ verb: String, _ arguments: [LinkMessage.PropertyEntry]) async -> Bool {
        await station.waitForMessage(within: .seconds(30)) { message in
            guard let invoke = AccessoryPagesTests.invoke(message) else {
                return false
            }
            return invoke.verb == verb && invoke.args == arguments
        } != nil
    }

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

    /// The main screen with the band fed, upright or sideways, written with `NEREUS_MAIN_SHOTS` set.
    private func shootBand(_ name: String, model: AppModel, sideways: Bool) async throws {
        let size = sideways ? CGSize(width: 874, height: 402) : CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: CGPoint(x: 0, y: sideways ? 120 : 0), size: size)
        window.windowLevel = .alert + 1
        let root = GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        if sideways {
            host.additionalSafeAreaInsets = UIEdgeInsets(top: 0, left: 62, bottom: 21, right: 62)
        }
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        let band = model.main.band
        BandFlagShotTests.feed(band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(band, in: window, after: bandDraw)
        write(name, window: window)
    }

    /// A Tools page over the tab bar on an upright phone, written with `NEREUS_MAIN_SHOTS` set.
    private func shoot<Content: View>(_ name: String, height: CGFloat = 874,
                                      @ViewBuilder content: () -> Content) async throws {
        let size = CGSize(width: 402, height: height)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        let root = VStack(spacing: 0) {
            content()
            TabBar(selection: .constant(.tools), sideways: false)
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
        write(name, window: window)
    }

    private func write(_ name: String, window: UIWindow) {
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try? data.write(to: url)
            print("Wrote \(url.path)")
        }
    }
}
