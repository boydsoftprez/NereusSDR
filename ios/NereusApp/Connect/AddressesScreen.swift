// NereusSDR for iOS: Addresses: the addresses this phone reaches one paired Core at, with Remove and Add an address, and those learned from the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink
import SwiftUI

/// Addresses, from a Core row's "⋯" (D78): the addresses this phone keeps
/// for one paired Core, up to four, tried in this order when it connects.
/// The one the last session reached the Core at is marked. Each has
/// Remove, greyed with its reason on the Core's only address, and Add an
/// address opens Enter an address for this Core only: the phone checks
/// that the Core answering there is this one before it keeps the address.
struct AddressesScreen: View {
    @ObservedObject var flow: ConnectionFlow

    static let explanation =
        "This phone tries these in order when it connects, starting with the one that worked last. Add one to reach this Core another way, such as its IPv4 address or a VPN address."

    var body: some View {
        VStack(spacing: 0) {
            ConnectChrome.NavBar(title: "Addresses", back: "Cores", onBack: { flow.back() })
            ScrollView {
                VStack(alignment: .leading, spacing: 8) {
                    if let station = flow.addressesStation {
                        Text(station.label.isEmpty ? ConnectionFlow.CoreRow(station: station, needsPairing: false).address
                                                   : station.label)
                            .font(.system(size: 15, weight: .bold, design: .monospaced))
                            .foregroundStyle(ChromeColours.textBright)
                            .padding(.top, 14)
                            .accessibilityIdentifier("addressesCore")
                        Text(Self.explanation)
                            .font(.system(size: 13))
                            .foregroundStyle(ChromeColours.text)
                            .fixedSize(horizontal: false, vertical: true)
                        ConnectChrome.Heading(text: "Addresses")
                            .padding(.top, 10)
                        ForEach(station.dialOrder, id: \.self) { endpoint in
                            row(endpoint, station: station)
                        }
                        let learned = ConnectionFlow.learnedAddresses(of: station)
                        if station.endpoints.isEmpty && !learned.isEmpty {
                            ConnectChrome.Note(text: ConnectionFlow.noneAddedText)
                                .accessibilityIdentifier("noneAdded")
                        } else if station.endpoints.isEmpty {
                            // Paired through the remote access service, with no address.
                            ConnectChrome.Note(text: ConnectionFlow.noAddressesText)
                                .accessibilityIdentifier("noAddresses")
                        } else if !flow.canRemoveAddress {
                            ConnectChrome.Note(text: ConnectionFlow.keepOneAddressText)
                                .accessibilityIdentifier("keepOneAddress")
                        }
                        ConnectChrome.WideButton(title: "Add an address",
                                                 enabled: station.endpoints.count < PairedStation.maxEndpoints) {
                            flow.addAddress()
                        }
                        .padding(.top, 14)
                        .accessibilityIdentifier("addAnAddress")
                        if station.endpoints.count >= PairedStation.maxEndpoints {
                            ConnectChrome.Note(text: ConnectionFlow.fourAddressesText)
                                .accessibilityIdentifier("fourAddresses")
                        }
                        if !learned.isEmpty {
                            ConnectChrome.Heading(text: ConnectionFlow.learnedHeadingText)
                                .padding(.top, 18)
                                .accessibilityIdentifier("learnedFromTheCore")
                            ConnectChrome.Note(text: ConnectionFlow.learnedNoteText)
                            ForEach(learned, id: \.self) { endpoint in
                                learnedRow(endpoint, station: station)
                            }
                        }
                    }
                }
                .padding(.horizontal, 14)
                .padding(.bottom, 20)
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }

    private func row(_ endpoint: StationEndpoint, station: PairedStation) -> some View {
        let lastGood = station.isLastGood(endpoint)
        return HStack(spacing: 12) {
            ConnectChrome.Pill(colour: lastGood ? ConnectChrome.pillOn : ConnectChrome.pillUnknown)
            VStack(alignment: .leading, spacing: 3) {
                Text(ConnectionFlow.addressText(endpoint))
                    .font(.system(size: 14, weight: .bold, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .lineLimit(2)
                    .truncationMode(.middle)
                    .fixedSize(horizontal: false, vertical: true)
                if lastGood {
                    Text(ConnectionFlow.lastWorkedText)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                }
            }
            Spacer(minLength: 0)
            ConnectChrome.RowButton(title: "Remove") {
                flow.removeAddress(endpoint)
            }
            .disabled(!flow.canRemoveAddress)
            .opacity(flow.canRemoveAddress ? 1 : 0.5)
            .accessibilityHint(flow.canRemoveAddress ? "" : ConnectionFlow.keepOneAddressText)
            .accessibilityIdentifier("removeAddress-\(ConnectionFlow.addressText(endpoint))")
        }
        .padding(.vertical, 10)
        .padding(.leading, 12)
        .padding(.trailing, 8)
        .frame(minHeight: 60)
        .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("address-\(ConnectionFlow.addressText(endpoint))")
    }

    /// A row the Core taught this phone: the same row as a typed address,
    /// read-only, since the phone keeps the list up to date itself.
    private func learnedRow(_ endpoint: StationEndpoint, station: PairedStation) -> some View {
        let lastGood = station.isLastGood(endpoint)
        return HStack(spacing: 12) {
            ConnectChrome.Pill(colour: lastGood ? ConnectChrome.pillOn : ConnectChrome.pillUnknown)
            VStack(alignment: .leading, spacing: 3) {
                Text(ConnectionFlow.addressText(endpoint))
                    .font(.system(size: 14, weight: .bold, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .lineLimit(2)
                    .truncationMode(.middle)
                    .fixedSize(horizontal: false, vertical: true)
                if lastGood {
                    Text(ConnectionFlow.lastWorkedText)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                }
            }
            Spacer(minLength: 0)
        }
        .padding(.vertical, 10)
        .padding(.leading, 12)
        .padding(.trailing, 8)
        .frame(maxWidth: .infinity, minHeight: 60, alignment: .leading)
        .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("learned-\(ConnectionFlow.addressText(endpoint))")
    }
}
