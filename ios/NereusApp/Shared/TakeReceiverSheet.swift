// NereusSDR for iOS: every receiver in use: pick one to take from another device, as the Core lists them
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// Every receiver in use (D49, spec section 5.8 item 8, picture 22): the
/// Core's `confirm.request` of kind `takeReceiver` (or `takeSlice`, when
/// the radio's slices run out before its receivers) lists each receiver
/// with who has it, what they are doing and when they were last active.
/// The choice starts on the first one that can be taken; one that can't
/// shows the Core's reason and can't be picked. Confirm sends
/// `confirm.proceed` with the picked receiver's choice; the button names
/// the device it is taken from.
struct TakeReceiverSheet: View {
    @ObservedObject var devices: SeveralDevicesClient
    let question: SeveralDevices.Question
    /// The catalogue's label for a mode, `USB`.
    let modeLabel: (Int) -> String

    @State private var picked: Int64?

    var body: some View {
        let chosen = question.choices.first { $0.choice == (picked ?? question.firstTakeable?.choice) }
        ConfirmationSheetChrome.Sheet(kicker: kicker) {
            ConfirmationSheetChrome.Note(text: lead, lead: true)
            VStack(spacing: 6) {
                ForEach(question.choices) { choice in
                    ConfirmationSheetChrome.Pick(title: Self.title(choice), detail: detail(choice),
                                                 picked: choice.choice == chosen?.choice, enabled: choice.takeable) {
                        picked = choice.choice
                    }
                    .accessibilityIdentifier("receiverChoice-\(choice.choice)")
                }
            }
            ConfirmationSheetChrome.Note(text: note)
            ConfirmationSheetChrome.Answers(devices: devices, go: go(chosen), busy: busy,
                                            enabled: chosen?.takeable == true,
                                            choice: chosen?.choice ?? SeveralDevices.noChoice)
        }
        .accessibilityIdentifier("takeReceiverSheet")
    }

    private var slices: Bool { question.kind == .takeSlice }

    private var kicker: String {
        slices ? "Every slice is in use" : "Every receiver is in use"
    }

    private var lead: String {
        slices ? "To open a slice, take one from another device."
            : "To listen there, take a receiver from another device."
    }

    private var note: String {
        slices ? "That slice closes on its device, which is told who took it and can take it back."
            : "The slice on it closes on that device, which is told who took the receiver and can take it back."
    }

    private var busy: String {
        slices ? "Taking the slice\u{2026}" : "Taking the receiver\u{2026}"
    }

    private func go(_ choice: SeveralDevices.Choice?) -> String {
        Self.go(choice, slices: slices)
    }

    /// "Take the iPad\u{2019}s receiver", "Take Grant\u{2019}s iPhone\u{2019}s
    /// receiver"; a free receiver is just used.
    static func go(_ choice: SeveralDevices.Choice?, slices: Bool) -> String {
        guard let device = choice?.devices.first else {
            return "Use this receiver"
        }
        let name = SeveralDevicesWords.theDevices(SeveralDevicesWords.shortName(device.shortName, device.name))
        return slices ? "Take \(name) slice" : "Take \(name) receiver"
    }

    /// "Receiver 2 \u{00B7} iPad Pro": the receiver, shown from 1, and who has it.
    static func title(_ choice: SeveralDevices.Choice) -> String {
        let receiver = choice.streamIndex >= 0 ? "Receiver \(choice.streamIndex + 1)" : "Receiver"
        let names = choice.devices.map(\.name).filter { !$0.isEmpty }
        return names.isEmpty ? receiver : receiver + " \u{00B7} " + names.joined(separator: ", ")
    }

    /// Each slice on it with its device's state and last activity, and the
    /// Core's reason when it can't be taken.
    private func detail(_ choice: SeveralDevices.Choice) -> String {
        var lines: [String] = choice.slices.map { slice in
            var parts = [SeveralDevicesWords.slice(slice.letter, hz: slice.frequencyHz, mode: modeLabel(slice.mode))]
            if let device = choice.devices.first(where: { $0.deviceId == slice.deviceId }) ?? choice.devices.first {
                parts.append(SeveralDevicesWords.state(device.state))
                parts.append("last active " + SeveralDevicesWords.ago(seconds: device.lastActivitySeconds))
            }
            return parts.joined(separator: " \u{00B7} ")
        }
        if lines.isEmpty {
            lines.append("Free")
        }
        if !choice.takeable, !choice.why.isEmpty {
            lines.append(choice.why)
        }
        return lines.joined(separator: "\n")
    }
}
