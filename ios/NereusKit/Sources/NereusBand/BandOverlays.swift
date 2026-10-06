// NereusSDR for iOS: what the renderer draws around the Core's frame: the axes, the plan, the palette and the settings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import NereusModels

/// Everything ``BandRenderer`` draws that the Core's frame does not carry:
/// the endpoint context's centre and span (the frequency axis), the pan's
/// display settings, the band plan and palette from the Core's catalogue,
/// and whether the band is paused (its display suspended for want of room,
/// so the frozen band must not pass for a live one).
public struct BandOverlays: Equatable, Sendable {
    /// The view drawn: the band's centre and span now, which may be ahead of
    /// the Core's context while a pan or zoom waits for it (D74).
    public var centerHz: Double
    public var spanHz: Double
    /// The frequencies the frame drawn covers (its context); nil when it
    /// covers the view exactly.
    public var frameCoverage: BandCoverage?
    /// Pixels per point.
    public var scale: CGFloat
    public var settings: BandDisplaySettings
    /// The plan the strip shows; see ``BandPlanStrip/plan(in:stationPlanName:)``.
    public var bandPlan: StationCatalog.BandPlan?
    /// The waterfall's palette; see ``BandPalette/palette(id:in:)``.
    public var palette: StationCatalog.Palette?
    public var paused: Bool
    /// The slices' markers, the selected slice's last; see ``SliceMarkers``.
    public var markers: [SliceMarkers.Marker]
    /// The classic peak hold's trace, one level per trace sample; nil while
    /// off (``BandState/peakHold``).
    public var peakHold: [Float]?
    /// How many lines back the waterfall is looked at; 0 shows it live.
    public var lookBackLines: Int = 0
    /// The 3D view, while the pan draws it; nil draws the flat spectrum.
    public var stacked: StackedTraceOverlay?

    /// What the band shows while paused.
    public static let pausedText = "Paused"

    public init(centerHz: Double, spanHz: Double, scale: CGFloat, settings: BandDisplaySettings = .desktopDefaults,
                bandPlan: StationCatalog.BandPlan? = nil, palette: StationCatalog.Palette? = nil,
                paused: Bool = false, markers: [SliceMarkers.Marker] = [], frameCoverage: BandCoverage? = nil) {
        self.centerHz = centerHz
        self.spanHz = spanHz
        self.frameCoverage = frameCoverage
        self.scale = scale
        self.settings = settings
        self.bandPlan = bandPlan
        self.palette = palette
        self.paused = paused
        self.markers = markers
    }

    /// The overlays for `settings` with the palette they choose from
    /// `catalog`, and the plan the Core shows (its `BandPlanName` setting
    /// is `stationPlanName`).
    public init(centerHz: Double, spanHz: Double, scale: CGFloat, settings: BandDisplaySettings,
                catalog: StationCatalog?, stationPlanName: String? = nil, paused: Bool = false,
                markers: [SliceMarkers.Marker] = [], frameCoverage: BandCoverage? = nil) {
        self.init(centerHz: centerHz, spanHz: spanHz, scale: scale, settings: settings,
                  bandPlan: catalog.flatMap { BandPlanStrip.plan(in: $0.bandPlans, stationPlanName: stationPlanName) },
                  palette: catalog.flatMap { BandPalette.palette(id: settings.waterfallPaletteId, in: $0.palettes) },
                  paused: paused, markers: markers, frameCoverage: frameCoverage)
    }
}

/// What the renderer needs to draw the 3D view besides its rows
/// (``BandState/stack``) and the pan's settings.
public struct StackedTraceOverlay: Equatable, Sendable {
    /// The Core's noise floor the surface stands on, without the pan's NF
    /// shift; nil before the Core has sent one, when the scale's bottom stands in.
    public var noiseFloorDbm: Double?
    /// How far the stack has glided toward the next line, 0 to 1.
    public var glide: Double
    /// The Core's spectrum beside the pan, in the pan's widths (its wide
    /// span over the pan's span); 1 or less when it sends none.
    public var availableSpanFactor: Double

    public init(noiseFloorDbm: Double?, glide: Double, availableSpanFactor: Double) {
        self.noiseFloorDbm = noiseFloorDbm
        self.glide = glide
        self.availableSpanFactor = availableSpanFactor
    }

    /// The floor the surface is drawn over for `settings`: the Core's, or
    /// without one the scale's bottom.
    public func noiseFloor(_ settings: BandDisplaySettings) -> Double {
        noiseFloorDbm ?? settings.scaleRange.lowerBound
    }
}
