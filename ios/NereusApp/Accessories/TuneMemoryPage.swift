// NereusSDR for iOS: the Tuner Genius's tune memory: the stored tunes by antenna and band, as the Core keeps them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The tune memory (spec section 5.4 item 3): each stored tune, by band
/// then antenna as the Core sends them, with the antenna's name and the
/// three relay settings, and Clear, which forgets it at the Core, as the
/// desktop's Tuner Genius menu does (parity row M13); then whether the Core
/// recalls a stored tune by itself when the band or antenna changes. The
/// Core keeps the memory; recalling a tune is a tune on the air, so the
/// phone only shows it.
struct TuneMemoryPage: View {
    @ObservedObject var model: AccessoriesModel

    static let autoRecallLabel = "Recall a stored tune on a band or antenna change"

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if let note = model.notes[.tunerGenius] {
                AccessoryChrome.Refusal(text: note) { model.dismissNote(.tunerGenius) }
            }
            if let records = model.records {
                if records.tuneMemory.isEmpty {
                    AccessoryChrome.Card {
                        AccessoryChrome.Note(text: "No stored tunes.")
                    }
                } else {
                    AccessoryChrome.Rows {
                        ForEach(Array(records.tuneMemory.enumerated()), id: \.element.id) { index, tune in
                            if index > 0 {
                                AccessoryChrome.RowDivider()
                            }
                            row(tune, labels: records.tunerLabels)
                        }
                    }
                }
                AccessoryChrome.Card {
                    AccessoryFields.SwitchRow(label: Self.autoRecallLabel, isOn: records.autoRecall,
                                              enabled: model.recordsReason == nil,
                                              identifier: "tuneMemoryAutoRecall") {
                        model.setAutoRecall($0)
                    }
                    if let reason = model.recordsReason {
                        AccessoryChrome.Note(text: reason)
                    }
                }
            } else {
                AccessoryChrome.Card {
                    AccessoryChrome.Note(text: AccessoriesModel.noRecordsText)
                }
            }
        }
        .accessibilityIdentifier("tuneMemoryPage")
    }

    private func row(_ tune: AccessoriesModel.StoredTune, labels: [String]) -> some View {
        HStack(spacing: 10) {
            VStack(alignment: .leading, spacing: 2) {
                Text("\(tune.band) \u{00B7} \(Self.antennaName(tune.antenna, labels: labels))")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(ChromeColours.text)
                Text("C1 \(tune.relays[0]) \u{00B7} L \(tune.relays[1]) \u{00B7} C2 \(tune.relays[2])")
                    .font(.system(size: 11, design: .monospaced))
                    .foregroundStyle(ChromeColours.textDim)
            }
            Spacer(minLength: 0)
            AccessoryFields.SmallButton(title: "Clear", enabled: model.recordsReason == nil) {
                model.clearStoredTune(tune)
            }
            .accessibilityLabel("Clear \(tune.band) \(Self.antennaName(tune.antenna, labels: labels))")
            .accessibilityHint(model.recordsReason ?? "Forgets this stored tune")
            .accessibilityIdentifier("tuneMemoryClear.\(tune.id)")
        }
        .padding(10)
        .accessibilityElement(children: .contain)
    }

    /// "ANT 1 (Beam)", or "ANT 1" without a name.
    static func antennaName(_ antenna: Int64, labels: [String]) -> String {
        let index = Int(antenna) - 1
        guard labels.indices.contains(index), !labels[index].isEmpty else {
            return "ANT \(antenna)"
        }
        return "ANT \(antenna) (\(labels[index]))"
    }

    /// The Tune memory row's line.
    static func summary(_ memory: [AccessoriesModel.StoredTune]?) -> String {
        guard let memory else {
            return "Kept by the Core"
        }
        switch memory.count {
        case 0:
            return "No stored tunes"
        case 1:
            return "1 stored tune"
        default:
            return "\(memory.count) stored tunes, by antenna and band"
        }
    }
}
