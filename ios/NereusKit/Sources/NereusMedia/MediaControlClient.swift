// NereusSDR for iOS: media control with the Core: the media connection's start and signalling, display endpoints, keyframes and audio
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CryptoKit
import Foundation
import NereusLink
import os

/// One immutable reading for the app's current logical media owner. The ID
/// and generation describe the selected `media` peer, never a pending
/// replacement or a draining old peer.
public struct SelectedMediaRouteSnapshot: Sendable, Equatable {
    public let owner: UInt64
    public let routeGeneration: Int
    public let mediaID: String?
    public let observation: SelectedRouteObservation
}

/// One reading from the authenticated logical owner's connected peer only.
/// A replacement under construction and a draining old peer are excluded.
public struct SelectedMediaTrafficSnapshot: Sendable, Equatable {
    public let owner: UInt64
    public let routeGeneration: Int
    public let mediaID: String?
    public let observation: MediaTrafficObservation?
}

/// One actor-coherent view of the connected, admitted current media peer.
/// Pending replacement and draining peers cannot contribute to this reading.
public struct SelectedMediaDiagnosticsSnapshot: Sendable {
    public let owner: UInt64
    public let routeGeneration: Int
    public let mediaID: String?
    public let route: SelectedRouteObservation
    public let traffic: MediaTrafficObservation?
    public let audioAdmission: MediaPeer.AudioAdmissionObservation?
    /// The accepted Core audio context, independent of the probe activity.
    public let acceptedAudioGeneration: UInt32?
    public let clock: SelectedAudioClockSnapshot
}

/// The app's side of media control (the media control document,
/// `2026-09-20-remote-media-control-v1.md`, and the link document's
/// section 11). Fed the session's events with ``handle(_:)``, it:
///
/// - starts one media connection per session, only when the Core offers
///   media (`remoteMediaVersion` 1 or more at agreed minor 1 or more) and
///   only after `snapshot.complete`, under a fresh canonical connection ID
///   that every operation carries;
/// - answers the Core's offer through a ``MediaPeerConnection`` and trickles
///   candidates both ways;
/// - sends `subscribe`, `unsubscribe`, `keyframe` and `audio` in exactly the
///   shapes the negotiated gates give (``MediaFeatureGates``);
/// - reports the Core's operations as ``MediaControlEvent``s, each decoded
///   in exactly its negotiated shape, and decodes each endpoint's display
///   frames once its context is accepted (frames before it are discarded),
///   with the NSDX display extras sent beside them;
/// - re-anchors playback on each accepted audio context and sets the peer's
///   expected SSRC.
///
/// A connection that is not connected within ``connectDeadline`` of its
/// `start`, or that fails, closes or is lost, is
/// restarted while the session stays ready: the first time at once, then
/// on the capped ``restartDelays`` schedule. A Core that dropped the
/// connection itself says so with one of ``restartingRefusalReasons``,
/// which restarts it too; any other whole-peer refusal is final. A session that ends retires
/// the connection with its subscriptions; the next session starts a new one. Audio stays off until
/// ``setAudioEnabled(_:)`` asks for it, and that wish carries over to the
/// next connection.
public actor MediaControlClient {
    @TaskLocal private static var operationOwner: UInt64?
    public typealias Sender = @Sendable (LinkMessage) async throws -> Void
    public typealias PeerFactory = @Sendable () -> any MediaPeerConnection
    /// Milliseconds on a clock that never goes back.
    public typealias MillisecondClock = @Sendable () -> UInt64
    /// Nanoseconds from the playback adapter's monotonic clock origin.
    public typealias NanosecondClock = @Sendable () -> Int64

    /// Keyframe requests allowed per endpoint in any second.
    public static let keyframesPerSecond = 5
    static let keyframeWindowMs: UInt64 = 1000
    /// The audio SSRC's derivation prefix (media control document, "Media peer").
    static let audioSsrcPrefix = "NereusSDR/media-audio-ssrc/v1:"
    /// The receiver streams', the headphones mix's and the microphone
    /// line's derivation prefixes (the same section).
    static let receiverSsrcPrefix = "NereusSDR/media-receiver-ssrc/v1:"
    static let headphonesSsrcPrefix = "NereusSDR/media-headphones-ssrc/v1:"
    static let microphoneSsrcPrefix = "NereusSDR/media-mic-ssrc/v1:"
    /// The Core's receiver streams, whose SSRCs the others step around.
    static let receiverStreams = 4
    /// NSDC display frames start with this, then the endpoint ID and context
    /// generation at bytes 8 and 12, big-endian (display codec document).
    static let displayMagic: [UInt8] = Array("NSDC".utf8)
    /// NSDX display extras datagrams start with this (display extras document).
    static let extrasMagic: [UInt8] = Array("NSDX".utf8)

    /// A new media connection that is not connected (display channel and
    /// audio line both open) this long after its `start` is restarted.
    /// ICE and DTLS finish in well under a second on a working path; 5 s
    /// leaves room for a lost first ICE check (the transport resends at
    /// 0.5 s, doubling) and a lost first DTLS flight on a slow cellular
    /// path, and cuts off the transport's own late retries, which run to
    /// 30 s and more. The phone is the DTLS client, so a lost flight is
    /// resent on the phone's own timer (RFC 6347 starts it at 1 s), which
    /// Mbed TLS then backs off towards 60 s: this deadline, not the Core,
    /// is what bounds that wait. The desktop's media deadline
    /// (src/gui/RemoteMediaController.h:30-70, kMediaEstablishmentDeadlineMs,
    /// 125.5 s) bounds a stalled connection before giving up; it is far too
    /// long to model how soon the phone tries again.
    public static let connectDeadline: Duration = .milliseconds(5_000)
    /// The wait before each restart in a row without a connection: the
    /// first at once, then doubling from 1 s, capped at
    /// ``maximumRestartDelay``. A connection that comes up starts it over.
    public static let restartDelays: [Duration] = [.zero, .seconds(1), .seconds(2), .seconds(4), .seconds(8)]
    public static let maximumRestartDelay: Duration = .seconds(10)
    /// A selected relay or tunnel with expected playing RX audio is retried
    /// before the ICE consent timeout when audio goes silent this long.
    public static let audioStallDeadline: Duration = .seconds(3)

    /// Why a media connection was started again.
    public enum RestartReason: String, Sendable, Equatable {
        case notConnectedInTime = "not connected within the deadline"
        case failed = "the connection failed"
        case closed = "the connection closed"
        case disconnected = "the connection was lost"
        case droppedByCore = "the Core dropped the connection"
        case networkChanged = "the phone's network changed"
        case asked = "asked"
        case audioStalled = "expected audio stalled"
        case directSilence = "no audio or display after moving back to the tunnel"
    }

    /// The whole-peer refusals (`rejected` with endpoint and revision 0)
    /// with which a Core says it dropped the media connection itself and
    /// the app should start it again: ICE consent lost or DTLS failed, and
    /// closed. Matched exactly, as the desktop window matches them; every
    /// other whole-peer refusal is final (among them "The Core could not
    /// start audio and display.", where a restart would only loop). The
    /// Core's reasons from codex/multi-client 2c4bac25 and 30b9117f.
    public static let restartingRefusalReasons: Set<String> = [
        "The Core lost the audio and display connection.",
        "The audio and display connection to the Core closed.",
    ]

    /// The wait before the `failures`th restart in a row (1 for the first).
    public static func restartDelay(afterFailures failures: Int) -> Duration {
        let index = max(failures, 1) - 1
        return index < restartDelays.count ? restartDelays[index] : maximumRestartDelay
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "media.control")

    /// Everything the client reports, in order.
    public nonisolated let events: AsyncStream<MediaControlEvent>
    /// The microphone line and the transmit keepalive on the current
    /// media connection (Task 55).
    public nonisolated let uplink = MediaUplink()
    private nonisolated let eventSink: AsyncStream<MediaControlEvent>.Continuation

    private let send: Sender
    private var makePeer: PeerFactory
    private let now: MillisecondClock
    private let nowNs: NanosecondClock
    private let playback: AudioPlaybackCore?
    private let mediaConnected: @Sendable () async -> Void
    /// Runs the connect deadline and the restart waits.
    private let timers: any LinkClock
    /// This client's connect deadline: ``connectDeadline`` on a direct
    /// path, ``IceSettings/connectDeadline`` for a peer that came through
    /// the remote access service, whose gathering alone may take 23.5 s.
    private var peerDeadline: Duration

    private var agreedMinor: UInt16 = 0
    private var capabilityVersions: [String: Int64] = [:]
    private var snapshotComplete = false
    /// A media connection was started in this session; once retired it is
    /// started again only by a restart (``restartMedia(because:)`` or the
    /// client's own restart schedule), never by the next reconcile.
    private var startedThisSession = false
    private var startToken = 0

    private var media: Media?
    /// At most one candidate and one old peer draining after acknowledgement.
    private var replacement: Media?
    private var draining: Media?
    private var replacementDeadline: (any LinkTimer)?
    private var drainTimer: (any LinkTimer)?
    private var replacementPoll: (any LinkTimer)?
    private var replacementPollSerial = 0
    private var pendingMove = false
    private var routeGeneration = 0
    /// AppModel assigns increasing owners to logical sessions. An old
    /// suspended operation may finish only against the session that began it.
    private var logicalOwner: UInt64 = 0
    private var logicalSession: StationSession?
    /// Revokes microphone events already delivered to the persistent stream
    /// when their selected peer retires or a replacement takes over (D88).
    private var microphoneEventAuthority = CommandSendPermit()
    /// At most one timer and eight wire probes belong to the selected peer.
    private var clockActivity: MediaClockPlaybackActivity?
    private var clockActivityRevision: UInt64 = 0
    private var clockProbeTimer: (any LinkTimer)?
    private var clockProbeArm: UInt64 = 0
    private var clockContext: UInt64 = 0
    private var nextClockProbeID: UInt32 = 0
    private var nextClockRequestSerial: UInt64 = 0
    private struct PendingClockProbe {
        let id: UInt32
        let t0: Double
        let t0Ns: Int64
        let serial: UInt64
    }
    private var pendingClockProbes: [PendingClockProbe] = []
    private var clockEstimator = AudioClockEstimator()
    private var clockCapture: AudioCaptureAnchor?
    private var lastClockEchoNs: Int64?
    private var clockAudioContextPlaying = false
    private var replacingRouteGeneration = 0
    private var onAirRefusals = 0
    private var safeToReplace: (@Sendable () async -> Bool)?
    /// The Core duplicates timestamps across peers for each semantic stream.
    private var dualAudio: DualReceiveAudio?
    private var dualAudioOldId: String?
    private var dualAudioNewId: String?
    private var dualAudioTimer: (any LinkTimer)?
    private var dualAudioTimerSerial = 0
    private var replacementMergeFinished = false
    /// A NEW display can arrive before its control acknowledgement. Keep
    /// only a small bounded prefix, including its first keyframe.
    private var earlyReplacementDisplay: [Data] = []
    private var earlyReplacementDisplayBytes = 0
    private static let earlyDisplayMaximumBytes = 2 * 1024 * 1024
    private static let earlyDisplayMaximumCount = 16
    private var endpoints: [UInt32: Endpoint] = [:]
    /// The last revision sent for each endpoint ID on this connection.
    private var lastRevisions: [UInt32: UInt32] = [:]
    private var audioWanted = false
    /// What this phone asks of the Core's main audio besides on or off.
    private var audioRequest = AudioRequest()
    /// The lossless link trial failed on this session, so Opus is asked for
    /// while lossless stays chosen (the desktop's `losslessFallback`).
    public private(set) var losslessFallback = false
    /// The phone's check that the network carries lossless, while it plays.
    private var linkTrial = LosslessLinkTrial()
    private var linkTrialTimer: (any LinkTimer)?
    private var linkTrialToken = 0
    /// The trial is running (tests).
    var losslessTrialActive: Bool { linkTrial.active }
    var losslessTrialClosedWindows: Int { linkTrial.closedWindows }
    var losslessTrialLastWindowLoss: Double? { linkTrial.lastWindowLoss }
    private var expectedAudioSilenceDuringTransmit = false
    private var receivedPlayingAudio = false
    private var lastAudioAtMs: UInt64 = 0
    private var audioStallTimer: (any LinkTimer)?
    private var audioStallToken = 0
    /// Rises with every `audio` sent, across connections; never 0.
    private var audioRevision: UInt32 = 0
    private var audioGeneration: UInt32?
    /// What the Core's last enabled audio-context on this connection runs:
    /// lossless, or Opus at its bitrate (nil without detail).
    private var runningAudio: (lossless: Bool, opusBitrate: Int?)?
    /// The Core has answered the last `audio` sent on this connection.
    private var audioAnswered = true
    /// A bitrate came while the Core had not answered an `audio` sent
    /// without one; it goes after the answer unless the Core runs it.
    private var bitrateAwaitsAnswer = false
    /// Where the app wants the transmit monitor; sent on a connection whose
    /// start declared `txMonitorAudioVersion`, again on each new one.
    private var monitorWanted: MonitorRoute = .none
    /// Rises with every `monitor-audio` sent, across connections; never 0.
    private var monitorRevision: UInt32 = 0
    /// How many of each endpoint's `clarity-retune` requests may still be
    /// refused. The Core answers only a refusal, a `rejected` naming the
    /// endpoint with revision 0, which no subscription carries.
    private var pendingRetunes: [UInt32: Int] = [:]
    /// NSDX datagrams refused on this connection, by reason.
    private var extrasRefusals: [DisplayExtrasDecoder.Reason: Int] = [:]
    /// The current connection's connect deadline, until it connects.
    private var deadline: (any LinkTimer)?
    /// A restart waiting out its delay.
    private var restartTimer: (any LinkTimer)?
    private var restartToken = 0
    /// Media connections in a row that ended without connecting, or ended.
    private var failuresInARow = 0

    // The direct media ladder (``MediaDirectLadder``).
    /// The app's peers and transmit reading for the ladder; nil: no ladder.
    private var directLadder: DirectLadderPeers?
    enum ReplacementKind: Sendable { case move, direct, tunnelFallback }
    /// What the replacement under way is: a control route's move, a
    /// direct-only step, or the fallback onto the tunnel alone.
    private var replacementKind: ReplacementKind = .move
    /// The next direct-only step, an index into ``MediaDirectLadder/directSteps``.
    private var directStep = 0
    private var directTimer: (any LinkTimer)?
    private var directSerial = 0
    private var silenceTimer: (any LinkTimer)?
    private var silenceSerial = 0
    /// The connected media rides the tunnel, or a path that is neither the
    /// tunnel nor a relay; read when media connects or moves.
    private var ladderOnTunnel = false
    private var ladderOnDirectPath = false
    /// When audio or display last arrived since this media start.
    private var ladderLastMediaMs: UInt64?
    /// Audio has arrived since this media start.
    private var ladderHeardAudio = false
    /// The fallback ran for this silence; only a media packet arms it again.
    private var silenceFallbackFired = false
    /// When the fallback finished (media moved, or its replace failed),
    /// until media arrives.
    private var fallbackFinishedMs: UInt64?

    /// One media connection.
    private final class Media {
        let id: String
        let peer: any MediaPeerConnection
        let audioSsrc: UInt32
        /// The microphone line's SSRC, when this connection's start asked
        /// for the line.
        var microphoneSsrc: UInt32?
        /// ``MediaControlEvent/microphoneLine(_:)`` last said true for it.
        var reportedMicrophoneLine = false
        var tasks: [Task<Void, Never>] = []
        var connected = false
        var selectedRouteGeneration = 0
        /// When its `start` went out, on the client's millisecond clock.
        var startedAtMs: UInt64 = 0
        /// This connection's `audio` carried `profile`, which selects the
        /// audio context's profile shape.
        var audioProfileSent = false
        /// This connection's `audio` carried `opusBitrate`, so its contexts
        /// may carry `opusBitrateRefusal`.
        var audioBitrateSent = false
        /// The `txDisplayVersion` this connection's `start` declared, or nil
        /// when it declared none: its subscribes may carry the transmit
        /// window (and at 3 `duplex`), and its contexts carry `transmit`.
        var txDisplayDeclared: Int?
        /// This connection's `start` declared `txMonitorAudioVersion`, so
        /// it may carry `monitor-audio`.
        var txMonitorDeclared = false

        init(id: String, peer: any MediaPeerConnection) {
            self.id = id
            self.peer = peer
            audioSsrc = MediaControlClient.audioSsrc(forConnection: id)
        }
    }

    /// One display endpoint held on the current connection.
    private struct Endpoint {
        var subscription: DisplaySubscription
        /// The endpoint's first revision on this connection: every revision
        /// from it to `subscription`'s is one this app sent.
        var firstRevision: UInt32
        /// The subscribe carried `extendedView`, so its contexts carry `wideband`.
        var widebandNegotiated: Bool
        var context: MediaControlEvent.DisplayContext?
        var decoder: DisplayFrameDecoder?
        var keyframeTimes: [UInt64] = []
        /// The last extras accepted for the current context; a frame that
        /// comes without its datagram leaves them in place.
        var extras: DisplayExtras?
    }

    /// `onMediaConnected` runs once for each media connection that comes
    /// up; ``attached(to:peerFactory:playback:)`` points it at the session's redial
    /// schedule. `timers` runs the connect deadline and the restart waits.
    public init(send: @escaping Sender,
                peerFactory: @escaping PeerFactory = { MediaPeer() },
                playback: AudioPlaybackCore? = nil,
                clock: @escaping MillisecondClock = { DispatchTime.now().uptimeNanoseconds / 1_000_000 },
                nanosecondClock: @escaping NanosecondClock = { Int64(clamping: DispatchTime.now().uptimeNanoseconds) },
                timers: any LinkClock = SystemLinkClock(),
                connectDeadline: Duration = MediaControlClient.connectDeadline,
                onMediaConnected: @escaping @Sendable () async -> Void = {},
                clockProbeIDSeed: UInt32 = 0) {
        self.send = send
        makePeer = peerFactory
        self.playback = playback
        now = clock
        nowNs = nanosecondClock
        nextClockProbeID = clockProbeIDSeed
        self.timers = timers
        peerDeadline = connectDeadline
        mediaConnected = onMediaConnected
        (events, eventSink) = AsyncStream.makeStream(of: MediaControlEvent.self)
    }

    /// A client that sends through `session`, whose redial schedule then
    /// waits for the media connection (``StationSession/setWaitsForMedia(_:)``).
    /// Call before the session connects, and feed it the session's events.
    public static func attached(to session: StationSession,
                                peerFactory: @escaping PeerFactory = { MediaPeer() },
                                playback: AudioPlaybackCore? = nil) async -> MediaControlClient {
        await session.setWaitsForMedia(true)
        return MediaControlClient(send: { message in try await session.send(message) },
                                  peerFactory: peerFactory, playback: playback,
                                  onMediaConnected: { await session.mediaConnectionUp() })
    }

    deinit {
        eventSink.finish()
    }

    /// The peers this client makes from its next media connection on, and
    /// their connect deadline: for a session reached directly, the host
    /// candidates and ``connectDeadline``; for one that came through the
    /// remote access service, a peer with that connection's ICE settings
    /// and ``IceSettings/connectDeadline``, since its gathering alone may
    /// take 23.5 s. Set before the session connects; a connection already
    /// made keeps its own peer.
    public func usePeers(_ factory: @escaping PeerFactory, connectDeadline: Duration) {
        makePeer = factory
        peerDeadline = connectDeadline
    }

    /// Bind media to the exact logical session before it receives a snapshot.
    /// Increasing owners also invalidate any preparation or replacement that
    /// was suspended inside the previous session's actor call.
    @discardableResult
    public func activateLogicalSession(_ session: StationSession, owner: UInt64) -> Bool {
        guard owner > 0 else { return false }
        if owner == logicalOwner { return logicalSession === session }
        guard owner > logicalOwner else { return false }
        startToken &+= 1
        routeGeneration &+= 1
        cancelRestart()
        retire()
        clockActivityRevision = 0
        pendingMove = false
        safeToReplace = nil
        replacementPoll?.cancel()
        replacementPoll = nil
        agreedMinor = 0
        capabilityVersions = [:]
        snapshotComplete = false
        startedThisSession = false
        expectedAudioSilenceDuringTransmit = false
        resetAudioStall()
        directLadder = nil
        resetLadder()
        logicalOwner = owner
        logicalSession = session
        return true
    }

    /// End exactly one app-owned logical session. Keep its owner as the
    /// high-water mark so a delayed event or retry cannot revive it, while
    /// releasing its route factory and safety closure promptly. Peer claims
    /// still retire through each MediaPeer's native close barrier.
    @discardableResult
    public func retireLogicalSession(owner: UInt64) -> Bool {
        guard owner == logicalOwner, logicalSession != nil else { return false }
        logicalSession = nil
        startToken &+= 1
        routeGeneration &+= 1
        cancelRestart()
        retire()
        clockActivityRevision = 0
        pendingMove = false
        safeToReplace = nil
        replacementPoll?.cancel()
        replacementPoll = nil
        makePeer = { MediaPeer() }
        peerDeadline = Self.connectDeadline
        agreedMinor = 0
        capabilityVersions = [:]
        snapshotComplete = false
        startedThisSession = false
        failuresInARow = 0
        onAirRefusals = 0
        expectedAudioSilenceDuringTransmit = false
        resetAudioStall()
        directLadder = nil
        resetLadder()
        return true
    }

    /// Set the next peer factory only if it still belongs to `owner`.
    @discardableResult
    public func usePeers(_ factory: @escaping PeerFactory, connectDeadline: Duration,
                         owner: UInt64) -> Bool {
        guard owner == logicalOwner, logicalSession != nil else { return false }
        usePeers(factory, connectDeadline: connectDeadline)
        return true
    }

    /// The direct media ladder's peers and transmit reading
    /// (``MediaDirectLadder``); nil turns the ladder off. It runs only with
    /// a Core that offers it (``MediaFeatureGates/mediaDirect``).
    public func useDirectLadder(_ peers: DirectLadderPeers?) {
        directLadder = peers
        if peers == nil { resetLadder() }
        ladderMediaChanged()
    }

    /// Set the ladder only if it still belongs to `owner`.
    @discardableResult
    public func useDirectLadder(_ peers: DirectLadderPeers?, owner: UInt64) -> Bool {
        guard owner == logicalOwner, logicalSession != nil else { return false }
        useDirectLadder(peers)
        return true
    }

    /// The next direct-only step's index (tests).
    var directLadderStep: Int { directStep }
    /// When the ladder last saw audio or display (tests).
    var ladderMediaStamp: UInt64? { ladderLastMediaMs }
    /// How many media connections this client has finished bringing up:
    /// the session told, a waiting move tried and the ladder armed (tests).
    private(set) var mediaUpHandled = 0

    // MARK: Reading

    /// What the Core and the agreed minor allow.
    public var gates: MediaFeatureGates {
        MediaFeatureGates(agreedMinor: agreedMinor) { capabilityVersions[$0] ?? 0 }
    }

    /// The collector forwards actual playback output state at most once a
    /// second and immediately on pause, stop or output retirement. The
    /// revision is assigned before its asynchronous read and must increase
    /// within an owner, including inactive updates. A delayed old/equal
    /// update cannot restore an earlier output. This input alone does not
    /// wire the playback adapter; caller integration is required.
    @discardableResult
    public func setClockPlaybackActivity(_ activity: MediaClockPlaybackActivity,
                                         owner: UInt64) -> Bool {
        guard owner == logicalOwner, owner == 0 || logicalSession != nil,
              activity.revision > clockActivityRevision,
              let current = media, current.connected, current.id == activity.mediaID else {
            return false
        }
        clockActivityRevision = activity.revision
        guard activity.isPlaying else {
            stopClockProbing()
            return true
        }
        let time = nowNs()
        guard activity.observedNs >= 0, activity.observedNs <= time,
              time - activity.observedNs <= Self.clockActivityFreshNs,
              gates.audioClock, audioWanted, clockAudioContextPlaying,
              activity.generation != 0, audioGeneration == activity.generation else {
            stopClockProbing()
            return false
        }
        if let old = clockActivity,
           old.mediaID != activity.mediaID || old.playbackLifetime != activity.playbackLifetime
            || old.outputEpoch != activity.outputEpoch || old.generation != activity.generation {
            stopClockProbing()
        }
        clockActivity = activity
        armClockProbeIfNeeded()
        return true
    }

    /// Atomic owner/media-tagged clock reading. A missing offset or capture
    /// means exact unavailability to `measureAudioDelay`.
    public func selectedAudioClock(owner: UInt64) -> SelectedAudioClockSnapshot {
        guard owner == logicalOwner, owner == 0 || logicalSession != nil else {
            return SelectedAudioClockSnapshot(owner: owner, mediaID: nil, supported: false,
                                              active: false, playbackLifetime: nil,
                                              outputEpoch: nil, playingGeneration: nil,
                                              lastEchoNs: nil, offset: nil, capture: nil)
        }
        let time = nowNs()
        if clockActivity != nil && !clockActivityIsCurrent(at: time) { stopClockProbing() }
        let current = media?.connected == true ? media : nil
        let activity = clockActivity
        let active = activity != nil && current != nil && gates.audioClock
        let capture = clockCapture.flatMap { anchor in
            anchor.generation == activity?.generation ? anchor : nil
        }
        return SelectedAudioClockSnapshot(owner: owner, mediaID: current?.id,
                                          supported: current != nil && gates.audioClock,
                                          active: active,
                                          playbackLifetime: activity?.playbackLifetime,
                                          outputEpoch: activity?.outputEpoch,
                                          playingGeneration: activity?.generation,
                                          lastEchoNs: active ? lastClockEchoNs : nil,
                                          offset: active ? clockEstimator.offset(atLocalNs: time) : nil,
                                          capture: active ? capture : nil)
    }

    /// Test-facing bound; probe contents remain private to the actor.
    var pendingClockProbeCount: Int { pendingClockProbes.count }

    /// The current media connection's ID, lowercase and hyphenated.
    public var connectionId: String? {
        media?.id
    }
    /// True only when ICE actually selected this peer's direct WSS tunnel.
    public var selectedTunnel: Bool { media?.peer.selectedTunnel ?? false }
    /// A late heartbeat query for another logical session cannot observe this one.
    public func selectedTunnel(owner: UInt64) -> Bool {
        owner == logicalOwner && logicalSession != nil && selectedTunnel
    }

    /// Read the active peer only for its current app-owned logical session.
    /// Actor isolation keeps the ID and generation fixed while the peer's
    /// synchronous getter takes its own guarded bridge snapshot.
    public func selectedMediaRoute(owner: UInt64) -> SelectedMediaRouteSnapshot {
        guard owner == logicalOwner, logicalSession != nil else {
            return SelectedMediaRouteSnapshot(owner: owner, routeGeneration: 0, mediaID: nil,
                                              observation: .unavailable(.retired))
        }
        guard let current = media, current.connected else {
            return SelectedMediaRouteSnapshot(owner: owner, routeGeneration: routeGeneration, mediaID: nil,
                                              observation: .unavailable(.noCurrentMedia))
        }
        let reading = current.peer.selectedRouteObservation
        guard media === current, logicalOwner == owner, logicalSession != nil else {
            return SelectedMediaRouteSnapshot(owner: owner, routeGeneration: 0, mediaID: nil,
                                              observation: .unavailable(.retired))
        }
        return SelectedMediaRouteSnapshot(owner: owner, routeGeneration: current.selectedRouteGeneration,
                                          mediaID: current.id, observation: reading)
    }

    public func selectedMediaTraffic(owner: UInt64) -> SelectedMediaTrafficSnapshot {
        guard owner == logicalOwner, logicalSession != nil else {
            return SelectedMediaTrafficSnapshot(owner: owner, routeGeneration: 0,
                                                mediaID: nil, observation: nil)
        }
        guard let current = media, current.connected else {
            return SelectedMediaTrafficSnapshot(owner: owner, routeGeneration: routeGeneration,
                                                mediaID: nil, observation: nil)
        }
        let reading = current.peer.trafficObservation
        guard media === current, logicalOwner == owner, logicalSession != nil else {
            return SelectedMediaTrafficSnapshot(owner: owner, routeGeneration: 0,
                                                mediaID: nil, observation: nil)
        }
        return SelectedMediaTrafficSnapshot(owner: owner, routeGeneration: current.selectedRouteGeneration,
                                            mediaID: current.id,
                                            observation: reading?.active == true ? reading : nil)
    }

    public func selectedMediaDiagnostics(owner: UInt64) -> SelectedMediaDiagnosticsSnapshot {
        let clock = selectedAudioClock(owner: owner)
        guard owner == logicalOwner, logicalSession != nil else {
            return SelectedMediaDiagnosticsSnapshot(owner: owner, routeGeneration: 0, mediaID: nil,
                                                    route: .unavailable(.retired), traffic: nil,
                                                    audioAdmission: nil, acceptedAudioGeneration: nil,
                                                    clock: clock)
        }
        guard let current = media, current.connected else {
            return SelectedMediaDiagnosticsSnapshot(owner: owner, routeGeneration: routeGeneration,
                                                    mediaID: nil, route: .unavailable(.noCurrentMedia),
                                                    traffic: nil, audioAdmission: nil,
                                                    acceptedAudioGeneration: nil, clock: clock)
        }
        let route = current.peer.selectedRouteObservation
        let traffic = current.peer.trafficObservation
        let admission = current.peer.currentAudioAdmissionObservation
        guard media === current, logicalOwner == owner, logicalSession != nil else {
            return SelectedMediaDiagnosticsSnapshot(owner: owner, routeGeneration: 0, mediaID: nil,
                                                    route: .unavailable(.retired), traffic: nil,
                                                    audioAdmission: nil, acceptedAudioGeneration: nil,
                                                    clock: clock)
        }
        let activeTraffic = traffic?.active == true ? traffic : nil
        let activeAdmission = admission?.active == true && admission?.peerLifetime == activeTraffic?.lifetime
            ? admission : nil
        return SelectedMediaDiagnosticsSnapshot(owner: owner,
                                                routeGeneration: current.selectedRouteGeneration,
                                                mediaID: current.id, route: route,
                                                traffic: activeTraffic, audioAdmission: activeAdmission,
                                                acceptedAudioGeneration: audioGeneration,
                                                clock: clock)
    }

    /// Feed ownership diagnostics for the scripted replacement tests.
    var replacementAudioLeadMs: UInt64? { dualAudio?.leadMs }
    var replacementAudioHeld: Int { dualAudio?.heldCount ?? 0 }

    /// Called after the session's successful control-path barrier. The
    /// factory captures that path's ICE settings. A later ordinary restart
    /// also uses it on Cores that do not offer media replacement.
    public func controlRouteDidMove(peerFactory: @escaping PeerFactory,
                                    connectDeadline: Duration,
                                    safeToReplace: @escaping @Sendable () async -> Bool) async {
        usePeers(peerFactory, connectDeadline: connectDeadline)
        self.safeToReplace = safeToReplace
        routeGeneration &+= 1
        onAirRefusals = 0
        guard gates.mediaReplace, snapshotComplete, media != nil else {
            return
        }
        pendingMove = true
        await attemptPendingReplacement()
    }

    /// A route callback from a retired logical session cannot replace the
    /// current session's peer factory or start a replacement after an await.
    @discardableResult
    public func controlRouteDidMove(peerFactory: @escaping PeerFactory,
                                    connectDeadline: Duration,
                                    safeToReplace: @escaping @Sendable () async -> Bool,
                                    owner: UInt64) async -> Bool {
        guard owner == logicalOwner, logicalSession != nil else { return false }
        await Self.$operationOwner.withValue(owner) {
            await controlRouteDidMove(peerFactory: peerFactory, connectDeadline: connectDeadline,
                                      safeToReplace: safeToReplace)
        }
        return owner == logicalOwner && logicalSession != nil
    }

    /// The display endpoint IDs held on the current connection.
    public var endpointIds: [UInt32] {
        endpoints.keys.sorted()
    }

    /// The last display extras accepted for an endpoint's current context.
    public func displayExtras(endpointId: UInt32) -> DisplayExtras? {
        endpoints[endpointId]?.extras
    }

    /// NSDX datagrams refused on the current connection, by reason.
    public var displayExtrasRefusals: [DisplayExtrasDecoder.Reason: Int] {
        extrasRefusals
    }

    /// The media connection's audio SSRC: SHA-256 over the ASCII prefix
    /// `NereusSDR/media-audio-ssrc/v1:` and the connection ID, the first four
    /// digest bytes big-endian, with 0 mapped to 1.
    public static func audioSsrc(forConnection connectionId: String) -> UInt32 {
        let value = digestSsrc(audioSsrcPrefix + connectionId)
        return value == 0 ? 1 : value
    }

    /// The four receiver streams' SSRCs: receiver `n`'s from the prefix,
    /// the decimal `n`, a colon and the connection ID, stepped past 0, the
    /// main SSRC and every earlier receiver's (modulo 2^32).
    public static func receiverSsrcs(forConnection connectionId: String) -> [UInt32] {
        let main = audioSsrc(forConnection: connectionId)
        var ssrcs: [UInt32] = []
        for receiver in 0..<receiverStreams {
            var ssrc = digestSsrc(receiverSsrcPrefix + String(receiver) + ":" + connectionId)
            while ssrc == 0 || ssrc == main || ssrcs.contains(ssrc) {
                ssrc &+= 1
            }
            ssrcs.append(ssrc)
        }
        return ssrcs
    }

    /// The headphones mix's SSRC, stepped past 0, the main SSRC and the four
    /// receivers', declared or not.
    public static func headphonesSsrc(forConnection connectionId: String) -> UInt32 {
        let main = audioSsrc(forConnection: connectionId)
        let receivers = receiverSsrcs(forConnection: connectionId)
        var ssrc = digestSsrc(headphonesSsrcPrefix + connectionId)
        while ssrc == 0 || ssrc == main || receivers.contains(ssrc) {
            ssrc &+= 1
        }
        return ssrc
    }

    /// The microphone line's SSRC (Task 36): SHA-256 over
    /// `NereusSDR/media-mic-ssrc/v1:` and the connection ID, the first four
    /// digest bytes big-endian, stepped past 0, the main SSRC, the four
    /// receivers' and the headphones mix's, declared or not.
    public static func microphoneSsrc(forConnection connectionId: String) -> UInt32 {
        let main = audioSsrc(forConnection: connectionId)
        let receivers = receiverSsrcs(forConnection: connectionId)
        let headphones = headphonesSsrc(forConnection: connectionId)
        var ssrc = digestSsrc(microphoneSsrcPrefix + connectionId)
        while ssrc == 0 || ssrc == main || receivers.contains(ssrc) || ssrc == headphones {
            ssrc &+= 1
        }
        return ssrc
    }

    private static func digestSsrc(_ identity: String) -> UInt32 {
        let digest = Array(SHA256.hash(data: Data(identity.utf8)))
        return UInt32(digest[0]) << 24 | UInt32(digest[1]) << 16 | UInt32(digest[2]) << 8 | UInt32(digest[3])
    }

    // MARK: The session

    /// One session event.
    public func handle(_ event: StationSession.Event) async {
        switch event {
        case .stateChanged(let state):
            if state != .ready {
                startToken &+= 1
                snapshotComplete = false
                startedThisSession = false
                cancelRestart()
                failuresInARow = 0
                retire()
            }
        case .refused:
            break
        case .message(let message):
            switch message {
            case .hello(let hello):
                agreedMinor = min(LinkVersionPolicy.minor, hello.minor)
                // A fall back and its trial belong to one session; the
                // choice outlives it.
                setLosslessFallback(false)
                endLinkTrial()
            case .capabilities(let set):
                var versions: [String: Int64] = [:]
                for entry in set.properties {
                    switch entry.value {
                    case .i64(let value):
                        versions[entry.name] = value
                    default:
                        break
                    }
                }
                capabilityVersions = versions
                if !gates.audioClock { stopClockProbing() }
                await reconcile()
                ladderMediaChanged()
            case .snapshotComplete:
                snapshotComplete = true
                await reconcile()
            case .mediaControl(let control):
                await receive(control.payload)
            default:
                break
            }
        }
    }

    /// Feed an app-owned session event with its captured logical owner.
    /// Events that arrive after another session activates are ignored.
    public func handle(_ event: StationSession.Event, owner: UInt64) async {
        guard owner == logicalOwner, logicalSession != nil else { return }
        await Self.$operationOwner.withValue(owner) {
            await handle(event)
        }
    }

    /// Starts a media connection when the Core offers media and none was
    /// started this session; retires the connection when media is withdrawn.
    private func reconcile() async {
        if !gates.media {
            cancelRestart()
            failuresInARow = 0
            retire()
        } else if snapshotComplete && !startedThisSession && media == nil {
            await start()
        }
    }

    /// Starts a new media connection at once, when the session is ready and
    /// the Core offers media: the phone's network changed, say. The
    /// restart waits start over. The client restarts on its own a
    /// connection that does not connect in time, fails, closes or is lost,
    /// or that the Core dropped, on the ``restartDelays`` schedule.
    public func restartMedia(because reason: RestartReason = .asked) async {
        guard snapshotComplete, gates.media else {
            return
        }
        failuresInARow = 0
        await performRestart(reason)
    }

    private func performRestart(_ reason: RestartReason) async {
        cancelRestart()
        Self.logger.notice("Media restarted: \(reason.rawValue, privacy: .public)")
        retire()
        await start()
    }

    /// The media connection ended without the session: retire it and start
    /// another after the next restart delay.
    private func mediaEnded(_ reason: RestartReason, reporting: Bool) async {
        retire(reporting: reporting)
        guard snapshotComplete, gates.media else {
            return
        }
        failuresInARow += 1
        let delay = Self.restartDelay(afterFailures: failuresInARow)
        cancelRestart()
        if delay == .zero {
            if Task.isCancelled {
                // A peer's own event-drain task is canceled by retire().
                // Its canceled context cannot prepare the next peer.
                let token = restartToken
                Task { [weak self] in await self?.restartDue(token, reason: reason) }
            } else {
                await performRestart(reason)
            }
            return
        }
        Self.logger.notice("Media restarts in \(String(describing: delay), privacy: .public): \(reason.rawValue, privacy: .public)")
        let token = restartToken
        restartTimer = timers.schedule(after: delay) { [weak self] in
            await self?.restartDue(token, reason: reason)
        }
    }

    private func restartDue(_ token: Int, reason: RestartReason) async {
        guard token == restartToken, snapshotComplete, gates.media, media == nil else {
            return
        }
        restartTimer = nil
        await performRestart(reason)
    }

    private func cancelRestart() {
        restartToken += 1
        restartTimer?.cancel()
        restartTimer = nil
    }

    private func deadlinePassed(connection id: String) async {
        guard let current = media, current.id == id, !current.connected else {
            return
        }
        Self.logger.warning("The media connection did not connect within \(String(describing: self.peerDeadline), privacy: .public)")
        await mediaEnded(.notConnectedInTime, reporting: true)
    }

    // MARK: The media connection

    private func start() async {
        // A new media start begins the direct schedule again.
        resetLadder()
        startedThisSession = true
        startToken &+= 1
        let token = startToken
        // Swift's uuidString is uppercase; the Core drops a non-canonical ID.
        let id = UUID().uuidString.lowercased()
        let peer = makePeer()
        let started = Media(id: id, peer: peer)
        do {
            try await peer.prepare(connectionId: UUID(uuidString: id)!, gates: gates)
        } catch {
            peer.close()
            guard token == startToken else { return }
            startedThisSession = false
            Self.logger.warning("The media carrier could not be prepared: \(String(describing: error), privacy: .public)")
            await mediaEnded(.notConnectedInTime, reporting: false)
            return
        }
        guard token == startToken, snapshotComplete, !Task.isCancelled, media == nil else {
            peer.close()
            return
        }
        started.startedAtMs = now()
        started.selectedRouteGeneration = routeGeneration
        media = started
        deadline?.cancel()
        deadline = timers.schedule(after: peerDeadline) { [weak self] in
            await self?.deadlinePassed(connection: id)
        }
        started.tasks = [
            Task { [weak self] in
                for await sdp in peer.localDescription {
                    await self?.localDescription(sdp, connection: id)
                }
            },
            Task { [weak self] in
                for await local in peer.localCandidates {
                    await self?.localCandidate(local.candidate, mid: local.mid, connection: id)
                }
            },
            Task { [weak self] in
                for await state in peer.state {
                    await self?.peerState(state, connection: id)
                }
            },
            Task { [weak self] in
                for await packet in peer.audioPackets {
                    await self?.audioPacket(packet, connection: id)
                }
            },
            Task { [weak self] in
                for await datagram in peer.displayDatagrams {
                    await self?.display(datagram, connection: id)
                }
            },
            Task { [weak self] in
                for await _ in peer.microphoneLineClosed {
                    await self?.microphoneLineClosed(connection: id)
                }
            },
        ]
        var payload: [String: LinkJSON] = ["op": .string("start"), "connectionId": .string(id)]
        // A GUI whose Core advertised audioProfileVersion adds it (the media
        // control document's start row); the app asks only for Opus.
        if gates.audioProfile {
            payload["audioProfileVersion"] = .number(1)
        }
        // A client the Core told remoteTxVersion adds it, and the offer
        // then carries the microphone line and the "tx" channel.
        if gates.remoteTx {
            let microphone = Self.microphoneSsrc(forConnection: id)
            started.microphoneSsrc = microphone
            peer.enableMicrophoneLine(ssrc: microphone)
            payload["remoteTxVersion"] = .number(1)
        }
        // A client the Core told txDisplayVersion adds it: 3 to a Core
        // that sends 3, so its subscribes may carry `duplex`, else 1. Its
        // contexts then carry `transmit`, and while keyed the transmitting
        // pan gets the Core's transmit display.
        if let declared = gates.declaredTxDisplayVersion {
            started.txDisplayDeclared = declared
            payload["txDisplayVersion"] = .number(Double(declared))
        }
        // A client the Core told txMonitorAudioVersion adds it, and may
        // then ask where the transmit monitor goes (`monitor-audio`).
        if gates.txMonitorAudio {
            started.txMonitorDeclared = true
            payload["txMonitorAudioVersion"] = .number(1)
        }
        if gates.mediaTunnel { payload["mediaTunnelVersion"] = .number(1) }
        if gates.mediaRelayRouting { payload["mediaRelayRoutingVersion"] = .number(1) }
        await transmit(payload)
        Self.logger.notice("Media start sent")
    }

    /// Ends the current media connection, its subscriptions and its audio,
    /// and reports it closed unless its own state already said so.
    private func retire(reporting: Bool = true) {
        retireMicrophoneEvents()
        stopClockProbing()
        resetLadder()
        guard let retiring = media else {
            return
        }
        media = nil
        routeGeneration &+= 1
        pendingMove = false
        safeToReplace = nil
        replacementPoll?.cancel()
        replacementPoll = nil
        replacementDeadline?.cancel()
        replacementDeadline = nil
        drainTimer?.cancel()
        drainTimer = nil
        dualAudioTimer?.cancel()
        dualAudioTimer = nil
        dualAudio = nil
        dualAudioOldId = nil
        dualAudioNewId = nil
        replacementMergeFinished = false
        playback?.endReplacement()
        earlyReplacementDisplay.removeAll()
        earlyReplacementDisplayBytes = 0
        if let replacement { closePeer(replacement) }
        if let draining { closePeer(draining) }
        replacement = nil
        draining = nil
        uplink.detach()
        if retiring.reportedMicrophoneLine {
            reportMicrophone(.line(false))
        }
        deadline?.cancel()
        deadline = nil
        closePeer(retiring)
        endpoints.removeAll()
        lastRevisions.removeAll()
        pendingRetunes.removeAll()
        extrasRefusals.removeAll()
        audioGeneration = nil
        runningAudio = nil
        audioAnswered = true
        bitrateAwaitsAnswer = false
        clockAudioContextPlaying = false
        resetAudioStall()
        endLinkTrial()
        playback?.reanchor(nil)
        if reporting {
            eventSink.yield(.mediaState(.closed))
        }
    }

    private func isCurrent(_ id: String) -> Bool {
        media?.id == id || replacement?.id == id
    }

    private func closePeer(_ peer: Media) {
        peer.tasks.forEach { $0.cancel() }
        peer.peer.close()
    }

    private func localDescription(_ sdp: String, connection id: String) async {
        guard isCurrent(id) else {
            return
        }
        await transmit(["op": .string("description"), "connectionId": .string(id),
                        "sdp": .string(sdp), "type": .string("answer")])
    }

    private func localCandidate(_ candidate: String, mid: String, connection id: String) async {
        guard isCurrent(id) else {
            return
        }
        await transmit(["op": .string("candidate"), "connectionId": .string(id),
                        "candidate": .string(candidate), "mid": .string(mid)])
    }

    /// The Core closed the microphone line's track on `id` (M6): when that
    /// is the current connection and the line was last said to be carried,
    /// say it is gone, so the phone stops counting on a microphone the Core
    /// no longer hears, then that the track closed while the connection
    /// stays up, so a key held on this phone is released. A replacement's
    /// line is judged when it takes over. The connection ending says only
    /// that the line is gone (``retire``).
    private func retireMicrophoneEvents() {
        microphoneEventAuthority.revoke()
        microphoneEventAuthority = CommandSendPermit()
    }

    private func reportMicrophone(_ change: MediaControlEvent.MicrophoneLifecycle.Change) {
        if logicalOwner == 0 {
            switch change {
            case .line(let carried): eventSink.yield(.microphoneLine(carried))
            case .trackClosed: eventSink.yield(.microphoneTrackClosed)
            }
        } else {
            eventSink.yield(.microphoneLifecycle(.init(owner: logicalOwner,
                                                       authority: microphoneEventAuthority, change: change)))
        }
    }

    private func microphoneLineClosed(connection id: String) {
        guard let current = media, current.id == id, current.reportedMicrophoneLine else {
            return
        }
        current.reportedMicrophoneLine = false
        reportMicrophone(.line(false))
        reportMicrophone(.trackClosed)
    }

    private func peerState(_ state: MediaPeer.State, connection id: String) async {
        if let candidate = replacement, candidate.id == id {
            switch state {
            case .connected where !candidate.connected:
                candidate.connected = true
                replacementDeadline?.cancel()
                replacementDeadline = nil
            case .failed, .closed, .disconnected:
                await abortReplacement()
            default:
                break
            }
            return
        }
        guard let current = media, current.id == id else {
            return
        }
        eventSink.yield(.mediaState(state))
        switch state {
        case .connected where !current.connected:
            // Counted on every way out, so a test knows the client is done.
            defer { mediaUpHandled &+= 1 }
            let generation = routeGeneration
            current.connected = true
            deadline?.cancel()
            deadline = nil
            failuresInARow = 0
            let elapsed = now() &- current.startedAtMs
            Self.logger.notice("Media connected \(elapsed, privacy: .public) ms after its start")
            if let microphone = current.microphoneSsrc {
                uplink.attach(current.peer, microphoneSsrc: microphone)
                if current.peer.microphoneLineReady {
                    current.reportedMicrophoneLine = true
                    reportMicrophone(.line(true))
                }
            }
            // Audio goes before the first suspension: a setAudioEnabled(_:)
            // that runs while this waits then sends on its own, not twice.
            if audioWanted {
                await sendAudio()
            }
            // The Core forgets the monitor's route with each connection.
            if current.txMonitorDeclared, monitorWanted != .none, media?.id == id {
                await sendMonitor()
            }
            guard routeGeneration == generation, media?.id == id else { return }
            if let logicalSession {
                await logicalSession.mediaConnectionUp()
            } else {
                await mediaConnected()
            }
            guard routeGeneration == generation, media?.id == id else { return }
            await attemptPendingReplacement()
            ladderMediaChanged()
        case .failed, .closed:
            Self.logger.notice("The media connection to the Core ended: \(String(describing: state), privacy: .public)")
            // Its own state already said failed or closed.
            await mediaEnded(state == .failed ? .failed : .closed, reporting: false)
        case .disconnected:
            Self.logger.notice("The media connection to the Core was lost")
            await mediaEnded(.disconnected, reporting: true)
        default:
            break
        }
    }

    // MARK: In-session media replacement

    /// A move may arrive before OLD is ready or while NEW from an earlier
    /// move is in flight. Keep only the newest route factory; finish the
    /// accepted replacement before attempting another.
    private func attemptPendingReplacement() async {
        if let owner = Self.operationOwner, owner != logicalOwner { return }
        guard pendingMove, gates.mediaReplace, snapshotComplete,
              replacement == nil, draining == nil,
              dualAudio?.active != true, let current = media else { return }
        guard current.connected, let safeToReplace else {
            scheduleReplacementPoll()
            return
        }
        let generation = routeGeneration
        let oldId = current.id
        let safe = await safeToReplace()
        guard generation == routeGeneration, pendingMove, snapshotComplete,
              media?.id == oldId, replacement == nil, draining == nil,
              dualAudio?.active != true else { return }
        guard safe else {
            scheduleReplacementPoll()
            return
        }
        replacementPoll?.cancel()
        replacementPoll = nil
        pendingMove = false
        replacingRouteGeneration = generation
        let id = UUID().uuidString.lowercased()
        let peer = makePeer()
        do {
            try await peer.prepare(connectionId: UUID(uuidString: id)!, gates: gates)
        } catch {
            peer.close()
            guard routeGeneration == generation, media?.id == oldId else { return }
            if !gates.mediaRelayRouting,
               let refusal = error as? RelayICEError,
               refusal == .alreadyClaimed || refusal == .mediaModeConflict {
                // Legacy raw relay media cannot overlap the current socket.
                // Leave OLD intact and wait for a later ordinary restart.
                pendingMove = false
            } else {
                pendingMove = true
                scheduleReplacementPoll()
            }
            return
        }
        guard routeGeneration == generation, snapshotComplete, media?.id == oldId,
              replacement == nil, !Task.isCancelled else {
            peer.close()
            return
        }
        let stillSafe = await safeToReplace()
        guard routeGeneration == generation, media?.id == oldId,
              !Task.isCancelled else {
            peer.close()
            return
        }
        guard stillSafe else {
            peer.close()
            pendingMove = true
            scheduleReplacementPoll()
            return
        }
        replacementKind = .move
        await installReplacement(peer, id: id, current: current, deadline: peerDeadline, direct: false)
    }

    /// Makes `peer` the replacement for `current` and sends its `replace`.
    private func installReplacement(_ peer: any MediaPeerConnection, id: String, current: Media,
                                    deadline: Duration, direct: Bool) async {
        let oldId = current.id
        let candidate = Media(id: id, peer: peer)
        candidate.startedAtMs = now()
        candidate.audioProfileSent = current.audioProfileSent
        candidate.audioBitrateSent = current.audioBitrateSent
        candidate.txDisplayDeclared = current.txDisplayDeclared
        candidate.txMonitorDeclared = current.txMonitorDeclared
        if current.microphoneSsrc != nil {
            candidate.microphoneSsrc = Self.microphoneSsrc(forConnection: id)
            peer.enableMicrophoneLine(ssrc: candidate.microphoneSsrc!)
        }
        replacement = candidate
        earlyReplacementDisplay.removeAll()
        earlyReplacementDisplayBytes = 0
        dualAudioTimer?.cancel()
        dualAudioTimer = nil
        playback?.beginReplacement()
        peer.setExpectedAudioSsrc(nil)
        current.peer.setExpectedAudioSsrc(nil)
        playback?.acceptEquivalentSsrc(candidate.audioSsrc)
        dualAudio = DualReceiveAudio(audioFormat: playback?.anchor?.format ?? .opus)
        dualAudioOldId = oldId
        dualAudioNewId = id
        replacementMergeFinished = false
        candidate.tasks = [
            Task { [weak self] in
                for await sdp in peer.localDescription { await self?.localDescription(sdp, connection: id) }
            },
            Task { [weak self] in
                for await local in peer.localCandidates {
                    await self?.localCandidate(local.candidate, mid: local.mid, connection: id)
                }
            },
            Task { [weak self] in
                for await state in peer.state { await self?.peerState(state, connection: id) }
            },
            Task { [weak self] in
                for await packet in peer.audioPackets { await self?.audioPacket(packet, connection: id) }
            },
            Task { [weak self] in
                for await datagram in peer.displayDatagrams { await self?.display(datagram, connection: id) }
            },
            Task { [weak self] in
                for await _ in peer.microphoneLineClosed { await self?.microphoneLineClosed(connection: id) }
            },
        ]
        replacementDeadline = timers.schedule(after: deadline) { [weak self] in
            await self?.replacementTimedOut(id: id)
        }
        // The wire has exactly these three fields. Start's version keys are
        // inherited by the Core from the current peer. A direct-only
        // replace, to a Core that offers it, adds `mediaDirectVersion` 1.
        var payload: [String: LinkJSON] = ["op": .string("replace"), "connectionId": .string(id),
                                           "replaces": .string(oldId)]
        if direct { payload["mediaDirectVersion"] = .number(1) }
        await transmit(payload)
    }

    private func scheduleReplacementPoll() {
        guard replacementPoll == nil, pendingMove else { return }
        replacementPollSerial &+= 1
        let serial = replacementPollSerial
        replacementPoll = timers.schedule(after: .milliseconds(500)) { [weak self] in
            await self?.replacementPollFired(serial: serial)
        }
    }

    private func replacementPollFired(serial: Int) async {
        guard serial == replacementPollSerial, replacementPoll != nil else { return }
        replacementPoll = nil
        await attemptPendingReplacement()
    }

    private func replacementTimedOut(id: String) async {
        guard let replacement, replacement.id == id, !replacement.connected else { return }
        await abortReplacement()
    }

    private func abortReplacement(onAirRefusal: Bool = false) async {
        guard let failed = replacement else { return }
        let kind = replacementKind
        replacementKind = .move
        replacement = nil
        replacementDeadline?.cancel()
        replacementDeadline = nil
        closePeer(failed)
        dualAudioTimer?.cancel()
        dualAudioTimer = nil
        dualAudio = nil
        dualAudioOldId = nil
        dualAudioNewId = nil
        replacementMergeFinished = false
        playback?.endReplacement()
        earlyReplacementDisplay.removeAll()
        earlyReplacementDisplayBytes = 0
        if let current = media {
            current.peer.setExpectedAudioSsrc(current.audioSsrc)
            playback?.keepOnlySsrc(current.audioSsrc)
        }
        // The direct media ladder: a direct-only replace that fails waits
        // for the next step and never marks a move pending; a fallback that
        // fails has finished, and media gets one window to arrive.
        if kind == .tunnelFallback { fallbackFinishedMs = now() }
        ladderMediaChanged()
        if kind == .move && onAirRefusal && replacingRouteGeneration == routeGeneration && onAirRefusals < 3 {
            onAirRefusals += 1
            pendingMove = true
            scheduleReplacementPoll()
        } else if pendingMove {
            await attemptPendingReplacement()
        }
    }

    /// Core acknowledgement is authoritative: its connected callback may
    /// still be queued on another stream, and a new key press may have
    /// occurred since the Core decided to switch.
    private func acceptReplacement(_ payload: [String: LinkJSON]) async {
        guard Set(payload.keys) == ["op", "connectionId", "replaces"],
              let candidate = replacement, let current = media,
              MediaControlDecoder.string(payload["connectionId"]) == candidate.id,
              MediaControlDecoder.string(payload["replaces"]) == current.id else { return }
        candidate.connected = true
        replacementDeadline?.cancel()
        replacementDeadline = nil
        replacement = nil
        let expectedPlayingAudio = receivedPlayingAudio && audioWanted && audioGeneration != nil
        candidate.selectedRouteGeneration = routeGeneration
        retireMicrophoneEvents()
        media = candidate
        // The direct media ladder: the connection in use is now the new one,
        // and its silence is counted from here; a finished fallback waits
        // one window for media.
        let kind = replacementKind
        replacementKind = .move
        ladderLastMediaMs = now()
        if kind == .tunnelFallback { fallbackFinishedMs = ladderLastMediaMs }
        stopClockProbing()
        resetAudioStall()
        // OLD proved that this session was delivering the requested audio.
        // NEW may connect but never deliver its first packet, so its own
        // fresh RX deadline starts at the acknowledged replacement.
        if expectedPlayingAudio {
            receivedPlayingAudio = true
            lastAudioAtMs = now()
            armAudioStallIfNeeded()
        }
        if let microphone = candidate.microphoneSsrc {
            uplink.switchPeer(candidate.peer, microphoneSsrc: microphone)
        } else {
            uplink.detach()
        }
        let lineReady = candidate.microphoneSsrc != nil && candidate.peer.microphoneLineReady
        // Republish even an unchanged app-owned line: the old peer's
        // buffered availability was revoked when this peer took over.
        if logicalOwner > 0 || current.reportedMicrophoneLine != lineReady {
            reportMicrophone(.line(lineReady))
        }
        candidate.reportedMicrophoneLine = lineReady
        current.reportedMicrophoneLine = false
        if let draining { closePeer(draining) }
        drainTimer?.cancel()
        draining = current
        let oldId = current.id
        let generation = routeGeneration
        let owner = logicalOwner
        drainTimer = timers.schedule(after: .milliseconds(2_000)) { [weak self] in
            await self?.finishDrain(oldId: oldId, owner: owner)
        }
        ladderMediaChanged()
        // Display context remains valid, but the new peer's first frame is
        // a keyframe and needs a fresh frame decoder.
        for id in Array(endpoints.keys) { endpoints[id]?.decoder = DisplayFrameDecoder() }
        let earlyDisplay = earlyReplacementDisplay
        earlyReplacementDisplay.removeAll()
        earlyReplacementDisplayBytes = 0
        for datagram in earlyDisplay {
            guard routeGeneration == generation, media?.id == candidate.id else { return }
            await display(datagram, connection: candidate.id)
        }
        for id in endpoints.keys.sorted() {
            guard routeGeneration == generation, media?.id == candidate.id else { return }
            await requestKeyframe(endpointId: id)
        }
        guard routeGeneration == generation, media?.id == candidate.id else { return }
        if pendingMove { await attemptPendingReplacement() }
    }

    private func finishDrain(oldId: String, owner: UInt64) async {
        guard logicalOwner == owner, let old = draining, old.id == oldId else { return }
        // A same-session move may have advanced routeGeneration during the
        // two-second drain. Capture it only after admitting this OLD cleanup;
        // a later owner/move must not be consumed after mergeSettled awaits.
        let generation = routeGeneration
        draining = nil
        drainTimer = nil
        closePeer(old)
        ladderMediaChanged()
        if let dualAudio, dualAudioOldId == oldId {
            let steps = dualAudio.easeSteps
            for ready in dualAudio.oldPathDone(nowMs: now()) {
                if let newId = dualAudioNewId,
                   Self.semanticStream(ready.ssrc, connection: newId) == 0 {
                    playback?.receive(ready)
                }
            }
            if dualAudio.easeSteps > steps { playback?.replacementLeadEased() }
            scheduleDualAudioTick()
        }
        if let media { playback?.keepOnlySsrc(media.audioSsrc) }
        await mergeSettled()
        guard routeGeneration == generation, logicalOwner == owner else { return }
        await attemptPendingReplacement()
    }

    /// Semantic indices match the Core's audioSsrcsOf order: band audio,
    /// four receiver streams, then the headphones mix. Each has its own
    /// duplicate history even though this phone currently plays band audio.
    private static func semanticStream(_ ssrc: UInt32, connection id: String) -> Int? {
        if ssrc == audioSsrc(forConnection: id) { return 0 }
        if let index = receiverSsrcs(forConnection: id).firstIndex(of: ssrc) { return index + 1 }
        if ssrc == headphonesSsrc(forConnection: id) { return receiverStreams + 1 }
        return nil
    }

    private func audioPacket(_ packet: RtpPacket, connection id: String) async {
        guard media?.id == id || replacement?.id == id || draining?.id == id,
              let stream = Self.semanticStream(packet.ssrc, connection: id) else { return }
        if media?.id == id || replacement?.id == id { noteLadderMedia(audio: true) }
        if stream == 0, audioWanted, audioGeneration != nil,
           (media?.id == id || replacement?.id == id) {
            receivedPlayingAudio = true
            lastAudioAtMs = now()
            armAudioStallIfNeeded()
        }
        guard let dualAudio, let oldId = dualAudioOldId, let newId = dualAudioNewId,
              id == oldId || id == newId else {
            if stream == 0 && media?.id == id { playback?.receive(packet) }
            return
        }
        let steps = dualAudio.easeSteps
        let readyPackets = dualAudio.submit(packet, stream: stream, fromNew: id == newId, nowMs: now())
        if dualAudio.easeSteps > steps { playback?.replacementLeadEased() }
        for ready in readyPackets {
            if stream == 0 { playback?.receive(ready) }
        }
        scheduleDualAudioTick()
        await mergeSettled()
    }

    private func scheduleDualAudioTick() {
        guard dualAudioTimer == nil, dualAudio?.needsTick == true else { return }
        dualAudioTimerSerial &+= 1
        let serial = dualAudioTimerSerial
        dualAudioTimer = timers.schedule(after: .milliseconds(10)) { [weak self] in
            await self?.tickDualAudio(serial: serial)
        }
    }

    private func tickDualAudio(serial: Int) async {
        guard serial == dualAudioTimerSerial, dualAudioTimer != nil else { return }
        dualAudioTimer = nil
        guard let dualAudio else { return }
        let steps = dualAudio.easeSteps
        let readyPackets = dualAudio.tick(nowMs: now())
        if dualAudio.easeSteps > steps { playback?.replacementLeadEased() }
        for ready in readyPackets {
            if let newId = dualAudioNewId,
               Self.semanticStream(ready.ssrc, connection: newId) == 0 {
                playback?.receive(ready)
            }
        }
        scheduleDualAudioTick()
        await mergeSettled()
    }

    private func mergeSettled() async {
        guard dualAudio?.active == false, !replacementMergeFinished else { return }
        replacementMergeFinished = true
        playback?.endReplacement()
        await attemptPendingReplacement()
    }

    // MARK: The direct media ladder

    /// Everything the ladder counts belongs to one media start.
    private func resetLadder() {
        directTimer?.cancel()
        directTimer = nil
        directSerial &+= 1
        silenceTimer?.cancel()
        silenceTimer = nil
        silenceSerial &+= 1
        directStep = 0
        ladderOnTunnel = false
        ladderOnDirectPath = false
        ladderLastMediaMs = nil
        ladderHeardAudio = false
        silenceFallbackFired = false
        fallbackFinishedMs = nil
        replacementKind = .move
    }

    /// Media connected, moved, or a replacement ended: read the path the
    /// connection in use settled on, then arm the schedule or the silence
    /// check it needs.
    private func ladderMediaChanged() {
        if let current = media, current.connected {
            ladderOnTunnel = current.peer.selectedTunnel
            ladderOnDirectPath = !current.peer.selectedRelayOrTunnel
        } else {
            ladderOnTunnel = false
            ladderOnDirectPath = false
        }
        updateDirectSchedule()
        armSilenceCheck()
    }

    private var ladderRuns: Bool {
        directLadder != nil && gates.mediaDirect && snapshotComplete && media?.connected == true
    }

    /// While media rides the tunnel on a Core that offers the ladder, the
    /// next direct-only replace waits for its step (the last one repeats).
    private func updateDirectSchedule() {
        guard ladderRuns, ladderOnTunnel else {
            directTimer?.cancel()
            directTimer = nil
            directSerial &+= 1
            return
        }
        guard directTimer == nil, replacement == nil else { return }
        let steps = MediaDirectLadder.directSteps
        directSerial &+= 1
        let serial = directSerial
        let owner = logicalOwner
        directTimer = timers.schedule(after: steps[min(directStep, steps.count - 1)]) { [weak self] in
            await self?.directStepDue(serial: serial, owner: owner)
        }
    }

    private func directStepDue(serial: Int, owner: UInt64) async {
        guard serial == directSerial, owner == logicalOwner, directTimer != nil else { return }
        directTimer = nil
        guard ladderRuns, ladderOnTunnel else {
            updateDirectSchedule()
            return
        }
        let generation = routeGeneration
        // A move already waiting, or one under way, goes first.
        if !pendingMove, replacement == nil, draining == nil, dualAudio?.active != true {
            _ = await startLadderReplacement(.direct)
        }
        guard generation == routeGeneration, owner == logicalOwner else { return }
        directStep = min(directStep + 1, MediaDirectLadder.directSteps.count - 1)
        // A replacement under way re-arms the schedule when it ends.
        guard replacement == nil else { return }
        updateDirectSchedule()
    }

    /// A direct-only replace (`.direct`) or the fallback onto the tunnel
    /// alone (`.tunnelFallback`). Neither starts while this phone is keyed
    /// or the Core is on the air, and a direct-only replace also waits while
    /// VOX is armed. Returns whether the replace went out.
    private func startLadderReplacement(_ kind: ReplacementKind) async -> Bool {
        if let owner = Self.operationOwner, owner != logicalOwner { return false }
        guard let ladder = directLadder, gates.mediaReplace, snapshotComplete,
              kind != .direct || (gates.mediaDirect && !pendingMove),
              replacement == nil, draining == nil, dualAudio?.active != true,
              let current = media, current.connected else { return false }
        let generation = routeGeneration
        let owner = logicalOwner
        let oldId = current.id
        func stillCurrent() -> Bool {
            generation == routeGeneration && owner == logicalOwner && snapshotComplete
                && media?.id == oldId && replacement == nil && draining == nil && !Task.isCancelled
        }
        guard await ladder.state().allows(kind), stillCurrent() else { return false }
        let id = UUID().uuidString.lowercased()
        let peer: any MediaPeerConnection
        let deadline: Duration
        switch kind {
        case .direct:
            peer = ladder.direct()
            // The Core drops a new peer not ready within ICE's own deadline.
            deadline = IceSettings.connectDeadline
        case .tunnelFallback, .move:
            peer = (ladder.tunnelAlone ?? makePeer)()
            deadline = peerDeadline
        }
        do {
            try await peer.prepare(connectionId: UUID(uuidString: id)!, gates: gates)
        } catch {
            peer.close()
            return false
        }
        guard stillCurrent(), await ladder.state().allows(kind), stillCurrent() else {
            peer.close()
            return false
        }
        replacingRouteGeneration = generation
        replacementKind = kind
        Self.logger.notice("\(kind == .direct ? "Trying audio and display on a direct connection" : "Moving audio and display back to the tunnel", privacy: .public)")
        await installReplacement(peer, id: id, current: current, deadline: deadline, direct: kind == .direct)
        return true
    }

    /// Audio or display arrived: the silence is over, and the fallback may
    /// run again for the next one.
    private func noteLadderMedia(audio: Bool) {
        ladderLastMediaMs = now()
        if audio { ladderHeardAudio = true }
        silenceFallbackFired = false
        fallbackFinishedMs = nil
        armSilenceCheck()
    }

    /// Arms the silence check when a window could run out: a finished
    /// fallback waiting for media, or a direct path with receive audio
    /// wanted whose audio has been heard.
    private func armSilenceCheck() {
        guard silenceTimer == nil, ladderRuns else { return }
        let window = MediaDirectLadder.silenceFallbackMs
        let time = now()
        if let finished = fallbackFinishedMs {
            scheduleSilenceCheck(after: window - min(window, time &- finished))
        } else if ladderOnDirectPath, !silenceFallbackFired, audioWanted, ladderHeardAudio,
                  !expectedAudioSilenceDuringTransmit, let last = ladderLastMediaMs {
            scheduleSilenceCheck(after: window - min(window, time &- last))
        }
    }

    private func scheduleSilenceCheck(after ms: UInt64) {
        silenceTimer?.cancel()
        silenceSerial &+= 1
        let serial = silenceSerial
        let owner = logicalOwner
        silenceTimer = timers.schedule(after: .milliseconds(Int64(ms))) { [weak self] in
            await self?.silenceCheckDue(serial: serial, owner: owner)
        }
    }

    private func silenceCheckDue(serial: Int, owner: UInt64) async {
        guard serial == silenceSerial, owner == logicalOwner, silenceTimer != nil else { return }
        silenceTimer = nil
        await checkMediaSilence()
    }

    /// On a direct path whose media stopped for
    /// ``MediaDirectLadder/silenceFallbackMs`` while the control session
    /// still runs, the ordinary replace onto the tunnel alone, and the direct
    /// schedule starts over. Once per silence: if media has not returned a
    /// window after that fallback finished, a new media start. Nothing here
    /// runs while the Core is on the air, and nothing starts while this
    /// phone is keyed.
    private func checkMediaSilence() async {
        guard let ladder = directLadder, ladderRuns, replacement == nil, draining == nil,
              let current = media else { return }
        // Silence is expected while audio is not asked for, before any
        // arrived, and while the Core transmits; each end re-arms the check.
        guard audioWanted, ladderHeardAudio, !expectedAudioSilenceDuringTransmit else { return }
        let window = MediaDirectLadder.silenceFallbackMs
        let id = current.id
        if let finished = fallbackFinishedMs {
            let elapsed = now() &- finished
            guard elapsed >= window else {
                scheduleSilenceCheck(after: window - elapsed)
                return
            }
            let state = await ladder.state()
            guard media?.id == id, fallbackFinishedMs == finished, ladderRuns else { return }
            guard !state.keyed, !state.coreOnAir, !expectedAudioSilenceDuringTransmit else {
                scheduleSilenceCheck(after: MediaDirectLadder.transmitPollMs)
                return
            }
            Self.logger.notice("No audio or display from the Core \(elapsed, privacy: .public) ms after moving back to the tunnel; starting audio and display again")
            await restartMedia(because: .directSilence)
            return
        }
        guard ladderOnDirectPath, !silenceFallbackFired, let last = ladderLastMediaMs else { return }
        let elapsed = now() &- last
        guard elapsed >= window else {
            scheduleSilenceCheck(after: window - elapsed)
            return
        }
        let state = await ladder.state()
        guard media?.id == id, ladderLastMediaMs == last, !silenceFallbackFired, fallbackFinishedMs == nil,
              replacement == nil, draining == nil, ladderRuns else { return }
        // A relayed control connection's media keeps the stall rule.
        guard !state.controlRelayed else { return }
        guard !state.keyed, !state.coreOnAir, !expectedAudioSilenceDuringTransmit else {
            scheduleSilenceCheck(after: MediaDirectLadder.transmitPollMs)
            return
        }
        Self.logger.notice("No audio or display from the Core for \(elapsed, privacy: .public) ms on a direct path; moving back to the tunnel")
        ladderLastMediaMs = now()
        silenceFallbackFired = true
        directStep = 0
        directTimer?.cancel()
        directTimer = nil
        directSerial &+= 1
        let started = await startLadderReplacement(.tunnelFallback)
        guard !started, media?.id == id, ladderRuns, replacement == nil else { return }
        // Nothing started, so nothing will finish: wait one window from here.
        fallbackFinishedMs = now()
        armSilenceCheck()
    }

    // MARK: Audio

    /// Asks the Core for audio or stops it. Sent at once on a connected
    /// media connection, otherwise when the next one connects.
    public func setAudioEnabled(_ enabled: Bool) async {
        let wasWanted = audioWanted
        audioWanted = enabled
        if !enabled {
            resetAudioStall()
            stopClockProbing()
            endLinkTrial()
        } else if !wasWanted {
            // The direct media ladder: audio was not asked for, so its
            // silence was expected; the window starts again from here.
            if ladderLastMediaMs != nil { ladderLastMediaMs = now() }
            armSilenceCheck()
        }
        guard let media, media.connected else {
            return
        }
        await sendAudio()
    }

    /// The app sets this only from a fresh authenticated txStateVersion 1+
    /// state reporting keyed, tuning, or txEnding. A false value starts a
    /// fresh three-second RX window after half-duplex transmit silence.
    public func setExpectedAudioSilenceDuringTransmit(_ expected: Bool) {
        guard expected != expectedAudioSilenceDuringTransmit else { return }
        expectedAudioSilenceDuringTransmit = expected
        audioStallTimer?.cancel()
        audioStallTimer = nil
        audioStallToken &+= 1
        if !expected {
            lastAudioAtMs = now()
            armAudioStallIfNeeded()
            // The direct media ladder: the silence while the Core transmitted
            // was expected, so the return to receive restarts the silence
            // window and a finished fallback's wait.
            let time = now()
            if ladderLastMediaMs != nil { ladderLastMediaMs = time }
            if fallbackFinishedMs != nil { fallbackFinishedMs = time }
            armSilenceCheck()
        }
    }

    /// An older session's delayed Mirror update cannot suppress the current
    /// session's expected-audio recovery timer.
    @discardableResult
    public func setExpectedAudioSilenceDuringTransmit(_ expected: Bool, owner: UInt64) -> Bool {
        guard owner == logicalOwner, logicalSession != nil else { return false }
        setExpectedAudioSilenceDuringTransmit(expected)
        return true
    }

    private func resetAudioStall() {
        audioStallTimer?.cancel()
        audioStallTimer = nil
        audioStallToken &+= 1
        receivedPlayingAudio = false
        lastAudioAtMs = 0
    }

    private func armAudioStallIfNeeded() {
        guard audioStallTimer == nil, !expectedAudioSilenceDuringTransmit,
              receivedPlayingAudio, audioWanted, audioGeneration != nil,
              let media, media.connected else { return }
        let token = audioStallToken
        let id = media.id
        audioStallTimer = timers.schedule(after: Self.audioStallDeadline) { [weak self] in
            await self?.audioStallDue(token: token, connectionId: id)
        }
    }

    private func audioStallDue(token: Int, connectionId id: String) async {
        guard token == audioStallToken, let media, media.id == id else { return }
        audioStallTimer = nil
        guard !expectedAudioSilenceDuringTransmit, receivedPlayingAudio,
              audioWanted, audioGeneration != nil, media.connected else { return }
        let elapsed = now() &- lastAudioAtMs
        if elapsed < 3_000 {
            audioStallTimer = timers.schedule(after: .milliseconds(Int64(3_000 - elapsed))) { [weak self] in
                await self?.audioStallDue(token: token, connectionId: id)
            }
        } else if media.peer.selectedRelayOrTunnel {
            await mediaEnded(.audioStalled, reporting: true)
        }
    }

    /// What this phone asks of the Core's main audio (R-IOS-09; the media
    /// control document, "Per-device audio quality"): lossless or Opus,
    /// and the Opus bitrate, one of the catalogue's `audio.opusProfiles`,
    /// or nil for the Core's own.
    public struct AudioRequest: Sendable, Equatable {
        public var lossless: Bool
        public var opusBitrate: Int?

        public init(lossless: Bool = false, opusBitrate: Int? = nil) {
            self.lossless = lossless
            self.opusBitrate = opusBitrate
        }
    }

    /// Sets what this phone asks of the Core's audio. A change goes at once
    /// on a connected media connection while audio is wanted, otherwise with
    /// the next `audio`. `profile` is sent only where the Core offers the
    /// profile, and `opusBitrate` only where it offers audio quality.
    /// `choosing` is the operator choosing now: as on the desktop, choosing
    /// again gives lossless a fresh chance on this link after a fall back.
    public func setAudioRequest(_ request: AudioRequest, choosing: Bool = false) async {
        var wasFallback = false
        if choosing {
            wasFallback = losslessFallback
            setLosslessFallback(false)
            endLinkTrial()
        }
        guard request != audioRequest || wasFallback else {
            return
        }
        let addsBitrate = !wasFallback && Self.onlyAddsBitrate(request, to: audioRequest)
        audioRequest = request
        guard audioWanted, let media, media.connected else {
            return
        }
        if addsBitrate {
            guard audioAnswered else {
                bitrateAwaitsAnswer = true
                return
            }
            if Self.coreRuns(request, running: runningAudio) {
                return
            }
        }
        await sendAudio()
    }

    /// The catalogue can come after audio is up, so the first `audio` went
    /// without a bitrate. Each accepted `audio` starts a new context at
    /// the Core (and a new stream here), so a bitrate that changes nothing
    /// the Core runs waits for the next `audio`: the Opus it already runs at
    /// that bitrate, or lossless still playing, where the bitrate is for
    /// the Opus a fall back would bring (``fallBackToOpus()`` sends it).
    /// Before the Core answers, it waits for the answer.
    private static func onlyAddsBitrate(_ request: AudioRequest, to previous: AudioRequest) -> Bool {
        previous.opusBitrate == nil && request.opusBitrate != nil && request.lossless == previous.lossless
    }

    private static func coreRuns(_ request: AudioRequest, running: (lossless: Bool, opusBitrate: Int?)?) -> Bool {
        guard let running, running.lossless == request.lossless else {
            return false
        }
        return request.lossless || running.opusBitrate == request.opusBitrate
    }

    /// The Core answered: a bitrate that waited goes unless it already runs.
    private func sendBitrateAwaitingAnswer() async {
        guard bitrateAwaitsAnswer, audioAnswered else {
            return
        }
        bitrateAwaitsAnswer = false
        guard audioWanted, let media, media.connected, !Self.coreRuns(audioRequest, running: runningAudio) else {
            return
        }
        await sendAudio()
    }

    // MARK: The lossless link trial

    /// One trial while lossless plays, none otherwise (the desktop's
    /// `reconcileLinkTrial`).
    private func reconcileLinkTrial(losslessPlaying: Bool) {
        if losslessPlaying && !linkTrial.active {
            linkTrial.begin(nowMs: Int64(now()))
            scheduleLinkTrialSample()
        } else if !losslessPlaying && linkTrial.active {
            endLinkTrial()
        }
    }

    private func setLosslessFallback(_ fallenBack: Bool) {
        losslessFallback = fallenBack
        uplink.setLosslessFallback(fallenBack)
    }

    private func endLinkTrial() {
        linkTrial.end()
        linkTrialTimer?.cancel()
        linkTrialTimer = nil
        linkTrialToken &+= 1
    }

    private func scheduleLinkTrialSample() {
        linkTrialTimer?.cancel()
        let token = linkTrialToken
        linkTrialTimer = timers.schedule(after: .milliseconds(LosslessLinkTrial.sampleMs)) { [weak self] in
            await self?.sampleLinkTrial(token: token)
        }
    }

    /// The desktop's `checkLosslessLink`: one sample of the playing stream.
    private func sampleLinkTrial(token: Int) async {
        guard token == linkTrialToken, linkTrial.active else { return }
        linkTrialTimer = nil
        let observation = await playback?.observation()
        guard token == linkTrialToken, linkTrial.active else { return }
        var sample: LosslessLinkTrial.Sample?
        if let observation, observation.stream?.format == .l16 {
            let counters = observation.counters
            sample = LosslessLinkTrial.Sample(
                running: true, generation: observation.streamEpoch,
                expectedPackets: counters.reception.expectedPackets,
                missingPackets: counters.reception.missingPackets,
                decodedPackets: counters.decodedPackets, concealedPackets: counters.concealedIntervals,
                linkInterruptions: counters.linkInterruptionEvents)
        }
        if linkTrial.observe(nowMs: Int64(now()), sample) == .failed {
            await fallBackToOpus()
        } else {
            scheduleLinkTrialSample()
        }
    }

    /// The desktop's `fallBackToOpus`: Opus is asked for at once, with one
    /// notice; lossless stays chosen.
    private func fallBackToOpus() async {
        endLinkTrial()
        setLosslessFallback(true)
        if audioWanted, let media, media.connected {
            await sendAudio()
        }
        eventSink.yield(.losslessFallback)
    }

    private func sendAudio() async {
        guard let media else {
            return
        }
        audioAnswered = false
        bitrateAwaitsAnswer = false
        audioRevision &+= 1
        if audioRevision == 0 {
            audioRevision = 1
        }
        var payload: [String: LinkJSON] = [
            "op": .string("audio"),
            "connectionId": .string(media.id),
            "revision": .number(Double(audioRevision)),
            "enabled": .bool(audioWanted),
        ]
        if gates.audioProfile {
            let profile: MediaControlEvent.AudioContext.Profile =
                audioRequest.lossless && !losslessFallback ? .lossless : .opus
            payload["profile"] = .string(profile.rawValue)
            media.audioProfileSent = true
            // opusBitrate only beside profile, and only to a Core that told
            // this device audioQualityVersion; otherwise the Core cannot
            // read the control at all.
            if gates.audioQuality, let bitrate = audioRequest.opusBitrate {
                payload["opusBitrate"] = .number(Double(bitrate))
                media.audioBitrateSent = true
            }
        }
        await transmit(payload)
    }

    // MARK: Transmit monitor

    /// Asks the Core to put the transmit monitor in `route`. Sent at once on
    /// a connected media connection whose start declared
    /// `txMonitorAudioVersion`; otherwise kept, and sent when the next such
    /// connection connects (a `none` then needs nothing sent).
    public func setMonitorRoute(_ route: MonitorRoute) async {
        monitorWanted = route
        guard let media, media.connected, media.txMonitorDeclared else {
            return
        }
        await sendMonitor()
    }

    private func sendMonitor() async {
        guard let media else {
            return
        }
        monitorRevision &+= 1
        if monitorRevision == 0 {
            monitorRevision = 1
        }
        await transmit([
            "op": .string("monitor-audio"),
            "connectionId": .string(media.id),
            "revision": .number(Double(monitorRevision)),
            "route": .string(monitorWanted.rawValue),
        ])
    }

    private func receiveMonitorAudioContext(_ payload: [String: LinkJSON], on media: Media) {
        guard media.txMonitorDeclared,
              let context = MediaControlDecoder.monitorAudioContext(payload),
              context.revision == monitorRevision else {
            return
        }
        eventSink.yield(.monitorAudioContext(context))
    }

    private func receiveAudioContext(_ payload: [String: LinkJSON], on media: Media) {
        guard let context = MediaControlDecoder.audioContext(payload, detail: gates.audioDetail,
                                                             profile: media.audioProfileSent,
                                                             bitrate: media.audioBitrateSent),
              context.revision == audioRevision,
              audioGeneration.map({ Self.isNewer(context.anchor.generation, than: $0) }) ?? true,
              context.anchor.ssrc == media.audioSsrc else {
            return
        }
        audioGeneration = context.anchor.generation
        runningAudio = context.enabled ? (context.lossless, context.encoder?.targetBitrate) : nil
        audioAnswered = true
        clockAudioContextPlaying = context.enabled && !context.lossless
        if clockActivity?.generation != context.anchor.generation || !clockAudioContextPlaying {
            stopClockProbing()
        } else if clockCapture?.generation != context.anchor.generation {
            clockCapture = nil
        }
        // Lossless plays as Opus does, so its silence is a stall too; the
        // clock probes above stay with Opus.
        if !context.enabled { resetAudioStall() }
        // A replacement keeps the playout clock for lossless as for Opus,
        // so long as the stream still carries the format being played.
        let playing = playback?.anchor
        let sameStream = context.enabled && dualAudioNewId == media.id
            && playing?.format == context.anchor.format
            && playing?.firstSequence == context.anchor.firstSequence
            && playing?.firstTimestamp == context.anchor.firstTimestamp
        if sameStream {
            playback?.adoptEquivalentAnchor(context.anchor)
        } else {
            if dualAudioNewId != media.id, replacement != nil {
                // OLD started a genuinely new audio generation while NEW
                // connects. Its earlier held packets and duplicate history
                // belong to the previous generation.
                dualAudioTimer?.cancel()
                dualAudioTimer = nil
                dualAudio = DualReceiveAudio(audioFormat: context.anchor.format)
                playback?.beginReplacement()
            }
            playback?.reanchor(context.enabled ? context.anchor : nil)
            if dualAudioNewId == media.id {
                // A genuinely new generation starts a new decoder history;
                // old-path packets cannot join that generation.
                dualAudioTimer?.cancel()
                dualAudioTimer = nil
                dualAudio = nil
                dualAudioOldId = nil
                dualAudioNewId = nil
                replacementMergeFinished = true
                playback?.endReplacement()
                // A queued later route was waiting for this merge to ease.
                // With its timer gone, the safety poll must resume that move.
                scheduleReplacementPoll()
            } else if context.enabled, let candidate = replacement {
                // Audio may have been enabled during overlap. Reanchoring
                // OLD must keep NEW's equivalent SSRC eligible.
                playback?.acceptEquivalentSsrc(candidate.audioSsrc)
            }
        }
        media.peer.setExpectedAudioSsrc(dualAudio == nil ? context.anchor.ssrc : nil)
        reconcileLinkTrial(losslessPlaying: context.enabled && context.lossless && audioWanted)
        eventSink.yield(.audioContext(context))
    }

    // MARK: Display endpoints

    /// Asks the Core for a display endpoint, or renews one with a newer
    /// revision. Throws, sending nothing, for a request the Core would
    /// refuse (``DisplayEndpointRequest/Invalid``) or without a media connection.
    public func subscribe(_ asked: DisplaySubscription) async throws {
        guard let media else {
            throw MediaControlError.noMediaConnection
        }
        let gates = self.gates.declaring(txDisplayVersion: media.txDisplayDeclared)
        // The transmit window and duplex go only where this connection's
        // start declared them; a Core that began offering them after the
        // start gets them on the next connection.
        var subscription = asked
        if !gates.txDisplay {
            subscription.txWindow = nil
        }
        if !gates.displayDuplex {
            subscription.duplex = nil
        }
        try DisplayEndpointRequest.validate(subscription, gates: gates)
        if let last = lastRevisions[subscription.endpointId], !Self.isNewer(subscription.revision, than: last) {
            throw DisplayEndpointRequest.Invalid.staleRevision
        }
        if endpoints[subscription.endpointId] == nil && endpoints.count >= DisplayEndpointRequest.maximumEndpoints {
            throw DisplayEndpointRequest.Invalid.tooManyEndpoints
        }
        let held = endpoints[subscription.endpointId]
        endpoints[subscription.endpointId] = Endpoint(
            subscription: subscription, firstRevision: held?.firstRevision ?? subscription.revision,
            widebandNegotiated: gates.wideband && subscription.extendedView != nil,
            context: held?.context, decoder: held?.decoder, keyframeTimes: held?.keyframeTimes ?? [],
            extras: held?.extras)
        lastRevisions[subscription.endpointId] = subscription.revision
        await transmit(DisplayEndpointRequest.subscribe(subscription, connectionId: media.id, gates: gates))
    }

    /// Lets an endpoint go. On the display budget wire the release carries
    /// the endpoint's next revision.
    public func unsubscribe(endpointId: UInt32) async throws {
        guard let media else {
            throw MediaControlError.noMediaConnection
        }
        guard let endpoint = endpoints.removeValue(forKey: endpointId) else {
            throw MediaControlError.unknownEndpoint
        }
        pendingRetunes[endpointId] = nil
        var revision = endpoint.subscription.revision &+ 1
        if revision == 0 {
            revision = 1
        }
        let gates = self.gates
        if gates.displayBudget {
            lastRevisions[endpointId] = revision
        }
        await transmit(DisplayEndpointRequest.unsubscribe(endpointId: endpointId, revision: revision,
                                                          connectionId: media.id, gates: gates))
    }

    /// Asks the Core for a keyframe of the endpoint's current context: only
    /// once a context is accepted, and at most ``keyframesPerSecond`` in any
    /// second. Returns whether it was sent.
    @discardableResult
    public func requestKeyframe(endpointId: UInt32) async -> Bool {
        guard let media, var endpoint = endpoints[endpointId], let context = endpoint.context else {
            return false
        }
        let time = now()
        endpoint.keyframeTimes.removeAll { time &- $0 >= Self.keyframeWindowMs }
        guard endpoint.keyframeTimes.count < Self.keyframesPerSecond else {
            return false
        }
        endpoint.keyframeTimes.append(time)
        endpoints[endpointId] = endpoint
        await transmit(DisplayEndpointRequest.keyframe(endpointId: endpointId,
                                                       contextGeneration: context.contextGeneration,
                                                       connectionId: media.id))
        return true
    }

    /// Asks the Core to re-estimate the noise floor for the endpoint's
    /// Clarity now (the desktop's Re-tune): only when the Core offers it
    /// (``MediaFeatureGates/clarityRetune``) and the endpoint has an
    /// accepted context. The Core answers only a refusal, reported as
    /// ``MediaControlEvent/clarityRetuneRefused(_:)``.
    public func retuneClarity(endpointId: UInt32) async throws {
        guard let media else {
            throw MediaControlError.noMediaConnection
        }
        guard let endpoint = endpoints[endpointId] else {
            throw MediaControlError.unknownEndpoint
        }
        guard gates.clarityRetune, endpoint.context != nil else {
            throw MediaControlError.notOffered
        }
        pendingRetunes[endpointId, default: 0] += 1
        await transmit(DisplayEndpointRequest.clarityRetune(endpointId: endpointId, connectionId: media.id))
    }

    /// A context the Core sent for one of this endpoint's revisions. While a
    /// drag, pinch or tune sends revision after revision, the Core answers
    /// each with its result at once and its context with the next frame, so
    /// by the time a context comes the app has often asked again. A context
    /// for a revision this app sent, newer than the one it holds, is taken
    /// all the same: its frames say which frequencies they cover, so the
    /// band keeps drawing live lines where they fall until the newest
    /// revision's context comes. Refusing them left the band with no
    /// frames at all for as long as the finger moved.
    private func receiveContext(_ payload: [String: LinkJSON]) async {
        guard let rawId = MediaControlDecoder.whole(payload["endpointId"], 1...MediaControlDecoder.maxUInt32),
              var endpoint = endpoints[UInt32(rawId)],
              let context = MediaControlDecoder.context(payload, wideband: endpoint.widebandNegotiated,
                                                        grant: gates.spectrumGrant,
                                                        transmit: (media?.txDisplayDeclared ?? 0) >= 1),
              Self.sent(context.revision, first: endpoint.firstRevision, latest: endpoint.subscription.revision),
              endpoint.context.map({ held in
                  held.revision == context.revision
                      ? Self.isNewer(context.contextGeneration, than: held.contextGeneration)
                      : Self.isNewer(context.revision, than: held.revision)
              }) ?? true else {
            return
        }
        endpoint.context = context
        endpoint.decoder = DisplayFrameDecoder()
        // Extras belong to the context they were decoded against.
        endpoint.extras = nil
        endpoints[context.endpointId] = endpoint
        eventSink.yield(.context(context))
        await requestKeyframe(endpointId: context.endpointId)
    }

    private func display(_ datagram: Data, connection id: String) async {
        if replacement?.id == id || media?.id == id { noteLadderMedia(audio: false) }
        if replacement?.id == id {
            if earlyReplacementDisplay.count < Self.earlyDisplayMaximumCount,
               earlyReplacementDisplayBytes + datagram.count <= Self.earlyDisplayMaximumBytes {
                earlyReplacementDisplay.append(datagram)
                earlyReplacementDisplayBytes += datagram.count
            }
            return
        }
        guard media?.id == id else {
            return
        }
        if datagram.prefix(4).elementsEqual(Self.extrasMagic) {
            displayExtras(datagram)
            return
        }
        guard datagram.count >= 16, datagram.prefix(4).elementsEqual(Self.displayMagic) else {
            return
        }
        let bytes = [UInt8](datagram.prefix(16))
        let endpointId = Self.bigEndian(bytes, at: 8)
        let generation = Self.bigEndian(bytes, at: 12)
        // A frame before its endpoint's context, or of any other context, is discarded.
        guard let endpoint = endpoints[endpointId], let context = endpoint.context,
              context.contextGeneration == generation, let decoder = endpoint.decoder else {
            return
        }
        let result = decoder.decode(datagram)
        switch result.disposition {
        case .accepted:
            if let frame = result.frame {
                eventSink.yield(.displayFrame(frame))
            }
        case .needKeyframe:
            await requestKeyframe(endpointId: endpointId)
        default:
            break
        }
    }

    /// One NSDX datagram, decoded against its endpoint's accepted context;
    /// an endpoint with none refuses it as a context mismatch.
    private func displayExtras(_ datagram: Data) {
        var context: DisplayExtrasDecoder.Context?
        if datagram.count >= 12 {
            let endpointId = Self.bigEndian([UInt8](datagram.prefix(12)), at: 8)
            context = endpoints[endpointId]?.context.map(DisplayExtrasDecoder.Context.init)
        }
        let result = DisplayExtrasDecoder.decode(datagram, context: context)
        guard let extras = result.extras, var endpoint = endpoints[extras.endpointId] else {
            extrasRefusals[result.reason, default: 0] += 1
            return
        }
        endpoint.extras = extras
        endpoints[extras.endpointId] = endpoint
        eventSink.yield(.displayExtras(extras))
    }

    // MARK: The Core's operations

    private func receive(_ payload: [String: LinkJSON]) async {
        let receivedNs = nowNs()
        guard let id = MediaControlDecoder.string(payload["connectionId"]),
              let op = MediaControlDecoder.string(payload["op"]) else {
            return
        }
        if op == "replace" {
            await acceptReplacement(payload)
            return
        }
        guard let peer = media?.id == id ? media : (replacement?.id == id ? replacement : nil) else {
            return
        }
        switch op {
        case "description":
            guard let description = MediaControlDecoder.description(payload) else {
                return
            }
            eventSink.yield(.description(description))
            do {
                try peer.peer.setRemoteDescription(description.sdp)
            } catch {
                Self.logger.warning("The Core's media offer was refused: \(String(describing: error), privacy: .public)")
            }
        case "candidate":
            guard let candidate = MediaControlDecoder.candidate(payload) else {
                return
            }
            eventSink.yield(.candidate(candidate))
            do {
                try peer.peer.addRemoteCandidate(candidate.candidate, mid: candidate.mid)
            } catch {
                Self.logger.info("A media candidate from the Core was refused: \(String(describing: error), privacy: .public)")
            }
        case "context":
            guard media?.id == id else { return }
            await receiveContext(payload)
        case "rejected":
            await receiveRejection(payload, connection: id)
        case "noise-floor":
            guard media?.id == id else { return }
            guard let floor = MediaControlDecoder.noiseFloor(payload),
                  let context = endpoints[floor.endpointId]?.context,
                  context.revision == floor.revision, context.contextGeneration == floor.contextGeneration else {
                return
            }
            eventSink.yield(.noiseFloor(floor))
        case "audio-context":
            guard media?.id == id else { return }
            receiveAudioContext(payload, on: peer)
            await sendBitrateAwaitingAnswer()
        case "monitor-audio-context":
            guard media?.id == id else { return }
            receiveMonitorAudioContext(payload, on: peer)
        case "clock-echo":
            guard media?.id == id, peer.connected else { return }
            receiveClockEcho(payload, receivedNs: receivedNs)
        case "allocation-result":
            guard media?.id == id else { return }
            receiveAllocationResult(payload)
        default:
            Self.logger.info("Ignoring a media control operation the app does not know: \(op, privacy: .public)")
        }
    }

    private func receiveRejection(_ payload: [String: LinkJSON], connection id: String) async {
        guard let rejection = MediaControlDecoder.rejected(payload) else {
            return
        }
        if replacement?.id == id {
            guard rejection.refusesWholePeer else { return }
            eventSink.yield(.rejected(rejection))
            let onAir = rejection.reason ==
                "The Core did not move audio and display: the radio is transmitting."
            await abortReplacement(onAirRefusal: onAir)
            return
        }
        guard media?.id == id else { return }
        if rejection.refusesWholePeer {
            Self.logger.warning("The Core refused this media connection: \(rejection.reason, privacy: .private)")
            eventSink.yield(.rejected(rejection))
            if Self.restartingRefusalReasons.contains(rejection.reason) {
                // The Core dropped the connection itself: start another.
                await mediaEnded(.droppedByCore, reporting: true)
            } else {
                // Final: no restart in this session.
                cancelRestart()
                retire()
            }
            return
        }
        // A refused retune names the endpoint with revision 0, and only a
        // retune's refusal does; the endpoint itself stands. A rejected
        // carrying a subscription's revision is that subscription's own.
        if rejection.revision == 0 {
            if let waiting = pendingRetunes[rejection.endpointId] {
                pendingRetunes[rejection.endpointId] = waiting > 1 ? waiting - 1 : nil
                eventSink.yield(.clarityRetuneRefused(rejection))
            }
            return
        }
        // On the display budget wire allocation-result replaces rejected.
        guard !gates.displayBudget, let endpoint = endpoints[rejection.endpointId],
              endpoint.subscription.revision == rejection.revision else {
            return
        }
        endpoints.removeValue(forKey: rejection.endpointId)
        eventSink.yield(.rejected(rejection))
    }

    private func receiveAllocationResult(_ payload: [String: LinkJSON]) {
        guard gates.displayBudget, let result = MediaControlDecoder.allocationResult(payload) else {
            return
        }
        if let endpoint = endpoints[result.endpointId], endpoint.subscription.revision == result.revision,
           !result.accepted, result.acceptedRevision == 0 {
            // The Core holds nothing for this endpoint any more.
            endpoints.removeValue(forKey: result.endpointId)
        }
        eventSink.yield(.allocationResult(result))
    }

    // MARK: Core clock probes

    /// Activity evidence arrives at 1 Hz; one missed collector turn is
    /// tolerated, but an absent third turn stops probing.
    private static let clockActivityFreshNs: Int64 = 3_000_000_000
    private static let clockProbeInterval: Duration = .seconds(1)
    private static let maximumPendingClockProbes = 8

    private func clockActivityIsCurrent(at time: Int64) -> Bool {
        guard let activity = clockActivity, activity.isPlaying,
              let media, media.connected, media.id == activity.mediaID,
              gates.audioClock, audioWanted, clockAudioContextPlaying,
              audioGeneration == activity.generation,
              time >= 0, activity.observedNs >= 0, activity.observedNs <= time else {
            return false
        }
        return time - activity.observedNs <= Self.clockActivityFreshNs
    }

    /// Invalidate already-started timer callbacks and all answers in the
    /// retired context. The owner-scoped activity revision remains intact.
    private func stopClockProbing() {
        clockProbeArm &+= 1
        clockContext &+= 1
        clockProbeTimer?.cancel()
        clockProbeTimer = nil
        clockActivity = nil
        pendingClockProbes.removeAll()
        clockEstimator.reset()
        clockCapture = nil
        lastClockEchoNs = nil
    }

    private func armClockProbeIfNeeded() {
        guard clockProbeTimer == nil, clockActivityIsCurrent(at: nowNs()),
              let media else { return }
        clockProbeArm &+= 1
        let arm = clockProbeArm
        let owner = logicalOwner
        let id = media.id
        clockProbeTimer = timers.schedule(after: Self.clockProbeInterval) { [weak self] in
            await self?.clockProbeDue(arm: arm, owner: owner, mediaID: id)
        }
    }

    private func clockProbeDue(arm: UInt64, owner: UInt64, mediaID: String) async {
        guard arm == clockProbeArm, owner == logicalOwner,
              let media, media.connected, media.id == mediaID,
              clockProbeTimer != nil else { return }
        clockProbeTimer = nil
        let time = nowNs()
        guard clockActivityIsCurrent(at: time),
              time >= 0, Double(time) < Double(Int64.max) else {
            stopClockProbing()
            return
        }
        // LinkJSON writes this Double, so the stored sample uses the
        // quantized integer that Core will echo, including after long uptime.
        let wireT0 = Double(time)
        let t0Ns = Int64(wireT0)
        nextClockProbeID &+= 1
        nextClockRequestSerial &+= 1
        let request = PendingClockProbe(id: nextClockProbeID, t0: wireT0,
                                        t0Ns: t0Ns, serial: nextClockRequestSerial)
        pendingClockProbes.removeAll { $0.id == request.id }
        pendingClockProbes.append(request)
        if pendingClockProbes.count > Self.maximumPendingClockProbes {
            pendingClockProbes.removeFirst()
        }
        let context = clockContext
        let sender = logicalSession
        let payload: [String: LinkJSON] = [
            "op": .string("clock-probe"), "connectionId": .string(mediaID),
            "id": .number(Double(request.id)), "t0": .number(wireT0),
        ]
        // The next tick is armed before suspension. Completion can only
        // remove this exact pending request and cannot arm NEW's timer.
        armClockProbeIfNeeded()
        do {
            let message = LinkMessage.mediaControl(.init(payload: payload))
            if let sender {
                try await sender.send(message)
            } else {
                try await send(message)
            }
        } catch {
            guard context == clockContext, owner == logicalOwner,
                  self.media?.id == mediaID else { return }
            pendingClockProbes.removeAll { $0.serial == request.serial }
            Self.logger.warning("Could not send a clock probe: \(String(describing: error), privacy: .public)")
        }
    }

    private func receiveClockEcho(_ payload: [String: LinkJSON], receivedNs: Int64) {
        guard clockActivityIsCurrent(at: receivedNs),
              let echo = MediaClockEcho(payload),
              let index = pendingClockProbes.firstIndex(where: { $0.id == echo.id && $0.t0 == echo.t0 }),
              pendingClockProbes[index].t0Ns == echo.t0Ns else { return }
        let pending = pendingClockProbes.remove(at: index)
        guard clockEstimator.add(AudioClockSample(t0Ns: pending.t0Ns,
                                                  t1Ns: echo.t1Ns, t2Ns: echo.t2Ns,
                                                  t3Ns: receivedNs)) else { return }
        lastClockEchoNs = receivedNs
        if echo.generation == 0 {
            clockCapture = nil
        } else if echo.generation == clockActivity?.generation {
            clockCapture = AudioCaptureAnchor(generation: echo.generation,
                                              rtpTimestamp: echo.rtpTimestamp,
                                              capturedNs: echo.capturedNs)
        } else {
            clockCapture = nil
        }
    }

    // MARK: Helpers

    private func transmit(_ payload: [String: LinkJSON]) async {
        if let owner = Self.operationOwner,
           (owner != logicalOwner || logicalSession == nil) { return }
        do {
            let message = LinkMessage.mediaControl(LinkMessage.MediaControl(payload: payload))
            if let logicalSession {
                try await logicalSession.send(message)
            } else {
                try await send(message)
            }
        } catch {
            Self.logger.warning("Could not send a media control operation: \(String(describing: error), privacy: .public)")
        }
    }

    /// Serial-number order over 32 bits (RFC 1982), as the Core orders
    /// revisions and generations.
    /// Whether `revision` lies from `first` to `latest`: one this app sent.
    static func sent(_ revision: UInt32, first: UInt32, latest: UInt32) -> Bool {
        revision == latest || revision == first
            || (isNewer(revision, than: first) && isNewer(latest, than: revision))
    }

    static func isNewer(_ value: UInt32, than previous: UInt32) -> Bool {
        value != previous && value &- previous < 0x8000_0000
    }

    private static func bigEndian(_ bytes: [UInt8], at offset: Int) -> UInt32 {
        UInt32(bytes[offset]) << 24 | UInt32(bytes[offset + 1]) << 16
            | UInt32(bytes[offset + 2]) << 8 | UInt32(bytes[offset + 3])
    }
}
