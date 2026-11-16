// NereusSDR for iOS: opening the app with Reconnect automatically on: the last Core, how far the connection has got, and Cancel
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-05: Reconnect automatically. J.J. Boyd (KG4VCF), AI-assisted via
// Claude Code.

import SwiftUI

/// What a launch shows while it connects to the last Core: that Core's row as
/// Your Cores draws it while connecting, with its Cancel. Cancel, or a Core
/// that can't be reached or needs pairing, leads to Your Cores.
struct ConnectingOnOpenScreen: View {
    @ObservedObject var flow: ConnectionFlow

    var body: some View {
        VStack(spacing: 0) {
            ConnectChrome.NavBar(title: "Cores") { EmptyView() }
            VStack(spacing: 8) {
                ConnectChrome.Heading(text: "Connecting")
                    .padding(.top, 10)
                HStack(spacing: 12) {
                    ConnectChrome.Pill(colour: ConnectChrome.pillStale)
                    VStack(alignment: .leading, spacing: 3) {
                        Text(label)
                            .font(.system(size: 15, weight: .bold, design: .monospaced))
                            .foregroundStyle(ChromeColours.text)
                            .lineLimit(1)
                            .truncationMode(.tail)
                        Text(flow.connectingNote ?? "Connecting\u{2026}")
                            .font(.system(size: 12))
                            .foregroundStyle(ChromeColours.textDim)
                            .lineLimit(3)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    Spacer(minLength: 0)
                    ConnectChrome.RowButton(title: "Cancel") {
                        Task { await flow.cancelConnecting() }
                    }
                    .accessibilityIdentifier("cancelConnecting")
                }
                .padding(.vertical, 10)
                .padding(.horizontal, 12)
                .frame(minHeight: 66)
                .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius))
                .overlay(RoundedRectangle(cornerRadius: WorkspacePalette.chipRadius)
                    .strokeBorder(ConnectChrome.rowBorder, lineWidth: 1))
                Spacer(minLength: 0)
            }
            .padding(.horizontal, 14)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
    }

    private var label: String {
        flow.cores.first { $0.id == flow.connectingTo }?.label ?? ""
    }
}
