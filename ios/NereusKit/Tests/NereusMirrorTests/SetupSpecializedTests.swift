// NereusSDR for iOS: the closed Setup panels read their values whole, send exactly the described commands, and cancel when things move on
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

/// Task 58 step 1, the closed panels: the notch table (setup description
/// v2), the settings check, the antenna rows and the two PA readings.
/// The Core's own category files are read from the checkout at run time.
@MainActor
@Suite struct SetupSpecializedTests {
    static let mac = "00:1C:C0:A2:13:5F"

    // MARK: The Core's closed controls

    static func resourceControl(_ category: String, _ id: String) throws -> [String: Any] {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/\(category).json").standardizedFileURL
        let root = try #require(JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
        for page in try #require(root["pages"] as? [[String: Any]]) {
            for section in try #require(page["sections"] as? [[String: Any]]) {
                for control in try #require(section["controls"] as? [[String: Any]]) where control["id"] as? String == id {
                    return control
                }
            }
        }
        throw SetupDescription.ParseError("no \(id)")
    }

    static func category(_ id: String, version: Int, controls: [[String: Any]]) throws -> String {
        let root: [String: Any] = [
            "version": version,
            "category": ["id": id, "title": id, "where": "station"],
            "pages": [["id": "\(id).page", "title": "Page", "where": "station",
                       "sections": [["title": "Section", "controls": controls]]]],
        ]
        return String(decoding: try JSONSerialization.data(withJSONObject: root), as: UTF8.self)
    }

    /// The hardware tables as the Core sends them: with 2 m's row last
    /// (the resource, for a phone at `band2mVersion` 1), or without it.
    static func antennaControl(_ id: String, band2m: Bool) throws -> [String: Any] {
        var control = try resourceControl("hardware", id)
        if !band2m {
            control["rows"] = try #require(control["rows"] as? [[String: Any]]).filter {
                $0["band"] as? Int != SetupDescription.band2m
            }
        }
        return control
    }

    static func categories(band2m: Bool = true, legacyHygiene: Bool = false) throws -> [(String, String)] {
        var health = try resourceControl("diagnostics", "diagnostics.settingsValidation.health")
        if legacyHygiene {
            var actions = try #require(health["actions"] as? [[String: Any]])
            actions[1] = SetupDescriptionTests.legacyReset
            health["actions"] = actions
        }
        return [
            ("dsp", try category("dsp", version: 3, controls: [resourceControl("dsp", "dsp.tnf.list"),
                                                                resourceControl("dsp", "dsp.tnf.add")])),
            ("diagnostics", try category("diagnostics", version: 3, controls: [health])),
            ("hardware", try category("hardware", version: 6, controls: [
                antennaControl("hardware.antenna.txRows", band2m: band2m),
                antennaControl("hardware.antenna.rxRows", band2m: band2m)])),
            ("pa", try category("pa", version: 5, controls: [resourceControl("pa", "pa.values.paCurrent"),
                                                             resourceControl("pa", "pa.values.dcVoltage")])),
        ]
    }

    static let twoNotches = #"[{"id":3,"centreHz":7074000,"widthHz":100,"active":true},{"id":5,"centreHz":14074000,"widthHz":250,"active":false}]"#

    // MARK: The rig

    final class Flag: @unchecked Sendable {
        var on = true
    }

    @MainActor final class Rig {
        let route = TestRoute()
        let clock = ManualLinkClock()
        let store: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let selection = CurrentValueSubject<Int?, Never>(0)
        let paired = Flag()
        let dispatcher: SetupControlDispatcher

        init() {
            let route = route
            store = MirrorStore(send: { _ in }, clock: clock)
            settings = SettingsProxyClient(send: { _ in }, captureSender: { route.capture() })
            commands = CommandClient(send: { _ in }, captureSender: { route.capture() })
            feed = SetupDescriptionFeed(store: store)
            let selection = selection
            let paired = paired
            dispatcher = SetupControlDispatcher(store: store, feed: feed, settings: settings, commands: commands,
                                                captureSender: { route.capture() },
                                                selectedSlice: { selection.value },
                                                selectionChanges: selection.eraseToAnyPublisher(), phone: nil,
                                                signedInWithDeviceKey: { paired.on })
        }

        func event(_ event: StationSession.Event) async {
            store.handle(event)
            settings.handle(event)
            await commands.handle(event)
        }

        func settleUp() async {
            for _ in 0..<8 { await Task.yield() }
        }

        func connect(session: Int, keyed: Bool = false, notches: String = SetupSpecializedTests.twoNotches,
                     revision: Int64 = 7, txAntennas: String = "1,1,1,1,2,2,2,2,3,3,3,1,1,1,1",
                     rxAntennas: String = "1,1,1,1,1,1,1,1,1,1,1,1,1,1,1",
                     rxOnlyAntennas: String = "0,0,0,0,0,0,0,0,0,0,0,0,0,2,0", band2m: Bool = true,
                     telemetry: Int64 = 4, mac: String = SetupSpecializedTests.mac,
                     hygieneVersion: Int64 = 2, legacyHygiene: Bool = false) async throws {
            route.use(session)
            await event(.stateChanged(.receivingSnapshot))
            let categories = try SetupSpecializedTests.categories(band2m: band2m, legacyHygiene: legacyHygiene)
            var fields = [LinkMessage.SchemaField(ordinal: 0, name: "revision", kind: .i64)]
            var values = [LinkMessage.PropertyEntry(ordinal: 0, name: "revision", value: .i64(1))]
            for (index, category) in categories.enumerated() {
                fields.append(.init(ordinal: UInt16(index + 1), name: category.0, kind: .utf8))
                values.append(.init(ordinal: UInt16(index + 1), name: category.0, value: .utf8(category.1)))
            }
            let messages: [LinkMessage] = [
                FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                .capabilities(.init(properties: [LinkMessage.PropertyEntry]([
                    .init(name: "setupDescriptionVersion", value: .i64(10)),
                    .init(name: "notchControlVersion", value: .i64(2)),
                    .init(name: "settingsHygieneVersion", value: .i64(hygieneVersion)),
                    .init(name: "radioAntennaRowsVersion", value: .i64(1)),
                    .init(name: "stationTelemetryVersion", value: .i64(telemetry)),
                ]) + (band2m ? [.init(name: "band2mVersion", value: .i64(1))] : []) + [
                    .init(name: "macAddress", value: .utf8(mac)),
                    .init(name: "radioConnected", value: .bool(true)),
                    .init(name: "txPermitted", value: .bool(false)),
                ])),
                .schema(.init(className: "SetupDescription", fields: fields)),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: values)),
                .objectCreate(.init(key: "slice:0", className: "SliceModel", properties: [
                    .init(name: "sliceIndex", value: .i64(0)),
                ])),
                .objectCreate(.init(key: "slice:1", className: "SliceModel", properties: [
                    .init(name: "sliceIndex", value: .i64(1)),
                ])),
                .objectCreate(.init(key: "notches", className: "NotchModel", properties: [
                    .init(name: "listJson", value: .utf8(notches)), .init(name: "revision", value: .i64(revision)),
                ])),
                .objectCreate(.init(key: "alexAntennas", className: "AlexAntennaFacade", properties: [
                    .init(name: "txAntennas", value: .utf8(txAntennas)),
                    .init(name: "rxAntennas", value: .utf8(rxAntennas)),
                    .init(name: "rxOnlyAntennas", value: .utf8(rxOnlyAntennas)),
                    .init(name: "blockTxAnt2", value: .bool(false)), .init(name: "blockTxAnt3", value: .bool(true)),
                ])),
                .objectCreate(.init(key: "txState", className: "TransmitState", properties: [
                    .init(name: "keyed", value: .bool(keyed)), .init(name: "tuning", value: .bool(false)),
                    .init(name: "twoTone", value: .bool(false)), .init(name: "txEnding", value: .bool(false)),
                ])),
                .snapshotComplete,
            ]
            for message in messages { await event(.message(message)) }
            await event(.stateChanged(.ready))
            await settleUp()
        }

        func control(_ category: String, _ id: String) -> SetupDescription.Control {
            guard let description = feed.description(for: category) else {
                preconditionFailure("no current \(category) description")
            }
            return description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == id }!
        }

        func answer(_ index: Int, accepted: Bool, reason: String = "",
                    values: [LinkMessage.PropertyEntry]? = nil) async {
            guard case .commandInvoke(let invoke) = route.sent[index].message else {
                Issue.record("no command at \(index)")
                return
            }
            await commands.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                        reason: reason, affected: [], values: values)))
        }

        func invoke(_ index: Int) -> LinkMessage.CommandInvoke? {
            guard route.sent.count > index, case .commandInvoke(let invoke) = route.sent[index].message else {
                return nil
            }
            return invoke
        }

        func setTxState(keyed: Bool) async {
            store.apply(.delta(.init(key: "txState", properties: [.init(name: "keyed", value: .bool(keyed))])))
            await settleUp()
        }
    }

    // MARK: The notch list is read whole

    @Test func theNotchListIsReadWholeOrRefusedWithAReason() throws {
        let control = try SetupDescription.parse(json: try Self.categories()[0].1).pages[0].sections[0].controls[0]
        guard case .table(let table)? = control.binding else {
            Issue.record("the TNF table has no table binding"); return
        }
        func read(_ json: String, revision: MirrorValue? = .int(4)) -> Result<SetupNotchList, SetupSpecializedWire.NotchFailure> {
            SetupSpecializedWire.notches(json: .text(json), revision: revision, table: table)
        }
        guard case .success(let list) = read(Self.twoNotches) else {
            Issue.record("a good list was refused"); return
        }
        #expect(list.revision == 4)
        #expect(list.rows == [SetupNotch(id: 3, centreHz: 7_074_000, widthHz: 100, active: true),
                              SetupNotch(id: 5, centreHz: 14_074_000, widthHz: 250, active: false)])
        #expect(read("[]").map(\.rows) == .success([]))
        // Malformed: a missing, an extra or a wrongly typed field, an id out of range, not an array.
        for bad in [#"[{"id":3,"centreHz":7074000,"widthHz":100}]"#,
                    #"[{"id":3,"centreHz":7074000,"widthHz":100,"active":true,"x":1}]"#,
                    #"[{"id":true,"centreHz":7074000,"widthHz":100,"active":true}]"#,
                    #"[{"id":3.5,"centreHz":7074000,"widthHz":100,"active":true}]"#,
                    #"[{"id":0,"centreHz":7074000,"widthHz":100,"active":true}]"#,
                    #"[{"id":2147483648,"centreHz":7074000,"widthHz":100,"active":true}]"#,
                    #"[{"id":3,"centreHz":"7074000","widthHz":100,"active":true}]"#,
                    #"[{"id":3,"centreHz":7074000,"widthHz":100,"active":1}]"#,
                    #"{"id":3}"#, "not json"] {
            #expect(read(bad) == .failure(.unreadable), "\(bad)")
        }
        // A bad revision refuses the list too.
        #expect(read(Self.twoNotches, revision: nil) == .failure(.unreadable))
        #expect(read(Self.twoNotches, revision: .int(-1)) == .failure(.unreadable))
        #expect(read(Self.twoNotches, revision: .text("4")) == .failure(.unreadable))
        // Out of range, and not finite.
        #expect(read(#"[{"id":3,"centreHz":99999,"widthHz":100,"active":true}]"#) == .failure(.outOfRange))
        #expect(read(#"[{"id":3,"centreHz":7074000,"widthHz":10001,"active":true}]"#) == .failure(.outOfRange))
        #expect(read(#"[{"id":3,"centreHz":7074000,"widthHz":-1,"active":true}]"#) == .failure(.outOfRange))
        let huge = read(#"[{"id":3,"centreHz":1e999,"widthHz":100,"active":true}]"#)
        #expect(huge == .failure(.outOfRange) || huge == .failure(.unreadable))
        // Duplicates.
        #expect(read(#"[{"id":3,"centreHz":7074000,"widthHz":100,"active":true},{"id":3,"centreHz":7075000,"widthHz":100,"active":true}]"#)
                == .failure(.duplicate))
        // More rows than the documented maximum.
        let row = #"{"id":%d,"centreHz":7074000,"widthHz":100,"active":true}"#
        let full = "[" + (1...1024).map { String(format: row, $0) }.joined(separator: ",") + "]"
        let over = "[" + (1...1025).map { String(format: row, $0) }.joined(separator: ",") + "]"
        #expect(read(full).map(\.rows.count) == .success(1024))
        #expect(read(over) == .failure(.tooLong))
    }

    @Test func aRefusedListIsShownWithItsReasonAndCannotBeEdited() async throws {
        let rig = Rig()
        try await rig.connect(session: 1, notches: #"[{"id":3,"centreHz":7074000,"widthHz":100,"active":true},{"id":3,"centreHz":7075000,"widthHz":100,"active":true}]"#)
        let table = rig.control("dsp", "dsp.tnf.list")
        let state = rig.dispatcher.notchTable(table, in: "dsp")
        #expect(state.list == nil)
        #expect(state.listReason == SetupControlDispatcher.notchListDuplicateReason)
        #expect(state.reason == SetupControlDispatcher.notchListDuplicateReason)
        if case .success = rig.dispatcher.admitNotch(table, in: "dsp", row: 3) { Issue.record("admitted") }
        #expect(rig.route.sent.isEmpty)
    }

    // MARK: The notch table's row actions

    @Test func eachRowActionSendsItsDescribedCommand() async throws {
        let rig = Rig()
        try await rig.connect(session: 1)
        let table = rig.control("dsp", "dsp.tnf.list")
        let dispatcher = rig.dispatcher
        #expect(dispatcher.notchTable(table, in: "dsp").reason == nil)

        // Move: centre and width together, the row's id.
        let move = try dispatcher.admitNotch(table, in: "dsp", row: 3).get()
        let moving = Task { await dispatcher.performNotch(move, edit: .move(centreHz: 7_075_000, widthHz: 150)) }
        #expect(await rig.route.waitForCount(1, unless: moving))
        let moved = try #require(rig.invoke(0))
        #expect(moved.verb == "notch.move")
        #expect(moved.args == [.init(name: "centreHz", value: .f64(7_075_000)), .init(name: "id", value: .i64(3)),
                               .init(name: "widthHz", value: .f64(150))])
        await rig.answer(0, accepted: true)
        #expect(await moving.value == .applied)

        // Active.
        let active = try dispatcher.admitNotch(table, in: "dsp", row: 5).get()
        let activating = Task { await dispatcher.performNotch(active, edit: .setActive(true)) }
        #expect(await rig.route.waitForCount(2, unless: activating))
        let activated = try #require(rig.invoke(1))
        #expect(activated.verb == "notch.setActive")
        #expect(activated.args == [.init(name: "active", value: .bool(true)), .init(name: "id", value: .i64(5))])
        await rig.answer(1, accepted: false, reason: "That notch is no longer on this Core.")
        #expect(await activating.value == .refused("That notch is no longer on this Core."))

        // Delete.
        let delete = try dispatcher.admitNotch(table, in: "dsp", row: 5).get()
        let deleting = Task { await dispatcher.performNotch(delete, edit: .delete) }
        #expect(await rig.route.waitForCount(3, unless: deleting))
        let deleted = try #require(rig.invoke(2))
        #expect(deleted.verb == "notch.delete" && deleted.args == [.init(name: "id", value: .i64(5))])
        await rig.answer(2, accepted: true)
        #expect(await deleting.value == .applied)

        // Outside the columns' ranges nothing is sent; a row that is not there is refused.
        let wide = try dispatcher.admitNotch(table, in: "dsp", row: 3).get()
        #expect(await dispatcher.performNotch(wide, edit: .move(centreHz: 7_074_000, widthHz: 10_001))
                == .notSent(SetupControlDispatcher.outOfRangeReason))
        #expect(await dispatcher.performNotch(try dispatcher.admitNotch(table, in: "dsp", row: 3).get(),
                                              edit: .move(centreHz: .nan, widthHz: 100))
                == .notSent(SetupControlDispatcher.outOfRangeReason))
        guard case .failure(let gone) = dispatcher.admitNotch(table, in: "dsp", row: 99) else {
            Issue.record("a missing row was admitted"); return
        }
        #expect(gone.reason == SetupControlDispatcher.notchGoneReason)
        #expect(rig.route.sent.count == 3)

        // Held for a question: not a refusal (the link document, section 7.3).
        let held = try dispatcher.admitNotch(table, in: "dsp", row: 3).get()
        let holding = Task { await dispatcher.performNotch(held, edit: .move(centreHz: 7_076_000, widthHz: 150)) }
        #expect(await rig.route.waitForCount(4, unless: holding))
        await rig.answer(3, accepted: false, reason: SeveralDevices.waitingReason,
                         values: [.init(name: "phase", value: .utf8(SeveralDevices.needsConfirmationPhase))])
        let waited = await holding.value
        #expect(waited != .refused(SeveralDevices.waitingReason))
        #expect(waited == .awaitingConfirmation)
    }

    @Test func aNewRevisionARemovedRowAnotherSelectionOrSessionCancelsTheEdit() async throws {
        let rig = Rig()
        try await rig.connect(session: 1)
        let table = rig.control("dsp", "dsp.tnf.list")
        let dispatcher = rig.dispatcher

        // The list's revision moves on between the gesture and the send.
        let edit = try dispatcher.admitNotch(table, in: "dsp", row: 3).get()
        rig.store.apply(.delta(.init(key: "notches", properties: [.init(name: "revision", value: .i64(8))])))
        await rig.settleUp()
        #expect(edit.isRevoked)
        #expect(await dispatcher.performNotch(edit, edit: .setActive(false))
                == .notSent(SetupControlDispatcher.notchChangedReason))

        // The row goes, at a new revision.
        let second = try dispatcher.admitNotch(table, in: "dsp", row: 5).get()
        rig.store.apply(.delta(.init(key: "notches", properties: [
            .init(name: "listJson", value: .utf8(#"[{"id":3,"centreHz":7074000,"widthHz":100,"active":true}]"#)),
            .init(name: "revision", value: .i64(9)),
        ])))
        await rig.settleUp()
        #expect(second.isRevoked)
        #expect(await dispatcher.performNotch(second, edit: .delete) == .notSent(SetupControlDispatcher.notchChangedReason))

        // Another slice is selected.
        let third = try dispatcher.admitNotch(table, in: "dsp", row: 3).get()
        rig.selection.send(1)
        await rig.settleUp()
        #expect(third.isRevoked)
        #expect(await dispatcher.performNotch(third, edit: .delete) == .notSent(SetupControlDispatcher.notchChangedReason))

        // Held at the handoff while the list changes: the permit is gone, nothing leaves.
        let hold = HandoffHold()
        rig.route.holdNext(hold)
        let fourth = try dispatcher.admitNotch(table, in: "dsp", row: 3).get()
        let sending = Task { await dispatcher.performNotch(fourth, edit: .setActive(false)) }
        await hold.waitUntilEntered()
        rig.store.apply(.delta(.init(key: "notches", properties: [.init(name: "revision", value: .i64(10))])))
        await rig.settleUp()
        await hold.release()
        if case .applied = await sending.value { Issue.record("a stale notch edit was applied") }

        // A new session: an edit admitted in the old one never reaches it.
        let fifth = try dispatcher.admitNotch(table, in: "dsp", row: 3).get()
        await rig.event(.stateChanged(.waitingToRetry(seconds: 1)))
        try await rig.connect(session: 2)
        if case .applied = await dispatcher.performNotch(fifth, edit: .delete) { Issue.record("old session sent") }
        #expect(rig.route.sent.isEmpty)
    }

    // MARK: The settings check

    static func report(_ mac: String, _ issues: String) -> [LinkMessage.PropertyEntry] {
        [.init(name: "mac", value: .utf8(mac)), .init(name: "issuesJson", value: .utf8(issues))]
    }

    @Test func reValidateSendsTheRadioAndShowsTheCoresAnswer() async throws {
        let rig = Rig()
        try await rig.connect(session: 1)
        let health = rig.control("diagnostics", "diagnostics.settingsValidation.health")
        let dispatcher = rig.dispatcher
        var panel = dispatcher.hygiene(health, in: "diagnostics")
        #expect(panel.report == nil && panel.reportReason == SetupControlDispatcher.hygieneNotCheckedText)
        #expect(panel.validateReason == nil && panel.forgetReason == nil)
        #expect(panel.repairReason == nil && panel.repairLabel == "Repair Invalid Settings")
        #expect(panel.repairConfirmation?.title == "Repair Settings")
        #expect(panel.resetLabel.isEmpty)
        #expect(panel.confirmation?.defaultAction == "cancel")

        let check = try dispatcher.admitHygiene(health, in: "diagnostics", action: .validate).get()
        let checking = Task { await dispatcher.performHygiene(check) }
        #expect(await rig.route.waitForCount(1, unless: checking))
        let sent = try #require(rig.invoke(0))
        #expect(sent.verb == "station.validateSettings" && sent.args == [.init(name: "mac", value: .utf8(Self.mac))])
        // Serialized: a second check waits until the first is answered.
        #expect(dispatcher.hygiene(health, in: "diagnostics").validateReason == SetupControlDispatcher.hygieneBusyReason)
        guard case .failure = dispatcher.admitHygiene(health, in: "diagnostics", action: .validate) else {
            Issue.record("a second check was admitted while one was waiting"); return
        }
        let issues = #"[{"severity":"warning","key":"hardware/00:1C:C0:A2:13:5F/rate","summary":"Sample rate","detail":"Too high for this radio.","fixActionId":"clampRate"}]"#
        await rig.answer(0, accepted: true, values: Self.report(Self.mac, issues))
        #expect(await checking.value == .applied)
        panel = dispatcher.hygiene(health, in: "diagnostics")
        #expect(panel.report?.issues.map(\.summary) == ["Sample rate"])
        #expect(panel.report?.issues.first?.severity == .warning)
        #expect(panel.reportReason == nil)

        // An empty list is a healthy radio only when the Core says so.
        let again = try dispatcher.admitHygiene(health, in: "diagnostics", action: .validate).get()
        let checkingAgain = Task { await dispatcher.performHygiene(again) }
        #expect(await rig.route.waitForCount(2, unless: checkingAgain))
        await rig.answer(1, accepted: true, values: Self.report(Self.mac, "[]"))
        #expect(await checkingAgain.value == .applied)
        #expect(dispatcher.hygiene(health, in: "diagnostics").report?.issues == [])
    }

    @Test func aMalformedStaleOrRefusedAnswerIsUnavailableNeverAnEmptyList() async throws {
        let rig = Rig()
        try await rig.connect(session: 1)
        let health = rig.control("diagnostics", "diagnostics.settingsValidation.health")
        let dispatcher = rig.dispatcher
        let answers: [(accepted: Bool, reason: String, values: [LinkMessage.PropertyEntry]?, shown: String)] = [
            (true, "", Self.report("00:1C:C0:A2:13:60", "[]"), SetupControlDispatcher.hygieneUnreadableReason),
            (true, "", Self.report(Self.mac, "{}"), SetupControlDispatcher.hygieneUnreadableReason),
            (true, "", Self.report(Self.mac, #"[{"severity":"bad","key":"","summary":"","detail":"","fixActionId":""}]"#),
             SetupControlDispatcher.hygieneUnreadableReason),
            (true, "", [.init(name: "mac", value: .utf8(Self.mac))], SetupControlDispatcher.hygieneUnreadableReason),
            (true, "", Self.report(Self.mac, "[]") + [.init(name: "extra", value: .utf8(""))],
             SetupControlDispatcher.hygieneUnreadableReason),
            (true, "", [.init(name: "mac", value: .utf8(Self.mac)), .init(name: "issuesJson", value: .i64(0))],
             SetupControlDispatcher.hygieneUnreadableReason),
            (true, "", Self.report(Self.mac, "[" + Array(repeating: #"{"severity":"info","key":"","summary":"","detail":"","fixActionId":""}"#, count: 33).joined(separator: ",") + "]"),
             SetupControlDispatcher.hygieneUnreadableReason),
            (false, "That radio is no longer connected to the Core.", nil, "That radio is no longer connected to the Core."),
        ]
        for (index, answer) in answers.enumerated() {
            let check = try dispatcher.admitHygiene(health, in: "diagnostics", action: .validate).get()
            let checking = Task { await dispatcher.performHygiene(check) }
            #expect(await rig.route.waitForCount(index + 1, unless: checking))
            await rig.answer(index, accepted: answer.accepted, reason: answer.reason, values: answer.values)
            _ = await checking.value
            let panel = dispatcher.hygiene(health, in: "diagnostics")
            #expect(panel.report == nil, "answer \(index)")
            #expect(panel.reportReason == answer.shown, "answer \(index)")
        }
        // Away from the Core no answer is shown as current.
        await rig.event(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(dispatcher.hygiene(health, in: "diagnostics").report == nil)
        #expect(dispatcher.hygiene(health, in: "diagnostics").validateReason == SetupControlDispatcher.notConnectedReason)
    }

    @Test func forgetNeedsPairingOffAirAndTheSameRadioAfterItsQuestion() async throws {
        let rig = Rig()
        try await rig.connect(session: 1)
        let health = rig.control("diagnostics", "diagnostics.settingsValidation.health")
        let dispatcher = rig.dispatcher

        // Not signed in with this phone's paired key.
        rig.paired.on = false
        #expect(dispatcher.hygiene(health, in: "diagnostics").forgetReason == SetupControlDispatcher.hygienePairReason)
        #expect(dispatcher.hygiene(health, in: "diagnostics").repairReason
                == SetupControlDispatcher.hygieneRepairPairReason)
        rig.paired.on = true

        // On the air: Forget waits; Re-validate does not.
        await rig.setTxState(keyed: true)
        #expect(dispatcher.hygiene(health, in: "diagnostics").forgetReason == SetupControlDispatcher.onAirReason)
        #expect(dispatcher.hygiene(health, in: "diagnostics").repairReason == SetupControlDispatcher.onAirReason)
        #expect(dispatcher.hygiene(health, in: "diagnostics").validateReason == nil)
        await rig.setTxState(keyed: false)

        // Admitted, then keyed while the question is open: nothing is sent.
        let keyedDuring = try dispatcher.admitHygiene(health, in: "diagnostics", action: .forget).get()
        await rig.setTxState(keyed: true)
        #expect(keyedDuring.isRevoked)
        #expect(await dispatcher.performHygiene(keyedDuring) == .notSent(SetupControlDispatcher.changedFirstReason))
        await rig.setTxState(keyed: false)

        // Admitted, then the Core names another radio: nothing is sent.
        let otherRadio = try dispatcher.admitHygiene(health, in: "diagnostics", action: .forget).get()
        rig.store.apply(.capabilities(.init(properties: rig.store.capabilities.map {
            LinkMessage.PropertyEntry(name: $0.key, value: $0.key == "macAddress" ? .utf8("00:1C:C0:A2:13:60") : $0.value.wireValue)
        })))
        await rig.settleUp()
        #expect(otherRadio.isRevoked)
        #expect(await dispatcher.performHygiene(otherRadio) == .notSent(SetupControlDispatcher.changedFirstReason))

        // Confirmed on the same radio: forget sends only that radio's address.
        let forget = try dispatcher.admitHygiene(health, in: "diagnostics", action: .forget).get()
        let forgetting = Task { await dispatcher.performHygiene(forget) }
        #expect(await rig.route.waitForCount(1, unless: forgetting))
        let sent = try #require(rig.invoke(0))
        #expect(sent.verb == "station.forgetSettings"
                && sent.args == [.init(name: "mac", value: .utf8("00:1C:C0:A2:13:60"))])
        await rig.answer(0, accepted: true, values: Self.report("00:1C:C0:A2:13:60", "[]"))
        #expect(await forgetting.value == .applied)
        #expect(rig.route.sent.count == 1)
        #expect(!rig.route.sent.contains { entry in
            if case .commandInvoke(let invoke) = entry.message { return invoke.verb.lowercased().contains("reset") }
            return false
        })
    }

    @Test func repairSendsThisRadioAfterItsQuestionUnderForgetsRules() async throws {
        let rig = Rig()
        try await rig.connect(session: 1)
        let health = rig.control("diagnostics", "diagnostics.settingsValidation.health")
        let dispatcher = rig.dispatcher

        // Admitted, then keyed while the question is open: nothing is sent.
        let keyedDuring = try dispatcher.admitHygiene(health, in: "diagnostics", action: .repair).get()
        await rig.setTxState(keyed: true)
        #expect(keyedDuring.isRevoked)
        #expect(await dispatcher.performHygiene(keyedDuring) == .notSent(SetupControlDispatcher.changedFirstReason))
        await rig.setTxState(keyed: false)

        // Admitted, then this phone is no longer signed in with its key.
        let unpaired = try dispatcher.admitHygiene(health, in: "diagnostics", action: .repair).get()
        rig.paired.on = false
        #expect(await dispatcher.performHygiene(unpaired) == .notSent(SetupControlDispatcher.changedFirstReason))
        rig.paired.on = true

        // Confirmed: repair sends only this radio's address and shows the
        // Core's answer as the current check.
        let repair = try dispatcher.admitHygiene(health, in: "diagnostics", action: .repair).get()
        let repairing = Task { await dispatcher.performHygiene(repair) }
        #expect(await rig.route.waitForCount(1, unless: repairing))
        let sent = try #require(rig.invoke(0))
        #expect(sent.verb == "station.repairSettings" && sent.args == [.init(name: "mac", value: .utf8(Self.mac))])
        #expect(dispatcher.hygiene(health, in: "diagnostics").repairReason == SetupControlDispatcher.hygieneBusyReason)
        await rig.answer(0, accepted: true, values: Self.report(Self.mac, "[]"))
        #expect(await repairing.value == .applied)
        #expect(dispatcher.hygiene(health, in: "diagnostics").report?.issues == [])
        #expect(rig.route.sent.count == 1)
    }

    @Test func anOlderCoreKeepsRepairOrResetGreyedInItsOwnWords() async throws {
        // A Core that describes Repair but answers settingsHygieneVersion 1.
        let rig = Rig()
        try await rig.connect(session: 1, hygieneVersion: 1)
        let health = rig.control("diagnostics", "diagnostics.settingsValidation.health")
        var panel = rig.dispatcher.hygiene(health, in: "diagnostics")
        #expect(panel.repairReason == "Repair invalid settings is not available on this Core. Updating the Core may help.")
        #expect(panel.validateReason == nil && panel.forgetReason == nil)
        guard case .failure = rig.dispatcher.admitHygiene(health, in: "diagnostics", action: .repair) else {
            Issue.record("repair was admitted on a version 1 Core"); return
        }

        // A version 1 Core's own description: Reset greyed, as today.
        let older = Rig()
        try await older.connect(session: 1, hygieneVersion: 1, legacyHygiene: true)
        let legacy = older.control("diagnostics", "diagnostics.settingsValidation.health")
        panel = older.dispatcher.hygiene(legacy, in: "diagnostics")
        #expect(panel.resetLabel == "Reset to Defaults")
        #expect(panel.resetReason == "Reset to defaults is not available on this Core.")
        #expect(panel.repairLabel.isEmpty && panel.repairReason != nil)
        #expect(panel.validateReason == nil && panel.forgetReason == nil)
        guard case .failure = older.dispatcher.admitHygiene(legacy, in: "diagnostics", action: .repair) else {
            Issue.record("repair was admitted without the Core describing it"); return
        }
        #expect(rig.route.sent.isEmpty && older.route.sent.isEmpty)
    }

    @Test func noRadioOrAnUnreadableAddressDisablesTheCheck() async throws {
        let rig = Rig()
        try await rig.connect(session: 1, mac: "00:1c:c0:a2:13:5f")
        let health = rig.control("diagnostics", "diagnostics.settingsValidation.health")
        #expect(rig.dispatcher.hygiene(health, in: "diagnostics").validateReason
                == SetupControlDispatcher.radioUnknownReason)
        let tx = rig.control("hardware", "hardware.antenna.txRows")
        #expect(rig.dispatcher.antennaTable(tx, in: "hardware").reason == SetupControlDispatcher.radioUnknownReason)
        rig.store.apply(.capabilities(.init(properties: [
            .init(name: "settingsHygieneVersion", value: .i64(1)), .init(name: "radioAntennaRowsVersion", value: .i64(1)),
            .init(name: "setupDescriptionVersion", value: .i64(10)), .init(name: "radioConnected", value: .bool(false)),
            .init(name: "macAddress", value: .utf8(Self.mac)),
        ])))
        await rig.settleUp()
        #expect(rig.dispatcher.hygiene(health, in: "diagnostics").validateReason == SetupControlDispatcher.noRadioReason)
        rig.store.apply(.capabilities(.init(properties: [
            .init(name: "setupDescriptionVersion", value: .i64(10)), .init(name: "radioConnected", value: .bool(true)),
            .init(name: "macAddress", value: .utf8(Self.mac)),
        ])))
        await rig.settleUp()
        #expect(rig.dispatcher.hygiene(health, in: "diagnostics").validateReason
                == SetupControlDispatcher.needsNewerCoreReason)
        #expect(rig.route.sent.isEmpty)
    }

    // MARK: The antenna rows

    @Test func anAntennaCellSendsItsBandsVerbForTheCurrentRadio() async throws {
        let rig = Rig()
        try await rig.connect(session: 1)
        let dispatcher = rig.dispatcher
        let tx = rig.control("hardware", "hardware.antenna.txRows")
        let rx = rig.control("hardware", "hardware.antenna.rxRows")
        let txTable = dispatcher.antennaTable(tx, in: "hardware")
        #expect(txTable.reason == nil)
        #expect(txTable.grid?.selected[4] == [false, true, false])
        #expect(txTable.grid?.blocked == [false, false, true])
        let rxTable = dispatcher.antennaTable(rx, in: "hardware")
        #expect(rxTable.grid?.selected[13] == [true, false, false, false, true, false])
        #expect(rxTable.grid?.blocked == [false, false, false, false, false, false])

        let choose = try dispatcher.admitAntenna(tx, in: "hardware").get()
        let choosing = Task { await dispatcher.performAntenna(choose, band: 3, column: "tx2") }
        #expect(await rig.route.waitForCount(1, unless: choosing))
        let sent = try #require(rig.invoke(0))
        #expect(sent.verb == "setAlexTxAntennaForRadio")
        #expect(sent.args == [.init(name: "mac", value: .utf8(Self.mac)), .init(name: "band", value: .i64(3)),
                              .init(name: "antenna", value: .i64(2))])
        await rig.answer(0, accepted: true)
        #expect(await choosing.value == .applied)

        let receive = try dispatcher.admitAntenna(rx, in: "hardware").get()
        let receiving = Task { await dispatcher.performAntenna(receive, band: 0, column: "rxOnly3") }
        #expect(await rig.route.waitForCount(2, unless: receiving))
        let rxSent = try #require(rig.invoke(1))
        #expect(rxSent.verb == "setAlexRxAntennaForRadio")
        #expect(rxSent.args == [.init(name: "mac", value: .utf8(Self.mac)), .init(name: "band", value: .i64(0)),
                                .init(name: "antenna", value: .i64(3)), .init(name: "rxOnly", value: .bool(true))])
        await rig.answer(1, accepted: false, reason: "An antenna blocked for transmit cannot be a band's TX antenna.")
        #expect(await receiving.value == .refused("An antenna blocked for transmit cannot be a band's TX antenna."))

        // A blocked port and the antenna already chosen send nothing.
        #expect(await dispatcher.performAntenna(try dispatcher.admitAntenna(tx, in: "hardware").get(), band: 0, column: "tx3")
                == .notSent(SetupControlDispatcher.antennaBlockedReason))
        #expect(await dispatcher.performAntenna(try dispatcher.admitAntenna(tx, in: "hardware").get(), band: 0, column: "tx1")
                == .applied)
        #expect(rig.route.sent.count == 2)
    }

    @Test func transmitRowsLockOnTheAirAndReceiveRowsDoNot() async throws {
        let rig = Rig()
        try await rig.connect(session: 1)
        let dispatcher = rig.dispatcher
        let tx = rig.control("hardware", "hardware.antenna.txRows")
        let rx = rig.control("hardware", "hardware.antenna.rxRows")
        let admitted = try dispatcher.admitAntenna(tx, in: "hardware").get()
        await rig.setTxState(keyed: true)
        #expect(admitted.isRevoked)
        #expect(dispatcher.antennaTable(tx, in: "hardware").reason == SetupControlDispatcher.onAirReason)
        #expect(dispatcher.antennaTable(rx, in: "hardware").reason == nil)
        #expect(await dispatcher.performAntenna(admitted, band: 3, column: "tx2")
                == .notSent(SetupControlDispatcher.changedFirstReason))
        // Receive rows need no transmit permission, even with transmit held elsewhere.
        #expect(rig.store.capabilities["txPermitted"] == .bool(false))
        let receive = try dispatcher.admitAntenna(rx, in: "hardware").get()
        let receiving = Task { await dispatcher.performAntenna(receive, band: 1, column: "rx2") }
        #expect(await rig.route.waitForCount(1, unless: receiving))
        #expect(rig.invoke(0)?.verb == "setAlexRxAntennaForRadio")
        await rig.answer(0, accepted: true)
        #expect(await receiving.value == .applied)
    }

    @Test func unreadableAntennaValuesDisableTheWholeTable() async throws {
        // Fourteen entries where the Core described 2 m's row is unreadable too.
        for bad in ["1,1,1", "1,1,1,1,2,2,2,2,3,3,3,1,1,1,4", "1,1,1,1,2,2,2,2,3,3,3,1,1,1, 1", "1,1,1,1,2,2,2,2,3,3,3,1,1,1,",
                    "1,1,1,1,2,2,2,2,3,3,3,1,1,1"] {
            let rig = Rig()
            try await rig.connect(session: 1, txAntennas: bad)
            let tx = rig.control("hardware", "hardware.antenna.txRows")
            let table = rig.dispatcher.antennaTable(tx, in: "hardware")
            #expect(table.grid == nil && table.reason == SetupControlDispatcher.antennaUnreadableReason, "\(bad)")
            if case .success = rig.dispatcher.admitAntenna(tx, in: "hardware") { Issue.record("admitted \(bad)") }
        }
    }

    @Test func twoMetresIsTheFifteenthEntryAndItsVerbNamesBand27() async throws {
        let rig = Rig()
        try await rig.connect(session: 1, txAntennas: "1,1,1,1,2,2,2,2,3,3,3,1,1,1,2",
                              rxOnlyAntennas: "0,0,0,0,0,0,0,0,0,0,0,0,0,2,1")
        let dispatcher = rig.dispatcher
        let tx = rig.control("hardware", "hardware.antenna.txRows")
        let rx = rig.control("hardware", "hardware.antenna.rxRows")
        let txTable = dispatcher.antennaTable(tx, in: "hardware")
        #expect(txTable.reason == nil)
        #expect(txTable.grid?.selected.count == 15)
        // The fifteenth entry is 2 m's; XVTR's stays the fourteenth.
        #expect(txTable.grid?.selected[14] == [false, true, false])
        #expect(txTable.grid?.selected[13] == [true, false, false])
        #expect(dispatcher.antennaTable(rx, in: "hardware").grid?.selected[14] == [true, false, false, true, false, false])

        let receive = try dispatcher.admitAntenna(rx, in: "hardware").get()
        let receiving = Task { await dispatcher.performAntenna(receive, band: SetupDescription.band2m, column: "rx3") }
        #expect(await rig.route.waitForCount(1, unless: receiving))
        let sent = try #require(rig.invoke(0))
        #expect(sent.verb == "setAlexRxAntennaForRadio")
        #expect(sent.args == [.init(name: "mac", value: .utf8(Self.mac)), .init(name: "band", value: .i64(27)),
                              .init(name: "antenna", value: .i64(3)), .init(name: "rxOnly", value: .bool(false))])
        await rig.answer(0, accepted: true)
        #expect(await receiving.value == .applied)
        // 2 m's antenna already chosen sends nothing.
        #expect(await dispatcher.performAntenna(try dispatcher.admitAntenna(tx, in: "hardware").get(),
                                                band: SetupDescription.band2m, column: "tx2") == .applied)
        #expect(rig.route.sent.count == 1)
    }

    @Test func aCoreWithout2mSendsFourteenRowsAndEntries() async throws {
        let rig = Rig()
        try await rig.connect(session: 1, txAntennas: "1,1,1,1,2,2,2,2,3,3,3,1,1,1",
                              rxAntennas: "1,1,1,1,1,1,1,1,1,1,1,1,1,1",
                              rxOnlyAntennas: "0,0,0,0,0,0,0,0,0,0,0,0,0,2", band2m: false)
        let tx = rig.control("hardware", "hardware.antenna.txRows")
        let table = rig.dispatcher.antennaTable(tx, in: "hardware")
        #expect(table.reason == nil)
        #expect(table.grid?.selected.count == 14)
        #expect(table.grid?.selected[4] == [false, true, false])
        // No row names 2 m, so no tap can send band 27.
        if case .antennaRows(let rows)? = tx.binding {
            #expect(!rows.rows.contains { $0.band == SetupDescription.band2m })
        } else { Issue.record("Antenna rows were not typed") }
        #expect(await rig.dispatcher.performAntenna(try rig.dispatcher.admitAntenna(tx, in: "hardware").get(),
                                                    band: SetupDescription.band2m, column: "tx1")
                == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.isEmpty)
    }

    // MARK: The PA readings

    @Test func paReadingsShowOnlyFreshValuesWithTheirAge() async throws {
        let rig = Rig()
        try await rig.connect(session: 1)
        let amps = rig.control("pa", "pa.values.paCurrent")
        let volts = rig.control("pa", "pa.values.dcVoltage")
        let dispatcher = rig.dispatcher
        func now() -> Int64 { rig.clock.nowMilliseconds }
        #expect(dispatcher.telemetryReading(amps, in: "pa", nowMilliseconds: now()).reason
                == SetupControlDispatcher.telemetryWaitingReason)
        rig.store.apply(.stationMetrics(.init(payload: [
            "sequence": .number(1), "sampledElapsedMs": .number(1_000),
            "radio": .object(["connected": .bool(true), "paCurrentAmps": .number(0)]),
        ])))
        // A present zero is zero; an absent reading is absent, never zero.
        #expect(dispatcher.telemetryReading(amps, in: "pa", nowMilliseconds: now())
                == SetupTelemetryReading(value: 0, ageMilliseconds: 0, reason: nil))
        #expect(dispatcher.telemetryReading(volts, in: "pa", nowMilliseconds: now())
                == SetupTelemetryReading(value: nil, ageMilliseconds: nil,
                                         reason: SetupControlDispatcher.telemetryAbsentReason))
        await rig.clock.advance(by: 1_500)
        rig.store.apply(.stationMetrics(.init(payload: [
            "sequence": .number(2), "sampledElapsedMs": .number(2_000),
            "radio": .object(["connected": .bool(true), "paCurrentAmps": .number(1.25), "supplyVolts": .number(13.8)]),
        ])))
        await rig.clock.advance(by: 2_000)
        #expect(dispatcher.telemetryReading(volts, in: "pa", nowMilliseconds: now())
                == SetupTelemetryReading(value: 13.8, ageMilliseconds: 2_000, reason: nil))
        await rig.clock.advance(by: 1_001)
        #expect(dispatcher.telemetryReading(amps, in: "pa", nowMilliseconds: now()).reason
                == SetupControlDispatcher.telemetryOldReason)
        // Away from the Core, nothing from the last session.
        await rig.event(.stateChanged(.waitingToRetry(seconds: 1)))
        #expect(dispatcher.telemetryReading(amps, in: "pa", nowMilliseconds: now()).value == nil)
    }

    @Test func anOlderCoresTelemetryIsNeverRead() async throws {
        let rig = Rig()
        try await rig.connect(session: 1, telemetry: 3)
        let amps = rig.control("pa", "pa.values.paCurrent")
        rig.store.apply(.stationMetrics(.init(payload: [
            "sequence": .number(1), "sampledElapsedMs": .number(1_000),
            "radio": .object(["connected": .bool(true), "paCurrentAmps": .number(2)]),
        ])))
        #expect(rig.dispatcher.telemetryReading(amps, in: "pa", nowMilliseconds: rig.clock.nowMilliseconds)
                == SetupTelemetryReading(value: nil, ageMilliseconds: nil,
                                         reason: SetupControlDispatcher.needsNewerCoreReason))
    }
}
