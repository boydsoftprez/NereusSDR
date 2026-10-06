// NereusSDR for iOS: Pair with a code: which Core, the code, where the code is, and this phone's name for the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Pair with a code, as picture 24 draws it (spec section 5.3 item 7,
/// D65): the Core it pairs with (the typed address and port, or the
/// listed Core's name), the code, where the code is, and this phone's
/// name on the Core. A wrong code shows the Core's words and its wait
/// before the new code; the fifth wrong code in a row says pairing closed
/// and has to be opened again at the Core. A code with a word not in the
/// list is caught before anything is sent, with the words it may be.
/// From Your Cores with no Core chosen, the code alone pairs from anywhere,
/// through the remote access service (picture 07); an address is given
/// with Enter an address, which comes here with it filled in.
struct PairByCodeScreen: View {
    @ObservedObject var flow: ConnectionFlow
    @FocusState private var codeFocused: Bool

    static let codeFromAnywhere =
        "Type the code your Core shows, or the one under Add a device on a phone or computer that's already paired."

    static let whereTheCodeIs =
        "A number and two words. The code is on the Core's status page, or run nereusd pairing show on the computer the Core runs on."
    static let codeWorksOnce = "A code works once. A wrong guess uses it up, and the Core shows a new one."
    static let closedText =
        "That was the fifth wrong code in a row, so the Core closed pairing. To pair, open it again at the Core: run sudo nereusd pairing open on its computer, or use Add a device on a phone or computer already paired with it."
    static let closedNote =
        "A Core that is already closed answers the same way when a pairing starts: \u{201C}This Core is not taking new devices. Open pairing on the Core or on a paired device first.\u{201D}"

    var body: some View {
        ZStack(alignment: .bottom) {
            VStack(spacing: 0) {
                ConnectChrome.NavBar(title: "Pair with a code", back: backTitle, onBack: { flow.back() })
                ScrollView {
                    VStack(alignment: .leading, spacing: 8) {
                        target
                            .padding(.top, 14)
                        if let problem = flow.pairingProblem {
                            problemBox(problem)
                        }
                        if flow.pairingProblem == .pairingClosed {
                            closedActions
                        } else {
                            form
                        }
                    }
                    .padding(.horizontal, 14)
                    .padding(.bottom, 20)
                }
                .scrollDismissesKeyboard(.interactively)
            }
            if let trouble = flow.trouble {
                TroubleSheet(flow: flow, trouble: trouble)
                    .transition(.move(edge: .bottom))
            }
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .animation(.easeOut(duration: 0.2), value: flow.trouble)
    }

    private var backTitle: String {
        flow.pairTarget?.back == .typeAddress ? "Address" : "Cores"
    }

    /// Which Core this pairs with, or, with none chosen, where the code comes from.
    @ViewBuilder
    private var target: some View {
        if let target = flow.pairTarget, let endpoint = target.endpoint {
            VStack(alignment: .leading, spacing: 2) {
                Text("PAIRING WITH")
                    .font(.system(size: 11, weight: .bold))
                    .kerning(1.1)
                    .foregroundStyle(ConnectChrome.heading)
                if let label = target.label, label != endpoint.host {
                    Text(label)
                        .font(.system(size: 14, weight: .bold, design: .monospaced))
                        .foregroundStyle(ChromeColours.textBright)
                    Text(endpoint.host)
                        .font(.system(size: 12, design: .monospaced))
                        .foregroundStyle(ChromeColours.textDim)
                } else {
                    Text(endpoint.host)
                        .font(.system(size: 14, weight: .bold, design: .monospaced))
                        .foregroundStyle(ChromeColours.textBright)
                }
                Text("Port \(String(endpoint.port))")
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
            }
            .fixedSize(horizontal: false, vertical: true)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
            .accessibilityElement(children: .combine)
            .accessibilityIdentifier("pairingWith")
        } else {
            VStack(alignment: .leading, spacing: 8) {
                if let label = flow.pairTarget?.label {
                    // A listed Core this phone keeps no address for.
                    VStack(alignment: .leading, spacing: 2) {
                        Text("PAIRING WITH")
                            .font(.system(size: 11, weight: .bold))
                            .kerning(1.1)
                            .foregroundStyle(ConnectChrome.heading)
                        Text(label)
                            .font(.system(size: 14, weight: .bold, design: .monospaced))
                            .foregroundStyle(ChromeColours.textBright)
                        Text(ConnectionFlow.fromAnywhereText)
                            .font(.system(size: 12))
                            .foregroundStyle(ChromeColours.textDim)
                    }
                    .fixedSize(horizontal: false, vertical: true)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 10)
                    .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
                    .accessibilityElement(children: .combine)
                    .accessibilityIdentifier("pairingWith")
                }
                Text(Self.codeFromAnywhere)
                    .font(.system(size: 13))
                    .foregroundStyle(ChromeColours.text)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("codeFromAnywhere")
            }
        }
    }

    private var form: some View {
        VStack(alignment: .leading, spacing: 6) {
            ConnectChrome.Heading(text: "Pairing code")
                .padding(.top, 8)
            TextField("", text: $flow.codeText)
                .font(.system(size: 24, weight: .bold, design: .monospaced))
                .foregroundStyle(wrong ? ConnectChrome.badText : ConnectChrome.codeText)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.asciiCapable)
                .focused($codeFocused)
                .padding(.horizontal, 14)
                .frame(height: 58)
                .background(ConnectChrome.fieldFill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(wrong ? ConnectChrome.badBorder : ChromeColours.accent, lineWidth: 1))
                .accessibilityLabel("Pairing code")
                .accessibilityIdentifier("codeField")
            if case .notACode(let word?, let suggestions) = flow.pairingProblem, !suggestions.isEmpty {
                suggestionChips(word: word, suggestions: suggestions)
            }
            ConnectChrome.Note(text: Self.whereTheCodeIs)
            ConnectChrome.Heading(text: "This phone's name on the Core")
                .padding(.top, 12)
            TextField("", text: $flow.nameText)
                .font(.system(size: 17))
                .foregroundStyle(ChromeColours.textBright)
                .textInputAutocapitalization(.words)
                .autocorrectionDisabled()
                .padding(.horizontal, 14)
                .frame(height: 46)
                .background(ConnectChrome.fieldFill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ConnectChrome.nameBorder, lineWidth: 1))
                .accessibilityLabel("This phone's name on the Core")
                .accessibilityIdentifier("nameField")
            ConnectChrome.WideButton(title: "Pair", look: .go,
                                     enabled: !flow.pairHeld && !flow.codeText.isEmpty, busy: flow.pairing) {
                codeFocused = false
                Task { await flow.pair() }
            }
            .padding(.top, 14)
            .accessibilityIdentifier("pairButton")
            ConnectChrome.Note(text: Self.codeWorksOnce)
                .padding(.top, 4)
        }
    }

    private var wrong: Bool {
        if case .wrongCode = flow.pairingProblem {
            return true
        }
        return false
    }

    private func suggestionChips(word: String, suggestions: [String]) -> some View {
        HStack(spacing: 6) {
            ForEach(suggestions, id: \.self) { suggestion in
                Button(suggestion) { flow.useSuggestion(suggestion, for: word) }
                    .font(.system(size: 13, weight: .semibold, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .padding(.horizontal, 10)
                    .frame(height: 32)
                    .background(ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
                    .accessibilityIdentifier("suggestion-\(suggestion)")
            }
        }
    }

    @ViewBuilder
    private func problemBox(_ problem: ConnectionFlow.PairingProblem) -> some View {
        Group {
            switch problem {
            case .notACode(let word, _):
                if let word {
                    ConnectChrome.NoticeBox(tone: .bad, title: nil,
                                            text: "\u{201C}\(word)\u{201D} isn't one of the Core's words.")
                } else {
                    ConnectChrome.NoticeBox(tone: .bad, title: nil, text: ConnectionFlow.codeShapeText)
                }
            case .address(let text), .name(let text), .failed(let text):
                ConnectChrome.NoticeBox(tone: .bad, title: nil, text: text)
            case .wrongCode(let reason, let seconds):
                ConnectChrome.NoticeBox(tone: .bad, title: reason,
                                        text: "Its new code appears in \(seconds) s. Type that one.")
            case .refused(let reason, let seconds):
                ConnectChrome.NoticeBox(tone: .bad, title: reason,
                                        text: seconds > 0 ? "Try again in \(seconds) s." : nil)
            case .pairingClosed:
                ConnectChrome.NoticeBox(title: "Pairing is closed on this Core.", text: Self.closedText)
            }
        }
        .accessibilityIdentifier("pairingProblem")
    }

    private var closedActions: some View {
        VStack(spacing: 8) {
            ConnectChrome.WideButton(title: "Try again", look: .go) { flow.clearPairingProblem() }
                .accessibilityIdentifier("pairTryAgain")
            ConnectChrome.WideButton(title: "Back to Cores") { flow.show(.cores) }
                .accessibilityIdentifier("backToCores")
            ConnectChrome.Note(text: Self.closedNote)
                .padding(.top, 4)
        }
        .padding(.top, 6)
    }
}
