// NereusSDR for iOS: what the band says while the Core's radio is keyed on it: high SWR, and why the view is as it is
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// Over the band while the Core's radio is keyed on it (R-IOS-11, R-IOS-13;
/// desktop PR #317 parity): a red border and "High SWR" while the Core says
/// the SWR is over its limit (with the power turned down when the Core has
/// turned it down), and a line at the spectrum's foot when the band cannot
/// show what the desktop's would: a Core that does not send its transmit
/// display, or a transmit view another device is setting. The border's
/// sizes are the desktop's (6 points, 3 in from the edge, D83). Nothing
/// here takes a touch.
struct TransmitDisplayBanner: View {
    @ObservedObject var band: BandModel

    /// The desktop's pan status words for a Core that sends no transmit display.
    static let missingText = "This Core does not send its transmit display. Updating the Core may help."
    static let sharedText = "Another device is setting this transmit view."
    static let highSwrText = "High SWR"
    static let powerTurnedDownText = "High SWR: power turned down"
    static let borderWidth: CGFloat = 6
    static let borderInset: CGFloat = 3

    /// The line at the spectrum's foot, or nil for none.
    static func note(missing: Bool, shared: Bool) -> String? {
        if missing {
            return missingText
        }
        return shared ? sharedText : nil
    }

    /// The high-SWR words, or nil while the SWR is not high.
    static func swrText(_ transmit: TransmitDisplay) -> String? {
        guard transmit.highSwr else {
            return nil
        }
        return transmit.windBackLatched ? powerTurnedDownText : highSwrText
    }

    var body: some View {
        GeometryReader { proxy in
            ZStack(alignment: .topLeading) {
                if let swr = Self.swrText(band.transmit) {
                    Rectangle()
                        .strokeBorder(Color.red, lineWidth: Self.borderWidth)
                        .padding(Self.borderInset)
                        .frame(width: proxy.size.width, height: proxy.size.height)
                    // In the spectrum, clear of the keyed gauges on the waterfall.
                    let spectrum = band.pointGeometry(size: proxy.size)?.layout.spectrum
                        ?? CGRect(origin: .zero, size: proxy.size)
                    Text(swr)
                        .font(.system(size: 22, weight: .heavy))
                        .foregroundStyle(Color.red)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 10)
                        .padding(.vertical, 4)
                        .background(Color.black.opacity(0.7), in: RoundedRectangle(cornerRadius: 4))
                        .position(x: proxy.size.width / 2, y: spectrum.midY)
                        .accessibilityIdentifier("highSwr")
                }
                if let note = Self.note(missing: band.transmitDisplayMissing,
                                        shared: band.transmitViewShared && band.showsTransmit),
                   let axes = band.pointGeometry(size: proxy.size) {
                    Text(note)
                        .font(.system(size: 11, weight: .bold))
                        .foregroundStyle(BandColours.text)
                        .multilineTextAlignment(.leading)
                        .padding(.horizontal, 6)
                        .padding(.vertical, 3)
                        .background(BandColours.readoutBackground, in: RoundedRectangle(cornerRadius: 3))
                        .frame(width: axes.layout.dbmScale.minX - 12, alignment: .leading)
                        .offset(x: 6, y: max(axes.layout.strip.minY - 40, 4))
                        .accessibilityIdentifier("transmitDisplayNote")
                }
            }
        }
        .allowsHitTesting(false)
    }
}
