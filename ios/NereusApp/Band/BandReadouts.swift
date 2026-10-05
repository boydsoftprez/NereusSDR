// NereusSDR for iOS: the band's readouts: frames a second, bin width, the peak, the finger's frequency, the time and LIVE
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import SwiftUI
import UIKit

/// The words and numbers the desktop draws over its band (the parity
/// audit's rows 14 and 24), each as the pan's settings ask: the frames a
/// second at the spectrum's top right, the bin width at its bottom right,
/// the strongest signal in the chosen corner, the frequency under the finger
/// while it drags the band, and the time at the waterfall's top. While the
/// waterfall is looked back, a LIVE button at its top right brings it back;
/// it is the visible way back, as the Display sheet's Look back is the
/// visible way there. Nothing here takes a touch but LIVE. The marks for
/// a stretch the band was not sent are `BandAwayMarks`, drawn beneath the
/// band's controls; these readouts stay over them.
struct BandReadouts: View {
    @ObservedObject var band: BandModel
    var trailingInset: CGFloat = 0

    var body: some View {
        GeometryReader { proxy in
            if let axes = band.pointGeometry(size: proxy.size) {
                content(layout: axes.layout, geometry: axes.geometry)
                    .frame(width: proxy.size.width, height: proxy.size.height, alignment: .topLeading)
            }
        }
    }

    @ViewBuilder
    private func content(layout: BandLayout, geometry: BandGeometry) -> some View {
        let settings = band.settings
        let spectrum = CGRect(x: 0, y: 0, width: layout.dbmScale.minX, height: layout.strip.minY)
        let textColour = BandColours.withAlpha(settings.gridTextColour)
        ZStack(alignment: .topLeading) {
            if settings.showFps, let fps = band.framesPerSecond {
                readout(Self.fpsText(fps), colour: textColour, corner: .topRight, in: spectrum, id: "fpsReadout")
            }
            if settings.showBinWidth, let bin = band.binWidthHz {
                readout(Self.binWidthText(bin), colour: textColour, corner: .bottomRight, in: spectrum,
                        id: "binWidthReadout")
            }
            if settings.showPeakValue {
                TimelineView(.periodic(from: .now, by: Double(max(settings.peakValueDelayMs, 100)) / 1000)) { _ in
                    if let text = Self.peakText(band: band) {
                        readout(text, colour: BandColours.withAlpha(settings.peakValueColour),
                                corner: settings.peakValueCorner, in: spectrum, id: "peakValueReadout")
                    }
                }
            }
            if settings.showCursorFrequency, let hz = band.cursorHz {
                readout(Self.cursorText(hz), colour: BandColours.text, corner: .topLeft,
                        in: spectrum.insetBy(dx: 0, dy: 26), id: "cursorReadout",
                        background: BandColours.cursorBackground)
            }
            if settings.timestampPosition != .none, let time = band.topLineTime {
                let waterfall = layout.waterfall
                readout(Self.timeText(time, utc: settings.timestampUtc), colour: BandColours.timestamp,
                        size: BandColours.timestampPoints, weight: .regular,
                        corner: settings.timestampPosition == .left ? .topLeft : .topRight,
                        in: CGRect(x: 0, y: waterfall.minY, width: waterfall.width, height: waterfall.height),
                        id: "timestampReadout")
            }
            if band.lookBackLines > 0 {
                live(layout: layout)
            }
        }
    }

    private func readout(_ text: String, colour: Color, size: CGFloat = 11, weight: Font.Weight = .bold,
                         corner: OverlayCorner, in rect: CGRect, id: String,
                         background: Color = BandColours.readoutBackground) -> some View {
        let alignment: Alignment
        switch corner {
        case .topLeft:
            alignment = .topLeading
        case .topRight:
            alignment = .topTrailing
        case .bottomLeft:
            alignment = .bottomLeading
        case .bottomRight:
            alignment = .bottomTrailing
        }
        return Text(text)
            .font(.system(size: size, weight: weight).monospacedDigit())
            .foregroundStyle(colour)
            .padding(.horizontal, 6)
            .padding(.vertical, 3)
            .background(background, in: RoundedRectangle(cornerRadius: 3))
            .padding(6)
            .frame(width: max(rect.width, 0), height: max(rect.height, 0), alignment: alignment)
            .offset(x: rect.minX, y: rect.minY)
            .allowsHitTesting(false)
            .accessibilityIdentifier(id)
    }

    private func live(layout: BandLayout) -> some View {
        let waterfall = layout.waterfall
        return Button {
            band.goLive()
        } label: {
            Text(Self.liveText)
                .font(.system(size: 12, weight: .bold))
                .foregroundStyle(.white)
                .frame(minWidth: 56, minHeight: 32)
                .background(ChromeColours.txRed, in: RoundedRectangle(cornerRadius: 4))
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Back to the live waterfall")
        .accessibilityIdentifier("waterfallLive")
        .padding(8)
        .frame(width: waterfall.width - trailingInset, alignment: .topTrailing)
        .offset(x: 0, y: waterfall.minY)
    }

    // MARK: The words

    static let liveText = "LIVE"

    static func fpsText(_ fps: Double) -> String {
        String(format: "%.1f fps", fps)
    }

    /// The frames-a-second readout at its widest, three figures before the point.
    static let fpsWidestText = fpsText(999.9)

    /// Where the frames-a-second readout sits at its widest on a band laid
    /// out as `layout`: the spectrum's top right, as ``readout`` draws it
    /// (its word in an 11-point bold face, 6 points each side and 3 above
    /// and below, 6 points in from the corner). The WIDE chip keeps clear of it.
    static func fpsRect(layout: BandLayout) -> CGRect {
        let font = UIFont.monospacedDigitSystemFont(ofSize: 11, weight: .bold)
        let text = (fpsWidestText as NSString).size(withAttributes: [.font: font])
        let size = CGSize(width: ceil(text.width) + 12, height: ceil(text.height) + 6)
        return CGRect(x: layout.dbmScale.minX - 6 - size.width, y: 6, width: size.width, height: size.height)
    }

    static func binWidthText(_ hz: Double) -> String {
        String(format: "%.3f Hz/bin", hz)
    }

    static func cursorText(_ hz: Double) -> String {
        String(format: "%.4f MHz", hz / 1e6)
    }

    /// The strongest sample of the frame drawn, as the readout words it:
    /// `Peak -74 dBm, 7.2105 MHz`; nil before a frame.
    static func peakText(band: BandModel) -> String? {
        guard let frame = band.state.frame, !frame.traceDbm.isEmpty,
              let index = frame.traceDbm.indices.max(by: { frame.traceDbm[$0] < frame.traceDbm[$1] }),
              frame.traceDbm[index].isFinite else {
            return nil
        }
        let coverage = band.state.frameCoverage ?? BandCoverage(centerHz: band.centerHz, spanHz: band.spanHz)
        let hz = coverage.hz(forSample: index, samples: frame.traceDbm.count)
        // dBm to one decimal place, as the desktop's readout (value from
        // src/gui/SpectrumWidget.cpp:2429-2432).
        return "Peak \(String(format: "%.1f", Double(frame.traceDbm[index]))) dBm, \(String(format: "%.4f", hz / 1e6)) MHz"
    }

    /// A line's time as `HH:mm:ss`, UTC or the phone's own time zone.
    static func timeText(_ time: Double, utc: Bool) -> String {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = utc ? TimeZone(identifier: "UTC") : .current
        return formatter.string(from: Date(timeIntervalSince1970: time)) + (utc ? " UTC" : "")
    }
}
