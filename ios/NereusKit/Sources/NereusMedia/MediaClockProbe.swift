// NereusSDR for iOS: the current media owner's Core clock exchange values
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// Caller evidence from the actual playback output. Revisions increase for
/// every activity change within one logical owner, including pause and stop.
/// The caller captures the revision before asynchronous collection and checks
/// its output identity again before publishing. `observedNs` uses the same
/// monotonic nanosecond origin as AudioPlayoutPoint and AudioReleasePoint.
public struct MediaClockPlaybackActivity: Sendable, Equatable {
    public let mediaID: String
    public let playbackLifetime: UInt64
    public let outputEpoch: UInt64
    public let revision: UInt64
    public let generation: UInt32
    public let observedNs: Int64
    public let isPlaying: Bool

    public init(mediaID: String, playbackLifetime: UInt64, outputEpoch: UInt64,
                revision: UInt64, generation: UInt32, observedNs: Int64, isPlaying: Bool) {
        self.mediaID = mediaID; self.playbackLifetime = playbackLifetime
        self.outputEpoch = outputEpoch; self.revision = revision
        self.generation = generation; self.observedNs = observedNs; self.isPlaying = isPlaying
    }
}

/// One immutable reading from the connected, accepted media connection.
/// `offset` becomes nil at three seconds without an echo. The capture anchor
/// is present only for the playing audio generation.
public struct SelectedAudioClockSnapshot: Sendable, Equatable {
    public let owner: UInt64
    public let mediaID: String?
    public let supported: Bool
    public let active: Bool
    public let playbackLifetime: UInt64?
    public let outputEpoch: UInt64?
    public let playingGeneration: UInt32?
    public let lastEchoNs: Int64?
    public let offset: AudioClockOffset?
    public let capture: AudioCaptureAnchor?
}

/// Strictly the Core's nine-field reply to one probe. LinkJSON carries all
/// numbers as Double, so integer checks apply to that decoded value.
struct MediaClockEcho {
    let id: UInt32
    let t0: Double
    let t0Ns: Int64
    let t1Ns: Int64
    let t2Ns: Int64
    let generation: UInt32
    let rtpTimestamp: UInt32
    let capturedNs: Int64

    init?(_ payload: [String: LinkJSON]) {
        guard Set(payload.keys) == ["op", "connectionId", "id", "t0", "t1", "t2",
                                    "generation", "rtpTimestamp", "capturedNs"],
              payload["op"] == .string("clock-echo"),
              case .string(_) = payload["connectionId"],
              let id = Self.uint32(payload["id"]),
              case .number(let t0) = payload["t0"],
              let t0Ns = Self.nanoseconds(payload["t0"]),
              let t1Ns = Self.nanoseconds(payload["t1"]),
              let t2Ns = Self.nanoseconds(payload["t2"]),
              let generation = Self.uint32(payload["generation"]),
              let rtpTimestamp = Self.uint32(payload["rtpTimestamp"]),
              let capturedNs = Self.nanoseconds(payload["capturedNs"]) else { return nil }
        self.id = id; self.t0 = t0; self.t0Ns = t0Ns
        self.t1Ns = t1Ns; self.t2Ns = t2Ns
        self.generation = generation; self.rtpTimestamp = rtpTimestamp
        self.capturedNs = capturedNs
    }

    private static func uint32(_ value: LinkJSON?) -> UInt32? {
        guard case .number(let number) = value, number.isFinite,
              number >= 0, number <= Double(UInt32.max), number.rounded(.towardZero) == number else {
            return nil
        }
        return UInt32(number)
    }

    private static func nanoseconds(_ value: LinkJSON?) -> Int64? {
        guard case .number(let number) = value, number.isFinite,
              number >= 0, number < Double(Int64.max), number.rounded(.towardZero) == number else {
            return nil
        }
        return Int64(number)
    }
}
