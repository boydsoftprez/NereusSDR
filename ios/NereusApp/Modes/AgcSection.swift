// NereusSDR for iOS: the Modes tab's AGC: the Core's AGC modes, AGC-T and AUTO
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The AGC section: the AGC modes from the Core's catalogue (the RX
/// panel's), then AGC-T over the catalogue's threshold range and AUTO.
struct AgcSection: View {
    @ObservedObject var model: ModesTabModel
    @ObservedObject var rx: RxPanelModel

    var body: some View {
        ModesChrome.section("AGC") {
            ModesChrome.grid(columns: 5) {
                ForEach(rx.agc) { choice in
                    PanelButton(label: choice.label, lit: choice.lit, style: .blue) {
                        rx.selectAgc(choice.id)
                    }
                }
            }
            HStack(spacing: 6) {
                PanelSliderRow(label: "AGC-T", value: model.agcThreshold, range: model.agcThresholdRange,
                               accessibility: "AGC threshold", notConfirmed: model.isUnconfirmed("agcThreshold")) { model.setAgcThreshold($0) }
                PanelButton(label: "AUTO", lit: model.autoAgc == true, style: .dsp, disabled: model.autoAgc == nil) {
                    model.toggleAutoAgc()
                }
                .frame(width: 62)
                .accessibilityLabel("Automatic AGC threshold")
            }
            // What AUTO chose, as the desktop flag says it while AUTO is on (M1).
            if let info = model.autoAgcInfo {
                Text(info)
                    .font(.system(size: 11).monospacedDigit())
                    .foregroundStyle(ChromeColours.textDim)
                    .accessibilityIdentifier("modesAutoAgcInfo")
            }
            if model.olderCore.contains(ModesTabModel.Property.agcThreshold)
                || model.olderCore.contains(ModesTabModel.Property.autoAgcEnabled) {
                ModesChrome.note("AGC-T: \(CatalogFeed.needsNewerCoreText)")
            }
        }
    }
}
