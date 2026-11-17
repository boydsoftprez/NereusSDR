// NereusSDR for iOS: the waterfall's nearest-row search, against the walk row by row it replaced
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-11-15: J.J. Boyd (KG4VCF), AI-assisted via Claude Code (Opus 5.5):
//   halving to the nearest row must give the same rows as walking from the newest.
import Foundation
import Testing
@testable import NereusBand

/// ``WaterfallTimeAxis/nearestAge(at:history:)`` and
/// ``WaterfallTimeAxis/fillAges(history:heightPixels:scale:into:)`` find
/// the nearest row by halving. Here they are checked against the walks
/// they replaced, kept below as they were, over seeded histories: in and
/// out of order, wrapped round the ring, near and far apart in time, with
/// and without periods and breaks, at many times and looking far back.
@Suite struct WaterfallTimeAxisSearchTests {
    private static let levels = WaterfallHistory.Levels(lowDbm: -100, highDbm: 0)

    // MARK: The walks as they were

    private static func walkedNearestAge(_ axis: WaterfallTimeAxis, at time: Double,
                                         history: WaterfallHistory) -> Int? {
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

    private static func walkedValid(_ axis: WaterfallTimeAxis, age: Int, at time: Double,
                                    history: WaterfallHistory) -> Bool {
        guard let here = history.presentationTime(age: age) else { return false }
        if let period = history.presentationPeriod(age: age) {
            let neighbor = time < here ? age + 1 : age - 1
            if history.presentationTime(age: neighbor) != nil {
                if !history.presentationBreakBefore(age: min(age, neighbor)) { return true }
                let boundaryPeriod = max(period, history.presentationPeriod(age: neighbor) ?? period)
                return abs(time - here) <= boundaryPeriod / 2 + 0.000000001
            }
            return abs(time - here) <= period / 2 + 0.000000001
        }
        let half = history.presentationTime(age: age - 1).map { max(0, $0 - here) / 2 } ?? axis.secondsPerPoint / 2
        let older = history.presentationTime(age: age + 1)
        return time <= (history.presentationTime(age: 0) ?? here)
            && (older.map { $0 <= here } ?? (time >= here - half))
    }

    private static func walkedFillAges(_ axis: WaterfallTimeAxis, history: WaterfallHistory, heightPixels: Int,
                                       scale: Double) -> [UInt32] {
        let height = max(0, heightPixels)
        var rows = Array(repeating: WaterfallTimeAxis.blank, count: height)
        guard scale.isFinite, scale > 0, axis.topTime.isFinite, axis.secondsPerPoint.isFinite,
              axis.secondsPerPoint > 0 else {
            return rows
        }
        guard history.hasPresentationTimeline else {
            for y in rows.indices { rows[y] = y < history.count ? UInt32(y) : WaterfallTimeAxis.blank }
            return rows
        }
        var age = 0
        for y in rows.indices {
            let time = axis.topTime - (Double(y) + 0.5) / scale * axis.secondsPerPoint
            while age + 1 < history.count {
                guard let newer = history.presentationTime(age: age), let older = history.presentationTime(age: age + 1),
                      older <= newer else { break }
                if time >= older + (newer - older) / 2 { break }
                age += 1
            }
            rows[y] = walkedValid(axis, age: age, at: time, history: history) ? UInt32(age) : WaterfallTimeAxis.blank
        }
        return rows
    }

    // MARK: Seeded histories

    private struct Seeded {
        var state: UInt64
        mutating func next() -> UInt64 {
            state = state &* 6_364_136_223_846_793_005 &+ 1_442_695_040_888_963_407
            return state >> 33
        }
        mutating func unit() -> Double { Double(next() % 1_000_000) / 1_000_000 }
        mutating func chance(_ share: Double) -> Bool { unit() < share }
    }

    /// A history of `lines` rows in a ring of `capacity`, starting at `base`.
    private static func history(seed: UInt64, capacity: Int, lines: Int, base: Double,
                                disorder: Double = 0) -> WaterfallHistory {
        var random = Seeded(state: seed)
        var history = WaterfallHistory(capacity: capacity)
        var time = base
        for index in 0..<lines {
            // Mostly steady steps, some a few units of the last place
            // (where a midpoint rounds onto a row's own time), some long gaps.
            let pick = random.unit()
            let step = pick < 0.05 ? time.ulp * Double(1 + random.next() % 3)
                : pick < 0.1 ? 2 + random.unit() * 30 : 0.02 + random.unit() * 0.03
            time += random.chance(disorder) ? -step : step
            let period: Double? = random.chance(0.6) ? 0.01 + random.unit() * 0.05 : nil
            history.append(line: [Float(index % 100) - 100], levels: levels, time: 1_700_000_000 + time,
                           presentationTime: time, presentationPeriod: period,
                           presentationBreakBefore: random.chance(0.1))
        }
        return history
    }

    /// Times to ask for in `history`: every row's, every midpoint, a little
    /// either side of each, past both ends, and some at random.
    private static func times(in history: WaterfallHistory, seed: UInt64) -> [Double] {
        var random = Seeded(state: seed)
        var times: [Double] = []
        for age in 0..<history.count {
            guard let here = history.presentationTime(age: age) else { continue }
            times += [here, here.nextUp, here.nextDown]
            if let older = history.presentationTime(age: age + 1) {
                let middle = older + (here - older) / 2
                times += [middle, middle.nextUp, middle.nextDown]
            }
        }
        if let newest = history.presentationTime(age: 0),
           let oldest = history.presentationTime(age: history.count - 1) {
            times += [newest + 1, oldest - 1, newest + 1_000, oldest - 1_000]
            for _ in 0..<64 {
                times.append(min(oldest, newest) + random.unit() * abs(newest - oldest))
            }
        }
        return times + [.nan, .infinity, -.infinity]
    }

    private static let shapes: [(capacity: Int, lines: Int)] = [
        (1, 1), (1, 5), (2, 2), (2, 7), (8, 3), (8, 8), (8, 21), (64, 64), (64, 200), (512, 1_300), (2_048, 2_048),
    ]

    @Test func theNearestRowIsTheOneTheWalkFound() {
        var checked = 0
        for (index, shape) in Self.shapes.enumerated() {
            for base in [0.0, 1_000.0, 1_700_000_000.0] {
                for disorder in [0.0, 0.02] {
                    let seed = UInt64(index * 97 + 13) &+ UInt64(base) &+ (disorder > 0 ? 7 : 0)
                    let history = Self.history(seed: seed, capacity: shape.capacity, lines: shape.lines, base: base,
                                               disorder: disorder)
                    let axis = WaterfallTimeAxis(topTime: history.presentationTime(age: 0) ?? 0, periodMs: 30)
                    for time in Self.times(in: history, seed: seed) {
                        let found = axis.nearestAge(at: time, history: history)
                        let walked = Self.walkedNearestAge(axis, at: time, history: history)
                        #expect(found == walked, "capacity \(shape.capacity), \(shape.lines) lines, time \(time)")
                        #expect(axis.age(at: time, history: history)
                            == walked.flatMap { Self.walkedValid(axis, age: $0, at: time, history: history) ? $0 : nil })
                        checked += 1
                    }
                }
            }
        }
        #expect(checked > 10_000)
    }

    @Test func everyPixelRowsAgeIsTheOneTheWalkFound() {
        for (index, shape) in Self.shapes.enumerated() {
            for disorder in [0.0, 0.02] {
                let seed = UInt64(index * 31 + 5) &+ (disorder > 0 ? 3 : 0)
                let history = Self.history(seed: seed, capacity: shape.capacity, lines: shape.lines, base: 1_000,
                                           disorder: disorder)
                guard let newest = history.presentationTime(age: 0) else { continue }
                // Live at the newest, a glide short of it, looking back to
                // several rows and to past the oldest.
                var tops = [newest, newest + 0.012]
                for age in [1, history.count / 3, history.count / 2, history.count - 1, history.count + 5] {
                    if let time = history.presentationTime(age: min(age, history.count - 1)) {
                        tops.append(age >= history.count ? time - 5 : time)
                    }
                }
                for top in tops {
                    for periodMs in [1, 10, 30, 100] {
                        for scale in [1.0, 2.0, 3.0] {
                            let axis = WaterfallTimeAxis(topTime: top, periodMs: periodMs)
                            var rows: [UInt32] = []
                            axis.fillAges(history: history, heightPixels: 300, scale: scale, into: &rows)
                            let walked = Self.walkedFillAges(axis, history: history, heightPixels: 300, scale: scale)
                            #expect(rows == walked,
                                    "capacity \(shape.capacity), \(shape.lines) lines, top \(top), \(periodMs) ms, x\(scale)")
                        }
                    }
                }
            }
        }
    }

    @Test func aMapReusedFromAnotherHeightMatchesTheWalk() {
        let history = Self.history(seed: 99, capacity: 512, lines: 900, base: 50)
        let newest = history.presentationTime(age: 0) ?? 0
        var rows: [UInt32] = Array(repeating: 7, count: 40)
        for (height, top) in [(120, newest), (40, newest - 3), (0, newest), (500, newest - 0.5)] {
            let axis = WaterfallTimeAxis(topTime: top, periodMs: 20)
            axis.fillAges(history: history, heightPixels: height, scale: 3, into: &rows)
            #expect(rows == Self.walkedFillAges(axis, history: history, heightPixels: height, scale: 3))
        }
    }
}
