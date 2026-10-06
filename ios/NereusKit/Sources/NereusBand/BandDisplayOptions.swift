// NereusSDR for iOS: the display's choices beyond the basics: labels, overlays, detectors, rewind and the phone's own palette
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import NereusModels

/// Where the frequency labels sit on their ticks (the desktop's Freq Label
/// Align): left of, centred on or right of the tick; Auto centres them
/// (and so reads as Centre); Off draws none.
public enum FrequencyLabelAlignment: String, Equatable, Sendable, Codable, CaseIterable {
    case left
    case centre
    case right
    case auto
    case off

    public var title: String {
        switch self {
        case .left:
            return "Left"
        case .centre:
            return "Center"
        case .right:
            return "Right"
        case .auto:
            return "Auto"
        case .off:
            return "Off"
        }
    }
}

/// A corner of the spectrum, for a readout.
public enum OverlayCorner: String, Equatable, Sendable, Codable, CaseIterable {
    case topLeft
    case topRight
    case bottomLeft
    case bottomRight

    public var title: String {
        switch self {
        case .topLeft:
            return "Top left"
        case .topRight:
            return "Top right"
        case .bottomLeft:
            return "Bottom left"
        case .bottomRight:
            return "Bottom right"
        }
    }
}

/// Where the waterfall's time shows: nowhere, or at its top left or right.
public enum TimestampPosition: String, Equatable, Sendable, Codable, CaseIterable {
    case none
    case left
    case right

    public var title: String {
        switch self {
        case .none:
            return "None"
        case .left:
            return "Left"
        case .right:
            return "Right"
        }
    }
}

/// The Core's detector for a plane, by its number on the link
/// (`detector`, 0 to 4): the desktop's Spectrum Detector list. The
/// waterfall offers the first four.
public enum SpectrumDetector: Int, Equatable, Sendable, Codable, CaseIterable {
    case peak = 0
    case rosenfell = 1
    case average = 2
    case sample = 3
    case rms = 4

    public var title: String {
        switch self {
        case .peak:
            return "Peak"
        case .rosenfell:
            return "Rosenfell"
        case .average:
            return "Average"
        case .sample:
            return "Sample"
        case .rms:
            return "RMS"
        }
    }

    /// The waterfall's detectors: all but RMS, as the desktop offers.
    public static let waterfallCases: [SpectrumDetector] = [.peak, .rosenfell, .average, .sample]
}

/// The Core's averaging for a plane, by its number on the link
/// (`averageMode`, 0 to 3): the desktop's Averaging list.
public enum SpectrumAveraging: Int, Equatable, Sendable, Codable, CaseIterable {
    case none = 0
    case recursive = 1
    case timeWindow = 2
    case logRecursive = 3

    public var title: String {
        switch self {
        case .none:
            return "None"
        case .recursive:
            return "Recursive"
        case .timeWindow:
            return "Time window"
        case .logRecursive:
            return "Log recursive"
        }
    }
}

/// One stop of this phone's own waterfall gradient: where it sits, 0 to 1,
/// and its colour, `#RRGGBB`.
public struct PaletteStop: Equatable, Sendable, Codable {
    public var at: Double
    public var colour: String

    public init(at: Double, colour: String) {
        self.at = at
        self.colour = colour
    }

    /// The phone's own starting gradient: black through blue and yellow to red.
    public static let customDefault = [
        PaletteStop(at: 0, colour: "#000000"), PaletteStop(at: 0.4, colour: "#0050FF"),
        PaletteStop(at: 0.7, colour: "#FFE000"), PaletteStop(at: 1, colour: "#FF2000"),
    ]
}

extension BandDisplaySettings {
    // The ranges of the choices above, the desktop's (DisplaySetupPages.cpp,
    // SpectrumPeaksPage.cpp) where it has one.
    public static let waterfallPeriodRange: ClosedRange<Int> = 10...500
    public static let waterfallOpacityRange: ClosedRange<Int> = 0...100
    public static let decimationRange: ClosedRange<Int> = 1...32
    public static let gridNoiseFloorOffsetRange: ClosedRange<Int> = -60...60
    public static let peakValueDelayRange: ClosedRange<Int> = 100...5000
    public static let peakHoldDelayRange: ClosedRange<Int> = 100...10000
    /// How far back the waterfall can be looked at: the Core's Rewind
    /// history depths, 60 seconds and 5, 15 and 20 minutes (its
    /// `DisplayWaterfallHistoryMs` options), so this phone's sheet and the
    /// Core's Setup row offer the same.
    public static let rewindChoices: [Int] = [60, 300, 900, 1200]

    /// The offered depth nearest `seconds`; halfway between two, the
    /// longer, so no look-back already kept is cut.
    public static func nearestRewind(_ seconds: Int) -> Int {
        rewindChoices.min { lhs, rhs in
            let left = abs(lhs - seconds)
            let right = abs(rhs - seconds)
            return left == right ? lhs > rhs : left < right
        } ?? 300
    }
    /// The most lines the phone keeps for looking back, whatever the depth
    /// and line period ask: with a screen of lines they stay within
    /// ``waterfallRowsLimit``.
    public static let rewindLinesLimit = 7_000
    /// The most rows the waterfall's texture may have: the smallest
    /// texture height every iPhone's GPU (and the simulator's) allows.
    public static let waterfallRowsLimit = 8_192

    /// How many lines ``rewindSeconds`` holds at ``waterfallPeriodMs``,
    /// at most ``rewindLinesLimit``.
    public var rewindLines: Int {
        let period = min(max(waterfallPeriodMs, Self.waterfallPeriodRange.lowerBound), Self.waterfallPeriodRange.upperBound)
        let lines = max(0, rewindSeconds) * 1000 / period
        return min(lines, Self.rewindLinesLimit)
    }

    /// The levels a line is coloured against when the phone sets them: the
    /// scale's top and bottom with Use spectrum min/max, else the manual ones.
    public var phoneLevels: WaterfallHistory.Levels {
        useSpectrumMinMax
            ? WaterfallHistory.Levels(lowDbm: Float(scaleRange.lowerBound), highDbm: Float(scaleRange.upperBound))
            : manualLevels
    }

    /// The quantisation window the Core's frames are coded in (256 steps
    /// between its bottom and top), as the desktop's remote window asks it
    /// (`RemoteMediaController.cpp` `liveDbmWindow`): the scale's range
    /// widened to hold the levels the waterfall is coloured against. With
    /// the Core setting the levels, its latest levels (`coreLevels`) with
    /// ``quantisationHeadroomDb`` either side, kept as `held` while they still
    /// fit and are not far narrower; with the Core's AGC, the low level no
    /// further than ``quantisationLowReachDb`` under the scale. The scale's
    /// part is rounded outward to whole ten dB, so a drag of the scale asks
    /// again only every ten dB.
    public func quantisationWindow(coreLevels: WaterfallHistory.Levels?,
                                   held: ClosedRange<Double>?) -> ClosedRange<Double> {
        let panLow = (scaleRange.lowerBound / 10).rounded(.down) * 10
        let panHigh = (scaleRange.upperBound / 10).rounded(.up) * 10
        var levels = useSpectrumMinMax ? scaleRange : Double(waterfallLowDbm)...Double(max(waterfallHighDbm,
                                                                                            waterfallLowDbm))
        let runtime = !useSpectrumMinMax && waterfallLevelMode != .manual
        if runtime, let core = coreLevels, core.lowDbm.isFinite, core.highDbm.isFinite {
            var low = Double(core.lowDbm)
            if waterfallLevelMode != .clarity {
                let reach = (min(panLow, waterfallLowDbm) - Self.quantisationLowReachDb).rounded(.down)
                low = max(low, reach + Self.quantisationHeadroomDb)
            }
            let high = max(Double(core.highDbm), low)
            let slack = 2 * Self.quantisationHeadroomDb
            if let held, low >= held.lowerBound, high <= held.upperBound, held.lowerBound >= low - slack,
               held.upperBound <= high + slack {
                levels = held
            } else {
                levels = (low - Self.quantisationHeadroomDb).rounded(.down)...(high + Self.quantisationHeadroomDb)
                    .rounded(.up)
            }
        }
        let limits = Self.quantisationLimits
        let low = min(max(min(panLow, levels.lowerBound), limits.lowerBound), limits.upperBound - 1)
        let high = min(max(max(panHigh, levels.upperBound), low + 1), limits.upperBound)
        return (low * 10).rounded(.down) / 10...(high * 10).rounded(.up) / 10
    }

    /// The desktop remote window's own headroom about run-time levels, and
    /// how far under the scale the waterfall AGC's low level may take the
    /// window (`RemoteMediaController.cpp`, parity Task 17).
    public static let quantisationHeadroomDb = 10.0
    public static let quantisationLowReachDb = 60.0
    /// The window's limits on the link (`kMinDbmLimit`, `kMaxDbmLimit`).
    public static let quantisationLimits: ClosedRange<Double> = -400...100

    /// Whether the Core's noise floor is asked for: for the line, for the
    /// grid to follow, or for the 3D view's surface to stand on.
    public var needsNoiseFloor: Bool { noiseFloorLine || gridFollowsNoiseFloor || spectrumView == .stacked }

    /// The spectrum and waterfall back at their defaults, as the desktop's
    /// Reset to Smooth Defaults leaves them: each band's scale and 3D Floor,
    /// the band plan's size and the extended view are not touched.
    public func resetKeepingPlace() -> BandDisplaySettings {
        var reset = BandDisplaySettings.desktopDefaults
        reset.scaleBand = nil
        reset.scaleTopDbm = scaleTopDbm
        reset.scaleBottomDbm = scaleBottomDbm
        reset.bandScales = bandScales
        // Each band's 3D Floor is kept like its scale.
        reset.threeDFloors = threeDFloors
        reset.threeDFloorDb = threeDFloorDb
        reset.scaleBand = scaleBand
        reset.bandPlanSize = bandPlanSize
        reset.extendedView = extendedView
        reset.sideways = sideways
        return reset
    }
}
