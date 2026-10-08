// NereusSDR for iOS: CAT Control's pages: the four channels, each channel's ways in, the shared options, PTT, tester and log
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// CAT Control on the phone: one row for each of the Core's four CAT
/// channels with its state, then the settings every channel shares (CAT
/// Options, CAT PTT) and the tester with the Core's CAT log. Each opens its
/// own page, as Spot Hub's rows do. Every control is visible; one that
/// cannot change now is greyed with its reason written under it.
struct CatControlPage: View {
    @ObservedObject var model: CatControlModel
    let open: (CatControlModel.Route) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            if let reason = model.reason {
                SpotHubPage.Card {
                    ToolPageParts.Reason(text: reason, identifier: "cat.reason")
                }
            }
            SpotHubPage.Heading(text: "Channels", tag: .core)
            SpotHubPage.Card {
                ForEach(Array(StationCat.channels), id: \.self) { number in
                    if number > StationCat.channels.lowerBound {
                        SpotHubPage.Line()
                    }
                    SpotHubPage.Row(title: "CAT \(number)", detail: model.summary(number), dot: dot(number),
                                    enabled: model.reason == nil, identifier: "cat.channel.\(number)") {
                        open(.channel(number))
                    }
                }
            }
            SpotHubPage.Heading(text: "Every channel", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                SpotHubPage.Row(title: "CAT Options", detail: "Identity, automatic information and RTTY offsets",
                                enabled: model.reason == nil, identifier: "cat.options") {
                    open(.options)
                }
                SpotHubPage.Line()
                SpotHubPage.Row(title: "CAT PTT", detail: model.pttState.isEmpty ? "Input PTT from serial pins"
                                    : "PTT: \(model.pttState)",
                                enabled: model.reason == nil, identifier: "cat.ptt") {
                    open(.ptt)
                }
                SpotHubPage.Line()
                SpotHubPage.Row(title: "Test and log", detail: "Send a command, and watch the Core's CAT traffic",
                                enabled: model.reason == nil, identifier: "cat.test") {
                    open(.test)
                }
            }
        }
    }

    /// Green while a channel is on and listening, amber while on otherwise, grey off.
    private func dot(_ number: Int) -> Color {
        guard model.reason == nil, model.isOn(number) else {
            return ConnectChrome.pillUnknown
        }
        return model.channel(number).status.state == "Listening" ? ConnectChrome.pillOn : ConnectChrome.pillStale
    }
}

/// The parts CAT Control's pages share.
@MainActor
enum CatParts {
    /// A choice as a menu button showing the current one. An entry that
    /// cannot be chosen is greyed in the menu, and its reason is written
    /// under the row.
    struct Menu: View {
        let label: String
        let choices: [CatControlModel.Choice]
        let selected: String
        let enabled: Bool
        let identifier: String
        let pick: (String) -> Void

        var body: some View {
            VStack(alignment: .leading, spacing: 0) {
                HStack(spacing: 8) {
                    Text(label)
                        .font(.system(size: 13))
                        .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.textDim)
                    Spacer(minLength: 8)
                    SwiftUI.Menu {
                        ForEach(choices) { choice in
                            Button {
                                pick(choice.value)
                            } label: {
                                if choice.value == selected {
                                    Label(choice.label, systemImage: "checkmark")
                                } else {
                                    Text(choice.label)
                                }
                            }
                            .disabled(!choice.enabled)
                        }
                    } label: {
                        HStack(spacing: 6) {
                            Text(shown)
                                .font(.system(size: 13, weight: .semibold, design: .monospaced))
                                .lineLimit(1)
                                .truncationMode(.middle)
                            Image(systemName: "chevron.up.chevron.down")
                                .font(.system(size: 10, weight: .semibold))
                        }
                        .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
                        .padding(.horizontal, 10)
                        .frame(minHeight: 34)
                        .background(enabled ? ChromeColours.button : ChromeColours.buttonOff,
                                    in: RoundedRectangle(cornerRadius: 4))
                        .overlay(RoundedRectangle(cornerRadius: 4)
                            .strokeBorder(enabled ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder,
                                          lineWidth: 1))
                    }
                    .disabled(!enabled)
                    .accessibilityLabel(label)
                    .accessibilityValue(shown)
                    .accessibilityIdentifier(identifier)
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 7)
                ForEach(reasons, id: \.self) { reason in
                    Text(reason)
                        .font(.system(size: 11))
                        .foregroundStyle(ChromeColours.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                        .padding(.horizontal, 10)
                        .padding(.bottom, 6)
                        .accessibilityIdentifier("\(identifier).limit")
                }
            }
        }

        private var shown: String {
            choices.first { $0.value == selected }?.label ?? (selected.isEmpty ? "None" : selected)
        }

        /// Each greyed entry's reason, once, while the menu can be used.
        private var reasons: [String] {
            guard enabled else {
                return []
            }
            var seen: [String] = []
            for reason in choices.compactMap(\.reason) where !seen.contains(reason) {
                seen.append(reason)
            }
            return seen
        }
    }

    /// A setting switched on and off.
    struct Flag: View {
        let title: String
        var detail: String?
        let isOn: Bool
        let enabled: Bool
        let identifier: String
        let toggle: () -> Void

        var body: some View {
            SpotHubPage.SettingRow(title: title, detail: detail) {
                SpotHubPage.OnOff(isOn: isOn, enabled: enabled, identifier: identifier, toggle: toggle)
            }
        }
    }

    /// A whole number opened on the value pad.
    struct Number: View {
        let title: String
        var detail: String?
        let value: Int64?
        let enabled: Bool
        let identifier: String
        let open: () -> Void

        var body: some View {
            SpotHubPage.SettingRow(title: title, detail: detail) {
                ValueField(text: value.map { "\($0)" } ?? "--", accessibility: title, disabled: !enabled, minWidth: 64,
                           open: open)
                    .accessibilityIdentifier(identifier)
            }
        }
    }

    /// Text with Set beside it.
    struct Field: View {
        let label: String
        let value: String
        var placeholder = ""
        let enabled: Bool
        let identifier: String
        let set: (String) -> Void

        var body: some View {
            AccessoryFields.TextRow(label: label, value: value, placeholder: placeholder, enabled: enabled,
                                    identifier: identifier, set: set)
                .padding(.horizontal, 10)
                .padding(.vertical, 7)
        }
    }

    /// A row of a few choices, the chosen one blue.
    struct Row: View {
        let label: String
        let options: [(id: Int64, label: String)]
        let selected: Int64?
        let enabled: Bool
        let identifier: String
        let pick: (Int64) -> Void

        var body: some View {
            VStack(alignment: .leading, spacing: 6) {
                SwiftUI.Text(label)
                    .font(.system(size: 13))
                    .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.textDim)
                ToolPageParts.Choices(options: options, selected: selected, enabled: enabled, identifier: identifier,
                                      pick: pick)
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 8)
        }
    }

    /// A line of small print under a card's rows.
    struct Note: View {
        let text: String
        var identifier = ""

        var body: some View {
            SwiftUI.Text(text)
                .font(.system(size: 12))
                .foregroundStyle(ChromeColours.textDim)
                .frame(maxWidth: .infinity, alignment: .leading)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.horizontal, 10)
                .padding(.vertical, 8)
                .accessibilityIdentifier(identifier)
        }
    }

    /// The open-to-the-network warning, as the desktop's page gives it.
    struct NetworkWarning: View {
        let identifier: String

        var body: some View {
            ConnectChrome.NoticeBox(tone: .warn, title: nil, text: CatControlModel.openToNetworkWarning)
                .padding(.horizontal, 10)
                .padding(.vertical, 8)
                .accessibilityIdentifier(identifier)
        }
    }

    static func baudChoices() -> [CatControlModel.Choice] {
        CatControlModel.baudRates.map { CatControlModel.Choice(value: "\($0)", label: "\($0)") }
    }

    static func numbers(_ values: [Int64]) -> [(id: Int64, label: String)] {
        values.map { (id: $0, label: "\($0)") }
    }
}

/// One channel's page: its slices, its TCP and Hamlib rigctld listeners,
/// its serial port and its virtual serial port, as the desktop's Serial
/// Ports and TCP/IP CAT pages give them for one channel.
struct CatChannelPage: View {
    @ObservedObject var model: CatControlModel
    let number: Int

    var body: some View {
        let channel = model.channel(number)
        let config = channel.config
        let live = model.reason == nil && channel.received
        let prefix = "cat.\(number)"
        VStack(alignment: .leading, spacing: 8) {
            if let reason = model.reason {
                SpotHubPage.Card {
                    ToolPageParts.Reason(text: reason, identifier: "\(prefix).reason")
                }
            }
            SpotHubPage.Heading(text: "Slices", tag: .core)
            SpotHubPage.Card {
                slice(.a, "VFO A", live: live)
                SpotHubPage.Line()
                slice(.b, "VFO B", live: live)
                SpotHubPage.Line()
                CatParts.Note(text: CatControlModel.slicesNote)
            }
            SpotHubPage.Heading(text: "TCP", tag: .core).padding(.top, 8)
            listener(.tcp, isOn: config.tcpEnabled, address: config.tcpBindAddress, port: config.tcpPort,
                     status: model.tcpStatus(number), live: live)
            SpotHubPage.Heading(text: "Hamlib rigctld", tag: .core).padding(.top, 8)
            listener(.rigctld, isOn: config.rigctldEnabled, address: config.rigctldBindAddress,
                     port: config.rigctldPort, status: model.rigctldStatus(number), live: live)
            SpotHubPage.Heading(text: "Serial port", tag: .core).padding(.top, 8)
            serial(config, live: live)
            SpotHubPage.Heading(text: "Virtual serial port", tag: .core).padding(.top, 8)
            pty(config, live: live)
            if let note = model.note(.channel(number)) {
                ToolPageParts.Refusal(text: note, identifier: "\(prefix).note").padding(.top, 4)
            }
        }
        .onAppear { model.setOpen(.channel(number), true) }
        .onDisappear { model.setOpen(.channel(number), false) }
    }

    private func slice(_ vfo: CatControlModel.Vfo, _ title: String, live: Bool) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            CatParts.Menu(label: title, choices: model.sliceChoices(number, vfo),
                          selected: model.sliceSelected(number, vfo), enabled: live,
                          identifier: "cat.\(number).slice\(vfo == .a ? "A" : "B")") { choice in
                model.pickSlice(number, vfo, choice)
            }
            if let problem = model.sliceProblem(number, vfo) {
                ToolPageParts.Reason(text: problem, identifier: "cat.\(number).slice\(vfo == .a ? "A" : "B").closed")
            }
        }
    }

    private func listener(_ listener: CatControlModel.Listener, isOn: Bool, address: String, port: Int64,
                          status: String, live: Bool) -> some View {
        let name = listener == .tcp ? "tcp" : "rigctld"
        return SpotHubPage.Card {
            CatParts.Flag(title: listener == .tcp ? "TCP CAT" : "Hamlib rigctld", detail: status, isOn: isOn,
                          enabled: live, identifier: "cat.\(number).\(name).enabled") {
                model.change(number) { config in
                    switch listener {
                    case .tcp: config.tcpEnabled.toggle()
                    case .rigctld: config.rigctldEnabled.toggle()
                    }
                }
            }
            SpotHubPage.Line()
            CatParts.Field(label: "Listen on", value: address, placeholder: "127.0.0.1", enabled: live,
                          identifier: "cat.\(number).\(name).address") { text in
                model.setAddress(number, listener, text)
            }
            SpotHubPage.Line()
            CatParts.Number(title: "Port", detail: "0 lets the Core's computer choose", value: live ? port : nil,
                            enabled: live, identifier: "cat.\(number).\(name).port") {
                model.openPortPad(number, listener)
            }
            if model.opensToNetwork(number, listener) {
                SpotHubPage.Line()
                CatParts.NetworkWarning(identifier: "cat.\(number).\(name).warning")
            }
        }
    }

    private func serial(_ config: StationCat.ChannelConfig, live: Bool) -> some View {
        let reason = model.serialReason
        let usable = live && reason == nil
        return SpotHubPage.Card {
            CatParts.Flag(title: "Serial CAT", detail: model.serialStatus(number), isOn: config.serialEnabled,
                          enabled: usable, identifier: "cat.\(number).serial.enabled") {
                model.change(number) { $0.serialEnabled.toggle() }
            }
            SpotHubPage.Line()
            CatParts.Menu(label: "Device", choices: model.deviceChoices(current: config.serialDevice),
                          selected: config.serialDevice, enabled: usable,
                          identifier: "cat.\(number).serial.device") { device in
                model.change(number) { $0.serialDevice = device }
            }
            CatParts.Field(label: "Or type a path", value: config.serialDevice, placeholder: "/dev/cu.usbserial",
                          enabled: usable, identifier: "cat.\(number).serial.path") { path in
                model.change(number) { $0.serialDevice = path.trimmingCharacters(in: .whitespaces) }
            }
            SpotHubPage.Line()
            CatParts.Menu(label: "Baud", choices: CatParts.baudChoices(), selected: "\(config.serialBaud)",
                          enabled: usable, identifier: "cat.\(number).serial.baud") { baud in
                if let rate = Int64(baud) {
                    model.change(number) { $0.serialBaud = rate }
                }
            }
            SpotHubPage.Line()
            CatParts.Menu(label: "Parity", choices: model.parityChoices, selected: config.serialParity,
                          enabled: usable, identifier: "cat.\(number).serial.parity") { parity in
                model.change(number) { $0.serialParity = parity }
            }
            SpotHubPage.Line()
            CatParts.Row(label: "Data bits", options: CatParts.numbers(CatControlModel.channelDataBits),
                         selected: config.serialDataBits, enabled: usable,
                         identifier: "cat.\(number).serial.dataBits") { bits in
                model.change(number) { $0.serialDataBits = bits }
            }
            SpotHubPage.Line()
            CatParts.Menu(label: "Stop bits", choices: model.stopBitChoices, selected: config.serialStopBits,
                          enabled: usable, identifier: "cat.\(number).serial.stopBits") { stop in
                model.change(number) { $0.serialStopBits = stop }
            }
            if let reason, model.reason == nil {
                SpotHubPage.Line()
                ToolPageParts.Reason(text: reason, identifier: "cat.\(number).serial.reason")
            }
        }
    }

    private func pty(_ config: StationCat.ChannelConfig, live: Bool) -> some View {
        let reason = model.ptyReason
        let usable = live && reason == nil
        return SpotHubPage.Card {
            CatParts.Flag(title: "PTY", detail: usable ? model.ptyStatus(number) : nil, isOn: config.ptyEnabled,
                          enabled: usable, identifier: "cat.\(number).pty.enabled") {
                model.change(number) { $0.ptyEnabled.toggle() }
            }
            SpotHubPage.Line()
            CatParts.Menu(label: "Commands", choices: model.dialectChoices(number), selected: config.ptyDialect,
                          enabled: usable, identifier: "cat.\(number).pty.dialect") { dialect in
                model.change(number) { $0.ptyDialect = dialect }
            }
            SpotHubPage.Line()
            if let reason, model.reason == nil {
                ToolPageParts.Reason(text: reason, identifier: "cat.\(number).pty.reason")
            } else {
                CatParts.Note(text: model.ptyDialectLine(number), identifier: "cat.\(number).pty.note")
            }
        }
    }
}

/// CAT Options: what the Core reports to CAT programs, the automatic
/// frequency information it sends and where, and the RTTY offsets, as the
/// desktop's CAT Options page gives them.
struct CatOptionsPage: View {
    @ObservedObject var model: CatControlModel

    var body: some View {
        let global = model.global
        let live = model.reason == nil && global != nil
        VStack(alignment: .leading, spacing: 8) {
            if let reason = model.reason {
                SpotHubPage.Card {
                    ToolPageParts.Reason(text: reason, identifier: "cat.options.reason")
                }
            }
            SpotHubPage.Heading(text: "Compatibility and automatic information", tag: .core)
            SpotHubPage.Card {
                CatParts.Menu(label: "Report identity",
                              choices: CatControlModel.rigIdentities.map { .init(value: $0, label: $0) },
                              selected: global?.rigIdentity ?? "", enabled: live,
                              identifier: "cat.options.identity") { identity in
                    model.changeGlobal { $0.rigIdentity = identity }
                }
                SpotHubPage.Line()
                CatParts.Field(label: "ZZSN serial number", value: global?.serialNumber ?? "", enabled: live,
                              identifier: "cat.options.serialNumber") { text in
                    model.changeGlobal { $0.serialNumber = text }
                }
                ForEach(Self.flags, id: \.identifier) { flag in
                    SpotHubPage.Line()
                    CatParts.Flag(title: flag.title, isOn: global?[keyPath: flag.path] ?? false, enabled: live,
                                  identifier: flag.identifier) {
                        model.changeGlobal { $0[keyPath: flag.path].toggle() }
                    }
                }
                SpotHubPage.Line()
                CatParts.Flag(title: "Always recenter VFOs", detail: CatControlModel.recenterNote, isOn: global?.recenterVfo ?? false,
                              enabled: false, identifier: "cat.options.recenter") {}
            }
            SpotHubPage.Heading(text: "RTTY frequency reporting", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                CatParts.Flag(title: "Apply offset to VFO A", isOn: global?.rttyOffsetAEnabled ?? false, enabled: live,
                              identifier: "cat.options.rttyA") {
                    model.changeGlobal { $0.rttyOffsetAEnabled.toggle() }
                }
                SpotHubPage.Line()
                CatParts.Flag(title: "Apply offset to VFO B", isOn: global?.rttyOffsetBEnabled ?? false, enabled: live,
                              identifier: "cat.options.rttyB") {
                    model.changeGlobal { $0.rttyOffsetBEnabled.toggle() }
                }
                SpotHubPage.Line()
                CatParts.Number(title: "DIGU offset (Hz)", value: global?.rttyDiguHz, enabled: live,
                                identifier: "cat.options.digu") {
                    model.openRttyPad(digu: true)
                }
                SpotHubPage.Line()
                CatParts.Number(title: "DIGL offset (Hz)", value: global?.rttyDiglHz, enabled: live,
                                identifier: "cat.options.digl") {
                    model.openRttyPad(digu: false)
                }
            }
            if let note = model.note(.global) {
                ToolPageParts.Refusal(text: note, identifier: "cat.options.note").padding(.top, 4)
            }
        }
        .onAppear { model.setOpen(.options, true) }
        .onDisappear { model.setOpen(.options, false) }
    }

    struct FlagRow {
        let title: String
        let path: WritableKeyPath<StationCat.GlobalConfig, Bool>
        let identifier: String
    }

    /// The desktop's switches, in its order.
    static let flags: [FlagRow] = [
        FlagRow(title: "Send TCP welcome banner", path: \.sendWelcome, identifier: "cat.options.welcome"),
        FlagRow(title: "Allow Kenwood AI command", path: \.allowKenwoodAi, identifier: "cat.options.allowAi"),
        FlagRow(title: "Enable automatic frequency information", path: \.aiEnabled, identifier: "cat.options.ai"),
        FlagRow(title: "AI to TCP clients", path: \.aiTcp, identifier: "cat.options.aiTcp"),
        FlagRow(title: "AI to serial CAT 1", path: \.aiSerial1, identifier: "cat.options.aiSerial1"),
        FlagRow(title: "AI to serial CAT 2", path: \.aiSerial2, identifier: "cat.options.aiSerial2"),
        FlagRow(title: "AI to serial CAT 3", path: \.aiSerial3, identifier: "cat.options.aiSerial3"),
        FlagRow(title: "AI to serial CAT 4", path: \.aiSerial4, identifier: "cat.options.aiSerial4"),
        FlagRow(title: "Report DIGL / DIGU as LSB / USB", path: \.digitalReportsSideband,
                identifier: "cat.options.digitalSideband"),
        FlagRow(title: "Apply power limits to CAT power queries", path: \.limitReportedPower,
                identifier: "cat.options.limitPower"),
    ]
}

/// CAT PTT: transmit keyed from the pins of a serial port at the Core, as
/// the desktop's CAT PTT page gives it, with the PTT state.
struct CatPttPage: View {
    @ObservedObject var model: CatControlModel

    var body: some View {
        let global = model.global
        let reason = model.pttReason
        let live = reason == nil && global != nil
        VStack(alignment: .leading, spacing: 8) {
            if let reason = model.reason {
                SpotHubPage.Card {
                    ToolPageParts.Reason(text: reason, identifier: "cat.ptt.reason")
                }
            }
            SpotHubPage.Heading(text: "Input PTT", tag: .core)
            SpotHubPage.Card {
                ToolPageParts.Reading(title: "PTT", value: model.pttState.isEmpty ? "--" : model.pttState,
                                      identifier: "cat.ptt.state")
                SpotHubPage.Line()
                CatParts.Flag(title: "Enable input PTT", isOn: global?.pttEnabled ?? false, enabled: live,
                              identifier: "cat.ptt.enabled") {
                    model.changeGlobal { $0.pttEnabled.toggle() }
                }
                SpotHubPage.Line()
                CatParts.Menu(label: "Input source",
                              choices: CatControlModel.pttSources.map { .init(value: $0.value, label: $0.label) },
                              selected: global?.pttDeviceSource ?? "", enabled: live,
                              identifier: "cat.ptt.source") { source in
                    model.changeGlobal { $0.pttDeviceSource = source }
                }
                SpotHubPage.Line()
                CatParts.Flag(title: "Legacy RTS wiring", detail: "Samples the CTS input",
                              isOn: global?.pttUseCts ?? false, enabled: live, identifier: "cat.ptt.cts") {
                    model.changeGlobal { $0.pttUseCts.toggle() }
                }
                SpotHubPage.Line()
                CatParts.Flag(title: "Legacy DTR wiring", detail: "Samples the DSR input",
                              isOn: global?.pttUseDsr ?? false, enabled: live, identifier: "cat.ptt.dsr") {
                    model.changeGlobal { $0.pttUseDsr.toggle() }
                }
            }
            SpotHubPage.Heading(text: "Separate physical device", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                let device = global?.pttSerialDevice ?? ""
                CatParts.Menu(label: "Device", choices: model.deviceChoices(current: device), selected: device,
                              enabled: live, identifier: "cat.ptt.device") { picked in
                    model.changeGlobal { $0.pttSerialDevice = picked }
                }
                CatParts.Field(label: "Or type a path", value: device, placeholder: "/dev/cu.usbserial", enabled: live,
                              identifier: "cat.ptt.path") { path in
                    model.changeGlobal { $0.pttSerialDevice = path.trimmingCharacters(in: .whitespaces) }
                }
                SpotHubPage.Line()
                CatParts.Row(label: "Physical device's CAT channel", options: CatParts.numbers(StationCat.channels.map { Int64($0) }),
                             selected: global?.pttChannel, enabled: live, identifier: "cat.ptt.channel") { channel in
                    model.changeGlobal { $0.pttChannel = channel }
                }
                SpotHubPage.Line()
                CatParts.Menu(label: "Baud", choices: CatParts.baudChoices(),
                              selected: global.map { "\($0.pttSerialBaud)" } ?? "", enabled: live,
                              identifier: "cat.ptt.baud") { baud in
                    if let rate = Int64(baud) {
                        model.changeGlobal { $0.pttSerialBaud = rate }
                    }
                }
                SpotHubPage.Line()
                CatParts.Menu(label: "Parity", choices: model.parityChoices, selected: global?.pttSerialParity ?? "",
                              enabled: live, identifier: "cat.ptt.parity") { parity in
                    model.changeGlobal { $0.pttSerialParity = parity }
                }
                SpotHubPage.Line()
                CatParts.Row(label: "Data bits", options: CatParts.numbers(CatControlModel.pttDataBits),
                             selected: global?.pttSerialDataBits, enabled: live,
                             identifier: "cat.ptt.dataBits") { bits in
                    model.changeGlobal { $0.pttSerialDataBits = bits }
                }
                SpotHubPage.Line()
                CatParts.Menu(label: "Stop bits", choices: model.stopBitChoices,
                              selected: global?.pttSerialStopBits ?? "", enabled: live,
                              identifier: "cat.ptt.stopBits") { stop in
                    model.changeGlobal { $0.pttSerialStopBits = stop }
                }
                SpotHubPage.Line()
                if let reason, model.reason == nil {
                    ToolPageParts.Reason(text: reason, identifier: "cat.ptt.serialReason")
                } else {
                    CatParts.Note(text: CatControlModel.pttNote, identifier: "cat.ptt.note")
                }
            }
            if let note = model.note(.global) {
                ToolPageParts.Refusal(text: note, identifier: "cat.ptt.refusal").padding(.top, 4)
            }
        }
        .onAppear { model.setOpen(.ptt, true) }
        .onDisappear { model.setOpen(.ptt, false) }
    }
}

/// Test and log: the CAT Tester, which runs one command on a channel at
/// the Core and shows its reply, and the Core's CAT log while the page is open.
struct CatTestPage: View {
    @ObservedObject var model: CatControlModel
    @State private var command = "ID;"

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            if let reason = model.reason {
                SpotHubPage.Card {
                    ToolPageParts.Reason(text: reason, identifier: "cat.test.reason")
                }
            }
            SpotHubPage.Heading(text: "CAT Tester", tag: .core)
            SpotHubPage.Card {
                CatParts.Row(label: "Channel", options: (1...4).map { (id: Int64($0), label: "CAT \($0)") },
                             selected: Int64(model.testChannel), enabled: model.reason == nil,
                             identifier: "cat.test.channel") { channel in
                    model.testChannel = Int(channel)
                }
                SpotHubPage.Line()
                commandLine
                ForEach(model.tests) { run in
                    SpotHubPage.Line()
                    testRow(run)
                }
                SpotHubPage.Line()
                CatParts.Note(text: CatControlModel.testerNote, identifier: "cat.test.note")
            }
            SpotHubPage.Heading(text: "CAT log", tag: .core).padding(.top, 8)
            SpotHubPage.Card {
                if let reason = model.logReason {
                    ToolPageParts.Reason(text: reason, identifier: "cat.log.reason")
                } else {
                    logControls
                    console
                        .padding(.horizontal, 10)
                        .padding(.bottom, 10)
                    if model.paused {
                        CatParts.Note(text: CatControlModel.pausedNote, identifier: "cat.log.paused")
                    }
                }
            }
        }
        .onAppear { model.setOpen(.test, true) }
        .onDisappear { model.setOpen(.test, false) }
    }

    private var commandLine: some View {
        let ready = model.reason == nil && !command.trimmingCharacters(in: .whitespaces).isEmpty
        return HStack(spacing: 8) {
            TextField("Command", text: $command)
                .font(.system(size: 13, design: .monospaced))
                .foregroundStyle(ChromeColours.text)
                .textInputAutocapitalization(.characters)
                .autocorrectionDisabled()
                .submitLabel(.send)
                .onSubmit(send)
                .padding(.horizontal, 8)
                .frame(minHeight: 38)
                .background(ChromeColours.inset, in: RoundedRectangle(cornerRadius: 3))
                .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(ChromeColours.panelEdge, lineWidth: 1))
                .disabled(model.reason != nil)
                .accessibilityLabel("Command")
                .accessibilityIdentifier("cat.test.command")
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
            .accessibilityIdentifier("cat.test.send")
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
    }

    private func send() {
        model.test(command)
    }

    private func testRow(_ run: CatControlModel.TestRun) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 8) {
            Text("CAT \(run.channel)")
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(ChromeColours.textDim)
            Text(CatControlModel.escaped(run.command))
                .font(.system(size: 12, design: .monospaced))
                .foregroundStyle(ChromeColours.text)
            Image(systemName: "arrow.right")
                .font(.system(size: 10, weight: .semibold))
                .foregroundStyle(ChromeColours.textFaint)
            // The radio's reply in its own type; this phone's words, and
            // the Core's for a refusal, as notes.
            Text(run.answer ?? "Waiting for the Core")
                .font(.system(size: 12, design: run.answer != nil && run.kind == .reply ? .monospaced : .default))
                .foregroundStyle(run.answer == nil ? ChromeColours.textFaint
                                 : run.kind == .refused ? ChromeColours.buttonOnAmberText
                                 : run.kind == .note ? ChromeColours.textDim : ChromeColours.textBright)
                .fixedSize(horizontal: false, vertical: true)
                .frame(maxWidth: .infinity, alignment: .leading)
                .textSelection(.enabled)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("cat.test.run.\(run.id)")
    }

    private var logControls: some View {
        VStack(alignment: .leading, spacing: 8) {
            ToolPageParts.Choices(options: CatControlModel.LogFilter.allCases.map { (id: $0.rawValue, label: $0.label) },
                                  selected: model.logFilter.rawValue, identifier: "cat.log.filter") { picked in
                if let filter = CatControlModel.LogFilter(rawValue: picked) {
                    model.setLogFilter(filter)
                }
            }
            HStack(spacing: 8) {
                AccessoryFields.SmallButton(title: model.paused ? "Resume" : "Pause", enabled: true) {
                    model.setPaused(!model.paused)
                }
                .accessibilityIdentifier("cat.log.pause")
                AccessoryFields.SmallButton(title: "Clear", enabled: true) {
                    model.clearLog()
                }
                .accessibilityIdentifier("cat.log.clear")
                Spacer(minLength: 8)
                Text("Follow newest")
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                SpotHubPage.OnOff(isOn: model.followNewest, identifier: "cat.log.follow") {
                    model.followNewest.toggle()
                }
            }
            HStack(spacing: 8) {
                Spacer(minLength: 8)
                Text(CatControlModel.showBytesTitle)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                SpotHubPage.OnOff(isOn: model.showBytes, identifier: "cat.log.bytes") {
                    model.showBytes.toggle()
                }
            }
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 8)
    }

    /// The log as the desktop's window shows it, each line wrapped to the
    /// width; only the lines in view are drawn, so a full log of the
    /// Core's lines scrolls as easily as a short one.
    private var console: some View {
        let lines = model.logLines
        let bytes = model.showBytes
        return ScrollViewReader { reader in
            ScrollView(.vertical) {
                LazyVStack(alignment: .leading, spacing: 3) {
                    if lines.isEmpty {
                        Text(CatControlModel.noLinesText)
                            .foregroundStyle(ChromeColours.textFaint)
                    }
                    ForEach(lines) { line in
                        LogRow(line: line, bytes: bytes)
                            .id(line.id)
                    }
                }
                .font(.system(size: 10.5, design: .monospaced))
                .foregroundStyle(SpotColours.consoleText)
                .padding(10)
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .frame(height: 220)
            .background(SpotColours.console, in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(SpotColours.consoleBorder, lineWidth: 1))
            .onAppear {
                if let target = model.scrollTarget {
                    reader.scrollTo(target, anchor: .bottomLeading)
                }
            }
            .onChange(of: model.scrollTarget) { _, target in
                if let target {
                    reader.scrollTo(target, anchor: .bottomLeading)
                }
            }
            .accessibilityIdentifier("cat.log.console")
        }
    }

    /// One line of the log: when, then the line; the Core's traffic in its
    /// colours by way, this phone's diagnostics dimmer.
    private struct LogRow: View {
        let line: CatControlModel.LogEntry
        let bytes: Bool

        var body: some View {
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                Text(line.time)
                    .foregroundStyle(ChromeColours.textFaint)
                Text(CatControlModel.lineText(line, bytes: bytes))
                    .foregroundStyle(line.kind == .received ? SpotColours.consoleText
                                     : line.kind == .sent ? ChromeColours.textBright : ChromeColours.textDim)
                    .fixedSize(horizontal: false, vertical: true)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
    }
}
