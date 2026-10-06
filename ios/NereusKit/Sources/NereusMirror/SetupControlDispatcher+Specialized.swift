// NereusSDR for iOS: the closed Setup panels' reads and edits: the notch list, the settings check, the antenna rows, the PA readings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The four closed panels go through the same admission as every other
/// described control: each edit is admitted when its gesture begins
/// against the current session, the completed snapshot, the description's
/// generation and, for each panel, what else it depends on (the notch
/// list's revision and the selected slice, the radio's address, the
/// transmit state), and its permit is revoked the moment any of those
/// moves on. The permit reaches the final transport handoff.
public extension SetupControlDispatcher {
    // MARK: Words

    nonisolated static let notchListUnreadableReason = "The notch list from the Core could not be read."
    nonisolated static let notchListTooLongReason = "The Core sent more notches than this list can hold."
    nonisolated static let notchListDuplicateReason = "The notch list from the Core names one notch twice."
    nonisolated static let notchListRangeReason = "A notch from the Core is outside the range this list allows."
    nonisolated static let notchGoneReason = "This notch is no longer in the list."
    nonisolated static let notchChangedReason = "The notch list changed on the Core, so this edit was canceled."
    nonisolated static let noRadioReason = "The Core has no radio connected."
    nonisolated static let radioUnknownReason = "The Core did not say which radio is connected."
    nonisolated static let hygieneNotCheckedText = "Not checked in this connection."
    nonisolated static let hygieneUnreadableReason = "The Core's answer could not be read."
    nonisolated static let hygieneBusyReason = "Waiting for the Core to answer."
    nonisolated static let hygienePairReason = "Pair this phone with the Core to forget this radio's settings."
    nonisolated static let hygieneRepairPairReason = "Pair this phone with the Core to repair this radio's settings."
    nonisolated static let antennaUnreadableReason = "The antenna settings from the Core could not be read."
    nonisolated static let antennaBlockedReason = "This antenna is blocked for transmit."
    nonisolated static let telemetryWaitingReason = "No reading from the Core in this connection."
    nonisolated static let telemetryOldReason = "The Core's reading is out of date."
    nonisolated static let telemetryAbsentReason = "The radio has not reported this."

    /// How old a Core reading may be and still be shown (link section 10).
    nonisolated static let telemetryFreshMilliseconds: Int64 = 3_000
    nonisolated static let validateVerb = "station.validateSettings"
    nonisolated static let forgetVerb = "station.forgetSettings"
    nonisolated static let repairVerb = "station.repairSettings"
    nonisolated static let txAntennaVerb = "setAlexTxAntennaForRadio"
    nonisolated static let rxAntennaVerb = "setAlexRxAntennaForRadio"

    // MARK: The notch table

    /// The notch list as the Core sent it, whole or not at all, and whether
    /// it can be changed now.
    func notchTable(_ control: SetupDescription.Control, in category: String) -> SetupNotchTable {
        guard case .table(let table)? = control.binding else {
            return SetupNotchTable(list: nil, listReason: Self.unreadableReason, reason: Self.unreadableReason)
        }
        let read = notches(table)
        var listReason: String?
        var list: SetupNotchList?
        switch read {
        case .success(let value):
            list = value
        case .failure(let refusal):
            listReason = refusal.reason
        }
        return SetupNotchTable(list: list, listReason: listReason,
                               reason: specializedReason(control, in: category) ?? listReason)
    }

    /// Admits a change to notch `row` when its gesture begins: it holds the
    /// list's revision and the selected slice, and either changing cancels it.
    func admitNotch(_ control: SetupDescription.Control, in category: String,
                    row: Int64) -> Result<SetupAdmission, SetupRefusal> {
        if let reason = specializedReason(control, in: category) {
            return .failure(SetupRefusal(reason: reason))
        }
        guard case .table(let table)? = control.binding else {
            return .failure(SetupRefusal(reason: Self.unreadableReason))
        }
        let list: SetupNotchList
        switch notches(table) {
        case .success(let value):
            list = value
        case .failure(let refusal):
            return .failure(refusal)
        }
        guard list.row(row) != nil else {
            return .failure(SetupRefusal(reason: Self.notchGoneReason))
        }
        guard let sender = captureSender() else {
            return .failure(SetupRefusal(reason: Self.notConnectedReason))
        }
        var special = SetupAdmission.Special()
        special.notchRevision = list.revision
        special.notchRow = row
        special.selection = selectedSlice()
        special.tracksSelection = true
        return .success(admitSpecial(control, in: category, sender: sender, special: special))
    }

    /// Sends one change to the notch it was admitted for, with the exact
    /// row action the Core described, if the list, the row, the session
    /// and the selection are all still what they were.
    func performNotch(_ admission: SetupAdmission, edit: SetupNotchEdit) async -> SetupEditOutcome {
        defer { admissions.removeAll { $0 === admission } }
        guard stillCurrent(admission) else {
            admission.revoke()
            return .notSent(Self.notchChangedReason)
        }
        guard case .table(let table)? = admission.control.binding, let rowId = admission.special?.notchRow,
              case .success(let list) = notches(table), let row = list.row(rowId) else {
            admission.revoke()
            return .notSent(Self.notchChangedReason)
        }
        let actionId: String
        var edited: [String: MirrorValue] = [:]
        switch edit {
        case .move(let centreHz, let widthHz):
            guard let centre = table.columns.first(where: { $0.field == "centreHz" })?.range,
                  let width = table.columns.first(where: { $0.field == "widthHz" })?.range,
                  SetupSpecializedWire.fits(centreHz, centre), SetupSpecializedWire.fits(widthHz, width) else {
                return .notSent(Self.outOfRangeReason)
            }
            if centreHz == row.centreHz && widthHz == row.widthHz {
                return .applied
            }
            actionId = "move"
            edited = ["centreHz": .double(centreHz), "widthHz": .double(widthHz)]
        case .setActive(let on):
            if on == row.active {
                return .applied
            }
            actionId = "active"
            edited = ["active": .bool(on)]
        case .delete:
            actionId = "delete"
        }
        guard let action = table.rowActions.first(where: { $0.id == actionId }) else {
            return .notSent(Self.notOnThisPhoneReason)
        }
        var arguments: [CommandArgument] = []
        for (name, source) in action.command.arguments.sorted(by: { $0.key < $1.key }) {
            switch source {
            case .row(let field) where field == table.rowKey:
                arguments.append(CommandArgument(name: name, value: .int(rowId)))
            case .edit(let field):
                guard let value = edited[field] else { return .notSent(Self.unreadableReason) }
                arguments.append(CommandArgument(name: name, value: value))
            default:
                return .notSent(Self.unreadableReason)
            }
        }
        return await send(admission, verb: action.command.verb, arguments: arguments).outcome
    }

    // MARK: The settings check

    /// The settings check: the Core's last answer for this radio in this
    /// session, and whether each action can be used now.
    func hygiene(_ control: SetupDescription.Control, in category: String) -> SetupHygienePanel {
        let actions = control.hygieneActions ?? []
        let validate = actions.first { $0.id == SetupHygieneAction.validate.rawValue }
        let reset = actions.first { $0.id == "reset" }
        let repair = actions.first { $0.id == SetupHygieneAction.repair.rawValue }
        let forget = actions.first { $0.id == SetupHygieneAction.forget.rawValue }
        let base = specializedReason(control, in: category)
        var macReason: String?
        var mac: String?
        switch currentRadioMac() {
        case .success(let value):
            mac = value
        case .failure(let refusal):
            macReason = refusal.reason
        }
        var report: SetupHygieneReport?
        var reportReason: String?
        if base == nil, let mac, let outcome = hygieneOutcome, outcome.identity == store.snapshotIdentity,
           outcome.mac == mac {
            switch outcome.result {
            case .success(let value):
                report = value
            case .failure(let refusal):
                reportReason = refusal.reason
            }
        } else {
            reportReason = base ?? macReason ?? Self.hygieneNotCheckedText
        }
        let validateReason = base ?? macReason ?? (validate == nil ? Self.notOnThisPhoneReason : nil)
            ?? (hygieneBusy ? Self.hygieneBusyReason : nil)
        let common = base ?? macReason
        let forgetReason = common ?? guarded(forget, pairReason: Self.hygienePairReason)
        let repairReason = common ?? guarded(repair, pairReason: Self.hygieneRepairPairReason)
        return SetupHygienePanel(report: report, reportReason: report == nil ? reportReason : nil,
                                 busy: hygieneBusy, validateReason: validateReason, forgetReason: forgetReason,
                                 repairReason: repairReason,
                                 resetReason: reset?.reason ?? Self.notOnThisPhoneReason,
                                 confirmation: forget?.confirmation, repairConfirmation: repair?.confirmation,
                                 validateLabel: validate?.label ?? "", repairLabel: repair?.label ?? "",
                                 resetLabel: reset?.label ?? "", forgetLabel: forget?.label ?? "")
    }

    /// Why a paired, off-air action (Repair, Forget) cannot be used now:
    /// not described, below the Core version it needs (in the Core's
    /// words), waiting, not signed in with this phone's key, or on the air.
    private func guarded(_ action: SetupDescription.HygieneAction?, pairReason: String) -> String? {
        guard let action else { return Self.notOnThisPhoneReason }
        if !hygieneGateOpen(action) { return action.reason ?? Self.needsNewerCoreReason }
        if hygieneBusy { return Self.hygieneBusyReason }
        if action.paired, !signedInWithDeviceKey() { return pairReason }
        if action.offAir { return offAirReason() }
        return nil
    }

    /// Whether the Core sends the settingsHygieneVersion an action needs.
    private func hygieneGateOpen(_ action: SetupDescription.HygieneAction) -> Bool {
        guard let minimum = action.minimumVersion else { return true }
        return store.capabilityVersion("settingsHygieneVersion") >= minimum
    }

    /// Admits Re-validate, Repair or Forget when it is tapped: it holds
    /// the session and the radio's address. Repair's and Forget's question
    /// comes after this, and sending it checks everything again.
    func admitHygiene(_ control: SetupDescription.Control, in category: String,
                      action: SetupHygieneAction) -> Result<SetupAdmission, SetupRefusal> {
        let panel = hygiene(control, in: category)
        let reason: String?
        switch action {
        case .validate: reason = panel.validateReason
        case .repair: reason = panel.repairReason
        case .forget: reason = panel.forgetReason
        }
        if let reason {
            return .failure(SetupRefusal(reason: reason))
        }
        guard case .success(let mac) = currentRadioMac() else {
            return .failure(SetupRefusal(reason: Self.noRadioReason))
        }
        guard let sender = captureSender() else {
            return .failure(SetupRefusal(reason: Self.notConnectedReason))
        }
        var special = SetupAdmission.Special()
        special.radioMac = mac
        special.hygiene = action
        special.offAir = action != .validate
            && control.hygieneActions?.first { $0.id == action.rawValue }?.offAir == true
        return .success(admitSpecial(control, in: category, sender: sender, special: special))
    }

    /// Sends the admitted check, repair or forget for the radio it was admitted
    /// with; its answer shows until the session or the radio changes.
    func performHygiene(_ admission: SetupAdmission) async -> SetupEditOutcome {
        defer { admissions.removeAll { $0 === admission } }
        guard let action = admission.special?.hygiene, let mac = admission.special?.radioMac else {
            return .notSent(Self.unreadableReason)
        }
        guard stillCurrent(admission) else {
            admission.revoke()
            return .notSent(Self.changedFirstReason)
        }
        guard !hygieneBusy else {
            return .notSent(Self.hygieneBusyReason)
        }
        hygieneBusy = true
        changed()
        defer {
            hygieneBusy = false
            changed()
        }
        let verb: String
        switch action {
        case .validate: verb = Self.validateVerb
        case .repair: verb = Self.repairVerb
        case .forget: verb = Self.forgetVerb
        }
        let sent = await send(admission, verb: verb, arguments: [CommandArgument(name: "mac", value: .text(mac))])
        // An answer for an earlier session is not this session's answer.
        guard store.snapshotIdentity == admission.mirrorIdentity else {
            return .notSent(Self.changedFirstReason)
        }
        let result: Result<SetupHygieneReport, SetupRefusal>
        let outcome: SetupEditOutcome
        switch (sent.outcome, sent.result) {
        case (.applied, let answer?):
            if let report = SetupSpecializedWire.hygieneReport(answer, mac: mac) {
                result = .success(report)
                outcome = .applied
            } else {
                result = .failure(SetupRefusal(reason: Self.hygieneUnreadableReason))
                outcome = .refused(Self.hygieneUnreadableReason)
            }
        case (.awaitingConfirmation, _):
            // Held for the Core's question: no answer to show for now.
            return sent.outcome
        case (.refused(let reason), _), (.notSent(let reason), _):
            result = .failure(SetupRefusal(reason: reason))
            outcome = sent.outcome
        case (.applied, nil):
            result = .failure(SetupRefusal(reason: Self.hygieneUnreadableReason))
            outcome = .refused(Self.hygieneUnreadableReason)
        }
        if case .notSent = outcome, sent.result == nil, admission.isRevoked {
            // Nothing reached the Core: the previous answer still stands.
            return outcome
        }
        hygieneOutcome = HygieneOutcome(identity: admission.mirrorIdentity, mac: mac, result: result)
        return outcome
    }

    // MARK: The antenna rows

    /// One antenna table: every value checked together, and whether a cell
    /// can be changed now.
    func antennaTable(_ control: SetupDescription.Control, in category: String) -> SetupAntennaTable {
        guard case .antennaRows(let rows)? = control.binding else {
            return SetupAntennaTable(grid: nil, gridReason: Self.unreadableReason, reason: Self.unreadableReason)
        }
        var grid: SetupAntennaGrid?
        var gridReason: String?
        switch antennaGrid(rows) {
        case .success(let value):
            grid = value
        case .failure(let refusal):
            gridReason = refusal.reason
        }
        var reason = specializedReason(control, in: category)
        if reason == nil, case .failure(let refusal) = currentRadioMac() {
            reason = refusal.reason
        }
        return SetupAntennaTable(grid: grid, gridReason: gridReason, reason: reason ?? gridReason)
    }

    /// Admits a tap on an antenna cell: it holds the session and the radio's address.
    func admitAntenna(_ control: SetupDescription.Control, in category: String) -> Result<SetupAdmission, SetupRefusal> {
        if let reason = antennaTable(control, in: category).reason {
            return .failure(SetupRefusal(reason: reason))
        }
        guard case .success(let mac) = currentRadioMac() else {
            return .failure(SetupRefusal(reason: Self.noRadioReason))
        }
        guard let sender = captureSender() else {
            return .failure(SetupRefusal(reason: Self.notConnectedReason))
        }
        var special = SetupAdmission.Special()
        special.radioMac = mac
        return .success(admitSpecial(control, in: category, sender: sender, special: special))
    }

    /// Chooses `column`'s antenna on `band` for the radio the tap was
    /// admitted with, through the Core's one-band verb for that table.
    func performAntenna(_ admission: SetupAdmission, band: Int, column: String) async -> SetupEditOutcome {
        defer { admissions.removeAll { $0 === admission } }
        guard stillCurrent(admission) else {
            admission.revoke()
            return .notSent(Self.changedFirstReason)
        }
        guard case .antennaRows(let rows)? = admission.control.binding, let mac = admission.special?.radioMac,
              let columnIndex = rows.columns.firstIndex(where: { $0.id == column }),
              let rowIndex = rows.rows.firstIndex(where: { $0.band == band }),
              case .success(let grid) = antennaGrid(rows) else {
            return .notSent(Self.changedFirstReason)
        }
        if grid.blocked[columnIndex] {
            return .notSent(Self.antennaBlockedReason)
        }
        if grid.selected[rowIndex][columnIndex] {
            return .applied
        }
        let target = rows.columns[columnIndex]
        var arguments = [CommandArgument(name: "mac", value: .text(mac)),
                         CommandArgument(name: "band", value: .int(Int64(band))),
                         CommandArgument(name: "antenna", value: .int(Int64(target.antenna)))]
        let verb: String
        if rows.mode == "tx" {
            verb = Self.txAntennaVerb
        } else {
            verb = Self.rxAntennaVerb
            arguments.append(CommandArgument(name: "rxOnly", value: .bool(target.field == "rxOnly")))
        }
        return await send(admission, verb: verb, arguments: arguments).outcome
    }

    // MARK: The PA readings

    /// A PA reading from the Core's current telemetry, measured no more
    /// than 3 seconds ago: never a zero the radio did not report.
    func telemetryReading(_ control: SetupDescription.Control, in category: String,
                          nowMilliseconds: Int64) -> SetupTelemetryReading {
        func none(_ reason: String) -> SetupTelemetryReading {
            SetupTelemetryReading(value: nil, ageMilliseconds: nil, reason: reason)
        }
        guard case .telemetry(let reference)? = control.binding, control.metadataIssue == nil else {
            return none(Self.unreadableReason)
        }
        if let availability = control.availability, !availability.enabled {
            return none(availability.reason)
        }
        guard store.isSnapshotComplete, !store.isStale else {
            return none(Self.notConnectedReason)
        }
        guard currentControl(control.id, in: category) == control else {
            return none(Self.updatingReason)
        }
        if let reason = gateReason(control.gate) {
            return none(reason)
        }
        guard let receipt = store.currentTelemetryReceipt else {
            return none(Self.telemetryWaitingReason)
        }
        let age = nowMilliseconds - receipt.observedAtMilliseconds
        guard age >= 0, age <= Self.telemetryFreshMilliseconds else {
            return none(Self.telemetryOldReason)
        }
        let value: Double?
        switch reference.name {
        case "paCurrentAmps":
            value = receipt.metrics.radio?.paCurrentAmps
        case "supplyVolts":
            value = receipt.metrics.radio?.supplyVolts
        case "paVolts":
            value = receipt.metrics.radio?.paVolts
        case "paTemperatureCelsius":
            value = receipt.metrics.radio?.paTemperatureCelsius
        case "hl2RxBytesPerSecond":
            value = receipt.metrics.radio?.hl2RxBytesPerSecond
        case "hl2TxBytesPerSecond":
            value = receipt.metrics.radio?.hl2TxBytesPerSecond
        case "hl2Throttled":
            value = receipt.metrics.radio?.hl2Throttled.map { $0 ? 1 : 0 }
        case "hl2SequenceGaps":
            value = receipt.metrics.radio?.hl2SequenceGaps.map(Double.init)
        case "connectionAgeMs":
            value = receipt.metrics.radio?.connectionAgeMs.map(Double.init)
        default:
            return none(Self.unreadableReason)
        }
        guard let value, value.isFinite else {
            return none(Self.telemetryAbsentReason)
        }
        return SetupTelemetryReading(value: value, ageMilliseconds: age, reason: nil)
    }
}

// MARK: Shared parts

extension SetupControlDispatcher {
    static let notchesKey = "notches"

    /// The settings check's last answer, for the session and radio it was for.
    struct HygieneOutcome {
        let identity: UInt64
        let mac: String
        let result: Result<SetupHygieneReport, SetupRefusal>
    }

    /// What every closed panel needs before anything can change: readable
    /// metadata, the Core's own availability, a current session with the
    /// same description, and the control's own gate.
    func specializedReason(_ control: SetupDescription.Control, in category: String) -> String? {
        if control.metadataIssue != nil {
            return Self.unreadableReason
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
        return gateReason(control.gate)
    }

    /// The connected radio's address as the Core names it.
    func currentRadioMac() -> Result<String, SetupRefusal> {
        if store.capabilities["radioConnected"] == .bool(false) {
            return .failure(SetupRefusal(reason: Self.noRadioReason))
        }
        guard case .text(let mac)? = store.capabilities["macAddress"], !mac.isEmpty else {
            return .failure(SetupRefusal(reason: Self.noRadioReason))
        }
        guard SetupSpecializedWire.canonicalMac(mac) else {
            return .failure(SetupRefusal(reason: Self.radioUnknownReason))
        }
        return .success(mac)
    }

    /// Nil when the radio is off the air.
    func offAirReason() -> String? {
        watch(Self.txStateKey)
        guard let state = store.object(Self.txStateKey)?.values else {
            return Self.transmitStateUnknownReason
        }
        return Self.onAir(state)
    }

    func notches(_ table: SetupDescription.Table) -> Result<SetupNotchList, SetupRefusal> {
        watch(table.valueProperty.object)
        guard let object = store.object(table.valueProperty.object) else {
            return .failure(SetupRefusal(reason: Self.valueMissingReason))
        }
        switch SetupSpecializedWire.notches(json: object[table.valueProperty.name],
                                            revision: object[table.revisionProperty.name], table: table) {
        case .success(let list):
            return .success(list)
        case .failure(.tooLong):
            return .failure(SetupRefusal(reason: Self.notchListTooLongReason))
        case .failure(.duplicate):
            return .failure(SetupRefusal(reason: Self.notchListDuplicateReason))
        case .failure(.outOfRange):
            return .failure(SetupRefusal(reason: Self.notchListRangeReason))
        case .failure(.unreadable):
            return .failure(SetupRefusal(reason: Self.notchListUnreadableReason))
        }
    }

    func antennaGrid(_ rows: SetupDescription.AntennaRows) -> Result<SetupAntennaGrid, SetupRefusal> {
        watch(rows.object)
        guard let object = store.object(rows.object) else {
            return .failure(SetupRefusal(reason: Self.valueMissingReason))
        }
        let bands = rows.rows.count
        let unreadable = SetupRefusal(reason: Self.antennaUnreadableReason)
        var lists: [String: [Int]] = [:]
        var blocked2 = false
        var blocked3 = false
        if rows.mode == "tx" {
            guard let tx = SetupSpecializedWire.antennaList(object["txAntennas"], bands: bands, range: 1...3),
                  case .bool(let two)? = object["blockTxAnt2"], case .bool(let three)? = object["blockTxAnt3"] else {
                return .failure(unreadable)
            }
            lists["tx"] = tx
            blocked2 = two
            blocked3 = three
        } else {
            guard let rx = SetupSpecializedWire.antennaList(object["rxAntennas"], bands: bands, range: 1...3),
                  let rxOnly = SetupSpecializedWire.antennaList(object["rxOnlyAntennas"], bands: bands, range: 0...3) else {
                return .failure(unreadable)
            }
            lists["rx"] = rx
            lists["rxOnly"] = rxOnly
        }
        // A row's list entry is its position, not its band number: 2 m
        // (band 27) is the fifteenth entry (link document section 6.1).
        var selected: [[Bool]] = []
        for position in rows.rows.indices {
            selected.append(rows.columns.map { column in lists[column.field]?[position] == column.antenna })
        }
        let blocked = rows.columns.map { column in
            column.field == "tx" && (column.antenna == 2 && blocked2 || column.antenna == 3 && blocked3)
        }
        return .success(SetupAntennaGrid(selected: selected, blocked: blocked))
    }

    func admitSpecial(_ control: SetupDescription.Control, in category: String,
                      sender: @escaping MirrorStore.BoundSender, special: SetupAdmission.Special) -> SetupAdmission {
        watch(SetupDescriptionFeed.objectKey)
        watch(Self.txStateKey)
        let admission = SetupAdmission(control: control, category: category, generation: feed.generation,
                                       mirrorIdentity: store.snapshotIdentity, settingsIdentity: nil,
                                       sender: sender, objectKey: nil, sliceId: nil, special: special)
        admissions.removeAll { $0.isRevoked }
        admissions.append(admission)
        return admission
    }

    /// A closed panel's edit is still what it was admitted against.
    func specialStillCurrent(_ admission: SetupAdmission) -> Bool {
        let control = admission.control
        guard let special = admission.special,
              specializedReason(control, in: admission.category) == nil else {
            return false
        }
        if special.tracksSelection, selectedSlice() != special.selection {
            return false
        }
        if special.offAir, offAirReason() != nil {
            return false
        }
        switch control.specialized {
        case .notchTable?:
            guard case .table(let table)? = control.binding, let row = special.notchRow,
                  case .success(let list) = notches(table),
                  list.revision == special.notchRevision, list.row(row) != nil else {
                return false
            }
            return true
        case .settingsHygiene?:
            guard let mac = special.radioMac, case .success(mac) = currentRadioMac() else {
                return false
            }
            if let action = special.hygiene, action != .validate {
                guard signedInWithDeviceKey(),
                      let described = control.hygieneActions?.first(where: { $0.id == action.rawValue }),
                      hygieneGateOpen(described) else {
                    return false
                }
            }
            return true
        case .antennaRows?:
            guard let mac = special.radioMac, case .success(mac) = currentRadioMac(),
                  case .antennaRows(let rows)? = control.binding, case .success = antennaGrid(rows) else {
                return false
            }
            return true
        case .cfcBands?:
            guard case .cfcProfile? = control.binding else {
                return false
            }
            return cfcGateReason(control) == nil
        case .paTelemetry?, nil:
            return false
        }
    }

    /// Sends one closed-panel command through the admitted session, with
    /// its permit, checking everything again just before the handoff.
    func send(_ admission: SetupAdmission, verb: String,
              arguments: [CommandArgument],
              onLateOutcome: (@MainActor (SetupEditOutcome) -> Void)? = nil) async -> (outcome: SetupEditOutcome, result: CommandResult?) {
        guard let sender = admission.sender else {
            return (.notSent(Self.changedFirstReason), nil)
        }
        let late: (@Sendable (Result<CommandResult, CommandError>) async -> Void)?
        if let receive = onLateOutcome {
            late = { [weak self] answer in
                await self?.receiveSpecialLate(answer, admission: admission, receive: receive)
            }
        } else { late = nil }
        do {
            let result = try await commands.invokeBound(
                verb, arguments: arguments, timeout: Self.commandTimeout, sender: sender,
                stillAllowed: { [weak self] in await self?.stillCurrent(admission) ?? false },
                authority: admission.permit, onLateOutcome: late)
            return (Self.outcome(of: result), result)
        } catch CommandError.linkLost {
            return (.notSent(Self.linkLostReason), nil)
        } catch CommandError.timedOut {
            return (.notSent(Self.noAnswerReason), nil)
        } catch {
            return (.notSent(admission.isRevoked ? Self.changedFirstReason : Self.linkLostReason), nil)
        }
    }

    private func receiveSpecialLate(_ answer: Result<CommandResult, CommandError>, admission: SetupAdmission,
                                    receive: @escaping @MainActor (SetupEditOutcome) -> Void) {
        let outcome: SetupEditOutcome
        switch answer {
        case .success(let result): outcome = Self.outcome(of: result)
        case .failure(.timedOut): outcome = .notSent(Self.noAnswerReason)
        case .failure(.linkLost): outcome = .notSent(Self.linkLostReason)
        case .failure(.notSent): outcome = .notSent(Self.notConnectedReason)
        }
        deliverLate(outcome, admission: admission, to: receive)
    }

    /// A new list: a notch edit admitted against another revision, or for
    /// a row that has gone, is cancelled. `values` are the object's new
    /// values: the store publishes them before it holds them.
    func notchesChanged(_ values: [String: MirrorValue]) {
        for admission in admissions where admission.special?.notchRevision != nil {
            guard case .table(let table)? = admission.control.binding,
                  case .success(let list) = SetupSpecializedWire.notches(
                      json: values[table.valueProperty.name], revision: values[table.revisionProperty.name],
                      table: table),
                  list.revision == admission.special?.notchRevision,
                  let row = admission.special?.notchRow, list.row(row) != nil else {
                admission.revoke()
                continue
            }
        }
        admissions.removeAll { $0.isRevoked }
        changed()
    }

    /// Another radio, or none: an edit admitted for the old one is cancelled.
    func capabilitiesChanged(_ capabilities: [String: MirrorValue]) {
        if paResultOwners.values.contains(where: { $0.special?.pa?.capabilities != capabilities }) {
            retirePaResults()
        }
        let mac: String?
        if capabilities["radioConnected"] != .bool(false), case .text(let text)? = capabilities["macAddress"],
           SetupSpecializedWire.canonicalMac(text) {
            mac = text
        } else {
            mac = nil
        }
        for admission in admissions {
            if let context = admission.special?.pa, context.capabilities != capabilities { admission.revoke() }
            if let admitted = admission.special?.radioMac, admitted != mac {
                admission.revoke()
            }
        }
        admissions.removeAll { $0.isRevoked }
        changed()
    }
}
