// NereusSDR for iOS: the Modes tab's receive leveler (RXL): its mode, live gain and Custom settings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-11-14: New; J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-11-15: the live gain line watches only the gain (RxlGainLine). J.J.
//   Boyd (KG4VCF), AI-assisted via Claude Code.

import SwiftUI

/// RXL below the noise buttons: Off, Auto and Custom as the desktop flag's
/// popup offers them, the Core's live gain while it is on, and the five
/// settings Custom uses, greyed in Off and Auto, each bounded by the
/// Core's catalogue. The RXL switch itself sits beside APF in
/// ``NoiseSection``.
struct RxLevelerSection: View {
    @ObservedObject var model: ModesTabModel

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            ModesChrome.caption("RXL (receive leveler):")
            ModesChrome.grid(columns: 3) {
                ForEach(ModesTabModel.RxlMode.allCases) { mode in
                    PanelButton(label: mode.label, lit: model.rxlMode == mode, style: .blue,
                                disabled: model.rxlReason != nil) {
                        model.selectRxlMode(mode)
                    }
                    .accessibilityLabel("RXL \(mode.label)")
                    .accessibilityIdentifier("modesRxl\(mode.label)")
                }
            }
            if let reason = model.rxlReason {
                ModesChrome.note("RXL: \(reason)")
            }
            RxlGainLine(gain: model.rxlGain)
            if let note = model.rxlModeNote {
                ModesChrome.note(note)
            }
            ForEach(ModesTabModel.rxlSettings) { setting in
                PanelSliderRow(label: setting.label, value: model.rxlValues[setting.property],
                               range: model.rxlRanges[setting.rangeName],
                               accessibility: "RXL \(setting.label)",
                               format: { "\(Int($0.rounded())) \(setting.unit)" },
                               greyed: model.rxlSettingsReason != nil,
                               notConfirmed: model.isUnconfirmed(setting.property)) {
                    model.setRxlSetting(setting, $0)
                }
                .accessibilityIdentifier("modesRxl\(setting.property)")
            }
            if let reason = model.rxlSettingsReason, model.rxlReason == nil {
                ModesChrome.note(reason)
            }
        }
    }
}

/// "Gain now: +3.2 dB" while RXL is on: the only view a new live gain
/// redraws.
struct RxlGainLine: View {
    @ObservedObject var gain: ModesTabModel.RxlGain

    var body: some View {
        if let text = gain.text {
            Text(text)
                .font(.system(size: 11))
                .foregroundStyle(ChromeColours.textDim)
                .accessibilityIdentifier("modesRxlGain")
        }
    }
}
