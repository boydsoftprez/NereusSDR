// NereusSDR for iOS: captured TX Profile naming and save-before-switch decisions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation

/// Consumes the Core's metadata, using the same admissions and command results
/// as other described rows. No value is invented while a question or save waits.
@MainActor
public final class SetupTxProfileFlow: ObservableObject {
    public enum Question: Equatable {
        case name(SetupDescription.ProfilePrompt)
        case overwrite(title: String, text: String)
        case unsaved(title: String, text: String)
        public var title: String {
            switch self {
            case .name(let prompt): return prompt.title
            case .overwrite(let title, _), .unsaved(let title, _): return title
            }
        }
        public var text: String {
            switch self {
            case .name: return ""
            case .overwrite(_, let text), .unsaved(_, let text): return text
            }
        }
    }
    @Published private var pendingQuestion: Question?
    public private(set) var question: Question? {
        get { dispatcher == nil ? nil : pendingQuestion }
        set { pendingQuestion = newValue }
    }
    @Published public var name = ""
    @Published public private(set) var problem: String?
    public private(set) var problemControlId: String?
    @Published public private(set) var busy = false
    @Published public private(set) var dirty = false
    @Published public private(set) var currentName = ""
    public var questionIdentity: UUID? { dispatcher == nil ? nil : operation?.id }
    @Published public private(set) var presentationOwner: UUID?
    public let setupOwner = UUID()
    /// Older Core omits setupDescriptionVersion (StationCapabilities.cpp:408–409).
    /// Only a complete live old snapshot with the actual profile state qualifies;
    /// losing V15 metadata in a session that offered it never enables fallback.
    public static func legacySelectionIsAvailable(store: MirrorStore, canonicalSession: UInt64?) -> Bool {
        guard store.isSnapshotComplete, !store.isStale,
              (store.agreedMinor ?? 0) >= SetupDescriptionFeed.minimumMinor,
              store.capabilityVersion("transmitSettingsVersion") >= 3,
              canonicalSession != store.snapshotIdentity,
              let transmit = store.object("transmit"),
              let active = SetupControlDispatcher.text(transmit["activeTxProfile"]), !active.isEmpty,
              SetupControlDispatcher.jsonNames(transmit["txProfilesJson"]).contains(active) else { return false }
        switch store.capabilities["setupDescriptionVersion"] {
        case nil: return true
        case .int(let version)?, .enumeration(let version)?: return (1..<15).contains(version)
        default: return false
        }
    }
    public var usesLegacySelection: Bool {
        guard let dispatcher else { return false }
        return Self.legacySelectionIsAvailable(store: dispatcher.store, canonicalSession: canonicalSession)
            && dispatcher.captureSender() != nil
    }
    public var selectionReason: String? {
        guard let dispatcher else { return SetupControlDispatcher.notConnectedReason }
        guard dispatcher.store.isSnapshotComplete, !dispatcher.store.isStale else { return SetupControlDispatcher.notConnectedReason }
        if usesLegacySelection { return nil }
        guard let control, control.modern?.profileUnsavedChanges != nil else { return SetupControlDispatcher.updatingReason }
        return dispatcher.state(of: control, in: category).reason
    }
    public func problem(for owner: UUID) -> String? { dispatcher != nil && presentationOwner == owner ? problem : nil }
    public func retire(owner: UUID) {
        guard presentationOwner == owner else { return }
        cancel(); problem = nil; presentationOwner = nil
    }
    // Models and UIKit can retain this flow after the page dispatcher leaves.
    // The dispatcher's lazy flow ownership must not form a strong cycle.
    private weak var dispatcher: SetupControlDispatcher?
    private var control: SetupDescription.Control?
    private var category = "audio"
    private var trackedSnapshot: UInt64?
    private var trackedReference: SetupDescription.PropertyReference?
    private var canonicalSession: UInt64?
    private var trackedWatch: [String] = []
    private var trackedNamesProperty: String?
    private var values: [String: MirrorValue] = [:]
    private var objectIdentity: ObjectIdentifier?
    private var objectWatch: AnyCancellable?
    private var watches: Set<AnyCancellable> = []
    private var operation: Operation?
    private var revision: UInt64 = 0

    @MainActor
    private final class Operation {
        let id = UUID()
        let admission: SetupAdmission
        let saveAdmission: SetupAdmission?
        let revision: UInt64
        let currentName: String
        let selectedName: String?
        var submittedName: String?
        var started = false
        init(admission: SetupAdmission, saveAdmission: SetupAdmission? = nil, revision: UInt64,
             currentName: String, selectedName: String? = nil) {
            self.admission = admission; self.saveAdmission = saveAdmission
            self.revision = revision; self.currentName = currentName; self.selectedName = selectedName
        }
        func revoke() { admission.revoke(); saveAdmission?.revoke() }
    }

    init(dispatcher: SetupControlDispatcher) {
        self.dispatcher = dispatcher
        dispatcher.feed.$generation.dropFirst().sink { [weak self] _ in
            guard let self else { return }
            self.cancel(); self.problem = nil
            self.refreshMetadata()
        }.store(in: &watches)
        dispatcher.store.$isSnapshotComplete.dropFirst().sink { [weak self] _ in self?.authorityChanged() }.store(in: &watches)
        dispatcher.store.$isStale.dropFirst().sink { [weak self] _ in self?.authorityChanged() }.store(in: &watches)
        dispatcher.store.$objectKeys.dropFirst().sink { [weak self] keys in
            // Object keys publish before replacement. Retire synchronously,
            // then attach to the new object after the mutation completes.
            guard let self, let reference = self.trackedReference else { return }
            if !keys.contains(reference.object) { self.cancel(); self.problem = nil }
            Task { @MainActor [weak self] in
                guard let self else { return }
                guard let dispatcher = self.dispatcher else { self.retireUnavailableOwner(); return }
                let next = dispatcher.store.object(reference.object).map(ObjectIdentifier.init)
                if self.objectIdentity != next { self.cancel(); self.problem = nil; self.attach() }
            }
        }.store(in: &watches)
        dispatcher.store.$capabilities.sink { [weak self] capabilities in
            guard let self else { return }
            guard let dispatcher = self.dispatcher else { self.retireUnavailableOwner(); return }
            switch capabilities["setupDescriptionVersion"] {
            case .int(let version)?, .enumeration(let version)?:
                if version >= 15 { self.canonicalSession = dispatcher.store.snapshotIdentity }
            default: break
            }
            self.cancel()
        }.store(in: &watches)
        dispatcher.settings.$currentSnapshotIdentity.dropFirst().sink { [weak self] _ in self?.cancel() }.store(in: &watches)
        refreshMetadata()
    }
    private func refreshMetadata() {
        guard let dispatcher else { retireUnavailableOwner(); return }
        control = dispatcher.currentControl("audio.txProfile.activeProfile", in: "audio")
        if control?.modern?.profileUnsavedChanges != nil { canonicalSession = dispatcher.store.snapshotIdentity }
        attach()
    }

    /// Called by the real described page when it opens, before any user edit.
    public func open(_ control: SetupDescription.Control, in category: String) {
        guard let dispatcher else { retireUnavailableOwner(); return }
        guard control.modern?.profileUnsavedChanges != nil,
              dispatcher.currentControl(control.id, in: category) == control else { return }
        guard self.control != control || self.category != category || objectWatch == nil else { return }
        cancel(); problem = nil
        self.control = control; self.category = category
        canonicalSession = dispatcher.store.snapshotIdentity
        attach()
    }
    public func close() {
        // Closing Setup retires only its presentation. Other existing profile
        // surfaces continue to use the session's watched history.
        retire(owner: setupOwner)
    }
    public func cancel() {
        operation?.revoke(); operation = nil; question = nil; busy = false
    }
    private func retireUnavailableOwner() {
        cancel(); problem = nil; presentationOwner = nil
        control = nil; objectWatch = nil; watches.removeAll()
    }
    private func authorityChanged() {
        guard dispatcher != nil else { retireUnavailableOwner(); return }
        cancel(); problem = nil; objectWatch = nil
        Task { @MainActor [weak self] in self?.refreshMetadata() }
    }
    private func attach() {
        guard let dispatcher else { retireUnavailableOwner(); return }
        revision &+= 1
        guard dispatcher.store.isSnapshotComplete, !dispatcher.store.isStale,
              let control, case .command(let command)? = control.binding,
              let reference = command.valueProperty,
              let object = dispatcher.store.object(reference.object) else {
            // Metadata retirement revokes operations, but does not mean the
            // same live TX settings were saved. Keep its existing watch too.
            return
        }
        let sameOwner = objectIdentity == ObjectIdentifier(object)
            && trackedSnapshot == dispatcher.store.snapshotIdentity && trackedReference == reference
        if !sameOwner { values = [:]; dirty = false }
        objectWatch = nil; objectIdentity = ObjectIdentifier(object)
        trackedSnapshot = dispatcher.store.snapshotIdentity; trackedReference = reference
        trackedWatch = control.modern?.profileUnsavedChanges?.watch ?? trackedWatch
        if case .jsonNames(let source)? = control.modern?.choicesFrom { trackedNamesProperty = source.name }
        observe(object.values)
        objectWatch = object.$values.dropFirst().sink { [weak self] in self?.observe($0) }
    }
    private func observe(_ next: [String: MirrorValue]) {
        guard dispatcher != nil else { retireUnavailableOwner(); return }
        guard let property = trackedReference else { return }
        let active = SetupControlDispatcher.text(next[property.name]) ?? ""
        let profileChanged = active != currentName
        let watchedChanged = !values.isEmpty && trackedWatch.contains { next[$0] != values[$0] }
        let namesChanged: Bool
        if let source = trackedNamesProperty {
            namesChanged = !values.isEmpty && next[source] != values[source]
        } else { namesChanged = false }
        // Saving a new name may publish the new manifest before command.result.
        // That expected echo does not abandon its save result. The chosen
        // target is always resolved by its captured text after save completes.
        let listInvalidates = namesChanged && operation?.started != true
        if profileChanged || watchedChanged || listInvalidates {
            revision &+= 1; cancel(); problem = nil
        }
        if values.isEmpty || profileChanged { dirty = false }
        else if watchedChanged { dirty = true }
        currentName = active; values = next
    }

    /// The Save button captures its descriptor, session and sender before asking.
    public func beginSave(_ save: SetupDescription.Control, in category: String, owner: UUID? = nil) {
        guard let dispatcher else { retireUnavailableOwner(); return }
        guard !busy else { return }
        cancel(); problem = nil; problemControlId = save.id; presentationOwner = owner ?? setupOwner
        guard let prompt = save.modern?.profilePrompt else { return }
        switch dispatcher.admit(save, in: category) {
        case .failure(let refusal): problem = refusal.reason
        case .success(let admission):
            operation = Operation(admission: admission, revision: revision, currentName: currentName)
            name = SetupControlDispatcher.text(dispatcher.read(prompt.initial, sliceId: nil)) ?? ""
            question = .name(prompt)
        }
    }
    /// A trimmed empty name ends the question without sending a command.
    public func acceptName() async {
        guard let dispatcher else { retireUnavailableOwner(); return }
        guard case .name(let prompt)? = question, let operation, owns(operation) else { cancel(); return }
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { cancel(); return }
        operation.submittedName = trimmed
        let names = SetupControlDispatcher.jsonNames(dispatcher.read(prompt.namesFrom, sliceId: nil))
        if names.contains(trimmed) {
            question = .overwrite(title: prompt.overwriteTitle,
                                  text: prompt.overwriteQuestion.replacingOccurrences(of: "%1", with: trimmed))
        } else { await save(operation, name: trimmed) }
    }
    public func confirmOverwrite() async {
        guard dispatcher != nil else { retireUnavailableOwner(); return }
        guard case .overwrite? = question, let operation, let name = operation.submittedName else { return }
        await save(operation, name: name)
    }

    /// Pass the actual text from the menu, so a list reordering cannot retarget it.
    public func choose(_ selectedName: String, owner: UUID? = nil) {
        guard let dispatcher else { retireUnavailableOwner(); return }
        guard !busy, selectedName != currentName else { return }
        cancel(); problem = nil; presentationOwner = owner ?? setupOwner
        if let reason = selectionReason { problem = reason; return }
        guard let control, let metadata = control.modern?.profileUnsavedChanges else {
            problem = SetupControlDispatcher.updatingReason; return
        }
        problemControlId = control.id
        guard let chosen = choiceValue(selectedName, control: control) else { return }
        switch dispatcher.admit(control, in: category) {
        case .failure(let refusal): problem = refusal.reason
        case .success(let admission):
            if dirty {
                guard let saveControl = dispatcher.feed.description(for: category)?.pages.flatMap({ $0.sections })
                    .flatMap({ $0.controls }).first(where: {
                        if case .command(let command)? = $0.binding { return command.verb == metadata.saveVerb && $0.modern?.profilePrompt != nil }
                        return false
                    }) else { admission.revoke(); problem = SetupControlDispatcher.unreadableReason; return }
                switch dispatcher.admit(saveControl, in: category) {
                case .failure(let refusal): admission.revoke(); problem = refusal.reason
                case .success(let saveAdmission):
                    operation = Operation(admission: admission, saveAdmission: saveAdmission, revision: revision,
                                          currentName: currentName, selectedName: selectedName)
                    question = .unsaved(title: metadata.title,
                        text: metadata.question.replacingOccurrences(of: "%1", with: currentName))
                }
            } else {
                let captured = Operation(admission: admission, revision: revision, currentName: currentName, selectedName: selectedName)
                operation = captured
                Task { @MainActor in await self.select(captured, value: chosen) }
            }
        }
    }
    public func discardAndSwitch() async {
        guard dispatcher != nil else { retireUnavailableOwner(); return }
        guard case .unsaved? = question, let operation, let target = operation.selectedName,
              let value = choiceValue(target, control: operation.admission.control) else { return }
        operation.saveAdmission?.revoke()
        await select(operation, value: value)
    }
    public func saveAndSwitch() async {
        guard dispatcher != nil else { retireUnavailableOwner(); return }
        guard case .unsaved? = question, let operation else { return }
        await save(operation, name: operation.currentName)
    }
    private func choiceValue(_ name: String, control: SetupDescription.Control) -> SetupValue? {
        guard let dispatcher else { retireUnavailableOwner(); return nil }
        return dispatcher.resolved(control).modern?.optionLiterals.first { $0.value == .text(name) }.map { .integer($0.key) }
    }
    private func owns(_ captured: Operation) -> Bool {
        guard let dispatcher else { retireUnavailableOwner(); return false }
        return operation === captured && captured.revision == revision && captured.currentName == currentName
            && !captured.admission.isRevoked && dispatcher.stillCurrent(captured.admission)
    }
    private func save(_ captured: Operation, name: String) async {
        guard let dispatcher else { retireUnavailableOwner(); return }
        guard owns(captured), !captured.started else { return }
        captured.started = true; question = nil; busy = true
        let outcome = await dispatcher.perform(captured.saveAdmission ?? captured.admission, value: .text(name))
        guard owns(captured) else { return }
        guard outcome == .applied else { finish(captured, outcome: outcome); return }
        dirty = false
        if let selected = captured.selectedName,
           let value = choiceValue(selected, control: captured.admission.control) {
            captured.started = false
            await select(captured, value: value)
        } else { finish(captured, outcome: outcome) }
    }
    private func select(_ captured: Operation, value: SetupValue) async {
        guard let dispatcher else { retireUnavailableOwner(); return }
        guard owns(captured) else { return }
        question = nil; busy = true
        let outcome = await dispatcher.perform(captured.admission, value: value)
        // An active-profile echo may retire this operation during select.
        guard owns(captured) else { return }
        finish(captured, outcome: outcome)
    }
    private func finish(_ captured: Operation, outcome: SetupEditOutcome) {
        guard operation === captured else { return }
        switch outcome {
        case .applied, .awaitingConfirmation: problem = nil
        case .refused(let reason), .notSent(let reason): problem = reason
        }
        cancel()
    }
}
