// NereusSDR for iOS: the fields the accessory settings pages change: text with Set, choices, switches, steppers and buttons
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import SwiftUI

/// The accessory settings pages' controls. Each shows the Core's value and
/// sends only when the operator presses its visible button (Set, a choice,
/// a switch, a stepper's minus or plus); a field that can't change now is
/// disabled and its section says why.
enum AccessoryFields {
    /// The Power Genius's and Tuner Genius's name is one word, as the
    /// desktop's Advanced pages take it (PgxlAdvancedPage.cpp:393-399,
    /// TgxlAdvancedPage.cpp:464-470): the amp and tuner carry it as one
    /// field of a line where a space or "=" would start another, so neither
    /// can be typed. The Core refuses the same characters, and control
    /// characters with them (PgxlConnection.cpp:818-826,
    /// StationDeviceSettings.cpp:241-250); the field refuses all of them.
    static func refusedInName(_ character: Character) -> Bool {
        character.isWhitespace || character == "="
            || character.unicodeScalars.contains { $0.properties.generalCategory == .control }
    }

    /// The desktop's tooltip on its name field, read by VoiceOver here.
    static let nameTip = "One word: no spaces or equals signs."

    /// What the name field holds after an edit from `old` to `new`: the
    /// edit, or `old` when it would put in a character the name refuses.
    /// As the desktop's validator does, a refused keystroke or paste
    /// changes nothing.
    static func nameEdit(from old: String, to new: String) -> String {
        new.contains(where: refusedInName) ? old : new
    }

    /// A text value with Set beside it: Set sends the typed text, lit only once it differs.
    struct TextRow: View {
        let label: String
        let value: String
        var placeholder = ""
        var keyboard: UIKeyboardType = .default
        let enabled: Bool
        let identifier: String
        /// The edit the field keeps (`nameEdit` for a one-word name); nil keeps every edit.
        var edit: ((String, String) -> String)?
        /// VoiceOver's hint for the field.
        var hint = ""
        let set: (String) -> Void
        @State private var draft: String

        init(label: String, value: String, placeholder: String = "", keyboard: UIKeyboardType = .default,
             enabled: Bool, identifier: String, edit: ((String, String) -> String)? = nil, hint: String = "",
             set: @escaping (String) -> Void) {
            self.label = label
            self.value = value
            self.placeholder = placeholder
            self.keyboard = keyboard
            self.enabled = enabled
            self.identifier = identifier
            self.edit = edit
            self.hint = hint
            self.set = set
            _draft = State(initialValue: value)
        }

        var body: some View {
            HStack(spacing: 8) {
                Text(label)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .frame(width: 92, alignment: .leading)
                TextField(placeholder, text: $draft)
                    .font(.system(size: 13, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .keyboardType(keyboard)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()
                    .submitLabel(.done)
                    .padding(.horizontal, 8)
                    .frame(minHeight: 34)
                    .background(ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
                    .disabled(!enabled)
                    .accessibilityLabel(label)
                    .accessibilityHint(hint)
                    .accessibilityIdentifier(identifier)
                    .onChange(of: draft) { old, new in
                        // The Core's own value always shows; the rule is for edits.
                        if let edit, new != value {
                            let kept = edit(old, new)
                            if kept != new {
                                draft = kept
                            }
                        }
                    }
                SmallButton(title: "Set", enabled: enabled && draft != value) {
                    set(draft)
                }
                .accessibilityLabel("Set \(label)")
                .accessibilityIdentifier("\(identifier).set")
            }
            .onChange(of: value) { _, newValue in
                draft = newValue
            }
        }
    }

    /// A choice among a few words, as a menu button showing the current one.
    struct ChoiceRow: View {
        let label: String
        let value: String
        let options: [String]
        let enabled: Bool
        let identifier: String
        let pick: (String) -> Void

        var body: some View {
            HStack(spacing: 8) {
                Text(label)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                Spacer(minLength: 8)
                Menu {
                    ForEach(options, id: \.self) { option in
                        Button(option) {
                            pick(option)
                        }
                    }
                } label: {
                    HStack(spacing: 6) {
                        Text(value.isEmpty ? "-" : value)
                            .font(.system(size: 13, weight: .semibold, design: .monospaced))
                        Image(systemName: "chevron.up.chevron.down")
                            .font(.system(size: 10, weight: .semibold))
                    }
                    .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
                    .padding(.horizontal, 10)
                    .frame(minHeight: 34)
                    .background(enabled ? ChromeColours.button : ChromeColours.buttonOff,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(enabled ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder, lineWidth: 1))
                }
                .disabled(!enabled)
                .accessibilityLabel(label)
                .accessibilityValue(value)
                .accessibilityIdentifier(identifier)
            }
        }
    }

    /// An on or off setting.
    struct SwitchRow: View {
        let label: String
        let isOn: Bool
        let enabled: Bool
        let identifier: String
        let change: (Bool) -> Void

        var body: some View {
            Toggle(isOn: Binding(get: { isOn }, set: { change($0) })) {
                Text(label)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
            }
            .tint(ChromeColours.buttonOnGreen)
            .disabled(!enabled)
            .accessibilityIdentifier(identifier)
        }
    }

    /// A whole number the minus and plus change by `step` within `range`, each press sent.
    struct NumberRow: View {
        let label: String
        let value: Int64
        let range: ClosedRange<Int64>
        let step: Int64
        var unit = ""
        let enabled: Bool
        let identifier: String
        let change: (Int64) -> Void

        var body: some View {
            HStack(spacing: 8) {
                Text(label)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                Spacer(minLength: 8)
                SmallButton(title: "\u{2212}", enabled: enabled && value > range.lowerBound) {
                    change(max(value - step, range.lowerBound))
                }
                .accessibilityLabel("Less \(label)")
                .accessibilityIdentifier("\(identifier).down")
                Text(unit.isEmpty ? String(value) : "\(value) \(unit)")
                    .font(.system(size: 13, weight: .semibold, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .frame(minWidth: 72)
                    .accessibilityIdentifier(identifier)
                SmallButton(title: "+", enabled: enabled && value < range.upperBound) {
                    change(min(value + step, range.upperBound))
                }
                .accessibilityLabel("More \(label)")
                .accessibilityIdentifier("\(identifier).up")
            }
        }
    }

    /// A small bordered button.
    struct SmallButton: View {
        let title: String
        let enabled: Bool
        /// Fills the width it is given, for a row of two.
        var wide = false
        let action: () -> Void

        var body: some View {
            Button(action: action) {
                Text(title)
                    .font(.system(size: 13, weight: .bold))
                    .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
                    .padding(.horizontal, 8)
                    .frame(minWidth: 44, maxWidth: wide ? .infinity : nil, minHeight: 34)
                    .background(enabled ? ChromeColours.button : ChromeColours.buttonOff,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(enabled ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder, lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
        }
    }

    /// A full-width button; with `confirm`, it asks first.
    struct ActionButton: View {
        let title: String
        let enabled: Bool
        let identifier: String
        var confirm: (title: String, message: String)?
        let action: () -> Void
        @State private var asking = false

        var body: some View {
            Button {
                if confirm != nil {
                    asking = true
                } else {
                    action()
                }
            } label: {
                Text(title)
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(enabled ? ChromeColours.text : ChromeColours.buttonOffText)
                    .frame(maxWidth: .infinity, minHeight: 38)
                    .background(enabled ? ChromeColours.button : ChromeColours.buttonOff,
                                in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4)
                        .strokeBorder(enabled ? ChromeColours.buttonBorder : ChromeColours.buttonOffBorder, lineWidth: 1))
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!enabled)
            .accessibilityIdentifier(identifier)
            .confirmationDialog(confirm?.title ?? "", isPresented: $asking, titleVisibility: .visible) {
                Button(title, action: action)
            } message: {
                Text(confirm?.message ?? "")
            }
        }
    }

    /// The Core's address for a device: host and port, Save (without
    /// dialling, where the Core offers it), and Connect or Disconnect.
    struct AddressEditor: View {
        let host: String
        let port: Int64
        let connected: Bool
        let saveEnabled: Bool
        let connectEnabled: Bool
        let prefix: String
        let save: (String, Int64) -> Void
        let connect: (String, Int64) -> Void
        let disconnect: () -> Void
        @State private var draftHost: String
        @State private var draftPort: String

        init(host: String, port: Int64, connected: Bool, saveEnabled: Bool, connectEnabled: Bool, prefix: String,
             save: @escaping (String, Int64) -> Void, connect: @escaping (String, Int64) -> Void,
             disconnect: @escaping () -> Void) {
            self.host = host
            self.port = port
            self.connected = connected
            self.saveEnabled = saveEnabled
            self.connectEnabled = connectEnabled
            self.prefix = prefix
            self.save = save
            self.connect = connect
            self.disconnect = disconnect
            _draftHost = State(initialValue: host)
            _draftPort = State(initialValue: port > 0 ? String(port) : "")
        }

        var body: some View {
            VStack(alignment: .leading, spacing: 8) {
                field("Address", text: $draftHost, keyboard: .URL, identifier: "\(prefix).host")
                field("Port", text: $draftPort, keyboard: .numberPad, identifier: "\(prefix).port")
                HStack(spacing: 6) {
                    SmallButton(title: "Save", enabled: saveEnabled && changed, wide: true) {
                        save(draftHost.trimmingCharacters(in: .whitespaces), Int64(draftPort) ?? 0)
                    }
                    .accessibilityIdentifier("\(prefix).save")
                    if connected {
                        SmallButton(title: "Disconnect", enabled: connectEnabled, wide: true) {
                            disconnect()
                        }
                            .accessibilityIdentifier("\(prefix).disconnect")
                    } else {
                        SmallButton(title: "Connect", enabled: connectEnabled, wide: true) {
                            connect(draftHost.trimmingCharacters(in: .whitespaces), Int64(draftPort) ?? 0)
                        }
                            .accessibilityIdentifier("\(prefix).connect")
                    }
                }
            }
            .onChange(of: host) { _, newValue in
                draftHost = newValue
            }
            .onChange(of: port) { _, newValue in
                draftPort = newValue > 0 ? String(newValue) : ""
            }
        }

        private var changed: Bool {
            draftHost.trimmingCharacters(in: .whitespaces) != host || (Int64(draftPort) ?? 0) != port
        }

        private func field(_ label: String, text: Binding<String>, keyboard: UIKeyboardType,
                           identifier: String) -> some View {
            HStack(spacing: 8) {
                Text(label)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .frame(width: 92, alignment: .leading)
                TextField("", text: text)
                    .font(.system(size: 13, design: .monospaced))
                    .foregroundStyle(ChromeColours.text)
                    .keyboardType(keyboard)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()
                    .padding(.horizontal, 8)
                    .frame(minHeight: 34)
                    .background(ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
                    .disabled(!saveEnabled && !connectEnabled)
                    .accessibilityLabel(label)
                    .accessibilityIdentifier(identifier)
            }
        }
    }

    /// A device's network settings: DHCP or a fixed address, then Apply,
    /// which asks first (the device takes them at its next restart).
    struct NetworkEditor: View {
        let dhcp: Bool
        let address: String
        let netmask: String
        let gateway: String
        let enabled: Bool
        let prefix: String
        let deviceName: String
        let apply: (Bool, String, String, String) -> Void
        @State private var draftDhcp: Bool
        @State private var draftAddress: String
        @State private var draftNetmask: String
        @State private var draftGateway: String

        init(dhcp: Bool, address: String, netmask: String, gateway: String, enabled: Bool, prefix: String,
             deviceName: String, apply: @escaping (Bool, String, String, String) -> Void) {
            self.dhcp = dhcp
            self.address = address
            self.netmask = netmask
            self.gateway = gateway
            self.enabled = enabled
            self.prefix = prefix
            self.deviceName = deviceName
            self.apply = apply
            _draftDhcp = State(initialValue: dhcp)
            _draftAddress = State(initialValue: address)
            _draftNetmask = State(initialValue: netmask)
            _draftGateway = State(initialValue: gateway)
        }

        var body: some View {
            VStack(alignment: .leading, spacing: 8) {
                Toggle(isOn: $draftDhcp) {
                    Text("Use DHCP")
                        .font(.system(size: 12))
                        .foregroundStyle(ChromeColours.textDim)
                }
                .tint(ChromeColours.buttonOnGreen)
                .disabled(!enabled)
                .accessibilityIdentifier("\(prefix).dhcp")
                field("Address", $draftAddress, "\(prefix).address")
                field("Netmask", $draftNetmask, "\(prefix).netmask")
                field("Gateway", $draftGateway, "\(prefix).gateway")
                ActionButton(title: "Apply network settings", enabled: enabled && changed,
                             identifier: "\(prefix).apply",
                             confirm: ("Change the \(deviceName)'s network settings?",
                                       "It takes them when it restarts. Check the address before you save, or "
                                       + "the Core may not find it again.")) {
                    apply(draftDhcp, draftAddress, draftNetmask, draftGateway)
                }
            }
            .onChange(of: dhcp) { _, value in draftDhcp = value }
            .onChange(of: address) { _, value in draftAddress = value }
            .onChange(of: netmask) { _, value in draftNetmask = value }
            .onChange(of: gateway) { _, value in draftGateway = value }
        }

        private var changed: Bool {
            draftDhcp != dhcp || draftAddress != address || draftNetmask != netmask || draftGateway != gateway
        }

        private func field(_ label: String, _ text: Binding<String>, _ identifier: String) -> some View {
            HStack(spacing: 8) {
                Text(label)
                    .font(.system(size: 12))
                    .foregroundStyle(ChromeColours.textDim)
                    .frame(width: 92, alignment: .leading)
                TextField("", text: text)
                    .font(.system(size: 13, design: .monospaced))
                    .foregroundStyle(draftDhcp ? ChromeColours.textFaint : ChromeColours.text)
                    .keyboardType(.numbersAndPunctuation)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()
                    .padding(.horizontal, 8)
                    .frame(minHeight: 34)
                    .background(ChromeColours.button, in: RoundedRectangle(cornerRadius: 4))
                    .overlay(RoundedRectangle(cornerRadius: 4).strokeBorder(ChromeColours.buttonBorder, lineWidth: 1))
                    .disabled(!enabled || draftDhcp)
                    .accessibilityLabel(label)
                    .accessibilityIdentifier(identifier)
            }
        }
    }
}
