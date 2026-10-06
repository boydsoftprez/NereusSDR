// NereusSDR for iOS: the PTT button test's coordinator, a Push to Talk channel that never keys the radio (debug copies only)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#if PTT_PROBE
import AVFoundation
import Combine
import Foundation
import MediaPlayer
import UIKit
#if canImport(PushToTalk)
import PushToTalk
#endif

/// The PTT button test (R-IOS-15, plan Task 65 step 1). It joins a Push to
/// Talk channel of its own, lets the hardware buttons begin and end the
/// channel's transmissions, and records every press, begin and end with
/// its time, its source and whether the phone was locked. A transmission
/// here is Push to Talk's alone: the probe never calls the app's PTT, the
/// radio or the link, and the microphone's samples go nowhere.
@MainActor
final class PttProbe: NSObject, ObservableObject {
    static let shared = PttProbe()

    /// The channel's name in the system's Push to Talk interface.
    nonisolated static let channelName = "NereusSDR button test"
    /// Fixed, so a channel the system restores is the probe's.
    nonisolated static let channelId = UUID(uuidString: "6E3C1F0A-9B7D-4C2E-8A51-3D0F2B7E9C41") ?? UUID()
    /// How often a line says the app is still running while it is not in front.
    static let heartbeatSeconds: TimeInterval = 15

    let log = PttProbeLog()
    let microphone = PttProbeMicrophone()
    let bluetooth = PttProbeBluetooth()

    /// Joined to the probe's channel.
    @Published private(set) var joined = false
    /// Push to Talk maps a headset's button to begin and end (the system's default).
    @Published private(set) var headsetEvents = true
    /// Media commands (play, pause and the rest) are logged too.
    @Published private(set) var mediaCommands = false
    /// The last thing that went wrong, for the page.
    @Published private(set) var problem: String?

    private var observers: [NSObjectProtocol] = []
    private var mediaTargets: [(MPRemoteCommand, Any)] = []
    private var heartbeat: Timer?
    #if canImport(PushToTalk)
    private var manager: PTChannelManager?
    private var creating: Task<PTChannelManager?, Never>?
    #endif

    override private init() {
        super.init()
        watchThePhone()
        bluetooth.onEvent = { [weak self] event in
            self?.bluetoothEvent(event)
        }
        heartbeat = Timer.scheduledTimer(withTimeInterval: Self.heartbeatSeconds, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated {
                self?.beat()
            }
        }
        log.record(.phone, .note, moment: moment(), detail: "button test started")
    }

    /// The phone as it is now: locked when protected data is unavailable
    /// (which needs a passcode, and follows the lock by a few seconds).
    func moment() -> PttProbeMoment {
        let app = UIApplication.shared
        let state: PttProbeMoment.AppState = switch app.applicationState {
        case .active: .active
        case .inactive: .inactive
        case .background: .background
        @unknown default: .background
        }
        return PttProbeMoment(locked: !app.isProtectedDataAvailable, appState: state)
    }

    func note(_ source: PttProbeSource, _ detail: String) {
        log.record(source, .note, moment: moment(), detail: detail)
    }

    // MARK: The channel

    /// Makes the channel manager, once. Push to Talk also asks for it to
    /// be made at launch; the probe makes it when its page opens or the
    /// Action button runs it.
    func prepare() async {
        #if canImport(PushToTalk)
        _ = await readyManager()
        #else
        note(.channel, "Push to Talk is not available on this device")
        #endif
    }

    /// Joins the probe's channel. Push to Talk allows this only with the app in front.
    func join() async {
        #if canImport(PushToTalk)
        guard let manager = await readyManager() else { return }
        let session = AVAudioSession.sharedInstance()
        if session.category != .playAndRecord {
            do {
                try session.setCategory(.playAndRecord, mode: .default, options: [.defaultToSpeaker, .allowBluetoothA2DP])
                note(.channel, "audio category set to play and record")
            } catch {
                note(.channel, "audio category not set: \(error.localizedDescription)")
            }
        } else {
            note(.channel, "audio category already play and record")
        }
        note(.channel, "asks to join")
        manager.requestJoinChannel(channelUUID: Self.channelId,
                                   descriptor: PTChannelDescriptor(name: Self.channelName, image: nil))
        #else
        note(.channel, "Push to Talk is not available on this device")
        #endif
    }

    func leave() {
        #if canImport(PushToTalk)
        guard let manager else { return }
        note(.channel, "asks to leave")
        manager.leaveChannel(channelUUID: Self.channelId)
        #endif
    }

    /// Turns Push to Talk's mapping of a headset's button on or off.
    func setHeadsetEvents(_ on: Bool) async {
        #if canImport(PushToTalk)
        guard let manager, joined else { return }
        do {
            try await manager.setAccessoryButtonEventsEnabled(on, channelUUID: Self.channelId)
            headsetEvents = on
            note(.channel, on ? "headset button events on" : "headset button events off")
        } catch {
            note(.channel, "headset button events not changed: \(error.localizedDescription)")
        }
        #endif
    }

    // MARK: Presses

    /// A toggling press: the first begins a test transmission, the next ends it.
    func toggle(by source: PttProbeSource, detail: String = "") {
        let next = log.toggle(by: source, joined: joined, moment: moment(), detail: detail)
        #if canImport(PushToTalk)
        guard let manager else { return }
        switch next {
        case .begin:
            manager.requestBeginTransmitting(channelUUID: Self.channelId)
        case .end:
            manager.stopTransmitting(channelUUID: Self.channelId)
        case .nothing:
            break
        }
        #endif
    }

    /// The Action button ran the probe's intent. When iOS started the app
    /// for it, the channel manager is made first and the line says so.
    func actionButtonPressed() async {
        #if canImport(PushToTalk)
        let fresh = manager == nil
        await prepare()
        toggle(by: .actionButton, detail: fresh ? "app started by this press" : "")
        #else
        toggle(by: .actionButton)
        #endif
    }

    func bluetoothEvent(_ event: PttProbeBluetoothEvent) {
        switch event {
        case .note(let detail):
            note(.bluetooth, detail)
        case .value(press: true, let detail):
            toggle(by: .bluetooth, detail: detail)
        case .value(press: false, let detail):
            note(.bluetooth, "\(detail), not a press")
        }
    }

    // MARK: Media commands

    /// Logs the media commands a headset or the lock screen sends. Off by
    /// default; each is answered as handled and does nothing else.
    func setMediaCommands(_ on: Bool) {
        let center = MPRemoteCommandCenter.shared()
        for (command, target) in mediaTargets {
            command.removeTarget(target)
        }
        mediaTargets = []
        mediaCommands = on
        guard on else {
            note(.mediaButton, "media commands no longer logged")
            return
        }
        let commands: [(MPRemoteCommand, String)] = [
            (center.togglePlayPauseCommand, "play or pause"),
            (center.playCommand, "play"),
            (center.pauseCommand, "pause"),
            (center.nextTrackCommand, "next"),
            (center.previousTrackCommand, "previous"),
        ]
        for (command, name) in commands {
            let target = command.addTarget { [weak self] _ in
                MainActor.assumeIsolated {
                    guard let self else { return }
                    self.log.record(.mediaButton, .press, moment: self.moment(), detail: name)
                }
                return .success
            }
            mediaTargets.append((command, target))
        }
        note(.mediaButton, "media commands logged")
    }

    // MARK: The phone

    private func watchThePhone() {
        let center = NotificationCenter.default
        let lines: [(Notification.Name, String)] = [
            (UIApplication.protectedDataWillBecomeUnavailableNotification, "locking"),
            (UIApplication.protectedDataDidBecomeAvailableNotification, "unlocked"),
            (UIApplication.didEnterBackgroundNotification, "in the background"),
            (UIApplication.willEnterForegroundNotification, "coming to the front"),
            (UIApplication.didBecomeActiveNotification, "active"),
            (UIApplication.willResignActiveNotification, "no longer active"),
        ]
        for (name, line) in lines {
            observers.append(center.addObserver(forName: name, object: nil, queue: .main) { [weak self] _ in
                MainActor.assumeIsolated {
                    self?.note(.phone, line)
                }
            })
        }
        observers.append(center.addObserver(forName: AVAudioSession.interruptionNotification, object: nil,
                                            queue: .main) { [weak self] notification in
            let line = Self.interruption(notification.userInfo)
            MainActor.assumeIsolated {
                self?.note(.phone, line)
            }
        })
        observers.append(center.addObserver(forName: AVAudioSession.routeChangeNotification, object: nil,
                                            queue: .main) { [weak self] notification in
            let reason = (notification.userInfo?[AVAudioSessionRouteChangeReasonKey] as? UInt) ?? 0
            MainActor.assumeIsolated {
                self?.note(.phone, "audio route changed (reason \(reason)), output \(Self.output())")
            }
        })
    }

    nonisolated static func interruption(_ info: [AnyHashable: Any]?) -> String {
        let type = (info?[AVAudioSessionInterruptionTypeKey] as? UInt).flatMap(AVAudioSession.InterruptionType.init)
        switch type {
        case .began:
            let reason = (info?[AVAudioSessionInterruptionReasonKey] as? UInt) ?? 0
            return "audio interrupted (reason \(reason))"
        case .ended:
            let options = (info?[AVAudioSessionInterruptionOptionKey] as? UInt) ?? 0
            let resume = AVAudioSession.InterruptionOptions(rawValue: options).contains(.shouldResume)
            return resume ? "audio interruption ended, may resume" : "audio interruption ended"
        default:
            return "audio interruption of an unknown kind"
        }
    }

    /// The outputs sound goes to now, by kind (never a device's own name).
    static func output() -> String {
        let outputs = AVAudioSession.sharedInstance().currentRoute.outputs.map(\.portType.rawValue)
        return outputs.isEmpty ? "none" : outputs.joined(separator: ", ")
    }

    /// While joined and not in front, a line every few seconds shows the
    /// app is still running and where sound goes.
    private func beat() {
        guard joined, UIApplication.shared.applicationState != .active else { return }
        let session = AVAudioSession.sharedInstance()
        note(.phone, "still running, output \(Self.output()), other audio \(session.isOtherAudioPlaying ? "playing" : "silent")")
    }
}

#if canImport(PushToTalk)
extension PttProbe: PTChannelManagerDelegate, PTChannelRestorationDelegate {
    private func readyManager() async -> PTChannelManager? {
        if let manager {
            return manager
        }
        if let creating {
            return await creating.value
        }
        let task = Task { @MainActor [weak self] () -> PTChannelManager? in
            guard let self else { return nil }
            do {
                let made = try await PTChannelManager.channelManager(delegate: self, restorationDelegate: self)
                self.manager = made
                self.note(.channel, "channel manager ready")
                return made
            } catch {
                self.problem = error.localizedDescription
                self.note(.channel, "channel manager failed: \(error.localizedDescription)")
                return nil
            }
        }
        creating = task
        let made = await task.value
        creating = nil
        return made
    }

    /// Runs `work` on the main queue in the order the callbacks came.
    nonisolated private func onMain(_ work: @escaping @MainActor @Sendable (PttProbe) -> Void) {
        DispatchQueue.main.async {
            MainActor.assumeIsolated {
                work(self)
            }
        }
    }

    nonisolated static func requestSource(_ source: PTChannelTransmitRequestSource) -> PttProbeRequestSource {
        switch source {
        case .userRequest: .userRequest
        case .developerRequest: .developerRequest
        case .handsfreeButton: .handsfreeButton
        case .unknown: .unknown
        @unknown default: .unknown
        }
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager, didJoinChannel channelUUID: UUID,
                                    reason: PTChannelJoinReason) {
        let line = reason == .channelRestoration ? "joined (restored by the system)" : "joined"
        onMain { probe in
            probe.joined = true
            probe.headsetEvents = true
            probe.problem = nil
            probe.note(.channel, line)
        }
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager, didLeaveChannel channelUUID: UUID,
                                    reason: PTChannelLeaveReason) {
        let why = switch reason {
        case .userRequest: "left from the system's Push to Talk interface"
        case .developerRequest: "left"
        case .systemPolicy: "left by the system's policy"
        default: "left for a reason the system did not give"
        }
        onMain { probe in
            probe.joined = false
            probe.microphone.stop()
            probe.log.channelLeft(moment: probe.moment(), detail: why)
        }
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager, channelUUID: UUID,
                                    didBeginTransmittingFrom source: PTChannelTransmitRequestSource) {
        let request = Self.requestSource(source)
        onMain { probe in
            probe.log.began(from: request, moment: probe.moment())
        }
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager, channelUUID: UUID,
                                    didEndTransmittingFrom source: PTChannelTransmitRequestSource) {
        let request = Self.requestSource(source)
        onMain { probe in
            let buffers = probe.microphone.buffers
            probe.microphone.stop()
            probe.log.ended(from: request, moment: probe.moment(), microphoneBuffers: buffers)
        }
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager, receivedEphemeralPushToken pushToken: Data) {
        onMain { probe in
            probe.note(.channel, "push token received (the test does not use it)")
        }
    }

    nonisolated func incomingPushResult(channelManager: PTChannelManager, channelUUID: UUID,
                                        pushPayload: [String: Any]) -> PTPushResult {
        onMain { probe in
            probe.note(.channel, "a push arrived, which the test does not expect: leaving")
        }
        return .leaveChannel
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager, didActivate audioSession: AVAudioSession) {
        onMain { probe in
            probe.note(.channel, "audio session on, output \(Self.output())")
            probe.note(.channel, probe.microphone.start())
        }
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager, didDeactivate audioSession: AVAudioSession) {
        onMain { probe in
            let buffers = probe.microphone.buffers
            probe.microphone.stop()
            probe.note(.channel, "audio session off, microphone buffers \(buffers)")
        }
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager, failedToJoinChannel channelUUID: UUID,
                                    error: any Error) {
        let line = "did not join: \(error.localizedDescription)"
        onMain { probe in
            probe.problem = line
            probe.note(.channel, line)
        }
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager, failedToLeaveChannel channelUUID: UUID,
                                    error: any Error) {
        let line = "did not leave: \(error.localizedDescription)"
        onMain { probe in
            probe.note(.channel, line)
        }
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager,
                                    failedToBeginTransmittingInChannel channelUUID: UUID, error: any Error) {
        let line = "did not begin: \(error.localizedDescription)"
        onMain { probe in
            probe.log.requestFailed(moment: probe.moment(), detail: line)
        }
    }

    nonisolated func channelManager(_ channelManager: PTChannelManager,
                                    failedToStopTransmittingInChannel channelUUID: UUID, error: any Error) {
        let line = "did not end: \(error.localizedDescription)"
        onMain { probe in
            probe.log.requestFailed(moment: probe.moment(), detail: line)
        }
    }

    nonisolated func channelDescriptor(restoredChannelUUID channelUUID: UUID) -> PTChannelDescriptor {
        PTChannelDescriptor(name: PttProbe.channelName, image: nil)
    }
}
#endif
#endif
