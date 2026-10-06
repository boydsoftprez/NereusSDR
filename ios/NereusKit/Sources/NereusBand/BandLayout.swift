// NereusSDR for iOS: where the spectrum, the band-plan strip, the frequency scale and the waterfall sit in the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics

/// The band's parts, top to bottom, in pixels (D7, the board's pictures):
/// the spectrum with the dBm scale along its right edge, the band-plan
/// strip along the spectrum's foot, the frequency scale, then the
/// waterfall. Every part shares one frequency axis across the band's whole
/// width; the strip stops at the dBm scale's left edge, as the desktop's
/// does (D79).
public struct BandLayout: Equatable, Sendable {
    /// The frequency scale's height, in points.
    public static let scaleHeightPoints: CGFloat = 18
    /// The dBm scale's width, in points: a little larger for fingers
    /// (spec section 5.1 item 4).
    public static let dbmScaleWidthPoints: CGFloat = 40
    /// The dBm labels' size, in points.
    public static let labelPoints: CGFloat = 11
    /// The frequency labels' size, in points.
    public static let frequencyLabelPoints: CGFloat = 10
    /// The spectrum's share of the height the scale leaves, unless the
    /// pan's settings say otherwise: the desktop's 40 percent.
    public static let spectrumShare: CGFloat = CGFloat(BandDisplaySettings.spectrumShareDefaultPercent / 100)
    /// The dBm scale's arrow buttons at its top, each this high, in points;
    /// the scale's labels start under them.
    public static let dbmArrowHeightPoints: CGFloat = 26

    public let size: CGSize
    /// Pixels per point.
    public let scale: CGFloat
    public let spectrum: CGRect
    /// Along the spectrum's foot, inside it.
    public let strip: CGRect
    public let frequencyScale: CGRect
    public let waterfall: CGRect
    /// Along the spectrum's right edge, inside it.
    public let dbmScale: CGRect
    /// The dBm scale's two arrow buttons, raise over lower, at its top.
    public let dbmArrows: CGRect

    /// The layout with a strip `stripPoints` high (0 for none; see
    /// ``BandPlanSize/stripHeightPoints``) and the spectrum taking
    /// `spectrumShare` of the height the frequency scale leaves.
    public init(size: CGSize, scale: CGFloat, stripPoints: CGFloat,
                spectrumShare: CGFloat = BandLayout.spectrumShare, showsDbmScale: Bool = true) {
        self.size = size
        self.scale = max(scale, 0.1)
        let width = size.width
        let scaleHeight = (Self.scaleHeightPoints * self.scale).rounded()
        let stripHeight = (max(0, stripPoints) * self.scale).rounded()
        let shared = max(0, size.height - scaleHeight)
        let share = spectrumShare.isFinite ? min(max(spectrumShare, 0), 1) : Self.spectrumShare
        let spectrumHeight = (shared * share).rounded()
        // Hidden, the scale takes no width: the spectrum and strip run to the edge.
        let dbmWidth = showsDbmScale ? min(width, (Self.dbmScaleWidthPoints * self.scale).rounded()) : 0
        spectrum = CGRect(x: 0, y: 0, width: width, height: spectrumHeight)
        strip = CGRect(x: 0, y: max(0, spectrumHeight - stripHeight), width: width - dbmWidth,
                       height: min(stripHeight, spectrumHeight))
        frequencyScale = CGRect(x: 0, y: spectrumHeight, width: width, height: min(scaleHeight, size.height))
        let waterfallTop = spectrumHeight + scaleHeight
        waterfall = CGRect(x: 0, y: waterfallTop, width: width, height: max(0, size.height - waterfallTop))
        dbmScale = CGRect(x: width - dbmWidth, y: 0, width: dbmWidth, height: max(0, spectrumHeight - strip.height))
        dbmArrows = CGRect(x: dbmScale.minX, y: 0, width: dbmWidth,
                           height: min(dbmScale.height, (2 * Self.dbmArrowHeightPoints * self.scale).rounded()))
    }

    /// The layout for a pan's `settings`: its strip at their size, its
    /// spectrum at their share.
    public init(size: CGSize, scale: CGFloat, settings: BandDisplaySettings) {
        self.init(size: size, scale: scale, stripPoints: settings.bandPlanSize.stripHeightPoints,
                  spectrumShare: settings.spectrumShare, showsDbmScale: settings.showDbmScale)
    }

    /// The layout with the strip at Small, or none, and the spectrum at
    /// `spectrumShare`. For the screens' overlays that place themselves by
    /// the waterfall's top, which the strip's size does not move.
    public init(size: CGSize, scale: CGFloat, showsStrip: Bool = true,
                spectrumShare: CGFloat = BandLayout.spectrumShare) {
        self.init(size: size, scale: scale,
                  stripPoints: showsStrip ? BandPlanSize.small.stripHeightPoints : 0, spectrumShare: spectrumShare)
    }

    /// The spectrum's geometry for a context's centre and span: the trace's
    /// dBm axis runs from the top of the spectrum to the top of the strip.
    public func spectrumGeometry(centerHz: Double, spanHz: Double, dbmRange: ClosedRange<Double>) -> BandGeometry {
        BandGeometry(centerHz: centerHz, spanHz: spanHz,
                     size: CGSize(width: spectrum.width, height: spectrum.height - strip.height),
                     dbmRange: dbmRange)
    }

    /// How many waterfall lines fit: one per pixel row.
    public var waterfallLines: Int { max(1, Int(waterfall.height)) }
}
