// NereusSDR for iOS: Set up a Core: what a Core is, the two ways to run one, and what it does on its own
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Set up a Core (spec section 5.3 item 5, picture 06): what the Core is,
/// on a computer or on a small box, and what it does on its own.
struct SetUpCoreScreen: View {
    @ObservedObject var flow: ConnectionFlow

    var body: some View {
        VStack(spacing: 0) {
            ConnectChrome.NavBar(title: "Set up a Core", back: "Back", onBack: { flow.back() })
            ScrollView {
                VStack(alignment: .leading, spacing: 10) {
                    Text("The Core is the part that talks to the radio. It sits next to it, stays on, and does all the signal processing, so the phone only shows and controls.")
                        .font(.system(size: 13))
                        .foregroundStyle(ChromeColours.text)
                        .fixedSize(horizontal: false, vertical: true)
                    VStack(spacing: 0) {
                        way("On a computer",
                            "Install NereusSDR on the Windows, Mac or Linux computer connected to your radio, and turn on its Core.")
                        Divider().overlay(ChromeColours.button)
                        way("On a small box",
                            "Flash the NereusSDR Core card for a small single-board computer, plug it into your network next to the radio, and power it.")
                    }
                    .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
                    ConnectChrome.Heading(text: "Then").padding(.top, 8)
                    VStack(spacing: 0) {
                        step(1, "It finds the radio by itself", "on the same network, as the desktop does today.")
                        Divider().overlay(ChromeColours.button)
                        step(2, "It waits for its first device", "showing a code, with no time limit.")
                        Divider().overlay(ChromeColours.button)
                        step(3, "Come back and tap Find my Core", "or type its code from anywhere.")
                    }
                    .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
                    ConnectChrome.Note(text: "The full guide is at nereussdr.com")
                        .padding(.top, 4)
                }
                .padding(14)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }

    private func way(_ title: String, _ text: String) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(title).font(.system(size: 13, weight: .bold)).foregroundStyle(ChromeColours.textBright)
            Text(text).font(.system(size: 11)).foregroundStyle(ChromeColours.textDim)
                .fixedSize(horizontal: false, vertical: true)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
    }

    private func step(_ number: Int, _ title: String, _ text: String) -> some View {
        HStack(spacing: 12) {
            Text("\(number)")
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(ChromeColours.accent)
                .frame(width: 22, height: 22)
                .overlay(Circle().strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.system(size: 13, weight: .bold)).foregroundStyle(ChromeColours.textBright)
                Text(text).font(.system(size: 11)).foregroundStyle(ChromeColours.textDim)
            }
            Spacer(minLength: 0)
        }
        .padding(12)
        .accessibilityElement(children: .combine)
    }
}
