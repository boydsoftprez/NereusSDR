// NereusSDR for iOS: closed Setup extensions from the accepted Core V3-V12 contract
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreFoundation
import Foundation

extension SetupDescription {
    struct ExtensionMetadata {
        var defaultValue: Literal?
        var options: [Option]?
        var dependency: Dependency?
        var actions: [HygieneAction]?
        var antennaRows: AntennaRows?
        var phoneDependency: PhoneDependency?
        var perBand: PerBand?
        var confirm: String?
        var issue: String?
    }

    static func parseExtension(_ raw: [String: Any], id: String, category: String, version: Int,
                               kind: Kind?, binding: Binding?, applies: Applies?, gate: Gate?,
                               range: Range?, choices: [String]?, decimals: Int?, required: Int?) -> ExtensionMetadata {
        var result = ExtensionMetadata()
        func reject() { if result.issue == nil { result.issue = "Invalid closed Setup metadata." } }
        let extended = ["default", "options", "enabledWhen", "actions", "rows", "columnGroups"]
            .contains { raw[$0] != nil }

        if id == "diagnostics.settingsValidation.health" || kind == .settingsHygiene {
            guard category == "diagnostics", version >= 3, kind == .settingsHygiene,
                  binding == .settingsHygiene, applies == .live, required == 3,
                  exactGate(gate, "settingsHygieneVersion", 1),
                  Set(raw.keys) == ["id", "label", "tooltip", "kind", "binding", "applies",
                                    "requiresDescriptionVersion", "gate", "actions"],
                  raw["label"] as? String == "Validation Issues", raw["tooltip"] as? String == "",
                  let actions = hygieneActions(raw["actions"]) else { reject(); return result }
            result.actions = actions
            return result
        }

        if case .telemetry? = binding {
            let amps = id == "pa.values.paCurrent"
            guard category == "pa", version >= 5, kind == .readout, applies == .live,
                  required == 5, exactGate(gate, "stationTelemetryVersion", 4),
                  Set(raw.keys) == ["id", "label", "tooltip", "kind", "binding", "applies",
                                    "requiresDescriptionVersion", "gate", "decimals", "unit"],
                  raw["label"] as? String == (amps ? "PA Current:" : "DC Voltage:"),
                  raw["tooltip"] as? String == "", decimals == (amps ? 2 : 1),
                  raw["unit"] as? String == (amps ? "A" : "V") else { reject(); return result }
            return result
        }

        if case .antennaRows(let source)? = binding {
            guard category == "hardware", version >= 6, required == 6, kind == .table,
                  applies == .live, exactGate(gate, "radioAntennaRowsVersion", 1,
                                              offAir: source.mode == "tx"),
                  let table = antennaRows(raw, source: source) else { reject(); return result }
            result.antennaRows = table
            return result
        }

        if let spec = v12Specs[id] {
            // V12's rows: the rest of Setup > Display and Appearance's
            // Reset all colors, each checked as the exact closed object.
            guard let checked = checkV12(raw, spec: spec, id: id, category: category, version: version,
                                         kind: kind, binding: binding, applies: applies, gate: gate,
                                         range: range, decimals: decimals, required: required) else {
                reject(); return result
            }
            return checked
        }

        if category == "display", let spec = displayPhoneSpecs[id] {
            guard version >= 8, required == 8, applies == .subscription,
                  binding == .phone(spec.phone), kind == spec.kind,
                  exactGate(gate, spec.capability, spec.minimum),
                  raw["label"] is String, raw["tooltip"] is String,
                  raw["choices"] == nil, raw["enabledWhen"] == nil,
                  raw["actions"] == nil, raw["rows"] == nil,
                  raw["columns"] == nil, raw["columnGroups"] == nil,
                  raw["rowActions"] == nil,
                  let value = int(raw["default"]), value == spec.defaultValue else { reject(); return result }
            if let labels = spec.options {
                guard Set(raw.keys) == ["id", "label", "tooltip", "kind", "binding", "applies",
                                        "requiresDescriptionVersion", "gate", "options", "default"],
                      let options = numberedOptions(raw["options"], labels) else { reject(); return result }
                result.options = options
            } else {
                let hasUnit = spec.unit != nil
                let keys: Set<String> = Set(["id", "label", "tooltip", "kind", "binding", "applies",
                                             "requiresDescriptionVersion", "gate", "min", "max", "step", "default"])
                    .union(hasUnit ? ["unit"] : [])
                guard Set(raw.keys) == keys, range == spec.range,
                      raw["unit"] as? String == spec.unit else { reject(); return result }
            }
            result.defaultValue = .integer(value)
            return result
        }

        if category == "display", let spec = displayRenderSpecs[id] {
            // V9 rendering rows and V10 waterfall overlays: phone-owned, no
            // gate, exact binding, kind, apply mode, range, unit and default.
            var keys: Set<String> = ["id", "label", "tooltip", "kind", "binding", "applies",
                                     "requiresDescriptionVersion", "default"]
            if spec.range != nil { keys.formUnion(["min", "max", "step"]) }
            if spec.unit != nil { keys.insert("unit") }
            guard version >= spec.version, required == spec.version, applies == spec.applies,
                  binding == .phone(spec.phone), kind == spec.kind, gate == nil,
                  raw["label"] is String, raw["tooltip"] is String,
                  Set(raw.keys) == keys, range == spec.range,
                  raw["unit"] as? String == spec.unit,
                  let value = defaultLiteral(raw["default"], kind: kind),
                  value == spec.defaultValue else { reject(); return result }
            result.defaultValue = value
            return result
        }

        if category == "display", let spec = displayPeakSpecs[id] {
            // V11's Spectrum Peaks rows: phone-owned, each gated on the
            // display extras version that honors it, with exact binding,
            // kind, apply mode, range, unit and default.
            var keys: Set<String> = ["id", "label", "tooltip", "kind", "binding", "applies",
                                     "requiresDescriptionVersion", "gate", "default"]
            if spec.range != nil { keys.formUnion(["min", "max", "step"]) }
            if spec.unit != nil { keys.insert("unit") }
            guard version >= 11, required == 11, applies == spec.applies,
                  binding == .phone(spec.phone), kind == spec.kind,
                  exactGate(gate, "displayExtrasVersion", spec.minimum),
                  raw["label"] is String, raw["tooltip"] is String,
                  Set(raw.keys) == keys, range == spec.range,
                  raw["unit"] as? String == spec.unit,
                  let value = defaultLiteral(raw["default"], kind: kind),
                  value == spec.defaultValue else { reject(); return result }
            result.defaultValue = value
            return result
        }

        if category == "appearance", id.hasPrefix("appearance.meterStyles.") {
            guard version >= 7, required == 7, applies == .live, gate == nil,
                  let spec = meterSpecs[id], kind == spec.kind, binding == .phone(spec.phone),
                  raw["label"] as? String == spec.label,
                  raw["tooltip"] as? String == spec.tooltip else { reject(); return result }
            if let labels = spec.options {
                guard Set(raw.keys) == ["id", "label", "tooltip", "kind", "binding", "applies",
                                        "requiresDescriptionVersion", "options", "default"],
                      int(raw["default"]) == spec.defaultValue,
                      let options = numberedOptions(raw["options"], labels) else { reject(); return result }
                result.options = options
                result.defaultValue = .integer(spec.defaultValue)
            } else {
                guard Set(raw.keys) == ["id", "label", "tooltip", "kind", "binding", "applies",
                                        "requiresDescriptionVersion", "default"],
                      bool(raw["default"]) == true else { reject(); return result }
                result.defaultValue = .bool(true)
            }
            return result
        }

        if category == "appearance", id.hasPrefix("appearance.colorsTheme.") {
            guard kind == .colour, applies == .live, gate == nil, required == nil,
                  let spec = colourSpecs[id], binding == .phone(spec.0) else { reject(); return result }
            if version >= 4 {
                guard Set(raw.keys) == ["id", "label", "tooltip", "kind", "binding", "applies", "default"],
                      let colour = raw["default"] as? String, colour == spec.1,
                      isRGBA(colour) else { reject(); return result }
                result.defaultValue = .text(colour)
            } else if Set(raw.keys) != ["id", "label", "tooltip", "kind", "binding", "applies"] { reject() }
            return result
        }

        if category == "display", let expected = displayDefaults[id] {
            guard let spec = displaySettingSpecs[id], binding == .setting(spec.key),
                  kind == spec.kind, applies == spec.applies,
                  range == spec.range, choices == spec.choices,
                  raw["unit"] as? String == spec.unit,
                  (spec.tx ? exactGate(gate, "txDisplayVersion", 2) : gate == nil),
                  (spec.v4Only ? required == 4 : required == nil),
                  Set(raw.keys) == displayKeys(id: id, spec: spec, version: version) else { reject(); return result }
            if version >= 4 {
                guard let defaultValue = defaultLiteral(raw["default"], kind: kind),
                      defaultValue == expected, raw["actions"] == nil, raw["rows"] == nil,
                      raw["columnGroups"] == nil else { reject(); return result }
                result.defaultValue = defaultValue
                if id == "display.spectrumDefaults.fftSize" || id == "display.txDisplay.fftSize" {
                    let labels = (0..<7).map { String(4096 << $0) }
                    guard required == 4, kind == .slider, range == nil,
                          let options = fftOptions(raw["options"], labels),
                          raw["enabledWhen"] == nil else { reject(); return result }
                    result.options = options
                } else {
                    if raw["options"] != nil { reject() }
                    if id == "display.txDisplay.panNormalize" {
                        guard required == 4, kind == .toggle,
                              let dependency = normalizeDependency(raw["enabledWhen"]) else { reject(); return result }
                        result.dependency = dependency
                    } else if raw["enabledWhen"] != nil { reject() }
                }
                if id == "display.spectrumDefaults.hzPerBinTarget" {
                    if decimals != 2 { reject() }
                } else if kind != .readout && decimals != nil { reject() }
            } else if extended || (kind == .decimal && decimals != nil) { reject() }
            return result
        }

        if extended { reject() }
        if kind == .settingsHygiene { reject() }
        if required == 3 && category != "diagnostics" { reject() }
        if (4...highestVersion).contains(required ?? 0) { reject() }
        if raw["perBand"] != nil || raw["confirm"] != nil { reject() }
        return result
    }

    static func exactGate(_ gate: Gate?, _ capability: String, _ minimum: Int64,
                                  offAir: Bool = false) -> Bool {
        gate?.capability == capability && gate?.minimum == minimum && gate?.transmit == nil
            && gate?.board == nil && gate?.offAir == (offAir ? true : nil)
    }
    static func int(_ value: Any?) -> Int64? {
        guard let number = value as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID(),
              number.doubleValue.isFinite, number.doubleValue.rounded() == number.doubleValue,
              number.doubleValue >= Double(Int64.min), number.doubleValue < Double(Int64.max) else { return nil }
        return number.int64Value
    }
    static func bool(_ value: Any?) -> Bool? {
        guard let number = value as? NSNumber, CFGetTypeID(number) == CFBooleanGetTypeID() else { return nil }
        return number.boolValue
    }
    static func defaultLiteral(_ value: Any?, kind: Kind?) -> Literal? {
        if kind == .toggle, let value = bool(value) { return .bool(value) }
        if kind == .colour, let value = value as? String, isRGBA(value) { return .text(value) }
        if let value = int(value) { return .integer(value) }
        if kind == .decimal, let number = value as? NSNumber,
           CFGetTypeID(number) != CFBooleanGetTypeID(), number.doubleValue.isFinite {
            return .decimal(number.doubleValue)
        }
        return nil
    }
    private static func isRGBA(_ value: String) -> Bool {
        value.utf8.count == 9 && value.first == "#" && value.utf8.dropFirst().allSatisfy {
            (48...57).contains($0) || (65...70).contains($0)
        }
    }
    private static func numberedOptions(_ value: Any?, _ labels: [String]) -> [Option]? {
        guard let raw = value as? [[String: Any]], raw.count == labels.count else { return nil }
        var options: [Option] = []
        for (index, item) in raw.enumerated() {
            guard Set(item.keys) == ["value", "label"], int(item["value"]) == Int64(index),
                  item["label"] as? String == labels[index] else { return nil }
            options.append(Option(value: Int64(index), label: labels[index]))
        }
        return options
    }
    private static func fftOptions(_ value: Any?, _ labels: [String]) -> [Option]? {
        guard let raw = value as? [[String: Any]], raw.count == 7 else { return nil }
        var options: [Option] = []
        for (index, item) in raw.enumerated() {
            let number = Int64(4096 << index)
            guard Set(item.keys) == ["value", "label"], int(item["value"]) == number,
                  item["label"] as? String == labels[index] else { return nil }
            options.append(Option(value: number, label: labels[index]))
        }
        return options
    }
    private static func normalizeDependency(_ value: Any?) -> Dependency? {
        guard let raw = value as? [String: Any], Set(raw.keys) == ["setting", "oneOf"],
              raw["setting"] as? String == "DisplayTxPanDetector",
              raw["oneOf"] as? [String] == ["2", "3", "4"] else { return nil }
        return Dependency(setting: "DisplayTxPanDetector", oneOf: ["2", "3", "4"])
    }
    /// The settings check's actions: Re-validate, then Repair (a
    /// settingsHygieneVersion 2 Core's description) or a greyed Reset (a
    /// version 1 Core's), then Forget, each exactly as the Core words it.
    private static func hygieneActions(_ value: Any?) -> [HygieneAction]? {
        guard let raw = value as? [[String: Any]], raw.count == 3,
              Set(raw[0].keys) == ["id", "label"],
              raw[0]["id"] as? String == "validate", raw[0]["label"] as? String == "Re-validate",
              let middle = hygieneRepair(raw[1]) ?? hygieneReset(raw[1]),
              Set(raw[2].keys) == ["id", "label", "paired", "offAir", "confirmation"],
              raw[2]["id"] as? String == "forget", raw[2]["label"] as? String == "Forget This Radio",
              bool(raw[2]["paired"]) == true, bool(raw[2]["offAir"]) == true,
              let confirmation = raw[2]["confirmation"] as? [String: Any],
              Set(confirmation.keys) == ["title", "message", "default"],
              confirmation["title"] as? String == "Forget Radio",
              confirmation["message"] as? String == "Forget all settings for this radio?",
              confirmation["default"] as? String == "cancel" else { return nil }
        return [HygieneAction(id: "validate", label: "Re-validate", enabled: true, reason: nil,
                              paired: false, offAir: false, confirmation: nil, minimumVersion: nil),
                middle,
                HygieneAction(id: "forget", label: "Forget This Radio", enabled: true, reason: nil,
                              paired: true, offAir: true,
                              confirmation: Confirmation(title: "Forget Radio",
                                                         message: "Forget all settings for this radio?",
                                                         defaultAction: "cancel"),
                              minimumVersion: nil)]
    }
    private static func hygieneRepair(_ raw: [String: Any]) -> HygieneAction? {
        guard Set(raw.keys) == ["id", "label", "paired", "offAir", "gate", "reason", "confirmation"],
              raw["id"] as? String == "repair", raw["label"] as? String == "Repair Invalid Settings",
              bool(raw["paired"]) == true, bool(raw["offAir"]) == true,
              let gate = raw["gate"] as? [String: Any], Set(gate.keys) == ["capability", "min"],
              gate["capability"] as? String == "settingsHygieneVersion", int(gate["min"]) == 2,
              raw["reason"] as? String
                == "Repair invalid settings is not available on this Core. Updating the Core may help.",
              let confirmation = raw["confirmation"] as? [String: Any],
              Set(confirmation.keys) == ["title", "message", "default"],
              confirmation["title"] as? String == "Repair Settings",
              confirmation["message"] as? String == "Repair the settings that are invalid for this radio?",
              confirmation["default"] as? String == "cancel" else { return nil }
        return HygieneAction(id: "repair", label: "Repair Invalid Settings", enabled: true,
                             reason: "Repair invalid settings is not available on this Core. Updating the Core may help.",
                             paired: true, offAir: true,
                             confirmation: Confirmation(title: "Repair Settings",
                                                        message: "Repair the settings that are invalid for this radio?",
                                                        defaultAction: "cancel"),
                             minimumVersion: 2)
    }
    private static func hygieneReset(_ raw: [String: Any]) -> HygieneAction? {
        guard Set(raw.keys) == ["id", "label", "enabled", "reason"],
              raw["id"] as? String == "reset", raw["label"] as? String == "Reset to Defaults",
              bool(raw["enabled"]) == false,
              raw["reason"] as? String == "Reset to defaults is not available on this Core." else { return nil }
        return HygieneAction(id: "reset", label: "Reset to Defaults", enabled: false,
                             reason: "Reset to defaults is not available on this Core.",
                             paired: false, offAir: false, confirmation: nil, minimumVersion: nil)
    }

    private static func antennaRows(_ raw: [String: Any], source: AntennaRows) -> AntennaRows? {
        let tx = source.mode == "tx"
        let base: Set<String> = ["id", "label", "tooltip", "kind", "binding", "applies",
                                 "requiresDescriptionVersion", "gate", "rows", "columns"]
        guard Set(raw.keys) == base.union(tx ? [] : ["columnGroups"]),
              raw["label"] as? String == (tx ? "TX Antenna per Band" : "RX1 / RX2 Antenna per Band"),
              raw["tooltip"] as? String == "",
              let rawColumns = raw["columns"] as? [[String: Any]],
              rawColumns.count == (tx ? 3 : 6) else { return nil }
        var columns: [AntennaColumn] = []
        for (index, item) in rawColumns.enumerated() {
            let antenna = index % 3 + 1
            let field = tx ? "tx" : index < 3 ? "rx" : "rxOnly"
            let id = field + String(antenna)
            let label: String
            if tx { label = "Ant \(antenna)" }
            else if index < 3 { label = String(antenna) }
            else {
                guard let value = item["label"] as? String else { return nil }
                label = value
            }
            guard Set(item.keys) == ["id", "field", "antenna", "label"],
                  item["id"] as? String == id, item["field"] as? String == field,
                  int(item["antenna"]) == Int64(antenna), item["label"] as? String == label else { return nil }
            columns.append(AntennaColumn(id: id, field: field, antenna: antenna, label: label))
        }
        if !tx {
            let labels = Array(columns[3...5].map(\.label))
            guard [["RX1", "RX2", "XVTR"], ["EXT2", "EXT1", "XVTR"],
                   ["BYPS", "EXT1", "XVTR"]].contains(labels) else { return nil }
        }
        var groups: [AntennaColumnGroup] = []
        if !tx {
            guard let rawGroups = raw["columnGroups"] as? [[String: Any]], rawGroups.count == 2 else { return nil }
            let expected = [("RX1", ["rx1", "rx2", "rx3"]),
                            ("RX-only", ["rxOnly1", "rxOnly2", "rxOnly3"])]
            for (item, pair) in zip(rawGroups, expected) {
                guard Set(item.keys) == ["label", "columns"], item["label"] as? String == pair.0,
                      item["columns"] as? [String] == pair.1 else { return nil }
                groups.append(AntennaColumnGroup(label: pair.0, columns: pair.1))
            }
        }
        // One row per entry of the Core's antenna lists, in their order:
        // 160m to XVTR as their band numbers, then 2 m (band 27) for a
        // phone that declares band2m (setup description, "Each table has
        // one row per antenna-list entry"). A row carries its band number.
        var bands = bandNames.enumerated().map { (band: $0.offset, name: $0.element) }
        bands.append((band: band2m, name: band2mName))
        guard let rawRows = raw["rows"] as? [[String: Any]],
              rawRows.count == bandNames.count || rawRows.count == bands.count else { return nil }
        var rows: [AntennaRow] = []
        for (item, entry) in zip(rawRows, bands) {
            guard Set(item.keys) == ["band", "label", "cells"], int(item["band"]) == Int64(entry.band),
                  item["label"] as? String == entry.name,
                  let rawCells = item["cells"] as? [[String: Any]], rawCells.count == columns.count else { return nil }
            var cells: [AntennaCell] = []
            for (cell, column) in zip(rawCells, columns) {
                let tooltip = column.field == "tx" ? "TX Ant \(column.antenna) for \(entry.name)"
                    : column.field == "rx" ? "RX1 Ant \(column.antenna) for \(entry.name)"
                    : "RX-only \(column.label) for \(entry.name)"
                guard Set(cell.keys) == ["column", "tooltip"],
                      cell["column"] as? String == column.id,
                      cell["tooltip"] as? String == tooltip else { return nil }
                cells.append(AntennaCell(column: column.id, tooltip: tooltip))
            }
            rows.append(AntennaRow(band: entry.band, label: entry.name, cells: cells))
        }
        return AntennaRows(object: source.object, mode: source.mode, rows: rows,
                           columns: columns, columnGroups: groups)
    }

    private struct ChoiceSpec {
        let phone: String
        let kind: Kind
        let capability: String
        let minimum: Int64
        let defaultValue: Int64
        let options: [String]?
        let range: Range?
        let unit: String?
    }
    private static let spectrumDetectors = ["Peak", "Rosenfell", "Average", "Sample", "RMS"]
    private static let waterfallDetectors = ["Peak", "Rosenfell", "Average", "Sample"]
    private static let averagingModes = ["None", "Recursive", "Time Window", "Log Recursive"]
    private static let displayPhoneSpecs: [String: ChoiceSpec] = [
        "display.spectrumDefaults.detector": .init(phone: "DisplaySpectrumDetector", kind: .choice,
            capability: "remoteMediaVersion", minimum: 1, defaultValue: 0,
            options: spectrumDetectors, range: nil, unit: nil),
        "display.spectrumDefaults.averaging": .init(phone: "DisplaySpectrumAveraging", kind: .choice,
            capability: "displayExtrasVersion", minimum: 1, defaultValue: 3,
            options: averagingModes, range: nil, unit: nil),
        "display.spectrumDefaults.averageTime": .init(phone: "DisplaySpectrumAverageTimeMs", kind: .integer,
            capability: "displayExtrasVersion", minimum: 1, defaultValue: 30,
            options: nil, range: Range(minimum: 10, maximum: 9999, step: 10), unit: "ms"),
        "display.spectrumDefaults.decimation": .init(phone: "decimation", kind: .integer,
            capability: "spectrumGrantVersion", minimum: 2, defaultValue: 1,
            options: nil, range: Range(minimum: 1, maximum: 16, step: 1), unit: nil),
        "display.waterfallDefaults.detector": .init(phone: "DisplayWaterfallDetector", kind: .choice,
            capability: "remoteMediaVersion", minimum: 1, defaultValue: 0,
            options: waterfallDetectors, range: nil, unit: nil),
        "display.waterfallDefaults.averaging": .init(phone: "DisplayWaterfallAveraging", kind: .choice,
            capability: "displayExtrasVersion", minimum: 1, defaultValue: 0,
            options: averagingModes, range: nil, unit: nil),
        "display.waterfallDefaults.averageTime": .init(phone: "DisplayWaterfallAverageTimeMs", kind: .integer,
            capability: "displayExtrasVersion", minimum: 1, defaultValue: 120,
            options: nil, range: Range(minimum: 10, maximum: 9999, step: 10), unit: "ms"),
    ]
    private struct RenderSpec {
        let phone: String
        let kind: Kind
        let applies: Applies
        let version: Int
        let defaultValue: Literal
        let range: Range?
        let unit: String?
    }
    private static func toggleSpec(_ phone: String, _ version: Int, _ on: Bool) -> RenderSpec {
        RenderSpec(phone: phone, kind: .toggle, applies: .live, version: version,
                   defaultValue: .bool(on), range: nil, unit: nil)
    }
    /// V9's eight rendering rows and V10's four overlays, by control ID.
    private static let displayRenderSpecs: [String: RenderSpec] = [
        "display.spectrumDefaults.panFill": toggleSpec("DisplayPanFill", 9, true),
        "display.spectrumDefaults.fillAlpha": .init(phone: "DisplayFftFillAlpha", kind: .slider, applies: .live,
            version: 9, defaultValue: .integer(70), range: Range(minimum: 0, maximum: 100, step: 1), unit: "%"),
        "display.spectrumDefaults.gradient": toggleSpec("DisplayGradientEnabled", 9, false),
        "display.spectrumDefaults.peakHold": toggleSpec("DisplayPeakHoldEnabled", 9, false),
        "display.spectrumDefaults.peakDelay": .init(phone: "DisplayPeakHoldResetMs", kind: .integer, applies: .live,
            version: 9, defaultValue: .integer(2000), range: Range(minimum: 100, maximum: 10000, step: 100), unit: "ms"),
        "display.waterfallDefaults.updatePeriod": .init(phone: "DisplayWfUpdatePeriodMs", kind: .slider,
            applies: .subscription, version: 9, defaultValue: .integer(30),
            range: Range(minimum: 10, maximum: 500, step: 1), unit: "ms"),
        "display.waterfallDefaults.stopOnTx": toggleSpec("WaterfallStopOnTx", 9, false),
        "display.waterfallDefaults.opacity": .init(phone: "DisplayWfOpacity", kind: .slider, applies: .live,
            version: 9, defaultValue: .integer(100), range: Range(minimum: 0, maximum: 100, step: 1), unit: "%"),
        "display.waterfallDefaults.showRxFilter": toggleSpec("DisplayShowRxFilterOnWaterfall", 10, false),
        // The description's default is metadata; BandDisplaySettings keeps
        // the phone's own default, which matches it.
        "display.waterfallDefaults.showTxFilter": toggleSpec("DisplayShowTxFilterOnRxWaterfall", 10, true),
        "display.waterfallDefaults.showRxZeroLine": toggleSpec("DisplayShowRxZeroLine", 10, false),
        "display.waterfallDefaults.showTxZeroLine": toggleSpec("DisplayShowTxZeroLine", 10, false),
    ]
    private struct PeakSpec {
        let phone: String
        let kind: Kind
        let applies: Applies
        let minimum: Int64
        let defaultValue: Literal
        let range: Range?
        let unit: String?
    }
    private static func peakSwitch(_ phone: String, _ applies: Applies, minimum: Int64 = 1) -> PeakSpec {
        PeakSpec(phone: phone, kind: .toggle, applies: applies, minimum: minimum,
                 defaultValue: .bool(false), range: nil, unit: nil)
    }
    private static func peakColour(_ phone: String, _ colour: String) -> PeakSpec {
        PeakSpec(phone: phone, kind: .colour, applies: .live, minimum: 1,
                 defaultValue: .text(colour), range: nil, unit: nil)
    }
    /// V11's fifteen Spectrum Peaks rows, by control ID: the eleven that
    /// feed the display extras subscription, then the fill and the three
    /// colours the phone draws with. Hold duration and Update during TX
    /// need `displayExtrasVersion` 3; the rest 1.
    private static let displayPeakSpecs: [String: PeakSpec] = [
        "display.spectrumPeaks.activePeakHold": peakSwitch("DisplayActivePeakHoldEnabled", .subscription),
        "display.spectrumPeaks.activePeakHoldTime": .init(phone: "DisplayActivePeakHoldDurationMs", kind: .integer,
            applies: .subscription, minimum: 3, defaultValue: .integer(2000),
            range: Range(minimum: 100, maximum: 60000, step: 100), unit: "ms"),
        "display.spectrumPeaks.activePeakHoldDropRate": .init(phone: "DisplayActivePeakHoldDropDbPerSec",
            kind: .integer, applies: .subscription, minimum: 1, defaultValue: .integer(6),
            range: Range(minimum: 1, maximum: 60, step: 1), unit: "dB/s"),
        "display.spectrumPeaks.activePeakHoldFill": peakSwitch("DisplayActivePeakHoldFill", .live),
        "display.spectrumPeaks.activePeakHoldOnTx": peakSwitch("DisplayActivePeakHoldOnTx", .subscription, minimum: 3),
        "display.spectrumPeaks.activePeakHoldColor": peakColour("DisplayActivePeakHoldColor", "#FFD700FF"),
        "display.spectrumPeaks.peakBlobs": peakSwitch("DisplayPeakBlobsEnabled", .subscription),
        "display.spectrumPeaks.peakBlobCount": .init(phone: "DisplayPeakBlobsCount", kind: .integer,
            applies: .subscription, minimum: 1, defaultValue: .integer(3),
            range: Range(minimum: 1, maximum: 20, step: 1), unit: nil),
        "display.spectrumPeaks.peakBlobInsideFilter": peakSwitch("DisplayPeakBlobsInsideFilterOnly", .subscription),
        "display.spectrumPeaks.peakBlobHold": peakSwitch("DisplayPeakBlobsHoldEnabled", .subscription),
        "display.spectrumPeaks.peakBlobHoldTime": .init(phone: "DisplayPeakBlobsHoldMs", kind: .integer,
            applies: .subscription, minimum: 1, defaultValue: .integer(500),
            range: Range(minimum: 100, maximum: 60000, step: 100), unit: "ms"),
        "display.spectrumPeaks.peakBlobHoldDrop": peakSwitch("DisplayPeakBlobsHoldDrop", .subscription),
        "display.spectrumPeaks.peakBlobFallRate": .init(phone: "DisplayPeakBlobsFallDbPerSec", kind: .integer,
            applies: .subscription, minimum: 1, defaultValue: .integer(6),
            range: Range(minimum: 1, maximum: 60, step: 1), unit: "dB/s"),
        "display.spectrumPeaks.peakBlobColor": peakColour("DisplayPeakBlobColor", "#FF4500FF"),
        "display.spectrumPeaks.peakBlobTextColor": peakColour("DisplayPeakBlobTextColor", "#7FFF00FF"),
    ]
    private struct MeterSpec {
        let phone: String
        let kind: Kind
        let label: String
        let tooltip: String
        let defaultValue: Int64
        let options: [String]?
    }
    private static let meterSpecs: [String: MeterSpec] = [
        "appearance.meterStyles.face": .init(phone: "SMeter_FaceStyle", kind: .choice,
            label: "Face:", tooltip: "The S-meter's face", defaultValue: 0,
            options: ["Aged Cream", "VU Amber", "Collins White", "Blackface", "Carbon", "Ice", "Classic (flat)"]),
        "appearance.meterStyles.peakHold": .init(phone: "PeakHoldEnabled", kind: .toggle,
            label: "Peak hold", tooltip: "Hold the S-meter's peak reading", defaultValue: 1, options: nil),
        "appearance.meterStyles.peakDecay": .init(phone: "PeakDecayRate", kind: .choice,
            label: "Decay Rate:", tooltip: "How fast the held peak falls back", defaultValue: 1,
            options: ["Fast (20 dB/s)", "Medium (10 dB/s)", "Slow (5 dB/s)"]),
    ]
    private static let colourSpecs: [String: (String, String)] = [
        "appearance.colorsTheme.traceFillColor": ("DisplayFillColor", "#00E5FFFF"),
        "appearance.colorsTheme.gridColor": ("DisplayGridColor", "#FFFFFF28"),
        "appearance.colorsTheme.gridFineColor": ("DisplayGridFineColor", "#FFFFFF14"),
        "appearance.colorsTheme.hGridColor": ("DisplayHGridColor", "#FFFFFF28"),
        "appearance.colorsTheme.gridTextColor": ("DisplayGridTextColor", "#FFFF00FF"),
        "appearance.colorsTheme.bandEdgeColor": ("DisplayBandEdgeColor", "#FF0000FF"),
        "appearance.colorsTheme.rxZeroLineColor": ("DisplayRxZeroLineColor", "#FF0000FF"),
        "appearance.colorsTheme.txZeroLineColor": ("DisplayTxZeroLineColor", "#FFB800FF"),
        "appearance.colorsTheme.rxFilterColor": ("DisplayRxFilterColor", "#00B4D850"),
        "appearance.colorsTheme.txFilterColor": ("DisplayTxFilterColor", "#FF783C2E"),
    ]
    private static let displayDefaults: [String: Literal] = [
        "display.spectrumDefaults.fftSize": .integer(4096),
        "display.spectrumDefaults.window": .integer(1),
        "display.spectrumDefaults.hzPerBinTarget": .integer(0),
        "display.spectrumDefaults.fps": .integer(30),
        "display.multimeter.pollingDelay": .integer(100),
        "display.txDisplay.fftSize": .integer(32768),
        "display.txDisplay.window": .integer(4),
        "display.txDisplay.panDetector": .integer(0),
        "display.txDisplay.panAveraging": .integer(0),
        "display.txDisplay.panAvTime": .integer(30),
        "display.txDisplay.panNormalize": .bool(false),
        "display.txDisplay.wfDetector": .integer(0),
        "display.txDisplay.wfAveraging": .integer(0),
        "display.txDisplay.wfAvTime": .integer(120),
    ]
    private struct DisplaySettingSpec {
        let key: String
        let kind: Kind
        let applies: Applies
        let range: Range?
        let choices: [String]?
        let unit: String?
        let tx: Bool
        let v4Only: Bool
    }
    private static let fftWindows = ["Rectangular", "Blackman-Harris 4T", "Hann",
                                     "Flat-Top", "Hamming", "Kaiser", "Blackman-Harris 7T"]
    private static let displaySettingSpecs: [String: DisplaySettingSpec] = [
        "display.spectrumDefaults.fftSize": .init(key: "DisplayFftSize", kind: .slider,
            applies: .subscription, range: nil, choices: nil, unit: nil, tx: false, v4Only: true),
        "display.spectrumDefaults.window": .init(key: "DisplayFftWindow", kind: .choice,
            applies: .subscription, range: nil, choices: fftWindows, unit: nil, tx: false, v4Only: false),
        "display.spectrumDefaults.hzPerBinTarget": .init(key: "DisplayHzPerBinTarget", kind: .decimal,
            applies: .subscription, range: Range(minimum: 0, maximum: 200, step: 0.5),
            choices: nil, unit: "Hz/bin", tx: false, v4Only: false),
        "display.spectrumDefaults.fps": .init(key: "DisplaySpectrumFps", kind: .slider,
            applies: .subscription, range: Range(minimum: 10, maximum: 60, step: 1),
            choices: nil, unit: "fps", tx: false, v4Only: false),
        "display.multimeter.pollingDelay": .init(key: "MultimeterDelayMs", kind: .integer,
            applies: .live, range: Range(minimum: 10, maximum: 2000, step: 1),
            choices: nil, unit: "ms", tx: false, v4Only: false),
        "display.txDisplay.fftSize": .init(key: "DisplayTxFftSize", kind: .slider,
            applies: .live, range: nil, choices: nil, unit: nil, tx: true, v4Only: true),
        "display.txDisplay.window": .init(key: "DisplayTxWindowType", kind: .choice,
            applies: .live, range: nil, choices: fftWindows, unit: nil, tx: true, v4Only: false),
        "display.txDisplay.panDetector": .init(key: "DisplayTxPanDetector", kind: .choice,
            applies: .live, range: nil, choices: spectrumDetectors, unit: nil, tx: true, v4Only: false),
        "display.txDisplay.panAveraging": .init(key: "DisplayTxPanAveraging", kind: .choice,
            applies: .live, range: nil, choices: averagingModes, unit: nil, tx: true, v4Only: false),
        "display.txDisplay.panAvTime": .init(key: "DisplayTxPanAvTimeMs", kind: .integer,
            applies: .live, range: Range(minimum: 1, maximum: 9999, step: 1),
            choices: nil, unit: "ms", tx: true, v4Only: false),
        "display.txDisplay.panNormalize": .init(key: "DisplayTxPanNormalize", kind: .toggle,
            applies: .live, range: nil, choices: nil, unit: nil, tx: true, v4Only: true),
        "display.txDisplay.wfDetector": .init(key: "DisplayTxWfDetector", kind: .choice,
            applies: .live, range: nil, choices: waterfallDetectors, unit: nil, tx: true, v4Only: false),
        "display.txDisplay.wfAveraging": .init(key: "DisplayTxWfAveraging", kind: .choice,
            applies: .live, range: nil, choices: averagingModes, unit: nil, tx: true, v4Only: false),
        "display.txDisplay.wfAvTime": .init(key: "DisplayTxWfAvTimeMs", kind: .integer,
            applies: .live, range: Range(minimum: 1, maximum: 9999, step: 1),
            choices: nil, unit: "ms", tx: true, v4Only: false),
    ]
    private static func displayKeys(id: String, spec: DisplaySettingSpec, version: Int) -> Set<String> {
        var keys: Set<String> = ["id", "label", "tooltip", "kind", "binding", "applies"]
        if spec.tx { keys.insert("gate") }
        if spec.v4Only { keys.insert("requiresDescriptionVersion") }
        if spec.range != nil { keys.formUnion(["min", "max", "step"]) }
        if spec.choices != nil { keys.insert("choices") }
        if spec.unit != nil { keys.insert("unit") }
        if id == "display.txDisplay.panNormalize" { keys.insert("valueEncoding") }
        if version >= 4 {
            keys.insert("default")
            if id == "display.spectrumDefaults.fftSize" || id == "display.txDisplay.fftSize" {
                keys.insert("options")
            }
            if id == "display.txDisplay.panNormalize" { keys.insert("enabledWhen") }
            if id == "display.spectrumDefaults.hzPerBinTarget" { keys.insert("decimals") }
        }
        return keys
    }
}
