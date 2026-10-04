// NereusSDR for iOS: one accessory page under the Radio tab, with its bar and its way back
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// A page the Radio tab's accessories open (picture 11): the bar with the
/// page's title, the way back to the page before ("‹ Radio" from an
/// accessory's page) and the link chip, over the page itself.
struct AccessoryScreen<Chip: View>: View {
    @ObservedObject var model: AccessoriesModel
    /// Transmit, for the Tuner Genius's TUNE.
    let transmit: TransmitModel
    let route: AccessoriesSection.Route
    /// The page before, for the back button's words.
    let backTitle: String
    let coreName: String
    let back: () -> Void
    let open: (AccessoriesSection.Route) -> Void
    @ViewBuilder var chip: () -> Chip

    var body: some View {
        VStack(spacing: 0) {
            ConnectChrome.NavBar(title: route.title, back: backTitle, onBack: back) {
                chip()
            }
            ScrollView {
                page
                    .padding(12)
                    .padding(.bottom, 8)
            }
        }
        .background(ChromeColours.page.ignoresSafeArea(edges: [.top, .horizontal]))
    }

    @ViewBuilder
    private var page: some View {
        switch route {
        case .page(.powerGenius):
            PowerGeniusPage(model: model, coreName: coreName, open: open)
        case .page(.tunerGenius):
            TunerGeniusPage(model: model, transmit: transmit, open: open)
        case .page(.rfKit):
            Rf2ksPage(model: model, open: open)
        case .faults(let device):
            AccessoryFaultsPage(model: model, device: device)
        case .advanced(let device):
            AccessoryAdvancedPage(model: model, device: device, coreName: coreName)
        case .tuneMemory:
            TuneMemoryPage(model: model)
        }
    }
}
