// NereusSDR for iOS: this device's place was taken by another device
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The stopped band and the Core's taken-over end, with a deliberate way
/// back to the same fifth-device question.
struct PlaceTakenScreen: View {
    @ObservedObject var flow: ConnectionFlow
    let notice: ConnectionFlow.CoresNotice

    var body: some View {
        VStack(spacing: 12) {
            Text("TAKEN OVER")
                .font(.system(size: 20, weight: .heavy, design: .monospaced))
                .tracking(2)
                .foregroundStyle(ChromeColours.accent)
                .accessibilityAddTraits(.isHeader)
                .accessibilityIdentifier("placeTakenTitle")
            if case .placeTaken(let name, _, let happened, _) = notice {
                Text("\(name) took this phone's place at \(SeveralDevicesWords.timeOfDay(happened)).")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(ChromeColours.textBright)
                Text("This phone's slices and settings are saved on the Core. To get your place back, choose a device from the Core's list.")
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .multilineTextAlignment(.center)
                ConnectChrome.WideButton(title: "Take it back", look: .go) {
                    Task { await flow.takePlaceBack() }
                }
                .accessibilityIdentifier("placeTakenTakeBack")
            }
            ConnectChrome.WideButton(title: "Back to Cores") { flow.dismissPlaceTaken() }
                .accessibilityIdentifier("placeTakenBack")
        }
        .padding(18)
        .frame(maxWidth: 390)
        .background(ConnectChrome.sheetFill, in: RoundedRectangle(cornerRadius: 8))
        .overlay(RoundedRectangle(cornerRadius: 8).strokeBorder(ConnectChrome.sheetEdge, lineWidth: 1))
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .accessibilityIdentifier("placeTakenScreen")
    }
}
