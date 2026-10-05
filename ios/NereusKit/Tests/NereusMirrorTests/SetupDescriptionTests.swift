// NereusSDR for iOS: strict parsing of Setup descriptions received from a Core
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusMirror

@Suite struct SetupDescriptionTests {
    private func resourceControl(_ category: String, _ id: String) throws -> [String: Any] {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/\(category).json").standardizedFileURL
        let data = try Data(contentsOf: source)
        let root = try #require(JSONSerialization.jsonObject(with: data) as? [String: Any])
        for page in try #require(root["pages"] as? [[String: Any]]) {
            for section in try #require(page["sections"] as? [[String: Any]]) {
                for control in try #require(section["controls"] as? [[String: Any]]) {
                    if control["id"] as? String == id { return control }
                }
            }
        }
        throw SetupDescription.ParseError("Missing source control \(id)")
    }
    private func parsedControl(_ raw: [String: Any], category: String, version: Int) throws -> SetupDescription.Control {
        let root: [String: Any] = ["version": version,
            "category": ["id": category, "title": category, "where": "station"],
            "pages": [["id": "\(category).test", "title": "Test", "where": "station",
                       "sections": [["title": "Test", "controls": [raw]]]]]]
        let data = try JSONSerialization.data(withJSONObject: root)
        let parsed = try SetupDescription.parse(json: String(decoding: data, as: UTF8.self))
        return parsed.pages[0].sections[0].controls[0]
    }
    private func document(_ controls: String, version: Int = 1) -> String {
        """
        {"version":\(version),"category":{"id":"sample","title":"Sample","where":"mixed","coverage":"partial"},"pages":[{"id":"sample.main","title":"Main","where":"station","sections":[{"title":"First","controls":[\(controls)]}]}]}
        """
    }

    private func control(_ id: String, kind: String = "toggle", extras: String = "", binding: String = "{\"property\":{\"object\":\"radio\",\"name\":\"enabled\"}}") -> String {
        """
        {"id":"\(id)","label":"Fresh label","tooltip":"Fresh help","kind":"\(kind)","binding":\(binding),"applies":"live"\(extras)}
        """
    }

    @Test func orderedTypedControlsAndMetadata() throws {
        let json = document(control("sample.main.first", extras: ",\"gate\":{\"capability\":\"sampleVersion\",\"min\":2,\"transmit\":true,\"offAir\":true,\"board\":\"sample.present\"}") + "," + control("sample.main.second", kind: "integer", extras: ",\"min\":1,\"max\":9,\"step\":2,\"unit\":\"ticks\""))
        let value = try SetupDescription.parse(json: json)
        #expect(value.category.id == "sample")
        #expect(value.category.coverage == "partial")
        #expect(value.pages[0].sections[0].controls.map(\.id) == ["sample.main.first", "sample.main.second"])
        let first = value.pages[0].sections[0].controls[0]
        #expect(first.unavailableReason == nil)
        #expect(first.gate?.capability == "sampleVersion")
        #expect(first.gate?.minimum == 2)
        #expect(first.gate?.transmit == true)
        #expect(first.gate?.offAir == true)
        #expect(first.gate?.board == "sample.present")
        #expect(value.pages[0].sections[0].controls[1].range?.step == 2)
    }

    @Test func invalidMetadataCannotEnableControls() throws {
        let bad = [
            control("sample.main.one", kind: "future"),
            control("sample.main.two", extras: ",\"gate\":{\"mystery\":true}"),
            control("sample.main.three", extras: ",\"min\":10,\"max\":2,\"step\":1", binding: "{\"setting\":\"Scale\"}"),
            control("sample.main.four", binding: "{\"setting\":\"Key\",\"property\":{\"object\":\"radio\",\"name\":\"enabled\"}}"),
            control("sample.main.five", binding: "{\"command\":{\"verb\":\"setValue\",\"arguments\":{\"value\":{\"$controlValue\":true,\"$row\":\"id\"}}}}"),
            control("sample.main.six", kind: "choice", extras: ",\"choices\":[\"one\",2]"),
            control("sample.main.seven", binding: "{\"property\":{\"object\":\"radio\",\"name\":\"enabled\",\"fallback\":true}}"),
            control("sample.main.eight", extras: ",\"availability\":{\"enabled\":false,\"reason\":\"Awaiting policy\"}"),
            control("sample.main.nine", kind: "slider", extras: ",\"min\":0,\"max\":1,\"step\":2")
        ]
        let value = try SetupDescription.parse(json: document(bad.joined(separator: ",")))
        #expect(value.pages[0].sections[0].controls.allSatisfy { $0.unavailableReason != nil })
    }

    @Test func settingToggleRequiresExactEncodingAndReadoutPrecisionIsBounded() throws {
        let good = control("sample.main.one", extras: ",\"valueEncoding\":{\"true\":\"True\",\"false\":\"False\"}", binding: "{\"setting\":\"SampleEnabled\"}")
        let bad = control("sample.main.two", extras: ",\"valueEncoding\":{\"true\":\"true\",\"false\":\"false\"}", binding: "{\"setting\":\"SampleEnabled\"}")
        let readout = control("sample.main.three", kind: "readout", extras: ",\"decimals\":7")
        let value = try SetupDescription.parse(json: document([good, bad, readout].joined(separator: ",")))
        let controls = value.pages[0].sections[0].controls
        #expect(controls[0].unavailableReason == nil)
        #expect(controls[0].valueEncoding?.trueValue == "True")
        #expect(controls[1].unavailableReason != nil)
        #expect(controls[2].unavailableReason != nil)
        #expect(controls[2].isEditable == false)
    }

    @Test func scalarKindsAndPhoneBindingStayRepresentable() throws {
        let kinds = ["toggle", "integer", "decimal", "slider", "choice", "text", "colour", "button", "readout"]
        let controls = kinds.enumerated().map { index, kind -> String in
            let extras = ["integer", "decimal", "slider"].contains(kind) ? ",\"min\":0,\"max\":10,\"step\":1" : kind == "choice" ? ",\"choices\":[\"low\",\"high\"]" : ""
            return control("sample.main.kind\(index)", kind: kind, extras: extras,
                           binding: kind == "readout" ? "{\"property\":{\"object\":\"radio\",\"name\":\"meter\"}}" : "{\"phone\":\"local/\(index)\"}")
        }
        let parsed = try SetupDescription.parse(json: document(controls.joined(separator: ",")))
        #expect(parsed.pages[0].sections[0].controls.map(\.kind) == kinds.compactMap(SetupDescription.Kind.init(rawValue:)))
        #expect(parsed.pages[0].sections[0].controls.allSatisfy { $0.unavailableReason == nil })
        #expect(parsed.pages[0].sections[0].controls.last?.isEditable == false)
    }

    @Test func malformedCategoryAndUnknownVersionAreRejected() {
        #expect(throws: SetupDescription.ParseError.self) { try SetupDescription.parse(json: "{") }
        #expect(throws: SetupDescription.ParseError.self) { try SetupDescription.parse(json: document(control("sample.main.one"), version: 25)) }
    }

    @Test func commandSourcesStayTypedAndUnknownSourcesDisable() throws {
        let command = """
        {"command":{"verb":"setSample","valueProperty":{"object":"radio","name":"level"},"arguments":{"value":{"$controlValue":true},"peer":{"$property":{"object":"radio","name":"peer"}},"mode":"slow","count":2,"ratio":0.5,"armed":true}}}
        """
        let valid = control("sample.main.command", kind: "integer", extras: ",\"min\":0,\"max\":10,\"step\":1", binding: command)
        let invalid = control("sample.main.bad", kind: "button", binding: "{\"command\":{\"verb\":\"run\",\"arguments\":{\"value\":{\"$future\":true}}}}")
        let parsed = try SetupDescription.parse(json: document(valid + "," + invalid))
        let controls = parsed.pages[0].sections[0].controls
        if case .command(let value)? = controls[0].binding {
            #expect(value.valueProperty?.name == "level")
            #expect(value.arguments["value"] == .controlValue)
            #expect(value.arguments["peer"] == .property(.init(object: "radio", name: "peer")))
            #expect(value.arguments["armed"] == .literal(.bool(true)))
            #expect(value.arguments["count"] == .literal(.integer(2)))
            #expect(value.arguments["ratio"] == .literal(.decimal(0.5)))
        } else { Issue.record("Valid command was not typed") }
        #expect(controls[1].unavailableReason != nil)
    }

    @Test func v2TnfTableHasClosedColumnsAndActions() throws {
        let table = """
        {"id":"dsp.tnf.list","label":"Notches","tooltip":"","kind":"table","requiresDescriptionVersion":2,"binding":{"table":{"valueProperty":{"object":"notches","name":"listJson"},"revisionProperty":{"object":"notches","name":"revision"},"format":"json-array","rowKey":"id","maxRows":1024}},"applies":"live","gate":{"capability":"notchControlVersion","min":1},"columns":[{"id":"dsp.tnf.list.centreHz","field":"centreHz","label":"Centre","tooltip":"","kind":"decimal","min":100000,"max":61440000,"step":1},{"id":"dsp.tnf.list.widthHz","field":"widthHz","label":"Width","tooltip":"","kind":"decimal","min":0,"max":10000,"step":1},{"id":"dsp.tnf.list.active","field":"active","label":"Active","tooltip":"","kind":"toggle"},{"id":"dsp.tnf.list.delete","rowAction":"delete","label":"Delete","tooltip":"","kind":"button"}],"rowActions":[{"id":"move","command":{"verb":"notch.move","arguments":{"id":{"$row":"id"},"centreHz":{"$edit":"centreHz"},"widthHz":{"$edit":"widthHz"}}}},{"id":"active","command":{"verb":"notch.setActive","arguments":{"id":{"$row":"id"},"active":{"$edit":"active"}}}},{"id":"delete","command":{"verb":"notch.delete","arguments":{"id":{"$row":"id"}}}}]}
        """
        let json = """
        {"version":2,"category":{"id":"dsp","title":"DSP","where":"station"},"pages":[{"id":"dsp.tnf","title":"Notches","where":"station","sections":[{"title":"List","controls":[\(table)]}]}]}
        """
        let parsed = try SetupDescription.parse(json: json)
        let control = parsed.pages[0].sections[0].controls[0]
        #expect(control.unavailableReason == nil)
        #expect(control.isEditable)
        if case .table(let value)? = control.binding {
            #expect(value.maxRows == 1024)
            #expect(value.columns.map(\.field) == ["centreHz", "widthHz", "active", nil])
            #expect(value.rowActions[0].command.arguments["id"] == .row("id"))
        } else { Issue.record("Valid table was not typed") }
        let changed = json.replacingOccurrences(of: "\"$row\":\"id\"", with: "\"$row\":\"index\"")
        #expect(try SetupDescription.parse(json: changed).pages[0].sections[0].controls[0].unavailableReason != nil)
        let oversized = json.replacingOccurrences(of: "\"maxRows\":1024", with: "\"maxRows\":1025")
        #expect(try SetupDescription.parse(json: oversized).pages[0].sections[0].controls[0].unavailableReason != nil)
        let unknownColumn = json.replacingOccurrences(of: "dsp.tnf.list.active", with: "dsp.tnf.list.other")
        #expect(try SetupDescription.parse(json: unknownColumn).pages[0].sections[0].controls[0].unavailableReason != nil)
        let weakGate = json.replacingOccurrences(of: "\"min\":1},\"columns\"", with: "\"min\":0},\"columns\"")
        #expect(try SetupDescription.parse(json: weakGate).pages[0].sections[0].controls[0].unavailableReason != nil)
        let missingVersion = json.replacingOccurrences(of: "\"requiresDescriptionVersion\":2,", with: "")
        #expect(try SetupDescription.parse(json: missingVersion).pages[0].sections[0].controls[0].unavailableReason != nil)
    }

    @Test func currentCoreCategoryShapesSurviveWireProjection() throws {
        // Read the Core's source files at test time. The Core fills each
        // row it holds in the resource's form (rangeSource, the Radio Info
        // values, the Watt Meter points, telemetryFor, boardFamily) for its
        // radio, and its peer projection removes the rows newer than the
        // session's version (SetupDescription::fitCategoryForVersion).
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup").standardizedFileURL
        let names = ["general", "hardware", "audio", "dsp", "transmit", "test", "catNetwork",
                     "pa", "diagnostics", "display", "appearance"]
        var rejected: [String] = []
        for name in names {
            let data = try Data(contentsOf: source.appendingPathComponent("\(name).json"))
            for peerVersion in 1...24 {
                var root = try #require(JSONSerialization.jsonObject(with: data) as? [String: Any])
                var pages = try #require(root["pages"] as? [[String: Any]])
                var projectedPages: [[String: Any]] = []
                var includedIds: Set<String> = []
                let fifteen = ["dsp", "transmit", "audio", "diagnostics", "catNetwork"].contains(name)
                for var page in pages {
                    var sections: [[String: Any]] = []
                    for var section in try #require(page["sections"] as? [[String: Any]]) {
                        // The Core keeps a board family's section for its own radio.
                        section.removeValue(forKey: "boardFamily")
                        var controls: [[String: Any]] = []
                        for var control in try #require(section["controls"] as? [[String: Any]]) {
                            if name == "hardware", peerVersion < 18,
                               let id = control["id"] as? String, let closed = Self.hardwareV16ClockRows[id] {
                                // A peer below 18 gets the version 16 clock row, closed.
                                control = closed
                            }
                            if name == "audio", peerVersion < 24,
                               control["id"] as? String == "audio.txInput.hermesLineInGain" {
                                // A peer below 24 keeps the version 15 Line In Gain:
                                // whole decibels from -34.
                                control["requiresDescriptionVersion"] = 15
                                control["min"] = -34
                                control["step"] = 1
                                control.removeValue(forKey: "decimals")
                            }
                            if (control["requiresDescriptionVersion"] as? Int ?? 1) > peerVersion {
                                continue
                            }
                            if name == "catNetwork", peerVersion < 21 {
                                // CAT & Network 21 greys TCI's Forget row out while
                                // Duplicate is off: below 21 the row is always enabled.
                                control.removeValue(forKey: "enabledWhen")
                            }
                            if name == "dsp", peerVersion >= 22, let id = control["id"] as? String,
                               Self.rxBufferRows.contains(id) {
                                // DSP 22 on the air: the RX buffer rows say why they are locked.
                                control["availability"] = ["enabled": false, "reason": Self.onAirReason]
                            }
                            if name == "pa", peerVersion >= 20, let id = control["id"] as? String, id.hasPrefix("pa.gain."),
                               control["requiresDescriptionVersion"] as? Int == 14 {
                                // PA 20 on the air: the table's gate opens and each row says why it is locked.
                                if id == "pa.gain.table", var gate = control["gate"] as? [String: Any] {
                                    gate.removeValue(forKey: "offAir")
                                    control["gate"] = gate
                                } else {
                                    control["availability"] = ["enabled": false, "reason": Self.onAirReason]
                                }
                            }
                            Self.fillAsTheCore(&control, category: name)
                            if name == "general", control.removeValue(forKey: "rangeSource") != nil {
                                // One of the current Core board rows has this range.
                                control["min"] = 0
                                control["max"] = 31
                                control["step"] = 1
                            }
                            if peerVersion < 4 && (name == "display" || name == "appearance") {
                                control.removeValue(forKey: "default")
                                if control["kind"] as? String == "decimal" {
                                    control.removeValue(forKey: "decimals")
                                }
                            }
                            if let id = control["id"] as? String { includedIds.insert(id) }
                            controls.append(control)
                        }
                        if !controls.isEmpty {
                            section["controls"] = controls
                            sections.append(section)
                        }
                    }
                    if !sections.isEmpty {
                        page["sections"] = sections
                        if let coverage = page.removeValue(forKey: "coverageV15") as? String, peerVersion >= 15 {
                            if coverage.isEmpty { page.removeValue(forKey: "coverage") } else { page["coverage"] = coverage }
                        }
                        // The version 19 wording (DSP's CFC editor) only to a peer at 19.
                        if let coverage = page.removeValue(forKey: "coverageV19") as? String, peerVersion >= 19 {
                            if coverage.isEmpty { page.removeValue(forKey: "coverage") } else { page["coverage"] = coverage }
                        }
                        projectedPages.append(page)
                    }
                }
                if let coverage = root.removeValue(forKey: "coverageV15") as? String, peerVersion >= 15 {
                    root["coverage"] = coverage
                }
                if let coverage = root.removeValue(forKey: "coverageV19") as? String, peerVersion >= 19 {
                    root["coverage"] = coverage
                }
                if name == "dsp" {
                    // CFC's band editor comes at 19.
                    #expect(includedIds.contains("dsp.cfc.bands") == (peerVersion >= 19))
                    #expect(includedIds.contains("dsp.tnf.list") == (peerVersion >= 2))
                    #expect(includedIds.contains("dsp.tnf.add") == (peerVersion >= 2))
                }
                if name == "hardware" {
                    // The low-pass rows from 17; the clock rows closed from 16.
                    #expect(includedIds.contains("hardware.alex1Filters.lpf.160m.start") == (peerVersion >= 17))
                    #expect(includedIds.contains("hardware.alex1Filters.lpfBypass") == (peerVersion >= 17))
                    #expect(includedIds.contains("hardware.hl2Io.cl2Freq") == (peerVersion >= 16))
                }
                if name == "catNetwork" {
                    // The three TCI RX2 VFO rows reach every peer from 1, none of
                    // them closed; Forget's enabledWhen only at 21.
                    for id in ["catNetwork.tciServer.core.forgetRx2VfoBOnDisconnect",
                               "catNetwork.tciServer.core.useRx1VfoaForRx2Vfoa",
                               "catNetwork.tciServer.core.copyRx2VfobToVfoa"] {
                        #expect(includedIds.contains(id), "\(id) v\(peerVersion)")
                    }
                }
                if name == "audio" {
                    // The Saturn G2's Mic Tip-Ring comes at 24; Line In Gain from 15 at every version.
                    #expect(includedIds.contains("audio.txInput.saturnMicTipRing") == (peerVersion >= 24))
                    #expect(includedIds.contains("audio.txInput.hermesLineInGain") == (peerVersion >= 15))
                }
                if name == "pa" {
                    #expect(includedIds.contains("pa.values.paCurrent") == (peerVersion >= 5))
                    #expect(includedIds.contains("pa.values.dcVoltage") == (peerVersion >= 5))
                }
                pages = projectedPages
                if pages.isEmpty { continue }
                root["pages"] = pages
                // The Core's category versions: hardware changed at 6, 13, 16,
                // 17, 18 and 23, PA at 5, 13, 14 and 20, transmit at 13 and 15,
                // DSP at 15, 19 and 22, CAT & Network at 15 and 21, audio at
                // 15 and 24, and diagnostics at 15.
                let fitted = name == "hardware"
                    ? (peerVersion < 13 ? min(peerVersion, 6) : peerVersion < 16 ? 13 : peerVersion < 23 ? min(peerVersion, 18) : 23)
                    : name == "pa" ? (peerVersion < 13 ? min(peerVersion, 5) : peerVersion < 20 ? min(peerVersion, 14) : 20)
                    : name == "transmit" ? (peerVersion < 13 ? min(peerVersion, 3) : peerVersion < 15 ? 13 : 15)
                    : name == "dsp" ? (peerVersion < 15 ? min(peerVersion, 3) : peerVersion < 19 ? 15 : peerVersion < 22 ? 19 : 22)
                    : name == "catNetwork" ? (peerVersion < 15 ? min(peerVersion, 3) : peerVersion < 21 ? 15 : 21)
                    : name == "audio" ? (peerVersion < 15 ? min(peerVersion, 3) : peerVersion < 24 ? 15 : 24)
                    : fifteen ? (peerVersion < 15 ? min(peerVersion, 3) : 15)
                    : name == "appearance" ? (peerVersion < 7 ? min(peerVersion, 4) : peerVersion < 12 ? 7 : 12)
                    : name == "display" ? (peerVersion < 8 ? min(peerVersion, 4) : min(peerVersion, 12))
                    : min(peerVersion, 3)
                root["version"] = fitted
                if name == "display" {
                    // The Core's own counts: 11, 14, 21, 29, 33, 48 and 100 controls.
                    let expected = peerVersion < 4 ? 11 : peerVersion < 8 ? 14 : peerVersion == 8 ? 21
                        : peerVersion == 9 ? 29 : peerVersion == 10 ? 33 : peerVersion == 11 ? 48 : 100
                    #expect(includedIds.count == expected, "display v\(peerVersion)")
                    #expect(includedIds.contains("display.spectrumPeaks.activePeakHold") == (peerVersion >= 11))
                    #expect(includedIds.contains("display.waterfallDefaults.showTxFilter") == (peerVersion >= 10))
                    #expect(includedIds.contains("display.spectrumDefaults.panFill") == (peerVersion >= 9))
                    #expect(includedIds.contains("display.gridScales.showGrid") == (peerVersion >= 12))
                }
                let projected = try JSONSerialization.data(withJSONObject: root)
                do {
                    let parsed = try SetupDescription.parse(json: String(decoding: projected, as: UTF8.self))
                    let parsedControls = parsed.pages.flatMap { $0.sections.flatMap(\.controls) }
                    if name == "dsp", peerVersion >= 22 {
                        for control in parsedControls where Self.rxBufferRows.contains(control.id) {
                            #expect(control.unavailableReason == Self.onAirReason, "\(control.id)")
                        }
                    }
                    if name == "hardware", let clock = parsedControls.first(where: { $0.id == "hardware.hl2Io.cl2Freq" }) {
                        // From 18 the clock rows are live and CL2's frequency follows Enable CL2.
                        #expect((clock.availability?.enabled == false) == (peerVersion < 18), "cl2Freq v\(peerVersion)")
                        #expect((clock.modern?.radioSettingDependency != nil) == (peerVersion >= 18))
                    }
                    if name == "hardware", peerVersion >= 13 {
                        // Calibration's Level Cal section and its Rx1 6m LNA row come at 23.
                        let calibration = parsed.pages.first { $0.id == "hardware.calibration" }
                        #expect(parsedControls.contains { $0.id == "hardware.calibration.rx1_6mLna" } == (peerVersion >= 23),
                                "rx1_6mLna v\(peerVersion)")
                        #expect(calibration?.sections.contains { $0.title == "Level Cal" } == (peerVersion >= 23),
                                "Level Cal v\(peerVersion)")
                    }
                    if name == "audio", let gain = parsedControls.first(where: {
                        $0.id == "audio.txInput.hermesLineInGain" }) {
                        // Line In Gain in the radio's 1.5 dB steps from 24.
                        #expect(gain.range?.step == (peerVersion >= 24 ? 1.5 : 1), "lineInGain v\(peerVersion)")
                        #expect(gain.decimals == (peerVersion >= 24 ? 1 : nil), "lineInGain v\(peerVersion)")
                    }
                    if name == "catNetwork", let forget = parsedControls.first(where: {
                        $0.id == "catNetwork.tciServer.core.forgetRx2VfoBOnDisconnect" }) {
                        #expect((forget.modern?.propertyDependency != nil) == (peerVersion >= 21), "forget v\(peerVersion)")
                    }
                    for page in parsed.pages {
                        for section in page.sections {
                            for control in section.controls {
                                if let reason = control.metadataIssue {
                                    rejected.append("\(name) v\(peerVersion) \(control.id): \(reason)")
                                }
                            }
                        }
                    }
                } catch let error as SetupDescription.ParseError {
                    rejected.append("\(name) v\(peerVersion): \(error.reason)")
                }
            }
        }
        #expect(rejected.isEmpty, "Rejected current Core controls: \(rejected.joined(separator: "; "))")
    }

    /// DSP > Options' four RX buffer rows, which lock on the air from DSP 22.
    static let rxBufferRows: Set<String> = ["dsp.options.DspOptionsBufferSizePhoneRx", "dsp.options.DspOptionsBufferSizeFmRx",
                                            "dsp.options.DspOptionsBufferSizeCwRx", "dsp.options.DspOptionsBufferSizeDigRx"]
    /// The Core's on-air lock reason (RadioModel::paOnAirLockedReason and
    /// dspBufferOnAirLockedReason).
    static let onAirReason = "Can't change while transmitting."

    /// The three HL2 clock rows as the Core sends them to a peer below
    /// description 18: the version 16 rows, closed with the Core's reason
    /// (the Core's kHardwareV16Controls). The resource file holds the
    /// version 18 rows.
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

    /// A row as the Core fills it for a connected radio (loadCategory and
    /// SetupDescriptionV15::projectForRadio): a sample board's values.
    static func fillAsTheCore(_ control: inout [String: Any], category: String) {
        let binding = control["binding"] as? [String: Any] ?? [:]
        if category == "hardware" {
            if control["copyText"] != nil {
                control["copyText"] = "Board: Sample"
            } else if control["id"] as? String == "hardware.radioInfo.sampleRate" {
                control["options"] = [["value": 192000, "label": "192000"]]
            } else if binding["radioInfo"] != nil {
                control["value"] = "Sample"
            }
        }
        if category == "pa", control["rangeSource"] != nil,
           (binding["radioSetting"] as? String)?.hasPrefix("paCalibration/calPoint") == true {
            control.removeValue(forKey: "rangeSource")
            control["label"] = "10 W"
            control["min"] = 0
            control["max"] = 100
            control["step"] = 0.1
            control["decimals"] = 1
            control["default"] = 10.0
            control["boardClass"] = 1
        }
        if control["rangeSource"] as? String == "attOnTx" {
            control.removeValue(forKey: "rangeSource")
            control["min"] = 0
            control["max"] = 31
            control["step"] = 1
        }
        if control.removeValue(forKey: "telemetryFor") != nil {
            control["binding"] = ["telemetry": ["object": "radio", "name": "paVolts"]]
        }
        control.removeValue(forKey: "availableOn")
    }

    /// The settings check's middle action as a settingsHygieneVersion 1
    /// Core describes it.
    static var legacyReset: [String: Any] {
        ["id": "reset", "label": "Reset to Defaults", "enabled": false,
         "reason": "Reset to defaults is not available on this Core."]
    }

    @Test func closedExtensionsRetainTypedSourceAndRejectAlterations() throws {
        let hygiene = try resourceControl("diagnostics", "diagnostics.settingsValidation.health")
        let parsedHygiene = try parsedControl(hygiene, category: "diagnostics", version: 3)
        #expect(parsedHygiene.metadataIssue == nil)
        #expect(parsedHygiene.hygieneActions?.map(\.id) == ["validate", "repair", "forget"])
        #expect(parsedHygiene.hygieneActions?.last?.confirmation?.defaultAction == "cancel")
        // Repair, as the Core words it: paired, off the air, from
        // settingsHygieneVersion 2, with its own question.
        let repair = try #require(parsedHygiene.hygieneActions?[1])
        #expect(repair.label == "Repair Invalid Settings" && repair.enabled && repair.paired && repair.offAir)
        #expect(repair.minimumVersion == 2)
        #expect(repair.reason == "Repair invalid settings is not available on this Core. Updating the Core may help.")
        #expect(repair.confirmation == SetupDescription.Confirmation(
            title: "Repair Settings", message: "Repair the settings that are invalid for this radio?",
            defaultAction: "cancel"))
        var bad = hygiene
        bad["actions"] = [["id": "forget", "label": "Forget This Radio"]]
        #expect(try parsedControl(bad, category: "diagnostics", version: 3).unavailableReason != nil)
        // A repair worded, gated or asked differently is not read in part.
        var actions = try #require(hygiene["actions"] as? [[String: Any]])
        for change in ["label", "gate", "reason", "confirmation", "paired"] {
            var altered = actions
            switch change {
            case "label": altered[1]["label"] = "Repair"
            case "gate": altered[1]["gate"] = ["capability": "settingsHygieneVersion", "min": 1]
            case "reason": altered[1]["reason"] = "Not here."
            case "confirmation": altered[1]["confirmation"] = ["title": "Repair Settings",
                                                              "message": "Repair?", "default": "cancel"]
            default: altered[1]["paired"] = false
            }
            bad = hygiene
            bad["actions"] = altered
            #expect(try parsedControl(bad, category: "diagnostics", version: 3).unavailableReason != nil, "\(change)")
        }
        // An older Core's Reset, greyed with its reason, still reads.
        actions[1] = SetupDescriptionTests.legacyReset
        bad = hygiene
        bad["actions"] = actions
        let legacy = try parsedControl(bad, category: "diagnostics", version: 3)
        #expect(legacy.metadataIssue == nil)
        #expect(legacy.hygieneActions?.map(\.id) == ["validate", "reset", "forget"])
        #expect(legacy.hygieneActions?[1].enabled == false && legacy.hygieneActions?[1].minimumVersion == nil)
        #expect(legacy.hygieneActions?[1].reason == "Reset to defaults is not available on this Core.")

        let fft = try resourceControl("display", "display.spectrumDefaults.fftSize")
        let parsedFFT = try parsedControl(fft, category: "display", version: 4)
        #expect(parsedFFT.metadataIssue == nil)
        #expect(parsedFFT.options?.map(\.value) == [4096, 8192, 16384, 32768, 65536, 131072, 262144])
        #expect(parsedFFT.defaultValue == .integer(4096))
        bad = fft
        var options = try #require(bad["options"] as? [[String: Any]])
        options.swapAt(0, 1)
        bad["options"] = options
        #expect(try parsedControl(bad, category: "display", version: 4).unavailableReason != nil)
        bad = fft; bad["default"] = 0
        #expect(try parsedControl(bad, category: "display", version: 4).unavailableReason != nil)

        let normalize = try resourceControl("display", "display.txDisplay.panNormalize")
        #expect(try parsedControl(normalize, category: "display", version: 4).enabledWhen?.oneOf == ["2", "3", "4"])
        bad = normalize
        bad["enabledWhen"] = ["setting": "DisplayTxPanDetector", "oneOf": ["2", "3", "5"]]
        #expect(try parsedControl(bad, category: "display", version: 4).unavailableReason != nil)

        let colour = try resourceControl("appearance", "appearance.colorsTheme.traceFillColor")
        #expect(try parsedControl(colour, category: "appearance", version: 7).defaultValue == .text("#00E5FFFF"))
        bad = colour; bad["default"] = "#00E5FF"
        #expect(try parsedControl(bad, category: "appearance", version: 7).unavailableReason != nil)

        let telemetry = try resourceControl("pa", "pa.values.paCurrent")
        let parsedTelemetry = try parsedControl(telemetry, category: "pa", version: 5)
        #expect(parsedTelemetry.metadataIssue == nil)
        #expect(parsedTelemetry.binding == .telemetry(.init(object: "radio", name: "paCurrentAmps")))
        bad = telemetry; bad["binding"] = ["telemetry": ["object": "txState", "name": "paCurrentAmps"]]
        #expect(try parsedControl(bad, category: "pa", version: 5).unavailableReason != nil)

        let antenna = try resourceControl("hardware", "hardware.antenna.rxRows")
        let parsedAntenna = try parsedControl(antenna, category: "hardware", version: 6)
        #expect(parsedAntenna.metadataIssue == nil)
        #expect(!parsedAntenna.isEditable)
        if case .antennaRows(let rows)? = parsedAntenna.binding {
            // The Core's resource carries 2 m's row last, for a phone that declares band2m.
            #expect(rows.rows.count == 15)
            #expect(rows.rows.map(\.band) == Array(0...13) + [SetupDescription.band2m])
            #expect(rows.rows.last?.label == "2m")
            #expect(rows.columns.map(\.id) == ["rx1", "rx2", "rx3", "rxOnly1", "rxOnly2", "rxOnly3"])
            #expect(rows.columnGroups.map(\.label) == ["RX1", "RX-only"])
        } else { Issue.record("Antenna rows were not typed") }
        // A Core that sends no band2mVersion leaves 2 m's row out: the 14 rows are read as before.
        var fourteen = antenna
        fourteen["rows"] = Array(try #require(antenna["rows"] as? [[String: Any]]).prefix(14))
        if case .antennaRows(let rows)? = try parsedControl(fourteen, category: "hardware", version: 6).binding {
            #expect(rows.rows.map(\.band) == Array(0...13))
        } else { Issue.record("Fourteen antenna rows were not typed") }
        // 2 m's row is the fifteenth or none: out of place, renumbered or relabelled, the table is refused.
        var rawRows = try #require(antenna["rows"] as? [[String: Any]])
        var moved = antenna
        moved["rows"] = [rawRows[14]] + Array(rawRows.prefix(14))
        #expect(try parsedControl(moved, category: "hardware", version: 6).unavailableReason != nil)
        var renumbered = antenna
        rawRows[14]["band"] = 11
        renumbered["rows"] = rawRows
        #expect(try parsedControl(renumbered, category: "hardware", version: 6).unavailableReason != nil)
        var extra = antenna
        extra["rows"] = try #require(antenna["rows"] as? [[String: Any]]) + [rawRows[13]]
        #expect(try parsedControl(extra, category: "hardware", version: 6).unavailableReason != nil)
        bad = antenna
        var groups = try #require(bad["columnGroups"] as? [[String: Any]])
        groups.swapAt(0, 1); bad["columnGroups"] = groups
        #expect(try parsedControl(bad, category: "hardware", version: 6).unavailableReason != nil)

        let meter = try resourceControl("appearance", "appearance.meterStyles.face")
        #expect(try parsedControl(meter, category: "appearance", version: 7).options?.count == 7)
        bad = meter; bad["default"] = 6
        #expect(try parsedControl(bad, category: "appearance", version: 7).unavailableReason != nil)

        let detector = try resourceControl("display", "display.spectrumDefaults.detector")
        let parsedDetector = try parsedControl(detector, category: "display", version: 8)
        #expect(parsedDetector.metadataIssue == nil)
        #expect(parsedDetector.options?.map(\.label) == ["Peak", "Rosenfell", "Average", "Sample", "RMS"])
        bad = detector; bad["gate"] = ["capability": "remoteMediaVersion", "min": 2]
        #expect(try parsedControl(bad, category: "display", version: 8).unavailableReason != nil)
        bad = detector; bad["future"] = true
        #expect(try parsedControl(bad, category: "display", version: 8).unavailableReason != nil)
    }

    @Test func v9RenderingAndV10OverlayRowsAreClosedPhoneBindings() throws {
        let expected: [(String, String, SetupDescription.Kind, SetupDescription.Applies, Int, SetupDescription.Literal)] = [
            ("display.spectrumDefaults.panFill", "DisplayPanFill", .toggle, .live, 9, .bool(true)),
            ("display.spectrumDefaults.fillAlpha", "DisplayFftFillAlpha", .slider, .live, 9, .integer(70)),
            ("display.spectrumDefaults.gradient", "DisplayGradientEnabled", .toggle, .live, 9, .bool(false)),
            ("display.spectrumDefaults.peakHold", "DisplayPeakHoldEnabled", .toggle, .live, 9, .bool(false)),
            ("display.spectrumDefaults.peakDelay", "DisplayPeakHoldResetMs", .integer, .live, 9, .integer(2000)),
            ("display.waterfallDefaults.updatePeriod", "DisplayWfUpdatePeriodMs", .slider, .subscription, 9, .integer(30)),
            ("display.waterfallDefaults.stopOnTx", "WaterfallStopOnTx", .toggle, .live, 9, .bool(false)),
            ("display.waterfallDefaults.opacity", "DisplayWfOpacity", .slider, .live, 9, .integer(100)),
            ("display.waterfallDefaults.showRxFilter", "DisplayShowRxFilterOnWaterfall", .toggle, .live, 10, .bool(false)),
            ("display.waterfallDefaults.showTxFilter", "DisplayShowTxFilterOnRxWaterfall", .toggle, .live, 10, .bool(true)),
            ("display.waterfallDefaults.showRxZeroLine", "DisplayShowRxZeroLine", .toggle, .live, 10, .bool(false)),
            ("display.waterfallDefaults.showTxZeroLine", "DisplayShowTxZeroLine", .toggle, .live, 10, .bool(false)),
        ]
        for (id, phone, kind, applies, version, value) in expected {
            let raw = try resourceControl("display", id)
            let parsed = try parsedControl(raw, category: "display", version: 10)
            #expect(parsed.metadataIssue == nil, "\(id)")
            #expect(parsed.binding == .phone(phone))
            #expect(parsed.kind == kind && parsed.applies == applies)
            #expect(parsed.requiresDescriptionVersion == version)
            #expect(parsed.defaultValue == value)
            #expect(parsed.isEditable)
            // Below its own version it is refused.
            #expect(try parsedControl(raw, category: "display", version: version - 1).metadataIssue != nil, "\(id)")
        }
        let alpha = try parsedControl(try resourceControl("display", "display.spectrumDefaults.fillAlpha"),
                                      category: "display", version: 9)
        #expect(alpha.range == .init(minimum: 0, maximum: 100, step: 1) && alpha.unit == "%")
        let delay = try parsedControl(try resourceControl("display", "display.spectrumDefaults.peakDelay"),
                                      category: "display", version: 9)
        #expect(delay.range == .init(minimum: 100, maximum: 10000, step: 100) && delay.unit == "ms")

        // Every alteration is refused with a reason, never guessed.
        let fill = try resourceControl("display", "display.spectrumDefaults.fillAlpha")
        let overlay = try resourceControl("display", "display.waterfallDefaults.showTxFilter")
        var alterations: [[String: Any]] = []
        var bad = fill; bad["binding"] = ["phone": "DisplayFillAlpha"]; alterations.append(bad)
        bad = fill; bad["max"] = 255; alterations.append(bad)
        bad = fill; bad["default"] = 0.7; alterations.append(bad)
        bad = fill; bad["unit"] = "percent"; alterations.append(bad)
        bad = fill; bad["applies"] = "subscription"; alterations.append(bad)
        bad = fill; bad["gate"] = ["capability": "displayExtrasVersion", "min": 1]; alterations.append(bad)
        bad = fill; bad["kind"] = "integer"; alterations.append(bad)
        bad = fill; bad["decimals"] = 1; alterations.append(bad)
        bad = overlay; bad["default"] = false; alterations.append(bad)
        bad = overlay; bad["default"] = "True"; alterations.append(bad)
        bad = overlay; bad["binding"] = ["setting": "DisplayShowTxFilterOnRxWaterfall"]; alterations.append(bad)
        bad = overlay; bad["requiresDescriptionVersion"] = 9; alterations.append(bad)
        bad = overlay; bad["options"] = [["value": 0, "label": "Off"]]; alterations.append(bad)
        bad = overlay; bad.removeValue(forKey: "default"); alterations.append(bad)
        for altered in alterations {
            let parsed = try parsedControl(altered, category: "display", version: 10)
            #expect(parsed.metadataIssue != nil && parsed.unavailableReason != nil && !parsed.isEditable,
                    "\(altered)")
        }
        // A V10 row outside Display is not one of these.
        #expect(try parsedControl(overlay, category: "general", version: 10).metadataIssue != nil)
    }

    @Test func v11SpectrumPeaksRowsAreClosedPhoneBindingsGatedOnTheExtras() throws {
        // Every row of the Core's Spectrum Peaks page, in its order: phone
        // key, kind, apply mode, the display extras version it needs, and
        // its default.
        let expected: [(String, String, SetupDescription.Kind, SetupDescription.Applies, Int64,
                        SetupDescription.Literal)] = [
            ("activePeakHold", "DisplayActivePeakHoldEnabled", .toggle, .subscription, 1, .bool(false)),
            ("activePeakHoldTime", "DisplayActivePeakHoldDurationMs", .integer, .subscription, 3, .integer(2000)),
            ("activePeakHoldDropRate", "DisplayActivePeakHoldDropDbPerSec", .integer, .subscription, 1, .integer(6)),
            ("activePeakHoldFill", "DisplayActivePeakHoldFill", .toggle, .live, 1, .bool(false)),
            ("activePeakHoldOnTx", "DisplayActivePeakHoldOnTx", .toggle, .subscription, 3, .bool(false)),
            ("activePeakHoldColor", "DisplayActivePeakHoldColor", .colour, .live, 1, .text("#FFD700FF")),
            ("peakBlobs", "DisplayPeakBlobsEnabled", .toggle, .subscription, 1, .bool(false)),
            ("peakBlobCount", "DisplayPeakBlobsCount", .integer, .subscription, 1, .integer(3)),
            ("peakBlobInsideFilter", "DisplayPeakBlobsInsideFilterOnly", .toggle, .subscription, 1, .bool(false)),
            ("peakBlobHold", "DisplayPeakBlobsHoldEnabled", .toggle, .subscription, 1, .bool(false)),
            ("peakBlobHoldTime", "DisplayPeakBlobsHoldMs", .integer, .subscription, 1, .integer(500)),
            ("peakBlobHoldDrop", "DisplayPeakBlobsHoldDrop", .toggle, .subscription, 1, .bool(false)),
            ("peakBlobFallRate", "DisplayPeakBlobsFallDbPerSec", .integer, .subscription, 1, .integer(6)),
            ("peakBlobColor", "DisplayPeakBlobColor", .colour, .live, 1, .text("#FF4500FF")),
            ("peakBlobTextColor", "DisplayPeakBlobTextColor", .colour, .live, 1, .text("#7FFF00FF")),
        ]
        for (suffix, phone, kind, applies, minimum, value) in expected {
            let id = "display.spectrumPeaks.\(suffix)"
            let raw = try resourceControl("display", id)
            let parsed = try parsedControl(raw, category: "display", version: 11)
            #expect(parsed.metadataIssue == nil, "\(id)")
            #expect(parsed.binding == .phone(phone), "\(id)")
            #expect(parsed.kind == kind && parsed.applies == applies, "\(id)")
            #expect(parsed.requiresDescriptionVersion == 11)
            #expect(parsed.gate?.capability == "displayExtrasVersion" && parsed.gate?.minimum == minimum, "\(id)")
            #expect(parsed.defaultValue == value, "\(id)")
            #expect(parsed.isEditable)
            // A V10 description cannot carry it.
            #expect(try parsedControl(raw, category: "display", version: 10).metadataIssue != nil, "\(id)")
        }
        let hold = try parsedControl(try resourceControl("display", "display.spectrumPeaks.activePeakHoldTime"),
                                     category: "display", version: 11)
        #expect(hold.range == .init(minimum: 100, maximum: 60000, step: 100) && hold.unit == "ms")
        let count = try parsedControl(try resourceControl("display", "display.spectrumPeaks.peakBlobCount"),
                                      category: "display", version: 11)
        #expect(count.range == .init(minimum: 1, maximum: 20, step: 1) && count.unit == nil)
        let fall = try parsedControl(try resourceControl("display", "display.spectrumPeaks.peakBlobFallRate"),
                                     category: "display", version: 11)
        #expect(fall.range == .init(minimum: 1, maximum: 60, step: 1) && fall.unit == "dB/s")

        // Every alteration is refused with a reason, never guessed.
        let time = try resourceControl("display", "display.spectrumPeaks.activePeakHoldTime")
        let colour = try resourceControl("display", "display.spectrumPeaks.peakBlobColor")
        let toggle = try resourceControl("display", "display.spectrumPeaks.peakBlobHoldDrop")
        var alterations: [[String: Any]] = []
        var bad = time; bad["gate"] = ["capability": "displayExtrasVersion", "min": 1]; alterations.append(bad)
        bad = time; bad["gate"] = ["capability": "remoteMediaVersion", "min": 3]; alterations.append(bad)
        bad = time; bad.removeValue(forKey: "gate"); alterations.append(bad)
        bad = time; bad["max"] = 10000; alterations.append(bad)
        bad = time; bad["default"] = 1000; alterations.append(bad)
        bad = time; bad["unit"] = "s"; alterations.append(bad)
        bad = time; bad["applies"] = "live"; alterations.append(bad)
        bad = time; bad["binding"] = ["setting": "DisplayActivePeakHoldDurationMs"]; alterations.append(bad)
        bad = colour; bad["default"] = "#FF4500"; alterations.append(bad)
        bad = colour; bad["default"] = "#FF0000FF"; alterations.append(bad)
        bad = colour; bad["kind"] = "text"; alterations.append(bad)
        bad = toggle; bad["default"] = true; alterations.append(bad)
        bad = toggle; bad["binding"] = ["phone": "DisplayPeakBlobsFall"]; alterations.append(bad)
        bad = toggle; bad["requiresDescriptionVersion"] = 10; alterations.append(bad)
        bad = toggle; bad["gate"] = ["capability": "displayExtrasVersion", "min": 1, "offAir": true]
        alterations.append(bad)
        for altered in alterations {
            let parsed = try parsedControl(altered, category: "display", version: 11)
            #expect(parsed.metadataIssue != nil && parsed.unavailableReason != nil && !parsed.isEditable,
                    "\(altered)")
        }
        // A V11 row outside Display is not one of these.
        #expect(try parsedControl(toggle, category: "general", version: 11).metadataIssue != nil)
    }
}
