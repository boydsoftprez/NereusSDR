// NereusSDR for iOS: a media peer the media control tests play, with no network under it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
@testable import NereusMedia

/// Stands in for ``MediaPeer`` under ``MediaControlClient``: it records what
/// the client hands it and lets the test produce the answer, candidates,
/// state changes and received media.
final class ScriptedMediaPeer: MediaPeerConnection, @unchecked Sendable {
    let localDescription: AsyncStream<String>
    let localCandidates: AsyncStream<(candidate: String, mid: String)>
    let audioPackets: AsyncStream<RtpPacket>
    let displayDatagrams: AsyncStream<Data>
    let state: AsyncStream<MediaPeer.State>
    let microphoneLineClosed: AsyncStream<Void>

    private let descriptionSink: AsyncStream<String>.Continuation
    private let candidateSink: AsyncStream<(candidate: String, mid: String)>.Continuation
    private let audioSink: AsyncStream<RtpPacket>.Continuation
    private let displaySink: AsyncStream<Data>.Continuation
    private let stateSink: AsyncStream<MediaPeer.State>.Continuation
    private let microphoneClosedSink: AsyncStream<Void>.Continuation

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var descriptions: [String] = []
    private var candidates: [(candidate: String, mid: String)] = []
    private var ssrc: UInt32??
    private var closedByClient = false
    private var microphoneSsrc: UInt32?
    private var microphoneOpen = false
    private var microphone: [RtpPacket] = []
    private var tx: [Data] = []
    private let prepareHook: (@Sendable (UUID, MediaFeatureGates) async throws -> Void)?
    private var preparedId: UUID?
    private var selectedCarrier = false
    private var tunnelSelected = false
    private var routeReading: SelectedRouteObservation = .unavailable(.noSelectedPair)
    private var trafficReading: MediaTrafficObservation?

    init(prepareHook: (@Sendable (UUID, MediaFeatureGates) async throws -> Void)? = nil) {
        self.prepareHook = prepareHook
        (localDescription, descriptionSink) = AsyncStream.makeStream(of: String.self)
        (localCandidates, candidateSink) = AsyncStream.makeStream(of: (candidate: String, mid: String).self)
        (audioPackets, audioSink) = AsyncStream.makeStream(of: RtpPacket.self)
        (displayDatagrams, displaySink) = AsyncStream.makeStream(of: Data.self)
        (state, stateSink) = AsyncStream.makeStream(of: MediaPeer.State.self)
        (microphoneLineClosed, microphoneClosedSink) = AsyncStream.makeStream(of: Void.self)
    }

    // MARK: What the client does

    func prepare(connectionId: UUID, gates: MediaFeatureGates) async throws {
        try await prepareHook?(connectionId, gates)
        lock.withLock { preparedId = connectionId }
    }

    var preparedConnectionId: UUID? { lock.withLock { preparedId } }
    var selectedRelayOrTunnel: Bool { lock.withLock { selectedCarrier } }
    func selectCarrier(_ selected: Bool) { lock.withLock { selectedCarrier = selected } }
    var selectedTunnel: Bool { lock.withLock { tunnelSelected } }
    /// ICE nominated the Core's tunnel (a carrier as well).
    func selectTunnel(_ selected: Bool) {
        lock.withLock {
            tunnelSelected = selected
            selectedCarrier = selected
        }
    }
    var selectedRouteObservation: SelectedRouteObservation { lock.withLock { routeReading } }
    func observe(_ reading: SelectedRouteObservation) { lock.withLock { routeReading = reading } }
    var trafficObservation: MediaTrafficObservation? { lock.withLock { trafficReading } }
    func observeTraffic(_ reading: MediaTrafficObservation) { lock.withLock { trafficReading = reading } }

    func setRemoteDescription(_ sdp: String) throws {
        lock.withLock { descriptions.append(sdp) }
    }

    func addRemoteCandidate(_ candidate: String, mid: String) throws {
        lock.withLock { candidates.append((candidate, mid)) }
    }

    func setExpectedAudioSsrc(_ ssrc: UInt32?) {
        lock.withLock { self.ssrc = .some(ssrc) }
    }

    func enableMicrophoneLine(ssrc: UInt32) {
        lock.withLock { microphoneSsrc = ssrc }
    }

    var microphoneLineReady: Bool { lock.withLock { microphoneOpen && !closedByClient } }

    var txChannelReady: Bool { microphoneLineReady }

    func sendMicrophone(_ packet: RtpPacket) throws {
        try lock.withLock {
            guard microphoneOpen, !closedByClient, packet.ssrc == microphoneSsrc else {
                throw MediaPeerError.microphoneNotReady
            }
            microphone.append(packet)
        }
    }

    func sendTx(_ message: Data) throws {
        try lock.withLock {
            guard microphoneOpen, !closedByClient else {
                throw MediaPeerError.txChannelNotReady
            }
            tx.append(message)
        }
    }

    func close() {
        lock.withLock { closedByClient = true }
        descriptionSink.finish()
        candidateSink.finish()
        audioSink.finish()
        displaySink.finish()
        stateSink.finish()
        microphoneClosedSink.finish()
    }

    var remoteDescriptions: [String] { lock.withLock { descriptions } }
    var remoteCandidates: [(candidate: String, mid: String)] { lock.withLock { candidates } }
    /// The last expected SSRC the client set; nil when it never set one.
    var expectedSsrc: UInt32?? { lock.withLock { ssrc } }
    var isClosed: Bool { lock.withLock { closedByClient } }
    /// The microphone line's SSRC the client asked for, and what it sent.
    var requestedMicrophoneSsrc: UInt32? { lock.withLock { microphoneSsrc } }
    var microphonePackets: [RtpPacket] { lock.withLock { microphone } }
    var txMessages: [Data] { lock.withLock { tx } }

    // MARK: What the test makes happen

    func answer(_ sdp: String) {
        descriptionSink.yield(sdp)
    }

    func trickle(_ candidate: String, mid: String) {
        candidateSink.yield((candidate, mid))
    }

    /// The Core's offer carried the microphone line and it opened.
    func openMicrophoneLine() {
        lock.withLock { microphoneOpen = microphoneSsrc != nil }
    }

    /// The Core closed the microphone line's track; the connection stays up.
    func closeMicrophoneLine() {
        lock.withLock { microphoneOpen = false }
        microphoneClosedSink.yield()
    }

    func become(_ next: MediaPeer.State) {
        stateSink.yield(next)
    }

    func receiveAudio(_ packet: RtpPacket) {
        audioSink.yield(packet)
    }

    func receiveDisplay(_ datagram: Data) {
        displaySink.yield(datagram)
    }
}
