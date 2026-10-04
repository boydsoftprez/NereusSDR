// NereusSDR for iOS: the rate matcher's length and smoothness, its steering, and the playback pull path through it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import AVFoundation
import Foundation
import LinkTestSupport
import Testing
@testable import NereusMedia

/// Spec section 4.1: the phone plays the Core's audio at the phone's own
/// clock. No test here starts an audio engine; the pull path is driven by
/// hand into preallocated buffers.
@Suite struct DriftResamplerTests {
    /// Runs `input` (interleaved stereo) through `resampler`, `chunk`
    /// output frames at a time, until the input runs dry, and returns the
    /// frames made from input.
    static func resample(_ input: [Float], with resampler: DriftResampler, chunk: Int) async -> [Float] {
        var output: [Float] = []
        var read = 0
        let buffer = UnsafeMutablePointer<Float>.allocate(capacity: chunk * 2)
        defer { buffer.deallocate() }
        while true {
            let made = resampler.render(into: buffer, frames: chunk) { destination, capacity in
                let frames = min(capacity, input.count / 2 - read)
                input.withUnsafeBufferPointer { source in
                    if frames > 0 {
                        destination.update(from: source.baseAddress! + read * 2, count: frames * 2)
                    }
                }
                read += frames
                return frames
            }
            output.append(contentsOf: UnsafeBufferPointer(start: buffer, count: made * 2))
            // Keep each render and append together before suspending.
            await Task.yield()
            if made < chunk {
                return output
            }
        }
    }

    /// The largest step between successive frames, over both channels.
    static func largestStep(_ samples: [Float]) async -> Float {
        var largest: Float = 0
        for index in 2..<samples.count {
            largest = max(largest, abs(samples[index] - samples[index - 2]))
            if (index - 1) % 512 == 0 {
                await Task.yield()
            }
        }
        return largest
    }

    @Test func heldAtOnePointZeroZeroZeroTwoForSixtySecondsTheLengthIsRightAndSmooth() async {
        let frames = 60 * 48_000
        var input = [Float](repeating: 0, count: frames * 2)
        for frame in 0..<frames {
            let t = Double(frame) / 48_000
            input[frame * 2] = Float(0.5 * sin(2 * .pi * 1_000 * t))
            input[frame * 2 + 1] = Float(0.25 * sin(2 * .pi * 3_100 * t + 1))
            // Both channels of this generation chunk are committed.
            if (frame + 1) % 256 == 0 {
                await Task.yield()
            }
        }
        let resampler = DriftResampler(channels: 2, inputChunkFrames: 256)
        resampler.setRatio(1.0002)
        #expect(resampler.ratio == 1.0002)
        let output = await Self.resample(input, with: resampler, chunk: 509)
        // 1.0002 output frames per input frame.
        let expected = Double(frames) * 1.0002
        #expect(abs(Double(output.count / 2) - expected) <= 1, "\(output.count / 2) against \(expected)")
        // Straight-line interpolation between neighbours: no output step
        // is larger than the input's largest.
        let outputStep = await Self.largestStep(output)
        let inputStep = await Self.largestStep(input)
        #expect(outputStep <= inputStep + 1e-6)
        // The first frame is the first input frame.
        #expect(output[0] == input[0] && output[1] == input[1])
    }

    @Test func theRatioStaysWithinOnePartInAThousand() {
        let resampler = DriftResampler()
        resampler.setRatio(1.5)
        #expect(resampler.ratio == 1.001)
        resampler.setRatio(0.5)
        #expect(resampler.ratio == 0.999)
        resampler.setRatio(.nan)
        #expect(resampler.ratio == 0.999)
        resampler.setRatio(1)

        // More audio than the target consumes faster (fewer frames made per
        // frame taken), less consumes slower, and no error moves it past the bound.
        let over = DriftResampler()
        for _ in 0..<1000 {
            over.steer(depthMs: 500, targetMs: 180, frames: 512)
        }
        #expect(over.ratio == 0.999)
        let under = DriftResampler()
        for _ in 0..<1000 {
            under.steer(depthMs: 20, targetMs: 180, frames: 512)
        }
        #expect(under.ratio == 1.001)
        // 20 ms over, held long enough to settle: 1 - 20 x 0.00001.
        let slight = DriftResampler()
        for _ in 0..<4000 {
            slight.steer(depthMs: 200, targetMs: 180, frames: 480)
        }
        #expect(abs(slight.ratio - 0.9998) < 1e-9)
    }

    @Test func aDrySourceGivesSilenceAndCarriesOn() {
        let resampler = DriftResampler()
        var fed = 0
        let buffer = UnsafeMutablePointer<Float>.allocate(capacity: 64)
        defer { buffer.deallocate() }
        buffer.update(repeating: 7, count: 64)
        let made = resampler.render(into: buffer, frames: 32) { destination, capacity in
            let frames = min(capacity, 10 - fed)
            for index in 0..<(frames * 2) {
                destination[index] = 1
            }
            fed += frames
            return frames
        }
        // Ten input frames at ratio 1 make nine output frames; the tenth
        // waits for its right-hand neighbour.
        #expect(made == 9)
        #expect((0..<18).allSatisfy { buffer[$0] == 1 })
        #expect((18..<64).allSatisfy { buffer[$0] == 0 })
    }

    @Test func prefetchedInputIncludesUnrenderedChunkAndFractionalPair() {
        let resampler = DriftResampler(channels: 2, inputChunkFrames: 8)
        resampler.setRatio(1.0002)
        let output = UnsafeMutablePointer<Float>.allocate(capacity: 6)
        defer { output.deallocate() }
        var offered = false
        let made = resampler.render(into: output, frames: 3) { destination, capacity in
            guard !offered else { return 0 }
            offered = true
            for index in 0..<(capacity * 2) { destination[index] = Float(index) }
            return capacity
        }
        #expect(made == 3)
        #expect(resampler.prefetchedInputFrames > 5)
        #expect(resampler.prefetchedInputFrames < 5.01)
    }

    // MARK: The playback pull path

    @Test func packetsPlayFromTheBufferThroughTheResamplerIntoAPreallocatedOutput() throws {
        let vectors = try LinkFixtureLoader.mediaVectors()
        var fixtures: [RtpPacket] = []
        for index in 1...4 {
            let vector = try #require(vectors["media-opus-\(index)"])
            fixtures.append(try #require(RtpPacket(parsing: vector.bytes)))
        }
        let core = try AudioPlaybackCore()
        let ssrc: UInt32 = 0x0102_0304
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: ssrc, firstSequence: 10, firstTimestamp: 0))
        // Twelve packets (480 ms) cycling through the four vectors' payloads.
        for index in 0..<12 {
            let fixture = fixtures[index % 4]
            core.receive(RtpPacket(payloadType: 111, sequence: UInt16(10 + index),
                                   timestamp: UInt32(index * 1920), ssrc: ssrc, payload: fixture.payload))
        }
        core.flush()
        #expect(core.counters == AudioPlaybackCore.Counters(underruns: 0, lateDrops: 0, concealed: 0))

        let frames = 256
        let output = UnsafeMutablePointer<Float>.allocate(capacity: frames * 2)
        defer { output.deallocate() }
        var rendered: [Float] = []
        // 1.5 s of output, pumped as the feed timer would between renders.
        for _ in 0..<(72_000 / frames) {
            core.pumpNow()
            core.render(into: output, frames: frames)
            rendered.append(contentsOf: UnsafeBufferPointer(start: output, count: frames * 2))
        }
        #expect(rendered.contains { $0 != 0 }, "the decoded audio came through")
        #expect(rendered.allSatisfy { $0.isFinite && abs($0) <= 2 })
        let ratio = core.resamplerRatio
        #expect(ratio >= 0.999 && ratio <= 1.001)
        // The first packet's arrival found the ring empty, so two silent
        // blocks went in while the buffer filled. The first pull after that
        // found 480 ms held, cut it to the 180 ms target rounded up to five
        // packets (seven dropped), and played; after those five the buffer
        // ran dry: one underrun.
        #expect(core.counters == AudioPlaybackCore.Counters(underruns: 1, lateDrops: 7, concealed: 0))
    }

    @Test func theSourceNodesRenderFillsEachChannelOfItsBufferList() throws {
        let core = try AudioPlaybackCore()
        #expect(core.makeSourceNode() != nil)
        let format = try #require(AVAudioFormat(standardFormatWithSampleRate: 48_000, channels: 2))
        let buffer = try #require(AVAudioPCMBuffer(pcmFormat: format, frameCapacity: 2_000))
        let payload = try AudioJitterBufferTests.opusPayload()
        core.reanchor(AudioStreamAnchor(generation: 1, ssrc: 9, firstSequence: 0, firstTimestamp: 0))
        for sequence in UInt16(0)..<5 {
            core.receive(AudioJitterBufferTests.packet(sequence, ssrc: 9, payload: payload))
        }
        core.flush()
        var heard = false
        // The render block's path: more frames than one scratch piece, into
        // one buffer per channel.
        for _ in 0..<10 {
            core.pumpNow()
            core.render(frames: 2_000, into: buffer.mutableAudioBufferList)
            let left = try #require(buffer.floatChannelData?[0])
            let right = try #require(buffer.floatChannelData?[1])
            heard = heard || (0..<2_000).contains { left[$0] != 0 || right[$0] != 0 }
        }
        #expect(heard)
    }

    @Test func withoutAStreamThePullPathIsSilence() throws {
        let core = try AudioPlaybackCore()
        let output = UnsafeMutablePointer<Float>.allocate(capacity: 1024)
        defer { output.deallocate() }
        output.update(repeating: 3, count: 1024)
        core.pumpNow()
        core.render(into: output, frames: 512)
        #expect((0..<1024).allSatisfy { output[$0] == 0 })
        #expect(core.resamplerRatio == 1)
    }
}
