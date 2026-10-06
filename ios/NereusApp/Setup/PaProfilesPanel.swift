// NereusSDR for iOS: the Core's PA profile lifecycle and approved phone band list / iPad grid
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusMirror
import SwiftUI
import UIKit

/// App presentation ownership for one lifecycle control or one cell. An
/// admission is captured at the gesture, before creating any Task or pad.
/// It is retained through the prompt/question and both result paths.
@MainActor
final class PaEditorInteraction: ObservableObject {
    let dispatcher: SetupControlDispatcher
    private(set) var control: SetupDescription.Control
    let category: String
    let band: Int?
    let column: String?
    @Published private(set) var problem: String?
    @Published private(set) var prompt: SetupPaPrompt?
    @Published private(set) var question: SetupPaQuestion?
    @Published var name = ""
    private(set) var admission: SetupAdmission?
    private var padWatch: AnyCancellable?
    private var authorityWatch: AnyCancellable?
    private var operation: UInt64 = 0
#if DEBUG
    /// Pauses only the App consumer after the real dispatcher result, so a
    /// test can deliver the next Core event before presentation resumes.
    var beforeOutcomePresentationForTesting: (@MainActor (SetupEditOutcome) async -> Void)?
#endif

    init(dispatcher: SetupControlDispatcher, control: SetupDescription.Control, category: String,
         band: Int? = nil, column: String? = nil) {
        self.dispatcher = dispatcher
        self.control = control
        self.category = category
        self.band = band
        self.column = column
    }

    func rebind(_ current: SetupDescription.Control) {
        guard current != control else { return }
        retire()
        problem = nil
        control = current
    }

    @discardableResult
    func begin() -> Bool {
        retire()
        problem = nil
        switch dispatcher.admitPa(control, in: category, band: band, column: column) {
        case .failure(let refusal): problem = refusal.reason; return false
        case .success(let admitted): admission = admitted; return true
        }
    }

    func beginPrompt() -> Bool {
        guard begin(), let admission else { return false }
        switch dispatcher.paPrompt(for: admission) {
        case .success(let prompt): self.prompt = prompt; name = prompt.defaultValue; return true
        case .failure(let refusal): problem = refusal.reason; retire(); return false
        }
    }

    func beginQuestion() -> Bool {
        guard begin(), let admission else { return false }
        switch dispatcher.paQuestion(for: admission) {
        case .success(let question): self.question = question; return true
        case .failure(let refusal): problem = refusal.reason; retire(); return false
        }
    }

    /// The caller captures its admission synchronously. Neither immediate
    /// nor late results can update a replaced, abandoned or stale owner.
    func perform(_ captured: SetupAdmission, value: SetupValue? = nil,
                 prompt: SetupPaPrompt? = nil, question: SetupPaQuestion? = nil,
                 late: (@MainActor (PropertyWriteOutcome) -> Void)? = nil,
                 presentationCurrent: @escaping @MainActor () -> Bool = { true }) async -> PropertyWriteOutcome? {
        guard owns(captured), presentationCurrent() else { return nil }
        operation &+= 1
        let attempt = operation
        let outcome = await dispatcher.performPa(captured, value: value, prompt: prompt, confirmed: question,
                                                 onLateOutcome: { [weak self] outcome in
            guard let self, self.owns(captured), self.operation == attempt, presentationCurrent() else { return }
            self.receive(outcome)
            late?(Self.property(outcome))
        })
#if DEBUG
        if let beforeOutcomePresentationForTesting {
            await beforeOutcomePresentationForTesting(outcome)
        }
#endif
        guard owns(captured), operation == attempt, presentationCurrent() else { return nil }
        receive(outcome)
        return Self.property(outcome)
    }

    func submit(_ value: SetupValue? = nil) {
        guard let captured = admission else { return }
        let prompt = prompt
        let question = question
        Task { await perform(captured, value: value, prompt: prompt, question: question) }
    }

    func gesture(_ value: SetupValue) {
        guard begin() else { return }
        submit(value)
    }

    func owns(_ captured: SetupAdmission) -> Bool {
        admission === captured && dispatcher.paOwnsOutcome(captured)
    }

    func retire() {
        operation &+= 1
        admission?.revoke()
        admission = nil
        prompt = nil
        question = nil
        padWatch = nil
        authorityWatch = nil
    }

    private func receive(_ outcome: SetupEditOutcome) {
        switch outcome {
        case .applied: problem = nil
        case .refused(let reason), .notSent(let reason): problem = reason
        case .awaitingConfirmation: break
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

    /// Opening, closing or replacing the existing shared number pad also
    /// retires PA presentation ownership. Its digits are an unsaved draft;
    /// the displayed cell continues to read only the Core's facade.
    func openPad(host: ValuePadHost, column: SetupDescription.PaColumn, row: SetupDescription.PaRow,
                 value: Double, readCurrent: @escaping () -> Double?) {
        guard let range = column.range, let decimals = column.decimals, begin(), let captured = admission else { return }
        host.open { close in
            ValuePadModel(title: "\(row.label) \(column.label)", unit: "",
                          range: range.minimum...range.maximum, decimals: decimals,
                          step: SetupNumberRow.text(range.step, decimals: decimals, unit: ""), current: value,
                          sendWithLate: { [weak self] number, late in
                guard let self else { return nil }
                let pad = host.pad
                let revision = pad?.outcomeIdentity
                return await self.perform(captured, value: .decimal(number), late: late,
                                          presentationCurrent: { pad?.outcomeIdentity == revision && revision != nil })
            }, readCurrent: readCurrent, close: close)
        }
        let opened = host.pad?.id
        padWatch = host.$pad.sink { [weak self] pad in
            guard pad?.id == opened else { self?.retire(); return }
        }
        // Revocation is irreversible, including an accepted write's facade
        // echo arriving before its result continuation. Discard only this
        // opening's stale draft; a replacement pad retains its own owner.
        authorityWatch = dispatcher.objectWillChange.sink { [weak self, weak host] in
            guard let self, let host, self.admission === captured,
                  captured.isRevoked, host.pad?.id == opened else { return }
            host.close()
        }
    }
}

/// Drawn at the Core's existing control positions. The four lifecycle
/// controls share their approved row beginning at New; the table occupies
/// its existing group, so Bypass ANAN PA Settings and telemetry follow it.
struct PaProfilesPanel: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var pages: SetupDescribedPages
    @ObservedObject var dispatcher: SetupControlDispatcher
    var tablet = UIDevice.current.userInterfaceIdiom == .pad
    var initiallyExpandedBand: Int?

    private var controls: [SetupDescription.Control] {
        pages.categories[category]?.pages.flatMap(\.sections).flatMap(\.controls) ?? []
    }
    private var table: SetupDescription.Control? { controls.first { $0.id == "pa.gain.table" } }
    private var profiles: PaProfiles? { table.flatMap { dispatcher.paPanel($0, in: category).profiles } }

    /// The generic dispatcher deliberately has no PA renderer. Its generic
    /// limitation is replaced only for a parsed PA lifecycle binding; all
    /// actual Core gates, availability and missing-body reasons are kept.
    static func lifecycleReason(_ control: SetupDescription.Control, table: SetupDescription.Control?,
                                category: String, dispatcher: SetupControlDispatcher) -> String? {
        let state = dispatcher.state(of: control, in: category)
        guard case .paProfile? = control.binding else { return state.reason ?? SetupControlDispatcher.unreadableReason }
        if let reason = state.reason, reason != SetupControlDispatcher.notOnThisPhoneReason { return reason }
        guard let table else { return SetupControlDispatcher.unreadableReason }
        return dispatcher.paPanel(table, in: category).reason
    }

    static func groupsLifecycle(_ controls: [SetupDescription.Control]) -> Bool {
        controls.contains { item in
            guard case .paProfile(let binding)? = item.binding else { return false }
            return binding.action == .new && item.metadataIssue == nil && item.modern?.pendingReason == nil
        }
    }

    var body: some View {
        switch control.binding {
        case .paProfile(let binding)? where binding.action == .active:
            PaProfileSelection(control: control, category: category, dispatcher: dispatcher, profiles: profiles,
                               reason: Self.lifecycleReason(control, table: table, category: category, dispatcher: dispatcher))
        case .paProfile(let binding)? where binding.action == .new && Self.groupsLifecycle(controls):
            let actions = controls.filter { item in
                guard case .paProfile(let binding)? = item.binding else { return false }
                return binding.action != .active
            }
            ViewThatFits(in: .horizontal) {
                HStack(alignment: .top, spacing: 8) { lifecycle(actions) }
                VStack(alignment: .leading, spacing: 8) { lifecycle(actions) }
            }
        case .paProfile?:
            if Self.groupsLifecycle(controls) {
                EmptyView() // In the lifecycle row beginning at New, in Core order.
            } else {
                PaProfileActionButton(control: control, category: category, dispatcher: dispatcher,
                                      reason: Self.lifecycleReason(control, table: table, category: category, dispatcher: dispatcher))
            }
        case .paProfileGrid?:
            PaBandTable(control: control, category: category, dispatcher: dispatcher,
                        tablet: tablet, initiallyExpandedBand: initiallyExpandedBand)
        default:
            VStack(alignment: .leading, spacing: 4) {
                Text(control.label).foregroundStyle(.secondary)
                Text(dispatcher.state(of: control, in: category).reason ?? SetupControlDispatcher.unreadableReason)
                    .font(.footnote).foregroundStyle(.secondary)
                    .accessibilityIdentifier("\(control.id).reason")
            }.accessibilityIdentifier(control.id)
        }
    }

    @ViewBuilder private func lifecycle(_ actions: [SetupDescription.Control]) -> some View {
        ForEach(actions, id: \.id) { action in
            PaProfileActionButton(control: action, category: category, dispatcher: dispatcher,
                                  reason: Self.lifecycleReason(action, table: table, category: category, dispatcher: dispatcher))
        }
    }
}

private struct PaProfileSelection: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher
    let profiles: PaProfiles?
    let reason: String?
    @StateObject private var interaction: PaEditorInteraction

    init(control: SetupDescription.Control, category: String, dispatcher: SetupControlDispatcher,
         profiles: PaProfiles?, reason: String?) {
        self.control = control; self.category = category; self.dispatcher = dispatcher
        self.profiles = profiles; self.reason = reason
        _interaction = StateObject(wrappedValue: PaEditorInteraction(dispatcher: dispatcher, control: control, category: category))
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Picker(control.label, selection: Binding(get: { profiles?.active ?? "" }, set: { interaction.gesture(.text($0)) })) {
                if let profiles {
                    ForEach(profiles.names, id: \.self) { Text($0).tag($0) }
                } else { Text("--").tag("") }
            }
            .pickerStyle(.menu).disabled(reason != nil || profiles == nil)
            .accessibilityIdentifier(control.id)
            PaControlNotes(id: control.id, help: control.tooltip, reason: reason, problem: interaction.problem)
        }
        .onDisappear { interaction.retire() }
        .onChange(of: control) { _, current in interaction.rebind(current) }
        .onChange(of: dispatcher.outcomeIdentity) { _, _ in interaction.retire() }
    }
}

private struct PaProfileActionButton: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher
    let reason: String?
    @StateObject private var interaction: PaEditorInteraction
    @State private var askingName = false
    @State private var askingQuestion = false

    init(control: SetupDescription.Control, category: String, dispatcher: SetupControlDispatcher, reason: String?) {
        self.control = control; self.category = category; self.dispatcher = dispatcher; self.reason = reason
        _interaction = StateObject(wrappedValue: PaEditorInteraction(dispatcher: dispatcher, control: control, category: category))
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Button(control.label) {
                guard case .paProfile(let binding)? = control.binding else { return }
                if binding.prompt != nil { askingName = interaction.beginPrompt() }
                else { askingQuestion = interaction.beginQuestion() }
            }
            .buttonStyle(.bordered).frame(minHeight: 44).fixedSize(horizontal: true, vertical: false)
            .disabled(reason != nil).accessibilityIdentifier(control.id)
            .help(control.tooltip).accessibilityHint(control.tooltip)
            PaControlNotes(id: control.id, help: "", reason: reason, problem: interaction.problem)
        }
        .alert(interaction.prompt?.title ?? control.label, isPresented: $askingName) {
            TextField(interaction.prompt?.label ?? "", text: $interaction.name)
                .accessibilityIdentifier("\(control.id).name")
            Button(control.label) { interaction.submit(.text(interaction.name)) }
                .accessibilityIdentifier("\(control.id).submit")
            Button("Cancel", role: .cancel) { interaction.retire() }
                .accessibilityIdentifier("\(control.id).cancel")
        }
        .alert(control.label, isPresented: $askingQuestion) {
            Button(DescribedControl.yesText) { interaction.submit() }
                .accessibilityIdentifier("\(control.id).yes")
            Button(DescribedControl.noText, role: .cancel) { interaction.retire() }
                .accessibilityIdentifier("\(control.id).no")
        } message: { Text(interaction.question?.text ?? "") }
        .onDisappear { interaction.retire() }
        .onChange(of: control) { _, current in interaction.rebind(current); askingName = false; askingQuestion = false }
        .onChange(of: dispatcher.outcomeIdentity) { _, _ in interaction.retire(); askingName = false; askingQuestion = false }
    }
}

private struct PaControlNotes: View {
    let id: String
    let help: String
    let reason: String?
    let problem: String?
    var body: some View {
        if !help.isEmpty { Text(help).font(.footnote).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true) }
        if let reason {
            Text(reason).font(.footnote).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("\(id).reason")
        }
        if let problem {
            Text(problem).font(.footnote).foregroundStyle(ConnectChrome.badText).fixedSize(horizontal: false, vertical: true)
                .accessibilityIdentifier("\(id).problem")
        }
    }
}

private struct PaBandTable: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher
    let tablet: Bool
    @State private var expandedBand: Int?
    @ScaledMetric(relativeTo: .body) private var columnWidth: CGFloat = 84

    init(control: SetupDescription.Control, category: String, dispatcher: SetupControlDispatcher,
         tablet: Bool, initiallyExpandedBand: Int?) {
        self.control = control; self.category = category; self.dispatcher = dispatcher; self.tablet = tablet
        _expandedBand = State(initialValue: initiallyExpandedBand)
    }

    var body: some View {
        let panel = dispatcher.paPanel(control, in: category)
        VStack(alignment: .leading, spacing: 8) {
            if let grid = panel.grid {
                if tablet { fullGrid(grid, panel) }
                else {
                    ForEach(grid.rows, id: \.band) { row in
                        VStack(alignment: .leading, spacing: 6) {
                            Button {
                                expandedBand = expandedBand == row.band ? nil : row.band
                            } label: {
                                HStack {
                                    Text(row.label).fontWeight(.semibold)
                                    Spacer()
                                    Image(systemName: expandedBand == row.band ? "chevron.down" : "chevron.right")
                                }.frame(minHeight: 44).contentShape(Rectangle())
                            }
                            .buttonStyle(.plain).accessibilityIdentifier("\(control.id).band\(row.band)")
                            .accessibilityValue(expandedBand == row.band ? "Expanded" : "Collapsed")
                            if expandedBand == row.band {
                                ForEach(grid.columns, id: \.id) { column in
                                    cell(row, column, panel, compact: false)
                                }
                            }
                            PaControlNotes(id: "\(control.id).band\(row.band)", help: "",
                                           reason: panel.reason(for: row.band), problem: nil)
                            Divider()
                        }
                    }
                }
            }
            PaControlNotes(id: control.id, help: control.tooltip, reason: panel.reason, problem: nil)
        }.accessibilityElement(children: .contain).accessibilityIdentifier(control.id)
    }

    private func width(_ column: SetupDescription.PaColumn) -> CGFloat {
        columnWidth * (column.id == "gain" ? 1.35 : column.id == "maxPower" ? 1.2 : 1)
    }

    private func fullGrid(_ grid: SetupDescription.PaProfileGrid, _ panel: SetupPaPanel) -> some View {
        ScrollView(.horizontal) {
            Grid(alignment: .leading, horizontalSpacing: 6, verticalSpacing: 8) {
                GridRow {
                    Text("Band").fontWeight(.semibold).frame(width: columnWidth, alignment: .leading)
                    ForEach(grid.columns, id: \.id) { column in
                        Text(column.label).fontWeight(.semibold).frame(width: width(column))
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
                ForEach(grid.rows, id: \.band) { row in
                    GridRow {
                        Text(row.label).fontWeight(.semibold).frame(width: columnWidth, alignment: .leading).frame(minHeight: 44)
                            .accessibilityIdentifier("\(control.id).band\(row.band)")
                        ForEach(grid.columns, id: \.id) { column in
                            cell(row, column, panel, compact: true).frame(width: width(column))
                        }
                    }
                    if let reason = panel.reason(for: row.band) {
                        Text("\(row.label): \(reason)").font(.footnote).foregroundStyle(.secondary)
                            .gridCellColumns(grid.columns.count + 1)
                            .accessibilityIdentifier("\(control.id).band\(row.band).reason")
                    }
                }
            }.accessibilityElement(children: .contain).accessibilityIdentifier("\(control.id).grid")
        }
    }

    private func cell(_ row: SetupDescription.PaRow, _ column: SetupDescription.PaColumn,
                      _ panel: SetupPaPanel, compact: Bool) -> some View {
        PaProfileCell(control: control, category: category, dispatcher: dispatcher, row: row, column: column,
                      value: panel.profiles?.bands.first { $0.band == row.band }?.value(for: column),
                      reason: panel.reason(for: row.band), compact: compact)
            .id("\(row.band).\(column.id)")
    }
}

private struct PaProfileCell: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher
    let row: SetupDescription.PaRow
    let column: SetupDescription.PaColumn
    let value: SetupValue?
    let reason: String?
    let compact: Bool
    @StateObject private var interaction: PaEditorInteraction
    @Environment(\.valuePadHost) private var pads
    private var id: String { "\(control.id).band\(row.band).\(column.id)" }
    private var help: String { column.tooltip.replacingOccurrences(of: "%1", with: row.label) }
    private var editable: Bool { reason == nil && value != nil }

    init(control: SetupDescription.Control, category: String, dispatcher: SetupControlDispatcher,
         row: SetupDescription.PaRow, column: SetupDescription.PaColumn, value: SetupValue?, reason: String?, compact: Bool) {
        self.control = control; self.category = category; self.dispatcher = dispatcher
        self.row = row; self.column = column; self.value = value; self.reason = reason; self.compact = compact
        _interaction = StateObject(wrappedValue: PaEditorInteraction(dispatcher: dispatcher, control: control,
                                                                    category: category, band: row.band, column: column.id))
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if column.kind == .toggle, let on = value?.flag {
                Toggle(compact ? "" : column.label, isOn: Binding(get: { on }, set: { interaction.gesture(.bool($0)) }))
                    .labelsHiddenIf(compact).disabled(!editable).frame(minHeight: 44)
                    .accessibilityLabel("\(row.label) \(column.label)").accessibilityHint(help)
                    .accessibilityIdentifier(id)
            } else if let number = value?.number, let range = column.range, let decimals = column.decimals {
                if compact {
                    numberButton(number, decimals: decimals).frame(maxWidth: .infinity, minHeight: 44)
                        .background(.quaternary, in: RoundedRectangle(cornerRadius: 6))
                        .accessibilityIdentifier(id)
                } else {
                    Stepper {
                        HStack {
                            Text(column.label)
                            Spacer(minLength: 8)
                            numberButton(number, decimals: decimals)
                        }
                    } onIncrement: {
                        interaction.gesture(.decimal(DescribedControl.rounded(min(number + range.step, range.maximum), decimals: decimals)))
                    } onDecrement: {
                        interaction.gesture(.decimal(DescribedControl.rounded(max(number - range.step, range.minimum), decimals: decimals)))
                    }
                    .disabled(!editable).opacity(editable ? 1 : SetupNumberRow.greyedOpacity)
                    .accessibilityElement(children: .ignore)
                    .accessibilityLabel("\(row.label) \(column.label)")
                    .accessibilityValue(SetupNumberRow.text(number, decimals: decimals, unit: ""))
                    .accessibilityAdjustableAction { direction in
                        guard editable else { return }
                        switch direction {
                        case .increment: interaction.gesture(.decimal(DescribedControl.rounded(min(number + range.step, range.maximum), decimals: decimals)))
                        case .decrement: interaction.gesture(.decimal(DescribedControl.rounded(max(number - range.step, range.minimum), decimals: decimals)))
                        @unknown default: break
                        }
                    }
                    .accessibilityActions {
                        if editable, pads != nil {
                            Button(SetupNumberRow.enterValueAction) { openPad(number) }
                        }
                    }
                    .accessibilityIdentifier(id)
                }
            } else {
                LabeledContent(compact ? "" : column.label) { Text("--").foregroundStyle(.secondary) }
                    .frame(minHeight: 44).accessibilityLabel("\(row.label) \(column.label)")
                    .accessibilityIdentifier(id)
            }
            PaControlNotes(id: id, help: compact ? "" : help, reason: nil, problem: interaction.problem)
        }
        .help(help).accessibilityHint(help)
        .onDisappear { interaction.retire() }
        .onChange(of: control) { _, current in interaction.rebind(current) }
        .onChange(of: dispatcher.outcomeIdentity) { _, _ in interaction.retire() }
    }

    private func openPad(_ number: Double) {
        guard editable, let pads else { return }
        interaction.openPad(host: pads, column: column, row: row, value: number) {
            dispatcher.paPanel(control, in: category).profiles?.bands.first { $0.band == row.band }?.value(for: column)?.number
        }
    }

    private func numberButton(_ number: Double, decimals: Int) -> some View {
        Button { openPad(number) } label: {
            Text(SetupNumberRow.text(number, decimals: decimals, unit: "")).monospacedDigit()
                .foregroundStyle(editable ? AnyShapeStyle(.tint) : AnyShapeStyle(.secondary))
                .frame(minHeight: 44)
        }
        .buttonStyle(.borderless).disabled(!editable || pads == nil)
        .accessibilityLabel("\(row.label) \(column.label)")
        .accessibilityValue(SetupNumberRow.text(number, decimals: decimals, unit: ""))
        .accessibilityIdentifier("\(id).value")
    }
}

private extension View {
    @ViewBuilder func labelsHiddenIf(_ hidden: Bool) -> some View {
        if hidden { labelsHidden() } else { self }
    }
}
