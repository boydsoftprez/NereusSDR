// NereusSDR for iOS: the audio quality on this phone: High, Save data and Lossless, what each asks of the Core, greyed rows and the Core's answers
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import SwiftUI
import UIKit
import NereusKitTesting
import NereusLink
import NereusMedia
@testable import NereusSDR
import Testing

/// R-IOS-09 and R-IOS-20: Setup, Audio, On this phone, Audio quality.
/// High (Opus at 48 kbit/s, the default), Save data (Opus at 24 kbit/s)
/// and Lossless (about 1.6 Mbit/s, for digital modes), each with its cost
/// an hour, kept on this phone. The bitrate goes to a Core that offers the
/// choice and only from its catalogue; a row the Core does not offer is
/// greyed with its reason, never hidden; a Core's refusal shows in its
/// words and the tick goes back to what it runs; Lossless asks as the
/// desktop asks and falls back to Opus in the desktop's words. The
/// microphone follows: 48 kbit/s full band, 24 under Save data, lossless
/// where the line carries it.
@Suite("Audio quality on this phone", .serialized)
@MainActor
struct AudioQualityTests {
    typealias Model = AudioQualityModel
    typealias Inputs = AudioQualityModel.Inputs
    typealias Heard = AudioQualityModel.Heard

    private func settings() throws -> PhoneSettings {
        let defaults = try #require(UserDefaults(suiteName: "AudioQualityTests-\(UUID().uuidString)"))
        return PhoneSettings(defaults: defaults)
    }

    /// A Core with the quality, the profile and the measured table.
    static func offered(_ chosen: AudioQualityChoice, heard: Heard? = nil, fallback: Bool = false,
                        bitrates: [Int]? = [24000, 48000]) -> Inputs {
        Inputs(chosen: chosen, connected: true, qualityOffered: true, profileOffered: true, opusBitrates: bitrates,
               heard: heard, fallback: fallback)
    }

    // MARK: The words

    /// A fixed 24 MB talking estimate conceals the much larger negotiated
    /// Lossless microphone cost. The figures must also describe Save data
    /// and explain why a Lossless choice can still use the High rate.
    @Test("Data use qualifies the receive totals and every microphone format's estimated cost")
    func dataUseEstimatesCoverLosslessAndItsFallback() {
        let words = DataUsePage.footnote
        #expect(words.contains("The figures above include High audio."))
        #expect(words.contains("Save data audio uses about 13 MB an hour"))
        #expect(words.contains("Lossless audio uses about 720 MB an hour while the connection carries it"))
        #expect(words.contains("24 MB for each hour of talking at High, 13 MB at Save data"))
        #expect(words.contains("720 MB while the microphone connection carries Lossless"))
        #expect(words.contains("If it cannot, the microphone uses High."))
        #expect(words.contains("These are estimates; actual data use varies."))
    }

    @Test("High is the default, kept on this phone, and a value this build does not know reads as High")
    func theDefault() throws {
        let settings = try settings()
        #expect(settings.audioQuality == .high)
        settings.audioQuality = .saveData
        #expect(settings.audioQuality == .saveData)
        settings.setString("loud", for: PhoneSettings.audioQualityKey)
        #expect(settings.audioQuality == .high)
        #expect(PhoneSettings.audioQualityKey == "audio.quality")
    }

    @Test("the rows in plain words, each with its cost an hour")
    func theRows() {
        let rows = Model.rows(Self.offered(.high))
        #expect(rows.map(\.title) == ["High", "Save data", "Lossless"])
        #expect(rows.map(\.detail) == ["Opus at 48 kbit/s, audio up to 20 kHz",
                                       "Opus at 24 kbit/s, audio up to 8 kHz",
                                       "The Core's audio unchanged, for digital modes"])
        #expect(rows.map(\.cost) == ["About 24 MB an hour", "About 13 MB an hour", "About 720 MB an hour"])
        #expect(rows.allSatisfy { $0.enabled && $0.reason == nil })
        #expect(Model.footer == "High and Save data are compressed. Lossless needs about 1.6 Mbit/s; if the "
            + "network cannot carry it, audio stays on Opus. Saved on this phone.")
        for text in rows.flatMap({ [$0.title, $0.detail, $0.cost] }) + [Model.footer] {
            #expect(!text.contains("\u{2014}"), "no em dashes")
        }
    }

    // MARK: What goes to the Core

    @Test(arguments: [
        (AudioQualityChoice.high, false, Optional(48000)), (.saveData, false, 24000), (.lossless, true, 48000),
    ])
    func eachChoiceAsks(choice: AudioQualityChoice, lossless: Bool, bitrate: Int?) {
        #expect(Model.request(Self.offered(choice))
                == MediaControlClient.AudioRequest(lossless: lossless, opusBitrate: bitrate))
    }

    @Test("only the catalogue's bitrates go; a Core without the table gets none")
    func onlyTheCataloguesBitrates() {
        #expect(Model.request(Self.offered(.saveData, bitrates: [48000])).opusBitrate == nil)
        #expect(Model.request(Self.offered(.high, bitrates: nil)).opusBitrate == nil)
        #expect(Model.request(Self.offered(.lossless, bitrates: [24000])).opusBitrate == nil)
        #expect(Model.request(Self.offered(.lossless, bitrates: [24000])).lossless)
    }

    @Test("the microphone follows the choice")
    func theMicrophoneFollows() {
        #expect(Model.microphone(.high) == .high)
        #expect(Model.microphone(.saveData) == .saveData)
        #expect(Model.microphone(.lossless) == .lossless)
    }

    // MARK: Greyed, never hidden

    @Test("an older Core greys Save data with its reason and Lossless as the desktop says; High stays choosable")
    func anOlderCore() {
        var inputs = Self.offered(.lossless)
        inputs.qualityOffered = false
        inputs.opusBitrates = nil
        let rows = Model.rows(inputs)
        #expect(rows.count == 3)
        #expect(rows.map(\.enabled) == [true, false, true], "High asks with no bitrate: never stuck on Lossless")
        #expect(rows[0].reason == nil)
        #expect(rows[1].reason == Model.olderCoreReason)
        #expect(Model.olderCoreReason
                == "This Core does not offer a choice of audio quality. Updating the Core may help.")
        #expect(Model.request(inputs) == MediaControlClient.AudioRequest(lossless: true, opusBitrate: nil))
        inputs.chosen = .high
        #expect(Model.request(inputs) == MediaControlClient.AudioRequest(lossless: false, opusBitrate: nil))

        inputs.profileOffered = false
        let older = Model.rows(inputs)
        #expect(older.map(\.enabled) == [true, false, false])
        #expect(older[2].reason == "This Core cannot send lossless audio.")
    }

    @Test("while the catalogue comes, Save data is being checked, not greyed as not offered, and High can be chosen")
    func whileTheCatalogueComes() {
        var inputs = Self.offered(.lossless, bitrates: nil)
        inputs.catalogueLoaded = false
        let rows = Model.rows(inputs)
        #expect(rows.map(\.enabled) == [true, false, true])
        #expect(rows[0].reason == nil)
        #expect(rows[1].reason == "Checking what this Core offers.")
        #expect(!rows.contains { $0.reason == Model.notOfferedReason })
        // Once it comes, Save data is offered only from the table; High asks
        // with no bitrate when the table lacks it, so it stays choosable.
        inputs.catalogueLoaded = true
        inputs.opusBitrates = [24000]
        #expect(Model.rows(inputs).map(\.reason) == [nil, nil, nil])
        inputs.chosen = .high
        #expect(Model.request(inputs) == MediaControlClient.AudioRequest(lossless: false, opusBitrate: nil))
        inputs.opusBitrates = nil
        #expect(Model.rows(inputs).map(\.reason) == [nil, Model.notOfferedReason, nil])
    }

    @Test("a quality missing from the catalogue is greyed with its reason")
    func notInTheCatalogue() {
        let rows = Model.rows(Self.offered(.high, bitrates: [48000]))
        #expect(rows.map(\.enabled) == [true, false, true])
        #expect(rows[1].reason == "This Core does not offer this audio quality.")
    }

    @Test("a Core that does not allow lossless greys it in the desktop's words")
    func notAllowed() {
        let heard = Heard(lossless: false, opusBitrate: 48000, profileRefusal: .notAllowed, bitrateRefusal: nil)
        let inputs = Self.offered(.lossless, heard: heard)
        let rows = Model.rows(inputs)
        #expect(rows[2].enabled == false)
        #expect(rows[2].reason == "This Core does not allow lossless audio.")
        #expect(Model.shown(inputs) == .high, "the tick goes to the Opus the Core runs")
        #expect(Model.notes(inputs).isEmpty, "said once, on the row")
        // The same for a Core without the profile.
        var older = Self.offered(.lossless)
        older.profileOffered = false
        #expect(Model.rows(older)[2].reason == "This Core cannot send lossless audio.")
        #expect(Model.notes(older).isEmpty, "said once, on the row")
    }

    @Test("a bitrate the rows do not list ticks nothing")
    func anUnlistedBitrate() {
        let odd = Heard(lossless: false, opusBitrate: 32000, profileRefusal: nil, bitrateRefusal: nil)
        #expect(Model.shown(Self.offered(.high, heard: odd)) == nil)
        #expect(Model.shown(Self.offered(.saveData, heard: odd)) == nil)
    }

    @Test("on cellular with Lossless chosen, its row says its cost plainly")
    func losslessOnCellular() {
        var inputs = Self.offered(.lossless)
        inputs.cellular = true
        let rows = Model.rows(inputs)
        #expect(rows[2].warning == "On cellular, Lossless uses about 720 MB an hour.")
        #expect(rows[0].warning == nil && rows[1].warning == nil)
        inputs.cellular = false
        #expect(Model.rows(inputs).allSatisfy { $0.warning == nil }, "not on Wi-Fi")
        var high = Self.offered(.high)
        high.cellular = true
        #expect(Model.rows(high).allSatisfy { $0.warning == nil }, "only while Lossless is chosen")
    }

    @Test("away from a Core every row can be chosen, for the next connection")
    func notConnected() {
        let inputs = Inputs(chosen: .saveData, connected: false, qualityOffered: false, profileOffered: false,
                            opusBitrates: nil, heard: nil, fallback: false)
        #expect(Model.rows(inputs).allSatisfy { $0.enabled && $0.reason == nil })
        #expect(Model.shown(inputs) == .saveData)
        #expect(Model.notes(inputs).isEmpty)
    }

    // MARK: The Core's answers

    @Test("the Core's refusal shows in its words and the tick goes back to what it runs")
    func theRefusalReverts() {
        let words = "This Core does not offer that audio quality. The audio stays as it was."
        let heard = Heard(lossless: false, opusBitrate: 48000, profileRefusal: nil, bitrateRefusal: words)
        let inputs = Self.offered(.saveData, heard: heard)
        #expect(Model.shown(inputs) == .high)
        #expect(Model.notes(inputs) == [words])
    }

    @Test("what the Core runs is ticked; before it says, the choice is")
    func theTick() {
        #expect(Model.shown(Self.offered(.saveData)) == .saveData)
        let saving = Heard(lossless: false, opusBitrate: 24000, profileRefusal: nil, bitrateRefusal: nil)
        #expect(Model.shown(Self.offered(.saveData, heard: saving)) == .saveData)
        let lossless = Heard(lossless: true, opusBitrate: nil, profileRefusal: nil, bitrateRefusal: nil)
        #expect(Model.shown(Self.offered(.lossless, heard: lossless)) == .lossless)
        #expect(Model.notes(Self.offered(.lossless, heard: lossless)).isEmpty)
    }

    @Test("the lossless fall back and the connection's refusal, in the desktop's words")
    func theFallBack() {
        let opus = Heard(lossless: false, opusBitrate: 48000, profileRefusal: nil, bitrateRefusal: nil)
        let fell = Self.offered(.lossless, heard: opus, fallback: true)
        #expect(Model.notes(fell) == ["The network could not carry lossless audio; staying on Opus."])
        #expect(Model.shown(fell) == .high)
        #expect(Model.rows(fell)[2].enabled, "Lossless can be chosen again")

        let unavailable = Heard(lossless: false, opusBitrate: 48000, profileRefusal: .unavailable, bitrateRefusal: nil)
        #expect(Model.notes(Self.offered(.lossless, heard: unavailable))
                == ["This connection could not set up lossless audio; staying on Opus."])
        #expect(Model.notes(Self.offered(.high, heard: unavailable)).isEmpty, "only while Lossless is chosen")
    }

    // MARK: Against the fake Core

    @MainActor
    struct Rig {
        let model: AppModel
        let station: FakeStation
        var quality: AudioQualityModel { model.audioQuality }
    }

    private func connected(choice: AudioQualityChoice = .high, lossless: FakeStation.LosslessAnswer = .given,
                           additions: FakeStation.Additions = [.wideband, .audioQuality]) async throws -> Rig {
        let settings = try settings()
        settings.audioQuality = choice
        let station = try FakeStation(additions: additions)
        station.losslessAnswer = lossless
        let model = AppModel(phoneSettings: settings, mediaPeerFactory: station.mediaPeerFactory)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        try await MainScreenShotTests.fill(station)
        #expect(await ShotWait.until { model.connection == .connected && model.audioQuality.inputs.connected })
        #expect(await ShotWait.until { model.audioQuality.inputs.opusBitrates == [24000, 48000] })
        return Rig(model: model, station: station)
    }

    @Test("the first audio carries the saved choice, and a new choice goes at once with the microphone")
    func againstTheFake() async throws {
        let rig = try await connected(choice: .saveData)
        #expect(await ShotWait.until { rig.station.audioRequests.last?.opusBitrate == 24000 })
        #expect(rig.model.media.uplink.microphoneQuality == .saveData)
        #expect(await ShotWait.until { rig.quality.inputs.heard?.opusBitrate == 24000 })
        #expect(rig.quality.shown == .saveData)

        rig.quality.choose(.high)
        #expect(await ShotWait.until { rig.station.audioRequests.last?.opusBitrate == 48000 })
        #expect(rig.model.phoneSettings.audioQuality == .high)
        #expect(rig.model.media.uplink.microphoneQuality == .high)
        #expect(await ShotWait.until { rig.quality.inputs.heard?.opusBitrate == 48000 })
        #expect(rig.quality.shown == .high)
        await rig.model.disconnect()
    }

    @Test("after connecting, High's audio starts once, whether the catalogue or the audio comes first")
    func theAudioStartsOnce() async throws {
        let rig = try await connected(choice: .high)
        #expect(await ShotWait.until { rig.quality.inputs.heard?.opusBitrate == 48000 })
        await rig.quality.settle()
        // Anything already sent arrives before this, in order.
        await rig.model.media.setAudioEnabled(false)
        #expect(await ShotWait.until { rig.station.audioRequests.last?.enabled == false })
        let asked = rig.station.audioRequests.filter(\.enabled)
        #expect(asked.count == 1, "\(asked)")
        await rig.model.disconnect()
    }

    @Test("Lossless against the fake: given, the line carries it; not allowed, greyed and Opus plays")
    func losslessAgainstTheFake() async throws {
        let rig = try await connected(choice: .lossless)
        #expect(await ShotWait.until { rig.station.audioRequests.last?.profile == "lossless" })
        #expect(await ShotWait.until { rig.quality.shown == .lossless })
        #expect(rig.model.media.uplink.microphoneQuality == .lossless)
        await rig.model.disconnect()

        let denied = try await connected(choice: .lossless, lossless: .notAllowed)
        #expect(await ShotWait.until { denied.quality.inputs.heard?.profileRefusal == .notAllowed })
        #expect(denied.quality.shown == .high)
        #expect(denied.quality.rows[2].enabled == false)
        #expect(denied.quality.rows[2].reason == "This Core does not allow lossless audio.")
        #expect(denied.quality.notes.isEmpty, "said once, on the row")
        await denied.model.disconnect()
    }

    @Test("a Core without the quality gets no bitrate, and Save data is greyed while High stays choosable")
    func anOlderFake() async throws {
        let settings = try settings()
        let station = try FakeStation(additions: [.wideband])
        let model = AppModel(phoneSettings: settings, mediaPeerFactory: station.mediaPeerFactory)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await ShotWait.until { model.connection == .connected && model.audioQuality.inputs.connected })
        #expect(model.audioQuality.rows.map(\.enabled).prefix(2) == [true, false])
        #expect(await station.waitForMessage { message in
            if case .mediaControl(let control) = message {
                return control.payload["op"] == .string("audio")
            }
            return false
        } != nil)
        let audio = station.messages.compactMap { message -> [String: LinkJSON]? in
            if case .mediaControl(let control) = message, control.payload["op"] == .string("audio") {
                return control.payload
            }
            return nil
        }
        #expect(audio.allSatisfy { $0["opusBitrate"] == nil })
        await model.disconnect()
    }

    @Test("the lossless fall back reaches the page")
    func theFallBackReachesThePage() throws {
        let settings = try settings()
        settings.audioQuality = .lossless
        let model = AppModel(phoneSettings: settings)
        model.audioQuality.receive(.losslessFallback)
        #expect(model.audioQuality.inputs.fallback)
        model.audioQuality.choose(.lossless)
        #expect(!model.audioQuality.inputs.fallback, "choosing it again gives it a fresh chance")
    }

    // MARK: The microphone's encoding

    @Test("the microphone encodes L16 only on a line that carries it, else Opus at the choice's rate")
    func theMicrophonesEncoding() {
        // The uplink's own rule (chosen, not fallen back, carried by the
        // line) is NereusKit's MicrophoneLosslessTests.
        #expect(MicCapture.encoding(sendsLossless: true, profile: .microphone) == .l16)
        let uplink = MediaUplink()
        uplink.microphoneQuality = .lossless
        #expect(MicCapture.encoding(for: uplink) == .opus(.microphone), "no line carrying lossless: Opus")
        uplink.microphoneQuality = .saveData
        #expect(MicCapture.encoding(for: uplink) == .opus(.microphoneSaveData))
        uplink.microphoneQuality = .high
        #expect(MicCapture.encoding(for: uplink) == .opus(.microphone))
    }

    @Test("each frame goes in the format the line takes at its turn, so a fall back mid-key leaves L16")
    func theFormatIsDecidedEachFrame() throws {
        // The desktop's sendMicAudio decides on each pump (RemoteMediaController.cpp).
        var lossless = true
        var l16: [Data] = []
        var opus: [Data] = []
        let frame = MicCapture.heldFrame([Float](repeating: 0.25, count: 960))
        #expect(MicCapture.heldSamples(frame) == [Float](repeating: 0.25, count: 960))
        func send() -> Bool {
            MicCapture.sendFrame(frame, lossless: lossless, opus: { _ in Data([0xfc]) },
                                 sendL16: { l16.append($0); return true },
                                 sendOpus: { opus.append($0); return true })
        }
        #expect(send())
        #expect(l16.count == 1 && l16[0] == L16Audio.microphoneFrame(mono: [Float](repeating: 0.25, count: 960)))
        lossless = false // the link trial fell back between two frames
        #expect(send())
        #expect(opus == [Data([0xfc])])
        #expect(l16.count == 1, "nothing more as L16")
        lossless = true // chosen again
        #expect(send())
        #expect(l16.count == 2)
        // An encoder that fails sends nothing.
        #expect(!MicCapture.sendFrame(frame, lossless: false, opus: { _ in throw CancellationError() },
                                      sendL16: { _ in true }, sendOpus: { _ in true }))
    }

    // MARK: Pictures

    /// Draws the page and, with `NEREUS_MAIN_SHOTS` set (through
    /// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), writes it there.
    private func shoot(_ name: String, model: AppModel, scheme: ColorScheme, size: DynamicTypeSize) async throws {
        let frame = CGSize(width: 402, height: 1500)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: frame)
        window.windowLevel = .alert + 1
        // At large type the whole page is too tall to draw: the section alone.
        let root = NavigationStack {
            if size.isAccessibilitySize {
                List {
                    AudioQualitySection(model: model.audioQuality)
                }
                .navigationTitle("On this phone")
            } else {
                AudioOnThisPhonePage(settings: model.phoneSettings, quality: model.audioQuality)
            }
        }
        .preferredColorScheme(scheme)
        .dynamicTypeSize(size)
        let host = UIHostingController(rootView: root)
        host.overrideUserInterfaceStyle = scheme == .dark ? .dark : .light
        host.view.frame = CGRect(origin: .zero, size: frame)
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

    @Test("pictures: the rows in light and dark, at regular and large type, and greyed")
    func pictures() async throws {
        let rig = try await connected(choice: .high)
        #expect(await ShotWait.until { rig.quality.inputs.heard?.opusBitrate == 48000 })
        for scheme in [ColorScheme.light, .dark] {
            for size in [DynamicTypeSize.large, .accessibility3] {
                let name = "audioquality-\(scheme == .dark ? "dark" : "light")-\(size == .large ? "regular" : "large")"
                try await shoot(name, model: rig.model, scheme: scheme, size: size)
            }
        }
        await rig.model.disconnect()

        let denied = try await connected(choice: .lossless, lossless: .notAllowed)
        #expect(await ShotWait.until { denied.quality.inputs.heard?.profileRefusal == .notAllowed })
        try await shoot("audioquality-lossless-not-allowed-dark", model: denied.model, scheme: .dark, size: .large)
        await denied.model.disconnect()

        let settings = try settings()
        let station = try FakeStation(additions: [.wideband])
        let older = AppModel(phoneSettings: settings, mediaPeerFactory: station.mediaPeerFactory)
        await older.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await ShotWait.until { older.connection == .connected && older.audioQuality.inputs.connected })
        for scheme in [ColorScheme.light, .dark] {
            try await shoot("audioquality-greyed-\(scheme == .dark ? "dark" : "light")", model: older,
                            scheme: scheme, size: .large)
        }
        await older.disconnect()
    }
}
