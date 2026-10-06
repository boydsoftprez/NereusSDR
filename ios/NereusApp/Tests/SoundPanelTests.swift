// NereusSDR for iOS: the band's Sound panel mutes, lists, marks and moves the band's sound, in step with Setup
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Foundation
@testable import NereusSDR
import Testing

/// D77: one tap on the speaker button opens the Sound panel: Mute, then
/// Speaker, Earpiece and, only while connected, AirPods or headphones by
/// their own name, with a tick on the current one. Where the band plays is
/// the same choice Setup, Audio, On this phone makes, both ways.
@Suite("SoundPanel")
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

    @Test("the Mute switch mutes and unmutes the band")
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
}
