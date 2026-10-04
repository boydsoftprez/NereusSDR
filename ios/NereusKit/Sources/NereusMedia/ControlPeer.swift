// NereusSDR for iOS: the peer connection that carries the control session through the remote access service
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import os

/// The device's end of a control connection (link document section 20;
/// iPhone app plan Task 28a, R-IOS-16): a peer connection of its own
/// carrying one data channel, labelled `control`, reliable and ordered,
/// announced in band, made by this phone, which makes the offer.
///
/// - The offer holds one `m=application` line and no candidate, and goes out
///   in the introduction; the Core's answer must be the same shape.
/// - ICE is the service's (``IceSettings``): the peer is made with the one
///   STUN server, gathers nothing until ``gather(relays:)`` brings the relay
///   credentials the answer carried, and works to an MTU of 996 bytes.
/// - It is the session's ``ControlChannel``: each binary message goes out as
///   given, and what arrives reaches the transport's handler in order.
/// - ``remoteCertificateSHA256`` is the SHA-256 of the certificate the Core
///   presented in the DTLS handshake, as the DTLS verifier read it from that
///   certificate (ios/patches/libdatachannel/0002), never the fingerprint
///   the answer claims, which came through the service.
/// - Closing it deletes the connection, and with it each relay allocation it
///   holds is given back (ios/patches/libjuice/0001).
public final class ControlPeer: ControlChannel, @unchecked Sendable {
    /// The channel's label.
    public static let label = "control"
    /// The largest data-channel message the peer takes: one chunk.
    public static let maxMessageBytes = ControlChannelFraming.chunkBytes

    /// What the dial needs to hear, in order.
    public enum Signal: Sendable, Equatable {
        /// This side's offer, for the introduction.
        case offer(String)
        /// One of this side's candidates (`candidate:...`).
        case candidate(String)
        /// Every local candidate has been reported.
        case gatheringComplete
        /// The control channel opened.
        case opened
        /// The connection failed or closed before it opened, or later.
        case failed
    }

    public enum PeerError: Error, Equatable {
        /// The answer is not one control connection's description.
        case notAControlDescription
        /// The library refused a call; for the log.
        case library(String)
        /// The peer is closed.
        case closed
    }

    private static let logger = Logger(subsystem: "NereusSDR", category: "media.controlpeer")

    /// Everything the dial needs to hear, in order. Finishes when the peer
    /// closes.
    public let signals: AsyncStream<Signal>
    private let signalSink: AsyncStream<Signal>.Continuation
    private let onChannel: @Sendable (ControlChannelEvent) -> Void

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var ice: IceSettings
    private var bridge: RtcBridge?
    private var channel: Int32?
    private var channelOpen = false
    private var answerAccepted = false
    private var gatheringStarted = false
    private var remoteCandidates = 0
    private var admittedRelayCandidates = 0
    /// Where the Core's relay candidates are, so its relay learned as
    /// peer-reflexive still reads as a relayed path.
    private var farEndRelays: Set<String> = []
    private var observedFarEndRelays: Set<NumericRouteEndpoint> = []
    private var observationConnected = false
    private var relayContext: RelayRouteContext?
    private var relayClaim: RelayICEClaim?
    private var closed = false
    private let closeCompletion = PeerCloseCompletion()
    private let nativeLifetime = RtcPeerLifetime()
    private let closeSubmitted = PeerCloseCompletion()

    /// Makes the peer, its channel and its offer (``Signal/offer(_:)``).
    /// `onChannel` hears what the channel receives, from the library's
    /// threads, from the moment the peer exists.
    public init(ice: IceSettings, onChannel: @escaping @Sendable (ControlChannelEvent) -> Void) throws {
        self.ice = ice
        self.onChannel = onChannel
        (signals, signalSink) = AsyncStream.makeStream(of: Signal.self)
        MediaTransportLog.enable()
        let defaults = MediaPeer.Configuration()
        RtcBridge.applySctpSettingsOnce(sendBufferBytes: defaults.sctpSendBufferBytes,
                                        receiveBufferBytes: defaults.sctpReceiveBufferBytes)
        let settings = RtcPeerSettings(iceServers: ice.stunServerUrls, mtu: IceSettings.mtu,
                                       maxMessageSize: Self.maxMessageBytes, enableIceTcp: false,
                                       forceMediaTransport: false, disableAutoNegotiation: true,
                                       disableAutoGathering: true)
        let bridge: RtcBridge
        do {
            bridge = try RtcBridge(settings: settings, lifetime: nativeLifetime) { [weak self] event in
                self?.handle(event)
            }
        } catch {
            signalSink.finish()
            throw PeerError.library(String(describing: error))
        }
        lock.withLock { self.bridge = bridge }
        do {
            // Reliable and ordered: neither retransmission limit set.
            let channel = try bridge.createDataChannel(label: Self.label, unordered: false, maxRetransmits: nil)
            bridge.countTraffic(on: channel, as: .control)
            lock.withLock { self.channel = channel }
            try bridge.setLocalDescription(type: "offer")
        } catch {
            close()
            throw PeerError.library(String(describing: error))
        }
    }

    deinit {
        close()
    }

    // MARK: Signalling

    /// The Core's answer: one application line and no candidate, or it is
    /// refused.
    public func acceptAnswer(_ sdp: String) throws {
        guard Self.isControlDescription(sdp) else {
            throw PeerError.notAControlDescription
        }
        let bridge: RtcBridge = try lock.withLock {
            guard let bridge = self.bridge, !closed, !answerAccepted else {
                throw PeerError.closed
            }
            answerAccepted = true
            return bridge
        }
        do {
            try bridge.setRemoteDescription(sdp, type: "answer")
        } catch {
            throw PeerError.library(String(describing: error))
        }
    }

    /// Starts gathering, once, with `settings`' relay (the answer's
    /// credentials; none when the relay is not allowed or not offered).
    /// The settings stay this peer's for ``mediaIceSettings()``.
    public func gather(with settings: IceSettings) throws {
        let bridge: RtcBridge = try lock.withLock {
            guard let bridge = self.bridge, !closed, !gatheringStarted else {
                throw PeerError.closed
            }
            gatheringStarted = true
            ice = settings
            return bridge
        }
        do {
            try bridge.gatherLocalCandidates(relayServers: settings.relayAllowed ? settings.relayServerUrls : [])
        } catch {
            throw PeerError.library(String(describing: error))
        }
    }

    /// One of the Core's candidates (`candidate:...`, no `a=`): a relay one
    /// only while the relay is allowed, at most
    /// ``MediaPeer/maxRemoteCandidates``. False when it was not used.
    @discardableResult
    public func addRemoteCandidate(_ candidate: String) -> Bool {
        guard !candidate.isEmpty, candidate.utf8.count <= MediaPeer.maxCandidateBytes,
              !candidate.utf8.contains(0) else {
            return false
        }
        let bridge: RtcBridge? = lock.withLock {
            guard let bridge = self.bridge, !closed, answerAccepted, remoteCandidates < MediaPeer.maxRemoteCandidates,
                  ice.acceptsRemoteCandidate(candidate) else {
                return nil
            }
            remoteCandidates += 1
            if MediaPeer.candidateType(candidate) == "relay", let address = Self.address(of: candidate) {
                farEndRelays.insert(address)
            }
            return bridge
        }
        guard let bridge else {
            return false
        }
        let observedRelay = SelectedRouteObservation.farEndRelayEndpoint(from: candidate)
        do {
            try bridge.addRemoteCandidate(candidate, mid: "")
            lock.withLock {
                if candidate == relayClaim?.candidate { admittedRelayCandidates += 1 }
                if self.bridge === bridge, !closed, let endpoint = observedRelay {
                    observedFarEndRelays.insert(endpoint)
                }
            }
            return true
        } catch {
            Self.logger.info("A candidate from the Core could not be used")
            return false
        }
    }

    var admittedRelayCandidateCount: Int { lock.withLock { admittedRelayCandidates } }

    /// The claimed loopback candidate belongs to this same ICE agent. A
    /// successful peer keeps the context alive for its control lifetime.
    func attachRelay(_ context: RelayRouteContext, claim: RelayICEClaim) -> Bool {
        lock.withLock {
            guard !closed, relayClaim == nil else { return false }
            relayContext = context
            relayClaim = claim
            return true
        }
    }

    // MARK: What it settled on

    /// SHA-256 of the certificate the Core presented in the DTLS handshake
    /// (32 bytes), nil before the handshake or for another algorithm.
    public var remoteCertificateSHA256: Data? {
        guard let bridge = lock.withLock({ self.bridge }), let fingerprint = bridge.remoteFingerprint() else {
            return nil
        }
        return Self.sha256(fromFingerprint: fingerprint)
    }

    /// Whether the selected pair goes through a relay: either end's relay
    /// candidate, or the Core's relay learned as peer-reflexive. Nil before
    /// a pair is selected.
    public var selectedPathIsRelayed: Bool? {
        let (bridge, relays, claim) = lock.withLock { (self.bridge, farEndRelays, relayClaim) }
        guard let pair = bridge?.selectedCandidatePair() else {
            return nil
        }
        return Self.isRelayed(local: pair.local, remote: pair.remote, farEndRelays: relays)
            || Self.isWebRelay(pair: pair, claim: claim)
    }

    /// The pair ICE selected, as the library's candidate lines; nil before
    /// one is selected or once the peer is closed.
    public var selectedCandidateLines: (local: String, remote: String)? {
        lock.withLock { self.bridge }?.selectedCandidatePair()
    }

    /// Current service path rank from the selected pair. The claimed
    /// loopback endpoint is required for rank 4; an ordinary loopback pair
    /// remains direct ICE. Nil means ICE has selected no pair yet.
    public var selectedPathRank: Int? {
        let (bridge, relays, claim) = lock.withLock { (self.bridge, farEndRelays, relayClaim) }
        guard let pair = bridge?.selectedCandidatePair() else { return nil }
        if Self.isWebRelay(pair: pair, claim: claim) { return 4 }
        return Self.isRelayed(local: pair.local, remote: pair.remote, farEndRelays: relays) ? 3 : 2
    }

    /// Read only after this channel actually opened. Reading the retained
    /// bridge is followed by a current-bridge check, so retired ICE cannot
    /// be presented as this peer's fresh route.
    public var selectedRouteObservation: SelectedRouteObservation {
        let snapshot = lock.withLock { () -> (RtcBridge, Set<NumericRouteEndpoint>, RelayICEClaim?)? in
            guard !closed, channelOpen, observationConnected, let bridge else { return nil }
            return (bridge, observedFarEndRelays, relayClaim)
        }
        guard let (bridge, relays, claim) = snapshot else {
            return .unavailable(lock.withLock { closed ? .retired : .notReady })
        }
        let pair = bridge.selectedCandidatePair()
        let reading = SelectedRouteObservation.fromSelectedICEPair(
            local: pair?.local, remote: pair?.remote, knownFarEndRelays: relays,
            carrierClaim: claim.map { .webRelay(port: $0.port) })
        let stillCurrent = lock.withLock {
            !closed && channelOpen && observationConnected && self.bridge === bridge &&
                relayClaim == claim && observedFarEndRelays == relays
        }
        return stillCurrent ? reading : .unavailable(.retired)
    }

    /// The ICE settings the session's media connection uses: this
    /// connection's, without this end's relay when its path is direct
    /// (``IceSettings/forMedia(controlPathRelayed:)``).
    public func mediaIceSettings() -> IceSettings {
        mediaIceSettings(controlPathRelayed: selectedPathIsRelayed)
    }

    /// The media connection's ICE settings for a control path whose
    /// relaying is already known (read while the connection was open).
    public func mediaIceSettings(controlPathRelayed relayed: Bool?) -> IceSettings {
        let settings = lock.withLock { ice }
        return settings.forMedia(controlPathRelayed: relayed)
    }

    // MARK: ControlChannel

    public func send(_ frame: Data) -> Bool {
        let target: (RtcBridge, Int32)? = lock.withLock {
            guard let bridge, let channel, channelOpen, !closed else {
                return nil
            }
            return (bridge, channel)
        }
        guard let (bridge, channel) = target else {
            return false
        }
        do {
            try bridge.send(frame, on: channel)
            return true
        } catch {
            return false
        }
    }

    /// Closes the channel and the connection. The deletion runs off the
    /// caller's thread, since a close can come from inside one of the
    /// library's callbacks, where deleting the connection would wait on
    /// that same thread.
    public func close() {
        let closing: (RtcBridge?, RelayRouteContext?, RelayICEClaim?)? = lock.withLock {
            guard !closed else {
                return nil
            }
            closed = true
            defer {
                self.bridge = nil
                relayContext = nil
                relayClaim = nil
            }
            return (self.bridge, relayContext, relayClaim)
        }
        guard let (bridge, context, claim) = closing else { return }
        signalSink.finish()
        if let bridge {
            let completion = closeCompletion
            let nativeLifetime = nativeLifetime
            let submitted = closeSubmitted
            DispatchQueue.global(qos: .utility).async {
                bridge.close()
                submitted.finish()
                Task {
                    await nativeLifetime.wait()
                    if let context, let claim {
                        await context.releaseControl(claim)
                    }
                    completion.finish()
                }
            }
        } else {
            closeSubmitted.finish()
            closeCompletion.finish()
        }
    }

    /// Internal lifecycle observation for the held-teardown regression: the
    /// native handles are deleted, but the agent may still own its socket.
    func waitUntilNativeDeleteReturned() async {
        await closeSubmitted.wait()
    }

    /// Waits until libdatachannel has destroyed the old ICE agent and its
    /// persistent control claim has then been released. Safe from callers
    /// that might be handling a libdatachannel callback.
    public func closeWhenDeleted() async {
        close()
        await closeCompletion.wait()
    }

    // MARK: Library events

    private func handle(_ event: RtcEvent) {
        switch event {
        case .localDescription(let sdp, let type):
            guard type == "offer" else {
                return
            }
            guard Self.isControlDescription(sdp) else {
                Self.logger.warning("This phone's offer was not one control connection's description")
                signalSink.yield(.failed)
                return
            }
            signalSink.yield(.offer(sdp))
        case .localCandidate(let candidate, _):
            let value = candidate.hasPrefix("a=") ? String(candidate.dropFirst(2)) : candidate
            guard value.hasPrefix("candidate:"), value.utf8.count <= MediaPeer.maxCandidateBytes else {
                return
            }
            signalSink.yield(.candidate(value))
        case .gatheringComplete:
            signalSink.yield(.gatheringComplete)
        case .peerState(let state):
            switch state {
            case .connected:
                lock.withLock { if channelOpen { observationConnected = true } }
            case .disconnected:
                lock.withLock { observationConnected = false }
            case .failed, .closed:
                lock.withLock { observationConnected = false }
                signalSink.yield(.failed)
                if lock.withLock({ channelOpen }) {
                    onChannel(.closed)
                }
            default:
                break
            }
        case .dataChannel(let id):
            // The Core opens no channel of its own on a control connection.
            Self.logger.warning("The Core opened a data channel on the control connection; it was refused")
            lock.withLock { self.bridge }?.reject(id)
        case .track(let id):
            lock.withLock { self.bridge }?.reject(id)
        case .open(let id):
            let ours = lock.withLock { () -> Bool in
                guard id == channel, !channelOpen else {
                    return false
                }
                channelOpen = true
                observationConnected = true
                return true
            }
            if ours {
                signalSink.yield(.opened)
            }
        case .closed(let id), .error(let id, _):
            guard lock.withLock({ id == channel }) else {
                return
            }
            let wasOpen = lock.withLock { channelOpen }
            lock.withLock { observationConnected = false }
            signalSink.yield(.failed)
            if wasOpen {
                onChannel(.closed)
            }
        case .message(let id, let data):
            guard lock.withLock({ id == channel }) else {
                return
            }
            onChannel(.binary(data))
        case .textMessage(let id):
            guard lock.withLock({ id == channel }) else {
                return
            }
            onChannel(.text)
        }
    }

    // MARK: SDP and candidate text

    /// One `m=application` line and nothing else, and no candidate.
    static func isControlDescription(_ sdp: String) -> Bool {
        guard !sdp.isEmpty, sdp.utf8.count <= MediaPeer.maxDescriptionBytes, !sdp.utf8.contains(0),
              !MediaPeer.embedsCandidates(sdp) else {
            return false
        }
        let media = sdp.split(whereSeparator: \.isNewline).filter { $0.hasPrefix("m=") }
        return media.count == 1 && media[0].hasPrefix("m=application ")
    }

    /// `"sha-256 AB:CD:..."` as its 32 bytes; nil for anything else.
    static func sha256(fromFingerprint fingerprint: String) -> Data? {
        let parts = fingerprint.split(separator: " ")
        guard parts.count == 2, parts[0].lowercased() == "sha-256" else {
            return nil
        }
        let bytes = parts[1].split(separator: ":").compactMap { UInt8($0, radix: 16) }
        let groups = parts[1].split(separator: ":")
        guard bytes.count == 32, groups.count == 32, groups.allSatisfy({ $0.count == 2 }) else {
            return nil
        }
        return Data(bytes)
    }

    /// `address:port` of a candidate line, the address as written.
    static func address(of candidate: String) -> String? {
        let fields = candidate.split(separator: " ")
        guard fields.count >= 6 else {
            return nil
        }
        return "\(fields[4].lowercased()):\(fields[5])"
    }

    /// A relayed pair: a relay candidate at either end, or a remote
    /// peer-reflexive one at the address of one of the Core's relay
    /// candidates.
    static func isRelayed(local: String, remote: String, farEndRelays: Set<String>) -> Bool {
        if MediaPeer.candidateType(local) == "relay" || MediaPeer.candidateType(remote) == "relay" {
            return true
        }
        if MediaPeer.candidateType(remote) == "prflx", let address = address(of: remote) {
            return farEndRelays.contains(address)
        }
        return false
    }

    static func isWebRelay(pair: (local: String, remote: String), claim: RelayICEClaim?) -> Bool {
        guard let claim else { return false }
        // The claim is injected as a remote candidate into this peer's ICE
        // agent (Core DataChannelTransport.cpp, frozen 0b41e581, line 647).
        return address(of: pair.remote) == "127.0.0.1:\(claim.port)"
    }
}

/// A close may begin on a library callback and finish on a utility queue.
/// Waiters see completion only after that deletion and claim release.
private final class PeerCloseCompletion: @unchecked Sendable {
    private let lock = NSLock()
    private var finished = false
    private var waiters: [CheckedContinuation<Void, Never>] = []

    func wait() async {
        await withCheckedContinuation { continuation in
            let ready = lock.withLock { () -> Bool in
                if finished { return true }
                waiters.append(continuation)
                return false
            }
            if ready { continuation.resume() }
        }
    }

    func finish() {
        let pending = lock.withLock { () -> [CheckedContinuation<Void, Never>] in
            guard !finished else { return [] }
            finished = true
            defer { waiters.removeAll() }
            return waiters
        }
        for waiter in pending { waiter.resume() }
    }
}
