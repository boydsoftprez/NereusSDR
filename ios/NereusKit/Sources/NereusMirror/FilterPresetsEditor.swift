// NereusSDR for iOS: gesture-bound, sequential Core settings for Filter Presets
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusLink
import NereusModels

public struct SetupFilterPreset: Equatable, Hashable, Sendable, Identifiable {
    public let slot: Int
    public let name: String
    public let lowHz: Int
    public let highHz: Int
    public var id: Int { slot }
    public var widthHz: Int { abs(highHz - lowHz) }
}

/// A prompt, pad or field retains this owner from opening through its answer.
/// Retirement is permanent, even if a session, descriptor or row returns to A.
@MainActor
public final class FilterPresetsGesture {
    public let owner = UUID()
    public let question: String?
    let control: SetupDescription.Control
    let mode: String
    let slot: Int?
    let snapshot: UInt64
    let settingsSnapshot: UInt64
    let sender: SettingsProxyClient.BoundSender
    let generation: UInt64
    let catalogObject: ObjectIdentifier
    let capabilities: [String: MirrorValue]
    let permit = CommandSendPermit()
    let capturedBank: [String: [SetupFilterPreset]]
    var bank: [String: [SetupFilterPreset]]
    var confirmed: [String: String]
    var activeKey: String?
    var allowedRows: [String: Set<SetupFilterPreset>] = [:]
    var awaitingDefault: Set<String> = []
    var performing = false
    var sent = false
    public var isRevoked: Bool { permit.isRevoked }
    public func revoke() { permit.revoke() }

    init(control: SetupDescription.Control, mode: String, slot: Int?, snapshot: UInt64,
         settingsSnapshot: UInt64, sender: @escaping SettingsProxyClient.BoundSender, generation: UInt64, catalogObject: ObjectIdentifier,
         capabilities: [String: MirrorValue], bank: [String: [SetupFilterPreset]], confirmed: [String: String]) {
        self.control = control; self.mode = mode; self.slot = slot; self.snapshot = snapshot
        self.settingsSnapshot = settingsSnapshot; self.sender = sender; self.generation = generation; self.catalogObject = catalogObject
        self.capabilities = capabilities; self.capturedBank = bank; self.bank = bank; self.confirmed = confirmed
        question = control.confirm?.replacingOccurrences(of: "%1", with: mode)
    }
}

/// Values come only from the Core catalogue. SettingsProxyClient's optimistic
/// cache is never rendered as a preset or used to prepare another gesture.
@MainActor
public final class FilterPresetsEditor: ObservableObject {
    @Published public var selectedSlot: Int? { didSet { if oldValue != selectedSlot { retire() } } }
    private unowned let dispatcher: SetupControlDispatcher
    private var owners: [FilterPresetsGesture] = []
    private var subscriptions: Set<AnyCancellable> = []
    private var catalogWatch: AnyCancellable?
    private var descriptionWatch: AnyCancellable?
    private weak var watchedDescription: MirrorObject?
    private weak var watchedCatalog: MirrorObject?

    init(dispatcher: SetupControlDispatcher) {
        self.dispatcher = dispatcher
        let store = dispatcher.store
        store.$isSnapshotComplete.dropFirst().sink { [weak self] _ in self?.retire() }.store(in: &subscriptions)
        store.$isStale.dropFirst().sink { [weak self] _ in self?.retire() }.store(in: &subscriptions)
        store.$capabilities.dropFirst().sink { [weak self] _ in self?.retire() }.store(in: &subscriptions)
        store.$objectKeys.dropFirst().sink { [weak self] _ in
            self?.retire()
            Task { @MainActor [weak self] in self?.watchCatalog(); self?.watchDescription(); self?.objectWillChange.send() }
        }.store(in: &subscriptions)
        dispatcher.feed.$generation.dropFirst().sink { [weak self] _ in self?.retire() }.store(in: &subscriptions)
        dispatcher.settings.$currentSnapshotIdentity.dropFirst().sink { [weak self] _ in self?.retire() }.store(in: &subscriptions)
        dispatcher.settings.coreChanges.sink { [weak self] change in self?.settingsChanged(change) }.store(in: &subscriptions)
        dispatcher.phone?.changes.sink { [weak self] _ in
            guard let self else { return }
            for owner in self.owners where self.mode != owner.mode { owner.revoke() }
            self.objectWillChange.send()
        }.store(in: &subscriptions)
        watchCatalog()
        watchDescription()
    }

    public var mode: String? {
        guard let picker = dispatcher.currentControl("dsp.filterPresets.mode", in: "dsp"),
              picker.metadataIssue == nil, picker.binding == .phone("filterPresetsMode"),
              let id = dispatcher.state(of: picker, in: "dsp").value?.whole else { return nil }
        return picker.options?.first { $0.value == id }?.label
    }

    public var rows: [SetupFilterPreset] { mode.flatMap { bank()?[$0] } ?? [] }

    public func reason(for control: SetupDescription.Control, in category: String = "dsp") -> String? {
        guard category == "dsp", case .filterPresets? = control.binding, control.modern?.pendingReason == nil else {
            return SetupControlDispatcher.unreadableReason
        }
        if let reason = dispatcher.specializedReason(control, in: category) { return reason }
        guard dispatcher.settings.currentSnapshotIdentity != nil else { return SetupControlDispatcher.notConnectedReason }
        guard mode != nil, !rows.isEmpty else { return SetupControlDispatcher.catalogueWaitingReason }
        if case .filterPresets(let binding)? = control.binding, binding.action == .resetRow,
           !rows.contains(where: { $0.slot == selectedSlot }) { return SetupControlDispatcher.valueMissingReason }
        return nil
    }

    public func admit(_ control: SetupDescription.Control, slot: Int? = nil) -> Result<FilterPresetsGesture, SetupRefusal> {
        if let reason = reason(for: control) { return .failure(SetupRefusal(reason: reason)) }
        guard let mode, let bank = bank(), let object = dispatcher.store.object(CatalogFeed.objectKey),
              let settingsSnapshot = dispatcher.settings.currentSnapshotIdentity,
              let sender = dispatcher.settings.admittedSender(),
              case .filterPresets(let binding)? = control.binding else {
            return .failure(SetupRefusal(reason: SetupControlDispatcher.unreadableReason))
        }
        let chosen = binding.action == .resetRow ? selectedSlot : slot
        if binding.action == .table || binding.action == .resetRow {
            guard bank[mode]?.contains(where: { $0.slot == chosen }) == true else {
                return .failure(SetupRefusal(reason: SetupControlDispatcher.valueMissingReason))
            }
        }
        // Only one gesture may write the bank. A new gesture also replaces a
        // timed-out result owner; its late answer may still reconcile Core data.
        retire()
        let confirmed = dispatcher.settings.values.filter { $0.key.hasPrefix("filters/") }
        guard !dispatcher.settings.hasPendingFilterPresets else {
            return .failure(SetupRefusal(reason: SetupControlDispatcher.noAnswerReason))
        }
        let owner = FilterPresetsGesture(control: control, mode: mode, slot: chosen,
            snapshot: dispatcher.store.snapshotIdentity, settingsSnapshot: settingsSnapshot, sender: sender,
            generation: dispatcher.feed.generation, catalogObject: ObjectIdentifier(object),
            capabilities: dispatcher.store.capabilities, bank: bank,
            confirmed: Dictionary(uniqueKeysWithValues: confirmed.keys.compactMap { key in
                dispatcher.settings.confirmedValue(key).map { (key, $0) }
            }))
        owners = [owner]
        watchCatalog()
        watchDescription()
        return .success(owner)
    }

    public func owns(_ owner: FilterPresetsGesture) -> Bool {
        let store = dispatcher.store
        return owners.contains { $0 === owner } && !owner.isRevoked && store.isSnapshotComplete && !store.isStale
            && store.snapshotIdentity == owner.snapshot && dispatcher.settings.isCurrent(owner.settingsSnapshot)
            && dispatcher.feed.isCurrent(owner.generation, category: "dsp") && mode == owner.mode
            && store.capabilities == owner.capabilities
            && dispatcher.currentControl(owner.control.id, in: "dsp") == owner.control
            && store.object(CatalogFeed.objectKey).map(ObjectIdentifier.init) == owner.catalogObject
    }

    public func retire() {
        for owner in owners { owner.revoke() }
        owners = []
        objectWillChange.send()
    }

    /// A field commits the full captured row: FilterPresetStore.cpp:92-95
    /// reads an override only when name, low and high all exist. Order matches
    /// persistPreset :58-61. No rollback or atomic gesture is claimed.
    public func edit(_ owner: FilterPresetsGesture, field: String, value: SetupValue,
                     onLateOutcome: (@MainActor (SetupEditOutcome) -> Void)? = nil) async -> SetupEditOutcome {
        guard owns(owner), case .filterPresets(let binding)? = owner.control.binding, binding.action == .table,
              let slot = owner.slot, let row = owner.capturedBank[owner.mode]?.first(where: { $0.slot == slot }) else {
            return .notSent(SetupControlDispatcher.changedFirstReason)
        }
        var name = row.name; var low = row.lowHz; var high = row.highHz
        if field == "name", let text = value.text, let limit = binding.column(field)?.maxLength {
            guard text.utf16.count <= limit else { return .notSent(SetupControlDispatcher.tooLongReason) }
            // FilterPresetsSetupPage.cpp:336-340: trim and blank -> F<slot+1>.
            // Core echo/catalogue, rather than this normalized draft, owns display.
            name = text.trimmingCharacters(in: .whitespacesAndNewlines)
            if name.isEmpty { name = "F\(slot + 1)" }
        } else if (field == "lowHz" || field == "highHz"), case .integer(let integer) = value,
                  let range = binding.column(field)?.range,
                  Double(integer) >= range.minimum, Double(integer) <= range.maximum {
            if field == "lowHz" { low = Int(integer) } else { high = Int(integer) }
        } else { return .notSent(SetupControlDispatcher.outOfRangeReason) }
        return await run(owner, operations: writes(mode: owner.mode, slot: slot, name: name, low: low, high: high), onLateOutcome: onLateOutcome)
    }

    public func move(_ owner: FilterPresetsGesture, direction: Int,
                     onLateOutcome: (@MainActor (SetupEditOutcome) -> Void)? = nil) async -> SetupEditOutcome {
        guard owns(owner), case .filterPresets(let binding)? = owner.control.binding, binding.action == .table,
              (direction == -1 || direction == 1), let rows = owner.capturedBank[owner.mode], let index = rows.firstIndex(where: { $0.slot == owner.slot }),
              rows.indices.contains(index + direction) else { return .notSent(SetupControlDispatcher.outOfRangeReason) }
        let first = rows[index]; let second = rows[index + direction]
        return await run(owner, operations:
            writes(mode: owner.mode, slot: first.slot, name: second.name, low: second.lowHz, high: second.highHz)
            + writes(mode: owner.mode, slot: second.slot, name: first.name, low: first.lowHz, high: first.highHz), onLateOutcome: onLateOutcome)
    }

    public func reset(_ owner: FilterPresetsGesture, confirmed question: String? = nil,
                      onLateOutcome: (@MainActor (SetupEditOutcome) -> Void)? = nil) async -> SetupEditOutcome {
        guard owns(owner), case .filterPresets(let binding)? = owner.control.binding, binding.action != .table,
              owner.question == question else { return .notSent(SetupControlDispatcher.changedFirstReason) }
        let modes: [String]
        if binding.action == .resetAll {
            // Descriptor modes are the exact store resetAll's LSB..DRM set;
            // do not reset a catalogue's additional RADE modes.
            modes = dispatcher.currentControl("dsp.filterPresets.mode", in: "dsp")?.options?.map(\.label) ?? []
        } else { modes = [owner.mode] }
        // FilterPresetStore.cpp:73-79 clears all ten possible overrides,
        // including slots absent from the current mode's catalogue.
        let slots = binding.action == .resetRow ? [owner.slot].compactMap { $0 } : Array(0..<10)
        let operations = modes.flatMap { mode in slots.flatMap { slot in
            ["name", "low", "high"].map { Operation(key: "filters/\(mode)/\(slot)/\($0)", value: nil) }
        } }
        return await run(owner, operations: operations, onLateOutcome: onLateOutcome)
    }

    private struct Operation { let key: String; let value: String? }
    private func writes(mode: String, slot: Int, name: String, low: Int, high: Int) -> [Operation] {
        let prefix = "filters/\(mode)/\(slot)/"
        return [Operation(key: prefix + "name", value: name), Operation(key: prefix + "low", value: String(low)),
                Operation(key: prefix + "high", value: String(high))]
    }

    private func run(_ owner: FilterPresetsGesture, operations: [Operation],
                     onLateOutcome: (@MainActor (SetupEditOutcome) -> Void)?) async -> SetupEditOutcome {
        guard owns(owner), !owner.performing, !owner.sent else { return .notSent(SetupControlDispatcher.changedFirstReason) }
        owner.performing = true; owner.sent = true
        defer { owner.performing = false }
        for (index, operation) in operations.enumerated() {
            guard owns(owner) else { return .notSent(SetupControlDispatcher.changedFirstReason) }
            owner.activeKey = operation.key
            prepareCatalogueEcho(owner, operation: operation)
            let late: @MainActor (SettingsWriteOutcome) -> Void = { [weak self] result in
                guard let self, self.owns(owner) else { return }
                // A late answer settles only the last sent constituent key.
                // It never resumes a stopped sequence or calls a partial gesture applied.
                let outcome = result == .accepted && index < operations.count - 1
                    ? SetupEditOutcome.notSent(SetupControlDispatcher.noAnswerReason) : SetupControlDispatcher.settingOutcome(result)
                onLateOutcome?(outcome)
                self.objectWillChange.send()
            }
            let result: SettingsWriteOutcome
            if let value = operation.value {
                result = await dispatcher.settings.writeBound(operation.key, value, expectedSnapshotIdentity: owner.settingsSnapshot,
                    authority: owner.permit, capturedSender: owner.sender, onLateOutcome: late)
            } else {
                result = await dispatcher.settings.removeBound(operation.key, expectedSnapshotIdentity: owner.settingsSnapshot,
                    authority: owner.permit, capturedSender: owner.sender, onLateOutcome: late)
            }
            // Always check after the suspension, including the final key.
            guard owns(owner) else { return .notSent(SetupControlDispatcher.changedFirstReason) }
            guard result == .accepted else { return SetupControlDispatcher.settingOutcome(result) }
            owner.activeKey = nil
        }
        objectWillChange.send()
        return .applied
    }

    private func prepareCatalogueEcho(_ owner: FilterPresetsGesture, operation: Operation) {
        let parts = operation.key.split(separator: "/")
        guard parts.count == 4, let slot = Int(parts[2]), let current = owner.bank[String(parts[1])]?.first(where: { $0.slot == slot }) else { return }
        let key = parts.dropLast().joined(separator: "/")
        owner.allowedRows[key, default: []].insert(current)
        var projected = owner.confirmed
        projected[operation.key] = operation.value
        if let name = projected[key + "/name"], let lowText = projected[key + "/low"], let low = Int(lowText),
           let highText = projected[key + "/high"], let high = Int(highText) {
            owner.allowedRows[key, default: []].insert(SetupFilterPreset(slot: slot, name: name, lowHz: low, highHz: high))
        } else if operation.value == nil {
            // The wire catalogue has no separate factory bank. A removal
            // reveals the Core default once; its observed row then becomes
            // a strict candidate. Never synthesize DSP defaults on the phone.
            owner.awaitingDefault.insert(key)
        }
    }

    private func settingsChanged(_ change: SettingsProxyClient.CoreChange) {
        guard change.key.hasPrefix("filters/") else { return }
        for owner in owners {
            if change.ownEcho && owner.activeKey == change.key {
                owner.confirmed[change.key] = change.value
            } else if owner.confirmed[change.key] != change.value {
                owner.revoke()
            }
        }
        objectWillChange.send()
    }

    private func watchDescription() {
        let object = dispatcher.store.object(SetupDescriptionFeed.objectKey)
        guard object !== watchedDescription else { return }
        watchedDescription = object
        // Revoke at publication, before the feed's queued generation update;
        // a send suspended at final transport handoff must already be stale.
        descriptionWatch = object?.$values.dropFirst().sink { [weak self] _ in self?.retire() }
    }

    private func watchCatalog() {
        let object = dispatcher.store.object(CatalogFeed.objectKey)
        guard object !== watchedCatalog else { return }
        watchedCatalog = object
        catalogWatch = object?.$values.dropFirst().sink { [weak self] values in
            guard let self else { return }
            let next = self.bank(values)
            for owner in self.owners {
                guard let next, Set(next.keys) == Set(owner.bank.keys) else { owner.revoke(); continue }
                for (mode, rows) in owner.bank {
                    guard let current = next[mode], current.map(\.slot) == rows.map(\.slot) else { owner.revoke(); continue }
                    for (old, new) in zip(rows, current) where old != new {
                        // The Core emits catalogue updates while each key is
                        // applied. Only rows already touched by this sequence
                        // can move; outside settings changes revoke separately.
                        let key = "filters/\(mode)/\(old.slot)"
                        if owner.allowedRows[key]?.contains(new) == true {
                            owner.allowedRows[key] = [new]
                        } else if owner.awaitingDefault.remove(key) != nil {
                            owner.allowedRows[key] = [new]
                        } else { owner.revoke() }
                    }
                }
                owner.bank = next
            }
            self.objectWillChange.send()
        }
    }

    private func bank(_ values: [String: MirrorValue]? = nil) -> [String: [SetupFilterPreset]]? {
        let store = dispatcher.store
        guard store.isSnapshotComplete, !store.isStale,
              let object = store.object(CatalogFeed.objectKey), object.className == CatalogFeed.className,
              case .text(let json)? = (values ?? object.values)["json"],
              let catalog = StationCatalog.parse(json: json) else { return nil }
        var result: [String: [SetupFilterPreset]] = [:]
        for (mode, presets) in catalog.filterPresets {
            guard Set(presets.map(\.slot)).count == presets.count,
                  presets.allSatisfy({ (0..<10).contains($0.slot) && $0.lowHz.isFinite && $0.highHz.isFinite
                    && $0.lowHz.rounded() == $0.lowHz && $0.highHz.rounded() == $0.highHz
                    && (-10000...10000).contains($0.lowHz) && (-10000...10000).contains($0.highHz) }) else { return nil }
            result[mode] = presets.map { SetupFilterPreset(slot: $0.slot, name: $0.label, lowHz: Int($0.lowHz), highHz: Int($0.highHz)) }
        }
        return result
    }
}
