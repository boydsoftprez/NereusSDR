// NereusSDR for iOS: your FreeDV Reporter status message: type one or pick a saved one, then Send, Save or Clear
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The operator's status message (spec section 5.7 item 7): the message
/// listed beside your station on FreeDV Reporter. Type one or pick one of
/// the saved messages, then Send it (the Core keeps it for its next
/// connection too), Save it to the list, or Clear the one listed. The saved
/// messages are the Core's, as the desktop keeps them.
struct StatusMessageEditor: View {
    @ObservedObject var freedv: FreeDVReporterModel

    var body: some View {
        let reason = freedv.coreReason
        let text = freedv.statusText
        let draft = Binding(get: { freedv.statusText }, set: { freedv.statusDraft = $0 })
        FreeDVChrome.Sheet(identifier: "freedv.statusSheet") {
            HStack(alignment: .firstTextBaseline) {
                Text("My status")
                    .font(.system(size: 18, weight: .bold))
                    .foregroundStyle(ChromeColours.textBright)
                Spacer()
                Button("Done") { freedv.editingMessage = false }
                    .font(.system(size: 14, weight: .semibold))
                    .foregroundStyle(ChromeColours.accent)
                    .accessibilityIdentifier("freedv.status.done")
            }
            Text("Listed beside your station on FreeDV Reporter.")
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.textDim)
            TextField("Status message", text: draft)
                .font(.system(size: 14))
                .foregroundStyle(ChromeColours.textBright)
                .submitLabel(.send)
                .onSubmit { send(text) }
                .padding(.horizontal, 8)
                .frame(minHeight: 40)
                .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
                .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                .disabled(reason != nil)
                .accessibilityIdentifier("freedv.status.text")
            HStack(spacing: 8) {
                FreeDVChrome.Wide(title: "Send", primary: true, enabled: reason == nil,
                                  identifier: "freedv.status.send") { send(text) }
                FreeDVChrome.Wide(title: "Save", enabled: reason == nil && !text.trimmingCharacters(in: .whitespaces).isEmpty,
                                  identifier: "freedv.status.save") { freedv.saveMessage(text) }
                FreeDVChrome.Wide(title: "Clear", enabled: reason == nil, identifier: "freedv.status.clear") {
                    freedv.statusDraft = ""
                    freedv.sendMessage("")
                }
            }
            SpotHubPage.Heading(text: "Saved messages", tag: .core).padding(.top, 4)
            let saved = freedv.savedMessages
            VStack(spacing: 0) {
                if saved.isEmpty {
                    Text("Save a message to pick it here next time.")
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textFaint)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(10)
                }
                ForEach(Array(saved.enumerated()), id: \.offset) { index, message in
                    if index > 0 {
                        SpotHubPage.Line()
                    }
                    Button {
                        freedv.statusDraft = message
                    } label: {
                        HStack {
                            Text(message)
                                .font(.system(size: 13))
                                .foregroundStyle(message == text ? ChromeColours.accent : ChromeColours.text)
                                .lineLimit(2)
                                .frame(maxWidth: .infinity, alignment: .leading)
                            if message == text {
                                Image(systemName: "checkmark")
                                    .font(.system(size: 12, weight: .bold))
                                    .foregroundStyle(ChromeColours.accent)
                            }
                        }
                        .padding(.horizontal, 10)
                        .frame(minHeight: 40)
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .disabled(reason != nil)
                    .accessibilityIdentifier("freedv.status.saved.\(index)")
                }
            }
            .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.insetBorder, lineWidth: 1))
            if let note = freedv.note {
                AccessoryChrome.Refusal(text: note) { freedv.note = nil }
            } else if let reason {
                AccessoryChrome.Note(text: reason)
            }
        }
    }

    private func send(_ text: String) {
        freedv.sendMessage(text)
        freedv.editingMessage = false
    }
}
