// NereusSDR for iOS: where a frequency, a dBm level and a trace sample sit on the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation

/// The band's axes (R-IOS-11): frequency across, from the low edge of the
/// span at x 0 to the high edge at the width, and dBm down, from the top of
/// the range at y 0 to its bottom at the height. The units are whatever
/// `size` is in: the renderer works in pixels, the screens above it in points.
///
/// The centre and span are the endpoint's context (the Core's `centreHz`
/// and `spanHz`), so every trace sample the Core sends covers an equal slice
/// of the span, and sample `i` of `n` sits at the centre of its slice.
public struct BandGeometry: Equatable, Sendable {
    public var centerHz: Double
    public var spanHz: Double
    public var size: CGSize
    /// The dBm at the bottom (lower bound) and at the top (upper bound).
    public var dbmRange: ClosedRange<Double>

    /// The endpoint width the band asks for: its width in pixels, within the
    /// media control document's 1 to 4096.
    public static let requestablePixels = 1...4096

    public init(centerHz: Double, spanHz: Double, size: CGSize, dbmRange: ClosedRange<Double>) {
        self.centerHz = centerHz
        self.spanHz = spanHz
        self.size = size
        self.dbmRange = dbmRange
    }

    public var lowHz: Double { centerHz - spanHz / 2 }
    public var highHz: Double { centerHz + spanHz / 2 }

    public func x(forHz hz: Double) -> Double {
        guard spanHz > 0 else {
            return Double(size.width) / 2
        }
        return (hz - lowHz) / spanHz * Double(size.width)
    }

    public func hz(forX x: Double) -> Double {
        guard size.width > 0 else {
            return centerHz
        }
        return lowHz + x / Double(size.width) * spanHz
    }

    public func y(forDbm dbm: Double) -> Double {
        let height = dbmRange.upperBound - dbmRange.lowerBound
        guard height > 0 else {
            return Double(size.height) / 2
        }
        return (dbmRange.upperBound - dbm) / height * Double(size.height)
    }

    public func dbm(forY y: Double) -> Double {
        guard size.height > 0 else {
            return dbmRange.upperBound
        }
        return dbmRange.upperBound - y / Double(size.height) * (dbmRange.upperBound - dbmRange.lowerBound)
    }

    /// The x of trace sample `index` of a trace `traceSamples` long: the
    /// centre of the slice of the span it covers. The spectrum grant can
    /// make `traceSamples` smaller than the width asked for, so a sample is
    /// never taken for a pixel.
    public func x(forTraceSample index: Int, traceSamples: Int) -> Double {
        guard traceSamples > 0 else {
            return Double(size.width) / 2
        }
        return (Double(index) + 0.5) / Double(traceSamples) * Double(size.width)
    }

    /// The frequency at the centre of trace sample `index`.
    public func hz(forTraceSample index: Int, traceSamples: Int) -> Double {
        guard traceSamples > 0 else {
            return centerHz
        }
        return lowHz + (Double(index) + 0.5) / Double(traceSamples) * spanHz
    }

    /// The same band with its span multiplied by `factor` about `hz`: the
    /// frequency under the fingers stays at the same x.
    public func zoomed(by factor: Double, about hz: Double) -> BandGeometry {
        guard factor.isFinite, factor > 0 else {
            return self
        }
        var zoomed = self
        zoomed.spanHz = spanHz * factor
        // hz keeps its fraction of the width: lowHz' = hz - f * (hz - lowHz).
        let low = hz - factor * (hz - lowHz)
        zoomed.centerHz = low + zoomed.spanHz / 2
        return zoomed
    }

    /// The endpoint width for a band `widthPixels` wide.
    public static func requestedPixels(forWidthPixels widthPixels: Double) -> Int {
        let rounded = widthPixels.isFinite ? Int(widthPixels.rounded()) : requestablePixels.lowerBound
        return min(max(rounded, requestablePixels.lowerBound), requestablePixels.upperBound)
    }

    /// The frequencies of the vertical grid lines: a 1, 2 or 5 step (times a
    /// power of ten) that keeps them at least `minimumSpacing` apart, from
    /// the first multiple of the step inside the span.
    public func frequencyTicks(minimumSpacing: Double) -> (stepHz: Double, ticks: [Double]) {
        guard spanHz > 0, size.width > 0, minimumSpacing > 0 else {
            return (0, [])
        }
        let wanted = spanHz * minimumSpacing / Double(size.width)
        let step = Self.niceStep(atLeast: wanted)
        var ticks: [Double] = []
        var tick = (lowHz / step).rounded(.up) * step
        while tick <= highHz, ticks.count < 1000 {
            ticks.append(tick)
            tick += step
        }
        return (step, ticks)
    }

    /// The dBm levels of the horizontal grid lines, every `stepDb` from the
    /// first multiple inside the range, top first.
    public func dbmTicks(stepDb: Double) -> [Double] {
        guard stepDb > 0 else {
            return []
        }
        var ticks: [Double] = []
        var level = (dbmRange.upperBound / stepDb).rounded(.down) * stepDb
        while level >= dbmRange.lowerBound, ticks.count < 1000 {
            ticks.append(level)
            level -= stepDb
        }
        return ticks
    }

    /// A frequency scale label: megahertz with three decimals, or more
    /// when the grid step is finer than a kilohertz.
    public static func frequencyLabel(hz: Double, stepHz: Double) -> String {
        var decimals = 3
        if stepHz > 0, stepHz < 1000 {
            decimals = min(6, Int((-log10(stepHz / 1_000_000)).rounded(.up)))
        }
        return String(format: "%.\(decimals)f", hz / 1_000_000)
    }

    /// The smallest 1, 2 or 5 times a power of ten at or above `value`.
    static func niceStep(atLeast value: Double) -> Double {
        guard value.isFinite, value > 0 else {
            return 1
        }
        let power = pow(10, (log10(value)).rounded(.down))
        for multiple in [1.0, 2.0, 5.0, 10.0] where multiple * power >= value * (1 - 1e-12) {
            return multiple * power
        }
        return 10 * power
    }
}
