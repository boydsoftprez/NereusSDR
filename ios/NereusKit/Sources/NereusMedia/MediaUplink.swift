// NereusSDR for iOS: what the phone sends the Core on its media connection: the microphone line and the transmit keepalive
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The phone's way onto the current media connection for transmit (the
/// media control document, "Microphone line" and "The "tx" data channel";
/// link document section 18.7). ``MediaControlClient`` points it at each
/// connection that carries the microphone line and takes it away when the
/// connection ends, so the microphone and the keepalive always go on the
/// current one, and on none while there is none.
///
/// It is called straight from the capture's own queue and from the PTT's
/// keepalive, never through an actor hop, so packets leave in the order
/// they were encoded. Everything is guarded by one lock; `sendLock` keeps
/// each take-and-send whole, so a paced L16 packet and the capture's next
/// packet cannot pass each other.
public final class MediaUplink: @unchecked Sendable {
    /// The microphone line's frames: 20 ms at 48 kHz, the RTP timestamp step.
    public static let frameSamples: UInt32 = 960
    /// The "tx" channel's keepalive: byte 0 is 1, then the sequence (8
    /// bytes) and the epoch (4 bytes), both big-endian: 13 bytes.
    public static let keepaliveKind: UInt8 = 1
    public static let keepaliveBytes = 13
    /// The lossless microphone's 192-frame packets leave this far apart:
    /// the 4 ms of audio each carries, not five at once per 20 ms frame.
    public static let l16PacketSpacingNs: UInt64 = 4_000_000

    /// Runs `send` after `delayNs` nanoseconds, on one serial queue.
    public typealias Pacer = @Sendable (_ delayNs: UInt64, _ send: @escaping @Sendable () -> Void) -> Void

    private static let pacingQueue = DispatchQueue(label: "NereusSDR.media.uplink.pacer", qos: .userInteractive)

    /// The pacer the app runs: the uplink's own serial queue.
    public static let dispatchPacer: Pacer = { delayNs, send in
        pacingQueue.asyncAfter(deadline: .now() + .nanoseconds(Int(delayNs)), execute: send)
    }

    private let lock = NSLock()
    private let sendLock = NSLock()
    private let pacer: Pacer
    // L16 packets of the last frame still waiting their turn, and the turn
    // count that makes an older pacer callback a no-op.
    private var paced: [RtpPacket] = []
    private var paceEpoch = 0
    private var peer: (any MediaPeerConnection)?
    private var ssrc: UInt32 = 0
    private var sequence: UInt16 = 0
    private var timestamp: UInt32 = 0
    private var sent = 0
    private var quality = MicrophoneQuality.high
    private var fallback = false

    /// How this phone's microphone goes (R-IOS-09): the phone's audio
    /// quality choice, which the app sets.
    public enum MicrophoneQuality: Sendable, Equatable {
        /// Opus at 48 kbit/s, full band.
        case high
        /// Opus at 24 kbit/s.
        case saveData
        /// L16 where the line negotiated it and the link carries lossless,
        /// otherwise Opus at 48 kbit/s.
        case lossless
    }

    public init(pacer: @escaping Pacer = MediaUplink.dispatchPacer) {
        self.pacer = pacer
    }

    /// Points the uplink at a connection whose microphone line sends `ssrc`.
    /// The RTP sequence and timestamp start at random values (RFC 3550).
    func attach(_ peer: any MediaPeerConnection, microphoneSsrc: UInt32) {
        lock.withLock {
            self.peer = peer
            ssrc = microphoneSsrc
            sequence = UInt16.random(in: .min ... .max)
            timestamp = UInt32.random(in: .min ... .max)
        }
    }

    /// The Core maps the replacement SSRC into the same microphone stream.
    /// Keep RTP sequence and timestamp continuous across the peer handoff.
    func switchPeer(_ peer: any MediaPeerConnection, microphoneSsrc: UInt32) {
        lock.withLock {
            self.peer = peer
            ssrc = microphoneSsrc
        }
    }

    /// The connection ended: nothing goes out until the next one.
    func detach() {
        lock.withLock {
            peer = nil
            ssrc = 0
            paced = []
            paceEpoch &+= 1
        }
    }

    /// The current connection's microphone line is open.
    public var microphoneLineReady: Bool {
        let peer = lock.withLock { self.peer }
        return peer?.microphoneLineReady ?? false
    }

    /// The phone's audio quality choice, for the microphone.
    public var microphoneQuality: MicrophoneQuality {
        get { lock.withLock { quality } }
        set { lock.withLock { quality = newValue } }
    }

    /// The media client's lossless link trial failed on this session.
    public var losslessFallback: Bool {
        lock.withLock { fallback }
    }

    func setLosslessFallback(_ fallenBack: Bool) {
        lock.withLock { fallback = fallenBack }
    }

    /// The microphone goes as L16 now: Lossless chosen and not fallen back,
    /// on a line answered with L16 (the desktop's `sendMicAudio`).
    public var microphoneSendsLossless: Bool {
        let (peer, lossless) = lock.withLock { (self.peer, quality == .lossless && !fallback && ssrc != 0) }
        return lossless && (peer?.microphoneLosslessNegotiated ?? false)
    }

    /// The Opus encoder the microphone uses when it does not go as L16.
    public var microphoneOpusProfile: OpusEncoder.Profile {
        microphoneQuality == .saveData ? .microphoneSaveData : .microphone
    }

    /// The microphone packets sent since this uplink was made (diagnostics, tests).
    public var microphonePacketsSent: Int {
        lock.withLock { sent }
    }

    /// Sends one 20 ms Opus frame of the microphone on the line, as the
    /// next RTP packet (payload type 111, the line's SSRC). False when no
    /// line is open or the packet could not go.
    @discardableResult
    public func sendMicrophone(_ opus: Data) -> Bool {
        sendLock.withLock {
            let next: (any MediaPeerConnection, [RtpPacket], RtpPacket)? = lock.withLock {
                guard let peer, ssrc != 0 else {
                    return nil
                }
                let packet = RtpPacket(payloadType: MediaPeer.opusPayloadType, sequence: sequence,
                                       timestamp: timestamp, ssrc: ssrc, payload: opus)
                sequence &+= 1
                timestamp &+= Self.frameSamples
                return (peer, takePaced(), packet)
            }
            guard let next else {
                return false
            }
            _ = send(next.1, on: next.0)
            return send([next.2], on: next.0)
        }
    }

    /// Sends one 20 ms frame of the microphone as L16
    /// (``L16Audio/microphoneFrame(mono:)``): five packets of 192 frames at
    /// payload type 96, each the next RTP packet, the timestamp 192 on
    /// each. False when no line is open, the frame is not whole packets, or
    /// a packet could not go.
    @discardableResult
    public func sendMicrophoneL16(_ frame: Data) -> Bool {
        let size = L16Audio.payloadBytes
        guard !frame.isEmpty, frame.count % size == 0 else {
            return false
        }
        let count = frame.count / size
        let next: (any MediaPeerConnection, [RtpPacket])? = lock.withLock {
            guard let peer, ssrc != 0 else {
                return nil
            }
            var packets: [RtpPacket] = []
            for index in 0..<count {
                let start = frame.startIndex + index * size
                packets.append(RtpPacket(payloadType: L16Audio.payloadType, sequence: sequence,
                                         timestamp: timestamp, ssrc: ssrc,
                                         payload: Data(frame[start..<(start + size)])))
                sequence &+= 1
                timestamp &+= UInt32(L16Audio.packetFrames)
            }
            return (peer, packets)
        }
        guard let next else {
            return false
        }
        return sendLock.withLock {
            // What the last frame still had waiting goes now, then this
            // frame's first packet; the rest follow one spacing apart.
            let (late, epoch): ([RtpPacket], Int) = lock.withLock {
                let late = takePaced()
                paced = Array(next.1.dropFirst())
                return (late, paceEpoch)
            }
            _ = send(late, on: next.0)
            let first = send(Array(next.1.prefix(1)), on: next.0)
            if next.1.count > 1 { schedulePaced(epoch) }
            return first
        }
    }

    /// Takes what waits to be paced and ends its turns; called under `lock`.
    private func takePaced() -> [RtpPacket] {
        let waiting = paced
        paced = []
        paceEpoch &+= 1
        return waiting
    }

    private func schedulePaced(_ epoch: Int) {
        pacer(Self.l16PacketSpacingNs) { [weak self] in self?.sendPaced(epoch) }
    }

    private func sendPaced(_ epoch: Int) {
        sendLock.withLock {
            let next: (any MediaPeerConnection, RtpPacket, Bool)? = lock.withLock {
                guard epoch == paceEpoch, let peer, ssrc != 0, !paced.isEmpty else {
                    return nil
                }
                let packet = paced.removeFirst()
                return (peer, packet, !paced.isEmpty)
            }
            guard let next else {
                return
            }
            _ = send([next.1], on: next.0)
            if next.2 { schedulePaced(epoch) }
        }
    }

    /// Sends each packet; false when any could not go. Called under `sendLock`.
    private func send(_ packets: [RtpPacket], on peer: any MediaPeerConnection) -> Bool {
        var all = true
        for packet in packets {
            do {
                try peer.sendMicrophone(packet)
                lock.withLock { sent += 1 }
            } catch {
                all = false
            }
        }
        return all
    }

    /// Sends one transmit keepalive on the "tx" data channel. False when the
    /// channel is not open, so the keepalive goes on the session instead.
    public func sendKeepalive(sequence: Int64, epoch: Int64) -> Bool {
        let peer = lock.withLock { self.peer }
        guard let peer, peer.txChannelReady else {
            return false
        }
        do {
            try peer.sendTx(Self.keepaliveMessage(sequence: sequence, epoch: epoch))
            return true
        } catch {
            return false
        }
    }

    /// The keepalive as the "tx" channel carries it.
    public static func keepaliveMessage(sequence: Int64, epoch: Int64) -> Data {
        var bytes = [keepaliveKind]
        let wideSequence = UInt64(bitPattern: sequence)
        for shift in stride(from: 56, through: 0, by: -8) {
            bytes.append(UInt8((wideSequence >> UInt64(shift)) & 0xff))
        }
        let narrowEpoch = UInt32(truncatingIfNeeded: epoch)
        for shift in stride(from: 24, through: 0, by: -8) {
            bytes.append(UInt8((narrowEpoch >> UInt32(shift)) & 0xff))
        }
        return Data(bytes)
    }
}
