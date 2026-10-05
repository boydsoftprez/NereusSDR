// NereusSDR for iOS: Setup rows from description versions 13 to 16: their readings, choices, limits and radio settings
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The NR3 models this session's dspAssets.list named (setup description,
/// `choicesFrom` `{"dspAssets":"nr3"}`), asked once per session.
struct SetupNr3ModelList {
    /// The mirror session the list belongs to.
    var identity: UInt64 = 0
    var models: [(id: String, label: String)] = []
    /// The list's request, from its send to its answer.
    var task: Task<Void, Never>?
}

/// What versions 13 to 16 add to a described row (R-IOS-18): a readout's
/// `format` in the desktop's words, limits from the station catalogue,
/// choices from a list the Core holds, a shown offset, rows that wait for
/// their button (`staged`), rows that follow another Core value, and the
/// radio's own settings kept per radio at the Core.
extension SetupControlDispatcher {
    nonisolated static let micLineReason = "Turn on this phone's microphone to the Core first."
    nonisolated static let otherModelReason = "This point was saved for another model of radio."
    nonisolated static let catalogueWaitingReason = "Waiting for the Core to send the limits for this setting."
    nonisolated static let notNowReason = "Not available right now."
    nonisolated static let tooLongReason = "This text is longer than the Core allows."
    nonisolated static let missingModelText = "Missing model"
    nonisolated static let noLimitText = "No limit"
    nonisolated static let dashText = "\u{2013}"

    // MARK: The row as drawn

    /// The row with what the Core holds now filled in: its limits from the
    /// catalogue, its choices from the named list, and a model the list no
    /// longer holds shown as "Missing model". Draw this; edit the original.
    public func resolved(_ control: SetupDescription.Control) -> SetupDescription.Control {
        guard var modern = control.modern else { return control }
        var result = control
        if let name = modern.rangeFrom {
            result.range = catalogueTransmitRange?(name)
        }
        switch modern.choicesFrom {
        case .jsonNames(let reference)?:
            let names = Self.jsonNames(read(reference, sliceId: selectedSlice()))
            result.options = names.enumerated().map { SetupDescription.Option(value: Int64($0.offset), label: $0.element) }
            modern.optionLiterals = Dictionary(uniqueKeysWithValues: names.enumerated().map {
                (Int64($0.offset), SetupDescription.Literal.text($0.element))
            })
        case .dspAssets(let family)?:
            // The row's own options, then the Core's valid models from
            // dspAssets.list, as the desktop's NR3 picker lists them
            // (src/gui/DspAssetDialog.cpp, Nr3ModelPicker::populate); a
            // model the list does not hold is shown by name as missing.
            if family == Self.nr3AssetFamily {
                for model in nr3Models(for: control)
                where !modern.optionLiterals.values.contains(.text(model.id)) {
                    let next = Int64(result.options?.count ?? 0)
                    result.options = (result.options ?? []) + [SetupDescription.Option(value: next, label: model.label)]
                    modern.optionLiterals[next] = .text(model.id)
                }
            }
            if case .text(let current)? = rawValue(control),
               !modern.optionLiterals.values.contains(.text(current)) {
                let next = Int64(result.options?.count ?? 0)
                result.options = (result.options ?? []) + [SetupDescription.Option(value: next, label: Self.missingModelText)]
                modern.optionLiterals[next] = .text(current)
            }
        case nil:
            break
        }
        result.modern = modern
        return result
    }

    // MARK: NR3 models

    nonisolated static let nr3AssetFamily = "nr3"
    nonisolated static let dspAssetsListVerb = "dspAssets.list"
    /// DspAssetKind::Nr3Model (src/core/dsp/DspAssetValidation.h).
    nonisolated static let nr3ModelKind = 2
    /// The desktop picker's limits (src/gui/DspAssetDialog.cpp:48-49): a
    /// longer list is not read at all, and an id over 128 is skipped.
    nonisolated static let nr3ListMaximumBytes = 256 * 1024
    nonisolated static let nr3ListMaximumRows = 128
    nonisolated static let nr3IdMaximumLength = 128

    /// This session's NR3 models, asking the Core for them the first time
    /// the row is drawn while its gate is open (the desktop picker asks
    /// once, when it opens). Until the answer, and if it does not read,
    /// the row has only its own options.
    func nr3Models(for control: SetupDescription.Control) -> [(id: String, label: String)] {
        let identity = store.snapshotIdentity
        if nr3List.identity != identity {
            nr3List = SetupNr3ModelList(identity: identity)
        }
        if nr3List.task == nil, store.isSnapshotComplete, !store.isStale, gateReason(control.gate) == nil,
           let sender = captureSender() {
            nr3List.task = Task { @MainActor [weak self] in
                await self?.listNr3Models(sender: sender, identity: identity)
            }
        }
        return nr3List.models
    }

    private func listNr3Models(sender: @escaping MirrorStore.BoundSender, identity: UInt64) async {
        let result = try? await commands.invokeBound(
            Self.dspAssetsListVerb, arguments: [], timeout: Self.commandTimeout, sender: sender,
            stillAllowed: { [weak self] in await self?.store.snapshotIdentity == identity })
        guard nr3List.identity == identity, store.snapshotIdentity == identity,
              let result, result.accepted, case .text(let json)? = result.values["assets"],
              json.utf8.count <= Self.nr3ListMaximumBytes else {
            return
        }
        nr3List.models = Self.nr3Models(json: json)
        changed()
    }

    /// The valid NR3 models in a dspAssets.list `assets` array, each with
    /// its label or a short id (Nr3ModelPicker::onRequestCompleted). Other
    /// kinds, and anything that does not read, are skipped.
    nonisolated static func nr3Models(json: String) -> [(id: String, label: String)] {
        guard let array = (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [Any],
              array.count <= nr3ListMaximumRows else {
            return []
        }
        var models: [(id: String, label: String)] = []
        for item in array {
            guard let object = item as? [String: Any],
                  let kind = object["kind"] as? NSNumber, !isBoolean(kind), kind.doubleValue == Double(nr3ModelKind),
                  let valid = object["valid"] as? NSNumber, isBoolean(valid), valid.boolValue,
                  let id = object["id"] as? String, !id.isEmpty, id.utf16.count <= nr3IdMaximumLength else {
                continue
            }
            let label = object["label"] as? String ?? ""
            models.append((id, label.isEmpty ? shortAssetId(id) : label))
        }
        return models
    }

    /// The desktop's short id (src/gui/DspAssetDialog.cpp:64-70).
    nonisolated static func shortAssetId(_ id: String) -> String {
        let prefix = "sha256:"
        if id.hasPrefix(prefix), id.utf16.count > 23 {
            return String(id.prefix(23)) + "\u{2026}"
        }
        return id
    }

    private nonisolated static func isBoolean(_ number: NSNumber) -> Bool {
        CFGetTypeID(number) == CFBooleanGetTypeID()
    }

    /// The row's own value on the Core, before any conversion.
    private func rawValue(_ control: SetupDescription.Control) -> MirrorValue? {
        switch control.binding {
        case .property(let reference)?:
            return read(reference, sliceId: selectedSlice())
        case .command(let command)?:
            return command.valueProperty.flatMap { read($0, sliceId: selectedSlice()) }
        default:
            return nil
        }
    }

    /// A staged row's held value, or its value on the Core.
    func controlValue(_ control: SetupDescription.Control, sliceId: Int?) -> SetupValue? {
        stagedValues[control.id] ?? value(of: control, sliceId: sliceId)
    }

    /// What a phone action takes from its row: the page to open, or the
    /// text to copy.
    func phoneArgument(_ control: SetupDescription.Control) -> String? {
        control.modern?.target ?? control.modern?.copyText
    }

    /// Why a V13 to V16 row cannot change now beyond the V12 checks.
    func modernReason(_ control: SetupDescription.Control, sliceId: Int?) -> String? {
        guard let modern = control.modern else { return nil }
        if let dependency = modern.propertyDependency {
            guard let current = read(dependency.property, sliceId: sliceId),
                  dependency.oneOf.contains(where: { Self.matches(current, $0) }) else {
                return control.kind == .button ? Self.notNowReason : Self.dependsReason
            }
        }
        if modern.rangeFrom != nil, resolved(control).range == nil {
            return Self.catalogueWaitingReason
        }
        if case .jsonNames? = modern.choicesFrom, resolved(control).options?.isEmpty != false {
            return SetupDescription.noChoicesReason
        }
        return nil
    }

    // MARK: Values in and out

    /// A Core value as the row's value: a listed literal as its index, and
    /// a shown offset added.
    func modernDecode(_ raw: MirrorValue, for control: SetupDescription.Control, sliceId: Int?) -> SetupValue? {
        guard control.modern != nil else {
            return SetupValueCoding.decodeProperty(raw, for: control)
        }
        let drawn = resolved(control)
        guard let modern = drawn.modern else { return nil }
        if !modern.optionLiterals.isEmpty {
            return modern.optionLiterals.sorted { $0.key < $1.key }
                .first { Self.matches(raw, $0.value) }.map { .integer($0.key) }
        }
        guard let value = SetupValueCoding.decodeProperty(raw, for: drawn) else { return nil }
        if let offset = modern.valueOffset {
            return Self.shifted(value, by: offset)
        }
        return value
    }

    /// The row's value as the Core takes it, or nil when it does not fit.
    func modernWire(_ value: SetupValue, for control: SetupDescription.Control) -> MirrorValue? {
        guard control.modern != nil else {
            return SetupValueCoding.encodeProperty(value, for: control)
        }
        var drawn = resolved(control)
        guard let modern = drawn.modern else { return nil }
        if !modern.optionLiterals.isEmpty {
            guard let whole = value.whole, let literal = modern.optionLiterals[whole] else { return nil }
            return Self.mirror(literal)
        }
        if let offset = modern.valueOffset {
            guard SetupValueCoding.fits(value, drawn), let inner = Self.shifted(value, by: -offset) else { return nil }
            drawn.range = drawn.range.map {
                SetupDescription.Range(minimum: $0.minimum - offset, maximum: $0.maximum - offset, step: $0.step)
            }
            return SetupValueCoding.encodeProperty(inner, for: drawn)
        }
        return SetupValueCoding.encodeProperty(value, for: drawn)
    }

    /// A value a staged row may hold.
    func stagedFits(_ value: SetupValue, _ control: SetupDescription.Control) -> Bool {
        let drawn = resolved(control)
        switch control.kind {
        case .toggle?:
            return value.flag != nil
        case .text?:
            guard let text = value.text else { return false }
            return drawn.modern?.maxLength.map { text.count <= $0 } ?? true
        default:
            if let literals = drawn.modern?.optionLiterals, !literals.isEmpty {
                return value.whole.map { literals[$0] != nil } ?? false
            }
            return SetupValueCoding.fits(value, drawn)
        }
    }

    /// Text longer than the row allows is refused before it is sent.
    func tooLong(_ value: SetupValue?, _ control: SetupDescription.Control) -> Bool {
        guard let limit = control.modern?.maxLength, let text = value?.text else { return false }
        return text.count > limit
    }

    static func shifted(_ value: SetupValue, by offset: Double) -> SetupValue? {
        if let whole = value.whole, offset.rounded() == offset, abs(offset) < 9.0e15 {
            return .integer(whole + Int64(offset))
        }
        return value.number.map { .decimal($0 + offset) }
    }

    static func mirror(_ literal: SetupDescription.Literal) -> MirrorValue {
        switch literal {
        case .bool(let on): return .bool(on)
        case .integer(let whole): return .int(whole)
        case .decimal(let number): return .double(number)
        case .text(let text): return .text(text)
        }
    }

    /// A Core value is the literal.
    static func matches(_ value: MirrorValue, _ literal: SetupDescription.Literal) -> Bool {
        switch (value, literal) {
        case (.bool(let a), .bool(let b)):
            return a == b
        case (.int(let a), .integer(let b)), (.enumeration(let a), .integer(let b)):
            return a == b
        case (.double(let a), .integer(let b)):
            return a == Double(b)
        case (.double(let a), .decimal(let b)):
            return a == b
        case (.int(let a), .decimal(let b)), (.enumeration(let a), .decimal(let b)):
            return Double(a) == b
        case (.text(let a), .text(let b)):
            return a == b
        default:
            return false
        }
    }

    /// The names a JSON array of strings holds; none when it is not one.
    static func jsonNames(_ value: MirrorValue?) -> [String] {
        guard case .text(let text)? = value, let data = text.data(using: .utf8),
              let names = (try? JSONSerialization.jsonObject(with: data)) as? [Any] else { return [] }
        let result = names.compactMap { $0 as? String }
        return result.count == names.count ? result : []
    }

    // MARK: The radio's own settings

    /// The Core keeps these per radio under `hardware/<address>/<path>`.
    private func radioKey(_ path: String) -> String? {
        guard case .success(let mac) = currentRadioMac() else { return nil }
        return "hardware/\(mac)/\(path)"
    }

    private func boardClassKey() -> String? {
        radioKey("paCalibration/boardClass")
    }

    /// The model a saved calibration point belongs to: nil when none is
    /// saved, else the stored class.
    private func storedBoardClass() -> Int64? {
        guard let key = boardClassKey(), let text = settings.value(key),
              let stored = Int64(text.trimmingCharacters(in: .whitespaces)), stored != 0 else { return nil }
        return stored
    }

    func radioSettingValue(_ control: SetupDescription.Control, path: String) -> SetupValue? {
        guard let key = radioKey(path) else { return nil }
        if let wanted = control.modern?.boardClass {
            guard let stored = storedBoardClass() else {
                return control.defaultValue.flatMap(Self.setupValue)
            }
            guard stored == wanted else { return nil }
        }
        guard let text = settings.value(key) else {
            return control.defaultValue.flatMap(Self.setupValue)
        }
        return SetupValueCoding.decodeSetting(text, for: control)
    }

    /// V18: a row that follows another radio setting (the CL2 frequency
    /// follows Enable CL2) is greyed while that setting is not one of the
    /// Core's values. The setting is read as its own row reads it, its
    /// default when the radio has none kept.
    func radioSettingDependencyReason(_ control: SetupDescription.Control, in category: String) -> String? {
        guard let dependency = control.modern?.radioSettingDependency else { return nil }
        let source = feed.description(for: category)?.pages
            .flatMap { $0.sections.flatMap(\.controls) }
            .first { $0.binding == .radioSetting(dependency.path) }
        let current: SetupValue?
        if let source {
            current = radioSettingValue(source, path: dependency.path)
        } else {
            current = nil
        }
        guard let current, dependency.oneOf.contains(where: { Self.matches(current, $0) }) else {
            return Self.dependsReason
        }
        return nil
    }

    func radioSettingReason(_ control: SetupDescription.Control, path: String) -> String? {
        if case .failure(let refusal) = currentRadioMac() {
            return refusal.reason
        }
        guard settings.currentSnapshotIdentity != nil else {
            return Self.notConnectedReason
        }
        if let wanted = control.modern?.boardClass, let stored = storedBoardClass(), stored != wanted {
            return Self.otherModelReason
        }
        guard radioSettingValue(control, path: path) != nil else {
            return Self.valueMissingReason
        }
        return nil
    }

    /// Writes a radio's own setting; a calibration point first names the
    /// model it belongs to, as the desktop does.
    func performRadioSetting(_ admission: SetupAdmission, path: String, value: SetupValue?,
                             onLateOutcome: (@MainActor (SetupEditOutcome) -> Void)? = nil) async -> SetupEditOutcome {
        let control = admission.control
        guard let value, let text = SetupValueCoding.encodeSetting(value, for: control) else {
            return .notSent(Self.outOfRangeReason)
        }
        guard let identity = admission.settingsIdentity, let key = radioKey(path) else {
            return .notSent(Self.changedFirstReason)
        }
        if let wanted = control.modern?.boardClass, storedBoardClass() == nil {
            guard let classKey = boardClassKey() else { return .notSent(Self.changedFirstReason) }
            let first = await write(classKey, String(wanted), identity: identity, admission: admission, onLateOutcome: { outcome in
                // The board class alone is not completion of the row's value.
                if outcome != .applied { onLateOutcome?(outcome) }
            })
            guard first == .applied else { return first }
            guard stillCurrent(admission) else { return .notSent(Self.changedFirstReason) }
        }
        return await write(key, text, identity: identity, admission: admission, onLateOutcome: onLateOutcome)
    }

    private func write(_ key: String, _ text: String, identity: UInt64,
                       admission: SetupAdmission,
                       onLateOutcome: (@MainActor (SetupEditOutcome) -> Void)? = nil) async -> SetupEditOutcome {
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
            // The value stays, marked; the Core's answer may still come.
            return .notSent(PropertyWriteOutcome.notConfirmed.reason)
        }
    }

    static func setupValue(_ literal: SetupDescription.Literal) -> SetupValue {
        switch literal {
        case .bool(let on): return .bool(on)
        case .integer(let whole): return .integer(whole)
        case .decimal(let number): return .decimal(number)
        case .text(let text): return .text(text)
        }
    }

    // MARK: Readouts

    /// A V13 to V16 readout: its value in the desktop's words, or why there
    /// is none. Never a zero the Core did not send.
    func modernReadout(_ control: SetupDescription.Control, in category: String) -> SetupControlState {
        func none(_ reason: String) -> SetupControlState {
            SetupControlState(value: nil, editable: false, reason: reason)
        }
        if control.metadataIssue != nil { return none(Self.unreadableReason) }
        if let pending = control.modern?.pendingReason { return none(pending) }
        if let availability = control.availability, !availability.enabled { return none(availability.reason) }
        guard store.isSnapshotComplete, !store.isStale else { return none(Self.notConnectedReason) }
        guard currentControl(control.id, in: category) == control else { return none(Self.updatingReason) }
        if let reason = gateReason(control.gate) { return none(reason) }
        let sliceId = selectedSlice()
        let shown: SetupValue?
        switch control.binding {
        case .telemetry?:
            let reading = telemetryReading(control, in: category, nowMilliseconds: telemetryNow())
            guard let number = reading.value else { return none(reading.reason ?? Self.valueMissingReason) }
            shown = formatted(control, raw: .double(number), sibling: { _ in nil })
        case .radioInfo?:
            guard let text = control.modern?.readoutValue, !text.isEmpty else {
                return none(SetupDescription.unfilledReason)
            }
            shown = .text(text)
        case .adcOverload(let object)?:
            guard let key = resolve(object, sliceId: sliceId), let values = store.object(key)?.values else {
                return none(Self.valueMissingReason)
            }
            watch(key)
            shown = .text(Self.adcOverloadText(values))
        case .property(let reference)?:
            if reference.object == Self.activeSliceAlias, resolve(reference.object, sliceId: sliceId) == nil {
                return none(Self.noSliceReason)
            }
            guard let raw = read(reference, sliceId: sliceId), let key = resolve(reference.object, sliceId: sliceId) else {
                return none(Self.valueMissingReason)
            }
            shown = formatted(control, raw: raw, sibling: { [store] in store.object(key)?[$0] })
        case .command(let command)?:
            guard let reference = command.valueProperty, let raw = read(reference, sliceId: sliceId) else {
                return none(Self.valueMissingReason)
            }
            shown = formatted(control, raw: raw, sibling: { _ in nil })
        default:
            return none(Self.notOnThisPhoneReason)
        }
        guard let shown else { return none(Self.valueMissingReason) }
        return SetupControlState(value: shown, editable: false, reason: nil)
    }

    static func adcOverloadText(_ values: [String: MirrorValue]) -> String {
        if Self.number(values["overloadAdc0"]).map({ $0 > 0 }) == true { return "Yes (ADC 0)" }
        if Self.number(values["overloadAdc1"]).map({ $0 > 0 }) == true { return "Yes (ADC 1)" }
        return "No"
    }

    static func number(_ value: MirrorValue?) -> Double? {
        switch value {
        case .int(let whole)?, .enumeration(let whole)?: return Double(whole)
        case .double(let number)?: return number.isFinite ? number : nil
        case .bool(let on)?: return on ? 1 : 0
        default: return nil
        }
    }

    static func flag(_ value: MirrorValue?) -> Bool? {
        switch value {
        case .bool(let on)?: return on
        default: return number(value).map { $0 != 0 }
        }
    }

    static func text(_ value: MirrorValue?) -> String? {
        if case .text(let text)? = value { return text }
        return nil
    }

    static func oneDecimal(_ number: Double) -> String {
        String(format: "%.1f", number)
    }

    /// A readout's value: formatted text in the desktop's words, or the
    /// plain value (with its unit, drawn by the page) when it has no format
    /// the phone knows.
    func formatted(_ control: SetupDescription.Control, raw: MirrorValue,
                   sibling: (String) -> MirrorValue?) -> SetupValue? {
        let modern = control.modern
        if let unitKey = modern?.temperatureUnit {
            guard let celsius = Self.number(raw) else { return nil }
            return .text(temperatureText(celsius, unitKey: unitKey, decimals: control.decimals ?? 1))
        }
        guard let format = modern?.format else { return Self.plain(raw, control: control) }
        switch format {
        case "nnrLimit":
            switch Self.number(raw) {
            case 0?: return .text(Self.noLimitText)
            case 1?: return .text("Noise reduction is using the Standard model. The Core computer could not keep up with Premium.")
            case 2?: return .text("Noise reduction was turned off. The Core computer could not keep up.")
            default: return Self.plain(raw, control: control)
            }
        case "nnrModelSlot":
            switch Self.number(raw) {
            case 0?: return .text("Standard")
            case 1?: return .text("Premium")
            default: return Self.plain(raw, control: control)
            }
        case "nnrStatus":
            if let error = Self.text(sibling("nnrLastError")), !error.isEmpty { return .text(error) }
            return Self.plain(raw, control: control)
        case "positiveOrNone":
            guard let number = Self.number(raw) else { return Self.plain(raw, control: control) }
            return number <= 0 ? .text("none") : Self.plain(raw, control: control)
        case "enabledOff":
            return Self.flag(raw).map { .text($0 ? "enabled" : "off") }
        case "phaseRotator":
            guard let on = Self.flag(raw) else { return nil }
            guard on else { return .text("OFF") }
            let hz = Self.number(sibling("phaseRotatorFreqHz")).map { String(Int64($0)) } ?? Self.dashText
            let stages = Self.number(sibling("phaseRotatorStages")).map { String(Int64($0)) } ?? Self.dashText
            return .text("ON \u{00B7} \(hz) Hz \u{00B7} \(stages) stages")
        case "cfcBands":
            return Self.flag(raw).map { .text($0 ? "ON \u{00B7} 10 bands" : "OFF") }
        case "cessb":
            guard let on = Self.flag(raw) else { return nil }
            if on { return .text("ON") }
            return .text(Self.flag(sibling("cpdrOn")) == true ? "OFF" : "OFF (gated on CPDR)")
        case "radioName":
            if let name = Self.text(raw), !name.isEmpty { return .text(name) }
            if let model = Self.text(sibling("model")), !model.isEmpty { return .text(model) }
            return .text(Self.dashText)
        case "uptime":
            guard let ms = Self.number(raw), ms >= 0 else { return .text(Self.dashText) }
            return .text(Self.duration(Int64(ms / 1_000), shortest: false))
        case "txMode":
            return Self.flag(raw).map { .text($0 ? "TX" : "RX (idle)") }
        case "paTemperature":
            guard let celsius = Self.number(raw) else { return nil }
            return .text(temperatureText(celsius, unitKey: Self.paTempUnitKey, decimals: 1))
        case "wattsWhileKeyed":
            guard Self.flag(sibling("keyed")) == true, let watts = Self.number(raw) else { return .text("\(Self.dashText) W") }
            return .text("\(Self.oneDecimal(watts)) W")
        case "swrWhileKeyed":
            guard Self.flag(sibling("keyed")) == true, let swr = Self.number(raw) else { return .text("1.0:1") }
            return .text("\(Self.oneDecimal(swr)):1")
        case "kilobytesPerSecond":
            return Self.number(raw).map { .text("\(Self.oneDecimal($0 / 1_024)) KB/s") }
        case "throttleActive":
            return Self.flag(raw).map { .text($0 ? "Active" : "None active") }
        case "throttledOk":
            return Self.flag(raw).map { .text($0 ? "THROTTLED" : "ok") }
        case "connectionPhase":
            return Self.number(raw).map { .text(Self.connectionPhaseText(Int64($0), sibling: sibling)) }
        case "bandFollow":
            switch Self.number(raw) {
            case 2?: return .text("Band follow: following the radio")
            case 1?: return .text("Band follow: waiting for the Power Genius to pair with the radio.")
            default: return .text("Band follow: off while the Power Genius is not connected.")
            }
        case "rfkitBandFollow":
            return .text(Self.rfkitBandFollowText(Int64(Self.number(raw) ?? 0), sibling: sibling))
        case "fourO3AListener":
            if Self.flag(raw) == true { return .text("Core listening on TCP 4992") }
            if let error = Self.text(sibling("fourO3AListenerError")), !error.isEmpty {
                return .text("Core listener error: \(error)")
            }
            if Self.flag(sibling("fourO3AEnabled")) == true { return .text("Core listener starting") }
            return .text("Disabled at Core")
        case "sinceMs":
            guard let epoch = Self.number(raw), epoch > 0 else { return .text("--") }
            let seconds = max(0, Int64((wallClockNow().timeIntervalSince1970 * 1_000 - epoch) / 1_000))
            return .text(Self.duration(seconds, shortest: true))
        case "bytes":
            return Self.number(raw).map { .text(Self.bytesText($0)) }
        case "rttAverage":
            guard let ms = Self.number(raw), ms != 0 else { return .text("--") }
            return .text("\(Int64(ms.rounded())) ms avg")
        case "clockTime":
            guard let epoch = Self.number(raw), epoch > 0 else { return .text("--") }
            let formatter = DateFormatter()
            formatter.dateFormat = "HH:mm:ss"
            return .text(formatter.string(from: Date(timeIntervalSince1970: epoch / 1_000)))
        default:
            // A format this phone does not know shows the plain value.
            return Self.plain(raw, control: control)
        }
    }

    /// A value without a format: Yes or No for a switch, else as sent.
    static func plain(_ raw: MirrorValue, control: SetupDescription.Control) -> SetupValue? {
        switch raw {
        case .bool(let on):
            return .text(on ? "Yes" : "No")
        case .int(let whole), .enumeration(let whole):
            return .integer(whole)
        case .double(let number):
            return number.isFinite ? .decimal(number) : nil
        case .text(let text):
            return .text(text)
        }
    }

    // MARK: The PA temperature unit

    /// The desktop's key for its C/F choice (src/core/PaTempUnit.h), which
    /// the phone keeps as its own.
    public nonisolated static let paTempUnitKey = "PaTempUnit"
    nonisolated static let celsiusUnit = "C"
    nonisolated static let fahrenheitUnit = "F"

    /// The phone key a reading's temperature unit is kept under, or nil
    /// when the row is no temperature.
    public func temperatureUnitKey(of control: SetupDescription.Control) -> String? {
        if let key = control.modern?.temperatureUnit {
            return key
        }
        return control.modern?.format == "paTemperature" ? Self.paTempUnitKey : nil
    }

    /// Whether the reading shows in Fahrenheit, as this phone keeps it;
    /// nil when the row is no temperature or the phone keeps no unit.
    /// The desktop flips its unit by a click on the readout
    /// (src/gui/MainWindow.cpp, SystemTile::paTempClicked); the phone
    /// offers the same choice as a visible control beside the reading.
    public func temperatureInFahrenheit(_ control: SetupDescription.Control) -> Bool? {
        guard let key = temperatureUnitKey(of: control), let unit = phone?.value(forPhoneKey: key)?.text else {
            return nil
        }
        return unit.trimmingCharacters(in: .whitespaces).uppercased() == Self.fahrenheitUnit
    }

    /// Keeps the unit on this phone; false when the phone keeps none.
    @discardableResult
    public func setTemperatureInFahrenheit(_ fahrenheit: Bool, for control: SetupDescription.Control) -> Bool {
        guard let key = temperatureUnitKey(of: control), let phone else {
            return false
        }
        return phone.set(.text(fahrenheit ? Self.fahrenheitUnit : Self.celsiusUnit), forPhoneKey: key)
    }

    /// Degrees in the viewer's own PA temperature unit (setup description:
    /// `PaTempUnit` `C` or `F`, default `C`): this phone's setting of that
    /// name. The Core's `PaTempUnit` is its own operator's
    /// (SettingsScope: operator-local), never read here. Celsius while the
    /// phone keeps no such setting.
    func temperatureText(_ celsius: Double, unitKey: String, decimals: Int) -> String {
        let fahrenheit = phone?.value(forPhoneKey: unitKey)?.text?.trimmingCharacters(in: .whitespaces)
            .uppercased() == Self.fahrenheitUnit
        let shown = fahrenheit ? celsius * 9 / 5 + 32 : celsius
        return String(format: "%.\(max(0, min(decimals, 6)))f", shown) + (fahrenheit ? " \u{00B0}F" : " \u{00B0}C")
    }

    /// "1h 02m 03s", "2m 03s", or with `shortest` "3s".
    static func duration(_ seconds: Int64, shortest: Bool) -> String {
        let hours = seconds / 3_600
        let minutes = (seconds % 3_600) / 60
        let rest = seconds % 60
        if hours > 0 { return String(format: "%lldh %02lldm %02llds", hours, minutes, rest) }
        if minutes > 0 || !shortest { return String(format: "%lldm %02llds", minutes, rest) }
        return "\(rest)s"
    }

    static func bytesText(_ bytes: Double) -> String {
        let units = ["KB", "MB", "GB"]
        guard bytes >= 1_024 else { return "\(Int64(bytes)) B" }
        var value = bytes / 1_024
        var index = 0
        while value >= 1_024, index < units.count - 1 {
            value /= 1_024
            index += 1
        }
        return "\(oneDecimal(value)) \(units[index])"
    }

    static func connectionPhaseText(_ phase: Int64, sibling: (String) -> MirrorValue?) -> String {
        let error = text(sibling("connectionError")) ?? ""
        switch phase {
        case 0: return "Disabled at the Core"
        case 1: return "Disconnected"
        case 2: return "Discovering at the Core"
        case 3: return "Connecting at the Core"
        case 4: return "Identifying device"
        case 5: return error.isEmpty ? "Retrying at the Core" : "Retrying at the Core: \(error)"
        case 6:
            let parts = [text(sibling("deviceModel")) ?? "", text(sibling("deviceSerial")) ?? ""].filter { !$0.isEmpty }
            return parts.isEmpty ? "Connected" : "Connected: " + parts.joined(separator: " ")
        case 7: return "Error: \(error)"
        default: return "Disconnected"
        }
    }

    static func rfkitBandFollowText(_ state: Int64, sibling: (String) -> MirrorValue?) -> String {
        switch state {
        case 2:
            return "Band follow: following the radio"
        case 1:
            let address = text(sibling("bandFollowAddress")) ?? ""
            let port = number(sibling("bandFollowPort")) ?? 0
            if address.isEmpty || port <= 0 {
                return "Band follow: waiting for the amplifier to connect to the TCI server."
            }
            return "Band follow: enter \(address), port \(Int64(port)) as the TCI server on the amplifier."
        case 3:
            return "Band follow: the TCI server accepts only apps on its own computer, so the amplifier cannot reach it."
        default:
            return "Band follow: off. Turn on the TCI server so the amplifier can follow the radio."
        }
    }
}
