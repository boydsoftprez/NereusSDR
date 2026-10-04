// NereusSDR for iOS: each slice's marker on the band: its centre line, triangle, passband edges and shading
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics

/// The slices' markers on the band (D10, R-IOS-11), as the desktop draws
/// them: the selected slice's centre line, triangle and passband edges in
/// its own colour; every other slice with a darker line and triangle and
/// grey edges; the selected slice's marker drawn last, on top. One slice is
/// selected for the whole band. The shaded passband is the operator's own
/// colour for every slice, and it is hidden while keyed.
///
/// The colours are the Core's (the catalogue's `sliceColours`); the darker
/// partner is that colour at half its brightness, so no colour table lives
/// in the app (D4).
public enum SliceMarkers {
    /// How one marker is coloured.
    public struct Style: Equatable, Sendable {
        /// The centre line, straight RGBA from 0 to 1.
        public var line: SIMD4<Float>
        /// The triangle: the line's colour, opaque.
        public var triangle: SIMD4<Float>
        /// The passband's two edges.
        public var edge: SIMD4<Float>
        /// Whether this is the selected slice's marker, drawn on top.
        public var selected: Bool
    }

    /// One marker for the renderer, in the band's frequencies. The triangle's
    /// top is in points from the top of the band: just under the slice's
    /// flag, or the top of the band when there is no flag.
    public struct Marker: Equatable, Sendable {
        public var sliceId: Int
        public var centerHz: Double
        public var passbandHz: ClosedRange<Double>
        public var style: Style
        public var triangleTopPoints: CGFloat
        /// False while keyed: the passband's shading gives way to the TX
        /// filter (spec section 5.1 item 6); its edges stay.
        public var showsPassband: Bool
        /// Another device's slice (``ForeignSliceMarkers``): a dashed line,
        /// a hollow triangle and dashed edges, with no shading.
        public var foreign: Bool

        public init(sliceId: Int, centerHz: Double, passbandHz: ClosedRange<Double>, style: Style,
                    triangleTopPoints: CGFloat = 0, showsPassband: Bool = true, foreign: Bool = false) {
            self.sliceId = sliceId
            self.centerHz = centerHz
            self.passbandHz = passbandHz
            self.style = style
            self.triangleTopPoints = triangleTopPoints
            self.showsPassband = showsPassband
            self.foreign = foreign
        }
    }

    /// The centre line's opacity, 220 of 255 as on the desktop.
    public static let lineAlpha: Float = 220 / 255
    /// The edges' opacity, 130 of 255 as on the desktop.
    public static let edgeAlpha: Float = 130 / 255
    /// The darker partner of a slice's colour: this share of its brightness.
    public static let dimFactor: Float = 0.5
    /// The edges of a slice that is not selected: the band's secondary grey.
    public static let otherEdgeColour = "#8090A0"
    /// The centre line's width, in points; one point where an edge is
    /// within four points of it (a CW passband).
    public static let lineWidthPoints: CGFloat = 2
    public static let narrowLineWidthPoints: CGFloat = 1
    public static let narrowWithinPoints: CGFloat = 4
    /// The edges' width, in points.
    public static let edgeWidthPoints: CGFloat = 1
    /// The triangle, in points: 12 wide and 10 tall, pointing down.
    public static let triangleSize = CGSize(width: 12, height: 10)

    /// The style of a slice drawn in `colour` (`#RRGGBB`), selected or not.
    public static func style(for colour: String, selected: Bool) -> Style {
        let own = BandPalette.rgba(colour) ?? BandPalette.rgba(BandSlice.colourUnknown) ?? SIMD4(0.5, 0.5, 0.5, 1)
        if selected {
            return Style(line: withAlpha(own, lineAlpha), triangle: withAlpha(own, 1), edge: withAlpha(own, edgeAlpha),
                         selected: true)
        }
        let dim = dimmed(own)
        let grey = BandPalette.rgba(otherEdgeColour) ?? SIMD4(0.5, 0.56, 0.63, 1)
        return Style(line: withAlpha(dim, lineAlpha), triangle: withAlpha(dim, 1), edge: withAlpha(grey, edgeAlpha),
                     selected: false)
    }

    /// `colour` at ``dimFactor`` of its brightness, opaque.
    public static func dimmed(_ colour: SIMD4<Float>) -> SIMD4<Float> {
        SIMD4(colour.x * dimFactor, colour.y * dimFactor, colour.z * dimFactor, 1)
    }

    /// Every slice's marker, the selected slice's last so it is drawn on
    /// top. `placements` are the flags' (``FlagLayout``), in the order of
    /// `slices`; each triangle hangs from its flag's foot, kept inside the
    /// spectrum (`spectrumHeightPoints`).
    public static func markers(slices: [BandSlice], activeSliceId: Int?, placements: [FlagPlacement],
                               spectrumHeightPoints: CGFloat, keyed: Bool = false) -> [Marker] {
        var others: [Marker] = []
        var selected: [Marker] = []
        for (index, slice) in slices.enumerated() {
            let isSelected = slice.id == activeSliceId
            var top: CGFloat = 0
            if index < placements.count {
                top = placements[index].rect.maxY
            }
            top = max(0, min(top, spectrumHeightPoints - triangleSize.height))
            let marker = Marker(sliceId: slice.id, centerHz: slice.frequencyHz, passbandHz: slice.passbandHz,
                                style: style(for: slice.colour, selected: isSelected), triangleTopPoints: top,
                                showsPassband: !keyed)
            if isSelected {
                selected.append(marker)
            } else {
                others.append(marker)
            }
        }
        return others + selected
    }

    static func withAlpha(_ colour: SIMD4<Float>, _ alpha: Float) -> SIMD4<Float> {
        SIMD4(colour.x, colour.y, colour.z, alpha)
    }
}
