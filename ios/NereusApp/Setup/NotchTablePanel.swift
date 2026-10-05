// NereusSDR for iOS: the Core's notch list in Setup, DSP: each notch's centre, width and Active, with Edit and Delete
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror
import SwiftUI

/// The Core's notch table (Setup description v2): every notch with its
/// centre and width, an Active switch, Edit and Delete, the columns named
/// as the Core names them. Edit holds the list as it was when it began;
/// if the Core's list or the selected slice changes before Save, the edit
/// is cancelled with a plain reason. The whole list is read at once: a
/// list the Core sent that cannot be read is not shown in part.
struct NotchTablePanel: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher

    /// The notch being edited, with what its edit was admitted against.
    @State private var editing: Editing?
    @State private var problems: [Int64: String] = [:]
    @State private var problem: String?

    struct Editing {
        let row: Int64
        let admission: SetupAdmission
        var centre: String
        var width: String
    }

    static let emptyText = "No notches."
    static let editText = "Edit"
    static let saveText = "Save"
    static let cancelText = "Cancel"

    var body: some View {
        let state = dispatcher.notchTable(control, in: category)
        VStack(alignment: .leading, spacing: 8) {
            Text(control.label)
                .font(.body.weight(.semibold))
            if !control.tooltip.isEmpty {
                Text(control.tooltip)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
            if let list = state.list {
                if list.rows.isEmpty {
                    Text(Self.emptyText)
                        .foregroundStyle(.secondary)
                        .accessibilityIdentifier("\(control.id).empty")
                }
                ForEach(list.rows) { notch in
                    row(notch, editable: state.reason == nil)
                    Divider()
                }
            } else if let listReason = state.listReason {
                Text(listReason)
                    .font(.footnote)
                    .foregroundStyle(ConnectChrome.badText)
                    .accessibilityIdentifier("\(control.id).listReason")
            }
            if let reason = state.reason, reason != state.listReason {
                Text(reason)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .accessibilityIdentifier("\(control.id).reason")
            }
            if let problem {
                Text(problem)
                    .font(.footnote)
                    .foregroundStyle(ConnectChrome.badText)
                    .accessibilityIdentifier("\(control.id).problem")
            }
        }
        .onReceive(dispatcher.objectWillChange) { _ in
            // An edit the Core's list or the selection has overtaken ends here.
            if let editing, editing.admission.isRevoked {
                self.editing = nil
                problem = SetupControlDispatcher.notchChangedReason
            }
        }
        .accessibilityIdentifier(control.id)
    }

    // MARK: One notch

    private func column(_ field: String) -> SetupDescription.Column? {
        guard case .table(let table)? = control.binding else { return nil }
        return table.columns.first { $0.field == field || $0.rowAction == field }
    }

    @ViewBuilder
    private func row(_ notch: SetupNotch, editable: Bool) -> some View {
        let centreLabel = column("centreHz")?.label ?? ""
        let widthLabel = column("widthHz")?.label ?? ""
        VStack(alignment: .leading, spacing: 6) {
            if let draft = editing, draft.row == notch.id {
                LabeledContent(centreLabel) {
                    TextField(centreLabel, text: Binding(get: { editing?.centre ?? "" },
                                                         set: { editing?.centre = $0 }))
                        .keyboardType(.decimalPad)
                        .multilineTextAlignment(.trailing)
                        .accessibilityIdentifier("notch.\(notch.id).centreField")
                }
                LabeledContent(widthLabel) {
                    TextField(widthLabel, text: Binding(get: { editing?.width ?? "" },
                                                        set: { editing?.width = $0 }))
                        .keyboardType(.decimalPad)
                        .multilineTextAlignment(.trailing)
                        .accessibilityIdentifier("notch.\(notch.id).widthField")
                }
                HStack {
                    Button(Self.cancelText) {
                        draft.admission.revoke()
                        editing = nil
                    }
                    .buttonStyle(.bordered)
                    .accessibilityIdentifier("notch.\(notch.id).cancel")
                    Spacer()
                    Button(Self.saveText) { if let editing { save(editing) } }
                        .buttonStyle(.borderedProminent)
                        .accessibilityIdentifier("notch.\(notch.id).save")
                }
            } else {
                LabeledContent(centreLabel) {
                    Text(Self.hertz(notch.centreHz))
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                }
                LabeledContent(widthLabel) {
                    Text(Self.hertz(notch.widthHz))
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                }
                Toggle(column("active")?.label ?? "",
                       isOn: Binding(get: { notch.active }, set: { change(notch.id, .setActive($0)) }))
                    .disabled(!editable)
                    .accessibilityIdentifier("notch.\(notch.id).active")
                HStack {
                    Button(Self.editText) { beginEdit(notch) }
                        .buttonStyle(.bordered)
                        .disabled(!editable || editing != nil)
                        .accessibilityIdentifier("notch.\(notch.id).edit")
                    Spacer()
                    Button(column("delete")?.label ?? "", role: .destructive) { change(notch.id, .delete) }
                        .buttonStyle(.bordered)
                        .disabled(!editable)
                        .accessibilityIdentifier("notch.\(notch.id).delete")
                }
            }
            if let problem = problems[notch.id] {
                Text(problem)
                    .font(.footnote)
                    .foregroundStyle(ConnectChrome.badText)
                    .accessibilityIdentifier("notch.\(notch.id).problem")
            }
        }
    }

    /// Whole hertz, grouped: "7,074,000".
    static func hertz(_ value: Double) -> String {
        value.rounded() == value ? Int64(value).formatted(.number.grouping(.automatic))
            : value.formatted(.number.precision(.fractionLength(0...2)))
    }

    /// Hertz typed on the phone: digits, with or without grouping.
    static func typedHertz(_ text: String) -> Double? {
        let digits = text.filter { !$0.isWhitespace && $0 != "," }
        guard !digits.isEmpty, let value = Double(digits), value.isFinite else { return nil }
        return value
    }

    // MARK: Changes

    private func beginEdit(_ notch: SetupNotch) {
        problem = nil
        problems[notch.id] = nil
        switch dispatcher.admitNotch(control, in: category, row: notch.id) {
        case .failure(let refusal):
            problems[notch.id] = refusal.reason
        case .success(let admission):
            editing = Editing(row: notch.id, admission: admission,
                              centre: String(Int64(notch.centreHz.rounded())),
                              width: String(Int64(notch.widthHz.rounded())))
        }
    }

    private func save(_ draft: Editing) {
        guard let centre = Self.typedHertz(draft.centre), let width = Self.typedHertz(draft.width) else {
            problems[draft.row] = SetupControlDispatcher.outOfRangeReason
            return
        }
        editing = nil
        let dispatcher = dispatcher
        Task { @MainActor in
            report(draft.row, await dispatcher.performNotch(draft.admission,
                                                            edit: .move(centreHz: centre, widthHz: width)))
        }
    }

    private func change(_ row: Int64, _ edit: SetupNotchEdit) {
        problem = nil
        problems[row] = nil
        switch dispatcher.admitNotch(control, in: category, row: row) {
        case .failure(let refusal):
            problems[row] = refusal.reason
        case .success(let admission):
            let dispatcher = dispatcher
            Task { @MainActor in
                report(row, await dispatcher.performNotch(admission, edit: edit))
            }
        }
    }

    private func report(_ row: Int64, _ outcome: SetupEditOutcome) {
        switch outcome {
        case .applied, .awaitingConfirmation:
            problems[row] = nil
        case .refused(let reason), .notSent(let reason):
            problems[row] = reason
        }
    }
}
