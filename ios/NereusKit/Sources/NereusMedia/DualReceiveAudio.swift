// NereusSDR for iOS: merge equal-timestamp audio from the old and new media paths
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The Core's DualPathAudio timing contract. One actor drives this on its
/// own clock. A faster NEW packet waits for OLD's copy so earlier OLD-only
/// packets are not overtaken; after OLD is gone its measured lead eases by
/// one 40 ms packet every 2000 ms. Timestamp history is per semantic stream.
final class DualReceiveAudio {
    static let maximumUnknownLeadMs: UInt64 = 600
    static let easeIntervalMs: UInt64 = 2_000
    static let easeStepMs: UInt64 = 40
    static let duplicateHistory = 64
    static let maximumOldArrivals = 1_024
    /// More than three seconds of all six 40 ms streams; a stalled caller
    /// cannot retain packets without limit.
    static let maximumHeldPackets = 512

    struct Key: Hashable {
        let stream: Int
        let timestamp: UInt32
    }

    private struct Held {
        let packet: RtpPacket
        let arrivalMs: UInt64
    }

    private var held: [Key: Held] = [:]
    private var heldOrder: [Key] = []
    private var oldArrivals: [Key: UInt64] = [:]
    private var recent: [Int: [UInt32]] = [:]
    private(set) var leadMs: UInt64?
    private(set) var easeSteps = 0
    private var oldDone = false
    private var nextEaseMs: UInt64 = 0
    private(set) var active = true
    /// Timestamps remembered per stream. Lossless packets are 4 ms, ten to
    /// one 40 ms Opus packet, so the same span of audio needs ten times as
    /// many (the jitter buffer scales its own history the same way).
    let duplicateHistory: Int

    init(audioFormat: AudioStreamFormat = .opus) {
        duplicateHistory = Self.duplicateHistory * (audioFormat == .l16 ? 10 : 1)
    }

    var needsTick: Bool { active && (!held.isEmpty || (oldDone && (leadMs ?? 0) > 0)) }
    var heldCount: Int { held.count }

    func submit(_ packet: RtpPacket, stream: Int, fromNew: Bool, nowMs: UInt64) -> [RtpPacket] {
        let key = Key(stream: stream, timestamp: packet.timestamp)
        guard active else { return deliver(packet, stream: stream) }
        if !fromNew {
            if oldArrivals.count >= Self.maximumOldArrivals { oldArrivals.removeAll() }
            oldArrivals[key] = nowMs
            if let early = held.removeValue(forKey: key) {
                heldOrder.removeAll { $0 == key }
                leadMs = max(leadMs ?? 0, nowMs >= early.arrivalMs ? nowMs - early.arrivalMs : 0)
            }
            return deliver(packet, stream: stream)
        }
        if oldArrivals[key] != nil {
            leadMs = max(leadMs ?? 0, 0)
            return deliver(packet, stream: stream)
        }
        if oldDone && (leadMs ?? 0) == 0 { return deliver(packet, stream: stream) }
        guard held[key] == nil else { return [] }
        if heldOrder.count >= Self.maximumHeldPackets {
            let oldest = heldOrder.removeFirst()
            held.removeValue(forKey: oldest)
        }
        held[key] = Held(packet: packet, arrivalMs: nowMs)
        heldOrder.append(key)
        return tick(nowMs: nowMs)
    }

    func oldPathDone(nowMs: UInt64) -> [RtpPacket] {
        oldDone = true
        nextEaseMs = nowMs &+ Self.easeIntervalMs
        return tick(nowMs: nowMs)
    }

    func tick(nowMs: UInt64) -> [RtpPacket] {
        guard active else { return [] }
        if oldDone, let leadMs, leadMs > 0, nowMs >= nextEaseMs {
            self.leadMs = leadMs > Self.easeStepMs ? leadMs - Self.easeStepMs : 0
            easeSteps += 1
            nextEaseMs = nowMs &+ Self.easeIntervalMs
        }
        let waitMs = leadMs ?? Self.maximumUnknownLeadMs
        var ready: [RtpPacket] = []
        // Arrival order, never RTP timestamp order: timestamps wrap.
        while let key = heldOrder.first, let item = held[key],
              nowMs >= item.arrivalMs && nowMs - item.arrivalMs >= waitMs {
            heldOrder.removeFirst()
            held.removeValue(forKey: key)
            ready += deliver(item.packet, stream: key.stream)
        }
        if oldDone && held.isEmpty && (leadMs ?? 0) == 0 {
            active = false
            oldArrivals.removeAll()
        }
        return ready
    }

    private func deliver(_ packet: RtpPacket, stream: Int) -> [RtpPacket] {
        var timestamps = recent[stream, default: []]
        guard !timestamps.contains(packet.timestamp) else { return [] }
        timestamps.append(packet.timestamp)
        if timestamps.count > duplicateHistory { timestamps.removeFirst() }
        recent[stream] = timestamps
        return [packet]
    }
}
