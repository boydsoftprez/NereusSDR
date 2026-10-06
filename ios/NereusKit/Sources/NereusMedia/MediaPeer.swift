// NereusSDR for iOS: the answering side of the Core's media peer (DTLS, SCTP display, SRTP audio)
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import os

/// The app's end of one media connection to the Core (the media control
/// document, `2026-09-20-remote-media-control-v1.md`, "Media peer"). The
/// Core offers and the app answers. Display frames arrive on the `display`
/// data channel (unordered, no retransmissions) and audio as RTP on the
/// `audio` media line, send-only from the Core, both secured by DTLS against
/// the fingerprint in the Core's offer.
///
/// Signalling is trickle only: the answer from ``localDescription`` carries
/// no candidates, and each local candidate goes out through
/// ``localCandidates``. Every limit of the media control document is checked
/// here, before anything reaches libdatachannel.
///
/// Received media waits in bounded queues between drains; when a queue is
/// full the oldest entry is dropped, so a slow reader always gets the newest
/// audio and the newest display frame.
///
/// With ``enableMicrophoneLine(ssrc:)`` (a start that carried
/// `remoteTxVersion`, iPhone app plan Task 55) the peer also answers the
/// Core's microphone line, `a=mid:mic`, send-only with Opus alone and the
/// microphone's SSRC declared, and takes the Core's "tx" data channel
/// (media control document, "Microphone line"). The line and the channel
/// carry the phone's microphone and its transmit keepalive.
public final class MediaPeer: @unchecked Sendable {
    public enum Route: Sendable {
        case direct
        case relay(RelayRouteContext)
        case tunnel(MediaTunnelContext)
    }

    private enum RouteClaim: Sendable {
        case relay(RelayRouteContext, RelayICEClaim)
        case tunnel(MediaTunnelContext, MediaTunnelClaim)
    }
    /// How the peer connection is set up. The defaults are the Core's own
    /// peer settings.
    public struct Configuration: Sendable, Equatable {
        /// STUN and TURN servers: none on a direct-address session unless a
        /// STUN server is known (``directAddress(stunServers:)``).
        public var iceServers: [String]
        /// The path MTU libdatachannel works to, in bytes.
        public var mtu: Int
        /// The largest data channel message, in bytes.
        public var maxMessageSize: Int
        /// Only host candidates are sent or accepted. False by default: a
        /// direct-address session's media climbs a ladder (R-IOS-16), IPv6
        /// direct, an IPv4 hole punch through STUN, then the Core's tunnel,
        /// so the Core's server reflexive candidates are kept.
        public var hostCandidatesOnly: Bool
        /// Relay candidates are neither sent nor accepted. True by default:
        /// a direct-address session takes no relay of its own and uses none
        /// of the Core's. A session through the remote access service
        /// (``throughRendezvous(_:)``) accepts them.
        public var refusesRelayCandidates: Bool
        /// Only the route's own candidate (the Core's tunnel, claimed in
        /// ``MediaPeer/prepare(connectionId:gates:)``): none of the Core's
        /// signalled candidates is taken and none of this end's is sent, so
        /// ICE can nominate the tunnel alone (the direct media ladder's
        /// fallback, ``tunnelAlone``).
        public var onlyRouteCandidate: Bool
        /// ICE over TCP.
        public var enableIceTcp: Bool
        /// The answer is set explicitly, with the media transport always on.
        public var forceMediaTransport: Bool
        public var disableAutoNegotiation: Bool
        /// libdatachannel's process-wide SCTP buffers, applied once per
        /// process before the first peer.
        public var sctpSendBufferBytes: Int
        public var sctpReceiveBufferBytes: Int

        public init(iceServers: [String] = [],
                    mtu: Int = 1000,
                    maxMessageSize: Int = 65536,
                    hostCandidatesOnly: Bool = false,
                    refusesRelayCandidates: Bool = true,
                    onlyRouteCandidate: Bool = false,
                    enableIceTcp: Bool = false,
                    forceMediaTransport: Bool = true,
                    disableAutoNegotiation: Bool = true,
                    sctpSendBufferBytes: Int = 65536,
                    sctpReceiveBufferBytes: Int = 131072) {
            self.iceServers = iceServers
            self.mtu = mtu
            self.maxMessageSize = maxMessageSize
            self.hostCandidatesOnly = hostCandidatesOnly
            self.refusesRelayCandidates = refusesRelayCandidates
            self.onlyRouteCandidate = onlyRouteCandidate
            self.enableIceTcp = enableIceTcp
            self.forceMediaTransport = forceMediaTransport
            self.disableAutoNegotiation = disableAutoNegotiation
            self.sctpSendBufferBytes = sctpSendBufferBytes
            self.sctpReceiveBufferBytes = sctpReceiveBufferBytes
        }

        /// A peer for a session that came through the remote access
        /// service: the Core's ICE settings (``IceSettings``), one STUN
        /// server and the relay, an MTU of 996, and candidates of every
        /// type. Throws ``IceSettings/NotReady`` until the relay credentials
        /// are known, so a peer never gathers without them: libdatachannel
        /// takes its servers when the peer is made and gathers only after.
        public static func throughRendezvous(_ ice: IceSettings) throws -> Configuration {
            guard ice.relayKnown else {
                throw IceSettings.NotReady()
            }
            return Configuration(iceServers: ice.libdatachannelServers, mtu: IceSettings.mtu,
                                 hostCandidatesOnly: false, refusesRelayCandidates: false)
        }

        /// A peer for a session to a Core dialled by address (R-IOS-16):
        /// every candidate type but relay, and the first `stun:` server of
        /// `stunServers`, if any, for the IPv4 hole punch. libdatachannel
        /// picks one server at random, so exactly one is passed; anything
        /// else in the list (a `turn:` URL, credentials, a query) is dropped,
        /// so no relay server is ever passed. With a STUN server the MTU is
        /// the Core's remote one, ``IceSettings/mtu``. `stunServers` is where
        /// a STUN list from any source plugs in.
        public static func directAddress(stunServers: [String] = []) -> Configuration {
            let stun = stunServers.first { url in
                url.hasPrefix("stun:") && url.count > 5
                    && !url.contains { $0 == "@" || $0 == "?" || $0.isWhitespace }
            }
            guard let stun else {
                return Configuration()
            }
            return Configuration(iceServers: [stun], mtu: IceSettings.mtu)
        }

        /// The direct media ladder's fallback (the Core's
        /// `MediaTunnel::tunnelIceFor`): on ``MediaPeer/Route/tunnel(_:)``,
        /// the tunnel's candidate alone. No STUN server, no relay, none of
        /// this phone's candidates sent and none of the Core's taken, so the
        /// connection runs on the tunnel or not at all. The wire is the
        /// ordinary three-field replace.
        public static let tunnelAlone = Configuration(onlyRouteCandidate: true)
    }

    /// Where the media connection stands.
    public enum State: Sendable, Equatable {
        case new
        case connecting
        /// Connected, with the display channel and the audio line open.
        case connected
        case disconnected
        /// The connection failed (a wrong DTLS fingerprint among the causes).
        case failed
        case closed
    }

    // The media control document's signalling limits.
    public static let maxDescriptionBytes = 64 * 1024
    public static let maxCandidateBytes = 4 * 1024
    public static let maxMidBytes = 256
    public static let maxRemoteCandidates = 64

    // The Core's media lines.
    public static let displayLabel = "display"
    public static let audioMid = "audio"
    public static let opusPayloadType: UInt8 = 111
    /// The Core's microphone line, the stream name its answer declares on
    /// it, and the transmit keepalive's data channel (Tasks 36 and 37).
    public static let microphoneMid = "mic"
    public static let microphoneStreamName = "nereus-microphone"
    public static let txLabel = "tx"
    /// The largest display message and the RTP packet size bounds.
    public static let maxDisplayMessageBytes = 64 * 1024
    public static let minRtpBytes = RtpPacket.headerBytes
    public static let maxRtpBytes = 940

    // The receive bounds between drains. Audio holds 2.56 s whatever its
    // format: 64 of Opus's 40 ms packets, as the media control document
    // gives for each declared stream (2026-09-20-remote-media-control-v1.md,
    // "64 packets per declared stream"), and 640 of the lossless profile's
    // 4 ms L16 ones (R-IOS-09), so a burst of Opus never queues ten times
    // the audio it did before the phone played lossless.
    public static let opusAudioQueuePackets = 64
    public static let losslessAudioQueuePackets = 640

    public static func audioQueuePackets(for format: AudioStreamFormat) -> Int {
        format == .l16 ? losslessAudioQueuePackets : opusAudioQueuePackets
    }
    public static let displayQueueMessages = 8
    public static let displayQueueBytes = 256 * 1024

    public let configuration: Configuration

    private static let logger = Logger(subsystem: "NereusSDR", category: "media.peer")

    /// The app's answer, once, after ``setRemoteDescription(_:)``.
    public let localDescription: AsyncStream<String>
    /// Each local candidate with its mid, to send to the Core.
    public let localCandidates: AsyncStream<(candidate: String, mid: String)>
    /// Audio packets, oldest first, at most ``audioQueuePackets(for:)``
    /// of the arriving packet's format waiting.
    public let audioPackets: AsyncStream<RtpPacket>
    /// Display messages, oldest first, at most ``displayQueueMessages`` or
    /// ``displayQueueBytes`` waiting.
    public let displayDatagrams: AsyncStream<Data>
    /// Each change of ``State``.
    public let state: AsyncStream<State>
    /// One element each time the Core closes the microphone line's track
    /// while the connection stays up (``microphoneClosedWhileUp``).
    public let microphoneLineClosed: AsyncStream<Void>

    private let descriptionContinuation: AsyncStream<String>.Continuation
    private let candidateContinuation: AsyncStream<(candidate: String, mid: String)>.Continuation
    private let stateContinuation: AsyncStream<State>.Continuation
    private let microphoneClosedContinuation: AsyncStream<Void>.Continuation
    let audioQueue: MediaReceiveQueue<RtpPacket>
    let displayQueue: MediaReceiveQueue<Data>

    // Everything below is read and written under `lock`. The lock is never
    // held while libdatachannel is called, since the library can call back
    // on the calling thread.
    private let lock = NSLock()
    private var bridge: RtcBridge?
    private let route: Route
    private let nativeLifetime = RtcPeerLifetime()
    private var routeClaim: RouteClaim?
    private var closeStarted = false
    private let closeSubmitted = MediaPeerCloseCompletion()
    private let closeCompletion = MediaPeerCloseCompletion()
    private var current: State = .new
    private var remoteDescriptionSet = false
    private var remoteCandidates = 0
    private var pendingCandidates: [(candidate: String, mid: String)] = []
    private var observedFarEndRelays: Set<NumericRouteEndpoint> = []
    private var displayChannel: Int32?
    private var audioTrack: Int32?
    private var transportConnected = false
    private var displayOpen = false
    private var audioOpen = false
    private var expectedAudioSsrc: UInt32?
    private var foreignSsrcDropped = 0
    private var rejectedAudioHeaders: UInt64 = 0
    /// The microphone line's SSRC, when this connection asked for the line.
    private var microphoneSsrc: UInt32?
    /// The line's track: the handle the offer brought, then the one this
    /// side's description of it gave. Both name the same line.
    private var microphoneTracks: [Int32] = []
    private var microphoneOpen = false
    /// This side's description of the line carries the Core's L16.
    private var microphoneLossless = false
    private var txChannel: Int32?
    private var txOpen = false
    private let trafficLifetime = UUID()
    private var receivedDisplayPayloadBytes: UInt64 = 0
    private var receivedRtpBytes: UInt64 = 0
    private var submittedRtpBytes: UInt64 = 0
    private var submittedTxChannelBytes: UInt64 = 0

    public var trafficObservation: MediaTrafficObservation? {
        lock.withLock {
            MediaTrafficObservation(lifetime: trafficLifetime,
                                    active: !closeStarted && current != .closed && current != .failed,
                                    receivedDisplayPayloadBytes: receivedDisplayPayloadBytes,
                                    receivedRtpBytes: receivedRtpBytes,
                                    submittedRtpBytes: submittedRtpBytes,
                                    submittedTxChannelBytes: submittedTxChannelBytes)
        }
    }

    public struct AudioAdmissionObservation: Sendable, Equatable {
        public let peerLifetime: UUID
        public let active: Bool
        public let rejectedHeaders: UInt64
        public let rejectedSsrc: UInt64
        public let acceptedIntoReceiveQueue: UInt64
        public let droppedByReceiveQueue: UInt64
        public let heldByReceiveQueue: Int
    }

    /// Parsing and bounded-queue facts that playback cannot see. The queue
    /// remains single-consumer; observing its counts does not drain packets.
    public var audioAdmissionObservation: AudioAdmissionObservation {
        let peer = lock.withLock {
            (active: !closeStarted && current != .closed && current != .failed,
             rejectedHeaders: rejectedAudioHeaders, rejectedSsrc: UInt64(foreignSsrcDropped))
        }
        let queued = audioQueue.counts
        return AudioAdmissionObservation(peerLifetime: trafficLifetime, active: peer.active,
                                         rejectedHeaders: peer.rejectedHeaders,
                                         rejectedSsrc: peer.rejectedSsrc,
                                         acceptedIntoReceiveQueue: UInt64(queued.accepted),
                                         droppedByReceiveQueue: UInt64(queued.dropped),
                                         heldByReceiveQueue: queued.held)
    }

    public init(configuration: Configuration = Configuration(), route: Route = .direct) {
        self.configuration = configuration
        self.route = route
        (localDescription, descriptionContinuation) = AsyncStream.makeStream(of: String.self)
        (localCandidates, candidateContinuation) =
            AsyncStream.makeStream(of: (candidate: String, mid: String).self)
        (state, stateContinuation) = AsyncStream.makeStream(of: State.self)
        (microphoneLineClosed, microphoneClosedContinuation) = AsyncStream.makeStream(of: Void.self)
        audioQueue = MediaReceiveQueue(maxCount: Self.losslessAudioQueuePackets)
        displayQueue = MediaReceiveQueue(maxCount: Self.displayQueueMessages,
                                         maxBytes: Self.displayQueueBytes,
                                         size: { $0.count })
        audioPackets = audioQueue.stream
        displayDatagrams = displayQueue.stream

        // The transport's own log goes to the device console, scrubbed, so
        // a slow reconnect can be read afterwards.
        MediaTransportLog.enable()
        RtcBridge.applySctpSettingsOnce(sendBufferBytes: configuration.sctpSendBufferBytes,
                                        receiveBufferBytes: configuration.sctpReceiveBufferBytes)
        let settings = RtcPeerSettings(iceServers: configuration.iceServers,
                                       mtu: configuration.mtu,
                                       maxMessageSize: configuration.maxMessageSize,
                                       enableIceTcp: configuration.enableIceTcp,
                                       forceMediaTransport: configuration.forceMediaTransport,
                                       disableAutoNegotiation: configuration.disableAutoNegotiation)
        do {
            let bridge = try RtcBridge(settings: settings, lifetime: nativeLifetime) { [weak self] event in
                self?.handle(event)
            }
            lock.withLock {
                self.bridge = bridge
            }
        } catch {
            finish(as: .failed)
        }
    }

    deinit {
        close()
    }

    /// Claims one generation's low-priority route before the Core sees start
    /// or replace. The same ICE agent keeps its usual local/STUN candidates.
    public func prepare(connectionId: UUID, gates: MediaFeatureGates) async throws {
        let claim: RouteClaim
        let candidate: String
        switch route {
        case .direct:
            return
        case .relay(let context):
            let owned = try await context.claimMedia(connectionId: gates.mediaRelayRouting ? connectionId : nil)
            claim = .relay(context, owned)
            candidate = owned.candidate
        case .tunnel(let context):
            guard gates.mediaTunnel else { return }
            let owned = try await context.claimMedia(connectionId: connectionId)
            claim = .tunnel(context, owned)
            candidate = owned.candidate
        }
        let accepted = lock.withLock { () -> Bool in
            guard bridge != nil, current != .closed, routeClaim == nil else { return false }
            routeClaim = claim
            return true
        }
        guard accepted else {
            await Self.release(claim)
            throw MediaPeerError.closed
        }
        do {
            try admitRemoteCandidate(candidate, mid: Self.audioMid, fromRoute: true)
        } catch {
            // Once attached, even a rejected candidate may have reached the
            // native agent. Its claim belongs to close's lifetime barrier.
            close()
            throw error
        }
    }

    private static func release(_ claim: RouteClaim) async {
        switch claim {
        case .relay(let context, let owned): await context.releaseMedia(owned)
        case .tunnel(let context, let owned): await context.releaseMedia(owned)
        }
    }

    public var selectedTunnel: Bool {
        let selection = lock.withLock { () -> (RtcBridge, RouteClaim)? in
            guard let bridge, let routeClaim else { return nil }
            return (bridge, routeClaim)
        }
        guard let (bridge, claim) = selection,
              case .tunnel(_, let tunnel) = claim,
              let pair = bridge.selectedCandidatePair() else { return false }
        return Self.isLoopbackCandidate(pair.remote, port: tunnel.port)
    }

    public var selectedRelayOrTunnel: Bool {
        let selection = lock.withLock { () -> (RtcBridge, RouteClaim?)? in
            guard let bridge else { return nil }
            return (bridge, routeClaim)
        }
        guard let (bridge, claim) = selection,
              let pair = bridge.selectedCandidatePair() else { return false }
        let port: UInt16?
        switch claim {
        case .relay(_, let relay): port = relay.port
        case .tunnel(_, let tunnel): port = tunnel.port
        case nil: port = nil
        }
        return Self.selectedPairUsesRelayOrTunnel(pair, loopbackPort: port)
    }

    /// Evidence from this connected peer's nominated ICE pair only. A held
    /// relay or tunnel claim changes the meaning of its selected loopback
    /// candidate; an unclaimed loopback remains ordinary ICE.
    public var selectedRouteObservation: SelectedRouteObservation {
        let snapshot = lock.withLock { () -> (RtcBridge, Set<NumericRouteEndpoint>,
                                              SelectedRouteObservation.CarrierClaim?)? in
            guard current == .connected, transportConnected, !closeStarted, let bridge else { return nil }
            let claim: SelectedRouteObservation.CarrierClaim?
            switch routeClaim {
            case .relay(_, let relay): claim = .webRelay(port: relay.port)
            case .tunnel(_, let tunnel): claim = .wssTunnel(port: tunnel.port)
            case nil: claim = nil
            }
            return (bridge, observedFarEndRelays, claim)
        }
        guard let (bridge, relays, claim) = snapshot else {
            return .unavailable(lock.withLock { closeStarted || current == .closed || current == .failed
                ? .retired : .notReady })
        }
        let pair = bridge.selectedCandidatePair()
        let reading = SelectedRouteObservation.fromSelectedICEPair(
            local: pair?.local, remote: pair?.remote, knownFarEndRelays: relays, carrierClaim: claim)
        let stillCurrent = lock.withLock {
            current == .connected && transportConnected && !closeStarted && self.bridge === bridge &&
                observedFarEndRelays == relays
        }
        return stillCurrent ? reading : .unavailable(.retired)
    }

    static func selectedPairUsesRelayOrTunnel(_ pair: (local: String, remote: String),
                                              loopbackPort: UInt16?) -> Bool {
        if candidateType(pair.local) == "relay" || candidateType(pair.remote) == "relay" { return true }
        guard let loopbackPort else { return false }
        return isLoopbackCandidate(pair.remote, port: loopbackPort)
    }

    private static func isLoopbackCandidate(_ candidate: String, port: UInt16) -> Bool {
        let fields = candidate.split(separator: " ")
        return fields.count >= 8 && fields[4] == "127.0.0.1"
            && fields[5] == Substring(String(port)) && candidateType(candidate) == "host"
    }

    // MARK: Signalling

    /// Sets the Core's offer and starts the answer, which arrives on
    /// ``localDescription``. Refused when over the limits, when it embeds
    /// candidates, or when an offer was already set.
    public func setRemoteDescription(_ sdp: String) throws {
        let bytes = sdp.utf8.count
        guard bytes > 0 else {
            throw MediaPeerError.emptyDescription
        }
        guard bytes <= Self.maxDescriptionBytes else {
            throw MediaPeerError.descriptionTooLarge(bytes: bytes)
        }
        guard !sdp.utf8.contains(0) else {
            throw MediaPeerError.containsNul
        }
        guard !Self.embedsCandidates(sdp) else {
            throw MediaPeerError.candidatesInDescription
        }
        let bridge: RtcBridge = try lock.withLock {
            guard let bridge = self.bridge, current != .closed, current != .failed else {
                throw MediaPeerError.closed
            }
            guard !remoteDescriptionSet else {
                throw MediaPeerError.remoteDescriptionAlreadySet
            }
            remoteDescriptionSet = true
            return bridge
        }
        try bridge.setRemoteDescription(sdp, type: "offer")
        try describeMicrophoneLine(on: bridge)
        if configuration.disableAutoNegotiation {
            try bridge.setLocalDescription(type: "answer")
        }
        let buffered = lock.withLock {
            defer { pendingCandidates.removeAll() }
            return pendingCandidates
        }
        for pending in buffered {
            try bridge.addRemoteCandidate(pending.candidate, mid: pending.mid)
            recordAdmittedFarEndRelay(pending.candidate, on: bridge)
        }
        update { current in
            current == .new ? .connecting : current
        }
    }

    /// Adds one of the Core's candidates. A candidate that arrives before the
    /// offer is held until the offer is set; both count toward
    /// ``maxRemoteCandidates``.
    public func addRemoteCandidate(_ candidate: String, mid: String) throws {
        try admitRemoteCandidate(candidate, mid: mid, fromRoute: false)
    }

    /// `fromRoute`: the candidate is this peer's own route claim's, not one
    /// the Core signalled.
    private func admitRemoteCandidate(_ candidate: String, mid: String, fromRoute: Bool) throws {
        let candidateBytes = candidate.utf8.count
        guard candidateBytes > 0, candidateBytes <= Self.maxCandidateBytes else {
            throw MediaPeerError.invalidCandidateLength(bytes: candidateBytes)
        }
        let midBytes = mid.utf8.count
        guard midBytes > 0, midBytes <= Self.maxMidBytes else {
            throw MediaPeerError.invalidMidLength(bytes: midBytes)
        }
        guard !candidate.utf8.contains(0), !mid.utf8.contains(0) else {
            throw MediaPeerError.containsNul
        }
        if configuration.onlyRouteCandidate && !fromRoute {
            throw MediaPeerError.notRouteCandidate
        }
        if configuration.hostCandidatesOnly && Self.candidateType(candidate) != "host" {
            throw MediaPeerError.notHostCandidate
        }
        if configuration.refusesRelayCandidates && Self.candidateType(candidate) == "relay" {
            throw MediaPeerError.relayCandidate
        }
        let bridge: RtcBridge? = try lock.withLock {
            guard let bridge = self.bridge, current != .closed, current != .failed else {
                throw MediaPeerError.closed
            }
            guard remoteCandidates < Self.maxRemoteCandidates else {
                throw MediaPeerError.tooManyCandidates
            }
            remoteCandidates += 1
            guard remoteDescriptionSet else {
                pendingCandidates.append((candidate, mid))
                return nil
            }
            return bridge
        }
        try bridge?.addRemoteCandidate(candidate, mid: mid)
        if let bridge { recordAdmittedFarEndRelay(candidate, on: bridge) }
    }

    private func recordAdmittedFarEndRelay(_ candidate: String, on bridge: RtcBridge) {
        guard let endpoint = SelectedRouteObservation.farEndRelayEndpoint(from: candidate) else { return }
        lock.withLock {
            if self.bridge === bridge, !closeStarted { observedFarEndRelays.insert(endpoint) }
        }
    }

    // MARK: Media

    /// When set, audio packets with any other SSRC are dropped; nil takes
    /// every SSRC. The session sets it from each audio context.
    public func setExpectedAudioSsrc(_ ssrc: UInt32?) {
        lock.withLock {
            expectedAudioSsrc = ssrc
        }
    }

    /// Sends one RTP packet on the audio line.
    public func sendAudio(_ packet: RtpPacket) throws {
        let bytes = packet.bytes
        guard bytes.count <= Self.maxRtpBytes else {
            throw MediaPeerError.packetTooLarge(bytes: bytes.count)
        }
        let target: (RtcBridge, Int32)? = lock.withLock {
            guard let bridge, let audioTrack, audioOpen, current != .closed else {
                return nil
            }
            return (bridge, audioTrack)
        }
        guard let target else {
            throw MediaPeerError.audioNotReady
        }
        lock.withLock { submittedRtpBytes &+= UInt64(bytes.count) }
        try target.0.send(bytes, on: target.1)
    }

    // MARK: The microphone line and the "tx" channel

    /// Asks for the Core's microphone line on this connection, sending
    /// `ssrc` on it, and takes the "tx" data channel. Call before
    /// ``setRemoteDescription(_:)``; an SSRC of 0 is ignored.
    public func enableMicrophoneLine(ssrc: UInt32) {
        guard ssrc != 0 else {
            return
        }
        lock.withLock {
            if !remoteDescriptionSet {
                microphoneSsrc = ssrc
            }
        }
    }

    /// The microphone line is open: the Core's offer carried it and the
    /// connection is up.
    public var microphoneLineReady: Bool {
        lock.withLock { microphoneOpen && transportConnected && current != .closed }
    }

    /// The microphone line was answered with the Core's L16 (R-IOS-09).
    public var microphoneLosslessNegotiated: Bool {
        lock.withLock { microphoneLossless && !microphoneTracks.isEmpty }
    }

    /// The "tx" data channel is open.
    public var txChannelReady: Bool {
        lock.withLock { txOpen && current != .closed }
    }

    /// Sends one RTP packet on the microphone line; its SSRC must be the line's.
    public func sendMicrophone(_ packet: RtpPacket) throws {
        let bytes = packet.bytes
        guard bytes.count <= Self.maxRtpBytes else {
            throw MediaPeerError.packetTooLarge(bytes: bytes.count)
        }
        let target: (RtcBridge, Int32)? = lock.withLock {
            guard let bridge, let track = microphoneTracks.last, microphoneOpen, current != .closed,
                  packet.ssrc == microphoneSsrc else {
                return nil
            }
            return (bridge, track)
        }
        guard let target else {
            throw MediaPeerError.microphoneNotReady
        }
        lock.withLock { submittedRtpBytes &+= UInt64(bytes.count) }
        try target.0.send(bytes, on: target.1)
    }

    /// Sends one binary message on the "tx" data channel.
    public func sendTx(_ message: Data) throws {
        let target: (RtcBridge, Int32)? = lock.withLock {
            guard let bridge, let txChannel, txOpen, current != .closed else {
                return nil
            }
            return (bridge, txChannel)
        }
        guard let target else {
            throw MediaPeerError.txChannelNotReady
        }
        lock.withLock { submittedTxChannelBytes &+= UInt64(message.count) }
        try target.0.send(message, on: target.1)
    }

    /// After the offer is set and before the answer is written: this side's
    /// description of the microphone line, send-only, Opus alone, with the
    /// microphone's SSRC. The library's track callback cannot do it, since
    /// the library holds its track table's lock there.
    private func describeMicrophoneLine(on bridge: RtcBridge) throws {
        let pending: (Int32, UInt32)? = lock.withLock {
            guard let ssrc = microphoneSsrc, let offered = microphoneTracks.first else {
                return nil
            }
            return (offered, ssrc)
        }
        guard let pending else {
            return
        }
        guard let offered = bridge.description(ofTrack: pending.0),
              let answer = Self.microphoneAnswer(from: offered, ssrc: pending.1) else {
            lock.withLock { microphoneTracks.removeAll() }
            bridge.reject(pending.0)
            return
        }
        let described = try bridge.describeTrack(answer)
        bridge.countTraffic(on: described, as: .audio)
        let open = bridge.isOpen(described)
        lock.withLock {
            microphoneTracks.append(described)
            microphoneLossless = Self.describesL16(answer)
        }
        if open {
            markOpen(described)
        }
    }

    /// Ends the media connection and every stream. Safe to call twice.
    public func close() {
        let closing: (RtcBridge?, RouteClaim?)? = lock.withLock {
            guard !closeStarted else { return nil }
            closeStarted = true
            defer { self.bridge = nil; self.routeClaim = nil }
            return (self.bridge, self.routeClaim)
        }
        guard let closing else { return }
        let lifetime = nativeLifetime
        let submitted = closeSubmitted
        let completion = closeCompletion
        DispatchQueue.global(qos: .utility).async {
            closing.0?.close()
            submitted.finish()
            Task {
                if closing.0 != nil { await lifetime.wait() }
                if let claim = closing.1 {
                    await Self.release(claim)
                }
                completion.finish()
            }
        }
        finish(as: .closed)
    }

    func waitUntilNativeDeleteReturned() async { await closeSubmitted.wait() }

    /// Completes after the native ICE agent and its candidate claim retire.
    public func closeWhenDeleted() async {
        close()
        await closeCompletion.wait()
    }

    /// Received media counts, for tests and diagnostics.
    var receiveCounts: (audio: (accepted: Int, dropped: Int, held: Int),
                        foreignSsrcDropped: Int,
                        display: (accepted: Int, dropped: Int, held: Int)) {
        let foreign = lock.withLock { foreignSsrcDropped }
        return (audioQueue.counts, foreign, displayQueue.counts)
    }

    // MARK: Library events

    /// Whether a microphone track closing now is the Core closing the line
    /// on a connection that stays up: the transport connected, the display
    /// channel and audio line open, no close of this phone's own and the
    /// state connected. libdatachannel moves the peer to disconnected,
    /// failed or closed before it closes the tracks of a connection going
    /// down, so a track that closes with the connection fails this.
    static func microphoneClosedWhileUp(transportConnected: Bool, displayOpen: Bool, audioOpen: Bool,
                                        closeStarted: Bool, state: State) -> Bool {
        transportConnected && displayOpen && audioOpen && !closeStarted && state == .connected
    }

    private func handle(_ event: RtcEvent) {
        switch event {
        case .localDescription(let sdp, _):
            descriptionContinuation.yield(Self.withoutCandidates(sdp))
        case .gatheringComplete:
            // The Core's media connection needs no end of candidates.
            return
        case .localCandidate(let candidate, let mid):
            guard candidate.utf8.count <= Self.maxCandidateBytes,
                  mid.utf8.count <= Self.maxMidBytes,
                  !configuration.onlyRouteCandidate,
                  !configuration.hostCandidatesOnly || Self.candidateType(candidate) == "host",
                  !configuration.refusesRelayCandidates || Self.candidateType(candidate) != "relay" else {
                return
            }
            candidateContinuation.yield((candidate: candidate, mid: mid))
        case .peerState(let peerState):
            switch peerState {
            case .new, .connecting:
                setTransportConnected(false)
            case .connected:
                setTransportConnected(true)
            case .disconnected:
                lock.withLock { transportConnected = false }
                update { _ in .disconnected }
            case .failed:
                finish(as: .failed)
            case .closed:
                finish(as: .closed)
            }
        case .dataChannel(let id):
            adoptDataChannel(id)
        case .track(let id):
            adoptAudioTrack(id)
        case .open(let id):
            markOpen(id)
        case .closed(let id):
            // The Core closing its display channel or audio line ends the
            // connection. The library is not called back into from its own
            // callback; close() or deinit deletes the peer.
            let (ours, microphoneClosed) = lock.withLock { () -> (Bool, Bool) in
                var microphoneClosed = false
                if microphoneTracks.contains(id) {
                    microphoneClosed = microphoneOpen && Self.microphoneClosedWhileUp(
                        transportConnected: transportConnected, displayOpen: displayOpen, audioOpen: audioOpen,
                        closeStarted: closeStarted, state: current)
                    microphoneOpen = false
                } else if id == txChannel {
                    txOpen = false
                }
                return (id == displayChannel || id == audioTrack, microphoneClosed)
            }
            if microphoneClosed {
                // The line is gone while the connection stays up: the
                // client says so, so the phone stops counting on it (M6)
                // and a key held here is released. A line that goes with
                // the connection is reported when the client retires it.
                microphoneClosedContinuation.yield()
            }
            if ours {
                finish(as: .closed)
            }
        case .message(let id, let data):
            receive(data, on: id)
        case .error, .textMessage:
            break
        }
    }

    /// The Core's `display` channel, and its `tx` channel when this
    /// connection asked for the microphone line; both unordered with no
    /// retransmissions. Anything else is refused.
    private func adoptDataChannel(_ id: Int32) {
        guard let bridge = lock.withLock({ self.bridge }) else {
            return
        }
        let label = bridge.label(ofChannel: id)
        let fits = (label == Self.displayLabel || label == Self.txLabel)
            && bridge.isUnorderedWithoutRetransmissions(channel: id)
        let adopted = fits && lock.withLock {
            if label == Self.txLabel {
                guard microphoneSsrc != nil, txChannel == nil else {
                    return false
                }
                txChannel = id
                return true
            }
            guard displayChannel == nil else {
                return false
            }
            displayChannel = id
            return true
        }
        guard adopted else {
            bridge.reject(id)
            return
        }
        bridge.countTraffic(on: id, as: label == Self.txLabel ? .transmit : .display)
        if bridge.isOpen(id) {
            markOpen(id)
        }
    }

    private func adoptAudioTrack(_ id: Int32) {
        guard let bridge = lock.withLock({ self.bridge }) else {
            return
        }
        let mid = bridge.mid(ofTrack: id)
        let opus = Self.describesOpus(bridge.description(ofTrack: id) ?? "")
        let adopted = opus && lock.withLock {
            if mid == Self.microphoneMid {
                // Described after the offer is set (describeMicrophoneLine).
                guard microphoneSsrc != nil, microphoneTracks.isEmpty else {
                    return false
                }
                microphoneTracks = [id]
                return true
            }
            guard mid == Self.audioMid, audioTrack == nil else {
                return false
            }
            audioTrack = id
            return true
        }
        guard adopted else {
            bridge.reject(id)
            return
        }
        bridge.countTraffic(on: id, as: .audio)
        if bridge.isOpen(id) {
            markOpen(id)
        }
    }

    private func markOpen(_ id: Int32) {
        lock.withLock {
            if id == displayChannel {
                displayOpen = true
            } else if id == audioTrack {
                audioOpen = true
            } else if microphoneTracks.contains(id) {
                microphoneOpen = true
            } else if id == txChannel {
                txOpen = true
            }
        }
        setTransportConnected(nil)
    }

    /// Records the transport's state (nil keeps it) and moves to
    /// ``State/connected`` once the display channel and audio line are open.
    private func setTransportConnected(_ connected: Bool?) {
        let ready: Bool = lock.withLock {
            if let connected {
                transportConnected = connected
            }
            // A line the offer brought is open too before media counts
            // as up, so the microphone is ready with it.
            return transportConnected && displayOpen && audioOpen
                && (microphoneTracks.isEmpty || microphoneOpen)
        }
        update { current in
            switch current {
            case .closed, .failed:
                return current
            default:
                return ready ? .connected : (current == .connected ? .connecting : current)
            }
        }
    }

    private func receive(_ data: Data, on id: Int32) {
        enum Route {
            case display, audio(expected: UInt32?), none
        }
        let route: Route = lock.withLock {
            if id == displayChannel {
                return .display
            }
            if id == audioTrack {
                return .audio(expected: expectedAudioSsrc)
            }
            return .none
        }
        switch route {
        case .display:
            receiveDisplay(data)
        case .audio(let expected):
            receiveRtp(data, expected: expected)
        case .none:
            break
        }
    }

    func receiveDisplay(_ data: Data) {
        guard !data.isEmpty, data.count <= Self.maxDisplayMessageBytes else { return }
        let accepted = lock.withLock { () -> Bool in
            guard !closeStarted && current != .closed && current != .failed else { return false }
            receivedDisplayPayloadBytes &+= UInt64(data.count)
            return true
        }
        if accepted { displayQueue.push(data) }
    }

    func receiveRtp(_ data: Data) {
        receiveRtp(data, expected: lock.withLock { expectedAudioSsrc })
    }

    private func receiveRtp(_ data: Data, expected: UInt32?) {
        guard data.count >= Self.minRtpBytes, data.count <= Self.maxRtpBytes else {
            lock.withLock {
                if !closeStarted && current != .closed && current != .failed { rejectedAudioHeaders += 1 }
            }
            return
        }
        let accepted = lock.withLock { () -> Bool in
            guard !closeStarted && current != .closed && current != .failed else { return false }
            receivedRtpBytes &+= UInt64(data.count)
            return true
        }
        guard accepted else { return }
        guard let packet = RtpPacket(parsing: data) else {
            lock.withLock { rejectedAudioHeaders += 1 }
            return
        }
        if let expected, packet.ssrc != expected {
            lock.withLock { foreignSsrcDropped += 1 }
            return
        }
        let format: AudioStreamFormat = packet.payloadType == L16Audio.payloadType ? .l16 : .opus
        audioQueue.push(packet, maxCount: Self.audioQueuePackets(for: format))
    }

    // MARK: State

    private func update(_ next: (State) -> State) {
        let changed: State? = lock.withLock {
            let proposed = next(current)
            guard proposed != current else {
                return nil
            }
            current = proposed
            return proposed
        }
        if let changed {
            Self.logger.notice("The media peer is now \(String(describing: changed), privacy: .public)")
            stateContinuation.yield(changed)
        }
    }

    /// Moves to a final state and ends the streams. The peer connection
    /// itself is deleted by ``close()`` or when the peer is released.
    private func finish(as final: State) {
        let changed: Bool = lock.withLock {
            guard current != .closed, current != .failed else {
                return false
            }
            current = final
            return true
        }
        guard changed else {
            return
        }
        Self.logger.notice("The media peer is now \(String(describing: final), privacy: .public)")
        stateContinuation.yield(final)
        stateContinuation.finish()
        microphoneClosedContinuation.finish()
        descriptionContinuation.finish()
        candidateContinuation.finish()
        audioQueue.finish()
        displayQueue.finish()
    }

    // MARK: SDP text

    /// Whether an SDP holds an `a=candidate` or `a=end-of-candidates` line.
    static func embedsCandidates(_ sdp: String) -> Bool {
        sdp.split(whereSeparator: \.isNewline).contains { line in
            line.hasPrefix("a=candidate:") || line.hasPrefix("a=end-of-candidates")
        }
    }

    /// An SDP with every `a=candidate` and `a=end-of-candidates` line taken
    /// out, line endings kept as CRLF.
    static func withoutCandidates(_ sdp: String) -> String {
        guard embedsCandidates(sdp) else {
            return sdp
        }
        let lines = sdp.components(separatedBy: "\r\n").filter { line in
            !line.hasPrefix("a=candidate:") && !line.hasPrefix("a=end-of-candidates")
        }
        return lines.joined(separator: "\r\n")
    }

    /// A candidate's type, the value after `typ` (RFC 8839 section 5.1).
    static func candidateType(_ candidate: String) -> String? {
        let fields = candidate.split(separator: " ")
        guard let index = fields.firstIndex(of: "typ"), index + 1 < fields.count else {
            return nil
        }
        return String(fields[index + 1])
    }

    /// This side's description of the microphone line from the one the
    /// offer gave it: send-only, the Opus payload type, the Core's L16 at
    /// 96 beside it when the offer carries it (R-IOS-09; a Core offers it
    /// only with lossless allowed), and `a=ssrc:<ssrc> cname:nereus-microphone`.
    /// Nil when the offer's line carries no Opus.
    static func microphoneAnswer(from offered: String, ssrc: UInt32) -> String? {
        let opus = String(opusPayloadType)
        let kept: Set<Substring> = describesL16(offered) ? [Substring(opus), Substring(String(L16Audio.payloadType))]
            : [Substring(opus)]
        var lines = offered.components(separatedBy: "\r\n").flatMap { $0.components(separatedBy: "\n") }
            .filter { !$0.isEmpty }
        guard let first = lines.first, first.hasPrefix("m="), describesOpus(offered) else {
            return nil
        }
        var fields = first.split(separator: " ").map(String.init)
        guard fields.count >= 4 else {
            return nil
        }
        fields = Array(fields.prefix(3)) + [opus] + (kept.count > 1 ? [String(L16Audio.payloadType)] : [])
        lines[0] = fields.joined(separator: " ")
        let directions: Set<String> = ["a=sendrecv", "a=recvonly", "a=sendonly", "a=inactive"]
        lines = lines.filter { line in
            if directions.contains(line) || line.hasPrefix("a=ssrc:") || line.hasPrefix("a=msid") {
                return false
            }
            for prefix in ["a=rtpmap:", "a=fmtp:", "a=rtcp-fb:"] where line.hasPrefix(prefix) {
                let type = line.dropFirst(prefix.count).prefix { $0 != " " }
                return kept.contains(type)
            }
            return true
        }
        let midIndex = lines.firstIndex { $0.hasPrefix("a=mid:") } ?? 0
        lines.insert("a=sendonly", at: midIndex + 1)
        lines.append("a=ssrc:\(ssrc) cname:\(microphoneStreamName)")
        return lines.joined(separator: "\r\n") + "\r\n"
    }

    /// Whether a media description maps the lossless payload type, 96, to
    /// L16/48000/2, as the Core's `describesLosslessAudio` reads it.
    static func describesL16(_ media: String) -> Bool {
        media.split(whereSeparator: \.isNewline).contains { line in
            line.lowercased().hasPrefix("a=rtpmap:\(L16Audio.payloadType) l16/48000/2")
        }
    }

    /// Whether a media description maps the Opus payload type to Opus.
    static func describesOpus(_ media: String) -> Bool {
        media.split(whereSeparator: \.isNewline).contains { line in
            line.lowercased().hasPrefix("a=rtpmap:\(opusPayloadType) opus/48000")
        }
    }
}

extension MediaPeer.Configuration: CustomStringConvertible {
    /// Never a relay's credentials: each server's `user:password@` is taken out.
    public var description: String {
        "MediaPeer.Configuration(iceServers: \(iceServers.map(RtcBridge.withoutCredentials)), mtu: \(mtu), "
            + "hostCandidatesOnly: \(hostCandidatesOnly), refusesRelayCandidates: \(refusesRelayCandidates), "
            + "onlyRouteCandidate: \(onlyRouteCandidate))"
    }
}

private final class MediaPeerCloseCompletion: @unchecked Sendable {
    private let lock = NSLock()
    private var done = false
    private var waiters: [CheckedContinuation<Void, Never>] = []

    func wait() async {
        await withCheckedContinuation { continuation in
            let complete = lock.withLock { () -> Bool in
                if done { return true }
                waiters.append(continuation)
                return false
            }
            if complete { continuation.resume() }
        }
    }

    func finish() {
        let pending = lock.withLock { () -> [CheckedContinuation<Void, Never>] in
            guard !done else { return [] }
            done = true
            defer { waiters.removeAll() }
            return waiters
        }
        for waiter in pending { waiter.resume() }
    }
}
