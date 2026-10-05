// NereusSDR for iOS: the dBm scale's two arrow buttons: the scale's top by 10 dB, or in 3D, 3D Floor by 1 dB
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The dBm scale's ▲ and ▼ at its top (R-IOS-11, D78), as the desktop's
/// scale has them: each tap moves the scale's top 10 dB up or down and
/// keeps its bottom. Visible buttons in the scale's own column; the
/// renderer leaves their room free of labels (``BandLayout/dbmArrows``). An
/// arrow that can go no further is greyed. While the band draws 3D they
/// step 3D Floor by 1 dB instead, and say so (JJ's 3D View board,
/// recommendation 2).
struct DbmScaleArrows: View {
    @ObservedObject var display: DisplaySheetModel
    @ObservedObject var band: BandModel
    /// Sideways, the phone's rounded edge on the right, which the arrows keep inside.
    var trailingInset: CGFloat = 0

    static let raiseLabel = "Raise the scale's top 10 dB"
    static let lowerLabel = "Lower the scale's top 10 dB"
    static let floorUpLabel = "3D Floor 1 dB deeper"
    static let floorDownLabel = "3D Floor 1 dB shallower"

    var body: some View {
        GeometryReader { proxy in
            let rect = BandLayout(size: proxy.size, scale: 1, settings: band.shownSettings).dbmArrows
            let floor = display.arrowsMoveFloor
            VStack(spacing: 0) {
                arrow("\u{25B2}", label: floor ? Self.floorUpLabel : Self.raiseLabel,
                      enabled: floor ? display.canRaiseFloor : display.canRaiseTop, id: "dbmRaise") {
                    if floor {
                        display.nudgeFloor(up: true)
                    } else {
                        display.nudgeTop(up: true)
                    }
                }
                Rectangle().fill(BandColours.zoomBorder).frame(height: 1)
                arrow("\u{25BC}", label: floor ? Self.floorDownLabel : Self.lowerLabel,
                      enabled: floor ? display.canLowerFloor : display.canLowerTop, id: "dbmLower") {
                    if floor {
                        display.nudgeFloor(up: false)
                    } else {
                        display.nudgeTop(up: false)
                    }
                }
            }
            .frame(width: rect.width, height: rect.height)
            .background(BandColours.zoomBackground, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(BandColours.zoomBorder, lineWidth: 1))
            .clipShape(RoundedRectangle(cornerRadius: 4))
            .offset(x: rect.minX - trailingInset, y: rect.minY)
        }
    }

    private func arrow(_ glyph: String, label: String, enabled: Bool, id: String,
                       action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(glyph)
                .font(.system(size: 12, weight: .bold))
                .foregroundStyle(enabled ? BandColours.text : ChromeColours.buttonOffText)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityLabel(label)
        .accessibilityIdentifier(id)
    }
}
