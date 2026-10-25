// NereusSDR for iOS: Setup description V26, Waterfall Defaults' level gates and the allOf dependency, parsed from the Core's own file
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-24: the app asks for Setup description 30 (General's Filters
//   page). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import Testing
@testable import NereusMirror

/// The Display description a Core sends a phone below 26, made from the
/// Core's file as the Core makes it (the Core's kDisplayV12Controls): the
/// six Waterfall Defaults level rows in their version 12 form, and the
/// document at version 12.
enum DisplayV12Fixture {
    static let gatedIds: Set<String> = [
        "display.waterfallDefaults.highThreshold", "display.waterfallDefaults.lowThreshold",
        "display.waterfallDefaults.agc", "display.waterfallDefaults.useSpectrumMinMax",
        "display.waterfallDefaults.nfAgc", "display.waterfallDefaults.nfAgcOffset",
    ]

    /// Reset to Smooth Defaults, whose sentences changed at 26, and the
    /// two a peer below 26 is sent (the Core's kDisplayV12Controls).
    static let smoothDefaultsId = "display.spectrumDefaults.smoothDefaults"
    static let smoothDefaultsV12Tooltip = "Overwrite this panadapter's spectrum and waterfall look with the NereusSDR smooth-default profile: Clarity Blue palette, log-recursive averaging with a 650 ms averaging time, a white trace without fill, waterfall AGC on and a 30 ms waterfall update period. FFT size, frequency, band stack, and per-band grid ranges are not affected."
    static let smoothDefaultsV12Confirm = "This will overwrite your current Spectrum and Waterfall display settings with the NereusSDR smooth-default profile.\n\nYour FFT size, frequency, band stack, and per-band grid ranges are NOT affected.\n\nContinue?"

    /// Enable Clarity, whose tooltip changed at 26, and the sentence a
    /// peer below 26 is sent (the Core's kDisplayV12Controls).
    static let clarityId = "display.spectrumDefaults.clarity"
    static let clarityV12Tooltip = "Clarity keeps the waterfall thresholds centered on the actual noise floor as band conditions and tuning change. Uses a 30th-percentile estimator with 3-second EWMA smoothing and a \u{00B1}2 dB deadband. When off, thresholds are fixed at their last values."
    static let clarityV26Tooltip = "Clarity keeps the waterfall's black point just above the noise floor as band conditions and tuning change. Black Level and Color Gain set how far above the noise the waterfall starts and how much range it shows. When Clarity is off, Low and High set the levels."

    /// A row as a peer below 26 is sent it; any other row unchanged.
    static func row(_ raw: [String: Any]) -> [String: Any] {
        guard let id = raw["id"] as? String,
              gatedIds.contains(id) || id == smoothDefaultsId || id == clarityId,
              raw["requiresDescriptionVersion"] as? Int == 26 else { return raw }
        var older = raw
        older["requiresDescriptionVersion"] = 12
        if id == smoothDefaultsId {
            older["tooltip"] = smoothDefaultsV12Tooltip
            older["confirm"] = smoothDefaultsV12Confirm
            return older
        }
        if id == clarityId {
            older["tooltip"] = clarityV12Tooltip
            return older
        }
        if id == "display.waterfallDefaults.useSpectrumMinMax" {
            older.removeValue(forKey: "enabledWhen")
        } else {
            older["enabledWhen"] = ["phone": "DisplayWfUseSpectrumMinMax", "oneOf": [false]] as [String: Any]
        }
        return older
    }

    /// The whole Display document at version 12, from the Core's file or
    /// from its version 26 form.
    static func document(_ root: [String: Any]) -> [String: Any] {
        let root = DisplayV26Fixture.document(root)
        var older = root
        older["version"] = 12
        older["pages"] = ((root["pages"] as? [[String: Any]]) ?? []).map { page -> [String: Any] in
            var page = page
            page["sections"] = ((page["sections"] as? [[String: Any]]) ?? []).map { section -> [String: Any] in
                var section = section
                section["controls"] = ((section["controls"] as? [[String: Any]]) ?? []).map(row)
                return section
            }
            return page
        }
        return older
    }
}

/// V26 (the display pill and levels design, section 8.3): the six Waterfall
/// Defaults level rows grey out as the desktop page greys them. Five carry
/// the `allOf` form of `enabledWhen`; Use spectrum min/max the single form.
@Suite struct SetupDescriptionV26Tests {
    typealias Dependency = SetupDescription.PhoneDependency
    static let clarityOff = Dependency(phone: "ClarityEnabled", oneOf: [.bool(false)])
    static let minMaxOff = Dependency(phone: "DisplayWfUseSpectrumMinMax", oneOf: [.bool(false)])
    static let agcOn = Dependency(phone: "DisplayWfAgc", oneOf: [.bool(true)])

    /// The Display description at version 26: the Core's file as a phone
    /// that asks for 26 or 27 is sent it (the file itself is at 28,
    /// SetupDescriptionV28Tests).
    static func latest() throws -> [String: Any] {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/legacy/display-v28.json").standardizedFileURL
        let root = try #require(JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
        return DisplayV26Fixture.document(root)
    }

    static func gatedRows() throws -> [[String: Any]] {
        SetupDescriptionV12Tests.rawControls(try latest()).filter {
            $0["requiresDescriptionVersion"] as? Int == 26
                && DisplayV12Fixture.gatedIds.contains($0["id"] as? String ?? "")
        }
    }

    /// Rulings wave R7: Reset to Smooth Defaults is a version 26 row whose
    /// tooltip and question say it sets every panadapter's defaults; a
    /// phone below 26 is sent the version 12 sentences.
    @Test func resetToSmoothDefaultsIsAVersionTwentySixRowWithItsNewSentences() throws {
        let id = DisplayV12Fixture.smoothDefaultsId
        let raw = try #require(SetupDescriptionV12Tests.rawControls(try Self.latest())
            .first { $0["id"] as? String == id })
        #expect(raw["requiresDescriptionVersion"] as? Int == 26)
        let row = try SetupDescriptionV12Tests.single(raw, category: "display", version: 26)
        #expect(row.metadataIssue == nil && row.isEditable)
        #expect(row.kind == .button && row.binding == .phone("smoothDefaults"))
        #expect(row.tooltip == "Set every panadapter's default spectrum and waterfall look to the NereusSDR smooth profile: Clarity Blue palette, log-recursive averaging with a 650 ms averaging time, a white trace without fill, waterfall AGC on and a 30 ms waterfall update period. A panadapter you have adjusted from its Display panel keeps its own settings. FFT size, frequency, band stack, and per-band grid ranges are not affected.")
        #expect(row.confirm == raw["confirm"] as? String)
        #expect(row.confirm?.hasPrefix("This will set every panadapter's default Spectrum and Waterfall display settings") == true)
        // Closed: nothing may be changed, added or taken away.
        var alterations: [[String: Any]] = []
        var bad = raw; bad["applies"] = "subscription"; alterations.append(bad)
        bad = raw; bad["binding"] = ["phone": "Other"]; alterations.append(bad)
        bad = raw; bad["surprise"] = true; alterations.append(bad)
        bad = raw; bad["requiresDescriptionVersion"] = 12; alterations.append(bad)
        bad = raw; bad["requiresDescriptionVersion"] = 25; alterations.append(bad)
        bad = raw; bad["gate"] = ["capability": "displayExtrasVersion", "min": 1]; alterations.append(bad)
        bad = raw; bad["confirm"] = ""; alterations.append(bad)
        bad = raw; bad.removeValue(forKey: "confirm"); alterations.append(bad)
        bad = raw; bad["default"] = true; alterations.append(bad)
        for altered in alterations {
            let parsed = try SetupDescriptionV12Tests.single(altered, category: "display", version: 26)
            #expect(parsed.metadataIssue != nil && !parsed.isEditable, "\(altered)")
        }
        // A document below 26 cannot carry it; it carries the version 12 row.
        for version in [12, 25] {
            #expect(try SetupDescriptionV12Tests.single(raw, category: "display", version: version).metadataIssue != nil)
            let older = try SetupDescriptionV12Tests.single(DisplayV12Fixture.row(raw), category: "display",
                                                            version: version)
            #expect(older.metadataIssue == nil && older.isEditable, "v\(version)")
            #expect(older.tooltip == DisplayV12Fixture.smoothDefaultsV12Tooltip)
            #expect(older.confirm == DisplayV12Fixture.smoothDefaultsV12Confirm)
        }
    }

    /// Follow-up wave F2: Enable Clarity is a version 26 row whose tooltip
    /// says what Clarity does now; a phone below 26 is sent the version 12
    /// sentence.
    @Test func enableClarityIsAVersionTwentySixRowWithItsNewTooltip() throws {
        let id = DisplayV12Fixture.clarityId
        let raw = try #require(SetupDescriptionV12Tests.rawControls(try Self.latest())
            .first { $0["id"] as? String == id })
        #expect(raw["requiresDescriptionVersion"] as? Int == 26)
        let row = try SetupDescriptionV12Tests.single(raw, category: "display", version: 26)
        #expect(row.metadataIssue == nil && row.isEditable)
        #expect(row.kind == .toggle && row.binding == .phone("ClarityEnabled"))
        #expect(row.defaultValue == .bool(true))
        #expect(row.gate?.capability == "displayExtrasVersion" && row.gate?.minimum == 1)
        #expect(row.tooltip == DisplayV12Fixture.clarityV26Tooltip)
        #expect(row.phoneDependencies.isEmpty)
        // Closed: nothing may be changed, added or taken away.
        var alterations: [[String: Any]] = []
        var bad = raw; bad["applies"] = "live"; alterations.append(bad)
        bad = raw; bad["binding"] = ["phone": "Other"]; alterations.append(bad)
        bad = raw; bad["surprise"] = true; alterations.append(bad)
        bad = raw; bad["requiresDescriptionVersion"] = 25; alterations.append(bad)
        bad = raw; bad["gate"] = ["capability": "displayExtrasVersion", "min": 9]; alterations.append(bad)
        bad = raw; bad.removeValue(forKey: "default"); alterations.append(bad)
        bad = raw; bad["confirm"] = "Sure?"; alterations.append(bad)
        for altered in alterations {
            let parsed = try SetupDescriptionV12Tests.single(altered, category: "display", version: 26)
            #expect(parsed.metadataIssue != nil && !parsed.isEditable, "\(altered)")
        }
        // A document below 26 cannot carry it; it carries the version 12 row.
        for version in [12, 25] {
            #expect(try SetupDescriptionV12Tests.single(raw, category: "display", version: version).metadataIssue != nil)
            let older = try SetupDescriptionV12Tests.single(DisplayV12Fixture.row(raw), category: "display",
                                                            version: version)
            #expect(older.metadataIssue == nil && older.isEditable, "v\(version)")
            #expect(older.tooltip == DisplayV12Fixture.clarityV12Tooltip)
        }
    }

    @Test func theCoresV26DisplayParsesWholeAndExposesEachTerm() throws {
        let display = try SetupDescriptionV12Tests.parse(try Self.latest())
        #expect(display.version == 26)
        let all = SetupDescriptionV12Tests.controls(display)
        #expect(all.count == 100)
        #expect(all.filter { $0.requiresDescriptionVersion == 26 }.map(\.id).sorted()
                == (DisplayV12Fixture.gatedIds
                    + [DisplayV12Fixture.smoothDefaultsId, DisplayV12Fixture.clarityId]).sorted())
        #expect(all.filter { $0.requiresDescriptionVersion == 12 }.count == 44)
        let issues = all.compactMap { control in control.metadataIssue.map { "\(control.id): \($0)" } }
        #expect(issues.isEmpty, "\(issues)")
        func control(_ id: String) throws -> SetupDescription.Control {
            try #require(all.first { $0.id == id })
        }
        for id in ["display.waterfallDefaults.highThreshold", "display.waterfallDefaults.lowThreshold",
                   "display.waterfallDefaults.agc"] {
            let row = try control(id)
            #expect(row.phoneAllOf == [Self.clarityOff, Self.minMaxOff], "\(id)")
            #expect(row.phoneDependency == nil, "\(id)")
            #expect(row.phoneDependencies == [Self.clarityOff, Self.minMaxOff], "\(id)")
            #expect(row.isEditable, "\(id)")
        }
        for id in ["display.waterfallDefaults.nfAgc", "display.waterfallDefaults.nfAgcOffset"] {
            #expect(try control(id).phoneAllOf == [Self.clarityOff, Self.minMaxOff, Self.agcOn], "\(id)")
        }
        // Use spectrum min/max keeps the single form.
        let minMax = try control("display.waterfallDefaults.useSpectrumMinMax")
        #expect(minMax.phoneAllOf == nil && minMax.phoneDependency == Self.clarityOff)
        #expect(minMax.phoneDependencies == [Self.clarityOff])
        // Everything else about the rows is as version 12 has it.
        let high = try control("display.waterfallDefaults.highThreshold")
        #expect(high.kind == .slider && high.binding == .phone("DisplayWfHighLevel") && high.unit == "dBm")
        #expect(high.range == .init(minimum: -200, maximum: 0, step: 1) && high.defaultValue == .integer(-62))
        #expect(high.gate?.capability == "displayExtrasVersion" && high.gate?.minimum == 1)
        // A row with no dependency follows nothing.
        #expect(try control("display.waterfallDefaults.colorScheme").phoneDependencies.isEmpty)
    }

    @Test func aPhoneBelowTwentySixStillReadsTheVersionTwelveRows() throws {
        let display = try SetupDescriptionV12Tests.parse(DisplayV12Fixture.document(try Self.latest()))
        #expect(display.version == 12)
        let all = SetupDescriptionV12Tests.controls(display)
        #expect(all.count == 100)
        #expect(all.filter { $0.requiresDescriptionVersion == 12 }.count == 52)
        #expect(all.allSatisfy { $0.metadataIssue == nil && $0.phoneAllOf == nil })
        for id in DisplayV12Fixture.gatedIds {
            let row = try #require(all.first { $0.id == id })
            #expect(row.phoneDependency == (id == "display.waterfallDefaults.useSpectrumMinMax" ? nil : Self.minMaxOff),
                    "\(id)")
        }
    }

    @Test func everyV26RowRefusesAnyAlterationAndAnOlderDocument() throws {
        let rows = try Self.gatedRows()
        #expect(rows.count == 6)
        for raw in rows {
            let id = try #require(raw["id"] as? String)
            #expect(try SetupDescriptionV12Tests.single(raw, category: "display", version: 26).metadataIssue == nil, "\(id)")
            var alterations: [[String: Any]] = []
            var bad = raw; bad["applies"] = "live"; alterations.append(bad)
            bad = raw; bad["binding"] = ["phone": "Other"]; alterations.append(bad)
            bad = raw; bad["surprise"] = true; alterations.append(bad)
            bad = raw; bad["requiresDescriptionVersion"] = 12; alterations.append(bad)
            bad = raw; bad["requiresDescriptionVersion"] = 25; alterations.append(bad)
            bad = raw; bad["gate"] = ["capability": "displayExtrasVersion", "min": 9]; alterations.append(bad)
            bad = raw; bad.removeValue(forKey: "default"); alterations.append(bad)
            bad = raw; bad.removeValue(forKey: "enabledWhen"); alterations.append(bad)
            bad = raw; bad["confirm"] = "Sure?"; alterations.append(bad)
            // The version 12 form is not a version 26 row.
            alterations.append(DisplayV12Fixture.row(raw))
            for altered in alterations {
                let parsed = try SetupDescriptionV12Tests.single(altered, category: "display", version: 26)
                #expect(parsed.metadataIssue != nil && !parsed.isEditable, "\(id): \(altered)")
                #expect(parsed.phoneAllOf == nil && parsed.phoneDependency == nil, "\(id): \(altered)")
            }
            // A document below 26 cannot carry it.
            for version in [12, 25] {
                let parsed = try SetupDescriptionV12Tests.single(raw, category: "display", version: version)
                #expect(parsed.metadataIssue != nil && !parsed.isEditable, "\(id) v\(version)")
            }
            // Outside Display it is not one of these: the open reader has no
            // allOf form, so the row is shown greyed and carries no terms.
            let elsewhere = try SetupDescriptionV12Tests.single(raw, category: "general", version: 26)
            #expect(!elsewhere.isEditable && elsewhere.unavailableReason != nil && elsewhere.phoneAllOf == nil, "\(id)")
        }
    }

    @Test func aMalformedAllOfFailsClosed() throws {
        let rows = try Self.gatedRows()
        let high = try #require(rows.first { $0["id"] as? String == "display.waterfallDefaults.highThreshold" })
        let clarity: [String: Any] = ["phone": "ClarityEnabled", "oneOf": [false]]
        let minMax: [String: Any] = ["phone": "DisplayWfUseSpectrumMinMax", "oneOf": [false]]
        let agc: [String: Any] = ["phone": "DisplayWfAgc", "oneOf": [true]]
        var extraKey = clarity
        extraKey["note"] = "x"
        let malformed: [(String, Any)] = [
            ("no terms", ["allOf": [Any]()]),
            ("a term missing", ["allOf": [clarity]]),
            ("a term added", ["allOf": [clarity, minMax, agc]]),
            ("terms reordered", ["allOf": [minMax, clarity]]),
            ("a term with another key", ["allOf": [extraKey, minMax]]),
            ("a nested allOf", ["allOf": [clarity, ["allOf": [minMax]]]]),
            ("a term that is not an object", ["allOf": [clarity, "DisplayWfUseSpectrumMinMax"]]),
            ("allOf that is not a list", ["allOf": clarity]),
            ("allOf beside the single form", ["allOf": [clarity, minMax], "phone": "ClarityEnabled", "oneOf": [false]]),
            ("the single form", clarity),
            ("another value", ["allOf": [["phone": "ClarityEnabled", "oneOf": [true]], minMax]]),
            ("a number for a switch", ["allOf": [["phone": "ClarityEnabled", "oneOf": [0]], minMax]]),
            ("another key", ["allOf": [["phone": "DisplayWfAgc", "oneOf": [false]], minMax]]),
            ("not an object", "ClarityEnabled"),
        ]
        for (what, dependency) in malformed {
            var bad = high
            bad["enabledWhen"] = dependency
            let parsed = try SetupDescriptionV12Tests.single(bad, category: "display", version: 26)
            #expect(parsed.metadataIssue != nil && !parsed.isEditable && parsed.unavailableReason != nil, "\(what)")
            #expect(parsed.phoneAllOf == nil, "\(what)")
        }
        // Use spectrum min/max is closed on its single form.
        let minMaxRow = try #require(rows.first { $0["id"] as? String == "display.waterfallDefaults.useSpectrumMinMax" })
        var bad = minMaxRow
        bad["enabledWhen"] = ["allOf": [clarity]]
        #expect(try SetupDescriptionV12Tests.single(bad, category: "display", version: 26).metadataIssue != nil)
        // No other Display row may carry the form, at 12 or asking for 26.
        var other = try #require(SetupDescriptionV12Tests.rawControls(try Self.latest())
            .first { $0["id"] as? String == "display.spectrumDefaults.normalize" })
        other["enabledWhen"] = ["allOf": [clarity, minMax]]
        var parsed = try SetupDescriptionV12Tests.single(other, category: "display", version: 26)
        #expect(parsed.metadataIssue != nil && !parsed.isEditable && parsed.phoneAllOf == nil)
        other["requiresDescriptionVersion"] = 26
        parsed = try SetupDescriptionV12Tests.single(other, category: "display", version: 26)
        #expect(!parsed.isEditable && parsed.phoneAllOf == nil)
    }

    @MainActor
    @Test func aDocumentAboveTheHighestIsRefusedAndThePhoneReadsDisplayAtTheCoresVersion() throws {
        #expect(SetupDescription.highestVersion == 30)
        var root = try Self.latest()
        root["version"] = 31
        #expect(throws: SetupDescription.ParseError.self) { try SetupDescriptionV12Tests.parse(root) }
        for (cap, version) in [7: 4, 11: 11, 12: 12, 24: 12, 25: 12, 26: 26, 27: 26, 28: 28] as [Int64: Int] {
            #expect(SetupDescriptionFeed.projectedVersion(maximum: cap, category: "display") == version, "\(cap)")
        }
        #expect(SetupDescriptionFeed.projectedVersion(maximum: 31, category: "display") == nil)
    }
}
