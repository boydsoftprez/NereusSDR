// NereusSDR for iOS: the 3D view's stacked trace: its shape by angle, its floor, height and colour, where each row lands
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation

/// How the spectrum is drawn (the desktop's `DisplaySpectrumRenderMode`):
/// the flat trace, or the stack of recent traces seen in perspective.
public enum SpectrumView: Int, Equatable, Sendable, Codable, CaseIterable {
    case flat = 0
    case stacked = 1
}

/// The 3D view (Display V12's 3D View page, JJ's board of 2026-09-29): the
/// spectrum above the frequency scale becomes a stack of the last
/// ``visibleRows`` waterfall lines, the newest across the front, older ones
/// leaning back and narrowing. Only the spectrum changes: the waterfall,
/// the scales, the strip, the flags and the markers stay flat.
///
/// Written for this phone from the behaviour the Core's V12 description and
/// the desktop's design document give; the numbers are the desktop's values
/// (D83), the code is this app's own (D4).
public enum StackedTrace {
    /// The rows seen, front to back, and the one more kept so the back edge
    /// stays filled while the stack glides to the next line.
    public static let visibleRows = 96
    public static let keptRows = visibleRows + 1

    /// Fade toward the background with depth, at the back row.
    public static let haze = 0.16
    /// Colour reads through a fixed 45 dB above the floor, so a high scale
    /// top does not turn every signal blue.
    public static let colourSpanDb = 45.0
    /// The height curve: a strength's height is its share to this power.
    /// Straight, as JJ's approved board draws the stack (the desktop lifts
    /// the band just over the floor with 0.6; on the board's example
    /// signals that stood the noise up as dense spikes that hid the rows).
    public static let heightCurve = 1.0
    /// The fill under each row's ridge is the ridge's colour at this share
    /// over the background; the ridge line itself is the whole colour.
    public static let fillShare = 0.5

    /// The 3D Angle's curves: the back row's width and the rise of the back
    /// row's foot, from angle 0 to angle 100, and the tallest ridge allowed.
    public static let backWidthAtZero = 0.35
    public static let backWidthAtHundred = 0.85
    public static let depthSpanAtZero = 0.36
    public static let depthSpanAtHundred = 0.80
    public static let largestRidge = 0.75
    /// The classic angle's shape (angle 50): back width 0.60, rise 0.58,
    /// ridge 0.46. Every angle's ridge keeps the same headroom it has.
    static let classicBackWidth = 0.60
    static let classicDepthSpan = 0.58
    static let classicRidge = 0.46
    static let ridgeHeadroom = classicRidge * classicBackWidth / (1 - classicDepthSpan)

    /// The widest row the phone asks the Core to cover, in the pan's
    /// widths: the widest angle's (angle 0) back row reaching both edges.
    /// This is the subscription's `wideSpanFactor` in 3D, as the Core's V12
    /// description defines it.
    public static let widestRowSpan = 1 / backWidthAtZero

    /// Slice Shadow: how dark a passband goes on the surface, and how
    /// strong its centre cue is in the slice's colour.
    public static let shadowStrength = 0.42
    public static let shadowCueStrength = 0.36

    /// The most samples one stored row keeps across its window: the
    /// board's 457 across the widest row, about 160 across the pan, so each
    /// row reads as a trace rather than a comb of single-sample spikes.
    public static let maxColumns = 457

    /// The five settings' defaults and limits (the Core's V12 rows).
    public static let floorDefaultDb = 6
    public static let gainDefault = 70
    public static let spanDefault = 100
    public static let angleDefault = 50
    public static let floorRange: ClosedRange<Int> = 0...24
    public static let percentRange: ClosedRange<Int> = 0...100

    /// The colour curve for 3D Gain: 4^((50 - gain) / 50). Gain 50 is
    /// straight, 100 lifts colour down to the floor (0.25), 0 keeps it for
    /// the strongest signals only (4).
    public static func gamma(gain: Int) -> Double {
        let clamped = Double(min(max(gain, percentRange.lowerBound), percentRange.upperBound))
        return pow(4, (50 - clamped) / 50)
    }

    /// Where the surface starts: the noise floor less 3D Floor.
    public static func floorDbm(noiseFloorDbm: Double, floorDepthDb: Int) -> Double {
        noiseFloorDbm - Double(min(max(floorDepthDb, floorRange.lowerBound), floorRange.upperBound))
    }

    /// The dB the front row's tallest ridge stands for: the dBm scale's
    /// range to the nearest half dB, at least 1.
    public static func heightRangeDb(scale: ClosedRange<Double>) -> Double {
        let span = scale.upperBound - scale.lowerBound
        guard span.isFinite else {
            return 1
        }
        return max(1, (span * 2).rounded() / 2)
    }

    /// The dB colour spans: the height range, never more than ``colourSpanDb``.
    public static func colourRangeDb(heightRangeDb: Double) -> Double {
        max(1, min(heightRangeDb, colourSpanDb))
    }

    /// How wide the near rows run, in the pan's widths: 1 (the classic
    /// narrowing trapezoid) at 3D Span 0, up to what the Core's spectrum
    /// beside the pan covers (`availableFactor`, its wide span over the
    /// pan's), never past where the back row reaches both edges.
    public static func rowSpan(spanPercent: Int, shape: StackedTraceShape, availableFactor: Double) -> Double {
        guard availableFactor.isFinite, availableFactor > 1 else {
            return 1
        }
        let available = min(availableFactor, 1 / shape.backWidth)
        let fraction = Double(min(max(spanPercent, percentRange.lowerBound), percentRange.upperBound)) / 100
        return 1 + fraction * (available - 1)
    }
}

/// The stack's shape for a 3D Angle: the back row's width as a share of
/// the front's, how far up the plot the back row's foot rises, and the
/// tallest front ridge as a share of the plot's height.
public struct StackedTraceShape: Equatable, Sendable {
    public var backWidth: Double
    public var depthSpan: Double
    public var ridge: Double

    public init(backWidth: Double, depthSpan: Double, ridge: Double) {
        self.backWidth = backWidth
        self.depthSpan = depthSpan
        self.ridge = ridge
    }

    /// The shape at `percent`, 0 (looking along the traces) to 100 (looking
    /// down on them). The ridge follows the angle so a back row's peak never
    /// leaves the top of the plot.
    public static func forAngle(_ percent: Int) -> StackedTraceShape {
        let t = Double(min(max(percent, 0), 100)) / 100
        let back = StackedTrace.backWidthAtZero + t * (StackedTrace.backWidthAtHundred - StackedTrace.backWidthAtZero)
        let rise = StackedTrace.depthSpanAtZero + t * (StackedTrace.depthSpanAtHundred - StackedTrace.depthSpanAtZero)
        let ridge = min(StackedTrace.largestRidge, StackedTrace.ridgeHeadroom * (1 - rise) / back)
        return StackedTraceShape(backWidth: back, depthSpan: rise, ridge: ridge)
    }

    /// A row's width at `depth` (0 the front, 1 the back) as a share of the front's.
    public func depthScale(_ depth: Double) -> Double {
        1 - min(max(depth, 0), 1) * (1 - backWidth)
    }
}

/// Where the stack's rows land in the spectrum's plot, in pixels from its
/// top-left corner, for one set of settings: what the renderer's shader
/// computes for each vertex, here for the tests and the dBm scale.
public struct StackedTraceGeometry: Equatable, Sendable {
    public let size: CGSize
    public let shape: StackedTraceShape
    /// The surface's foot, in dBm.
    public let floorDbm: Double
    /// The dB the front's tallest ridge stands for, and the dB colour spans.
    public let heightRangeDb: Double
    public let colourRangeDb: Double
    /// The colour curve for 3D Gain.
    public let gamma: Double

    /// The plot `size` for `settings`, over the Core's `noiseFloorDbm`, with
    /// the dBm scale's `scale` setting the height range.
    public init(size: CGSize, settings: BandDisplaySettings, noiseFloorDbm: Double, scale: ClosedRange<Double>) {
        self.size = size
        shape = StackedTraceShape.forAngle(settings.threeDAngle)
        floorDbm = StackedTrace.floorDbm(noiseFloorDbm: noiseFloorDbm, floorDepthDb: settings.threeDFloorDb)
        heightRangeDb = StackedTrace.heightRangeDb(scale: scale)
        colourRangeDb = StackedTrace.colourRangeDb(heightRangeDb: heightRangeDb)
        gamma = StackedTrace.gamma(gain: settings.threeDGain)
    }

    /// The depth of the row `age` lines old, `glide` of the way to the next
    /// line (0 just after a line arrived, towards 1 before the next).
    public static func depth(age: Int, glide: Double) -> Double {
        (Double(age) + min(max(glide, 0), 1)) / Double(StackedTrace.visibleRows)
    }

    /// Where a frequency lands across the plot at `depth`: `unit` is its
    /// place in the pan, 0 at its left edge and 1 at its right.
    public func x(unit: Double, depth: Double) -> Double {
        Double(size.width) * (0.5 + (unit - 0.5) * shape.depthScale(depth))
    }

    /// A row's foot at `depth`.
    public func baselineY(depth: Double) -> Double {
        Double(size.height) * (1 - min(max(depth, 0), 1) * shape.depthSpan)
    }

    /// A level's height as a share of the tallest ridge, 0 to 1.
    public func heightShare(dbm: Double) -> Double {
        guard dbm.isFinite else {
            return 0
        }
        let linear = min(max((dbm - floorDbm) / heightRangeDb, 0), 1)
        return pow(linear, StackedTrace.heightCurve)
    }

    /// The top of the ridge at `dbm` in the row at `depth`.
    public func ridgeY(dbm: Double, depth: Double) -> Double {
        baselineY(depth: depth) - heightShare(dbm: dbm) * shape.ridge * shape.depthScale(depth) * Double(size.height)
    }

    /// A level's place in the palette, 0 at the floor to 1 at the colour span's top.
    public func colourPosition(dbm: Double) -> Double {
        guard dbm.isFinite else {
            return 0
        }
        return pow(min(max((dbm - floorDbm) / colourRangeDb, 0), 1), gamma)
    }

    /// The dBm scale in 3D: levels every `stepDb` from the floor up to the
    /// height range's top, each at its height on the front row.
    public func scaleTicks(stepDb: Double) -> [(dbm: Double, y: Double)] {
        guard stepDb > 0 else {
            return []
        }
        var ticks: [(dbm: Double, y: Double)] = []
        var level = (floorDbm / stepDb).rounded(.up) * stepDb
        while level <= floorDbm + heightRangeDb {
            ticks.append((level, ridgeY(dbm: level, depth: 0)))
            level += stepDb
        }
        return ticks
    }
}

/// What a pan asks the Core for in 3D (JJ's board, recommendation 5): the
/// spectrum beside the pan only while 3D is drawn and 3D Span is above 0,
/// and not while the band saves data (Saver, or cellular without Full).
public extension BandDisplaySettings {
    /// The subscription's `wideSpanFactor`: the Core's 3D factor, or 0.
    func wideSpanFactor(drawsStack: Bool, savesData: Bool) -> Double {
        guard drawsStack, threeDSpan > 0, !savesData else {
            return 0
        }
        return StackedTrace.widestRowSpan
    }
}
