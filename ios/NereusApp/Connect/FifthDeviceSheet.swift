// NereusSDR for iOS: the Core's visible fifth-device choice
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import SwiftUI

/// A full Core asks which place to take. The rows stay in the Core's order;
/// selecting a row only changes this sheet, and the button sends the answer.
struct FifthDeviceSheet: View {
    @ObservedObject var flow: ConnectionFlow
    let question: LinkMessage.SessionHeld

    static func buttonTitle(for device: LinkMessage.SessionHeld.Device) -> String {
        device.state == .transmitting ? "Unkey and take \(device.shortName)'s place"
            : "Take \(device.shortName)'s place"
    }

    private var chosen: LinkMessage.SessionHeld.Device? {
        question.devices.first { $0.deviceId == flow.heldChoiceID && $0.replaceable }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Capsule().fill(ConnectChrome.grab).frame(width: 40, height: 5).frame(maxWidth: .infinity)
            Text(question.placeFreed == nil ? "Four devices are on \(flow.heldCoreName)"
                 : "Your place went to another device")
                .font(.system(size: 19, weight: .heavy))
                .foregroundStyle(ChromeColours.textBright)
                .accessibilityAddTraits(.isHeader)
                .accessibilityIdentifier("fifthDeviceTitle")
            Text(question.placeFreed == nil
                 ? "There is room for four devices. To connect, pick one for this phone to take the place of."
                 : "You were away more than 3 minutes, so your place was freed. To connect, pick one to take the place of.")
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.textDim)
                .fixedSize(horizontal: false, vertical: true)
            ScrollView {
                VStack(spacing: 6) {
                    ForEach(question.devices, id: \.deviceId) { device in
                        row(device)
                    }
                }
            }
            .frame(maxHeight: 330)
            if let chosen {
                Text(chosen.state == .transmitting
                     ? "The Core will unkey this device before taking its place. Its slices and settings stay saved."
                     : "This device will lose its place. Its slices and settings stay saved, and it can take it back the same way.")
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                confirmButton(chosen)
            }
            if let problem = flow.heldProblem {
                Text(problem).font(.system(size: 12)).foregroundStyle(ConnectChrome.badText)
            }
            ConnectChrome.WideButton(title: "Cancel", enabled: !flow.heldBusy) {
                Task { await flow.answerHeld(cancel: true) }
            }
            .accessibilityIdentifier("fifthDeviceCancel")
        }
        .padding(.horizontal, 18)
        .padding(.top, 8)
        .padding(.bottom, 20)
        .frame(maxWidth: .infinity)
        .background(ConnectChrome.sheetFill, in: UnevenRoundedRectangle(topLeadingRadius: 14, topTrailingRadius: 14))
        .overlay(alignment: .top) {
            UnevenRoundedRectangle(topLeadingRadius: 14, topTrailingRadius: 14)
                .stroke(ConnectChrome.sheetEdge, lineWidth: 1)
        }
        .accessibilityIdentifier("fifthDeviceSheet")
    }

    private func row(_ device: LinkMessage.SessionHeld.Device) -> some View {
        let selected = device.deviceId == flow.heldChoiceID
        return Button { flow.selectHeldDevice(device.deviceId) } label: {
            HStack(alignment: .top, spacing: 9) {
                Image(systemName: selected ? "largecircle.fill.circle" : "circle")
                    .foregroundStyle(selected ? ChromeColours.accent : ChromeColours.textDim)
                    .padding(.top, 3)
                VStack(alignment: .leading, spacing: 3) {
                    Text(device.name)
                        .font(.system(size: 13, weight: .bold))
                        .foregroundStyle(device.replaceable ? ChromeColours.textBright : ChromeColours.textDim)
                    Text(Self.status(device))
                        .font(.system(size: 11))
                        .foregroundStyle(device.state == .transmitting ? ConnectChrome.badText : ChromeColours.textDim)
                    Text(Self.details(device))
                        .font(.system(size: 10))
                        .foregroundStyle(ChromeColours.textFaint)
                    if !device.replaceable {
                        Text("This computer runs the Core and cannot be replaced.")
                            .font(.system(size: 10)).foregroundStyle(ConnectChrome.warn)
                    }
                }
                Spacer(minLength: 0)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(9)
            .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4)
                .strokeBorder(selected ? ChromeColours.accent : ConnectChrome.rowBorder, lineWidth: 1))
        }
        .buttonStyle(.plain)
        .disabled(!device.replaceable || flow.heldBusy)
        .accessibilityIdentifier("fifthDeviceRow-\(device.deviceId)")
    }

    private func confirmButton(_ device: LinkMessage.SessionHeld.Device) -> some View {
        let red = device.state == .transmitting
        return Button { Task { await flow.answerHeld() } } label: {
            HStack {
                if flow.heldBusy { ProgressView().tint(.white) }
                Text(Self.buttonTitle(for: device))
            }
            .font(.system(size: 14, weight: .bold))
            .foregroundStyle(.white)
            .frame(maxWidth: .infinity)
            .frame(height: 48)
            .background(red ? ConnectChrome.rgb(0xD8, 0x38, 0x38) : ChromeColours.buttonOnBlue,
                        in: RoundedRectangle(cornerRadius: 4))
        }
        .buttonStyle(.plain)
        .disabled(flow.heldBusy)
        .accessibilityIdentifier("fifthDeviceConfirm")
    }

    static func status(_ device: LinkMessage.SessionHeld.Device) -> String {
        switch device.state {
        case .away: return "Away for \(duration(device.awayForSeconds))"
        case .transmitting:
            let slice = device.transmittingOn.map { " on \($0.letter) at \(frequency($0.frequencyHz))" } ?? ""
            return "Transmitting\(slice) · \(duration(device.transmittingForSeconds))"
        case .listening:
            let slices = device.listeningOn.map { "\($0.letter) at \(frequency($0.frequencyHz))" }.joined(separator: ", ")
            return slices.isEmpty ? "Listening" : "Listening on \(slices)"
        }
    }

    static func details(_ device: LinkMessage.SessionHeld.Device) -> String {
        "Connected \(duration(device.connectedForSeconds)) · last active \(duration(device.lastActivitySeconds)) ago · \(device.from)"
    }

    private static func frequency(_ hz: Double) -> String { String(format: "%.3f MHz", hz / 1_000_000) }

    private static func duration(_ seconds: Int64) -> String {
        if seconds >= 3_600 { return "\(seconds / 3_600) hour\(seconds / 3_600 == 1 ? "" : "s")" }
        if seconds >= 60 { return "\(seconds / 60) minute\(seconds / 60 == 1 ? "" : "s")" }
        return "\(seconds) seconds"
    }
}
