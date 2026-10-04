// NereusSDR for iOS: the Radio tab's Core and link: the Core's name, how it is reached, Disconnect and Remove Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The Core and link (the board's `.stn`): the Core's name over how it is
/// reached, and Disconnect (the board's `.revoke`) at the right.
struct RadioCoreCard: View {
    let name: String
    let status: String
    let canDisconnect: Bool
    let removeReason: String?
    let disconnect: () -> Void
    let remove: () -> Void

    /// The Core's name as the band's toolbar shows it, else its address.
    static func name(coreName: String?, coreHost: String?) -> String {
        coreName ?? coreHost ?? ""
    }

    /// The line under the name: the link's state, then while it is up how
    /// it is reached and the round-trip time once one is measured.
    static func status(_ link: LinkState) -> String {
        if link.offline {
            return "Offline"
        }
        if link.isUp {
            var parts = ["Connected", link.pathWord.lowercased()]
            if let ms = link.roundTripMs {
                parts.append("\(ms) ms")
            }
            return parts.joined(separator: " \u{00B7} ")
        }
        if link.isLost {
            return "Reconnecting"
        }
        switch link.connection {
        case .connecting, .signingIn, .loading:
            return "Connecting"
        case .notConnected, .connected, .waitingToRetry, .refused:
            return "Not connected"
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 10) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(name)
                        .font(.system(size: 17, weight: .bold, design: .monospaced))
                        .foregroundStyle(ChromeColours.text)
                        .lineLimit(1)
                        .truncationMode(.middle)
                        .accessibilityIdentifier("radioCoreName")
                    Text(status)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .lineLimit(1)
                        .accessibilityIdentifier("radioCoreStatus")
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                Button(action: disconnect) {
                    Text("Disconnect")
                        .font(.system(size: 13, weight: .bold))
                        .foregroundStyle(canDisconnect ? ChromeColours.revokeText : ChromeColours.buttonOffText)
                        .padding(.horizontal, 12)
                        .frame(height: 36)
                        .background(canDisconnect ? ChromeColours.revoke : ChromeColours.buttonOff,
                                    in: RoundedRectangle(cornerRadius: 4))
                        .overlay(RoundedRectangle(cornerRadius: 4)
                            .strokeBorder(canDisconnect ? ChromeColours.revokeBorder : ChromeColours.buttonOffBorder,
                                          lineWidth: 1))
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(!canDisconnect)
                .accessibilityIdentifier("radioDisconnect")
            }
            Button("Remove Core", role: .destructive, action: remove)
                .font(.system(size: 13, weight: .bold))
                .disabled(removeReason != nil)
                .accessibilityIdentifier("radioRemoveCore")
            if let removeReason {
                Text(removeReason)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .accessibilityIdentifier("radioRemoveReason")
            }
        }
        .padding(12)
        .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.stationBorder, lineWidth: 1))
    }
}
