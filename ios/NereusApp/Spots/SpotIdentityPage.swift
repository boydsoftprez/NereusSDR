// NereusSDR for iOS: your callsign and grid square, which the Core's spot sources sign in and report with
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Spot Hub's identity (spec section 5.7 item 1): your callsign and grid
/// square as the Core keeps them. Every source uses them unless its own
/// page sets another callsign.
struct SpotIdentityPage: View {
    @ObservedObject var spots: SpotsModel

    static let note = "Every source signs in or reports with these unless its own page sets another callsign."

    var body: some View {
        let enabled = spots.connected
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: "Identity", tag: .core)
            SpotHubPage.Card {
                AccessoryFields.TextRow(label: "Callsign", value: spots.setting(SpotsModel.callsignKey, ""),
                                        enabled: enabled, identifier: "spotIdentity.callsign") { value in
                    spots.write(SpotsModel.callsignKey, value.uppercased(), for: nil)
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 7)
                SpotHubPage.Line()
                AccessoryFields.TextRow(label: "Grid square", value: spots.setting(SpotsModel.gridKey, ""),
                                        enabled: enabled, identifier: "spotIdentity.grid") { value in
                    spots.write(SpotsModel.gridKey, value.uppercased(), for: nil)
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 7)
            }
            if !enabled {
                AccessoryChrome.Note(text: SpotsModel.notConnectedReason)
            }
            if let note = spots.displayNote {
                AccessoryChrome.Refusal(text: note) { spots.displayNote = nil }
            }
            ConnectChrome.Note(text: Self.note).padding(.top, 4)
        }
    }
}
