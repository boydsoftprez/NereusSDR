// NereusSDR for iOS: the Modes tab's filter: the mode's presets and the slice's low and high edges, typed in
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Filter section: the active mode's presets from the Core's catalogue
/// (the RX panel's, ``RxPanelModel``), lit where the slice's filter is one,
/// then the slice's low and high edges as the Core holds them. A tap on an
/// edge opens the number pad for it (C2), as the desktop's typed and
/// dragged edges set it; a drag on the passband still tunes (D74).
struct FilterSection: View {
    @ObservedObject var model: ModesTabModel
    @ObservedObject var rx: RxPanelModel

    var body: some View {
        ModesChrome.section("Filter", subtitle: model.modeLabel.map { "\($0) presets" }) {
            ModesChrome.grid(columns: 5) {
                ForEach(rx.presets) { preset in
                    PanelButton(label: preset.text, lit: preset.lit, style: .blue) {
                        rx.selectPreset(preset)
                    }
                    .accessibilityLabel("\(preset.name), \(preset.text)")
                }
            }
            HStack(spacing: 6) {
                ModesChrome.label("Low")
                ValueField(text: ModesChrome.hertz(model.filterLowHz), accessibility: "Filter low edge",
                           disabled: model.filterLowHz == nil, minWidth: 86) {
                    model.openFilterEdgePad(low: true)
                }
                .accessibilityIdentifier("modesFilterLow")
                Text("High")
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                    .padding(.leading, 6)
                ValueField(text: ModesChrome.hertz(model.filterHighHz), accessibility: "Filter high edge",
                           disabled: model.filterHighHz == nil, minWidth: 86) {
                    model.openFilterEdgePad(low: false)
                }
                .accessibilityIdentifier("modesFilterHigh")
                Spacer(minLength: 0)
            }
        }
    }
}
