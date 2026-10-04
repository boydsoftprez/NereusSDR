// NereusSDR for iOS: deterministic device render timeline regressions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusMedia

@Suite struct PlaybackRenderTimelineTests {
    @Test func rollbackAboveInitialSampleIsUnavailable() {
        var timeline = PlaybackRenderTimeline()
        #expect(timeline.progress(sampleTime: 1_000, hostTime: 10, rateHz: 48_000) == 0)
        #expect(timeline.progress(sampleTime: 49_000, hostTime: 20, rateHz: 48_000) == 48_000)
        #expect(timeline.progress(sampleTime: 25_000, hostTime: 30, rateHz: 48_000) == nil)
        #expect(timeline.progress(sampleTime: 50_000, hostTime: 40, rateHz: 48_000) == nil,
                "a discontinuous output epoch cannot silently recover")
        timeline.reset()
        #expect(timeline.progress(sampleTime: 50_000, hostTime: 40, rateHz: 48_000) == 0)
    }

    @Test func changedRateOrHostRollbackEndsEpoch() {
        var rate = PlaybackRenderTimeline()
        #expect(rate.progress(sampleTime: 100, hostTime: 100, rateHz: 48_000) == 0)
        #expect(rate.progress(sampleTime: 200, hostTime: 200, rateHz: 44_100) == nil)
        var host = PlaybackRenderTimeline()
        #expect(host.progress(sampleTime: 100, hostTime: 100, rateHz: 48_000) == 0)
        #expect(host.progress(sampleTime: 200, hostTime: 90, rateHz: 48_000) == nil)
    }
}
