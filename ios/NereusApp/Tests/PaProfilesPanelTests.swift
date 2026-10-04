// NereusSDR for iOS: PA editor production dispatch, result ownership, exact cell mapping and native page proof
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

@Suite("PA profile editor", .serialized)
@MainActor
struct PaProfilesPanelTests {
    /// Actual Core resource, projected as SetupDescriptionService does for
    /// v20 rows and v14-19 whole-table gating; lower PA controls are retained.
    static func description(version: Int = 20, keyed: Bool = false, holder: Int? = nil) throws -> String {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../resources/setup/pa.json").standardizedFileURL
        var root = try #require(JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
        // SetupDescriptionService projects PA to 14 for peer maxima 14-19.
        root["version"] = version < 20 ? 14 : 20
        var pages = try #require(root["pages"] as? [[String: Any]])
        var sections = try #require(pages[0]["sections"] as? [[String: Any]])
        for index in sections.indices {
            var controls = try #require(sections[index]["controls"] as? [[String: Any]])
            for c in controls.indices {
                let id = controls[c]["id"] as? String ?? ""
                if version >= 20, id == "pa.gain.table" {
                    var gate = try #require(controls[c]["gate"] as? [String: Any])
                    gate.removeValue(forKey: "offAir")
                    controls[c]["gate"] = gate
                    if keyed {
                        var rows = try #require(controls[c]["rows"] as? [[String: Any]])
                        for band in rows.indices where band != holder {
                            rows[band]["availability"] = ["enabled": false, "reason": band == 5 && holder == nil
                                ? "Only the device that is transmitting can change this." : "Can't change while transmitting."]
                        }
                        controls[c]["rows"] = rows
                    }
                } else if version >= 20, keyed, id.hasPrefix("pa.gain."), id != "pa.gain.bypassPaSettings" {
                    controls[c]["availability"] = ["enabled": false, "reason": "Can't change while transmitting."]
                }
            }
            sections[index]["controls"] = controls
        }
        pages[0]["sections"] = sections
        root["pages"] = pages
        return String(decoding: try JSONSerialization.data(withJSONObject: root), as: UTF8.self)
    }

    static func grid() throws -> SetupDescription.PaProfileGrid {
        let controls = try SetupDescription.parse(json: description()).pages.flatMap(\.sections).flatMap(\.controls)
        guard case .paProfileGrid(let grid)? = controls.first(where: { $0.id == "pa.gain.table" })?.binding else {
            throw SetupRefusal(reason: "The test producer table was not parsed.")
        }
        return grid
    }

    /// Distinct synthetic values exercise all row and drive-step mappings;
    /// none is a hardware gain/default or proposed factory calibration.
    static func body(active: String = "User", gain: Double = 50) throws -> String {
        let rows = try grid().rows
        let bands: [[String: Any]] = rows.map { row in
            ["band": row.label, "gain": gain + Double(row.band) / 10,
             "adjust": (0..<9).map { -10 + Double(row.band) / 2 + Double($0) / 10 },
             "maxPower": Double(row.band) * 10, "useMax": row.band % 2 == 0]
        }
        return String(decoding: try JSONSerialization.data(withJSONObject: ["names": ["Factory", "User", "Other"],
            "active": active, "factory": false, "bands": bands]), as: UTF8.self)
    }

    final class Sent: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: [LinkMessage.CommandInvoke] = []
        var invokes: [LinkMessage.CommandInvoke] { lock.withLock { stored } }
        func record(_ message: LinkMessage) {
            guard case .commandInvoke(let invoke) = message else { return }
            lock.withLock { stored.append(invoke) }
        }
    }

    @MainActor
    final class Rig {
        let sent = Sent()
        let clock = TestLinkClock()
        let mirror: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        let pages: SetupDescribedPages
        let dispatcher: SetupControlDispatcher

        init() {
            let sent = sent
            let clock = clock
            let sender: MirrorStore.BoundSender = { message, permit in
                guard permit.handoff({ sent.record(message); return true }) else { throw LinkSendError.notConnected }
            }
            mirror = MirrorStore(send: { sent.record($0) }, clock: clock)
            settings = SettingsProxyClient(send: { sent.record($0) }, captureSender: { sender }, clock: clock)
            commands = CommandClient(clock: clock, send: { sent.record($0) }, captureSender: { sender })
            feed = SetupDescriptionFeed(store: mirror)
            pages = SetupDescribedPages(feed: feed, store: mirror, hello: Just(nil).eraseToAnyPublisher())
            dispatcher = SetupControlDispatcher(store: mirror, feed: feed, settings: settings, commands: commands,
                                                captureSender: { sender }, selectedSlice: { nil },
                                                selectionChanges: Just(nil).eraseToAnyPublisher(), phone: nil)
        }

        func connect(version: Int = 20, keyed: Bool = false, holder: Int? = nil, permitted: Bool = true) async throws {
            mirror.handle(.stateChanged(.receivingSnapshot))
            settings.handle(.stateChanged(.receivingSnapshot))
            await commands.handle(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [
                .hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")),
                .authResult(.init(accepted: true, reason: "", retryable: false)),
                .capabilities(.init(properties: [.init(name: "setupDescriptionVersion", value: .i64(Int64(version))),
                    .init(name: "paProfileVersion", value: .i64(1)), .init(name: "txPermitted", value: .bool(permitted))])),
                .schema(.init(className: "SetupDescription", fields: [.init(ordinal: 0, name: "revision", kind: .i64),
                    .init(ordinal: 1, name: "pa", kind: .utf8)])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "pa", value: .utf8(try PaProfilesPanelTests.description(version: version, keyed: keyed, holder: holder)))])),
                .objectCreate(.init(key: "paProfiles", className: "PaProfilesFacade", properties: [
                    .init(name: "json", value: .utf8(try PaProfilesPanelTests.body())), .init(name: "revision", value: .i64(1))])),
                .objectCreate(.init(key: "txState", className: "TransmitState", properties: [
                    .init(name: "keyed", value: .bool(keyed)), .init(name: "tuning", value: .bool(false)),
                    .init(name: "twoTone", value: .bool(false)), .init(name: "txEnding", value: .bool(false))])),
                .settingsSnapshot(.init(properties: [])), .snapshotComplete,
            ]
            for message in messages { mirror.apply(message); settings.apply(message); await commands.receive(message) }
            mirror.handle(.stateChanged(.ready)); settings.handle(.stateChanged(.ready))
            await commands.handle(.stateChanged(.ready))
            await MainQueue.drained()
            try #require(await ShotWait.until {
                self.feed.description(for: "pa") != nil
                    && self.pages.page("pa.gain", in: "pa") != nil
                    && self.pages.categories["pa"] == self.feed.description(for: "pa")
            }, "The current PA description must reach the page model before controls are used")
            pages.refresh()
            try #require(pages.isCurrent)
        }

        func control(_ suffix: String) throws -> SetupDescription.Control {
            try #require(pages.page("pa.gain", in: "pa")?.sections.flatMap(\.controls).first { $0.id == "pa.gain." + suffix })
        }
        func owner(_ suffix: String = "table", band: Int? = nil, column: String? = nil) throws -> PaEditorInteraction {
            PaEditorInteraction(dispatcher: dispatcher, control: try control(suffix), category: "pa", band: band, column: column)
        }
        func invoke(_ index: Int) async throws -> LinkMessage.CommandInvoke {
            try #require(await ShotWait.until { self.sent.invokes.count > index }, "The production PA control did not send")
            return sent.invokes[index]
        }
        func answer(_ invoke: LinkMessage.CommandInvoke, accepted: Bool = true, reason: String = "") async {
            await commands.receive(.commandResult(.init(verb: invoke.verb, id: invoke.id, accepted: accepted,
                                                       reason: reason, affected: [], values: [])))
        }
        func push(active: String = "User", gain: Double = 50, revision: Int64 = 2) throws {
            mirror.apply(.delta(.init(key: "paProfiles", properties: [
                .init(name: "json", value: .utf8(try PaProfilesPanelTests.body(active: active, gain: gain))),
                .init(name: "revision", value: .i64(revision))])))
        }
    }

#if DEBUG
    /// A causal scheduling barrier, with no time-based ordering. Arrival
    /// means the real CommandClient has answered and performPa has returned;
    /// only the App's result-presentation continuation is held.
    @MainActor
    private final class PresentationBarrier {
        private var outcome: SetupEditOutcome?
        private var arrival: CheckedContinuation<SetupEditOutcome, Never>?
        private var resume: CheckedContinuation<Void, Never>?

        func suspend(_ outcome: SetupEditOutcome) async {
            self.outcome = outcome
            await withCheckedContinuation { continuation in
                resume = continuation
                arrival?.resume(returning: outcome)
                arrival = nil
            }
        }

        func waitForArrival() async -> SetupEditOutcome {
            if let outcome, resume != nil { return outcome }
            return await withCheckedContinuation { arrival = $0 }
        }

        func release() {
            resume?.resume()
            resume = nil
        }
    }

    private func openGainPad(_ rig: Rig, host: ValuePadHost) throws -> PaEditorInteraction {
        let owner = try rig.owner(band: 5, column: "gain")
        let table = try rig.control("table")
        let panel = rig.dispatcher.paPanel(table, in: "pa")
        let grid = try #require(panel.grid)
        let value = try #require(panel.profiles?.bands[5].gain)
        owner.openPad(host: host, column: try #require(grid.column("gain")), row: grid.rows[5], value: value,
                      readCurrent: { rig.dispatcher.paPanel(table, in: "pa").profiles?.bands[5].gain })
        return owner
    }

    /// The echo changes exactly the admitted cell, keeping every other
    /// fixture value intact, as the Core's accepted setGain does.
    private func echoGain(_ rig: Rig) throws {
        var body = try #require(JSONSerialization.jsonObject(with: Data(Self.body().utf8)) as? [String: Any])
        var bands = try #require(body["bands"] as? [[String: Any]])
        bands[5]["gain"] = 60.0
        body["bands"] = bands
        let json = String(decoding: try JSONSerialization.data(withJSONObject: body), as: UTF8.self)
        rig.mirror.apply(.delta(.init(key: "paProfiles", properties: [
            .init(name: "json", value: .utf8(json)), .init(name: "revision", value: .i64(2))])))
    }

#endif

    @Test("the live App hook claims exactly the six PA controls")
    func liveAppDispatchClaimsOnlyPaControls() throws {
        let suite = "PaProfilesPanelTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let app = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                           displaySettings: BandDisplaySettingsStore(defaults: defaults),
                           meterSettings: SMeterSettingsStore(defaults: defaults))
        let live = SetupSpecializedPanels.live(app)
        let controls = try SetupDescription.parse(json: Self.description()).pages.flatMap(\.sections).flatMap(\.controls)
        for control in controls {
            #expect((live.makePa?(control, "pa") != nil) == SetupSpecializedPanels.isPaProfile(control))
        }
        #expect(SetupSpecializedPanels.none.makePa == nil)
    }

    @Test("all 14 rows and 12 columns use production gesture ownership and exact typed commands")
    func allBandCellsSendExactTypedArguments() async throws {
        let rig = Rig()
        try await rig.connect()
        let control = try rig.control("table")
        let panel = rig.dispatcher.paPanel(control, in: "pa")
        let grid = try #require(panel.grid)
        let profiles = try #require(panel.profiles)
        #expect(grid.rows.map(\.label) == ["160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "GEN", "WWV", "XVTR"])
        #expect(grid.columns.map(\.label) == ["Gain (dB)", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "Max W", "Use Max"])
        var index = 0
        for row in grid.rows {
            for column in grid.columns {
                let owner = try rig.owner(band: row.band, column: column.id)
                #expect(owner.begin())
                let admission = try #require(owner.admission)
                let original = try #require(profiles.bands[row.band].value(for: column))
                let edit = Task { await owner.perform(admission, value: original) }
                let invoke = try await rig.invoke(index)
                let expectedVerb = column.field == "gain" ? "setGain" : column.field == "adjust" ? "setAdjust"
                    : column.field == "maxPower" ? "setMaxPower" : "setUseMax"
                #expect(invoke.verb == "paProfile." + expectedVerb)
                #expect(invoke.args.first { $0.name == "band" }?.value == .i64(Int64(row.band)))
                if column.field == "useMax" {
                    #expect(invoke.args.count == 2 && invoke.args.first { $0.name == "on" }?.value == .bool(profiles.bands[row.band].useMax))
                } else {
                    #expect(invoke.args.first { $0.name == "value" }?.value == .f64(try #require(original.number)))
                    #expect(invoke.args.count == (column.driveStep == nil ? 2 : 3))
                    if let step = column.driveStep { #expect(invoke.args.first { $0.name == "step" }?.value == .i64(Int64(step))) }
                }
                await rig.answer(invoke)
                #expect(await edit.value?.accepted == true)
                #expect(owner.problem == nil && rig.dispatcher.paPanel(control, in: "pa").profiles == profiles)
                owner.retire()
                index += 1
            }
        }
        #expect(index == 168 && rig.sent.invokes.count == 168)
    }

    @Test("all lifecycle gestures retain their Core prompt or question", arguments: ["profile", "new", "copy", "delete", "reset"])
    func lifecycleUsesCapturedCorePromptAndQuestion(action: String) async throws {
        let rig = Rig()
        try await rig.connect()
        let owner = try rig.owner(action)
        if action == "new" || action == "copy" {
            #expect(owner.beginPrompt())
            #expect(owner.prompt?.title == (action == "new" ? "New PA Profile" : "Copy PA Profile"))
            #expect(owner.prompt?.label == (action == "new" ? "Profile name:" : "New profile name:"))
            #expect(owner.name == (action == "new" ? "" : "User (copy)"))
        } else if action == "profile" { #expect(owner.begin()) }
        else {
            #expect(owner.beginQuestion())
            #expect(owner.question?.text == (action == "delete" ? "Delete profile \"User\"?" : "Reset the active profile to factory defaults?"))
        }
        let admission = try #require(owner.admission)
        let prompt = owner.prompt
        let question = owner.question
        let value: SetupValue? = action == "profile" ? .text("Other") : prompt == nil ? nil : .text(" New user profile ")
        let edit = Task { await owner.perform(admission, value: value, prompt: prompt, question: question) }
        let invoke = try await rig.invoke(0)
        #expect(invoke.verb == "paProfile." + (action == "profile" ? "select" : action))
        if action == "reset" { #expect(invoke.args.isEmpty) }
        else { #expect(invoke.args.count == 1 && invoke.args[0].name == "name") }
        if prompt != nil { #expect(invoke.args[0].value == .utf8(" New user profile ")) }
        await rig.answer(invoke, accepted: false, reason: "Core lifecycle refusal.")
        #expect(await edit.value?.reason == "Core lifecycle refusal." && owner.problem == "Core lifecycle refusal.")
        #expect(rig.dispatcher.paPanel(try rig.control("table"), in: "pa").profiles?.active == "User")
    }

    @Test("profile changes cannot adopt the old naming prompt or manufacture a new admission")
    func stalePromptSendsNothing() async throws {
        let rig = Rig()
        try await rig.connect()
        let owner = try rig.owner("copy")
        #expect(owner.beginPrompt())
        let admission = try #require(owner.admission)
        let prompt = owner.prompt
        try rig.push(active: "Other")
        #expect(await owner.perform(admission, value: .text("New"), prompt: prompt) == nil)
        #expect(owner.problem == nil && rig.sent.invokes.isEmpty && admission.isRevoked)
    }

    @Test("immediate replies never update abandoned, replacement or Core-stale controls", arguments: ["abandon", "replace", "profile"])
    func immediateOutcomeChecksOwnership(change: String) async throws {
        let rig = Rig()
        try await rig.connect()
        let owner = try rig.owner(band: 5, column: "gain")
        #expect(owner.begin())
        let captured = try #require(owner.admission)
        let edit = Task { await owner.perform(captured, value: .decimal(60)) }
        let invoke = try await rig.invoke(0)
        if change == "abandon" { owner.retire() }
        else if change == "replace" { #expect(owner.begin()) }
        else { try rig.push(active: "Other") }
        await rig.answer(invoke, accepted: false, reason: "Obsolete immediate refusal.")
        #expect(await edit.value == nil && owner.problem == nil)
        #expect(!rig.dispatcher.paOwnsOutcome(captured))
    }

    @Test("stale queued work cannot poison the current edit's result owner")
    func retiredQueuedWorkCannotPoisonCurrentOutcome() async throws {
        let rig = Rig()
        try await rig.connect()
        let owner = try rig.owner(band: 5, column: "gain")
        #expect(owner.begin())
        let retired = try #require(owner.admission)
        #expect(owner.begin())
        let current = try #require(owner.admission)
        let edit = Task { await owner.perform(current, value: .decimal(60)) }
        let invoke = try await rig.invoke(0)
        #expect(await owner.perform(retired, value: .decimal(70)) == nil)
        await rig.answer(invoke, accepted: false, reason: "Current gesture refusal.")
        #expect(await edit.value?.reason == "Current gesture refusal.")
        #expect(owner.problem == "Current gesture refusal." && rig.sent.invokes.count == 1)
    }

    @Test("retained interaction retires its old descriptor and admits the new holder row")
    func retainedInteractionRebindsCurrentDescriptor() async throws {
        let rig = Rig()
        try await rig.connect()
        let owner = try rig.owner(band: 5, column: "gain")
        #expect(owner.begin())
        let retired = try #require(owner.admission)
        rig.mirror.apply(.delta(.init(key: "txState", properties: [.init(name: "keyed", value: .bool(true))])))
        rig.mirror.apply(.delta(.init(key: "setup", properties: [
            .init(ordinal: 0, name: "revision", value: .i64(2)),
            .init(ordinal: 1, name: "pa", value: .utf8(try Self.description(keyed: true, holder: 5))),
        ])))
        try #require(await ShotWait.until {
            guard rig.feed.revision == 2, let description = rig.feed.description(for: "pa"),
                  let table = description.pages.flatMap(\.sections).flatMap(\.controls)
                    .first(where: { $0.id == "pa.gain.table" }) else { return false }
            return table != owner.control && rig.pages.categories["pa"] == description
        })
        rig.pages.refresh()
        owner.rebind(try rig.control("table"))
        #expect(retired.isRevoked && owner.admission == nil && owner.begin())
        let current = try #require(owner.admission)
        #expect(current !== retired && current.control == owner.control)
        let edit = Task { await owner.perform(current, value: .decimal(60)) }
        let invoke = try await rig.invoke(0)
        await rig.answer(invoke)
        #expect(await edit.value?.accepted == true && owner.problem == nil)
        #expect(await owner.perform(retired, value: .decimal(70)) == nil)
        #expect(rig.sent.invokes.count == 1)
    }

    @Test("missing or malformed New cannot hide Copy, Delete or Reset Defaults", arguments: [false, true])
    func lifecycleWithoutValidAnchorRemainsVisible(malformed: Bool) async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let rig = Rig()
        try await rig.connect()
        var root = try #require(JSONSerialization.jsonObject(with: Data(Self.description().utf8)) as? [String: Any])
        var pages = try #require(root["pages"] as? [[String: Any]])
        var sections = try #require(pages[0]["sections"] as? [[String: Any]])
        var controls = try #require(sections[0]["controls"] as? [[String: Any]])
        if malformed { controls[1].removeValue(forKey: "prompt") }
        else { controls.remove(at: 1) }
        sections[0]["controls"] = controls
        pages[0]["sections"] = sections
        root["pages"] = pages
        let json = String(decoding: try JSONSerialization.data(withJSONObject: root), as: UTF8.self)
        rig.mirror.apply(.delta(.init(key: "setup", properties: [
            .init(ordinal: 0, name: "revision", value: .i64(2)), .init(ordinal: 1, name: "pa", value: .utf8(json)),
        ])))
        try #require(await ShotWait.until {
            guard rig.feed.revision == 2, let description = rig.feed.description(for: "pa") else { return false }
            let anchor = description.pages.flatMap(\.sections).flatMap(\.controls)
                .first { $0.id == "pa.gain.new" }
            let intendedAnchor = malformed ? anchor?.metadataIssue != nil : anchor == nil
            return intendedAnchor && anchor?.binding == nil && rig.pages.categories["pa"] == description
        })
        rig.pages.refresh()
        let current = try #require(rig.pages.page("pa.gain", in: "pa"))
        #expect(!PaProfilesPanel.groupsLifecycle(current.sections.flatMap(\.controls)))
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: 402, height: 874)
        window.windowLevel = .alert + 1
        var specialized = SetupSpecializedPanels { _, _, _ in nil }
        specialized.makePa = { control, category in
            AnyView(PaProfilesPanel(control: control, category: category, pages: rig.pages, dispatcher: rig.dispatcher))
        }
        let host = UIHostingController(rootView: NavigationStack {
            DescribedPage(pages: rig.pages, dispatcher: rig.dispatcher, category: "pa", pageId: "pa.gain", specialized: specialized)
        })
        window.rootViewController = host
        window.makeKeyAndVisible()
        defer { window.isHidden = true; window.rootViewController = nil }
        await ShotWait.laidOut(window)
        for suffix in ["copy", "delete", "reset"] {
            try #require(SetupTypedEntryTests.element("pa.gain." + suffix, in: window))
        }
        #expect(rig.sent.invokes.isEmpty)
    }

    @Test("late replies need the same active presentation owner", arguments: [false, true])
    func lateOutcomeChecksOwnership(replace: Bool) async throws {
        let rig = Rig()
        try await rig.connect()
        let owner = try rig.owner(band: 5, column: "gain")
        #expect(owner.begin())
        let captured = try #require(owner.admission)
        let edit = Task { await owner.perform(captured, value: .decimal(60)) }
        let invoke = try await rig.invoke(0)
        await rig.clock.advance(by: 5_000)
        #expect(await edit.value?.reason == SetupControlDispatcher.noAnswerReason)
        if replace { #expect(owner.begin()) }
        await rig.answer(invoke, accepted: false, reason: "Actual late Core refusal.")
        await MainQueue.drained()
        #expect(owner.problem == (replace ? nil : "Actual late Core refusal."))
        #expect(rig.sent.invokes.count == 1)
    }

    @Test("pad opening captures ownership; cancel/replacement revokes without a write", arguments: [false, true])
    func padAdmissionAndAbandonment(replace: Bool) async throws {
        let rig = Rig()
        try await rig.connect()
        let owner = try rig.owner(band: 5, column: "adjust9")
        let grid = try Self.grid()
        let host = ValuePadHost()
        owner.openPad(host: host, column: try #require(grid.column("adjust9")), row: grid.rows[5], value: -6.7,
                      readCurrent: { -6.7 })
        let original = try #require(owner.admission)
        let pad = try #require(host.pad)
        #expect(rig.dispatcher.paOwnsOutcome(original))
        #expect(pad.title == "20m 90%" && pad.decimals == 1 && pad.numberRange == -10...10 && pad.stepText == "0.1")
        if replace {
            host.open { close in ValuePadModel(title: "Replacement", unit: "", range: 0...10, current: 1, close: close) }
        } else { host.close() }
        #expect(original.isRevoked && owner.admission == nil)
        #expect(await pad.enter() == false && rig.sent.invokes.isEmpty)
    }

    @Test("typed pad uses the admitted band and column, preserves Core values and shows actual refusal")
    func typedPadSendsAndShowsRefusal() async throws {
        let rig = Rig()
        try await rig.connect()
        let owner = try rig.owner(band: 13, column: "adjust9")
        let grid = try Self.grid()
        let host = ValuePadHost()
        let table = try rig.control("table")
        owner.openPad(host: host, column: try #require(grid.column("adjust9")), row: grid.rows[13], value: -2.7,
                      readCurrent: { rig.dispatcher.paPanel(table, in: "pa").profiles?.bands[13].adjust[8] })
        let pad = try #require(host.pad)
        SetupTypedEntryTests.type("-1.5", on: pad)
        let edit = Task { await pad.enter() }
        let invoke = try await rig.invoke(0)
        #expect(invoke.verb == "paProfile.setAdjust")
        #expect(invoke.args.first { $0.name == "band" }?.value == .i64(13))
        #expect(invoke.args.first { $0.name == "step" }?.value == .i64(8))
        #expect(invoke.args.first { $0.name == "value" }?.value == .f64(-1.5))
        await rig.answer(invoke, accepted: false, reason: "Actual PA refusal.")
        #expect(await edit.value == false)
        #expect(pad.refusal == "Actual PA refusal." && owner.problem == "Actual PA refusal.")
        let currentTable = try rig.control("table")
        #expect(host.pad === pad && rig.dispatcher.paPanel(currentTable, in: "pa").profiles?.bands[13].adjust[8] == -2.7)
        host.close()
    }

#if DEBUG
    @Test("a real accepted result followed by its cell echo retires the exact pad before presentation")
    func acknowledgedPaEchoRetiresExactPadBeforePresentation() async throws {
        let rig = Rig()
        try await rig.connect()
        let host = ValuePadHost()
        defer { host.close() }
        let owner = try openGainPad(rig, host: host)
        let pad = try #require(host.pad)
        let captured = try #require(owner.admission)
        let barrier = PresentationBarrier()
        owner.beforeOutcomePresentationForTesting = { await barrier.suspend($0) }
        defer { barrier.release() }
        SetupTypedEntryTests.type("60", on: pad)
        let edit = Task { await pad.enter() }
        let invoke = try await rig.invoke(0)
        #expect(invoke.verb == "paProfile.setGain")
        #expect(invoke.args.first { $0.name == "band" }?.value == .i64(5))
        #expect(invoke.args.first { $0.name == "value" }?.value == .f64(60))
        await rig.answer(invoke)
        #expect(await barrier.waitForArrival() == .applied)
        #expect(await rig.commands.waitingCount == 0)
        #expect(host.pad === pad && rig.dispatcher.paOwnsOutcome(captured))
        try echoGain(rig)
        // Assert the visible editor has gone BEFORE releasing the result.
        #expect(host.pad == nil && owner.admission == nil)
        #expect(pad.outcomeIdentity == nil && !pad.canEnter)
        #expect(captured.isRevoked && !rig.dispatcher.paOwnsOutcome(captured))
        let table = try rig.control("table")
        #expect(rig.dispatcher.paPanel(table, in: "pa").profiles?.bands[5].gain == 60)
        barrier.release()
        #expect(await edit.value == false)
        #expect(owner.problem == nil && pad.refusal == nil && host.pad == nil)
        #expect(rig.sent.invokes.count == 1)
    }

    @Test("an echo retires the timed-out pad while its real late acceptance remains fenced")
    func timedOutPaPadClosesOnEchoBeforeLateAcceptance() async throws {
        let rig = Rig()
        try await rig.connect()
        let host = ValuePadHost()
        defer { host.close() }
        let owner = try openGainPad(rig, host: host)
        let pad = try #require(host.pad)
        let captured = try #require(owner.admission)
        SetupTypedEntryTests.type("60", on: pad)
        let edit = Task { await pad.enter() }
        let invoke = try await rig.invoke(0)
        await rig.clock.advance(by: 5_000)
        #expect(await edit.value == false)
        #expect(host.pad === pad && pad.canEnter && rig.dispatcher.paOwnsOutcome(captured))
        #expect(pad.refusal == SetupControlDispatcher.noAnswerReason)
        try echoGain(rig)
        #expect(host.pad == nil && pad.outcomeIdentity == nil && !pad.canEnter)
        #expect(captured.isRevoked && !rig.dispatcher.paOwnsOutcome(captured))
        let replacementOwner = try openGainPad(rig, host: host)
        let replacement = try #require(host.pad)
        let replacementAdmission = try #require(replacementOwner.admission)
        await rig.answer(invoke)
        #expect(await rig.commands.waitingCount == 0)
        await MainQueue.drained()
        #expect(host.pad === replacement && replacement.canEnter && replacement.number == 60)
        #expect(replacement.refusal == nil && replacementOwner.problem == nil)
        #expect(rig.dispatcher.paOwnsOutcome(replacementAdmission) && rig.sent.invokes.count == 1)
    }

    @Test("a still-current pad closes on acceptance and keeps the Core's refusal", arguments: [false, true])
    func unchangedPaPadResultKeepsCurrentPresentation(accepted: Bool) async throws {
        let rig = Rig()
        try await rig.connect()
        let host = ValuePadHost()
        defer { host.close() }
        let owner = try openGainPad(rig, host: host)
        let pad = try #require(host.pad)
        let captured = try #require(owner.admission)
        let barrier = PresentationBarrier()
        owner.beforeOutcomePresentationForTesting = { await barrier.suspend($0) }
        defer { barrier.release() }
        SetupTypedEntryTests.type("60", on: pad)
        let edit = Task { await pad.enter() }
        let invoke = try await rig.invoke(0)
        await rig.answer(invoke, accepted: accepted, reason: accepted ? "" : "Actual current PA refusal.")
        #expect(await barrier.waitForArrival() == (accepted ? .applied : .refused("Actual current PA refusal.")))
        #expect(host.pad === pad && rig.dispatcher.paOwnsOutcome(captured))
        barrier.release()
        #expect(await edit.value == accepted)
        if accepted {
            #expect(host.pad == nil && pad.outcomeIdentity == nil && owner.problem == nil)
        } else {
            #expect(host.pad === pad && pad.canEnter && rig.dispatcher.paOwnsOutcome(captured))
            #expect(pad.refusal == "Actual current PA refusal." && owner.problem == "Actual current PA refusal.")
        }
        let table = try rig.control("table")
        #expect(rig.dispatcher.paPanel(table, in: "pa").profiles?.bands[5].gain == 50.5)
        #expect(rig.sent.invokes.count == 1)
    }

    @Test("restored authority cannot reopen an invalidated pad or present its old acceptance",
          arguments: ["profile", "capability", "transmit", "session", "descriptor"])
    func invalidatedPaPadDoesNotReviveAfterAuthorityTransitions(change: String) async throws {
        let rig = Rig()
        try await rig.connect()
        let host = ValuePadHost()
        defer { host.close() }
        let owner = try openGainPad(rig, host: host)
        let pad = try #require(host.pad)
        let captured = try #require(owner.admission)
        let barrier = PresentationBarrier()
        owner.beforeOutcomePresentationForTesting = { await barrier.suspend($0) }
        defer { barrier.release() }
        SetupTypedEntryTests.type("60", on: pad)
        let edit = Task { await pad.enter() }
        await rig.answer(try await rig.invoke(0))
        #expect(await barrier.waitForArrival() == .applied)
        switch change {
        case "profile":
            try rig.push(active: "Other", revision: 2)
            try rig.push(active: "User", revision: 3)
        case "capability":
            for version in [Int64(0), Int64(1)] {
                rig.mirror.apply(.capabilities(.init(properties: [
                    .init(name: "setupDescriptionVersion", value: .i64(20)),
                    .init(name: "paProfileVersion", value: .i64(version)),
                    .init(name: "txPermitted", value: .bool(true))])))
            }
        case "transmit":
            for keyed in [true, false] {
                rig.mirror.apply(.delta(.init(key: "txState", properties: [
                    .init(name: "keyed", value: .bool(keyed))])))
            }
        case "session":
            let lost = StationSession.Event.stateChanged(.waitingToRetry(seconds: 1))
            rig.mirror.handle(lost)
            rig.settings.handle(lost)
            await rig.commands.handle(lost)
            try await rig.connect()
        case "descriptor":
            for revision in [Int64(2), Int64(3)] {
                rig.mirror.apply(.delta(.init(key: "setup", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(revision)),
                    .init(ordinal: 1, name: "pa", value: .utf8(try Self.description(keyed: revision == 2, holder: 5)))])))
                await MainQueue.drained()
            }
            try #require(await ShotWait.until {
                self.descriptionIsRestored(rig)
            }, "The restored PA descriptor must reach the page model")
        default: Issue.record("Unknown authority transition")
        }
        #expect(host.pad == nil && owner.admission == nil)
        #expect(captured.isRevoked && !rig.dispatcher.paOwnsOutcome(captured))
        #expect(pad.outcomeIdentity == nil && !pad.canEnter)
        barrier.release()
        #expect(await edit.value == false)
        #expect(host.pad == nil && owner.problem == nil && pad.refusal == nil)
        #expect(rig.sent.invokes.count == 1)
    }

    private func descriptionIsRestored(_ rig: Rig) -> Bool {
        guard rig.feed.revision == 3, let current = rig.feed.description(for: "pa"),
              current == (try? SetupDescription.parse(json: Self.description())) else { return false }
        return rig.pages.categories["pa"] == current && rig.pages.isCurrent
    }

    @Test("an old result and its echo cannot close a moved pad or a fresh PA edit", arguments: [false, true])
    func replacementPadSurvivesRetiredPaResultAndEcho(freshPa: Bool) async throws {
        let rig = Rig()
        try await rig.connect()
        let host = ValuePadHost()
        defer { host.close() }
        let owner = try openGainPad(rig, host: host)
        let pad = try #require(host.pad)
        let captured = try #require(owner.admission)
        let barrier = PresentationBarrier()
        owner.beforeOutcomePresentationForTesting = { await barrier.suspend($0) }
        defer { barrier.release() }
        SetupTypedEntryTests.type("60", on: pad)
        let edit = Task { await pad.enter() }
        await rig.answer(try await rig.invoke(0))
        #expect(await barrier.waitForArrival() == .applied)
        var replacementOwner: PaEditorInteraction?
        if freshPa {
            try echoGain(rig)
            replacementOwner = try openGainPad(rig, host: host)
        } else {
            host.open { close in ValuePadModel(title: "Replacement", unit: "", range: 0...10, current: 7, close: close) }
            try echoGain(rig)
        }
        let replacement = try #require(host.pad)
        #expect(replacement.id != pad.id && replacement.canEnter)
        #expect(replacement.number == (freshPa ? 60 : 7))
        if let replacementOwner {
            #expect(rig.dispatcher.paOwnsOutcome(try #require(replacementOwner.admission)))
        }
        #expect(captured.isRevoked && !rig.dispatcher.paOwnsOutcome(captured))
        barrier.release()
        #expect(await edit.value == false)
        #expect(host.pad === replacement && replacement.canEnter && replacement.refusal == nil)
        #expect(owner.problem == nil && rig.sent.invokes.count == 1)
        // Keep the fresh PA interaction alive through the stale continuation.
        #expect(replacementOwner?.problem == nil)
    }

#endif

    @Test("Core row reasons, legacy gate and receive-only authority remain visible", arguments: [14, 19, 20])
    func availabilityAndReceiveOnlyFollowCore(version: Int) async throws {
        let rig = Rig()
        try await rig.connect(version: version, keyed: true, holder: version == 20 ? 5 : nil, permitted: false)
        let table = try rig.control("table")
        let panel = rig.dispatcher.paPanel(table, in: "pa")
        #expect(panel.reason(for: 5) == (version == 20 ? nil : SetupControlDispatcher.onAirReason))
        #expect(panel.reason(for: 4) == (version == 20 ? "Can't change while transmitting." : SetupControlDispatcher.onAirReason))
        #expect(PaProfilesPanel.lifecycleReason(try rig.control("new"), table: table, category: "pa", dispatcher: rig.dispatcher)
            == (version == 20 ? "Can't change while transmitting." : SetupControlDispatcher.onAirReason))
        let receiveOnly = Rig()
        try await receiveOnly.connect(version: version, permitted: false)
        #expect(PaProfilesPanel.lifecycleReason(try receiveOnly.control("new"), table: try receiveOnly.control("table"),
                                              category: "pa", dispatcher: receiveOnly.dispatcher) == nil)
        #expect(try receiveOnly.owner(band: 5, column: "gain").begin())
    }

    @Test("production PA page routing, native typed-entry action, all lower controls and approved screenshots")
    func nativePageDispatchAndApprovedShots() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let rig = Rig()
        try await rig.connect()
        let page = try #require(rig.pages.page("pa.gain", in: "pa"))
        let all = page.sections.flatMap(\.controls)
        #expect(all.filter(SetupSpecializedPanels.isPaProfile).count == 6)
        #expect(all.contains { $0.id == "pa.gain.bypassPaSettings" })
        #expect(all.filter(SetupSpecializedPanels.isPaProfile).allSatisfy { DescribedControl.row(for: $0) == .panel })
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let configurations: [(String, CGFloat, CGFloat, Bool, Int?, DynamicTypeSize, ColorScheme)] = [
            ("phone-overview", 402, 874, false, nil, .large, .dark),
            ("phone-expanded", 402, 874, false, 5, .large, .dark),
            ("ipad-landscape", 1366, 1024, true, nil, .large, .dark),
            ("ipad-portrait", 1024, 1366, true, nil, .large, .dark),
            ("ipad-large-type", 1024, 1366, true, nil, .accessibility2, .dark),
            ("phone-overview-light", 402, 874, false, nil, .large, .light),
            ("phone-expanded-light", 402, 874, false, 5, .large, .light),
        ]
        for (name, width, height, tablet, expanded, type, scheme) in configurations {
            let hostPads = ValuePadHost()
            var specialized = SetupSpecializedPanels { _, _, _ in nil }
            specialized.makePa = { control, category in
                AnyView(PaProfilesPanel(control: control, category: category, pages: rig.pages, dispatcher: rig.dispatcher,
                                        tablet: tablet, initiallyExpandedBand: expanded))
            }
            let window = UIWindow(windowScene: scene)
            window.frame = CGRect(x: 0, y: 0, width: width, height: height)
            window.windowLevel = .alert + 1
            let root = NavigationStack {
                DescribedPage(pages: rig.pages, dispatcher: rig.dispatcher, category: "pa", pageId: "pa.gain", specialized: specialized)
            }.valuePads(hostPads).environment(\.dynamicTypeSize, type).preferredColorScheme(scheme)
            let host = UIHostingController(rootView: root)
            host.view.frame = window.bounds
            window.rootViewController = host
            window.makeKeyAndVisible()
            defer { hostPads.close(); window.isHidden = true; window.rootViewController = nil }
            await ShotWait.laidOut(window)
            try #require(SetupTypedEntryTests.element("pa.gain.profile", in: window))
            try #require(SetupTypedEntryTests.element("pa.gain.new", in: window))
            if expanded != nil {
                try #require(await reveal("pa.gain.table.band5.gain", in: window, root: host.view),
                             "The expanded band's gain must be reachable in the page's actual scroll viewport")
                let row = try #require(SetupTypedEntryTests.element("pa.gain.table.band5.gain", in: window))
                #expect(SetupTypedEntryTests.actions(row).contains(SetupNumberRow.enterValueAction))
                #expect(SetupTypedEntryTests.perform(SetupNumberRow.enterValueAction, on: row))
                let pad = try #require(hostPads.pad)
                #expect(pad.title == "20m Gain (dB)" && pad.numberRange == 38.8...100 && pad.decimals == 1)
                hostPads.close()
                await ShotWait.laidOut(window)
            }
            if tablet {
                try #require(SetupTypedEntryTests.element("pa.gain.table.grid", in: window))
                if width == 1024 {
                    #expect(horizontalScroll(in: host.view) != nil, "The complete grid must scroll horizontally when narrower than its columns")
                }
            }
            try save("pa-" + name, window: window)
            if let list = SetupDescribedPagesTests.firstScrollView(in: host.view) {
                list.contentOffset.y = max(-list.adjustedContentInset.top,
                    list.contentSize.height + list.adjustedContentInset.bottom - list.bounds.height)
                await ShotWait.laidOut(window)
                try #require(SetupTypedEntryTests.element("pa.gain.bypassPaSettings", in: window))
                try save("pa-" + name + "-lower", window: window)
            }
        }
        #expect(rig.sent.invokes.isEmpty, "Native opening and cancellation send no PA command")
    }

    /// Bring the exact typed-entry control into the real List viewport.
    /// Stop at finite scroll progress or the existing 30-second limit.
    private func reveal(_ id: String, in window: UIWindow, root: UIView) async -> Bool {
        let list = SetupDescribedPagesTests.firstScrollView(in: root)
        let deadline = Date().addingTimeInterval(30)
        var previousOffset: CGFloat?
        for _ in 0..<32 {
            await ShotWait.laidOut(window)
            if let element = SetupTypedEntryTests.element(id, in: window) {
                let viewport = UIAccessibility.convertToScreenCoordinates(window.bounds, in: window)
                if !element.accessibilityFrame.isEmpty, viewport.intersects(element.accessibilityFrame) { return true }
            }
            guard Date() < deadline, let list else { return false }
            if let previousOffset, list.contentOffset.y <= previousOffset + 1 { return false }
            let offset = list.contentOffset.y
            let end = max(-list.adjustedContentInset.top,
                          list.contentSize.height + list.adjustedContentInset.bottom - list.bounds.height)
            let next = min(end, offset + max(44, list.bounds.height / 2))
            guard next > offset + 1 else { return false }
            previousOffset = offset
            list.contentOffset.y = next
        }
        return false
    }

    private func horizontalScroll(in view: UIView) -> UIScrollView? {
        if let scroll = view as? UIScrollView, scroll.contentSize.width > scroll.bounds.width + 1 { return scroll }
        for child in view.subviews { if let found = horizontalScroll(in: child) { return found } }
        return nil
    }
    private func save(_ name: String, window: UIWindow) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty else { return }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in window.drawHierarchy(in: window.bounds, afterScreenUpdates: true) }
        try #require(image.pngData()).write(to: URL(fileURLWithPath: directory).appendingPathComponent(name + ".png"))
    }
}
