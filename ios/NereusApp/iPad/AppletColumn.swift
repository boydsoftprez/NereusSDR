// NereusSDR for iOS: the iPad's applet column on its side: the analog S-meter at its head, then RX, then TX with the amp and tuner
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The applet column (spec section 5.6 item 1, D30, picture 18): with the
/// iPad on its side, one column on the band's right like the desktop's main
/// window. The analog S-meter stays at its head; under it RX and then TX
/// (with the amp and tuner) scroll together. They are the phone's RX and TX
/// panels, laid in the column instead of sliding over the band. The
/// toolbar's corner button hides the column and brings it back.
struct AppletColumn: View {
    @ObservedObject var main: MainScreenModel
    @ObservedObject var rx: RxPanelModel
    @ObservedObject var transmit: TransmitModel
    /// Opens the Radio tab, where an amplifier or tuner is set up.
    var openRadio: () -> Void = {}
    var openSettings: () -> Void = {}

    init(main: MainScreenModel, openRadio: @escaping () -> Void = {}, openSettings: @escaping () -> Void = {}) {
        self.main = main
        rx = main.rx
        transmit = main.transmit
        self.openRadio = openRadio
        self.openSettings = openSettings
    }

    static let width = IPadLayout.columnWidth

    var body: some View {
        VStack(spacing: 0) {
            AnalogSMeter(model: main.sMeter)
            ScrollView {
                VStack(spacing: 0) {
                    RxPanel(model: rx, sliceColour: main.sliceColour, width: nil, scrolls: false)
                    TxPanel(transmit: transmit, accessories: main.accessories, micLevel: main.micLevel, modes: main.modes,
                            meters: main.band.catalog?.meters, openRadio: openRadio, width: nil, scrolls: false,
                            modMonitor: main.modMonitor, take: main.take, openSettings: openSettings)
                }
            }
            .overlay {
                // The column scrolls both panels, so it shows their number pad.
                ValuePadLayer(pad: rx.pad ?? transmit.pad)
            }
        }
        .frame(width: Self.width)
        .frame(maxHeight: .infinity, alignment: .top)
        .background(ChromeColours.panel)
        .overlay(alignment: .leading) {
            Rectangle().fill(ChromeColours.panelEdge).frame(width: 1)
        }
        .accessibilityElement(children: .contain)
        .accessibilityLabel("Applets")
        .accessibilityIdentifier("appletColumn")
    }
}
