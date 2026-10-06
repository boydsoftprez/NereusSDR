// NereusSDR for iOS: the phone's audio session: the band's playback, where it plays, and interruptions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import os

/// Sound on the phone (R-IOS-20, spec sections 4.7 and 5.4 items 6 and 10).
///
/// The session's category is `.playAndRecord` in mode `.default` from the
/// start: the category the microphone will need when the phone transmits,
/// so the route does not move when it does, and the default mode, so iOS's
/// voice processing stays off. Bluetooth is allowed only as A2DP, so AirPods
/// play at full quality and the microphone stays on the iPhone.
///
/// The band plays through the speaker by default, or through whatever is
/// connected (AirPods, headphones), as iOS does. ``select(_:)`` moves it
/// (the Sound panel and Setup): the speaker is forced with an output override, the
/// earpiece by leaving both the speaker default and Bluetooth out of the
/// options, and a connected device by leaving the choice to iOS. A device
/// that connects takes the sound. When headphones or AirPods go away the
/// sound pauses with a ``notice``, and plays on the speaker only when the
/// operator taps it (``resumeOnSpeaker()``).
///
/// A call, Siri or another app's audio interrupts: playback pauses when the
/// interruption begins and resumes when it ends with iOS's `.shouldResume`;
/// an end without it leaves the band paused with a ``notice`` until the
/// operator taps it (``resumeFromNotice()``). When the phone's media
/// services reset, the session and the output are built again and playback
/// comes back if it was playing. Activating the session never asks for the
/// microphone; nothing here records (``MicCapture`` does, on this session).
/// Each time the category is set, haptics and system sounds are allowed
/// while recording, so the dial's ticks play while the microphone is open
/// and after it closes (TestFlight build 5, 2026-09-27: they stopped until
/// the app was closed once the mic level meter had opened the microphone).
///
/// Transmit (Task 55; spec section 5.4 items 6 and 7): an interruption's
/// beginning also runs ``onInterruptionBegan``, which unkeys and stops the
/// microphone; its end resumes the band, never transmit. While this phone
/// transmits the band is silenced on the speaker and the earpiece, so they
/// cannot feed the microphone, and MON (in the Core's sound while keyed)
/// plays only on headphones: both settings on by default
/// (``bandAudible(transmitting:onHeadphones:muteBand:monInHeadphonesOnly:monitorOn:)``).
/// The operator's microphone is the iPhone's by default; choosing the
/// AirPods' allows Bluetooth's hands-free profile, which iOS then uses both ways.
///
/// Every call into the session (category, activation, override, reading the
/// route) runs in order on the controller's own serial queue, because
/// activation can block the main thread. The state here, the output and
/// the notifications' handlers stay on the main actor; each piece of
/// session work reports back to the main queue in the order it was queued.
/// A result that arrives after the state has moved on (a stop while a start
/// was activating) is dropped.
///
/// Keeping the route (JJ's device, 2026-09-26: Speaker and the default
/// both played on the receiver): iOS drops an output override on a
/// category change, a route change or when audio units start, and does
/// not always honour `.defaultToSpeaker` then. So after the output starts,
/// and after every route change while playing, the route the session
/// reports is checked against the choice: the speaker chosen, or no choice
/// with the sound on the receiver, gets the speaker override again
/// (``correctiveOverride(for:outputs:)``); the earpiece is left alone. A
/// device that connects clears that override by setting the choice again.
/// Every configuration and route change logs the session's category,
/// mode, options, override and output port types (never names or
/// addresses) to the device console.
@MainActor
final class AudioSessionController: ObservableObject {
    enum PlaybackState: Equatable, Sendable {
        case stopped
        case playing
        /// Paused by an interruption, to resume when it ends.
        case interrupted
        /// Paused because headphones or AirPods went away, to resume on the
        /// speaker when the operator taps the notice.
        case paused
    }

    static let category: AVAudioSession.Category = .playAndRecord
    static let mode: AVAudioSession.Mode = .default

    private static let logger = Logger(subsystem: "NereusSDR", category: "audio.session")

    /// Where the band plays now, as the session reports it.
    @Published private(set) var route: AudioRoute?
    /// The sound goes to wired or Bluetooth headphones now: MON may play
    /// (D80), where the loudspeaker would feed back into the microphone.
    @Published private(set) var onHeadphones = false
    /// The places the band can play now, in the Sound panel's order: the
    /// speaker, the earpiece on an iPhone, and the connected device if any.
    @Published private(set) var routes: [AudioRoute] = []
    @Published private(set) var state: PlaybackState = .stopped
    /// What the band screen should show about the sound, if anything.
    @Published private(set) var notice: AudioNotice?
    /// The band is silenced because this phone transmits.
    @Published private(set) var bandMuted = false
    /// The microphone the operator talks into.
    @Published private(set) var microphone: MicrophoneChoice = .standard
    /// Silence the band while this phone transmits (on by default).
    @Published private(set) var muteBandWhileTalking = true
    /// MON plays in headphones only (on by default).
    @Published private(set) var monInHeadphonesOnly = true

    /// Runs when an interruption (a call, Siri) begins: the app unkeys and
    /// stops the microphone there.
    var onInterruptionBegan: (@MainActor () -> Void)?
    /// Runs first when the phone's media services reset: the app unkeys and
    /// builds the microphone again there.
    var onMediaServicesReset: (@MainActor () -> Void)?

    /// True on an iPhone, false on an iPad.
    let isPhone: Bool

    private let session: any AudioSessionPort
    private let output: any PlaybackOutput

    /// A MainActor reading of the actual output timeline. The output may
    /// retire an epoch while diagnosing a discontinuity, so callers recheck
    /// its identity after this call before publishing joined observations.
    func diagnosticsOutputTiming() -> PlaybackOutputTimingObservation {
        output.timingObservation()
    }
    private let center: NotificationCenter
    private let queue = DispatchQueue(label: "NereusSDR.audio.session")
    /// The operator's choice, or nil to leave it to iOS. Setup's Play the
    /// band through and the band's Sound panel both set it with
    /// ``select(_:)`` and both tick ``markedRoute``, so the two agree.
    @Published private(set) var chosen: AudioRoute?
    /// The connected device's name, once the session has played through it.
    private var connectedDevice: String?
    /// The outputs the session last reported.
    private var lastOutputs: [AudioOutputPort] = []
    /// Moves on with every change of ``state``, so session work queued
    /// before the change does not act after it.
    private var generation = 0
    /// This phone transmits now, and the Core's MON is on.
    private var transmitting = false
    private var monitorOn = false
    nonisolated(unsafe) private var observers: [any NSObjectProtocol] = []

    init(session: any AudioSessionPort, output: any PlaybackOutput, notificationCenter: NotificationCenter = .default) {
        self.session = session
        self.output = output
        isPhone = session.isPhone
        center = notificationCenter
        observeSession()
        updateRoutes()
        refreshRoute()
    }

    deinit {
        for observer in observers {
            center.removeObserver(observer)
        }
    }

    /// The session options and output override for a choice; nil leaves it
    /// to iOS. The AirPods' microphone adds Bluetooth's hands-free profile,
    /// except on the earpiece, which takes no Bluetooth at all.
    static func configuration(for choice: AudioRoute?,
                              microphone: MicrophoneChoice = .standard) -> AudioSessionConfiguration {
        var options: AVAudioSession.CategoryOptions = [.defaultToSpeaker, .allowBluetoothA2DP]
        if microphone == .airPods {
            options.insert(.allowBluetoothHFP)
        }
        switch choice {
        case .speaker:
            return AudioSessionConfiguration(options: options, outputOverride: .speaker)
        case .earpiece:
            // Without the speaker default the category plays through the
            // receiver, and without A2DP connected AirPods do not take it.
            return AudioSessionConfiguration(options: [], outputOverride: .none)
        case .external, nil:
            return AudioSessionConfiguration(options: options, outputOverride: .none)
        }
    }

    /// Whether the band is heard while this phone transmits or not. Keyed,
    /// headphones play it (with MON in it when MON is on); the speaker and
    /// the earpiece play it only when the band is not muted while talking,
    /// and, with MON on, only when MON is not kept to headphones.
    static func bandAudible(transmitting: Bool, onHeadphones: Bool, muteBand: Bool, monInHeadphonesOnly: Bool,
                            monitorOn: Bool) -> Bool {
        guard transmitting, !onHeadphones else {
            return true
        }
        if muteBand {
            return false
        }
        return !(monInHeadphonesOnly && monitorOn)
    }

    // MARK: Transmit

    /// This phone transmits (its key, or a VOX key of its own), and whether
    /// the Core's MON is on: the band follows the policy at once.
    func setTransmitting(_ transmitting: Bool, monitorOn: Bool) {
        self.transmitting = transmitting
        self.monitorOn = monitorOn
        applyMute()
    }

    /// The two While you transmit settings.
    func setWhileTransmitting(muteBand: Bool, monInHeadphonesOnly: Bool) {
        muteBandWhileTalking = muteBand
        self.monInHeadphonesOnly = monInHeadphonesOnly
        applyMute()
    }

    /// The microphone the operator talks into; takes effect at once while
    /// playing, and when playback starts or resumes otherwise.
    func setMicrophone(_ choice: MicrophoneChoice) {
        guard choice != microphone else {
            return
        }
        microphone = choice
        guard state == .playing else {
            return
        }
        apply()
    }

    private func applyMute() {
        let onHeadphones = lastOutputs.first?.isHeadphones ?? false
        let muted = !Self.bandAudible(transmitting: transmitting, onHeadphones: onHeadphones,
                                      muteBand: muteBandWhileTalking, monInHeadphonesOnly: monInHeadphonesOnly,
                                      monitorOn: monitorOn)
        guard muted != bandMuted else {
            return
        }
        bandMuted = muted
        output.setMuted(muted)
    }

    // MARK: Playback

    /// Sets the session up, activates it and starts playing. Does nothing
    /// unless stopped.
    func start() {
        guard state == .stopped else {
            return
        }
        activateAndPlay()
    }

    /// Stops playing and lets other apps' audio resume.
    func stop() {
        guard state != .stopped else {
            return
        }
        move(to: .stopped)
        notice = nil
        output.stop()
        perform("stop", "the audio session did not deactivate") { session in
            try session.setActive(false, options: .notifyOthersOnDeactivation)
        }
    }

    /// The notice's tap: after headphones went away the band plays on the
    /// speaker; after an interruption that did not ask to resume it plays
    /// again where it played.
    func resumeFromNotice() {
        guard state == .paused, let notice else {
            return
        }
        switch notice {
        case .headphonesDisconnected:
            resumeOnSpeaker()
        case .interruptionEnded:
            self.notice = nil
            activateAndPlay()
        }
    }

    /// Plays the band on the speaker after headphones went away (the
    /// notice's tap).
    func resumeOnSpeaker() {
        guard state == .paused else {
            return
        }
        notice = nil
        chosen = .speaker
        activateAndPlay()
    }

    /// Sets the session up for the current choice, activates it, then
    /// starts the output, unless the state has moved on by then.
    private func activateAndPlay() {
        let expected = move(to: .playing)
        let configuration = Self.configuration(for: chosen, microphone: microphone)
        let category = Self.category
        let mode = Self.mode
        perform("start", "the band's audio did not start", { session in
            try session.setCategory(category, mode: mode, options: configuration.options)
            session.allowHapticsWhileRecording()
            try session.setActive(true, options: [])
            try session.overrideOutputAudioPort(configuration.outputOverride)
        }, then: { [weak self] succeeded in
            guard let self, self.generation == expected else {
                return
            }
            guard succeeded else {
                self.move(to: .stopped)
                return
            }
            do {
                try self.output.start()
                // Starting the engine can move the route: check it again.
                self.keepRoute()
            } catch {
                Self.logger.warning("the band's audio did not start: \(error.localizedDescription)")
                self.move(to: .stopped)
            }
        })
    }

    @discardableResult
    private func move(to next: PlaybackState) -> Int {
        state = next
        generation += 1
        return generation
    }

    // MARK: Routes

    /// The route Setup and the Sound panel tick: where the band plays while
    /// it plays; otherwise where it will play, the operator's choice when
    /// there is one, else where the session would play it now.
    var markedRoute: AudioRoute? {
        if state == .playing {
            return route
        }
        return chosen ?? route
    }

    /// Moves the band to `route`. Takes effect at once while playing, and
    /// when playback starts or resumes otherwise.
    func select(_ route: AudioRoute) {
        chosen = route
        guard state == .playing else {
            return
        }
        apply()
    }

    private func apply() {
        let configuration = Self.configuration(for: chosen, microphone: microphone)
        let category = Self.category
        let mode = Self.mode
        perform("route", "the audio route did not change") { session in
            try session.setCategory(category, mode: mode, options: configuration.options)
            session.allowHapticsWhileRecording()
            try session.overrideOutputAudioPort(configuration.outputOverride)
        }
    }

    private func refreshRoute() {
        perform("route change", "the audio route could not be read") { _ in }
    }

    /// The override that puts the sound back where the choice wants it, or
    /// nil when it is there: the speaker, chosen, off the speaker; no
    /// choice, or a connected device chosen, on the receiver (so nothing is
    /// connected and iOS left the speaker default behind). The earpiece is
    /// never corrected.
    nonisolated static func correctiveOverride(for choice: AudioRoute?,
                                   outputs: [AudioOutputPort]) -> AVAudioSession.PortOverride? {
        guard let first = outputs.first else {
            return nil
        }
        switch choice {
        case .earpiece:
            return nil
        case .speaker:
            return first.kind == .speaker ? nil : .speaker
        case .external, nil:
            return first.kind == .earpiece ? .speaker : nil
        }
    }

    /// Reads the route and, while playing, puts the sound back on the
    /// speaker when the choice wants it there and iOS moved it.
    private func keepRoute() {
        guard state == .playing else {
            return
        }
        let choice = chosen
        perform("keep route", "the audio route could not be kept", keeping: false) { session in
            if let override = Self.correctiveOverride(for: choice, outputs: session.currentOutputs) {
                try session.overrideOutputAudioPort(override)
            }
        }
    }

    private func update(outputs: [AudioOutputPort], keeping: Bool) {
        lastOutputs = outputs
        let current = outputs.first
        if let current, current.kind == .external {
            connectedDevice = current.name
        }
        route = current?.route
        let headphones = current?.isHeadphones ?? false
        if headphones != onHeadphones {
            onHeadphones = headphones
        }
        updateRoutes()
        applyMute()
        if keeping, state == .playing, Self.correctiveOverride(for: chosen, outputs: outputs) != nil {
            Self.logger.notice("the sound left the chosen route; putting it back")
            keepRoute()
        }
    }

    private func updateRoutes() {
        var available: [AudioRoute] = [.speaker]
        if isPhone {
            available.append(.earpiece)
        }
        if let connectedDevice {
            available.append(.external(name: connectedDevice))
        }
        routes = available
    }

    // MARK: The session queue

    /// Runs `work` on the session queue, logs the session as it then stands
    /// under `what`, then, on the main queue, takes the route the session
    /// reports (checking it against the choice unless `keeping` is false)
    /// and runs `then` with whether `work` succeeded.
    private func perform(_ what: String, _ failure: String, keeping: Bool = true,
                         _ work: @escaping @Sendable (any AudioSessionPort) throws -> Void,
                         then: @escaping @MainActor @Sendable (Bool) -> Void = { _ in }) {
        let session = self.session
        let logger = Self.logger
        queue.async { [weak self] in
            var succeeded = true
            do {
                try work(session)
            } catch {
                succeeded = false
                logger.warning("\(failure): \(error.localizedDescription)")
            }
            logger.notice("audio session, \(what, privacy: .public): \(session.diagnostics, privacy: .public)")
            let outputs = session.currentOutputs
            DispatchQueue.main.async {
                MainActor.assumeIsolated {
                    self?.update(outputs: outputs, keeping: keeping)
                    then(succeeded)
                }
            }
        }
    }

    /// Waits until the session work queued so far has reported back (tests).
    func settle() async {
        await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
            queue.async {
                DispatchQueue.main.async {
                    continuation.resume()
                }
            }
        }
    }

    // MARK: The session's notifications

    /// `shouldResume` is iOS's `.shouldResume` on the end: without it the
    /// band stays paused with a notice, as Apple asks of an app whose sound
    /// another app's ended interruption may not want back.
    func handleInterruption(_ type: AVAudioSession.InterruptionType, shouldResume: Bool) {
        switch type {
        case .began:
            // Transmit ends first, whatever the band is doing.
            onInterruptionBegan?()
            guard state == .playing else {
                return
            }
            output.pause()
            move(to: .interrupted)
        case .ended:
            guard state == .interrupted else {
                return
            }
            guard shouldResume else {
                move(to: .paused)
                notice = .interruptionEnded
                return
            }
            activateAndPlay()
        @unknown default:
            break
        }
    }

    /// `previousOutputs` is the route before the change, when the
    /// notification carries it; otherwise the route last read is used.
    func handleRouteChange(_ reason: AVAudioSession.RouteChangeReason, previousOutputs: [AudioOutputPort]? = nil) {
        switch reason {
        case .newDeviceAvailable:
            // Something connected takes the sound, as it does in every app;
            // setting the choice again also clears a kept speaker override.
            chosen = nil
            if state == .playing {
                apply()
            }
        case .oldDeviceUnavailable:
            connectedDevice = nil
            if case .external = chosen {
                chosen = nil
            }
            let previous = previousOutputs ?? lastOutputs
            // Headphones or AirPods went away: pause rather than play the
            // band out loud, until the operator taps the notice.
            if previous.contains(where: \.pausesOnRemoval), state == .playing || state == .interrupted {
                output.pause()
                move(to: .paused)
                notice = .headphonesDisconnected
            }
            updateRoutes()
        default:
            break
        }
        refreshRoute()
    }

    /// The phone's media services restarted: every audio object is gone.
    /// Runs ``onMediaServicesReset`` first, then builds the output again
    /// and, if it was playing, the session with it.
    /// An interrupted or paused band sets the session up when it resumes.
    func handleMediaServicesReset() {
        // Transmit ends first: the microphone's input is gone with the rest.
        onMediaServicesReset?()
        output.reset()
        if state == .playing {
            activateAndPlay()
        }
    }

    private func observeSession() {
        observers.append(center.addObserver(forName: AVAudioSession.interruptionNotification, object: nil,
                                            queue: nil) { [weak self] note in
            guard let raw = note.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
                  let type = AVAudioSession.InterruptionType(rawValue: raw) else {
                return
            }
            let options = AVAudioSession.InterruptionOptions(
                rawValue: note.userInfo?[AVAudioSessionInterruptionOptionKey] as? UInt ?? 0)
            Self.onMain { self?.handleInterruption(type, shouldResume: options.contains(.shouldResume)) }
        })
        observers.append(center.addObserver(forName: AVAudioSession.routeChangeNotification, object: nil,
                                            queue: nil) { [weak self] note in
            guard let raw = note.userInfo?[AVAudioSessionRouteChangeReasonKey] as? UInt,
                  let reason = AVAudioSession.RouteChangeReason(rawValue: raw) else {
                return
            }
            let previous = (note.userInfo?[AVAudioSessionRouteChangePreviousRouteKey] as? AVAudioSessionRouteDescription)?
                .outputs.map(SystemAudioSession.port(for:))
            Self.onMain { self?.handleRouteChange(reason, previousOutputs: previous) }
        })
        observers.append(center.addObserver(forName: AVAudioSession.mediaServicesWereResetNotification, object: nil,
                                            queue: nil) { [weak self] _ in
            Self.onMain { self?.handleMediaServicesReset() }
        })
    }

    /// Runs `body` on the main actor: at once when posted there (the tests,
    /// and most of the session's notifications), otherwise queued to the
    /// main queue behind what is already there.
    private nonisolated static func onMain(_ body: @escaping @MainActor @Sendable () -> Void) {
        if Thread.isMainThread {
            MainActor.assumeIsolated(body)
        } else {
            DispatchQueue.main.async {
                MainActor.assumeIsolated(body)
            }
        }
    }
}
