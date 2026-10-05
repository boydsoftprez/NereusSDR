// NereusSDR for iOS: the welcome: what the app needs, in one picture, and the two ways on
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The first screen with nothing paired (spec section 5.3 item 1, D19,
/// picture 06): one picture of the radio, the Core and the phone, and two
/// ways on, Find my Core and Set up a Core. There is no demo mode.
struct WelcomeScreen: View {
    @ObservedObject var flow: ConnectionFlow
    @State private var showingAbout = false

    var body: some View {
        VStack(spacing: 0) {
            Spacer(minLength: 24)
            Image("WelcomeMark")
                .resizable()
                .frame(width: 68, height: 68)
                .clipShape(RoundedRectangle(cornerRadius: 15))
                .accessibilityHidden(true)
            Text("NereusSDR")
                .font(.system(size: 28, weight: .heavy))
                .foregroundStyle(ChromeColours.textBright)
                .padding(.top, 14)
            Text("Your station, from anywhere.")
                .font(.system(size: 15))
                .foregroundStyle(ChromeColours.accent)
                .padding(.top, 4)
            Text("This app runs your radio through a NereusSDR Core: a computer or a small box connected to the radio at home.")
                .font(.system(size: 14))
                .foregroundStyle(ChromeColours.text)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.top, 26)
                .padding(.horizontal, 28)
            StationPicture()
                .padding(.top, 26)
            Spacer(minLength: 24)
            VStack(spacing: 8) {
                ConnectChrome.WideButton(title: "Find my Core", look: .go) { flow.findMyCore() }
                    .accessibilityIdentifier("findMyCore")
                ConnectChrome.WideButton(title: "Set up a Core") { flow.show(.setUpCore) }
                    .accessibilityIdentifier("setUpCore")
                Button("About this app") { showingAbout = true }
                    .font(.footnote)
                    .foregroundStyle(ChromeColours.accent)
                    .frame(minHeight: 44)
                    .accessibilityIdentifier("welcomeAbout")
            }
            .padding(.horizontal, 20)
            .padding(.bottom, 20)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(
            RadialGradient(colors: [ChromeColours.accent.opacity(0.12), ChromeColours.bar], center: .top,
                           startRadius: 0, endRadius: 360)
                .ignoresSafeArea()
        )
        .sheet(isPresented: $showingAbout) {
            NavigationStack { AboutAppPage(showsDone: true) }
        }
    }
}

/// The radio, the Core and the phone, joined, with their names under them.
private struct StationPicture: View {
    var body: some View {
        HStack(alignment: .bottom, spacing: 0) {
            part("Your radio", width: 48, height: 26) { size in
                var path = Path(roundedRect: CGRect(origin: .zero, size: size), cornerRadius: 3)
                path.addEllipse(in: CGRect(x: size.width - 20, y: 6, width: 14, height: 14))
                path.addRect(CGRect(x: 6, y: 7, width: 18, height: 6))
                return path
            }
            link
            part("The Core", width: 48, height: 22) { size in
                var path = Path(roundedRect: CGRect(origin: .zero, size: size), cornerRadius: 3)
                path.addEllipse(in: CGRect(x: 8, y: size.height / 2 - 2, width: 4, height: 4))
                path.move(to: CGPoint(x: 18, y: size.height / 2))
                path.addLine(to: CGPoint(x: size.width - 8, y: size.height / 2))
                return path
            }
            link
            part("This phone", width: 24, height: 40) { size in
                var path = Path(roundedRect: CGRect(origin: .zero, size: size), cornerRadius: 5)
                path.move(to: CGPoint(x: 5, y: 26))
                path.addLine(to: CGPoint(x: 9, y: 18))
                path.addLine(to: CGPoint(x: 13, y: 24))
                path.addLine(to: CGPoint(x: 19, y: 12))
                return path
            }
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Your radio, connected to the Core, reached by this phone")
    }

    private var link: some View {
        Rectangle()
            .fill(ChromeColours.accent.opacity(0.6))
            .frame(width: 28, height: 1.5)
            .padding(.bottom, 32)
    }

    private func part(_ name: String, width: CGFloat, height: CGFloat,
                      shape: @escaping (CGSize) -> Path) -> some View {
        VStack(spacing: 8) {
            Canvas { context, size in
                context.stroke(shape(size), with: .color(ChromeColours.accent), lineWidth: 1.4)
            }
            .frame(width: width, height: height)
            Text(name)
                .font(.system(size: 10))
                .foregroundStyle(ChromeColours.textDim)
                .fixedSize()
        }
        .frame(width: 70)
    }
}
