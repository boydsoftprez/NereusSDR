// NereusSDR for iOS: actual keyed microphone encoding follows a mid-key quality change and Lossless fallback without replaying the key
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusKitTesting
@testable import NereusMedia
@testable import NereusSDR
import Testing

/// R-IOS-09: exercise the actual capture's retained Opus encoder, sender
/// and uplink. The input engine and both pacing clocks are test stand-ins;
/// the encoder, held samples and RTP packet construction are real.
@Suite("Microphone quality during one key", .serialized)
@MainActor
struct MicrophoneQualityTransitionTests {
    private final class Clock: MicrophoneSendClock, @unchecked Sendable {
        private let lock = NSLock()
        private var time: UInt64 = 1_000_000_000
        private var waiting: (deadline: UInt64, work: @Sendable () -> Void)?
        private var closed = false

        var now: UInt64 { lock.withLock { time } }
        var isClosed: Bool { lock.withLock { closed } }
        var waitingWork: (@Sendable () -> Void)? { lock.withLock { waiting?.work } }

        func schedule(at deadline: UInt64, _ work: @escaping @Sendable () -> Void) {
            lock.withLock {
                if !closed { waiting = (deadline, work) }
            }
        }

        func close() {
            lock.withLock { closed = true; waiting = nil }
        }

        func advance(by duration: UInt64) {
            let target = now + duration
            while true {
                let due: (@Sendable () -> Void)? = lock.withLock {
                    guard !closed, let next = waiting, next.deadline <= target else { return nil }
                    waiting = nil
                    time = next.deadline
                    return next.work
                }
                guard let due else { break }
                due()
            }
            lock.withLock { time = max(time, target) }
        }
    }

    private final class Clocks: @unchecked Sendable {
        let clock = Clock()
        private let lock = NSLock()
        private var starts = 0
        var count: Int { lock.withLock { starts } }
        func make() -> any MicrophoneSendClock {
            lock.withLock { starts += 1 }
            return clock
        }
    }

    private final class Pacer: @unchecked Sendable {
        private let lock = NSLock()
        private var waiting: [(delay: UInt64, work: @Sendable () -> Void)] = []
        var delays: [UInt64] { lock.withLock { waiting.map(\.delay) } }
        var schedule: MediaUplink.Pacer {
            { [self] delay, work in lock.withLock { waiting.append((delay, work)) } }
        }
        func runAll() {
            while let next = lock.withLock({ waiting.isEmpty ? nil : waiting.removeFirst() }) { next.work() }
        }
    }

    @MainActor
    private final class Rig {
        let clocks: Clocks
        let pacer: Pacer
        let peer: FakeMediaPeer
        let port: FakeIOBufferDuration
        let uplink: MediaUplink
        let capture: MicCapture
        private var originalEncoder: ObjectIdentifier?

        init(negotiatedL16: Bool) {
            let clocks = Clocks()
            let pacer = Pacer()
            let peer = FakeMediaPeer()
            let port = FakeIOBufferDuration(preferred: 0.02)
            let uplink = MediaUplink(pacer: pacer.schedule)
            peer.enableMicrophoneLine(ssrc: 42)
            peer.microphoneLosslessNegotiated = negotiatedL16
            uplink.attach(peer, microphoneSsrc: 42)
            uplink.microphoneQuality = .saveData
            self.clocks = clocks
            self.pacer = pacer
            self.peer = peer
            self.port = port
            self.uplink = uplink
            capture = MicCapture(uplink: uplink, permitted: { true }, ioBuffer: port,
                                 allowHaptics: {}, startsEngine: false, makeClock: { clocks.make() })
        }

        func begin() async throws {
            #expect(await capture.start() == .started)
            #expect(capture.activeOpusProfile == .microphoneSaveData,
                    "the actual stored key encoder starts at Save data")
            #expect(capture.activeOpusBitrate == 24_000, "libopus starts at Save data")
            originalEncoder = try #require(capture.activeOpusIdentity)
            capture.takeSamplesWithoutEngine(Self.frame(amplitude: 0.125, frequency: 600))
            #expect(peer.microphonePackets.count == 1)
            // Two distinct frames wait for their real sender turns, not
            // already encoded Opus; a quality change must affect their sends.
            capture.takeSamplesWithoutEngine(Self.frame(amplitude: 0.25, frequency: 1_000)
                                             + Self.frame(amplitude: 0.5, frequency: 1_400))
            #expect(peer.microphonePackets.count == 1)
        }

        static func frame(amplitude: Float, frequency: Double) -> [Float] {
            (0..<960).map { amplitude * Float(sin(2 * .pi * frequency * Double($0) / 48_000)) }
        }

        func checkOneKey() {
            #expect(clocks.count == 1, "quality changes do not begin/replay a key")
            #expect(clocks.clock.now == 1_040_000_000, "both queued frames keep their 20 ms turns")
            #expect(capture.activeOpusIdentity == originalEncoder, "reuse the actual codec state within this key")
            #expect(capture.activeOpusProfile == uplink.microphoneOpusProfile, "reported configuration matches the actual routed choice")
            #expect(!clocks.clock.isClosed)
            #expect(port.requests == [KeyedIOBuffer.keyedDuration], "no unkey/rekey buffer cycle")
        }

        func finish() {
            let before = peer.microphonePackets.count
            // Leave another frame queued, then stop in the capture's
            // ordered control queue and invoke the previously saved tick.
            capture.takeSamplesWithoutEngine([Float](repeating: 0.75, count: 960))
            let staleTick = clocks.clock.waitingWork
            capture.stop()
            capture.settle()
            staleTick?()
            clocks.clock.advance(by: 100_000_000)
            pacer.runAll()
            capture.takeSamplesWithoutEngine([Float](repeating: 0.875, count: 960))
            #expect(peer.microphonePackets.count == before, "nothing queued or fed after unkey is replayed")
            #expect(clocks.count == 1 && clocks.clock.isClosed)
            #expect(port.requests == [KeyedIOBuffer.keyedDuration, 0.02])
        }
    }

    private func checkPackets(_ packets: [RtpPacket], formats: [UInt8]) throws {
        #expect(packets.map(\.payloadType) == formats)
        let first = try #require(packets.first)
        var timestamp = first.timestamp
        let decoder = try OpusDecoder(channels: 1)
        var l16Offset = 0
        var opusIndex = 0
        let frequencies: [Double] = formats.contains(L16Audio.payloadType) ? [600, 1_400] : [600, 1_000, 1_400]
        let amplitudes: [Double] = formats.contains(L16Audio.payloadType) ? [0.125, 0.5] : [0.125, 0.25, 0.5]
        for (index, packet) in packets.enumerated() {
            #expect(packet.ssrc == 42)
            #expect(packet.sequence == first.sequence &+ UInt16(index), "no duplicate or reset sequence")
            #expect(packet.timestamp == timestamp, "no gap or key replay")
            if packet.payloadType == L16Audio.payloadType {
                let samples = try #require(L16Audio.decode(packet.payload))
                #expect(samples.count == 384)
                let expected = Rig.frame(amplitude: 0.25, frequency: 1_000)
                #expect(samples.enumerated().allSatisfy { offset, sample in
                    abs(sample - expected[l16Offset + offset / 2]) <= 1.0 / 32_768
                }, "ordered middle-frame PCM is duplicated into L16 stereo with only quantisation error")
                l16Offset += L16Audio.packetFrames
                timestamp &+= UInt32(L16Audio.packetFrames)
            } else {
                let decoded = try decoder.decode(packet.payload)
                #expect(decoded.count == 960, "real Opus keeps 20 ms frames")
                #expect(decoded.allSatisfy { $0.isFinite })
                // The last 10 ms is beyond Opus's lookahead and the frame's
                // transition. Its distinct tone and amplitude identify the
                // original held PCM, even though compression is lossy.
                let tail = decoded.suffix(480).map(Double.init)
                let energy = tail.reduce(0) { $0 + $1 * $1 }
                let frequency = frequencies[opusIndex]
                var cosine = 0.0
                var sine = 0.0
                for (offset, sample) in tail.enumerated() {
                    let phase = 2 * Double.pi * frequency * Double(offset) / 48_000
                    cosine += sample * cos(phase)
                    sine += sample * sin(phase)
                }
                let carrierEnergy = 2 * (cosine * cosine + sine * sine) / Double(tail.count)
                #expect(carrierEnergy > 0.75 * energy && energy > 0,
                        "decoded PCM belongs to this distinct held frame, not a replayed neighbour")
                let rms = sqrt(energy / Double(tail.count))
                let inputRms = amplitudes[opusIndex] / sqrt(2)
                #expect(rms > 0.65 * inputRms && rms < 1.35 * inputRms,
                        "this frame's PCM amplitude survives lossy encoding")
                opusIndex += 1
                timestamp &+= MediaUplink.frameSamples
            }
        }
    }

    @Test("Save data to High uses the actual High encoder at the next frame, within one key")
    func saveToHighChangesTheEncoderInOneKey() async throws {
        let rig = Rig(negotiatedL16: false)
        defer { rig.capture.stop(); rig.capture.settle() }
        try await rig.begin()
        rig.uplink.microphoneQuality = .high
        rig.clocks.clock.advance(by: 20_000_000)
        #expect(rig.capture.activeOpusBitrate == 48_000,
                "libopus must encode this emitted frame at High, not its retained Save data bitrate")
        rig.clocks.clock.advance(by: 20_000_000)
        try checkPackets(rig.peer.microphonePackets, formats: [111, 111, 111])
        rig.checkOneKey()
        rig.finish()
    }

    @Test("Save data to High and back to Save data follows both queued turns without a new codec")
    func highReturnsToSaveDataWithinTheSameKey() async throws {
        let rig = Rig(negotiatedL16: false)
        defer { rig.capture.stop(); rig.capture.settle() }
        try await rig.begin()
        rig.uplink.microphoneQuality = .high
        rig.clocks.clock.advance(by: 20_000_000)
        #expect(rig.capture.activeOpusBitrate == 48_000)
        rig.uplink.microphoneQuality = .saveData
        rig.clocks.clock.advance(by: 20_000_000)
        #expect(rig.capture.activeOpusBitrate == 24_000)
        try checkPackets(rig.peer.microphonePackets, formats: [111, 111, 111])
        rig.checkOneKey()
        rig.finish()
    }

    @Test("Save data to Lossless without an L16 line uses the actual High encoder within one key")
    func saveToUnnegotiatedLosslessUsesHighInOneKey() async throws {
        let rig = Rig(negotiatedL16: false)
        defer { rig.capture.stop(); rig.capture.settle() }
        try await rig.begin()
        rig.uplink.microphoneQuality = .lossless
        #expect(!rig.uplink.microphoneSendsLossless)
        rig.clocks.clock.advance(by: 20_000_000)
        #expect(rig.capture.activeOpusBitrate == 48_000,
                "libopus must encode unnegotiated Lossless at High, not the retained Save data bitrate")
        rig.clocks.clock.advance(by: 20_000_000)
        try checkPackets(rig.peer.microphonePackets, formats: [111, 111, 111])
        rig.checkOneKey()
        rig.finish()
    }

    @Test("A key started in Save data really sends L16, then falls back to the actual High encoder without replay")
    func l16FallsBackToHighAfterAKeyStartedInSaveData() async throws {
        let rig = Rig(negotiatedL16: true)
        defer { rig.capture.stop(); rig.capture.settle() }
        try await rig.begin()
        rig.uplink.microphoneQuality = .lossless
        #expect(rig.uplink.microphoneSendsLossless)
        rig.clocks.clock.advance(by: 20_000_000)
        #expect(rig.peer.microphonePackets.map(\.payloadType) == [111, 96])
        #expect(rig.pacer.delays == [MediaUplink.l16PacketSpacingNs])
        // This is the actual setter MediaControlClient uses when its
        // lossless trial fails. The real uplink flushes the four remaining
        // L16 packets before the next actual Opus frame.
        rig.uplink.setLosslessFallback(true)
        #expect(!rig.uplink.microphoneSendsLossless)
        rig.clocks.clock.advance(by: 20_000_000)
        #expect(rig.capture.activeOpusBitrate == 48_000,
                "libopus must encode fallback at High after this Save data key, not at its original bitrate")
        try checkPackets(rig.peer.microphonePackets, formats: [111, 96, 96, 96, 96, 96, 111])
        let beforeStalePacer = rig.peer.microphonePackets.count
        rig.pacer.runAll()
        #expect(rig.peer.microphonePackets.count == beforeStalePacer, "flushed L16 callbacks never send twice")
        rig.checkOneKey()
        rig.finish()
    }
}
