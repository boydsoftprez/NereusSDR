// NereusSDR for iOS: the Tuner Genius's three antennas, ANT 1 to 3 with the operator's names, on its page and the TX panel
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Tuner Genius's antennas as the desktop's tuner applet lays them out:
/// three equal buttons in a row, ANT 1 to 3, each with the name the
/// operator gave it. The one the Core reports is lit, and a tap asks the
/// Core for that antenna (`setTgxlAntenna`, 1 to 3). While the antennas
/// cannot be chosen, the buttons are greyed and the reason is under them.
/// The Tuner Genius page and the TX panel both show this row.
struct TunerAntennaRow: View {
    @ObservedObject var model: AccessoriesModel
    let tuner: AccessoriesModel.TunerGenius
    /// Each button's accessibility identifier is this and its number.
    let identifierPrefix: String

    var body: some View {
        let reason = model.tunerAntennaReason
        let labels = model.records?.tunerLabels ?? ["", "", ""]
        VStack(alignment: .leading, spacing: 6) {
            AccessoryChrome.Caption(text: "Antenna:")
            HStack(spacing: 4) {
                ForEach(1...3, id: \.self) { (port: Int) in
                    AccessoryChrome.ChoiceButton(label: "ANT \(port)", detail: labels[port - 1],
                                                 lit: Self.antennaLit(tuner, port: port), enabled: reason == nil) {
                        model.setTunerAntenna(Int64(port))
                    }
                    .accessibilityHint(reason ?? "")
                    .accessibilityIdentifier("\(identifierPrefix)\(port)")
                }
            }
            if let reason {
                AccessoryChrome.Note(text: reason)
            }
        }
    }

    /// Whether the ANT `port` button (1 to 3) is lit: the tuner's antenna,
    /// already counted from 1 by ``AccessoriesModel``.
    static func antennaLit(_ tuner: AccessoriesModel.TunerGenius, port: Int) -> Bool {
        tuner.antenna == Int64(port)
    }
}
