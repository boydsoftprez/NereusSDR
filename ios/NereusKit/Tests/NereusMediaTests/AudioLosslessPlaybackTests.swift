// NereusSDR for iOS: the Core's lossless audio (L16) through the jitter buffer: whole packets, a lost one as 4 ms of silence
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusMedia

/// R-IOS-09: the lossless profile the Core sends (the media control
/// document's L16 encoder: payload type 96, 48 kHz stereo, 192 frames of
/// 16-bit big-endian samples, 768 bytes a packet, one every 4 ms). The
/// buffer plays it in the same 40 ms pulls as Opus, ten packets a pull; a
/// packet that never comes is 4 ms of silence, as on the desktop.
@Suite struct AudioLosslessPlaybackTests {
    static let ssrc: UInt32 = 0x4E53_0001

    static func anchor(firstSequence: UInt16 = 0) -> AudioStreamAnchor {
        AudioStreamAnchor(generation: 1, ssrc: ssrc, firstSequence: firstSequence, firstTimestamp: 0,
                          format: .l16)
    }

    /// Packet `sequence` of a stream whose every left sample is `left(sequence)`
    /// and every right sample its negative.
    static func packet(_ sequence: UInt16, left: Int16? = nil, payloadType: UInt8 = 96) -> RtpPacket {
        let value = left ?? Int16(sequence) &* 64
        var bytes: [UInt8] = []
        for _ in 0..<L16Audio.packetFrames {
            for sample in [value, value == .min ? .max : -value] {
                let bits = UInt16(bitPattern: sample)
                bytes.append(UInt8(bits >> 8))
                bytes.append(UInt8(bits & 0xff))
            }
        }
        return RtpPacket(payloadType: payloadType, sequence: sequence,
                         timestamp: UInt32(sequence) &* UInt32(L16Audio.packetFrames), ssrc: ssrc,
                         payload: Data(bytes))
    }

    static func samples(_ block: AudioJitterBuffer.Block) -> [Float]? {
        switch block {
        case .audio(let samples), .concealed(let samples): samples
        case .silence: nil
        }
    }

    @Test func theShapeIsTheCoresL16() {
        #expect(L16Audio.payloadType == 96)
        #expect(L16Audio.packetFrames == 192)
        #expect(L16Audio.payloadBytes == 768)
        #expect(AudioJitterBuffer.packetFrames % L16Audio.packetFrames == 0)
    }

    @Test func wholePacketsPlayTenToAPullAndDecodeExactly() throws {
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(Self.anchor())
        // 180 ms is 45 packets of 4 ms.
        for sequence in UInt16(0)..<44 {
            buffer.push(Self.packet(sequence))
        }
        #expect(buffer.depthMs == 176)
        #expect(buffer.pull() == .silence, "still filling below the 180 ms target")
        buffer.push(Self.packet(44))
        #expect(buffer.depthMs == 180)
        guard case .audio(let first) = buffer.pull() else {
            Issue.record("ten whole packets play as audio")
            return
        }
        #expect(first.count == AudioJitterBuffer.packetFrames * AudioJitterBuffer.channels)
        for slot in 0..<10 {
            let left = Float(Int16(slot) &* 64) / 32768
            let start = slot * L16Audio.packetFrames * 2
            #expect(first[start] == left)
            #expect(first[start + 1] == -left)
            #expect(first[start + L16Audio.packetFrames * 2 - 2] == left)
        }
        #expect(buffer.observation.decodedPackets == 10)
        #expect(buffer.observation.concealedIntervals == 0)
        #expect(buffer.lastPulledSequence == 9)
        #expect(buffer.lastPullRelease?.rtpTimestamp == UInt32(10 * 192))
        #expect(buffer.depthMs == 140)
    }

    @Test func aLostPacketIsFourMillisecondsOfSilence() throws {
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(Self.anchor())
        for sequence in UInt16(0)..<50 where sequence != 3 {
            buffer.push(Self.packet(sequence, left: 1000))
        }
        guard case .concealed(let block) = buffer.pull() else {
            Issue.record("a pull with a lost packet is a repair")
            return
        }
        let slot = L16Audio.packetFrames * 2
        #expect(block[(3 * slot)..<(4 * slot)].allSatisfy { $0 == 0 })
        #expect(block[0..<(3 * slot)].allSatisfy { abs($0) == Float(1000) / 32768 })
        #expect(block[(4 * slot)...].allSatisfy { abs($0) == Float(1000) / 32768 })
        #expect(buffer.observation.decodedPackets == 9)
        #expect(buffer.observation.concealedIntervals == 1)
        #expect(buffer.concealed == 1)
        // The pull's last packet came, so its end is known.
        #expect(buffer.lastPullRelease?.rtpTimestamp == UInt32(10 * 192))
    }

    @Test func aPacketOfTheWrongShapeOrTypeIsNotPlayed() throws {
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(Self.anchor())
        // Opus on the lossless stream is another profile's packet.
        buffer.push(Self.packet(0, payloadType: 111))
        #expect(buffer.otherPayloadDrops == 1)
        #expect(buffer.observation.rejectedProfilePackets == 1)
        // A short L16 payload is held as invalid and repaired as silence.
        buffer.push(RtpPacket(payloadType: 96, sequence: 0, timestamp: 0, ssrc: Self.ssrc,
                              payload: Data(repeating: 1, count: 100)))
        #expect(buffer.observation.invalidPackets == 1)
        for sequence in UInt16(1)..<50 {
            buffer.push(Self.packet(sequence, left: 2000))
        }
        let block = try #require(Self.samples(buffer.pull()))
        #expect(block[0..<(L16Audio.packetFrames * 2)].allSatisfy { $0 == 0 })
        #expect(buffer.observation.concealedIntervals == 1)
    }

    @Test func nothingHeldIsAnUnderrunAndTheTargetRises() throws {
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(Self.anchor())
        for sequence in UInt16(0)..<45 {
            buffer.push(Self.packet(sequence))
        }
        for _ in 0..<5 {
            #expect(Self.samples(buffer.pull()) != nil)
        }
        #expect(buffer.pull() == .silence)
        #expect(buffer.underruns == 1)
        #expect(buffer.targetMs == AudioJitterBuffer.initialTargetMs + AudioJitterBuffer.targetRiseMs)
    }

    @Test func tooMuchHeldIsTrimmedToTheTargetInWholePackets() throws {
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(Self.anchor())
        // 410 ms held: more than 400, so the pull drops the oldest down to
        // the 180 ms target (45 packets) first, then plays ten.
        for sequence in UInt16(0)..<UInt16(103) {
            buffer.push(Self.packet(sequence))
        }
        #expect(buffer.depthMs == 412)
        _ = buffer.pull()
        #expect(buffer.observation.overflowEvents == 1)
        #expect(buffer.lastPulledSequence == 67)
        #expect(buffer.depthMs == 140)
    }

    @Test func aPacketFarAheadMovesThePlayoutPosition() throws {
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(Self.anchor())
        buffer.push(Self.packet(0))
        // 32 pulls ahead (1280 ms, 320 packets) is the limit.
        buffer.push(Self.packet(319))
        #expect(buffer.observation.streamGapEvents == 0)
        buffer.push(Self.packet(320))
        #expect(buffer.observation.streamGapEvents == 1)
    }

    @Test func anOpusAnchorStillDropsL16() throws {
        let buffer = try AudioJitterBuffer()
        buffer.reanchor(AudioStreamAnchor(generation: 1, ssrc: Self.ssrc, firstSequence: 0, firstTimestamp: 0))
        buffer.push(Self.packet(0))
        #expect(buffer.otherPayloadDrops == 1)
        #expect(buffer.depthMs == 0)
    }

    @Test func theUplinkQuantisesAsTheCoreDoes() {
        #expect(L16Audio.quantise(1.0) == 32767)
        #expect(L16Audio.quantise(2.0) == 32767)
        #expect(L16Audio.quantise(-1.0) == -32768)
        #expect(L16Audio.quantise(-3.0) == -32768)
        #expect(L16Audio.quantise(0.5) == 16384)
        #expect(L16Audio.quantise(0) == 0)
        #expect(L16Audio.quantise(.nan) == 0)
        let mono = (0..<L16Audio.packetFrames).map { Float($0) / 1000 - 0.1 }
        let payload = L16Audio.stereoPayload(mono: mono[...])
        #expect(payload.count == L16Audio.payloadBytes)
        let decoded = L16Audio.decode(payload)
        for index in 0..<L16Audio.packetFrames {
            let expected = Float(L16Audio.quantise(mono[index])) / 32768
            #expect(decoded?[2 * index] == expected)
            #expect(decoded?[2 * index + 1] == expected)
        }
    }
}
