// NereusSDR for iOS: the Setup pages the Core describes: their places in the tree, every kind drawn, each control's owner, and pictures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

/// R-IOS-18 (Task 58, step 1): the Core's described pages join the app's
/// own in the Setup tree, in the desktop's order and marks; a page the
/// fake Core adds appears in its place with no change to the app; every
/// generic kind is drawn as a visible control; each control writes to its
/// owner; a control that cannot change now is shown greyed with its
/// reason. With `NEREUS_MAIN_SHOTS` set to a directory (through
/// `TEST_RUNNER_NEREUS_MAIN_SHOTS`), the pages are written there as PNGs.
@Suite("Setup, described by the Core", .serialized)
@MainActor
struct SetupDescribedPagesTests {
    // MARK: The Core's own descriptions, read from the checkout at run time

    /// The Core's category files projected for a version-11 session, as the
    /// Core projects them: later controls dropped, the attenuator range
    /// filled for one board, each category at its own version.
    static func coreCategories(peer: Int = 11) throws -> [(id: String, json: String)] {
        let folder = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent().deletingLastPathComponent().appendingPathComponent("resources/setup")
        let ceilings = ["hardware": 6, "pa": 5, "appearance": peer >= 12 ? 12 : 7, "display": min(peer, 12)]
        return try FakeStation.setupCategoryIds.map { id in
            let data = try Data(contentsOf: folder.appendingPathComponent("\(id).json"))
            var root = try #require(JSONSerialization.jsonObject(with: data) as? [String: Any])
            var pages: [[String: Any]] = []
            for var page in root["pages"] as? [[String: Any]] ?? [] {
                var sections: [[String: Any]] = []
                for var section in page["sections"] as? [[String: Any]] ?? [] {
                    var controls: [[String: Any]] = []
                    for var control in section["controls"] as? [[String: Any]] ?? [] {
                        if id == "hardware", peer < 18, let controlId = control["id"] as? String,
                           let closed = hardwareV16ClockRows[controlId] {
                            // A peer below 18 gets the version 16 clock row, closed.
                            control = closed
                        }
                        if id == "audio", peer < 24, control["id"] as? String == "audio.txInput.hermesLineInGain" {
                            // A peer below 24 keeps the version 15 Line In Gain: whole decibels from -34.
                            control["requiresDescriptionVersion"] = 15
                            control["min"] = -34
                            control["step"] = 1
                            control.removeValue(forKey: "decimals")
                        }
                        if (control["requiresDescriptionVersion"] as? Int ?? 1) > peer { continue }
                        // The Core fills availableOn for its radio; on a radio it
                        // applies to, the row comes with no availability.
                        control.removeValue(forKey: "availableOn")
                        if id == "catNetwork", peer < 21 {
                            // CAT & Network 21 greys TCI's Forget row out while
                            // Duplicate is off: below 21 the row is always enabled.
                            control.removeValue(forKey: "enabledWhen")
                        }
                        if control.removeValue(forKey: "rangeSource") != nil {
                            control["min"] = 0
                            control["max"] = 31
                            control["step"] = 1
                        }
                        controls.append(control)
                    }
                    if !controls.isEmpty {
                        section["controls"] = controls
                        sections.append(section)
                    }
                }
                if !sections.isEmpty {
                    page["sections"] = sections
                    // The version 19 wording (DSP's CFC editor) goes only to a peer at 19.
                    if let coverage = page.removeValue(forKey: "coverageV19") as? String, peer >= 19 {
                        if coverage.isEmpty { page.removeValue(forKey: "coverage") } else { page["coverage"] = coverage }
                    }
                    pages.append(page)
                }
            }
            root["pages"] = pages
            if let coverage = root.removeValue(forKey: "coverageV19") as? String, peer >= 19 {
                root["coverage"] = coverage
            }
            // From 13 on each category keeps the version its file carries,
            // raised to the Core's floor of 3 as the Core sends it.
            if peer < 13 {
                root["version"] = ceilings[id] ?? 3
            } else if id == "hardware" {
                // Hardware changed at 13, 16, 17, 18 (the HL2 clock rows) and 23
                // (Calibration's Level Cal section).
                root["version"] = peer < 16 ? 13 : peer < 23 ? min(peer, 18) : 23
            } else if id == "dsp" {
                // DSP changed at 15, 19 (CFC's band editor) and 22 (the RX
                // buffer rows on the air): 15 to 18 see 15, 19 to 21 see 19.
                root["version"] = peer < 19 ? 15 : peer < 22 ? 19 : 22
            } else if id == "audio" {
                // Audio changed at 15 and 24 (Line In Gain's 1.5 dB steps, the
                // Saturn G2's Mic Tip-Ring): 13 to 23 see 15.
                root["version"] = peer < 24 ? 15 : 24
            } else if id == "pa", peer >= 20 {
                // PA changed at 20 (the profile rows on the air).
                root["version"] = 20
            } else if id == "catNetwork", peer < 21 {
                // CAT & Network changed at 15 and 21 (TCI Forget's enabledWhen):
                // 15 to 20 see 15.
                root["version"] = min(root["version"] as? Int ?? 3, 15)
            } else {
                root["version"] = max(root["version"] as? Int ?? 3, ceilings[id] ?? 3)
            }
            let json = try JSONSerialization.data(withJSONObject: root)
            return (id, String(decoding: json, as: UTF8.self))
        }
    }

    /// The three HL2 clock rows as the Core sends them to a peer below
    /// description 18: its version 16 rows, closed with its reason. The
    /// resource file holds the version 18 rows.
    static var hardwareV16ClockRows: [String: [String: Any]] {
        let closed: [String: Any] = ["enabled": false, "reason": "NereusSDR does not change the radio's clock settings."]
        let gate: [String: Any] = ["capability": "transmitSettingsVersion", "min": 8]
        let flag: [String: Any] = ["true": "True", "false": "False"]
        return [
            "hardware.hl2Io.cl2Enable": ["id": "hardware.hl2Io.cl2Enable", "label": "Enable CL2", "tooltip": "",
                                         "kind": "toggle", "binding": ["radioSetting": "hl2/cl2Enable"],
                                         "valueEncoding": flag, "applies": "live", "gate": gate,
                                         "requiresDescriptionVersion": 16, "default": false, "availability": closed],
            "hardware.hl2Io.cl2Freq": ["id": "hardware.hl2Io.cl2Freq", "label": "CL2 frequency", "tooltip": "",
                                       "kind": "integer", "binding": ["radioSetting": "hl2/cl2FreqMHz"],
                                       "applies": "live", "gate": gate, "requiresDescriptionVersion": 16,
                                       "min": 1, "max": 200, "step": 1, "unit": "MHz", "default": 116,
                                       "availability": closed],
            "hardware.hl2Io.ext10MHz": ["id": "hardware.hl2Io.ext10MHz", "label": "External 10 MHz reference",
                                        "tooltip": "", "kind": "toggle", "binding": ["radioSetting": "hl2/ext10MHz"],
                                        "valueEncoding": flag, "applies": "live", "gate": gate,
                                        "requiresDescriptionVersion": 16, "default": false, "availability": closed],
        ]
    }

    // MARK: The tree

    @Test("the described pages sit among the app's own, in the desktop's order, each category marked")
    func treeOrderAndMarks() throws {
        var described: [String: SetupDescription] = [:]
        for (id, json) in try Self.coreCategories() {
            described[id] = try SetupDescription.parse(json: json)
        }
        let tree = SetupTree.categories(described: described, order: FakeStation.setupCategoryIds, unreadable: [:])
        #expect(tree.map(\.title) == ["Devices", "General", "Hardware", "PA", "Audio", "DSP", "Display", "Transmit",
                                      "Appearance", "CAT & Network", "Test", "Diagnostics", "About this app"])
        #expect(tree.map(\.tag) == [.core, .both, .core, .core, .both, .core, .both, .both, .thisPhone, .both, .core,
                                    .both, .thisPhone])
        func pages(_ title: String) -> [String] { tree.first { $0.title == title }?.pages.map(\.title) ?? [] }
        #expect(pages("General") == ["Startup & Preferences", "Navigation", "Battery and sessions", "Options"])
        // A Core below description 12 lists 3D View greyed with its reason (the 3D View board).
        #expect(pages("Display") == ["On this phone", "Spectrum Defaults", "Spectrum Peaks", "Waterfall Defaults",
                                     "Multimeter", "TX Display", "3D View"])
        #expect(pages("Transmit") == ["Power", "DEXP/VOX", "PTT buttons"])
        #expect(pages("CAT & Network") == ["TCI Server", "Data use"])
        #expect(pages("Audio") == ["On this phone", "TX Profile"])
        #expect(pages("Appearance") == ["Colors & Theme", "Meter Styles"])
        // Each described page is marked where its settings live.
        let display = try #require(tree.first { $0.title == "Display" })
        #expect(display.pages.map(\.tag) == [.both, .both, .thisPhone, .thisPhone, .both, .both, .thisPhone])
        #expect(display.pages.map { $0.unavailableReason != nil } == [false, false, false, false, false, false, true])
        #expect(display.summary == "On this phone \u{00B7} Spectrum Defaults \u{00B7} Spectrum Peaks \u{00B7} Waterfall Defaults \u{00B7} Multimeter \u{00B7} TX Display \u{00B7} 3D View")
        // Nothing the phone leaves off, whatever a description says.
        let titles = tree.flatMap { $0.pages.map(\.title) }
        for gone in ["Keyboard", "Skins", "Collapsible Display", "Remote Access"] {
            #expect(!titles.contains(gone))
        }
        var sneaky = described
        let skins = FakeStation.syntheticSetup("appearance", title: "Colors & Theme", version: 7, extraPage: "Skins")
            .replacingOccurrences(of: #""where":"station"}"#, with: #""where":"phone"}"#)
        sneaky["appearance"] = try SetupDescription.parse(json: skins)
        let withSkins = SetupTree.categories(described: sneaky, order: FakeStation.setupCategoryIds, unreadable: [:])
        #expect(withSkins.first { $0.title == "Appearance" }?.pages.map(\.title) == ["Colors & Theme"])
        // A category the Core sent unreadable is shown with its reason, never guessed.
        let unreadable = SetupTree.categories(described: [:], order: ["dsp"],
                                              unreadable: ["dsp": SetupDescribedPages.unreadablePageReason])
        let dsp = try #require(unreadable.first { $0.title == "DSP" })
        #expect(dsp.pages.isEmpty && dsp.summary == SetupDescribedPages.unreadablePageReason)
    }

    @Test("every generic kind is drawn as a visible control, and the closed ones as a panel or a greyed row")
    func everyKindHasARow() throws {
        var rows: [String: DescribedControl.Row] = [:]
        for (id, json) in try Self.coreCategories() {
            let description = try SetupDescription.parse(json: json)
            for page in description.pages {
                for section in page.sections {
                    for control in section.controls {
                        #expect(control.metadataIssue == nil, "\(id) \(control.id)")
                        rows[control.rawKind] = try #require(DescribedControl.row(for: control))
                    }
                }
            }
        }
        #expect(rows["toggle"] == .switchRow)
        #expect(rows["integer"] == .numberRow && rows["decimal"] == .numberRow)
        #expect(rows["slider"] == .slider && rows["choice"] == .menu && rows["text"] == .textField)
        #expect(rows["colour"] == .colour && rows["button"] == .button && rows["readout"] == .reading)
        #expect(rows["table"] == .panel && rows["settingsHygiene"] == .panel)
        let generic = try SetupDescription.parse(json: #"{"version":3,"category":{"id":"general","title":"General","where":"station"},"pages":[{"id":"general.grid","title":"Grid","where":"station","sections":[{"title":"Grid","controls":[{"id":"general.grid.table","label":"Grid","tooltip":"","kind":"table","binding":{"setting":"SliceGrid"},"applies":"live"}]}]}]}"#)
        let table = generic.pages[0].sections[0].controls[0]
        #expect(DescribedControl.row(for: table) == .table)
        #expect(table.unavailableReason != nil)
        // A reading never shows a zero it was not sent.
        let reading = try SetupDescription.parse(json: #"{"version":3,"category":{"id":"pa","title":"PA","where":"station"},"pages":[{"id":"pa.values","title":"PA Values","where":"station","sections":[{"title":"PA","controls":[{"id":"pa.values.drive","label":"Drive","tooltip":"","kind":"readout","binding":{"property":{"object":"transmit","name":"power"}},"applies":"live","decimals":1,"unit":"W"}]}]}]}"#)
            .pages[0].sections[0].controls[0]
        #expect(DescribedControl.reading(nil, control: reading) == DescribedControl.unavailableText)
        #expect(DescribedControl.reading(.integer(0), control: reading) == "0.0 W")
        #expect(DescribedControl.reading(.decimal(12.345), control: reading) == "12.3 W")
    }

    // MARK: Against the fake Core

    @Test("a page the fake Core adds appears in its place, with no change to the app")
    func addedPageAppears() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        await rig.station.deliverSetup([
            ("general", FakeStation.syntheticSetup("general", title: "Options", version: 3)),
            ("dsp", ""),
        ])
        let pages = rig.app.setupPages
        #expect(await settle { pages.categories["general"] != nil && pages.isCurrent })
        func general() -> [String] {
            SetupTree.categories(described: pages.categories, order: pages.order, unreadable: pages.unreadable)
                .first { $0.title == "General" }?.pages.map(\.title) ?? []
        }
        #expect(general() == ["Navigation", "Battery and sessions", "Options"])
        #expect(!SetupTree.categories(described: pages.categories, order: pages.order, unreadable: pages.unreadable)
            .contains { $0.title == "DSP" }, "a category with no page is left out")
        await rig.station.publishSetup("general", json: FakeStation.syntheticSetup(
            "general", title: "Options", version: 3, extraPage: "Startup & Preferences"), revision: 2)
        #expect(await settle { general().count == 4 })
        #expect(general() == ["Startup & Preferences", "Navigation", "Battery and sessions", "Options"])
        try await Self.shoot("setup-described-general", rig: rig, path: [.category("General")])
        await rig.app.disconnect()
    }

    @Test("each control writes to its owner: a Core setting, a property, a verb, and this phone's own settings")
    func eachOwner() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        var categories = try Self.coreCategories()
        categories.removeAll { $0.id == "general" }
        categories.insert(("general", Self.ownersPage), at: 0)
        await rig.station.deliverSetup(categories)
        let app = rig.app
        let controls = app.setupControls
        #expect(await settle { app.setupPages.isCurrent && app.setupFeed.description(for: "display") != nil })
        func control(_ category: String, _ id: String) throws -> SetupDescription.Control {
            let description = try #require(app.setupFeed.description(for: category))
            return try #require(description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == id })
        }

        // A Core setting: the Core's value, then the exact string written.
        await rig.station.deliver(.settingsValue(.init(key: "MultimeterDelayMs", origin: "",
                                                       properties: [.init(name: "MultimeterDelayMs", value: .utf8("100"))])))
        let polling = try control("display", "display.multimeter.pollingDelay")
        #expect(await settle { controls.state(of: polling, in: "display").value == .integer(100) })
        #expect(controls.state(of: polling, in: "display") == .init(value: .integer(100), editable: true, reason: nil))
        let writing = Task { await controls.edit(polling, in: "display", to: .integer(250)) }
        let write = try await Self.settingsWrite("MultimeterDelayMs", rig.station)
        #expect(write.properties.first?.value == .utf8("250"))
        await rig.station.deliver(.settingsValue(.init(key: write.key, origin: write.origin, properties: write.properties)))
        #expect(await writing.value == .applied)

        // A property of a mirrored object.
        let power = try control("general", "general.owners.power")
        let current = try #require(controls.state(of: power, in: "general").value?.whole)
        let setting = Task { await controls.edit(power, in: "general", to: .integer(current == 40 ? 41 : 40)) }
        let propertyWrite = try await Self.propertyWrite("transmit", rig.station)
        #expect(propertyWrite.properties.first?.name == "power")
        _ = setting

        // A verb: the Core's refusal comes back in its own words.
        let preset = try control("general", "general.owners.preset")
        rig.station.refuseNext("sample.loadPreset", reason: "The radio is busy.")
        let pressed = await controls.edit(preset, in: "general", to: nil)
        #expect(pressed == .refused("The radio is busy."))
        #expect(rig.station.messages.contains { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == "sample.loadPreset" }
            return false
        })

        // This phone's own: V9 and V10 rows into this pan's settings, V7 into the S-meter.
        let band = app.main.band
        let opacity = try control("display", "display.waterfallDefaults.opacity")
        #expect(controls.state(of: opacity, in: "display").value == .integer(100))
        #expect(await controls.edit(opacity, in: "display", to: .integer(60)) == .applied)
        #expect(band.settings.waterfallOpacityPercent == 60)
        let alpha = try control("display", "display.spectrumDefaults.fillAlpha")
        #expect(await controls.edit(alpha, in: "display", to: .integer(25)) == .applied)
        #expect(abs(band.settings.traceFillOpacity - 0.25) < 1e-9)
        let overlay = try control("display", "display.waterfallDefaults.showTxFilter")
        #expect(controls.state(of: overlay, in: "display").value == .bool(true), "the phone's own default, on")
        #expect(await controls.edit(overlay, in: "display", to: .bool(false)) == .applied)
        #expect(!band.settings.showTxFilterOnWaterfall)
        let zero = try control("display", "display.waterfallDefaults.showRxZeroLine")
        #expect(await controls.edit(zero, in: "display", to: .bool(true)) == .applied)
        #expect(band.settings.showRxZeroLineOnWaterfall)
        let detector = try control("display", "display.waterfallDefaults.detector")
        // The fake's media has no detectors: the control follows its own gate.
        let refusedDetector = await controls.edit(detector, in: "display", to: .integer(2))
        #expect(refusedDetector == .notSent(SetupControlDispatcher.needsNewerCoreReason))
        #expect(await controls.edit(opacity, in: "display", to: .integer(101)) == .notSent(SetupControlDispatcher.outOfRangeReason))
        let kept = BandDisplaySettingsStore(defaults: rig.defaults).settings(forPan: BandSubscriber.panId)
        #expect(kept.waterfallOpacityPercent == 60 && !kept.showTxFilterOnWaterfall && kept.showRxZeroLineOnWaterfall)
        let face = try control("appearance", "appearance.meterStyles.face")
        #expect(await controls.edit(face, in: "appearance", to: .integer(3)) == .applied)
        #expect(app.main.sMeter.state.settings.face == .blackface)
        let grid = try control("appearance", "appearance.colorsTheme.gridColor")
        #expect(await controls.edit(grid, in: "appearance", to: .text("#10203040")) == .applied)
        #expect(band.settings.gridColour == "#10203040")
        let edge = try control("appearance", "appearance.colorsTheme.bandEdgeColor")
        #expect(controls.state(of: edge, in: "appearance").reason == SetupControlDispatcher.notOnThisPhoneReason)
        let rxFilter = try control("appearance", "appearance.colorsTheme.rxFilterColor")
        #expect(controls.state(of: rxFilter, in: "appearance").value == .text("#00B4D850"))

        // Away from the Core: the pages keep their places and last values, greyed with a reason.
        await rig.app.disconnect()
        #expect(await settle { !app.setupPages.isCurrent })
        #expect(app.setupPages.page("display.waterfallDefaults", in: "display") != nil)
        let away = controls.state(of: overlay, in: "display")
        #expect(away == .init(value: .bool(false), editable: false, reason: SetupControlDispatcher.notConnectedReason))
        #expect(controls.state(of: polling, in: "display").value == .integer(250))
        #expect(!controls.state(of: polling, in: "display").editable)
        try await Self.shoot("setup-described-disabled", rig: rig,
                             path: [.category("Display"), .described(category: "display", page: "display.multimeter")])
    }

    @Test("Spectrum Peaks keeps this phone's peak settings once, each row following its display extras gate")
    func spectrumPeaks() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        await rig.station.deliverSetup(try Self.coreCategories())
        let app = rig.app
        let controls = app.setupControls
        #expect(await settle { app.setupPages.isCurrent && app.setupFeed.description(for: "display") != nil })
        let page = try #require(app.setupFeed.description(for: "display")?.pages.first { $0.id == "display.spectrumPeaks" })
        #expect(page.title == "Spectrum Peaks" && page.whereOwned == .phone)
        #expect(page.sections.map(\.title) == ["Active Peak Hold", "Peak Blobs"])
        #expect(page.sections.flatMap(\.controls).count == 15)
        func control(_ suffix: String) throws -> SetupDescription.Control {
            try #require(page.sections.flatMap(\.controls).first { $0.id == "display.spectrumPeaks.\(suffix)" })
        }
        func capabilities(displayExtras: Int64) {
            var entries = app.mirror.capabilities.map { name, value in
                LinkMessage.PropertyEntry(ordinal: 0, name: name, value: value.wireValue)
            }
            entries.removeAll { $0.name == "displayExtrasVersion" }
            entries.append(.init(ordinal: 0, name: "displayExtrasVersion", value: .i64(displayExtras)))
            app.mirror.apply(.capabilities(LinkMessage.Capabilities(properties: entries)))
        }
        let enable = try control("activePeakHold")
        let time = try control("activePeakHoldTime")
        let onTx = try control("activePeakHoldOnTx")

        // The fake's Core computes no display extras: every row greyed with its reason, its value still shown.
        #expect(app.mirror.capabilityVersion("displayExtrasVersion") == 0)
        #expect(controls.state(of: enable, in: "display")
                == .init(value: .bool(false), editable: false, reason: SetupControlDispatcher.needsNewerCoreReason))
        #expect(await controls.edit(enable, in: "display", to: .bool(true))
                == .notSent(SetupControlDispatcher.needsNewerCoreReason))
        #expect(!app.main.band.settings.activePeakHold)

        // Version 1: all but Hold duration and Update during TX.
        capabilities(displayExtras: 1)
        #expect(await settle { controls.state(of: enable, in: "display").editable })
        #expect(controls.state(of: time, in: "display").reason == SetupControlDispatcher.needsNewerCoreReason)
        #expect(controls.state(of: onTx, in: "display").reason == SetupControlDispatcher.needsNewerCoreReason)
        try await Self.shoot("setup-described-display-peaks-older-core", rig: rig,
                             path: [.category("Display"), .described(category: "display", page: "display.spectrumPeaks")],
                             height: 1_900)

        // Version 3: every row, each into this phone's one kept copy.
        capabilities(displayExtras: 3)
        #expect(await settle { controls.state(of: time, in: "display").editable })
        let band = app.main.band
        let edits: [(String, SetupValue)] = [
            ("activePeakHold", .bool(true)), ("activePeakHoldTime", .integer(3500)),
            ("activePeakHoldDropRate", .integer(12)), ("activePeakHoldFill", .bool(true)),
            ("activePeakHoldOnTx", .bool(true)), ("activePeakHoldColor", .text("#11223344")),
            ("peakBlobs", .bool(true)), ("peakBlobCount", .integer(7)), ("peakBlobInsideFilter", .bool(true)),
            ("peakBlobHold", .bool(true)), ("peakBlobHoldTime", .integer(900)), ("peakBlobHoldDrop", .bool(true)),
            ("peakBlobFallRate", .integer(20)), ("peakBlobColor", .text("#55667788")),
            ("peakBlobTextColor", .text("#99AABBCC")),
        ]
        for (suffix, value) in edits {
            let row = try control(suffix)
            #expect(await controls.edit(row, in: "display", to: value) == .applied, "\(suffix)")
            #expect(controls.state(of: row, in: "display").value == value, "\(suffix)")
        }
        let settings = band.settings
        #expect(settings.activePeakHold && settings.activePeakHoldMs == 3500)
        #expect(settings.activePeakHoldFallDbPerSec == 12 && settings.activePeakHoldFill && settings.activePeakHoldOnTx)
        #expect(settings.peakHoldColour == "#11223344")
        #expect(settings.peakBlobs && settings.peakBlobCount == 7 && settings.peakBlobsInsideFilterOnly)
        #expect(settings.peakBlobHold && settings.peakBlobHoldMs == 900 && settings.peakBlobFall)
        #expect(settings.peakBlobFallDbPerSec == 20)
        #expect(settings.peakBlobColour == "#55667788" && settings.peakBlobTextColour == "#99AABBCC")
        // Out of the Core's range: refused, nothing kept.
        #expect(await controls.edit(time, in: "display", to: .integer(60_100))
                == .notSent(SetupControlDispatcher.outOfRangeReason))
        #expect(band.settings.activePeakHoldMs == 3500)
        // Kept once, as the desktop keeps these keys once for every pan.
        let kept = BandDisplaySettingsStore(defaults: rig.defaults).settings(forPan: BandSubscriber.panId)
        #expect(kept.activePeakHoldMs == 3500 && kept.activePeakHoldOnTx && kept.peakBlobCount == 7)
        #expect(kept.peakBlobTextColour == "#99AABBCC")
        try await Self.shoot("setup-described-display-peaks", rig: rig,
                             path: [.category("Display"), .described(category: "display", page: "display.spectrumPeaks")],
                             height: 1_900)
        await rig.app.disconnect()
    }

    // MARK: Description V12

    /// Raises the mirror's capabilities as a V12 Core with every display
    /// feature would send them.
    static func asV12Core(_ app: AppModel, displayExtras: Int64 = 4, setupDescription: Int64 = 12) {
        var entries = app.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(ordinal: 0, name: name, value: value.wireValue)
        }
        let raised: [String: Int64] = ["setupDescriptionVersion": setupDescription, "displayExtrasVersion": displayExtras,
                                       "txDisplayVersion": 1, "remoteMediaVersion": 1]
        entries.removeAll { raised[$0.name] != nil }
        for (name, value) in raised {
            entries.append(.init(ordinal: 0, name: name, value: .i64(value)))
        }
        app.mirror.apply(.capabilities(LinkMessage.Capabilities(properties: entries)))
    }

    @Test("V22: the phone asks for 22 and draws every V12 row; a V11 Core stays V11 with 3D View greyed")
    func v12DrawsEveryRow() async throws {
        #expect(LinkFeatures.app["setupDescription"] == 24)
        #expect(LinkFeatures.app["alexLpf"] == 1)
        #expect(LinkFeatures.app["cfcProfile"] == 1)
        #expect(SetupDescription.highestVersion == 24)
        #expect(SetupDescribedPages.notDrawnYet.isEmpty)
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        let controls = app.setupControls
        // First a V11 Core: Display and Appearance exactly as V11 sends them.
        await rig.station.deliverSetup(try Self.coreCategories())
        #expect(await settle { app.setupPages.isCurrent && app.setupFeed.description(for: "display")?.version == 11
            && app.setupPages.categories["display"] != nil })
        let v11 = try #require(app.setupPages.categories["display"])
        #expect(v11 == app.setupFeed.description(for: "display"), "nothing of V11 is left out")
        #expect(!v11.pages.contains { $0.id == "display.threeD" })
        #expect(app.setupPages.categories["appearance"]?.version == 7)
        #expect(!app.main.band.stackOffered)
        let olderTree = SetupTree.categories(described: app.setupPages.categories, order: app.setupPages.order,
                                             unreadable: app.setupPages.unreadable,
                                             stackOffered: app.main.band.stackOffered)
        let olderDisplay = try #require(olderTree.first { $0.title == "Display" })
        #expect(olderDisplay.pages.last?.title == "3D View")
        #expect(olderDisplay.pages.last?.unavailableReason == StackedTraceHold.coreOlderText)

        // Then the V12 Core: everything it describes is drawn.
        Self.asV12Core(app)
        await rig.station.deliverSetup(try Self.coreCategories(peer: 12))
        // Settled once the pages hold what the feed holds for both
        // categories: the feed turns to 12 a queued refresh before the pages do.
        #expect(await settle {
            app.setupPages.isCurrent && app.setupFeed.description(for: "display")?.version == 12
                && ["display", "appearance"].allSatisfy { id in
                    app.setupPages.categories[id] != nil
                        && app.setupPages.categories[id] == app.setupFeed.description(for: id)
                }
        })
        for id in ["display", "appearance"] {
            let fed = try #require(app.setupFeed.description(for: id))
            let shown = try #require(app.setupPages.categories[id])
            #expect(shown == fed, "\(id): every V12 row is drawn")
        }
        let shown = try #require(app.setupPages.categories["display"])
        #expect(shown.pages.map(\.id) == ["display.spectrumDefaults", "display.spectrumPeaks",
                                          "display.waterfallDefaults", "display.gridScales", "display.multimeter",
                                          "display.txDisplay", "display.threeD"])
        let rows = shown.pages.flatMap(\.sections).flatMap(\.controls)
        #expect(rows.count == 100)
        #expect(rows.allSatisfy { $0.metadataIssue == nil })
        // Every V12 row the Core describes is there, and every phone-bound
        // one reaches a setting of this phone (none reads "not available").
        let v12 = rows.filter { $0.requiresDescriptionVersion == 12 }
        #expect(v12.count == 52)
        for control in rows {
            guard case .phone? = control.binding else { continue }
            #expect(controls.state(of: control, in: "display").reason
                    != SetupControlDispatcher.notOnThisPhoneReason, "\(control.id)")
        }
        let multimeter = try #require(shown.pages.first { $0.id == "display.multimeter" })
        #expect(multimeter.sections.flatMap(\.controls).map(\.id)
                == ["display.multimeter.pollingDelay", "display.multimeter.showDecimal",
                    "display.multimeter.unitMode", "display.multimeter.historyDuration"])
        // The tree lists Grid & Scales in the desktop's place, and 3D View last, live on this Core.
        #expect(await settle { app.main.band.stackOffered })
        let tree = SetupTree.categories(described: app.setupPages.categories, order: app.setupPages.order,
                                        unreadable: app.setupPages.unreadable, stackOffered: app.main.band.stackOffered)
        let display = try #require(tree.first { $0.title == "Display" })
        #expect(display.pages.map(\.title) == ["On this phone", "Spectrum Defaults", "Spectrum Peaks",
                                               "Waterfall Defaults", "Grid & Scales", "Multimeter", "TX Display",
                                               "3D View"])
        #expect(display.pages.allSatisfy { $0.unavailableReason == nil })
        let appearance = try #require(app.setupPages.categories["appearance"])
        #expect(appearance.version == 12)
        #expect(appearance.pages[0].sections.map(\.title) == ["Spectrum", "Reset"])
        await rig.app.disconnect()
    }

    @Test("V12: every drawn row keeps its value in this phone's display settings, once for the band shown")
    func v12RowsReachThisPhonesSettings() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        let controls = app.setupControls
        Self.asV12Core(app)
        await rig.station.deliverSetup(try Self.coreCategories(peer: 12))
        #expect(await settle { app.setupPages.isCurrent && app.setupFeed.description(for: "display")?.version == 12 })
        let all = try #require(app.setupPages.categories["display"]).pages.flatMap(\.sections).flatMap(\.controls)
        func control(_ id: String) throws -> SetupDescription.Control {
            try #require(all.first { $0.id == "display.\(id)" })
        }
        // Each row's value as the phone reads it before any edit: the phone's own defaults (D99).
        let initial = try control("spectrumDefaults.clarity")
        #expect(controls.state(of: initial, in: "display") == .init(value: .bool(true), editable: true, reason: nil))

        let edits: [(String, SetupValue)] = [
            ("spectrumDefaults.clarity", .bool(false)),
            ("spectrumDefaults.showCursorFreq", .bool(false)), ("spectrumDefaults.showBinWidth", .bool(true)),
            ("spectrumDefaults.showNoiseFloor", .bool(true)), ("spectrumDefaults.noiseFloorShift", .decimal(-2.5)),
            ("spectrumDefaults.noiseFloorLineWidth", .decimal(2.5)),
            ("spectrumDefaults.noiseFloorColor", .text("#11223344")),
            ("spectrumDefaults.noiseFloorTextColor", .text("#22334455")),
            ("spectrumDefaults.noiseFloorFastColor", .text("#33445566")),
            ("spectrumDefaults.showPeakValue", .bool(true)), ("spectrumDefaults.peakValuePosition", .integer(2)),
            ("spectrumDefaults.peakTextDelay", .integer(1500)),
            ("waterfallDefaults.highThreshold", .integer(-50)), ("waterfallDefaults.lowThreshold", .integer(-130)),
            ("waterfallDefaults.agc", .bool(false)), ("waterfallDefaults.nfAgc", .bool(true)),
            ("waterfallDefaults.nfAgcOffset", .integer(-12)), ("waterfallDefaults.colorScheme", .integer(7)),
            ("waterfallDefaults.historyDepth", .integer(900_000)),
            ("waterfallDefaults.timestampPosition", .integer(2)), ("waterfallDefaults.timestampMode", .integer(1)),
            ("gridScales.showGrid", .bool(false)), ("gridScales.dbmScale", .bool(false)),
            ("gridScales.dbMax", .integer(-30)), ("gridScales.dbMin", .integer(-150)),
            ("gridScales.dbStep", .integer(5)), ("gridScales.freqLabelAlign", .integer(4)),
            ("gridScales.zeroLine", .bool(true)), ("gridScales.showFps", .bool(true)),
            ("gridScales.adjustGridMinToNoiseFloor", .bool(true)), ("gridScales.noiseFloorOffset", .integer(7)),
            ("gridScales.maintainGridRange", .bool(true)),
            ("txDisplay.wfLowLevel", .integer(-80)), ("txDisplay.wfHighLevel", .integer(40)),
            ("txDisplay.wfPalette", .integer(0)), ("txDisplay.wfLowColor", .text("#0A0B0CFF")),
            // Last, as it waits on the detector: Average makes Normalize live.
            ("spectrumDefaults.normalize", .bool(true)),
        ]
        let normalize = try control("spectrumDefaults.normalize")
        #expect(controls.state(of: normalize, in: "display").reason == SetupControlDispatcher.dependsReason)
        app.main.changeDisplay { $0.spectrumDetector = .average }
        for (id, value) in edits {
            let row = try control(id)
            #expect(await controls.edit(row, in: "display", to: value) == .applied, "\(id)")
            #expect(controls.state(of: row, in: "display").value == value, "\(id)")
        }
        let settings = app.main.band.settings
        #expect(!settings.clarityEnabled && !settings.waterfallAgc && settings.waterfallNfAgc)
        #expect(settings.waterfallLevelMode == .noiseFloorAgc && settings.waterfallOffsetDb == -12)
        #expect(!settings.showCursorFrequency && settings.showBinWidth && settings.noiseFloorLine)
        #expect(settings.noiseFloorShiftDb == -2.5 && settings.noiseFloorLineWidth == 2.5)
        #expect(settings.noiseFloorColour == "#11223344" && settings.noiseFloorTextColour == "#22334455")
        #expect(settings.noiseFloorFastColour == "#33445566" && settings.normalize)
        #expect(settings.showPeakValue && settings.peakValueCorner == .bottomLeft && settings.peakValueDelayMs == 1500)
        #expect(settings.waterfallHighDbm == -50 && settings.waterfallLowDbm == -130)
        #expect(settings.waterfallPaletteId == 7 && settings.rewindSeconds == 900)
        #expect(settings.timestampPosition == .right && !settings.timestampUtc)
        #expect(!settings.grid && !settings.showDbmScale && settings.scaleTopDbm == -30 && settings.scaleBottomDbm == -150)
        #expect(settings.gridStepDb == 5 && settings.frequencyLabelAlignment == .off && settings.showZeroLine)
        #expect(settings.showFps && settings.gridFollowsNoiseFloor && settings.gridNoiseFloorOffsetDb == 7)
        #expect(settings.gridKeepsRange)
        #expect(settings.txWaterfallLowDbm == -80 && settings.txWaterfallHighDbm == 40)
        #expect(settings.txWaterfallPaletteId == 0 && settings.txWaterfallLowColour == "#0A0B0CFF")
        // The desktop's Custom scheme: shown disabled with its reason, never chosen,
        // and never this phone's own gradient.
        for id in ["waterfallDefaults.colorScheme", "txDisplay.wfPalette"] {
            let row = try control(id)
            #expect(controls.unavailableOptions(of: row) == [6: PhoneSetupKeys.customSchemeReason], "\(id)")
            #expect(await controls.edit(row, in: "display", to: .integer(6))
                    == .notSent(PhoneSetupKeys.customSchemeReason), "\(id)")
        }
        #expect(app.main.band.settings.waterfallPaletteId == 7 && app.main.band.settings.txWaterfallPaletteId == 0)
        #expect(controls.unavailableOptions(of: try control("gridScales.freqLabelAlign")).isEmpty)
        // This phone's own gradient, chosen on its sheet, is none of the Core's options.
        app.main.changeDisplay { $0.waterfallPaletteId = BandPalette.customPaletteId }
        #expect(controls.state(of: try control("waterfallDefaults.colorScheme"), in: "display").value
                == .integer(Int64(BandPalette.customPaletteId)))
        // Outside the Core's range or options: refused, nothing kept.
        #expect(await controls.edit(try control("waterfallDefaults.historyDepth"), in: "display", to: .integer(600_000))
                == .notSent(SetupControlDispatcher.outOfRangeReason))
        #expect(app.main.band.settings.rewindSeconds == 900)
        // Use spectrum min/max greys the four rows that wait on it.
        #expect(await controls.edit(try control("waterfallDefaults.useSpectrumMinMax"), in: "display", to: .bool(true))
                == .applied)
        for id in ["waterfallDefaults.highThreshold", "waterfallDefaults.lowThreshold", "waterfallDefaults.agc",
                   "waterfallDefaults.nfAgc", "waterfallDefaults.nfAgcOffset"] {
            #expect(controls.state(of: try control(id), in: "display").reason
                    == SetupControlDispatcher.dependsReason, "\(id)")
        }
        // The per-band scale: kept for the band the pan is on.
        if let band = app.main.band.settings.scaleBand {
            #expect(app.main.band.settings.bandScales[band] == .init(topDbm: -30, bottomDbm: -150))
            let name = SetupDescription.bandNames[Int(band) ?? 0]
            #expect(controls.label(of: try control("gridScales.dbMax")) == "dB Max (\(name)):")
        }
        // Kept once, in this phone's one kept copy.
        let kept = BandDisplaySettingsStore(defaults: rig.defaults).settings(forPan: BandSubscriber.panId)
        // The Multimeter rows: the decimal point and the units reach the
        // S-meter's settings, kept once for the phone; History duration is
        // greyed with its reason and keeps nothing.
        let decimal = try control("multimeter.showDecimal")
        let unit = try control("multimeter.unitMode")
        #expect(controls.state(of: decimal, in: "display") == .init(value: .bool(true), editable: true, reason: nil))
        #expect(controls.state(of: unit, in: "display") == .init(value: .integer(1), editable: true, reason: nil))
        #expect(await controls.edit(decimal, in: "display", to: .bool(false)) == .applied)
        // The Core's options in order: 0 S, 1 dBm, 2 uV.
        let choices: [(Int64, SMeterUnit)] = [(2, .microvolts), (0, .sUnits), (1, .dBm)]
        for (index, chosen) in choices {
            #expect(await controls.edit(unit, in: "display", to: .integer(index)) == .applied)
            #expect(app.main.sMeter.state.settings.unit == chosen)
            #expect(controls.state(of: unit, in: "display").value == .integer(index))
        }
        #expect(await controls.edit(unit, in: "display", to: .integer(2)) == .applied)
        #expect(controls.state(of: decimal, in: "display").value == .bool(false))
        #expect(await controls.edit(unit, in: "display", to: .integer(3))
                == .notSent(SetupControlDispatcher.outOfRangeReason))
        #expect(SMeterSettingsStore(defaults: rig.defaults).settings.readout
                == SMeterReadout(unit: .microvolts, showDecimal: false))
        let history = try control("multimeter.historyDuration")
        #expect(controls.state(of: history, in: "display")
                == .init(value: nil, editable: false, reason: PhoneSetupKeys.noHistoryGraphReason))
        #expect(PhoneSetupKeys.noHistoryGraphReason == "This phone has no signal history graph.")
        #expect(await controls.edit(history, in: "display", to: .integer(30_000))
                == .notSent(PhoneSetupKeys.noHistoryGraphReason))
        #expect(kept.noiseFloorLineWidth == 2.5 && kept.txWaterfallLowColour == "#0A0B0CFF" && !kept.clarityEnabled)
        try await Self.shoot("setup-v12-grid-scales", rig: rig,
                             path: [.category("Display"), .described(category: "display", page: "display.gridScales")],
                             height: 1_500)
        await rig.app.disconnect()
    }

    @Test("V12: the buttons do what the desktop's do, and Smooth Defaults moves only its own settings (D97)")
    func v12Buttons() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        let controls = app.setupControls
        Self.asV12Core(app)
        await rig.station.deliverSetup(try Self.coreCategories(peer: 12))
        #expect(await settle { app.setupPages.isCurrent && app.setupFeed.description(for: "display")?.version == 12 })
        let rows = try #require(app.setupPages.categories["display"]).pages.flatMap(\.sections).flatMap(\.controls)
            + (try #require(app.setupPages.categories["appearance"])).pages.flatMap(\.sections).flatMap(\.controls)
        func control(_ id: String) throws -> SetupDescription.Control {
            try #require(rows.first { $0.id == id })
        }
        let smooth = try control("display.spectrumDefaults.smoothDefaults")
        #expect(smooth.confirm?.contains("per-band grid ranges") == true)
        // Reset to Smooth Defaults: its seven values, nothing else.
        app.main.changeDisplay {
            $0.waterfallPaletteId = 2
            $0.spectrumAveraging = .none
            $0.spectrumAverageTimeMs = 120
            $0.traceColour = "#123456"
            $0.traceFill = true
            $0.waterfallAgc = false
            $0.waterfallPeriodMs = 200
            $0.gridStepDb = 5
            $0.showZeroLine = true
        }
        let before = app.main.band.settings
        #expect(controls.state(of: smooth, in: "display").editable)
        #expect(await controls.edit(smooth, in: "display", to: nil) == .applied)
        var expected = before
        expected.waterfallPaletteId = 7
        expected.spectrumAveraging = .logRecursive
        expected.spectrumAverageTimeMs = 650
        expected.traceColour = "#FFFFFF"
        expected.traceFill = false
        expected.waterfallAgc = true
        expected.waterfallPeriodMs = 30
        #expect(app.main.band.settings == expected)

        // Copy spectrum min/max: the thresholds become the scale as drawn.
        app.main.changeDisplay {
            $0.scaleTopDbm = -35
            $0.scaleBottomDbm = -125
        }
        #expect(await controls.edit(try control("display.waterfallDefaults.copySpectrumMinMax"), in: "display", to: nil)
                == .applied)
        #expect(app.main.band.settings.waterfallHighDbm == -35 && app.main.band.settings.waterfallLowDbm == -125)
        // Copy waterfall thresholds: the band's dB Max and dB Min become them, rounded.
        app.main.changeDisplay {
            $0.waterfallHighDbm = -44.6
            $0.waterfallLowDbm = -118.4
        }
        #expect(await controls.edit(try control("display.gridScales.copyWaterfallThresholds"), in: "display", to: nil)
                == .applied)
        #expect(app.main.band.settings.scaleTopDbm == -45 && app.main.band.settings.scaleBottomDbm == -118)

        // Reset all colors: the swatches back to the Core's described defaults.
        app.main.changeDisplay {
            $0.gridColour = "#01020304"
            $0.passbandColour = "#010203"
            $0.passbandOpacity = 1
            $0.txZeroLineColour = "#05060708"
        }
        let reset = try control("appearance.colorsTheme.resetColors")
        #expect(reset.confirm?.isEmpty == false)
        #expect(await controls.edit(reset, in: "appearance", to: nil) == .applied)
        for swatch in (try #require(app.setupPages.categories["appearance"])).pages[0].sections[0].controls {
            let state = controls.state(of: swatch, in: "appearance")
            guard state.value != nil else {
                // The band edge colour: this phone keeps none.
                #expect(state.reason == SetupControlDispatcher.notOnThisPhoneReason)
                continue
            }
            #expect(state.value.flatMap(\.text) == swatch.defaultValue.flatMap {
                if case .text(let colour) = $0 { return colour }
                return nil
            }, "\(swatch.id)")
        }

        // Get Monitor Hz writes the Core's FPS setting as an FPS edit.
        let monitor = try control("display.spectrumDefaults.getMonitorHz")
        let fps = try control("display.spectrumDefaults.fps")
        #expect(controls.state(of: monitor, in: "display").reason == controls.state(of: fps, in: "display").reason)
        await rig.app.disconnect()
    }

    @Test("V12: pictures of each changed page, upright and sideways, and the densest in large type")
    func v12Pictures() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        Self.asV12Core(rig.app)
        await rig.station.deliverSetup(try Self.coreCategories(peer: 12))
        #expect(await settle { rig.app.setupPages.isCurrent
            && rig.app.setupFeed.description(for: "display")?.version == 12 })
        // Name, category, page, picture height and pictures to cover it upright.
        let pages: [(String, String, String, CGFloat, Int)] = [
            ("spectrum-defaults", "display", "display.spectrumDefaults", 2_600, 2),
            ("waterfall-defaults", "display", "display.waterfallDefaults", 2_600, 2),
            ("grid-scales", "display", "display.gridScales", 1_700, 1),
            ("multimeter", "display", "display.multimeter", 1_400, 1),
            ("tx-display", "display", "display.txDisplay", 2_400, 1),
            ("colors-theme", "appearance", "appearance.colorsTheme", 1_600, 1),
        ]
        for (name, category, page, height, parts) in pages {
            let top: SetupTree.Route = .category(category == "display" ? "Display" : "Appearance")
            let path: [SetupTree.Route] = [top, .described(category: category, page: page)]
            try await Self.shoot("setup-v12-\(name)", rig: rig, path: path, height: height, parts: parts)
            // Sideways: the phone's width turned, the page drawn to its full length.
            try await Self.shoot("setup-v12-\(name)-sideways", rig: rig, path: path, height: height,
                                 width: 874, sideways: true, parts: parts)
        }
        try await Self.shoot("setup-v12-spectrum-defaults-large-type", rig: rig,
                             path: [.category("Display"), .described(category: "display", page: "display.spectrumDefaults")],
                             height: 2_600, dynamicType: .accessibility3, parts: 4)
        await rig.app.disconnect()
    }

    /// A made-up General page with a property and a verb the fake Core has.
    static let ownersPage = #"{"version":3,"category":{"id":"general","title":"General","where":"mixed"},"pages":[{"id":"general.owners","title":"Options","where":"station","sections":[{"title":"Sample","controls":[{"id":"general.owners.power","label":"Sample power","tooltip":"","kind":"integer","binding":{"property":{"object":"transmit","name":"power"}},"applies":"live","min":0,"max":100,"step":1,"unit":"W"},{"id":"general.owners.preset","label":"Load sample preset","tooltip":"","kind":"button","binding":{"command":{"verb":"sample.loadPreset","arguments":{"name":"sample"}}},"applies":"live"}]}]}]}"#

    // MARK: Trunk 40b089c9a

    @Test("the Core's TCI server settings draw live from the stationTci object; RX2 attenuation is greyed with a reason")
    func trunkRowsDrawn() async throws {
        #expect(LinkFeatures.app["stationTciSettings"] == 1)
        // The phone declares adcAttenuators for the Modes tab; these rows
        // take their range from the board, which the phone does not draw.
        #expect(LinkFeatures.app["adcAttenuators"] == 1)
        let rig = try await Self.connected(additions: [.bands, .setupDescription, .stationTci])
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        var entries = app.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(ordinal: 0, name: name, value: value.wireValue)
        }
        entries.removeAll { $0.name == "stationTciSettingsVersion" }
        entries.append(.init(ordinal: 0, name: "stationTciSettingsVersion", value: .i64(1)))
        app.mirror.apply(.capabilities(LinkMessage.Capabilities(properties: entries)))
        try await rig.station.deliverStationTci(.board)
        await rig.station.deliverSetup(try Self.coreCategories())
        #expect(await settle { app.setupPages.isCurrent && app.setupPages.categories["catNetwork"] != nil
            && app.setupPages.categories["general"] != nil && app.mirror.object("stationTci") != nil })
        let controls = app.setupControls
        let tci = try #require(app.setupPages.categories["catNetwork"]?.pages.first { $0.id == "catNetwork.tciServer" })
        let settings = tci.sections.flatMap(\.controls).filter { $0.id.hasPrefix("catNetwork.tciServer.core.") }
        #expect(settings.count == 11)
        // The Core now acts on the three RX2 VFO rows: none is closed, and
        // Forget comes without its version 21 enabledWhen to this phone at 17.
        #expect(settings.allSatisfy { $0.availability?.enabled != false })
        #expect(settings.allSatisfy { $0.enabledWhen == nil })
        for control in settings {
            let state = controls.state(of: control, in: "catNetwork")
            #expect(state.editable && state.reason == nil, "\(control.id): \(state.reason ?? "")")
            #expect(state.value != nil, "\(control.id)")
        }
        let options = try #require(app.setupPages.categories["general"]?.pages.first { $0.id == "general.options" })
        let rx2 = options.sections.flatMap(\.controls).filter { $0.id.hasPrefix("general.options.rx2") }
        #expect(rx2.map(\.id) == ["general.options.rx2StepAttEnable", "general.options.rx2StepAtt",
                                   "general.options.rx2AutoAttEnable", "general.options.rx2AutoAttUndo"])
        for control in rx2 {
            let state = controls.state(of: control, in: "general")
            #expect(!state.editable && state.reason?.isEmpty == false, "\(control.id) is greyed with a reason")
        }
        try await Self.shoot("trunk-40b089-tci-settings", rig: rig,
                             path: [.category("CAT & Network"),
                                    .described(category: "catNetwork", page: "catNetwork.tciServer")],
                             height: 2_600, parts: 2)
        try await Self.shoot("trunk-40b089-general-options", rig: rig,
                             path: [.category("General"), .described(category: "general", page: "general.options")],
                             height: 2_600, parts: 2)
        await rig.app.disconnect()
    }

    // MARK: Pictures

    @Test("pictures of the tree and one described page for each category that has one")
    func pictures() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        await rig.station.deliverSetup(try Self.coreCategories())
        let pages = rig.app.setupPages
        #expect(await settle { pages.isCurrent && pages.categories.count >= 10 })
        let shots: [(String, [SetupTree.Route], CGFloat)] = [
            ("setup-described-tree", [], 1_300),
            ("setup-described-general-options", [.category("General"),
                                                 .described(category: "general", page: "general.options")], 874),
            ("setup-described-hardware", [.category("Hardware"),
                                          .described(category: "hardware", page: "hardware.antennaAlex")], 874),
            ("setup-described-pa", [.category("PA"), .described(category: "pa", page: "pa.values")], 874),
            ("setup-described-audio", [.category("Audio"),
                                       .described(category: "audio", page: "audio.txProfile")], 874),
            ("setup-described-dsp", [.category("DSP"), .described(category: "dsp", page: "dsp.agcAlc")], 874),
            ("setup-described-display-category", [.category("Display")], 874),
            ("setup-described-display-spectrum", [.category("Display"),
                                                  .described(category: "display", page: "display.spectrumDefaults")],
             2_400),
            ("setup-described-display-waterfall", [.category("Display"),
                                                   .described(category: "display", page: "display.waterfallDefaults")],
             1_900),
            ("setup-described-transmit", [.category("Transmit"),
                                          .described(category: "transmit", page: "transmit.power")], 874),
            ("setup-described-appearance", [.category("Appearance"),
                                            .described(category: "appearance", page: "appearance.meterStyles")], 874),
            ("setup-described-cat", [.category("CAT & Network"),
                                     .described(category: "catNetwork", page: "catNetwork.tciServer")], 874),
            ("setup-described-test", [.category("Test"),
                                      .described(category: "test", page: "test.twoToneImd")], 874),
            ("setup-described-diagnostics", [.category("Diagnostics"),
                                             .described(category: "diagnostics",
                                                        page: "diagnostics.settingsValidation")], 874),
        ]
        for (name, path, height) in shots {
            try await Self.shoot(name, rig: rig, path: path, height: height)
        }
        await rig.app.disconnect()
    }

    @Test("V16: pictures of the pages descriptions 13 to 16 added, each row the phone cannot run greyed with its reason")
    func v16Pictures() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        Self.asV12Core(rig.app, setupDescription: 16)
        await rig.station.deliverSetup(try Self.coreCategories(peer: 16))
        let pages = rig.app.setupPages
        #expect(await settle {
            pages.isCurrent && pages.categories["hardware"]?.version == 16 && pages.categories["dsp"]?.version == 15
        })
        let shots: [(name: String, category: String, id: String, page: String, height: CGFloat)] = [
            ("trunk-284aa6-filter-presets", "DSP", "dsp", "dsp.filterPresets", 1_100),
            ("trunk-284aa6-speech-processor", "Transmit", "transmit", "transmit.speechProcessor", 1_900),
            ("trunk-284aa6-tx-input", "Audio", "audio", "audio.txInput", 1_500),
            ("trunk-284aa6-radio-status", "Diagnostics", "diagnostics", "diagnostics.radioStatus", 1_700),
            ("trunk-284aa6-connection-quality", "Diagnostics", "diagnostics", "diagnostics.connectionQuality", 874),
            ("trunk-284aa6-4o3a", "CAT & Network", "catNetwork", "catNetwork.fourO3A", 2_000),
            ("trunk-284aa6-rf-kit", "CAT & Network", "catNetwork", "catNetwork.rfKit", 2_400),
            ("trunk-284aa6-general-options", "General", "general", "general.options", 2_000),
            ("trunk-284aa6-pa-values", "PA", "pa", "pa.values", 1_700),
        ]
        for shot in shots {
            let category = try #require(pages.categories[shot.id], "\(shot.id)")
            let page = try #require(category.pages.first { $0.id == shot.page }, "\(shot.page)")
            #expect(page.sections.flatMap(\.controls).allSatisfy { $0.metadataIssue == nil }, "\(shot.page)")
            // General is longer than one picture can hold, so it is drawn in two.
            try await Self.shoot(shot.name, rig: rig,
                                 path: [.category(shot.category), .described(category: shot.id, page: shot.page)],
                                 height: shot.height, parts: shot.id == "general" ? 2 : 1)
        }
        await rig.app.disconnect()
    }

    // MARK: Description V17: the Alex-1 low-pass rows

    /// Raises (or sets) the mirror's capabilities as the Core would send them.
    static func withCapabilities(_ app: AppModel, _ raised: [String: LinkMessage.PropertyValue]) {
        var entries = app.mirror.capabilities.map { name, value in
            LinkMessage.PropertyEntry(ordinal: 0, name: name, value: value.wireValue)
        }
        entries.removeAll { raised[$0.name] != nil }
        for (name, value) in raised {
            entries.append(.init(ordinal: 0, name: name, value: value))
        }
        app.mirror.apply(.capabilities(LinkMessage.Capabilities(properties: entries)))
    }

    /// `radio`'s alexLpfBits, at its place in the Core's surface.
    static let lowPassBitsOrdinal: UInt16 = 29

    static let alexPage = "hardware.alex1Filters"
    static let lowPassBands = ["160m", "80m", "40m", "20m", "15m", "10m", "6m"]
    static let receiveOnlyReason = "This Core is set to receive only."

    @Test("V17: the Alex-1 low-pass rows edit one edge, show the Core's moves, and mark the filter in use")
    func v17AlexLowPass() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        Self.asV12Core(app, setupDescription: 17)
        Self.withCapabilities(app, ["radioHardwareVersion": .i64(10), "txPermitted": .bool(true)])
        await rig.station.deliverSetup(try Self.coreCategories(peer: 17))
        #expect(await settle { app.setupPages.isCurrent && app.setupPages.categories["hardware"]?.version == 17 })
        let hardware = try #require(app.setupPages.categories["hardware"])
        let page = try #require(hardware.pages.first { $0.id == Self.alexPage })
        let section = try #require(page.sections.first { $0.title == "Alex LPF Bands" })
        let edges = Self.lowPassBands.flatMap { ["hardware.alex1Filters.lpf.\($0).start", "hardware.alex1Filters.lpf.\($0).end"] }
        #expect(section.controls.map(\.id) == edges + ["hardware.alex1Filters.lpfBypass"])
        let controls = app.setupControls
        for control in section.controls {
            #expect(control.metadataIssue == nil, "\(control.id)")
            #expect(controls.state(of: control, in: "hardware").editable, "\(control.id)")
            #expect(DescribedControl.row(for: control) == (control.kind == .toggle ? .switchRow : .numberRow))
        }
        // The HL2 clock rows arrive closed from the Core at 17, with its reason.
        let clock = try #require(hardware.pages.flatMap(\.sections).flatMap(\.controls)
            .first { $0.id == "hardware.hl2Io.cl2Freq" })
        #expect(controls.state(of: clock, in: "hardware").reason == "NereusSDR does not change the radio's clock settings.")

        // The filter in use: 0x10 is the 6m filter.
        await rig.station.deliver(.delta(.init(key: "radio", properties: [
            .init(ordinal: Self.lowPassBitsOrdinal, name: "alexLpfBits", value: .i64(0x10)),
        ])))
        func control(_ id: String) throws -> SetupDescription.Control {
            try #require(section.controls.first { $0.id == id })
        }
        #expect(await settle { controls.lowPassLamp(for: (try? control("hardware.alex1Filters.lpf.6m.start"))!) == .inUse })
        #expect(controls.lowPassLamp(for: try control("hardware.alex1Filters.lpf.160m.end")) == .notInUse)
        #expect(controls.lowPassLamp(for: try control("hardware.alex1Filters.lpfBypass")) == nil)

        // One edge is written, as the Core names it; the Core's move of the
        // neighbour comes back as its own setting and is shown as it came.
        let start = try control("hardware.alex1Filters.lpf.80m.start")
        let writing = Task { await controls.edit(start, in: "hardware", to: .decimal(1.8)) }
        let write = try await Self.settingsWrite("hardware/\(FakeStation.setupPanelsRadioMac)/alex/lpf/80m/start", rig.station)
        #expect(write.properties.first?.value == .utf8("1.8"))
        await rig.station.deliver(.settingsValue(.init(key: write.key, origin: write.origin, properties: write.properties)))
        await rig.station.deliver(.settingsValue(.init(
            key: "hardware/\(FakeStation.setupPanelsRadioMac)/alex/lpf/160m/end", origin: "station",
            properties: [.init(name: "value", value: .utf8("1.799999"))])))
        #expect(await writing.value == .applied)
        let moved = try control("hardware.alex1Filters.lpf.160m.end")
        #expect(await settle { controls.state(of: moved, in: "hardware").value == .decimal(1.799999) })

        // Pictures: the filter marked, then no mark (-1), then a receive-only session.
        try await Self.shootAlexFilters("trunk-ec86bd-alex-lpf-in-use", rig: rig)
        await rig.station.deliver(.delta(.init(key: "radio", properties: [
            .init(ordinal: Self.lowPassBitsOrdinal, name: "alexLpfBits", value: .i64(-1)),
        ])))
        #expect(await settle { controls.lowPassLamp(for: start) == nil })
        try await Self.shootAlexFilters("trunk-ec86bd-alex-lpf-no-mark", rig: rig)
        await rig.station.deliver(.delta(.init(key: "radio", properties: [
            .init(ordinal: Self.lowPassBitsOrdinal, name: "alexLpfBits", value: .i64(0x10)),
        ])))
        Self.withCapabilities(app, ["txPermitted": .bool(false), "txRefusalReason": .utf8(Self.receiveOnlyReason)])
        #expect(await settle { controls.state(of: start, in: "hardware").reason == Self.receiveOnlyReason })
        #expect(controls.state(of: try control("hardware.alex1Filters.lpfBypass"), in: "hardware").editable)
        try await Self.shootAlexFilters("trunk-ec86bd-alex-lpf-receive-only", rig: rig)

        // A Core below radioHardwareVersion 10 says so on every row and the bypass.
        Self.withCapabilities(app, ["radioHardwareVersion": .i64(9), "txPermitted": .bool(true)])
        #expect(await settle {
            section.controls.allSatisfy {
                controls.state(of: $0, in: "hardware").reason == SetupControlDispatcher.lowPassNeedsNewerCoreReason
            }
        })
        await rig.app.disconnect()
    }

    /// The Alex-1 Filters page scrolled to its low-pass rows: upright and
    /// sideways, light and dark.
    static func shootAlexFilters(_ name: String, rig: Rig) async throws {
        let path: [SetupTree.Route] = [.category("Hardware"), .described(category: "hardware", page: alexPage)]
        for scheme in [ColorScheme.light, .dark] {
            let look = scheme == .dark ? "dark" : "light"
            try await shoot("\(name)-portrait-\(look)", rig: rig, path: path, scheme: scheme, atBottom: true)
            try await shoot("\(name)-landscape-\(look)", rig: rig, path: path, height: 402, width: 874,
                            sideways: true, scheme: scheme, atBottom: true)
        }
    }

    /// The Filter Presets table's own reason, as the phone words it.
    static let filterPresetsReason = "Filter presets are edited on the desktop."

    @Test("the supported Filter Presets table uses the Core catalogue or says why once")
    func filterPresetsSaysWhyOnce() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        Self.asV12Core(rig.app, setupDescription: 17)
        await rig.station.deliverSetup(try Self.coreCategories(peer: 17))
        let pages = rig.app.setupPages
        #expect(await settle { pages.isCurrent && pages.categories["dsp"]?.version == 15 })
        let presets = try #require(pages.categories["dsp"]?.pages.first { $0.id == "dsp.filterPresets" }?
            .sections.flatMap(\.controls).first { $0.kind == .table })
        #expect(presets.metadataIssue == nil && presets.modern?.pendingReason == nil)
        #expect(DescribedControl.row(for: presets) == .panel)
        let state = rig.app.setupControls.state(of: presets, in: "dsp")
        #expect(state.reason == (rig.app.setupControls.filterPresets.rows.isEmpty ? "Waiting for the Core to send the limits for this setting." : nil))
        // One line: the row's own reason; the tables' standard words only for a table without one.
        #expect(DescribedControl.tableReason(Self.filterPresetsReason) == Self.filterPresetsReason)
        #expect(DescribedControl.tableReason(nil) == SetupControlDispatcher.notOnThisPhoneReason)
        try await Self.shoot("trunk-ec86bd-filter-presets", rig: rig,
                             path: [.category("DSP"), .described(category: "dsp", page: "dsp.filterPresets")],
                             height: 1_100)
        await rig.app.disconnect()
    }

    // MARK: Description V22: the HL2 clock rows and CFC's band editor

    static let clockPage = "hardware.hl2Io"
    static let clockRefusal = "Choose a CL2 frequency from 1 to 200 MHz."

    @Test("V22: the HL2 clock rows are live: CL2 frequency follows Enable CL2 and the Core's refusal comes back as sent")
    func v22ClockRows() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        Self.asV12Core(app, setupDescription: 22)
        Self.withCapabilities(app, ["transmitSettingsVersion": .i64(15), "txPermitted": .bool(true)])
        await rig.station.deliverSetup(try Self.coreCategories(peer: 22))
        #expect(await settle { app.setupPages.isCurrent && app.setupPages.categories["hardware"]?.version == 18 })
        let page = try #require(app.setupPages.categories["hardware"]?.pages.first { $0.id == Self.clockPage })
        func control(_ id: String) throws -> SetupDescription.Control {
            try #require(page.sections.flatMap(\.controls).first { $0.id == "hardware.hl2Io.\(id)" })
        }
        let controls = app.setupControls
        let enable = try control("cl2Enable")
        let frequency = try control("cl2Freq")
        let external = try control("ext10MHz")
        #expect(DescribedControl.row(for: frequency) == .numberRow)
        #expect(frequency.range == .init(minimum: 1, maximum: 200, step: 0.1))
        #expect(frequency.decimals == 3)
        // Enable CL2 off: the frequency is greyed with the Core's default.
        #expect(controls.state(of: enable, in: "hardware").editable)
        #expect(controls.state(of: external, in: "hardware").editable)
        let greyed = controls.state(of: frequency, in: "hardware")
        #expect(!greyed.editable && greyed.reason == SetupControlDispatcher.dependsReason)
        #expect(greyed.value == .decimal(116))
        try await Self.shootClock("setup-latest-hl2-clock-off", rig: rig)

        let mac = FakeStation.setupPanelsRadioMac
        let turning = Task { await controls.edit(enable, in: "hardware", to: .bool(true)) }
        let on = try await Self.settingsWrite("hardware/\(mac)/hl2/cl2Enable", rig.station)
        #expect(on.properties.first?.value == .utf8("True"))
        await rig.station.deliver(.settingsValue(.init(key: on.key, origin: on.origin, properties: on.properties)))
        #expect(await turning.value == .applied)
        #expect(await settle { controls.state(of: frequency, in: "hardware").editable })

        let key = "hardware/\(mac)/hl2/cl2FreqMHz"
        let writing = Task { await controls.edit(frequency, in: "hardware", to: .decimal(24.576)) }
        let write = try await Self.settingsWrite(key, rig.station)
        #expect(write.properties.first?.value == .utf8("24.576"))
        await rig.station.deliver(.settingsValue(.init(key: write.key, origin: write.origin, properties: write.properties)))
        #expect(await writing.value == .applied)
        #expect(await settle { controls.state(of: frequency, in: "hardware").value == .decimal(24.576) })
        try await Self.shootClock("setup-latest-hl2-clock-on", rig: rig)
        try await Self.shoot("setup-latest-hl2-clock-on-large-type", rig: rig,
                             path: [.category("Hardware"), .described(category: "hardware", page: Self.clockPage)],
                             height: 2_400, dynamicType: .accessibility2)

        // The Core's refusal, as it words it; the phone adds no rule of its own.
        let refused = Task { await controls.edit(frequency, in: "hardware", to: .decimal(150.5)) }
        let rejected = try await Self.settingsWrite(key, rig.station) { $0.properties.first?.value == .utf8("150.5") }
        await rig.station.deliver(.settingsReject(LinkMessage.SettingsReject(
            key: rejected.key, properties: [.init(name: "value", value: .utf8("24.576"))], reason: Self.clockRefusal)))
        #expect(await refused.value == .refused(Self.clockRefusal))
        await rig.app.disconnect()
    }

    /// The HL2 I/O page: upright and sideways, light and dark.
    static func shootClock(_ name: String, rig: Rig) async throws {
        let path: [SetupTree.Route] = [.category("Hardware"), .described(category: "hardware", page: clockPage)]
        for scheme in [ColorScheme.light, .dark] {
            let look = scheme == .dark ? "dark" : "light"
            try await shoot("\(name)-portrait-\(look)", rig: rig, path: path, scheme: scheme, atBottom: true)
            try await shoot("\(name)-landscape-\(look)", rig: rig, path: path, height: 402, width: 874,
                            sideways: true, scheme: scheme, atBottom: true)
        }
    }

    // MARK: Description V23: Calibration's Level Cal

    static let calibrationPage = "hardware.calibration"

    @Test("V23: Calibration opens with Level Cal, the Core's Rx1 6m LNA row live in it; below 23 the group comes without that row")
    func v23LevelCal() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        Self.asV12Core(app, setupDescription: 23)
        Self.withCapabilities(app, ["transmitSettingsVersion": .i64(15), "txPermitted": .bool(true),
                                    "radioHardwareVersion": .i64(12)])
        await rig.station.deliverSetup(try Self.coreCategories(peer: 23))
        #expect(await settle { app.setupPages.isCurrent && app.setupPages.categories["hardware"]?.version == 23 })
        let page = try #require(app.setupPages.categories["hardware"]?.pages.first { $0.id == Self.calibrationPage })
        #expect(page.sections.first?.title == LevelCalSection.title)
        let lna = try #require(page.sections.first?.controls.first)
        #expect(lna.id == "hardware.calibration.rx1_6mLna" && lna.label == "Rx1 6m LNA:")
        #expect(DescribedControl.row(for: lna) == .numberRow)
        #expect(lna.range == .init(minimum: 0, maximum: 25, step: 1) && lna.decimals == 1 && lna.unit == "dB")
        #expect(app.setupControls.state(of: lna, in: "hardware").editable)
        // The sign-in session carries no slice; slice A, this phone's own, comes active.
        await rig.station.deliver(BandFlagShotTests.slice(0, active: true))
        let levelCal = app.levelCal
        #expect(await settle { app.main.slices.activeSliceId == 0 })
        #expect(await settle { levelCal.startEnabled && levelCal.sliceLine == "Calibrates slice A." })
        let path: [SetupTree.Route] = [.category("Hardware"), .described(category: "hardware", page: Self.calibrationPage)]
        for scheme in [ColorScheme.light, .dark] {
            let look = scheme == .dark ? "dark" : "light"
            try await Self.shoot("setup-levelcal-ready-portrait-\(look)", rig: rig, path: path, height: 1_300,
                                 scheme: scheme)
            try await Self.shoot("setup-levelcal-ready-landscape-\(look)", rig: rig, path: path, height: 402,
                                 width: 874, sideways: true, parts: 3, scheme: scheme)
        }
        try await Self.shoot("setup-levelcal-ready-large-type", rig: rig, path: path, height: 2_600,
                             dynamicType: .accessibility2)

        // A run going: Start and Reset greyed with one reason, Cancel live, the bar at the Core's percent.
        app.mirror.apply(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 31, name: "levelCalRunning", value: .bool(true)),
            .init(ordinal: 32, name: "levelCalPercent", value: .i64(42)),
            .init(ordinal: 33, name: "levelCalMessage", value: .utf8("")),
            .init(ordinal: 34, name: "levelCalSucceeded", value: .bool(false)),
        ])))
        #expect(await settle { levelCal.running && levelCal.cancelEnabled && levelCal.percent == 42 })
        #expect(levelCal.reasons == [LevelCalModel.runningText])
        try await Self.shoot("setup-levelcal-running-portrait-dark", rig: rig, path: path, height: 1_300)
        try await Self.shoot("setup-levelcal-running-landscape-dark", rig: rig, path: path, height: 402, width: 874,
                             sideways: true, parts: 3)
        try await Self.shoot("setup-levelcal-running-large-type", rig: rig, path: path, height: 2_600,
                             dynamicType: .accessibility2)
        app.mirror.apply(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: 31, name: "levelCalRunning", value: .bool(false)),
            .init(ordinal: 33, name: "levelCalMessage", value: .utf8("Level calibration was canceled.")),
        ])))
        #expect(await settle { !levelCal.running && levelCal.message == "Level calibration was canceled." })
        try await Self.shoot("setup-levelcal-canceled-portrait-dark", rig: rig, path: path, height: 1_300)

        // A Core at 22 and radioHardwareVersion 11: no Rx1 6m LNA row, the
        // group first all the same, its three buttons greyed with the
        // desktop's two reasons.
        Self.asV12Core(app, setupDescription: 22)
        Self.withCapabilities(app, ["radioHardwareVersion": .i64(11)])
        await rig.station.deliverSetup(try Self.coreCategories(peer: 22))
        #expect(await settle { app.setupPages.isCurrent && app.setupPages.categories["hardware"]?.version == 18 })
        let older = try #require(app.setupPages.categories["hardware"]?.pages.first { $0.id == Self.calibrationPage })
        #expect(!older.sections.contains { $0.title == LevelCalSection.title })
        #expect(!older.sections.flatMap(\.controls).contains { $0.id == "hardware.calibration.rx1_6mLna" })
        #expect(await settle { levelCal.reasons == [LevelCalModel.runOlderCoreText, LevelCalModel.resetOlderCoreText] })
        try await Self.shoot("setup-levelcal-older-core-portrait-dark", rig: rig, path: path, height: 1_300)
        await rig.app.disconnect()
    }

    /// The Core's CFC profile, as `transmit.cfcProfile` carries it.
    static let cfcProfile = #"{"bands":[{"compressionDb":2,"compressionQ":2,"frequencyHz":0,"postEqGainDb":-3,"postEqQ":3},{"compressionDb":4,"compressionQ":2,"frequencyHz":500,"postEqGainDb":-1,"postEqQ":3},{"compressionDb":6,"compressionQ":2,"frequencyHz":1000,"postEqGainDb":0,"postEqQ":3},{"compressionDb":8,"compressionQ":2,"frequencyHz":2000,"postEqGainDb":1,"postEqQ":3},{"compressionDb":10,"compressionQ":2,"frequencyHz":4000,"postEqGainDb":3,"postEqQ":3}],"maxHz":4000,"minHz":0,"parametric":true,"postEqGainDb":-2,"precompDb":4,"revision":"4dcd43a8aec093bf","state":"saved"}"#
    static let cfcStaleReason = "The CFC settings changed on the Core. Check the new values and try again."

    @Test("V22: DSP > CFC opens the band editor; an edit goes whole through cfc.setProfile and a refusal shows as sent")
    func v22CfcEditor() async throws {
        let rig = try await Self.connected()
        defer { UserDefaults.standard.removePersistentDomain(forName: rig.suite) }
        let app = rig.app
        Self.asV12Core(app, setupDescription: 22)
        Self.withCapabilities(app, ["transmitSettingsVersion": .i64(14), "txPermitted": .bool(true)])
        await rig.station.deliverSetup(try Self.coreCategories(peer: 22))
        #expect(await settle { app.setupPages.isCurrent && app.setupPages.categories["dsp"]?.version == 22 })
        let bands = try #require(app.setupPages.categories["dsp"]?.pages.flatMap(\.sections).flatMap(\.controls)
            .first { $0.id == "dsp.cfc.bands" })
        #expect(bands.specialized == .cfcBands)
        #expect(SetupSpecializedPanels.live(app).make(.cfcBands, bands, "dsp") != nil)
        let controls = app.setupControls
        // A Core below transmit settings 15 cannot take the profile.
        #expect(controls.cfcPanel(bands, in: "dsp").reason == SetupControlDispatcher.cfcNoTransmitSettingsReason)
        Self.withCapabilities(app, ["transmitSettingsVersion": .i64(15)])
        #expect(await settle { controls.cfcPanel(bands, in: "dsp").reason == SetupControlDispatcher.cfcProfileMissingReason })

        if app.mirror.object("transmit") == nil {
            await rig.station.deliver(.objectCreate(.init(key: "transmit", className: "TransmitModel", properties: [
                .init(name: "cfcProfile", value: .utf8(Self.cfcProfile)),
            ])))
        } else {
            await rig.station.deliver(.delta(.init(key: "transmit", properties: [
                .init(name: "cfcProfile", value: .utf8(Self.cfcProfile)),
            ])))
        }
        #expect(await settle { controls.cfcPanel(bands, in: "dsp").editable })
        let shown = try #require(controls.cfcPanel(bands, in: "dsp").profile)
        #expect(shown.bands.count == 5 && shown.parametric)
        #expect(CfcBandsEditor.summary(shown) == "5-band, 0\u{2013}4000 Hz")

        let editor: [SetupTree.Route] = [.category("DSP"), .described(category: "dsp", page: "dsp.cfc"),
                                         .cfcBands(category: "dsp", control: bands.id)]
        try await Self.shoot("setup-latest-cfc-page", rig: rig,
                             path: [.category("DSP"), .described(category: "dsp", page: "dsp.cfc")])
        for scheme in [ColorScheme.light, .dark] {
            let look = scheme == .dark ? "dark" : "light"
            try await Self.shoot("setup-latest-cfc-editor-portrait-\(look)", rig: rig, path: editor, scheme: scheme)
            try await Self.shoot("setup-latest-cfc-editor-landscape-\(look)", rig: rig, path: editor, height: 402,
                                 width: 874, sideways: true, scheme: scheme)
        }
        try await Self.shoot("setup-latest-cfc-editor-large-type", rig: rig, path: editor,
                             dynamicType: .accessibility2)

        // One edit, sent whole after the pause with the Core's revision;
        // the Core's refusal is shown as it words it and its profile comes back.
        controls.cfcSendPause = .milliseconds(40)
        rig.station.refuseNext(CfcProfile.setProfileVerb, reason: Self.cfcStaleReason)
        controls.editCfc(bands, in: "dsp") { $0.precompDb = 6 }
        #expect(controls.cfcPanel(bands, in: "dsp").profile?.precompDb == 6)
        let sent = await rig.station.waitForMessage { message in
            if case .commandInvoke(let invoke) = message { return invoke.verb == CfcProfile.setProfileVerb }
            return false
        }
        guard case .commandInvoke(let invoke)? = sent else { Issue.record("no cfc.setProfile"); return }
        #expect(invoke.args.map(\.name) == [CfcProfile.profileArgumentName, CfcProfile.revisionArgumentName])
        #expect(invoke.args.last?.value == .utf8("4dcd43a8aec093bf"))
        guard case .utf8(let json)? = invoke.args.first?.value else { Issue.record("no profile"); return }
        // The values only: state and revision are the Core's to say.
        #expect(json.contains(#""precompDb":6"#) && !json.contains("revision") && !json.contains("state"))
        #expect(await settle { controls.cfcPanel(bands, in: "dsp").problem == Self.cfcStaleReason })
        #expect(controls.cfcPanel(bands, in: "dsp").profile?.precompDb == 4)
        try await Self.shoot("setup-latest-cfc-editor-refused", rig: rig, path: editor, atBottom: true)
        await rig.app.disconnect()
    }

    // MARK: The rig

    struct Rig {
        let app: AppModel
        let flow: ConnectionFlow
        let station: FakeStation
        let router: SetupRouter
        let defaults: UserDefaults
        let suite: String
    }

    /// The app connected, through Your Cores, to a fake Core named
    /// KG4VCF/shack that sends Setup descriptions.
    static func connected(additions: FakeStation.Additions = [.bands, .setupDescription]) async throws -> Rig {
        let suite = "SetupDescribedPagesTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        let app = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                           displaySettings: BandDisplaySettingsStore(defaults: defaults),
                           meterSettings: SMeterSettingsStore(defaults: defaults))
        let station = try FakeStation(fixture: "session-device-sign-in", additions: additions,
                                      stationLabel: "KG4VCF/shack")
        let stations = PairedStationStore(item: InMemorySecretItem())
        try stations.save(PairedStation(identityKey: station.identity.publicKey, label: "KG4VCF/shack",
                                        endpoints: [station.endpoint]))
        let flow = ConnectionFlow(app: app, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()), stations: stations, kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
        await flow.connect(to: try #require(flow.cores.first))
        #expect(await settle { flow.screen == .band })
        #expect(app.mirror.capabilityVersion("setupDescriptionVersion") == 11)
        return Rig(app: app, flow: flow, station: station, router: SetupRouter(), defaults: defaults, suite: suite)
    }

    /// Waits for `condition` by yielding to the app's queued work: no clock.
    static func settle(_ condition: () -> Bool) async -> Bool {
        for _ in 0..<200_000 {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }

    private func settle(_ condition: () -> Bool) async -> Bool {
        await Self.settle(condition)
    }

    static func settingsWrite(_ key: String, _ station: FakeStation,
                                      where accept: @escaping @Sendable (LinkMessage.SettingsWrite) -> Bool = { _ in true })
        async throws -> LinkMessage.SettingsWrite {
        let sent = await station.waitForMessage { message in
            if case .settingsWrite(let write) = message { return write.key == key && accept(write) }
            return false
        }
        guard case .settingsWrite(let write)? = sent else { throw SetupRenderingTests.SetupTestError.notSent(key) }
        return write
    }

    private static func propertyWrite(_ key: String, _ station: FakeStation) async throws -> LinkMessage.PropertyWrite {
        let sent = await station.waitForMessage { message in
            if case .propertyWrite(let write) = message { return write.key == key }
            return false
        }
        guard case .propertyWrite(let write)? = sent else { throw SetupRenderingTests.SetupTestError.notSent(key) }
        return write
    }

    /// Draws the Setup tab at `path` in an upright phone's window, `height` points tall.
    static func shoot(_ name: String, rig: Rig, path: [SetupTree.Route], height: CGFloat = 874,
                      width: CGFloat = 402, sideways: Bool = false,
                      dynamicType: DynamicTypeSize = .large, parts: Int = 1,
                      scheme: ColorScheme = .dark, atBottom: Bool = false) async throws {
        let size = CGSize(width: width, height: height)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        rig.router.path = path
        let root = VStack(spacing: 0) {
            SetupTab(app: rig.app, flow: rig.flow, router: rig.router, buildTag: nil)
            TabBar(selection: .constant(.setup), sideways: sideways)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .preferredColorScheme(scheme)
        .environment(\.dynamicTypeSize, dynamicType)
        let host = UIHostingController(rootView: root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        if atBottom, let list = Self.firstScrollView(in: host.view) {
            // The page's last section: the list scrolled to its end. The
            // list measures its rows as they come on screen, so scroll again
            // until its end stops moving.
            for _ in 0..<6 {
                let end = max(-list.adjustedContentInset.top,
                              list.contentSize.height + list.adjustedContentInset.bottom - list.bounds.height)
                if abs(list.contentOffset.y - end) < 1 {
                    break
                }
                list.contentOffset.y = end
                await ShotWait.laidOut(window)
            }
        }
        // A page longer than one picture is drawn in `parts`, the list
        // scrolled down by most of the window between them (a picture over
        // about 8000 pixels tall draws blank).
        for part in 0..<parts {
            if part > 0, let list = Self.firstScrollView(in: host.view) {
                let step = list.bounds.height - 120
                let bottom = max(0, list.contentSize.height + list.adjustedContentInset.bottom - list.bounds.height)
                list.contentOffset.y = min(CGFloat(part) * step - list.adjustedContentInset.top, bottom)
                await ShotWait.laidOut(window)
            }
            let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
                window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
            }
            let file = parts == 1 ? name : "\(name)-\(part + 1)"
            if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
               let data = image.pngData() {
                let url = URL(fileURLWithPath: directory).appendingPathComponent("\(file).png")
                try data.write(to: url)
                print("Wrote \(url.path)")
            }
        }
    }

    /// The first scroll view under `view` that can scroll down: the page's list.
    /// Its insets count: a page under a large title can be shorter than the
    /// window and still run past its bottom by the title's height.
    static func firstScrollView(in view: UIView) -> UIScrollView? {
        if let scroll = view as? UIScrollView,
           scroll.contentSize.height + scroll.adjustedContentInset.top + scroll.adjustedContentInset.bottom
            > scroll.bounds.height {
            return scroll
        }
        for child in view.subviews {
            if let found = firstScrollView(in: child) {
                return found
            }
        }
        return nil
    }
}
