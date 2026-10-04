// NereusSDR for iOS: Setup's Audio page on this phone: its rows, and what each choice keeps and applies
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Foundation
@testable import NereusSDR
import Testing

/// R-IOS-20 (Task 55): the board's words for each row, the defaults (the
/// iPhone's microphone, both While you transmit settings on), and each
/// choice kept on this phone and applied to the band's sound at once.
@Suite("Audio on this phone")
@MainActor
struct AudioOnThisPhonePageTests {
    private func settings() throws -> PhoneSettings {
        let defaults = try #require(UserDefaults(suiteName: "AudioOnThisPhonePageTests-\(UUID().uuidString)"))
        return PhoneSettings(defaults: defaults)
    }

    @Test("the defaults: the iPhone's microphone and the band muted while you talk")
    func defaults() throws {
        let settings = try settings()
        #expect(settings.microphone == .iPhone)
        #expect(settings.muteBandWhileTalking)
    }

    @Test("MON in headphones only is the rule, stated under While you transmit, not a switch (D80, audit M12)")
    func monRule() throws {
        let settings = try settings()
        let audio = AudioSessionController(session: FakeAudioSession(device: nil), output: FakePlaybackOutput(),
                                           notificationCenter: NotificationCenter())
        let page = AudioOnThisPhonePage(settings: settings, audio: audio)
        page.setMuteBandWhileTalking(false)
        #expect(audio.monInHeadphonesOnly)
        page.setMuteBandWhileTalking(true)
        #expect(audio.monInHeadphonesOnly)
        #expect(AudioOnThisPhonePage.monRule
            == "Monitor (MON) plays in headphones only, so the speaker can't feed back into the microphone.")
    }

    @Test("the audio quality's rows are offered, High by default (R-IOS-09; AudioQualityTests has the rest)")
    func qualityRows() throws {
        let settings = try settings()
        #expect(settings.audioQuality == .high)
        #expect(AudioQualityModel.rows(AudioQualityModel.Inputs(
            chosen: settings.audioQuality, connected: false, qualityOffered: false, profileOffered: false,
            opusBitrates: nil, heard: nil, fallback: false, catalogueLoaded: false)).map(\.title) == ["High", "Save data", "Lossless"])
    }

    @Test("the route rows in the board's words, with a tick on the current one")
    func routeRows() {
        let rows = AudioOnThisPhonePage.routeRows(routes: [.speaker, .earpiece, .external(name: "AirPods Pro")],
                                                  current: .external(name: "AirPods Pro"), isPhone: true)
        #expect(rows.map(\.title) == ["iPhone speaker", "Earpiece", "AirPods Pro"])
        #expect(rows.map(\.detail) == ["The default", "Hold it to your ear, like a call",
                                       "Connected \u{00B7} full quality"])
        #expect(rows.map(\.isCurrent) == [false, false, true])
        #expect(AudioOnThisPhonePage.title(.iPhone) == "iPhone microphone")
        #expect(AudioOnThisPhonePage.title(.airPods) == "AirPods microphone")
    }

    @Test("each choice is kept on this phone and applied to the sound at once")
    func choicesApply() async throws {
        let settings = try settings()
        let session = FakeAudioSession(device: .bluetooth("AirPods Pro"))
        let output = FakePlaybackOutput()
        let audio = AudioSessionController(session: session, output: output, notificationCenter: NotificationCenter())
        audio.start()
        await audio.settle()
        await audio.settle()
        let page = AudioOnThisPhonePage(settings: settings, audio: audio)

        page.choose(.airPods)
        await audio.settle()
        #expect(settings.microphone == .airPods)
        #expect(audio.microphone == .airPods)
        #expect(session.categories.last?.options.contains(.allowBluetoothHFP) == true)

        page.choose(.speaker)
        await audio.settle()
        await audio.settle()
        #expect(audio.route == .speaker)

        audio.setTransmitting(true, monitorOn: false)
        #expect(output.isMuted)
        page.setMuteBandWhileTalking(false)
        #expect(!settings.muteBandWhileTalking)
        #expect(!output.isMuted)
        #expect(audio.monInHeadphonesOnly)
    }
}
