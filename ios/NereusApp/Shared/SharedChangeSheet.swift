// NereusSDR for iOS: a change another device hears: the setting from and to, and whose slices it reaches
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// Shared settings (D53, spec section 5.8 item 11, picture 22): the Core's
/// `confirm.request` of kind `sharedSetting` shows the change in the Core's
/// words (the attenuator on ADC 1 from 0 dB to 20 dB) and each device and
/// slice it reaches, with what happens to each. Confirm sends
/// `confirm.proceed` and the control shows the Core's readback; Cancel
/// sends `confirm.cancel` and nothing changes.
struct SharedChangeSheet: View {
    @ObservedObject var devices: SeveralDevicesClient
    let question: SeveralDevices.Question
    let modeLabel: (Int) -> String
    var kindOf: (String) -> String = { _ in "" }

    var body: some View {
        ConfirmationSheetChrome.Sheet(kicker: Self.kicker(question)) {
            if let change = question.change {
                ConfirmationSheetChrome.What(label: change.label, from: change.from, to: change.to)
            }
            ForEach(Array(question.affected.enumerated()), id: \.offset) { _, device in
                ConfirmationSheetChrome.Who(kind: kindOf(device.deviceId), name: device.deviceName,
                                           detail: Self.detail(device, modeLabel))
            }
            ConfirmationSheetChrome.Note(text: Self.note(question))
            if let change = question.change {
                ConfirmationSheetChrome.Answers(devices: devices, go: "Set \(change.to)",
                                                busy: "Setting \(change.to)\u{2026}")
            } else {
                ConfirmationSheetChrome.Answers(devices: devices, go: "Make the change", busy: "Changing\u{2026}")
            }
        }
        .accessibilityIdentifier("sharedChangeSheet")
    }

    /// "This changes what the MacBook hears".
    static func kicker(_ question: SeveralDevices.Question) -> String {
        guard question.affected.count == 1, let device = question.affected.first else {
            return "This changes what other devices hear"
        }
        return "This changes what \(SeveralDevicesWords.the(MoveSharedReceiverSheet.short(device))) hears"
    }

    /// "The MacBook is told you changed it."
    static func note(_ question: SeveralDevices.Question) -> String {
        guard question.affected.count == 1, let device = question.affected.first else {
            return "Each device is told you changed it."
        }
        return "\(SeveralDevicesWords.the(MoveSharedReceiverSheet.short(device), capitalised: true)) is told you changed it."
    }

    /// Each of the device's slices it reaches, through which ADC, and what
    /// happens to it when it does more than change.
    static func detail(_ device: SeveralDevices.AffectedDevice, _ modeLabel: (Int) -> String) -> String {
        guard !device.slices.isEmpty else {
            return device.holdsTransmit ? "Has transmit" : SeveralDevicesWords.state(device.state).capitalisedFirst
        }
        return device.slices.map { slice in
            var line = SeveralDevicesWords.slice(slice.letter, hz: slice.frequencyHz, mode: modeLabel(slice.mode))
                + ", through ADC \(slice.adc + 1)"
            switch slice.effect {
            case .moves:
                line += ", moves to another receiver"
            case .closes:
                line += ", closes"
            case .pausesWhileTransmitting:
                line += ", pauses while the radio transmits"
            case .changes, .other:
                break
            }
            return line
        }.joined(separator: "\n")
    }
}
