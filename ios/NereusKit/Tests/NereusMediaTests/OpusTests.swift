// NereusSDR for iOS: tests for the Opus decoder and encoder, with the link's Opus conformance vectors
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import Testing
import COpus
@testable import NereusMedia

@Suite struct OpusTests {
    // MARK: The link's Opus vectors (link document section 16.4)

    /// Every expectation field an Opus vector may hold.
    private static let opusFields: Set<String> = [
        "status", "sequence", "timestamp", "ssrc", "channels", "bandwidth",
        "samplesPerChannel", "pcm16", "tolerance", "after",
    ]

    @Test func everyOpusVectorDecodesWithinItsTolerance() throws {
        let all = try LinkFixtureLoader.mediaVectors()
        let opus = all.values.filter { $0.codec == "opus" }.sorted { $0.id < $1.id }
        #expect(opus.count >= 4, "the suite holds opus-1 to opus-4")
        for vector in opus {
            if let failure = check(vector, in: all) {
                Issue.record("\(vector.id): \(failure)")
            }
        }
    }

    /// Returns why `vector` fails, or nil when it passes.
    private func check(_ vector: LinkFixtureLoader.MediaVector,
                       in all: [String: LinkFixtureLoader.MediaVector]) -> String? {
        let expect = vector.expect
        if let unknown = Set(expect.keys).subtracting(Self.opusFields).sorted().first {
            return "unknown expectation field \"\(unknown)\""
        }
        guard let tolerance = expect["tolerance"] as? [String: Any], tolerance.count == 1,
              let key = tolerance.keys.first, key == "minSnrDb" || key == "lsb16",
              let limit = (tolerance[key] as? NSNumber)?.doubleValue else {
            return "an Opus tolerance is {\"minSnrDb\": <dB>} or {\"lsb16\": <steps>}"
        }
        guard let reference = (expect["pcm16"] as? [NSNumber])?.map(\.doubleValue) else {
            return "pcm16 must be an array of numbers"
        }
        do {
            let decoder = try OpusDecoder(channels: 2)
            for earlier in try LinkFixtureLoader.after(vector, in: all) {
                _ = try decoder.decode(RtpTestHeader(earlier.bytes).payload)
            }
            let header = try RtpTestHeader(vector.bytes)
            let decoded = try decoder.decode(header.payload)

            let found: [String: Int] = [
                "sequence": header.sequence,
                "timestamp": header.timestamp,
                "ssrc": header.ssrc,
                "channels": Int(opus_packet_get_nb_channels(Array(header.payload))),
                "bandwidth": Int(opus_packet_get_bandwidth(Array(header.payload))),
                "samplesPerChannel": Int(opus_packet_get_nb_samples(
                    Array(header.payload), Int32(header.payload.count), 48000)),
            ]
            for (field, value) in found.sorted(by: { $0.key < $1.key }) {
                guard let wanted = expect[field] as? Int else {
                    return "\(field) is missing"
                }
                if value != wanted {
                    return "\(field): expected \(wanted), got \(value)"
                }
            }
            let frames = found["samplesPerChannel", default: 0]
            if decoded.count != frames * 2 {
                return "decoded \(decoded.count) samples, expected \(frames * 2)"
            }
            if expect["status"] as? String != "accepted" {
                return "status: expected \(String(describing: expect["status"])), got accepted"
            }
            if decoded.count != reference.count {
                return "pcm16: expected \(reference.count) samples, got \(decoded.count)"
            }
            return compare(decoded, with: reference, tolerance: key, limit: limit)
        } catch {
            return "\(error)"
        }
    }

    /// Holds the decoder's output, converted as round(sample * 32767), to the
    /// station's reference PCM under the vector's tolerance.
    private func compare(_ decoded: [Float], with reference: [Double], tolerance: String,
                         limit: Double) -> String? {
        var signal = 0.0
        var noise = 0.0
        var worst = 0.0
        var worstAt = 0
        for (index, sample) in decoded.enumerated() {
            let pcm16 = min(max((Double(sample) * 32767).rounded(), -32768), 32767)
            let difference = pcm16 - reference[index]
            signal += reference[index] * reference[index]
            noise += difference * difference
            if abs(difference) > worst {
                worst = abs(difference)
                worstAt = index
            }
        }
        if tolerance == "lsb16", worst > limit {
            return "pcm16[\(worstAt)]: \(worst) steps from the reference, more than \(limit)"
        }
        if tolerance == "minSnrDb", noise > 0 {
            let snr = signal > 0 ? 10 * log10(signal / noise) : -Double.infinity
            if snr < limit {
                return "pcm16: \(snr) dB from the reference, less than \(limit) dB (largest difference at [\(worstAt)])"
            }
        }
        return nil
    }

    @Test func malformedAfterIsReported() throws {
        func vector(_ id: String, codec: String = "opus", after: Any? = nil) -> LinkFixtureLoader.MediaVector {
            var expect: [String: Any] = [:]
            if let after {
                expect["after"] = after
            }
            return LinkFixtureLoader.MediaVector(id: id, bytes: Data(), codec: codec, expect: expect)
        }
        let cases: [(LinkFixtureLoader.MediaVector, [LinkFixtureLoader.MediaVector], String)] = [
            (vector("a", after: ["a"]), [], "after names the fixture itself"),
            (vector("a", after: ["gone"]), [], "after names no media fixture \"gone\""),
            (vector("a", after: ["n"]), [vector("n", codec: "nsdc1")], "after names n, a fixture of another codec"),
            (vector("a", after: ["b"]), [vector("b", after: ["a"])], "after forms a cycle"),
            (vector("a", after: ["b"]), [vector("b", after: ["c"]), vector("c", after: ["b"])], ""),
            (vector("a", after: "b"), [vector("b")], "after must be an array of fixture ids"),
        ]
        for (subject, others, message) in cases {
            var all = [subject.id: subject]
            for other in others {
                all[other.id] = other
            }
            if message.isEmpty {
                // b and c name each other but never a; a itself is well formed.
                #expect(try LinkFixtureLoader.after(subject, in: all).map(\.id) == ["b"])
                continue
            }
            do {
                _ = try LinkFixtureLoader.after(subject, in: all)
                Issue.record("expected \"\(message)\"")
            } catch let malformed as LinkFixtureLoader.Malformed {
                #expect(malformed.description == "a: \(message)")
            }
        }
    }

    @Test func afterIsNotFollowedFurther() throws {
        let all = try LinkFixtureLoader.mediaVectors()
        let fourth = try #require(all["media-opus-4"])
        let earlier = try LinkFixtureLoader.after(fourth, in: all)
        #expect(earlier.map(\.id) == ["media-opus-1", "media-opus-2", "media-opus-3"])
    }

    // MARK: Loss concealment

    @Test func concealLossContinuesADecodedStream() throws {
        let all = try LinkFixtureLoader.mediaVectors()
        let first = try #require(all["media-opus-1"])
        let decoder = try OpusDecoder(channels: 2)
        _ = try decoder.decode(RtpTestHeader(first.bytes).payload)
        let concealed = try decoder.concealLoss(frames: 1920)
        #expect(concealed.count == 3840)
        let finite = concealed.allSatisfy { $0.isFinite }
        #expect(finite)
    }

    @Test func decoderRefusesWhatOpusCannotCode() throws {
        #expect(throws: OpusCodecError.unsupportedChannelCount(3)) {
            try OpusDecoder(channels: 3)
        }
        let decoder = try OpusDecoder(channels: 2)
        #expect(throws: OpusCodecError.emptyPacket) {
            try decoder.decode(Data())
        }
        #expect(throws: OpusCodecError.invalidFrameCount(0)) {
            try decoder.concealLoss(frames: 0)
        }
    }

    // MARK: The microphone's uplink

    /// R-IOS-09: the microphone goes full band at 48 kbit/s (High, and
    /// Lossless while it is carried as Opus) and at 24 kbit/s under Save
    /// data; the rest is the desktop's remote microphone (VOIP, 20 ms
    /// frames, in-band FEC for 10 % loss, voice, no bandwidth set).
    @Test(arguments: [(OpusEncoder.Profile.microphone, 48000), (OpusEncoder.Profile.microphoneSaveData, 24000)])
    func microphoneProfileIsTheUplinkSetting(profile: OpusEncoder.Profile, bitrate: Int) throws {
        #expect(profile.channels == 1)
        #expect(profile.sampleRate == 48000)
        #expect(profile.frameSamples == 960)
        #expect(profile.application == .voip)
        #expect(profile.bitrate == bitrate)
        #expect(profile.variableBitrate && profile.constrainedVariableBitrate)
        #expect(profile.inbandFEC)
        #expect(profile.expectedLossPercent == 10)
        #expect(!profile.discontinuousTransmission)
        #expect(profile.complexity == 9)
        #expect(profile.signal == .voice)
        #expect(try OpusEncoder(profile: profile).bitrate == bitrate)
    }

    /// At 48 kbit/s the encoder codes a voice-band-and-up signal full band:
    /// each packet's TOC names a full band configuration (hybrid 14 or 15,
    /// CELT 28 to 31), once the encoder has settled.
    @Test func theMicrophoneAt48KbitsIsFullBand() throws {
        let profile = OpusEncoder.Profile.microphone
        let encoder = try OpusEncoder(profile: profile)
        // Half a second of a voice-like mix: 200 Hz harmonics to 12 kHz,
        // with a fixed pseudo-random hiss, no clock or system randomness.
        var seed: UInt32 = 0x1234_5678
        var configs: [UInt8] = []
        for frame in 0..<25 {
            var samples = [Float](repeating: 0, count: profile.frameSamples)
            for index in 0..<profile.frameSamples {
                let t = Double(frame * profile.frameSamples + index) / Double(profile.sampleRate)
                var value = 0.0
                for harmonic in stride(from: 200.0, through: 12_000.0, by: 200.0) {
                    value += 0.01 * sin(2 * Double.pi * harmonic * t)
                }
                seed = seed &* 1_664_525 &+ 1_013_904_223
                value += 0.02 * (Double(seed >> 8) / Double(1 << 24) - 0.5)
                samples[index] = Float(value)
            }
            let packet = try encoder.encode(samples)
            configs.append(packet[packet.startIndex] >> 3)
        }
        let settled = configs.suffix(10)
        #expect(settled.allSatisfy { [14, 15, 28, 29, 30, 31].contains($0) }, "TOC configs \(configs)")
    }

    /// R-IOS-09: rate changes keep this codec's history and 20 ms frames.
    @Test func microphoneBitrateChangesKeepTheCodecState() throws {
        let encoder = try OpusEncoder(profile: .microphoneSaveData)
        let identity = ObjectIdentifier(encoder)
        let decoder = try OpusDecoder(channels: 1)
        let samples = (0..<960).map { Float(0.25 * sin(2 * Double.pi * 1_000 * Double($0) / 48_000)) }
        for bitrate in [24_000, 48_000, 48_000, 24_000, 48_000] {
            try encoder.setBitrate(bitrate)
            #expect(encoder.bitrate == bitrate, "actual libopus configuration")
            var expected = OpusEncoder.Profile.microphone
            expected.bitrate = bitrate
            #expect(encoder.profile == expected, "only bitrate changes")
            #expect(ObjectIdentifier(encoder) == identity)
            #expect(try decoder.decode(encoder.encode(samples)).count == 960)
        }
    }

    @Test func refusedMicrophoneBitrateKeepsTheWorkingConfiguration() throws {
        let encoder = try OpusEncoder(profile: .microphoneSaveData)
        for bitrate in [0, Int.max] {
            #expect(throws: OpusCodecError.library(code: -1)) {
                try encoder.setBitrate(bitrate)
            }
            #expect(encoder.profile == .microphoneSaveData)
            #expect(encoder.bitrate == 24_000)
        }
        let decoder = try OpusDecoder(channels: 1)
        #expect(try decoder.decode(encoder.encode([Float](repeating: 0.25, count: 960))).count == 960)
    }

    @Test func encoderTakesOneFrameAtATime() throws {
        let encoder = try OpusEncoder(profile: .microphone)
        #expect(throws: OpusCodecError.wrongSampleCount(expected: 960, got: 959)) {
            try encoder.encode([Float](repeating: 0, count: 959))
        }
    }

    @Test func microphoneRoundTripKeepsASine() throws {
        let profile = OpusEncoder.Profile.microphone
        let encoder = try OpusEncoder(profile: profile)
        let decoder = try OpusDecoder(channels: 1)

        // One second of a 1 kHz sine at -12 dBFS.
        let amplitude = pow(10.0, -12.0 / 20.0)
        let input = (0..<profile.sampleRate).map { index in
            Float(amplitude * sin(2 * Double.pi * 1000 * Double(index) / Double(profile.sampleRate)))
        }

        var packets: [Data] = []
        var output: [Float] = []
        for start in stride(from: 0, to: input.count, by: profile.frameSamples) {
            let packet = try encoder.encode(Array(input[start..<start + profile.frameSamples]))
            packets.append(packet)
            output += try decoder.decode(packet)
        }
        #expect(packets.count == 50, "50 packets for one second")
        #expect(packets.allSatisfy { $0.count <= 1000 }, "largest packet \(packets.map(\.count).max() ?? 0) bytes")
        #expect(output.count == input.count)

        // Align for the codec's delay, then measure after the first 100 ms,
        // where the encoder is still settling. The delay is the lag, within
        // one frame, that best matches a 100 ms window; the SNR is then taken
        // over the rest of the second at that lag.
        let settle = profile.sampleRate / 10
        func snr(delay: Int, count: Int) -> Double {
            var signal = 0.0
            var noise = 0.0
            for index in settle..<settle + count {
                let x = Double(input[index])
                let difference = Double(output[index + delay]) - x
                signal += x * x
                noise += difference * difference
            }
            return 10 * log10(signal / max(noise, .leastNonzeroMagnitude))
        }
        let window = profile.sampleRate / 10
        let delay = (0..<profile.frameSamples).max {
            snr(delay: $0, count: window) < snr(delay: $1, count: window)
        } ?? 0
        let best = snr(delay: delay, count: input.count - settle - profile.frameSamples)
        #expect(best >= 20, "SNR \(best) dB at a delay of \(delay) samples")
    }
}
