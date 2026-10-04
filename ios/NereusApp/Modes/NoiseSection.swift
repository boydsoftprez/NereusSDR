// NereusSDR for iOS: the Modes tab's noise buttons: NB, the noise reductions, ANF, SNB and APF, NNR's step-back and settings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Noise section: the RX panel's noise buttons (``RxPanelModel``), each
/// one the Core cannot run greyed with the Core's reason, and NNR marked in
/// the warning colour with the Core's reason and Try again while the Core
/// holds it back; then APF, in every mode as on the desktop flag, with its
/// tune for CW; then the reducers' settings (``NnrSettingsSection``).
struct NoiseSection: View {
    @ObservedObject var model: ModesTabModel
    @ObservedObject var rx: RxPanelModel

    var body: some View {
        ModesChrome.section("Noise") {
            ModesChrome.grid(columns: 4) {
                ForEach(rx.noise) { button in
                    PanelButton(label: button.label, lit: button.lit, style: .dsp, disabled: button.reason != nil,
                                warning: button.warning != nil) {
                        rx.tap(button)
                    }
                    .accessibilityHint(button.reason ?? button.warning ?? "")
                }
                PanelButton(label: "APF", lit: model.apf == true, style: .dsp, disabled: model.apfReason != nil) {
                    model.toggleApf()
                }
                .accessibilityHint(model.apfReason ?? "")
                .accessibilityIdentifier("modesApf")
            }
            ForEach(rx.noise.filter { $0.reason != nil }) { button in
                ModesChrome.note("\(button.label): \(button.reason ?? "")")
            }
            NnrStepBack(rx: rx, identifier: "modesNnr")
            if let reason = model.apfReason {
                ModesChrome.note("APF: \(reason)")
            }
            PanelSliderRow(label: "APF tune", value: model.apfTuneHz,
                           range: ModesTabModel.apfTuneRange,
                           accessibility: "APF tune", format: { "\(Int($0.rounded())) Hz" },
                           greyed: model.apfTuneReason != nil, notConfirmed: model.isUnconfirmed("apfTuneHz")) {
                model.setApfTune($0)
            }
            .accessibilityIdentifier("modesApfTune")
            if let reason = model.apfTuneReason {
                ModesChrome.note("APF tune: \(reason)")
            }
            NnrSettingsSection(model: model, rx: rx)
        }
    }
}
