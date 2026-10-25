// NereusSDR for iOS: closed V29 Display/Appearance description regressions
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// J.J. Boyd (KG4VCF), October 12, 2026; AI-assisted via OpenAI Codex.
// 2026-10-24: the app asks for Setup description 30 (General's Filters
//   page). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusLink
@testable import NereusMirror
import Testing

@Suite struct SetupDescriptionV29Tests {
    static func document(_ category: String) throws -> [String: Any] {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/\(category).json").standardizedFileURL
        return try #require(JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
    }

    static func raw(_ id: String) throws -> [String: Any] {
        try #require(SetupDescriptionV12Tests.rawControls(try document("display"))
            .first { $0["id"] as? String == id })
    }

    @Test func theLiveV29DisplayParsesEveryApprovedPageAndClosedRow() throws {
        let display = try SetupDescriptionV12Tests.parse(try Self.document("display"))
        #expect(display.version == 29)
        #expect(display.pages.map(\.title) == ["Spectrum", "Levels", "Waterfall", "View", "Peaks & Markers",
                                                "FFT & Engine", "Transmit Display", "Multimeter"])
        let rows = SetupDescriptionV12Tests.controls(display)
        #expect(rows.allSatisfy { $0.metadataIssue == nil })
        let ids = rows.map(\.id)
        #expect(Set(ids).count == ids.count)
        #expect(!ids.contains("display.spectrumDefaults.clarity"))
        #expect(!ids.contains("display.waterfallDefaults.agc"))
        #expect(!ids.contains("display.waterfallDefaults.nfAgc"))
        #expect(!ids.contains("display.waterfallDefaults.useSpectrumMinMax"))
    }

    @Test func theCompositeChoicesUseExactLocalTextOptions() throws {
        let expected: [(id: String, key: String, tokens: [String], labels: [String], start: String)] = [
            ("display.levels.waterfallMode", "DisplayWaterfallLevelsMode",
             ["Clarity", "Auto", "Floor", "Manual", "Scale"], ["Clarity", "Auto", "Floor", "Manual", "Scale"], "Clarity"),
            ("display.levels.scaleMode", "DisplaySpectrumScaleMode",
             ["Manual", "TrackFloor"], ["Manual", "Track floor"], "Manual"),
        ]
        for want in expected {
            let raw = try Self.raw(want.id)
            let row = try SetupDescriptionV12Tests.single(raw, category: "display", version: 29)
            #expect(row.metadataIssue == nil)
            #expect(row.binding == .phone(want.key) && row.kind == .choice && row.applies == .subscription)
            #expect(row.requiresDescriptionVersion == 29 && row.defaultValue == .text(want.start))
            #expect(row.options?.map(\.label) == want.labels)
            for (index, token) in want.tokens.enumerated() {
                #expect(row.modern?.optionLiterals[Int64(index)] == .text(token))
            }
            for (field, bad) in [("owner", "mixed"), ("default", "UnknownMode"), ("unexpected", "extra")] {
                var changed = raw
                changed[field] = bad
                #expect(try SetupDescriptionV12Tests.single(changed, category: "display", version: 29).metadataIssue != nil)
            }
            var older = raw
            older["requiresDescriptionVersion"] = 28
            #expect(try SetupDescriptionV12Tests.single(older, category: "display", version: 28).metadataIssue != nil)
        }
    }

    @Test func theEditorAndReadoutEnumsAreStrictV29Rows() throws {
        let expected: [(id: String, field: String, word: String)] = [
            ("display.waterfall.customGradient", "editor", "receiveGradient"),
            ("display.transmit.customGradient", "editor", "transmitGradient"),
            ("display.waterfallDefaults.lowThreshold", "readout", "waterfallLow"),
            ("display.waterfallDefaults.highThreshold", "readout", "waterfallHigh"),
            ("display.gridScales.dbMax", "readout", "spectrumMax"),
            ("display.gridScales.dbMin", "readout", "spectrumMin"),
        ]
        for want in expected {
            let raw = try Self.raw(want.id)
            #expect(raw[want.field] as? String == want.word)
            #expect(try SetupDescriptionV12Tests.single(raw, category: "display", version: 29).metadataIssue == nil)
            var changed = raw
            changed[want.field] = "UnknownMetadata"
            #expect(try SetupDescriptionV12Tests.single(changed, category: "display", version: 29).metadataIssue != nil)
            changed = raw
            changed["requiresDescriptionVersion"] = 28
            #expect(try SetupDescriptionV12Tests.single(changed, category: "display", version: 28).metadataIssue != nil)
        }
    }

    @MainActor
    @Test func thePhoneAsksFor29AndProjectsBothChangedCategories() throws {
        #expect(SetupDescription.highestVersion == 30)
        #expect(LinkFeatures.app["setupDescription"] == 30)
        for (category, oldVersion) in [("display", 28), ("appearance", 27)] {
            #expect(SetupDescriptionFeed.projectedVersion(maximum: 28, category: category) == oldVersion)
            #expect(SetupDescriptionFeed.projectedVersion(maximum: 29, category: category) == 29)
        }
        var beyond = try Self.document("display")
        beyond["version"] = 31
        #expect(throws: SetupDescription.ParseError.self) { try SetupDescriptionV12Tests.parse(beyond) }
    }

    @Test func sectionOwnersAndExplanatoryEmptyControlsAreStrictV29() throws {
        let root = try Self.document("display")
        let parsed = try SetupDescriptionV12Tests.parse(root)
        #expect(parsed.version == 29)
        let originalPages = try #require(root["pages"] as? [[String: Any]])
        let firstSections = try #require(originalPages[0]["sections"] as? [[String: Any]])
        #expect(firstSections[0]["owner"] as? String == "pan")
        let levels = try #require(originalPages.first { $0["title"] as? String == "Levels" })
        let levelSections = try #require(levels["sections"] as? [[String: Any]])
        let explanatory = try #require(levelSections.first { $0["title"] as? String == "While transmitting" })
        #expect((explanatory["controls"] as? [[String: Any]])?.isEmpty == true)
        #expect(explanatory["note"] as? String == "A pan that transmits is drawn with the transmit scale and the transmit levels until it stops. Transmit Display →")
        for bad in ["mixed", "computer", "unknown"] {
            var changed = root
            var pages = originalPages
            var sections = firstSections
            sections[0]["owner"] = bad
            pages[0]["sections"] = sections
            changed["pages"] = pages
            #expect(throws: SetupDescription.ParseError.self) { try SetupDescriptionV12Tests.parse(changed) }
        }
        var changed = root
        var pages = originalPages
        var sections = firstSections
        sections[0]["controls"] = [] as [[String: Any]]
        sections[0]["note"] = "An unapproved empty group"
        pages[0]["sections"] = sections
        changed["pages"] = pages
        #expect(throws: SetupDescription.ParseError.self) { try SetupDescriptionV12Tests.parse(changed) }
        // The older envelope cannot acquire the new field by claiming it
        // carries the same ordinary controls as before.
        let frozenPath = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/legacy/display-v28.json").standardizedFileURL
        var old = try #require(JSONSerialization.jsonObject(with: Data(contentsOf: frozenPath)) as? [String: Any])
        var oldPages = try #require(old["pages"] as? [[String: Any]])
        var oldSections = try #require(oldPages[0]["sections"] as? [[String: Any]])
        oldSections[0]["owner"] = "pan"
        oldPages[0]["sections"] = oldSections
        old["pages"] = oldPages
        #expect(throws: SetupDescription.ParseError.self) { try SetupDescriptionV12Tests.parse(old) }
    }
}


extension SetupDescriptionV29Tests {
    @Test func everyV29RowIsExactAndCarriesTypedOwnerEditorAndReadout() throws {
        for category in ["display", "appearance"] {
            let root = try Self.document(category)
            let parsed = try SetupDescriptionV12Tests.parse(root)
            let rows = SetupDescriptionV12Tests.controls(parsed)
            #expect(rows.allSatisfy { $0.metadataIssue == nil && $0.owner != nil })
            for raw in SetupDescriptionV12Tests.rawControls(root) {
                var changed = raw
                changed["unexpected"] = true
                #expect(try SetupDescriptionV12Tests.single(changed, category: category, version: 29).metadataIssue != nil)
                for key in ["owner", "default", "enabledWhen", "options", "editor", "readout", "availability"] where raw[key] != nil {
                    changed = raw
                    changed.removeValue(forKey: key)
                    #expect(try SetupDescriptionV12Tests.single(changed, category: category, version: 29).metadataIssue != nil)
                }
            }
        }
        let parsed = try SetupDescriptionV12Tests.parse(Self.document("display"))
        let rows = SetupDescriptionV12Tests.controls(parsed)
        #expect(rows.first { $0.id == "display.waterfall.customGradient" }?.editor == .receiveGradient)
        #expect(rows.first { $0.id == "display.transmit.customGradient" }?.editor == .transmitGradient)
        #expect(rows.first { $0.id == "display.waterfallDefaults.lowThreshold" }?.readout == .waterfallLow)
        #expect(rows.first { $0.id == "display.gridScales.dbMax" }?.readout == .spectrumMax)
        #expect(parsed.pages[0].sections[0].owner == .pan)
        #expect(parsed.pages[1].sections.last?.note?.isEmpty == false)
    }
}


extension SetupDescriptionV29Tests {
    @Test func transmitRenderingChoicesScaleAndDefinitionsAreAllComputerOwned() throws {
        let parsed = try SetupDescriptionV12Tests.parse(Self.document("display"))
        let rows = SetupDescriptionV12Tests.controls(parsed)
        for id in ["display.txDisplay.wfLowLevel", "display.txDisplay.wfHighLevel",
                   "display.txDisplay.wfPalette", "display.txDisplay.wfLowColor"] {
            #expect(rows.first { $0.id == id }?.owner == .phone)
        }
        for id in ["display.transmit.scaleMax", "display.transmit.scaleMin", "display.transmit.customGradient"] {
            #expect(rows.first { $0.id == id }?.owner == .phone)
        }
    }

    @Test func frozen28NeverDescribesLegacyBlackOrGainAndCannotAcquireTheirNewRows() throws {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/legacy/display-v28.json").standardizedFileURL
        let old = try #require(JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
        let parsed = try SetupDescriptionV12Tests.parse(old)
        let rows = SetupDescriptionV12Tests.controls(parsed)
        #expect(!rows.contains { $0.binding == .phone("DisplayWfBlackLevel") || $0.binding == .phone("DisplayWfColorGain") })
        for id in ["display.levels.blackLevel", "display.levels.colorGain"] {
            var fabricated = try Self.raw(id)
            fabricated["requiresDescriptionVersion"] = 12
            fabricated.removeValue(forKey: "owner")
            let row = try SetupDescriptionV12Tests.single(fabricated, category: "display", version: 28)
            #expect(row.metadataIssue != nil)
        }
    }
}


extension SetupDescriptionV29Tests {
    @Test func textPositionRemainsStoredButIsHonestlyUnavailableWithItsCompleteReason() throws {
        let row = try SetupDescriptionV12Tests.single(Self.raw("display.peaksMarkers.noiseFloorPosition"), category: "display", version: 29)
        #expect(row.metadataIssue == nil)
        #expect(row.availability?.enabled == false)
        #expect(row.availability?.reason == "Not used: the noise floor text sits beside its box. The setting is kept but has no visual effect.")
    }
}
