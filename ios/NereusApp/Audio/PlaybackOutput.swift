// NereusSDR for iOS: the audio output the band plays through, so tests can stand in for the engine
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// A platform render timeline, tied to one output lifetime. AVAudioTime's
/// device sample time tracks rendering and does not prove audible consumption.
struct PlaybackOutputTimingObservation: Equatable {
    enum UnavailableReason: Equatable {
        case stopped
        case notRunning
        case noRenderTime
        case invalidRenderTime
        case routeChanged
    }
    let outputEpoch: UInt64?
    let renderedDeviceFrames: Int64?
    let deviceRateHz: Double?
    let progressAgeMs: Double?
    let presentationLatencyMs: Double?
    let unavailableReason: UnavailableReason?
    /// A separate raw platform read; no queue depth or audible-consumption claim.
    let schedule: PlaybackOutputScheduleObservation?

    init(outputEpoch: UInt64?, renderedDeviceFrames: Int64?, deviceRateHz: Double?,
         progressAgeMs: Double?, presentationLatencyMs: Double?,
         unavailableReason: UnavailableReason?, schedule: PlaybackOutputScheduleObservation? = nil) {
        self.outputEpoch = outputEpoch
        self.renderedDeviceFrames = renderedDeviceFrames
        self.deviceRateHz = deviceRateHz
        self.progressAgeMs = progressAgeMs
        self.presentationLatencyMs = presentationLatencyMs
        self.unavailableReason = unavailableReason
        self.schedule = schedule
    }
    /// AVAudioEngine has no exact audible-consumption counter here.
    var consumedDeviceFrames: Int64? { nil }
}

/// One off-render read of the current output graph. The source node's maximum
/// downstream latency includes the device term and must not be added to it.
struct PlaybackOutputScheduleObservation: Equatable {
    let outputEpoch: UInt64
    let measuredNs: UInt64
    let readWindowNs: UInt64
    let deviceRenderHostTicks: UInt64?
    let deviceRenderSampleTime: Int64?
    let deviceRenderSampleRateHz: Double?
    let sourceNodeOutputRateHz: Double
    let deviceOutputRateHz: Double
    let sourceOutputPresentationLatencyMs: Double?
    let devicePresentationLatencyMs: Double?
}

/// What ``AudioSessionController`` starts, pauses and stops. The app's is
/// ``EnginePlaybackOutput``; tests use a stand-in and start no audio engine.
@MainActor
protocol PlaybackOutput: AnyObject {
    /// Starts playing, or resumes after ``pause()``.
    func start() throws
    /// Stops the sound for a while, keeping everything ready to resume.
    func pause()
    /// Stops playing.
    func stop()
    /// Silences the band, or lets it be heard again, while it keeps
    /// playing: the audio still flows, so unmuting is instant.
    func setMuted(_ muted: Bool)
    /// Throws the output away after the phone's media services reset; the
    /// next ``start()`` builds it again.
    func reset()
    /// Read-only platform timing, or a reason it is unavailable.
    func timingObservation() -> PlaybackOutputTimingObservation
}
