// NereusSDR for iOS: where one audio context's RTP stream starts, as the Core's audio-context gives it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The start of one audio context's stream (the media control document's
/// `audio-context`): its `generation`, the `ssrc` its packets carry, and
/// the RTP sequence number and timestamp its first packet has. Playback
/// starts over from here each time the Core sends a new context. `format`
/// is what its packets carry: Opus, or the lossless profile's L16.
public struct AudioStreamAnchor: Sendable, Equatable {
    public var generation: UInt32
    public var ssrc: UInt32
    public var firstSequence: UInt16
    public var firstTimestamp: UInt32
    public var format: AudioStreamFormat

    public init(generation: UInt32, ssrc: UInt32, firstSequence: UInt16, firstTimestamp: UInt32,
                format: AudioStreamFormat = .opus) {
        self.generation = generation
        self.ssrc = ssrc
        self.firstSequence = firstSequence
        self.firstTimestamp = firstTimestamp
        self.format = format
    }
}

/// What one audio stream's RTP packets carry.
public enum AudioStreamFormat: Sendable, Equatable {
    /// Opus at payload type 111, 1920 frames (40 ms) a packet.
    case opus
    /// The lossless profile: L16 at payload type 96, 192 frames (4 ms) a packet.
    case l16
}
