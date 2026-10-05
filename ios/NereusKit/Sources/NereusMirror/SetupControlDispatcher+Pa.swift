// NereusSDR for iOS: PA profiles through the Core's nine typed verbs
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// A PA naming prompt belongs to the gesture that opened it. Keep this
/// value with that admission until Submit; a fresh admission cannot adopt it.
public struct SetupPaPrompt: Equatable, Sendable {
    public let title: String
    public let label: String
    public let defaultValue: String
    public let owner: UUID
}

public struct SetupPaQuestion: Equatable, Sendable {
    public let text: String
    public let owner: UUID
}

/// The table's values are always the Core's body, including while a
/// command is awaiting its answer. Availability is described per band.
public struct SetupPaPanel: Equatable, Sendable {
    public let grid: SetupDescription.PaProfileGrid?
    public let profiles: PaProfiles?
    public let reason: String?

    public func reason(for band: Int) -> String? {
        if let reason { return reason }
        guard let row = grid?.rows.first(where: { $0.band == band }) else {
            return SetupControlDispatcher.unreadableReason
        }
        return row.availability?.enabled == false ? row.availability?.reason : nil
    }
}

public extension SetupAdmission {
    /// The future PA row's result owner. It is independent of command IDs
    /// and survives a timeout only for this same gesture and session.
    var paOwner: UUID? { special?.pa?.owner }
}

public extension SetupControlDispatcher {
    /// A future row checks this before displaying an asynchronous result.
    /// Starting a newer gesture for the same row/cell retires the old result.
    func paOwnsOutcome(_ admission: SetupAdmission) -> Bool {
        guard !admission.isRevoked, let context = admission.special?.pa,
              paResultOwners[PaResultKey(admission)] === admission,
              store.isSnapshotComplete, !store.isStale, store.snapshotIdentity == admission.mirrorIdentity,
              feed.isCurrent(admission.generation, category: admission.category),
              store.capabilities == context.capabilities,
              store.object(Self.txStateKey)?.values == context.transmitState,
              let grid = paGrid(admission.control, in: admission.category), let current = corePaProfiles(grid),
              let object = store.object(grid.object), ObjectIdentifier(object) == context.objectIdentity else { return false }
        return current.active == context.profiles.active
    }

    nonisolated static let paProfileUnreadableReason = "The PA profiles from the Core could not be read."

    func paPanel(_ control: SetupDescription.Control, in category: String) -> SetupPaPanel {
        guard case .paProfileGrid(let grid)? = control.binding else {
            return SetupPaPanel(grid: nil, profiles: nil, reason: Self.unreadableReason)
        }
        // Stale values are never a new session's profile body.
        let profiles = store.isSnapshotComplete && !store.isStale ? corePaProfiles(grid) : nil
        let reason = paReason(control, in: category) ?? (profiles == nil ? Self.paProfileUnreadableReason : nil)
        return SetupPaPanel(grid: grid, profiles: profiles, reason: reason)
    }

    /// Admits lifecycle gestures, or one specific grid cell, before a
    /// prompt, confirmation or direct numeric entry is opened.
    func admitPa(_ control: SetupDescription.Control, in category: String,
                 band: Int? = nil, column: String? = nil) -> Result<SetupAdmission, SetupRefusal> {
        if let reason = paReason(control, in: category) { return .failure(SetupRefusal(reason: reason)) }
        guard let grid = paGrid(control, in: category), let profiles = corePaProfiles(grid),
              let object = store.object(grid.object) else {
            return .failure(SetupRefusal(reason: Self.paProfileUnreadableReason))
        }
        switch control.binding {
        case .paProfile?:
            guard band == nil, column == nil else { return .failure(SetupRefusal(reason: Self.unreadableReason)) }
        case .paProfileGrid?:
            guard let band, let column, grid.column(column) != nil,
                  grid.rows.contains(where: { $0.band == band }) else {
                return .failure(SetupRefusal(reason: Self.unreadableReason))
            }
            if let reason = paPanel(control, in: category).reason(for: band) {
                return .failure(SetupRefusal(reason: reason))
            }
        default: return .failure(SetupRefusal(reason: Self.unreadableReason))
        }
        guard let sender = captureSender() else { return .failure(SetupRefusal(reason: Self.notConnectedReason)) }
        watch(grid.object)
        var special = SetupAdmission.Special()
        special.pa = SetupAdmission.PaContext(owner: UUID(), profiles: profiles, capabilities: store.capabilities,
                                             transmitState: store.object(Self.txStateKey)?.values,
                                             objectIdentity: ObjectIdentifier(object), band: band, column: column)
        let admission = admitSpecial(control, in: category, sender: sender, special: special)
        let key = PaResultKey(admission)
        for earlier in admissions where earlier !== admission && earlier.special?.pa != nil && PaResultKey(earlier) == key {
            earlier.revoke()
        }
        // Result ownership outlives the pending send (including timeout),
        // so retire the previous row owner even after its defer removed it.
        paResultOwners[key]?.revoke()
        paResultOwners[key] = admission
        return .success(admission)
    }

    /// Core words with %1 filled from the profile captured at the gesture.
    func paPrompt(for admission: SetupAdmission) -> Result<SetupPaPrompt, SetupRefusal> {
        guard stillCurrent(admission), let context = admission.special?.pa,
              case .paProfile(let binding)? = admission.control.binding, let prompt = binding.prompt else {
            return .failure(SetupRefusal(reason: Self.changedFirstReason))
        }
        return .success(SetupPaPrompt(title: prompt.title, label: prompt.label,
                                     defaultValue: prompt.defaultValue.replacingOccurrences(of: "%1", with: context.profiles.active),
                                     owner: context.owner))
    }

    func paQuestion(for admission: SetupAdmission) -> Result<SetupPaQuestion, SetupRefusal> {
        guard stillCurrent(admission), let context = admission.special?.pa,
              let text = admission.control.confirm, case .paProfile? = admission.control.binding else {
            return .failure(SetupRefusal(reason: Self.changedFirstReason))
        }
        return .success(SetupPaQuestion(text: text.replacingOccurrences(of: "%1", with: context.profiles.active),
                                       owner: context.owner))
    }

    /// All arguments follow SessionCommandDispatcher::handlePaProfile.
    /// There is no revision argument on this wire contract: freshness is
    /// checked locally and at transport handoff, then Core owns the change.
    func performPa(_ admission: SetupAdmission, value: SetupValue? = nil,
                   prompt: SetupPaPrompt? = nil, confirmed: SetupPaQuestion? = nil,
                   onLateOutcome: (@MainActor (SetupEditOutcome) -> Void)? = nil) async -> SetupEditOutcome {
        defer { admissions.removeAll { $0 === admission } }
        guard stillCurrent(admission), let context = admission.special?.pa else {
            admission.revoke()
            return .notSent(Self.changedFirstReason)
        }
        let verb: String
        var arguments: [CommandArgument] = []
        switch admission.control.binding {
        case .paProfile(let binding)?:
            switch binding.action {
            case .active:
                guard let name = value?.text, context.profiles.names.contains(name) else {
                    return .notSent(Self.outOfRangeReason)
                }
                verb = "paProfile.select"
                arguments = [.init(name: "name", value: .text(name))]
            case .new, .copy:
                guard let prompt, case .success(let expected) = paPrompt(for: admission), prompt == expected,
                      let name = value?.text else { return .notSent(Self.changedFirstReason) }
                // The Core trims and validates the name against its complete
                // bank; the visible name list cannot replace that authority.
                verb = "paProfile." + binding.action.rawValue
                arguments = [.init(name: "name", value: .text(name))]
            case .delete, .reset:
                guard let confirmed, case .success(let expected) = paQuestion(for: admission), confirmed == expected,
                      value == nil else { return .notSent(Self.changedFirstReason) }
                verb = "paProfile." + binding.action.rawValue
                if binding.action == .delete {
                    arguments = [.init(name: "name", value: .text(context.profiles.active))]
                }
            }
        case .paProfileGrid(let grid)?:
            guard let band = context.band, let id = context.column, let column = grid.column(id), let value else {
                return .notSent(Self.unreadableReason)
            }
            arguments = [.init(name: "band", value: .int(Int64(band)))]
            if column.field == "useMax" {
                guard case .bool(let on) = value else { return .notSent(Self.outOfRangeReason) }
                verb = "paProfile.setUseMax"
                arguments.append(.init(name: "on", value: .bool(on)))
            } else {
                guard case .decimal(let number) = value, column.fits(number) else { return .notSent(Self.outOfRangeReason) }
                verb = column.field == "gain" ? "paProfile.setGain"
                    : column.field == "adjust" ? "paProfile.setAdjust" : "paProfile.setMaxPower"
                if let step = column.driveStep { arguments.append(.init(name: "step", value: .int(Int64(step)))) }
                arguments.append(.init(name: "value", value: .double(number)))
            }
        default: return .notSent(Self.unreadableReason)
        }
        // No local mutation on success: only the PaProfilesFacade echo
        // supplies values, active selection and factory provenance.
        let late: (@MainActor (SetupEditOutcome) -> Void)?
        if let receive = onLateOutcome {
            late = { [weak self] outcome in
                guard self?.paOwnsOutcome(admission) == true else { return }
                receive(outcome)
            }
        } else {
            late = nil
        }
        let sent = await send(admission, verb: verb, arguments: arguments, onLateOutcome: late)
        guard store.snapshotIdentity == admission.mirrorIdentity,
              feed.isCurrent(admission.generation, category: admission.category) else {
            return .notSent(Self.changedFirstReason)
        }
        return sent.outcome
    }
}

extension SetupControlDispatcher {
    struct PaResultKey: Hashable {
        let control: String
        let band: Int?
        let column: String?
        init(_ admission: SetupAdmission) {
            control = admission.control.id
            band = admission.special?.pa?.band
            column = admission.special?.pa?.column
        }
    }

    static let paProfilesKey = "paProfiles"

    /// These retained row owners survive performPa's pending-admission
    /// removal. Retirement is irreversible: restored values cannot revive
    /// an old result, and a later gesture installs a different admission.
    func retirePaResults() {
        for owner in paResultOwners.values { owner.revoke() }
        paResultOwners = [:]
    }

    func paGrid(_ control: SetupDescription.Control, in category: String) -> SetupDescription.PaProfileGrid? {
        if case .paProfileGrid(let grid)? = control.binding { return grid }
        guard case .paProfile? = control.binding,
              let table = currentControl("pa.gain.table", in: category), table.metadataIssue == nil,
              case .paProfileGrid(let grid)? = table.binding else { return nil }
        return grid
    }

    func corePaProfiles(_ grid: SetupDescription.PaProfileGrid) -> PaProfiles? {
        watch(grid.object)
        guard let object = store.object(grid.object), object.className == "PaProfilesFacade",
              case .text(let json)? = object["json"],
              case .int(let revision)? = object["revision"] else { return nil }
        return PaProfiles(json: json, revision: revision, grid: grid)
    }

    func paReason(_ control: SetupDescription.Control, in category: String) -> String? {
        guard category == "pa", control.metadataIssue == nil, control.modern?.pendingReason == nil,
              paGrid(control, in: category) != nil else { return Self.unreadableReason }
        // Use the actual Core's gate and availability. txPermitted=false
        // alone does not deny PA settings on a receive-only Core.
        if let reason = specializedReason(control, in: category) { return reason }
        guard let grid = paGrid(control, in: category), corePaProfiles(grid) != nil else {
            return Self.paProfileUnreadableReason
        }
        return nil
    }

    func paStillCurrent(_ admission: SetupAdmission) -> Bool {
        guard let context = admission.special?.pa, paReason(admission.control, in: admission.category) == nil,
              store.capabilities == context.capabilities,
              store.object(Self.txStateKey)?.values == context.transmitState,
              let grid = paGrid(admission.control, in: admission.category),
              let object = store.object(grid.object), ObjectIdentifier(object) == context.objectIdentity,
              corePaProfiles(grid) == context.profiles else { return false }
        if let band = context.band {
            return paPanel(admission.control, in: admission.category).reason(for: band) == nil
        }
        return true
    }

    /// Published before MirrorObject stores the values: revoke immediately,
    /// including a send paused between its last check and transport handoff.
    func paProfilesChanged(_ values: [String: MirrorValue]) {
        // Include timed-out/completed owners, which are no longer in admissions.
        for admission in paResultOwners.values {
            guard let context = admission.special?.pa, let grid = paGrid(admission.control, in: admission.category),
                  case .text(let json)? = values["json"], case .int(let revision)? = values["revision"],
                  PaProfiles(json: json, revision: revision, grid: grid) == context.profiles else {
                admission.revoke()
                continue
            }
        }
        paResultOwners = paResultOwners.filter { !$0.value.isRevoked }
        admissions.removeAll { $0.isRevoked }
        changed()
    }
}
