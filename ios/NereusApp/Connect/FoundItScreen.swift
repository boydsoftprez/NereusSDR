// NereusSDR for iOS: Found it: the Cores on this Wi-Fi that take a new device, one tap for an unclaimed one
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import SwiftUI

/// Found it (spec section 5.3 item 3, picture 06; D71, picture 24): nothing
/// paired yet, and the Cores found on this Wi-Fi that take a new device. An
/// unclaimed Core that allows it pairs with one tap and goes straight on to
/// the band; one taking the code opens the code with its address filled in;
/// an unclaimed Core with pairing closed is greyed and says so. Pair with a
/// code and Enter an address stay below for a Core elsewhere.
struct FoundItScreen: View {
    @ObservedObject var flow: ConnectionFlow

    var body: some View {
        ZStack(alignment: .bottom) {
            VStack(spacing: 0) {
                ConnectChrome.NavBar(title: "Find my Core", back: "Back", onBack: { flow.back() })
                ScrollView {
                    VStack(spacing: 8) {
                        ConnectChrome.Heading(text: "Your Cores")
                            .padding(.top, 10)
                        Text(YourStationsScreen.noneListed)
                            .font(.system(size: 12))
                            .foregroundStyle(ChromeColours.textDim)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding(12)
                            .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
                            .overlay(RoundedRectangle(cornerRadius: 4)
                                .strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
                        ConnectChrome.Heading(text: "On this network")
                            .padding(.top, 10)
                        NearbyList(flow: flow)
                        ConnectChrome.Note(text: flow.nearby.contains { $0.offer == .closed }
                                           && !flow.nearby.contains { $0.offer == .oneTap }
                                           ? ConnectionFlow.closedNote : ConnectionFlow.newCoreNote)
                            .padding(.top, 4)
                        FindCoreWaysOn(flow: flow)
                            .padding(.top, 12)
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

/// Pair with a code and Enter an address, and where a code works from.
struct FindCoreWaysOn: View {
    @ObservedObject var flow: ConnectionFlow

    var body: some View {
        VStack(spacing: 8) {
            ConnectChrome.WideButton(title: "Pair with a code") { flow.pairWithCode() }
                .accessibilityIdentifier("pairWithCode")
            ConnectChrome.WideButton(title: "Enter an address") { flow.enterAddress() }
                .accessibilityIdentifier("enterAddress")
            ConnectChrome.Note(text: ConnectionFlow.elsewhereNote)
                .padding(.top, 12)
        }
    }
}

/// The found Cores under On this network, and a refused one tap's words.
struct NearbyList: View {
    @ObservedObject var flow: ConnectionFlow

    var body: some View {
        VStack(spacing: 8) {
            ForEach(flow.nearby) { row in
                NearbyCoreRow(flow: flow, row: row)
                if let problem = flow.nearbyProblem, problem.id == row.id {
                    NearbyProblemBox(flow: flow, problem: problem)
                }
            }
        }
    }
}

/// A one tap that didn't pair: the Core's words as sent, and the code instead.
struct NearbyProblemBox: View {
    @ObservedObject var flow: ConnectionFlow
    let problem: ConnectionFlow.NearbyProblem

    var body: some View {
        let parts = YourStationsScreen.firstSentence(problem.words)
        if problem.offersCode {
            ConnectChrome.NoticeBox(title: parts.first, text: parts.rest, button: "Use code") {
                flow.useCodeAfterRefusal()
            }
            .accessibilityIdentifier("nearbyProblem")
        } else {
            ConnectChrome.NoticeBox(title: parts.first, text: parts.rest)
                .accessibilityIdentifier("nearbyProblem")
        }
    }
}

/// One found Core: its name, its address, and Pair, Use code or pairing closed.
struct NearbyCoreRow: View {
    @ObservedObject var flow: ConnectionFlow
    let row: ConnectionFlow.NearbyRow

    var body: some View {
        HStack(spacing: 12) {
            ConnectChrome.Pill(colour: row.offer == .closed ? ConnectChrome.pillOff : ConnectChrome.pillOn)
            VStack(alignment: .leading, spacing: 3) {
                Text(row.label)
                    .font(.system(size: 15, weight: .bold, design: .monospaced))
                    .foregroundStyle(row.offer == .closed ? ChromeColours.textDim : ChromeColours.text)
                    .lineLimit(1)
                    .truncationMode(.tail)
                Text(row.offer == .closed ? "\(row.address) \u{00B7} pairing closed" : row.address)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
            Spacer(minLength: 0)
            button
        }
        .padding(.vertical, 10)
        .padding(.leading, 12)
        .padding(.trailing, 10)
        .frame(minHeight: 66)
        .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
        .opacity(row.offer == .closed ? 0.75 : 1)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("nearby-\(row.label)")
    }

    @ViewBuilder
    private var button: some View {
        switch row.offer {
        case .oneTap:
            if flow.claiming == row.id {
                ProgressView().tint(ChromeColours.text).frame(minWidth: 88, minHeight: 40)
            } else {
                ConnectChrome.RowButton(title: "Pair", go: true) {
                    Task { await flow.pairNearby(row) }
                }
                .disabled(flow.claiming != nil || flow.connectingTo != nil || flow.keyUnreadable)
                .accessibilityIdentifier("claim-\(row.label)")
            }
        case .code:
            ConnectChrome.RowButton(title: "Use code") { flow.useCode(row) }
                .disabled(flow.claiming != nil || flow.keyUnreadable)
                .accessibilityIdentifier("useCode-\(row.label)")
        case .closed:
            EmptyView()
        }
    }
}
