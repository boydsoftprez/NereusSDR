// NereusSDR for iOS: the fake Core's Setup descriptions, sent as the Core sends them, and a page added later
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The fake Core's `setup` object (the Setup description, link document
/// section 7): made with ``Additions/setupDescription``, the fake
/// advertises `setupDescriptionVersion` 11, and ``deliverSetup(_:)``
/// sends the `SetupDescription` schema and object, one property per
/// category in the order given, then ``publishSetup(_:json:revision:)``
/// changes one as the Core does when a page is added. The fake's own
/// descriptions (``syntheticSetup``) are made up for the tests: none of
/// their words or values come from the desktop.
extension FakeStation {
    /// The description version the fake advertises.
    public static let setupDescriptionVersion = 11

    /// The desktop's category IDs, in the order the Core's schema lists them.
    public static let setupCategoryIds = ["general", "hardware", "pa", "audio", "dsp", "display", "transmit",
                                          "appearance", "catNetwork", "test", "diagnostics"]

    /// The schema and object for `categories`, in their order; a category
    /// with no pages is sent as an empty string.
    public static func setupMessages(_ categories: [(id: String, json: String)], revision: Int64 = 1)
        -> [LinkMessage] {
        var fields = [LinkMessage.SchemaField(ordinal: 0, name: "revision", kind: .i64)]
        var properties = [LinkMessage.PropertyEntry(ordinal: 0, name: "revision", value: .i64(revision))]
        for (index, category) in categories.enumerated() {
            let ordinal = UInt16(index + 1)
            fields.append(.init(ordinal: ordinal, name: category.id, kind: .utf8))
            properties.append(.init(ordinal: ordinal, name: category.id, value: .utf8(category.json)))
        }
        return [.schema(.init(className: "SetupDescription", fields: fields)),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: properties))]
    }

    /// Sends the `setup` schema and object on the newest connection. A
    /// fake made without ``Additions/band2m`` leaves 2 m's antenna rows out,
    /// as the Core does for a peer it sends no `band2mVersion` (link
    /// document section 6.1).
    public func deliverSetup(_ categories: [(id: String, json: String)]) async {
        let fitted = categories.map { (id: $0.id, json: fittedForBand2m($0.json)) }
        for message in Self.setupMessages(fitted) {
            await deliver(message)
        }
    }

    /// `json` as this fake's Core sends it: with 2 m's antenna rows only
    /// when made with ``Additions/band2m``.
    func fittedForBand2m(_ json: String) -> String {
        guard !additions.contains(.band2m), json.contains("\"band\":27") || json.contains("\"band\": 27"),
              var root = (try? JSONSerialization.jsonObject(with: Data(json.utf8))) as? [String: Any],
              var pages = root["pages"] as? [[String: Any]] else {
            return json
        }
        for pageIndex in pages.indices {
            guard var sections = pages[pageIndex]["sections"] as? [[String: Any]] else { continue }
            for sectionIndex in sections.indices {
                guard var controls = sections[sectionIndex]["controls"] as? [[String: Any]] else { continue }
                for controlIndex in controls.indices {
                    guard let rows = controls[controlIndex]["rows"] as? [[String: Any]] else { continue }
                    controls[controlIndex]["rows"] = rows.filter { ($0["band"] as? Int) != Self.band2mButton.id }
                }
                sections[sectionIndex]["controls"] = controls
            }
            pages[pageIndex]["sections"] = sections
        }
        root["pages"] = pages
        guard let data = try? JSONSerialization.data(withJSONObject: root, options: [.sortedKeys]) else {
            return json
        }
        return String(decoding: data, as: UTF8.self)
    }

    /// Changes one category's description, one revision on, as the Core
    /// does when the desktop builds a page.
    public func publishSetup(_ category: String, json: String, revision: Int64) async {
        await deliver(.delta(.init(key: "setup", properties: [
            .init(name: category, value: .utf8(fittedForBand2m(json))), .init(name: "revision", value: .i64(revision)),
        ])))
    }

    // MARK: Made-up descriptions

    /// One made-up page for `category` at `version`, with a control of each
    /// generic kind the tests need. `extraPage` adds a second page after it.
    public static func syntheticSetup(_ category: String, title: String, version: Int,
                                      extraPage: String? = nil) -> String {
        func page(_ id: String, _ pageTitle: String) -> String {
            let controls = [
                #"{"id":"\#(category).\#(id).switch","label":"Sample switch","tooltip":"Turns the sample on.","kind":"toggle","binding":{"setting":"SliceSample\#(id)On"},"applies":"live","valueEncoding":{"true":"True","false":"False"}}"#,
                #"{"id":"\#(category).\#(id).count","label":"Sample count","tooltip":"","kind":"integer","binding":{"setting":"SliceSample\#(id)Count"},"applies":"live","min":1,"max":9,"step":1,"unit":"times"}"#,
            ]
            return #"{"id":"\#(category).\#(id)","title":"\#(pageTitle)","where":"station","sections":[{"title":"Sample","controls":["#
                + controls.joined(separator: ",") + "]}]}"
        }
        var pages = [page("main", title)]
        if let extraPage {
            pages.append(page("extra", extraPage))
        }
        return #"{"version":\#(version),"category":{"id":"\#(category)","title":"\#(category)","where":"station"},"pages":["#
            + pages.joined(separator: ",") + "]}"
    }
}

// MARK: The closed panels' objects and verbs

/// What the fake Core holds for the Setup panels (``FakeStation/Additions/setupPanels``):
/// its notch list, its antenna lists and the problems its settings check
/// reports. The values are made up for the tests.
struct SetupPanelState {
    var notches: [FakeStation.SetupNotch] = [
        .init(id: 1, centreHz: 7_074_000, widthHz: 100, active: true),
        .init(id: 2, centreHz: 14_074_000, widthHz: 250, active: false),
    ]
    var notchRevision: Int64 = 1
    var nextNotchId: Int64 = 3
    var tx = [1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1]
    var rx = Array(repeating: 1, count: 14)
    var rxOnly = Array(repeating: 0, count: 14)
    var blockTx2 = false
    var blockTx3 = true
    var issuesJson = "[]"

    /// The lists as a Core with 2 m sends them: a fifteenth entry, 2 m's.
    mutating func withBand2m() {
        tx.append(1)
        rx.append(1)
        rxOnly.append(0)
    }
}

extension FakeStation.Additions {
    /// The closed Setup panels: `settingsHygieneVersion` 2,
    /// `radioAntennaRowsVersion` 1 and `stationTelemetryVersion` 4; the
    /// notch row verbs, the settings check's three verbs and the two
    /// radio-bound antenna verbs, each played as the link contract says
    /// the Core answers it. ``FakeStation/deliverSetupPanels()`` sends the
    /// objects. Not in ``all``.
    public static let setupPanels = FakeStation.Additions(rawValue: 1 << 18)
    /// 2 m as its own band (link document section 6.1): `band2mVersion`
    /// 1; the catalogue's grid with 2 m (``FakeStation/band2mButton``),
    /// `slice.selectBand` taking it, 15-entry antenna lists and the
    /// antenna verbs taking band 27; the Setup descriptions keep 2 m's
    /// antenna rows. Not in ``all``.
    public static let band2m = FakeStation.Additions(rawValue: 1 << 44)
}

extension FakeStation {
    /// One notch of the fake's list.
    public struct SetupNotch: Equatable, Sendable {
        public let id: Int64
        public let centreHz: Double
        public let widthHz: Double
        public let active: Bool

        public init(id: Int64, centreHz: Double, widthHz: Double, active: Bool) {
            self.id = id
            self.centreHz = centreHz
            self.widthHz = widthHz
            self.active = active
        }
    }

    /// The radio the fixture's Core names (its `macAddress` capability).
    public static let setupPanelsRadioMac = "AA:BB:CC:DD:EE:01"
    /// The Core's refusal of a radio-bound verb for another radio.
    public static let otherRadioReason = "That radio is no longer connected to the Core."
    public static let notchGoneReason = "That notch is no longer on this Core."
    public static let blockedAntennaReason = "An antenna blocked for transmit cannot be a band's TX antenna."

    /// Sends the notch list, the antenna lists and an off-air transmit state.
    public func deliverSetupPanels() async throws {
        await deliver(notchesDelta())
        await deliver(.schema(try Self.schema(ofClass: "AlexAntennaFacade")))
        await deliver(.objectCreate(try Self.objectCreate(key: "alexAntennas", className: "AlexAntennaFacade",
                                                          values: antennaValues())))
        await deliver(.schema(try Self.schema(ofClass: "TransmitState")))
        await deliver(.objectCreate(try Self.objectCreate(key: "txState", className: "TransmitState", values: [:])))
    }

    /// Replaces the notch list, one revision on, as the Core does when
    /// another window changes it.
    public func setSetupNotches(_ notches: [SetupNotch]) async {
        withSetupPanels { state in
            state.notches = notches
            state.notchRevision += 1
        }
        await deliver(notchesDelta())
    }

    /// The problems the next settings check reports, as the Core's compact JSON.
    public func setSettingsIssues(_ json: String) {
        withSetupPanels { $0.issuesJson = json }
    }

    /// Keys or unkeys the radio, as the transmit state tells every window.
    public func setSetupKeyed(_ keyed: Bool) async {
        await deliver(.delta(.init(key: "txState", properties: [.init(name: "keyed", value: .bool(keyed))])))
    }

    /// One telemetry sample with the radio's PA readings; nil leaves one out.
    public func deliverPaTelemetry(sequence: Int, paCurrentAmps: Double?, supplyVolts: Double?,
                                   paTemperatureCelsius: Double? = nil) async {
        var radio: [String: LinkJSON] = ["connected": .bool(true)]
        radio["paCurrentAmps"] = paCurrentAmps.map(LinkJSON.number)
        radio["supplyVolts"] = supplyVolts.map(LinkJSON.number)
        radio["paTemperatureCelsius"] = paTemperatureCelsius.map(LinkJSON.number)
        await deliver(.stationMetrics(.init(payload: [
            "sequence": .number(Double(sequence)), "sampledElapsedMs": .number(Double(sequence) * 1_000),
            "radio": .object(radio),
        ])))
    }

    func notchesDelta() -> LinkMessage {
        let (json, revision) = withSetupPanels { state -> (String, Int64) in
            let rows = state.notches.map { notch in
                #"{"id":\#(notch.id),"centreHz":\#(Self.number(notch.centreHz)),"widthHz":\#(Self.number(notch.widthHz)),"active":\#(notch.active)}"#
            }
            return ("[" + rows.joined(separator: ",") + "]", state.notchRevision)
        }
        return .delta(.init(key: "notches", properties: [
            .init(name: "listJson", value: .utf8(json)), .init(name: "revision", value: .i64(revision)),
        ]))
    }

    private func antennaValues() -> [String: LinkMessage.PropertyValue] {
        withSetupPanels { state in
            ["txAntennas": .utf8(state.tx.map(String.init).joined(separator: ",")),
             "rxAntennas": .utf8(state.rx.map(String.init).joined(separator: ",")),
             "rxOnlyAntennas": .utf8(state.rxOnly.map(String.init).joined(separator: ",")),
             "blockTxAnt2": .bool(state.blockTx2), "blockTxAnt3": .bool(state.blockTx3)]
        }
    }

    /// The list entry a band's antennas sit at: 160 m to XVTR (0 to 13) at
    /// their own number, 2 m (27) at 14 with ``Additions/band2m``.
    private func antennaEntry(_ band: Int64) -> Int? {
        if (0..<14).contains(band) {
            return Int(band)
        }
        return additions.contains(.band2m) && band == Int64(Self.band2mButton.id) ? 14 : nil
    }

    private static func number(_ value: Double) -> String {
        value.rounded() == value && abs(value) < 1e15 ? String(Int64(value)) : String(value)
    }

    /// The fake's answer to a Setup panel verb; nil leaves it to the rest of the fake.
    func setupPanelReplies(_ invoke: LinkMessage.CommandInvoke) -> [LinkMessage]? {
        let verbs = ["notch.move", "notch.setActive", "notch.delete", "station.validateSettings",
                     "station.repairSettings", "station.forgetSettings", "setAlexTxAntennaForRadio", "setAlexRxAntennaForRadio"]
        guard additions.contains(.setupPanels), verbs.contains(invoke.verb) else {
            return nil
        }
        func result(_ accepted: Bool, _ reason: String = "", affected: [String] = [],
                    values: [LinkMessage.PropertyEntry]? = nil) -> LinkMessage {
            .commandResult(LinkMessage.CommandResult(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                     reason: reason, affected: affected, values: values))
        }
        func argument(_ name: String) -> LinkMessage.PropertyValue? {
            invoke.args.first { $0.name == name }?.value
        }
        if let reason = takeRefusal(invoke.verb) {
            return [result(false, reason)]
        }
        switch invoke.verb {
        case "notch.move", "notch.setActive", "notch.delete":
            guard case .i64(let id)? = argument("id") else {
                return [result(false, "This notch change is not one this Core understands.")]
            }
            let revision = withSetupPanels { state -> Int64? in
                guard let index = state.notches.firstIndex(where: { $0.id == id }) else { return nil }
                let notch = state.notches[index]
                switch (invoke.verb, argument("centreHz"), argument("widthHz"), argument("active")) {
                case ("notch.move", .f64(let centre)?, .f64(let width)?, _):
                    state.notches[index] = SetupNotch(id: id, centreHz: centre, widthHz: width, active: notch.active)
                case ("notch.setActive", _, _, .bool(let on)?):
                    state.notches[index] = SetupNotch(id: id, centreHz: notch.centreHz, widthHz: notch.widthHz,
                                                      active: on)
                case ("notch.delete", _, _, _):
                    state.notches.remove(at: index)
                default:
                    return nil
                }
                state.notchRevision += 1
                return state.notchRevision
            }
            guard let revision else {
                return [result(false, Self.notchGoneReason)]
            }
            return [result(true, affected: ["notches"], values: [.init(name: "revision", value: .i64(revision))]),
                    notchesDelta()]
        case "station.validateSettings", "station.repairSettings", "station.forgetSettings":
            guard case .utf8(let mac)? = argument("mac"), mac == Self.setupPanelsRadioMac else {
                return [result(false, Self.otherRadioReason)]
            }
            let issues = withSetupPanels { $0.issuesJson }
            return [result(true, values: [.init(name: "mac", value: .utf8(mac)),
                                          .init(name: "issuesJson", value: .utf8(issues))])]
        default:
            guard case .utf8(let mac)? = argument("mac"), mac == Self.setupPanelsRadioMac,
                  case .i64(let band)? = argument("band"),
                  let entry = antennaEntry(band),
                  case .i64(let antenna)? = argument("antenna"), (1...3).contains(antenna) else {
                return [result(false, Self.otherRadioReason)]
            }
            let transmit = invoke.verb == "setAlexTxAntennaForRadio"
            var rxOnly = false
            if case .bool(let flag)? = argument("rxOnly") { rxOnly = flag }
            let accepted = withSetupPanels { state -> Bool in
                if transmit {
                    if antenna == 2 && state.blockTx2 || antenna == 3 && state.blockTx3 { return false }
                    state.tx[entry] = Int(antenna)
                } else if rxOnly {
                    state.rxOnly[entry] = Int(antenna)
                } else {
                    state.rx[entry] = Int(antenna)
                }
                return true
            }
            guard accepted else {
                return [result(false, Self.blockedAntennaReason)]
            }
            let values = antennaValues()
            let name = transmit ? "txAntennas" : rxOnly ? "rxOnlyAntennas" : "rxAntennas"
            return [result(true, affected: ["alexAntennas"]),
                    .delta(.init(key: "alexAntennas", properties: [.init(name: name, value: values[name]!)]))]
        }
    }
}
