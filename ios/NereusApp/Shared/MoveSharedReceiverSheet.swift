// NereusSDR for iOS: moving a receiver another device shares: who shares it, and what happens to its slice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// Moving a shared receiver (D50, spec section 5.8 item 10, picture 22):
/// the Core's `confirm.request` of kind `panMove` names each device on the
/// receiver and what happens to each of its slices, `moves` to a free
/// receiver or `closes`. With the Core's bands in `change` the buttons read
/// "Go to 20 m" and "Stay on 40 m". Confirm sends `confirm.proceed`, Cancel
/// `confirm.cancel`; the band moves only when the Core says so.
struct MoveSharedReceiverSheet: View {
    @ObservedObject var devices: SeveralDevicesClient
    let question: SeveralDevices.Question
    let modeLabel: (Int) -> String
    /// A device's kind (`phone`, `computer`, ...) by its id, from who is on the Core.
    var kindOf: (String) -> String = { _ in "" }

    var body: some View {
        ConfirmationSheetChrome.Sheet(kicker: Self.kicker(question)) {
            ForEach(Array(question.affected.enumerated()), id: \.offset) { _, device in
                ConfirmationSheetChrome.Who(kind: kindOf(device.deviceId), name: device.deviceName, detail: Self.detail(device, modeLabel))
            }
            ConfirmationSheetChrome.Note(text: Self.note(question))
            if let change = question.change {
                ConfirmationSheetChrome.Answers(devices: devices, go: "Go to \(change.to)",
                                                busy: "Moving to \(change.to)\u{2026}", cancel: "Stay on \(change.from)")
            } else {
                ConfirmationSheetChrome.Answers(devices: devices, go: "Move the receiver",
                                                busy: "Moving the receiver\u{2026}")
            }
        }
        .accessibilityIdentifier("moveSharedReceiverSheet")
    }

    /// "The MacBook shares this receiver", "Grant\u{2019}s iPhone shares this receiver".
    static func kicker(_ question: SeveralDevices.Question) -> String {
        guard question.affected.count == 1, let device = question.affected.first else {
            return "Other devices share this receiver"
        }
        return "\(SeveralDevicesWords.the(short(device), capitalised: true)) shares this receiver"
    }

    /// What going there does, and what happens to each device's slices.
    static func note(_ question: SeveralDevices.Question) -> String {
        var sentences: [String] = []
        if let change = question.change {
            sentences.append("Going to \(change.to) moves the receiver off \(change.from).")
        } else {
            sentences.append("Moving this panadapter moves the receiver.")
        }
        for device in question.affected {
            let short = Self.short(device)
            let closing = device.slices.filter { $0.effect == .closes }.map(\.letter)
            let moving = device.slices.filter { $0.effect == .moves }.map(\.letter)
            let told = "and \(SeveralDevicesWords.the(short)) is told who moved it."
            if !closing.isEmpty {
                sentences.append("No other receiver is free, so \(SeveralDevicesWords.theDevices(short)) "
                                 + "\(Self.sliceWord(closing)) \(SeveralDevicesWords.letters(closing)) "
                                 + "\(closing.count == 1 ? "closes" : "close"), \(told)")
            }
            if !moving.isEmpty {
                sentences.append("\(SeveralDevicesWords.theDevices(short, capitalised: true)) \(Self.sliceWord(moving)) "
                                 + "\(SeveralDevicesWords.letters(moving)) "
                                 + "\(moving.count == 1 ? "moves" : "move") to a free receiver, \(told)")
            }
        }
        return sentences.joined(separator: " ")
    }

    static func detail(_ device: SeveralDevices.AffectedDevice, _ modeLabel: (Int) -> String) -> String {
        let state = SeveralDevicesWords.state(device.state)
        guard !device.slices.isEmpty else {
            return device.holdsTransmit ? "Has transmit" : state.capitalisedFirst
        }
        return device.slices.map { slice in
            SeveralDevicesWords.slice(slice.letter, hz: slice.frequencyHz, mode: modeLabel(slice.mode)) + " \u{00B7} " + state
        }.joined(separator: "\n")
    }

    static func short(_ device: SeveralDevices.AffectedDevice) -> String {
        SeveralDevicesWords.shortName(device.deviceShortName, device.deviceName)
    }

    static func sliceWord(_ letters: [String]) -> String {
        letters.count == 1 ? "slice" : "slices"
    }
}

extension String {
    /// The text with its first letter upper case.
    var capitalisedFirst: String {
        guard let first else {
            return self
        }
        return first.uppercased() + dropFirst()
    }
}
