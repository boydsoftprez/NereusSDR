// NereusSDR for iOS: the closed rows of Setup description V12, the rest of Setup > Display and Reset all colors
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

extension SetupDescription {
    /// One V12 row as the Core publishes it (`display-v12-for-phone.md`,
    /// `resources/setup/display.json` and `appearance.json`): its phone key
    /// or action, kind, apply mode, gate, default, range, unit, places,
    /// options, dependency, per-band label and whether it asks first.
    struct V12Spec {
        let phone: String
        let kind: Kind
        let applies: Applies
        var gate: (capability: String, minimum: Int64)?
        var defaultValue: Literal?
        var range: Range?
        var unit: String?
        var decimals: Int?
        var options: [(value: Int64, label: String)]?
        var dependency: (phone: String, oneOf: [Literal])?
        var perBand: String?
        var confirm = false

        init(phone: String, kind: Kind, applies: Applies, gate: (String, Int64)? = nil,
             defaultValue: Literal? = nil, range: Range? = nil, unit: String? = nil, decimals: Int? = nil,
             options: [(Int64, String)]? = nil, dependency: (String, [Literal])? = nil,
             perBand: String? = nil, confirm: Bool = false) {
            self.phone = phone
            self.kind = kind
            self.applies = applies
            self.gate = gate.map { (capability: $0.0, minimum: $0.1) }
            self.defaultValue = defaultValue
            self.range = range
            self.unit = unit
            self.decimals = decimals
            self.options = options?.map { (value: $0.0, label: $0.1) }
            self.dependency = dependency.map { (phone: $0.0, oneOf: $0.1) }
            self.perBand = perBand
            self.confirm = confirm
        }
    }

    /// Checks a V12 row against its spec; nil when it differs in any way.
    static func checkV12(_ raw: [String: Any], spec: V12Spec, id: String, category: String, version: Int,
                         kind: Kind?, binding: Binding?, applies: Applies?, gate: Gate?,
                         range: Range?, decimals: Int?, required: Int?) -> ExtensionMetadata? {
        guard category == (id.hasPrefix("appearance.") ? "appearance" : "display"),
              version >= 12, required == 12, kind == spec.kind, binding == .phone(spec.phone),
              applies == spec.applies, raw["label"] is String, raw["tooltip"] is String,
              range == spec.range, raw["unit"] as? String == spec.unit, decimals == spec.decimals else {
            return nil
        }
        if let expected = spec.gate {
            guard exactGate(gate, expected.capability, expected.minimum) else { return nil }
        } else if gate != nil {
            return nil
        }
        var keys: Set<String> = ["id", "label", "tooltip", "kind", "binding", "applies", "requiresDescriptionVersion"]
        if spec.gate != nil { keys.insert("gate") }
        if spec.defaultValue != nil { keys.insert("default") }
        if spec.range != nil { keys.formUnion(["min", "max", "step"]) }
        if spec.unit != nil { keys.insert("unit") }
        if spec.decimals != nil { keys.insert("decimals") }
        if spec.options != nil { keys.insert("options") }
        if spec.dependency != nil { keys.insert("enabledWhen") }
        if spec.perBand != nil { keys.insert("perBand") }
        if spec.confirm { keys.insert("confirm") }
        guard Set(raw.keys) == keys else { return nil }
        var result = ExtensionMetadata()
        if let expected = spec.defaultValue {
            guard let value = defaultLiteral(raw["default"], kind: kind), value == expected else { return nil }
            result.defaultValue = value
        }
        if let expected = spec.options {
            guard let items = raw["options"] as? [[String: Any]], items.count == expected.count else { return nil }
            for (item, want) in zip(items, expected) {
                guard Set(item.keys) == ["value", "label"], int(item["value"]) == want.value,
                      item["label"] as? String == want.label else { return nil }
            }
            result.options = expected.map { Option(value: $0.value, label: $0.label) }
        }
        if let expected = spec.dependency {
            guard let object = raw["enabledWhen"] as? [String: Any], Set(object.keys) == ["phone", "oneOf"],
                  object["phone"] as? String == expected.phone,
                  let values = object["oneOf"] as? [Any], values.count == expected.oneOf.count else { return nil }
            for (value, want) in zip(values, expected.oneOf) {
                switch want {
                case .bool(let on):
                    guard bool(value) == on else { return nil }
                case .integer(let whole):
                    guard int(value) == whole else { return nil }
                default:
                    return nil
                }
            }
            result.phoneDependency = PhoneDependency(phone: expected.phone, oneOf: expected.oneOf)
        }
        if let expected = spec.perBand {
            guard let object = raw["perBand"] as? [String: Any], Set(object.keys) == ["label"],
                  object["label"] as? String == expected else { return nil }
            result.perBand = PerBand(label: expected)
        }
        if spec.confirm {
            guard let question = raw["confirm"] as? String, !question.isEmpty else { return nil }
            result.confirm = question
        }
        return result
    }

    /// Every V12 row, by control ID: the 52 new Display rows (the 3D View
    /// and Multimeter ones included, though this phone does not draw them
    /// yet) and Appearance's Reset all colors.
    static let v12Specs: [String: V12Spec] = [
        "display.spectrumDefaults.smoothDefaults": .init(phone: "smoothDefaults", kind: .button, applies: .live, confirm: true),
        "display.spectrumDefaults.clarity": .init(phone: "ClarityEnabled", kind: .toggle, applies: .subscription, gate: ("displayExtrasVersion", 1), defaultValue: .bool(true)),
        "display.spectrumDefaults.showCursorFreq": .init(phone: "DisplayShowCursorFreq", kind: .toggle, applies: .live, defaultValue: .bool(true)),
        "display.spectrumDefaults.showBinWidth": .init(phone: "DisplayShowBinWidth", kind: .toggle, applies: .live, defaultValue: .bool(false)),
        "display.spectrumDefaults.showNoiseFloor": .init(phone: "DisplayShowNoiseFloor", kind: .toggle, applies: .subscription, gate: ("displayExtrasVersion", 1), defaultValue: .bool(false)),
        "display.spectrumDefaults.noiseFloorShift": .init(phone: "DisplayNoiseFloorShiftDb", kind: .decimal, applies: .subscription, gate: ("displayExtrasVersion", 1), defaultValue: .integer(0), range: Range(minimum: -12, maximum: 12, step: 0.5), unit: "dB", decimals: 1),
        "display.spectrumDefaults.noiseFloorLineWidth": .init(phone: "DisplayNoiseFloorLineWidth", kind: .decimal, applies: .live, gate: ("displayExtrasVersion", 1), defaultValue: .integer(1), range: Range(minimum: 1, maximum: 5, step: 0.5), unit: "px", decimals: 1),
        "display.spectrumDefaults.noiseFloorColor": .init(phone: "DisplayNoiseFloorColor", kind: .colour, applies: .live, gate: ("displayExtrasVersion", 1), defaultValue: .text("#FF40FFFF")),
        "display.spectrumDefaults.noiseFloorTextColor": .init(phone: "DisplayNoiseFloorTextColor", kind: .colour, applies: .live, gate: ("displayExtrasVersion", 1), defaultValue: .text("#FFFF00FF")),
        "display.spectrumDefaults.noiseFloorFastColor": .init(phone: "DisplayNoiseFloorFastColor", kind: .colour, applies: .live, gate: ("displayExtrasVersion", 4), defaultValue: .text("#C8C8C8FF")),
        "display.spectrumDefaults.normalize": .init(phone: "DisplayDispNormalize", kind: .toggle, applies: .subscription, gate: ("displayExtrasVersion", 1), defaultValue: .bool(false), dependency: ("DisplaySpectrumDetector", [.integer(2), .integer(3), .integer(4)])),
        "display.spectrumDefaults.showPeakValue": .init(phone: "DisplayShowPeakValueOverlay", kind: .toggle, applies: .live, defaultValue: .bool(false)),
        "display.spectrumDefaults.peakValuePosition": .init(phone: "DisplayPeakValuePosition", kind: .choice, applies: .live, defaultValue: .integer(1), options: [(0, "Top Left"), (1, "Top Right"), (2, "Bottom Left"), (3, "Bottom Right")]),
        "display.spectrumDefaults.peakTextDelay": .init(phone: "DisplayPeakTextDelayMs", kind: .integer, applies: .live, defaultValue: .integer(500), range: Range(minimum: 50, maximum: 10000, step: 50), unit: "ms"),
        "display.spectrumDefaults.getMonitorHz": .init(phone: "getMonitorHz", kind: .button, applies: .live),
        "display.waterfallDefaults.highThreshold": .init(phone: "DisplayWfHighLevel", kind: .slider, applies: .subscription, gate: ("displayExtrasVersion", 1), defaultValue: .integer(-62), range: Range(minimum: -200, maximum: 0, step: 1), unit: "dBm", dependency: ("DisplayWfUseSpectrumMinMax", [.bool(false)])),
        "display.waterfallDefaults.lowThreshold": .init(phone: "DisplayWfLowLevel", kind: .slider, applies: .subscription, gate: ("displayExtrasVersion", 1), defaultValue: .integer(-122), range: Range(minimum: -200, maximum: 0, step: 1), unit: "dBm", dependency: ("DisplayWfUseSpectrumMinMax", [.bool(false)])),
        "display.waterfallDefaults.agc": .init(phone: "DisplayWfAgc", kind: .toggle, applies: .subscription, gate: ("displayExtrasVersion", 1), defaultValue: .bool(true), dependency: ("DisplayWfUseSpectrumMinMax", [.bool(false)])),
        "display.waterfallDefaults.useSpectrumMinMax": .init(phone: "DisplayWfUseSpectrumMinMax", kind: .toggle, applies: .subscription, gate: ("displayExtrasVersion", 1), defaultValue: .bool(false)),
        "display.waterfallDefaults.copySpectrumMinMax": .init(phone: "copySpectrumMinMax", kind: .button, applies: .live),
        "display.waterfallDefaults.nfAgc": .init(phone: "WaterfallNFAGCEnabled", kind: .toggle, applies: .subscription, gate: ("displayExtrasVersion", 1), defaultValue: .bool(false), dependency: ("DisplayWfUseSpectrumMinMax", [.bool(false)])),
        "display.waterfallDefaults.nfAgcOffset": .init(phone: "WaterfallAGCOffsetDb", kind: .integer, applies: .subscription, gate: ("displayExtrasVersion", 1), defaultValue: .integer(0), range: Range(minimum: -60, maximum: 60, step: 1), unit: "dB", dependency: ("DisplayWfUseSpectrumMinMax", [.bool(false)])),
        "display.waterfallDefaults.colorScheme": .init(phone: "DisplayWfColorScheme", kind: .choice, applies: .live, defaultValue: .integer(0), options: [(0, "Default"), (1, "Enhanced"), (2, "Spectran"), (3, "BlackWhite"), (4, "LinLog"), (5, "LinRad"), (6, "Custom"), (7, "Clarity Blue")]),
        "display.waterfallDefaults.historyDepth": .init(phone: "DisplayWaterfallHistoryMs", kind: .choice, applies: .live, defaultValue: .integer(1200000), options: [(60000, "60 seconds"), (300000, "5 minutes"), (900000, "15 minutes"), (1200000, "20 minutes")]),
        "display.waterfallDefaults.timestampPosition": .init(phone: "DisplayWfTimestampPos", kind: .choice, applies: .live, defaultValue: .integer(0), options: [(0, "None"), (1, "Left"), (2, "Right")]),
        "display.waterfallDefaults.timestampMode": .init(phone: "DisplayWfTimestampMode", kind: .choice, applies: .live, defaultValue: .integer(0), options: [(0, "UTC"), (1, "Local")]),
        "display.gridScales.showGrid": .init(phone: "DisplayGridEnabled", kind: .toggle, applies: .live, defaultValue: .bool(true)),
        "display.gridScales.dbmScale": .init(phone: "DisplayDbmScaleVisible", kind: .toggle, applies: .live, defaultValue: .bool(true)),
        "display.gridScales.dbMax": .init(phone: "DisplayGridMax", kind: .integer, applies: .live, defaultValue: .integer(-40), range: Range(minimum: -200, maximum: 0, step: 1), unit: "dB", perBand: "dB Max (%1):"),
        "display.gridScales.dbMin": .init(phone: "DisplayGridMin", kind: .integer, applies: .live, defaultValue: .integer(-140), range: Range(minimum: -200, maximum: 0, step: 1), unit: "dB", perBand: "dB Min (%1):"),
        "display.gridScales.dbStep": .init(phone: "DisplayGridStep", kind: .integer, applies: .live, defaultValue: .integer(10), range: Range(minimum: 1, maximum: 40, step: 1), unit: "dB"),
        "display.gridScales.freqLabelAlign": .init(phone: "DisplayFreqLabelAlign", kind: .choice, applies: .live, defaultValue: .integer(1), options: [(0, "Left"), (1, "Center"), (2, "Right"), (3, "Auto"), (4, "Off")]),
        "display.gridScales.zeroLine": .init(phone: "DisplayShowZeroLine", kind: .toggle, applies: .live, defaultValue: .bool(false)),
        "display.gridScales.showFps": .init(phone: "DisplayShowFps", kind: .toggle, applies: .live, defaultValue: .bool(false)),
        "display.gridScales.adjustGridMinToNoiseFloor": .init(phone: "DisplayAdjustGridMinToNoiseFloor", kind: .toggle, applies: .live, gate: ("displayExtrasVersion", 4), defaultValue: .bool(false)),
        "display.gridScales.noiseFloorOffset": .init(phone: "DisplayNFOffsetGridFollow", kind: .integer, applies: .live, gate: ("displayExtrasVersion", 4), defaultValue: .integer(0), range: Range(minimum: -60, maximum: 60, step: 1), unit: "dB", dependency: ("DisplayAdjustGridMinToNoiseFloor", [.bool(true)])),
        "display.gridScales.maintainGridRange": .init(phone: "DisplayMaintainNFAdjustDelta", kind: .toggle, applies: .live, gate: ("displayExtrasVersion", 4), defaultValue: .bool(false), dependency: ("DisplayAdjustGridMinToNoiseFloor", [.bool(true)])),
        "display.gridScales.copyWaterfallThresholds": .init(phone: "copyWaterfallThresholds", kind: .button, applies: .live),
        "display.multimeter.showDecimal": .init(phone: "MultimeterShowDecimal", kind: .toggle, applies: .live, defaultValue: .bool(true)),
        "display.multimeter.unitMode": .init(phone: "MultimeterUnitMode", kind: .choice, applies: .live, defaultValue: .integer(1), options: [(0, "S"), (1, "dBm"), (2, "uV")]),
        "display.multimeter.historyDuration": .init(phone: "MultimeterSignalHistoryDurationMs", kind: .integer, applies: .live, defaultValue: .integer(60000), range: Range(minimum: 1000, maximum: 600000, step: 1), unit: "ms"),
        "display.txDisplay.wfLowLevel": .init(phone: "DisplayTxWfLowLevel", kind: .integer, applies: .live, gate: ("txDisplayVersion", 1), defaultValue: .integer(-70), range: Range(minimum: -200, maximum: 200, step: 5), unit: "dBm"),
        "display.txDisplay.wfHighLevel": .init(phone: "DisplayTxWfHighLevel", kind: .integer, applies: .live, gate: ("txDisplayVersion", 1), defaultValue: .integer(30), range: Range(minimum: -200, maximum: 200, step: 5), unit: "dBm"),
        "display.txDisplay.wfPalette": .init(phone: "DisplayTxWfPalette", kind: .choice, applies: .live, gate: ("txDisplayVersion", 1), defaultValue: .integer(1), options: [(1, "Enhanced"), (2, "Spectran"), (3, "BlackWhite"), (4, "LinLog"), (5, "LinRad"), (0, "LinAuto"), (6, "Custom")]),
        "display.txDisplay.wfLowColor": .init(phone: "DisplayTxWfLowColor", kind: .colour, applies: .live, gate: ("txDisplayVersion", 1), defaultValue: .text("#000000FF")),
        "display.threeD.reset": .init(phone: "reset3d", kind: .button, applies: .live, confirm: true),
        "display.threeD.renderMode": .init(phone: "DisplaySpectrumRenderMode", kind: .choice, applies: .subscription, gate: ("remoteMediaVersion", 1), defaultValue: .integer(0), options: [(0, "2D Waterfall"), (1, "3D Stacked Trace")]),
        "display.threeD.floor": .init(phone: "Display3DFloorDepth", kind: .slider, applies: .live, defaultValue: .integer(6), range: Range(minimum: 0, maximum: 24, step: 1), unit: "dB", perBand: "3D Floor:"),
        "display.threeD.gain": .init(phone: "Display3DGain", kind: .slider, applies: .live, defaultValue: .integer(70), range: Range(minimum: 0, maximum: 100, step: 1), unit: "%"),
        "display.threeD.span": .init(phone: "Display3DSpan", kind: .slider, applies: .live, defaultValue: .integer(100), range: Range(minimum: 0, maximum: 100, step: 1), unit: "%"),
        "display.threeD.angle": .init(phone: "Display3DAngle", kind: .slider, applies: .live, defaultValue: .integer(50), range: Range(minimum: 0, maximum: 100, step: 1), unit: "%"),
        "display.threeD.sliceShadow": .init(phone: "Display3DSliceShadow", kind: .toggle, applies: .live, defaultValue: .bool(false)),
        "appearance.colorsTheme.resetColors": .init(phone: "resetColors", kind: .button, applies: .live, confirm: true),
    ]
}
