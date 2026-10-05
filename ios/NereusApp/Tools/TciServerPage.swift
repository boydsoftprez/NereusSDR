// NereusSDR for iOS: TCI Server, one Tools page: the Core's TCI switch, port and options, and the apps connected to it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The TCI Server page (spec section 5.2 item 4, R-IOS-18): the one TCI
/// switch and port the Core keeps for its own server, where it listens, its
/// four options for apps that connect, and the apps connected to it, each
/// with a Disconnect button that asks first.
struct TciServerPage: View {
    @ObservedObject var model: TciServerModel

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            SpotHubPage.Heading(text: "Server", tag: .core)
            SpotHubPage.Card {
                SpotHubPage.SettingRow(title: "TCI server", detail: model.stateText) {
                    SpotHubPage.OnOff(isOn: model.enabled ?? false,
                                      enabled: model.reason == nil && !model.sending, identifier: "tci.enabled") {
                        model.setEnabled(!(model.enabled ?? false))
                    }
                }
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Port", detail: "Apps connect to the Core on this port") {
                    ValueField(text: model.port.map { "\($0)" } ?? "--", accessibility: "TCI port",
                               disabled: model.reason != nil || model.sending, minWidth: 64) {
                        model.openPortPad()
                    }
                    .accessibilityIdentifier("tci.port")
                }
                if !model.stationAddress.isEmpty {
                    SpotHubPage.Line()
                    ToolPageParts.Reading(title: "At the station", value: model.stationAddress,
                                          identifier: "tci.stationAddress")
                }
                if !model.error.isEmpty {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: model.error, identifier: "tci.error")
                }
                if let reason = model.reason {
                    SpotHubPage.Line()
                    ToolPageParts.Reason(text: reason, identifier: "tci.reason")
                }
            }
            SpotHubPage.Heading(text: "Options", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                ForEach(Array(TciServerModel.optionRows.enumerated()), id: \.offset) { index, option in
                    if index > 0 {
                        SpotHubPage.Line()
                    }
                    let isOn = model.options?[keyPath: option.id] ?? false
                    SpotHubPage.SettingRow(title: option.title, detail: option.detail) {
                        SpotHubPage.OnOff(isOn: isOn, enabled: model.optionsReason == nil,
                                          identifier: option.identifier) {
                            model.setOption(option.id, !isOn)
                        }
                    }
                }
                SpotHubPage.Line()
                if let reason = model.optionsReason {
                    ToolPageParts.Reason(text: reason, identifier: "tci.optionsReason")
                } else {
                    Text(TciServerModel.optionsNote)
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .fixedSize(horizontal: false, vertical: true)
                        .padding(.horizontal, 10)
                        .padding(.vertical, 8)
                        .accessibilityIdentifier("tci.optionsNote")
                }
            }
            SpotHubPage.Heading(text: "Connected apps", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                if let reason = model.clientsReason {
                    ToolPageParts.Reason(text: reason, identifier: "tci.clientsReason")
                } else if model.clients.isEmpty {
                    ToolPageParts.Reading(title: TciServerModel.noClientsText, value: "", identifier: "tci.noClients")
                } else {
                    ForEach(Array(model.clients.enumerated()), id: \.element.id) { index, client in
                        if index > 0 {
                            SpotHubPage.Line()
                        }
                        clientRow(client)
                    }
                    if let reason = model.disconnectReason {
                        SpotHubPage.Line()
                        ToolPageParts.Reason(text: reason, identifier: "tci.disconnectReason")
                    }
                }
            }
            if let note = model.note {
                ToolPageParts.Refusal(text: note, identifier: "tci.note").padding(.top, 4)
            }
        }
        .onAppear { model.setOpen(true) }
        .onDisappear { model.setOpen(false) }
        .confirmationDialog(model.closing.map { "Disconnect \(Self.name($0))?" } ?? "",
                            isPresented: Binding(get: { model.closing != nil }, set: { if !$0 { model.closing = nil } }),
                            titleVisibility: .visible) {
            Button("Disconnect", role: .destructive) {
                if let client = model.closing {
                    model.disconnect(client)
                }
            }
        } message: {
            Text("The app can connect to the Core again.")
        }
    }

    private func clientRow(_ client: StationTciClient) -> some View {
        HStack(spacing: 10) {
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 6) {
                    Text(Self.name(client))
                        .font(.system(size: 14, weight: .bold))
                        .foregroundStyle(ChromeColours.textBright)
                    if client.transmitting {
                        Text("TX")
                            .font(.system(size: 10, weight: .bold))
                            .foregroundStyle(ChromeColours.revokeText)
                            .padding(.horizontal, 5)
                            .padding(.vertical, 1)
                            .background(ChromeColours.revoke, in: RoundedRectangle(cornerRadius: 3))
                    }
                }
                Text(client.address)
                    .font(.system(size: 11).monospacedDigit())
                    .foregroundStyle(ChromeColours.textDim)
                if !client.subscriptions.isEmpty {
                    Text(client.subscriptions.joined(separator: ", "))
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textDim)
                }
                if !client.lastCommand.isEmpty {
                    Text("Last: \(client.lastCommand)")
                        .font(.system(size: 11).monospaced())
                        .foregroundStyle(ChromeColours.textFaint)
                        .lineLimit(1)
                        .truncationMode(.tail)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            Button("Disconnect") {
                model.closing = client
            }
            .font(.system(size: 12, weight: .semibold))
            .foregroundStyle(ChromeColours.revokeText)
            .padding(.horizontal, 10)
            .frame(minHeight: 30)
            .background(ChromeColours.revoke, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.revokeBorder, lineWidth: 1))
            .buttonStyle(.plain)
            .disabled(model.disconnectReason != nil)
            .opacity(model.disconnectReason == nil ? 1 : 0.5)
            .accessibilityLabel("Disconnect \(Self.name(client))")
            .accessibilityIdentifier("tci.disconnect.\(client.id)")
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 9)
        .accessibilityIdentifier("tci.client.\(client.id)")
    }

    /// An app's name as it gave it, or "An app" when it gave none.
    static func name(_ client: StationTciClient) -> String {
        client.name.isEmpty ? "An app" : client.name
    }
}
