// NereusSDR for iOS: the phone's own display settings for one pan, from the desktop's defaults
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import NereusMedia

/// One pan's display settings on this phone (R-IOS-11, D7). Display
/// settings are each device's own: the Core owns only the FFT size and
/// window, the hertz per bin and the frame rate. So the phone starts from
/// the desktop's defaults (its `SpectrumWidget`, Spectrum Peaks page and
/// Clarity loaders) and keeps its own values per pan.
///
/// What the Core computes (the waterfall's levels, the noise-floor line,
/// the peak features, calibration, normalise and averaging) goes into the
/// pan's subscription through ``extrasRequest(gates:)``; the renderer
/// draws the rest (palette, grid, trace, band plan) itself.
public struct BandDisplaySettings: Equatable, Sendable, Codable {
    /// How the waterfall's levels are set.
    public enum WaterfallLevelMode: String, Equatable, Sendable, Codable, CaseIterable {
        /// The Core's Clarity sets them (the desktop's default).
        case clarity
        /// The Core's waterfall AGC sets them.
        case agc
        /// The Core's noise-floor AGC sets them.
        case noiseFloorAgc
        /// The manual low and high levels.
        case manual
    }

    // MARK: Computed by the Core

    /// The desktop's three level switches, each kept as it keeps them
    /// (Enable Clarity, Enable NF-AGC and AGC, `ClarityEnabled`,
    /// `WaterfallNFAGCEnabled` and `DisplayWfAgc`): Clarity first, then the
    /// noise-floor AGC, then the AGC, else the manual levels. Settings kept
    /// before them held only the mode (``BandDisplaySettingsStore``).
    public var clarityEnabled: Bool = true
    public var waterfallNfAgc: Bool = false
    public var waterfallAgc: Bool = true

    /// How the waterfall's levels are set, from the three switches.
    /// Choosing a mode sets them as the desktop's controls would: Clarity
    /// on; or Clarity off and the chosen AGC on; or all three off.
    public var waterfallLevelMode: WaterfallLevelMode {
        get {
            if clarityEnabled {
                return .clarity
            }
            if waterfallNfAgc {
                return .noiseFloorAgc
            }
            return waterfallAgc ? .agc : .manual
        }
        set {
            switch newValue {
            case .clarity:
                clarityEnabled = true
            case .noiseFloorAgc:
                clarityEnabled = false
                waterfallNfAgc = true
            case .agc:
                clarityEnabled = false
                waterfallNfAgc = false
                waterfallAgc = true
            case .manual:
                clarityEnabled = false
                waterfallNfAgc = false
                waterfallAgc = false
            }
        }
    }
    /// The manual levels, also used until the Core first sends levels and
    /// whenever the Core offers no display extras.
    public var waterfallLowDbm: Double = -122
    public var waterfallHighDbm: Double = -62
    /// The noise-floor AGC's offset, whole decibels.
    public var waterfallOffsetDb: Int = 0
    /// The waterfall's Color Gain, 0 to 100, and Black Level, 0 to 125,
    /// the desktop's defaults (`SpectrumWidget.cpp` `DisplayWfColorGain`,
    /// `DisplayWfBlackLevel`). The phone applies them to whatever levels
    /// colour a line, the Core's or the manual ones, as the desktop does;
    /// the Core's levels carry neither. See ``WaterfallHistory/Levels/adjusted(colorGain:blackLevel:)``.
    public var waterfallColorGain: Int = 45
    public var waterfallBlackLevel: Int = 104

    public var noiseFloorLine: Bool = false
    public var noiseFloorShiftDb: Double = 0
    /// The noise-floor line's width in the desktop's pixels, 1 to 5 by
    /// halves (its `DisplayNoiseFloorLineWidth`, 1).
    public var noiseFloorLineWidth: Double = 1

    public var normalize: Bool = false
    /// Kept as stored, never asked of the Core: the Core already calibrates
    /// the display for its radio, so an offset here would count it twice
    /// (the desktop's remote window greys its Cal Offset for the same
    /// reason). See ``calibrationReason``.
    public var calibrationOffsetDb: Double = 0

    public var spectrumAverageTimeMs: Int = 30
    public var waterfallAverageTimeMs: Int = 120

    public var peakBlobs: Bool = false
    public var peakBlobCount: Int = 3
    public var peakBlobHold: Bool = false
    public var peakBlobHoldMs: Int = 500
    public var peakBlobFall: Bool = false
    public var peakBlobFallDbPerSec: Double = 6
    public var peakBlobsInsideFilterOnly: Bool = false

    public var activePeakHold: Bool = false
    public var activePeakHoldMs: Int = 2000
    public var activePeakHoldFallDbPerSec: Double = 6
    /// Shades between the peak-hold trace and the live trace, as the
    /// desktop's Active Peak Hold Fill does. Off, the desktop's default.
    public var activePeakHoldFill: Bool = false
    /// Keeps the peak-hold trace running while the band transmits, as the
    /// desktop's Update during TX does. Off, the desktop's default: the
    /// Core then sends no trace while the band transmits, and none is
    /// drawn. Asked of a Core that sends `displayExtrasVersion` 3.
    public var activePeakHoldOnTx: Bool = false

    /// The extended view: the pan's subscription asks for the radio's full
    /// width either side of the band, while the Core offers it. Off by
    /// default; settings kept before it existed read as off.
    public var extendedView: Bool = false

    // MARK: The renderer's own

    /// The waterfall palette, by the desktop's palette number in the
    /// Core's catalogue (0, the desktop's default).
    public var waterfallPaletteId: Int = 0
    public var grid: Bool = true
    /// The dB between horizontal grid lines.
    public var gridStepDb: Double = 10
    /// The dBm scale's top and bottom. Each band keeps its own (the
    /// desktop's per-band dB Max and dB Min): a change is kept for the band
    /// the pan is on, and a new band brings its own back; see ``enterBand(_:)``.
    public var scaleTopDbm: Double = BandDisplaySettings.scaleTopDefaultDbm {
        didSet { keepBandScale() }
    }
    public var scaleBottomDbm: Double = BandDisplaySettings.scaleBottomDefaultDbm {
        didSet { keepBandScale() }
    }
    /// Each band's top and bottom, by the Core's band number as text.
    public var bandScales: [String: BandScale] = [:]
    /// The band the pan is on, by the Core's band number as text; nil
    /// before the Core names one.
    public var scaleBand: String?
    /// The spectrum's share of the band's height upright, in percent: 40,
    /// the desktop's default, from 20 to 80.
    public var spectrumSharePercent: Double = BandDisplaySettings.spectrumShareDefaultPercent
    /// The same share while the band is turned sideways (D84): 55 by
    /// default, since a sideways phone is too short for a flag and a spot
    /// row at 40. Settings kept before it existed read as the default; the
    /// upright share they kept stays upright.
    public var spectrumSharePercentSideways: Double = BandDisplaySettings.spectrumShareSidewaysDefaultPercent
    /// The band is turned sideways now, so the layout, the split drag and
    /// the Spectrum height slider use the sideways share. Set by the main
    /// screen from its size; never read back from what the store keeps.
    public var sideways: Bool = false
    /// `#RRGGBB`: the desktop's cyan `#00E5FF` for the trace and its fill
    /// (D83; value from `src/gui/SpectrumWidget.h:2390`).
    public var traceColour: String = "#00E5FF"
    /// The trace's line width in points (D75): from one screen pixel to
    /// ``traceWidthMaximumPoints``, starting at half a point, which reads
    /// lighter on a phone than the desktop's line. Settings kept before it
    /// existed read as the default.
    public var traceWidthPoints: Double = BandDisplaySettings.traceWidthDefaultPoints
    public var traceFill: Bool = true
    /// The fill's strength, 0 to 1, read as the desktop reads its Fill
    /// Alpha: the fill is drawn flat at four tenths of it, or with
    /// ``traceGradient`` from the whole of it at the top of the spectrum
    /// to nothing at its foot. The desktop's Fill Alpha 70 (D83; value
    /// from `src/gui/SpectrumWidget.cpp:665`).
    public var traceFillOpacity: Double = 0.70
    /// The desktop's Trace gradient: off, its default.
    public var traceGradient: Bool = false
    /// The band-plan strip's size on this phone (D79): the desktop's View >
    /// Band Plan sizes, starting at Small. Which plan it shows is the
    /// Core's, not this phone's. Settings kept before the size existed read
    /// their on/off as Small or Off (``BandDisplaySettingsStore``).
    public var bandPlanSize: BandPlanSize = .small
    /// The shaded passband, the operator's colour for every slice (D10):
    /// `#RRGGBB` and its opacity, 0 to 1. The desktop's default is this
    /// blue at 80 of 255.
    public var passbandColour: String = "#00B4D8"
    public var passbandOpacity: Double = 80.0 / 255.0

    // MARK: Task 54e part 2: the rest of the desktop's display
    // (the parity audit's rows 13, 14, 16, 19, 21, 22, 24 to 28, 30 to 34;
    // see BandDisplayOptions.swift for their types and ranges)

    /// Colours as `#RRGGBBAA` (``BandPalette/rgbaWithAlpha(_:)``): the
    /// vertical grid, the fine grid between its lines, the horizontal (dB)
    /// grid, and the frequency labels: the desktop's white at 40, 20 and
    /// 40 of 255, and its yellow text (D83; values from
    /// `src/gui/SpectrumWidget.h:2653-2656`).
    public var gridColour: String = "#FFFFFF28"
    public var gridFineColour: String = "#FFFFFF14"
    public var hGridColour: String = "#FFFFFF28"
    public var gridTextColour: String = "#FFFF00FF"
    /// The frequency labels' alignment on their ticks.
    public var frequencyLabelAlignment: FrequencyLabelAlignment = .centre
    /// The dBm scale along the spectrum's right edge, with its arrows.
    public var showDbmScale: Bool = true
    /// The 0 dBm line across the spectrum, dashed.
    public var showZeroLine: Bool = false
    /// The receive zero line's colour (the spectrum's 0 dBm line and the
    /// slice's line on the waterfall), the desktop's red, and the transmit
    /// zero line's, its amber (D83; values from `src/gui/SpectrumWidget.h:2658-2659`).
    public var rxZeroLineColour: String = "#FF0000FF"
    public var txZeroLineColour: String = "#FFB800FF"
    /// The transmit passband while keyed, the phone's orange.
    public var txPassbandColour: String = "#FF783C2E"
    /// Over the waterfall: the receive filter as a band, the transmit
    /// filter while keyed, the receive zero line, the transmit zero line
    /// while keyed. The desktop's defaults: the transmit filter on (as its
    /// renderer loads it; JJ, 2026-09-28), the others off. A kept choice,
    /// a stored off included, stays as it was.
    public var showRxFilterOnWaterfall: Bool = false
    public var showTxFilterOnWaterfall: Bool = true
    public var showRxZeroLineOnWaterfall: Bool = false
    public var showTxZeroLineOnWaterfall: Bool = false
    /// The waterfall's opacity over the band's background, 0 to 100.
    public var waterfallOpacityPercent: Int = 100
    /// How often the waterfall scrolls a line, in milliseconds: the
    /// desktop's 30, from 10 to 500.
    public var waterfallPeriodMs: Int = 30
    /// The waterfall stops while this phone transmits.
    public var waterfallStopOnTx: Bool = false
    /// The Core's detector and averaging for the trace and the waterfall,
    /// and its decimation: the desktop's Peak and Log Recursive for the
    /// trace, Peak and None for the waterfall, decimation 1.
    public var spectrumDetector: SpectrumDetector = .peak
    public var spectrumAveraging: SpectrumAveraging = .logRecursive
    public var waterfallDetector: SpectrumDetector = .peak
    public var waterfallAveraging: SpectrumAveraging = .none
    public var decimation: Int = 1
    /// The waterfall coloured against the dBm scale's top and bottom
    /// instead of its own levels (the desktop's Use spectrum min/max).
    public var useSpectrumMinMax: Bool = false
    /// The scale's bottom follows the noise floor the Core measures, plus
    /// an offset; with the range kept, the top moves with it.
    public var gridFollowsNoiseFloor: Bool = false
    public var gridNoiseFloorOffsetDb: Int = 0
    public var gridKeepsRange: Bool = false
    /// The strongest signal's level and frequency, in a corner, refreshed
    /// every ``peakValueDelayMs``.
    public var showPeakValue: Bool = false
    public var peakValueCorner: OverlayCorner = .topRight
    /// The desktop's 500 ms and blue `#1E90FF` (D83; values from
    /// `src/gui/SpectrumWidget.cpp:1022-1023` and `src/gui/SpectrumWidget.h:2934`).
    public var peakValueDelayMs: Int = 500
    public var peakValueColour: String = "#1E90FFFF"
    /// The bin width, the frames a second, and the frequency under the
    /// finger while it drags the band.
    public var showBinWidth: Bool = false
    public var showFps: Bool = false
    public var showCursorFrequency: Bool = true
    /// The desktop's classic Peak hold (not Active Peak Hold): the highest
    /// level at each point, drawn dotted, started afresh every
    /// ``peakHoldDelayMs``. Drawn on this phone from the Core's frames.
    public var peakHold: Bool = false
    public var peakHoldDelayMs: Int = 2000
    /// The extras' colours, the desktop's (D83): the noise floor's magenta
    /// and yellow text (values from `src/gui/SpectrumWidget.h:2868-2869`),
    /// the peak hold's gold (`src/gui/SpectrumWidget.cpp:760-762`), the
    /// peak blobs' orange-red and green text (`src/gui/SpectrumWidget.cpp:792-797`).
    public var noiseFloorColour: String = "#FF40FFFF"
    public var noiseFloorTextColour: String = "#FFFF00FF"
    /// The line, box and text while the floor is in fast attack, the
    /// desktop's grey (`DisplayNoiseFloorFastColor`, `#C8C8C8`).
    public var noiseFloorFastColour: String = "#C8C8C8FF"
    public var peakHoldColour: String = "#FFD700FF"
    public var peakBlobColour: String = "#FF4500FF"
    public var peakBlobTextColour: String = "#7FFF00FF"
    /// How far back the waterfall can be looked at, in seconds.
    public var rewindSeconds: Int = 300
    /// The time at the waterfall's top edge: none, left or right, UTC or local.
    public var timestampPosition: TimestampPosition = .none
    public var timestampUtc: Bool = true
    /// This phone's own waterfall gradient, used when
    /// ``waterfallPaletteId`` is ``BandPalette/customPaletteId``.
    public var customPalette: [PaletteStop] = PaletteStop.customDefault
    /// CTUN, the desktop's independent pan (on, its default): the band
    /// stays where it is put while the slice tunes. Off, the band follows
    /// the active slice, keeping it in the middle.
    public var ctun: Bool = true

    // MARK: Task 54f: the transmit display (desktop PR #317)

    /// The transmit grid, while the Core's radio is keyed on this band: its
    /// top and its range in dB, the desktop's 20 dBm and 100 dB (D83; its
    /// `SpectrumWidget` transmit reference level and dynamic range).
    public var txGridTopDbm: Double = 20
    public var txGridRangeDb: Double = 100
    /// The transmit waterfall's levels, the desktop's -70 and 30 dBm (its
    /// `DisplayTxWfLowLevel` and `DisplayTxWfHighLevel`).
    public var txWaterfallLowDbm: Double = -70
    public var txWaterfallHighDbm: Double = 30
    /// The transmit waterfall's palette by the Core's catalogue number: the
    /// desktop's Enhanced, 1 (its `DisplayTxWfPalette`).
    public var txWaterfallPaletteId: Int = 1
    /// The transmit waterfall's colour below its low level, the desktop's
    /// black (`DisplayTxWfLowColor`).
    public var txWaterfallLowColour: String = "#000000FF"
    /// The colour the waterfall draws a line at or under its low level
    /// in, instead of the palette's first: set only for the transmit
    /// display (``keyedOverlay``), nil otherwise.
    public var waterfallLowColour: String?
    /// The keyed view's span, kept from the last zoom made while keyed
    /// (the desktop's `DisplayTxViewBandwidth`); 8 kHz before any.
    public var txViewSpanHz: Double = TransmitDisplay.firstSpanHz
    /// Display duplex (DUP): while keyed the band keeps the receiver instead
    /// of the Core's transmit display. Off, the desktop's default.
    public var displayDuplex: Bool = false

    // MARK: The 3D view (Display V12's 3D View page)

    /// The spectrum as a flat trace or as the stacked trace (the desktop's
    /// `DisplaySpectrumRenderMode`, 0 or 1): 2D, its default.
    public var spectrumView: SpectrumView = .flat
    /// 3D Floor, whole dB under the noise floor where the surface starts,
    /// 0 to 24, for the band the pan is on (the desktop's per-band
    /// `Display3DFloorDepth_<band>`, 6). Each band keeps its own in
    /// ``threeDFloors``; see ``enterBand(_:)``.
    public var threeDFloorDb: Int = StackedTrace.floorDefaultDb {
        didSet { keepThreeDFloor() }
    }
    /// Each band's 3D Floor, by the Core's band number as text.
    public var threeDFloors: [String: Int] = [:]
    /// 3D Gain, 3D Span and 3D Angle, 0 to 100 (the desktop's 70, 100, 50),
    /// and 3D Slice Shadow (off).
    public var threeDGain: Int = StackedTrace.gainDefault
    public var threeDSpan: Int = StackedTrace.spanDefault
    public var threeDAngle: Int = StackedTrace.angleDefault
    public var threeDSliceShadow: Bool = false

    /// 1 once the settings carry the desktop's colours and sizes (D83).
    /// Settings kept before then lack it, and their colours still at the
    /// phone's earlier defaults read as the desktop's (``BandDisplaySettingsStore``).
    public var desktopValuesVersion: Int = BandDisplaySettings.desktopValuesCurrent

    public init() {}

    /// The ``desktopValuesVersion`` this build writes.
    public static let desktopValuesCurrent = 1

    /// One band's dBm scale.
    public struct BandScale: Equatable, Sendable, Codable {
        public var topDbm: Double
        public var bottomDbm: Double

        public init(topDbm: Double, bottomDbm: Double) {
            self.topDbm = topDbm
            self.bottomDbm = bottomDbm
        }
    }

    /// The scale a band starts with before it keeps its own.
    public static let scaleTopDefaultDbm = -40.0
    public static let scaleBottomDefaultDbm = -140.0

    /// The split's defaults, upright and sideways (D84), and its limits,
    /// in percent of the band's height.
    public static let spectrumShareDefaultPercent = 40.0
    public static let spectrumShareSidewaysDefaultPercent = 55.0
    public static let spectrumShareRange: ClosedRange<Double> = 20...80

    /// The share in percent for the band as it is turned now, upright or
    /// sideways; setting it changes only that one.
    public var currentSpectrumSharePercent: Double {
        get { sideways ? spectrumSharePercentSideways : spectrumSharePercent }
        set {
            if sideways {
                spectrumSharePercentSideways = newValue
            } else {
                spectrumSharePercent = newValue
            }
        }
    }

    /// The spectrum's share of the band's height as it is turned now, 0.2 to 0.8.
    public var spectrumShare: CGFloat {
        let fallback = sideways ? Self.spectrumShareSidewaysDefaultPercent : Self.spectrumShareDefaultPercent
        let percent = currentSpectrumSharePercent.isFinite ? currentSpectrumSharePercent : fallback
        return CGFloat(min(max(percent, Self.spectrumShareRange.lowerBound), Self.spectrumShareRange.upperBound) / 100)
    }

    /// The limits of Color Gain and Black Level.
    public static let colorGainRange: ClosedRange<Int> = 0...100
    public static let blackLevelRange: ClosedRange<Int> = 0...125

    /// Why Calibration offset is greyed on the phone, the desktop remote
    /// window's words (`DisplaySetupPages.cpp`, Cal Offset).
    public static let calibrationReason = "The Core calibrates the display for its radio."

    /// The pan is now on `band` (the Core's band number as text): its own
    /// top and bottom come back, or, for a band that kept none, the
    /// defaults. The first band named keeps the scale as it is, so a scale
    /// set before bands were kept is not lost.
    public mutating func enterBand(_ band: String?) {
        guard let band, band != scaleBand else {
            return
        }
        let first = scaleBand == nil && bandScales.isEmpty
        scaleBand = band
        if let kept = bandScales[band] {
            scaleTopDbm = kept.topDbm
            scaleBottomDbm = kept.bottomDbm
        } else if first {
            keepBandScale()
        } else {
            scaleTopDbm = Self.scaleTopDefaultDbm
            scaleBottomDbm = Self.scaleBottomDefaultDbm
        }
        // 3D Floor the same way: the band's own, or the default for a new one.
        if let kept = threeDFloors[band] {
            threeDFloorDb = kept
        } else if first {
            keepThreeDFloor()
        } else {
            threeDFloorDb = StackedTrace.floorDefaultDb
        }
    }

    /// Keeps 3D Floor for the band the pan is on.
    private mutating func keepThreeDFloor() {
        guard let band = scaleBand, threeDFloors[band] != threeDFloorDb else {
            return
        }
        threeDFloors[band] = threeDFloorDb
    }

    /// Reset 3D to defaults, as the desktop's does: the view back to 2D and
    /// the five 3D settings to their defaults, 3D Floor for the pan's band
    /// only; other bands keep theirs.
    public mutating func resetThreeD() {
        spectrumView = .flat
        threeDFloorDb = StackedTrace.floorDefaultDb
        threeDGain = StackedTrace.gainDefault
        threeDSpan = StackedTrace.spanDefault
        threeDAngle = StackedTrace.angleDefault
        threeDSliceShadow = false
    }

    /// Keeps the scale for the band the pan is on.
    private mutating func keepBandScale() {
        guard let band = scaleBand else {
            return
        }
        let scale = BandScale(topDbm: scaleTopDbm, bottomDbm: scaleBottomDbm)
        if bandScales[band] != scale {
            bandScales[band] = scale
        }
    }

    /// The waterfall's Color Gain and Black Level, as the history applies
    /// them to a line's levels.
    public var waterfallAdjustment: WaterfallHistory.Adjustment {
        WaterfallHistory.Adjustment(colorGain: waterfallColorGain, blackLevel: waterfallBlackLevel)
    }

    /// Whether the band-plan strip shows: any size but Off.
    public var bandPlanStrip: Bool { bandPlanSize != .off }

    /// The desktop's defaults.
    public static let desktopDefaults = BandDisplaySettings()

    /// The trace's width limits and default, in points (D75).
    public static let traceWidthDefaultPoints = 0.5
    public static let traceWidthMaximumPoints = 3.0

    /// The trace widths a screen of `scale` pixels per point can show:
    /// from one pixel to ``traceWidthMaximumPoints``.
    public static func traceWidthRange(scale: CGFloat) -> ClosedRange<Double> {
        let pixel = scale > 0 ? 1 / Double(scale) : 1
        return min(pixel, traceWidthMaximumPoints)...traceWidthMaximumPoints
    }

    /// The trace's width in pixels on a screen of `scale` pixels per
    /// point, never thinner than one pixel.
    public func traceWidthPixels(scale: CGFloat) -> CGFloat {
        let points = traceWidthPoints.isFinite ? min(traceWidthPoints, Self.traceWidthMaximumPoints) : Self.traceWidthDefaultPoints
        return max(1, CGFloat(points) * scale)
    }

    /// The manual levels as the waterfall uses them.
    public var manualLevels: WaterfallHistory.Levels {
        WaterfallHistory.Levels(lowDbm: Float(waterfallLowDbm), highDbm: Float(waterfallHighDbm))
    }

    /// The dBm scale's range.
    public var scaleRange: ClosedRange<Double> {
        let bottom = min(scaleBottomDbm, scaleTopDbm - 1)
        return bottom...scaleTopDbm
    }

    /// Whether the Core computes the display extras. When it does not (an
    /// older Core), the waterfall uses the manual levels and Setup shows the
    /// peak features greyed with the catalogue's "Needs a newer Core" (D23).
    public static func extrasAvailable(gates: MediaFeatureGates) -> Bool {
        gates.displayExtras
    }

    /// What the pan's subscription asks the Core to compute: nil while the
    /// Core offers no display extras, and nothing for a feature that is off.
    public func extrasRequest(gates: MediaFeatureGates) -> DisplayExtrasRequest? {
        guard gates.displayExtras else {
            return nil
        }
        var request = DisplayExtrasRequest()
        let mode: DisplayExtrasRequest.WaterfallLevels.Mode
        switch waterfallLevelMode {
        case .clarity:
            mode = .clarity
        case .agc:
            mode = .agc
        case .noiseFloorAgc:
            mode = .noiseFloorAgc
        case .manual:
            mode = .manual
        }
        if useSpectrumMinMax {
            // The phone colours against the scale; the Core computes no levels.
            request.waterfallLevels = .init(mode: .manual, lowDbm: scaleRange.lowerBound,
                                            highDbm: scaleRange.upperBound, offsetDb: waterfallOffsetDb)
        } else {
            request.waterfallLevels = .init(mode: mode, lowDbm: waterfallLowDbm, highDbm: waterfallHighDbm,
                                            offsetDb: waterfallOffsetDb)
        }
        if needsNoiseFloor {
            // The NF shift always, since the grid follows the display floor
            // that carries it; and the fast-attack state where the Core
            // offers it, so the line takes its fast colour and the grid
            // holds while it lasts (the Core's V12 note, update 2).
            request.noiseFloor = .init(enabled: true, shiftDb: noiseFloorShiftDb,
                                       fastAttack: gates.noiseFloorFastAttack ? true : nil)
        }
        if normalize {
            request.normalize = true
        }
        // Never calibrationOffsetDb: the Core calibrates (calibrationReason).
        request.averageTimeMs = spectrumAverageTimeMs
        request.waterfallAverageTimeMs = waterfallAverageTimeMs
        if peakBlobs {
            // The desktop's switches as the Core rebuilds them: with the
            // hold off, drop and fall rate are ignored; with the hold on and
            // drop off, a blob goes at the end of its hold.
            request.peakBlobs = .init(count: peakBlobCount, holdMs: peakBlobHold ? peakBlobHoldMs : 0,
                                      fallDbPerSec: peakBlobHold && peakBlobFall ? peakBlobFallDbPerSec : 0,
                                      insideOnly: peakBlobsInsideFilterOnly)
        }
        if activePeakHold {
            request.activePeakHold = .init(enabled: true, holdMs: activePeakHoldMs,
                                           fallDbPerSec: activePeakHoldFallDbPerSec,
                                           onTx: gates.peakHoldOnTx ? activePeakHoldOnTx : nil)
        }
        return request
    }

    /// The transmit grid's limits, the desktop's: top -200 to 200 dBm,
    /// range 10 to 200 dB; the transmit waterfall's levels -200 to 200 dBm.
    public static let txGridTopRange: ClosedRange<Double> = -200...200
    public static let txGridRangeRange: ClosedRange<Double> = 10...200
    public static let txWaterfallLevelRange: ClosedRange<Double> = -200...200

    /// The window the Core quantises this band's transmit display to
    /// (``TransmitDisplay/dbmWindow(gridTopDbm:gridRangeDb:waterfallLowDbm:waterfallHighDbm:)``).
    public var txDbmWindow: ClosedRange<Double> {
        TransmitDisplay.dbmWindow(gridTopDbm: txGridTopDbm, gridRangeDb: txGridRangeDb,
                                  waterfallLowDbm: txWaterfallLowDbm, waterfallHighDbm: txWaterfallHighDbm)
    }

    /// The settings the band is drawn with while the Core's radio is keyed
    /// on it, as the desktop draws its transmitting pan: the transmit grid
    /// as the scale, the transmit waterfall levels colouring each line
    /// whatever the Core sent, and the transmit palette. The Core computes
    /// no noise floor, peaks or levels for the transmit display, so those
    /// are left off; the peak-hold trace stays only with Update during TX
    /// on, drawn where the band keeps its receiver and the Core still
    /// sends the trace.
    public var keyedOverlay: BandDisplaySettings {
        var keyed = self
        let range = min(max(txGridRangeDb, Self.txGridRangeRange.lowerBound), Self.txGridRangeRange.upperBound)
        keyed.scaleTopDbm = txGridTopDbm
        keyed.scaleBottomDbm = txGridTopDbm - range
        keyed.bandScales = bandScales
        keyed.gridFollowsNoiseFloor = false
        keyed.waterfallLevelMode = .manual
        keyed.useSpectrumMinMax = false
        keyed.waterfallLowDbm = min(txWaterfallLowDbm, txWaterfallHighDbm - 1)
        keyed.waterfallHighDbm = txWaterfallHighDbm
        keyed.waterfallPaletteId = txWaterfallPaletteId
        keyed.waterfallLowColour = txWaterfallLowColour
        keyed.noiseFloorLine = false
        keyed.peakBlobs = false
        keyed.activePeakHold = activePeakHold && activePeakHoldOnTx
        return keyed
    }

    /// `subscription` with this pan's extras request in place.
    public func applied(to subscription: DisplaySubscription, gates: MediaFeatureGates) -> DisplaySubscription {
        var subscription = subscription
        subscription.extras = extrasRequest(gates: gates)
        return subscription
    }
}
