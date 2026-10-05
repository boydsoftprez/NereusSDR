// NereusSDR for iOS: exact Filter Presets metadata and Core-authoritative sequential edits
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@MainActor
@Suite struct SetupFilterPresetsTests {
    static func description(mutate: ((inout [[String: Any]]) -> Void)? = nil) throws -> String {
        let resource = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../../resources/setup/dsp.json").standardizedFileURL
        var root = try #require(JSONSerialization.jsonObject(with: Data(contentsOf: resource)) as? [String: Any])
        let pages = try #require(root["pages"] as? [[String: Any]])
        var page = try #require(pages.first { $0["id"] as? String == "dsp.filterPresets" })
        let sections = try #require(page["sections"] as? [[String: Any]])
        var controls = sections.flatMap { $0["controls"] as? [[String: Any]] ?? [] }
        mutate?(&controls)
        page["sections"] = [["title": "Filter Presets", "controls": controls]]
        root["pages"] = [page]; root["version"] = 15
        // Core removes the version-19 coverage wording for a version-15 peer;
        // match SetupDescribedPagesTests.coreCategories(peer: 15).
        root.removeValue(forKey: "coverageV19")
        return String(decoding: try JSONSerialization.data(withJSONObject: root), as: UTF8.self)
    }
    static func controls(_ json: String) throws -> [SetupDescription.Control] {
        try SetupDescription.parse(json: json).pages.flatMap(\.sections).flatMap(\.controls)
    }
    static func catalog() throws -> [String: Any] {
        let entry = try #require(try LinkFixtureLoader.manifest().first { $0.id == "session-catalog-hermes-lite-2" })
        let fixture = try LinkFixtureLoader.jsonObject(at: LinkFixtureLoader.root.appendingPathComponent(entry.file))
        let steps = try #require(fixture["steps"] as? [[String: Any]])
        let message = try #require(steps.compactMap { $0["message"] as? [String: Any] }.first { $0["key"] as? String == "catalog" })
        let properties = try #require(message["properties"] as? [[String: Any]])
        let json = try #require(properties.first { $0["name"] as? String == "json" }?["value"] as? String)
        return try #require(JSONSerialization.jsonObject(with: Data(json.utf8)) as? [String: Any])
    }

    @Test func exactProducerMetadataPreservesWordsLimitsAndOrder() throws {
        let controls = try Self.controls(Self.description())
        #expect(controls.count == 5)
        #expect(controls.allSatisfy { $0.metadataIssue == nil && $0.modern?.pendingReason == nil })
        let table = try #require(controls.first { $0.id.hasSuffix(".presets") })
        guard case .filterPresets(let binding)? = table.binding else { Issue.record("Missing Filter Presets binding"); return }
        #expect(binding.columns.map(\.label) == ["#", "Name", "Low (Hz)", "High (Hz)", "Width (Hz)", "Reorder"])
        #expect(binding.column("name")?.maxLength == 32)
        #expect(binding.column("lowHz")?.range == .init(minimum: -10000, maximum: 10000, step: 1))
        #expect(binding.column("highHz")?.range == binding.column("lowHz")?.range)
        #expect(controls[0].options?.map(\.label) == ["LSB", "USB", "DSB", "CWL", "CWU", "FM", "AM", "DIGU", "SPEC", "DIGL", "SAM", "DRM"])
        #expect(controls[3].confirm == "Reset all presets for %1 to the defaults?")
        #expect(controls[4].confirm == "Reset ALL filter presets for ALL modes to the defaults?\n\nThis cannot be undone.")
    }

    @Test(arguments: ["owner", "modeFrom", "range", "precision", "length", "column", "confirm", "modeSet", "modeOwner"])
    func mutatedClosedMetadataCannotWrite(_ mutation: String) throws {
        let controls = try Self.controls(Self.description { controls in
            switch mutation {
            case "owner": controls[1]["id"] = "hardware.filterPresets.presets"
            case "modeFrom": controls[1]["binding"] = ["filterPresets": ["modeFrom": "slice:0"]]
            case "range", "precision", "length", "column":
                var columns = controls[1]["columns"] as! [[String: Any]]
                if mutation == "range" { columns[2]["max"] = 10001 }
                if mutation == "precision" { columns[2]["step"] = 0.1 }
                if mutation == "length" { columns[1]["maxLength"] = 33 }
                if mutation == "column" { columns.swapAt(2, 3) }
                controls[1]["columns"] = columns
            case "confirm": controls[3]["confirm"] = "Reset?"
            case "modeSet":
                var options = controls[0]["options"] as! [[String: Any]]
                options.append(["value": 12, "label": "RADE"]); controls[0]["options"] = options
            default: controls[0]["binding"] = ["property": ["object": "slice:active", "property": "mode"]]
            }
        })
        #expect(controls.contains { $0.metadataIssue != nil || $0.modern?.pendingReason != nil })
    }

    @MainActor final class Phone: SetupPhoneKeys {
        var choice: Int64 = 1
        let subject = PassthroughSubject<Void, Never>()
        var changes: AnyPublisher<Void, Never> { subject.eraseToAnyPublisher() }
        func value(forPhoneKey key: String) -> SetupValue? { key == "filterPresetsMode" ? .integer(choice) : nil }
        func set(_ value: SetupValue, forPhoneKey key: String) -> Bool {
            guard key == "filterPresetsMode", let number = value.whole else { return false }
            choice = number; subject.send(); return true
        }
    }
    @MainActor final class Rig {
        let route = TestRoute()
        let store: MirrorStore
        let settings: SettingsProxyClient
        let feed: SetupDescriptionFeed
        let commands: CommandClient
        let dispatcher: SetupControlDispatcher
        let phone = Phone()
        var rawCatalog: [String: Any] = [:]
        var revision: Int64 = 1
        var editor: FilterPresetsEditor { dispatcher.filterPresets }
        init(clock: any LinkClock = SystemLinkClock()) {
            let route = route
            store = MirrorStore(send: { _ in })
            settings = SettingsProxyClient(send: { _ in }, captureSender: { route.capture() }, clock: clock)
            feed = SetupDescriptionFeed(store: store)
            commands = CommandClient(send: { _ in }, captureSender: { route.capture() })
            dispatcher = SetupControlDispatcher(store: store, feed: feed, settings: settings, commands: commands,
                captureSender: { route.capture() }, selectedSlice: { nil }, selectionChanges: Just(nil).eraseToAnyPublisher(), phone: phone)
        }
        func connect() async throws {
            rawCatalog = try SetupFilterPresetsTests.catalog()
            var bank = rawCatalog["filterPresets"] as! [String: [[String: Any]]]
            // Clearly synthetic, distinguishable values, not DSP defaults.
            bank["USB"] = [["slot": 0, "label": "Test A", "lowHz": 500, "highHz": -500],
                           ["slot": 1, "label": "Test B", "lowHz": -90, "highHz": 210]]
            rawCatalog["filterPresets"] = bank
            route.use(1)
            await event(.stateChanged(.receivingSnapshot))
            let descriptionGeneration = feed.generation
            let values = ["filters/USB/0/name": "Test A", "filters/USB/0/low": "500", "filters/USB/0/high": "-500",
                          "filters/USB/1/name": "Test B", "filters/USB/1/low": "-90", "filters/USB/1/high": "210"]
            for message in [FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                .capabilities(.init(properties: [.init(name: "setupDescriptionVersion", value: .i64(15)), .init(name: "stationCatalogVersion", value: .i64(1))])),
                .schema(.init(className: "SetupDescription", fields: [.init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "dsp", kind: .utf8)])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [.init(name: "revision", value: .i64(1)), .init(name: "dsp", value: .utf8(try SetupFilterPresetsTests.description()))])),
                .objectCreate(.init(key: "catalog", className: "StationCatalog", properties: [.init(name: "json", value: .utf8(try catalogJSON())), .init(name: "revision", value: .i64(1))])),
                .settingsSnapshot(.init(properties: values.map { .init(name: $0.key, value: .utf8($0.value)) })), .snapshotComplete] {
                await event(.message(message))
            }
            await event(.stateChanged(.ready))
            if feed.generation == descriptionGeneration {
                var publication: AnyCancellable?
                await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
                    publication = feed.$generation.first { $0 != descriptionGeneration }.sink { _ in continuation.resume() }
                }
                publication?.cancel()
            }
            // Publication completes the queued parse, including schema/value
            // identities; a malformed fixture fails here rather than at admission.
            _ = try #require(feed.description(for: "dsp"))
            _ = editor
        }
        func event(_ event: StationSession.Event) async { store.handle(event); settings.handle(event); await commands.handle(event) }
        func control(_ suffix: String = "presets") throws -> SetupDescription.Control {
            try #require(feed.description(for: "dsp")?.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == "dsp.filterPresets." + suffix })
        }
        func admit(_ suffix: String = "presets", slot: Int? = 0) throws -> FilterPresetsGesture { try editor.admit(control(suffix), slot: slot).get() }
        func catalogJSON() throws -> String { String(decoding: try JSONSerialization.data(withJSONObject: rawCatalog), as: UTF8.self) }
        func push(slot: Int = 0, name: String? = nil, low: Int? = nil, high: Int? = nil) throws {
            var bank = rawCatalog["filterPresets"] as! [String: [[String: Any]]]
            var rows = bank["USB"]!
            if let name { rows[slot]["label"] = name }; if let low { rows[slot]["lowHz"] = low }; if let high { rows[slot]["highHz"] = high }
            bank["USB"] = rows; rawCatalog["filterPresets"] = bank; revision += 1
            store.handle(.message(.delta(.init(key: "catalog", properties: [.init(name: "json", value: .utf8(try catalogJSON())), .init(name: "revision", value: .i64(revision))]))))
        }
        func answer(_ index: Int, accepted: Bool = true, reason: String = "Core test refusal") throws {
            let message = route.sent[index].message
            switch message {
            case .settingsWrite(let write):
                if accepted { settings.apply(.settingsValue(.init(key: write.key, origin: settings.origin, properties: write.properties))) }
                else { settings.apply(.settingsReject(.init(key: write.key, properties: settings.confirmedValue(write.key).map { [.init(name: write.key, value: .utf8($0))] } ?? [], reason: reason))) }
            case .settingsRemove(let remove):
                if accepted { settings.apply(.settingsValue(.init(key: remove.key, origin: "", properties: []))) }
                else { settings.apply(.settingsReject(.init(key: remove.key, properties: [], reason: reason))) }
            default: Issue.record("Unexpected message")
            }
        }
    }

    @Test(arguments: ["name", "lowHz", "highHz"])
    func completeCapturedRowWritesAndOnlyCatalogueChangesDisplay(_ field: String) async throws {
        let rig = Rig(); try await rig.connect()
        let owner = try rig.admit()
        let before = rig.editor.rows
        #expect(before[0].widthHz == 1000)
        let task = Task { await rig.editor.edit(owner, field: field, value: field == "name" ? .text("  Kept  ") : .integer(-10000)) }
        for index in 0..<3 {
            #expect(await rig.route.waitForCount(index + 1, unless: task))
            #expect(rig.editor.rows == before)
            try rig.answer(index)
        }
        #expect(await task.value == .applied)
        #expect(rig.route.sent.compactMap { if case .settingsWrite(let w) = $0.message { return w.key }; return nil } == ["filters/USB/0/name", "filters/USB/0/low", "filters/USB/0/high"])
        if field == "name" { try rig.push(name: "Kept") }
        else if field == "lowHz" { try rig.push(low: -10000) }
        else { try rig.push(high: -10000) }
        #expect(rig.editor.rows != before)
        #expect(rig.editor.owns(owner))
    }

    @Test(arguments: [SetupValue.integer(-10001), .integer(10001), .decimal(1.5), .text("bad")])
    func invalidIntegerSendsNothing(_ value: SetupValue) async throws {
        let rig = Rig(); try await rig.connect()
        #expect(await rig.editor.edit(try rig.admit(), field: "lowHz", value: value) == .notSent(SetupControlDispatcher.outOfRangeReason))
        #expect(rig.route.sent.isEmpty)
    }

    @Test func nameLengthAndBlankNormalization() async throws {
        let rig = Rig(); try await rig.connect()
        #expect(await rig.editor.edit(try rig.admit(), field: "name", value: .text(String(repeating: "x", count: 33))) == .notSent(SetupControlDispatcher.tooLongReason))
        #expect(rig.route.sent.isEmpty)
        let owner = try rig.admit()
        let task = Task { await rig.editor.edit(owner, field: "name", value: .text("  \n ")) }
        #expect(await rig.route.waitForCount(1, unless: task))
        if case .settingsWrite(let w) = rig.route.sent[0].message { #expect(w.properties[0].value == .utf8("F1")) }
        try rig.answer(0, accepted: false)
        #expect(await task.value == .refused("Core test refusal"))
    }

    @Test(arguments: [0, 1, 2, 3, 4, 5])
    func reorderStopsAtFirstRefusalAndKeepsConfirmedKeys(_ refusalIndex: Int) async throws {
        let rig = Rig(); try await rig.connect()
        let owner = try rig.admit()
        let task = Task { await rig.editor.move(owner, direction: 1) }
        for index in 0...refusalIndex {
            #expect(await rig.route.waitForCount(index + 1, unless: task))
            try rig.answer(index, accepted: index != refusalIndex)
        }
        #expect(await task.value == .refused("Core test refusal"))
        #expect(rig.route.sent.count == refusalIndex + 1)
        if refusalIndex > 0 { #expect(rig.settings.confirmedValue("filters/USB/0/name") == "Test B") }
        #expect(rig.editor.rows[0].name == "Test A") // no optimistic swap
    }

    @Test func reorderMovesWholeNeighborAndRejectsEndpoints() async throws {
        let rig = Rig(); try await rig.connect()
        #expect(await rig.editor.move(try rig.admit(), direction: -1) == .notSent(SetupControlDispatcher.outOfRangeReason))
        let owner = try rig.admit()
        let task = Task { await rig.editor.move(owner, direction: 1) }
        for index in 0..<6 {
            #expect(await rig.route.waitForCount(index + 1, unless: task)); try rig.answer(index)
        }
        #expect(await task.value == .applied)
        let sent = rig.route.sent.compactMap { if case .settingsWrite(let w) = $0.message, case .utf8(let value) = w.properties[0].value { return value }; return nil }
        #expect(sent == ["Test B", "-90", "210", "Test A", "500", "-500"])
    }

    @Test(arguments: ["resetRow", "resetMode", "resetAll"])
    func resetsRemoveOnlyExistingStoreDomainWithCapturedCoreQuestion(_ action: String) async throws {
        let rig = Rig(); try await rig.connect(); rig.editor.selectedSlot = 1
        let owner = try rig.admit(action)
        if action == "resetMode" { #expect(owner.question == "Reset all presets for USB to the defaults?") }
        let task = Task { await rig.editor.reset(owner, confirmed: owner.question) }
        let count = action == "resetRow" ? 3 : action == "resetMode" ? 30 : 360
        for index in 0..<count { #expect(await rig.route.waitForCount(index + 1, unless: task)); try rig.answer(index) }
        #expect(await task.value == .applied)
        let keys = rig.route.sent.compactMap { if case .settingsRemove(let r) = $0.message { return r.key }; return nil }
        #expect(keys.count == count && keys.allSatisfy { $0.hasPrefix("filters/") && !$0.contains("RADE") })
        if action == "resetRow" { #expect(keys == ["filters/USB/1/name", "filters/USB/1/low", "filters/USB/1/high"]) }
        if action == "resetMode" { #expect(keys.last == "filters/USB/9/high") }
    }

    @Test func partialResetKeepsObservedCoreDefaultAndStopsOnRefusal() async throws {
        let rig = Rig(); try await rig.connect(); rig.editor.selectedSlot = 0
        let owner = try rig.admit("resetRow")
        let task = Task { await rig.editor.reset(owner) }
        #expect(await rig.route.waitForCount(1, unless: task)); try rig.answer(0)
        try rig.push(name: "Observed Core default", low: -20, high: 800)
        #expect(await rig.route.waitForCount(2, unless: task)); try rig.answer(1, accepted: false)
        #expect(await task.value == .refused("Core test refusal"))
        #expect(rig.route.sent.count == 2)
        #expect(rig.settings.confirmedValue("filters/USB/0/name") == nil)
        #expect(rig.editor.rows[0].name == "Observed Core default" && rig.editor.rows[0].widthHz == 820)
    }

    @Test func senderStaysCapturedAcrossConstituentKeys() async throws {
        let rig = Rig(); try await rig.connect()
        let owner = try rig.admit()
        let task = Task { await rig.editor.edit(owner, field: "lowHz", value: .integer(10)) }
        #expect(await rig.route.waitForCount(1, unless: task))
        // Transport route changes without a new snapshot cannot cause the
        // second key to adopt the new route by recapturing a current sender.
        rig.route.use(2); try rig.answer(0)
        #expect(await task.value == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.count == 1 && rig.route.sent[0].session == 1)
    }

    @Test(arguments: ["mode", "descriptor", "catalogue", "settings", "selection", "session"])
    func authorityABARevokesUnsentAndResultOwner(_ transition: String) async throws {
        let rig = Rig(); try await rig.connect()
        let owner = try rig.admit()
        switch transition {
        case "mode": _ = rig.phone.set(.integer(0), forPhoneKey: "filterPresetsMode"); _ = rig.phone.set(.integer(1), forPhoneKey: "filterPresetsMode")
        case "descriptor":
            rig.store.handle(.message(.delta(.init(key: "setup", properties: [.init(name: "revision", value: .i64(2)), .init(name: "dsp", value: .utf8(try Self.description()))]))))
            for _ in 0..<5 { await Task.yield() }
        case "catalogue": try rig.push(slot: 1, name: "Other device"); try rig.push(slot: 1, name: "Test B")
        case "settings":
            rig.settings.apply(.settingsValue(.init(key: "filters/USB/1/name", origin: "other", properties: [.init(name: "filters/USB/1/name", value: .utf8("Other"))])))
            rig.settings.apply(.settingsValue(.init(key: "filters/USB/1/name", origin: "other", properties: [.init(name: "filters/USB/1/name", value: .utf8("Test B"))])))
        case "selection": rig.editor.selectedSlot = 1; rig.editor.selectedSlot = 0
        default: await rig.event(.stateChanged(.stopped)); try await rig.connect()
        }
        #expect(!rig.editor.owns(owner))
        #expect(await rig.editor.edit(owner, field: "lowHz", value: .integer(12)) == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.isEmpty)
    }

    @Test func pendingSendRetiredAtHandoffAndFinalResultSessionGuard() async throws {
        let rig = Rig(); try await rig.connect()
        let hold = HandoffHold(); rig.route.holdNext(hold)
        let owner = try rig.admit()
        let task = Task { await rig.editor.edit(owner, field: "lowHz", value: .integer(10)) }
        await hold.waitUntilEntered()
        _ = rig.phone.set(.integer(0), forPhoneKey: "filterPresetsMode")
        _ = rig.phone.set(.integer(1), forPhoneKey: "filterPresetsMode")
        await hold.release()
        #expect(await task.value == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.route.sent.isEmpty)
        let current = try rig.admit()
        let final = Task { await rig.editor.edit(current, field: "lowHz", value: .integer(20)) }
        for index in 0..<3 {
            #expect(await rig.route.waitForCount(index + 1, unless: final)); try rig.answer(index)
            if index == 2 { rig.store.handle(.stateChanged(.stopped)); rig.settings.handle(.stateChanged(.stopped)) }
        }
        #expect(await final.value == .notSent(SetupControlDispatcher.changedFirstReason))
    }

    @Test(arguments: [false, true])
    func timeoutStopsSequenceAndLateKeyNeverClaimsPartialGesture(_ finalKey: Bool) async throws {
        let clock = ManualLinkClock(); let rig = Rig(clock: clock); try await rig.connect()
        let owner = try rig.admit(); var late: [SetupEditOutcome] = []
        let task = Task { await rig.editor.edit(owner, field: "lowHz", value: .integer(10), onLateOutcome: { late.append($0) }) }
        let target = finalKey ? 2 : 0
        for index in 0...target {
            #expect(await rig.route.waitForCount(index + 1, unless: task))
            if index < target { try rig.answer(index) }
        }
        await clock.advance(by: 5000)
        #expect(await task.value == .notSent(SetupControlDispatcher.noAnswerReason))
        #expect(rig.route.sent.count == target + 1)
        try rig.answer(target)
        #expect(late == [finalKey ? .applied : .notSent(SetupControlDispatcher.noAnswerReason)])
        #expect(rig.route.sent.count == target + 1)
    }
}
