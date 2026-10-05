// NereusSDR for iOS: reads each described Setup control from its owner and sends its edits there
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink

/// Each control the Core describes writes to its owner (R-IOS-18): a
/// setting through the settings proxy, a property through the mirror, a
/// verb through the command client, a phone key into the phone's own
/// stores. An edit is admitted when its gesture begins, against the
/// current session, the completed snapshot, the description's generation,
/// the settings snapshot and the selected slice; if any of those moves on
/// before the edit reaches the transport, its permit is revoked and
/// nothing is sent. Disabling a control on screen is never the only guard.
///
/// Each control follows its own gate: an `offAir` control locks while the
/// radio is on the air, and a control with no gate stays live. Without a
/// current session and snapshot a control shows its last value, disabled,
/// with a plain reason.
@MainActor
public final class SetupControlDispatcher: ObservableObject {
    public typealias CaptureSender = () -> MirrorStore.BoundSender?

    // MARK: Words

    public nonisolated static let notConnectedReason = "Connect to the Core to change this."
    public nonisolated static let updatingReason = "The Core is updating this page."
    public nonisolated static let unreadableReason = "This control from the Core could not be read."
    public nonisolated static let notOnThisPhoneReason = "This control is not available on this phone."
    public nonisolated static let needsNewerCoreReason = "Needs a newer Core"
    public nonisolated static let onAirReason = "The radio is on the air. Try again when it stops."
    public nonisolated static let transmitStateUnknownReason = "Waiting for the radio to say whether it is on the air."
    public nonisolated static let transmitNotAllowedReason = "This phone can't transmit on this Core."
    public nonisolated static let valueMissingReason = "The Core has not sent this value."
    public nonisolated static let noSliceReason = "Choose one of your slices first."
    public nonisolated static let dependsReason = "Needs a different choice above."
    public nonisolated static let outOfRangeReason = "This value is outside what the Core allows."
    public nonisolated static let changedFirstReason = "Things changed before this was sent, so nothing was changed."
    public nonisolated static let linkLostReason = PropertyWriteOutcome.linkLost.reason
    public nonisolated static let refusedReason = "The Core did not take this change."
    public nonisolated static let noAnswerReason = PropertyWriteOutcome.notConfirmed.reason
    public nonisolated static let readingReason = "This value can only be read."
    public nonisolated static let commandTimeout: Duration = .seconds(5)

    /// The mirrored alias for the phone's selected slice.
    public nonisolated static let activeSliceAlias = "slice:active"
    static let sliceClass = "SliceModel"
    static let txStateKey = "txState"

    let store: MirrorStore
    let feed: SetupDescriptionFeed
    let settings: SettingsProxyClient
    public lazy var txProfiles = SetupTxProfileFlow(dispatcher: self)
    public lazy var filterPresets = FilterPresetsEditor(dispatcher: self)
    let commands: CommandClient
    let captureSender: CaptureSender
    let selectedSlice: () -> Int?
    weak var phone: (any SetupPhoneKeys)?
    /// This session signed in with this phone's paired key.
    let signedInWithDeviceKey: () -> Bool
    /// The settings check in flight, and its last answer in this session.
    var hygieneBusy = false
    var hygieneOutcome: HygieneOutcome?
    var admissions: [SetupAdmission] = []
    // Retain each row's admission after its send ends so later authority
    // transitions retire timed-out result ownership as well as pending sends.
    var paResultOwners: [PaResultKey: SetupAdmission] = [:]
    /// A described result owner retires with the authenticated mirror session.
    public var outcomeIdentity: UInt64 { store.snapshotIdentity }
    /// V15's staged rows: the value each holds while the operator prepares
    /// it, sent only by the button that names it (`$control`).
    var stagedValues: [String: SetupValue] = [:]
    /// Each held value's row: its category, its description of that
    /// category when the value was held, and whether it is a slice's.
    var stagedRows: [String: StagedRow] = [:]
    struct StagedRow {
        let category: String
        let description: SetupDescription
        let slices: Bool
    }
    /// V16's NR3 models from this session's dspAssets.list.
    var nr3List = SetupNr3ModelList()
    /// V19's CFC band editor: the edit held on the phone and its send.
    var cfc = CfcEditorState()
    /// How long the CFC editor waits after the last change before it sends.
    public var cfcSendPause: Duration = .milliseconds(600)
    /// The clock that pause is measured on; a test moves its own.
    public var cfcClock: any Clock<Duration> = ContinuousClock()
    /// Whether this phone's microphone line to the Core is open now (V15's
    /// `gate.micLine`); nil reads as closed.
    public var microphoneLineOpen: (() -> Bool)?
    /// The station catalogue's transmit range by name (V15's `rangeFrom`),
    /// or nil before the catalogue arrives.
    public var catalogueTransmitRange: ((String) -> SetupDescription.Range?)?
    /// The clock telemetry receipts are stamped with, in milliseconds.
    public var telemetryNow: () -> Int64 = { Int64(ProcessInfo.processInfo.systemUptime * 1_000) }
    /// The wall clock, for readouts of times the Core sends as dates.
    public var wallClockNow: () -> Date = { Date() }
    private var watches: Set<AnyCancellable> = []
    private var objectWatches: [String: (id: ObjectIdentifier, watch: AnyCancellable)] = [:]

    public init(store: MirrorStore, feed: SetupDescriptionFeed, settings: SettingsProxyClient,
                commands: CommandClient, captureSender: @escaping CaptureSender,
                selectedSlice: @escaping () -> Int?, selectionChanges: AnyPublisher<Int?, Never>,
                phone: (any SetupPhoneKeys)?, signedInWithDeviceKey: @escaping () -> Bool = { false }) {
        self.store = store
        self.feed = feed
        self.settings = settings
        self.commands = commands
        self.captureSender = captureSender
        self.selectedSlice = selectedSlice
        self.phone = phone
        self.signedInWithDeviceKey = signedInWithDeviceKey
        store.$isSnapshotComplete.dropFirst().sink { [weak self] _ in self?.retireAll() }.store(in: &watches)
        store.$isStale.dropFirst().sink { [weak self] _ in self?.retireAll() }.store(in: &watches)
        // A session that ends drops the CFC editor's held edit and any send
        // without its answer; the next session follows the Core's profile.
        store.$isSnapshotComplete.dropFirst().sink { [weak self] complete in
            if !complete { self?.resetCfc() }
        }.store(in: &watches)
        store.$isStale.dropFirst().sink { [weak self] stale in
            if stale { self?.resetCfc() }
        }.store(in: &watches)
        store.$capabilities.dropFirst().sink { [weak self] capabilities in
            self?.capabilitiesChanged(capabilities)
        }.store(in: &watches)
        store.$objectKeys.sink { [weak self] _ in self?.objectsChanged() }.store(in: &watches)
        // Published after the descriptions it counts (SetupDescriptionFeed.publish).
        feed.$generation.dropFirst().sink { [weak self] _ in self?.descriptionsChanged() }.store(in: &watches)
        settings.$currentSnapshotIdentity.dropFirst().sink { [weak self] _ in self?.retireAll() }.store(in: &watches)
        settings.$values.dropFirst().sink { [weak self] _ in self?.changed() }.store(in: &watches)
        selectionChanges.sink { [weak self] _ in self?.selectionChanged() }.store(in: &watches)
        phone?.changes.sink { [weak self] _ in self?.changed() }.store(in: &watches)
    }

    // MARK: Reading

    /// The control as the page draws it now.
    public func state(of control: SetupDescription.Control, in category: String) -> SetupControlState {
        if control.kind == .readout, control.modern != nil {
            return modernReadout(control, in: category)
        }
        if control.kind == .readout {
            // A readout shows only this session's value, never a cached one.
            let live = store.isSnapshotComplete && !store.isStale && control.metadataIssue == nil
            return SetupControlState(value: live ? value(of: control, sliceId: selectedSlice()) : nil,
                                     editable: false, reason: nil)
        }
        let value = control.applies == .staged
            ? stagedValues[control.id] ?? value(of: control, sliceId: selectedSlice())
            : value(of: control, sliceId: selectedSlice())
        if let reason = unavailableReason(control, in: category, sliceId: selectedSlice()) {
            return SetupControlState(value: value, editable: false, reason: reason)
        }
        return SetupControlState(value: value, editable: true, reason: nil)
    }

    /// The label a control shows: a per-band row's names the band the pan
    /// is on (V12's `perBand`), or keeps the Core's label before the Core
    /// names a band.
    public func label(of control: SetupDescription.Control) -> String {
        guard let perBand = control.perBand, let band = phone?.perBandName else {
            return control.label
        }
        return perBand.label(for: band)
    }

    /// The options of `control` this phone cannot show, each with why:
    /// drawn disabled with the reason, and refused if sent.
    public func unavailableOptions(of control: SetupDescription.Control) -> [Int64: String] {
        guard case .phone(let key)? = control.binding, control.options != nil else {
            return [:]
        }
        return phone?.unavailableOptions(forPhoneKey: key) ?? [:]
    }

    // MARK: Editing

    /// Admits an edit when its gesture begins, or says why not.
    public func admit(_ control: SetupDescription.Control, in category: String) -> Result<SetupAdmission, SetupRefusal> {
        switch control.binding {
        case .paProfile?, .paProfileGrid?: return admitPa(control, in: category)
        case .filterPresets?:
            // The closed editor admits a FilterPresetsGesture that captures
            // the entire row and bank; a generic admission cannot substitute.
            return .failure(SetupRefusal(reason: Self.notOnThisPhoneReason))
        default: break
        }
        let sliceId = selectedSlice()
        if let reason = unavailableReason(control, in: category, sliceId: sliceId) {
            return .failure(SetupRefusal(reason: reason))
        }
        // The objects whose changes retire this edit are watched before it exists.
        watch(SetupDescriptionFeed.objectKey)
        watch(Self.txStateKey)
        guard let binding = control.binding else {
            return .failure(SetupRefusal(reason: Self.unreadableReason))
        }
        var objectKey: String?
        var sender: MirrorStore.BoundSender?
        switch binding {
        case .property(let reference):
            objectKey = resolve(reference.object, sliceId: sliceId)
            sender = captureSender()
        case .command:
            sender = captureSender()
        default:
            break
        }
        if case .property = binding, sender == nil {
            return .failure(SetupRefusal(reason: Self.notConnectedReason))
        }
        if case .command = binding, sender == nil {
            return .failure(SetupRefusal(reason: Self.notConnectedReason))
        }
        let admission = SetupAdmission(control: control, category: category, generation: feed.generation,
                                       mirrorIdentity: store.snapshotIdentity,
                                       settingsIdentity: settings.currentSnapshotIdentity, sender: sender,
                                       objectKey: objectKey, sliceId: Self.needsSlice(control) ? sliceId : nil)
        admissions.removeAll { $0.isRevoked }
        admissions.append(admission)
        return .success(admission)
    }

    /// Sends an admitted edit to its owner. `value` is nil for a button.
    /// `asked` is the question the row asked before it, when it names
    /// something: nothing is sent unless it still names the same.
    public func perform(_ admission: SetupAdmission, value: SetupValue?,
                        asked: SetupQuestion? = nil,
                        onLateOutcome: (@MainActor (SetupEditOutcome) -> Void)? = nil) async -> SetupEditOutcome {
        if admission.special?.pa != nil {
            return await performPa(admission, value: value, onLateOutcome: onLateOutcome)
        }
        defer { admissions.removeAll { $0 === admission } }
        guard stillCurrent(admission) else {
            admission.revoke()
            return .notSent(Self.changedFirstReason)
        }
        let control = admission.control
        if tooLong(value, control) {
            return .notSent(Self.tooLongReason)
        }
        if control.applies == .staged {
            // Held here until its button sends it; nothing goes to the Core.
            guard let value, stagedFits(value, control) else {
                return .notSent(Self.outOfRangeReason)
            }
            guard let description = feed.description(for: admission.category) else {
                return .notSent(Self.changedFirstReason)
            }
            stagedValues[control.id] = value
            stagedRows[control.id] = StagedRow(category: admission.category, description: description,
                                               slices: Self.needsSlice(control))
            changed()
            return .applied
        }
        if case .radioSetting(let path)? = control.binding {
            return await performRadioSetting(admission, path: path, value: value, onLateOutcome: onLateOutcome)
        }
        switch control.binding {
        case .setting(let key)?:
            guard let value, let text = SetupValueCoding.encodeSetting(value, for: control) else {
                return .notSent(Self.outOfRangeReason)
            }
            guard let identity = admission.settingsIdentity else {
                return .notSent(Self.changedFirstReason)
            }
            let outcome = await settings.writeBound(key, text, expectedSnapshotIdentity: identity,
                                                    authority: admission.permit, onLateOutcome: { [weak self] outcome in
                self?.deliverLate(Self.settingOutcome(outcome), admission: admission, to: onLateOutcome)
            })
            switch outcome {
            case .accepted:
                return .applied
            case .rejected(let reason):
                if Self.waitsForConfirmation(reason) {
                    return .awaitingConfirmation
                }
                return .refused(reason.isEmpty ? Self.refusedReason : reason)
            case .keptOnThisDevice:
                return .refused(Self.refusedReason)
            case .notSent:
                return .notSent(Self.changedFirstReason)
            case .linkLost:
                return .notSent(Self.linkLostReason)
            case .notConfirmed:
                // Latest Core value shows, marked; the actual answer may still come.
                return .notSent(PropertyWriteOutcome.notConfirmed.reason)
            }
        case .property(let reference)?:
            guard let value, let typed = modernWire(value, for: control) else {
                return .notSent(Self.outOfRangeReason)
            }
            guard let key = admission.objectKey, let sender = admission.sender else {
                return .notSent(Self.changedFirstReason)
            }
            let outcome = await store.writeBound(key, property: reference.name, value: typed, sender: sender,
                                                 authority: admission.permit, onLateOutcome: { [weak self] outcome in
                guard outcome.isCurrent else { return }
                self?.deliverLate(Self.propertyOutcome(outcome), admission: admission, to: onLateOutcome)
            })
            if outcome.accepted {
                return .applied
            }
            if !outcome.answeredByCore {
                return .notSent(admission.isRevoked ? Self.changedFirstReason : outcome.reason)
            }
            if Self.waitsForConfirmation(outcome.reason) {
                return .awaitingConfirmation
            }
            return .refused(outcome.reason.isEmpty ? Self.refusedReason : outcome.reason)
        case .command(let command)?:
            guard let sender = admission.sender else {
                return .notSent(Self.changedFirstReason)
            }
            guard let arguments = arguments(command, admission: admission, value: value) else {
                return .notSent(Self.changedFirstReason)
            }
            if let named = asked?.named, Self.firstText(arguments) != named {
                // The question named another value than the one Yes would send.
                return .notSent(Self.changedFirstReason)
            }
            do {
                let result = try await commands.invokeBound(
                    command.verb, arguments: arguments, timeout: Self.commandTimeout, sender: sender,
                    stillAllowed: { [weak self] in await self?.stillCurrent(admission) ?? false },
                    authority: admission.permit)
                return Self.outcome(of: result)
            } catch CommandError.linkLost {
                return .notSent(Self.linkLostReason)
            } catch CommandError.timedOut {
                return .notSent(Self.noAnswerReason)
            } catch {
                return .notSent(Self.changedFirstReason)
            }
        case .phone(let key)? where control.kind == .button:
            if key == Self.monitorHzAction {
                // Written as an edit of the FPS row, under its own rules.
                guard let fps = currentControl(Self.fpsControlId, in: admission.category),
                      let range = fps.range, let rate = phone?.screenRefreshRate else {
                    return .notSent(Self.notOnThisPhoneReason)
                }
                let held = min(max(Double(rate), range.minimum), range.maximum)
                return await edit(fps, in: admission.category, to: .integer(Int64(held.rounded())))
            }
            guard let phone, phone.perform(key, argument: phoneArgument(control)) else {
                return .notSent(Self.notOnThisPhoneReason)
            }
            return .applied
        case .phone(let key)?:
            guard let value, Self.fitsPhone(value, control) else {
                return .notSent(Self.outOfRangeReason)
            }
            if let whole = value.whole, let reason = phone?.unavailableOptions(forPhoneKey: key)[whole] {
                return .notSent(reason)
            }
            guard let phone, phone.set(value, forPhoneKey: key) else {
                return .notSent(Self.notOnThisPhoneReason)
            }
            return .applied
        default:
            return .notSent(Self.notOnThisPhoneReason)
        }
    }

    /// Admits and sends one edit at once: a switch, a choice, a stepper's
    /// step, a slider let go, a button.
    public func edit(_ control: SetupDescription.Control, in category: String,
                     to value: SetupValue?, asked: SetupQuestion? = nil,
                     onLateOutcome: (@MainActor (SetupEditOutcome) -> Void)? = nil) async -> SetupEditOutcome {
        switch admit(control, in: category) {
        case .failure(let refusal):
            return .notSent(refusal.reason)
        case .success(let admission):
            return await perform(admission, value: value, asked: asked, onLateOutcome: onLateOutcome)
        }
    }

    /// The row's question (V12's `confirm`) as it is asked now, its `%1`
    /// replaced by the value the command's first text argument resolves to
    /// (setup description: "the value the command's first text argument
    /// resolves to (the profile name)"). Nil when the row asks nothing; a
    /// refusal when there is nothing to name, so no question shows a bare
    /// `%1`. Pass the result to ``edit(_:in:to:asked:)`` on Yes.
    public func question(for control: SetupDescription.Control, in category: String,
                         value: SetupValue?) -> Result<SetupQuestion, SetupRefusal>? {
        guard let confirm = control.confirm else {
            return nil
        }
        guard confirm.contains(Self.questionPlaceholder) else {
            return .success(SetupQuestion(text: confirm, named: nil))
        }
        let sliceId = Self.needsSlice(control) ? selectedSlice() : nil
        guard case .command(let command)? = control.binding,
              let arguments = arguments(command, control: control, category: category, sliceId: sliceId, value: value),
              let named = Self.firstText(arguments) else {
            return .failure(SetupRefusal(reason: Self.valueMissingReason))
        }
        return .success(SetupQuestion(text: confirm.replacingOccurrences(of: Self.questionPlaceholder, with: named),
                                      named: named))
    }

    /// Edits admitted and not yet finished.
    public var openAdmissionCount: Int {
        admissions.filter { !$0.isRevoked }.count
    }

    // MARK: Availability

    func unavailableReason(_ control: SetupDescription.Control, in category: String,
                                   sliceId: Int?) -> String? {
        if control.specialized != nil {
            return Self.notOnThisPhoneReason
        }
        if control.kind == .readout {
            // Read-only even when the source property is writable.
            return Self.readingReason
        }
        if control.metadataIssue != nil {
            return Self.unreadableReason
        }
        if let pending = control.modern?.pendingReason {
            return pending
        }
        if let availability = control.availability, !availability.enabled {
            return availability.reason
        }
        guard store.isSnapshotComplete, !store.isStale else {
            return Self.notConnectedReason
        }
        guard currentControl(control.id, in: category) == control else {
            return Self.updatingReason
        }
        if let reason = gateReason(control.gate) {
            return lowPassGateReason(control, reason)
        }
        if let reason = modernReason(control, sliceId: sliceId) {
            return reason
        }
        if let reason = radioSettingDependencyReason(control, in: category) {
            return reason
        }
        switch control.binding {
        case .filterPresets?:
            return filterPresets.reason(for: control, in: category)
        case .setting(let key)?:
            guard SettingsScope.of(key) == .station else {
                // The Core keeps only station settings; this one would never be sent.
                return Self.notOnThisPhoneReason
            }
            guard settings.currentSnapshotIdentity != nil else {
                return Self.notConnectedReason
            }
            guard let text = settings.value(key), SetupValueCoding.decodeSetting(text, for: control) != nil else {
                return Self.valueMissingReason
            }
            if let dependency = control.enabledWhen {
                guard let chosen = settings.value(dependency.setting), dependency.oneOf.contains(chosen) else {
                    return Self.dependsReason
                }
            }
            return nil
        case .property(let reference)?:
            if reference.object == Self.activeSliceAlias, resolve(reference.object, sliceId: sliceId) == nil {
                return Self.noSliceReason
            }
            guard value(of: control, sliceId: sliceId) != nil else {
                return Self.valueMissingReason
            }
            return nil
        case .command(let command)?:
            if Self.needsSlice(control), resolve(Self.activeSliceAlias, sliceId: sliceId) == nil {
                return Self.noSliceReason
            }
            if let valueProperty = command.valueProperty, read(valueProperty, sliceId: sliceId) == nil {
                return Self.valueMissingReason
            }
            for argument in command.arguments.values {
                switch argument {
                case .property(let reference):
                    if read(reference, sliceId: sliceId) == nil { return Self.valueMissingReason }
                case .prompt:
                    guard let prompt = control.modern?.profilePrompt,
                          read(prompt.initial, sliceId: sliceId) != nil,
                          read(prompt.namesFrom, sliceId: sliceId) != nil else { return Self.unreadableReason }
                case .row, .edit:
                    return Self.unreadableReason
                case .control(let id):
                    guard let named = currentControl(id, in: category), controlValue(named, sliceId: sliceId) != nil else {
                        return Self.valueMissingReason
                    }
                case .controlValue:
                    if control.kind == .button { return Self.unreadableReason }
                default:
                    break
                }
            }
            return nil
        case .phone(let key)? where control.kind == .button:
            if key == Self.monitorHzAction {
                // Greyed whenever the FPS row is.
                guard phone?.screenRefreshRate != nil,
                      let fps = currentControl(Self.fpsControlId, in: category) else {
                    return Self.notOnThisPhoneReason
                }
                return unavailableReason(fps, in: category, sliceId: sliceId)
            }
            guard let phone, phone.canPerform(key) else {
                return phone?.unavailableReason(forPhoneKey: key) ?? Self.notOnThisPhoneReason
            }
            return nil
        case .radioSetting(let path)?:
            return radioSettingReason(control, path: path)
        case .phone(let key)?:
            if let reason = phone?.unavailableReason(forPhoneKey: key) {
                return reason
            }
            guard let phone, phone.value(forPhoneKey: key) != nil else {
                return Self.notOnThisPhoneReason
            }
            if let dependency = control.phoneDependency {
                guard let chosen = phone.value(forPhoneKey: dependency.phone),
                      dependency.oneOf.contains(where: { Self.matches(chosen, $0) }) else {
                    return Self.dependsReason
                }
            }
            return nil
        default:
            return Self.notOnThisPhoneReason
        }
    }

    func gateReason(_ gate: SetupDescription.Gate?) -> String? {
        guard let gate else {
            return nil
        }
        if let capability = gate.capability, store.capabilityVersion(capability) < (gate.minimum ?? 1) {
            return Self.needsNewerCoreReason
        }
        if gate.transmit == true, store.capabilities["txPermitted"] != .bool(true) {
            if case .text(let reason)? = store.capabilities["txRefusalReason"], !reason.isEmpty {
                return reason
            }
            return Self.transmitNotAllowedReason
        }
        if gate.micLine == true, microphoneLineOpen?() != true {
            return Self.micLineReason
        }
        // A board flag the radio lacks removes its control at the Core; a
        // board gate that arrives is one the radio has.
        if gate.offAir == true {
            watch(Self.txStateKey)
            guard let state = store.object(Self.txStateKey)?.values else {
                return Self.transmitStateUnknownReason
            }
            return Self.onAir(state)
        }
        return nil
    }

    /// Nil when the radio is off the air; a reason otherwise. keyed, tuning
    /// and twoTone must be sent; txEnding counts when the Core sends it.
    static func onAir(_ state: [String: MirrorValue]) -> String? {
        var flags: [Bool] = []
        for name in ["keyed", "tuning", "twoTone"] {
            guard case .bool(let on)? = state[name] else {
                return transmitStateUnknownReason
            }
            flags.append(on)
        }
        if let ending = state["txEnding"] {
            guard case .bool(let on) = ending else { return transmitStateUnknownReason }
            flags.append(on)
        }
        return flags.contains(true) ? onAirReason : nil
    }

    // MARK: Values

    func value(of control: SetupDescription.Control, sliceId: Int?) -> SetupValue? {
        switch control.binding {
        case .setting(let key)?:
            return settings.value(key).flatMap { SetupValueCoding.decodeSetting($0, for: control) }
        case .property(let reference)?:
            return read(reference, sliceId: sliceId).flatMap { modernDecode($0, for: control, sliceId: sliceId) }
        case .command(let command)?:
            return command.valueProperty.flatMap { read($0, sliceId: sliceId) }
                .flatMap { modernDecode($0, for: control, sliceId: sliceId) }
        case .radioSetting(let path)?:
            return radioSettingValue(control, path: path)
        case .phone(let key)?:
            return phone?.value(forPhoneKey: key)
        default:
            return nil
        }
    }

    func read(_ reference: SetupDescription.PropertyReference, sliceId: Int?) -> MirrorValue? {
        guard let key = resolve(reference.object, sliceId: sliceId) else {
            return nil
        }
        watch(key)
        return store.object(key)?[reference.name]
    }

    /// `slice:active` is the selected slice of this session; any other
    /// object key is itself, when the Core has created it.
    func resolve(_ object: String, sliceId: Int?) -> String? {
        if object == Self.activeSliceAlias {
            guard let sliceId else { return nil }
            let key = "slice:\(sliceId)"
            guard store.object(key)?.className == Self.sliceClass else { return nil }
            return key
        }
        return store.object(object) == nil ? nil : object
    }

    func arguments(_ command: SetupDescription.Command, admission: SetupAdmission,
                           value: SetupValue?) -> [CommandArgument]? {
        arguments(command, control: admission.control, category: admission.category, sliceId: admission.sliceId,
                  value: value)
    }

    /// The command's arguments in name order, each resolved now.
    func arguments(_ command: SetupDescription.Command, control: SetupDescription.Control, category: String,
                   sliceId: Int?, value: SetupValue?) -> [CommandArgument]? {
        var result: [CommandArgument] = []
        for (name, argument) in command.arguments.sorted(by: { $0.key < $1.key }) {
            let typed: MirrorValue
            switch argument {
            case .literal(.bool(let on)):
                typed = .bool(on)
            case .literal(.integer(let whole)):
                typed = .int(whole)
            case .literal(.decimal(let number)):
                typed = .double(number)
            case .literal(.text(let text)):
                typed = .text(text)
            case .controlValue:
                guard let value, let converted = modernWire(value, for: control) else {
                    return nil
                }
                typed = converted
            case .control(let id):
                // The staged row's value now, or what it started from.
                guard let named = currentControl(id, in: category),
                      let held = controlValue(named, sliceId: sliceId),
                      let converted = modernWire(held, for: named) else {
                    return nil
                }
                typed = converted
            case .property(let reference):
                // Resolved now, from this same session, just before dispatch.
                guard let current = read(reference, sliceId: sliceId) else { return nil }
                typed = current
            case .selectedOwnedSliceId:
                guard let sliceId else { return nil }
                typed = .int(Int64(sliceId))
            case .prompt:
                guard control.modern?.profilePrompt != nil, let text = value?.text,
                      !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return nil }
                typed = .text(text.trimmingCharacters(in: .whitespacesAndNewlines))
            case .row, .edit:
                return nil
            }
            result.append(CommandArgument(name: name, value: typed))
        }
        return result
    }

    /// The value of the first text argument in name order, which a
    /// question's `%1` stands for.
    static func firstText(_ arguments: [CommandArgument]) -> String? {
        for argument in arguments {
            if case .text(let text) = argument.value {
                return text
            }
        }
        return nil
    }

    static let questionPlaceholder = "%1"

    // MARK: Freshness

    func currentControl(_ id: String, in category: String) -> SetupDescription.Control? {
        guard let description = feed.description(for: category) else {
            return nil
        }
        for page in description.pages {
            for section in page.sections {
                if let control = section.controls.first(where: { $0.id == id }) {
                    return control
                }
            }
        }
        return nil
    }

    func deliverLate(_ outcome: SetupEditOutcome, admission: SetupAdmission,
                     to receive: (@MainActor (SetupEditOutcome) -> Void)?) {
        guard let receive, !admission.isRevoked,
              store.snapshotIdentity == admission.mirrorIdentity,
              feed.isCurrent(admission.generation, category: admission.category),
              admission.sliceId == nil || admission.sliceId == selectedSlice() else { return }
        if outcome == .awaitingConfirmation { return }
        // Loss can arrive during retirement, after ready state is withdrawn.
        // All answered outcomes still require current description/admission.
        let lost = outcome == .notSent(Self.linkLostReason) || outcome == .notSent(PropertyWriteOutcome.linkLost.reason)
        guard lost || stillCurrent(admission, allowsMissingValue: true) else { return }
        receive(outcome)
    }

    static func settingOutcome(_ outcome: SettingsWriteOutcome) -> SetupEditOutcome {
        switch outcome {
        case .accepted: return .applied
        case .rejected(let reason):
            return waitsForConfirmation(reason) ? .awaitingConfirmation : .refused(reason.isEmpty ? refusedReason : reason)
        case .keptOnThisDevice: return .refused(refusedReason)
        case .notSent: return .notSent(changedFirstReason)
        case .linkLost: return .notSent(linkLostReason)
        case .notConfirmed: return .notSent(PropertyWriteOutcome.notConfirmed.reason)
        }
    }

    static func propertyOutcome(_ outcome: PropertyWriteOutcome) -> SetupEditOutcome {
        if outcome.accepted { return .applied }
        if outcome.heldForQuestion { return .awaitingConfirmation }
        if !outcome.answeredByCore { return .notSent(outcome.reason) }
        return .refused(outcome.reason.isEmpty ? refusedReason : outcome.reason)
    }

    /// Everything the edit was admitted against still holds.
    func stillCurrent(_ admission: SetupAdmission, allowsMissingValue: Bool = false) -> Bool {
        guard !admission.isRevoked, store.isSnapshotComplete, !store.isStale,
              store.snapshotIdentity == admission.mirrorIdentity,
              feed.isCurrent(admission.generation, category: admission.category),
              admission.sliceId == nil || admission.sliceId == selectedSlice() else {
            return false
        }
        if admission.special?.pa != nil { return paStillCurrent(admission) }
        if admission.control.specialized != nil {
            return specialStillCurrent(admission)
        }
        let unavailable = unavailableReason(admission.control, in: admission.category, sliceId: admission.sliceId)
        // A current refusal can itself remove the setting. Missing returned
        // data does not retire its owner or suppress the Core's exact reason.
        guard unavailable == nil || (allowsMissingValue && unavailable == Self.valueMissingReason) else { return false }
        if case .setting? = admission.control.binding {
            guard let identity = admission.settingsIdentity, settings.isCurrent(identity) else { return false }
            if let dependency = admission.control.enabledWhen {
                guard let value = settings.value(dependency.setting), dependency.oneOf.contains(value) else { return false }
            }
        }
        if case .property(let reference)? = admission.control.binding {
            guard let key = admission.objectKey,
                  resolve(reference.object, sliceId: admission.sliceId) == key else { return false }
        }
        return true
    }

    /// A `settings.reject` or `property.result` the Core holds for its
    /// question: the one reason it gives a held change (the link document,
    /// sections 7.3 and 8.1; StationClient::isAwaitingConfirmation).
    static func waitsForConfirmation(_ reason: String) -> Bool {
        reason == SeveralDevices.waitingReason
    }

    /// A command's answer as an edit's end: a change held for a question,
    /// by its phase or its words (SeveralDevices.waitsForConfirmation), is
    /// not a refusal.
    static func outcome(of result: CommandResult) -> SetupEditOutcome {
        if result.accepted {
            return .applied
        }
        if SeveralDevices.waitsForConfirmation(result) {
            return .awaitingConfirmation
        }
        return .refused(result.reason.isEmpty ? refusedReason : result.reason)
    }

    /// V12's Get Monitor Hz, which writes the FPS row.
    public nonisolated static let monitorHzAction = "getMonitorHz"
    public nonisolated static let fpsControlId = "display.spectrumDefaults.fps"

    /// A kept value is the dependency's literal.
    static func matches(_ value: SetupValue, _ literal: SetupDescription.Literal) -> Bool {
        switch literal {
        case .bool(let on):
            return value.flag == on
        case .integer(let whole):
            return value.whole == whole
        case .decimal(let number):
            return value.number == number
        case .text(let text):
            return value.text == text
        }
    }

    /// A phone key's value matches its control's kind, range and choices.
    static func fitsPhone(_ value: SetupValue, _ control: SetupDescription.Control) -> Bool {
        switch control.kind {
        case .toggle?:
            return value.flag != nil
        case .colour?:
            guard let text = value.text else { return false }
            return text.utf8.count == 9 && text.first == "#" && text.dropFirst().allSatisfy(\.isHexDigit)
        case .text?:
            return value.text != nil
        case .integer?, .decimal?, .slider?, .choice?:
            return SetupValueCoding.fits(value, control)
        default:
            return false
        }
    }

    private static func needsSlice(_ control: SetupDescription.Control) -> Bool {
        switch control.binding {
        case .property(let reference)?:
            return reference.object == activeSliceAlias
        case .command(let command)?:
            if command.valueProperty?.object == activeSliceAlias { return true }
            return command.arguments.values.contains { argument in
                switch argument {
                case .selectedOwnedSliceId:
                    return true
                case .property(let reference):
                    return reference.object == activeSliceAlias
                default:
                    return false
                }
            }
        default:
            return false
        }
    }

    // MARK: Watching

    private func retireAll() {
        revokeAdmissions()
        stagedValues = [:]
        stagedRows = [:]
        changed()
    }

    private func revokeAdmissions() {
        retirePaResults()
        for admission in admissions {
            admission.revoke()
        }
        admissions = []
    }

    /// The Core's descriptions changed: edits in flight end, and a held
    /// value goes only when its own category's description changed.
    private func descriptionsChanged() {
        revokeAdmissions()
        for (id, row) in stagedRows where feed.description(for: row.category) != row.description {
            stagedValues[id] = nil
            stagedRows[id] = nil
        }
        changed()
    }

    private func selectionChanged() {
        // Only a held value of the selected slice's row belongs to it.
        for (id, row) in stagedRows where row.slices {
            stagedValues[id] = nil
            stagedRows[id] = nil
        }
        for admission in admissions where admission.sliceId != nil || admission.special?.tracksSelection == true {
            admission.revoke()
        }
        admissions.removeAll { $0.isRevoked }
        changed()
    }

    private func objectsChanged() {
        // A PA facade removed/replaced at handoff cannot be the captured
        // object's bank, even if its next body happens to look identical.
        retirePaResults()
        for admission in admissions where admission.special?.pa != nil { admission.revoke() }
        // The store publishes before it changes: look again once it has.
        Task { @MainActor [weak self] in
            guard let self else { return }
            // The description object and the transmit state retire edits
            // themselves; other objects only redraw.
            self.watch(SetupDescriptionFeed.objectKey)
            self.watch(Self.txStateKey)
            for key in self.objectWatches.keys where self.store.object(key) == nil {
                self.objectWatches[key] = nil
            }
        }
        changed()
    }

    func watch(_ key: String) {
        guard let object = store.object(key) else {
            objectWatches[key] = nil
            return
        }
        let id = ObjectIdentifier(object)
        guard objectWatches[key]?.id != id else {
            return
        }
        let watch = object.$values.dropFirst().sink { [weak self] values in
            guard let self else { return }
            if key == SetupDescriptionFeed.objectKey {
                // The feed reads the change next; its generation retires
                // the held values whose category it changed.
                self.revokeAdmissions()
                self.changed()
            } else if key == Self.txStateKey {
                self.transmitStateChanged(values)
            } else if key == Self.paProfilesKey {
                self.paProfilesChanged(values)
            } else if key == Self.notchesKey {
                self.notchesChanged(values)
            } else {
                self.changed()
            }
        }
        objectWatches[key] = (id, watch)
    }

    private func transmitStateChanged(_ values: [String: MirrorValue]) {
        if paResultOwners.values.contains(where: { $0.special?.pa?.transmitState != values }) {
            retirePaResults()
        }
        for admission in admissions {
            if let context = admission.special?.pa, context.transmitState != values { admission.revoke() }
        }
        if Self.onAir(values) != nil {
            for admission in admissions where admission.control.gate?.offAir == true
                || admission.special?.offAir == true {
                admission.revoke()
            }
            admissions.removeAll { $0.isRevoked }
        }
        changed()
    }

    func changed() {
        objectWillChange.send()
    }
}
