// NereusSDR for iOS: FreeDV in Spot Hub: the Core's reporter connection, "Hide my station", its console, and this phone's units
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The FreeDV page under Spot Hub (spec section 5.7 item 10, picture 21's
/// fourth phone): the desktop's FreeDV tab. The reporter's state with
/// Connect or Disconnect and its auto-start, and "Hide my station from the
/// dashboard", all at the Core, so your station stays listed while the phone
/// is away; then this phone's miles and kHz; then the reporter's console.
/// Everything that can't act says why.
struct FreeDVSourcePage: View {
    @ObservedObject var freedv: FreeDVReporterModel
    @ObservedObject var spots: SpotsModel

    var body: some View {
        let reason = freedv.coreReason
        VStack(alignment: .leading, spacing: 8) {
            header(reason: reason)
            if let reason {
                AccessoryChrome.Note(text: reason)
            }
            if let note = freedv.sourceNote {
                AccessoryChrome.Refusal(text: note) { freedv.sourceNote = nil }
            }
            SpotHubPage.Heading(text: "At the Core", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                let hidden = freedv.status?.hidden ?? false
                SpotHubPage.SettingRow(title: "Hide my station from the dashboard", detail: hideDetail) {
                    SpotHubPage.OnOff(isOn: hidden, enabled: reason == nil, identifier: "freedv.hide") {
                        freedv.setHidden(!hidden)
                    }
                }
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Auto-start", detail: "When the Core starts") {
                    SpotHubPage.OnOff(isOn: freedv.autoStart, enabled: reason == nil, identifier: "freedv.autoStart") {
                        freedv.setAutoStart(!freedv.autoStart)
                    }
                }
            }
            SpotHubPage.Heading(text: "On this phone", tag: .thisPhone).padding(.top, 8)
            SpotHubPage.Card {
                SpotHubPage.SettingRow(title: "Distances in miles", detail: "Off: kilometres") {
                    SpotHubPage.OnOff(isOn: freedv.miles, identifier: "freedv.miles") {
                        freedv.setMiles(!freedv.miles)
                    }
                }
                SpotHubPage.Line()
                SpotHubPage.SettingRow(title: "Frequencies in kHz", detail: "Off: MHz") {
                    SpotHubPage.OnOff(isOn: freedv.kilohertz, identifier: "freedv.khz") {
                        freedv.setKilohertz(!freedv.kilohertz)
                    }
                }
            }
            if freedv.gridSquare.isEmpty {
                ConnectChrome.Note(text: FreeDVReporterModel.gridNote).padding(.top, 2)
            }
            SpotHubPage.Heading(text: "Console").padding(.top, 8)
            console
        }
        .onAppear { freedv.consoleOpened() }
        .onDisappear { freedv.consoleClosed() }
    }

    /// "Others won't see KG4VCF", or where to set the callsign.
    private var hideDetail: String {
        let call = freedv.callsign
        return call.isEmpty ? "Others won't see your station" : "Others won't see \(call)"
    }

    /// The header's words: its state, then where it is connected and how many stations are on the air, else the Core's words.
    static func headerWords(_ freedv: FreeDVReporterModel) -> (title: String, detail: String) {
        guard let status = freedv.status else {
            return ("Not running", freedv.coreReason ?? FreeDVReporterModel.olderCoreReason)
        }
        switch status.state {
        case .connected:
            let count = freedv.stations.count
            let stations = count == 1 ? "1 station on the air" : "\(count) stations on the air"
            return ("Connected", "\(FreeDVReporterModel.server) \u{00B7} \(stations)")
        case .connecting:
            return ("Connecting", status.text.isEmpty ? FreeDVReporterModel.server : status.text)
        case .error:
            return ("Error", status.text.isEmpty ? "The connection stopped with an error." : status.text)
        case .off:
            return ("Stopped", status.text.isEmpty ? "Not connected to \(FreeDVReporterModel.server)" : status.text)
        }
    }

    private func header(reason: String?) -> some View {
        let state = freedv.status?.state ?? .off
        let running = state == .connected || state == .connecting
        let words = Self.headerWords(freedv)
        return HStack(spacing: 10) {
            Circle().fill(SpotHubPage.dot(spots, .freeDv)).frame(width: 12, height: 12)
            VStack(alignment: .leading, spacing: 2) {
                Text(words.title)
                    .font(.system(size: 16, weight: .bold))
                    .foregroundStyle(ChromeColours.textBright)
                Text(words.detail)
                    .font(.system(size: 11))
                    .foregroundStyle(ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            Button {
                freedv.setRunning(!running)
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
            .accessibilityIdentifier("freedv.connect")
        }
        .padding(12)
        .background(ConnectChrome.rowFill, in: RoundedRectangle(cornerRadius: 4))
        .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.stationBorder, lineWidth: 1))
    }

    private var console: some View {
        let lines = freedv.console
        return ScrollViewReader { reader in
            ScrollView([.vertical, .horizontal]) {
                VStack(alignment: .leading, spacing: 0) {
                    if lines.isEmpty {
                        Text(freedv.runsReporter ? "No lines from FreeDV Reporter." : " ")
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
            .accessibilityIdentifier("freedv.console")
        }
    }
}
