// NereusSDR for iOS: Enter an address: the Core's address, growing to a second line, and its port in a field of its own
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Enter an address (spec section 5.3 item 7, D69, picture 07): the
/// Core's name, IPv4 address or IPv6 address, with or without brackets,
/// in a field that grows to a second line so a whole IPv6 address stays
/// readable; below it the port, filled in with the Core's standard one.
/// Pasting an address with its port moves the port into the port field.
/// Connect asks which Core answers there: a Core this phone has paired
/// gets the address and signs in, and any other goes on to the code.
/// Opened from a Core's Addresses page, Add keeps the address only when
/// that Core answers there.
struct TypeAddressScreen: View {
    @ObservedObject var flow: ConnectionFlow
    @FocusState private var addressFocused: Bool

    var body: some View {
        VStack(spacing: 0) {
            ConnectChrome.NavBar(title: "Enter an address", back: flow.addingAddressFor == nil ? "Cores" : "Addresses",
                                 onBack: { flow.back() })
            ScrollView {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Type your Core's address: its name, or its IPv4 or IPv6 address. No brackets needed.")
                        .font(.system(size: 13))
                        .foregroundStyle(ChromeColours.text)
                        .fixedSize(horizontal: false, vertical: true)
                        .padding(.top, 14)
                    AddressFields(flow: flow, focused: $addressFocused)
                        .padding(.top, 8)
                    ConnectChrome.Note(text: "\(ConnectionFlow.standardPortText) is the Core's standard port. Change it only if your Core uses another.")
                    if let problem = flow.addressProblem {
                        ConnectChrome.NoticeBox(tone: .bad, title: nil, text: problem)
                            .accessibilityIdentifier("addressProblem")
                    }
                    ConnectChrome.WideButton(title: flow.addingAddressFor == nil ? "Connect" : "Add",
                                             look: .go, busy: flow.connectingTo != nil || flow.identifying) {
                        Task { await flow.connectToTypedAddress() }
                    }
                    .padding(.top, 14)
                    .accessibilityIdentifier("connectAddress")
                    if let core = flow.addingAddressFor {
                        ConnectChrome.Note(text: ConnectionFlow.addingNote(core))
                            .padding(.top, 4)
                    } else {
                        ConnectChrome.Note(text: ConnectionFlow.typedAddressNote)
                            .padding(.top, 4)
                    }
                }
                .padding(.horizontal, 14)
                .padding(.bottom, 20)
            }
            .scrollDismissesKeyboard(.interactively)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .onAppear { addressFocused = flow.addressText.isEmpty }
    }
}

/// The address field and the narrow port field below it, shared by Enter
/// an address and by Pair with a code when no address is known yet.
struct AddressFields: View {
    @ObservedObject var flow: ConnectionFlow
    var focused: FocusState<Bool>.Binding

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            ConnectChrome.Heading(text: "Address")
            TextField("", text: $flow.addressText, axis: .vertical)
                .lineLimit(1...3)
                .font(.system(size: 21, weight: .bold, design: .monospaced))
                .foregroundStyle(ConnectChrome.codeText)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.URL)
                .submitLabel(.next)
                .focused(focused)
                .padding(.horizontal, 14)
                .padding(.vertical, 12)
                .background(ConnectChrome.fieldFill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.accent, lineWidth: 1))
                .onChange(of: flow.addressText) { old, new in
                    flow.addressChanged(from: old, to: new)
                }
                .accessibilityLabel("Address")
                .accessibilityIdentifier("addressField")
            ConnectChrome.Heading(text: "Port")
                .padding(.top, 10)
            TextField("", text: $flow.portText)
                .font(.system(size: 21, weight: .bold, design: .monospaced))
                .foregroundStyle(ConnectChrome.codeText)
                .keyboardType(.numberPad)
                .padding(.horizontal, 14)
                .frame(height: 50)
                .background(ConnectChrome.fieldFill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.accent, lineWidth: 1))
                .containerRelativeFrame(.horizontal) { width, _ in width * 0.44 }
                .accessibilityLabel("Port")
                .accessibilityIdentifier("portField")
        }
    }
}
