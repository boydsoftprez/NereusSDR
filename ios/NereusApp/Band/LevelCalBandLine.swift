// NereusSDR for iOS: the band's line while a receive level calibration runs, saying why the slice moved
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// Over the band's waterfall while the Core reports a level calibration
/// running (JJ's Level Cal board, 2026-09-30): one line in the phone's own
/// words, "Level calibration is running on slice A.", on the card the
/// band's other notices use. It says why a slice jumped to the run's
/// frequency and mode; it goes when the run ends.
struct LevelCalBandLine: View {
    @ObservedObject var band: BandModel
    @ObservedObject var levelCal: LevelCalModel
    var sideways = false

    /// The board's blue dot.
    static let dot = Color(red: 0x5B / 255.0, green: 0x9C / 255.0, blue: 0xF5 / 255.0)

    var body: some View {
        GeometryReader { proxy in
            if let line = levelCal.bandLine, let axes = band.pointGeometry(size: proxy.size) {
                HStack(spacing: 10) {
                    Circle()
                        .fill(Self.dot)
                        .frame(width: 9, height: 9)
                        .accessibilityHidden(true)
                    Text(line)
                        .font(.system(size: 13))
                        .foregroundStyle(ChromeColours.text)
                        .fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: 0)
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 10)
                .background(StackNotice.card, in: RoundedRectangle(cornerRadius: 8))
                .overlay(RoundedRectangle(cornerRadius: 8).strokeBorder(StackNotice.cardEdge, lineWidth: 1))
                .frame(maxWidth: sideways ? 546 : .infinity)
                .padding(.horizontal, 10)
                .offset(y: axes.layout.waterfall.minY + 8)
                .allowsHitTesting(false)
                .accessibilityElement(children: .combine)
                .accessibilityIdentifier("levelCalBandLine")
            }
        }
    }
}
