// NereusSDR for iOS: the microphone line in lossless: the answer keeps L16 when offered, and the uplink's 4 ms packets
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// R-IOS-09: with Lossless chosen and not fallen back, and the microphone
/// line negotiated with L16 (the Core offers it only with audio_lossless
/// allowed), the microphone goes as the desktop sends it: 4 ms L16 packets
/// at payload type 96, the mono sample in both channels. Otherwise it
/// stays on Opus: 48 kbit/s, or 24 kbit/s under Save data.
@Suite struct MicrophoneLosslessTests {
    static func micLine(lossless: Bool) -> String {
        var lines = ["m=audio 9 UDP/TLS/RTP/SAVPF 111" + (lossless ? " 96" : ""),
                     "c=IN IP4 0.0.0.0", "a=mid:mic", "a=recvonly",
                     "a=rtpmap:111 opus/48000/2", "a=fmtp:111 minptime=10;useinbandfec=1"]
        if lossless {
            lines.append("a=rtpmap:96 L16/48000/2")
        }
        lines.append("a=rtcp-mux")
        return lines.joined(separator: "\r\n") + "\r\n"
    }

    // MARK: The answer

    @Test func theAnswerKeepsL16WhenTheCoreOffersIt() throws {
        let answer = try #require(MediaPeer.microphoneAnswer(from: Self.micLine(lossless: true), ssrc: 7))
        #expect(answer.hasPrefix("m=audio 9 UDP/TLS/RTP/SAVPF 111 96\r\n"))
        #expect(answer.contains("a=rtpmap:96 L16/48000/2\r\n"))
        #expect(answer.contains("a=rtpmap:111 opus/48000/2\r\n"))
        #expect(answer.contains("a=sendonly\r\n"))
        #expect(MediaPeer.describesL16(answer))
    }

    @Test func theAnswerIsOpusAloneWhenTheCoreOffersNoL16() throws {
        let answer = try #require(MediaPeer.microphoneAnswer(from: Self.micLine(lossless: false), ssrc: 7))
        #expect(answer.hasPrefix("m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"))
        #expect(!answer.contains("L16"))
        #expect(!MediaPeer.describesL16(answer))
        // L16 at a payload type that is not the Core's 96 is not lossless.
        let elsewhere = Self.micLine(lossless: true).replacingOccurrences(of: "rtpmap:96", with: "rtpmap:97")
        #expect(!MediaPeer.describesL16(elsewhere))
    }

    // MARK: The uplink

    final class Line: MediaPeerConnection, @unchecked Sendable {
        let lock = NSLock()
        var sent: [RtpPacket] = []
        let losslessNegotiated: Bool
        init(losslessNegotiated: Bool) { self.losslessNegotiated = losslessNegotiated }
        var localDescription: AsyncStream<String> { AsyncStream { _ in } }
        var localCandidates: AsyncStream<(candidate: String, mid: String)> { AsyncStream { _ in } }
        var audioPackets: AsyncStream<RtpPacket> { AsyncStream { _ in } }
        var displayDatagrams: AsyncStream<Data> { AsyncStream { _ in } }
        var state: AsyncStream<MediaPeer.State> { AsyncStream { _ in } }
        func setRemoteDescription(_ sdp: String) throws {}
        func addRemoteCandidate(_ candidate: String, mid: String) throws {}
        func setExpectedAudioSsrc(_ ssrc: UInt32?) {}
        var microphoneLineReady: Bool { true }
        var microphoneLosslessNegotiated: Bool { losslessNegotiated }
        func sendMicrophone(_ packet: RtpPacket) throws { lock.withLock { sent.append(packet) } }
        func close() {}
    }

    /// Holds the uplink's paced sends until the test runs them.
    final class Pacer: @unchecked Sendable {
        let lock = NSLock()
        private var waiting: [(UInt64, @Sendable () -> Void)] = []
        var delays: [UInt64] { lock.withLock { waiting.map(\.0) } }
        var schedule: MediaUplink.Pacer {
            { [self] delay, work in lock.withLock { waiting.append((delay, work)) } }
        }
        /// Runs what is waiting, and what that schedules, in order.
        func runAll() {
            while let next = lock.withLock({ waiting.isEmpty ? nil : waiting.removeFirst() }) { next.1() }
        }
        /// Runs the one send waiting first.
        func runOne() {
            lock.withLock { waiting.isEmpty ? nil : waiting.removeFirst() }?.1()
        }
    }

    @Test func aLosslessFrameGoesAsFiveL16Packets() throws {
        let pacer = Pacer()
        let uplink = MediaUplink(pacer: pacer.schedule)
        let line = Line(losslessNegotiated: true)
        uplink.attach(line, microphoneSsrc: 42)
        let mono = (0..<960).map { Float($0 % 192) / 256 }
        #expect(uplink.sendMicrophoneL16(L16Audio.microphoneFrame(mono: mono)))
        pacer.runAll()
        #expect(line.sent.count == 5)
        #expect(line.sent.allSatisfy { $0.payloadType == 96 && $0.ssrc == 42 && $0.payload.count == 768 })
        let first = try #require(line.sent.first)
        for (index, packet) in line.sent.enumerated() {
            #expect(packet.sequence == first.sequence &+ UInt16(index))
            #expect(packet.timestamp == first.timestamp &+ UInt32(index * 192))
        }
        let decoded = try #require(L16Audio.decode(line.sent[1].payload))
        #expect(decoded[0] == decoded[1], "the mono sample in both channels")
        #expect(decoded[2] == Float(L16Audio.quantise(mono[193])) / 32768)
        #expect(uplink.microphonePacketsSent == 5)
        // Opus after it carries on the line's sequence and timestamp.
        #expect(uplink.sendMicrophone(Data([0xfc])))
        #expect(line.sent.last?.sequence == first.sequence &+ 5)
        #expect(line.sent.last?.timestamp == first.timestamp &+ 960)
        #expect(line.sent.last?.payloadType == 111)
    }

    /// The five packets of a 20 ms frame leave about 4 ms apart, as the
    /// audio they carry, not in one burst; a frame that comes before the
    /// last one has gone sends what is left first, so the order holds.
    @Test func aLosslessFrameIsPacedFourMillisecondsApart() throws {
        let pacer = Pacer()
        let uplink = MediaUplink(pacer: pacer.schedule)
        let line = Line(losslessNegotiated: true)
        uplink.attach(line, microphoneSsrc: 42)
        let frame = L16Audio.microphoneFrame(mono: [Float](repeating: 0.25, count: 960))
        #expect(uplink.sendMicrophoneL16(frame))
        #expect(line.sent.count == 1, "one goes now")
        #expect(pacer.delays == [MediaUplink.l16PacketSpacingNs])
        #expect(MediaUplink.l16PacketSpacingNs == 4_000_000)
        pacer.runOne()
        #expect(line.sent.count == 2)
        #expect(pacer.delays == [MediaUplink.l16PacketSpacingNs])
        // The next frame is early: the three left go first, then its first.
        #expect(uplink.sendMicrophoneL16(frame))
        #expect(line.sent.count == 6)
        pacer.runAll()
        #expect(line.sent.count == 10)
        let first = try #require(line.sent.first)
        #expect(line.sent.map(\.sequence) == (0..<10).map { first.sequence &+ UInt16($0) })
        #expect(uplink.microphonePacketsSent == 10)
        // Opus after a fall back goes behind anything still waiting.
        #expect(uplink.sendMicrophoneL16(frame))
        #expect(uplink.sendMicrophone(Data([0xfc])))
        #expect(line.sent.count == 16)
        #expect(line.sent.last?.payloadType == 111)
        #expect(line.sent.map(\.sequence) == (0..<16).map { first.sequence &+ UInt16($0) })
        pacer.runAll()
        #expect(line.sent.count == 16, "nothing left to pace")
    }

    @Test func aDetachedLineDropsWhatWaits() {
        let pacer = Pacer()
        let uplink = MediaUplink(pacer: pacer.schedule)
        let line = Line(losslessNegotiated: true)
        uplink.attach(line, microphoneSsrc: 42)
        #expect(uplink.sendMicrophoneL16(L16Audio.microphoneFrame(mono: [Float](repeating: 0, count: 960))))
        uplink.detach()
        pacer.runAll()
        #expect(line.sent.count == 1)
    }

    @Test func aFrameOfTheWrongSizeIsNotSent() {
        let uplink = MediaUplink()
        let line = Line(losslessNegotiated: true)
        uplink.attach(line, microphoneSsrc: 42)
        #expect(!uplink.sendMicrophoneL16(Data(count: 100)))
        #expect(line.sent.isEmpty)
    }

    @Test(arguments: [
        (MediaUplink.MicrophoneQuality.lossless, true, false, true),
        (MediaUplink.MicrophoneQuality.lossless, false, false, false),
        (MediaUplink.MicrophoneQuality.lossless, true, true, false),
        (MediaUplink.MicrophoneQuality.high, true, false, false),
        (MediaUplink.MicrophoneQuality.saveData, true, false, false),
    ])
    func theLineSendsLosslessOnlyWhenChosenNegotiatedAndCarried(quality: MediaUplink.MicrophoneQuality,
                                                                 negotiated: Bool, fallenBack: Bool,
                                                                 lossless: Bool) {
        let uplink = MediaUplink()
        uplink.attach(Line(losslessNegotiated: negotiated), microphoneSsrc: 42)
        uplink.microphoneQuality = quality
        uplink.setLosslessFallback(fallenBack)
        #expect(uplink.microphoneSendsLossless == lossless)
        #expect(uplink.microphoneOpusProfile == (quality == .saveData ? .microphoneSaveData : .microphone))
    }

    @Test func noLineIsNoLossless() {
        let uplink = MediaUplink()
        uplink.microphoneQuality = .lossless
        #expect(!uplink.microphoneSendsLossless)
    }

    // MARK: In the media client

    typealias Rig = MediaControlClientTests.Rig

    @Test func theClientTellsTheUplinkOfAFallBack() async throws {
        let rig = try Rig()
        _ = try await LosslessLinkTrialTests.losslessUp(rig)
        #expect(!rig.client.uplink.losslessFallback)
        let ssrc = MediaControlClient.audioSsrc(forConnection: try await rig.connectionId)
        for second in 0..<6 {
            LosslessLinkTrialTests.playOneSecond(rig, from: UInt16(second * 250), ssrc: ssrc, dropEvery: 10)
            rig.recorder.advanceClock(by: 1_000)
            await rig.timers.advance(by: 1_000)
            await rig.recorder.settle { LosslessLinkTrialTests.fellBack(rig) }
            if LosslessLinkTrialTests.fellBack(rig) { break }
        }
        #expect(rig.client.uplink.losslessFallback)
        await rig.client.setAudioRequest(MediaControlClient.AudioRequest(lossless: true, opusBitrate: 48000),
                                         choosing: true)
        #expect(!rig.client.uplink.losslessFallback)
    }
}
