// NereusSDR for iOS: what the media control client reports: the Core's media control operations, typed, and the media connection's own news
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// One thing ``MediaControlClient`` reports, in order. The Core's
/// operations arrive decoded in exactly the shape the negotiated
/// capabilities give (the media control document and the link document's
/// section 11); anything else is dropped before it gets here.
public enum MediaControlEvent: Sendable, Equatable {
    /// The Core's `description` (its offer).
    case description(Description)
    /// One of the Core's trickled `candidate`s.
    case candidate(Candidate)
    /// A display endpoint's accepted `context`.
    case context(DisplayContext)
    /// A `rejected`: one endpoint, or with endpoint and revision 0 the whole peer.
    case rejected(Rejection)
    /// A `noise-floor` for an endpoint's current context.
    case noiseFloor(NoiseFloor)
    /// An accepted `audio-context`.
    case audioContext(AudioContext)
    /// An `allocation-result`, on the display budget wire only.
    case allocationResult(AllocationResult)
    /// The media connection's state; `closed` also when the client retires it.
    case mediaState(MediaPeer.State)
    /// A display frame of an endpoint's current context, decoded.
    case displayFrame(DisplayFrame)
    /// The display extras the Core sent beside one of those frames, decoded
    /// against the endpoint's current context.
    case displayExtras(DisplayExtras)
    /// The Core refused a `clarity-retune` for an endpoint, in the shape of
    /// a refused display operation; the endpoint stays as it was.
    case clarityRetuneRefused(Rejection)
    /// The media connection now carries this phone's microphone line (true,
    /// once it is up with the line open) or no longer does (false, when it
    /// ends): VOX and the microphone follow it.
    case microphoneLine(Bool)
    /// The Core closed this phone's microphone line's track while the media
    /// connection stays up; it follows ``microphoneLine(_:)`` false. A line
    /// that goes with the connection never says this: a key held here is
    /// released for this, and a dying link has its own way of ending one.
    case microphoneTrackClosed
    /// App-owned microphone news retains its producer's logical owner and
    /// revocable selected-peer lifetime even while buffered in this stream.
    case microphoneLifecycle(MicrophoneLifecycle)

    public struct MicrophoneLifecycle: Sendable, Equatable {
        public enum Change: Sendable, Equatable {
            case line(Bool)
            case trackClosed
        }

        public let owner: UInt64
        public let authority: CommandSendPermit
        public let change: Change

        public var event: MediaControlEvent {
            switch change {
            case .line(let carried): .microphoneLine(carried)
            case .trackClosed: .microphoneTrackClosed
            }
        }

        public static func == (lhs: Self, rhs: Self) -> Bool {
            lhs.owner == rhs.owner && lhs.authority === rhs.authority && lhs.change == rhs.change
        }
    }
    /// An accepted `monitor-audio-context`, the answer to this phone's
    /// latest `monitor-audio` on the current media connection.
    case monitorAudioContext(MonitorAudioContext)
    /// The phone's lossless link trial failed: the network could not carry
    /// lossless audio, so the client asked the Core for Opus (R-IOS-09).
    /// Lossless stays chosen; choosing it again gives it a fresh chance.
    case losslessFallback

    public struct Description: Sendable, Equatable {
        public var sdp: String
        public var type: String
    }

    public struct Candidate: Sendable, Equatable {
        public var candidate: String
        public var mid: String
    }

    /// `context`: the 19 fields, plus `wideband` for a subscription that
    /// negotiated the extended view and the five grant fields with
    /// `spectrumGrantVersion` at minor 9 or more.
    public struct DisplayContext: Sendable, Equatable {
        public var endpointId: UInt32
        public var revision: UInt32
        public var contextGeneration: UInt32
        public var sourceStream: Int
        public var sourceCentreHz: Double
        public var sampleRateHz: Double
        public var centreHz: Double
        public var spanHz: Double
        public var wideCentreHz: Double
        public var wideSpanHz: Double
        public var traceSamples: Int
        public var waterfallSamples: Int
        public var wideSamples: Int
        public var minDbm: Double
        public var maxDbm: Double
        public var fps: Int
        public var framesPerLine: Int
        public var wideband: Wideband?
        public var grant: Grant?
        /// `transmit`: true for the Core's transmit display while keyed,
        /// false for the receiver's; nil on a connection whose `start`
        /// declared no `txDisplayVersion`, whose contexts never carry it.
        public var transmit: Bool? = nil

        /// The widest span this context allows: the ADC's half rate when
        /// the Core says the extended view is available and that is wider,
        /// else the DDC's sample rate.
        public var maxSpanHz: Double {
            Self.spanCeilingHz(sampleRateHz: sampleRateHz, wideband: wideband)
        }

        /// The span ceiling for a context of this rate and wideband object;
        /// the decoder checks the context's span against it.
        public static func spanCeilingHz(sampleRateHz: Double, wideband: Wideband?) -> Double {
            wideband.flatMap { $0.available ? max(sampleRateHz, ($0.adcRateHz ?? 0) / 2) : nil } ?? sampleRateHz
        }
    }

    /// A context's `wideband` object: `version`, `available` and `active`,
    /// and while available the ADC it reads.
    public struct Wideband: Sendable, Equatable {
        public var available: Bool
        public var active: Bool
        public var physicalAdcIndex: Int?
        public var filterChainIndex: Int?
        public var sourceGeneration: UInt32?
        public var adcRateHz: Double?
    }

    /// What the Core granted an endpoint.
    public struct Grant: Sendable, Equatable {
        public enum Limit: String, Sendable, Equatable {
            case none
            case largestSize = "largest-size"
            case shared
            case sourceBins = "source-bins"
        }

        public var grantedFftSize: Int
        public var grantedTier: DisplaySubscription.Tier
        public var requestedPixels: Int
        public var grantedPixels: Int
        public var limit: Limit
    }

    public struct Rejection: Sendable, Equatable {
        public var endpointId: UInt32
        public var revision: UInt32
        /// The Core's words, as sent.
        public var reason: String

        public init(endpointId: UInt32, revision: UInt32, reason: String) {
            self.endpointId = endpointId
            self.revision = revision
            self.reason = reason
        }

        /// Endpoint and revision 0: the Core refused the whole media peer.
        public var refusesWholePeer: Bool {
            endpointId == 0 && revision == 0
        }
    }

    public struct NoiseFloor: Sendable, Equatable {
        public var endpointId: UInt32
        public var revision: UInt32
        public var contextGeneration: UInt32
        public var floorDbm: Double
    }

    /// `audio-context`: the eight fields, plus with the audio detail
    /// `encoder` (on) or `reason` (off), plus with the audio profile
    /// `profile` and perhaps `profileRefusal`.
    public struct AudioContext: Sendable, Equatable {
        public enum OffReason: String, Sendable, Equatable {
            case clientDisabled = "client-disabled"
            case mediaNotReady = "media-not-ready"
            case radioOffline = "radio-offline"
            case encoderUnavailable = "encoder-unavailable"
        }

        public enum Profile: String, Sendable, Equatable {
            case opus
            case lossless
        }

        public enum ProfileRefusal: String, Sendable, Equatable {
            case notAllowed = "lossless-not-allowed"
            case unavailable = "lossless-unavailable"
        }

        /// The Opus encoder the Core runs.
        public struct OpusEncoder: Sendable, Equatable {
            public var sampleRate: Int
            public var channels: Int
            public var frameSamples: Int
            public var targetBitrate: Int
            public var audioBandwidthHz: Int
        }

        public var revision: UInt32
        public var enabled: Bool
        public var anchor: AudioStreamAnchor
        public var encoder: OpusEncoder?
        /// True when the Core described lossless (L16) packets; the anchor's
        /// format is then ``AudioStreamFormat/l16``.
        public var lossless: Bool
        public var offReason: OffReason?
        public var profile: Profile?
        public var profileRefusal: ProfileRefusal?
        /// Why the Core did not take this device's `opusBitrate`, in its
        /// own words; the encoder it runs stays as it was.
        public var opusBitrateRefusal: String? = nil
    }

    public struct AllocationResult: Sendable, Equatable {
        public var endpointId: UInt32
        public var revision: UInt32
        public var accepted: Bool
        /// Empty when accepted; the Core's words otherwise.
        public var reason: String
        public var budgetGeneration: UInt32
        /// The revision the Core now holds for the endpoint, or 0.
        public var acceptedRevision: UInt32
        public var applicationBytesPerSecond: UInt64
        public var spectrumSampleUnitsPerSecond: UInt64
        public var messagesPerSecond: UInt32
    }
}
