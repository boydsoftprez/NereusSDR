// NereusSDR for iOS: bounded time series for connection diagnostics
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The closed inventory of measurements graphed by connection diagnostics.
public enum DiagnosticsMetric: Int, CaseIterable, Hashable, Sendable {
    case radioRxMbps, radioTxMbps, radioRttMs
    case sessionPayloadRxKbps, sessionPayloadTxKbps, sessionRttMs
    case audioSourceFramesPerSecond, audioEncodedPacketsPerSecond
    case audioSendAcceptedPerSecond, audioSendRejectedPerSecond, audioSourceDropsPerSecond
    case playbackDecodedPacketsPerSecond, playbackConcealedPacketsPerSecond
    case playbackLatePacketsPerSecond, playbackUnderflowsPerSecond
    case playbackOverflowsPerSecond, playbackPacketAgeMs
    case coreGuiRxKbps, coreGuiTxKbps, coreGuiTotalKbps
    case audioRtpRxKbps, audioPayloadRxKbps, speakerBufferMs
    case coreSystemCpuPercent, coreProcessCpuPercent
    case coreMemoryAvailableMiB, coreProcessResidentMiB, coreHottestZoneCelsius
    case coreReceiverLoadPercentSlot0, coreReceiverLoadPercentSlot1
    case coreReceiverLoadPercentSlot2, coreReceiverLoadPercentSlot3
    case coreReceiverLoadPercentSlot4
    case audioDelayMs, audioDelayAccuracyMs, audioDeliveryDelayMs

    public static func receiverLoad(slot: Int) -> DiagnosticsMetric? {
        guard (0..<5).contains(slot) else { return nil }
        return DiagnosticsMetric(rawValue: coreReceiverLoadPercentSlot0.rawValue + slot)
    }
}

public enum DiagnosticsRange: Int, CaseIterable, Sendable {
    case oneMinute = 60
    case fiveMinutes = 300
    case fifteenMinutes = 900
    case oneHour = 3_600
    case oneDay = 86_400
    case sevenDays = 604_800

    public var seconds: Int { rawValue }

    public var bucketMilliseconds: Int64 {
        if seconds <= 300 { return 1_000 }
        return max(5_000, Int64(seconds) * 1_000 / 300)
    }

    public var maximumPointCount: Int {
        Int(Int64(seconds) * 1_000 / bucketMilliseconds) + 1
    }
}

/// A caller's monotonic time and logical Core/session identity identify one observation.
/// An updated metric absent from `values` is missing; an untouched metric is unaffected.
public struct DiagnosticsSample: Sendable {
    public let monotonicMilliseconds: Int64
    public let sessionSegment: UInt64
    public let updated: Set<DiagnosticsMetric>
    public let values: [DiagnosticsMetric: Double]
    public let connected: Bool
    public let stale: Bool

    public init(monotonicMilliseconds: Int64, sessionSegment: UInt64,
                updated: Set<DiagnosticsMetric>, values: [DiagnosticsMetric: Double],
                connected: Bool = true, stale: Bool = false) {
        self.monotonicMilliseconds = monotonicMilliseconds
        self.sessionSegment = sessionSegment
        self.updated = updated
        self.values = values
        self.connected = connected
        self.stale = stale
    }
}

public struct DiagnosticsPoint: Equatable, Sendable {
    public let seconds: Double
    public let value: Double
    public let breakBefore: Bool
    public let sessionSegment: UInt64
}

public struct DiagnosticsSeries: Sendable {
    public let points: [DiagnosticsPoint]
    public let maximumConnectGapSeconds: Double
}

private struct HistoryQueue<Element> {
    private var elements: [Element] = []
    private var firstIndex = 0

    var count: Int { elements.count - firstIndex }
    var first: Element? { firstIndex < elements.count ? elements[firstIndex] : nil }
    var last: Element? { count > 0 ? elements.last : nil }
    var live: ArraySlice<Element> { elements[firstIndex...] }

    mutating func append(_ element: Element) { elements.append(element) }

    mutating func replaceLast(_ element: Element) {
        guard count > 0 else { return }
        elements[elements.count - 1] = element
    }

    @discardableResult mutating func popFirst() -> Element? {
        guard let value = first else { return nil }
        firstIndex += 1
        if firstIndex >= 256 && firstIndex * 2 >= elements.count {
            elements.removeFirst(firstIndex)
            firstIndex = 0
        }
        return value
    }
}

private struct HistoryRecord {
    var time: Int64
    var segment: UInt64
    var mean: Double
    var weight: Int
    var breakBefore: Bool
    var invalid: Bool = false
}

private struct MetricHistory {
    var raw = HistoryQueue<HistoryRecord>()
    var minutes = HistoryQueue<HistoryRecord>()
    var lastValidTime: Int64?
    var lastSegment: UInt64?
    var pendingBreak = false
}

/// One bounded history per fixed metric. The owner supplies time and session identity.
public struct DiagnosticsHistory {
    public static let rawRetentionMilliseconds: Int64 = 3_600_000
    public static let minuteRetentionMilliseconds: Int64 = 604_800_000
    public static let rawCapacity = 3_600
    public static let minuteCapacity = 10_080

    private var metrics = Array(repeating: MetricHistory(), count: DiagnosticsMetric.allCases.count)
    private var latestTime: Int64?

    public init() {}

    public mutating func breakMetric(_ metric: DiagnosticsMetric) {
        metrics[metric.rawValue].pendingBreak = true
    }

    public mutating func append(_ sample: DiagnosticsSample) {
        if let latestTime, sample.monotonicMilliseconds < latestTime { return }
        latestTime = sample.monotonicMilliseconds
        for metric in DiagnosticsMetric.allCases {
            update(&metrics[metric.rawValue], metric: metric, sample: sample)
        }
    }

    private func update(_ history: inout MetricHistory, metric: DiagnosticsMetric,
                        sample: DiagnosticsSample) {
        defer { prune(&history, at: sample.monotonicMilliseconds) }
        if !sample.connected {
            history.pendingBreak = true
            return
        }
        guard sample.updated.contains(metric) else { return }
        guard !sample.stale, let value = sample.values[metric], value.isFinite else {
            history.pendingBreak = true
            return
        }
        let changedSegment = history.lastSegment.map { $0 != sample.sessionSegment } ?? false
        let sourceGap = history.lastValidTime.map {
            sample.monotonicMilliseconds - $0 > 3_000
        } ?? false
        history.raw.append(HistoryRecord(time: sample.monotonicMilliseconds,
                                         segment: sample.sessionSegment, mean: value,
                                         weight: 1,
                                         breakBefore: history.pendingBreak || changedSegment || sourceGap))
        history.lastValidTime = sample.monotonicMilliseconds
        history.lastSegment = sample.sessionSegment
        history.pendingBreak = false
    }

    public func rawObservationCount(_ metric: DiagnosticsMetric) -> Int {
        metrics[metric.rawValue].raw.count
    }

    public func minuteObservationCount(_ metric: DiagnosticsMetric) -> Int {
        metrics[metric.rawValue].minutes.count
    }

    private func prune(_ history: inout MetricHistory, at now: Int64) {
        let rawCutoff = now - Self.rawRetentionMilliseconds
        while let first = history.raw.first,
              first.time < rawCutoff || history.raw.count > Self.rawCapacity {
            let evicted = history.raw.popFirst()!
            insertMinute(evicted, into: &history)
        }
        let minuteCutoff = now - Self.minuteRetentionMilliseconds
        while let first = history.minutes.first, first.time < minuteCutoff {
            history.minutes.popFirst()
        }
        while history.minutes.count > Self.minuteCapacity {
            history.minutes.popFirst()
        }
    }

    private func insertMinute(_ raw: HistoryRecord, into history: inout MetricHistory) {
        let minuteStart = raw.time - raw.time % 60_000
        var incoming = raw
        incoming.time = minuteStart
        guard var previous = history.minutes.last, previous.time == minuteStart else {
            if history.minutes.last?.invalid == true { incoming.breakBefore = true }
            history.minutes.append(incoming)
            return
        }
        if previous.invalid { return }
        if incoming.breakBefore || previous.segment != incoming.segment {
            previous.invalid = true
            previous.weight = 0
            previous.breakBefore = true
        } else {
            previous.mean = Self.weighted(previous.mean, previous.weight, incoming.mean, incoming.weight)
            previous.weight += incoming.weight
        }
        history.minutes.replaceLast(previous)
    }

    private static func weighted(_ left: Double, _ leftWeight: Int,
                                 _ right: Double, _ rightWeight: Int) -> Double {
        let total = Double(leftWeight) + Double(rightWeight)
        return left * (Double(leftWeight) / total) + right * (Double(rightWeight) / total)
    }

    public func series(_ metric: DiagnosticsMetric, at now: Int64,
                       range: DiagnosticsRange) -> DiagnosticsSeries {
        let period = range.bucketMilliseconds
        let end = period == 1_000 ? now : now - now % period
        let cutoff = end - Int64(range.seconds) * 1_000
        let history = metrics[metric.rawValue]
        var records: [HistoryRecord] = []
        records.reserveCapacity(history.raw.count + history.minutes.count)
        for var minute in history.minutes.live {
            minute.time += 30_000
            if minute.time >= cutoff && minute.time <= end { records.append(minute) }
        }
        for raw in history.raw.live where raw.time >= cutoff && raw.time <= end {
            records.append(raw)
        }
        records.sort { $0.time < $1.time }

        var points: [DiagnosticsPoint] = []
        points.reserveCapacity(min(records.count, range.maximumPointCount))
        var bucket: Int64?
        var segment: UInt64 = 0
        var mean = 0.0
        var weight = 0
        var broken = false
        var invalid = false
        var nextBreak = false

        func emit() {
            guard let bucket else { return }
            let centre = period == 1_000 ? bucket : bucket + period / 2
            if invalid || weight == 0 || centre < cutoff || centre > end {
                nextBreak = true
            } else {
                points.append(DiagnosticsPoint(seconds: Double(centre - cutoff) / 1_000,
                                               value: mean, breakBefore: broken || nextBreak,
                                               sessionSegment: segment))
                nextBreak = false
            }
        }

        for record in records {
            let start = record.time - record.time % period
            if start != bucket {
                emit()
                bucket = start
                segment = record.segment
                mean = 0
                weight = 0
                broken = record.breakBefore
                invalid = record.invalid
            } else if record.invalid || record.breakBefore || record.segment != segment {
                invalid = true
            }
            if !invalid {
                mean = weight == 0 ? record.mean : Self.weighted(mean, weight, record.mean, record.weight)
                weight += record.weight
            }
        }
        emit()
        return DiagnosticsSeries(points: points,
                                 maximumConnectGapSeconds: Double(period) * 3 / 1_000)
    }
}
