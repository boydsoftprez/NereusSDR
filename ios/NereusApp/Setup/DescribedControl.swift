// NereusSDR for iOS: one control the Core describes, drawn as a visible native control for its kind
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusBand
import NereusMirror
import SwiftUI

/// One described control: a switch, a number with minus and plus, a
/// slider, a menu of choices, a text field, a colour, a button, a reading
/// or a table, each a visible control with the Core's label, range, unit
/// and help. A control that cannot change now is greyed with its reason
/// under it; the owner's refusal shows under it in its own words.
struct DescribedControl: View {
    let control: SetupDescription.Control
    let category: String
    @ObservedObject var dispatcher: SetupControlDispatcher
    var specialized: SetupSpecializedPanels = .none

    @StateObject private var outcomeOwner = SetupRowOutcomeOwner()
    @State private var typedEdit: UInt64?
    private var problem: String? {
        get { outcomeOwner.problem }
        nonmutating set { outcomeOwner.problem = newValue }
    }
    /// A question is showing: a button's (V12's `confirm`), or a row's
    /// before it is set to the value that asks (V15's `confirmWhen`).
    @State private var asking = false
    /// The value a row's question holds until Yes; nil for a button.
    @State private var askedValue: SetupValue?
    /// The question as it was asked, with what it named then.
    @State private var question: SetupQuestion?
    /// A slider's position while the finger is on it.
    @State private var draft: Double?
    @State private var textEntry = SetupTextEntryDraft()

    init(control: SetupDescription.Control, category: String, dispatcher: SetupControlDispatcher,
         specialized: SetupSpecializedPanels = .none, outcomeOwner: SetupRowOutcomeOwner = SetupRowOutcomeOwner()) {
        self.control = control
        self.category = category
        self.dispatcher = dispatcher
        self.specialized = specialized
        _outcomeOwner = StateObject(wrappedValue: outcomeOwner)
        _textEntry = State(initialValue: control.kind == .text
            ? Self.textEntry(for: control, category: category, dispatcher: dispatcher)
            : SetupTextEntryDraft())
    }

    /// The source belongs to this entry lifetime, so even an old binding
    /// getter reads the replacement entry's current source and display.
    private static func textEntry(for control: SetupDescription.Control, category: String,
                                  dispatcher: SetupControlDispatcher) -> SetupTextEntryDraft {
        SetupTextEntryDraft(readCurrent: {
            dispatcher.state(of: control, in: category).value?.text ?? ""
        })
    }

    /// A value the Core has not sent.
    static let unavailableText = "Unavailable"
    static let onText = "On"
    static let offText = "Off"
    /// A button's question's answers (V12's `confirm`), No the default.
    static let yesText = "Yes"
    static let noText = "No"

    /// The row as drawn: its choices and limits filled from the Core's lists
    /// and the station catalogue where it takes them from there (V15).
    private var drawn: SetupDescription.Control { dispatcher.resolved(control) }

    var body: some View {
        if SetupSpecializedPanels.isPaProfile(control) {
            if let panel = specialized.makePa?(control, category) {
                panel
            } else {
                unavailableRow(SetupSpecializedPanels.unavailableReason)
            }
        } else if let kind = control.specialized {
            if let panel = specialized.make(kind, control, category) {
                panel
            } else {
                unavailableRow(SetupSpecializedPanels.unavailableReason)
            }
        } else {
            let state = dispatcher.state(of: control, in: category)
            VStack(alignment: .leading, spacing: 4) {
                marked(row(state))
                    .alert(control.label, isPresented: $asking) {
                        Button(Self.yesText) { commit(askedValue, asked: question) }
                            .accessibilityIdentifier("\(control.id).yes")
                        Button(Self.noText, role: .cancel) {}
                            .accessibilityIdentifier("\(control.id).no")
                    } message: {
                        Text(question?.text ?? "")
                    }
                if !control.tooltip.isEmpty, control.kind != .toggle {
                    Text(control.tooltip)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                        .lineLimit(3)
                }
                // A table says its own reason in its row, once.
                if let reason = state.reason, control.kind != .table {
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
            .onDisappear {
                outcomeOwner.retire()
                if control.kind == .text { textEntry.retirePresentation() }
            }
            .onChange(of: control) { _, _ in
                outcomeOwner.retire()
                problem = nil
                if control.kind == .text {
                    textEntry = Self.textEntry(for: control, category: category, dispatcher: dispatcher)
                }
            }
            .onChange(of: dispatcher.outcomeIdentity) { _, _ in
                outcomeOwner.retire()
                problem = nil
                if control.kind == .text {
                    textEntry = Self.textEntry(for: control, category: category, dispatcher: dispatcher)
                }
            }
        }
    }

    // MARK: Rows by kind

    /// The visible native control each kind is drawn as.
    enum Row: Equatable {
        case switchRow, numberRow, slider, menu, textField, colour, button, reading, table, panel
    }

    /// The row a control is drawn as; nil only for a kind the Core has not
    /// defined, which shows as an unavailable reading.
    static func row(for control: SetupDescription.Control) -> Row? {
        if control.specialized != nil || SetupSpecializedPanels.isPaProfile(control) {
            return .panel
        }
        switch control.kind {
        case .toggle?: return .switchRow
        case .integer?, .decimal?: return .numberRow
        case .slider?: return .slider
        case .choice?: return .menu
        case .text?: return .textField
        case .colour?: return .colour
        case .button?: return .button
        case .readout?: return .reading
        case .table?: return .table
        case .settingsHygiene?: return .panel
        case nil: return nil
        }
    }

    @ViewBuilder
    private func row(_ state: SetupControlState) -> some View {
        switch control.kind {
        case .toggle?:
            SetupSwitchRow(title: title, detail: control.tooltip,
                           isOn: Binding(get: { state.value?.flag ?? false },
                                         set: { request(.bool($0)) }),
                           enabled: state.editable && state.value != nil, id: control.id)
        case .integer?, .decimal?:
            if let number = state.value?.number, let range = drawn.range {
                SetupNumberRow(title: title, value: number, range: range.minimum...range.maximum,
                               step: range.step, unit: control.unit ?? "", decimals: decimals,
                               enabled: state.editable, id: control.id,
                               enter: { typed in await enter(numberValue(typed)) },
                               enterWithLate: { typed, late in await enter(numberValue(typed), onLateOutcome: late) },
                               onOutcome: { outcome in
                                   if let edit = typedEdit { outcomeOwner.receive(outcome, edit: edit) }
                               }, readCurrent: { dispatcher.state(of: control, in: category).value?.number }) { value in
                    request(numberValue(value))
                }
            } else {
                valueRow(Self.unavailableText)
            }
        case .slider?:
            slider(state)
        case .choice?:
            choice(state)
        case .text?:
            let entryIdentity = textEntry.identity
            TextField(control.label, text: Binding(get: { textEntry.displayedValue }, set: { value in
                var edited = textEntry
                if edited.type(value, identity: entryIdentity) { textEntry = edited }
            }))
                .onSubmit { submitText(identity: entryIdentity) }
                .disabled(!state.editable)
                .accessibilityIdentifier(control.id)
        case .colour?:
            colour(state)
        case .button?:
            Button(control.label) {
                request(nil)
            }
            .buttonStyle(.bordered)
            .disabled(!state.editable)
            .accessibilityIdentifier(control.id)
        case .readout?:
            if dispatcher.temperatureUnitKey(of: control) != nil {
                temperature(state)
            } else {
                valueRow(Self.reading(state.value, control: control))
            }
        case .table?:
            table(reason: state.reason)
        default:
            valueRow(Self.unavailableText)
        }
    }

    /// The row's label: a per-band row names the band (V12's `perBand`).
    private var title: String {
        dispatcher.label(of: control)
    }

    private func valueRow(_ value: String) -> some View {
        LabeledContent(title) {
            Text(value)
                .monospacedDigit()
                .foregroundStyle(.secondary)
        }
        .accessibilityIdentifier(control.id)
    }

    /// A temperature reading with this phone's choice of unit under it,
    /// where the desktop flips its unit by a click on the reading.
    private func temperature(_ state: SetupControlState) -> some View {
        let fahrenheit = dispatcher.temperatureInFahrenheit(control)
        return VStack(alignment: .leading, spacing: 8) {
            valueRow(Self.reading(state.value, control: control))
            LabeledContent(Self.temperatureUnitText) {
                Picker(Self.temperatureUnitText, selection: Binding(
                    get: { fahrenheit ?? false },
                    set: { dispatcher.setTemperatureInFahrenheit($0, for: control) })) {
                    Text(Self.celsiusText).tag(false)
                    Text(Self.fahrenheitText).tag(true)
                }
                .pickerStyle(.segmented)
                .fixedSize()
                .disabled(fahrenheit == nil)
                .accessibilityIdentifier("\(control.id).unit")
            }
        }
    }

    /// The temperature unit choice's words.
    static let temperatureUnitText = "Unit"
    static let celsiusText = "\u{00B0}C"
    static let fahrenheitText = "\u{00B0}F"

    private func unavailableRow(_ reason: String) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(control.label)
                .font(.body.weight(.semibold))
                .foregroundStyle(.secondary)
            Text(reason)
                .font(.footnote)
                .foregroundStyle(.secondary)
                .accessibilityIdentifier("\(control.id).reason")
        }
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier(control.id)
    }

    @ViewBuilder
    private func slider(_ state: SetupControlState) -> some View {
        if let options = drawn.options, !options.isEmpty {
            // The options by index, their values written (the FFT sizes).
            let index = options.firstIndex { $0.value == state.value?.whole } ?? 0
            let shown = Int((draft ?? Double(index)).rounded())
            VStack(alignment: .leading) {
                LabeledContent(title) {
                    Text(state.value == nil ? Self.unavailableText : options[min(max(shown, 0), options.count - 1)].label)
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                }
                Slider(value: Binding(get: { draft ?? Double(index) }, set: { draft = $0 }),
                       in: 0...Double(max(options.count - 1, 1)), step: 1) { editing in
                    if !editing, let draft {
                        let chosen = options[min(max(Int(draft.rounded()), 0), options.count - 1)]
                        self.draft = nil
                        request(.integer(chosen.value))
                    }
                }
                .disabled(!state.editable || state.value == nil || options.count < 2)
            }
            .accessibilityIdentifier(control.id)
        } else if let range = drawn.range, let number = state.value?.number {
            VStack(alignment: .leading) {
                LabeledContent(title) {
                    Text(SetupNumberRow.text(draft ?? number, decimals: decimals, unit: control.unit ?? ""))
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                }
                Slider(value: Binding(get: { draft ?? number }, set: { draft = $0 }),
                       in: range.minimum...range.maximum, step: range.step) { editing in
                    if !editing, let draft {
                        self.draft = nil
                        request(wholeRange ? .integer(Int64(draft.rounded())) : .decimal(draft))
                    }
                }
                .disabled(!state.editable)
            }
            .accessibilityIdentifier(control.id)
        } else {
            valueRow(Self.unavailableText)
        }
    }

    @ViewBuilder
    private func choice(_ state: SetupControlState) -> some View {
        let items: [(value: Int64, label: String)] = drawn.options.map { $0.map { ($0.value, $0.label) } }
            ?? (drawn.choices ?? []).enumerated().map { (Int64($0.offset), $0.element) }
        // Options this phone cannot show: in the menu, disabled, each with its reason below.
        let unavailable = dispatcher.unavailableOptions(of: control)
        VStack(alignment: .leading, spacing: 4) {
            Picker(title, selection: Binding(get: { state.value?.whole ?? Int64.min },
                                             set: { request(.integer($0)) })) {
                if state.value == nil {
                    Text(Self.unavailableText).tag(Int64.min)
                }
                ForEach(items, id: \.value) { item in
                    Text(item.label).tag(item.value)
                        .selectionDisabled(unavailable[item.value] != nil)
                }
            }
            .pickerStyle(.menu)
            .disabled(!state.editable)
            .accessibilityIdentifier(control.id)
            ForEach(items.filter { unavailable[$0.value] != nil }, id: \.value) { item in
                Text("\(item.label): \(unavailable[item.value] ?? "")")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityIdentifier("\(control.id).option.\(item.value)")
            }
        }
    }

    @ViewBuilder
    private func colour(_ state: SetupControlState) -> some View {
        if let hex = state.value?.text {
            let opaque: Bool = {
                if case .phone(let key)? = control.binding { return PhoneSetupKeys.opaqueColourKeys.contains(key) }
                return false
            }()
            HStack {
                ColorPicker(title, selection: Binding(get: { BandColours.withAlpha(hex) },
                                                              set: { request(.text(DisplayOnThisPhonePage.hexWithAlpha($0))) }),
                            supportsOpacity: !opaque)
                Text(hex)
                    .font(.caption.monospaced())
                    .foregroundStyle(.secondary)
            }
            .disabled(!state.editable)
            .accessibilityIdentifier(control.id)
        } else {
            valueRow(Self.unavailableText)
        }
    }

    /// A generic table: its columns and rows as the Core names them. Only
    /// the closed tables change anything, and they have their own panels.
    /// One line says why it cannot change here: the row's own reason when
    /// it has one, else the tables' standard words.
    private func table(reason: String?) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(control.label)
                .font(.body.weight(.semibold))
            Text(Self.tableReason(reason))
                .font(.footnote)
                .foregroundStyle(.secondary)
                .accessibilityIdentifier("\(control.id).reason")
        }
        .accessibilityIdentifier(control.id)
    }

    /// The one line a table shows under its label.
    static func tableReason(_ reason: String?) -> String {
        reason ?? SetupControlDispatcher.notOnThisPhoneReason
    }

    // MARK: The low-pass filter in use

    /// The size of the mark beside an Alex-1 low-pass row.
    static let lowPassMarkSize: CGFloat = 10

    /// `content` with the mark the desktop's lamp is, when this row is an
    /// Alex-1 low-pass row and the Core has said which filter is in use:
    /// filled for the filter in use, an outline for the others. It only
    /// shows; touching it does nothing.
    @ViewBuilder
    private func marked(_ content: some View) -> some View {
        if let lamp = dispatcher.lowPassLamp(for: control) {
            HStack(spacing: 10) {
                Self.lowPassMark(lamp)
                    .accessibilityIdentifier("\(control.id).lamp")
                content
            }
        } else {
            content
        }
    }

    /// The mark itself: VoiceOver reads the filled one as In use and skips the outlines.
    @ViewBuilder
    static func lowPassMark(_ lamp: SetupControlDispatcher.LowPassLamp) -> some View {
        switch lamp {
        case .inUse:
            Circle()
                .fill(Color.accentColor)
                .frame(width: lowPassMarkSize, height: lowPassMarkSize)
                .accessibilityElement()
                .accessibilityLabel(SetupControlDispatcher.lowPassInUseText)
        case .notInUse:
            Circle()
                .strokeBorder(Color.secondary, lineWidth: 1.5)
                .frame(width: lowPassMarkSize, height: lowPassMarkSize)
                .accessibilityHidden(true)
        }
    }

    // MARK: Values

    private var decimals: Int {
        if let decimals = control.decimals {
            return decimals
        }
        guard control.kind == .decimal || !wholeRange, let step = control.range?.step, step > 0 else {
            return 0
        }
        var places = 0
        var scaled = step
        while places < 6, scaled.rounded() != scaled {
            scaled *= 10
            places += 1
        }
        return places
    }

    /// A stepped decimal held to the places the Core describes, so a step
    /// of 0.001 from 2.500001 writes 2.501001 and not the float's tail.
    static func rounded(_ value: Double, decimals: Int) -> Double {
        guard decimals > 0, decimals <= 9 else { return value }
        let scale = pow(10, Double(decimals))
        let result = (value * scale).rounded() / scale
        return result.isFinite ? result : value
    }

    private var wholeRange: Bool {
        guard let range = control.range else { return false }
        return [range.minimum, range.maximum, range.step].allSatisfy { $0.rounded() == $0 }
    }

    /// A reading's text: its number to the described places with its unit,
    /// On or Off, its words, or Unavailable. Never a zero it did not send.
    static func reading(_ value: SetupValue?, control: SetupDescription.Control) -> String {
        let unit = control.unit ?? ""
        switch value {
        case .integer(let whole)?:
            let number = control.decimals.map { String(format: "%.\($0)f", Double(whole)) } ?? String(whole)
            return unit.isEmpty ? number : "\(number) \(unit)"
        case .decimal(let number)?:
            let text = String(format: "%.\(control.decimals ?? 2)f", number)
            return unit.isEmpty ? text : "\(text) \(unit)"
        case .bool(let on)?:
            return on ? onText : offText
        case .text(let words)?:
            return words
        case nil:
            return unavailableText
        }
    }

    /// A number row's value as its kind writes it: whole, or held to its decimal places.
    private func numberValue(_ number: Double) -> SetupValue {
        control.kind == .integer ? .integer(Int64(number.rounded())) : .decimal(Self.rounded(number, decimals: decimals))
    }

    /// A value typed on the number pad: sent the way minus and plus send
    /// theirs, asking first where the row asks. The pad shows the answer:
    /// it closes on a kept value and shows the refusal as sent.
    private func enter(_ value: SetupValue,
                       onLateOutcome: (@MainActor (PropertyWriteOutcome) -> Void)? = nil) async -> PropertyWriteOutcome? {
        if asks(value) {
            // The pad closes and the row's question asks, or the pad says
            // why it cannot ask.
            if let reason = ask(value) {
                return PropertyWriteOutcome(accepted: false, reason: reason, value: nil, answeredByCore: false)
            }
            return SetupNumberRow.kept
        }
        typedEdit = outcomeOwner.begin()
        let outcome = await dispatcher.edit(control, in: category, to: value, onLateOutcome: { outcome in
            onLateOutcome?(Self.padAnswer(outcome))
        })
        return Self.padAnswer(outcome)
    }

    /// The words an edit's end leaves under the row: none when applied.
    static func problem(_ outcome: SetupEditOutcome) -> String? {
        switch outcome {
        case .applied, .awaitingConfirmation:
            // A change the Core holds for its question is not a refusal.
            return nil
        case .refused(let reason), .notSent(let reason):
            return reason
        }
    }

    /// An edit's end as the number pad reads it: kept closes the pad; a
    /// refusal, or why nothing was sent, stays on it in those words.
    static func padAnswer(_ outcome: SetupEditOutcome) -> PropertyWriteOutcome {
        switch outcome {
        case .applied, .awaitingConfirmation:
            // Held for the Core's question: the pad closes and it asks.
            return SetupNumberRow.kept
        case .refused(let reason):
            return PropertyWriteOutcome(accepted: false, reason: reason, value: nil)
        case .notSent(let reason):
            return PropertyWriteOutcome(accepted: false, reason: reason, value: nil, answeredByCore: false)
        }
    }

    /// Sends the value, asking first when the row asks for it: a button with
    /// a question, or a row set to the value its question is about.
    private func request(_ value: SetupValue?) {
        if asks(value) {
            if let reason = ask(value) {
                problem = reason
            }
        } else {
            commit(value)
        }
    }

    /// Shows the row's question, read now with what it names; nil when it
    /// shows, else why it cannot ask (nothing to name), and nothing is sent.
    private func ask(_ value: SetupValue?) -> String? {
        switch dispatcher.question(for: control, in: category, value: value) {
        case .success(let asked)?:
            question = asked
            askedValue = value
            asking = true
            return nil
        case .failure(let refusal)?:
            return refusal.reason
        case nil:
            commit(value)
            return nil
        }
    }

    private func asks(_ value: SetupValue?) -> Bool {
        guard control.confirm != nil else { return false }
        guard let value else { return true }
        guard let when = control.modern?.confirmWhen else { return false }
        switch (value, when) {
        case (.bool(let on), .bool(let asked)):
            return on == asked
        case (.integer(let index), _):
            if let literal = control.modern?.optionLiterals[index] { return literal == when }
            if case .integer(let asked) = when { return index == asked }
            return false
        case (.decimal(let number), .decimal(let asked)):
            return number == asked
        case (.text(let words), .text(let asked)):
            return words == asked
        default:
            return false
        }
    }

    private func submitText(identity: UUID) {
        guard textEntry.identity == identity else { return }
        var aligned = textEntry
        if aligned.alignDisplayedBaseline() { textEntry = aligned }
        request(.text(aligned.value))
    }

    private func commit(_ value: SetupValue?, asked: SetupQuestion? = nil) {
        let dispatcher = dispatcher
        let control = control
        let category = category
        let owner = outcomeOwner
        // Only the entry actually submitted owns its result. A question
        // still sends the value it asked about, without adopting newer text.
        let textEdit: SetupTextEntryDraft.Submission?
        if control.kind == .text {
            var aligned = textEntry
            textEdit = value?.text.flatMap { aligned.submit($0, owner: owner) }
            textEntry = aligned
        } else {
            textEdit = nil
        }
        let rowEdit = control.kind == .text ? nil : owner.begin()
        let receive: @MainActor (SetupEditOutcome) -> Void = { outcome in
            if let textEdit { textEntry.receive(outcome, edit: textEdit, owner: owner) }
            else if let rowEdit { owner.receive(outcome, edit: rowEdit) }
        }
        // Bind permission to this gesture before the task can yield to a
        // replacement session or settings snapshot. perform rechecks that
        // captured admission; an old gesture cannot obtain fresh authority.
        let admission = dispatcher.admit(control, in: category)
        Task { @MainActor in
            let outcome: SetupEditOutcome
            switch admission {
            case .success(let admission):
                outcome = await dispatcher.perform(admission, value: value, asked: asked, onLateOutcome: receive)
            case .failure(let refusal):
                outcome = .notSent(refusal.reason)
            }
            receive(outcome)
        }
    }
}

/// The result owner shared by a described row and its typed-value path.
/// Its lifetime and edit identity are independent of transport admission.
@MainActor
final class SetupRowOutcomeOwner: ObservableObject {
    @Published var problem: String?
    private var edit: UInt64 = 0
    private var active = true

    func begin() -> UInt64 { edit &+= 1; active = true; return edit }
    func retire() { active = false; edit &+= 1 }
    func owns(_ edit: UInt64) -> Bool { active && self.edit == edit }
    func receive(_ outcome: SetupEditOutcome, edit: UInt64) {
        guard active, self.edit == edit, outcome != .awaitingConfirmation else { return }
        problem = DescribedControl.problem(outcome)
    }
    func receive(_ outcome: PropertyWriteOutcome, edit: UInt64) {
        guard active, self.edit == edit, outcome.isCurrent, !outcome.heldForQuestion else { return }
        problem = outcome.accepted ? nil : (outcome.reason.isEmpty ? nil : outcome.reason)
    }
}

/// The real text field's display owner: the current source, a human's
/// unsent text, or the submitted text until the dispatcher's result.
@MainActor
struct SetupTextEntryDraft {
    struct Submission { let identity: UUID; let revision: UInt64; let rowEdit: UInt64 }
    private enum Phase { case followingCore, unsentLocal, submittedPresentation }
    fileprivate let identity = UUID()
    private var revision: UInt64 = 0
    private var phase: Phase = .followingCore
    /// Kept after the first result so a current late answer can own its note.
    private var lastSubmission: Submission?
    private let readCurrent: (@MainActor () -> String)?
    private(set) var value: String

    init(readCurrent: (@MainActor () -> String)? = nil) {
        self.readCurrent = readCurrent
        value = readCurrent?() ?? ""
    }

    /// This getter has no State writes. A submitted presentation also
    /// covers the interval before perform establishes any model hold.
    var displayedValue: String {
        phase == .followingCore ? readCurrent?() ?? value : value
    }

    /// Align only when following the source; local text owns its baseline.
    @discardableResult
    mutating func alignDisplayedBaseline() -> Bool {
        guard phase == .followingCore else { return false }
        let displayed = displayedValue
        guard value != displayed else { return false }
        value = displayed
        return true
    }

    @discardableResult
    mutating func type(_ value: String, identity expected: UUID? = nil) -> Bool {
        guard expected == nil || expected == identity else { return false }
        let displayed = displayedValue
        guard value != displayed else { return false }
        alignDisplayedBaseline()
        revision &+= 1
        self.value = value
        phase = .unsentLocal
        lastSubmission = nil
        return true
    }

    mutating func submit(owner: SetupRowOutcomeOwner) -> Submission {
        alignDisplayedBaseline()
        let edit = Submission(identity: identity, revision: revision, rowEdit: owner.begin())
        lastSubmission = edit
        phase = .submittedPresentation
        return edit
    }

    mutating func submit(_ value: String, owner: SetupRowOutcomeOwner) -> Submission? {
        // Yes keeps its asked value, but cannot adopt a different display.
        alignDisplayedBaseline()
        guard self.value == value else { return nil }
        return submit(owner: owner)
    }

    /// A fixture may seed a source-less draft; the real row reads its source
    /// directly and never copies Core text in a value-change callback.
    mutating func modelChanged(_ value: String, identity expected: UUID? = nil) {
        guard expected == nil || expected == identity, phase == .followingCore else { return }
        self.value = value
    }

    /// A hidden row may retain State. Its retired result cannot leave a
    /// local presentation held forever; genuine unsent text remains local.
    mutating func retirePresentation() {
        lastSubmission = nil
        if phase == .submittedPresentation {
            phase = .followingCore
            alignDisplayedBaseline()
        }
    }

    mutating func receive(_ outcome: SetupEditOutcome, edit: Submission, owner: SetupRowOutcomeOwner) {
        guard identity == edit.identity, revision == edit.revision,
              lastSubmission?.identity == edit.identity,
              lastSubmission?.revision == edit.revision,
              lastSubmission?.rowEdit == edit.rowEdit, owner.owns(edit.rowEdit) else { return }
        // A returned awaitingConfirmation hands presentation to the model's
        // held-question rules too; it must not create an endless local hold.
        phase = .followingCore
        alignDisplayedBaseline()
        owner.receive(outcome, edit: edit.rowEdit)
    }
}
