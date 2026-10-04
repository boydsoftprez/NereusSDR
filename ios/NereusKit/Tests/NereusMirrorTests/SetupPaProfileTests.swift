// NereusSDR for iOS: PA producer metadata, authoritative values and gesture-bound commands
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@MainActor
@Suite struct SetupPaProfileTests {
    /// The actual producer resource, narrowed to the six assigned controls.
    static func description(version: Int = 20, keyed: Bool = false, holderBand: Int? = nil,
                            mutate: ((inout [[String: Any]]) -> Void)? = nil) throws -> String {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/pa.json").standardizedFileURL
        let root = try #require(JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
        let pages = try #require(root["pages"] as? [[String: Any]])
        let sections = try #require(pages[0]["sections"] as? [[String: Any]])
        var controls = sections.prefix(2).flatMap { $0["controls"] as? [[String: Any]] ?? [] }
        if version >= 20 {
            for index in controls.indices {
                if controls[index]["id"] as? String == "pa.gain.table" {
                    var gate = try #require(controls[index]["gate"] as? [String: Any])
                    gate.removeValue(forKey: "offAir")
                    controls[index]["gate"] = gate
                    if keyed {
                        var rows = try #require(controls[index]["rows"] as? [[String: Any]])
                        for row in rows.indices where row != holderBand {
                            rows[row]["availability"] = ["enabled": false, "reason": holderBand == nil && row == 5
                                ? "Only the device that is transmitting can change this." : "Can't change while transmitting."]
                        }
                        controls[index]["rows"] = rows
                    }
                } else if keyed {
                    controls[index]["availability"] = ["enabled": false, "reason": "Can't change while transmitting."]
                }
            }
        }
        mutate?(&controls)
        let category: [String: Any] = ["version": version, "category": ["id": "pa", "title": "PA", "where": "station"],
            "pages": [["id": "pa.gain", "title": "PA Gain", "where": "station",
                       "sections": [["title": "Profile", "controls": controls]]]]]
        return String(decoding: try JSONSerialization.data(withJSONObject: category), as: UTF8.self)
    }

    static func controls(_ json: String) throws -> [SetupDescription.Control] {
        try SetupDescription.parse(json: json).pages.flatMap(\.sections).flatMap(\.controls)
    }

    static func grid(_ json: String) throws -> SetupDescription.PaProfileGrid {
        let table = try #require(try controls(json).first { $0.id == "pa.gain.table" })
        guard case .paProfileGrid(let grid)? = table.binding else { throw SetupRefusal(reason: "Missing test grid") }
        return grid
    }

    /// Representative test values, with distinct cells; no hardware defaults.
    static func body(active: String = "User", gain: Double = 50, mutate: ((inout [String: Any]) -> Void)? = nil) throws -> String {
        let grid = try grid(description())
        let bands: [[String: Any]] = grid.rows.map { row in
            ["band": row.label, "gain": gain + Double(row.band) / 10,
             "adjust": (0..<9).map { -10 + Double(row.band) / 2 + Double($0) / 10 },
             "maxPower": Double(row.band) * 10, "useMax": row.band % 2 == 0]
        }
        var root: [String: Any] = ["names": ["Factory", "User", "Other"], "active": active, "factory": active == "Factory", "bands": bands]
        mutate?(&root)
        return String(decoding: try JSONSerialization.data(withJSONObject: root), as: UTF8.self)
    }

    @Test func producerMetadataPreservesOrderWordsPromptsAndRanges() throws {
        for version in [14, 19, 20] {
            let rows = try Self.controls(Self.description(version: version))
            #expect(rows.count == 6)
            #expect(rows.allSatisfy { $0.metadataIssue == nil && $0.modern?.pendingReason == nil })
            #expect(rows.map(\.label) == ["PA profile", "New", "Copy", "Delete", "Reset Defaults", "PA Gain by Band (dB)"])
            let grid = try Self.grid(Self.description(version: version))
            #expect(grid.rows.map(\.band) == Array(0..<14))
            #expect(grid.rows.map(\.label) == ["160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "GEN", "WWV", "XVTR"])
            #expect(grid.columns.map(\.label) == ["Gain (dB)", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "Max W", "Use Max"])
            #expect(grid.columns.filter { $0.field == "adjust" }.map(\.driveStep) == (0..<9).map(Optional.some))
            #expect(grid.column("gain")?.range == .init(minimum: 38.8, maximum: 100, step: 0.1))
            #expect(grid.column("adjust9")?.range == .init(minimum: -10, maximum: 10, step: 0.1))
            #expect(grid.column("maxPower")?.range == .init(minimum: 0, maximum: 1500, step: 0.1))
            #expect(grid.columns.filter { $0.kind == .decimal }.allSatisfy { $0.decimals == 1 })
            #expect(rows.last?.gate?.offAir == (version < 20 ? true : nil))
            guard case .paProfile(let copy)? = rows[2].binding else { Issue.record("Missing Copy binding"); return }
            #expect(copy.prompt?.title == "Copy PA Profile")
            #expect(copy.prompt?.label == "New profile name:")
            #expect(copy.prompt?.defaultValue == "%1 (copy)")
            #expect(rows[3].confirm == "Delete profile \"%1\"?")
            #expect(rows[4].confirm == "Reset the active profile to factory defaults?")
        }
        #expect(LinkFeatures.app["paProfiles"] == 1)
    }

    @Test(arguments: ["band", "rowCount", "step", "field", "columnCount", "range", "precision", "object", "prompt", "gate", "availability"])
    func malformedMetadataCannotAdmit(operation: String) throws {
        let json = try Self.description { controls in
            var rows = controls[5]["rows"] as! [[String: Any]]
            var columns = controls[5]["columns"] as! [[String: Any]]
            switch operation {
            case "band": rows[13]["band"] = 12
            case "rowCount": rows.removeLast()
            case "step": columns[9]["driveStep"] = 9
            case "field": columns[0]["field"] = "hardwareGain"
            case "columnCount": columns.removeLast()
            case "range": columns[0]["max"] = 1
            case "precision": columns[0]["decimals"] = 7
            case "object": controls[5]["binding"] = ["paProfileGrid": ["object": "radio"]]
            case "prompt": controls[1].removeValue(forKey: "prompt")
            case "gate": controls[5]["gate"] = ["capability": "paProfileVersion", "min": 0]
            default: rows[0]["availability"] = ["enabled": false]
            }
            controls[5]["rows"] = rows
            controls[5]["columns"] = columns
        }
        #expect(try Self.controls(json).contains { $0.metadataIssue != nil })
    }

    @Test func allFourteenRowsAndNineAdjustmentsMapByOrderedDescription() throws {
        let grid = try Self.grid(Self.description())
        let body = try #require(PaProfiles(json: Self.body(), revision: 3, grid: grid))
        #expect(body.names == ["Factory", "User", "Other"])
        #expect(body.active == "User" && !body.factory && body.revision == 3)
        for band in 0..<14 {
            #expect(body.bands[band].band == band)
            #expect(body.bands[band].label == grid.rows[band].label)
            for step in 0..<9 {
                let column = try #require(grid.column("adjust\(step + 1)"))
                #expect(body.bands[band].value(for: column) == .decimal(-10 + Double(band) / 2 + Double(step) / 10))
            }
        }
    }

    @Test(arguments: ["active", "duplicate", "empty", "factory", "bands", "label", "adjust", "number", "boolNumber", "range"])
    func malformedBodyIsRejected(operation: String) throws {
        let grid = try Self.grid(Self.description())
        let json = try Self.body { root in
            var bands = root["bands"] as! [[String: Any]]
            switch operation {
            case "active": root["active"] = "Missing"
            case "duplicate": root["names"] = ["User", "user"]
            case "empty": root["names"] = ["", "User"]
            case "factory": root["factory"] = 1
            case "bands": bands.removeLast()
            case "label": bands.swapAt(0, 1)
            case "adjust": bands[13]["adjust"] = [0, 1]
            case "number": bands[0]["gain"] = "50"
            case "boolNumber": bands[0]["gain"] = true
            default: bands[0]["gain"] = 100.1
            }
            root["bands"] = bands
        }
        #expect(PaProfiles(json: json, revision: 1, grid: grid) == nil)
        #expect(PaProfiles(json: try Self.body(), revision: -1, grid: grid) == nil)
        #expect(PaProfiles(json: try Self.body(), revision: Int64(UInt32.max) + 1, grid: grid) == nil)
        #expect(PaProfiles(json: "", revision: 1, grid: grid) == nil)
    }

    @MainActor final class Rig {
        let route = TestRoute()
        let store: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let dispatcher: SetupControlDispatcher

        init(clock: any LinkClock = SystemLinkClock()) {
            let route = route
            store = MirrorStore(send: { _ in })
            settings = SettingsProxyClient(send: { _ in }, captureSender: { route.capture() })
            commands = CommandClient(clock: clock, send: { _ in }, captureSender: { route.capture() })
            feed = SetupDescriptionFeed(store: store)
            dispatcher = SetupControlDispatcher(store: store, feed: feed, settings: settings, commands: commands,
                                                captureSender: { route.capture() }, selectedSlice: { nil },
                                                selectionChanges: Just(nil).eraseToAnyPublisher(), phone: nil)
        }

        func event(_ event: StationSession.Event) async {
            store.handle(event)
            settings.handle(event)
            await commands.handle(event)
        }

        func connect(version: Int = 20, keyed: Bool = false, holderBand: Int? = nil, txPermitted: Bool = true) async throws {
            route.use(1)
            await event(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                .capabilities(.init(properties: [.init(name: "setupDescriptionVersion", value: .i64(Int64(version))),
                    .init(name: "paProfileVersion", value: .i64(1)), .init(name: "txPermitted", value: .bool(txPermitted))])),
                .schema(.init(className: "SetupDescription", fields: [.init(ordinal: 0, name: "revision", kind: .i64),
                    .init(ordinal: 1, name: "pa", kind: .utf8)])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "pa", value: .utf8(try SetupPaProfileTests.description(version: version < 20 ? 14 : 20, keyed: keyed, holderBand: holderBand)))])),
                .objectCreate(.init(key: "paProfiles", className: "PaProfilesFacade", properties: [
                    .init(name: "json", value: .utf8(try SetupPaProfileTests.body())), .init(name: "revision", value: .i64(1))])),
                .objectCreate(.init(key: "txState", className: "TransmitState", properties: [
                    .init(name: "keyed", value: .bool(keyed)), .init(name: "tuning", value: .bool(false)),
                    .init(name: "twoTone", value: .bool(false)), .init(name: "txEnding", value: .bool(false))])),
                .settingsSnapshot(.init(properties: [])), .snapshotComplete]
            for message in messages { await event(.message(message)) }
            await event(.stateChanged(.ready))
            for _ in 0..<5 { await Task.yield() }
        }

        func control(_ suffix: String) throws -> SetupDescription.Control {
            let description = try #require(feed.description(for: "pa"))
            return try #require(description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == "pa.gain." + suffix })
        }

        func admit(_ suffix: String, band: Int? = nil, column: String? = nil) throws -> SetupAdmission {
            try dispatcher.admitPa(control(suffix), in: "pa", band: band, column: column).get()
        }

        func invoke(_ index: Int = 0) throws -> LinkMessage.CommandInvoke {
            guard route.sent.count > index, case .commandInvoke(let invoke) = route.sent[index].message else {
                throw SetupRefusal(reason: "Missing test invocation")
            }
            return invoke
        }

        func answer(_ invoke: LinkMessage.CommandInvoke, accepted: Bool = true, reason: String = "") async {
            await commands.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                       reason: reason, affected: [], values: [])))
        }

        func push(active: String = "User", revision: Int64 = 2, gain: Double = 50) async throws {
            await event(.message(.delta(.init(key: "paProfiles", properties: [
                .init(name: "json", value: .utf8(try SetupPaProfileTests.body(active: active, gain: gain))),
                .init(name: "revision", value: .i64(revision))]))))
        }
    }

    @Test(arguments: ["select", "new", "copy", "delete", "reset", "setGain", "setAdjust", "setMaxPower", "setUseMax"])
    func eachVerbSendsOnlyItsTypedArguments(action: String) async throws {
        let rig = Rig()
        try await rig.connect()
        let table = action.hasPrefix("set")
        let column = action == "setGain" ? "gain" : action == "setAdjust" ? "adjust9" : action == "setMaxPower" ? "maxPower" : "useMax"
        let admission = try rig.admit(table ? "table" : action == "select" ? "profile" : action,
                                      band: table ? 13 : nil, column: table ? column : nil)
        var value: SetupValue?
        var prompt: SetupPaPrompt?
        var question: SetupPaQuestion?
        var expected: [String: LinkMessage.PropertyValue] = [:]
        switch action {
        case "select": value = .text("Other"); expected = ["name": .utf8("Other")]
        case "new", "copy":
            prompt = try rig.dispatcher.paPrompt(for: admission).get()
            #expect(prompt?.defaultValue == (action == "new" ? "" : "User (copy)"))
            value = .text(" New Name "); expected = ["name": .utf8(" New Name ")]
        case "delete", "reset":
            question = try rig.dispatcher.paQuestion(for: admission).get()
            #expect(question?.text == (action == "delete" ? "Delete profile \"User\"?" : "Reset the active profile to factory defaults?"))
            if action == "delete" { expected = ["name": .utf8("User")] }
        case "setGain": value = .decimal(38.8); expected = ["band": .i64(13), "value": .f64(38.8)]
        case "setAdjust": value = .decimal(-10); expected = ["band": .i64(13), "step": .i64(8), "value": .f64(-10)]
        case "setMaxPower": value = .decimal(1500); expected = ["band": .i64(13), "value": .f64(1500)]
        default: value = .bool(true); expected = ["band": .i64(13), "on": .bool(true)]
        }
        let heldValue = value, heldPrompt = prompt, heldQuestion = question
        let edit = Task { await rig.dispatcher.performPa(admission, value: heldValue, prompt: heldPrompt, confirmed: heldQuestion) }
        #expect(await rig.route.waitForCount(1, unless: edit))
        let invoke = try rig.invoke()
        #expect(invoke.verb == "paProfile." + action)
        #expect(Dictionary(uniqueKeysWithValues: invoke.args.map { ($0.name, $0.value) }) == expected)
        // Accepted result alone does not invent new/copy/reset/delete values.
        let before = rig.dispatcher.paPanel(try rig.control("table"), in: "pa").profiles
        await rig.answer(invoke)
        #expect(await edit.value == .applied)
        #expect(rig.dispatcher.paPanel(try rig.control("table"), in: "pa").profiles == before)
        try await rig.push(active: "Other", gain: 55)
        #expect(rig.dispatcher.paPanel(try rig.control("table"), in: "pa").profiles?.active == "Other")
        #expect(rig.dispatcher.paPanel(try rig.control("table"), in: "pa").profiles?.bands[0].gain == 55)
    }

    @Test(arguments: ["gain", "adjust1", "adjust9", "maxPower"])
    func invalidAndNonfiniteCellValuesNeverSend(column: String) async throws {
        let rig = Rig()
        try await rig.connect()
        let grid = try Self.grid(Self.description())
        let metadata = try #require(grid.column(column))
        let range = try #require(metadata.range)
        for number in [Double.nan, .infinity, -.infinity, range.minimum - 0.1, range.maximum + 0.1, range.minimum + 0.01] {
            let admission = try rig.admit("table", band: 0, column: column)
            #expect(await rig.dispatcher.performPa(admission, value: .decimal(number)) == .notSent(SetupControlDispatcher.outOfRangeReason))
        }
        let wrongKind = try rig.admit("table", band: 0, column: column)
        #expect(await rig.dispatcher.performPa(wrongKind, value: .bool(true)) == .notSent(SetupControlDispatcher.outOfRangeReason))
        #expect(rig.route.sent.isEmpty)
    }

    @Test func booleanAndBandAndColumnSchemaAreClosed() async throws {
        let rig = Rig()
        try await rig.connect()
        for band in [-1, 14] {
            if case .success = rig.dispatcher.admitPa(try rig.control("table"), in: "pa", band: band, column: "gain") {
                Issue.record("Invalid band admitted")
            }
        }
        if case .success = rig.dispatcher.admitPa(try rig.control("table"), in: "pa", band: 0, column: "adjust10") {
            Issue.record("Invalid adjustment admitted")
        }
        let admission = try rig.admit("table", band: 0, column: "useMax")
        #expect(await rig.dispatcher.performPa(admission, value: .integer(1)) == .notSent(SetupControlDispatcher.outOfRangeReason))
        #expect(rig.route.sent.isEmpty)
    }

    @Test(arguments: ["revision", "profile", "permissions", "description", "session", "transmit", "object"])
    func aChangedScopeCancelsAQueuedCellAtFinalHandoff(change: String) async throws {
        let rig = Rig()
        try await rig.connect()
        let admission = try rig.admit("table", band: 5, column: "adjust1")
        let hold = HandoffHold()
        rig.route.holdNext(hold)
        let edit = Task { await rig.dispatcher.performPa(admission, value: .decimal(1)) }
        await hold.waitUntilEntered()
        switch change {
        case "revision": try await rig.push(revision: 2)
        case "profile": try await rig.push(active: "Other", revision: 1)
        case "permissions":
            var capabilities = rig.store.capabilities
            capabilities["txPermitted"] = .bool(false)
            await rig.event(.message(.capabilities(.init(properties: capabilities.map { .init(name: $0.key, value: $0.value.wireValue) }))))
            capabilities["txPermitted"] = .bool(true)
            await rig.event(.message(.capabilities(.init(properties: capabilities.map { .init(name: $0.key, value: $0.value.wireValue) }))))
        case "description":
            let changed = try Self.description { $0[5]["label"] = "Core changed label" }
            await rig.event(.message(.delta(.init(key: "setup", properties: [.init(name: "pa", value: .utf8(changed))]))))
        case "session":
            rig.route.use(2)
            await rig.event(.stateChanged(.waitingToRetry(seconds: 1)))
            await rig.event(.stateChanged(.receivingSnapshot))
        case "transmit":
            await rig.event(.message(.delta(.init(key: "txState", properties: [.init(name: "keyed", value: .bool(true))]))))
            await rig.event(.message(.delta(.init(key: "txState", properties: [.init(name: "keyed", value: .bool(false))]))))
        default:
            // Recreating a same-key object is a different facade identity.
            await rig.event(.message(.objectDestroy(.init(key: "paProfiles", className: "PaProfilesFacade"))))
            await rig.event(.message(.objectCreate(.init(key: "paProfiles", className: "PaProfilesFacade", properties: [
                .init(name: "json", value: .utf8(try Self.body())), .init(name: "revision", value: .i64(1))]))))
        }
        #expect(admission.isRevoked)
        await hold.release()
        #expect(await edit.value == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.isEmpty)
    }

    @Test(arguments: ["new", "copy", "delete", "reset"])
    func oldQuestionsCannotAdoptFreshAdmissions(action: String) async throws {
        let rig = Rig()
        try await rig.connect()
        let old = try rig.admit(action)
        let oldPrompt = try? rig.dispatcher.paPrompt(for: old).get()
        let oldQuestion = try? rig.dispatcher.paQuestion(for: old).get()
        try await rig.push(active: "Other")
        let fresh = try rig.admit(action)
        #expect(old.paOwner != fresh.paOwner)
        #expect(await rig.dispatcher.performPa(fresh, value: oldPrompt == nil ? nil : .text("Name"),
                                               prompt: oldPrompt, confirmed: oldQuestion) == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(await rig.dispatcher.performPa(old, value: oldPrompt == nil ? nil : .text("Name"),
                                               prompt: oldPrompt, confirmed: oldQuestion) == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.isEmpty)
    }

    @Test(arguments: [14, 19, 20])
    func legacyWholeTableAndModernHolderRowsFollowCoreAvailability(version: Int) async throws {
        let rig = Rig()
        try await rig.connect(version: version, keyed: true, holderBand: version >= 20 ? 5 : nil)
        let table = try rig.control("table")
        let panel = rig.dispatcher.paPanel(table, in: "pa")
        if version < 20 {
            #expect(panel.reason == SetupControlDispatcher.onAirReason)
            #expect(panel.reason(for: 5) == SetupControlDispatcher.onAirReason)
        } else {
            #expect(panel.reason == nil && panel.reason(for: 5) == nil)
            for band in 0..<14 where band != 5 { #expect(panel.reason(for: band) == "Can't change while transmitting.") }
            #expect(try rig.admit("table", band: 5, column: "gain").paOwner != nil)
            for suffix in ["profile", "new", "copy", "delete", "reset"] {
                if case .failure(let refusal) = rig.dispatcher.admitPa(try rig.control(suffix), in: "pa") {
                    #expect(refusal.reason == "Can't change while transmitting.")
                } else { Issue.record("Lifecycle action admitted while on air") }
            }
        }
        #expect(rig.route.sent.isEmpty)
    }

    @Test func modernNonholderReceivesTheExactRowReason() async throws {
        let rig = Rig()
        try await rig.connect(keyed: true)
        let control = try rig.control("table")
        #expect(rig.dispatcher.paPanel(control, in: "pa").reason(for: 5) == "Only the device that is transmitting can change this.")
        if case .failure(let refusal) = rig.dispatcher.admitPa(control, in: "pa", band: 5, column: "adjust1") {
            #expect(refusal.reason == "Only the device that is transmitting can change this.")
        } else { Issue.record("Nonholder's locked band admitted") }
    }

    @Test func receiveOnlyStylePermissionDoesNotBlanketDenyPaAndCoreRefusalIsUnchanged() async throws {
        let rig = Rig()
        try await rig.connect(txPermitted: false)
        let admission = try rig.admit("table", band: 5, column: "gain")
        let edit = Task { await rig.dispatcher.performPa(admission, value: .decimal(60)) }
        #expect(await rig.route.waitForCount(1, unless: edit))
        let words = "Only the device that is transmitting can change this."
        await rig.answer(try rig.invoke(), accepted: false, reason: words)
        #expect(await edit.value == .refused(words))
        #expect(rig.dispatcher.paPanel(try rig.control("table"), in: "pa").profiles?.bands[5].gain == 50.5)
    }

    @Test func unknownOrDisconnectedProfileNeverOffersAnEdit() async throws {
        let rig = Rig()
        try await rig.connect()
        await rig.event(.message(.delta(.init(key: "paProfiles", properties: [.init(name: "json", value: .utf8(""))]))))
        let control = try rig.control("table")
        #expect(rig.dispatcher.paPanel(control, in: "pa").profiles == nil)
        #expect(rig.dispatcher.paPanel(control, in: "pa").reason == SetupControlDispatcher.paProfileUnreadableReason)
        await rig.event(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(rig.dispatcher.paPanel(control, in: "pa").profiles == nil)
        #expect(rig.dispatcher.paPanel(control, in: "pa").reason == SetupControlDispatcher.notConnectedReason)
    }

    @Test func timeoutKeepsCoreValuesAndANewerGestureOwnsItsOwnLateOutcome() async throws {
        let clock = ManualLinkClock()
        let rig = Rig(clock: clock)
        try await rig.connect()
        let first = try rig.admit("table", band: 0, column: "gain")
        var obsolete: SetupEditOutcome?
        let edit = Task { await rig.dispatcher.performPa(first, value: .decimal(60), onLateOutcome: { obsolete = $0 }) }
        #expect(await rig.route.waitForCount(1, unless: edit))
        let invoke = try rig.invoke()
        await clock.advance(by: 5_000)
        #expect(await edit.value == .notSent(SetupControlDispatcher.noAnswerReason))
        #expect(rig.dispatcher.paPanel(try rig.control("table"), in: "pa").profiles?.bands[0].gain == 50)
        let second = try rig.admit("table", band: 0, column: "gain")
        #expect(!rig.dispatcher.paOwnsOutcome(first) && rig.dispatcher.paOwnsOutcome(second))
        await rig.answer(invoke, accepted: false, reason: "Obsolete refusal")
        for _ in 0..<20 { await Task.yield() }
        #expect(obsolete == nil)
        #expect(rig.route.sent.count == 1)
    }

    @Test(arguments: ["gain", "adjust5", "maxPower", "useMax"])
    func modernHolderSendsOnlyTheCoreDescribedTransmittingBand(column: String) async throws {
        let rig = Rig()
        try await rig.connect(keyed: true, holderBand: 5)
        let table = try rig.control("table")
        if case .success = rig.dispatcher.admitPa(table, in: "pa", band: 4, column: column) {
            Issue.record("Nontransmitting band admitted")
        }
        let admission = try rig.admit("table", band: 5, column: column)
        let value: SetupValue = column == "useMax" ? .bool(true) : .decimal(column == "gain" ? 60 : 1)
        let edit = Task { await rig.dispatcher.performPa(admission, value: value) }
        #expect(await rig.route.waitForCount(1, unless: edit))
        let invoke = try rig.invoke()
        #expect(invoke.args.first { $0.name == "band" }?.value == .i64(5))
        await rig.answer(invoke)
        #expect(await edit.value == .applied)
    }

    @Test func advertisedRangesAndPrecisionDriveValidation() throws {
        let json = try Self.description { controls in
            var columns = controls[5]["columns"] as! [[String: Any]]
            columns[0]["min"] = 40
            columns[0]["max"] = 60
            columns[0]["step"] = 0.5
            controls[5]["columns"] = columns
        }
        let grid = try Self.grid(json)
        let gain = try #require(grid.column("gain"))
        #expect(!gain.fits(38.8) && gain.fits(40) && gain.fits(60))
        #expect(gain.fits(50.5) && !gain.fits(50.1))
        #expect(!gain.fits(.nan) && !gain.fits(.infinity))
    }

    @Test func genericDispatchUsesTheSamePaAdmissionWithoutBypassingPrompts() async throws {
        let rig = Rig()
        try await rig.connect()
        let admission = try rig.dispatcher.admit(rig.control("profile"), in: "pa").get()
        let edit = Task { await rig.dispatcher.perform(admission, value: .text("Other")) }
        #expect(await rig.route.waitForCount(1, unless: edit))
        let invoke = try rig.invoke()
        #expect(invoke.verb == "paProfile.select")
        await rig.answer(invoke)
        #expect(await edit.value == .applied)
        // A generic button cannot send New without the UUID-owned prompt.
        let new = try rig.dispatcher.admit(rig.control("new"), in: "pa").get()
        #expect(await rig.dispatcher.perform(new, value: .text("Name")) == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.count == 1)
    }


    /// Complete A→B→A without changing the session, profile or Setup
    /// description. Present-value equality cannot undo an authority loss.
    private static func applyPaAuthorityABA(_ rig: Rig, change: String) async {
        if change == "capability" {
            var capabilities = rig.store.capabilities
            capabilities["txPermitted"] = .bool(false)
            await rig.event(.message(.capabilities(.init(properties: capabilities.map {
                .init(name: $0.key, value: $0.value.wireValue)
            }))))
            capabilities["txPermitted"] = .bool(true)
            await rig.event(.message(.capabilities(.init(properties: capabilities.map {
                .init(name: $0.key, value: $0.value.wireValue)
            }))))
        } else {
            await rig.event(.message(.delta(.init(key: "txState", properties: [.init(name: "keyed", value: .bool(true))]))))
            await rig.event(.message(.delta(.init(key: "txState", properties: [.init(name: "keyed", value: .bool(false))]))))
        }
    }

    /// Positive-control completion is causal: the test resumes only after
    /// the production late-outcome callback actually delivers its result.
    @MainActor private final class PaLateOutcomeProbe {
        private(set) var outcomes: [SetupEditOutcome] = []
        private var waiter: CheckedContinuation<SetupEditOutcome, Never>?

        func receive(_ outcome: SetupEditOutcome) {
            outcomes.append(outcome)
            waiter?.resume(returning: outcome)
            waiter = nil
        }

        func next() async -> SetupEditOutcome {
            if let outcome = outcomes.first { return outcome }
            return await withCheckedContinuation { waiter = $0 }
        }
    }

    @Test(arguments: ["capability", "transmit"])
    func timedOutPaResultOwnershipCannotReviveAfterAuthorityABA(change: String) async throws {
        let clock = ManualLinkClock()
        let rig = Rig(clock: clock)
        try await rig.connect()
        let admission = try rig.admit("table", band: 0, column: "gain")
        let probe = PaLateOutcomeProbe()
        let edit = Task {
            await rig.dispatcher.performPa(admission, value: .decimal(60), onLateOutcome: { probe.receive($0) })
        }
        // TestRoute observes actual handoff. CommandClient.begin schedules
        // the deadline before that handoff, so advancing cannot outrun it.
        #expect(await rig.route.waitForCount(1, unless: edit))
        #expect(clock.pendingDueTimes.contains(5_000))
        let invoke = try rig.invoke()
        #expect(rig.dispatcher.paOwnsOutcome(admission))
        await clock.advance(by: 5_000)
        #expect(await edit.value == .notSent(SetupControlDispatcher.noAnswerReason))
        // Awaiting edit.value also observes performPa's defer: the timeout
        // owner is no longer in the pending-admission array.
        #expect(rig.dispatcher.openAdmissionCount == 0)
        #expect(rig.dispatcher.paOwnsOutcome(admission))
        let identity = rig.store.snapshotIdentity
        let generation = rig.feed.generation
        let capabilities = rig.store.capabilities
        let transmitState = rig.store.object("txState")?.values
        await Self.applyPaAuthorityABA(rig, change: change)
        #expect(rig.store.snapshotIdentity == identity && rig.feed.generation == generation)
        #expect(rig.store.capabilities == capabilities && rig.store.object("txState")?.values == transmitState)
        // Deterministic RED point: historical invalidation must survive
        // restored values even though the timed-out admission was removed.
        #expect(!rig.dispatcher.paOwnsOutcome(admission))
        await rig.answer(invoke, accepted: false, reason: "Retired PA refusal after authority change.")
        // Result arrival cannot restore the old row's presentation owner.
        #expect(!rig.dispatcher.paOwnsOutcome(admission))
        #expect(rig.route.sent.count == 1)
        #expect(rig.dispatcher.paPanel(try rig.control("table"), in: "pa").profiles?.bands[0].gain == 50)
    }

    @Test(arguments: ["capability", "transmit"])
    func awaitingPaResultOwnershipCannotReviveAfterAuthorityABA(change: String) async throws {
        let clock = ManualLinkClock()
        let rig = Rig(clock: clock)
        try await rig.connect()
        let admission = try rig.admit("table", band: 0, column: "gain")
        let edit = Task { await rig.dispatcher.performPa(admission, value: .decimal(60)) }
        #expect(await rig.route.waitForCount(1, unless: edit))
        #expect(clock.pendingDueTimes.contains(5_000) && clock.now == 0)
        let invoke = try rig.invoke()
        #expect(rig.dispatcher.paOwnsOutcome(admission))
        let identity = rig.store.snapshotIdentity
        let generation = rig.feed.generation
        let capabilities = rig.store.capabilities
        let transmitState = rig.store.object("txState")?.values
        await Self.applyPaAuthorityABA(rig, change: change)
        #expect(rig.store.snapshotIdentity == identity && rig.feed.generation == generation)
        #expect(rig.store.capabilities == capabilities && rig.store.object("txState")?.values == transmitState)
        #expect(admission.isRevoked)
        // Existing in-flight revocation must also retire result ownership.
        #expect(!rig.dispatcher.paOwnsOutcome(admission))
        let reason = "Retired immediate PA refusal after authority change."
        await rig.answer(invoke, accepted: false, reason: reason)
        #expect(await edit.value == .refused(reason))
        #expect(!rig.dispatcher.paOwnsOutcome(admission))
        #expect(rig.route.sent.count == 1 && clock.now == 0)
        #expect(rig.dispatcher.paPanel(try rig.control("table"), in: "pa").profiles?.bands[0].gain == 50)
    }

    @Test func unchangedTimedOutPaOwnerReceivesTheExactLateRefusal() async throws {
        let clock = ManualLinkClock()
        let rig = Rig(clock: clock)
        try await rig.connect()
        let admission = try rig.admit("table", band: 0, column: "gain")
        let probe = PaLateOutcomeProbe()
        let edit = Task {
            await rig.dispatcher.performPa(admission, value: .decimal(60), onLateOutcome: { probe.receive($0) })
        }
        #expect(await rig.route.waitForCount(1, unless: edit))
        #expect(clock.pendingDueTimes.contains(5_000))
        let invoke = try rig.invoke()
        await clock.advance(by: 5_000)
        #expect(await edit.value == .notSent(SetupControlDispatcher.noAnswerReason))
        #expect(rig.dispatcher.openAdmissionCount == 0)
        #expect(rig.dispatcher.paOwnsOutcome(admission))
        let reason = "Current PA owner receives this late Core refusal unchanged."
        await rig.answer(invoke, accepted: false, reason: reason)
        #expect(await probe.next() == .refused(reason))
        #expect(probe.outcomes == [.refused(reason)])
        #expect(rig.dispatcher.paOwnsOutcome(admission))
        #expect(rig.route.sent.count == 1)
        #expect(rig.dispatcher.paPanel(try rig.control("table"), in: "pa").profiles?.bands[0].gain == 50)
    }

}
