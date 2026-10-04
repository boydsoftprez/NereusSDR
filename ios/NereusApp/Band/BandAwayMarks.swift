// NereusSDR for iOS: the band's sound-only marks, drawn across the waterfall beneath the band's controls
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The marks across the waterfall for each stretch the band was not sent
/// (spec section 5.5 item 12, picture 16), in a layer of their own drawn
/// over the band and beneath everything on it: the flags and markers, the
/// dial with its step box and wheel, zoom, the dBm arrows, PTT and the
/// readouts. A mark scrolls down the waterfall with its lines and passes
/// under those controls, never over them. Nothing here takes a touch.
struct BandAwayMarks: View {
    @ObservedObject var band: BandModel
    @Environment(\.displayScale) private var displayScale

    var body: some View {
        GeometryReader { proxy in
            if let axes = band.pointGeometry(size: proxy.size) {
                ZStack(alignment: .topLeading) {
                    ForEach(Array(band.awayMarks.enumerated()), id: \.offset) { _, mark in
                        awayMark(mark, layout: axes.layout)
                    }
                }
                .frame(width: proxy.size.width, height: proxy.size.height, alignment: .topLeading)
            }
        }
        .allowsHitTesting(false)
    }

    /// A stretch the band was not sent, across the waterfall where it fell
    /// (spec section 5.5 item 12, picture 16): a strip the band's width,
    /// dashed above and below, with its words in the middle, as the board
    /// draws it. One waterfall line is one pixel row.
    @ViewBuilder
    private func awayMark(_ mark: BandModel.AwayMark, layout: BandLayout) -> some View {
        let waterfall = layout.waterfall
        let rows = CGFloat(mark.linesSince - band.lookBackLines) / max(displayScale, 1)
        if rows >= 0, rows <= waterfall.height {
            Text(mark.text)
                .font(.system(size: 10, weight: .semibold, design: .monospaced))
                .foregroundStyle(BandColours.awayMarkText)
                .lineLimit(1)
                .frame(width: waterfall.width, height: Self.awayMarkHeight)
                .background(BandColours.awayMarkBackground)
                .overlay(alignment: .top) { dashes(width: waterfall.width) }
                .overlay(alignment: .bottom) { dashes(width: waterfall.width) }
                .offset(x: 0, y: waterfall.minY + rows - Self.awayMarkHeight / 2)
                .allowsHitTesting(false)
                .accessibilityIdentifier("awayMark")
        }
    }

    private func dashes(width: CGFloat) -> some View {
        Path { path in
            path.move(to: CGPoint(x: 0, y: 0.5))
            path.addLine(to: CGPoint(x: width, y: 0.5))
        }
        .stroke(BandColours.awayMarkEdge, style: StrokeStyle(lineWidth: 1, dash: [3, 3]))
        .frame(width: width, height: 1)
    }

    static let awayMarkHeight: CGFloat = 18
}
