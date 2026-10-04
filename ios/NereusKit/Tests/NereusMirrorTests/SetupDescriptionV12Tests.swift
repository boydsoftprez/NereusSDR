// NereusSDR for iOS: Setup description V12, the rest of Setup > Display and Reset all colors, parsed from the Core's own files
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusMirror

/// V12 (`display-v12-for-phone.md`): 100 Display controls on seven pages
/// (52 new rows) and Appearance's Reset all colors, each a closed object
/// with its phone key or action, and the three new fields `enabledWhen`
/// on a phone key, `perBand` and `confirm`.
@Suite struct SetupDescriptionV12Tests {
    static func resource(_ category: String) throws -> [String: Any] {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/\(category).json").standardizedFileURL
        return try #require(JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
    }

    static func parse(_ root: [String: Any]) throws -> SetupDescription {
        let data = try JSONSerialization.data(withJSONObject: root)
        return try SetupDescription.parse(json: String(decoding: data, as: UTF8.self))
    }

    static func controls(_ description: SetupDescription) -> [SetupDescription.Control] {
        description.pages.flatMap(\.sections).flatMap(\.controls)
    }

    /// Every control of the Core's file with `id`, wrapped in a one-row document at `version`.
    static func single(_ raw: [String: Any], category: String, version: Int = 12) throws -> SetupDescription.Control {
        let root: [String: Any] = ["version": version,
            "category": ["id": category, "title": category, "where": "mixed"],
            "pages": [["id": "\(category).test", "title": "Test", "where": "phone",
                       "sections": [["title": "Test", "controls": [raw]]]]]]
        return try #require(controls(try parse(root)).first)
    }

    static func rawControls(_ root: [String: Any]) -> [[String: Any]] {
        ((root["pages"] as? [[String: Any]]) ?? []).flatMap { ($0["sections"] as? [[String: Any]]) ?? [] }
            .flatMap { ($0["controls"] as? [[String: Any]]) ?? [] }
    }

    @Test func theCoresV12DisplayParsesWholeAndInTheDesktopsOrder() throws {
        let display = try Self.parse(try Self.resource("display"))
        #expect(display.version == 12)
        #expect(display.pages.map(\.id) == ["display.spectrumDefaults", "display.spectrumPeaks",
                                            "display.waterfallDefaults", "display.gridScales", "display.multimeter",
                                            "display.txDisplay", "display.threeD"])
        let all = Self.controls(display)
        #expect(all.count == 100)
        #expect(all.filter { $0.requiresDescriptionVersion == 12 }.count == 52)
        let issues = all.compactMap { control in control.metadataIssue.map { "\(control.id): \($0)" } }
        #expect(issues.isEmpty, "\(issues)")
        #expect(display.pages[0].sections.map(\.title)
                == ["Profile", "Fast Fourier Transform", "Rendering", "Spectrum Overlays"])
        #expect(display.pages[2].sections.map(\.title)
                == ["Levels", "Waterfall NF-AGC", "Display", "Overlays", "Rewind history", "Time"])
        #expect(display.pages[3].whereOwned == .phone && display.pages[3].coverage == nil)
        #expect(display.pages[3].sections.map(\.title) == ["Grid", "Labels", "Noise-Floor Tracking", "Copy"])
        #expect(display.pages[4].sections.map(\.title) == ["Multimeter", "Signal Units", "Signal History"])
        #expect(display.pages[5].sections.last?.title == "Waterfall Amplitude Scale")
        #expect(display.pages[6].whereOwned == .phone && display.pages[6].coverage == nil)

        let appearance = try Self.parse(try Self.resource("appearance"))
        #expect(appearance.version == 12)
        #expect(Self.controls(appearance).count == 14)
        let reset = try #require(Self.controls(appearance).first { $0.id == "appearance.colorsTheme.resetColors" })
        #expect(reset.metadataIssue == nil && reset.kind == .button && reset.binding == .phone("resetColors"))
        #expect(reset.confirm?.isEmpty == false)
        #expect(appearance.pages[0].sections.map(\.title) == ["Spectrum", "Reset"])
    }

    @Test func theNewFieldsAreReadAsSent() throws {
        let display = try Self.parse(try Self.resource("display"))
        let raw = Self.rawControls(try Self.resource("display"))
        func control(_ id: String) throws -> SetupDescription.Control {
            try #require(Self.controls(display).first { $0.id == id })
        }
        func sent(_ id: String) throws -> [String: Any] {
            try #require(raw.first { $0["id"] as? String == id })
        }
        // confirm: the desktop's exact question.
        for id in ["display.spectrumDefaults.smoothDefaults", "display.threeD.reset"] {
            #expect(try control(id).confirm == (try sent(id)["confirm"] as? String), "\(id)")
            #expect(try control(id).confirm?.isEmpty == false)
        }
        #expect(try control("display.spectrumDefaults.getMonitorHz").confirm == nil)
        // perBand: the label with %1 for the band's name.
        let dbMax = try control("display.gridScales.dbMax")
        #expect(dbMax.perBand == .init(label: "dB Max (%1):"))
        #expect(dbMax.perBand?.label(for: "20m") == "dB Max (20m):")
        #expect(try control("display.gridScales.dbMin").perBand?.label(for: "GEN") == "dB Min (GEN):")
        #expect(try control("display.threeD.floor").perBand?.label(for: "40m") == "3D Floor:")
        #expect(try control("display.gridScales.dbStep").perBand == nil)
        // enabledWhen on a phone key, with its switch or whole numbers.
        #expect(try control("display.spectrumDefaults.normalize").phoneDependency
                == .init(phone: "DisplaySpectrumDetector", oneOf: [.integer(2), .integer(3), .integer(4)]))
        #expect(try control("display.waterfallDefaults.highThreshold").phoneDependency
                == .init(phone: "DisplayWfUseSpectrumMinMax", oneOf: [.bool(false)]))
        #expect(try control("display.gridScales.maintainGridRange").phoneDependency
                == .init(phone: "DisplayAdjustGridMinToNoiseFloor", oneOf: [.bool(true)]))
        #expect(try control("display.spectrumDefaults.normalize").enabledWhen == nil)
        // Options with their own values, in the Core's order.
        #expect(try control("display.txDisplay.wfPalette").options?.map(\.value) == [1, 2, 3, 4, 5, 0, 6])
        #expect(try control("display.waterfallDefaults.historyDepth").options?.map(\.label)
                == ["60 seconds", "5 minutes", "15 minutes", "20 minutes"])
        // Gates as the note, with the tracking rows on version 4.
        #expect(try control("display.gridScales.adjustGridMinToNoiseFloor").gate?.minimum == 4)
        #expect(try control("display.spectrumDefaults.noiseFloorFastColor").gate?.minimum == 4)
        #expect(try control("display.txDisplay.wfLowColor").gate?.capability == "txDisplayVersion")
        #expect(try control("display.gridScales.showGrid").gate == nil)
        // A decimal row keeps its places.
        let shift = try control("display.spectrumDefaults.noiseFloorShift")
        #expect(shift.decimals == 1 && shift.range == .init(minimum: -12, maximum: 12, step: 0.5) && shift.unit == "dB")
    }

    @Test func everyV12RowRefusesAnyAlterationAndAnOlderDocument() throws {
        let rows = Self.rawControls(try Self.resource("display")).filter { $0["requiresDescriptionVersion"] as? Int == 12 }
            + Self.rawControls(try Self.resource("appearance")).filter { $0["requiresDescriptionVersion"] as? Int == 12 }
        #expect(rows.count == 53)
        for raw in rows {
            let id = try #require(raw["id"] as? String)
            let category = id.hasPrefix("appearance.") ? "appearance" : "display"
            #expect(try Self.single(raw, category: category).metadataIssue == nil, "\(id)")
            var alterations: [[String: Any]] = []
            var bad = raw; bad["applies"] = (raw["applies"] as? String) == "live" ? "subscription" : "live"
            alterations.append(bad)
            bad = raw; bad["binding"] = ["phone": "Other"]; alterations.append(bad)
            bad = raw; bad["surprise"] = true; alterations.append(bad)
            bad = raw; bad["requiresDescriptionVersion"] = 11; alterations.append(bad)
            bad = raw; bad["gate"] = ["capability": "displayExtrasVersion", "min": 9]; alterations.append(bad)
            if raw["default"] != nil {
                bad = raw; bad.removeValue(forKey: "default"); alterations.append(bad)
            }
            if raw["confirm"] != nil {
                bad = raw; bad["confirm"] = ""; alterations.append(bad)
            } else {
                bad = raw; bad["confirm"] = "Sure?"; alterations.append(bad)
            }
            if raw["perBand"] != nil {
                bad = raw; bad["perBand"] = ["label": "Other (%1):"]; alterations.append(bad)
            } else {
                bad = raw; bad["perBand"] = ["label": "%1"]; alterations.append(bad)
            }
            if var dependency = raw["enabledWhen"] as? [String: Any] {
                dependency["oneOf"] = [1]
                bad = raw; bad["enabledWhen"] = dependency; alterations.append(bad)
            }
            if var options = raw["options"] as? [[String: Any]] {
                options[0]["label"] = "Other"
                bad = raw; bad["options"] = options; alterations.append(bad)
            }
            for altered in alterations {
                let parsed = try Self.single(altered, category: category)
                #expect(parsed.metadataIssue != nil && !parsed.isEditable, "\(id): \(altered)")
            }
            // A V11 document cannot carry it.
            #expect(try Self.single(raw, category: category, version: 11).metadataIssue != nil, "\(id)")
            // Outside its own category it is not one of these.
            #expect(try Self.single(raw, category: category == "display" ? "general" : "display").metadataIssue != nil)
        }
        // perBand and confirm are closed: no other row may carry them.
        var other = Self.rawControls(try Self.resource("display")).first { $0["id"] as? String == "display.spectrumDefaults.panFill" }!
        other["confirm"] = "Sure?"
        #expect(try Self.single(other, category: "display", version: 12).metadataIssue != nil)
    }

    @Test func versionTwentyFourIsTheHighestAndOlderVersionsStillParse() throws {
        #expect(SetupDescription.highestVersion == 24)
        var root = try Self.resource("display")
        root["version"] = 25
        #expect(throws: SetupDescription.ParseError.self) { try Self.parse(root) }
        var raw = Self.rawControls(try Self.resource("display")).first { $0["id"] as? String == "display.gridScales.showGrid" }!
        raw["requiresDescriptionVersion"] = 13
        #expect(try Self.single(raw, category: "display").metadataIssue == "Unsupported Setup control version.")
    }

    @Test func rowsCanBeLeftOutAndEmptySectionsAndPagesGoToo() throws {
        let display = try Self.parse(try Self.resource("display"))
        let trimmed = display.leavingOut(["display.threeD", "display.multimeter.showDecimal",
                                          "display.multimeter.unitMode", "display.multimeter.historyDuration"])
        #expect(!trimmed.pages.contains { $0.id == "display.threeD" })
        let multimeter = try #require(trimmed.pages.first { $0.id == "display.multimeter" })
        #expect(multimeter.sections.map(\.title) == ["Multimeter"])
        #expect(multimeter.sections[0].controls.map(\.id) == ["display.multimeter.pollingDelay"])
        #expect(Self.controls(trimmed).count == 100 - 7 - 3)
        #expect(display.leavingOut([]) == display)
        #expect(trimmed.version == 12 && trimmed.category == display.category)
    }

    @Test func theBandNamesAreTheDesktops() {
        #expect(SetupDescription.bandNames.count == 14)
        #expect(SetupDescription.bandNames.first == "160m" && SetupDescription.bandNames[5] == "20m")
        #expect(SetupDescription.bandNames.suffix(3) == ["GEN", "WWV", "XVTR"])
    }
}
