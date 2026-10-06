// NereusSDR for iOS: the Tuner Genius XL's page: OPERATE, TUNE, its relays, the three antennas and its tune memory
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Tuner Genius XL (spec section 5.4 item 3, picture 11): on the
/// station's network; OPERATE; the power and SWR through it and its three
/// matching relays, each with minus and plus that move it a step as a
/// remote window's wheel does; TUNE and BYPASS; a line when its relays hold a tune from its
/// memory; the three antennas with the operator's names; the tune memory;
/// and Advanced.
///
/// TUNE runs the Core's autotune, a key of this phone's (`tx.tunerTune` at
/// `remoteTxVersion` 2, keyed and ended through the PTT as the TX panel's
/// tuner TUNE is); greyed, its reason shows under it.
struct TunerGeniusPage: View {
    @ObservedObject var model: AccessoriesModel
    @ObservedObject var transmit: TransmitModel
    let open: (AccessoriesSection.Route) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if let note = model.notes[.tunerGenius] {
                AccessoryChrome.Refusal(text: note) { model.dismissNote(.tunerGenius) }
            }
            if let tuner = model.tunerGenius {
                AccessorySetupCard(model: model, device: .tunerGenius, open: open)
                let operating = tuner.operate && !tuner.bypass
                AccessoryChrome.Header(name: "TGXL", status: AccessoryStatusLine.tunerGenius(tuner),
                                       button: Self.buttonLabel(tuner), lit: operating,
                                       reason: model.switchReason(.tunerGenius)) {
                    model.setTunerOperate(!operating)
                }
                if !tuner.link.error.isEmpty {
                    AccessoryChrome.Note(text: tuner.link.error)
                }
                AccessoryChrome.Card {
                    LinearGauge(scale: .ampPower, value: tuner.forwardW)
                    LinearGauge(scale: .ampSwr, value: tuner.swr)
                    ForEach(0..<3, id: \.self) { (relay: Int) in
                        relayRow(relay, value: tuner.relays[relay])
                    }
                    if let reason = model.relayReason {
                        AccessoryChrome.Note(text: reason)
                    }
                }
                HStack(spacing: 4) {
                    AccessoryChrome.ChoiceButton(label: "TUNE", lit: transmit.ptt.tunerTuning,
                                                 enabled: transmit.tunerTuneReason == nil) {
                        transmit.toggleTunerTune()
                    }
                    .accessibilityHint(transmit.tunerTuneReason ?? "")
                    .accessibilityIdentifier("tgxlTune")
                    AccessoryChrome.ChoiceButton(label: "BYPASS", lit: tuner.bypass,
                                                 enabled: model.switchReason(.tunerGenius) == nil) {
                        model.setTunerBypass(!tuner.bypass)
                    }
                    .accessibilityHint(model.switchReason(.tunerGenius) ?? "")
                    .accessibilityIdentifier("tgxlBypass")
                }
                if let reason = transmit.tunerTuneReason {
                    AccessoryChrome.Note(text: "TUNE: " + reason)
                        .accessibilityIdentifier("tgxlTuneReason")
                }
                TxNoticeCard(transmit: transmit)
                if let records = model.records,
                   let line = Self.recalledLine(tuner, memory: records.tuneMemory) {
                    AccessoryChrome.Note(text: line)
                        .accessibilityIdentifier("tgxlRecalled")
                }
                TunerAntennaRow(model: model, tuner: tuner, identifierPrefix: "tgxlAntenna")
                AccessoryChrome.Rows {
                    AccessoryChrome.PageRow(title: "Tune memory",
                                            detail: TuneMemoryPage.summary(model.records?.tuneMemory),
                                            identifier: "tgxlTuneMemory") {
                        open(.tuneMemory)
                    }
                    AccessoryChrome.RowDivider()
                    AccessoryChrome.PageRow(title: "Advanced",
                                            detail: "Identity \u{00B7} Antenna Labels \u{00B7} Network \u{00B7} "
                                                + "Diagnostics",
                                            identifier: "tgxlAdvanced") {
                        open(.advanced(.tunerGenius))
                    }
                }
            } else {
                AccessoryChrome.Card {
                    AccessoryChrome.Note(text: Self.notReportedText)
                }
            }
        }
        .accessibilityIdentifier("tunerGeniusPage")
    }

    static let notReportedText = "This Core does not report its Tuner Genius to this app. Updating the Core may help."

    /// One matching relay's bar, with minus and plus that move it one step.
    private func relayRow(_ relay: Int, value: Int64) -> some View {
        let names = ["C1", "L", "C2"]
        let enabled = model.relayReason == nil
        return HStack(spacing: 6) {
            AccessoryFields.SmallButton(title: "\u{2212}", enabled: enabled) {
                model.moveTunerRelay(Int64(relay), direction: -1)
            }
            .accessibilityLabel("Move \(names[relay]) down")
            .accessibilityIdentifier("tgxlRelay\(relay).down")
            AccessoryChrome.RelayBar(label: names[relay], value: value)
            AccessoryFields.SmallButton(title: "+", enabled: enabled) {
                model.moveTunerRelay(Int64(relay), direction: 1)
            }
            .accessibilityLabel("Move \(names[relay]) up")
            .accessibilityIdentifier("tgxlRelay\(relay).up")
        }
    }

    /// Whether the ANT `port` button (1 to 3) is lit: the tuner's antenna,
    /// already counted from 1 by ``AccessoriesModel``.
    static func antennaLit(_ tuner: AccessoriesModel.TunerGenius, port: Int) -> Bool {
        TunerAntennaRow.antennaLit(tuner, port: port)
    }

    /// OPERATE, BYPASS or STANDBY, as the tuner reports it.
    static func buttonLabel(_ tuner: AccessoriesModel.TunerGenius) -> String {
        if tuner.bypass {
            return "BYPASS"
        }
        return tuner.operate ? "OPERATE" : "STANDBY"
    }

    /// "Recalled from tune memory for ANT 1 on 40m." while the tuner's
    /// relays hold the stored tune for its antenna; nil otherwise. The Core
    /// sends no separate recall event, so the relays matching a stored tune
    /// for the chosen antenna is what the line reads.
    static func recalledLine(_ tuner: AccessoriesModel.TunerGenius,
                             memory: [AccessoriesModel.StoredTune]) -> String? {
        guard tuner.link.connected, tuner.relays.contains(where: { $0 != 0 }),
              let stored = memory.first(where: { $0.antenna == tuner.antenna && $0.relays == tuner.relays }) else {
            return nil
        }
        return "Recalled from tune memory for ANT \(stored.antenna) on \(stored.band)."
    }
}
