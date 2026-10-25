// NereusSDR for iOS: Setup description V28, Spectrum Defaults' Sizing choice and averaging from 1 ms, parsed from the Core's own file
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-24: the app asks for Setup description 30 (General's Filters
//   page). J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusLink
import NereusModels
import Testing
@testable import NereusMirror

/// The Display description a Core sends a phone below 28, made from the
/// Core's file as the Core makes it: no Sizing row, the Size row without
/// its gate, the Hz/bin Target row from 0 ("Off") with its earlier
/// sentence, the two averaging times from 10 ms by 10, and the document at
/// version 26.
enum DisplayV26Fixture {
    static let sizingId = "display.spectrumDefaults.fftSizing"
    static let sizeId = "display.spectrumDefaults.fftSize"
    static let targetId = "display.spectrumDefaults.hzPerBinTarget"
    static let averageIds: Set<String> = ["display.spectrumDefaults.averageTime",
                                          "display.waterfallDefaults.averageTime"]
    static let targetV26Tooltip = "Auto-zoom override: target a constant Hz/bin regardless of zoom. Set to 0 (\"Off\") to use the default bins-in-window behavior (FFT replans on zoom). When > 0, the FFT size is fixed at sample rate / target so the trace delivers the requested resolution at any zoom level, which is handy for hunting narrow signals (CW, digital). Floor at the FFT slider value still applies (slider sets the minimum FFT size)."

    /// A row as a peer below 28 is sent it: nil for the Sizing row, which
    /// such a peer is not sent; any row that is not one of version 28's
    /// unchanged.
    static func row(_ raw: [String: Any]) -> [String: Any]? {
        guard let id = raw["id"] as? String, raw["requiresDescriptionVersion"] as? Int == 28 else { return raw }
        var older = raw
        switch id {
        case sizingId:
            return nil
        case sizeId:
            older["requiresDescriptionVersion"] = 4
            older.removeValue(forKey: "enabledWhen")
        case targetId:
            older.removeValue(forKey: "requiresDescriptionVersion")
            older.removeValue(forKey: "enabledWhen")
            older["tooltip"] = targetV26Tooltip
            older["min"] = 0
            older["default"] = 0
        case _ where averageIds.contains(id):
            older["requiresDescriptionVersion"] = 8
            older["min"] = 10
            older["step"] = 10
        default:
            break
        }
        return older
    }

    /// The whole Display document at version 26.
    static func document(_ root: [String: Any]) -> [String: Any] {
        var older = root
        older["version"] = 26
        older["pages"] = ((root["pages"] as? [[String: Any]]) ?? []).map { page -> [String: Any] in
            var page = page
            page["sections"] = ((page["sections"] as? [[String: Any]]) ?? []).map { section -> [String: Any] in
                var section = section
                section["controls"] = ((section["controls"] as? [[String: Any]]) ?? []).compactMap(row)
                return section
            }
            return page
        }
        return older
    }
}

/// V28 (the FFT sizing chooser): Spectrum Defaults says how the FFT size is
/// chosen. Sizing is a choice on a Core setting that holds a word; Size is
/// live only in Fixed and Hz/bin Target only in Hz/bin; the two averaging
/// times go down to 1 ms.
@Suite struct SetupDescriptionV28Tests {
    typealias Sizing = StationCatalog.Display.FftSizing

    /// The Core's Display file as it is.
    static func latest() throws -> [String: Any] {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/legacy/display-v28.json").standardizedFileURL
        return try #require(JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
    }

    static func raw(_ id: String) throws -> [String: Any] {
        try #require(SetupDescriptionV12Tests.rawControls(try latest()).first { $0["id"] as? String == id })
    }

    static func single(_ raw: [String: Any], version: Int = 28) throws -> SetupDescription.Control {
        try SetupDescriptionV12Tests.single(raw, category: "display", version: version)
    }

    static let ids = [DisplayV26Fixture.sizingId, DisplayV26Fixture.sizeId, DisplayV26Fixture.targetId]
        + DisplayV26Fixture.averageIds.sorted()

    @Test func theCoresV28DisplayParsesWholeWithSizingAboveSize() throws {
        let display = try SetupDescriptionV12Tests.parse(try Self.latest())
        #expect(display.version == 28)
        let all = SetupDescriptionV12Tests.controls(display)
        #expect(all.count == 101)
        let issues = all.compactMap { control in control.metadataIssue.map { "\(control.id): \($0)" } }
        #expect(issues.isEmpty, "\(issues)")
        #expect(all.filter { $0.requiresDescriptionVersion == 28 }.map(\.id).sorted() == Self.ids.sorted())
        // Sizing, Size, Hz/bin Target, Window: the desktop page's order.
        let fft = try #require(display.pages[0].sections.first { $0.title == "Fast Fourier Transform" })
        #expect(fft.controls.map(\.id) == [DisplayV26Fixture.sizingId, DisplayV26Fixture.sizeId,
                                           DisplayV26Fixture.targetId, "display.spectrumDefaults.window"])
    }

    @Test func sizingIsAChoiceOfWordsOnTheCoresSetting() throws {
        let sizing = try Self.single(try Self.raw(DisplayV26Fixture.sizingId))
        #expect(sizing.metadataIssue == nil && sizing.isEditable)
        #expect(sizing.kind == .choice && sizing.binding == .setting(Sizing.settingsKey))
        #expect(sizing.applies == .subscription && sizing.label == "Sizing")
        #expect(sizing.options?.map(\.value) == [0, 1, 2])
        #expect(sizing.options?.map(\.label) == ["Auto", "Hz/bin", "Fixed"])
        #expect(sizing.modern?.optionLiterals == [0: .text("Auto"), 1: .text("HzPerBin"), 2: .text("Fixed")])
        #expect(sizing.defaultValue == .text("Auto"))
        #expect(sizing.enabledWhen == nil && sizing.gate == nil)
        // The words are read and written as the Core stores them.
        for (index, mode) in Sizing.allCases.enumerated() {
            #expect(SetupValueCoding.decodeSetting(mode.rawValue, for: sizing) == .integer(Int64(index)))
            #expect(SetupValueCoding.encodeSetting(.integer(Int64(index)), for: sizing) == mode.rawValue)
        }
        #expect(SetupValueCoding.decodeSetting("Manual", for: sizing) == nil)
        #expect(SetupValueCoding.decodeSetting("1", for: sizing) == nil)
        #expect(SetupValueCoding.encodeSetting(.integer(3), for: sizing) == nil)
    }

    @Test func sizeAndTheTargetAreGatedOnSizing() throws {
        let size = try Self.single(try Self.raw(DisplayV26Fixture.sizeId))
        #expect(size.metadataIssue == nil)
        #expect(size.kind == .slider && size.binding == .setting("DisplayFftSize"))
        #expect(size.options?.map(\.value) == Sizing.fixedSizes.map(Int64.init))
        #expect(size.defaultValue == .integer(4096))
        #expect(size.enabledWhen == .init(setting: Sizing.settingsKey, oneOf: ["Fixed"]))
        let target = try Self.single(try Self.raw(DisplayV26Fixture.targetId))
        #expect(target.metadataIssue == nil)
        #expect(target.kind == .decimal && target.binding == .setting("DisplayHzPerBinTarget"))
        // 0 ("Off") is no longer a choice: Sizing says who is in charge.
        #expect(target.range == .init(minimum: 0.5, maximum: 200, step: 0.5))
        #expect(target.decimals == 2 && target.unit == "Hz/bin" && target.defaultValue == .integer(200))
        #expect(target.enabledWhen == .init(setting: Sizing.settingsKey, oneOf: ["HzPerBin"]))
    }

    @Test func theAveragingTimesGoDownToOneMillisecond() throws {
        for (id, key, value) in [("display.spectrumDefaults.averageTime", "DisplaySpectrumAverageTimeMs", 30),
                                 ("display.waterfallDefaults.averageTime", "DisplayWaterfallAverageTimeMs", 120)] {
            let row = try Self.single(try Self.raw(id))
            #expect(row.metadataIssue == nil && row.isEditable, "\(id)")
            #expect(row.kind == .integer && row.binding == .phone(key) && row.unit == "ms", "\(id)")
            #expect(row.range == .init(minimum: 1, maximum: 9999, step: 1), "\(id)")
            #expect(row.defaultValue == .integer(Int64(value)), "\(id)")
            #expect(row.gate?.capability == "displayExtrasVersion" && row.gate?.minimum == 1, "\(id)")
        }
    }

    @Test func everyV28RowRefusesAnyAlterationAndAnOlderDocument() throws {
        for id in Self.ids {
            let raw = try Self.raw(id)
            #expect(raw["requiresDescriptionVersion"] as? Int == 28, "\(id)")
            #expect(try Self.single(raw).metadataIssue == nil, "\(id)")
            var alterations: [[String: Any]] = []
            var bad = raw; bad["applies"] = "live"; alterations.append(bad)
            bad = raw; bad["binding"] = ["setting": "Other"]; alterations.append(bad)
            bad = raw; bad["surprise"] = true; alterations.append(bad)
            bad = raw; bad["requiresDescriptionVersion"] = 27; alterations.append(bad)
            bad = raw; bad["requiresDescriptionVersion"] = 12; alterations.append(bad)
            bad = raw; bad.removeValue(forKey: "requiresDescriptionVersion"); alterations.append(bad)
            bad = raw; bad.removeValue(forKey: "default"); alterations.append(bad)
            bad = raw; bad["confirm"] = "Sure?"; alterations.append(bad)
            if raw["enabledWhen"] != nil {
                bad = raw; bad.removeValue(forKey: "enabledWhen"); alterations.append(bad)
                bad = raw; bad["enabledWhen"] = ["setting": Sizing.settingsKey, "oneOf": ["Auto"]]
                alterations.append(bad)
                bad = raw; bad["enabledWhen"] = ["setting": "DisplayFftWindow", "oneOf": ["Fixed"]]
                alterations.append(bad)
                bad = raw; bad["enabledWhen"] = ["setting": Sizing.settingsKey, "oneOf": ["Fixed", "HzPerBin"]]
                alterations.append(bad)
                bad = raw; bad["enabledWhen"] = ["phone": Sizing.settingsKey, "oneOf": ["Fixed"]]
                alterations.append(bad)
            } else {
                bad = raw; bad["enabledWhen"] = ["setting": Sizing.settingsKey, "oneOf": ["Fixed"]]
                alterations.append(bad)
            }
            if raw["min"] != nil {
                bad = raw; bad["min"] = 0; alterations.append(bad)
                bad = raw; bad["step"] = 10; alterations.append(bad)
                bad = raw; bad["max"] = 500; alterations.append(bad)
                bad = raw; bad["unit"] = "s"; alterations.append(bad)
            }
            // The form a phone below 28 is sent is not a version 28 row.
            if let older = DisplayV26Fixture.row(raw) {
                alterations.append(older)
            }
            for altered in alterations {
                let parsed = try Self.single(altered)
                #expect(parsed.metadataIssue != nil && !parsed.isEditable, "\(id): \(altered)")
            }
            // A document below 28 cannot carry it.
            for version in [12, 26, 27] {
                let parsed = try Self.single(raw, version: version)
                #expect(parsed.metadataIssue != nil && !parsed.isEditable, "\(id) v\(version)")
            }
        }
    }

    @Test func sizingsOptionsAreClosed() throws {
        let raw = try Self.raw(DisplayV26Fixture.sizingId)
        let options = try #require(raw["options"] as? [[String: Any]])
        var changes: [(String, Any)] = []
        var altered = options; altered.swapAt(0, 2); changes.append(("reordered", altered))
        altered = options; altered.removeLast(); changes.append(("one missing", altered))
        altered = options; altered.append(["value": "Manual", "label": "Manual"]); changes.append(("one added", altered))
        altered = options; altered[1]["value"] = "Hz/bin"; changes.append(("another word", altered))
        altered = options; altered[1]["label"] = "Bin width"; changes.append(("another label", altered))
        altered = options; altered[0]["value"] = 0; changes.append(("a number for a word", altered))
        altered = options; altered[0]["note"] = "x"; changes.append(("another key", altered))
        changes.append(("not a list", "Auto"))
        for (what, value) in changes {
            var bad = raw
            bad["options"] = value
            let parsed = try Self.single(bad)
            #expect(parsed.metadataIssue != nil && !parsed.isEditable, "\(what)")
        }
        var bad = raw
        bad["default"] = "Fixed"
        #expect(try Self.single(bad).metadataIssue != nil)
        bad["default"] = 0
        #expect(try Self.single(bad).metadataIssue != nil)
        // No other Display row may hold words: Window keeps its numbers.
        var window = try Self.raw("display.spectrumDefaults.window")
        window.removeValue(forKey: "choices")
        window["options"] = options
        #expect(try Self.single(window).metadataIssue != nil)
    }

    /// A phone build reads version 26 and 27 descriptions as before.
    @Test func aPhoneBelowTwentyEightIsSentTheEarlierRows() throws {
        let display = try SetupDescriptionV12Tests.parse(DisplayV26Fixture.document(try Self.latest()))
        #expect(display.version == 26)
        let all = SetupDescriptionV12Tests.controls(display)
        #expect(all.count == 100)
        #expect(all.allSatisfy { $0.metadataIssue == nil })
        #expect(!all.contains { $0.id == DisplayV26Fixture.sizingId })
        let size = try #require(all.first { $0.id == DisplayV26Fixture.sizeId })
        #expect(size.enabledWhen == nil && size.requiresDescriptionVersion == 4)
        let target = try #require(all.first { $0.id == DisplayV26Fixture.targetId })
        #expect(target.enabledWhen == nil && target.requiresDescriptionVersion == nil)
        #expect(target.range == .init(minimum: 0, maximum: 200, step: 0.5) && target.defaultValue == .integer(0))
        #expect(target.tooltip == DisplayV26Fixture.targetV26Tooltip)
        for id in DisplayV26Fixture.averageIds {
            let row = try #require(all.first { $0.id == id })
            #expect(row.range == .init(minimum: 10, maximum: 9999, step: 10), "\(id)")
            #expect(row.requiresDescriptionVersion == 8, "\(id)")
        }
        // The same rows in a document at 27, and each alone at 12.
        var at27 = DisplayV26Fixture.document(try Self.latest())
        at27["version"] = 27
        #expect(SetupDescriptionV12Tests.controls(try SetupDescriptionV12Tests.parse(at27))
            .allSatisfy { $0.metadataIssue == nil })
        for id in Self.ids where id != DisplayV26Fixture.sizingId {
            let older = try #require(DisplayV26Fixture.row(try Self.raw(id)))
            #expect(try Self.single(older, version: 12).metadataIssue == nil, "\(id)")
        }
    }

    @MainActor
    @Test func versionTwentyEightStaysClosedBelowTheHighest() throws {
        #expect(SetupDescription.highestVersion == 30)
        #expect(SetupDescription.v28Version == 28)
        #expect(LinkFeatures.app["setupDescription"] == 30)
        var root = try Self.latest()
        root["version"] = 31
        #expect(throws: SetupDescription.ParseError.self) { try SetupDescriptionV12Tests.parse(root) }
        for (cap, version) in [12: 12, 25: 12, 26: 26, 27: 26, 28: 28] as [Int64: Int] {
            #expect(SetupDescriptionFeed.projectedVersion(maximum: cap, category: "display") == version, "\(cap)")
        }
        #expect(SetupDescriptionFeed.projectedVersion(maximum: 31, category: "display") == nil)
        // The two settings are the Core's, so the phone may write them.
        #expect(SettingsScope.of(Sizing.settingsKey) == .station)
        #expect(SettingsScope.of(Sizing.rememberedTargetKey) == .station)
    }
}

/// V28 through the Setup dispatcher: the mode a Core that never wrote the
/// choice is read as, the two rows it gates and what they show while
/// grayed, and the two writes a choice makes.
@MainActor
@Suite struct SetupControlDispatcherV28Tests {
    typealias Rig = SetupControlDispatcherV12Tests.Rig
    typealias Sizing = StationCatalog.Display.FftSizing

    static func rig(_ stored: [String: String], version: Int64 = 28) async throws -> Rig {
        let rig = Rig()
        try await rig.connect(version: version, stored: stored)
        return rig
    }

    func state(_ rig: Rig, _ id: String) throws -> SetupControlState {
        rig.dispatcher.state(of: try rig.control(id), in: "display")
    }

    /// Answers the write at `index` as the Core does, and returns it.
    func accept(_ rig: Rig, _ index: Int) throws -> (key: String, text: String) {
        guard case .settingsWrite(let write) = rig.route.sent[index].message,
              case .utf8(let text)? = write.properties.first?.value else {
            throw SetupDescription.ParseError("no settings write at \(index)")
        }
        rig.settings.apply(.settingsValue(.init(key: write.key, origin: write.origin, properties: write.properties)))
        return (write.key, text)
    }

    @Test func theModeIsReadByTheSharedRuleWhenTheCoreNeverWroteIt() async throws {
        // No choice stored, no target: Auto, and neither row is in charge.
        var rig = try await Self.rig(["DisplayFftSize": "4096", "DisplayHzPerBinTarget": "0"])
        #expect(rig.feed.description(for: "display")?.version == 28)
        #expect(rig.dispatcher.sizingMode == .auto)
        var sizing = try state(rig, DisplayV26Fixture.sizingId)
        #expect(sizing == .init(value: .integer(0), editable: true, reason: nil))
        #expect(try state(rig, DisplayV26Fixture.sizeId).reason == SetupControlDispatcher.dependsReason)
        #expect(try state(rig, DisplayV26Fixture.targetId).reason == SetupControlDispatcher.dependsReason)
        // No choice stored, a target above 0: Hz/bin, as that Core runs.
        rig = try await Self.rig(["DisplayFftSize": "4096", "DisplayHzPerBinTarget": "12.5"])
        #expect(rig.dispatcher.sizingMode == .hzPerBin)
        sizing = try state(rig, DisplayV26Fixture.sizingId)
        #expect(sizing == .init(value: .integer(1), editable: true, reason: nil))
        #expect(try state(rig, DisplayV26Fixture.targetId) == .init(value: .decimal(12.5), editable: true, reason: nil))
        #expect(try state(rig, DisplayV26Fixture.sizeId).reason == SetupControlDispatcher.dependsReason)
        // "HzPerBin" stored with no target runs as Auto, so it reads as Auto.
        rig = try await Self.rig(["DisplayFftSize": "4096", "DisplayHzPerBinTarget": "0",
                                  Sizing.settingsKey: "HzPerBin"])
        #expect(rig.dispatcher.sizingMode == .auto)
        #expect(try state(rig, DisplayV26Fixture.sizingId).value == .integer(0))
        // A word this phone does not know reads by the same rule.
        rig = try await Self.rig(["DisplayFftSize": "4096", "DisplayHzPerBinTarget": "3",
                                  Sizing.settingsKey: "Manual"])
        #expect(try state(rig, DisplayV26Fixture.sizingId).value == .integer(1))
        // Fixed: Size is in charge, a stray target or not.
        rig = try await Self.rig(["DisplayFftSize": "32768", "DisplayHzPerBinTarget": "12.5",
                                  Sizing.settingsKey: "Fixed"])
        #expect(try state(rig, DisplayV26Fixture.sizingId).value == .integer(2))
        #expect(try state(rig, DisplayV26Fixture.sizeId) == .init(value: .integer(32768), editable: true, reason: nil))
        #expect(try state(rig, DisplayV26Fixture.targetId).reason == SetupControlDispatcher.dependsReason)
    }

    @Test func aCoreThatStoredNeitherSizeNorTargetStillGraysAndFreesTheRows() async throws {
        // The Core sends only what it has stored; a new one holds none of the three keys.
        let rig = try await Self.rig([:])
        #expect(rig.settings.value(Sizing.sizeKey) == nil && rig.settings.value(Sizing.targetKey) == nil)
        #expect(rig.dispatcher.sizingMode == .auto)
        rig.dispatcher.runningFftSize = { 16_384 }
        rig.dispatcher.runningBinWidthHz = { 192_000.0 / 16_384 }
        // Auto: both rows are gray by Sizing and show what runs.
        #expect(try state(rig, DisplayV26Fixture.sizeId)
                == .init(value: .integer(16_384), editable: false, reason: SetupControlDispatcher.dependsReason))
        #expect(try state(rig, DisplayV26Fixture.targetId)
                == .init(value: .decimal(11.71875), editable: false, reason: SetupControlDispatcher.dependsReason))
        #expect(await rig.dispatcher.edit(try rig.control(DisplayV26Fixture.sizeId), in: "display", to: .integer(8192))
                == .notSent(SetupControlDispatcher.dependsReason))
        #expect(rig.route.sent.isEmpty)
        // Fixed with no size stored: Size is free at the built-in size.
        rig.settings.apply(.settingsValue(.init(key: Sizing.settingsKey, origin: "core",
                                                properties: [.init(name: Sizing.settingsKey, value: .utf8("Fixed"))])))
        #expect(try state(rig, DisplayV26Fixture.sizeId) == .init(value: .integer(4096), editable: true, reason: nil))
        #expect(try state(rig, DisplayV26Fixture.targetId).reason == SetupControlDispatcher.dependsReason)
        // A description below 28 reads a key the Core never stored as before.
        let older = try await Self.rig([:], version: 27)
        #expect(try state(older, DisplayV26Fixture.sizeId).reason == SetupControlDispatcher.valueMissingReason)
        #expect(try state(older, DisplayV26Fixture.targetId).reason == SetupControlDispatcher.valueMissingReason)
    }

    @Test func aGrayedRowShowsWhatRunsNotWhatIsStored() async throws {
        let rig = try await Self.rig(["DisplayFftSize": "65536", "DisplayHzPerBinTarget": "0",
                                      Sizing.settingsKey: "Auto"])
        // No pan runs yet: no size to show, and no target but never 0.
        #expect(try state(rig, DisplayV26Fixture.sizeId).value == nil)
        #expect(try state(rig, DisplayV26Fixture.targetId).value == nil)
        // The pan runs 16384 at 192 kHz: Size shows it, not the stored
        // 65536; the target shows the running bin width.
        rig.dispatcher.runningFftSize = { 16_384 }
        rig.dispatcher.runningBinWidthHz = { 192_000.0 / 16_384 }
        var size = try state(rig, DisplayV26Fixture.sizeId)
        #expect(size == .init(value: .integer(16_384), editable: false, reason: SetupControlDispatcher.dependsReason))
        var target = try state(rig, DisplayV26Fixture.targetId)
        #expect(target == .init(value: .decimal(11.71875), editable: false, reason: SetupControlDispatcher.dependsReason))
        // A remembered target is shown before the running width.
        rig.settings.apply(.settingsValue(.init(key: Sizing.rememberedTargetKey, origin: "core",
                                                properties: [.init(name: Sizing.rememberedTargetKey, value: .utf8("12.5"))])))
        target = try state(rig, DisplayV26Fixture.targetId)
        #expect(target.value == .decimal(12.5) && !target.editable)
        // A grayed row takes no edit.
        #expect(await rig.dispatcher.edit(try rig.control(DisplayV26Fixture.sizeId), in: "display", to: .integer(8192))
                == .notSent(SetupControlDispatcher.dependsReason))
        #expect(await rig.dispatcher.edit(try rig.control(DisplayV26Fixture.targetId), in: "display", to: .decimal(5))
                == .notSent(SetupControlDispatcher.dependsReason))
        #expect(rig.route.sent.isEmpty)
        // A running size the Size row does not offer is not shown as one.
        rig.dispatcher.runningFftSize = { 2048 }
        size = try state(rig, DisplayV26Fixture.sizeId)
        #expect(size.value == nil && !size.editable)
    }

    /// Choosing a mode writes the choice, then the target, in that order.
    @Test func aChoiceWritesTheModeThenTheTarget() async throws {
        let rig = try await Self.rig(["DisplayFftSize": "4096", "DisplayHzPerBinTarget": "0",
                                      Sizing.settingsKey: "Auto"])
        rig.dispatcher.runningBinWidthHz = { 192_000.0 / 8192 }
        let sizing = try rig.control(DisplayV26Fixture.sizingId)
        // Into Hz/bin with nothing remembered: the running 23.4375 Hz
        // rounded up to the step, so the size does not double.
        var choosing = Task { await rig.dispatcher.edit(sizing, in: "display", to: .integer(1)) }
        #expect(await rig.route.waitForCount(1, unless: choosing))
        var write = try accept(rig, 0)
        #expect(write.key == Sizing.settingsKey && write.text == "HzPerBin")
        #expect(await rig.route.waitForCount(2, unless: choosing))
        write = try accept(rig, 1)
        #expect(write.key == "DisplayHzPerBinTarget" && write.text == "23.5")
        #expect(await choosing.value == .applied)
        #expect(rig.dispatcher.sizingMode == .hzPerBin)
        #expect(try state(rig, DisplayV26Fixture.targetId) == .init(value: .decimal(23.5), editable: true, reason: nil))
        // Into Fixed: the choice, then 0.
        let fixed = try rig.control(DisplayV26Fixture.sizingId)
        choosing = Task { await rig.dispatcher.edit(fixed, in: "display", to: .integer(2)) }
        #expect(await rig.route.waitForCount(3, unless: choosing))
        write = try accept(rig, 2)
        #expect(write.key == Sizing.settingsKey && write.text == "Fixed")
        #expect(await rig.route.waitForCount(4, unless: choosing))
        write = try accept(rig, 3)
        #expect(write.key == "DisplayHzPerBinTarget" && write.text == "0")
        #expect(await choosing.value == .applied)
        #expect(try state(rig, DisplayV26Fixture.sizeId).editable)
        // Back into Hz/bin: the remembered target, not the running width.
        rig.settings.apply(.settingsValue(.init(key: Sizing.rememberedTargetKey, origin: "core",
                                                properties: [.init(name: Sizing.rememberedTargetKey, value: .utf8("12.5"))])))
        let again = try rig.control(DisplayV26Fixture.sizingId)
        choosing = Task { await rig.dispatcher.edit(again, in: "display", to: .integer(1)) }
        #expect(await rig.route.waitForCount(5, unless: choosing))
        write = try accept(rig, 4)
        #expect(write.key == Sizing.settingsKey && write.text == "HzPerBin")
        #expect(await rig.route.waitForCount(6, unless: choosing))
        write = try accept(rig, 5)
        #expect(write.key == "DisplayHzPerBinTarget" && write.text == "12.5")
        #expect(await choosing.value == .applied)
        // Into Auto: the choice, then 0.
        let auto = try rig.control(DisplayV26Fixture.sizingId)
        choosing = Task { await rig.dispatcher.edit(auto, in: "display", to: .integer(0)) }
        #expect(await rig.route.waitForCount(7, unless: choosing))
        write = try accept(rig, 6)
        #expect(write.key == Sizing.settingsKey && write.text == "Auto")
        #expect(await rig.route.waitForCount(8, unless: choosing))
        write = try accept(rig, 7)
        #expect(write.key == "DisplayHzPerBinTarget" && write.text == "0")
        #expect(await choosing.value == .applied)
        #expect(rig.dispatcher.sizingMode == .auto)
        #expect(rig.route.sent.count == 8)
    }

    @Test func intoHzPerBinWithNoWidthStartsAtTheCoarsestTarget() async throws {
        let rig = try await Self.rig(["DisplayFftSize": "4096", "DisplayHzPerBinTarget": "0"])
        let sizing = try rig.control(DisplayV26Fixture.sizingId)
        let choosing = Task { await rig.dispatcher.edit(sizing, in: "display", to: .integer(1)) }
        #expect(await rig.route.waitForCount(1, unless: choosing))
        _ = try accept(rig, 0)
        #expect(await rig.route.waitForCount(2, unless: choosing))
        let write = try accept(rig, 1)
        #expect(write.key == "DisplayHzPerBinTarget" && write.text == "200")
        #expect(await choosing.value == .applied)
    }

    @Test func theTargetWrittenAndShownAreTheSharedRules() {
        #expect(Sizing.auto.targetWritten(remembered: 12.5, runningBinWidthHz: 23.4) == 0)
        #expect(Sizing.fixed.targetWritten(remembered: 12.5, runningBinWidthHz: 23.4) == 0)
        #expect(Sizing.hzPerBin.targetWritten(remembered: 12.5, runningBinWidthHz: 23.4) == 12.5)
        #expect(Sizing.hzPerBin.targetWritten(remembered: 0, runningBinWidthHz: 23.4375) == 23.5)
        #expect(Sizing.hzPerBin.targetWritten(remembered: nil, runningBinWidthHz: nil) == 200)
        #expect(Sizing.hzPerBin.targetWritten(remembered: .nan, runningBinWidthHz: 0.1) == 0.5)
        #expect(Sizing.targetShown(remembered: 12.5, runningBinWidthHz: 23.4) == 12.5)
        #expect(Sizing.targetShown(remembered: 0, runningBinWidthHz: 23.4) == 23.4)
        #expect(Sizing.targetShown(remembered: nil, runningBinWidthHz: 0) == nil)
        #expect(Sizing.targetShown(remembered: nil, runningBinWidthHz: nil) == nil)
    }

    /// Against a Core that sends 26, the rows are as they were: Size and
    /// the target both live, the target from 0, and no Sizing row.
    @Test func anOlderCoresRowsBehaveAsBefore() async throws {
        let rig = try await Self.rig(["DisplayFftSize": "65536", "DisplayHzPerBinTarget": "0"], version: 26)
        #expect(rig.feed.description(for: "display")?.version == 26)
        let sent = try #require(rig.feed.description(for: "display"))
        #expect(!SetupDescriptionV12Tests.controls(sent).contains { $0.id == DisplayV26Fixture.sizingId })
        rig.dispatcher.runningFftSize = { 16_384 }
        rig.dispatcher.runningBinWidthHz = { 11.7 }
        #expect(try state(rig, DisplayV26Fixture.sizeId) == .init(value: .integer(65_536), editable: true, reason: nil))
        #expect(try state(rig, DisplayV26Fixture.targetId) == .init(value: .decimal(0), editable: true, reason: nil))
        let target = try rig.control(DisplayV26Fixture.targetId)
        let setting = Task { await rig.dispatcher.edit(target, in: "display", to: .decimal(5)) }
        #expect(await rig.route.waitForCount(1, unless: setting))
        let write = try accept(rig, 0)
        #expect(write.key == "DisplayHzPerBinTarget" && write.text == "5")
        #expect(await setting.value == .applied)
        #expect(rig.route.sent.count == 1)
    }
}
