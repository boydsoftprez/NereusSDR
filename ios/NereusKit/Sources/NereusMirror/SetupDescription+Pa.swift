// NereusSDR for iOS: the Core's closed PA profile controls and ordered band editor
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

public extension SetupDescription {
    /// resources/setup/pa.json and SetupDescriptionService.cpp's paV14Controls.
    enum PaProfileAction: String, Equatable, Sendable {
        case active, new, copy, delete, reset
    }

    struct PaPrompt: Equatable, Sendable {
        public let title: String
        public let label: String
        public let defaultValue: String
    }

    struct PaProfileBinding: Equatable, Sendable {
        public let action: PaProfileAction
        public let prompt: PaPrompt?
    }

    struct PaRow: Equatable, Sendable {
        public let band: Int
        public let label: String
        public let availability: Availability?
    }

    struct PaColumn: Equatable, Sendable {
        public let id: String
        public let label: String
        public let field: String
        public let driveStep: Int?
        public let kind: Kind
        public let range: Range?
        public let decimals: Int?
        public let tooltip: String

        public func fits(_ value: Double) -> Bool {
            guard value.isFinite, let range, let decimals,
                  value >= range.minimum, value <= range.maximum else { return false }
            let scaled = value * pow(10, Double(decimals))
            let steps = (value - range.minimum) / range.step
            return abs(scaled - scaled.rounded()) < 0.000001 && abs(steps - steps.rounded()) < 0.000001
        }
    }

    struct PaProfileGrid: Equatable, Sendable {
        public let object: String
        public let rows: [PaRow]
        public let columns: [PaColumn]

        public func column(_ id: String) -> PaColumn? { columns.first { $0.id == id } }
    }
}

extension SetupDescription {
    static func paMetadataValid(_ raw: [String: Any], version: Int, grid: Bool) -> Bool {
        guard version >= 14, integer(raw["requiresDescriptionVersion"]) == 14,
              raw["applies"] as? String == "live", let gate = raw["gate"] as? [String: Any],
              gate["capability"] as? String == "paProfileVersion", integer(gate["min"]) == 1,
              (grid && version >= 20 ? gate["offAir"] == nil : boolean(gate["offAir"]) == true) else { return false }
        if raw["availability"] != nil {
            guard let availability = raw["availability"] as? [String: Any],
                  let enabled = boolean(availability["enabled"]), let reason = availability["reason"] as? String,
                  enabled || !reason.isEmpty else { return false }
        }
        return true
    }

    static func paProfileBinding(_ value: Any?, raw: [String: Any], id: String, kind: Kind?) -> PaProfileBinding? {
        guard let text = value as? String, let action = PaProfileAction(rawValue: text),
              id == "pa.gain." + (action == .active ? "profile" : action.rawValue),
              kind == (action == .active ? .choice : .button) else { return nil }
        var prompt: PaPrompt?
        if action == .new || action == .copy {
            guard let source = raw["prompt"] as? [String: Any],
                  let title = nonempty(source["title"]), let label = nonempty(source["label"]),
                  let defaultValue = source["default"] as? String else { return nil }
            prompt = PaPrompt(title: title, label: label, defaultValue: defaultValue)
        } else if raw["prompt"] != nil { return nil }
        if action == .delete || action == .reset {
            guard nonempty(raw["confirm"]) != nil else { return nil }
        }
        return PaProfileBinding(action: action, prompt: prompt)
    }

    static func paProfileGrid(_ value: Any?, raw: [String: Any], id: String, kind: Kind?, version: Int) -> PaProfileGrid? {
        guard id == "pa.gain.table", kind == .table,
              let binding = value as? [String: Any], binding.count == 1, binding["object"] as? String == "paProfiles",
              let rawRows = raw["rows"] as? [[String: Any]], rawRows.count == 14,
              let rawColumns = raw["columns"] as? [[String: Any]], rawColumns.count == 12 else { return nil }
        var rows: [PaRow] = []
        // PaProfilesFacade.cpp: each body's position is Band 0 through 13.
        for (position, row) in rawRows.enumerated() {
            guard integer(row["band"]) == Int64(position), let label = nonempty(row["label"]),
                  row["holderMayEdit"] == nil else { return nil }
            var availability: Availability?
            if row["availability"] != nil {
                guard version >= 20, let source = row["availability"] as? [String: Any],
                      let enabled = boolean(source["enabled"]), let reason = source["reason"] as? String,
                      enabled || !reason.isEmpty else { return nil }
                availability = Availability(enabled: enabled, reason: reason)
            }
            rows.append(PaRow(band: position, label: label, availability: availability))
        }
        var columns: [PaColumn] = []
        for (position, column) in rawColumns.enumerated() {
            let expectedId = position == 0 ? "gain" : position < 10 ? "adjust\(position)" : position == 10 ? "maxPower" : "useMax"
            let field = position == 0 ? "gain" : position < 10 ? "adjust" : position == 10 ? "maxPower" : "useMax"
            guard column["id"] as? String == expectedId, column["field"] as? String == field,
                  let label = nonempty(column["label"]), let tooltip = column["tooltip"] as? String,
                  let kind = (column["kind"] as? String).flatMap(Kind.init(rawValue:)),
                  kind == (position == 11 ? .toggle : .decimal) else { return nil }
            let driveStep = column["driveStep"].flatMap(integer).map(Int.init)
            if position > 0 && position < 10 {
                guard driveStep == position - 1 else { return nil }
            } else if column["driveStep"] != nil { return nil }
            var range: Range?
            var decimals: Int?
            if kind == .decimal {
                guard let low = number(column["min"]), let high = number(column["max"]),
                      let step = number(column["step"]), low.isFinite, high.isFinite, step.isFinite,
                      low < high, step > 0, step <= high - low,
                      let precision = integer(column["decimals"]), (0...6).contains(precision) else { return nil }
                range = Range(minimum: low, maximum: high, step: step)
                decimals = Int(precision)
            }
            columns.append(PaColumn(id: expectedId, label: label, field: field, driveStep: driveStep,
                                    kind: kind, range: range, decimals: decimals, tooltip: tooltip))
        }
        return PaProfileGrid(object: "paProfiles", rows: rows, columns: columns)
    }
}
