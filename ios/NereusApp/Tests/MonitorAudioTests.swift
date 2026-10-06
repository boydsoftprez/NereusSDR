// NereusSDR for iOS: MON in the phone's headphones, against a fake Core: its reasons, monEnabled, monitor-audio, the route going away, and pictures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMedia
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// Task 55b (D80, R-IOS-20, R-IOS-13): MON plays the transmit monitor in
/// the phone's headphones. It is greyed with a reason unless the Core sends
/// the transmit monitor (`txMonitorAudioVersion`) and the phone's sound is
/// in wired or Bluetooth headphones. On, it writes the Core's `monEnabled`
/// once and asks for `monitor-audio {route: headphones}`; off, it writes
/// `monEnabled` off and asks for route none; the headphones going away does
/// both. MON shows the Core's `monEnabled` as the Core applied the route.
/// The TX panel and the Modes tab show and write the same. With
/// `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the pictures are written there.
@Suite("MON in the phone's headphones", .serialized)
@MainActor
struct MonitorAudioTests {
    private let center = NotificationCenter()
    /// This test's own lock, background time and idle timer.
    private let platform = TestPlatform()
    /// Writes already answered, so a later wait takes the next one.
    private let answered = MonitorAnsweredWrites()

    @MainActor
    struct Rig {
        let model: AppModel
        let station: FakeStation
        let session: FakeAudioSession
        let audio: AudioSessionController
        let output: FakePlaybackOutput
        let microphone: FakeMicrophone

        var transmit: TransmitModel { model.main.transmit }
    }

    static let withMonitor: FakeStation.Additions = [.remoteTx, .wideband, .monitorAudio]

    // MARK: The reasons

    @Test("the two reasons, word for word")
    func theReasonsWordForWord() {
        #expect(TransmitModel.monNotSentText
                == "This Core does not send the transmit monitor. Updating the Core may help.")
        #expect(TransmitModel.monNeedsHeadphonesText
                == "Plug in headphones to hear your transmit. The loudspeaker would feed back into the microphone.")
    }

    @Test("MON is greyed with the first reason on a Core without the transmit monitor, headphones or not")
    func greyedOnACoreWithoutTheMonitor() async throws {
        let rig = try await connected(additions: [.remoteTx, .wideband], device: .wired("Headphones"))
        let transmit = rig.transmit
        #expect(await ShotWait.until { transmit.onHeadphones })
        #expect(transmit.monReason == TransmitModel.monNotSentText)
        // The start declares no transmit monitor to a Core that does not send it.
        #expect(try #require(Self.start(rig.station))["txMonitorAudioVersion"] == nil)
        transmit.toggleMon()
        await idle()
        #expect(Self.monWrites(rig.station).isEmpty)
        #expect(Self.monitorOps(rig.station).isEmpty)
        #expect(!transmit.mon)
        await rig.model.disconnect()
    }

    @Test("MON is greyed with the second reason on the loudspeaker and the earpiece, and sends nothing")
    func greyedOffHeadphones() async throws {
        let rig = try await connected()
        let transmit = rig.transmit
        #expect(rig.audio.route == .speaker)
        #expect(!transmit.onHeadphones)
        #expect(await ShotWait.until { transmit.monReason == TransmitModel.monNeedsHeadphonesText })
        // A Core that sends the monitor is told this phone may ask for it.
        #expect(try #require(Self.start(rig.station))["txMonitorAudioVersion"] == .number(1))
        transmit.toggleMon()
        await idle()
        #expect(Self.monWrites(rig.station).isEmpty)
        #expect(rig.station.monitorAudioRequests.isEmpty)

        rig.audio.select(.earpiece)
        await rig.audio.settle()
        #expect(await ShotWait.until { rig.audio.route == .earpiece })
        #expect(!transmit.onHeadphones)
        #expect(transmit.monReason == TransmitModel.monNeedsHeadphonesText)
        transmit.toggleMon()
        await idle()
        #expect(Self.monWrites(rig.station).isEmpty)
        #expect(rig.station.monitorAudioRequests.isEmpty)
        await rig.model.disconnect()
    }

    // MARK: On and off

    @Test("on wired headphones MON on writes monEnabled once and asks for headphones; off writes it off and asks for none")
    func onAndOffOnWiredHeadphones() async throws {
        let rig = try await connected(device: .wired("Headphones"))
        let transmit = rig.transmit
        #expect(await ShotWait.until { transmit.onHeadphones && transmit.monReason == nil })
        #expect(rig.station.monitorAudioRequests.isEmpty)

        transmit.toggleMon()
        let on = try #require(await answer(rig.station))
        #expect(on.key == "transmit")
        #expect(on.properties.map(\.name) == ["monEnabled"])
        #expect(on.properties.first?.value == .bool(true))
        #expect(await ShotWait.until { rig.station.monitorAudioRequests.map(\.route) == ["headphones"] })
        // The Core applies headphones as this phone's main stream, and MON
        // shows lit from its monEnabled and that answer.
        #expect(await ShotWait.until { transmit.mon && transmit.monitorApplied == .speakers })
        await idle()
        #expect(Self.monWrites(rig.station) == [true])
        #expect(rig.station.monitorAudioRequests.count == 1)

        transmit.toggleMon()
        let off = try #require(await answer(rig.station))
        #expect(off.properties.first?.value == .bool(false))
        #expect(await ShotWait.until { rig.station.monitorAudioRequests.map(\.route) == ["headphones", "none"] })
        #expect(await ShotWait.until { !transmit.mon && transmit.monitorApplied == MonitorRoute.none })
        await idle()
        #expect(Self.monWrites(rig.station) == [true, false])
        let id = try #require(await rig.model.media.connectionId)
        #expect(rig.station.monitorAudioRequests.allSatisfy { $0.connectionId == id })
        #expect(rig.station.monitorAudioRequests.map(\.revision) == [1, 2])
        await rig.model.disconnect()
    }

    @Test("Bluetooth headphones enable MON as wired ones do")
    func onBluetoothHeadphones() async throws {
        let rig = try await connected(device: .bluetooth("AirPods"))
        let transmit = rig.transmit
        #expect(await ShotWait.until { transmit.onHeadphones && transmit.monReason == nil })
        transmit.toggleMon()
        let on = try #require(await answer(rig.station))
        #expect(on.properties.first?.value == .bool(true))
        #expect(await ShotWait.until { rig.station.monitorAudioRequests.map(\.route) == ["headphones"] })
        #expect(await ShotWait.until { transmit.mon })
        await rig.model.disconnect()
    }

    @Test("the headphones going away with MON on ask for none, write monEnabled off once, and MON shows off")
    func headphonesGoAway() async throws {
        let rig = try await connected(device: .wired("Headphones"))
        let transmit = rig.transmit
        #expect(await ShotWait.until { transmit.monReason == nil })
        transmit.toggleMon()
        _ = try #require(await answer(rig.station))
        #expect(await ShotWait.until { transmit.mon })

        await plug(rig, nil)
        #expect(await ShotWait.until { !transmit.onHeadphones })
        #expect(await ShotWait.until { rig.station.monitorAudioRequests.map(\.route) == ["headphones", "none"] })
        let off = try #require(await answer(rig.station))
        #expect(off.properties.first?.value == .bool(false))
        #expect(await ShotWait.until { !transmit.mon })
        #expect(transmit.monReason == TransmitModel.monNeedsHeadphonesText)
        await idle()
        #expect(Self.monWrites(rig.station) == [true, false])
        #expect(rig.station.monitorAudioRequests.count == 2)

        // Plugged in again, MON stays off until the operator turns it on.
        await plug(rig, .wired("Headphones"))
        #expect(await ShotWait.until { transmit.onHeadphones && transmit.monReason == nil })
        await idle()
        #expect(!transmit.mon)
        #expect(Self.monWrites(rig.station) == [true, false])
        #expect(rig.station.monitorAudioRequests.count == 2)
        await rig.model.disconnect()
    }

    // MARK: Keyed

    @Test("keyed with MON on in headphones the band keeps playing, which carries the Core's monitor; keying sends nothing more")
    func keyedWithMonInHeadphones() async throws {
        let rig = try await connected(device: .wired("Headphones"))
        let transmit = rig.transmit
        #expect(await ShotWait.until { transmit.monReason == nil })
        transmit.toggleMon()
        _ = try #require(await answer(rig.station))
        #expect(await ShotWait.until { transmit.mon })
        let requests = rig.station.monitorAudioRequests

        transmit.tapPtt()
        #expect(await ShotWait.until { transmit.ptt.state.isKeyed && transmit.transmittingHere })
        #expect(rig.station.keyed)
        // The Core mixes MON at its level into this phone's main stream,
        // which the band's player plays: not silenced while keyed.
        await idle()
        #expect(!rig.audio.bandMuted)
        #expect(!rig.output.isMuted)

        transmit.tapPtt()
        #expect(await ShotWait.until { transmit.ptt.state == .idle })
        await idle()
        #expect(!rig.output.isMuted)
        // Key and unkey add no monitor request and no MON write.
        #expect(rig.station.monitorAudioRequests == requests)
        #expect(Self.monWrites(rig.station) == [true])
        await rig.model.disconnect()
    }

    // MARK: The TX panel and the Modes tab

    @Test("the TX panel and the Modes tab show MON the same way, and either one writes it")
    func panelAndModesAgree() async throws {
        let rig = try await connected(device: nil)
        let transmit = rig.transmit
        #expect(await ShotWait.until { transmit.monReason == TransmitModel.monNeedsHeadphonesText })
        try await onScreen(rig.model) { window in
            #expect(await buttons(hint: TransmitModel.monNeedsHeadphonesText, enabled: false, in: window))
            #expect(await notes(TransmitModel.monNeedsHeadphonesText, in: window))

            await plug(rig, .wired("Headphones"))
            #expect(await buttons(hint: "", enabled: true, in: window))
            #expect(await notes(nil, in: window))

            // The TX panel's MON turns it on; the Modes tab's turns it off.
            #expect(try #require(SetupTypedEntryTests.element("txMon", in: window)).accessibilityActivate())
            let fromPanel = try #require(await answer(rig.station))
            #expect(fromPanel.properties.first?.value == .bool(true))
            #expect(await ShotWait.until { transmit.mon })
            #expect(await buttonsLit(true, in: window))
            #expect(try #require(SetupTypedEntryTests.element("modesMon", in: window)).accessibilityActivate())
            let fromModes = try #require(await answer(rig.station))
            #expect(fromModes.key == fromPanel.key)
            #expect(fromModes.properties.map(\.ordinal) == fromPanel.properties.map(\.ordinal))
            #expect(fromModes.properties.first?.value == .bool(false))
            #expect(await ShotWait.until { !transmit.mon })
            #expect(await buttonsLit(false, in: window))
            #expect(rig.station.monitorAudioRequests.map(\.route) == ["headphones", "none"])
        }
        await rig.model.disconnect()
    }

    @Test("both places grey MON with the first reason on a Core without the transmit monitor")
    func panelAndModesAgreeWithoutTheMonitor() async throws {
        let rig = try await connected(additions: [.remoteTx, .wideband], device: .wired("Headphones"))
        #expect(await ShotWait.until { rig.transmit.onHeadphones })
        try await onScreen(rig.model) { window in
            #expect(await buttons(hint: TransmitModel.monNotSentText, enabled: false, in: window))
            #expect(await notes(TransmitModel.monNotSentText, in: window))
        }
        await rig.model.disconnect()
    }

    // MARK: Pictures

    @Test("pictures: MON greyed for each reason and on with headphones, in the TX panel and the Modes tab")
    func shots() async throws {
        // A Core without the transmit monitor, the phone on headphones.
        let older = try await shotRig(additions: [.remoteTx, .accessoryOperate], device: .wired("Headphones"))
        #expect(await ShotWait.until {
            older.transmit.onHeadphones && older.transmit.monReason == TransmitModel.monNotSentText
        })
        try await shootAll("mon-greyed-older-core", older.model)
        await older.model.disconnect()

        // A Core that sends it, the phone on its loudspeaker.
        let rig = try await shotRig(additions: [.remoteTx, .accessoryOperate, .monitorAudio], device: nil)
        let transmit = rig.transmit
        #expect(await ShotWait.until { transmit.monReason == TransmitModel.monNeedsHeadphonesText })
        try await shootAll("mon-greyed-loudspeaker", rig.model)

        // Headphones in, MON on and the Core's answer.
        await plug(rig, .wired("Headphones"))
        #expect(await ShotWait.until { transmit.monReason == nil })
        transmit.toggleMon()
        _ = try #require(await answer(rig.station))
        rig.model.main.receive(.monitorAudioContext(.init(revision: 1, route: .speakers)))
        #expect(await ShotWait.until { transmit.mon })
        try await shootAll("mon-on-headphones", rig.model)
        await rig.model.disconnect()
    }

    // MARK: The fake Core

    /// A model connected to a fake Core with `additions`, the phone's sound
    /// on `device` (nil: the loudspeaker), this phone permitted to transmit
    /// and its microphone line up.
    private func connected(additions: FakeStation.Additions = MonitorAudioTests.withMonitor,
                           device: FakeAudioSession.Device? = nil) async throws -> Rig {
        let defaults = try #require(UserDefaults(suiteName: "MonitorAudioTests-\(UUID().uuidString)"))
        let station = try FakeStation(additions: additions)
        let session = FakeAudioSession()
        session.device = device
        let output = FakePlaybackOutput()
        let microphone = FakeMicrophone()
        let audio = AudioSessionController(session: session, output: output, notificationCenter: center)
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             mediaPeerFactory: station.mediaPeerFactory, audio: audio,
                             microphone: { _ in microphone },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        model.main.transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        await TransmitScreenTests.fillTransmit(station)
        #expect(await ShotWait.until {
            model.main.transmit.permitted && model.connection == .connected && model.main.transmit.microphoneLine
        })
        await audio.settle()
        return Rig(model: model, station: station, session: session, audio: audio, output: output,
                   microphone: microphone)
    }

    /// A model for the pictures: no media, the suite's band and the TX
    /// panel's settings, with the phone's sound on `device`.
    private func shotRig(additions: FakeStation.Additions, device: FakeAudioSession.Device?) async throws -> Rig {
        let defaults = try #require(UserDefaults(suiteName: "MonitorAudioTests-\(UUID().uuidString)"))
        let station = try FakeStation(additions: additions)
        let session = FakeAudioSession()
        session.device = device
        let output = FakePlaybackOutput()
        let microphone = FakeMicrophone()
        let audio = AudioSessionController(session: session, output: output, notificationCenter: center)
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults), audio: audio,
                             microphone: { _ in microphone },
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let transmit = model.main.transmit
        transmit.thisDeviceId = TransmitScreenTests.thisDeviceId
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await ShotWait.until { model.connection == .connected })
        await station.deliver(.objectCreate(LinkMessage.ObjectCreate(
            key: "txState", className: "TransmitState",
            properties: TransmitScreenTests.holder("", short: "", keyed: false)
                + [.init(ordinal: 3, name: "txSliceId", value: .i64(0)),
                   .init(ordinal: 17, name: "stopSerial", value: .i64(0))])))
        #expect(await ShotWait.until { transmit.permitted })
        try await MainScreenShotTests.fill(station)
        #expect(await ShotWait.until { model.main.slices.entries.count == 2 })
        await TransmitScreenTests.fillTransmit(station)
        #expect(await ShotWait.until { transmit.amp != nil && transmit.rfPower == 100 })
        let band = model.main.band
        band.endpointId = 1
        band.receive(.context(try #require(BandFlagShotTests.context())))
        await audio.settle()
        return Rig(model: model, station: station, session: session, audio: audio, output: output,
                   microphone: microphone)
    }

    /// The phone's output changes to `device` (nil: the headphones go away).
    private func plug(_ rig: Rig, _ device: FakeAudioSession.Device?) async {
        rig.session.device = device
        let reason: AVAudioSession.RouteChangeReason = device == nil ? .oldDeviceUnavailable : .newDeviceAvailable
        center.post(name: AVAudioSession.routeChangeNotification, object: nil,
                    userInfo: [AVAudioSessionRouteChangeReasonKey: reason.rawValue])
        await rig.audio.settle()
    }

    /// The app's media `start`, the newest.
    static func start(_ station: FakeStation) -> [String: LinkJSON]? {
        station.messages.compactMap { message -> [String: LinkJSON]? in
            if case .mediaControl(let control) = message, control.payload["op"] == .string("start") {
                return control.payload
            }
            return nil
        }.last
    }

    /// Every `monitor-audio` the app sent, taken or not.
    static func monitorOps(_ station: FakeStation) -> [[String: LinkJSON]] {
        station.messages.compactMap { message -> [String: LinkJSON]? in
            if case .mediaControl(let control) = message, control.payload["op"] == .string("monitor-audio") {
                return control.payload
            }
            return nil
        }
    }

    /// The values of every `monEnabled` write the app sent, in order.
    static func monWrites(_ station: FakeStation) -> [Bool] {
        station.messages.compactMap { message -> Bool? in
            guard case .propertyWrite(let write) = message,
                  let entry = write.properties.first(where: { $0.name == TransmitModel.monEnabledProperty }) else {
                return nil
            }
            if case .bool(let on) = entry.value {
                return on
            }
            return nil
        }
    }

    /// Waits for the app's next write of `transmit.monEnabled`, then answers
    /// it as the Core does: taken, with a delta of the value.
    private func answer(_ station: FakeStation) async -> LinkMessage.PropertyWrite? {
        let property = TransmitModel.monEnabledProperty
        let done = answered.ids
        let sent = await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.key == "transmit" && write.properties.first?.name == property
                    && !done.contains(write.writeId ?? 0)
            }
            return false
        }
        guard case .propertyWrite(let write)? = sent, let writeId = write.writeId,
              let entry = write.properties.first else {
            Issue.record("no write of transmit.\(property) reached the Core")
            return nil
        }
        answered.ids.insert(writeId)
        #expect(write.properties.count == 1, "the write of \(property) carries only it")
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "transmit", writeId: writeId, results: [
            .init(property: property, accepted: true, reason: "", value: entry),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "transmit", properties: [entry])))
        return write
    }

    /// Lets anything the app might send go.
    private func idle() async {
        for _ in 0..<2_000 {
            await Task.yield()
        }
        try? await Task.sleep(for: .milliseconds(200))
        for _ in 0..<2_000 {
            await Task.yield()
        }
    }

    // MARK: On screen

    private static let buttonIds = ["txMon", "modesMon"]
    private static let noteIds = ["txMonReason", "modesMonReason"]

    /// Both MON buttons carry `hint` ("" for none) and are enabled or not, waited for.
    private func buttons(hint: String, enabled: Bool, in window: UIWindow) async -> Bool {
        await ShotWait.until {
            window.layoutIfNeeded()
            return Self.buttonIds.allSatisfy { id in
                guard let element = SetupTypedEntryTests.element(id, in: window) else {
                    return false
                }
                return (element.accessibilityHint ?? "") == hint
                    && element.accessibilityTraits.contains(.notEnabled) == !enabled
            }
        }
    }

    /// Both MON buttons are lit (selected) or not, waited for.
    private func buttonsLit(_ lit: Bool, in window: UIWindow) async -> Bool {
        await ShotWait.until {
            window.layoutIfNeeded()
            return Self.buttonIds.allSatisfy { id in
                SetupTypedEntryTests.element(id, in: window)?.accessibilityTraits.contains(.selected) == lit
            }
        }
    }

    /// Both places show MON's reason note with `reason`, or neither shows one (nil).
    private func notes(_ reason: String?, in window: UIWindow) async -> Bool {
        await ShotWait.until {
            window.layoutIfNeeded()
            return Self.noteIds.allSatisfy { id in
                let note = SetupTypedEntryTests.element(id, in: window)
                guard let reason else {
                    return note == nil
                }
                return note?.accessibilityLabel == "MON: " + reason
            }
        }
    }

    /// The TX panel and the Modes tab's Transmit section in one window,
    /// with application accessibility on, while `check` runs.
    private func onScreen(_ model: AppModel, _ check: (UIWindow) async throws -> Void) async throws {
        let main = model.main
        let window = try BandFlagShotTests.window(size: CGSize(width: TxPanel.width, height: 2_600))
        let root = VStack(spacing: 0) {
            TxPanel(transmit: main.transmit, accessories: main.accessories, micLevel: main.micLevel,
                    modes: main.modes, meters: nil, scrolls: false)
            TransmitSection(model: main.modes, transmit: main.transmit, micLevel: main.micLevel)
        }
        let host = UIHostingController(rootView: root)
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
        try await check(window)
    }

    // MARK: Drawing

    /// One state's pictures: the main screen with the TX panel open, and the
    /// Modes tab's Transmit section, each light, dark and in large type.
    private func shootAll(_ name: String, _ model: AppModel) async throws {
        for (suffix, scheme, large) in [("dark", ColorScheme.dark, false), ("light", .light, false),
                                        ("large-type", .dark, true)] {
            try await shootPanel("\(name)-txpanel-\(suffix)", model: model, scheme: scheme, largeText: large)
            try await shootModes("\(name)-modes-\(suffix)", model: model, scheme: scheme, largeText: large)
        }
    }

    /// The main screen at this simulator's size with the TX panel open,
    /// scrolled so MON's row shows.
    private func shootPanel(_ name: String, model: AppModel, scheme: ColorScheme, largeText: Bool) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let size = scene.screen.bounds.size
        let window = try BandFlagShotTests.window(size: size)
        let root = MonitorShotRoot(model: model)
            .environment(\.dynamicTypeSize, largeText ? .accessibility1 : .large)
            .preferredColorScheme(scheme)
        let host = UIHostingController(rootView: root)
        host.overrideUserInterfaceStyle = scheme == .dark ? .dark : .light
        host.view.frame = CGRect(origin: .zero, size: size)
        let wasOn = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
            SetupTypedEntryTests.setApplicationAccessibility(wasOn)
        }
        let bandDraw = try await ShotWait.requireBandLaidOut(model.main.band, in: window)
        BandFlagShotTests.feed(model.main.band, width: bandDraw.requestedPixels, lines: Int(size.height * 3))
        try await ShotWait.requireBandShown(model.main.band, in: window, after: bandDraw)
        try await scrollToMon(window, lead: largeText ? 120 : 100)
        try write(name, render(window))
    }

    /// The Modes tab's Transmit section at the simulator's width, on the
    /// tab's page colour, tall enough for MON and its reason.
    private func shootModes(_ name: String, model: AppModel, scheme: ColorScheme, largeText: Bool) async throws {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let size = CGSize(width: scene.screen.bounds.width, height: largeText ? 2_400 : 1_300)
        let window = try BandFlagShotTests.window(size: size)
        let main = model.main
        let root = ScrollView { TransmitSection(model: main.modes, transmit: main.transmit, micLevel: main.micLevel) }
            .background(ChromeColours.page)
            .preferredColorScheme(scheme)
            .environment(\.dynamicTypeSize, largeText ? .accessibility1 : .large)
        let host = UIHostingController(rootView: root)
        host.overrideUserInterfaceStyle = scheme == .dark ? .dark : .light
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        try write(name, render(window))
    }

    /// Scrolls the TX panel so MON's row sits `lead` points below its top.
    private func scrollToMon(_ window: UIWindow, lead: CGFloat) async throws {
        await ShotWait.laidOut(window)
        let scrolls = Self.scrollViews(in: window).filter {
            $0.contentSize.height > $0.bounds.height + 1 && abs($0.contentSize.width - TxPanel.width) < 24
        }
        let all = Self.scrollViews(in: window).map { "\($0.bounds.size) in \($0.contentSize)" }
        let scroll = try #require(scrolls.first, "no scrolling TX panel among \(all)")
        // The panel draws its rows as they come near: step down until MON is there.
        var found = SetupTypedEntryTests.element("txMon", in: window)
        while found == nil, scroll.contentOffset.y < scroll.contentSize.height - scroll.bounds.height - 1 {
            let next = min(scroll.contentOffset.y + scroll.bounds.height / 2,
                           scroll.contentSize.height - scroll.bounds.height)
            scroll.setContentOffset(CGPoint(x: 0, y: next), animated: false)
            await ShotWait.laidOut(window)
            found = SetupTypedEntryTests.element("txMon", in: window)
        }
        let row = try #require(found, "MON is not in the TX panel")
        let frame = scroll.convert(scroll.bounds, to: nil)
        let furthest = max(scroll.contentSize.height - scroll.bounds.height, 0)
        let target = min(max(scroll.contentOffset.y + row.accessibilityFrame.minY - (frame.minY + lead), 0), furthest)
        scroll.setContentOffset(CGPoint(x: 0, y: target), animated: false)
        await ShotWait.laidOut(window)
        let moved = try #require(SetupTypedEntryTests.element("txMon", in: window))
        #expect(frame.contains(moved.accessibilityFrame), "MON at \(moved.accessibilityFrame), panel \(frame)")
    }

    private func render(_ window: UIWindow) -> UIImage {
        UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
    }

    private func write(_ name: String, _ image: UIImage) throws {
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    private static func scrollViews(in view: UIView) -> [UIScrollView] {
        var found: [UIScrollView] = []
        if let scroll = view as? UIScrollView {
            found.append(scroll)
        }
        for child in view.subviews {
            found += scrollViews(in: child)
        }
        return found
    }
}

/// The write ids a test has answered.
@MainActor
private final class MonitorAnsweredWrites {
    var ids: Set<UInt32> = []
}

/// The app's root as `RootView` lays it out, with the TX panel open.
private struct MonitorShotRoot: View {
    @ObservedObject var model: AppModel

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 0) {
                MainScreen(app: model, main: model.main, txPanelOpen: true)
                TabBar(selection: .constant(.panadapter), sideways: proxy.size.width > proxy.size.height)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
