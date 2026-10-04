// NereusSDR for iOS: the iPad upright: the S-meter, RX and TX in three columns below the band, like a radio's front panel
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The front panel (spec section 5.6 item 2, D31, picture 19): with the
/// iPad upright the band keeps the whole width, so both flags stay full,
/// and the applets sit below it in three equal columns: the analog S-meter,
/// RX, and TX with the amp and tuner. RX and TX each scroll on their own,
/// under their title bars. Turned on its side, the screen goes back to the
/// applet column.
struct FrontPanelColumns: View {
    @ObservedObject var main: MainScreenModel
    /// Opens the Radio tab, where an amplifier or tuner is set up.
    var openRadio: () -> Void = {}
    var openSettings: () -> Void = {}

    /// The three columns, in the board's order.
    enum Column: CaseIterable {
        case sMeter
        case rx
        case tx
    }

    /// Each column's width in a panel `width` wide: equal thirds, the
    /// dividing lines taken from the columns they follow.
    static func columnWidth(panelWidth width: CGFloat) -> CGFloat {
        (max(width, 0) / CGFloat(Column.allCases.count)).rounded(.down)
    }

    var body: some View {
        GeometryReader { proxy in
            let column = Self.columnWidth(panelWidth: proxy.size.width)
            HStack(spacing: 0) {
                AnalogSMeter(model: main.sMeter)
                    .frame(width: column)
                    .frame(maxHeight: .infinity, alignment: .top)
                    .background(ChromeColours.panel)
                divider
                RxPanel(model: main.rx, sliceColour: main.sliceColour, width: nil)
                    .frame(width: column - 1)
                divider
                TxPanel(transmit: main.transmit, accessories: main.accessories, micLevel: main.micLevel, modes: main.modes,
                        meters: main.band.catalog?.meters, openRadio: openRadio, width: nil,
                        modMonitor: main.modMonitor, take: main.take, openSettings: openSettings)
                    .frame(maxWidth: .infinity)
            }
        }
        .background(ChromeColours.panel)
        .overlay(alignment: .top) {
            Rectangle().fill(ChromeColours.panelEdge).frame(height: 1)
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Applets")
        .accessibilityIdentifier("frontPanel")
    }

    private var divider: some View {
        Rectangle().fill(ChromeColours.panelEdge).frame(width: 1)
    }
}
