// NereusSDR for iOS: the Setup tab's tree and pages: their order and marks, what each control writes, and pictures
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

/// R-IOS-18, R-IOS-08, R-IOS-17, R-IOS-12, R-IOS-21 (Task 58, step 2): the
/// Setup tree in the spec's order with its marks and only the pages that
/// exist on the phone; Devices against a fake Core that shares itself with
/// three devices and has one more paired (picture 22's cast); the transmit
/// time-out written to the Core; Navigation's haptic switches reaching the
/// dial; Setup's Display page changing this pan's settings. With
/// `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), each page is written there as a PNG.
@Suite("Setup", .serialized)
@MainActor
struct SetupRenderingTests {
    static let macBookId = "macbook-device-id"
    static let iPadId = "ipad-device-id"
    static let thinkPadId = "thinkpad-device-id"

    // MARK: The tree

    @Test("Devices first, then the desktop's categories in order, each marked; categories with no page are left out")
    func tree() {
        let categories = SetupTree.categories
        #expect(categories.map(\.title) == ["Devices", "General", "Audio", "Display", "Transmit", "CAT & Network",
                                            "Diagnostics", "About this app"])
        #expect(categories.map(\.tag) == [.core, .both, .both, .both, .both, .both, .both, .thisPhone])
        #expect(categories.map(\.summary) == ["Paired phones and computers \u{00B7} Add a device",
                                              "Navigation \u{00B7} Battery and sessions", "On this phone",
                                              "On this phone", "PTT buttons", "Data use", "Logs",
                                              "Build · Credits · Licenses"])
        // Nothing the phone leaves off (spec section 5.2 item 7), and no page the Core would describe.
        let titles = categories.flatMap { $0.pages.map(\.title) }
        for gone in ["Keyboard", "Skins", "Collapsible Display", "Remote Access", "Hardware Config", "PA Gain",
                     "Colors & Theme"] {
            #expect(!titles.contains(gone))
        }
        #expect(Set(categories.flatMap { $0.pages.compactMap(\.page) }) == Set(SetupTree.Page.allCases))
        #expect(SetupTree.route(to: .devices) == [.page(.devices)])
        #expect(SetupTree.route(to: .displayOnThisPhone) == [.category("Display"), .page(.displayOnThisPhone)])
        #expect(SetupTree.route(to: .pttButtons) == [.category("Transmit"), .page(.pttButtons)])
        #expect(SetupTab.note(coreName: "KG4VCF/shack")
            == "Core settings are shared by every device paired with the Core, KG4VCF/shack. Settings marked This phone stay on this phone.")
    }

    @Test("More display options in Setup opens Display's page through its category")
    func moreDisplayOptions() {
        let router = SetupRouter()
        router.open(.displayOnThisPhone)
        #expect(router.path == [.category("Display"), .page(.displayOnThisPhone)])
    }

    @Test("no greyed row promises what's to come, and every reason is plain")
    func plainReasons() {
        let texts = [PttButtonsPage.buttonsReason, PttButtonsPage.lockedReason,
                     DataModeText.cellularNote(.balanced), DataModeText.monthWarning("5.02 GB"),
                     SessionController.awayText(from: Date(), to: Date(), locked: true),
                     SessionController.awayText(from: Date(), to: Date(), locked: false),
                     BatteryAndSessionsPage.sleepFooter(Date()),
                     AudioQualityModel.olderCoreReason, AudioQualityModel.notOfferedReason,
                     AudioQualityModel.checkingReason, AudioQualityModel.cellularLosslessWarning,
                     AudioQualityModel.coreCannotSendText, AudioQualityModel.coreNotAllowedText,
                     AudioQualityModel.connectionUnavailableText, AudioQualityModel.networkTooSlowText,
                     AudioQualityModel.footer, DevicesModel.hostsCoreReason,
                     DevicesModel.tokenReason, DataUsePage.footnote, DataUsePage.losslessOnCellular,
                     BatteryAndSessionsPage.soundOnlyDetail, DisplayOnThisPhonePage.calibrationNote,
                     DbmScaleArrows.raiseLabel, DbmScaleArrows.lowerLabel]
        for text in texts {
            let words = text.lowercased().split { !$0.isLetter }
            for promise in ["yet", "soon", "later", "coming", "build", "bench"] {
                #expect(!words.contains(Substring(promise)), "\(text)")
            }
            #expect(!words.contains("station"), "\(text)")
            #expect(!text.contains("\u{2014}"), "\(text)")
        }
        #expect(PttButtonsPage.buttonsReason
            == "Only the PTT on the screen keys the radio from this phone. These buttons don't key it.")
        #expect(PttButtonsPage.lockedReason == "A locked phone can't be keyed. Locking the phone ends a transmission.")
    }

    // MARK: Navigation

    @Test("the two haptic switches start on, are kept on this phone and reach the dial")
    func hapticSwitches() async throws {
        let (settings, suite) = try Self.phoneSettings()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        #expect(settings.dialDetentTicks)
        #expect(settings.dialKilohertzBumps)
        let model = AppModel(phoneSettings: settings)
        #expect(model.main.tuning.dialHaptics == (true, true))
        settings.dialDetentTicks = false
        #expect(await settle { model.main.tuning.dialHaptics == (false, true) })
        settings.dialKilohertzBumps = false
        #expect(await settle { model.main.tuning.dialHaptics == (false, false) })
        #expect(!PhoneSettings(defaults: try #require(UserDefaults(suiteName: suite))).dialDetentTicks)

        // Off, a detent plays no tick and a kilohertz no bump.
        let recorder = DialTests.Recorder()
        let haptics = DialHaptics(generator: recorder)
        haptics.ticksOnDetents = false
        haptics.play([.detent(7_236_100), .wholeKilohertz(7_237_000)])
        #expect(recorder.impacts == [.rigid])
        haptics.bumpsOnKilohertz = false
        haptics.ticksOnDetents = true
        haptics.play([.detent(7_236_100), .wholeKilohertz(7_237_000)])
        #expect(recorder.impacts == [.rigid, .light])
    }

    @Test("Touch's choices are kept on this phone, and the double tap reads in Setup's words")
    func touch() throws {
        let (settings, suite) = try Self.phoneSettings()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        #expect(settings.dragToTune && settings.tapToTune && settings.pinchToZoom && !settings.snapTapToStep)
        settings.pinchToZoom = false
        settings.doubleTapAction = .center
        let again = PhoneSettings(defaults: try #require(UserDefaults(suiteName: suite)))
        #expect(!again.pinchToZoom)
        #expect(again.doubleTapAction == .center)
        #expect(TuneGestures.DoubleTapAction.allCases.map(NavigationPage.title) == ["Tune", "Center", "None"])
    }

    // MARK: Display

    @Test("Setup's Display page changes this pan's settings on this phone, within the desktop's ranges")
    func displayPage() async throws {
        let (settings, suite) = try Self.phoneSettings()
        defer { UserDefaults.standard.removePersistentDomain(forName: suite) }
        let defaults = try #require(UserDefaults(suiteName: suite))
        let model = AppModel(phoneSettings: settings, displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let page = DisplayOnThisPhonePage(main: model.main)
        let band = model.main.band
        page.setHighLevel(-50)
        #expect(band.settings.waterfallHighDbm == -50)
        page.setLowLevel(-20)
        #expect(band.settings.waterfallLowDbm == -51, "kept below the high level")
        page.setBottom(band.settings.scaleTopDbm + 10)
        #expect(band.settings.scaleBottomDbm == band.settings.scaleTopDbm - 1)
        // Without the Core's extras only Manual can be chosen.
        page.setLevelMode(.noiseFloorAgc)
        #expect(band.settings.waterfallLevelMode == .clarity)
        page.setLevelMode(.manual)
        #expect(band.settings.waterfallLevelMode == .manual)
        page.change { $0.peakBlobCount = 7 }
        #expect(BandDisplaySettingsStore(defaults: defaults).settings(forPan: BandSubscriber.panId).peakBlobCount == 7)
        // Task 54e: the desktop's gradient, peak hold's fill, the split, gain and black.
        page.change { $0.traceGradient = true }
        page.change { $0.activePeakHoldFill = true }
        model.main.display.setSpectrumHeight(55)
        model.main.display.setColorGain(30)
        model.main.display.setBlackLevel(110)
        let kept = BandDisplaySettingsStore(defaults: defaults).settings(forPan: BandSubscriber.panId)
        #expect(kept.traceGradient && kept.activePeakHoldFill)
        #expect(kept.spectrumSharePercent == 55)
        #expect(kept.waterfallColorGain == 30 && kept.waterfallBlackLevel == 110)
        #expect(DisplayOnThisPhonePage.spectrumHeightRange == 20...80)
        #expect(DisplayOnThisPhonePage.blackLevelRange == 0...125)
        #expect(DisplayOnThisPhonePage.hex(Color(red: 1, green: 0, blue: 0.5)) == "#FF0080")
    }

    // MARK: Devices

    @Test("Devices lists who is connected with their slices, bands and TX, then who is only paired")
    func devicesLists() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let devices = rig.devices
        #expect(await settle { devices.connected.count == 3 && devices.paired.count == 1 })
        #expect(devices.coreName == "KG4VCF/shack")
        #expect(devices.listsConnected && devices.administers && devices.pairs)
        let phone = devices.connected[0]
        #expect(phone.isThisPhone && phone.revoke == .none)
        #expect(phone.slices == [DevicesModel.SliceTag(letter: "A", colour: phone.slices[0].colour, band: "40 m")])
        #expect(phone.line == "this phone")
        let macBook = devices.connected[1]
        #expect(macBook.name == "MacBook Pro" && macBook.holdsTransmit && macBook.revoke == .available)
        #expect(macBook.slices.map(\.letter) == ["B", "D"])
        #expect(macBook.slices.map(\.band) == ["40 m", "17 m"])
        #expect(macBook.line == "2 hours")
        #expect(devices.connected[2].line == "40 minutes")
        #expect(devices.connected[2].slices.map(\.band) == ["20 m"])
        let old = devices.paired[0]
        #expect(old.name == "Old ThinkPad" && old.revoke == .available)
        #expect(old.line.hasPrefix("Paired "))
        #expect(old.line.hasSuffix("\u{00B7} last seen 41 days ago"))
        #expect(devices.keyBackupNeeded)
        #expect(devices.deviceLimit == 4)

        // Rename is the connected Core's, as from Your Cores.
        let row = try #require(rig.flow.connectedCoreRow)
        #expect(rig.flow.renameAvailability(row) == .available)

        // Away: an amber state and "away for 1 minute".
        await rig.station.deliver(Self.connectedDevices(phoneId: rig.phoneId, iPadAwayFor: 60))
        #expect(await settle { devices.connected.count == 3 && devices.connected[2].away })
        #expect(devices.connected[2].line == "away for 1 minute")

        // The computer running the Core, and a token window, can't be revoked, and say why.
        await rig.station.deliver(Self.connectedDevices(phoneId: rig.phoneId, iPadAwayFor: nil, withHostAndToken: true))
        #expect(await settle { devices.connected.count == 5 })
        #expect(devices.connected[3].revoke == .unavailable(DevicesModel.hostsCoreReason))
        #expect(devices.connected[4].revoke == .unavailable(DevicesModel.tokenReason))
        await rig.app.disconnect()
    }

    @Test("Revoke sends devices.revoke for that device; a refusal shows the Core's words under it")
    func revoke() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let devices = rig.devices
        #expect(await settle { devices.connected.count == 3 })

        let revoking = Task { await devices.revoke(Self.macBookId) }
        let sent = await rig.station.waitForMessage { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "devices.revoke" } else { return false }
        }
        guard case .commandInvoke(let invoke)? = sent else {
            Issue.record("no devices.revoke")
            return
        }
        #expect(invoke.args == [LinkMessage.PropertyEntry(name: "id", value: .utf8(Self.macBookId))])
        await rig.station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: "devices.revoke", id: invoke.id, accepted: true, reason: "", affected: ["devices"])))
        await revoking.value
        #expect(devices.revokeProblems.isEmpty)

        // This phone is never revoked from here.
        let before = rig.station.messages.count
        await devices.revoke(rig.phoneId)
        #expect(!rig.station.messages.dropFirst(before).contains { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "devices.revoke" } else { return false }
        })

        // Refused: the Core's words, under that device.
        let reason = "Pair another device first, or reset this Core from its own computer."
        rig.station.refuseNext("devices.revoke", reason: reason)
        await devices.revoke(Self.thinkPadId)
        #expect(devices.revokeProblems[Self.thinkPadId] == reason)
        try await Self.shoot("setup-devices-refused", rig: rig, path: [.page(.devices)])
        await rig.app.disconnect()
    }

    @Test("Add a device opens pairing and shows the Core's code; Close pairing closes it; the backup is confirmed")
    func pairingAndBackup() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let devices = rig.devices
        #expect(await settle { devices.pairs && devices.connected.count == 3 })
        let code = "\(Int.random(in: 1...999))-\(PairingCodeText.words.randomElement() ?? "a")-"
            + (PairingCodeText.words.randomElement() ?? "b")

        let opening = Task { await devices.openPairing() }
        let open = try await Self.invoke("pairing.open", rig.station)
        #expect(open.args.isEmpty)
        await rig.station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: "pairing.open", id: open.id, accepted: true, reason: "", affected: ["devices"],
            values: [.init(name: "code", value: .utf8(code))])))
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "devices", properties: [
            .init(ordinal: 7, name: "pairingWindowOpen", value: .bool(true)),
            .init(ordinal: 8, name: "pairingCode", value: .utf8(code)),
        ])))
        await opening.value
        #expect(await settle { devices.pairingOpen && devices.pairingCode == code })
        try await Self.shoot("setup-devices-add", rig: rig, path: [.page(.devices)])

        let closing = Task { await devices.closePairing() }
        let close = try await Self.invoke("pairing.close", rig.station)
        await rig.station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: "pairing.close", id: close.id, accepted: true, reason: "", affected: ["devices"])))
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "devices", properties: [
            .init(ordinal: 7, name: "pairingWindowOpen", value: .bool(false)),
            .init(ordinal: 8, name: "pairingCode", value: .utf8("")),
        ])))
        await closing.value
        #expect(await settle { !devices.pairingOpen && devices.pairingCode.isEmpty })

        #expect(devices.keyBackupNeeded)
        let acknowledging = Task { await devices.acknowledgeBackup() }
        let backup = try await Self.invoke("station.acknowledgeKeyBackup", rig.station)
        await rig.station.deliver(.commandResult(LinkMessage.CommandResult(
            verb: "station.acknowledgeKeyBackup", id: backup.id, accepted: true, reason: "", affected: ["devices"])))
        await rig.station.deliver(.delta(LinkMessage.Delta(key: "devices", properties: [
            .init(ordinal: 5, name: "keyBackupAcknowledged", value: .bool(true)),
        ])))
        await acknowledging.value
        #expect(await settle { !devices.keyBackupNeeded })
        await rig.app.disconnect()
    }

    // MARK: The transmit time-out

    @Test("the time-out reads 3 minutes by default and writes the Core's two settings; a refusal shows its words")
    func transmitTimeOut() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let timeOut = TransmitTimeOutModel(settings: rig.app.settings)
        #expect(timeOut.choice == 180)
        #expect(TransmitTimeOutModel.text(timeOut.choice) == "3 minutes")
        #expect(TransmitTimeOutModel.text(30) == "30 seconds")
        #expect(TransmitTimeOutModel.text(150) == "2 minutes 30 seconds")
        #expect(TransmitTimeOutModel.text(nil) == "Off")

        let choosing = Task { await timeOut.choose(300) }
        try await Self.echo(TransmitTimeOutModel.secondsKey, "300", rig: rig)
        try await Self.echo(TransmitTimeOutModel.enabledKey, "True", rig: rig)
        await choosing.value
        #expect(timeOut.choice == 300)
        #expect(timeOut.problem == nil)

        let off = Task { await timeOut.choose(nil) }
        try await Self.echo(TransmitTimeOutModel.enabledKey, "False", rig: rig)
        await off.value
        #expect(timeOut.choice == nil)
        try await Self.shoot("setup-ptt-buttons-off", rig: rig, path: [.category("Transmit"), .page(.pttButtons)])

        let refused = Task { await timeOut.choose(60) }
        let write = try await Self.settingsWrite(TransmitTimeOutModel.secondsKey, rig.station)
        await rig.station.deliver(.settingsReject(LinkMessage.SettingsReject(
            key: write.key, properties: [.init(name: write.key, value: .utf8("300"))],
            reason: "The Core keeps this setting for its own computer.")))
        await refused.value
        #expect(timeOut.problem == "The Core keeps this setting for its own computer.")
        #expect(timeOut.choice == nil, "still off: the switch was never sent")
        await rig.app.disconnect()
    }

    // MARK: Pictures

    @Test("pictures of the tree and each page")
    func pictures() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        #expect(await settle { rig.devices.connected.count == 3 })
        let shots: [(String, [SetupTree.Route])] = [
            ("setup-tree", []),
            ("setup-devices", [.page(.devices)]),
            ("setup-general", [.category("General")]),
            ("setup-navigation", [.category("General"), .page(.navigation)]),
            ("setup-battery-and-sessions", [.category("General"), .page(.batteryAndSessions)]),
            ("setup-audio-on-this-phone", [.category("Audio"), .page(.audioOnThisPhone)]),
            ("setup-display", [.category("Display"), .page(.displayOnThisPhone)]),
            ("setup-ptt-buttons", [.category("Transmit"), .page(.pttButtons)]),
            ("setup-data-use", [.category("CAT & Network"), .page(.dataUse)]),
            ("setup-about", [.page(.about)]),
        ]
        for (name, path) in shots {
            try await Self.shoot(name, rig: rig, path: path)
        }
        await rig.app.disconnect()
    }

    // MARK: The rig

    struct Rig {
        let app: AppModel
        let flow: ConnectionFlow
        let station: FakeStation
        let devices: DevicesModel
        let router: SetupRouter
        let phoneId: String
        let suite: String
    }

    private static func phoneSettings() throws -> (PhoneSettings, String) {
        let suite = "SetupRenderingTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        return (PhoneSettings(defaults: defaults), suite)
    }

    /// The app connected, through Your Cores, to a fake Core named
    /// KG4VCF/shack that shares itself and opens pairing, with picture 22's
    /// cast: this phone (slice A on 40 m), the MacBook (B on 40 m and D on
    /// 17 m, with transmit, 2 hours) and the iPad (C on 20 m, 40 minutes),
    /// and the Old ThinkPad paired, last seen 41 days ago.
    private static func connected() async throws -> Rig {
        let (settings, suite) = try phoneSettings()
        let defaults = try #require(UserDefaults(suiteName: suite))
        let app = AppModel(phoneSettings: settings, displaySettings: BandDisplaySettingsStore(defaults: defaults))
        let station = try FakeStation(fixture: "session-device-sign-in", additions: [.bands],
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
        let phoneId = try #require(app.main.transmit.thisDeviceId)
        var capabilities = app.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(name: name, value: value.wireValue)
        }
        for name in [SeveralDevices.capability, StationDevices.pairingCapability] {
            capabilities.removeAll { $0.name == name }
            capabilities.append(LinkMessage.PropertyEntry(name: name, value: .i64(1)))
        }
        await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities)))
        let json = try #require(MainScreenTests.catalogueJSON())
        await station.deliver(.delta(LinkMessage.Delta(key: "catalog", properties: [
            .init(ordinal: 0, name: "json", value: .utf8(station.catalogue(json))),
            .init(ordinal: 1, name: "revision", value: .i64(2)),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "devices", properties: [
            .init(ordinal: 0, name: "listJson", value: .utf8(pairedList(phoneId: phoneId))),
            .init(ordinal: 1, name: "revision", value: .i64(5)),
        ])))
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(key: "connectedDevices",
                                                                     className: "ConnectedDevicesFacade",
                                                                     properties: [
            .init(ordinal: 0, name: "listJson", value: .utf8("[]")),
            .init(ordinal: 1, name: "revision", value: .i64(1)),
            .init(ordinal: 2, name: "deviceLimit", value: .i64(4)),
        ])))
        await station.deliver(connectedDevices(phoneId: phoneId, iPadAwayFor: nil))
        let transmit = app.main.transmit
        let devices = DevicesModel(mirror: app.mirror, commands: app.commands, catalogFeed: app.main.catalogFeed,
                                   thisDeviceId: { [weak transmit] in transmit?.thisDeviceId })
        return Rig(app: app, flow: flow, station: station, devices: devices, router: SetupRouter(),
                   phoneId: phoneId, suite: suite)
    }

    /// The paired devices in pairing order: this phone, the MacBook, the
    /// iPad, then the Old ThinkPad, last seen 41 days ago.
    static func pairedList(phoneId: String) -> String {
        let now = Date()
        func time(daysAgo: Double) -> String {
            ISO8601DateFormatter().string(from: now.addingTimeInterval(-daysAgo * 86_400))
        }
        func device(_ id: String, _ name: String, _ short: String, _ kind: String, paired: Double, seen: Double,
                    connected: Bool) -> LinkJSON {
            .object(["id": .string(id), "name": .string(name), "shortName": .string(short), "kind": .string(kind),
                     "pairedAt": .string(time(daysAgo: paired)), "lastSeen": .string(time(daysAgo: seen)),
                     "connected": .bool(connected)])
        }
        return LinkJSON.array([
            device(phoneId, "JJ's iPhone", "iPhone", "phone", paired: 55, seen: 0, connected: true),
            device(macBookId, "MacBook Pro", "MacBook", "computer", paired: 50, seen: 0, connected: true),
            device(iPadId, "iPad Pro", "iPad", "tablet", paired: 45, seen: 0, connected: true),
            device(thinkPadId, "Old ThinkPad", "Computer", "computer", paired: 88, seen: 41.2, connected: false),
        ]).compactText
    }

    /// Who is connected: picture 22's three, with the iPad away when asked,
    /// and a hosting desktop and a token window when asked.
    static func connectedDevices(phoneId: String, iPadAwayFor away: Int64?, withHostAndToken: Bool = false)
        -> LinkMessage {
        func slice(_ id: Int, _ letter: String, band: Int) -> LinkJSON {
            .object(["sliceId": .number(Double(id)), "letter": .string(letter), "band": .number(Double(band)),
                     "mode": .number(0)])
        }
        func device(_ id: String, _ name: String, _ short: String, _ kind: String, state: String = "listening",
                    connected: Int64, away: Int64 = 0, holds: Bool = false, paired: Bool = true,
                    hostsCore: Bool = false, revocable: Bool = true, slices: [LinkJSON]) -> LinkJSON {
            .object(["deviceId": .string(id), "name": .string(name), "shortName": .string(short), "kind": .string(kind),
                     "paired": .bool(paired), "hostsCore": .bool(hostsCore), "revocable": .bool(revocable),
                     "state": .string(state), "holdsTransmit": .bool(holds), "lastActivitySeconds": .number(60),
                     "connectedForSeconds": .number(Double(connected)), "awayForSeconds": .number(Double(away)),
                     "transmittingForSeconds": .number(0), "listeningOn": .array(slices)])
        }
        // Band ids as the catalogue's grid numbers them: 40 m is 3, 20 m is 5, 17 m is 6.
        var list = [
            device(phoneId, "JJ's iPhone", "iPhone", "phone", connected: 1800, slices: [slice(0, "A", band: 3)]),
            device(macBookId, "MacBook Pro", "MacBook", "computer", connected: 7300, holds: true,
                   slices: [slice(1, "B", band: 3), slice(3, "D", band: 6)]),
            device(iPadId, "iPad Pro", "iPad", "tablet", state: away == nil ? "listening" : "away", connected: 2400,
                   away: away ?? 0, slices: [slice(2, "C", band: 5)]),
        ]
        if withHostAndToken {
            list.append(device("mac-mini-id", "Mac mini", "Mac mini", "station", connected: 90_000, hostsCore: true,
                               revocable: false, slices: []))
            list.append(device("token:1", "Computer at 192.0.2.7", "Computer", "computer", connected: 600,
                               paired: false, revocable: false, slices: []))
        }
        return .delta(LinkMessage.Delta(key: "connectedDevices", properties: [
            .init(ordinal: 0, name: "listJson", value: .utf8(LinkJSON.array(list).compactText)),
            .init(ordinal: 1, name: "revision", value: .i64(away == nil ? (withHostAndToken ? 4 : 2) : 3)),
        ]))
    }

    private static func invoke(_ verb: String, _ station: FakeStation) async throws -> LinkMessage.CommandInvoke {
        let sent = await station.waitForMessage { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == verb } else { return false }
        }
        guard case .commandInvoke(let invoke)? = sent else {
            throw SetupTestError.notSent(verb)
        }
        return invoke
    }

    private static func settingsWrite(_ key: String, _ station: FakeStation) async throws
        -> LinkMessage.SettingsWrite {
        let sent = await station.waitForMessage { message in
            if case .settingsWrite(let write) = message { return write.key == key } else { return false }
        }
        guard case .settingsWrite(let write)? = sent else {
            throw SetupTestError.notSent(key)
        }
        return write
    }

    /// Waits for this app's write of `key` with `value`, then echoes it as the Core does.
    private static func echo(_ key: String, _ value: String, rig: Rig) async throws {
        let sent = await rig.station.waitForMessage { message in
            if case .settingsWrite(let write) = message {
                return write.key == key && write.properties.first?.value == .utf8(value)
            }
            return false
        }
        guard case .settingsWrite(let write)? = sent else {
            throw SetupTestError.notSent(key)
        }
        await rig.station.deliver(.settingsValue(LinkMessage.SettingsValue(key: key, origin: write.origin,
                                                                           properties: write.properties)))
    }

    enum SetupTestError: Error {
        case notSent(String)
    }

    private static func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            try? await Task.sleep(for: .milliseconds(20))
        }
        return condition()
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        await Self.settle(seconds: seconds, condition)
    }

    /// Draws the Setup tab at `path` in an upright phone's window.
    private static func shoot(_ name: String, rig: Rig, path: [SetupTree.Route]) async throws {
        let size = CGSize(width: 402, height: 874)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        rig.router.path = path
        let root = VStack(spacing: 0) {
            SetupTab(app: rig.app, flow: rig.flow, router: rig.router, buildTag: nil, devices: rig.devices)
            TabBar(selection: .constant(.setup), sideways: false)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
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
}
