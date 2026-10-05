// NereusSDR for iOS: deterministic clock exchange and measured audio delay contract tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusMedia

@Suite struct AudioClockModelTests {
    private static let ms: Int64 = 1_000_000
    private static let second: Int64 = 1_000_000_000

    private func exchange(sent: Int64, offset: Int64 = 5_000_000_000,
                          forward: Int64, held: Int64 = 0, back: Int64) -> AudioClockSample {
        AudioClockSample(t0Ns: sent, t1Ns: sent + forward + offset,
                         t2Ns: sent + forward + held + offset,
                         t3Ns: sent + forward + held + back)
    }

    private func accept(_ clock: inout AudioClockEstimator, _ sample: AudioClockSample) -> Bool {
        clock.add(sample)
    }

    @Test func symmetricAndAsymmetricClockRoutes() throws {
        var clock = AudioClockEstimator()
        #expect(accept(&clock, exchange(sent: 10 * Self.second, forward: 12 * Self.ms, back: 12 * Self.ms)))
        let symmetric = try #require(clock.offset(atLocalNs: 10 * Self.second + 24 * Self.ms))
        #expect(symmetric.offsetNs == 5 * Self.second)
        #expect(symmetric.roundTripNs == 24 * Self.ms)
        #expect(symmetric.boundNs(atLocalNs: symmetric.sampleNs) == 12 * Double(Self.ms) + 2_400)

        #expect(accept(&clock, exchange(sent: 11 * Self.second, forward: 2 * Self.ms, back: 8 * Self.ms)))
        let asymmetric = try #require(clock.offset(atLocalNs: 11 * Self.second + 10 * Self.ms))
        #expect(asymmetric.offsetNs == 5 * Self.second - 3 * Self.ms)
        #expect(asymmetric.roundTripNs == 10 * Self.ms)
    }

    @Test func selectionWindowFreshnessAndEqualRoundTrips() throws {
        var clock = AudioClockEstimator()
        #expect(accept(&clock, exchange(sent: 0, forward: Self.ms, back: Self.ms)))
        #expect(accept(&clock, exchange(sent: Self.second, forward: Self.ms, back: Self.ms)))
        #expect(clock.offset(atLocalNs: Self.second + 2 * Self.ms)?.sampleNs == Self.second + 2 * Self.ms)
        #expect(clock.offset(atLocalNs: Self.second + 2 * Self.ms + 3 * Self.second) != nil)
        #expect(clock.offset(atLocalNs: Self.second + 2 * Self.ms + 3 * Self.second + 1) == nil)
        #expect(clock.offset(atLocalNs: Self.second) == nil)

        for index in 2...16 {
            #expect(accept(&clock, exchange(sent: Int64(index) * Self.second,
                                       forward: 10 * Self.ms, back: 10 * Self.ms)))
        }
        #expect(clock.offset(atLocalNs: 16 * Self.second + 20 * Self.ms)?.roundTripNs == 2 * Self.ms)
        for index in 17...18 {
            #expect(accept(&clock, exchange(sent: Int64(index) * Self.second,
                                       forward: 10 * Self.ms, back: 10 * Self.ms)))
        }
        #expect(clock.offset(atLocalNs: 18 * Self.second + 20 * Self.ms)?.roundTripNs == 20 * Self.ms)
        #expect(clock.sampleCount <= 17)
    }

    @Test func rejectsBadAndBackwardsEchoWithoutPoisoningState() {
        var clock = AudioClockEstimator()
        #expect(accept(&clock, exchange(sent: 1_000, forward: 100, back: 100)))
        let original = clock.offset(atLocalNs: 1_200)
        #expect(!accept(&clock, AudioClockSample(t0Ns: 2_000, t1Ns: 3_000, t2Ns: 4_000, t3Ns: 1_999)))
        #expect(!accept(&clock, AudioClockSample(t0Ns: 2_000, t1Ns: 3_100, t2Ns: 3_000, t3Ns: 2_200)))
        #expect(!accept(&clock, AudioClockSample(t0Ns: 2_000, t1Ns: 3_000, t2Ns: 4_000, t3Ns: 2_200)))
        #expect(!accept(&clock, AudioClockSample(t0Ns: 0, t1Ns: Int64.max, t2Ns: Int64.max, t3Ns: Int64.max - 1)))
        #expect(!accept(&clock, exchange(sent: 0, forward: 100, back: 100)))
        #expect(clock.sampleCount == 1)
        #expect(clock.offset(atLocalNs: 1_200) == original)
        clock.reset()
        #expect(clock.offset(atLocalNs: 1_200) == nil)
    }

    @Test func boundedFloodPreservesBestAtNormalCadence() {
        var clock = AudioClockEstimator()
        for index in 0..<1_000 {
            let path = index == 8 ? 5 * Self.ms : 10 * Self.ms
            #expect(accept(&clock, exchange(sent: Int64(index) * 10 * Self.ms,
                                       forward: path, back: path)))
        }
        #expect(clock.sampleCount <= AudioClockEstimator.maximumSamples)
        #expect(clock.offset(atLocalNs: 10 * Self.second + 10 * Self.ms)?.roundTripNs == 10 * Self.ms)
        #expect(clock.offset(atLocalNs: 10 * Self.second + 10 * Self.ms + AudioClockEstimator.echoStaleNs + 1) == nil)
    }

    @Test func inclusiveOldestBoundaryThenExpires() {
        var clock = AudioClockEstimator()
        #expect(accept(&clock, exchange(sent: 0, forward: Self.ms, back: Self.ms)))
        #expect(accept(&clock, exchange(sent: 16 * Self.second - 18 * Self.ms,
                                        forward: 10 * Self.ms, back: 10 * Self.ms)))
        #expect(clock.offset(atLocalNs: 16 * Self.second + 2 * Self.ms)?.roundTripNs == 2 * Self.ms)
        #expect(clock.offset(atLocalNs: 16 * Self.second + 2 * Self.ms + 1)?.roundTripNs == 20 * Self.ms)
    }

    @Test func largeUptimeKeepsNanosecondDifference() throws {
        var clock = AudioClockEstimator()
        let base: Int64 = 8_000_000_000_000_000_000
        #expect(accept(&clock, AudioClockSample(t0Ns: base, t1Ns: base + 1_013,
                                          t2Ns: base + 1_113, t3Ns: base + 200)))
        let result = try #require(clock.offset(atLocalNs: base + 200))
        #expect(result.offsetNs == 963)
        #expect(result.roundTripNs == 100)
    }

    private func inputs(rtp: UInt32 = 480_000, generation: UInt32 = 7,
                        device: Int64? = nil, ratio: Double = 1,
                        release: AudioReleasePoint? = nil) -> AudioDelayInputs {
        let offset = AudioClockOffset(offsetNs: 5 * Self.second, roundTripNs: 4 * Self.ms,
                                      sampleNs: 19 * Self.second, exchangeNs: 4 * Self.ms)
        let point = AudioPlayoutPoint(rtpTimestamp: rtp, measuredNs: 20 * Self.second + 45 * Self.ms,
                                      matcherFillFrames: 480, speakerQueuedFrames: 960,
                                      deviceRateHz: 48_000, pipelineDelayFrames: 0,
                                      codecDelayFrames: 0, callbackFrames: 0,
                                      readWindowNs: 0, matcherRatio: ratio,
                                      deviceLatencyNs: device)
        return AudioDelayInputs(offset: offset,
                                capture: AudioCaptureAnchor(generation: generation,
                                                            rtpTimestamp: 480_000,
                                                            capturedNs: 25 * Self.second),
                                playingGeneration: generation, playout: point, release: release)
    }

    @Test func playoutAndReleaseAreDistinctMeasurements() throws {
        let released = AudioReleasePoint(rtpTimestamp: 480_000, releasedNs: 20 * Self.second + 75 * Self.ms)
        let estimate = try #require(measureAudioDelay(inputs(device: 15 * Self.ms, release: released)))
        #expect(abs(estimate.delayMs - 90) < 0.000001)
        #expect(abs((estimate.deliveryMs ?? 0) - 75) < 0.000001)
        #expect(estimate.includesDevice)
        #expect(estimate.boundMs >= 2)
        let withoutDevice = try #require(measureAudioDelay(inputs()))
        #expect(abs(withoutDevice.delayMs - 75) < 0.000001)
        #expect(!withoutDevice.includesDevice)
        #expect(withoutDevice.deliveryMs == nil)
    }

    @Test func deliverySurvivesUnavailableOutputWithoutChangingIntegratedResult() throws {
        let released = AudioReleasePoint(rtpTimestamp: 480_000, releasedNs: 20 * Self.second + 75 * Self.ms)
        let source = inputs(release: released)
        let independent = try #require(measureAudioDelivery(offset: source.offset, capture: source.capture,
                                                            playingGeneration: 7, release: released))
        let integrated = try #require(measureAudioDelay(source))
        #expect(independent.delayMs == integrated.deliveryMs)
        #expect(independent.boundMs == integrated.deliveryBoundMs)
        #expect(measureAudioDelay(AudioDelayInputs(offset: source.offset, capture: source.capture,
                                                  playingGeneration: 7, playout: nil,
                                                  release: released)) == nil)
        #expect(independent.delayMs == 75)
    }

    @Test func deliveryRejectsRetiredGenerationAndInvalidRelease() {
        let source = inputs()
        let good = AudioReleasePoint(rtpTimestamp: 480_000, releasedNs: 20 * Self.second + 75 * Self.ms)
        let tooFar = AudioReleasePoint(rtpTimestamp: 480_000 + 61 * 48_000,
                                       releasedNs: good.releasedNs)
        #expect(measureAudioDelivery(offset: source.offset, capture: source.capture,
                                     playingGeneration: 8, release: good) == nil)
        #expect(measureAudioDelivery(offset: source.offset, capture: source.capture,
                                     playingGeneration: 7, release: tooFar) == nil)
        #expect(measureAudioDelivery(offset: source.offset, capture: source.capture,
                                     playingGeneration: 7, release: nil) == nil)
    }

    @Test func driftAndAsymmetryBoundKnownTruth() throws {
        var clock = AudioClockEstimator()
        let t0 = 19 * Self.second
        let coreOffset = 5 * Self.second
        func core(_ local: Int64) -> Int64 { local + coreOffset + local / 10_000 }
        #expect(accept(&clock, AudioClockSample(t0Ns: t0,
                                                t1Ns: core(t0 + 2 * Self.ms),
                                                t2Ns: core(t0 + 2 * Self.ms),
                                                t3Ns: t0 + 8 * Self.ms)))
        let offset = try #require(clock.offset(atLocalNs: 20 * Self.second))
        let point = AudioPlayoutPoint(rtpTimestamp: 480_000, measuredNs: 20 * Self.second + 45 * Self.ms,
                                      matcherFillFrames: 480, speakerQueuedFrames: 960,
                                      deviceRateHz: 48_000, pipelineDelayFrames: 0,
                                      codecDelayFrames: 0, callbackFrames: 0,
                                      readWindowNs: 0, matcherRatio: 1, deviceLatencyNs: nil)
        let result = try #require(measureAudioDelay(AudioDelayInputs(offset: offset,
                                                                      capture: AudioCaptureAnchor(generation: 7,
                                                                                                  rtpTimestamp: 480_000,
                                                                                                  capturedNs: core(20 * Self.second)),
                                                                      playingGeneration: 7, playout: point,
                                                                      release: nil)))
        #expect(abs(result.delayMs - 75) <= result.boundMs)
        #expect(result.boundMs >= 4)
    }

    @Test func wrapBothDirectionsAndGenerationGates() throws {
        let start = inputs(rtp: 0)
        let forward = AudioDelayInputs(offset: start.offset,
                                       capture: AudioCaptureAnchor(generation: 7, rtpTimestamp: UInt32.max - 47_999,
                                                                   capturedNs: 25 * Self.second),
                                       playingGeneration: 7, playout: start.playout, release: nil)
        #expect(measureAudioDelay(forward) != nil)
        let other = inputs(rtp: UInt32.max - 47_999)
        let backward = AudioDelayInputs(offset: other.offset,
                                        capture: AudioCaptureAnchor(generation: 7, rtpTimestamp: 0,
                                                                    capturedNs: 25 * Self.second),
                                        playingGeneration: 7, playout: other.playout, release: nil)
        #expect(measureAudioDelay(backward) != nil)
        #expect(measureAudioDelay(AudioDelayInputs(offset: forward.offset, capture: forward.capture,
                                                  playingGeneration: 8, playout: forward.playout,
                                                  release: nil)) == nil)
        #expect(measureAudioDelay(inputs(rtp: 480_000 + 61 * 48_000)) == nil)
        let missing = inputs()
        #expect(measureAudioDelay(AudioDelayInputs(offset: missing.offset, capture: missing.capture,
                                                  playingGeneration: 7, playout: nil, release: nil)) == nil)
    }

    @Test func measuredPipelineDeviceRateAndMatcherRatio() throws {
        let base = inputs(ratio: 1.004)
        let fastPoint = AudioPlayoutPoint(rtpTimestamp: 480_000,
                                          measuredNs: 20 * Self.second,
                                          matcherFillFrames: 4_800, speakerQueuedFrames: 960,
                                          deviceRateHz: 48_000, pipelineDelayFrames: 381,
                                          codecDelayFrames: 312, callbackFrames: 48,
                                          readWindowNs: 2 * Self.ms, matcherRatio: 1.004,
                                          deviceLatencyNs: nil)
        #expect(fastPoint.playoutNs == 20 * Self.second + 128_437_500)
        #expect(fastPoint.accuracyNs == 1_500_000)
        let stretch = try #require(fastPoint.matcherStretchNs)
        #expect(stretch > 480_000 && stretch < 490_000)
        let fast = try #require(measureAudioDelay(AudioDelayInputs(offset: base.offset, capture: base.capture,
                                                                    playingGeneration: 7, playout: fastPoint,
                                                                    release: nil)))
        let steadyPoint = AudioPlayoutPoint(rtpTimestamp: 480_000, measuredNs: 20 * Self.second,
                                            matcherFillFrames: 4_800, speakerQueuedFrames: 960,
                                            deviceRateHz: 48_000, pipelineDelayFrames: 381,
                                            codecDelayFrames: 312, callbackFrames: 48,
                                            readWindowNs: 2 * Self.ms, matcherRatio: 1,
                                            deviceLatencyNs: nil)
        let steady = try #require(measureAudioDelay(AudioDelayInputs(offset: base.offset, capture: base.capture,
                                                                      playingGeneration: 7, playout: steadyPoint,
                                                                      release: nil)))
        #expect(abs((steady.delayMs - fast.delayMs) - Double(stretch) / 1e6) < 1e-6)
        let fasterDevice = AudioPlayoutPoint(rtpTimestamp: 480_000, measuredNs: 20 * Self.second,
                                              matcherFillFrames: 4_800, speakerQueuedFrames: 960,
                                              deviceRateHz: 96_000, pipelineDelayFrames: 381,
                                              codecDelayFrames: 312, callbackFrames: 48,
                                              readWindowNs: 2 * Self.ms, matcherRatio: 1.004,
                                              deviceLatencyNs: nil)
        #expect(try #require(fasterDevice.playoutNs) < #require(fastPoint.playoutNs))
    }

    @Test func invalidPlayoutAndRoundingAreUnavailable() throws {
        let base = inputs()
        for (ratio, window, fill, rate) in [(Double.nan, Int64(0), 0, 48_000),
                                            (1.0, -1, 0, 48_000),
                                            (1.0, 0, -1, 48_000),
                                            (1.0, 0, 0, 0)] {
            let point = AudioPlayoutPoint(rtpTimestamp: 480_000, measuredNs: 20 * Self.second,
                                          matcherFillFrames: fill, speakerQueuedFrames: 0,
                                          deviceRateHz: rate, pipelineDelayFrames: 0,
                                          codecDelayFrames: 0, callbackFrames: 0,
                                          readWindowNs: window, matcherRatio: ratio,
                                          deviceLatencyNs: nil)
            #expect(measureAudioDelay(AudioDelayInputs(offset: base.offset, capture: base.capture,
                                                       playingGeneration: 7, playout: point,
                                                       release: nil)) == nil)
        }
        #expect(roundAudioDelay(valueMs: 85.2, boundMs: 0.4) == AudioDelayDisplay(valueMs: 85, accuracyMs: 1))
        #expect(roundAudioDelay(valueMs: 85.6, boundMs: 1.3) == AudioDelayDisplay(valueMs: 86, accuracyMs: 2))
        #expect(roundAudioDelay(valueMs: -.infinity, boundMs: 1) == nil)
        #expect(roundAudioDelay(valueMs: 0, boundMs: .nan) == nil)
        #expect(roundAudioDelay(valueMs: Double(Int64.max), boundMs: 0) == nil)
        #expect(roundAudioDelay(valueMs: -0.6, boundMs: 0.2) == AudioDelayDisplay(valueMs: -1, accuracyMs: 1))
    }

    @Test func roundingKeepsExactBinaryIntervalOutsideIntegerEdge() {
        let justOverSixTenths = 0.6.nextUp
        #expect(roundAudioDelay(valueMs: 0.4, boundMs: justOverSixTenths)
                == AudioDelayDisplay(valueMs: 0, accuracyMs: 2))
        #expect(roundAudioDelay(valueMs: 0.4, boundMs: 0.6)
                == AudioDelayDisplay(valueMs: 0, accuracyMs: 1))
        #expect(roundAudioDelay(valueMs: 0.5, boundMs: 0.5)
                == AudioDelayDisplay(valueMs: 1, accuracyMs: 1))
    }

    @Test func malformedCallerValuesAndReleaseDoNotFabricateDelivery() throws {
        let base = inputs()
        let invalidOffset = AudioClockOffset(offsetNs: 5 * Self.second, roundTripNs: 1,
                                              sampleNs: 1, exchangeNs: 2)
        #expect(measureAudioDelay(AudioDelayInputs(offset: invalidOffset, capture: base.capture,
                                                   playingGeneration: 7, playout: base.playout,
                                                   release: nil)) == nil)
        let huge = AudioPlayoutPoint(rtpTimestamp: 480_000, measuredNs: 20 * Self.second,
                                     matcherFillFrames: Int.max, speakerQueuedFrames: 0,
                                     deviceRateHz: 48_000, pipelineDelayFrames: 0,
                                     codecDelayFrames: 0, callbackFrames: 0,
                                     readWindowNs: 0, matcherRatio: 1, deviceLatencyNs: nil)
        #expect(measureAudioDelay(AudioDelayInputs(offset: base.offset, capture: base.capture,
                                                   playingGeneration: 7, playout: huge,
                                                   release: nil)) == nil)
        let badRelease = AudioReleasePoint(rtpTimestamp: 480_000 + 61 * 48_000,
                                           releasedNs: 20 * Self.second + 50 * Self.ms)
        let result = try #require(measureAudioDelay(inputs(release: badRelease)))
        #expect(result.deliveryMs == nil)
        #expect(result.delayMs == 75)
    }

    @Test func boundedNegativeDelayIsPreserved() throws {
        let base = inputs()
        let point = AudioPlayoutPoint(rtpTimestamp: 480_000,
                                      measuredNs: 20 * Self.second - 31 * Self.ms,
                                      matcherFillFrames: 480, speakerQueuedFrames: 960,
                                      deviceRateHz: 48_000, pipelineDelayFrames: 0,
                                      codecDelayFrames: 0, callbackFrames: 0,
                                      readWindowNs: 0, matcherRatio: 1,
                                      deviceLatencyNs: nil)
        let result = try #require(measureAudioDelay(AudioDelayInputs(offset: base.offset, capture: base.capture,
                                                                      playingGeneration: 7, playout: point,
                                                                      release: nil)))
        #expect(result.delayMs == -1)
        #expect(result.boundMs >= 2)
    }

    @Test func newerRtpReleaseAfterOlderPlayoutStillMeasuresDelivery() throws {
        let newerRelease = AudioReleasePoint(rtpTimestamp: 481_920,
                                             releasedNs: 20 * Self.second + 80 * Self.ms)
        let result = try #require(measureAudioDelay(inputs(release: newerRelease)))
        #expect(result.delayMs == 75)
        #expect(result.deliveryMs == 40)
        #expect(result.deliveryBoundMs != nil)
    }
}
