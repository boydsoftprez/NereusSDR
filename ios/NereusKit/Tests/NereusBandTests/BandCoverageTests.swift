// NereusSDR for iOS: a frame's coverage, each way between a frequency and a sample
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-11-03: Overlay redesign D23, the reverse mapping Your pan needs.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import NereusBand
import Testing

@Suite("Band coverage")
struct BandCoverageTests {
    private let coverage = BandCoverage(centerHz: 7_200_000, spanHz: 48_000)

    @Test("sample(forHz:) round-trips hz(forSample:) within one sample")
    func roundTrips() throws {
        for samples in [1, 2, 7, 1_206, 4_096] {
            for index in 0..<samples {
                let hz = coverage.hz(forSample: index, samples: samples)
                let back = try #require(coverage.sample(forHz: hz, samples: samples))
                #expect(abs(back - index) <= 1)
            }
        }
    }

    @Test("both edges are inside, and the top edge is the last sample")
    func edges() {
        #expect(coverage.sample(forHz: coverage.lowHz, samples: 100) == 0)
        #expect(coverage.sample(forHz: coverage.highHz, samples: 100) == 99)
    }

    @Test("nil outside the coverage, and with nothing to map to")
    func outside() {
        #expect(coverage.sample(forHz: coverage.lowHz - 1, samples: 100) == nil)
        #expect(coverage.sample(forHz: coverage.highHz + 1, samples: 100) == nil)
        #expect(coverage.sample(forHz: coverage.centerHz, samples: 0) == nil)
        #expect(BandCoverage(centerHz: 7_200_000, spanHz: 0).sample(forHz: 7_200_000, samples: 100) == nil)
    }
}
