// NereusSDR for iOS: the S-meter's menu: RX Mode, TX Mode, Peak Hold and Meter Face, as the desktop's meter menu has them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import SwiftUI

/// The S-meter's menu (D86): the ☰ on its title bar opens it, and a press
/// and hold on the meter opens the same items as a shortcut (D78). RX Mode
/// (Signal, Sig Avg, Signal Peak, Max Bin), TX Mode (Power, SWR, Level,
/// Compression), Peak Hold (Enabled, Decay, Reset) and Meter Face (the
/// six vintage faces, then Classic). A mode whose reading the Core does
/// not send is disabled, with its reason under it.
struct SMeterMenu: View {
    @ObservedObject var model: SMeterModel

    var body: some View {
        let settings = model.state.settings
        Menu("RX Mode") {
            ForEach(SMeterRxMode.allCases, id: \.self) { mode in
                item(mode.label, chosen: settings.rxMode == mode, reason: model.reason(for: mode)) {
                    model.chooseRx(mode)
                }
            }
        }
        Menu("TX Mode") {
            ForEach(SMeterTxMode.allCases, id: \.self) { mode in
                item(mode.label, chosen: settings.txMode == mode, reason: model.reason(for: mode)) {
                    model.chooseTx(mode)
                }
            }
        }
        Menu("Peak Hold") {
            item("Enabled", chosen: settings.peakHold, reason: nil) {
                model.setPeakHold(!settings.peakHold)
            }
            Menu("Decay") {
                ForEach(SMeterPeakDecay.allCases, id: \.self) { decay in
                    item(decay.label, chosen: settings.peakDecay == decay, reason: nil) {
                        model.choosePeakDecay(decay)
                    }
                }
            }
            Divider()
            Button("Reset") {
                model.resetPeak()
            }
        }
        Menu("Meter Face") {
            ForEach(SMeterFace.vintage, id: \.self) { face in
                item(face.label, chosen: settings.face == face, reason: nil) {
                    model.chooseFace(face)
                }
            }
            Divider()
            item(SMeterFace.classic.label, chosen: settings.face == .classic, reason: nil) {
                model.chooseFace(.classic)
            }
        }
    }

    /// One choice: a tick when chosen, and when it cannot be chosen its
    /// reason under it and the item disabled.
    private func item(_ title: String, chosen: Bool, reason: String?,
                      action: @escaping () -> Void) -> some View {
        Button(action: action) {
            if chosen {
                Label(title, systemImage: "checkmark")
            } else {
                Text(title)
            }
            if let reason {
                Text(reason)
            }
        }
        .disabled(reason != nil)
    }
}
