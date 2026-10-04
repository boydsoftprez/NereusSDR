// NereusSDR for iOS: Setup rows from description versions 13 to 22, read leniently
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

extension SetupDescription {
    /// Where a V15 choice row takes its list from while the page is open.
    public enum ChoicesSource: Equatable, Sendable {
        /// A property holding a JSON array of names; each name is a choice.
        case jsonNames(PropertyReference)
        /// The Core's installed DSP files of one family (`nr3`).
        case dspAssets(String)
    }

    /// V15's `enabledWhen` on a Core property: the row is live only while
    /// that property's value is one of `oneOf`.
    public struct PropertyDependency: Equatable, Sendable {
        public let property: PropertyReference
        public let oneOf: [Literal]
    }

    /// V18's `enabledWhen` on a radio setting: the row is live only while
    /// the connected radio's setting at `path` is one of `oneOf` (the CL2
    /// frequency follows Enable CL2).
    public struct RadioSettingDependency: Equatable, Sendable {
        public let path: String
        public let oneOf: [Literal]
    }

    /// One of the CFC editor's values: a header field or a band column,
    /// with the Core's label, range, precision and unit.
    public struct CfcField: Equatable, Sendable {
        public let id: String
        public let label: String
        public let kind: Kind
        public let range: Range?
        public let decimals: Int
        public let unit: String
    }

    /// V19's CFC band editor (`dsp.cfc.bands`): the profile it edits, the
    /// verb that changes it, the band counts it offers, how far apart Low
    /// and High stay, and its header fields and band columns in order.
    public struct CfcEditor: Equatable, Sendable {
        public let property: PropertyReference
        public let verb: String
        public let bandCounts: [Int]
        public let minSpanHz: Double
        public let fields: [CfcField]
        public let columns: [CfcField]

        public static let fieldIds: Set<String> = ["minHz", "maxHz", "parametric", "precompDb", "postEqGainDb"]
        public static let columnIds: Set<String> = ["frequencyHz", "compressionDb", "compressionQ", "postEqGainDb", "postEqQ"]

        public func field(_ id: String) -> CfcField? { fields.first { $0.id == id } }
        public func column(_ id: String) -> CfcField? { columns.first { $0.id == id } }
    }

    /// The CFC editor as the Core describes it; nil when any part of it
    /// does not read, and the row stays greyed.
    static func cfcEditor(_ binding: Any?, _ raw: [String: Any]) -> CfcEditor? {
        guard let source = binding as? [String: Any], let object = nonempty(source["object"]),
              let name = nonempty(source["name"]), let verb = nonempty(source["command"]),
              let rawCounts = raw["bandCounts"] as? [Any], !rawCounts.isEmpty,
              let span = number(raw["minSpanHz"]), span.isFinite, span >= 0,
              let fields = cfcFields(raw["fields"]), let columns = cfcFields(raw["columns"]),
              Set(fields.map(\.id)).isSuperset(of: CfcEditor.fieldIds),
              Set(columns.map(\.id)).isSuperset(of: CfcEditor.columnIds) else { return nil }
        let counts = rawCounts.compactMap(integer).map(Int.init)
        guard counts.count == rawCounts.count, counts.allSatisfy({ $0 >= 2 }) else { return nil }
        return CfcEditor(property: PropertyReference(object: object, name: name), verb: verb, bandCounts: counts,
                         minSpanHz: span, fields: fields, columns: columns)
    }

    private static func cfcFields(_ value: Any?) -> [CfcField]? {
        guard let rawFields = value as? [[String: Any]], !rawFields.isEmpty else { return nil }
        var fields: [CfcField] = []
        for rawField in rawFields {
            guard let id = nonempty(rawField["id"]), let label = nonempty(rawField["label"]),
                  let kind = (rawField["kind"] as? String).flatMap(Kind.init(rawValue:)),
                  kind == .toggle || kind == .decimal || kind == .integer else { return nil }
            var range: Range?
            if kind != .toggle {
                guard let low = number(rawField["min"]), let high = number(rawField["max"]),
                      let step = number(rawField["step"]), low.isFinite, high.isFinite, step.isFinite,
                      low < high, step > 0, step <= high - low else { return nil }
                range = Range(minimum: low, maximum: high, step: step)
            }
            let decimals = rawField["decimals"].flatMap(integer).map(Int.init) ?? 0
            guard (0...6).contains(decimals) else { return nil }
            fields.append(CfcField(id: id, label: label, kind: kind, range: range, decimals: decimals,
                                   unit: (rawField["unit"] as? String ?? "").trimmingCharacters(in: .whitespaces)))
        }
        return fields
    }

    /// What a V13 to V16 row adds to the V12 fields. Rows the phone cannot
    /// run carry `pendingReason`, a plain reason shown with the greyed row;
    /// they are never hidden and never counted as bad metadata.
    public struct Modern: Equatable, Sendable {
        public var format: String?
        public var valueOffset: Double?
        /// The station catalogue's transmit range this row's limits come from.
        public var rangeFrom: String?
        public var choicesFrom: ChoicesSource?
        /// The Setup page an `openSetupPage` button opens.
        public var target: String?
        public var copyText: String?
        /// A fixed reading the Core filled in (the radio facts).
        public var readoutValue: String?
        public var temperatureUnit: String?
        public var maxLength: Int?
        /// The PA board class a calibration point belongs to.
        public var boardClass: Int64?
        /// The value each option sends, by its index, when options are not
        /// whole numbers (a switch or a name).
        public var optionLiterals: [Int64: Literal] = [:]
        public var propertyDependency: PropertyDependency?
        public var radioSettingDependency: RadioSettingDependency?
        /// A question is asked only when the row is set to this value.
        public var confirmWhen: Literal?
        public var pendingReason: String?

        public init() {}
    }

    static let desktopOnlyReason = "This setting is changed on the desktop."
    static let filterPresetsReason = "Filter presets are edited on the desktop."
    static let paProfileReason = "PA profiles are chosen and edited on the desktop."
    static let promptReason = "Naming a profile is done on the desktop."
    static let unsavedChangesReason = "Switching profiles is done on the desktop, where changes can be saved first."
    static let unfilledReason = "Connect a radio to use this setting."
    static let noChoicesReason = "The Core sent no choices for this setting."
    static let noLimitsReason = "The Core sent no limits for this setting."

    static func parseModernControl(_ raw: [String: Any], id: String, label: String, version: Int,
                                   required: Int?) -> Control {
        var modern = Modern()
        var issue: String?
        func reject(_ reason: String) { if issue == nil { issue = reason } }
        func pending(_ reason: String) { if modern.pendingReason == nil { modern.pendingReason = reason } }

        if let required, required > min(version, highestVersion) { reject("Unsupported Setup control version.") }
        let rawKind = raw["kind"] as? String ?? ""
        let kind = Kind(rawValue: rawKind)
        if kind == nil || kind == .settingsHygiene { pending(desktopOnlyReason) }
        let applies: Applies? = (raw["applies"] as? String).flatMap(Applies.init(rawValue:))
        if applies == nil { pending(desktopOnlyReason) }

        // Options: whole numbers keep their values; anything else is chosen
        // by index and sends its own literal.
        var options: [Option]?
        if let rawOptions = raw["options"] as? [[String: Any]] {
            var parsed: [Option] = []
            let literals = rawOptions.map { literal($0["value"]) }
            let whole = literals.allSatisfy { if case .integer? = $0 { return true } else { return false } }
            for (index, rawOption) in rawOptions.enumerated() {
                guard let text = rawOption["label"] as? String, let value = literals[index] else {
                    reject("Invalid Setup options."); break
                }
                if whole, case .integer(let number) = value {
                    parsed.append(Option(value: number, label: text))
                } else {
                    parsed.append(Option(value: Int64(index), label: text))
                    modern.optionLiterals[Int64(index)] = value
                }
            }
            options = parsed
        } else if raw["options"] != nil {
            reject("Invalid Setup options.")
        }
        var choices: [String]?
        if let values = raw["choices"] as? [String] { choices = values }

        if let source = raw["choicesFrom"] as? [String: Any] {
            if let names = property(source["jsonNames"]) {
                modern.choicesFrom = .jsonNames(names)
            } else if let family = nonempty(source["dspAssets"]) {
                modern.choicesFrom = .dspAssets(family)
            } else {
                pending(desktopOnlyReason)
            }
        }
        if let source = raw["rangeFrom"] as? [String: Any] {
            if let name = nonempty(source["catalogueTransmit"]) { modern.rangeFrom = name } else { pending(desktopOnlyReason) }
        }

        let availability: Availability?
        if let rawAvailability = raw["availability"] as? [String: Any],
           let enabled = boolean(rawAvailability["enabled"]) {
            let reason = rawAvailability["reason"] as? String ?? ""
            availability = Availability(enabled: enabled, reason: enabled || !reason.isEmpty ? reason : desktopOnlyReason)
        } else {
            availability = nil
        }
        let greyed = availability?.enabled == false

        if kind == .choice, options?.isEmpty != false, choices?.isEmpty != false, modern.choicesFrom == nil, !greyed,
           (raw["binding"] as? [String: Any])?["paProfile"] as? String != "active" {
            pending(noChoicesReason)
        }

        var range: Range?
        let numeric = kind == .integer || kind == .decimal || (kind == .slider && options == nil)
        if numeric {
            if let low = number(raw["min"]), let high = number(raw["max"]), let step = number(raw["step"]) {
                if low.isFinite, high.isFinite, step.isFinite, low < high, step > 0, step <= high - low {
                    range = Range(minimum: low, maximum: high, step: step)
                } else {
                    reject("Invalid Setup numeric range.")
                }
            } else if modern.rangeFrom == nil && !greyed && raw["rangeSource"] == nil {
                pending(noLimitsReason)
            }
        }

        let decimals = raw["decimals"].flatMap(integer).map(Int.init).flatMap { (0...6).contains($0) ? $0 : nil }

        var gate: Gate?
        if let rawGate = raw["gate"] as? [String: Any] {
            gate = Gate(capability: rawGate["capability"] as? String, minimum: rawGate["min"].flatMap(integer),
                        transmit: rawGate["transmit"].flatMap(boolean), board: rawGate["board"] as? String,
                        offAir: rawGate["offAir"].flatMap(boolean), micLine: rawGate["micLine"].flatMap(boolean))
        }

        var encoding: ValueEncoding?
        if let rawEncoding = raw["valueEncoding"] as? [String: Any],
           let yes = rawEncoding["true"] as? String, let no = rawEncoding["false"] as? String {
            encoding = ValueEncoding(trueValue: yes, falseValue: no)
        }

        if let rawWhen = raw["enabledWhen"] as? [String: Any] {
            if let reference = property(rawWhen["property"]), let values = rawWhen["oneOf"] as? [Any] {
                modern.propertyDependency = PropertyDependency(property: reference, oneOf: values.compactMap(literal))
            } else if let path = nonempty(rawWhen["radioSetting"]), let values = rawWhen["oneOf"] as? [Any] {
                modern.radioSettingDependency = RadioSettingDependency(path: path, oneOf: values.compactMap(literal))
            } else {
                pending(desktopOnlyReason)
            }
        }

        var binding: Binding?
        if let rawBinding = raw["binding"] as? [String: Any], rawBinding.count == 1, let (key, data) = rawBinding.first {
            switch key {
            case "setting":
                binding = nonempty(data).map(Binding.setting)
            case "phone":
                if let name = nonempty(data) {
                    if name == "NotchVisualEnabled" {
                        // Visual Notch is a Station setting on the Core; the
                        // phone keeps no copy of its own.
                        binding = .setting(name)
                        encoding = ValueEncoding(trueValue: "True", falseValue: "False")
                    } else {
                        binding = .phone(name)
                    }
                }
            case "property":
                binding = property(data).map(Binding.property)
            case "command":
                binding = command(data, id: id, row: false, modern: true).map(Binding.command)
            case "telemetry":
                binding = property(data).map(Binding.telemetry)
            case "radioSetting":
                binding = nonempty(data).map(Binding.radioSetting)
            case "radioInfo":
                binding = nonempty(data).map(Binding.radioInfo)
            case "adcOverload":
                binding = ((data as? [String: Any]).flatMap { nonempty($0["object"]) }).map(Binding.adcOverload)
            case "cfcProfile":
                if let editor = cfcEditor(data, raw) {
                    binding = .cfcProfile(editor)
                } else {
                    binding = .unsupported(key); pending(desktopOnlyReason)
                }
            case "filterPresets":
                binding = .unsupported(key); pending(filterPresetsReason)
            case "paProfile":
                if paMetadataValid(raw, version: version, grid: false) {
                    binding = paProfileBinding(data, raw: raw, id: id, kind: kind).map(Binding.paProfile)
                }
                if binding == nil { reject("Invalid PA profile metadata.") }
            case "paProfileGrid":
                if paMetadataValid(raw, version: version, grid: true) {
                    binding = paProfileGrid(data, raw: raw, id: id, kind: kind, version: version).map(Binding.paProfileGrid)
                }
                if binding == nil { reject("Invalid PA profile metadata.") }
            default:
                binding = .unsupported(key); pending(desktopOnlyReason)
            }
            if binding == nil { reject("Invalid Setup binding or argument source.") }
        } else {
            reject("Invalid Setup binding or argument source.")
        }
        if case .command(let value)? = binding, value.arguments.values.contains(.prompt) { pending(promptReason) }
        if raw["prompt"] != nil {
            if case .paProfile? = binding {} else { pending(promptReason) }
        }
        if raw["unsavedChanges"] != nil { pending(unsavedChangesReason) }
        if kind == .table {
            switch binding {
            case .unsupported?, .cfcProfile?, .paProfileGrid?: break
            default: pending(desktopOnlyReason)
            }
        }
        if raw["rangeSource"] != nil || raw["telemetryFor"] != nil || raw["availableOn"] != nil { pending(unfilledReason) }
        if case .telemetry(let reference)? = binding, reference.object != "radio" { pending(desktopOnlyReason) }

        modern.format = raw["format"] as? String
        modern.valueOffset = number(raw["valueOffset"])
        modern.target = nonempty(raw["target"])
        modern.copyText = raw["copyText"] as? String
        modern.readoutValue = raw["value"] as? String
        modern.temperatureUnit = raw["temperatureUnit"] as? String
        modern.maxLength = raw["maxLength"].flatMap(integer).map(Int.init)
        modern.boardClass = raw["boardClass"].flatMap(integer)
        modern.confirmWhen = raw["confirmWhen"].flatMap(literal)
        if case .phone("openSetupPage")? = binding, modern.target == nil { pending(desktopOnlyReason) }

        // JSON drops the difference between 10 and 10.0; a decimal row's
        // default stays a decimal.
        var defaultValue = raw["default"].flatMap(literal)
        if kind == .decimal, case .integer(let whole)? = defaultValue { defaultValue = .decimal(Double(whole)) }

        let reason = issue ?? modern.pendingReason ?? (greyed ? availability?.reason : nil)
        return Control(id: id, label: label, tooltip: raw["tooltip"] as? String ?? "", rawKind: rawKind, kind: kind,
                       binding: binding, applies: applies, range: range, unit: raw["unit"] as? String,
                       choices: choices, decimals: decimals, gate: gate, availability: availability,
                       valueEncoding: encoding, requiresDescriptionVersion: required,
                       defaultValue: defaultValue, options: options,
                       enabledWhen: nil, hygieneActions: nil, phoneDependency: nil, perBand: nil,
                       confirm: raw["confirm"] as? String, metadataIssue: issue,
                       unavailableReason: reason, modern: modern)
    }

    static func literal(_ value: Any?) -> Literal? {
        guard let value else { return nil }
        if let bool = boolean(value) { return .bool(bool) }
        if let text = value as? String { return .text(text) }
        if let whole = integer(value) { return .integer(whole) }
        if let double = number(value), double.isFinite { return .decimal(double) }
        return nil
    }
}
