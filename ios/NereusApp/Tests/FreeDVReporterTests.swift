// NereusSDR for iOS: FreeDV Reporter against a fake Core that runs it: the list, tints, filters, status, QSY
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

/// R-IOS-26, D33, D78, spec section 5.7 items 5 to 10: the real app against
/// a fake Core that runs FreeDV Reporter (``FakeStation/Additions/spots``,
/// FakeStation+FreeDV.swift) and reaches no reporter. The app asks for the
/// Core's stations, folds each into two lines, tints and fades them on a
/// virtual clock, filters them, tunes on a tap, sends the status message
/// and a QSY request, runs the Core's reporter and "Hide my station", and
/// never shows or sends the Core's label. With `NEREUS_MAIN_SHOTS` set, the
/// screens are written there for comparing with `21-freedv-reporter.jpg`.
@Suite("FreeDV Reporter", .serialized)
@MainActor
struct FreeDVReporterTests {
    private let platform = TestPlatform()

    /// A clock the test moves.
    final class Clock {
        var now = Date(timeIntervalSince1970: 1_790_000_000)
    }

    // MARK: The list

    @Test("the app asks for the Core's stations and folds each into two lines, newest news first")
    func theList() async throws {
        let (model, station) = try await connected()
        let subscribe = await sent(station, "records.subscribe") { args in
            args.first?.value == .utf8(FakeStation.freedvStationsStream)
        }
        #expect(subscribe == [.init(name: "stream", value: .utf8("freedvStations")),
                              .init(name: "backlog", value: .i64(1000))])
        await station.deliverFreedv()
        let freedv = model.freedv
        #expect(await settle { freedv.stations.count == 7 })
        #expect(freedv.stations.map(\.callsign) == ["W1ABC", "K4XYZ", "G4ABC", "VK3XYZ", "DL1XYZ", "N0ABC", "JA1XYZ"])
        let w1 = try #require(freedv.stations.first)
        let k4 = try #require(freedv.stations.first { $0.callsign == "K4XYZ" })
        #expect(FreeDVReporterModel.stateLine(w1).lead == "Transmitting")
        let hearing = FreeDVReporterModel.stateLine(k4)
        #expect(hearing.lead == "Hearing" && hearing.callsign == "W1ABC" && hearing.rest == "RADE, SNR 6 dB")
        #expect(FreeDVReporterModel.stateLine(freedv.stations[6]).lead == "Receiving")
        // MHz by default, kHz on this phone when asked.
        #expect(freedv.frequencyText(w1.frequencyHz) == "14.2360")
        freedv.setKilohertz(true)
        #expect(freedv.frequencyText(w1.frequencyHz) == "14236.0")
        #expect(freedv.frequencyWithUnit(w1.frequencyHz) == "14236.0 kHz")
        freedv.setKilohertz(false)
        // The Core has no grid square: distance and heading stay blank.
        #expect(freedv.distanceText(w1) == nil && FreeDVReporterModel.headingText(w1) == nil)
        let rows = freedv.detailRows(w1)
        #expect(rows.map(\.name) == ["Locator", "Distance", "Heading", "Frequency", "Mode", "Status", "Message",
                                     "Last TX", "SNR", "Software", "Last update"])
        #expect(rows.first { $0.name == "Distance" }?.value == "Set your grid to see it")
        #expect(rows.first { $0.name == "Frequency" }?.value == "14.2360 MHz")
        // Once the Core knows both grid squares, it sends them; miles are this phone's.
        var news = FakeStation.FreeDVScene.board.stations[5]
        news.distanceKm = 812.4
        news.headingDeg = 44.6
        news.headingCardinal = "NE"
        news.lastUpdateUtc = "2026-09-28T19:41:00Z"
        await station.deliverFreedvStation(news)
        #expect(await settle { freedv.station(id: "s2")?.hasBearing == true })
        let known = try #require(freedv.station(id: "s2"))
        #expect(freedv.distanceText(known) == "812 km" && FreeDVReporterModel.headingText(known) == "045\u{00B0} NE")
        freedv.setMiles(true)
        #expect(freedv.distanceText(known) == "505 mi")
        #expect(FreeDVReporterModel.qrzURL("W1ABC")?.absoluteString == "https://www.qrz.com/db/W1ABC")
        #expect(FreeDVReporterModel.hamQthURL("W1ABC")?.absoluteString == "https://www.hamqth.com/W1ABC")
        await model.disconnect()
        #expect(await settle { freedv.stations.isEmpty })
    }

    @Test("rows tint rust while transmitting, slate while hearing, mauve on a new message, and fade after six seconds")
    func tints() async throws {
        let (model, station) = try await connected()
        let clock = Clock()
        let freedv = FreeDVReporterModel(records: model.records, mirror: model.mirror, commands: nil, settings: nil,
                                         spots: model.spots, slices: model.main.slices,
                                         catalogFeed: model.main.catalogFeed, phone: model.phoneSettings,
                                         now: { clock.now })
        await station.deliverFreedv()
        #expect(await settle { freedv.stations.count == 7 })
        func tint(_ call: String) -> FreeDVReporterModel.Tint? {
            freedv.stations.first { $0.callsign == call }.flatMap { freedv.tint($0) }
        }
        // The first list: what stations are doing now tints; old messages do not.
        #expect(tint("W1ABC") == .transmitting)
        #expect(tint("K4XYZ") == .hearing)
        #expect(tint("G4ABC") == nil && tint("DL1XYZ") == nil && tint("JA1XYZ") == nil)
        clock.now += 3
        // G4ABC changes its message: mauve, from when the phone saw it.
        var g4 = FakeStation.FreeDVScene.board.stations[4]
        g4.userMessage = "Listening on 20, beam north-west"
        g4.messageChangedAtMs = 1_790_000_200_000
        await station.deliverFreedvStation(g4)
        #expect(await settle { freedv.station(id: "s3")?.messageChangedAtMs == 1_790_000_200_000 })
        #expect(tint("G4ABC") == .message)
        clock.now += 2.9
        #expect(tint("W1ABC") == .transmitting && tint("K4XYZ") == .hearing)
        clock.now += 0.2
        // Six seconds after it was seen, the transmit and receive tints fade; the message's has 3 s to go.
        #expect(tint("W1ABC") == nil && tint("K4XYZ") == nil)
        #expect(tint("G4ABC") == .message)
        clock.now += 3
        #expect(tint("G4ABC") == nil)
        // A message change beats transmitting.
        var w1 = FakeStation.FreeDVScene.board.stations[6]
        w1.messageChangedAtMs = 1_790_000_300_000
        w1.userMessage = "QRV RADE on 40m"
        await station.deliverFreedvStation(w1)
        #expect(await settle { freedv.station(id: "s1")?.userMessage == "QRV RADE on 40m" })
        #expect(tint("W1ABC") == .message)
        #expect(FreeDVColours.colour(.transmitting) != FreeDVColours.colour(.hearing))
        await model.disconnect()
    }

    // MARK: Filters

    @Test("All and follow by frequency work; the band buttons and follow by band are greyed with their reason")
    func filtersWithoutBands() async throws {
        let (model, station) = try await connected(additions: [.spots, .bands])
        try await MainScreenShotTests.fill(station)
        await station.deliverFreedv()
        let freedv = model.freedv
        #expect(await settle { freedv.stations.count == 7 && model.main.slices.active != nil
                && !freedv.bandButtons.isEmpty })
        // Version 1 names no band, even though these records carry one: nothing filters by it.
        #expect(freedv.version == 1)
        #expect(freedv.bandReason == FreeDVReporterModel.noBandReason)
        #expect(freedv.follow == .band && freedv.effectiveFollow == nil)
        #expect(freedv.selectedBand == nil && freedv.listed.count == 7)
        #expect(freedv.bandButtons.first?.label == "160m" && freedv.bandButtons.contains { $0.label == "40m" })
        freedv.pickBand(3)
        #expect(freedv.bandFilter == nil && freedv.note == FreeDVReporterModel.noBandReason)
        freedv.toggleFollow(.band)
        #expect(freedv.effectiveFollow == nil)
        // Follow the radio by frequency: exactly the slice's.
        freedv.toggleFollow(.frequency)
        #expect(freedv.effectiveFollow == .frequency)
        #expect(freedv.listed.isEmpty)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 1, name: "frequency", value: .f64(14_236_000)),
        ])))
        #expect(await settle { freedv.listed.count == 4 })
        #expect(Set(freedv.listed.map(\.callsign)) == ["W1ABC", "G4ABC", "VK3XYZ", "JA1XYZ"])
        // The lit one again turns following off.
        freedv.toggleFollow(.frequency)
        #expect(freedv.effectiveFollow == nil && freedv.listed.count == 7)
        await model.disconnect()
    }

    @Test("a Core that names each station's band lights the band buttons and follows the radio's band")
    func filtersWithBands() async throws {
        let defaults = try #require(UserDefaults(suiteName: "FreeDVReporterTests-\(UUID().uuidString)"))
        let (model, station) = try await connected(additions: [.spots, .bands, .freedvBand],
                                                   phone: PhoneSettings(defaults: defaults))
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(5)),
        ])))
        await station.deliverFreedv()
        let freedv = model.freedv
        #expect(await settle { freedv.stations.count == 7 && model.main.slices.active?.band == 5 })
        #expect(freedv.version == 2 && freedv.bandReason == nil)
        // Following the radio's band (the desktop's default): 20 metres.
        #expect(freedv.effectiveFollow == .band && freedv.selectedBand == 5)
        #expect(freedv.listed.count == 4)
        // Picking a band stops following.
        freedv.pickBand(3)
        #expect(freedv.follow == nil && freedv.selectedBand == 3)
        #expect(Set(freedv.listed.map(\.callsign)) == ["DL1XYZ", "K4XYZ"])
        freedv.pickBand(nil)
        #expect(freedv.listed.count == 7)
        // Kept on this phone, and read back by the next model.
        freedv.pickBand(1)
        let again = FreeDVReporterModel(records: nil, mirror: model.mirror, commands: nil, settings: nil,
                                        spots: model.spots, slices: model.main.slices,
                                        catalogFeed: model.main.catalogFeed, phone: PhoneSettings(defaults: defaults))
        #expect(again.bandFilter == 1 && again.follow == nil)
        await model.disconnect()
    }

    @Test("on a Core that names bands, a station whose frequency is not known is listed under All only")
    func noBandListsUnderAllOnly() async throws {
        let (model, station) = try await connected(additions: [.spots, .bands, .freedvBand])
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(5)),
        ])))
        // The suite's band batch: 20 m, 2 m (GEN, 11, which no band button names) and no frequency.
        let time = "2026-09-28T18:00:00Z"
        await station.deliverFreedv(FakeStation.FreeDVScene(stations: [
            FakeStation.SceneStation(id: "sid-w1aw", callsign: "W1AW", gridSquare: "FN31", frequencyHz: 14_236_000,
                                     lastUpdateUtc: time, band: 5),
            FakeStation.SceneStation(id: "sid-k6aq", callsign: "K6AQ", gridSquare: "CM97", frequencyHz: 144_500_000,
                                     lastUpdateUtc: time, band: 11),
            FakeStation.SceneStation(id: "sid-g4abc", callsign: "G4ABC", gridSquare: "IO91", frequencyHz: 0,
                                     lastUpdateUtc: time),
        ]))
        let freedv = model.freedv
        #expect(await settle { freedv.stations.count == 3 && model.main.slices.active?.band == 5 })
        #expect(freedv.version == 2 && freedv.bandReason == nil)
        #expect(freedv.stations.first { $0.callsign == "G4ABC" }?.band == nil)
        // Following the radio's band: 20 m only.
        #expect(freedv.selectedBand == 5 && freedv.listed.map(\.callsign) == ["W1AW"])
        // No band button lists the station with no band, nor the 2 m one.
        #expect(!freedv.bandButtons.isEmpty && !freedv.bandButtons.contains { $0.id == 11 })
        for band in freedv.bandButtons {
            freedv.pickBand(band.id)
            #expect(!freedv.listed.contains { $0.callsign == "G4ABC" || $0.callsign == "K6AQ" }, "\(band.label)")
        }
        // All lists every station.
        freedv.pickBand(nil)
        #expect(Set(freedv.listed.map(\.callsign)) == ["W1AW", "K6AQ", "G4ABC"])
        await model.disconnect()
    }

    @Test("on a Core with 2 m, a 2 m station is band 27: its own button, followed on 2 m and never under GEN")
    func twoMetresIsItsOwnBand() async throws {
        let (model, station) = try await connected(additions: [.spots, .bands, .freedvBand, .band2m])
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(27)),
        ])))
        let time = "2026-09-28T18:00:00Z"
        await station.deliverFreedv(FakeStation.FreeDVScene(stations: [
            FakeStation.SceneStation(id: "sid-w1aw", callsign: "W1AW", gridSquare: "FN31", frequencyHz: 14_236_000,
                                     lastUpdateUtc: time, band: 5),
            FakeStation.SceneStation(id: "sid-k6aq", callsign: "K6AQ", gridSquare: "CM97", frequencyHz: 144_500_000,
                                     lastUpdateUtc: time, band: 27),
        ]))
        let freedv = model.freedv
        #expect(await settle { freedv.stations.count == 2 && model.main.slices.active?.band == 27 })
        #expect(freedv.version == 2 && freedv.bandReason == nil)
        // The band buttons are the Core's grid: 2 m after 6 m.
        let labels = freedv.bandButtons.map(\.label)
        #expect(labels.firstIndex(of: "2m") == labels.firstIndex(of: "6m").map { $0 + 1 })
        // Following the radio's band: 2 m, and only the 2 m station.
        #expect(freedv.selectedBand == 27 && freedv.listed.map(\.callsign) == ["K6AQ"])
        freedv.pickBand(5)
        #expect(freedv.listed.map(\.callsign) == ["W1AW"])
        freedv.pickBand(27)
        #expect(freedv.listed.map(\.callsign) == ["K6AQ"])
        await model.disconnect()
    }

    // MARK: Tuning, status, QSY

    @Test("a tap tunes the active slice to the station; one with no frequency says why")
    func aTapTunes() async throws {
        let (model, station) = try await connected()
        try await MainScreenShotTests.fill(station)
        await station.deliverFreedv()
        let freedv = model.freedv
        #expect(await settle { freedv.stations.count == 7 && model.main.slices.active != nil })
        let k4 = try #require(freedv.stations.first { $0.callsign == "K4XYZ" })
        freedv.showDetails(k4)
        freedv.tune(k4)
        #expect(freedv.openDetails == nil)
        #expect(await frequencyWritten(station, 7_177_000))
        var silent = k4
        silent.frequencyHz = 0
        #expect(freedv.tuneReason(silent) == FreeDVReporterModel.noFrequencyReason)
        await model.disconnect()
    }

    @Test("the status message goes to the Core, saved messages are the Core's, ten newest first")
    func statusMessage() async throws {
        let (model, station) = try await connected()
        await station.deliverFreedv()
        await station.deliver(setting(FreeDVReporterModel.messageKey, "QRV RADE on 40m"))
        await station.deliver(setting(FreeDVReporterModel.savedMessagesKey, "QRV RADE on 40m\nSked 7.177 at 20Z"))
        let freedv = model.freedv
        #expect(await settle { freedv.savedMessages.count == 2 })
        #expect(freedv.statusText == "QRV RADE on 40m")
        freedv.statusDraft = "Listening on 20"
        freedv.sendMessage(freedv.statusText)
        #expect(await sent(station, "freedv.setMessage") == [.init(name: "text", value: .utf8("Listening on 20"))])
        #expect(await settle { station.freedvMessage == "Listening on 20" })
        freedv.saveMessage("Listening on 20")
        #expect(await settingWritten(station, FreeDVReporterModel.savedMessagesKey)
                == "Listening on 20\nQRV RADE on 40m\nSked 7.177 at 20Z")
        // The Core echoes the save: another device's list that follows is
        // not held back by a write still waiting (the liveui rule,
        // StationClient.cpp:1040-1068).
        await echoLastWrite(station, FreeDVReporterModel.savedMessagesKey)
        // Saving one already there moves it to the top; the list keeps ten.
        await station.deliver(setting(FreeDVReporterModel.savedMessagesKey,
                                      (1...10).map { "Message \($0)" }.joined(separator: "\n")))
        #expect(await settle { freedv.savedMessages.count == 10 })
        freedv.saveMessage("New one")
        let written = await settingWritten(station, FreeDVReporterModel.savedMessagesKey, not: "Listening on 20")
        #expect(written?.split(separator: "\n").count == 10)
        #expect(written?.hasPrefix("New one\nMessage 1\n") == true && written?.hasSuffix("Message 9") == true)
        // Clear sends an empty message.
        freedv.sendMessage("")
        #expect(await settle { station.freedvMessage.isEmpty })
        await model.disconnect()
    }

    @Test("MY STATUS follows the Core's message once the typed one is sent, changed elsewhere, or the editor closes")
    func statusFollowsTheCore() async throws {
        let (model, station) = try await connected()
        await station.deliverFreedv()
        await station.deliver(setting(FreeDVReporterModel.messageKey, "QRV RADE on 40m"))
        let freedv = model.freedv
        #expect(await settle { freedv.statusText == "QRV RADE on 40m" })
        // Typed and sent: once the Core has it, another device's message shows.
        freedv.editingMessage = true
        freedv.statusDraft = "Listening on 20"
        freedv.sendMessage(freedv.statusText)
        #expect(await sent(station, "freedv.setMessage") == [.init(name: "text", value: .utf8("Listening on 20"))])
        await station.deliver(setting(FreeDVReporterModel.messageKey, "Sked 7.177 at 20Z"))
        #expect(await settle { freedv.message == "Sked 7.177 at 20Z" })
        #expect(freedv.statusText == "Sked 7.177 at 20Z")
        // Typed and never sent: Done drops it.
        freedv.statusDraft = "Not sent"
        #expect(freedv.statusText == "Not sent")
        freedv.editingMessage = false
        #expect(freedv.statusDraft == nil)
        #expect(freedv.statusText == "Sked 7.177 at 20Z")
        await model.disconnect()
    }

    @Test("Send from the editor keeps the typed message in MY STATUS until the Core answers; a refusal drops it")
    func sendKeepsTheDraftUntilTheCoreAnswers() async throws {
        let (model, station) = try await connected()
        await station.deliverFreedv()
        await station.deliver(setting(FreeDVReporterModel.messageKey, "QRV RADE on 40m"))
        let freedv = model.freedv
        #expect(await settle { freedv.statusText == "QRV RADE on 40m" })
        let window = try BandFlagShotTests.window(size: CGSize(width: 402, height: 900))
        let host = UIHostingController(rootView: StatusMessageEditor(freedv: freedv).preferredColorScheme(.dark))
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
        func pressSend() async throws {
            await ShotWait.laidOut(window)
            let send = try #require(AccessoryPagesTests.elements(labelled: "Send", in: window).first, "no Send button")
            #expect(send.accessibilityActivate())
        }

        // Accepted: Send closes the editor, and the bar keeps the message
        // sent until the Core's own message changes to it.
        freedv.editingMessage = true
        freedv.statusDraft = "Listening on 20"
        try await pressSend()
        #expect(!freedv.editingMessage)
        #expect(freedv.statusText == "Listening on 20", "closing on Send keeps the message in flight")
        #expect(await sent(station, "freedv.setMessage") == [.init(name: "text", value: .utf8("Listening on 20"))])
        try await LinkBarrier.roundTrip(model.commands)
        #expect(freedv.statusText == "Listening on 20", "accepted, and the Core's message has not changed")
        await station.deliver(setting(FreeDVReporterModel.messageKey, "Listening on 20"))
        #expect(await settle { freedv.message == "Listening on 20" })
        #expect(freedv.statusDraft == nil)
        #expect(freedv.statusText == "Listening on 20")

        // Refused: once the Core says no, the bar shows the Core's message and the refusal.
        station.refuseNext(FreeDVReporterModel.setMessageVerb, reason: "Not now")
        freedv.editingMessage = true
        freedv.statusDraft = "Sked 7.177 at 20Z"
        try await pressSend()
        #expect(!freedv.editingMessage)
        #expect(freedv.statusText == "Sked 7.177 at 20Z", "closing on Send keeps the message in flight")
        #expect(await settle { freedv.note == "Not now" })
        #expect(freedv.statusDraft == nil)
        #expect(freedv.statusText == "Listening on 20")
        await model.disconnect()
    }

    @Test("Ask to QSY sends the request and tunes this radio there too; the Core's refusals show")
    func askToQsy() async throws {
        let (model, station) = try await connected()
        try await MainScreenShotTests.fill(station)
        await station.deliverFreedv()
        let freedv = model.freedv
        #expect(await settle { freedv.stations.count == 7 && freedv.status?.state == .connected
                && model.main.slices.active != nil })
        #expect(freedv.qsyReason == nil && freedv.sliceLetter == "A")
        // The three frequencies.
        #expect(AskToQsySheet.frequency(.slice, slice: 7_236_400, theirs: 14_236_000, typed: "") == 7_236_400)
        #expect(AskToQsySheet.frequency(.theirs, slice: nil, theirs: 14_236_000, typed: "") == 14_236_000)
        #expect(AskToQsySheet.frequency(.typed, slice: nil, theirs: 0, typed: "7.2364") == 7_236_400)
        #expect(AskToQsySheet.frequency(.typed, slice: nil, theirs: 0, typed: "7,177") == 7_177_000)
        #expect(AskToQsySheet.frequency(.typed, slice: nil, theirs: 0, typed: "seven") == nil)
        #expect(AskToQsySheet.frequency(.theirs, slice: nil, theirs: 0, typed: "") == nil)
        #expect(AskToQsySheet.megahertz(7_236_400) == "7.236400 MHz")
        let w1 = try #require(freedv.stations.first { $0.callsign == "W1ABC" })
        freedv.askToQsy(w1)
        #expect(freedv.openQsy == w1)
        freedv.sendQsy(w1, hz: 7_177_000)
        #expect(await sent(station, "freedv.sendQsy") == [.init(name: "callsign", value: .utf8("W1ABC")),
                                                          .init(name: "frequencyHz", value: .i64(7_177_000))])
        #expect(await frequencyWritten(station, 7_177_000))
        #expect(await settle { freedv.qsySentTo == "W1ABC" })
        #expect(station.freedvQsyRequests.first?.frequencyHz == 7_177_000)
        // The Core's refusal shows.
        station.refuseNext("freedv.sendQsy", reason: "W1ABC is not on FreeDV Reporter now.")
        freedv.sendQsy(w1, hz: 7_177_000)
        #expect(await settle { freedv.note == "W1ABC is not on FreeDV Reporter now." })
        // With the reporter stopped, Send QSY is greyed with the reason, and nothing is sent.
        var scene = FakeStation.FreeDVScene.board
        scene.state = "off"
        await station.deliverFreedv(scene)
        #expect(await settle { freedv.status?.state == .off })
        #expect(freedv.qsyReason == FreeDVReporterModel.reporterOffReason)
        freedv.sendQsy(w1, hz: 7_177_000)
        #expect(freedv.note == FreeDVReporterModel.reporterOffReason)
        await model.disconnect()
    }

    // MARK: The Core's reporter

    @Test("the FreeDV page runs the Core's reporter, hides the station, shows its console; units stay on the phone")
    func theSourcePage() async throws {
        let (model, station) = try await connected()
        await station.deliverFreedv()
        let freedv = model.freedv
        #expect(await settle { freedv.status?.state == .connected && freedv.stations.count == 7 })
        #expect(FreeDVSourcePage.headerWords(freedv) == ("Connected", "qso.freedv.org \u{00B7} 7 stations on the air"))
        freedv.setRunning(false)
        #expect(await sent(station, "spots.disconnect") == [.init(name: "source", value: .utf8("freedvReporter"))])
        #expect(await settle { freedv.status?.state == .off && freedv.stations.isEmpty })
        #expect(FreeDVSourcePage.headerWords(freedv).title == "Stopped")
        #expect(FreeDVReporterPage.bannerWords(freedv) != nil)
        freedv.setRunning(true)
        #expect(await sent(station, "spots.connect") == [.init(name: "source", value: .utf8("freedvReporter"))])
        #expect(await settle { freedv.status?.state == .connected })
        #expect(FreeDVReporterPage.bannerWords(freedv) == nil)
        freedv.setHidden(true)
        #expect(await sent(station, "freedv.setHidden") == [.init(name: "on", value: .bool(true))])
        #expect(await settle { freedv.status?.hidden == true })
        #expect(model.spots.sourceLine(.freeDv) == "Connected \u{00B7} hidden from the dashboard")
        freedv.consoleOpened()
        #expect(await sent(station, "records.subscribe") { $0.first?.value == .utf8("spotConsole:freedvReporter") }
                == [.init(name: "stream", value: .utf8("spotConsole:freedvReporter")),
                    .init(name: "backlog", value: .i64(200))])
        #expect(await settle { freedv.console.count == 3 })
        freedv.consoleClosed()
        #expect(await sent(station, "records.unsubscribe")
                == [.init(name: "stream", value: .utf8("spotConsole:freedvReporter"))])
        freedv.setAutoStart(true)
        #expect(await settingWritten(station, FreeDVReporterModel.autoStartKey) == "True")
        // Miles and kHz are this phone's: nothing more goes to the Core.
        freedv.setMiles(true)
        freedv.setKilohertz(true)
        let autoStartKey = FreeDVReporterModel.autoStartKey
        let more = await station.waitForMessage(within: .seconds(1)) { message in
            if case .settingsWrite(let write) = message {
                return write.key != autoStartKey
            }
            return false
        }
        #expect(more == nil)
        await model.disconnect()
    }

    @Test("FreeDV Reporter lists and sends your callsign, never the Core's label")
    func callsignNotLabel() async throws {
        let (model, station) = try await connected()
        try await MainScreenShotTests.fill(station)
        await station.deliverFreedv()
        let freedv = model.freedv
        #expect(await settle { freedv.stations.count == 7 && model.main.coreName != nil })
        let label = try #require(model.main.coreName)
        // No callsign set: none, and never the label in its place.
        #expect(freedv.callsign.isEmpty)
        await station.deliver(setting("User/Callsign", "KG4VCF"))
        #expect(await settle { freedv.callsign == "KG4VCF" })
        await station.deliver(setting("FreeDvReporter/Callsign", "KG4VCF/P"))
        #expect(await settle { freedv.callsign == "KG4VCF/P" })
        #expect(freedv.callsign != label)
        freedv.sendMessage("QRV")
        let w1 = try #require(freedv.stations.first { $0.callsign == "W1ABC" })
        freedv.sendQsy(w1, hz: 14_236_000)
        freedv.setHidden(true)
        #expect(await settle { station.freedvQsyRequests.count == 1 })
        // Nothing the app sent for FreeDV Reporter carries the label.
        let carried = station.messages.contains { message in
            guard let invoke = AccessoryPagesTests.invoke(message), invoke.verb.hasPrefix("freedv.") else {
                return false
            }
            return invoke.args.contains { entry in
                if case .utf8(let text) = entry.value {
                    return text.contains(label)
                }
                return false
            }
        }
        #expect(!carried)
        await model.disconnect()
    }

    @Test("a Core before FreeDV Reporter: nothing is asked for, and every entry says why")
    func anOlderCore() async throws {
        let (model, station) = try await connected(without: [FakeStation.freedvCapability])
        let freedv = model.freedv
        #expect(await settle { freedv.connected })
        #expect(!freedv.runsReporter)
        #expect(freedv.coreReason == FreeDVReporterModel.olderCoreReason)
        #expect(FreeDVReporterPage.bannerWords(freedv) == FreeDVReporterModel.olderCoreReason)
        #expect(FreeDVSourcePage.headerWords(freedv).detail == FreeDVReporterModel.olderCoreReason)
        freedv.setRunning(true)
        #expect(freedv.sourceNote == FreeDVReporterModel.olderCoreReason)
        freedv.sendMessage("QRV")
        #expect(freedv.note == FreeDVReporterModel.olderCoreReason)
        let asked = await station.waitForMessage(within: .seconds(1)) { message in
            guard let invoke = AccessoryPagesTests.invoke(message) else {
                return false
            }
            return invoke.verb.hasPrefix("freedv.") || invoke.args.contains { $0.value == .utf8("freedvStations") }
                || invoke.args.contains { $0.value == .utf8("freedvReporter") }
        }
        #expect(asked == nil)
        await model.disconnect()
    }

    // MARK: Pictures

    @Test("the list with its tints, a station's details, Ask to QSY, the status editor, and FreeDV in Spot Hub")
    func shots() async throws {
        let (model, station) = try await connected(additions: [.spots, .bands])
        try await MainScreenShotTests.fill(station)
        await station.deliver(.delta(LinkMessage.Delta(key: "slice:0", properties: [
            .init(ordinal: 14, name: "band", value: .enumeration(3)),
        ])))
        await station.deliver(setting("User/Callsign", "KG4VCF"))
        await station.deliver(setting(FreeDVReporterModel.messageKey, "QRV RADE on 40m"))
        await station.deliver(setting(FreeDVReporterModel.savedMessagesKey,
                                      "QRV RADE on 40m\nListening on 20, beam north-west\nSked 7.177 at 20Z"))
        let clock = Clock()
        let freedv = FreeDVReporterModel(records: model.records, mirror: model.mirror, commands: model.commands,
                                         settings: model.settings, spots: model.spots, slices: model.main.slices,
                                         catalogFeed: model.main.catalogFeed, phone: model.phoneSettings,
                                         now: { clock.now })
        // The picture's clock: its newest news a moment ago, and every change seen just now.
        clock.now = try #require(FreeDVStation.time("2026-09-28T19:40:20Z"))
        await station.deliverFreedv()
        #expect(await settle { freedv.stations.count == 7 && freedv.savedMessages.count == 3 })
        var g4 = FakeStation.FreeDVScene.board.stations[4]
        g4.messageChangedAtMs = 1_790_000_200_000
        await station.deliverFreedvStation(g4)
        #expect(await settle { freedv.station(id: "s3")?.messageChangedAtMs == 1_790_000_200_000 })
        #expect(freedv.tint(try #require(freedv.station(id: "s3"))) == .message)
        let flow = Self.flow(model, station)
        try await shoot("freedv-reporter-list") { reporter(freedv) }
        let w1 = try #require(freedv.stations.first { $0.callsign == "W1ABC" })
        freedv.showDetails(w1)
        try await shoot("freedv-reporter-details") { reporter(freedv) }
        freedv.askToQsy(w1)
        try await shoot("freedv-reporter-qsy") { reporter(freedv) }
        freedv.openQsy = nil
        freedv.editingMessage = true
        try await shoot("freedv-reporter-status") { reporter(freedv) }
        freedv.editingMessage = false
        try await shoot("freedv-reporter-spothub") {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [.spotHub])
        }
        // The console arrives once asked for, as the page asks when it opens.
        model.freedv.consoleOpened()
        #expect(await settle { model.freedv.console.count == 3 })
        try await shoot("freedv-reporter-source", height: 1100) {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [.spotHub, .spotHubPage(.source(.freeDv))])
        }
        try await shoot("freedv-reporter-tools") {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [])
        }
        try await shoot("freedv-reporter-page") {
            ToolsTab(app: model, flow: flow, spots: model.spots, route: [.freedvReporter])
        }
        await model.disconnect()
    }

    // MARK: Inside

    /// The page as the Tools tab shows it, with a test's own model.
    private func reporter(_ freedv: FreeDVReporterModel) -> some View {
        VStack(spacing: 0) {
            ConnectChrome.NavBar(title: "FreeDV Reporter", back: "Tools") {
                Link("Website", destination: FreeDVReporterModel.website)
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(ChromeColours.accent)
            }
            FreeDVReporterPage(freedv: freedv)
        }
        .background(ChromeColours.page)
    }

    private func connected(additions: FakeStation.Additions = [.spots], phone: PhoneSettings? = nil,
                           without: Set<String> = []) async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "FreeDVReporterTests-\(UUID().uuidString)"))
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

    private func setting(_ key: String, _ value: String) -> LinkMessage {
        .settingsValue(LinkMessage.SettingsValue(key: key, origin: "", properties: [.init(name: key, value: .utf8(value))]))
    }

    /// The arguments of the next `verb` the app sent (that `matching` takes), or nil if none came.
    private func sent(_ station: FakeStation, _ verb: String,
                      matching: @escaping @Sendable ([LinkMessage.PropertyEntry]) -> Bool = { _ in true }) async
        -> [LinkMessage.PropertyEntry]? {
        await station.waitForMessage(within: .seconds(30)) { message in
            guard let invoke = AccessoryPagesTests.invoke(message) else {
                return false
            }
            return invoke.verb == verb && matching(invoke.args)
        }.flatMap(AccessoryPagesTests.invoke)?.args
    }

    /// The Core's echo of the app's last `settings.write` of `key`, as it answers one.
    private func echoLastWrite(_ station: FakeStation, _ key: String) async {
        let last = station.messages.reversed().compactMap { message -> LinkMessage.SettingsWrite? in
            if case .settingsWrite(let write) = message, write.key == key {
                return write
            }
            return nil
        }.first
        guard let last else {
            Issue.record("no settings.write of \(key) reached the Core")
            return
        }
        await station.deliver(.settingsValue(LinkMessage.SettingsValue(key: key, origin: last.origin,
                                                                       properties: last.properties)))
    }

    private func settingWritten(_ station: FakeStation, _ key: String, not earlier: String? = nil) async -> String? {
        let message = await station.waitForMessage(within: .seconds(30)) { message in
            guard case .settingsWrite(let write) = message, write.key == key else {
                return false
            }
            if let earlier, case .utf8(let value)? = write.properties.first?.value {
                return !value.hasPrefix(earlier)
            }
            return true
        }
        guard case .settingsWrite(let write)? = message, case .utf8(let value)? = write.properties.first?.value else {
            return nil
        }
        return value
    }

    private func frequencyWritten(_ station: FakeStation, _ hz: Double) async -> Bool {
        await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.key == "slice:0" && write.properties.contains {
                    $0.name == "frequency" && $0.value == .f64(hz)
                }
            }
            return false
        } != nil
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
