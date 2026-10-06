// NereusSDR for iOS: the accessories' own settings and connections, changed through the Core's verbs and station settings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMirror

/// What a remote window changes of each accessory (remote accessory
/// control document, Commands and Window behaviour), so the phone does it
/// too: the device's name, hardware and network, Save & Reboot and Revert;
/// the Core's address for it, Connect, Disconnect and its LAN scan; the
/// station's 4O3A and RF-Kit switches; the Power Genius's connection
/// settings and output limit; the Tuner Genius's bypass and relays; the
/// RF-Kit's error reset and TCI mode. Antenna names, the RF-Kit's
/// connection settings and the tune memory recall are station settings the
/// Core keeps (`TGXL_Ant<N>_Label`, `RfKit_Ant<N>_Label`,
/// `RfKit_AutoReconnect`, `RfKit_PollIntervalMs`,
/// `TGXL_AutoTuneMemoryRecall`, `TGXL_TuneMemory_Ant<N>_Band<band>`),
/// written through the settings proxy; the
/// Core publishes them back on `accessoryData`.
///
/// Each is gated as the Core gates it: the Core's version first, then the
/// Core's connection to the device where the device must answer, then the
/// radio off the air where it switches relays. A refusal shows the Core's
/// words on the device's page.
extension AccessoriesModel {
    /// A device the Core heard on its LAN scan.
    struct ScannedDevice: Equatable, Identifiable {
        let address: String
        let port: Int64
        let model: String
        let serial: String
        let nickname: String

        var id: String { "\(address):\(port)" }
    }

    /// A setting this Core can't change from a phone.
    static let olderCoreSettingText = "This Core can't change this from a phone. Updating the Core may help."

    static let pgxlAutoReconnectKey = "PGXL_AutoReconnect"
    static let pgxlKeepaliveKey = "PGXL_KeepaliveSec"
    static let pgxlPingKey = "PGXL_PingSec"
    static let rfKitAutoReconnectKey = "RfKit_AutoReconnect"
    static let rfKitPollKey = "RfKit_PollIntervalMs"
    static let autoRecallKey = "TGXL_AutoTuneMemoryRecall"

    static func labelKey(_ device: Device, port: Int) -> String {
        device == .rfKit ? "RfKit_Ant\(port)_Label" : "TGXL_Ant\(port)_Label"
    }

    static func isAccessorySetting(_ key: String) -> Bool {
        key.hasPrefix("PGXL_") || key.hasPrefix("TGXL_") || key.hasPrefix("RfKit_")
    }

    // MARK: Gates

    /// Why the device's own settings (name, hardware, network, Save & Reboot,
    /// Revert) can't be changed now, or nil.
    func deviceSettingsReason(_ device: Device) -> String? {
        switch device {
        case .powerGenius:
            if versions.powerGenius < 3 {
                return Self.olderCoreSettingText
            }
            return powerGenius?.link.connected == true ? nil : Self.ampNotConnectedReason
        case .tunerGenius:
            if versions.tunerGenius < 1 {
                return Self.olderCoreSettingText
            }
            return tunerGenius?.link.connected == true ? nil : Self.tunerNotConnectedReason
        case .rfKit:
            return nil
        }
    }

    /// Why the Core's address for the device, Connect and Disconnect can't be used, or nil.
    func connectionReason(_ device: Device) -> String? {
        switch device {
        case .powerGenius:
            return versions.powerGenius >= 2 ? nil : Self.olderCoreSettingText
        case .tunerGenius:
            return versions.tunerConnection >= 1 ? nil : Self.olderCoreSettingText
        case .rfKit:
            return versions.rfKit >= 2 ? nil : Self.olderCoreSettingText
        }
    }

    /// Why the address can be saved without Connect, or scanned for; nil when it can.
    func savedAddressReason(_ device: Device) -> String? {
        switch device {
        case .powerGenius:
            return versions.powerGenius >= 4 ? nil : Self.olderCoreSettingText
        case .tunerGenius:
            return versions.tunerGenius >= 4 ? nil : Self.olderCoreSettingText
        case .rfKit:
            return versions.rfKit >= 4 ? nil : Self.olderCoreSettingText
        }
    }

    /// Why the Core's records (output limit, antenna names, recall) can't change, or nil.
    var recordsReason: String? {
        versions.records >= 1 ? nil : Self.noRecordsText
    }

    /// Why the tuner's relays can't be nudged, or nil.
    var relayReason: String? {
        if versions.tunerGenius < 4 {
            return Self.olderCoreSettingText
        }
        return switchReason(.tunerGenius)
    }

    /// Why the RF-Kit's error reset or TCI mode can't be used, or nil.
    func rfKitActionReason(tciMode: Bool) -> String? {
        if versions.rfKit < (tciMode ? 4 : 3) {
            return Self.olderCoreSettingText
        }
        if rfKit?.link.connected != true {
            return Self.rfKitNotConnectedReason
        }
        return tciMode && onAir ? Self.onAirReason : nil
    }

    // MARK: The device's own settings

    func setName(_ device: Device, _ name: String) {
        let verb = device == .powerGenius ? "setPgxlName" : "setTgxlName"
        guard device != .rfKit, allowed(device, deviceSettingsReason(device)) else {
            return
        }
        send(device, verb, [CommandArgument(name: "name", value: .text(name))])
    }

    /// One Power Genius hardware setting: `biasMode`, `fanMode` or `ledIntensity`.
    func setPgxlHardware(_ name: String, _ value: MirrorValue) {
        guard allowed(.powerGenius, deviceSettingsReason(.powerGenius)) else {
            return
        }
        send(.powerGenius, "setPgxlHardware", [CommandArgument(name: name, value: value)])
    }

    func setNetwork(_ device: Device, dhcp: Bool, address: String, netmask: String, gateway: String) {
        guard device != .rfKit, allowed(device, deviceSettingsReason(device)) else {
            return
        }
        send(device, device == .powerGenius ? "setPgxlNetwork" : "setTgxlNetwork",
             [CommandArgument(name: "dhcp", value: .bool(dhcp)), CommandArgument(name: "address", value: .text(address)),
              CommandArgument(name: "netmask", value: .text(netmask)),
              CommandArgument(name: "gateway", value: .text(gateway))])
    }

    /// Save & Reboot: the device keeps its settings and restarts.
    func saveDeviceSettings(_ device: Device) {
        guard device != .rfKit, allowed(device, deviceSettingsReason(device)) else {
            return
        }
        send(device, device == .powerGenius ? "savePgxlSettings" : "saveTgxlSettings", [])
    }

    /// Revert: asks the device for its settings again.
    func revertDeviceSettings(_ device: Device) {
        guard device != .rfKit, allowed(device, deviceSettingsReason(device)) else {
            return
        }
        send(device, device == .powerGenius ? "readPgxlSettings" : "readTgxlSettings", [])
    }

    /// The Power Genius's output limit (`setPgxlPowerCap`, 100 to 2000 W).
    func setPowerCap(enabled: Bool, watts: Int64) {
        guard allowed(.powerGenius, recordsReason) else {
            return
        }
        send(.powerGenius, "setPgxlPowerCap", [CommandArgument(name: "enabled", value: .bool(enabled)),
                                               CommandArgument(name: "watts", value: .int(watts))])
    }

    // MARK: The Core's connection to the device

    private static func verb(_ device: Device, pgxl: String, tgxl: String, rfKit: String) -> String {
        switch device {
        case .powerGenius:
            return pgxl
        case .tunerGenius:
            return tgxl
        case .rfKit:
            return rfKit
        }
    }

    private static func address(_ host: String, _ port: Int64) -> [CommandArgument] {
        [CommandArgument(name: "host", value: .text(host)), CommandArgument(name: "port", value: .int(port))]
    }

    /// Saves the Core's address for the device without dialling it.
    func saveAddress(_ device: Device, host: String, port: Int64) {
        guard allowed(device, savedAddressReason(device)) else {
            return
        }
        send(device, Self.verb(device, pgxl: "setPgxlAddress", tgxl: "setTgxlAddress", rfKit: "setRfKitAddress"),
             Self.address(host, port))
    }

    /// Saves the address and has the Core connect to it.
    func connect(_ device: Device, host: String, port: Int64) {
        guard allowed(device, connectionReason(device)) else {
            return
        }
        send(device, Self.verb(device, pgxl: "configurePgxl", tgxl: "configureTgxl", rfKit: "configureRfKit"),
             Self.address(host, port))
    }

    func disconnect(_ device: Device) {
        guard allowed(device, connectionReason(device)) else {
            return
        }
        send(device, Self.verb(device, pgxl: "disconnectPgxl", tgxl: "disconnectTgxl", rfKit: "disconnectRfKit"), [])
    }

    /// The Core listens for the device on its network for three seconds.
    func scan(_ device: Device) {
        guard device != .rfKit, allowed(device, savedAddressReason(device)), let commands else {
            return
        }
        let verb = device == .powerGenius ? "scanPgxlLan" : "scanTgxlLan"
        scanning.insert(device)
        Task { [weak self] in
            defer { self?.scanning.remove(device) }
            do {
                let result = try await commands.invoke(verb, arguments: [], timeout: .seconds(10))
                guard result.accepted else {
                    self?.notes[device] = result.reason.isEmpty ? nil : result.reason
                    return
                }
                var json = "[]"
                if case .text(let text)? = result.values["devicesJson"] {
                    json = text
                }
                self?.scans[device] = Self.scanned(json)
            } catch {
                Self.logger.info("A \(verb, privacy: .public) request had no answer from the Core")
            }
        }
    }

    /// The scan's answer: a JSON array of `{"address","port","model","serial","nickname"}`.
    static func scanned(_ json: String) -> [ScannedDevice] {
        guard let array = (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [[String: Any]] else {
            return []
        }
        return array.map { entry in
            ScannedDevice(address: entry["address"] as? String ?? "",
                          port: (entry["port"] as? NSNumber)?.int64Value ?? 0,
                          model: entry["model"] as? String ?? "", serial: entry["serial"] as? String ?? "",
                          nickname: entry["nickname"] as? String ?? "")
        }
    }

    /// The station's 4O3A switch: the Power Genius and the Tuner Genius.
    func setFourO3AEnabled(_ enabled: Bool, from device: Device) {
        guard allowed(device, versions.fourO3A >= 1 ? nil : Self.olderCoreSettingText) else {
            return
        }
        send(device, "setFourO3AEnabled", [CommandArgument(name: "enabled", value: .bool(enabled))])
    }

    /// The station's RF-Kit switch.
    func setRfKitEnabled(_ enabled: Bool) {
        guard allowed(.rfKit, connectionReason(.rfKit)) else {
            return
        }
        send(.rfKit, "setRfKitEnabled", [CommandArgument(name: "enabled", value: .bool(enabled))])
    }

    /// The Power Genius's connection settings: automatic retry, keepalive and ping.
    func setPgxlConnectionSettings(autoReconnect: Bool, keepaliveSec: Int64, pingSec: Int64) {
        guard allowed(.powerGenius, connectionReason(.powerGenius)) else {
            return
        }
        send(.powerGenius, "setPgxlConnectionSettings",
             [CommandArgument(name: "autoReconnect", value: .bool(autoReconnect)),
              CommandArgument(name: "keepaliveSec", value: .int(keepaliveSec)),
              CommandArgument(name: "pingSec", value: .int(pingSec))])
    }

    /// The Power Genius's connection settings as the Core keeps them.
    var pgxlConnectionSettings: (autoReconnect: Bool, keepaliveSec: Int64, pingSec: Int64) {
        (stationSettings[Self.pgxlAutoReconnectKey].map { $0 == "True" } ?? true,
         stationSettings[Self.pgxlKeepaliveKey].flatMap { Int64($0) } ?? 30,
         stationSettings[Self.pgxlPingKey].flatMap { Int64($0) } ?? 0)
    }

    // MARK: The Tuner Genius

    /// BYPASS on or off (`setTgxlBypass`).
    func setTunerBypass(_ on: Bool) {
        guard allowed(.tunerGenius, switchReason(.tunerGenius)) else {
            return
        }
        send(.tunerGenius, "setTgxlBypass", [CommandArgument(name: "on", value: .bool(on))])
    }

    /// Moves one matching relay one step: 0 C1, 1 L, 2 C2; `direction` -1 or 1.
    func moveTunerRelay(_ relay: Int64, direction: Int64) {
        guard allowed(.tunerGenius, relayReason) else {
            return
        }
        send(.tunerGenius, "moveTgxlRelay", [CommandArgument(name: "relay", value: .int(relay)),
                                             CommandArgument(name: "direction", value: .int(direction))])
    }

    // MARK: The RF2K-S

    func resetRfKitError() {
        guard allowed(.rfKit, rfKitActionReason(tciMode: false)) else {
            return
        }
        send(.rfKit, "resetRfKitError", [])
    }

    func setRfKitTciMode() {
        guard allowed(.rfKit, rfKitActionReason(tciMode: true)) else {
            return
        }
        send(.rfKit, "setRfKitTciMode", [])
    }

    // MARK: Station settings

    /// An antenna's name, 1 to 3 on the Tuner Genius and 1 to 4 on the RF2K-S; empty is "ANT N".
    func setAntennaLabel(_ device: Device, port: Int, _ label: String) {
        writeSetting(device, Self.labelKey(device, port: port), label.trimmingCharacters(in: .whitespaces),
                     reason: recordsReason)
    }

    func setRfKitAutoReconnect(_ on: Bool) {
        writeSetting(.rfKit, Self.rfKitAutoReconnectKey, on ? "True" : "False",
                     reason: versions.rfKit >= 3 ? nil : Self.olderCoreSettingText)
    }

    /// How often the Core asks the amp for its readings, 250 to 5000 ms.
    func setRfKitPollInterval(_ milliseconds: Int64) {
        writeSetting(.rfKit, Self.rfKitPollKey, String(min(max(milliseconds, 250), 5000)),
                     reason: versions.rfKit >= 3 ? nil : Self.olderCoreSettingText)
    }

    var rfKitConnectionSettings: (autoReconnect: Bool, pollMs: Int64) {
        (stationSettings[Self.rfKitAutoReconnectKey].map { $0 == "True" } ?? true,
         stationSettings[Self.rfKitPollKey].flatMap { Int64($0) } ?? 1000)
    }

    /// Whether the Core recalls a stored tune when the band or antenna changes.
    func setAutoRecall(_ on: Bool) {
        writeSetting(.tunerGenius, Self.autoRecallKey, on ? "True" : "False", reason: recordsReason)
    }

    /// The station setting that holds one stored tune, by its antenna and band.
    static func storedTuneKey(_ tune: StoredTune) -> String {
        "TGXL_TuneMemory_Ant\(tune.antenna)_Band\(tune.band)"
    }

    /// Forgets one stored tune at the Core: its setting emptied, as the
    /// desktop's Clear tune memory does.
    func clearStoredTune(_ tune: StoredTune) {
        writeSetting(.tunerGenius, Self.storedTuneKey(tune), "", reason: recordsReason)
    }

    private func writeSetting(_ device: Device, _ key: String, _ value: String, reason: String?) {
        guard allowed(device, reason) else {
            return
        }
        Task { [weak self] in
            await self?.writeSettingNow(device, key, value)
        }
    }

    /// Writes one station setting and notes the Core's answer, once it has answered.
    @discardableResult
    func writeSettingNow(_ device: Device, _ key: String, _ value: String) async -> SettingsWriteOutcome? {
        guard let settings else {
            return nil
        }
        let slot = device.rawValue
        let edit = settingOutcomeOwner.begin(slot)
        let outcome = await settings.write(key, value, onLateOutcome: { [weak self] outcome in
            self?.receiveSetting(outcome, device: device, edit: edit)
        })
        receiveSetting(outcome, device: device, edit: edit)
        return outcome
    }

    private func receiveSetting(_ outcome: SettingsWriteOutcome, device: Device, edit: UInt64) {
        guard settingOutcomeOwner.isCurrent(edit, device.rawValue), !outcome.propertyOutcome.heldForQuestion else { return }
        switch outcome {
        case .accepted: notes[device] = nil
        case .rejected(let reason): if !reason.isEmpty { notes[device] = reason }
        case .notConfirmed: notes[device] = PropertyWriteOutcome.notConfirmed.reason
        case .linkLost: notes[device] = PropertyWriteOutcome.linkLost.reason
        case .keptOnThisDevice, .notSent:
            Self.logger.info("An accessory setting was not stored by the Core")
        }
    }
}
