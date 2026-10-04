// NereusSDR for iOS: bounded diagnostics history behavior
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusMirror

@Suite struct DiagnosticsHistoryTests {
    private let metric = DiagnosticsMetric.radioRxMbps

    private func sample(_ time: Int64, _ segment: UInt64 = 1, _ values: [DiagnosticsMetric: Double],
                        updated: Set<DiagnosticsMetric>? = nil, connected: Bool = true,
                        stale: Bool = false) -> DiagnosticsSample {
        DiagnosticsSample(monotonicMilliseconds: time, sessionSegment: segment,
                          updated: updated ?? Set(values.keys), values: values,
                          connected: connected, stale: stale)
    }

    @Test func zeroIsMeasuredWhileMissingAndNonfiniteBreakThePath() {
        var history = DiagnosticsHistory()
        history.append(sample(0, 1, [metric: 0]))
        history.append(sample(1_000, 1, [:], updated: [metric]))
        history.append(sample(2_000, 1, [metric: .nan]))
        history.append(sample(3_000, 1, [metric: 4]))
        let points = history.series(metric, at: 3_000, range: .fiveMinutes).points
        #expect(points.map(\.value) == [0, 4])
        #expect(points[1].breakBefore)
    }

    @Test func partialUpdateLeavesOtherSeriesContinuous() {
        var history = DiagnosticsHistory()
        let other = DiagnosticsMetric.radioTxMbps
        history.append(sample(0, 1, [metric: 1, other: 2]))
        history.append(sample(1_000, 1, [metric: 3]))
        history.append(sample(2_000, 1, [other: 4]))
        let points = history.series(other, at: 2_000, range: .oneMinute).points
        #expect(points.map(\.value) == [2, 4])
        #expect(!points[1].breakBefore)
    }

    @Test func sourceGapBoundaryAndOldCallback() {
        var history = DiagnosticsHistory()
        history.append(sample(0, 1, [metric: 1]))
        history.append(sample(3_000, 1, [metric: 2]))
        history.append(sample(4_000, 1, [metric: 3]))
        history.append(sample(1_000, 1, [metric: 99]))
        history.append(sample(7_001, 1, [metric: 4]))
        let points = history.series(metric, at: 7_001, range: .fiveMinutes).points
        #expect(points.map(\.value) == [1, 2, 3, 4])
        #expect(points.map(\.breakBefore) == [false, false, false, true])
    }

    @Test func changedSessionAndRapidReconnectRetainDistinctPaths() {
        var history = DiagnosticsHistory()
        history.append(sample(0, 1, [metric: 1]))
        history.append(sample(1_000, 1, [metric: 2]))
        history.append(sample(2_000, 2, [metric: 3]))
        history.append(sample(3_000, 3, [metric: 4]))
        history.breakMetric(metric)
        history.append(sample(4_000, 3, [metric: 5]))
        let points = history.series(metric, at: 4_000, range: .oneMinute).points
        #expect(points.map(\.breakBefore) == [false, false, true, true, true])
    }

    @Test func disconnectedAndStaleObservationsBreakAndPrune() {
        var history = DiagnosticsHistory()
        history.append(sample(0, 1, [metric: 1]))
        history.append(sample(1_000, 1, [:], connected: false))
        history.append(sample(2_000, 1, [metric: 2]))
        history.append(sample(3_000, 1, [:], updated: [metric], stale: true))
        history.append(sample(4_000, 1, [metric: 3]))
        let points = history.series(metric, at: 4_000, range: .oneMinute).points
        #expect(points.map(\.breakBefore) == [false, true, true])
        history.append(sample(DiagnosticsHistory.minuteRetentionMilliseconds + 5_000, 1, [:], connected: false))
        #expect(history.rawObservationCount(metric) == 0)
        #expect(history.minuteObservationCount(metric) == 0)
    }

    @Test func unequalWeightsSurviveRawToMinuteCompaction() {
        var history = DiagnosticsHistory()
        history.append(sample(58_000, 1, [metric: 10]))
        history.append(sample(59_000, 1, [metric: 20]))
        history.append(sample(60_000, 1, [metric: 100]))
        history.append(sample(3_660_001, 1, [metric: 1]))
        let points = history.series(metric, at: 3_660_001, range: .sevenDays).points
        #expect(points.contains { abs($0.value - (130.0 / 3.0)) < 0.0001 })
        #expect(history.minuteObservationCount(metric) == 2)
    }

    @Test func largeFiniteInputsRemainFinite() {
        var history = DiagnosticsHistory()
        history.append(sample(0, 1, [metric: 1e308]))
        history.append(sample(1_000, 1, [metric: 1e308]))
        history.append(sample(3_600_001, 1, [metric: 1e308]))
        let points = history.series(metric, at: 3_600_001, range: .sevenDays).points
        #expect(points.allSatisfy { $0.value.isFinite })
        #expect(points.contains { $0.value == 1e308 })
    }

    @Test func oppositeLargeFiniteInputsDoNotOverflow() {
        var history = DiagnosticsHistory()
        history.append(sample(0, 1, [metric: Double.greatestFiniteMagnitude]))
        history.append(sample(1_000, 1, [metric: -Double.greatestFiniteMagnitude]))
        history.append(sample(3_600_001, 1, [metric: 0]))
        let points = history.series(metric, at: 3_600_001, range: .sevenDays).points
        #expect(points.allSatisfy { $0.value.isFinite })
        #expect(points.contains { abs($0.value) < 1 })
    }

    @Test func exactRawAndMinuteEdgesAreRetained() {
        var history = DiagnosticsHistory()
        history.append(sample(0, 1, [metric: 1]))
        history.append(sample(3_600_000, 1, [metric: 2]))
        #expect(history.rawObservationCount(metric) == 2)
        history.append(sample(3_600_001, 1, [metric: 3]))
        #expect(history.rawObservationCount(metric) == 2)
        #expect(history.minuteObservationCount(metric) == 1)
        history.append(sample(DiagnosticsHistory.minuteRetentionMilliseconds, 1, [metric: 4]))
        #expect(history.minuteObservationCount(metric) >= 1)
        history.append(sample(DiagnosticsHistory.minuteRetentionMilliseconds + 30_001, 1, [metric: 5]))
        #expect(history.minuteObservationCount(metric) == 1)
    }

    @Test func memoryAndOutputStayBoundedAtHighCadenceAndOverSevenDays() {
        var history = DiagnosticsHistory()
        for index in 0..<12_000 {
            history.append(sample(Int64(index), UInt64(index % 4), [metric: Double(index)]))
        }
        #expect(history.rawObservationCount(metric) <= 3_600)
        #expect(history.minuteObservationCount(metric) <= 10_080)
        for range in DiagnosticsRange.allCases {
            let series = history.series(metric, at: 12_000, range: range)
            #expect(series.points.count <= range.maximumPointCount)
            #expect(series.points.allSatisfy { $0.seconds >= 0 && $0.seconds <= Double(range.seconds) })
        }
        history.append(sample(DiagnosticsHistory.minuteRetentionMilliseconds + 60_000, 2, [metric: 7]))
        #expect(history.minuteObservationCount(metric) <= 1)
        #expect(history.series(metric, at: DiagnosticsHistory.minuteRetentionMilliseconds + 60_000,
                               range: .oneMinute).points.last?.value == 7)
    }

    @Test func mixedMinuteAndQueryBucketsAreOmittedAndBreakFollowingPoint() {
        var history = DiagnosticsHistory()
        history.append(sample(0, 1, [metric: 1]))
        history.append(sample(1_000, 2, [metric: 2]))
        history.append(sample(60_000, 2, [metric: 3]))
        history.append(sample(3_601_001, 2, [metric: 4]))
        let points = history.series(metric, at: 3_601_001, range: .oneHour).points
        #expect(!points.contains { $0.value == 1 || $0.value == 2 })
        #expect(points.contains { $0.value == 3 && $0.breakBefore })
    }

    @Test func staleCoreUpdateDoesNotBreakFreshLocalSeries() {
        var history = DiagnosticsHistory()
        let local = DiagnosticsMetric.coreGuiRxKbps
        history.append(sample(0, 1, [metric: 1, local: 10]))
        history.append(sample(1_000, 1, [:], updated: [metric], stale: true))
        history.append(sample(2_000, 1, [metric: 2, local: 20]))
        let corePoints = history.series(metric, at: 2_000, range: .oneMinute).points
        let localPoints = history.series(local, at: 2_000, range: .oneMinute).points
        #expect(corePoints.last?.breakBefore == true)
        #expect(localPoints.last?.breakBefore == false)
        #expect(localPoints.last?.sessionSegment == 1)
    }

    @Test func metricInventoryAndAllRangesAreFixed() {
        #expect(DiagnosticsMetric.allCases.count == 36)
        #expect((0..<5).compactMap(DiagnosticsMetric.receiverLoad(slot:)).count == 5)
        #expect(DiagnosticsMetric.receiverLoad(slot: 5) == nil)
        #expect(DiagnosticsRange.allCases.map(\.seconds) == [60, 300, 900, 3_600, 86_400, 604_800])
        #expect(DiagnosticsRange.allCases.map(\.bucketMilliseconds) == [1_000, 1_000, 5_000,
                                                                          12_000, 288_000, 2_016_000])
    }

    @Test func everyRangeReturnsARealPointWithWindowRelativeTime() {
        for range in DiagnosticsRange.allCases {
            var history = DiagnosticsHistory()
            let period = range.bucketMilliseconds
            history.append(sample(period * 5, 7, [metric: 12]))
            let points = history.series(metric, at: period * 10, range: range).points
            #expect(points.count == 1)
            #expect(points.first?.value == 12)
            #expect(points.first?.sessionSegment == 7)
            #expect(points.allSatisfy { $0.seconds >= 0 && $0.seconds <= Double(range.seconds) })
        }
    }

    @Test func queryBucketCrossingSessionsIsOmitted() {
        var history = DiagnosticsHistory()
        history.append(sample(0, 1, [metric: 1]))
        history.append(sample(1_000, 2, [metric: 2]))
        history.append(sample(5_000, 2, [metric: 3]))
        let points = history.series(metric, at: 10_000, range: .fifteenMinutes).points
        #expect(points.map(\.value) == [3])
        #expect(points[0].breakBefore)
        #expect(points[0].sessionSegment == 2)
    }
}
