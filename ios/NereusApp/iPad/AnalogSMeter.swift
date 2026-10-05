// NereusSDR for iOS: the iPad's analog S-meter: its title bar with the meter menu, and the chosen face
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The S-meter applet at the head of the iPad's applets (spec section 5.6,
/// D30, D31, D86): its title bar, whose ☰ opens the meter's menu, then the
/// chosen face. Receiving, the needle reads the RX Mode's reading of the
/// active slice; on the air it swings to the TX Mode's reading. A press
/// and hold on the meter opens the same menu (D78: a shortcut to the ☰).
struct AnalogSMeter: View {
    @ObservedObject var model: SMeterModel

    /// The meter's height for its width (the desktop's proportions).
    static let heightShare: CGFloat = 0.5
    /// The title bar's height.
    static let titleHeight: CGFloat = 22

    var body: some View {
        VStack(spacing: 0) {
            title
            face
                .clipped()
                .aspectRatio(1 / Self.heightShare, contentMode: .fit)
                .frame(maxWidth: .infinity)
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("S-meter")
                .accessibilityValue(model.display.spoken)
                .accessibilityIdentifier("analogSMeter")
                .contextMenu {
                    SMeterMenu(model: model)
                }
        }
        .background(ChromeColours.page)
        .onAppear { model.appeared() }
        .onDisappear { model.disappeared() }
    }

    private var title: some View {
        HStack(spacing: 0) {
            Text("S-Meter")
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(ChromeColours.icon)
            Spacer(minLength: 0)
            Menu {
                SMeterMenu(model: model)
            } label: {
                Image(systemName: "line.3.horizontal")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(ChromeColours.icon)
                    .frame(width: 44, height: Self.titleHeight)
                    .contentShape(Rectangle())
            }
            .accessibilityLabel("Meter menu")
            .accessibilityIdentifier("sMeterMenu")
        }
        .padding(.leading, 8)
        .frame(maxWidth: .infinity, minHeight: Self.titleHeight, maxHeight: Self.titleHeight)
        .background(LinearGradient(stops: [.init(color: ChromeColours.titleTop, location: 0),
                                           .init(color: ChromeColours.titleMiddle, location: 0.5),
                                           .init(color: ChromeColours.titleBottom, location: 1)],
                                   startPoint: .top, endPoint: .bottom))
        .overlay(alignment: .bottom) {
            Rectangle().fill(ChromeColours.titleBorder).frame(height: 1)
        }
    }

    @ViewBuilder
    private var face: some View {
        let display = model.display
        if let theme = model.state.settings.face.theme {
            VintageMeterFace(display: display, needle: model.needle.fraction, theme: theme)
        } else {
            ClassicMeterFace(display: display, needle: model.needle.fraction)
        }
    }
}
