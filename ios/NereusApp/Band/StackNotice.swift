// NereusSDR for iOS: what the band says about its 3D view: why it shows 2D now, and that it is held while transmitting
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// Over the band (JJ's 3D View board, 2026-09-29): while the pan has 3D
/// chosen but shows 2D for a reason of this phone's (it could not keep up,
/// Low Power Mode, or heat), a notice at the waterfall's top says why, with
/// Try 3D again (only when the phone could not keep up) and OK. While 3D is
/// drawn and held still by Stop on TX, a chip in the spectrum says so. 3D
/// stays the pan's choice throughout.
struct StackNotice: View {
    @ObservedObject var band: BandModel
    var sideways = false

    static let tryAgainLabel = "Try 3D again"
    static let okLabel = "OK"
    static let pausedText = "Paused while transmitting"

    /// The board's colours: the notice's card and edge, the chip's.
    static let card = ChromeColours.sheet
    static let cardEdge = ChromeColours.panelEdge
    static let chip = ChromeColours.bar.opacity(0.92)
    static let chipEdge = WorkspacePalette.borderMuted

    var body: some View {
        GeometryReader { proxy in
            if let axes = band.pointGeometry(size: proxy.size) {
                ZStack(alignment: .topLeading) {
                    if band.stackPausedWhileTransmitting {
                        Text(Self.pausedText)
                            .font(.system(size: 11, weight: .bold))
                            .foregroundStyle(ChromeColours.text)
                            .padding(.horizontal, 8)
                            .padding(.vertical, 4)
                            .background(Self.chip, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                            .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius).strokeBorder(Self.chipEdge, lineWidth: 1))
                            .offset(x: 12, y: max(axes.layout.strip.minY - 34, 4))
                            .allowsHitTesting(false)
                            .accessibilityIdentifier("stackPaused")
                    }
                    if let hold = band.stackNotice {
                        notice(hold)
                            .frame(maxWidth: sideways ? 546 : .infinity)
                            .padding(.horizontal, 10)
                            .offset(y: axes.layout.waterfall.minY + 8)
                    }
                }
            }
        }
    }

    private func notice(_ hold: StackedTraceHold) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            (Text(StackedTraceHold.noticeTitle).bold() + Text(" " + hold.notice(pan: MainScreenModel.panNumber)))
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.text)
                .lineSpacing(2)
                .fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("stackNoticeText")
            HStack(spacing: 8) {
                if hold == .cannotKeepUp {
                    PanelButton(label: Self.tryAgainLabel, lit: false, style: .blue) { band.tryStackAgain() }
                        .accessibilityIdentifier("stackTryAgain")
                }
                PanelButton(label: Self.okLabel, lit: false, style: .blue) { band.dismissStackNotice() }
                    .accessibilityIdentifier("stackNoticeOk")
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .background(Self.card, in: RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius))
        .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.panelRadius).strokeBorder(Self.cardEdge, lineWidth: 1))
        .shadow(color: .black.opacity(0.55), radius: 11, y: 8)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("stackNotice")
    }
}
