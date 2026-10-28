// NereusSDR for iOS: the waterfall's visual time axis, independent of received RF row cadence
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex, 2026-10-28.
// 2026-10-28: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
//   use proven adjacent capture continuity without masking real capture gaps.

import Foundation

/// One requested period per logical point. All times stay Double on the CPU; the GPU gets only original row IDs.
public struct WaterfallTimeAxis: Equatable, Sendable {
    public var topTime: Double
    public var secondsPerPoint: Double
    public static let blank = UInt32.max

    public init(topTime: Double, periodMs: Int) {
        self.topTime = topTime
        secondsPerPoint = Double(max(1, periodMs)) / 1000
    }

    /// The live view lags one real observation, gliding only between known times. Starvation never invents future data.
    public static func liveTop(history: WaterfallHistory, progress: Double) -> Double? {
        guard history.hasPresentationTimeline, let newest = history.presentationTime(age: 0) else { return nil }
        guard let previous = history.presentationTime(age: 1), previous <= newest else { return newest }
        return previous + (newest - previous) * min(max(progress.isFinite ? progress : 1, 0), 1)
    }

    public func yPoints(age: Int, history: WaterfallHistory) -> Double? {
        guard history.hasPresentationTimeline, secondsPerPoint > 0, let time = history.presentationTime(age: age) else { return nil }
        return (topTime - time) / secondsPerPoint
    }

    /// The closest real observation; the edge ends half an actual interval after the oldest row.
    public func age(at time: Double, history: WaterfallHistory) -> Int? {
        guard let age = nearestAge(at: time, history: history) else { return nil }
        return valid(age: age, at: time, history: history) ? age : nil
    }

    public func nearestAge(at time: Double, history: WaterfallHistory) -> Int? {
        guard history.hasPresentationTimeline, time.isFinite, history.count > 0 else { return nil }
        var age = 0
        while age + 1 < history.count {
            guard let newer = history.presentationTime(age: age), let older = history.presentationTime(age: age + 1),
                  older <= newer else { break }
            if time >= older + (newer - older) / 2 { break }
            age += 1
        }
        return age
    }

    private func valid(age: Int, at time: Double, history: WaterfallHistory) -> Bool {
        guard let here = history.presentationTime(age: age) else { return false }
        if let period = history.presentationPeriod(age: age) {
            let neighbor = time < here ? age + 1 : age - 1
            if history.presentationTime(age: neighbor) != nil {
                // Consecutive advancing captures may land either side of a
                // nominal slot on the Core's FFT sample grid. Their midpoint
                // cells meet; no observation or captured timestamp is added.
                if history.presentationContinuous(age: min(age, neighbor)) { return true }
                // At a cadence transition the slower adjacent period owns
                // this shared boundary. Larger actual capture gaps stay empty.
                let boundaryPeriod = max(period, history.presentationPeriod(age: neighbor) ?? period)
                return abs(time - here) <= boundaryPeriod / 2 + 0.000000001
            }
            return abs(time - here) <= period / 2 + 0.000000001
        }
        let half = history.presentationTime(age: age - 1).map { max(0, $0 - here) / 2 } ?? secondsPerPoint / 2
        let older = history.presentationTime(age: age + 1)
        return time <= (history.presentationTime(age: 0) ?? here)
            && (older.map { $0 <= here } ?? (time >= here - half))
    }

    /// One ordered walk through output pixels and original rows. Reuses the caller's storage each frame.
    public func fillAges(history: WaterfallHistory, heightPixels: Int, scale: Double, into rows: inout [UInt32]) {
        let height = max(0, heightPixels)
        if rows.count != height { rows = Array(repeating: Self.blank, count: height) }
        guard scale.isFinite, scale > 0, topTime.isFinite, secondsPerPoint.isFinite, secondsPerPoint > 0 else {
            for y in rows.indices { rows[y] = Self.blank }
            return
        }
        guard history.hasPresentationTimeline else {
            for y in rows.indices { rows[y] = y < history.count ? UInt32(y) : Self.blank }
            return
        }
        var age = 0
        for y in rows.indices {
            let time = topTime - (Double(y) + 0.5) / scale * secondsPerPoint
            while age + 1 < history.count {
                guard let newer = history.presentationTime(age: age), let older = history.presentationTime(age: age + 1),
                      older <= newer else { break }
                if time >= older + (newer - older) / 2 { break }
                age += 1
            }
            rows[y] = valid(age: age, at: time, history: history) ? UInt32(age) : Self.blank
        }
    }
}
