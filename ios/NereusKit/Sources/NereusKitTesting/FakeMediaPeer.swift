// NereusSDR for iOS: the fake Core's end of a media connection: up at once, carrying the display rows it sends
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMedia

/// A media peer for the app's tests against ``FakeStation``: it reports
/// itself connected as soon as the media control client makes it, needs no
/// offer or candidates, and delivers the display datagrams the fake Core
/// sends. Nothing goes over a network. Asked for the microphone line, it
/// carries it at once, with its "tx" channel, and keeps what the app sends
/// on them.
public final class FakeMediaPeer: MediaPeerConnection, @unchecked Sendable {
    public let localDescription: AsyncStream<String>
    public let localCandidates: AsyncStream<(candidate: String, mid: String)>
    public let audioPackets: AsyncStream<RtpPacket>
    public let displayDatagrams: AsyncStream<Data>
    public let state: AsyncStream<MediaPeer.State>
    public let microphoneLineClosed: AsyncStream<Void>

    private let displaySink: AsyncStream<Data>.Continuation
    private let stateSink: AsyncStream<MediaPeer.State>.Continuation
    private let microphoneClosedSink: AsyncStream<Void>.Continuation
    /// The fake Core closed the microphone line's track (``closeMicrophoneLine()``).
    private var microphoneClosedByCore = false
    private let lock = NSLock()
    private var closed = false
    private var microphoneSsrc: UInt32?
    private var microphone: [RtpPacket] = []
    private var microphoneLossless = false
    private var tx: [Data] = []
    private let trafficLifetime = UUID()
    private var receivedDisplayPayloadBytes: UInt64 = 0
    private var submittedRtpBytes: UInt64 = 0
    private var submittedTxChannelBytes: UInt64 = 0

    public var trafficObservation: MediaTrafficObservation? {
        lock.withLock {
            MediaTrafficObservation(lifetime: trafficLifetime, active: !closed,
                receivedDisplayPayloadBytes: receivedDisplayPayloadBytes, receivedRtpBytes: 0,
                submittedRtpBytes: submittedRtpBytes, submittedTxChannelBytes: submittedTxChannelBytes)
        }
    }

    public init() {
        localDescription = AsyncStream { _ in }
        localCandidates = AsyncStream { _ in }
        audioPackets = AsyncStream { _ in }
        (displayDatagrams, displaySink) = AsyncStream.makeStream(of: Data.self)
        (state, stateSink) = AsyncStream.makeStream(of: MediaPeer.State.self)
        (microphoneLineClosed, microphoneClosedSink) = AsyncStream.makeStream(of: Void.self)
        stateSink.yield(.connected)
    }

    public func setRemoteDescription(_ sdp: String) throws {}
    public func addRemoteCandidate(_ candidate: String, mid: String) throws {}
    public func setExpectedAudioSsrc(_ ssrc: UInt32?) {}

    public func enableMicrophoneLine(ssrc: UInt32) {
        lock.withLock { microphoneSsrc = ssrc == 0 ? nil : ssrc }
    }

    public var microphoneLineReady: Bool {
        lock.withLock { microphoneSsrc != nil && !closed && !microphoneClosedByCore }
    }

    public var txChannelReady: Bool {
        microphoneLineReady
    }

    public func sendMicrophone(_ packet: RtpPacket) throws {
        try lock.withLock {
            guard let microphoneSsrc, !closed, !microphoneClosedByCore, packet.ssrc == microphoneSsrc else {
                throw MediaPeerError.microphoneNotReady
            }
            microphone.append(packet)
            submittedRtpBytes &+= UInt64(packet.bytes.count)
        }
    }

    public func sendTx(_ message: Data) throws {
        try lock.withLock {
            guard microphoneSsrc != nil, !closed, !microphoneClosedByCore else {
                throw MediaPeerError.txChannelNotReady
            }
            tx.append(message)
            submittedTxChannelBytes &+= UInt64(message.count)
        }
    }

    /// The microphone's packets and the "tx" channel's messages the app sent.
    public var microphonePackets: [RtpPacket] { lock.withLock { microphone } }
    public var txMessages: [Data] { lock.withLock { tx } }
    /// The microphone line's SSRC the app asked for, if it asked.
    public var requestedMicrophoneSsrc: UInt32? { lock.withLock { microphoneSsrc } }

    public func close() {
        lock.withLock { closed = true }
        displaySink.finish()
        stateSink.finish()
        microphoneClosedSink.finish()
    }

    /// The fake Core closes the microphone line's track; the connection
    /// stays up (``MediaPeer`` reports it the same way).
    public func closeMicrophoneLine() {
        let open = lock.withLock { () -> Bool in
            let open = microphoneSsrc != nil && !closed && !microphoneClosedByCore
            microphoneClosedByCore = true
            return open
        }
        if open { microphoneClosedSink.yield() }
    }

    /// Whether the microphone line carries lossless (L16) besides Opus: the
    /// fake Core offered it and the app's answer kept it.
    public var microphoneLosslessNegotiated: Bool {
        get { lock.withLock { microphoneLossless } }
        set { lock.withLock { microphoneLossless = newValue } }
    }

    /// Whether the client has closed this peer.
    public var isClosed: Bool { lock.withLock { closed } }

    /// One display datagram from the fake Core.
    func send(_ datagram: Data) {
        let active = lock.withLock { () -> Bool in
            guard !closed else { return false }
            receivedDisplayPayloadBytes &+= UInt64(datagram.count)
            return true
        }
        if active { displaySink.yield(datagram) }
    }
}
