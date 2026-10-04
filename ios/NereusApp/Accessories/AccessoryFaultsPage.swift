// NereusSDR for iOS: an accessory's fault history as the Core records it, newest first, and Clear all
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// A device's fault history (spec section 5.4 item 2's "its fault
/// history"): the Core keeps each device's last ten faults, newest first,
/// and says what happened in its own words (document, The fault record).
/// Clear all asks first, then asks the Core to empty the history
/// (`clearAccessoryFaults`); the list follows the Core's record.
struct AccessoryFaultsPage: View {
    @ObservedObject var model: AccessoriesModel
    let device: AccessoriesModel.Device
    var now: () -> Date = Date.init
    @State private var confirming = false

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if let note = model.notes[device] {
                AccessoryChrome.Refusal(text: note) { model.dismissNote(device) }
            }
            if let records = model.records {
                let faults = records.faults[device] ?? []
                if faults.isEmpty {
                    AccessoryChrome.Card {
                        AccessoryChrome.Note(text: "No faults recorded.")
                    }
                } else {
                    ForEach(faults) { fault in
                        AccessoryChrome.Card {
                            Text(fault.text)
                                .font(.system(size: 13))
                                .foregroundStyle(ChromeColours.text)
                                .fixedSize(horizontal: false, vertical: true)
                            Text(Self.when(fault.whenMs, now: now()))
                                .font(.system(size: 11))
                                .foregroundStyle(ChromeColours.textDim)
                            if !fault.detail.isEmpty {
                                AccessoryChrome.Note(text: fault.detail)
                            }
                        }
                        .accessibilityElement(children: .combine)
                    }
                    Button {
                        confirming = true
                    } label: {
                        Text("Clear all")
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(ChromeColours.revokeText)
                            .frame(maxWidth: .infinity, minHeight: 40)
                            .background(ChromeColours.revoke, in: RoundedRectangle(cornerRadius: 4))
                            .overlay(RoundedRectangle(cornerRadius: 4)
                                .strokeBorder(ChromeColours.revokeBorder, lineWidth: 1))
                    }
                    .buttonStyle(.plain)
                    .accessibilityIdentifier("faultsClear")
                    .confirmationDialog("Clear the fault history?", isPresented: $confirming,
                                        titleVisibility: .visible) {
                        Button("Clear all", role: .destructive) {
                            model.clearFaults(device)
                        }
                    } message: {
                        Text("The Core forgets these faults, for every phone and computer connected to it.")
                    }
                }
            } else {
                AccessoryChrome.Card {
                    AccessoryChrome.Note(text: AccessoriesModel.noRecordsText)
                }
            }
        }
        .accessibilityIdentifier("faultsPage")
    }

    /// The Fault history row's line: how many, and when the last was.
    static func summary(_ faults: [AccessoriesModel.Fault]?, now: Date) -> String {
        guard let faults else {
            return "Kept by the Core"
        }
        guard let last = faults.first else {
            return "No faults recorded"
        }
        let count = faults.count == 1 ? "1 entry" : "\(faults.count) entries"
        return "\(count) \u{00B7} last \(ago(last.whenMs, now: now))"
    }

    /// "3 days ago".
    static func ago(_ whenMs: Int64, now: Date) -> String {
        let formatter = RelativeDateTimeFormatter()
        formatter.unitsStyle = .full
        formatter.locale = Locale(identifier: "en_US_POSIX")
        return formatter.localizedString(for: Date(timeIntervalSince1970: Double(whenMs) / 1000), relativeTo: now)
    }

    /// "3 days ago · 23 Sep 2026, 14:02".
    static func when(_ whenMs: Int64, now: Date) -> String {
        let date = Date(timeIntervalSince1970: Double(whenMs) / 1000)
        let stamp = date.formatted(.dateTime.day().month(.abbreviated).year().hour().minute())
        return "\(ago(whenMs, now: now)) \u{00B7} \(stamp)"
    }
}
