// NereusSDR for iOS: Setup description V27, Appearance's Trace Color beside Fill Color, parsed from the Core's own file
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-24: the app asks for Setup description 30 (General's Filters
//   page). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import Testing
@testable import NereusMirror

/// The Appearance description a Core sends a phone below 27, made from the
/// Core's file as the Core makes it (the Core's
/// kAppearanceV12TraceFillColour): no Trace Color row, the one
/// "Trace & Fill Color:" row with its old sentence, and the document at
/// version 12.
enum AppearanceV12Fixture {
    static let traceId = "appearance.colorsTheme.traceColor"
    static let fillId = "appearance.colorsTheme.traceFillColor"
    static let v12Label = "Trace & Fill Color:"
    static let v12Tooltip = "Click to choose the spectrum trace line and fill color. The color picker lets you adjust alpha for the fill opacity."

    /// A row as a peer below 27 is sent it; any other row unchanged. The
    /// Trace Color row keeps its version 27, so the peer's projection
    /// drops it.
    static func row(_ raw: [String: Any]) -> [String: Any] {
        guard raw["id"] as? String == fillId else { return raw }
        var older = raw
        older["label"] = v12Label
        older["tooltip"] = v12Tooltip
        return older
    }

    /// The whole Appearance document at version 12.
    static func document(_ root: [String: Any]) -> [String: Any] {
        var older = root
        older["version"] = 12
        older["pages"] = ((root["pages"] as? [[String: Any]]) ?? []).map { page -> [String: Any] in
            var page = page
            page["sections"] = ((page["sections"] as? [[String: Any]]) ?? []).map { section -> [String: Any] in
                var section = section
                section["controls"] = ((section["controls"] as? [[String: Any]]) ?? [])
                    .filter { ($0["requiresDescriptionVersion"] as? Int ?? 1) <= 12 }.map(row)
                return section
            }
            return page
        }
        return older
    }
}

/// V27 (rulings wave R3): Appearance's Spectrum section has Trace Color
/// and Fill Color, as the desktop page has two pickers. Trace Color is one
/// closed colour row bound to `DisplayTraceColor`.
@Suite struct SetupDescriptionV27Tests {
    static func latest() throws -> [String: Any] {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/legacy/appearance-v27.json").standardizedFileURL
        return try #require(JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
    }

    static func traceRow() throws -> [String: Any] {
        try #require(SetupDescriptionV12Tests.rawControls(try latest())
            .first { $0["id"] as? String == AppearanceV12Fixture.traceId })
    }

    @Test func theCoresV27AppearanceParsesWholeWithTraceColourThenFillColour() throws {
        let appearance = try SetupDescriptionV12Tests.parse(try Self.latest())
        #expect(appearance.version == 27)
        let all = SetupDescriptionV12Tests.controls(appearance)
        #expect(all.count == 15)
        let issues = all.compactMap { control in control.metadataIssue.map { "\(control.id): \($0)" } }
        #expect(issues.isEmpty, "\(issues)")
        #expect(appearance.pages[0].sections.map(\.title) == ["Spectrum", "Reset"])
        let spectrum = appearance.pages[0].sections[0].controls
        #expect(spectrum.count == 11)
        let trace = spectrum[0]
        #expect(trace.id == AppearanceV12Fixture.traceId)
        #expect(trace.label == "Trace Color:")
        #expect(trace.tooltip == "Click to choose the spectrum trace line color. Until you choose one, the trace uses the fill color.")
        #expect(trace.kind == .colour && trace.applies == .live && trace.binding == .phone("DisplayTraceColor"))
        #expect(trace.requiresDescriptionVersion == 27)
        #expect(trace.defaultValue == .text("#00E5FFFF"))
        #expect(trace.isEditable)
        let fill = spectrum[1]
        #expect(fill.id == AppearanceV12Fixture.fillId)
        #expect(fill.label == "Fill Color:")
        #expect(fill.tooltip == "Click to choose the spectrum fill color. The color picker lets you adjust alpha for the fill opacity.")
        #expect(fill.binding == .phone("DisplayFillColor") && fill.requiresDescriptionVersion == nil)
        #expect(fill.metadataIssue == nil)
    }

    @Test func aPhoneBelowTwentySevenStillReadsTheOneTraceAndFillRow() throws {
        let older = try SetupDescriptionV12Tests.parse(AppearanceV12Fixture.document(try Self.latest()))
        #expect(older.version == 12)
        let all = SetupDescriptionV12Tests.controls(older)
        #expect(all.count == 14)
        #expect(all.allSatisfy { $0.metadataIssue == nil })
        #expect(!all.contains { $0.id == AppearanceV12Fixture.traceId })
        let spectrum = older.pages[0].sections[0].controls
        #expect(spectrum.count == 10)
        #expect(spectrum[0].id == AppearanceV12Fixture.fillId)
        #expect(spectrum[0].label == AppearanceV12Fixture.v12Label)
        #expect(spectrum[0].tooltip == AppearanceV12Fixture.v12Tooltip)
        #expect(spectrum[0].binding == .phone("DisplayFillColor"))
    }

    @Test func theTraceColourRowRefusesAnyAlterationAndAnOlderDocument() throws {
        let raw = try Self.traceRow()
        func single(_ row: [String: Any], category: String = "appearance", version: Int = 27) throws
            -> SetupDescription.Control {
            try SetupDescriptionV12Tests.single(row, category: category, version: version)
        }
        #expect(try single(raw).metadataIssue == nil)
        var alterations: [[String: Any]] = []
        var bad = raw; bad["applies"] = "subscription"; alterations.append(bad)
        bad = raw; bad["binding"] = ["phone": "DisplayFillColor"]; alterations.append(bad)
        bad = raw; bad["binding"] = ["phone": "Other"]; alterations.append(bad)
        bad = raw; bad["kind"] = "text"; alterations.append(bad)
        bad = raw; bad["surprise"] = true; alterations.append(bad)
        bad = raw; bad["requiresDescriptionVersion"] = 12; alterations.append(bad)
        bad = raw; bad["requiresDescriptionVersion"] = 26; alterations.append(bad)
        bad = raw; bad.removeValue(forKey: "requiresDescriptionVersion"); alterations.append(bad)
        bad = raw; bad["gate"] = ["capability": "displayExtrasVersion", "min": 1]; alterations.append(bad)
        bad = raw; bad.removeValue(forKey: "default"); alterations.append(bad)
        bad = raw; bad["default"] = "#00E5FF"; alterations.append(bad)
        bad = raw; bad["default"] = "#FFFFFFFF"; alterations.append(bad)
        bad = raw; bad["confirm"] = "Sure?"; alterations.append(bad)
        for altered in alterations {
            let parsed = try single(altered)
            #expect(parsed.metadataIssue != nil && !parsed.isEditable, "\(altered)")
        }
        // A document below 27 cannot carry it.
        for version in [12, 26] {
            #expect(try single(raw, version: version).metadataIssue != nil, "\(version)")
        }
        // The ten swatches still carry no version, at 27 as before.
        var fill = try #require(SetupDescriptionV12Tests.rawControls(try Self.latest())
            .first { $0["id"] as? String == AppearanceV12Fixture.fillId })
        #expect(try single(fill).metadataIssue == nil)
        #expect(try single(AppearanceV12Fixture.row(fill), version: 12).metadataIssue == nil)
        fill["requiresDescriptionVersion"] = 27
        #expect(try single(fill).metadataIssue != nil)
    }

    @MainActor
    @Test func aDocumentAboveTheHighestIsRefusedAndThePhoneReadsAppearanceAtTheCoresVersion() throws {
        #expect(SetupDescription.highestVersion == 30)
        #expect(SetupDescription.v27Version == 27)
        var root = try Self.latest()
        root["version"] = 31
        #expect(throws: SetupDescription.ParseError.self) { try SetupDescriptionV12Tests.parse(root) }
        for (cap, version) in [4: 4, 6: 4, 7: 7, 11: 7, 12: 12, 25: 12, 26: 12, 27: 27, 28: 27] as [Int64: Int] {
            #expect(SetupDescriptionFeed.projectedVersion(maximum: cap, category: "appearance") == version, "\(cap)")
        }
        #expect(SetupDescriptionFeed.projectedVersion(maximum: 31, category: "appearance") == nil)
    }
}
