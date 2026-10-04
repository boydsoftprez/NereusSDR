// NereusSDR for iOS: typed Setup metadata received from the Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreFoundation
import Foundation

/// Ordered Setup metadata. Text and ranges are supplied by the current Core.
/// An invalid control retains its ID and label with an unavailable reason.
public struct SetupDescription: Equatable, Sendable {
    public struct ParseError: Error, Equatable, Sendable {
        public let reason: String
        public init(_ reason: String) { self.reason = reason }
    }

    public enum Owner: String, Sendable { case station, phone, mixed }
    public enum Kind: String, Sendable {
        case toggle, integer, decimal, slider, choice, text, colour, button, readout, table, settingsHygiene
    }
    /// `staged` (V15): the value is held on this phone and sent only by a
    /// button that names it with `$control`.
    public enum Applies: String, Sendable { case live, subscription, staged }
    public struct Category: Equatable, Sendable {
        public let id: String
        public let title: String
        public let whereOwned: Owner
        public let coverage: String?
    }
    public struct Page: Equatable, Sendable {
        public let id: String
        public let title: String
        public let whereOwned: Owner
        public let coverage: String?
        public let sections: [Section]
    }
    public struct Section: Equatable, Sendable {
        public let title: String
        public let controls: [Control]
    }
    public struct Range: Equatable, Sendable {
        public let minimum: Double
        public let maximum: Double
        public let step: Double

        public init(minimum: Double, maximum: Double, step: Double) {
            self.minimum = minimum
            self.maximum = maximum
            self.step = step
        }
    }
    public struct Gate: Equatable, Sendable {
        public let capability: String?
        public let minimum: Int64?
        public let transmit: Bool?
        public let board: String?
        public let offAir: Bool?
        /// V15: the row needs the radio's own microphone line.
        public let micLine: Bool?

        public init(capability: String?, minimum: Int64?, transmit: Bool?, board: String?, offAir: Bool?,
                    micLine: Bool? = nil) {
            self.capability = capability
            self.minimum = minimum
            self.transmit = transmit
            self.board = board
            self.offAir = offAir
            self.micLine = micLine
        }
    }
    public struct Availability: Equatable, Sendable {
        public let enabled: Bool
        public let reason: String
    }
    public struct ValueEncoding: Equatable, Sendable {
        public let trueValue: String
        public let falseValue: String
    }
    public struct PropertyReference: Equatable, Sendable {
        public let object: String
        public let name: String
    }
    public enum Literal: Equatable, Sendable {
        case bool(Bool), integer(Int64), decimal(Double), text(String)
    }
    public enum Argument: Equatable, Sendable {
        case literal(Literal)
        case controlValue
        case property(PropertyReference)
        case selectedOwnedSliceId
        case row(String)
        case edit(String)
        /// V15: the value another row of the page holds (a staged row).
        case control(String)
        /// V15: a name the desktop asks for; never sent from the phone.
        case prompt
    }
    public struct Command: Equatable, Sendable {
        public let verb: String
        public let valueProperty: PropertyReference?
        public let arguments: [String: Argument]
    }
    public struct Table: Equatable, Sendable {
        public let valueProperty: PropertyReference
        public let revisionProperty: PropertyReference
        public let format: String
        public let rowKey: String
        public let maxRows: Int
        public let columns: [Column]
        public let rowActions: [RowAction]
    }
    public struct Column: Equatable, Sendable {
        public let id: String
        public let field: String?
        public let rowAction: String?
        public let label: String
        public let tooltip: String
        public let kind: Kind
        public let range: Range?
    }
    public struct RowAction: Equatable, Sendable {
        public let id: String
        public let command: Command
    }
    public struct Option: Equatable, Sendable {
        public let value: Int64
        public let label: String
    }
    public struct Dependency: Equatable, Sendable {
        public let setting: String
        public let oneOf: [String]
    }
    /// V12's `enabledWhen` on a phone key: the row is live only while that
    /// key's kept value is one of `oneOf` (a switch or a whole number).
    public struct PhoneDependency: Equatable, Sendable {
        public let phone: String
        public let oneOf: [Literal]
    }
    /// V12's `perBand`: the value is kept for each band, and the label is
    /// shown with `%1` replaced by the band's name.
    public struct PerBand: Equatable, Sendable {
        public let label: String

        /// The label for `band`, the desktop's band name.
        public func label(for band: String) -> String {
            label.replacingOccurrences(of: "%1", with: band)
        }
    }
    public struct HygieneAction: Equatable, Sendable {
        public let id: String
        public let label: String
        public let enabled: Bool
        public let reason: String?
        public let paired: Bool
        public let offAir: Bool
        public let confirmation: Confirmation?
        /// The `settingsHygieneVersion` the Core must send before this
        /// action can be used; below it `reason` says why. Nil: no gate.
        public let minimumVersion: Int64?
    }
    public struct Confirmation: Equatable, Sendable {
        public let title: String
        public let message: String
        public let defaultAction: String
    }
    public struct AntennaCell: Equatable, Sendable {
        public let column: String
        public let tooltip: String
    }
    public struct AntennaRow: Equatable, Sendable {
        public let band: Int
        public let label: String
        public let cells: [AntennaCell]
    }
    public struct AntennaColumn: Equatable, Sendable {
        public let id: String
        public let field: String
        public let antenna: Int
        public let label: String
    }
    public struct AntennaColumnGroup: Equatable, Sendable {
        public let label: String
        public let columns: [String]
    }
    public struct AntennaRows: Equatable, Sendable {
        public let object: String
        public let mode: String
        public let rows: [AntennaRow]
        public let columns: [AntennaColumn]
        public let columnGroups: [AntennaColumnGroup]
    }
    public enum Binding: Equatable, Sendable {
        case setting(String), phone(String), property(PropertyReference), command(Command), table(Table)
        case settingsHygiene, telemetry(PropertyReference), antennaRows(AntennaRows)
        /// V13: a per-radio setting kept under `hardware/<MAC>/<path>`.
        case radioSetting(String)
        /// V13: a fact about the connected radio, filled in by the Core.
        case radioInfo(String)
        /// V13: the ADC overload flags of a step attenuator object.
        case adcOverload(String)
        /// V19: the CFC band editor, on the profile the Core holds.
        case cfcProfile(CfcEditor)
        /// V14 lifecycle and band table, with V20 row availability.
        case paProfile(PaProfileBinding), paProfileGrid(PaProfileGrid)
        /// A V13 to V16 source this phone does not run; the row is greyed.
        case unsupported(String)
    }
    public struct Control: Equatable, Sendable {
        public let id: String
        public let label: String
        public let tooltip: String
        public let rawKind: String
        public let kind: Kind?
        public let binding: Binding?
        public let applies: Applies?
        public internal(set) var range: Range?
        public let unit: String?
        public internal(set) var choices: [String]?
        public let decimals: Int?
        public let gate: Gate?
        public let availability: Availability?
        public let valueEncoding: ValueEncoding?
        public let requiresDescriptionVersion: Int?
        public let defaultValue: Literal?
        public internal(set) var options: [Option]?
        public let enabledWhen: Dependency?
        public let hygieneActions: [HygieneAction]?
        /// V12: the row follows another phone key (see ``PhoneDependency``).
        public let phoneDependency: PhoneDependency?
        /// V12: the value is kept per band (see ``PerBand``).
        public let perBand: PerBand?
        /// V12: a button's question, asked with Yes and No before it runs.
        public internal(set) var confirm: String?
        /// Bad wire metadata, separate from a Core-declared disabled state.
        public let metadataIssue: String?
        public let unavailableReason: String?
        /// V13 to V16 additions; nil for an older row.
        public internal(set) var modern: Modern? = nil
        public var isEditable: Bool {
            if kind == .readout || kind == .settingsHygiene { return false }
            if case .antennaRows? = binding { return false }
            return unavailableReason == nil && availability?.enabled != false
        }
    }

    public let version: Int
    public let category: Category
    public let pages: [Page]

    /// The highest description version this parser reads. What the phone
    /// asks the Core for is separate: `LinkFeatures`' `setupDescription`.
    public static let highestVersion = 24

    /// The desktop's band names, by the Core's band number, as a per-band
    /// row's label names them (`src/models/Band.h`).
    public static let bandNames = ["160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m",
                                   "12m", "10m", "6m", "GEN", "WWV", "XVTR"]
    /// 2 m's band number on the link (`band2m` 1, link document 6.1) and
    /// its label, the Core's `2m`.
    public static let twoMetreBand = 27
    public static let twoMetreName = "2m"

    /// The desktop's name for the Core's band `number`: 160m to XVTR by
    /// their numbers 0 to 13, and 2m for 27; nil for any other.
    public static func bandName(_ number: Int) -> String? {
        if number == twoMetreBand {
            return twoMetreName
        }
        return bandNames.indices.contains(number) ? bandNames[number] : nil
    }

    /// 2 m, the band the Core numbers 27 (link document section 6.1,
    /// `band2m`): after XVTR in the Core's per-band lists, and only for a
    /// phone that declares `band2m` to a Core that sends `band2mVersion` 1.
    public static let band2m = 27
    public static let band2mName = "2m"

    /// This description without the controls in `ids` or the pages in
    /// `ids`; a section or page left empty goes too.
    public func leavingOut(_ ids: Set<String>) -> SetupDescription {
        guard !ids.isEmpty else { return self }
        let kept: [Page] = pages.compactMap { page in
            guard !ids.contains(page.id) else { return nil }
            let sections: [Section] = page.sections.compactMap { section in
                let controls = section.controls.filter { !ids.contains($0.id) }
                return controls.isEmpty ? nil : Section(title: section.title, controls: controls)
            }
            return sections.isEmpty ? nil : Page(id: page.id, title: page.title, whereOwned: page.whereOwned,
                                                 coverage: page.coverage, sections: sections)
        }
        return SetupDescription(version: version, category: category, pages: kept)
    }

    /// Structural corruption rejects a category. Invalid control metadata
    /// remains in order, visibly unavailable to the later renderer.
    public static func parse(json: String) throws -> SetupDescription {
        guard json.utf8.count <= 8 * 1024 * 1024 else { throw ParseError("Setup category exceeds the link message limit.") }
        guard let data = json.data(using: .utf8),
              let root = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw ParseError("Setup category is not readable JSON.")
        }
        guard let version = integer(root["version"]), (1...Int64(highestVersion)).contains(version) else {
            throw ParseError("Setup description version is unavailable.")
        }
        guard Set(root.keys).isSubset(of: ["version", "category", "coverage", "coverageV15", "pages"]) else {
            throw ParseError("Unknown Setup category metadata.")
        }
        guard let rawCategory = root["category"] as? [String: Any],
              let categoryId = nonempty(rawCategory["id"]),
              let categoryTitle = nonempty(rawCategory["title"]),
              let owner = (rawCategory["where"] as? String).flatMap(Owner.init(rawValue:)),
              let rawPages = root["pages"] as? [[String: Any]], !rawPages.isEmpty else {
            throw ParseError("Setup category metadata is incomplete.")
        }
        guard Set(rawCategory.keys).isSubset(of: ["id", "title", "where", "coverage"]),
              (rawCategory["coverage"] == nil || rawCategory["coverage"] is String),
              (root["coverage"] == nil || root["coverage"] is String) else {
            throw ParseError("Unknown Setup category metadata.")
        }
        let category = Category(id: categoryId, title: categoryTitle, whereOwned: owner,
                                coverage: rawCategory["coverage"] as? String ?? root["coverage"] as? String)
        var ids = Set<String>()
        var pages: [Page] = []
        for rawPage in rawPages {
            guard let pageId = nonempty(rawPage["id"]), ids.insert(pageId).inserted,
                  let title = nonempty(rawPage["title"]),
                  let pageOwner = (rawPage["where"] as? String).flatMap(Owner.init(rawValue:)),
                  let rawSections = rawPage["sections"] as? [[String: Any]], !rawSections.isEmpty else {
                throw ParseError("Setup page metadata is incomplete or duplicated.")
            }
            guard Set(rawPage.keys).isSubset(of: ["id", "title", "where", "coverage", "coverageV15", "sections"]),
                  (rawPage["coverage"] == nil || rawPage["coverage"] is String) else {
                throw ParseError("Unknown Setup page metadata.")
            }
            var sections: [Section] = []
            for rawSection in rawSections {
                guard let sectionTitle = nonempty(rawSection["title"]),
                      let rawControls = rawSection["controls"] as? [[String: Any]], !rawControls.isEmpty else {
                    throw ParseError("Setup section metadata is incomplete.")
                }
                guard Set(rawSection.keys).subtracting(["boardFamily"]) == ["title", "controls"] else {
                    throw ParseError("Unknown Setup section metadata.")
                }
                var controls: [Control] = []
                for raw in rawControls {
                    guard let id = nonempty(raw["id"]), ids.insert(id).inserted,
                          let label = nonempty(raw["label"]) else {
                        throw ParseError("Setup control identity is missing or duplicated.")
                    }
                    controls.append(parseControl(raw, id: id, label: label, version: Int(version), category: categoryId))
                }
                sections.append(Section(title: sectionTitle, controls: controls))
            }
            pages.append(Page(id: pageId, title: title, whereOwned: pageOwner,
                              coverage: rawPage["coverage"] as? String, sections: sections))
        }
        return SetupDescription(version: Int(version), category: category, pages: pages)
    }

    private static func parseControl(_ raw: [String: Any], id: String, label: String, version: Int, category: String) -> Control {
        if let required = raw["requiresDescriptionVersion"].flatMap(integer).map(Int.init), required >= 13 {
            return parseModernControl(raw, id: id, label: label, version: version, required: required)
        }
        let rawKind = raw["kind"] as? String ?? ""
        let kind = Kind(rawValue: rawKind)
        var issue: String?
        func reject(_ reason: String) { if issue == nil { issue = reason } }
        let allowed: Set<String> = ["id", "label", "tooltip", "kind", "binding", "applies", "min", "max", "step", "unit", "choices", "decimals", "gate", "availability", "valueEncoding", "requiresDescriptionVersion", "columns", "rowActions", "default", "options", "enabledWhen", "actions", "rows", "columnGroups", "perBand", "confirm"]
        if !Set(raw.keys).isSubset(of: allowed) { reject("Unknown Setup control metadata.") }
        if kind == nil { reject("Unknown Setup control kind.") }
        let tooltip = raw["tooltip"] as? String
        if tooltip == nil { reject("Setup control tooltip is missing.") }
        let applies = (raw["applies"] as? String).flatMap(Applies.init(rawValue:))
        if applies == nil { reject("Unknown Setup apply mode.") }
        let required = raw["requiresDescriptionVersion"].flatMap(integer).map(Int.init)
        if let required, !(2...12).contains(required) || required > version {
            reject("Unsupported Setup control version.")
        }
        if raw["requiresDescriptionVersion"] != nil && required == nil { reject("Unsupported Setup control version.") }
        let range: Range?
        if kind == .integer || kind == .decimal || (kind == .slider && raw["options"] == nil) {
            if let min = number(raw["min"]), let max = number(raw["max"]), let step = number(raw["step"]),
               min.isFinite, max.isFinite, step.isFinite, (max - min).isFinite,
               min < max, step > 0, step <= max - min,
               (kind != .integer || [min, max, step].allSatisfy({ $0.rounded() == $0 })) {
                range = Range(minimum: min, maximum: max, step: step)
            } else {
                range = nil; reject("Invalid Setup numeric range.")
            }
        } else {
            range = nil
            if raw["min"] != nil || raw["max"] != nil || raw["step"] != nil { reject("Unexpected Setup numeric range.") }
        }
        var choices: [String]?
        if kind == .choice && raw["options"] == nil {
            if let values = raw["choices"] as? [String], !values.isEmpty,
               values.allSatisfy({ !$0.isEmpty }) { choices = values }
            else { reject("Invalid Setup choices.") }
        } else if raw["choices"] != nil { reject("Unexpected Setup choices.") }
        let decimals = raw["decimals"].flatMap(integer).map(Int.init)
        if raw["decimals"] != nil && (decimals == nil || !(0...6).contains(decimals!)
            || (kind != .readout && !(category == "display" && kind == .decimal && version >= 4))) {
            reject("Invalid Setup readout precision.")
        }
        let gate: Gate?
        if let rawGate = raw["gate"] as? [String: Any] {
            let allowedGate: Set<String> = ["capability", "min", "transmit", "board", "offAir"]
            if !Set(rawGate.keys).isSubset(of: allowedGate) { reject("Unknown Setup gate.") }
            let cap = rawGate["capability"] as? String
            let min = rawGate["min"].flatMap(integer)
            let tx = rawGate["transmit"].flatMap(boolean)
            let board = rawGate["board"] as? String
            let offAir = rawGate["offAir"].flatMap(boolean)
            if (rawGate["capability"] != nil && (cap?.isEmpty != false || min == nil || min! < 1))
                || (rawGate["min"] != nil && cap == nil)
                || (rawGate["board"] != nil && board?.isEmpty != false)
                || (rawGate["transmit"] != nil && tx == nil)
                || (rawGate["offAir"] != nil && offAir != true) { reject("Invalid Setup gate.") }
            gate = Gate(capability: cap, minimum: min, transmit: tx, board: board, offAir: offAir)
        } else {
            gate = nil
            if raw["gate"] != nil { reject("Invalid Setup gate.") }
        }
        let availability: Availability?
        if let rawAvailability = raw["availability"] as? [String: Any],
           Set(rawAvailability.keys) == ["enabled", "reason"],
           let enabled = boolean(rawAvailability["enabled"]),
           let reason = rawAvailability["reason"] as? String,
           enabled || !reason.isEmpty {
            availability = Availability(enabled: enabled, reason: reason)
        } else {
            availability = nil
            if raw["availability"] != nil { reject("Invalid Setup availability.") }
        }
        let encoding: ValueEncoding?
        if let rawEncoding = raw["valueEncoding"] as? [String: Any],
           Set(rawEncoding.keys) == ["true", "false"],
           let yes = rawEncoding["true"] as? String, let no = rawEncoding["false"] as? String {
            encoding = ValueEncoding(trueValue: yes, falseValue: no)
        } else {
            encoding = nil
            if raw["valueEncoding"] != nil { reject("Invalid Setup toggle encoding.") }
        }
        var binding = parseBinding(raw["binding"], id: id, kind: kind, version: version,
                                   columns: raw["columns"], rowActions: raw["rowActions"])
        if binding == nil { reject("Invalid Setup binding or argument source.") }
        if case .setting? = binding, kind == .toggle {
            if encoding?.trueValue != "True" || encoding?.falseValue != "False" { reject("Invalid Setup toggle encoding.") }
        } else if encoding != nil { reject("Unexpected Setup toggle encoding.") }
        if kind == .readout {
            switch binding {
            case .property?, .telemetry?: break
            default: reject("Readout needs a property or telemetry source.")
            }
        }
        if kind == .table {
            switch binding {
            case .table?:
                if category != "dsp" || required != 2 || applies != .live || gate?.capability != "notchControlVersion"
                    || gate?.minimum != 1 || gate?.transmit != nil || gate?.board != nil || gate?.offAir != nil {
                    reject("TNF table version or capability gate is invalid.")
                }
            case .antennaRows?: break
            default: reject("Setup table binding is invalid.")
            }
        }
        if id == "dsp.tnf.add" {
            if category != "dsp" || kind != .button || required != 2 || applies != .live
                || gate?.capability != "notchControlVersion" || gate?.minimum != 2
                || gate?.transmit != nil || gate?.board != nil || gate?.offAir != nil {
                reject("TNF Add version or capability gate is invalid.")
            }
            if case .command(let command)? = binding,
               command.verb == "notch.addAtSlice", command.valueProperty == nil,
               command.arguments == ["sliceId": .selectedOwnedSliceId] {} else {
                reject("TNF Add command is invalid.")
            }
        }
        if kind != .table && (raw["columns"] != nil || raw["rowActions"] != nil || raw["rows"] != nil || raw["columnGroups"] != nil) { reject("Unexpected Setup table metadata.") }
        // V21: a CAT and Network row may follow another Core property (the
        // TCI Forget row follows Duplicate RX2 VFO B); read it as V15's
        // `enabledWhen` and keep it out of the closed V3 to V12 checks.
        var closedRaw = raw
        var propertyDependency: PropertyDependency?
        if category == "catNetwork", version >= 21, let rawWhen = raw["enabledWhen"] as? [String: Any] {
            if Set(rawWhen.keys) == ["property", "oneOf"], let reference = property(rawWhen["property"]),
               let values = rawWhen["oneOf"] as? [Any], !values.isEmpty,
               values.allSatisfy({ literal($0) != nil }) {
                propertyDependency = PropertyDependency(property: reference, oneOf: values.compactMap(literal))
                closedRaw.removeValue(forKey: "enabledWhen")
            } else {
                reject("Invalid closed Setup metadata.")
            }
        }
        let extensionMetadata = parseExtension(closedRaw, id: id, category: category, version: version,
                                               kind: kind, binding: binding, applies: applies, gate: gate,
                                               range: range, choices: choices, decimals: decimals, required: required)
        if let antennaRows = extensionMetadata.antennaRows { binding = .antennaRows(antennaRows) }
        if let reason = extensionMetadata.issue { reject(reason) }
        if raw["unit"] != nil && !(raw["unit"] is String) { reject("Invalid Setup unit.") }
        return Control(id: id, label: label, tooltip: tooltip ?? "", rawKind: rawKind, kind: kind,
                       binding: binding, applies: applies, range: range, unit: raw["unit"] as? String,
                       choices: choices, decimals: decimals, gate: gate, availability: availability,
                       valueEncoding: encoding, requiresDescriptionVersion: required,
                       defaultValue: extensionMetadata.defaultValue, options: extensionMetadata.options,
                       enabledWhen: extensionMetadata.dependency, hygieneActions: extensionMetadata.actions,
                       phoneDependency: extensionMetadata.phoneDependency, perBand: extensionMetadata.perBand,
                       confirm: extensionMetadata.confirm, metadataIssue: issue,
                       unavailableReason: issue ?? (availability?.enabled == false ? availability?.reason : nil),
                       modern: propertyDependency.map { dependency in
                           var modern = Modern()
                           modern.propertyDependency = dependency
                           return modern
                       })
    }

    private static func parseBinding(_ value: Any?, id: String, kind: Kind?, version: Int,
                                     columns: Any?, rowActions: Any?) -> Binding? {
        guard let raw = value as? [String: Any], raw.count == 1,
              let (key, data) = raw.first else { return nil }
        switch key {
        case "setting": return nonempty(data).map(Binding.setting)
        case "phone": return nonempty(data).map(Binding.phone)
        case "property": return property(data).map(Binding.property)
        case "settingsHygiene":
            guard id == "diagnostics.settingsValidation.health", version >= 3,
                  let object = data as? [String: Any], Set(object.keys) == ["version"],
                  integer(object["version"]) == 1 else { return nil }
            return .settingsHygiene
        case "telemetry":
            guard version >= 5, let reference = property(data), reference.object == "radio",
                  (id == "pa.values.paCurrent" && reference.name == "paCurrentAmps"
                   || id == "pa.values.dcVoltage" && reference.name == "supplyVolts") else { return nil }
            return .telemetry(reference)
        case "antennaRows":
            guard version >= 6, kind == .table, let object = data as? [String: Any],
                  Set(object.keys) == ["object", "mode"], object["object"] as? String == "alexAntennas",
                  let mode = object["mode"] as? String,
                  (id == "hardware.antenna.txRows" && mode == "tx"
                   || id == "hardware.antenna.rxRows" && mode == "rx") else { return nil }
            return .antennaRows(AntennaRows(object: "alexAntennas", mode: mode, rows: [], columns: [], columnGroups: []))
        case "command": return command(data, id: id, row: false).map(Binding.command)
        case "table":
            guard kind == .table, version >= 2, id == "dsp.tnf.list",
                  let rawTable = data as? [String: Any],
                  Set(rawTable.keys) == ["valueProperty", "revisionProperty", "format", "rowKey", "maxRows"],
                  let source = property(rawTable["valueProperty"]), source.object == "notches", source.name == "listJson",
                  let revision = property(rawTable["revisionProperty"]), revision.object == "notches", revision.name == "revision",
                  rawTable["format"] as? String == "json-array", rawTable["rowKey"] as? String == "id",
                  integer(rawTable["maxRows"]) == 1024,
                  let rawColumns = columns as? [[String: Any]], rawColumns.count == 4,
                  let rawActions = rowActions as? [[String: Any]], rawActions.count == 3 else { return nil }
            var parsedColumns: [Column] = []
            let fields = ["centreHz", "widthHz", "active", "delete"]
            for (index, rawColumn) in rawColumns.enumerated() {
                guard let columnId = nonempty(rawColumn["id"]),
                      let label = nonempty(rawColumn["label"]), let tooltip = rawColumn["tooltip"] as? String,
                      let colKind = (rawColumn["kind"] as? String).flatMap(Kind.init(rawValue:)) else { return nil }
                let field = rawColumn["field"] as? String
                let action = rawColumn["rowAction"] as? String
                let expected = fields[index]
                guard columnId == "dsp.tnf.list.\(expected)" else { return nil }
                let range: Range?
                if index < 2 {
                    guard Set(rawColumn.keys) == ["id", "field", "label", "tooltip", "kind", "min", "max", "step"],
                          field == expected, colKind == .decimal,
                          let min = number(rawColumn["min"]), let max = number(rawColumn["max"]),
                          let step = number(rawColumn["step"]),
                          min == (index == 0 ? 100_000 : 0), max == (index == 0 ? 61_440_000 : 10_000), step == 1 else { return nil }
                    range = Range(minimum: min, maximum: max, step: step)
                } else {
                    guard Set(rawColumn.keys) == (index == 2 ? ["id", "field", "label", "tooltip", "kind"] : ["id", "rowAction", "label", "tooltip", "kind"]),
                          (index == 2 ? field : action) == expected,
                          colKind == (index == 2 ? .toggle : .button) else { return nil }
                    range = nil
                }
                parsedColumns.append(Column(id: columnId, field: field, rowAction: action,
                                            label: label, tooltip: tooltip, kind: colKind, range: range))
            }
            let specs: [(String, String, [String: Argument])] = [
                ("move", "notch.move", ["id": .row("id"), "centreHz": .edit("centreHz"), "widthHz": .edit("widthHz")]),
                ("active", "notch.setActive", ["id": .row("id"), "active": .edit("active")]),
                ("delete", "notch.delete", ["id": .row("id")]),
            ]
            var actions: [RowAction] = []
            for (rawAction, expected) in zip(rawActions, specs) {
                guard Set(rawAction.keys) == ["id", "command"],
                      rawAction["id"] as? String == expected.0,
                      let parsed = command(rawAction["command"], id: id, row: true),
                      parsed.verb == expected.1, parsed.valueProperty == nil,
                      parsed.arguments == expected.2 else { return nil }
                actions.append(RowAction(id: expected.0, command: parsed))
            }
            return .table(Table(valueProperty: source, revisionProperty: revision, format: "json-array",
                                rowKey: "id", maxRows: 1024, columns: parsedColumns, rowActions: actions))
        default: return nil
        }
    }

    static func command(_ value: Any?, id: String, row: Bool, modern: Bool = false) -> Command? {
        guard let raw = value as? [String: Any],
              Set(raw.keys).isSubset(of: ["verb", "arguments", "valueProperty"]),
              let verb = nonempty(raw["verb"]), let rawArguments = raw["arguments"] as? [String: Any] else { return nil }
        let valueProperty = raw["valueProperty"].flatMap(property)
        if raw["valueProperty"] != nil && valueProperty == nil { return nil }
        var arguments: [String: Argument] = [:]
        for (name, source) in rawArguments {
            guard !name.isEmpty,
                  let argument = argument(source, id: id, verb: verb, name: name, row: row, modern: modern) else { return nil }
            arguments[name] = argument
        }
        return Command(verb: verb, valueProperty: valueProperty, arguments: arguments)
    }

    private static func argument(_ value: Any, id: String, verb: String, name: String, row: Bool,
                                 modern: Bool) -> Argument? {
        if let raw = value as? [String: Any] {
            guard raw.count == 1, let (marker, source) = raw.first else { return nil }
            switch marker {
            case "$controlValue": return boolean(source) == true && !row ? .controlValue : nil
            case "$property": return !row ? property(source).map(Argument.property) : nil
            case "$selectedOwnedSliceId" where modern:
                // V15: any verb may name the phone's own selected slice.
                return !row && boolean(source) == true ? .selectedOwnedSliceId : nil
            case "$control" where modern:
                return !row ? nonempty(source).map(Argument.control) : nil
            case "$prompt" where modern:
                return !row && boolean(source) == true ? .prompt : nil
            case "$selectedOwnedSliceId":
                return !row && id == "dsp.tnf.add" && verb == "notch.addAtSlice" && name == "sliceId" && boolean(source) == true ? .selectedOwnedSliceId : nil
            case "$row": return row && source as? String == "id" ? .row("id") : nil
            case "$edit":
                guard row, let field = source as? String, ["centreHz", "widthHz", "active"].contains(field) else { return nil }
                return .edit(field)
            default: return nil
            }
        }
        if let bool = boolean(value) { return .literal(.bool(bool)) }
        if let string = value as? String { return .literal(.text(string)) }
        if let int = integer(value) { return .literal(.integer(int)) }
        if let double = number(value), double.isFinite { return .literal(.decimal(double)) }
        return nil
    }

    static func property(_ value: Any?) -> PropertyReference? {
        guard let raw = value as? [String: Any], Set(raw.keys) == ["object", "name"],
              let object = nonempty(raw["object"]), let name = nonempty(raw["name"]) else { return nil }
        return PropertyReference(object: object, name: name)
    }
    static func nonempty(_ value: Any?) -> String? {
        guard let text = value as? String, !text.isEmpty else { return nil }
        return text
    }
    static func boolean(_ value: Any?) -> Bool? {
        guard let number = value as? NSNumber, CFGetTypeID(number) == CFBooleanGetTypeID() else { return nil }
        return number.boolValue
    }
    static func integer(_ value: Any?) -> Int64? {
        guard let number = value as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID(),
              number.doubleValue.isFinite, number.doubleValue.rounded() == number.doubleValue,
              number.doubleValue >= Double(Int64.min), number.doubleValue < Double(Int64.max) else { return nil }
        return number.int64Value
    }
    static func number(_ value: Any?) -> Double? {
        guard let number = value as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID() else { return nil }
        return number.doubleValue
    }
}
