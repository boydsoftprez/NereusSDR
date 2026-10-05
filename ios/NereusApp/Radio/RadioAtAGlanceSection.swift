// NereusSDR for iOS: the Radio tab's radio at a glance: model, firmware, protocol, rate, slices, PA, ADC and the Core's CPU
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The radio at a glance (spec section 5.2 item 5, the board's "Radio"
/// block): each line's name at the left and the Core's value at the right.
/// A value the Core does not send reads Unavailable, with why under it;
/// when every line is unavailable for one reason (no Core, no radio) the
/// reason shows once. While the tab is on screen it reads again each
/// second, so a reading older than three seconds goes unavailable.
struct RadioAtAGlanceSection: View {
    @ObservedObject var model: RadioTabModel
    /// The phone's clock the Core's telemetry was stamped with on arrival.
    let now: () -> Int64

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            AccessoryChrome.Caption(text: "Radio")
            // `revision` moves once a second only while the tab is on screen (RadioTabModel.show()).
            let _ = model.revision
            RadioAtAGlanceSection.Lines(rows: model.glance(nowMilliseconds: now()), identifier: "radio.glance")
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("radioGlance")
    }

    /// A card of name and value lines, shared with Protocol Info.
    struct Lines: View {
        let rows: [RadioAtAGlance.Row]
        let identifier: String

        var body: some View {
            let reasons = Set(rows.map(\.reading.reason))
            let shared: String? = rows.count > 1 && reasons.count == 1 ? reasons.first ?? nil : nil
            AccessoryChrome.Card {
                if let shared {
                    AccessoryChrome.Note(text: shared)
                        .accessibilityIdentifier("\(identifier).reason")
                }
                ForEach(rows) { row in
                    Line(row: row, showsReason: shared == nil, identifier: "\(identifier).\(row.id)")
                }
            }
        }
    }

    /// One line: its name, its value, and why it is unavailable.
    struct Line: View {
        let row: RadioAtAGlance.Row
        let showsReason: Bool
        let identifier: String

        var body: some View {
            VStack(alignment: .trailing, spacing: 2) {
                HStack(alignment: .firstTextBaseline, spacing: 10) {
                    Text(row.label)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                    Spacer(minLength: 8)
                    Text(row.reading.text)
                        .font(.system(size: 12, weight: .semibold, design: .monospaced))
                        .foregroundStyle(colour)
                        .multilineTextAlignment(.trailing)
                        .fixedSize(horizontal: false, vertical: true)
                        .textSelection(.enabled)
                }
                if showsReason, let reason = row.reading.reason {
                    Text(reason)
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textFaint)
                        .multilineTextAlignment(.trailing)
                        .fixedSize(horizontal: false, vertical: true)
                        .accessibilityIdentifier("\(identifier).reason")
                }
            }
            .accessibilityElement(children: .combine)
            .accessibilityLabel(row.label)
            .accessibilityValue(row.reading.reason.map { "\(RadioAtAGlance.unavailableText). \($0)" }
                ?? row.reading.text)
            .accessibilityIdentifier(identifier)
        }

        private var colour: Color {
            switch row.reading {
            case .unavailable:
                return ChromeColours.textFaint
            case .value(_, .plain):
                return ChromeColours.text
            case .value(_, .good):
                return ChromeColours.linkUpText
            case .value(_, .warning):
                return ChromeColours.refusalText
            }
        }
    }
}
