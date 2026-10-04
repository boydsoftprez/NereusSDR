// NereusSDR for iOS: how to back up the Core's key, and telling the Core it is done
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// "Show me how", from the Devices page's reminder (R-IOS-08; the pairing
/// design's section 3.2: the key is one file at a documented path, and the
/// app prompts for its backup). It names the file on the Core's computer,
/// says what to do with it, and "I've backed it up" tells the Core, which
/// then stops the reminder on every device.
struct KeyBackupSheet: View {
    @ObservedObject var model: DevicesModel
    let close: () -> Void

    static let steps = "On the Core's computer, copy this file somewhere safe, away from that computer: a USB stick, or another computer."
    static let privateNote = "Keep the copy private. Whoever has it can stand in for this Core."
    static let restoreNote = "If the Core's card or disk fails, put the file back in the same place and every device connects as before."

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Back up the Core's key")
                .font(.system(size: 19, weight: .heavy))
                .foregroundStyle(ChromeColours.textBright)
                .accessibilityAddTraits(.isHeader)
            Text(Self.steps)
                .font(.system(size: 13))
                .foregroundStyle(ChromeColours.text)
                .fixedSize(horizontal: false, vertical: true)
            Text(model.keyPath.isEmpty ? "The Core didn't say where its key file is." : model.keyPath)
                .font(.system(size: 13, weight: .bold, design: .monospaced))
                .foregroundStyle(ConnectChrome.codeText)
                .textSelection(.enabled)
                .padding(10)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(ConnectChrome.fieldFill, in: RoundedRectangle(cornerRadius: 4))
                .accessibilityIdentifier("keyPath")
            Text(Self.privateNote)
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.textDim)
                .fixedSize(horizontal: false, vertical: true)
            Text(Self.restoreNote)
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.textDim)
                .fixedSize(horizontal: false, vertical: true)
            if let problem = model.backupProblem {
                Text(problem)
                    .font(.system(size: 12))
                    .foregroundStyle(ConnectChrome.badText)
                    .accessibilityIdentifier("keyBackupProblem")
            }
            ConnectChrome.WideButton(title: "I've backed it up", look: .go, enabled: model.administers) {
                Task {
                    await model.acknowledgeBackup()
                    if model.backupProblem == nil {
                        close()
                    }
                }
            }
            .accessibilityIdentifier("keyBackupDone")
            ConnectChrome.WideButton(title: "Not now", action: close)
                .accessibilityIdentifier("keyBackupNotNow")
            Spacer(minLength: 0)
        }
        .padding(18)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(ConnectChrome.sheetFill.ignoresSafeArea())
    }
}
