// NereusSDR for iOS: paired: the microphone question, asked once right after the first pairing
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Paired (spec section 5.3 item 4, picture 06): the Core this phone just
/// paired with, and the microphone question, asked now so iOS never
/// interrupts the first transmission. Listening works without it. The
/// connection is already being made behind it; either answer, or Go to
/// the band, goes on to the band.
struct MicrophoneQuestionScreen: View {
    @ObservedObject var flow: ConnectionFlow

    var body: some View {
        VStack(spacing: 14) {
            if let paired = flow.pairedWith {
                HStack(spacing: 10) {
                    ConnectChrome.Pill(colour: ConnectChrome.pillOn)
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Paired with \(paired.label)")
                            .font(.system(size: 14, weight: .bold))
                            .foregroundStyle(ChromeColours.textBright)
                        Text(ConnectionFlow.addressText(paired.endpoints.first))
                            .font(.system(size: 12))
                            .foregroundStyle(ChromeColours.textDim)
                    }
                    Spacer(minLength: 0)
                }
                .padding(12)
                .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.stationBorder, lineWidth: 1))
                .accessibilityElement(children: .combine)
                .accessibilityIdentifier("pairedWith")
            }
            VStack(spacing: 12) {
                Image(systemName: "mic")
                    .font(.system(size: 30, weight: .light))
                    .foregroundStyle(ChromeColours.accent)
                    .accessibilityHidden(true)
                Text("To transmit, NereusSDR needs your microphone.")
                    .font(.system(size: 15, weight: .bold))
                    .foregroundStyle(ChromeColours.textBright)
                    .multilineTextAlignment(.center)
                Text("iOS asks once. Asking now means the question never interrupts your first transmission. Without it you can still listen and tune.")
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .multilineTextAlignment(.center)
                    .fixedSize(horizontal: false, vertical: true)
                ConnectChrome.WideButton(title: "Allow the microphone", look: .go) {
                    Task { await flow.answerMicrophone(allow: true) }
                }
                .accessibilityIdentifier("allowMicrophone")
                ConnectChrome.WideButton(title: "Not now") {
                    Task { await flow.answerMicrophone(allow: false) }
                }
                .accessibilityIdentifier("notNow")
            }
            .padding(16)
            .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
            Spacer()
            Button("Go to the band") { flow.goToBand() }
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(ChromeColours.accent)
                .padding(.bottom, 16)
                .accessibilityIdentifier("goToBand")
        }
        .padding(.horizontal, 20)
        .padding(.top, 20)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(ChromeColours.bar.ignoresSafeArea())
    }
}
