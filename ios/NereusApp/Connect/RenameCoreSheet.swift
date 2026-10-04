// NereusSDR for iOS: Rename this Core: the Core's own name, which every device shows
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// Rename this Core (D76, R-IOS-08), rising over Your Cores from a row's
/// press-and-hold menu: the name field, filled with the Core's name (empty
/// when it has none), the Core's rule under it, then Save and Cancel. Save
/// sends the name to the Core; a refusal stays here with the Core's words
/// under the field in place of the rule.
struct RenameCoreSheet: View {
    @ObservedObject var flow: ConnectionFlow
    let row: ConnectionFlow.CoreRow
    @FocusState private var fieldFocused: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Capsule()
                .fill(ConnectChrome.grab)
                .frame(width: 40, height: 5)
                .frame(maxWidth: .infinity)
            Text(ConnectionFlow.renameTitle)
                .font(.system(size: 19, weight: .heavy))
                .foregroundStyle(ChromeColours.textBright)
                .padding(.top, 4)
                .accessibilityAddTraits(.isHeader)
                .accessibilityIdentifier("renameTitle")
            Text(row.address)
                .font(.system(size: 12, design: .monospaced))
                .foregroundStyle(ChromeColours.textDim)
                .lineLimit(1)
                .truncationMode(.middle)
            TextField("", text: $flow.renameText)
                .font(.system(size: 17, weight: .bold, design: .monospaced))
                .foregroundStyle(problem == nil ? ChromeColours.textBright : ConnectChrome.badText)
                .textInputAutocapitalization(.characters)
                .autocorrectionDisabled()
                .keyboardType(.asciiCapable)
                .submitLabel(.done)
                .focused($fieldFocused)
                .onSubmit { save() }
                .disabled(flow.renameBusy)
                .padding(.horizontal, 14)
                .frame(height: 50)
                .background(ConnectChrome.fieldFill, in: RoundedRectangle(cornerRadius: 4))
                .overlay(RoundedRectangle(cornerRadius: 4)
                    .strokeBorder(problem == nil ? ChromeColours.accent : ConnectChrome.badBorder, lineWidth: 1))
                .padding(.top, 4)
                .accessibilityLabel("The Core's name")
                .accessibilityIdentifier("renameField")
            Group {
                if let problem {
                    Text(problem)
                        .foregroundStyle(ConnectChrome.badText)
                        .accessibilityIdentifier("renameProblem")
                } else {
                    Text(ConnectionFlow.renameRule)
                        .foregroundStyle(ChromeColours.textDim)
                        .accessibilityIdentifier("renameRule")
                }
            }
            .font(.system(size: 12))
            .lineSpacing(2)
            .fixedSize(horizontal: false, vertical: true)
            ConnectChrome.WideButton(title: "Save", look: .go, enabled: canSave, busy: flow.renameBusy) { save() }
                .padding(.top, 6)
                .accessibilityIdentifier("renameSave")
            ConnectChrome.WideButton(title: "Cancel") {
                fieldFocused = false
                Task { await flow.cancelRename() }
            }
            .accessibilityIdentifier("renameCancel")
        }
        .padding(.horizontal, 18)
        .padding(.top, 8)
        .padding(.bottom, 20)
        .frame(maxWidth: .infinity)
        .background(ConnectChrome.sheetFill, in: UnevenRoundedRectangle(topLeadingRadius: 14, topTrailingRadius: 14))
        .overlay(alignment: .top) {
            UnevenRoundedRectangle(topLeadingRadius: 14, topTrailingRadius: 14)
                .stroke(ConnectChrome.sheetEdge, lineWidth: 1)
        }
        .shadow(color: .black.opacity(0.5), radius: 15, y: -10)
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("renameSheet")
        .onAppear { fieldFocused = true }
    }

    private var problem: String? { flow.renameProblem }

    private var canSave: Bool {
        !flow.renameText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
            && flow.renameProblem != ConnectionFlow.needsNewerCoreText
    }

    private func save() {
        guard canSave, !flow.renameBusy else {
            return
        }
        Task { await flow.saveRename() }
    }
}
