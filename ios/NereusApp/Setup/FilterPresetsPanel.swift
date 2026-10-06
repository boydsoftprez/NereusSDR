// NereusSDR for iOS: the approved inline phone editor and compact iPad preset grid
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusMirror
import SwiftUI
import UIKit

@MainActor
final class FilterPresetInteraction: ObservableObject {
    @Published var problem: String?
    private let editor: FilterPresetsEditor
    private var owner: FilterPresetsGesture?
    private var padWatch: AnyCancellable?
    private var authorityWatch: AnyCancellable?
    private var ownerWatch: AnyCancellable?
    private var attempt: UInt64 = 0
#if DEBUG
    var beforeOutcomePresentationForTesting: (@MainActor (SetupEditOutcome) async -> Void)?
#endif
    init(editor: FilterPresetsEditor) { self.editor = editor }

    func begin(_ control: SetupDescription.Control, slot: Int? = nil) -> FilterPresetsGesture? {
        retire(); problem = nil
        switch editor.admit(control, slot: slot) {
        case .success(let gesture):
            owner = gesture
            ownerWatch = editor.objectWillChange.sink { [weak self] in
                guard let self, self.owner === gesture, gesture.isRevoked else { return }
                self.problem = nil
            }
            return gesture
        case .failure(let refusal): problem = refusal.reason; return nil
        }
    }

    func owns(_ gesture: FilterPresetsGesture) -> Bool { owner === gesture && editor.owns(gesture) }
    func retire() { attempt &+= 1; owner?.revoke(); owner = nil; padWatch = nil; authorityWatch = nil; ownerWatch = nil }

    func perform(_ gesture: FilterPresetsGesture,
                 late: (@MainActor (PropertyWriteOutcome) -> Void)? = nil,
                 presentationCurrent: @escaping @MainActor () -> Bool = { true },
                 send: (@escaping @MainActor (SetupEditOutcome) -> Void) async -> SetupEditOutcome) async -> PropertyWriteOutcome? {
        guard owns(gesture), presentationCurrent() else { return nil }
        attempt &+= 1
        let identity = attempt
        let outcome = await send { [weak self] outcome in
            guard let self, self.attempt == identity, self.owns(gesture), presentationCurrent() else { return }
            self.receive(outcome); late?(Self.property(outcome))
        }
#if DEBUG
        if let beforeOutcomePresentationForTesting { await beforeOutcomePresentationForTesting(outcome) }
#endif
        guard attempt == identity, owns(gesture), presentationCurrent() else { return nil }
        receive(outcome)
        return Self.property(outcome)
    }
    private func receive(_ outcome: SetupEditOutcome) {
        switch outcome {
        case .applied: problem = nil
        case .refused(let reason), .notSent(let reason): problem = reason
        case .awaitingConfirmation: problem = nil
        }
    }
    static func property(_ outcome: SetupEditOutcome) -> PropertyWriteOutcome {
        switch outcome {
        case .applied: return .init(accepted: true, reason: "", value: nil)
        case .refused(let reason): return .init(accepted: false, reason: reason, value: nil)
        case .notSent(let reason): return .init(accepted: false, reason: reason, value: nil, answeredByCore: false)
        case .awaitingConfirmation: return .init(accepted: false, reason: SeveralDevices.waitingReason, value: nil)
        }
    }

    func openPad(host: ValuePadHost, control: SetupDescription.Control, row: SetupFilterPreset,
                 column: SetupDescription.FilterPresetsColumn) {
        guard let range = column.range, let gesture = begin(control, slot: row.slot) else { return }
        let editor = editor
        host.open { close in
            SetupNumberRow.pad(title: "\(row.slot + 1) \(column.label)",
                value: Double(column.id == "lowHz" ? row.lowHz : row.highHz),
                range: range.minimum...range.maximum, unit: column.unit ?? "", decimals: 0,
                send: { _ in nil }, sendWithLate: { [weak self] number, late in
                    guard let self, let pad = host.pad, let revision = pad.outcomeIdentity else { return nil }
                    let outcome = await self.perform(gesture, late: late,
                        presentationCurrent: { host.pad === pad && pad.outcomeIdentity == revision }) { receive in
                            await editor.edit(gesture, field: column.id, value: .integer(Int64(number)), onLateOutcome: receive)
                        }
                    // A stopped whole-row write consumes this gesture. Close only
                    // its current opening; reopening admits a fresh captured row.
                    if let outcome, !outcome.accepted, self.owns(gesture),
                       host.pad === pad, pad.outcomeIdentity == revision {
                        host.close()
                    }
                    return outcome
                }, readCurrent: {
                    guard let current = editor.rows.first(where: { $0.slot == row.slot }) else { return nil }
                    return Double(column.id == "lowHz" ? current.lowHz : current.highHz)
                }, close: close)
        }
        let opened = host.pad?.id
        padWatch = host.$pad.sink { [weak self] pad in if pad?.id != opened { self?.retire() } }
        authorityWatch = editor.objectWillChange.sink { [weak self, weak host] in
            guard let self, let host, self.owner === gesture, gesture.isRevoked, host.pad?.id == opened else { return }
            host.close()
        }
    }
}

struct FilterPresetsPanel: View {
    let control: SetupDescription.Control
    @ObservedObject var dispatcher: SetupControlDispatcher
    @ObservedObject private var editor: FilterPresetsEditor
    @Environment(\.filterPresetsTablet) private var tabletOverride
    @ScaledMetric(relativeTo: .body) private var columnWidth: CGFloat = 100

    init(control: SetupDescription.Control, dispatcher: SetupControlDispatcher) {
        self.control = control; self.dispatcher = dispatcher
        _editor = ObservedObject(wrappedValue: dispatcher.filterPresets)
    }
    var body: some View {
        if case .filterPresets(let binding)? = control.binding {
            if binding.action == .table {
                VStack(alignment: .leading, spacing: 8) {
                    if tabletOverride ?? (UIDevice.current.userInterfaceIdiom == .pad) { grid(binding) }
                    else {
                        ForEach(editor.rows) { row in
                            VStack(alignment: .leading, spacing: 6) {
                                Button {
                                    editor.selectedSlot = editor.selectedSlot == row.slot ? nil : row.slot
                                } label: {
                                    HStack {
                                        if editor.selectedSlot == row.slot { Image(systemName: "checkmark") }
                                        Text("\(row.slot + 1)  \(row.name)").fontWeight(.semibold)
                                        Spacer()
                                        Image(systemName: editor.selectedSlot == row.slot ? "chevron.down" : "chevron.right")
                                    }.frame(minHeight: 44).contentShape(Rectangle())
                                }.buttonStyle(.plain).accessibilityIdentifier("\(control.id).slot\(row.slot)")
                                    .accessibilityValue(editor.selectedSlot == row.slot ? "Expanded" : "Collapsed")
                                if editor.selectedSlot == row.slot {
                                    FilterPresetName(control: control, row: row, editor: editor, compact: false)
                                    ForEach(binding.columns.filter { $0.range != nil }, id: \.id) { column in
                                        FilterPresetNumber(control: control, row: row, column: column, editor: editor, compact: false)
                                    }
                                    LabeledContent(binding.column("width")?.label ?? "") { Text("\(row.widthHz)").monospacedDigit() }.frame(minHeight: 44)
                                    FilterPresetReorder(control: control, editor: editor)
                                }
                                Divider()
                            }
                        }
                    }
                    FilterPresetNotes(id: control.id, reason: editor.reason(for: control), problem: nil)
                }.accessibilityElement(children: .contain).accessibilityIdentifier(control.id)
            } else { FilterPresetReset(control: control, editor: editor) }
        }
    }

    private func grid(_ binding: SetupDescription.FilterPresetsBinding) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            ScrollView(.horizontal) {
                Grid(alignment: .leading, horizontalSpacing: 8, verticalSpacing: 8) {
                    GridRow {
                        Text("\(binding.column("slot")?.label ?? "")  \(binding.column("name")?.label ?? "")").fontWeight(.semibold).frame(width: columnWidth * 2, alignment: .leading)
                        ForEach(binding.columns.filter { $0.range != nil || $0.id == "width" }, id: \.id) { column in
                            Text(column.label).fontWeight(.semibold).frame(width: columnWidth).fixedSize(horizontal: false, vertical: true)
                        }
                    }
                    ForEach(editor.rows) { row in
                        GridRow {
                            HStack {
                                Button("\(row.slot + 1)") { editor.selectedSlot = row.slot }
                                    .frame(minWidth: 44, minHeight: 44).buttonStyle(.bordered)
                                    .accessibilityIdentifier("\(control.id).slot\(row.slot)")
                                    .accessibilityValue(editor.selectedSlot == row.slot ? "Selected" : "")
                                FilterPresetName(control: control, row: row, editor: editor, compact: true)
                            }.frame(width: columnWidth * 2)
                            ForEach(binding.columns.filter { $0.range != nil }, id: \.id) { column in
                                FilterPresetNumber(control: control, row: row, column: column, editor: editor, compact: true).frame(width: columnWidth)
                            }
                            Text("\(row.widthHz)").monospacedDigit().frame(width: columnWidth).frame(minHeight: 44)
                                .accessibilityLabel(binding.column("width")?.label ?? "")
                        }.background(editor.selectedSlot == row.slot ? Color.accentColor.opacity(0.12) : Color.clear)
                    }
                }.accessibilityElement(children: .contain).accessibilityIdentifier("\(control.id).grid")
            }
            FilterPresetReorder(control: control, editor: editor)
        }
    }
}

private struct FilterPresetNotes: View {
    let id: String
    let reason: String?
    let problem: String?
    var body: some View {
        if let reason { Text(reason).font(.footnote).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true).accessibilityIdentifier("\(id).reason") }
        if let problem { Text(problem).font(.footnote).foregroundStyle(ConnectChrome.badText).fixedSize(horizontal: false, vertical: true).accessibilityIdentifier("\(id).problem") }
    }
}

private struct FilterPresetName: View {
    let control: SetupDescription.Control
    let row: SetupFilterPreset
    @ObservedObject var editor: FilterPresetsEditor
    let compact: Bool
    @StateObject private var interaction: FilterPresetInteraction
    @State private var gesture: FilterPresetsGesture?
    @State private var draft = ""
    @FocusState private var focused: Bool
    private var id: String { "\(control.id).slot\(row.slot).name" }
    init(control: SetupDescription.Control, row: SetupFilterPreset, editor: FilterPresetsEditor, compact: Bool) {
        self.control = control; self.row = row; self.editor = editor; self.compact = compact
        _interaction = StateObject(wrappedValue: FilterPresetInteraction(editor: editor))
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if !compact, case .filterPresets(let binding)? = control.binding { Text(binding.column("name")?.label ?? "") }
            TextField("", text: Binding(get: { focused ? draft : row.name }, set: { draft = $0 }))
                .textFieldStyle(.roundedBorder).focused($focused).submitLabel(.done).frame(minHeight: 44)
                .disabled(editor.reason(for: control) != nil).accessibilityIdentifier(id)
                .accessibilityLabel("\(row.slot + 1) Name")
                .onChange(of: focused) { _, focus in
                    if focus { draft = row.name; gesture = interaction.begin(control, slot: row.slot) }
                    else if gesture?.isRevoked == true { gesture = nil }
                }
                .onSubmit {
                    let value = draft; focused = false
                    guard let gesture else { return }
                    Task { await interaction.perform(gesture) { receive in await editor.edit(gesture, field: "name", value: .text(value), onLateOutcome: receive) } }
                }
            FilterPresetNotes(id: id, reason: nil, problem: interaction.problem)
        }.onDisappear { interaction.retire() }
            .onReceive(editor.objectWillChange) {
                if gesture?.isRevoked == true { focused = false; gesture = nil }
            }
            .onChange(of: control) { _, _ in focused = false; interaction.retire(); gesture = nil }
            .onChange(of: editor.mode) { _, _ in focused = false; interaction.retire(); gesture = nil }
    }
}

private struct FilterPresetNumber: View {
    let control: SetupDescription.Control
    let row: SetupFilterPreset
    let column: SetupDescription.FilterPresetsColumn
    @ObservedObject var editor: FilterPresetsEditor
    let compact: Bool
    @StateObject private var interaction: FilterPresetInteraction
    @Environment(\.valuePadHost) private var pads
    private var value: Int { column.id == "lowHz" ? row.lowHz : row.highHz }
    private var id: String { "\(control.id).slot\(row.slot).\(column.id)" }
    init(control: SetupDescription.Control, row: SetupFilterPreset, column: SetupDescription.FilterPresetsColumn,
         editor: FilterPresetsEditor, compact: Bool) {
        self.control = control; self.row = row; self.column = column; self.editor = editor; self.compact = compact
        _interaction = StateObject(wrappedValue: FilterPresetInteraction(editor: editor))
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if compact {
                Button("\(value)") { openPad() }.buttonStyle(.borderless).frame(maxWidth: .infinity, minHeight: 44)
                    .background(.quaternary, in: RoundedRectangle(cornerRadius: 6))
                    .disabled(editor.reason(for: control) != nil || pads == nil).accessibilityIdentifier(id)
                    .accessibilityLabel("\(row.slot + 1) \(column.label)")
            } else if let range = column.range {
                SetupNumberRow(title: column.label, value: Double(value), range: range.minimum...range.maximum,
                    step: range.step, unit: column.unit ?? "", enabled: editor.reason(for: control) == nil,
                    id: id, openValuePad: { openPad() }, set: { step($0) }).frame(minHeight: 44)
            }
            FilterPresetNotes(id: id, reason: nil, problem: interaction.problem)
        }.onDisappear { interaction.retire() }
            .onChange(of: control) { _, _ in interaction.retire() }
            .onChange(of: editor.mode) { _, _ in interaction.retire() }
    }
    private func openPad() { if let pads { interaction.openPad(host: pads, control: control, row: row, column: column) } }
    private func step(_ value: Double) {
        guard let gesture = interaction.begin(control, slot: row.slot) else { return }
        Task { await interaction.perform(gesture) { receive in await editor.edit(gesture, field: column.id, value: .integer(Int64(value)), onLateOutcome: receive) } }
    }
}

private struct FilterPresetReorder: View {
    let control: SetupDescription.Control
    @ObservedObject var editor: FilterPresetsEditor
    @StateObject private var interaction: FilterPresetInteraction
    init(control: SetupDescription.Control, editor: FilterPresetsEditor) {
        self.control = control; self.editor = editor
        _interaction = StateObject(wrappedValue: FilterPresetInteraction(editor: editor))
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if case .filterPresets(let binding)? = control.binding { Text(binding.column("reorder")?.label ?? "") }
            ViewThatFits(in: .horizontal) {
                HStack { up; down }
                VStack(alignment: .leading) { up; down }
            }
            FilterPresetNotes(id: "\(control.id).reorder", reason: nil, problem: interaction.problem)
        }.onDisappear { interaction.retire() }
            .onChange(of: control) { _, _ in interaction.retire() }
            .onChange(of: editor.mode) { _, _ in interaction.retire() }
    }
    private var up: some View { Button("Move up") { move(-1) }.buttonStyle(.bordered).frame(minHeight: 44).disabled(editor.reason(for: control) != nil || editor.selectedSlot == nil || editor.rows.first?.slot == editor.selectedSlot).accessibilityIdentifier("\(control.id).moveUp") }
    private var down: some View { Button("Move down") { move(1) }.buttonStyle(.bordered).frame(minHeight: 44).disabled(editor.reason(for: control) != nil || editor.selectedSlot == nil || editor.rows.last?.slot == editor.selectedSlot).accessibilityIdentifier("\(control.id).moveDown") }
    private func move(_ direction: Int) {
        guard let gesture = interaction.begin(control, slot: editor.selectedSlot) else { return }
        Task { await interaction.perform(gesture) { receive in await editor.move(gesture, direction: direction, onLateOutcome: receive) } }
    }
}

private struct FilterPresetReset: View {
    let control: SetupDescription.Control
    @ObservedObject var editor: FilterPresetsEditor
    @StateObject private var interaction: FilterPresetInteraction
    @State private var gesture: FilterPresetsGesture?
    @State private var asking = false
    init(control: SetupDescription.Control, editor: FilterPresetsEditor) {
        self.control = control; self.editor = editor
        _interaction = StateObject(wrappedValue: FilterPresetInteraction(editor: editor))
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Button(control.label) {
                gesture = interaction.begin(control)
                guard let gesture else { return }
                if gesture.question != nil { asking = true } else { submit(gesture) }
            }.buttonStyle(.bordered).frame(minHeight: 44).disabled(editor.reason(for: control) != nil).accessibilityIdentifier(control.id)
            FilterPresetNotes(id: control.id, reason: editor.reason(for: control), problem: interaction.problem)
        }.alert(control.label, isPresented: $asking) {
            Button(DescribedControl.yesText) { if let gesture { submit(gesture) } }.accessibilityIdentifier("\(control.id).yes")
            Button(DescribedControl.noText, role: .cancel) { interaction.retire(); gesture = nil }.accessibilityIdentifier("\(control.id).no")
        } message: { Text(gesture?.question ?? "") }
        .onDisappear { interaction.retire() }
        .onChange(of: control) { _, _ in asking = false; interaction.retire(); gesture = nil }
        .onChange(of: editor.mode) { _, _ in asking = false; interaction.retire(); gesture = nil }
        .onReceive(editor.objectWillChange) { if gesture?.isRevoked == true { asking = false; gesture = nil } }
    }
    private func submit(_ gesture: FilterPresetsGesture) {
        Task { await interaction.perform(gesture) { receive in await editor.reset(gesture, confirmed: gesture.question, onLateOutcome: receive) } }
    }
}

private struct FilterPresetsTabletKey: EnvironmentKey { static let defaultValue: Bool? = nil }
extension EnvironmentValues {
    /// Native idiom by default; the existing screenshot host can render both
    /// idioms without changing the actual Core controls or dispatch path.
    var filterPresetsTablet: Bool? {
        get { self[FilterPresetsTabletKey.self] }
        set { self[FilterPresetsTabletKey.self] = newValue }
    }
}
