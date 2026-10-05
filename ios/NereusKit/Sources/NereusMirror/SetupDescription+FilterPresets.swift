// NereusSDR for iOS: the Core's exact Filter Presets descriptor
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

public extension SetupDescription {
    enum FilterPresetsAction: String, Equatable, Sendable {
        case table, resetRow, resetMode, resetAll
    }
    struct FilterPresetsColumn: Equatable, Sendable {
        public let id: String
        public let label: String
        public let maxLength: Int?
        public let range: Range?
        public let unit: String?
    }
    struct FilterPresetsBinding: Equatable, Sendable {
        public let action: FilterPresetsAction
        public let columns: [FilterPresetsColumn]
        public func column(_ id: String) -> FilterPresetsColumn? { columns.first { $0.id == id } }
    }
}

extension SetupDescription {
    static func filterPresetsModeValid(_ raw: [String: Any], category: String, version: Int) -> Bool {
        guard category == "dsp", version >= 15, integer(raw["requiresDescriptionVersion"]) == 15,
              Set(raw.keys) == ["id", "label", "tooltip", "kind", "binding", "applies", "requiresDescriptionVersion", "options", "default"],
              raw["label"] as? String == "Mode:", raw["tooltip"] as? String == "", raw["kind"] as? String == "choice",
              raw["applies"] as? String == "live", integer(raw["default"]) == 1,
              let binding = raw["binding"] as? [String: Any], binding.count == 1,
              binding["phone"] as? String == "filterPresetsMode", let options = raw["options"] as? [[String: Any]] else { return false }
        // resources/setup/dsp.json:2370-2429; store resetAll is LSB..DRM.
        let labels = ["LSB", "USB", "DSB", "CWL", "CWU", "FM", "AM", "DIGU", "SPEC", "DIGL", "SAM", "DRM"]
        return options.count == labels.count && zip(options.indices, options).allSatisfy { index, option in
            Set(option.keys) == ["value", "label"] && integer(option["value"]) == Int64(index) && option["label"] as? String == labels[index]
        }
    }
    // Native descriptor validation, based on resources/setup/dsp.json:2435-2539
    // and src/core/setup/SetupDescriptionV15.cpp:262-331. No upstream code port.
    static func filterPresetsBinding(_ value: Any?, raw: [String: Any], id: String,
                                     category: String, version: Int) -> FilterPresetsBinding? {
        guard category == "dsp", version >= 15, integer(raw["requiresDescriptionVersion"]) == 15,
              raw["applies"] as? String == "live", raw["tooltip"] as? String == "",
              let source = value as? [String: Any] else { return nil }
        let action: FilterPresetsAction
        if id == "dsp.filterPresets.presets" {
            guard source.count == 1, source["modeFrom"] as? String == "dsp.filterPresets.mode" else { return nil }
            action = .table
        } else {
            guard source.count == 1, let name = source["action"] as? String,
                  let parsed = FilterPresetsAction(rawValue: name), parsed != .table,
                  id == "dsp.filterPresets." + name else { return nil }
            action = parsed
        }
        let labels: [FilterPresetsAction: String] = [.table: "Presets", .resetRow: "Reset Selected Row",
            .resetMode: "Reset All Rows for This Mode", .resetAll: "Reset Every Mode to Defaults"]
        var keys: Set<String> = ["id", "label", "tooltip", "kind", "applies", "requiresDescriptionVersion", "binding"]
        if action == .table { keys.insert("columns") }
        if action == .resetMode || action == .resetAll { keys.insert("confirm") }
        guard Set(raw.keys) == keys, raw["label"] as? String == labels[action],
              raw["kind"] as? String == (action == .table ? "table" : "button") else { return nil }
        if action == .resetMode {
            guard raw["confirm"] as? String == "Reset all presets for %1 to the defaults?" else { return nil }
        } else if action == .resetAll {
            guard raw["confirm"] as? String == "Reset ALL filter presets for ALL modes to the defaults?\n\nThis cannot be undone." else { return nil }
        }
        var columns: [FilterPresetsColumn] = []
        if action == .table {
            guard let entries = raw["columns"] as? [[String: Any]], entries.count == 6 else { return nil }
            let ids = ["slot", "name", "lowHz", "highHz", "width", "reorder"]
            let titles = ["#", "Name", "Low (Hz)", "High (Hz)", "Width (Hz)", "Reorder"]
            let kinds = ["readout", "text", "integer", "integer", "readout", "reorder"]
            for (index, entry) in entries.enumerated() {
                var expected: Set<String> = ["id", "label", "kind"]
                if index == 1 { expected.insert("maxLength") }
                if index == 2 || index == 3 { expected.formUnion(["min", "max", "step", "unit"]) }
                guard Set(entry.keys) == expected, entry["id"] as? String == ids[index],
                      entry["label"] as? String == titles[index], entry["kind"] as? String == kinds[index] else { return nil }
                if index == 1, integer(entry["maxLength"]) != 32 { return nil }
                let hz = index == 2 || index == 3
                if hz {
                    guard integer(entry["min"]) == -10000, integer(entry["max"]) == 10000,
                          integer(entry["step"]) == 1, entry["unit"] as? String == "Hz" else { return nil }
                }
                columns.append(FilterPresetsColumn(id: ids[index], label: titles[index], maxLength: index == 1 ? 32 : nil,
                    range: hz ? Range(minimum: -10000, maximum: 10000, step: 1) : nil, unit: hz ? "Hz" : nil))
            }
        }
        return FilterPresetsBinding(action: action, columns: columns)
    }
}
