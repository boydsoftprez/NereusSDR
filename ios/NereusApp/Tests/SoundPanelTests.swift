// NereusSDR for iOS: the band's Sound panel mutes this phone, sets the radio and Core speakers, lists, marks and moves the band's sound
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Foundation
import NereusKitTesting
import NereusLink
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// D77: one tap on the speaker button opens the Sound panel: Mute this
/// phone, then Speaker, Earpiece and, only while connected, AirPods or
/// headphones by their own name, with a tick on the current one. Where the
/// band plays is the same choice Setup, Audio, On this phone makes, both
/// ways. R-SPK-20 (D12): between them the Radio speaker section sets the
/// Core's speaker level and mute, follows the Core's value, and is greyed
/// with its reason when it cannot be used. R-AUD-29 (D25, D26): under it
/// the Core speaker section sets the Core's own speaker level and mute,
/// is left out while the Core plays on no card and comes back by itself.
/// With `NEREUS_MAIN_SHOTS` set, the panel is written there as PNGs.
@Suite("SoundPanel", .serialized)
@MainActor
struct SoundPanelTests {
    private let center = NotificationCenter()

    private func controller(_ session: FakeAudioSession) -> AudioSessionController {
        AudioSessionController(session: session, output: FakePlaybackOutput(), notificationCenter: center)
    }

    private func playing(_ session: FakeAudioSession) async -> AudioSessionController {
        let audio = controller(session)
        audio.start()
        await audio.settle()
        await audio.settle()
        return audio
    }

    private func settings() throws -> PhoneSettings {
        PhoneSettings(defaults: try #require(UserDefaults(suiteName: "SoundPanelTests-\(UUID().uuidString)")))
    }

    @Test("lists the speaker and the earpiece, with the speaker marked")
    func withoutAirPods() async {
        let panel = SoundPanel(app: AppModel(audio: await playing(FakeAudioSession())))
        #expect(panel.rows == [
            SoundPanel.Row(route: .speaker, title: "Speaker", isCurrent: true),
            SoundPanel.Row(route: .earpiece, title: "Earpiece", isCurrent: false),
        ])
    }

    @Test("lists AirPods by their own name only while they are connected")
    func airPodsOnlyWhileConnected() async {
        let session = FakeAudioSession(device: .bluetooth("AirPods Pro"))
        let audio = await playing(session)
        let panel = SoundPanel(app: AppModel(audio: audio))
        #expect(panel.rows == [
            SoundPanel.Row(route: .speaker, title: "Speaker", isCurrent: false),
            SoundPanel.Row(route: .earpiece, title: "Earpiece", isCurrent: false),
            SoundPanel.Row(route: .external(name: "AirPods Pro"), title: "AirPods Pro", isCurrent: true),
        ])

        // They go away: their row goes with them.
        session.device = nil
        audio.handleRouteChange(.oldDeviceUnavailable)
        await audio.settle()
        #expect(panel.rows.map(\.route) == [.speaker, .earpiece])

        // Wired headphones come: listed by their name.
        session.device = .wired("Headphones")
        audio.handleRouteChange(.newDeviceAvailable)
        await audio.settle()
        #expect(panel.rows.map(\.title) == ["Speaker", "Earpiece", "Headphones"])
    }

    @Test("an iPad lists its speaker and no earpiece")
    func onAnIPad() async {
        let panel = SoundPanel(app: AppModel(audio: await playing(FakeAudioSession(isPhone: false))))
        #expect(panel.rows == [SoundPanel.Row(route: .speaker, title: "Speaker", isCurrent: true)])
    }

    @Test("the Mute this phone switch mutes and unmutes the band")
    func muteSwitch() async {
        let app = AppModel(audio: await playing(FakeAudioSession()))
        let panel = SoundPanel(app: app)
        #expect(!app.audioMuted)
        panel.setMuted(true)
        #expect(app.audioMuted)
        panel.setMuted(false)
        #expect(!app.audioMuted)
    }

    @Test("a choice in the panel shows in Setup, and one in Setup shows in the panel")
    func inStepWithSetup() async throws {
        let audio = await playing(FakeAudioSession(device: .bluetooth("AirPods Pro")))
        let panel = SoundPanel(app: AppModel(audio: audio))
        let setup = AudioOnThisPhonePage(settings: try settings(), audio: audio)
        func setupTicked() -> [AudioRoute] {
            AudioOnThisPhonePage.routeRows(routes: audio.routes, current: audio.markedRoute, isPhone: true)
                .filter(\.isCurrent).map(\.route)
        }

        panel.choose(.earpiece)
        await audio.settle()
        #expect(audio.chosen == .earpiece)
        #expect(audio.route == .earpiece)
        #expect(setupTicked() == [.earpiece])

        setup.choose(.speaker)
        await audio.settle()
        await audio.settle()
        #expect(audio.chosen == .speaker)
        #expect(panel.rows.filter(\.isCurrent).map(\.route) == [.speaker])

        panel.choose(.external(name: "AirPods Pro"))
        await audio.settle()
        #expect(setupTicked() == [.external(name: "AirPods Pro")])
    }

    @Test("before the band plays, both tick the choice")
    func choiceWhileStopped() async throws {
        let audio = controller(FakeAudioSession())
        await audio.settle()
        let panel = SoundPanel(app: AppModel(audio: audio))
        let setup = AudioOnThisPhonePage(settings: try settings(), audio: audio)
        setup.choose(.earpiece)
        #expect(panel.rows.filter(\.isCurrent).map(\.route) == [.earpiece])
        panel.choose(.speaker)
        #expect(audio.markedRoute == .speaker)
    }

    @Test("the speaker re-assert still applies to a choice made in the panel")
    func speakerKept() async {
        let session = FakeAudioSession()
        session.ignoresDefaultToSpeaker = true
        let audio = await playing(session)
        let panel = SoundPanel(app: AppModel(audio: audio))
        panel.choose(.earpiece)
        await audio.settle()
        #expect(audio.route == .earpiece)
        panel.choose(.speaker)
        await audio.settle()
        await audio.settle()
        #expect(audio.route == .speaker)
        #expect(session.overrides.last == .speaker)
    }

    @Test("with no sound on this phone the panel keeps Mute and lists no places")
    func withoutSound() {
        let panel = SoundPanel(app: AppModel())
        #expect(panel.rows.isEmpty)
    }
    // MARK: The radio speaker (R-SPK-20)

    /// The Core's `radio` ordinals of the speaker's properties (surface.json).
    static let ordinals: [String: UInt16] = ["radioSpeakerVolume": 38, "radioSpeakerMuted": 39,
                                             "speakerAmplifierMode": 40, "radioSpeakerAvailability": 41,
                                             "speakerAmplifierAvailable": 42,
                                             "coreSpeakerVolume": 43, "coreSpeakerMuted": 44,
                                             "coreSpeakerDevice": 45, "coreSpeakerDevices": 46,
                                             "coreSpeakerState": 47, "coreSpeakerDetails": 48]

    static func radioDelta(_ values: [String: LinkMessage.PropertyValue]) -> LinkMessage {
        .delta(LinkMessage.Delta(key: "radio", properties: values.keys.sorted().map {
            LinkMessage.PropertyEntry(ordinal: ordinals[$0] ?? 0, name: $0, value: values[$0] ?? .i64(0))
        }))
    }

    /// A model connected to a fake Core; with `speaker`, the Core offers the
    /// radio speaker (`radioSpeakerVersion` 1) and reports `availability`,
    /// level 40 and not muted. With `core`, the Core also offers the Core
    /// speaker (`coreSpeakerVersion` 1) with `core` as its state, level 60
    /// and not muted.
    private func connected(speaker: Bool, availability: Int64 = 1, core: String? = nil,
                           audio: AudioSessionController? = nil) async throws -> (AppModel, FakeStation) {
        let model = AppModel(phoneSettings: try settings(), audio: audio)
        let station = try FakeStation()
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await ShotWait.until { model.connection == .connected })
        if speaker || core != nil {
            var capabilities = model.mirror.capabilities
            if speaker {
                capabilities[RadioSpeakerModel.capabilityName] = .int(1)
            }
            if core != nil {
                capabilities[CoreSpeakerModel.capabilityName] = .int(1)
            }
            await station.deliver(.capabilities(LinkMessage.Capabilities(properties: capabilities.keys.sorted().map {
                LinkMessage.PropertyEntry(name: $0, value: capabilities[$0]?.wireValue ?? .i64(0))
            })))
        }
        if speaker {
            await station.deliver(Self.radioDelta([
                "radioSpeakerVolume": .i64(40), "radioSpeakerMuted": .bool(false), "speakerAmplifierMode": .i64(0),
                "radioSpeakerAvailability": .i64(availability), "speakerAmplifierAvailable": .bool(false),
            ]))
            #expect(await ShotWait.until { model.mirror.capabilityVersion(RadioSpeakerModel.capabilityName) == 1 })
        }
        if let core {
            await station.deliver(Self.radioDelta([
                "coreSpeakerVolume": .i64(60), "coreSpeakerMuted": .bool(false), "coreSpeakerState": .utf8(core),
            ]))
            #expect(await ShotWait.until { model.mirror.capabilityVersion(CoreSpeakerModel.capabilityName) == 1 })
        }
        return (model, station)
    }

    /// Waits for the app's next write of `radio`'s `property`, then answers
    /// it as the Core does: taken, with a delta of the value.
    private func keep(_ station: FakeStation, _ property: String) async -> LinkMessage.PropertyWrite? {
        let sent = await station.waitForMessage(within: .seconds(30)) { message in
            if case .propertyWrite(let write) = message {
                return write.key == "radio" && write.properties.first?.name == property
            }
            return false
        }
        guard case .propertyWrite(let write)? = sent, let writeId = write.writeId,
              let entry = write.properties.first else {
            Issue.record("no write of radio.\(property) reached the Core")
            return nil
        }
        await station.deliver(.propertyResult(LinkMessage.PropertyResult(key: "radio", writeId: writeId, results: [
            .init(property: property, accepted: true, reason: "", value: entry),
        ])))
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [entry])))
        return write
    }

    private static func radioWrites(_ station: FakeStation) -> [LinkMessage.PropertyWrite] {
        station.messages.compactMap { message in
            if case .propertyWrite(let write) = message, write.key == "radio" { return write }
            return nil
        }
    }

    @Test("the slider writes the Core's radio speaker level and follows the Core's value")
    func radioSpeakerVolumeFollowsTheCore() async throws {
        let (app, station) = try await connected(speaker: true)
        let panel = SoundPanel(app: app)
        let speaker = panel.radioSpeaker
        #expect(await ShotWait.until { speaker.volume == 40 })
        #expect(speaker.isEnabled && speaker.reason == nil && speaker.note == nil)

        speaker.setVolume(72.4)
        let write = await keep(station, "radioSpeakerVolume")
        #expect(write?.properties.first?.value == .i64(72))
        #expect(await ShotWait.until { speaker.volume == 72 })

        // A change made on another window or phone moves the slider here.
        await station.deliver(Self.radioDelta(["radioSpeakerVolume": .i64(15)]))
        #expect(await ShotWait.until { speaker.volume == 15 })
        await app.disconnect()
    }

    @Test("Mute radio speaker toggles the Core's mute; Mute this phone mutes only the phone")
    func radioSpeakerMuteIsTheCores() async throws {
        let (app, station) = try await connected(speaker: true)
        let panel = SoundPanel(app: app)
        let speaker = panel.radioSpeaker
        #expect(await ShotWait.until { speaker.volume == 40 })

        speaker.setMuted(true)
        #expect(await keep(station, "radioSpeakerMuted")?.properties.first?.value == .bool(true))
        #expect(await ShotWait.until { speaker.muted })
        speaker.setMuted(false)
        #expect(await ShotWait.until {
            Self.radioWrites(station).filter { $0.properties.first?.name == "radioSpeakerMuted" }.count == 2
        })
        #expect(Self.radioWrites(station).last?.properties.first?.value == .bool(false))

        // The phone's own mute asks the Core for no sound, and leaves the radio speaker alone.
        let before = Self.radioWrites(station).count
        panel.setMuted(true)
        #expect(app.audioMuted)
        for _ in 0..<2_000 { await Task.yield() }
        #expect(Self.radioWrites(station).count == before)
        panel.setMuted(false)
        #expect(!app.audioMuted)
        await app.disconnect()
    }

    @Test("a Core without the radio speaker greys the section with its reason and sends nothing")
    func olderCoreGreysTheRadioSpeaker() async throws {
        let (app, station) = try await connected(speaker: false)
        let speaker = SoundPanel(app: app).radioSpeaker
        #expect(await ShotWait.until { speaker.reason == RadioSpeakerModel.olderCoreReason })
        #expect(speaker.reason == "This Core can't set the radio speaker. Update the Core.")
        #expect(!speaker.isEnabled && speaker.volume == nil)
        speaker.setVolume(80)
        speaker.setMuted(true)
        for _ in 0..<2_000 { await Task.yield() }
        #expect(Self.radioWrites(station).isEmpty)
        await app.disconnect()
    }

    @Test("with no radio at the Core, or no Core, the section is greyed: No radio connected")
    func noRadioGreysTheRadioSpeaker() async throws {
        let (app, station) = try await connected(speaker: true, availability: 0)
        let speaker = app.radioSpeaker
        #expect(await ShotWait.until { speaker.reason == "No radio connected" })
        speaker.setVolume(80)
        for _ in 0..<2_000 { await Task.yield() }
        #expect(Self.radioWrites(station).isEmpty)
        // The radio comes: the section opens.
        await station.deliver(Self.radioDelta(["radioSpeakerAvailability": .i64(1)]))
        #expect(await ShotWait.until { speaker.isEnabled && speaker.volume == 40 })
        await app.disconnect()
        #expect(await ShotWait.until { speaker.reason == RadioSpeakerModel.noRadioReason })

        let alone = AppModel()
        #expect(alone.radioSpeaker.reason == RadioSpeakerModel.noRadioReason)
    }

    @Test("on a Hermes Lite 2 the section stays live and notes the add-on board")
    func hermesLiteNotesTheAddOn() async throws {
        let (app, station) = try await connected(speaker: true, availability: 2)
        let speaker = app.radioSpeaker
        #expect(await ShotWait.until { speaker.note == RadioSpeakerModel.addOnNote })
        #expect(speaker.isEnabled)
        speaker.setVolume(55)
        #expect(await keep(station, "radioSpeakerVolume")?.properties.first?.value == .i64(55))
        await app.disconnect()
    }

    // MARK: The Core speaker (R-AUD-29)

    /// The Core's `coreSpeakerState`, as the Core writes it (keys sorted).
    nonisolated static func coreState(_ state: String, playing: String = "", chosen: String = "",
                          desktop: Bool = false) -> String {
        "{\"chosen\":\"\(chosen)\",\"desktop\":\(desktop),\"playing\":\"\(playing)\",\"state\":\"\(state)\"}"
    }

    nonisolated static let usb = "USB Audio Device"
    nonisolated static let headphones = "bcm2835 Headphones"
    nonisolated static let playingUsb = coreState("playing", playing: usb, chosen: usb)

    private static func writes(_ station: FakeStation, of property: String) -> [LinkMessage.PropertyWrite] {
        radioWrites(station).filter { $0.properties.first?.name == property }
    }

    @Test("the Core speaker shows while the Core plays; its slider writes the Core's level and follows it")
    func coreSpeakerVolumeFollowsTheCore() async throws {
        let (app, station) = try await connected(speaker: true, core: Self.playingUsb)
        let panel = SoundPanel(app: app)
        let core = panel.coreSpeaker
        #expect(await ShotWait.until { core.volume == 60 })
        #expect(core.isShown && core.isEnabled)
        #expect(core.reason == nil && core.missingNote == nil && core.refusal == nil)

        core.setVolume(33.6)
        let write = await keep(station, "coreSpeakerVolume")
        #expect(write?.properties.first?.value == .i64(34))
        #expect(await ShotWait.until { core.volume == 34 })

        // A change made on a desktop window or another phone moves the slider here.
        await station.deliver(Self.radioDelta(["coreSpeakerVolume": .i64(12)]))
        #expect(await ShotWait.until { core.volume == 12 })
        // The radio speaker is left alone.
        #expect(Self.writes(station, of: "radioSpeakerVolume").isEmpty)
        #expect(app.radioSpeaker.volume == 40)
        await app.disconnect()
    }

    @Test("Mute Core speaker toggles the Core's mute; neither other mute touches it")
    func coreSpeakerMuteIsTheCores() async throws {
        let (app, station) = try await connected(speaker: true, core: Self.playingUsb)
        let panel = SoundPanel(app: app)
        let core = panel.coreSpeaker
        #expect(await ShotWait.until { core.volume == 60 && app.radioSpeaker.volume == 40 })

        core.setMuted(true)
        #expect(await keep(station, "coreSpeakerMuted")?.properties.first?.value == .bool(true))
        #expect(await ShotWait.until { core.muted })
        #expect(!app.radioSpeaker.muted)

        // Mute radio speaker and Mute this phone leave the Core speaker alone.
        app.radioSpeaker.setMuted(true)
        #expect(await keep(station, "radioSpeakerMuted")?.properties.first?.value == .bool(true))
        panel.setMuted(true)
        #expect(app.audioMuted)
        for _ in 0..<2_000 { await Task.yield() }
        #expect(Self.writes(station, of: "coreSpeakerMuted").count == 1)
        #expect(core.muted)
        panel.setMuted(false)

        core.setMuted(false)
        #expect(await ShotWait.until { Self.writes(station, of: "coreSpeakerMuted").count == 2 })
        #expect(Self.writes(station, of: "coreSpeakerMuted").last?.properties.first?.value == .bool(false))
        await app.disconnect()
    }

    @Test("with its card missing, the section stays live and says in amber where it plays meanwhile")
    func coreSpeakerMissingCardNote() async throws {
        let unplugged = Self.coreState("notConnected", playing: Self.headphones, chosen: Self.usb)
        let (app, station) = try await connected(speaker: true, core: unplugged)
        let core = app.coreSpeaker
        #expect(await ShotWait.until { core.missingNote != nil })
        #expect(core.missingNote == "USB Audio Device is not connected at the Core. "
            + "Playing on the Core's default, bcm2835 Headphones, until it comes back.")
        #expect(core.isShown && core.isEnabled && core.volume == 60)
        core.setVolume(20)
        #expect(await keep(station, "coreSpeakerVolume")?.properties.first?.value == .i64(20))

        // Held by another program: the same, in use.
        await station.deliver(Self.radioDelta([
            "coreSpeakerState": .utf8(Self.coreState("inUse", playing: Self.headphones, chosen: Self.usb)),
        ]))
        #expect(await ShotWait.until { core.missingNote?.contains("in use") == true })
        #expect(core.missingNote == "USB Audio Device is in use by another program at the Core. "
            + "Playing on the Core's default, bcm2835 Headphones, until it comes back.")

        // It comes back: the note goes by itself.
        await station.deliver(Self.radioDelta(["coreSpeakerState": .utf8(Self.playingUsb)]))
        #expect(await ShotWait.until { core.missingNote == nil })
        #expect(core.isShown && core.isEnabled)
        await app.disconnect()
    }

    @Test("while the Core plays on no card the section is left out, and it comes back by itself", arguments: [
        SoundPanelTests.coreState("noCard"),
        SoundPanelTests.coreState("waitingForPick", desktop: true),
        SoundPanelTests.coreState("notConnected", chosen: SoundPanelTests.usb),
        SoundPanelTests.coreState("inUse", chosen: SoundPanelTests.usb),
    ])
    func coreSpeakerLeftOutWithNoCard(state: String) async throws {
        let (app, station) = try await connected(speaker: true, core: state)
        let core = app.coreSpeaker
        #expect(await ShotWait.until { !core.isShown })
        #expect(core.volume == nil && core.missingNote == nil && core.reason == nil)
        core.setVolume(80)
        core.setMuted(true)
        for _ in 0..<2_000 { await Task.yield() }
        #expect(Self.writes(station, of: "coreSpeakerVolume").isEmpty)
        #expect(Self.writes(station, of: "coreSpeakerMuted").isEmpty)
        // The radio speaker stays as it is.
        #expect(app.radioSpeaker.isEnabled && app.radioSpeaker.volume == 40)

        // A card is plugged into the Core: the section comes back with no action here.
        await station.deliver(Self.radioDelta(["coreSpeakerState": .utf8(Self.playingUsb)]))
        #expect(await ShotWait.until { core.isShown && core.volume == 60 })
        #expect(core.isEnabled)
        await app.disconnect()
    }

    @Test("a Core without the Core speaker greys the section with its reason and sends nothing")
    func olderCoreGreysTheCoreSpeaker() async throws {
        let (app, station) = try await connected(speaker: true)
        let core = app.coreSpeaker
        #expect(await ShotWait.until { core.reason == CoreSpeakerModel.olderCoreReason })
        #expect(core.reason == "This Core can't set its speaker from here. Update the Core.")
        #expect(core.isShown && !core.isEnabled && core.volume == nil)
        core.setVolume(80)
        core.setMuted(true)
        for _ in 0..<2_000 { await Task.yield() }
        #expect(Self.writes(station, of: "coreSpeakerVolume").isEmpty)
        #expect(Self.writes(station, of: "coreSpeakerMuted").isEmpty)
        await app.disconnect()
    }

    @Test("with no Core reachable the section is greyed: Connect to the Core to change these")
    func unreachableGreysTheCoreSpeaker() async throws {
        let alone = AppModel()
        #expect(alone.coreSpeaker.reason == "Connect to the Core to change these.")
        #expect(alone.coreSpeaker.isShown && !alone.coreSpeaker.isEnabled)

        let (app, _) = try await connected(speaker: true, core: Self.playingUsb)
        let core = app.coreSpeaker
        #expect(await ShotWait.until { core.isEnabled && core.volume == 60 })
        await app.disconnect()
        #expect(await ShotWait.until { core.reason == CoreSpeakerModel.unreachableReason })
        #expect(core.isShown && core.volume == nil)
    }

    @Test("the state's text reads in any key order and refuses anything else")
    func coreSpeakerStateDecodes() {
        let state = CoreSpeakerModel.State.decode(#"{"state":"inUse","playing":"A","desktop":true,"chosen":"B"}"#)
        #expect(state == CoreSpeakerModel.State(state: .inUse, playing: "A", chosen: "B", desktop: true))
        #expect(CoreSpeakerModel.State.decode(#"{"chosen":"","desktop":false,"playing":"","state":"asleep"}"#) == nil)
        #expect(CoreSpeakerModel.State.decode(#"{"chosen":"","playing":"","state":"playing"}"#) == nil)
        #expect(CoreSpeakerModel.State.decode("") == nil)
    }

    // MARK: Pictures (V-UI-5, against phone-sound-panel.html and phone-core-speaker-mockup.html)

    @Test("the panel with the radio speaker, available and greyed")
    func pictures() async throws {
        let audio = await playing(FakeAudioSession(device: .bluetooth("AirPods Pro")))
        let (app, _) = try await connected(speaker: true, audio: audio)
        #expect(await ShotWait.until { app.radioSpeaker.volume == 40 })
        try await shoot("sound-panel-radio-speaker-available", app: app)
        await app.disconnect()

        let (older, _) = try await connected(speaker: false, audio: controller(FakeAudioSession()))
        #expect(await ShotWait.until { older.radioSpeaker.reason == RadioSpeakerModel.olderCoreReason })
        try await shoot("sound-panel-radio-speaker-older-core", app: older)
        await older.disconnect()

        let (none, _) = try await connected(speaker: true, availability: 0, audio: controller(FakeAudioSession()))
        #expect(await ShotWait.until { none.radioSpeaker.reason == RadioSpeakerModel.noRadioReason })
        try await shoot("sound-panel-radio-speaker-no-radio", app: none)
        await none.disconnect()
    }

    @Test("the panel with the Core speaker in each state")
    func coreSpeakerPictures() async throws {
        let size = CGSize(width: 290, height: 680)
        let states: [(name: String, state: String, shown: Bool)] = [
            ("playing", Self.playingUsb, true),
            ("not-connected", Self.coreState("notConnected", playing: Self.headphones, chosen: Self.usb), true),
            ("in-use", Self.coreState("inUse", playing: Self.headphones, chosen: Self.usb), true),
            ("no-card", Self.coreState("noCard"), false),
            ("waiting-for-pick", Self.coreState("waitingForPick", desktop: true), false),
        ]
        for entry in states {
            let audio = await playing(FakeAudioSession(device: .bluetooth("AirPods Pro")))
            let (app, _) = try await connected(speaker: true, core: entry.state, audio: audio)
            #expect(await ShotWait.until { app.radioSpeaker.volume == 40 })
            #expect(await ShotWait.until {
                app.coreSpeaker.isShown == entry.shown && (app.coreSpeaker.volume == 60) == entry.shown
            }, "\(entry.name)")
            try await shoot("sound-panel-core-speaker-\(entry.name)", app: app, size: size)
            await app.disconnect()
        }

        let (older, _) = try await connected(speaker: true, audio: controller(FakeAudioSession()))
        #expect(await ShotWait.until { older.coreSpeaker.reason == CoreSpeakerModel.olderCoreReason })
        #expect(await ShotWait.until { older.radioSpeaker.volume == 40 })
        try await shoot("sound-panel-core-speaker-older-core", app: older, size: size)
        await older.disconnect()

        let (gone, _) = try await connected(speaker: true, core: Self.playingUsb, audio: controller(FakeAudioSession()))
        #expect(await ShotWait.until { gone.coreSpeaker.volume == 60 })
        await gone.disconnect()
        #expect(await ShotWait.until { gone.coreSpeaker.reason == CoreSpeakerModel.unreachableReason })
        try await shoot("sound-panel-core-speaker-unreachable", app: gone, size: size)
    }

    private func shoot(_ name: String, app: AppModel, size: CGSize = CGSize(width: 290, height: 520)) async throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty else {
            return
        }
        let window = try BandFlagShotTests.window(size: size)
        let root = ZStack(alignment: .top) {
            Color.black
            SoundPanel(app: app)
                .clipShape(RoundedRectangle(cornerRadius: 12))
                .padding(.top, 20)
        }
        .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        host.overrideUserInterfaceStyle = .dark
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        for _ in 0..<20 {
            window.layoutIfNeeded()
            await Task.yield()
        }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
        try #require(image.pngData()).write(to: url)
        print("Wrote \(url.path)")
    }
}
