// NereusSDR for iOS: an accessory's Advanced page: its name, hardware, network and connection to change, and what the Core knows of it
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// Advanced, for each accessory (spec section 5.4 items 2 and 3; the
/// RF2K-S's "RF-Kit settings"), with everything a remote window changes of
/// the device through the Core. The Power Genius: Identity (its name),
/// Hardware (bias, fan, LED, the output limit), Network (the Core's address
/// for it, Connect, the Core's LAN scan, the 4O3A switch, the connection
/// settings, the amp's own network, Save & Reboot and Revert), Pairing and
/// Diagnostics. The Tuner Genius: Identity, Antenna Labels, Network and
/// Diagnostics. The RF2K-S: Connection (the RF-Kit switch, its address,
/// automatic retry and reading interval, error reset and TCI mode), Antenna
/// labels and Live diagnostics. What has no command stays as information.
struct AccessoryAdvancedPage: View {
    @ObservedObject var model: AccessoriesModel
    let device: AccessoriesModel.Device
    let coreName: String
    var now: () -> Date = Date.init

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            if let note = model.notes[device] {
                AccessoryChrome.Refusal(text: note) { model.dismissNote(device) }
            }
            switch device {
            case .powerGenius:
                powerGenius
            case .tunerGenius:
                tunerGenius
            case .rfKit:
                rfKit
            }
        }
        .accessibilityIdentifier("advancedPage")
    }

    // MARK: The Power Genius

    @ViewBuilder
    private var powerGenius: some View {
        if let amp = model.powerGenius {
            let settings = model.deviceSettings
            let own = model.deviceSettingsReason(.powerGenius)
            section("Identity", reason: own) {
                AccessoryFields.TextRow(label: "Name", value: Self.text(settings["pgxlNickname"]) ?? amp.link.nickname,
                                        placeholder: "e.g. Shack_PGXL", enabled: own == nil, identifier: "pgxlName",
                                        edit: AccessoryFields.nameEdit, hint: AccessoryFields.nameTip) {
                    model.setName(.powerGenius, $0)
                }
                info(Self.identity(amp.link) + [Row(label: "State", value: Self.dash(amp.deviceState)),
                                                Row(label: "MEffA", value: Self.dash(amp.efficiency))])
            }
            section("Hardware", reason: own, footnote: "The amp uses bias, fan and LED after Save & Reboot.") {
                AccessoryFields.ChoiceRow(label: "Bias", value: Self.text(settings["pgxlBiasMode"]) ?? "",
                                          options: ["ClassA", "ClassAB"], enabled: own == nil,
                                          identifier: "pgxlBias") {
                    model.setPgxlHardware("biasMode", .text($0))
                }
                AccessoryFields.ChoiceRow(label: "Fan", value: Self.text(settings["pgxlFanMode"]) ?? "",
                                          options: ["Auto", "Quiet", "Continuous"], enabled: own == nil,
                                          identifier: "pgxlFan") {
                    model.setPgxlHardware("fanMode", .text($0))
                }
                AccessoryFields.NumberRow(label: "LED", value: Self.whole(settings["pgxlLedIntensity"]) ?? 0,
                                          range: 0...100, step: 10, unit: "%", enabled: own == nil,
                                          identifier: "pgxlLed") {
                    model.setPgxlHardware("ledIntensity", .int($0))
                }
            }
            if let records = model.records {
                section("Output limit", reason: model.recordsReason) {
                    AccessoryFields.SwitchRow(label: "Limit the output", isOn: records.powerCapEnabled,
                                              enabled: model.recordsReason == nil, identifier: "pgxlPowerCapOn") {
                        model.setPowerCap(enabled: $0, watts: max(records.powerCapW, 100))
                    }
                    AccessoryFields.NumberRow(label: "Limit", value: max(records.powerCapW, 100), range: 100...2000,
                                              step: 100, unit: "W", enabled: model.recordsReason == nil,
                                              identifier: "pgxlPowerCap") {
                        model.setPowerCap(enabled: records.powerCapEnabled, watts: $0)
                    }
                }
            }
            connection(.powerGenius, link: amp.link, connected: amp.link.connected)
            let connectionSettings = model.pgxlConnectionSettings
            section("Connection settings", reason: model.connectionReason(.powerGenius)) {
                let enabled = model.connectionReason(.powerGenius) == nil
                AccessoryFields.SwitchRow(label: "Reconnect by itself", isOn: connectionSettings.autoReconnect,
                                          enabled: enabled, identifier: "pgxlAutoReconnect") {
                    model.setPgxlConnectionSettings(autoReconnect: $0, keepaliveSec: connectionSettings.keepaliveSec,
                                                    pingSec: connectionSettings.pingSec)
                }
                AccessoryFields.NumberRow(label: "Check it's there every", value: connectionSettings.keepaliveSec,
                                          range: 1...3600, step: 5, unit: "s", enabled: enabled,
                                          identifier: "pgxlKeepalive") {
                    model.setPgxlConnectionSettings(autoReconnect: connectionSettings.autoReconnect, keepaliveSec: $0,
                                                    pingSec: connectionSettings.pingSec)
                }
                AccessoryFields.NumberRow(label: "Time its answer every", value: connectionSettings.pingSec,
                                          range: 0...3600, step: 5, unit: "s", enabled: enabled,
                                          identifier: "pgxlPing") {
                    model.setPgxlConnectionSettings(autoReconnect: connectionSettings.autoReconnect,
                                                    keepaliveSec: connectionSettings.keepaliveSec, pingSec: $0)
                }
            }
            deviceNetwork(.powerGenius, prefix: "pgxl", name: "Power Genius")
            section("Pairing") {
                info([Row(label: "Band", value: AccessoryStatusLine.ampBand(amp, bandLabel: model.bandLabel)),
                      Row(label: "Paired with", value: PowerGeniusPage.pairedWith(amp, coreName: coreName))])
            }
            section("Diagnostics") {
                info(Self.diagnostics("pgxl", counters: model.records?.counters ?? [:], now: now()))
            }
        }
    }

    // MARK: The Tuner Genius

    @ViewBuilder
    private var tunerGenius: some View {
        if let tuner = model.tunerGenius {
            let settings = model.deviceSettings
            let own = model.deviceSettingsReason(.tunerGenius)
            section("Identity", reason: own) {
                AccessoryFields.TextRow(label: "Name", value: Self.text(settings["tgxlNickname"]) ?? tuner.link.nickname,
                                        placeholder: "e.g. Shack_TGXL", enabled: own == nil, identifier: "tgxlName",
                                        edit: AccessoryFields.nameEdit, hint: AccessoryFields.nameTip) {
                    model.setName(.tunerGenius, $0)
                }
                info(Self.identity(tuner.link) + [Row(label: "State", value: AccessoryStatusLine.tunerState(tuner))])
            }
            labels(.tunerGenius, count: 3)
            section("Tune memory", reason: model.recordsReason) {
                AccessoryFields.SwitchRow(label: "Recall a stored tune on a band or antenna change",
                                          isOn: model.records?.autoRecall ?? false,
                                          enabled: model.recordsReason == nil, identifier: "tgxlAutoRecall") {
                    model.setAutoRecall($0)
                }
            }
            connection(.tunerGenius, link: tuner.link, connected: tuner.link.connected)
            deviceNetwork(.tunerGenius, prefix: "tgxl", name: "Tuner Genius")
            section("Diagnostics") {
                info(Self.diagnostics("tgxl", counters: model.records?.counters ?? [:], now: now()))
            }
        }
    }

    // MARK: The RF2K-S

    @ViewBuilder
    private var rfKit: some View {
        if let rfKit = model.rfKit {
            let counters = model.records?.counters ?? [:]
            let olderReason = model.versions.rfKit >= 3 ? nil : AccessoriesModel.olderCoreSettingText
            let retry = model.rfKitConnectionSettings
            section("Connection", reason: model.connectionReason(.rfKit)) {
                AccessoryFields.SwitchRow(label: "RF-Kit amplifier on the Core", isOn: model.rfKitEnabled,
                                          enabled: model.connectionReason(.rfKit) == nil, identifier: "rfkitEnabled") {
                    model.setRfKitEnabled($0)
                }
                address(.rfKit, link: rfKit.link, connected: rfKit.link.connected)
                info([Row(label: "Status", value: AccessoryStatusLine.phaseWords(rfKit.link) ?? "Connected"),
                      Row(label: "Model", value: Self.dash(rfKit.link.model)),
                      Row(label: "Firmware", value: Self.dash(rfKit.link.version)),
                      Row(label: "Name", value: Self.dash(rfKit.link.nickname)),
                      Row(label: "Follows the radio by", value: Self.dash(rfKit.interface)),
                      Row(label: "Band follow",
                          value: AccessoryStatusLine.rfKitBandFollow(rfKit) ?? "Following the radio")])
            }
            section("Reading the amp", reason: olderReason) {
                AccessoryFields.SwitchRow(label: "Reconnect by itself", isOn: retry.autoReconnect,
                                          enabled: olderReason == nil, identifier: "rfkitAutoReconnect") {
                    model.setRfKitAutoReconnect($0)
                }
                AccessoryFields.NumberRow(label: "Read it every", value: retry.pollMs, range: 250...5000, step: 250,
                                          unit: "ms", enabled: olderReason == nil, identifier: "rfkitPoll") {
                    model.setRfKitPollInterval($0)
                }
            }
            let reset = model.rfKitActionReason(tciMode: false)
            let tci = model.rfKitActionReason(tciMode: true)
            section("The amp", reason: reset ?? tci) {
                AccessoryFields.ActionButton(title: "Reset amp error", enabled: reset == nil,
                                             identifier: "rfkitReset") {
                    model.resetRfKitError()
                }
                AccessoryFields.ActionButton(title: "Set amp to TCI mode", enabled: tci == nil,
                                             identifier: "rfkitTciMode") {
                    model.setRfKitTciMode()
                }
            }
            labels(.rfKit, count: 4)
            section("Live diagnostics") {
                info([Row(label: "Connected", value: Self.since(counters["rfkitConnectedSinceMs"], now: now())),
                      Row(label: "Readings answered", value: String(counters["rfkitPollsOk"] ?? 0)),
                      Row(label: "Readings missed", value: String(counters["rfkitPollsFailed"] ?? 0)),
                      Row(label: "Reconnects", value: String(counters["rfkitReconnectCount"] ?? 0)),
                      Row(label: "Last answer", value: Self.since(counters["rfkitLastPollMs"], now: now())),
                      Row(label: "Response time", value: Self.milliseconds(counters["rfkitRttAvgMs"]))])
            }
        }
    }

    // MARK: Shared sections

    /// The Core's address for the device, Connect, the LAN scan and the 4O3A switch.
    @ViewBuilder
    private func connection(_ device: AccessoriesModel.Device, link: AccessoriesModel.Link,
                            connected: Bool) -> some View {
        let reason = model.connectionReason(device) ?? model.savedAddressReason(device)
        let switchReason = model.stationSwitchReason(device)
        section("Connection", reason: reason) {
            AccessoryFields.SwitchRow(label: AccessorySetupCard.switchLabel(device), isOn: model.stationSwitch(device),
                                      enabled: switchReason == nil, identifier: "\(device.rawValue)FourO3A") {
                model.setStationSwitch(device, $0)
            }
            if let switchReason, switchReason != reason {
                AccessoryChrome.Note(text: switchReason)
            }
            info([Row(label: "Status", value: AccessoryStatusLine.phaseWords(link) ?? "Connected")])
            address(device, link: link, connected: connected)
            let scanning = model.scanning.contains(device)
            AccessoryFields.ActionButton(title: scanning ? "Looking on the station's network\u{2026}"
                                            : "Look for it on the station's network",
                                         enabled: model.savedAddressReason(device) == nil && !scanning,
                                         identifier: "\(device.rawValue)Scan") {
                model.scan(device)
            }
            if let found = model.scans[device] {
                if found.isEmpty {
                    AccessoryChrome.Note(text: "The Core heard none on the station's network.")
                } else {
                    ForEach(found) { heard in
                        HStack(spacing: 8) {
                            VStack(alignment: .leading, spacing: 1) {
                                Text("\(heard.address):\(heard.port)")
                                    .font(.system(size: 12, weight: .semibold, design: .monospaced))
                                    .foregroundStyle(ChromeColours.text)
                                Text([heard.model, heard.nickname, heard.serial].filter { !$0.isEmpty }
                                    .joined(separator: " \u{00B7} "))
                                    .font(.system(size: 11))
                                    .foregroundStyle(ChromeColours.textDim)
                            }
                            Spacer(minLength: 8)
                            AccessoryFields.SmallButton(title: "Use", enabled: true) {
                                model.saveAddress(device, host: heard.address, port: heard.port)
                            }
                            .accessibilityLabel("Use \(heard.address)")
                            .accessibilityIdentifier("\(device.rawValue)Use.\(heard.address)")
                        }
                    }
                }
            }
        }
    }

    private func address(_ device: AccessoriesModel.Device, link: AccessoriesModel.Link,
                         connected: Bool) -> some View {
        AccessoryFields.AddressEditor(host: link.host, port: link.port,
                                      connected: link.phase != .disconnected && link.phase != .disabled
                                          && link.phase != .error,
                                      saveEnabled: model.savedAddressReason(device) == nil,
                                      connectEnabled: model.connectionReason(device) == nil,
                                      prefix: "\(device.rawValue)Address",
                                      save: { model.saveAddress(device, host: $0, port: $1) },
                                      connect: { model.connect(device, host: $0, port: $1) },
                                      disconnect: { model.disconnect(device) })
    }

    /// The device's own network settings, Save & Reboot and Revert.
    @ViewBuilder
    private func deviceNetwork(_ device: AccessoriesModel.Device, prefix: String, name: String) -> some View {
        let settings = model.deviceSettings
        let reason = model.deviceSettingsReason(device)
        section("\(name) network", reason: reason) {
            AccessoryFields.NetworkEditor(dhcp: settings["\(prefix)Dhcp"] == .bool(true),
                                          address: Self.text(settings["\(prefix)Address"]) ?? "",
                                          netmask: Self.text(settings["\(prefix)Netmask"]) ?? "",
                                          gateway: Self.text(settings["\(prefix)Gateway"]) ?? "",
                                          enabled: reason == nil, prefix: "\(prefix)Network", deviceName: name) {
                model.setNetwork(device, dhcp: $0, address: $1, netmask: $2, gateway: $3)
            }
            if let answer = Self.text(settings["\(prefix)Answer"]) {
                AccessoryChrome.Note(text: answer)
            }
            AccessoryFields.ActionButton(title: "Save & Reboot", enabled: reason == nil, identifier: "\(prefix)Save",
                                         confirm: ("Save and restart the \(name)?",
                                                   "It keeps these settings and is offline for about 20 seconds.")) {
                model.saveDeviceSettings(device)
            }
            AccessoryFields.ActionButton(title: "Revert", enabled: reason == nil, identifier: "\(prefix)Revert") {
                model.revertDeviceSettings(device)
            }
        }
    }

    /// The operator's antenna names, each with Set.
    private func labels(_ device: AccessoriesModel.Device, count: Int) -> some View {
        let current = device == .rfKit ? model.records?.rfKitLabels : model.records?.tunerLabels
        return section(device == .rfKit ? "Antenna labels" : "Antenna Labels", reason: model.recordsReason) {
            ForEach(1...count, id: \.self) { (port: Int) in
                AccessoryFields.TextRow(label: "ANT \(port)", value: current?[port - 1] ?? "", placeholder: "ANT \(port)",
                                        enabled: model.recordsReason == nil,
                                        identifier: "\(device.rawValue)Label\(port)") {
                    model.setAntennaLabel(device, port: port, $0)
                }
            }
        }
    }

    private func section<Content: View>(_ title: String, reason: String? = nil, footnote: String? = nil,
                                        @ViewBuilder content: () -> Content) -> some View {
        let body = content()
        return VStack(alignment: .leading, spacing: 6) {
            AccessoryChrome.Caption(text: title)
            AccessoryChrome.Card {
                body
            }
            if let reason {
                AccessoryChrome.Note(text: reason)
            } else if let footnote {
                AccessoryChrome.Note(text: footnote)
            }
        }
    }

    private func info(_ rows: [Row]) -> some View {
        ForEach(rows, id: \.label) { row in
            AccessoryChrome.ValueRow(label: row.label, value: row.value)
        }
    }

    // MARK: Information

    struct Row: Equatable {
        let label: String
        let value: String
    }

    static func identity(_ link: AccessoriesModel.Link) -> [Row] {
        [Row(label: "Model", value: dash(link.model)), Row(label: "Serial", value: dash(link.serial)),
         Row(label: "Firmware", value: dash(link.version))]
    }

    static func diagnostics(_ prefix: String, counters: [String: Int64], now: Date) -> [Row] {
        [Row(label: "Connected", value: since(counters["\(prefix)ConnectedSinceMs"], now: now)),
         Row(label: "Response time", value: milliseconds(counters["\(prefix)LastRttMs"])),
         Row(label: "Missed checks", value: String(counters["\(prefix)KeepaliveMissed"] ?? 0)),
         Row(label: "Reconnects", value: String(counters["\(prefix)ReconnectCount"] ?? 0)),
         Row(label: "Lines in / out", value: "\(counters["\(prefix)FramesIn"] ?? 0) / "
             + "\(counters["\(prefix)FramesOut"] ?? 0)"),
         Row(label: "Bytes in / out", value: "\(counters["\(prefix)BytesIn"] ?? 0) / "
             + "\(counters["\(prefix)BytesOut"] ?? 0)"),
         Row(label: "Faults this connection", value: String(counters["\(prefix)FaultsSession"] ?? 0))]
    }

    private static func since(_ ms: Int64?, now: Date) -> String {
        guard let ms, ms > 0 else {
            return "-"
        }
        return AccessoryFaultsPage.ago(ms, now: now)
    }

    private static func milliseconds(_ ms: Int64?) -> String {
        guard let ms, ms > 0 else {
            return "-"
        }
        return "\(ms) ms"
    }

    static func dash(_ text: String) -> String {
        text.isEmpty ? "-" : text
    }

    static func text(_ value: MirrorValue?) -> String? {
        if case .text(let text)? = value, !text.isEmpty {
            return text
        }
        return nil
    }

    static func whole(_ value: MirrorValue?) -> Int64? {
        switch value {
        case .int(let number)?, .enumeration(let number)?:
            return number
        default:
            return nil
        }
    }
}
