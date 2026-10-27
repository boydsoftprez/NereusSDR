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

    /// The seconds between two waterfall lines: the Core's frames per line
    /// at its frames a second, as the desktop times a remote pan
    /// (`SpectrumWidget::waterfallRowPeriodMs`), or the Update period
    /// setting (`fallbackMs`) before the Core has said. Timing the glide
    /// from the 30 ms setting while the Core sent a line every 33.3 ms held
    /// the stack still for a tenth of each row (2026-10-24).
    public static func rowPeriodSeconds(fps: Int, framesPerLine: Int, fallbackMs: Int) -> Double {
        guard fps > 0, framesPerLine > 0 else {
            return Double(max(fallbackMs, 1)) / 1000
        }
        return Double(framesPerLine) / Double(fps)
    }

    /// How far the stack has glided toward its next row, `since` seconds
    /// after the newest line, for rows of `divider` lines `rowPeriod`
    /// seconds apart: 0 at the line, 1 when the next row is due.
    public static func glide(since seconds: Double, rowPeriod: Double, divider: Int) -> Double {
        let rowSeconds = rowPeriod * Double(max(divider, 1))
        guard rowSeconds > 0 else {
            return 0
        }
        return min(max(seconds / rowSeconds, 0), 1)
    }

    /// How far the stack has glided toward its next row while the lines go
    /// in by the row clock (``WaterfallRowClock``): `lines` put in since
    /// the newest row and the clock's `progress` toward the last, over
    /// `divider` lines a row, 0 to 1. A row completed by a line starts at 0
    /// as that line goes in, and each line moves the stack on by a
    /// `divider`th of a row by the time the next is due, so the glide comes
    /// from the clock rather than from when a line arrived.
    public static func glide(lines: Int, progress: Double, divider: Int) -> Double {
        let share = (Double(max(lines, 0)) + min(max(progress, 0), 1)) / Double(max(divider, 1))
        return min(max(share, 0), 1)
    }

    /// Fade toward the background with depth, at the back row.
    public static let haze = 0.16
    /// Colour reads through a fixed 45 dB above the floor, so a high scale
    /// top does not turn every signal blue.
    public static let colourSpanDb = 45.0
    /// The height curve: a strength's height is its share to this power.
    /// Below 1 it lifts the band just over the floor, as the desktop's 3D
    /// does (JJ, 2026-10-26: the phone's 3D should look and feel like the
    /// desktop's, so the board's straight 1.0 gave way).
    public static let heightCurve = 0.6
    /// Each row hangs a curtain from its ridge to the plot's floor. Its
    /// colour runs from the level's place in the colour span at the ridge
    /// down to this share of it at the floor, through the palette.
    public static let curtainFootColour = 0.6
    /// The rows' ends fade out over this many pixels, so the stacked ends
    /// do not stand as a hard comb against the dark outside the surface.
    /// The shader draws with the same number.
    public static let sideFadePixels = 8.0

    /// The ridge lines over the stack (the desktop's values): a light grey
    /// ``lineGrey`` over the background, at ``lineFarStrength`` at the back
    /// up to that plus ``lineNearExtra`` at the front, and at
    /// ``lineOverFloorShare`` of that where the level is within
    /// ``lineFloorSpan`` of the colour span over the floor. The shader
    /// draws with the same numbers.
    public static let lineGrey = 0.92
    public static let lineFarStrength = 0.12
    public static let lineNearExtra = 0.5
    public static let lineOverFloorShare = 0.42
    public static let lineFloorSpan = 0.035
    /// Each ridge line is a ribbon this many pixels either side of the
    /// ridge, its edges softened, so it is thin at any screen scale.
    public static let lineHalfWidthPixels = 1.0

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

    /// The most columns a stored row keeps across the pan: the desktop's
    /// 768. The spectrum beside the pan keeps the same density, so the
    /// noise floor and a narrow carrier read the same at every depth.
    public static let panColumns = 768

    /// The most samples one stored row keeps across its window:
    /// ``panColumns`` across each pan's width of the widest row, 2195 in
    /// all. With 768 across the whole widest row a pan had about 270, so
    /// each column took the peak of about five samples where the desktop's
    /// takes two or three, and the phone's noise floor read about 2 dB
    /// higher and its narrow carriers wider (2026-10-26).
    public static let maxColumns = Int((Double(panColumns) * widestRowSpan).rounded(.up))

    /// The columns the surface's mesh lays across a row `rowSpan` pans
    /// wide, for rows of `columns` across the widest row: as many as the
    /// rows hold across that span, so each step along the mesh moves at most
    /// one stored column and no column (no narrow carrier) is stepped over,
    /// and no more, so a narrower span draws fewer vertices. At least 2.
    public static func meshColumns(columns: Int, rowSpan: Double) -> Int {
        guard columns > 0, rowSpan.isFinite, rowSpan > 0 else {
            return max(columns, 2)
        }
        let across = (Double(columns) * min(rowSpan, widestRowSpan) / widestRowSpan - 1e-9).rounded(.up)
        return min(max(Int(across), 2), max(columns, 2))
    }

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

    /// The depth of the row `age` rows old, `glide` of the way to the next
    /// row (0 just after a row arrived, towards 1 before the next). The
    /// front, at depth 0, stays put: it shows the row before the newest,
    /// and the newest grows in over it (``frontOpacity(glide:)``). Every
    /// older row glides back from the front, so the row one before the
    /// newest leaves the front as the newest takes its place. The back
    /// stays put too, at ``backDepth``: it shows the oldest row whole and
    /// the one after it growing in (``backOpacity(glide:)``), so the stack
    /// never slides a row out past its back edge.
    public static func depth(age: Int, glide: Double) -> Double {
        guard age > 0 else {
            return 0
        }
        guard age < StackedTrace.visibleRows - 1 else {
            return backDepth
        }
        return (Double(age) - 1 + min(max(glide, 0), 1)) / Double(StackedTrace.visibleRows)
    }

    /// The back's depth: one row short of 1.
    public static let backDepth = Double(StackedTrace.visibleRows - 1) / Double(StackedTrace.visibleRows)

    /// How much of the row one younger than the oldest the back shows over
    /// the oldest: none when a row arrives, all of it when the next is due,
    /// when it becomes the oldest and is shown whole.
    public static func backOpacity(glide: Double) -> Double {
        min(max(glide, 0), 1)
    }

    /// How much of the newest row the front shows over the row before it:
    /// none when the row arrives, all of it when the next is due. A row so
    /// reaches the front whole one row after it arrives, and the front
    /// changes a little on every frame rather than all at once on a row.
    public static func frontOpacity(glide: Double) -> Double {
        min(max(glide, 0), 1)
    }

    /// The depth of the ridge line `place` lines from the front. The lines
    /// stay put while the rows glide: each shows the two rows the stack is
    /// gliding between at its depth (``lineOpacities(glide:strength:)``).
    public static func lineDepth(place: Int) -> Double {
        Double(max(place, 0)) / Double(StackedTrace.visibleRows)
    }

    /// How strong a ridge line is at `depth` over a level `colourShare` of
    /// the colour span above the floor, before the glide shares it out.
    public static func lineStrength(depth: Double, colourShare: Double) -> Double {
        let nearness = 1 - min(max(depth, 0), 1)
        let share = min(max(colourShare, 0), 1)
        // The shader's smoothstep: an S curve from 0 to the floor span.
        let t = min(max(share / StackedTrace.lineFloorSpan, 0), 1)
        let overFloor = StackedTrace.lineOverFloorShare
            + (1 - StackedTrace.lineOverFloorShare) * (t * t * (3 - 2 * t))
        return (StackedTrace.lineFarStrength + StackedTrace.lineNearExtra * nearness) * overFloor
    }

    /// The opacities of a ridge line's two layers at one depth, `glide` of
    /// the way to the next row: the older row's line fades out, and the
    /// newer's, drawn over it, fades in strengthened so that where both
    /// rows draw the same line it keeps its `strength` all the way.
    public static func lineOpacities(glide: Double, strength: Double) -> (older: Double, newer: Double) {
        let share = min(max(glide, 0), 1)
        let older = strength * (1 - share)
        let newer = strength * share / max(1 - older, 0.0001)
        return (older, newer)
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
        curtainColourPosition(dbm: dbm, down: 0)
    }

    /// The palette place on the curtain under a ridge at `dbm`, `down` of
    /// the way from the ridge (0) to the plot's floor (1).
    public func curtainColourPosition(dbm: Double, down: Double) -> Double {
        guard dbm.isFinite else {
            return 0
        }
        let share = min(max((dbm - floorDbm) / colourRangeDb, 0), 1)
        let along = 1 - min(max(down, 0), 1) * (1 - StackedTrace.curtainFootColour)
        return pow(share * along, gamma)
    }

    /// How much of a row shows `pixels` in from its end: none at the end,
    /// all of it from ``StackedTrace/sideFadePixels`` in, on an S curve.
    public static func sideFade(pixels: Double) -> Double {
        let t = min(max(pixels / StackedTrace.sideFadePixels, 0), 1)
        return t * t * (3 - 2 * t)
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
