// NereusSDR for iOS: one FreeDV Reporter station's every column, with tune, ask to QSY, look up and copy
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI
import UIKit

/// A station's details (spec section 5.7 item 8, picture 21's second
/// phone): every column the desktop's list shows for it, then the
/// desktop's right-click actions: tune there, ask to QSY, look up the
/// callsign on QRZ.com or HamQTH (in Safari), copy the callsign. Reached
/// from the row's (i) button, or by press and hold as a shortcut (D78).
struct FreeDVStationDetails: View {
    @ObservedObject var freedv: FreeDVReporterModel
    let station: FreeDVStation
    @Environment(\.openURL) private var openURL
    @State private var copied = false

    var body: some View {
        FreeDVChrome.Sheet(identifier: "freedv.detailsSheet") {
            HStack(alignment: .firstTextBaseline) {
                Text(station.callsign)
                    .font(.system(size: 18, weight: .bold))
                    .foregroundStyle(ChromeColours.textBright)
                Spacer()
                Button("Close") { freedv.openDetails = nil }
                    .font(.system(size: 14, weight: .semibold))
                    .foregroundStyle(ChromeColours.accent)
                    .accessibilityIdentifier("freedv.details.close")
            }
            Grid(alignment: .leadingFirstTextBaseline, horizontalSpacing: 10, verticalSpacing: 5) {
                ForEach(freedv.detailRows(station), id: \.name) { row in
                    GridRow {
                        Text(row.name)
                            .foregroundStyle(ChromeColours.textDim)
                            .frame(width: 84, alignment: .leading)
                        Text(row.value)
                            .fontWeight(.semibold)
                            .foregroundStyle(ChromeColours.text)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
            }
            .font(.system(size: 12.5))
            .padding(10)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
            let tuneReason = freedv.tuneReason(station)
            let qsyReason = freedv.qsyReason
            HStack(spacing: 8) {
                FreeDVChrome.Wide(title: "Tune there", primary: true, enabled: tuneReason == nil,
                                  identifier: "freedv.details.tune") { freedv.tune(station) }
                FreeDVChrome.Wide(title: "Ask to QSY", enabled: qsyReason == nil,
                                  identifier: "freedv.details.qsy") { freedv.askToQsy(station) }
            }
            HStack(spacing: 8) {
                let qrz = FreeDVReporterModel.qrzURL(station.callsign)
                let hamQth = FreeDVReporterModel.hamQthURL(station.callsign)
                FreeDVChrome.Wide(title: "Look up on QRZ.com", enabled: qrz != nil,
                                  identifier: "freedv.details.qrz") { if let qrz { openURL(qrz) } }
                FreeDVChrome.Wide(title: "Look up on HamQTH", enabled: hamQth != nil,
                                  identifier: "freedv.details.hamqth") { if let hamQth { openURL(hamQth) } }
            }
            FreeDVChrome.Wide(title: copied ? "Copied" : "Copy callsign", enabled: !station.callsign.isEmpty,
                              identifier: "freedv.details.copy") {
                UIPasteboard.general.string = station.callsign
                copied = true
            }
            if let reason = tuneReason ?? qsyReason {
                AccessoryChrome.Note(text: reason)
            }
        }
    }
}
