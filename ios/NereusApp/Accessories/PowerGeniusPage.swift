// NereusSDR for iOS: the Power Genius XL's page: OPERATE, its readings, the band it follows, the TX interlock and its records
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Power Genius XL (spec section 5.4 item 2, picture 11): on the
/// station's network; OPERATE; the output, SWR and heat gauges; output and
/// efficiency; the band it got from the radio and what it is paired with;
/// the TX interlock the Core enforces (its mode, the SWR gate and its grace
/// time), with its "Change in Setup" once this phone's Setup has the
/// interlock's page (Task 58; D41 leaves it out until then); its fault
/// history; and Advanced.
struct PowerGeniusPage: View {
    @ObservedObject var model: AccessoriesModel
    /// The Core's name: the Core pairs the amp with its radio.
    let coreName: String
    let open: (AccessoriesSection.Route) -> Void
    var now: () -> Date = Date.init

    /// A Core that does not send the `amplifier` object (document, Window behaviour).
    static let notReportedText = "This Core does not report its Power Genius to this app. Updating the Core may help."

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if let note = model.notes[.powerGenius] {
                AccessoryChrome.Refusal(text: note) { model.dismissNote(.powerGenius) }
            }
            if let amp = model.powerGenius {
                AccessorySetupCard(model: model, device: .powerGenius, open: open)
                AccessoryChrome.Header(name: "PGXL", status: AccessoryStatusLine.powerGenius(amp),
                                       button: amp.operate ? "OPERATE" : "STANDBY", lit: amp.operate,
                                       reason: model.switchReason(.powerGenius)) {
                    model.setAmpOperate(!amp.operate)
                }
                if !amp.link.error.isEmpty {
                    AccessoryChrome.Note(text: amp.link.error)
                }
                if let alert = model.powerCapAlert {
                    AccessoryChrome.Alert(text: alert)
                        .accessibilityIdentifier("pgxlPowerCapAlert")
                }
                AccessoryChrome.Card {
                    LinearGauge(scale: .ampPower, value: amp.forwardW)
                    LinearGauge(scale: .ampSwr, value: amp.swr)
                    LinearGauge(scale: .ampTemperature, value: amp.temperatureC)
                }
                AccessoryChrome.Card {
                    AccessoryChrome.ValueRow(label: "Output", value: String(format: "%.0f W", amp.forwardW))
                    AccessoryChrome.ValueRow(label: "MEffA", value: amp.efficiency.isEmpty ? "-" : amp.efficiency)
                    AccessoryChrome.ValueRow(label: "Band",
                                             value: AccessoryStatusLine.ampBand(amp, bandLabel: model.bandLabel))
                    AccessoryChrome.ValueRow(label: "Paired with", value: Self.pairedWith(amp, coreName: coreName))
                }
                interlock
                AccessoryChrome.Rows {
                    AccessoryChrome.PageRow(title: "Fault history",
                                            detail: AccessoryFaultsPage.summary(model.records?.faults[.powerGenius],
                                                                                now: now()),
                                            identifier: "pgxlFaults") {
                        open(.faults(.powerGenius))
                    }
                    AccessoryChrome.RowDivider()
                    AccessoryChrome.PageRow(title: "Advanced",
                                            detail: "Identity \u{00B7} Hardware \u{00B7} Network \u{00B7} Pairing "
                                                + "\u{00B7} Diagnostics",
                                            identifier: "pgxlAdvanced") {
                        open(.advanced(.powerGenius))
                    }
                }
            } else {
                AccessoryChrome.Card {
                    AccessoryChrome.Note(text: Self.notReportedText)
                }
            }
        }
        .accessibilityIdentifier("powerGeniusPage")
    }

    /// The TX interlock (the board's "TX interlock: Block" card).
    @ViewBuilder
    private var interlock: some View {
        AccessoryChrome.Card {
            if let records = model.records {
                Text(AccessoryStatusLine.interlockTitle(records.interlock))
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(ChromeColours.accent)
                    .accessibilityIdentifier("interlockTitle")
                Text(AccessoryStatusLine.interlockSummary(records.interlock))
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.text)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("interlockSummary")
            } else {
                AccessoryChrome.Note(text: AccessoriesModel.noRecordsText)
            }
            // "Change in Setup" joins this card with the Setup the Core describes (Task 58, D41).
        }
    }

    /// What the amp is paired with: the Core's radio, named as the Core is.
    static func pairedWith(_ amp: AccessoriesModel.PowerGenius, coreName: String) -> String {
        switch amp.bandFollow {
        case .following:
            return coreName.isEmpty ? "The Core's radio" : coreName
        case .waiting:
            return "Not paired"
        case .off, .thisComputerOnly:
            return "Not connected"
        }
    }
}
