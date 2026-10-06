// NereusSDR for iOS: one of the Core's spot sources: its connection, its settings, its live console and a command line
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// One spot source the Core runs (spec section 5.7 item 4, picture 20's
/// fourth phone): the DX cluster, the Reverse Beacon Network, POTA or PSK
/// Reporter. Its state and Connect or Disconnect, its connection settings
/// as the Core keeps them (server, port, callsign and auto-connect for a
/// cluster; the poll interval for POTA; callsign and grid for PSK
/// Reporter), its live console, and a command line that types into the
/// Core's console for the sources that take commands. The Core holds the
/// connection, so it keeps running while the phone is away.
struct SpotSourcePage: View {
    @ObservedObject var spots: SpotsModel
    let source: SpotsModel.Source
    @State private var command = ""

    static let note = "The Core holds this connection, so spots keep coming while the phone is off."

    var body: some View {
        let status = spots.status(source)
        let reason = spots.connectReason(source)
        VStack(alignment: .leading, spacing: 8) {
            header(status, reason: reason)
            if let reason {
                AccessoryChrome.Note(text: reason)
            }
            if let note = spots.notes[source] {
                AccessoryChrome.Refusal(text: note) { spots.notes[source] = nil }
            }
            if let connection = SpotsModel.connection(source) {
                SpotHubPage.Heading(text: "Connection").padding(.top, 8)
                SpotHubPage.Card {
                    settings(connection, enabled: reason == nil)
                }
            }
            SpotHubPage.Heading(text: "Console").padding(.top, 8)
            console
            commandLine
            ConnectChrome.Note(text: Self.note).padding(.top, 4)
        }
        .onAppear { spots.pageOpened(source) }
        .onDisappear { spots.pageClosed(source) }
    }

    /// The line under the state: where the source is connected and how
    /// many of its spots the Core holds, else the Core's words.
    static func headerLine(_ spots: SpotsModel, _ source: SpotsModel.Source) -> String {
        let status = spots.status(source)
        if status.state == .connected {
            var parts: [String] = []
            if let hostKey = SpotsModel.connection(source)?.hostKey {
                parts.append(spots.setting(hostKey, SpotsModel.connection(source)?.hostDefault ?? ""))
            } else if !status.text.isEmpty {
                parts.append(status.text)
            }
            let count = spots.spots.filter { $0.source == source.spotLabel }.count
            parts.append(count == 1 ? "1 spot" : "\(count) spots")
            return parts.joined(separator: " \u{00B7} ")
        }
        return status.text.isEmpty ? "Not running" : status.text
    }

    private func header(_ status: SpotsModel.SourceStatus, reason: String?) -> some View {
        let running = status.state == .connected || status.state == .connecting
        return HStack(spacing: 10) {
            Circle().fill(SpotHubPage.dot(spots, source)).frame(width: 12, height: 12)
            VStack(alignment: .leading, spacing: 2) {
                Text(SpotsModel.stateWords(status.state) == "Off" ? "Disconnected" : SpotsModel.stateWords(status.state))
                    .font(.system(size: 16, weight: .bold))
                    .foregroundStyle(ChromeColours.textBright)
                Text(Self.headerLine(spots, source))
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            Button {
                spots.setRunning(source, !running)
            } label: {
                Text(running ? "Disconnect" : "Connect")
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(reason != nil ? ChromeColours.buttonOffText : (running ? ChromeColours.text : .white))
                    .padding(.horizontal, 12)
                    .frame(minWidth: 88, minHeight: 40)
                    .background(reason != nil ? ChromeColours.buttonOff
                                : (running ? ChromeColours.button : ChromeColours.buttonOnBlue),
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(reason != nil ? ChromeColours.buttonOffBorder
                                      : (running ? ChromeColours.buttonBorder : ChromeColours.buttonOnBlueBorder),
                                      lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(reason != nil)
            .accessibilityIdentifier("spotSource.connect")
        }
        .padding(12)
        .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.stationBorder, lineWidth: 1))
    }

    @ViewBuilder
    private func settings(_ connection: SpotsModel.Connection, enabled: Bool) -> some View {
        if let hostKey = connection.hostKey {
            field("Server", key: hostKey, placeholder: connection.hostDefault, enabled: enabled)
            SpotHubPage.Line()
        }
        if let portKey = connection.portKey {
            field("Port", key: portKey, placeholder: String(connection.portDefault), keyboard: .numberPad,
                  enabled: enabled)
            SpotHubPage.Line()
        }
        if let pollKey = connection.pollKey {
            field("Poll every", key: pollKey, placeholder: "\(connection.pollDefault)", keyboard: .numberPad,
                  enabled: enabled, unit: "sec")
            SpotHubPage.Line()
        }
        if let callsignKey = connection.callsignKey {
            fieldRow("Callsign", value: spots.callsign(for: source), placeholder: "", enabled: enabled,
                     identifier: "spotSource.\(callsignKey)") { value in
                spots.write(callsignKey, value.uppercased(), for: source)
            }
            SpotHubPage.Line()
        }
        if let gridKey = connection.gridKey {
            fieldRow("Grid", value: spots.setting(gridKey, spots.setting(SpotsModel.gridKey, "")), placeholder: "",
                     enabled: enabled, identifier: "spotSource.\(gridKey)") { value in
                spots.write(gridKey, value.uppercased(), for: source)
            }
            SpotHubPage.Line()
        }
        SpotHubPage.SettingRow(title: connection.autoTitle, detail: "When the Core starts") {
            SpotHubPage.OnOff(isOn: spots.flag(connection.autoKey), enabled: enabled, identifier: "spotSource.auto") {
                spots.write(connection.autoKey, spots.flag(connection.autoKey) ? "False" : "True", for: source)
            }
        }
    }

    private func field(_ title: String, key: String, placeholder: String, keyboard: UIKeyboardType = .default,
                       enabled: Bool, unit: String? = nil) -> some View {
        fieldRow(unit.map { "\(title) (\($0))" } ?? title, value: spots.stationSettings[key] ?? "",
                 placeholder: placeholder, keyboard: keyboard, enabled: enabled,
                 identifier: "spotSource.\(key)") { value in
            spots.write(key, value, for: source)
        }
    }

    private func fieldRow(_ title: String, value: String, placeholder: String, keyboard: UIKeyboardType = .default,
                          enabled: Bool, identifier: String, set: @escaping (String) -> Void) -> some View {
        AccessoryFields.TextRow(label: title, value: value, placeholder: placeholder, keyboard: keyboard,
                                enabled: enabled, identifier: identifier, set: set)
            .padding(.horizontal, 10)
            .padding(.vertical, 7)
    }

    private var console: some View {
        let lines = spots.consoles[source] ?? []
        return ScrollViewReader { reader in
            ScrollView([.vertical, .horizontal]) {
                VStack(alignment: .leading, spacing: 0) {
                    if lines.isEmpty {
                        Text(spots.available ? "No lines from this source." : " ")
                            .foregroundStyle(ChromeColours.textFaint)
                    }
                    ForEach(Array(lines.enumerated()), id: \.offset) { index, line in
                        Text(line.isEmpty ? " " : line)
                            .id(index)
                    }
                }
                .font(.system(size: 10.5, design: .monospaced))
                .foregroundStyle(SpotColours.consoleText)
                .lineSpacing(3)
                .padding(10)
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .frame(height: 150)
            .background(SpotColours.console, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(SpotColours.consoleBorder, lineWidth: 1))
            .onAppear { reader.scrollTo(lines.count - 1, anchor: .bottomLeading) }
            .onChange(of: lines.count) { _, count in
                reader.scrollTo(count - 1, anchor: .bottomLeading)
            }
            .accessibilityIdentifier("spotSource.console")
        }
    }

    private var commandLine: some View {
        let reason = spots.commandReason(source)
        return VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 8) {
                TextField(source.takesCommands ? "Command" : "", text: $command)
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()
                    .submitLabel(.send)
                    .onSubmit(send)
                    .padding(.horizontal, 8)
                    .frame(minHeight: 38)
                    .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
                    .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                    .disabled(reason != nil)
                    .accessibilityLabel("Command")
                    .accessibilityIdentifier("spotSource.command")
                let ready = reason == nil && !command.trimmingCharacters(in: .whitespaces).isEmpty
                Button(action: send) {
                    Text("Send")
                        .font(.system(size: 13, weight: .bold))
                        .foregroundStyle(ready ? Color.white : ChromeColours.buttonOffText)
                        .frame(minWidth: 72, minHeight: 38)
                        .background(ready ? ChromeColours.buttonOnBlue : ChromeColours.buttonOff,
                                    in: RoundedRectangle(cornerRadius: 4))
                        .overlay(RoundedRectangle(cornerRadius: 4)
                            .strokeBorder(ready ? ChromeColours.buttonOnBlueBorder : ChromeColours.buttonOffBorder,
                                          lineWidth: 1))
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(!ready)
                .accessibilityIdentifier("spotSource.send")
            }
            if let reason, reason != spots.connectReason(source) {
                AccessoryChrome.Note(text: reason)
            }
        }
    }

    private func send() {
        let text = command.trimmingCharacters(in: .whitespaces)
        guard !text.isEmpty, spots.commandReason(source) == nil else {
            return
        }
        spots.sendCommand(source, text)
        command = ""
    }
}
