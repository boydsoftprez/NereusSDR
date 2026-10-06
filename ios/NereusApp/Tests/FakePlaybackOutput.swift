// NereusSDR for iOS: a stand-in for the band's audio output that starts no audio engine
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

@testable import NereusSDR

/// A ``PlaybackOutput`` that only counts what it is asked to do.
@MainActor
final class FakePlaybackOutput: PlaybackOutput {
    private(set) var starts = 0
    private(set) var pauses = 0
    private(set) var stops = 0
    private(set) var resets = 0
    private(set) var isRunning = false
    private(set) var isMuted = false

    func start() throws {
        starts += 1
        isRunning = true
    }

    func pause() {
        pauses += 1
        isRunning = false
    }

    func stop() {
        stops += 1
        isRunning = false
    }

    func setMuted(_ muted: Bool) {
        isMuted = muted
    }

    func reset() {
        resets += 1
        isRunning = false
    }

    func timingObservation() -> PlaybackOutputTimingObservation {
        PlaybackOutputTimingObservation(outputEpoch: nil, renderedDeviceFrames: nil,
                                        deviceRateHz: nil, progressAgeMs: nil,
                                        presentationLatencyMs: nil, unavailableReason: .stopped)
    }
}
