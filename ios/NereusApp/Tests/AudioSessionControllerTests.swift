// NereusSDR for iOS: the audio session's category, playback, interruptions and route policy
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Foundation
@testable import NereusSDR
import Testing

@Suite("AudioSessionController")
@MainActor
struct AudioSessionControllerTests {
    /// Each test posts on its own notification centre, so no two tests (and
    /// no real audio session) hear each other.
    private let center = NotificationCenter()

    private func make(_ session: FakeAudioSession = FakeAudioSession(),
                      _ output: FakePlaybackOutput = FakePlaybackOutput()) -> AudioSessionController {
        AudioSessionController(session: session, output: output, notificationCenter: center)
    }

    /// A controller that has started and finished activating.
    private func playing(_ session: FakeAudioSession = FakeAudioSession(),
                         _ output: FakePlaybackOutput = FakePlaybackOutput()) async -> AudioSessionController {
        let audio = make(session, output)
        audio.start()
        await audio.settle()
        // The route check the start queues after the output starts.
        await audio.settle()
        return audio
    }

    /// An interruption's end carries `.shouldResume` unless `resume` is false.
    private func postInterruption(_ type: AVAudioSession.InterruptionType, resume: Bool = true) {
        var info: [AnyHashable: Any] = [AVAudioSessionInterruptionTypeKey: type.rawValue]
        if type == .ended, resume {
            info[AVAudioSessionInterruptionOptionKey] = AVAudioSession.InterruptionOptions.shouldResume.rawValue
        }
        center.post(name: AVAudioSession.interruptionNotification, object: nil, userInfo: info)
    }

    private func postRouteChange(_ reason: AVAudioSession.RouteChangeReason) {
        center.post(name: AVAudioSession.routeChangeNotification, object: nil,
                    userInfo: [AVAudioSessionRouteChangeReasonKey: reason.rawValue])
    }

    private func postMediaServicesReset() {
        center.post(name: AVAudioSession.mediaServicesWereResetNotification, object: nil)
    }

    // MARK: The session

    @Test("starting sets play-and-record, default mode, speaker default and A2DP only, then plays")
    func startConfiguresTheSession() async throws {
        let session = FakeAudioSession()
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        let category = try #require(session.categories.last)
        #expect(category.category == .playAndRecord)
        #expect(category.mode == .default)
        #expect(category.options == [.defaultToSpeaker, .allowBluetoothA2DP])
        #expect(!category.options.contains(.allowBluetoothHFP))
        #expect(session.isActive)
        #expect(output.isRunning)
        #expect(audio.state == .playing)
        #expect(audio.route == .speaker)
    }

    @Test("the session is activated and deactivated off the main thread")
    func activationOffTheMainThread() async {
        let session = FakeAudioSession()
        let audio = await playing(session)
        audio.stop()
        await audio.settle()
        #expect(session.activations.map(\.active) == [true, false])
        #expect(session.activations.allSatisfy { !$0.onMainThread })
    }

    @Test("the category and mode stay the same for every route")
    func everyRouteKeepsTheCategory() async {
        let session = FakeAudioSession(device: .bluetooth("AirPods Pro"))
        let audio = await playing(session)
        for route in [AudioRoute.speaker, .earpiece, .external(name: "AirPods Pro")] {
            audio.select(route)
        }
        await audio.settle()
        #expect(session.categories.count == 4)
        #expect(session.categories.allSatisfy { $0.category == .playAndRecord && $0.mode == .default })
        #expect(session.categories.allSatisfy { !$0.options.contains(.allowBluetoothHFP) })
    }

    @Test("stopping stops playback and lets other apps' audio resume")
    func stopDeactivates() async throws {
        let session = FakeAudioSession()
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        audio.stop()
        await audio.settle()
        #expect(output.stops == 1)
        #expect(!session.isActive)
        let last = try #require(session.activations.last)
        #expect(last.options == .notifyOthersOnDeactivation)
        #expect(audio.state == .stopped)
    }

    @Test("starting twice starts once")
    func startIsIdempotent() async {
        let output = FakePlaybackOutput()
        let audio = make(FakeAudioSession(), output)
        audio.start()
        audio.start()
        await audio.settle()
        #expect(output.starts == 1)
    }

    @Test("a stop while the session is still activating never starts the output")
    func stopWhileActivating() async {
        let session = FakeAudioSession()
        let output = FakePlaybackOutput()
        let audio = make(session, output)
        audio.start()
        audio.stop()
        await audio.settle()
        #expect(output.starts == 0)
        #expect(!session.isActive)
        #expect(audio.state == .stopped)
    }

    // MARK: Interruptions

    @Test("an interruption beginning pauses playback and its end resumes it")
    func interruptionPausesAndResumes() async {
        let session = FakeAudioSession()
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)

        postInterruption(.began)
        #expect(output.pauses == 1)
        #expect(!output.isRunning)
        #expect(audio.state == .interrupted)

        postInterruption(.ended)
        await audio.settle()
        #expect(output.starts == 2)
        #expect(output.isRunning)
        #expect(session.isActive)
        #expect(audio.state == .playing)
    }

    @Test("an interruption that ends without asking to resume leaves the band paused with a notice")
    func interruptionEndedWithoutResume() async {
        let session = FakeAudioSession()
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        audio.select(.earpiece)
        await audio.settle()

        postInterruption(.began)
        postInterruption(.ended, resume: false)
        await audio.settle()
        #expect(output.starts == 1, "nothing plays again by itself")
        #expect(!output.isRunning)
        #expect(audio.state == .paused)
        #expect(audio.notice != nil, "the band screen says the sound is paused")
        #expect(audio.notice == .interruptionEnded)

        // The tap plays it again where it played, not forced to the speaker.
        audio.resumeFromNotice()
        await audio.settle()
        await audio.settle()
        #expect(audio.notice == nil)
        #expect(output.isRunning)
        #expect(audio.state == .playing)
        #expect(audio.route == .earpiece)
    }

    @Test("an interruption that begins and ends before the session answers lands in order")
    func interruptionsLandInOrder() async {
        let output = FakePlaybackOutput()
        let audio = make(FakeAudioSession(), output)
        audio.start()
        postInterruption(.began)
        postInterruption(.ended)
        await audio.settle()
        // The first activation's answer is stale by then; only the resume starts the output.
        #expect(output.starts == 1)
        #expect(output.isRunning)
        #expect(audio.state == .playing)
    }

    @Test("an interruption while stopped leaves playback stopped")
    func interruptionWhileStopped() async {
        let output = FakePlaybackOutput()
        let audio = make(FakeAudioSession(), output)
        postInterruption(.began)
        postInterruption(.ended)
        await audio.settle()
        #expect(output.starts == 0)
        #expect(output.pauses == 0)
        #expect(audio.state == .stopped)
    }

    @Test("resuming after an interruption keeps the chosen route")
    func interruptionKeepsTheRoute() async {
        let audio = await playing()
        audio.select(.earpiece)
        postInterruption(.began)
        postInterruption(.ended)
        await audio.settle()
        #expect(audio.route == .earpiece)
    }

    // MARK: Media services reset

    @Test("a media services reset rebuilds the session and the output and plays again")
    func mediaServicesResetWhilePlaying() async throws {
        let session = FakeAudioSession()
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        audio.select(.earpiece)
        await audio.settle()
        let categoriesBefore = session.categories.count

        postMediaServicesReset()
        await audio.settle()
        #expect(output.resets == 1)
        #expect(output.starts == 2)
        #expect(output.isRunning)
        #expect(session.categories.count == categoriesBefore + 1)
        #expect(session.activations.map(\.active) == [true, true])
        #expect(audio.state == .playing)
        #expect(audio.route == .earpiece)
    }

    @Test("a media services reset while stopped rebuilds the output and stays stopped")
    func mediaServicesResetWhileStopped() async {
        let session = FakeAudioSession()
        let output = FakePlaybackOutput()
        let audio = make(session, output)
        postMediaServicesReset()
        await audio.settle()
        #expect(output.resets == 1)
        #expect(output.starts == 0)
        #expect(session.activations.isEmpty)
        #expect(audio.state == .stopped)
    }

    // MARK: Routes

    @Test("an iPhone offers the speaker and the earpiece, and plays through the speaker")
    func phoneRoutes() async {
        let audio = await playing()
        #expect(audio.routes == [.speaker, .earpiece])
        #expect(audio.route == .speaker)
    }

    @Test("an iPad has no earpiece")
    func padRoutes() async {
        let audio = await playing(FakeAudioSession(isPhone: false))
        #expect(audio.routes == [.speaker])
        #expect(!audio.isPhone)
    }

    @Test("connected AirPods are offered and take the sound")
    func airPodsTakeTheSound() async {
        let audio = await playing(FakeAudioSession(device: .bluetooth("AirPods Pro")))
        #expect(audio.routes == [.speaker, .earpiece, .external(name: "AirPods Pro")])
        #expect(audio.route == .external(name: "AirPods Pro"))
    }

    @Test("the earpiece plays even with AirPods connected")
    func earpieceOverAirPods() async {
        let session = FakeAudioSession(device: .bluetooth("AirPods Pro"))
        let audio = await playing(session)
        audio.select(.earpiece)
        await audio.settle()
        #expect(audio.route == .earpiece)
        #expect(session.categories.last?.options == [])
        #expect(session.overrides.last == AVAudioSession.PortOverride.none)
        // AirPods stay on offer to move back to.
        #expect(audio.routes.contains(.external(name: "AirPods Pro")))
    }

    @Test("the speaker plays even with AirPods connected, and choosing AirPods moves back")
    func speakerOverAirPods() async {
        let session = FakeAudioSession(device: .bluetooth("AirPods Pro"))
        let audio = await playing(session)
        audio.select(.speaker)
        await audio.settle()
        #expect(audio.route == .speaker)
        #expect(session.overrides.last == .speaker)
        audio.select(.external(name: "AirPods Pro"))
        await audio.settle()
        #expect(audio.route == .external(name: "AirPods Pro"))
        #expect(session.overrides.last == AVAudioSession.PortOverride.none)
    }

    @Test("a choice made while stopped applies when playback starts")
    func choiceWhileStopped() async {
        let session = FakeAudioSession()
        let audio = make(session)
        audio.select(.earpiece)
        await audio.settle()
        #expect(session.categories.isEmpty)
        audio.start()
        await audio.settle()
        #expect(audio.route == .earpiece)
    }

    @Test("AirPods connecting take the sound from a chosen speaker")
    func airPodsConnect() async {
        let session = FakeAudioSession()
        let audio = await playing(session)
        audio.select(.speaker)
        await audio.settle()
        session.device = .bluetooth("AirPods Pro")
        postRouteChange(.newDeviceAvailable)
        await audio.settle()
        #expect(audio.route == .external(name: "AirPods Pro"))
        #expect(audio.routes == [.speaker, .earpiece, .external(name: "AirPods Pro")])
    }

    @Test("wired headphones take the sound")
    func wiredHeadphones() async {
        let session = FakeAudioSession()
        let audio = await playing(session)
        session.device = .wired("Headphones")
        postRouteChange(.newDeviceAvailable)
        await audio.settle()
        #expect(audio.route == .external(name: "Headphones"))
    }

    // MARK: Headphones going away

    @Test("headphones going away pause the band and raise the notice")
    func headphonesGoAwayPause() async {
        let session = FakeAudioSession(device: .wired("Headphones"))
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        #expect(audio.route == .external(name: "Headphones"))

        session.device = nil
        postRouteChange(.oldDeviceUnavailable)
        await audio.settle()
        #expect(output.pauses == 1)
        #expect(!output.isRunning)
        #expect(audio.state == .paused)
        #expect(audio.notice == .headphonesDisconnected)
        #expect(audio.notice?.text == "Sound paused: your headphones disconnected. Tap to play on the speaker.")
        #expect(audio.routes == [.speaker, .earpiece])
    }

    @Test("AirPods going away pause the band too")
    func airPodsGoAwayPause() async {
        let session = FakeAudioSession(device: .bluetooth("AirPods Pro"))
        let audio = await playing(session)
        session.device = nil
        postRouteChange(.oldDeviceUnavailable)
        await audio.settle()
        #expect(audio.state == .paused)
        #expect(audio.notice == .headphonesDisconnected)
    }

    @Test("tapping the notice plays the band on the speaker")
    func resumeOnSpeaker() async {
        let session = FakeAudioSession(device: .wired("Headphones"))
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        session.device = nil
        postRouteChange(.oldDeviceUnavailable)
        await audio.settle()

        audio.resumeFromNotice()
        await audio.settle()
        #expect(audio.notice == nil)
        #expect(audio.state == .playing)
        #expect(output.isRunning)
        #expect(audio.route == .speaker)
        #expect(session.overrides.last == .speaker)
    }

    @Test("the paused band stays paused until the notice is tapped")
    func pausedStaysPaused() async {
        let session = FakeAudioSession(device: .wired("Headphones"))
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        session.device = nil
        postRouteChange(.oldDeviceUnavailable)
        await audio.settle()
        // An interruption coming and going does not play it.
        postInterruption(.began)
        postInterruption(.ended)
        audio.start()
        await audio.settle()
        #expect(audio.state == .paused)
        #expect(!output.isRunning)
        #expect(output.starts == 1)
    }

    @Test("a route change that is not a removal changes nothing")
    func otherRouteChanges() async {
        let session = FakeAudioSession(device: .wired("Headphones"))
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        for reason in [AVAudioSession.RouteChangeReason.categoryChange, .override, .routeConfigurationChange,
                       .wakeFromSleep, .noSuitableRouteForCategory, .unknown] {
            postRouteChange(reason)
        }
        await audio.settle()
        #expect(output.pauses == 0)
        #expect(output.isRunning)
        #expect(audio.state == .playing)
        #expect(audio.notice == nil)
        #expect(audio.route == .external(name: "Headphones"))
    }

    @Test("a removal while playing on the speaker does not pause")
    func removalFromTheSpeaker() async {
        let session = FakeAudioSession()
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        postRouteChange(.oldDeviceUnavailable)
        await audio.settle()
        #expect(output.pauses == 0)
        #expect(audio.state == .playing)
        #expect(audio.notice == nil)
    }

    @Test("headphones going away while stopped raise no notice")
    func removalWhileStopped() async {
        let session = FakeAudioSession(device: .wired("Headphones"))
        let audio = make(session)
        await audio.settle()
        session.device = nil
        postRouteChange(.oldDeviceUnavailable)
        await audio.settle()
        #expect(audio.state == .stopped)
        #expect(audio.notice == nil)
    }

    @Test("stopping clears the notice")
    func stopClearsTheNotice() async {
        let session = FakeAudioSession(device: .wired("Headphones"))
        let audio = await playing(session)
        session.device = nil
        postRouteChange(.oldDeviceUnavailable)
        await audio.settle()
        audio.stop()
        #expect(audio.notice == nil)
        #expect(audio.state == .stopped)
    }

    @Test("the policy for each choice")
    func policy() {
        #expect(AudioSessionController.configuration(for: nil)
            == AudioSessionConfiguration(options: [.defaultToSpeaker, .allowBluetoothA2DP], outputOverride: .none))
        #expect(AudioSessionController.configuration(for: .speaker)
            == AudioSessionConfiguration(options: [.defaultToSpeaker, .allowBluetoothA2DP], outputOverride: .speaker))
        #expect(AudioSessionController.configuration(for: .earpiece)
            == AudioSessionConfiguration(options: [], outputOverride: .none))
        #expect(AudioSessionController.configuration(for: .external(name: "AirPods Pro"))
            == AudioSessionConfiguration(options: [.defaultToSpeaker, .allowBluetoothA2DP], outputOverride: .none))
    }

    // MARK: Transmit (Task 55)

    @Test("an interruption beginning runs the transmit hook at once, and its end resumes playback, never transmit")
    func interruptionEndsTransmit() async {
        let output = FakePlaybackOutput()
        let audio = await playing(FakeAudioSession(), output)
        var began = 0
        audio.onInterruptionBegan = { began += 1 }
        postInterruption(.began)
        #expect(began == 1)
        #expect(audio.state == .interrupted)
        postInterruption(.ended)
        await audio.settle()
        #expect(began == 1, "the end runs nothing of transmit")
        #expect(audio.state == .playing)
        #expect(output.isRunning)
    }

    @Test("an interruption ends transmit even when the band is not playing")
    func interruptionEndsTransmitWhileStopped() async {
        let audio = make()
        var began = 0
        audio.onInterruptionBegan = { began += 1 }
        postInterruption(.began)
        #expect(began == 1)
        #expect(audio.state == .stopped)
    }

    @Test("keyed on the speaker the band is silent and MON plays nowhere; unkeyed it plays again")
    func keyedOnTheSpeakerIsSilent() async {
        let output = FakePlaybackOutput()
        let audio = await playing(FakeAudioSession(), output)
        #expect(!output.isMuted)
        audio.setTransmitting(true, monitorOn: true)
        #expect(output.isMuted)
        #expect(audio.bandMuted)
        audio.setTransmitting(false, monitorOn: true)
        #expect(!output.isMuted)
    }

    @Test("keyed on AirPods MON plays")
    func keyedOnAirPodsPlays() async {
        let output = FakePlaybackOutput()
        let audio = await playing(FakeAudioSession(device: .bluetooth("AirPods Pro")), output)
        #expect(audio.route == .external(name: "AirPods Pro"))
        audio.setTransmitting(true, monitorOn: true)
        #expect(!output.isMuted)
    }

    @Test("keyed with AirPods, moving the sound to the speaker silences it")
    func routeChangeWhileKeyed() async {
        let session = FakeAudioSession(device: .bluetooth("AirPods Pro"))
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        audio.setTransmitting(true, monitorOn: false)
        #expect(!output.isMuted)
        audio.select(.speaker)
        await audio.settle()
        #expect(audio.route == .speaker)
        #expect(output.isMuted)
    }

    @Test("the band while keyed, for each setting and output")
    func bandAudiblePolicy() {
        // Unkeyed, it always plays.
        #expect(AudioSessionController.bandAudible(transmitting: false, onHeadphones: false, muteBand: true,
                                                   monInHeadphonesOnly: true, monitorOn: true))
        // Keyed on headphones it plays, MON and all.
        #expect(AudioSessionController.bandAudible(transmitting: true, onHeadphones: true, muteBand: true,
                                                   monInHeadphonesOnly: true, monitorOn: true))
        // Keyed on the speaker with the defaults: silent.
        #expect(!AudioSessionController.bandAudible(transmitting: true, onHeadphones: false, muteBand: true,
                                                    monInHeadphonesOnly: true, monitorOn: false))
        // The band not muted while talking: it plays, unless MON is on and kept to headphones.
        #expect(AudioSessionController.bandAudible(transmitting: true, onHeadphones: false, muteBand: false,
                                                   monInHeadphonesOnly: true, monitorOn: false))
        #expect(!AudioSessionController.bandAudible(transmitting: true, onHeadphones: false, muteBand: false,
                                                    monInHeadphonesOnly: true, monitorOn: true))
        #expect(AudioSessionController.bandAudible(transmitting: true, onHeadphones: false, muteBand: false,
                                                   monInHeadphonesOnly: false, monitorOn: true))
    }

    @Test("the While you transmit settings apply at once")
    func whileTransmittingSettings() async {
        let output = FakePlaybackOutput()
        let audio = await playing(FakeAudioSession(), output)
        audio.setTransmitting(true, monitorOn: false)
        #expect(output.isMuted)
        audio.setWhileTransmitting(muteBand: false, monInHeadphonesOnly: true)
        #expect(!output.isMuted)
    }

    @Test("the AirPods' microphone allows the hands-free profile; the iPhone's, the default, never does")
    func microphoneChoice() async throws {
        #expect(AudioSessionController.configuration(for: nil, microphone: .airPods)
            == AudioSessionConfiguration(options: [.defaultToSpeaker, .allowBluetoothA2DP, .allowBluetoothHFP],
                                         outputOverride: .none))
        #expect(AudioSessionController.configuration(for: .earpiece, microphone: .airPods)
            == AudioSessionConfiguration(options: [], outputOverride: .none))
        let session = FakeAudioSession(device: .bluetooth("AirPods Pro"))
        let audio = await playing(session)
        #expect(audio.microphone == .iPhone)
        audio.setMicrophone(.airPods)
        await audio.settle()
        let category = try #require(session.categories.last)
        #expect(category.options.contains(.allowBluetoothHFP))
        #expect(category.category == .playAndRecord && category.mode == .default)
        audio.setMicrophone(.iPhone)
        await audio.settle()
        #expect(session.categories.last?.options.contains(.allowBluetoothHFP) == false)
    }

    // MARK: Keeping the route (JJ's device, 2026-09-26)

    private static let speakerDefault = AVAudioSession.CategoryOptions([.defaultToSpeaker, .allowBluetoothA2DP]).rawValue

    @Test("the session is set up in order: the category, haptics while recording, activation, then the override")
    func setUpOrder() async {
        let session = FakeAudioSession()
        _ = await playing(session)
        #expect(session.calls == ["category \(Self.speakerDefault)", "haptics allowed", "active true",
                                  "override none"])
    }

    /// TestFlight build 5 (2026-09-27): the dial's haptic ticks stopped once
    /// the mic level meter had opened the microphone, and came back only
    /// when the app was closed. iOS silences haptics while an app records
    /// unless the session allows them, so every setting of the category
    /// allows them, the start's, a route change's and a reset's.
    @Test("every setting of the category allows the dial's haptics while the microphone is open")
    func hapticsPlayWhileRecording() async {
        let session = FakeAudioSession(device: .bluetooth("AirPods Pro"))
        let output = FakePlaybackOutput()
        let audio = await playing(session, output)
        #expect(session.hapticsAllowed == 1)
        for route in [AudioRoute.speaker, .earpiece, .external(name: "AirPods Pro")] {
            audio.select(route)
        }
        audio.setMicrophone(.airPods)
        await audio.settle()
        postMediaServicesReset()
        await audio.settle()
        await audio.settle()
        #expect(session.categories.count >= 6)
        #expect(session.hapticsAllowed == session.categories.count)
        // Each right after its category, before anything else touches the session.
        for (index, call) in session.calls.enumerated() where call.hasPrefix("category ") {
            #expect(session.calls.indices.contains(index + 1) && session.calls[index + 1] == "haptics allowed")
        }
    }

    @Test("where the phone ignores the speaker default, no choice still plays on the speaker")
    func theDefaultStaysOnTheSpeaker() async {
        let session = FakeAudioSession()
        session.ignoresDefaultToSpeaker = true
        let audio = await playing(session)
        #expect(audio.route == .speaker)
        #expect(session.calls == ["category \(Self.speakerDefault)", "haptics allowed", "active true",
                                  "override none", "override speaker"])
    }

    @Test("Speaker plays on the speaker and the earpiece on the receiver, speaker default or not")
    func speakerAndEarpiece() async {
        let session = FakeAudioSession()
        session.ignoresDefaultToSpeaker = true
        let audio = await playing(session)
        audio.select(.earpiece)
        await audio.settle()
        #expect(audio.route == .earpiece)
        #expect(session.calls.last == "override none", "the earpiece is never corrected")
        audio.select(.speaker)
        await audio.settle()
        #expect(audio.route == .speaker)
    }

    @Test("an override iOS dropped is put back at the next route change")
    func aDroppedOverrideComesBack() async {
        let session = FakeAudioSession()
        session.ignoresDefaultToSpeaker = true
        let audio = await playing(session)
        audio.select(.speaker)
        await audio.settle()
        session.dropOverride()
        postRouteChange(.routeConfigurationChange)
        await audio.settle()
        await audio.settle()
        #expect(audio.route == .speaker)
        #expect(session.calls.last == "override speaker")
    }

    @Test("the speaker stays across an interruption and a media services reset")
    func theSpeakerStaysAcrossRestarts() async {
        let session = FakeAudioSession()
        session.ignoresDefaultToSpeaker = true
        let audio = await playing(session)
        postInterruption(.began)
        postInterruption(.ended)
        await audio.settle()
        await audio.settle()
        #expect(audio.route == .speaker)
        postMediaServicesReset()
        await audio.settle()
        await audio.settle()
        #expect(audio.route == .speaker)
        #expect(audio.state == .playing)
    }

    @Test("headphones connecting take the sound from a kept speaker override")
    func headphonesTakeTheSound() async {
        let session = FakeAudioSession()
        session.ignoresDefaultToSpeaker = true
        let audio = await playing(session)
        #expect(audio.route == .speaker)
        session.device = .wired("Headphones")
        postRouteChange(.newDeviceAvailable)
        await audio.settle()
        await audio.settle()
        #expect(audio.route == .external(name: "Headphones"))
    }

    @Test("the corrective override for each choice and output")
    func correctiveOverridePolicy() {
        let speaker = [AudioOutputPort(kind: .speaker, name: "Speaker")]
        let receiver = [AudioOutputPort(kind: .earpiece, name: "Receiver")]
        let airPods = [AudioOutputPort(kind: .external, name: "AirPods", pausesOnRemoval: true, isHeadphones: true)]
        #expect(AudioSessionController.correctiveOverride(for: nil, outputs: receiver) == .speaker)
        #expect(AudioSessionController.correctiveOverride(for: nil, outputs: speaker) == nil)
        #expect(AudioSessionController.correctiveOverride(for: nil, outputs: airPods) == nil)
        #expect(AudioSessionController.correctiveOverride(for: .speaker, outputs: receiver) == .speaker)
        #expect(AudioSessionController.correctiveOverride(for: .speaker, outputs: airPods) == .speaker)
        #expect(AudioSessionController.correctiveOverride(for: .earpiece, outputs: receiver) == nil)
        #expect(AudioSessionController.correctiveOverride(for: .earpiece, outputs: speaker) == nil)
        #expect(AudioSessionController.correctiveOverride(for: .speaker, outputs: []) == nil)
    }
}
