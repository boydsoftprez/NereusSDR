// NereusSDR for iOS: Setup, CAT & Network, Rotor: the rotor's connection, the rotor itself and its presets
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import SwiftUI

/// Setup, CAT & Network, Rotor (JJ 2026-10-08: the setup lives where the
/// desktop keeps it): the desktop Rotor Setup page's three groups in its
/// order, Connection, Rotor and Presets, set up at the Core through the
/// rotor commands. While the page is on screen it asks the Core to keep
/// reading its serial ports (`refreshRotorPorts`). With no Core, a Core
/// too old or no rotor object, every control stays shown and greyed with
/// the reason.
struct RotorSetupPage: View {
    @ObservedObject var model: RotorModel

    var body: some View {
        RotorSetupCard(model: model)
            .navigationTitle("Rotor")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    SetupTagBadge(tag: .core)
                }
            }
            .onAppear { model.setupShown() }
            .onDisappear { model.setupHidden() }
    }
}

/// The rotor setup (design, Desktop and iPhone: rotor setup), as Setup's
/// Rotor page lists it: the driver, the Core's serial port and baud rate,
/// or rotctld's address, Hamlib's model, the axes, the end stop and range,
/// and the calibration offset; then the presets. Save and connect sends it
/// to the Core, which saves it and connects; Disconnect keeps it. The
/// serial ports are the Core's own, as it lists them.
struct RotorSetupCard: View {
    @ObservedObject var model: RotorModel
    @State private var draft = RotorModel.Setup()
    @State private var reported: RotorModel.Setup?
    @State private var portText = ""
    @State private var modelText = ""
    @State private var presets = RotorPresetsDraft()
    @State private var editingPresets = false

    static let bauds: [Int64] = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200]

    var body: some View {
        let reason = model.setupReason
        let enabled = reason == nil
        List {
            if let reason {
                Section {
                    Text(reason)
                        .accessibilityIdentifier("rotor.setup.reason")
                }
            }
            if let note = model.note {
                Section {
                    Button {
                        model.dismissNote()
                    } label: {
                        HStack(alignment: .top, spacing: 8) {
                            Text(note)
                                .foregroundStyle(ChromeColours.gaugeRed)
                                .frame(maxWidth: .infinity, alignment: .leading)
                            Image(systemName: "xmark")
                                .foregroundStyle(.secondary)
                        }
                    }
                    .buttonStyle(.plain)
                    .accessibilityHint("Clears this message")
                    .accessibilityIdentifier("rotor.setup.note")
                }
            }
            connection(enabled: enabled)
            rotor(enabled: enabled)
            save(enabled: enabled, reason: reason)
            presetsSection(enabled: enabled)
        }
        .accessibilityIdentifier("rotor.setup")
        .onAppear {
            take(model.state)
            presets.follow(model.presets)
        }
        .onChange(of: model.state) { _, state in
            take(state)
            presets.follow(state?.presets ?? [])
        }
    }

    static let noPortsText = "The Core's computer lists no serial ports. Plug in the rotor's interface; the list follows within a few seconds."
    static let noRotctldText = "Hamlib's rotctld is not installed on the Core's computer."
    static let presetsNote = "Headings the Rotor page offers as one-tap buttons, 0 to 360 degrees."

    // MARK: Connection

    @ViewBuilder
    private func connection(enabled: Bool) -> some View {
        let ports = model.state?.serialPorts ?? []
        Section {
            Picker("Controller", selection: Binding(get: { draft.driver }, set: { draft.driver = $0 })) {
                ForEach(RotorModel.Driver.allCases.filter { $0 != RotorModel.Driver.none }, id: \.self) { driver in
                    Text(driver.title).tag(driver)
                }
            }
            .pickerStyle(.menu)
            .disabled(!enabled)
            .accessibilityIdentifier("rotor.setup.driver")
            if draft.driver.usesSerialPort {
                Picker("Serial port", selection: Binding(get: { draft.serialPort }, set: { draft.serialPort = $0 })) {
                    ForEach(Self.portChoices(ports, current: draft.serialPort), id: \.self) { port in
                        Text(port.isEmpty ? "-" : port).tag(port)
                    }
                }
                .pickerStyle(.menu)
                .disabled(!enabled || ports.isEmpty)
                .accessibilityIdentifier("rotor.setup.serialPort")
                Picker("Baud", selection: Binding(get: { draft.baud }, set: { draft.baud = $0 })) {
                    ForEach(Self.baudChoices(current: draft.baud), id: \.self) { baud in
                        Text(String(baud)).tag(baud)
                    }
                }
                .pickerStyle(.menu)
                .disabled(!enabled)
                .accessibilityIdentifier("rotor.setup.baud")
            }
            if draft.driver == .rotctldRunning {
                field("Host", text: $draft.host, keyboard: .URL, enabled: enabled, identifier: "rotor.setup.host")
                field("Port", text: $portText, keyboard: .numberPad, enabled: enabled, identifier: "rotor.setup.port")
            }
            if draft.driver == .rotctldStarted {
                field("Hamlib model", text: $modelText, keyboard: .numberPad, enabled: enabled,
                      identifier: "rotor.setup.hamlibModel")
            }
        } header: {
            Text("Connection")
        } footer: {
            if draft.driver.usesSerialPort, ports.isEmpty {
                Text(Self.noPortsText)
            } else if draft.driver == .rotctldStarted, model.state?.rotctldAvailable == false {
                Text(Self.noRotctldText)
            }
        }
    }

    /// The Core's ports, and the saved one when the Core no longer lists it.
    static func portChoices(_ ports: [String], current: String) -> [String] {
        if ports.contains(current) {
            return ports
        }
        return ports.isEmpty && current.isEmpty ? [""] : ports + (current.isEmpty ? [] : [current])
    }

    /// The baud rates, and the Core's own when it is not among them.
    static func baudChoices(current: Int64) -> [Int64] {
        bauds.contains(current) ? bauds : (bauds + [current]).sorted()
    }

    // MARK: Rotor

    private func rotor(enabled: Bool) -> some View {
        Section {
            Picker("Turns in", selection: Binding(get: { draft.axes }, set: { draft.axes = $0 })) {
                ForEach(RotorModel.Axes.allCases, id: \.self) { axes in
                    Text(axes.title).tag(axes)
                }
            }
            .pickerStyle(.menu)
            .disabled(!enabled)
            .accessibilityIdentifier("rotor.setup.axes")
            Picker("End stop", selection: Binding(get: { draft.endStop }, set: { draft.endStop = $0 })) {
                ForEach(RotorModel.EndStop.allCases, id: \.self) { stop in
                    Text(stop.title).tag(stop)
                }
            }
            .pickerStyle(.menu)
            .disabled(!enabled)
            .accessibilityIdentifier("rotor.setup.endStop")
            Picker("Travel", selection: Binding(get: { draft.rangeDeg }, set: { draft.rangeDeg = $0 })) {
                Text("360\u{00B0}").tag(Int64(360))
                Text("450\u{00B0}").tag(Int64(450))
            }
            .pickerStyle(.menu)
            .disabled(!enabled)
            .accessibilityIdentifier("rotor.setup.range")
            SetupNumberRow(title: "Heading offset", value: draft.offsetDeg.rounded(), range: -180...180, step: 1,
                           unit: "\u{00B0}", enabled: enabled, id: "rotor.setup.offset") { draft.offsetDeg = $0 }
        } header: {
            Text("Rotor")
        }
    }

    // MARK: Save and connect

    private func save(enabled: Bool, reason: String?) -> some View {
        Section {
            Button("Save and connect") {
                var setup = draft
                setup.port = Int64(portText) ?? setup.port
                setup.hamlibModel = Int64(modelText) ?? setup.hamlibModel
                model.configure(setup)
            }
            .disabled(!enabled || !changedOrIdle)
            .accessibilityIdentifier("rotor.setup.save")
            Button("Disconnect") {
                model.disconnect()
            }
            .disabled(!enabled || model.state?.driver == RotorModel.Driver.none)
            .accessibilityIdentifier("rotor.setup.disconnect")
        } footer: {
            if reason == nil, let error = model.state?.connectionError, !error.isEmpty {
                Text(error)
                    .accessibilityIdentifier("rotor.setup.error")
            }
        }
    }

    /// Save is lit once the setup differs from the Core's, or when no rotor is set up yet.
    private var changedOrIdle: Bool {
        guard let state = model.state, state.driver != .none, let reported else {
            return true
        }
        var current = draft
        current.port = Int64(portText) ?? current.port
        current.hamlibModel = Int64(modelText) ?? current.hamlibModel
        return current != reported
    }

    /// Follows the Core's setup when it changes, keeping an edit in progress otherwise.
    private func take(_ state: RotorModel.State?) {
        let next = state.map(RotorModel.Setup.init) ?? RotorModel.Setup()
        guard next != reported else {
            return
        }
        reported = next
        draft = next
        portText = String(next.port)
        modelText = String(next.hamlibModel)
    }

    private func field(_ label: String, text: Binding<String>, keyboard: UIKeyboardType, enabled: Bool,
                       identifier: String) -> some View {
        LabeledContent(label) {
            TextField(label, text: text)
                .keyboardType(keyboard)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .multilineTextAlignment(.trailing)
                .monospacedDigit()
                .disabled(!enabled)
                .accessibilityLabel(label)
                .accessibilityIdentifier(identifier)
        }
        .opacity(enabled ? 1 : SetupNumberRow.greyedOpacity)
    }

    // MARK: Presets

    @ViewBuilder
    private func presetsSection(enabled: Bool) -> some View {
        Section {
            ForEach(Array(presets.rows.enumerated()), id: \.element.id) { index, row in
                presetRow(row, index: index, enabled: enabled)
            }
            .onDelete { offsets in
                guard enabled else {
                    return
                }
                presets.remove(atOffsets: offsets)
            }
            .deleteDisabled(!enabled)
            HStack(spacing: 12) {
                Button("Add") {
                    presets.add()
                }
                .buttonStyle(.bordered)
                .disabled(!enabled)
                .accessibilityIdentifier("rotor.setup.presets.add")
                Button(editingPresets ? "Done" : "Edit") {
                    editingPresets.toggle()
                }
                .buttonStyle(.bordered)
                .disabled(!enabled || (presets.rows.isEmpty && !editingPresets))
                .accessibilityIdentifier("rotor.setup.presets.edit")
                Spacer(minLength: 8)
                Button("Save presets") {
                    savePresets()
                }
                .buttonStyle(.borderedProminent)
                .disabled(!enabled)
                .accessibilityIdentifier("rotor.setup.presets.save")
            }
        } header: {
            Text("Presets")
        } footer: {
            VStack(alignment: .leading, spacing: 4) {
                Text(Self.presetsNote)
                if let message = presets.message {
                    Text(message)
                        .foregroundStyle(ChromeColours.gaugeRed)
                        .accessibilityIdentifier("rotor.setup.presets.message")
                }
            }
        }
        .onChange(of: presets.rows.isEmpty) { _, empty in
            if empty {
                editingPresets = false
            }
        }
    }

    private func presetRow(_ row: RotorPresetsDraft.Row, index: Int, enabled: Bool) -> some View {
        HStack(spacing: 8) {
            if editingPresets {
                Button {
                    presets.remove(row.id)
                } label: {
                    Image(systemName: "minus.circle.fill")
                        .foregroundStyle(enabled ? ChromeColours.gaugeRed : Color.secondary)
                }
                .buttonStyle(.borderless)
                .disabled(!enabled)
                .accessibilityLabel("Remove \(row.name.isEmpty ? "this preset" : row.name)")
                .accessibilityIdentifier("rotor.setup.preset.\(index).remove")
            }
            TextField("Name", text: Binding(get: { row.name }, set: { presets.setName($0, of: row.id) }))
                .textInputAutocapitalization(.characters)
                .autocorrectionDisabled()
                .disabled(!enabled)
                .accessibilityLabel("Preset name")
                .accessibilityIdentifier("rotor.setup.preset.\(index).name")
            TextField("Heading", text: Binding(get: { row.heading }, set: { presets.setHeading($0, of: row.id) }))
                .keyboardType(.decimalPad)
                .multilineTextAlignment(.trailing)
                .monospacedDigit()
                .frame(maxWidth: 90)
                .foregroundStyle(presets.refusedRow == row.id ? ChromeColours.gaugeRed : Color.primary)
                .disabled(!enabled)
                .accessibilityLabel("Preset heading in degrees")
                .accessibilityIdentifier("rotor.setup.preset.\(index).heading")
            Text("\u{00B0}")
                .foregroundStyle(.secondary)
        }
        .opacity(enabled ? 1 : SetupNumberRow.greyedOpacity)
    }

    /// Save presets: the list as the Core keeps it, or the first bad heading's reason.
    private func savePresets() {
        guard let text = presets.validated() else {
            return
        }
        Task {
            if await model.savePresets(text) {
                presets.saved()
                presets.follow(model.presets)
            }
        }
    }
}

/// The presets list as the operator edits it on Setup's Rotor page. Like
/// the desktop page, an edit in progress is not replaced by the Core's
/// presets until it is saved and the Core accepts it; Save skips blank
/// rows and refuses a heading the Core would refuse, with the Core's words.
struct RotorPresetsDraft: Equatable {
    struct Row: Identifiable, Equatable {
        let id: UUID
        var name: String
        var heading: String
    }

    /// The Core's words for a heading it refuses (StationRotorController's notANumberReason and outOfRangeReason).
    static let notANumberReason = "That heading is not a number."
    static let outOfRangeReason = "That heading is outside the rotor's range."

    private(set) var rows: [Row] = []
    /// The operator changed the list since it last followed the Core.
    private(set) var touched = false
    /// Why the last Save was turned down here; nil when it was not.
    private(set) var message: String?
    /// The row whose heading was turned down.
    private(set) var refusedRow: UUID?

    /// Takes the Core's presets, unless an edit is in progress.
    mutating func follow(_ presets: [RotorModel.Preset]) {
        guard !touched else {
            return
        }
        let next = presets.map { (name: $0.name, heading: Self.text($0.degrees)) }
        guard next.map(\.name) != rows.map(\.name) || next.map(\.heading) != rows.map(\.heading) else {
            return
        }
        rows = next.map { Row(id: UUID(), name: $0.name, heading: $0.heading) }
    }

    mutating func add() {
        rows.append(Row(id: UUID(), name: "", heading: ""))
        touched = true
    }

    mutating func remove(_ id: UUID) {
        rows.removeAll { $0.id == id }
        touched = true
    }

    mutating func remove(atOffsets offsets: IndexSet) {
        rows.remove(atOffsets: offsets)
        touched = true
    }

    mutating func setName(_ name: String, of id: UUID) {
        guard let index = rows.firstIndex(where: { $0.id == id }), rows[index].name != name else {
            return
        }
        rows[index].name = name
        touched = true
    }

    mutating func setHeading(_ heading: String, of id: UUID) {
        guard let index = rows.firstIndex(where: { $0.id == id }), rows[index].heading != heading else {
            return
        }
        rows[index].heading = heading
        touched = true
    }

    /// The Core accepted the list: the page follows the Core again.
    mutating func saved() {
        touched = false
    }

    /// The `presets` text Save sends, or nil with ``message`` set.
    mutating func validated() -> String? {
        switch serialized() {
        case .success(let text):
            message = nil
            refusedRow = nil
            return text
        case .failure(let refusal):
            message = refusal.reason
            refusedRow = refusal.row
            return nil
        }
    }

    struct Refusal: Error, Equatable {
        let reason: String
        let row: UUID
    }

    /// One `name<TAB>degrees` line per preset in list order (the `presets`
    /// property's form). A row with no name and no heading is skipped; an
    /// empty or unreadable heading is refused, never taken as north, and one
    /// outside 0 to 360 is refused, never wrapped. A tab or line break in a
    /// name becomes a space, so it cannot split the line.
    func serialized() -> Result<String, Refusal> {
        var lines: [String] = []
        for row in rows {
            var name = row.name.trimmingCharacters(in: .whitespacesAndNewlines)
            let headingText = row.heading.trimmingCharacters(in: .whitespacesAndNewlines)
            if name.isEmpty && headingText.isEmpty {
                continue
            }
            name = name.replacingOccurrences(of: "\t", with: " ")
                .replacingOccurrences(of: "\n", with: " ")
                .replacingOccurrences(of: "\r", with: "")
            // Swift reads hexadecimal ("0x10"), which the desktop's reading refuses as not a number.
            let number = headingText.lowercased().contains("x") ? nil : Double(headingText)
            guard let number, let degrees = RotorModel.strict(number) else {
                let reason = number?.isFinite == true ? Self.outOfRangeReason : Self.notANumberReason
                return .failure(Refusal(reason: reason, row: row.id))
            }
            lines.append("\(name)\t\(Self.text(degrees))")
        }
        return .success(lines.joined(separator: "\n"))
    }

    /// A heading as the desktop writes it: up to ten significant digits, no trailing zeros.
    static func text(_ degrees: Double) -> String {
        String(format: "%.10g", degrees)
    }
}
