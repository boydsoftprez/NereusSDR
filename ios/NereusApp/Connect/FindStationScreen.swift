// NereusSDR for iOS: Find my Core while nothing is found yet: looking on this Wi-Fi
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Looking on this Wi-Fi (spec section 5.3 item 2, picture 06): the phone
/// looks for Cores on this network, which makes iOS ask its Local Network
/// question the first time, with the app's reason under it. When iOS says
/// no, the screen says where to allow it, and a code and an address still
/// work (item 3). What is found moves the screen on to Found it.
struct FindStationScreen: View {
    @ObservedObject var flow: ConnectionFlow

    var body: some View {
        ZStack(alignment: .bottom) {
            VStack(spacing: 0) {
                ConnectChrome.NavBar(title: "Find my Core", back: "Back", onBack: { flow.back() })
                ScrollView {
                    VStack(spacing: 8) {
                        ConnectChrome.Heading(text: "On this network")
                            .padding(.top, 10)
                        HStack(spacing: 10) {
                            if !flow.lookingDenied {
                                ProgressView().tint(ChromeColours.textDim)
                            }
                            Text(flow.lookingDenied ? ConnectionFlow.lookingDeniedText : ConnectionFlow.lookingText)
                                .font(.system(size: 12))
                                .foregroundStyle(ChromeColours.textDim)
                                .fixedSize(horizontal: false, vertical: true)
                            Spacer(minLength: 0)
                        }
                        .padding(12)
                        .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
                        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
                        .accessibilityElement(children: .combine)
                        .accessibilityIdentifier("looking")
                        FindCoreWaysOn(flow: flow)
                            .padding(.top, 16)
                    }
                    .padding(.horizontal, 14)
                    .padding(.bottom, 20)
                }
            }
            if let trouble = flow.trouble {
                TroubleSheet(flow: flow, trouble: trouble)
                    .transition(.move(edge: .bottom))
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .animation(.easeOut(duration: 0.2), value: flow.trouble)
    }
}
