// NereusSDR for iOS: plays the Core's audio through an audio engine on whatever route the session has
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Darwin
import NereusMedia
import os

/// ``PlaybackOutput`` over an `AVAudioEngine` that plays an
/// ``AudioPlaybackCore`` through its source node into the main mixer. The
/// engine uses only its output; it never touches the microphone.
///
/// When the output hardware changes (AirPods connect, the route moves to
/// the earpiece) the engine stops itself and posts a configuration change;
/// this restarts it if it was playing. After the phone's media services
/// reset, ``reset()`` throws the engine away and the next start builds a
/// new one.
@MainActor
final class EnginePlaybackOutput: PlaybackOutput {
    private static let logger = Logger(subsystem: "NereusSDR", category: "audio.playback")

    private let core: AudioPlaybackCore
    private var engine = AVAudioEngine()
    private var sourceNode: AVAudioSourceNode?
    /// True between ``start()`` and ``pause()`` or ``stop()``.
    private var wantsToPlay = false
    /// The band is silenced (``setMuted(_:)``), kept across a reset.
    private var muted = false
    /// Configuration changes heard while playing, for the log.
    private var configurationChanges = 0
    private var outputEpoch: UInt64?
    private var renderTimeline = PlaybackRenderTimeline()
    nonisolated(unsafe) private var observer: (any NSObjectProtocol)?

    init(core: AudioPlaybackCore) {
        self.core = core
        observeEngine()
    }

    deinit {
        if let observer {
            NotificationCenter.default.removeObserver(observer)
        }
    }

    func start() throws {
        if sourceNode == nil {
            guard let node = core.makeSourceNode() else {
                throw PlaybackOutputError.noSourceNode
            }
            engine.attach(node)
            engine.connect(node, to: engine.mainMixerNode, format: node.outputFormat(forBus: 0))
            sourceNode = node
        }
        core.start()
        engine.prepare()
        outputEpoch = core.outputDidStart()
        renderTimeline.reset()
        do {
            try engine.start()
        } catch {
            retireTiming()
            throw error
        }
        wantsToPlay = true
    }

    func pause() {
        retireTiming()
        wantsToPlay = false
        engine.pause()
    }

    func stop() {
        retireTiming()
        wantsToPlay = false
        engine.stop()
        core.stop()
    }

    func setMuted(_ muted: Bool) {
        self.muted = muted
        engine.mainMixerNode.outputVolume = muted ? 0 : 1
    }

    func reset() {
        retireTiming()
        wantsToPlay = false
        engine.stop()
        core.stop()
        if let observer {
            NotificationCenter.default.removeObserver(observer)
        }
        engine = AVAudioEngine()
        engine.mainMixerNode.outputVolume = muted ? 0 : 1
        sourceNode = nil
        observeEngine()
    }

    private func observeEngine() {
        let core = self.core
        observer = NotificationCenter.default.addObserver(forName: .AVAudioEngineConfigurationChange,
                                                          object: engine, queue: nil) { [weak self] _ in
            core.outputDidRetire()
            Task { @MainActor in
                self?.restartAfterConfigurationChange()
            }
        }
    }

    private func restartAfterConfigurationChange() {
        retireTiming()
        guard wantsToPlay else {
            return
        }
        configurationChanges += 1
        // Each one can break up the band (an I/O buffer or route change
        // stops the engine): the count shows a run of them in the log.
        let running = engine.isRunning
        let count = configurationChanges
        Self.logger.notice(
            "audio engine configuration change \(count, privacy: .public) while playing, running \(running, privacy: .public)")
        guard !running else {
            outputEpoch = core.outputDidStart()
            return
        }
        do {
            engine.prepare()
            outputEpoch = core.outputDidStart()
            try engine.start()
        } catch {
            retireTiming()
            Self.logger.warning("the audio engine did not restart after a route change: \(error.localizedDescription)")
        }
    }

    private func retireTiming() {
        core.outputDidRetire()
        outputEpoch = nil
        renderTimeline.reset()
    }

    func timingObservation() -> PlaybackOutputTimingObservation {
        guard wantsToPlay, let outputEpoch else {
            return PlaybackOutputTimingObservation(outputEpoch: nil, renderedDeviceFrames: nil,
                                                   deviceRateHz: nil, progressAgeMs: nil,
                                                   presentationLatencyMs: nil, unavailableReason: .stopped)
        }
        guard core.currentOutputEpoch == outputEpoch else {
            return PlaybackOutputTimingObservation(outputEpoch: nil, renderedDeviceFrames: nil,
                                                   deviceRateHz: nil, progressAgeMs: nil,
                                                   presentationLatencyMs: nil, unavailableReason: .routeChanged)
        }
        guard engine.isRunning else {
            return PlaybackOutputTimingObservation(outputEpoch: outputEpoch, renderedDeviceFrames: nil,
                                                   deviceRateHz: nil, progressAgeMs: nil,
                                                   presentationLatencyMs: nil, unavailableReason: .notRunning)
        }
        let readStartNs = DispatchTime.now().uptimeNanoseconds
        let time = engine.outputNode.lastRenderTime
        let sourceRate = sourceNode?.outputFormat(forBus: 0).sampleRate
        let deviceRate = engine.outputNode.outputFormat(forBus: 0).sampleRate
        let sourceLatency = sourceNode?.outputPresentationLatency
        let deviceLatency = engine.outputNode.presentationLatency
        let readEndNs = DispatchTime.now().uptimeNanoseconds
        let sourceLatencyMs = sourceLatency.map { $0 * 1_000 }.flatMap { $0.isFinite && $0 > 0 ? $0 : nil }
        let deviceLatencyMs = (deviceLatency * 1_000).isFinite && deviceLatency > 0
            ? deviceLatency * 1_000 : nil
        guard core.currentOutputEpoch == outputEpoch else {
            return PlaybackOutputTimingObservation(outputEpoch: nil, renderedDeviceFrames: nil,
                                                   deviceRateHz: nil, progressAgeMs: nil,
                                                   presentationLatencyMs: nil, unavailableReason: .routeChanged)
        }
        guard let time else {
            return PlaybackOutputTimingObservation(outputEpoch: outputEpoch, renderedDeviceFrames: nil,
                                                   deviceRateHz: nil, progressAgeMs: nil,
                                                   presentationLatencyMs: nil, unavailableReason: .noRenderTime)
        }
        let schedule: PlaybackOutputScheduleObservation?
        if let sourceRate, sourceRate.isFinite, sourceRate > 0,
           deviceRate.isFinite, deviceRate > 0,
           time.isHostTimeValid || time.isSampleTimeValid {
            schedule = PlaybackOutputScheduleObservation(
                outputEpoch: outputEpoch,
                measuredNs: readStartNs + (readEndNs - readStartNs) / 2,
                readWindowNs: readEndNs - readStartNs,
                deviceRenderHostTicks: time.isHostTimeValid ? time.hostTime : nil,
                deviceRenderSampleTime: time.isSampleTimeValid ? time.sampleTime : nil,
                deviceRenderSampleRateHz: time.sampleRate.isFinite && time.sampleRate > 0
                    ? time.sampleRate : nil,
                sourceNodeOutputRateHz: sourceRate,
                deviceOutputRateHz: deviceRate,
                sourceOutputPresentationLatencyMs: sourceLatencyMs,
                devicePresentationLatencyMs: deviceLatencyMs)
        } else {
            schedule = nil
        }
        guard time.isHostTimeValid, time.isSampleTimeValid, time.sampleRate.isFinite,
              time.sampleRate > 0, time.sampleTime >= 0 else {
            return PlaybackOutputTimingObservation(outputEpoch: outputEpoch, renderedDeviceFrames: nil,
                                                   deviceRateHz: nil, progressAgeMs: nil,
                                                   presentationLatencyMs: nil, unavailableReason: .invalidRenderTime,
                                                   schedule: schedule)
        }
        guard let renderedFrames = renderTimeline.progress(sampleTime: time.sampleTime,
                                                           hostTime: time.hostTime,
                                                           rateHz: time.sampleRate) else {
            retireTiming()
            return PlaybackOutputTimingObservation(outputEpoch: nil, renderedDeviceFrames: nil,
                                                   deviceRateHz: nil, progressAgeMs: nil,
                                                   presentationLatencyMs: nil, unavailableReason: .routeChanged)
        }
        let now = mach_absolute_time()
        let age = max(0, (AVAudioTime.seconds(forHostTime: now)
                          - AVAudioTime.seconds(forHostTime: time.hostTime)) * 1_000)
        return PlaybackOutputTimingObservation(
            outputEpoch: outputEpoch, renderedDeviceFrames: renderedFrames,
            deviceRateHz: time.sampleRate, progressAgeMs: age.isFinite ? age : nil,
            presentationLatencyMs: deviceLatencyMs,
            unavailableReason: nil, schedule: schedule)
    }
}
