// NereusSDR for iOS: which parts of media control the Core and the agreed link minor allow
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The two-key gates of media control (link document section 6.2): each
/// part is on only when the agreed minor has reached the minor it arrived
/// in and the Core advertises its capability version at 1 or more. The
/// minors are the station's (`SessionMessages.h`); the field sets each
/// gate selects are the media control document's.
public struct MediaFeatureGates: Sendable, Equatable {
    /// `kMediaSessionProtocolMinor`.
    public static let mediaMinor: UInt16 = 1
    /// `kRemoteWidebandSessionProtocolMinor`: the extended view.
    public static let widebandMinor: UInt16 = 6
    /// `kRemoteDisplayBudgetSessionProtocolMinor`: the display budget wire.
    public static let displayBudgetMinor: UInt16 = 7
    /// `kRemoteAudioStatusSessionProtocolMinor`: the audio context's detail
    /// and, with `audioProfileVersion`, the audio profile.
    public static let audioStatusMinor: UInt16 = 8
    /// `kRemoteSpectrumGrantSessionProtocolMinor`: the context's grant fields.
    public static let spectrumGrantMinor: UInt16 = 9
    /// The minor the Core's catalogue and display extras arrived in (11).
    public static let displayExtrasMinor: UInt16 = 11
    /// The minor remote transmit arrived in (11): the microphone line and
    /// the "tx" data channel ride on it (link document section 18).
    public static let remoteTxMinor: UInt16 = 11
    /// The minor the transmit display arrived in (11): `txDisplayVersion`
    /// in `start`, the transmit window and `duplex` in `subscribe`, and
    /// `transmit` in each context (the media control document, "Transmit
    /// display").
    public static let txDisplayMinor: UInt16 = 11
    /// The Core's in-session media replacement arrived in minor 11.
    public static let mediaReplaceMinor: UInt16 = 11
    /// The binary direct media tunnel and UUID-routed web relay arrived in minor 11.
    public static let mediaTunnelMinor: UInt16 = 11
    public static let mediaRelayRoutingMinor: UInt16 = 11
    /// The direct media ladder (`mediaDirect` 1) arrived in minor 11.
    public static let mediaDirectMinor: UInt16 = 11
    /// The transmit monitor (`monitor-audio`) rides the minor-11 block.
    public static let txMonitorAudioMinor: UInt16 = 11
    /// Per-device audio quality (`audioQualityVersion`) rides the minor-11
    /// block, last in it (the media control document, "Per-device audio
    /// quality").
    public static let audioQualityMinor: UInt16 = 11

    /// Media control at all: `remoteMediaVersion`.
    public var media: Bool
    /// `extendedView` in `subscribe` and `wideband` in its context.
    public var wideband: Bool
    /// `revision` in `unsubscribe`, and `allocation-result` in place of `rejected`.
    public var displayBudget: Bool
    /// The audio context's `encoder` or `reason`.
    public var audioDetail: Bool
    /// `profile` in `audio`, `audioProfileVersion` in `start`, and the
    /// context's profile fields.
    public var audioProfile: Bool
    /// Core answers exact clock probes for media audio, with no extra minor.
    public var audioClock: Bool
    /// The context's five grant fields.
    public var spectrumGrant: Bool
    /// `decimation` in `subscribe`: `spectrumGrantVersion` 2.
    public var decimation: Bool
    /// The display extras fields in `subscribe`, and NSDX datagrams beside
    /// the frames: `displayExtrasVersion`.
    public var displayExtras: Bool
    /// `clarity-retune`, the Core's Re-tune of a display endpoint's
    /// Clarity: `displayExtrasVersion` 2.
    public var clarityRetune: Bool
    /// `onTx` in `activePeakHold`, and the Core holding each peak bin for
    /// `holdMs` before it falls: `displayExtrasVersion` 3.
    public var peakHoldOnTx: Bool
    /// `fastAttack` in `noiseFloor`, and the one-byte state section 0x10
    /// that says whether the floor is in fast attack: `displayExtrasVersion` 4.
    public var noiseFloorFastAttack: Bool
    /// `remoteTxVersion` in `start`, and with it the microphone line and the
    /// "tx" data channel (the media control document, "Microphone line"):
    /// the Core tells a session `remoteTxVersion` only when its hello
    /// declared `remoteTx` 1 at minor 11.
    public var remoteTx: Bool
    /// The Core's transmit display on the transmitting pan while keyed:
    /// `txDisplayVersion` 1 or more.
    public var txDisplay: Bool
    /// The Core applies this app's writes of its transmit display settings
    /// (the nine `DisplayTx*` keys): `txDisplayVersion` 2 or more.
    public var txDisplaySettings: Bool
    /// Display duplex, the receiver kept on the band while keyed:
    /// `txDisplayVersion` 3 or more.
    public var displayDuplex: Bool
    /// `replace` beside an existing ready media peer.
    public var mediaReplace: Bool
    public var mediaTunnel: Bool
    public var mediaRelayRouting: Bool
    /// `mediaDirectVersion` 1 in a `replace`: a direct-only connection
    /// (the direct media ladder, ``MediaDirectLadder``). The Core sends the
    /// capability only to a peer whose hello declared `mediaDirect` 1.
    public var mediaDirect: Bool
    /// `txMonitorAudioVersion` 1 in `start`, and with it `monitor-audio`
    /// and its `monitor-audio-context` (the media control document,
    /// "Transmit monitor (monitor-audio)").
    public var txMonitorAudio: Bool
    /// `opusBitrate` in `audio`, beside `profile`, and `opusBitrateRefusal`
    /// in the answering context: `audioQualityVersion` 1, which the Core
    /// tells only a device whose hello declared `audioQuality` 1.
    public var audioQuality: Bool

    public init(agreedMinor: UInt16, capabilityVersion: (String) -> Int64) {
        media = agreedMinor >= Self.mediaMinor && capabilityVersion("remoteMediaVersion") >= 1
        wideband = media && agreedMinor >= Self.widebandMinor
            && capabilityVersion("remoteWidebandDisplayVersion") >= 1
        displayBudget = media && agreedMinor >= Self.displayBudgetMinor
            && capabilityVersion("remoteDisplayBudgetVersion") >= 1
        audioDetail = media && agreedMinor >= Self.audioStatusMinor
            && capabilityVersion("remoteAudioStatusVersion") >= 1
        audioProfile = audioDetail && capabilityVersion("audioProfileVersion") >= 1
        audioClock = media && capabilityVersion("audioClockVersion") >= 1
        spectrumGrant = media && agreedMinor >= Self.spectrumGrantMinor
            && capabilityVersion("spectrumGrantVersion") >= 1
        decimation = spectrumGrant && capabilityVersion("spectrumGrantVersion") >= 2
        displayExtras = media && agreedMinor >= Self.displayExtrasMinor
            && capabilityVersion("displayExtrasVersion") >= 1
        clarityRetune = displayExtras && capabilityVersion("displayExtrasVersion") >= 2
        peakHoldOnTx = displayExtras && capabilityVersion("displayExtrasVersion") >= 3
        noiseFloorFastAttack = displayExtras && capabilityVersion("displayExtrasVersion") >= 4
        remoteTx = media && agreedMinor >= Self.remoteTxMinor && capabilityVersion("remoteTxVersion") >= 1
        txDisplay = media && agreedMinor >= Self.txDisplayMinor && capabilityVersion("txDisplayVersion") >= 1
        txDisplaySettings = txDisplay && capabilityVersion("txDisplayVersion") >= 2
        displayDuplex = txDisplay && capabilityVersion("txDisplayVersion") >= 3
        mediaReplace = media && agreedMinor >= Self.mediaReplaceMinor
            && capabilityVersion("mediaReplaceVersion") >= 1
        mediaTunnel = media && agreedMinor >= Self.mediaTunnelMinor
            && capabilityVersion("mediaTunnelVersion") >= 1
        mediaRelayRouting = media && agreedMinor >= Self.mediaRelayRoutingMinor
            && capabilityVersion("mediaRelayRoutingVersion") >= 1
        mediaDirect = mediaReplace && agreedMinor >= Self.mediaDirectMinor
            && capabilityVersion("mediaDirectVersion") == 1
        txMonitorAudio = media && agreedMinor >= Self.txMonitorAudioMinor
            && capabilityVersion("txMonitorAudioVersion") >= 1
        // opusBitrate goes only beside profile, so the profile gate too.
        audioQuality = audioProfile && agreedMinor >= Self.audioQualityMinor
            && capabilityVersion("audioQualityVersion") >= 1
    }

    /// The `txDisplayVersion` a `start` declares to this Core: 3 where it
    /// offers display duplex, 1 where it offers the transmit display, and
    /// nil (the field left out) where it offers neither.
    public var declaredTxDisplayVersion: Int? {
        displayDuplex ? 3 : (txDisplay ? 1 : nil)
    }

    /// These gates as a connection whose `start` declared
    /// `txDisplayVersion` `declared` (nil for none) may use them: the
    /// transmit display's fields go only where the start declared it.
    public func declaring(txDisplayVersion declared: Int?) -> MediaFeatureGates {
        var gates = self
        let version = declared ?? 0
        gates.txDisplay = txDisplay && version >= 1
        gates.displayDuplex = displayDuplex && version >= 3
        return gates
    }

    /// Every gate off.
    public static let none = MediaFeatureGates(agreedMinor: 0) { _ in 0 }
}
