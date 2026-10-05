// NereusSDR for iOS: holds the Core's Opus packets until they are due, and fills in the ones that never come
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The receive buffer for the Core's audio: 48 kHz stereo Opus, one packet
/// of 1920 frames (40 ms) per RTP sequence number. Packets go in as they
/// arrive, in any order; ``pull()`` is called once per 40 ms of playback
/// and gives the next packet's audio, a concealment of a lost one, or
/// silence while the buffer fills.
///
/// Its rules, which the tests work their expected counts out from:
///
/// - Nothing is taken before ``reanchor(_:)`` names a stream. Each new
///   audio context re-anchors: the held packets go, the playout position
///   moves to the context's first sequence number, and only its SSRC is
///   taken from then on.
/// - Depth is the audio from the playout position to the newest packet
///   held, lost packets in between included, in whole packets of 40 ms.
/// - Filling: a pull gives silence until the depth reaches the target
///   (180 ms to start with), then playing begins with that pull.
/// - Playing: a pull plays the packet at the playout position. When that
///   packet is missing but a later one is held, the decoder conceals it
///   (`concealed`). When nothing is held at all, the pull is an underrun
///   (`underruns`): it gives silence, the buffer fills again, and the target
///   rises by 40 ms, to at most 400 ms.
/// - After 250 pulls (10 s) of playing without an underrun the target falls
///   by 20 ms, to no less than 80 ms.
/// - A packet of the anchored stream whose payload type is not its
///   format's (Opus's 111, or the lossless profile's L16 at 96) is dropped
///   before the decoder (`otherPayloadDrops`).
/// - An L16 stream (R-IOS-09) is held the same way in packets of 4 ms: a
///   pull plays ten of them, and one that never came is 4 ms of silence
///   (`concealed`, once for each). The limits are the same times: 32
///   pulls ahead is 320 packets, and the duplicate history ten times as
///   long.
/// - A packet behind the playout position is dropped (`lateDrops`), and so
///   is a duplicate (uncounted). A packet more than 32 packets ahead moves
///   the playout position up to it, dropping what it passes (`lateDrops`).
/// - A pull that finds more than 400 ms held first drops the oldest audio
///   (`lateDrops` for each packet held) down to the target, rounded up to
///   whole packets.
///
/// Not thread-safe: one serial context pushes, pulls and re-anchors.
public final class AudioJitterBuffer {
    public static let sampleRate = 48000
    public static let channels = 2
    /// One packet's frames: 40 ms at 48 kHz.
    public static let packetFrames = 1920
    public static let packetMs = 40
    public static let initialTargetMs = 180
    public static let minimumTargetMs = 80
    public static let maximumTargetMs = 400
    /// How far the target rises after an underrun, and falls after a quiet spell.
    public static let targetRiseMs = 40
    public static let targetFallMs = 20
    /// Playing pulls without an underrun before the target falls: 10 s.
    public static let pullsBeforeFalling = 250
    /// The furthest ahead a packet is held, in packets.
    public static let aheadLimitPackets = 32
    /// The payload type the Core's Opus packets carry.
    public static let opusPayloadType: UInt8 = 111
    /// L16 packets in one pull.
    static let l16PacketsPerPull = packetFrames / L16Audio.packetFrames

    /// One pull's audio: interleaved stereo, ``packetFrames`` frames.
    public enum Block: Equatable, Sendable {
        case audio([Float])
        case concealed([Float])
        case silence
    }

    public struct Observation: Sendable, Equatable {
        public var receivedContentBytes: UInt64 = 0
        public var acceptedPackets: UInt64 = 0
        public var decodedPackets: UInt64 = 0
        public var concealedIntervals: UInt64 = 0
        public var latePackets: UInt64 = 0
        public var duplicatePackets: UInt64 = 0
        public var invalidPackets: UInt64 = 0
        public var rejectedProfilePackets: UInt64 = 0
        public var rejectedSsrcPackets: UInt64 = 0
        public var startDiscardedPackets: UInt64 = 0
        public var trimmedPackets: UInt64 = 0
        public var skippedIntervals: UInt64 = 0
        public var streamGapEvents: UInt64 = 0
        public var linkInterruptionEvents: UInt64 = 0
        public var underflowEvents: UInt64 = 0
        public var overflowEvents: UInt64 = 0
        public var lastAdmittedPacketNs: Int64?
        public var release: AudioReleasePoint?
        public var reception = AudioReceptionStats()
        public var receptionAvailable = true
        public var reorderQueuedMs: Int?
        public var timelineDepthMs: Int?
        public var adaptiveTargetMs: Int?
    }

    public private(set) var observation = Observation()
    /// Valid end timestamp from this pull alone; nil for repair or silence.
    public private(set) var lastPullRelease: AudioReleasePoint?

    public private(set) var underruns = 0
    public private(set) var lateDrops = 0
    public private(set) var concealed = 0
    /// Packets of the anchored stream with a payload type other than Opus's.
    public private(set) var otherPayloadDrops = 0
    /// The sequence consumed on the last non-silent pull (test/diagnostics).
    public private(set) var lastPulledSequence: UInt16?
    /// The depth playing waits for, in ms.
    public private(set) var targetMs = AudioJitterBuffer.initialTargetMs
    /// The stream being played, or nil while audio is off.
    public private(set) var anchor: AudioStreamAnchor?
    public private(set) var isPlaying = false

    private var decoder: OpusDecoder
    private var cursor: UInt16 = 0
    private struct HeldPacket {
        let payload: Data
        let timestamp: UInt32
        let shapeValid: Bool
        let arrivalNs: Int64
    }
    private var held: [UInt16: HeldPacket] = [:]
    private var pullsWithoutUnderrun = 0
    /// A replacement peer carries the same semantic stream with a new SSRC.
    /// Only the feed queue mutates these values.
    private var equivalentSsrcs: Set<UInt32> = []
    private var recentTimestamps: [UInt32] = []
    public static let duplicateHistory = 64
    private var replacementActive = false
    private var pendingEaseShed = false
    private var startedOnce = false
    private struct ReceptionEvent {
        let sequence: UInt16
        let timestamp: UInt32
        let arrivalNs: Int64
    }
    private var pendingLateReception: [ReceptionEvent] = []
    private static let maximumPendingReception = 64

    public init() throws {
        decoder = try OpusDecoder(channels: Self.channels)
    }

    /// Held audio from the playout position to the newest packet, in ms.
    public var depthMs: Int {
        depthPackets * Self.packetMs / packetsPerPull
    }

    /// The anchored stream is the lossless profile's L16.
    private var isL16: Bool {
        anchor?.format == .l16
    }

    /// Packets in one 40 ms pull: 1 for Opus, 10 for L16.
    private var packetsPerPull: Int {
        isL16 ? Self.l16PacketsPerPull : 1
    }

    /// The target depth in whole packets, rounded up.
    private var targetPackets: Int {
        (targetMs * packetsPerPull + Self.packetMs - 1) / Self.packetMs
    }

    /// Starts over on a new stream, or stops taking packets with nil.
    @discardableResult public func reanchor(_ anchor: AudioStreamAnchor?) -> Observation {
        if !startedOnce { observation.startDiscardedPackets += UInt64(held.count) }
        let retired = observation
        self.anchor = anchor
        observation = Observation()
        lastPullRelease = nil
        held.removeAll()
        isPlaying = false
        pullsWithoutUnderrun = 0
        cursor = anchor?.firstSequence ?? 0
        equivalentSsrcs = Set(anchor.map { [$0.ssrc] } ?? [])
        recentTimestamps.removeAll()
        startedOnce = false
        pendingLateReception.removeAll()
        // A new stream starts a new decoder history; if a decoder cannot be
        // had the old one carries on, which only colours the first packet.
        if let fresh = try? OpusDecoder(channels: Self.channels) {
            decoder = fresh
        }
        if anchor != nil { refreshGauges() }
        return retired
    }

    /// Adds a replacement's SSRC without resetting the decoder, held audio,
    /// or playout position. The Core duplicates RTP timestamps during overlap.
    public func acceptEquivalentSsrc(_ ssrc: UInt32) {
        guard anchor != nil else { return }
        equivalentSsrcs.insert(ssrc)
    }

    public func acceptsEquivalentSsrc(_ ssrc: UInt32) -> Bool {
        anchor != nil && equivalentSsrcs.contains(ssrc)
    }

    /// After Core acknowledgement, the new context names a different SSRC
    /// for the same stream. Preserve the single playout clock.
    public func adoptEquivalentAnchor(_ next: AudioStreamAnchor) {
        guard anchor != nil, equivalentSsrcs.contains(next.ssrc) else {
            reanchor(next)
            return
        }
        anchor = next
    }

    /// Ends the old path's drain while keeping the new queue and clock.
    public func keepOnlySsrc(_ ssrc: UInt32) {
        equivalentSsrcs = anchor == nil ? [] : [ssrc]
    }

    /// During a dual-path transition, the merge releases packets earlier
    /// by one 40 ms interval every 2000 ms. Pair each such step with at
    /// most one buffered packet removal, rather than a bulk depth trim.
    public func beginReplacement() {
        replacementActive = true
        pendingEaseShed = false
    }

    public func replacementLeadEased() {
        if replacementActive { pendingEaseShed = true }
    }

    public func endReplacement() {
        replacementActive = false
        pendingEaseShed = false
    }

    /// Takes one packet of the anchored stream.
    public func push(_ packet: RtpPacket) {
        push(packet, arrivalNs: Int64(DispatchTime.now().uptimeNanoseconds))
    }

    public func push(_ packet: RtpPacket, arrivalNs: Int64) {
        guard anchor != nil, equivalentSsrcs.contains(packet.ssrc) else {
            if anchor != nil { observation.rejectedSsrcPackets += 1 }
            return
        }
        guard packet.payloadType == (isL16 ? L16Audio.payloadType : Self.opusPayloadType) else {
            otherPayloadDrops += 1
            observation.rejectedProfilePackets += 1
            return
        }
        guard !packet.payload.isEmpty else {
            observation.invalidPackets += 1
            return
        }
        observation.receivedContentBytes += UInt64(packet.payload.count)
        let shapeValid = isL16 ? packet.payload.count == L16Audio.payloadBytes
            : AudioPlaybackCodecDelay.hasValidPacketShape(packet.payload)
        let offset = Self.offset(of: packet.sequence, from: cursor)
        if offset < 0 {
            lateDrops += 1
            if shapeValid {
                observation.latePackets += 1
                if startedOnce {
                    observation.reception.observe(sequence: packet.sequence, timestamp: packet.timestamp,
                                                  arrivalNs: arrivalNs)
                } else if pendingLateReception.count < Self.maximumPendingReception {
                    pendingLateReception.append(ReceptionEvent(sequence: packet.sequence,
                                                                 timestamp: packet.timestamp, arrivalNs: arrivalNs))
                } else { observation.receptionAvailable = false }
            } else { observation.invalidPackets += 1 }
            return
        }
        let aheadLimit = Self.aheadLimitPackets * packetsPerPull
        if offset >= aheadLimit {
            observation.streamGapEvents += 1
            observation.linkInterruptionEvents += 1
            advance(by: offset - aheadLimit + 1)
        }
        guard !recentTimestamps.contains(packet.timestamp) else {
            observation.duplicatePackets += 1
            return
        }
        if held[packet.sequence] == nil {
            held[packet.sequence] = HeldPacket(payload: packet.payload, timestamp: packet.timestamp,
                                               shapeValid: shapeValid, arrivalNs: arrivalNs)
            recentTimestamps.append(packet.timestamp)
            if recentTimestamps.count > Self.duplicateHistory * packetsPerPull {
                recentTimestamps.removeFirst()
            }
            if startedOnce { observation.acceptedPackets += 1 }
            if shapeValid {
                if startedOnce { observation.lastAdmittedPacketNs = arrivalNs }
                if startedOnce {
                    observation.reception.observe(sequence: packet.sequence, timestamp: packet.timestamp,
                                                  arrivalNs: arrivalNs)
                }
            } else { observation.invalidPackets += 1 }
        } else {
            observation.duplicatePackets += 1
        }
        refreshGauges()
    }

    /// The next 40 ms of playback.
    public func pull() -> Block {
        pull(releasedNs: Int64(DispatchTime.now().uptimeNanoseconds))
    }

    public func pull(releasedNs: Int64) -> Block {
        lastPulledSequence = nil
        lastPullRelease = nil
        observation.release = nil
        guard anchor != nil else {
            return .silence
        }
        if replacementActive, pendingEaseShed, isPlaying,
           depthPackets > targetPackets,
           held[cursor] != nil {
            // One 40 ms interval: a packet of Opus, ten of L16.
            advance(by: packetsPerPull)
            pendingEaseShed = false
        } else if !replacementActive && depthMs > Self.maximumTargetMs {
            observation.overflowEvents += 1
            advance(by: depthPackets - targetPackets)
        }
        if !isPlaying {
            guard depthMs >= targetMs else {
                refreshGauges()
                return .silence
            }
            isPlaying = true
            if !startedOnce {
                observation.acceptedPackets += UInt64(held.count)
                observation.lastAdmittedPacketNs = held.values.filter(\.shapeValid).map(\.arrivalNs).max()
                let accepted = held.map { sequence, packet in
                    ReceptionEvent(sequence: sequence, timestamp: packet.timestamp, arrivalNs: packet.arrivalNs)
                }
                let ordered = (pendingLateReception + accepted.filter { event in
                    held[event.sequence]?.shapeValid == true
                }).sorted {
                    ($0.arrivalNs, $0.sequence) < ($1.arrivalNs, $1.sequence)
                }
                for event in ordered {
                    observation.reception.observe(sequence: event.sequence, timestamp: event.timestamp,
                                                  arrivalNs: event.arrivalNs)
                }
                pendingLateReception.removeAll()
                startedOnce = true
            }
        }
        if held.isEmpty {
            underruns += 1
            observation.underflowEvents += 1
            observation.linkInterruptionEvents += 1
            isPlaying = false
            pullsWithoutUnderrun = 0
            targetMs = min(targetMs + Self.targetRiseMs, Self.maximumTargetMs)
            refreshGauges()
            return .silence
        }
        let block: Block
        if isL16 {
            block = pullL16(releasedNs: releasedNs)
        } else {
            block = pullOpus(releasedNs: releasedNs)
        }
        pullsWithoutUnderrun += 1
        if pullsWithoutUnderrun >= Self.pullsBeforeFalling {
            pullsWithoutUnderrun = 0
            targetMs = max(targetMs - Self.targetFallMs, Self.minimumTargetMs)
        }
        refreshGauges()
        return block
    }

    /// One Opus packet's 40 ms, decoded or concealed.
    private func pullOpus(releasedNs: Int64) -> Block {
        let block: Block
        lastPulledSequence = cursor
        let packet = held.removeValue(forKey: cursor)
        if let packet, let pcm = try? decoder.decode(packet.payload) {
            block = .audio(Self.oneBlock(pcm))
            observation.decodedPackets += 1
            if packet.shapeValid {
                let point = AudioReleasePoint(rtpTimestamp: packet.timestamp &+ UInt32(Self.packetFrames),
                                              releasedNs: releasedNs)
                lastPullRelease = point
                observation.release = point
            }
        } else {
            // Lost, or a packet the decoder could not read: the same repair.
            concealed += 1
            observation.concealedIntervals += 1
            if let packet, packet.shapeValid { observation.invalidPackets += 1 }
            let pcm = (try? decoder.concealLoss(frames: Self.packetFrames)) ?? []
            block = .concealed(Self.oneBlock(pcm))
        }
        cursor &+= 1
        return block
    }

    /// Ten L16 packets' 40 ms. A packet that never came, or one not of the
    /// L16 shape, is 4 ms of silence, as the desktop plays it.
    private func pullL16(releasedNs: Int64) -> Block {
        var samples = [Float](repeating: 0, count: Self.packetFrames * Self.channels)
        let slotSamples = L16Audio.packetFrames * L16Audio.channels
        var repaired = false
        var lastRelease: AudioReleasePoint?
        for slot in 0..<Self.l16PacketsPerPull {
            lastPulledSequence = cursor
            let packet = held.removeValue(forKey: cursor)
            if let packet, packet.shapeValid, let pcm = L16Audio.decode(packet.payload) {
                samples.replaceSubrange((slot * slotSamples)..<((slot + 1) * slotSamples), with: pcm)
                observation.decodedPackets += 1
                lastRelease = slot == Self.l16PacketsPerPull - 1
                    ? AudioReleasePoint(rtpTimestamp: packet.timestamp &+ UInt32(L16Audio.packetFrames),
                                        releasedNs: releasedNs)
                    : nil
            } else {
                repaired = true
                concealed += 1
                observation.concealedIntervals += 1
                lastRelease = nil
            }
            cursor &+= 1
        }
        if let lastRelease {
            lastPullRelease = lastRelease
            observation.release = lastRelease
        }
        return repaired ? .concealed(samples) : .audio(samples)
    }

    // MARK: Helpers

    private var depthPackets: Int {
        held.keys.map { Self.offset(of: $0, from: cursor) + 1 }.max() ?? 0
    }

    /// Moves the playout position `count` packets on, dropping what it passes.
    private func advance(by count: Int) {
        guard count > 0 else {
            return
        }
        for _ in 0..<count {
            if held.removeValue(forKey: cursor) != nil {
                lateDrops += 1
                if startedOnce { observation.trimmedPackets += 1 }
                else { observation.startDiscardedPackets += 1 }
            }
            cursor &+= 1
            if startedOnce { observation.skippedIntervals += 1 }
        }
        refreshGauges()
    }

    private func refreshGauges() {
        observation.reorderQueuedMs = held.count * Self.packetMs / packetsPerPull
        observation.timelineDepthMs = depthMs
        observation.adaptiveTargetMs = targetMs
    }

    /// How far `sequence` is past `cursor`, in serial-number order (RFC 1982):
    /// negative when it is behind.
    private static func offset(of sequence: UInt16, from cursor: UInt16) -> Int {
        Int(Int16(bitPattern: sequence &- cursor))
    }

    /// Exactly one packet's samples: longer decodes are cut, shorter padded.
    private static func oneBlock(_ pcm: [Float]) -> [Float] {
        let count = packetFrames * channels
        if pcm.count == count {
            return pcm
        }
        var block = Array(pcm.prefix(count))
        block.append(contentsOf: repeatElement(0, count: count - block.count))
        return block
    }
}
