// NereusSDR for iOS: Filter Presets native routing and asynchronous result ownership
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusKitTesting
import NereusLink
import NereusMirror
@testable import NereusSDR
import SwiftUI
import Testing
import UIKit

@Suite("Filter Presets editor", .serialized)
@MainActor
struct FilterPresetsPanelTests {
    @MainActor final class Phone: SetupPhoneKeys {
        var value: Int64 = 1
        let subject = PassthroughSubject<Void, Never>()
        var changes: AnyPublisher<Void, Never> { subject.eraseToAnyPublisher() }
        func value(forPhoneKey key: String) -> SetupValue? { key == "filterPresetsMode" ? .integer(value) : nil }
        func set(_ value: SetupValue, forPhoneKey key: String) -> Bool {
            guard key == "filterPresetsMode", let number = value.whole else { return false }
            self.value = number; subject.send(); return true
        }
    }
    @MainActor final class Rig {
        let mirror: MirrorStore
        let settings: SettingsProxyClient
        let feed: SetupDescriptionFeed
        let pages: SetupDescribedPages
        let dispatcher: SetupControlDispatcher
        let phone = Phone()
        let sent = Sent()
        let clock = TestLinkClock()
        var editor: FilterPresetsEditor { dispatcher.filterPresets }
        @MainActor final class Sent { var messages: [LinkMessage] = [] }
        init() {
            let sent = sent
            let sender: MirrorStore.BoundSender = { message, permit in
                try await MainActor.run {
                    guard permit.handoff({ sent.messages.append(message); return true }) else { throw LinkSendError.notConnected }
                }
            }
            mirror = MirrorStore(send: { _ in }, clock: clock)
            settings = SettingsProxyClient(send: { _ in }, captureSender: { sender }, clock: clock)
            feed = SetupDescriptionFeed(store: mirror)
            let commands = CommandClient(clock: clock, send: { _ in }, captureSender: { sender })
            dispatcher = SetupControlDispatcher(store: mirror, feed: feed, settings: settings, commands: commands,
                captureSender: { sender }, selectedSlice: { nil }, selectionChanges: Just(nil).eraseToAnyPublisher(), phone: phone)
            pages = SetupDescribedPages(feed: feed, store: mirror, hello: Just(nil).eraseToAnyPublisher())
        }
        func connect() async throws {
            mirror.handle(.stateChanged(.receivingSnapshot)); settings.handle(.stateChanged(.receivingSnapshot))
            let category = try #require(try SetupDescribedPagesTests.coreCategories(peer: 15).first { $0.id == "dsp" })
            let catalog = try #require(MainScreenTests.catalogueJSON())
            let messages: [LinkMessage] = [.hello(.init(major: 1, minor: 11, settingsSchema: 0, peer: "nereusd")),
                .authResult(.init(accepted: true, reason: "", retryable: false)),
                .capabilities(.init(properties: [.init(name: "setupDescriptionVersion", value: .i64(15)), .init(name: "stationCatalogVersion", value: .i64(1))])),
                .schema(.init(className: "SetupDescription", fields: [.init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "dsp", kind: .utf8)])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [.init(name: "revision", value: .i64(1)), .init(name: "dsp", value: .utf8(category.json))])),
                .objectCreate(.init(key: "catalog", className: "StationCatalog", properties: [.init(name: "json", value: .utf8(catalog)), .init(name: "revision", value: .i64(1))])),
                .settingsSnapshot(.init(properties: [])), .snapshotComplete]
            for message in messages { mirror.apply(message); settings.apply(message) }
            mirror.handle(.stateChanged(.ready)); settings.handle(.stateChanged(.ready))
            await MainQueue.drained()
            try #require(await ShotWait.until { self.pages.page("dsp.filterPresets", in: "dsp") != nil })
            pages.refresh()
            try #require(!editor.rows.isEmpty)
        }
        func control(_ suffix: String = "presets") throws -> SetupDescription.Control {
            try #require(pages.page("dsp.filterPresets", in: "dsp")?.sections.flatMap(\.controls).first { $0.id == "dsp.filterPresets." + suffix })
        }
        func answer(_ index: Int, accepted: Bool = true) {
            if case .settingsWrite(let write) = sent.messages[index] {
                if accepted { settings.apply(.settingsValue(.init(key: write.key, origin: settings.origin, properties: write.properties))) }
                else { settings.apply(.settingsReject(.init(key: write.key, properties: [], reason: "Exact Core test refusal"))) }
            }
        }
    }

    actor PresentationBarrier {
        private var entered = false
        private var enteredWaiters: [CheckedContinuation<Void, Never>] = []
        private var resume: CheckedContinuation<Void, Never>?
        func hold() async {
            entered = true
            for waiter in enteredWaiters { waiter.resume() }
            enteredWaiters = []
            await withCheckedContinuation { resume = $0 }
        }
        func waitUntilEntered() async {
            if entered { return }
            await withCheckedContinuation { enteredWaiters.append($0) }
        }
        func release() { resume?.resume(); resume = nil }
    }

    @Test(arguments: [false, true])
    func replacementOwnerCannotPresentFinalRefusal(_ rebind: Bool) async throws {
        let rig = Rig(); try await rig.connect()
        let interaction = FilterPresetInteraction(editor: rig.editor)
        let control = try rig.control()
        let first = try #require(interaction.begin(control, slot: rig.editor.rows[0].slot))
        let barrier = PresentationBarrier()
        interaction.beforeOutcomePresentationForTesting = { _ in await barrier.hold() }
        let task = Task { await interaction.perform(first) { late in await rig.editor.edit(first, field: "lowHz", value: .integer(123), onLateOutcome: late) } }
        try #require(await ShotWait.until { rig.sent.messages.count == 1 })
        rig.answer(0, accepted: false)
        await barrier.waitUntilEntered()
        if rebind {
            rig.mirror.handle(.stateChanged(.stopped)); rig.settings.handle(.stateChanged(.stopped))
            try await rig.connect()
        } else { _ = interaction.begin(control, slot: rig.editor.rows[1].slot) }
        await barrier.release()
        #expect(await task.value == nil)
        #expect(interaction.problem == nil)
    }

    @Test func padCapturesRowAndClosesOnModeABAWithoutSending() async throws {
        let rig = Rig(); try await rig.connect()
        let interaction = FilterPresetInteraction(editor: rig.editor)
        let control = try rig.control()
        guard case .filterPresets(let binding)? = control.binding else { Issue.record("Missing table binding"); return }
        let host = ValuePadHost()
        let row = rig.editor.rows[0]
        interaction.openPad(host: host, control: control, row: row, column: try #require(binding.column("lowHz")))
        let pad = try #require(host.pad)
        #expect(pad.numberRange == -10000...10000 && pad.decimals == 0)
        _ = rig.phone.set(.integer(0), forPhoneKey: "filterPresetsMode")
        _ = rig.phone.set(.integer(1), forPhoneKey: "filterPresetsMode")
        #expect(host.pad == nil)
        #expect(rig.sent.messages.isEmpty)
    }

    /// The production number row opens the production shared pad. Its first
    /// constituent setting is stopped by the real proxy, rather than a mocked
    /// pad answer. A failed whole-row gesture must not leave a sendable-looking
    /// pad backed by its already-consumed owner.
    @Test("production numeric opening closes on stopped whole-row sequence and fresh reopen writes", arguments: [false, true])
    func productionNumberPadStoppedSequenceClosesAndFreshReopenWrites(_ timeout: Bool) async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let rig = Rig(); try await rig.connect()
        let original = rig.editor.rows
        rig.editor.selectedSlot = original[0].slot
        let pads = ValuePadHost()
        let window = try await numberWindow(rig: rig, pads: pads, label: timeout ? "number-timeout-prerequisite" : "number-refusal-prerequisite")
        defer { pads.close(); window.isHidden = true; window.rootViewController = nil }
        let id = "dsp.filterPresets.presets.slot\(original[0].slot).lowHz"
        let row = try #require(SetupTypedEntryTests.element(id, in: window))
        #expect(SetupTypedEntryTests.perform(SetupNumberRow.enterValueAction, on: row))
        let stoppedPad = try #require(pads.pad)
        SetupTypedEntryTests.type("123", on: stoppedPad)
        let stopped = Task { await stoppedPad.enter() }
        try #require(await ShotWait.until { rig.sent.messages.count == 1 })
        if timeout { await rig.clock.advance(by: 5000) }
        else { rig.answer(0, accepted: false) }
        #expect(await stopped.value == false)
        await ShotWait.laidOut(window)
        #expect(pads.pad == nil, "The exact stopped production numeric opening must close")
        #expect(!stoppedPad.canEnter && stoppedPad.outcomeIdentity == nil,
                "A consumed whole-row owner must not leave an Enter-enabled pad")
        let problem = try #require(SetupTypedEntryTests.element(id + ".problem", in: window))
        #expect(problem.accessibilityLabel == (timeout ? SetupControlDispatcher.noAnswerReason : "Exact Core test refusal"))
        #expect(rig.editor.rows == original, "Stopped settings must not fabricate a whole Core catalogue row")
        let current = try #require(SetupTypedEntryTests.element(id, in: window))
        #expect(current.accessibilityValue == SetupNumberRow.text(Double(original[0].lowHz), decimals: 0, unit: "Hz"))
        #expect(rig.sent.messages.count == 1, "The unsent low/high suffix must remain unsent")
        if timeout {
            // Settle the one already-sent key before admitting a new gesture.
            // The late echo must reconcile data without resuming the suffix.
            rig.answer(0)
            await MainQueue.drained()
            #expect(rig.sent.messages.count == 1 && pads.pad == nil)
        }
        let freshRow = try #require(SetupTypedEntryTests.element(id, in: window))
        #expect(SetupTypedEntryTests.perform(SetupNumberRow.enterValueAction, on: freshRow))
        let fresh = try #require(pads.pad)
        #expect(fresh.id != stoppedPad.id && fresh.canEnter)
        SetupTypedEntryTests.type("456", on: fresh)
        let kept = Task { await fresh.enter() }
        for index in 1...3 {
            try #require(await ShotWait.until { rig.sent.messages.count == index + 1 })
            rig.answer(index)
        }
        #expect(await kept.value == true)
        #expect(pads.pad == nil && !fresh.canEnter)
        let keys = rig.sent.messages.compactMap { message -> String? in
            if case .settingsWrite(let write) = message { return write.key }; return nil
        }
        #expect(keys == ["filters/USB/\(original[0].slot)/name", "filters/USB/\(original[0].slot)/name",
                         "filters/USB/\(original[0].slot)/low", "filters/USB/\(original[0].slot)/high"])
        #expect(rig.editor.rows == original, "Accepted settings alone do not invent a catalogue echo")
        try pushObservedLow(456, slot: original[0].slot, rig: rig)
        await ShotWait.laidOut(window)
        #expect(rig.editor.rows[0].lowHz == 456)
        let echoed = try #require(SetupTypedEntryTests.element(id, in: window))
        #expect(echoed.accessibilityValue == SetupNumberRow.text(456, decimals: 0, unit: "Hz"))
    }

    @Test("a stopped old production opening cannot close or annotate a replacement pad")
    func stoppedOldNumberPadDoesNotCloseReplacementOpening() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let rig = Rig(); try await rig.connect(); rig.editor.selectedSlot = rig.editor.rows[0].slot
        let pads = ValuePadHost()
        let window = try await numberWindow(rig: rig, pads: pads, label: "number-replacement-prerequisite")
        defer { pads.close(); window.isHidden = true; window.rootViewController = nil }
        let id = "dsp.filterPresets.presets.slot\(rig.editor.rows[0].slot).lowHz"
        let row = try #require(SetupTypedEntryTests.element(id, in: window))
        #expect(SetupTypedEntryTests.perform(SetupNumberRow.enterValueAction, on: row))
        let old = try #require(pads.pad)
        SetupTypedEntryTests.type("123", on: old)
        let task = Task { await old.enter() }
        try #require(await ShotWait.until { rig.sent.messages.count == 1 })
        // Host replacement is the existing shared-pad operation, and retires
        // the exact previous opening before its result can be presented.
        pads.open { close in SetupNumberRow.pad(title: "Replacement test opening", value: 77,
            range: -10000...10000, unit: "Hz", decimals: 0, send: { _ in SetupNumberRow.kept }, close: close) }
        let replacement = try #require(pads.pad)
        rig.answer(0, accepted: false)
        #expect(await task.value == false)
        await ShotWait.laidOut(window)
        #expect(pads.pad === replacement && replacement.canEnter && replacement.refusal == nil)
        #expect(!old.canEnter && old.outcomeIdentity == nil)
        #expect(SetupTypedEntryTests.element(id + ".problem", in: window) == nil)
        #expect(rig.sent.messages.count == 1)
    }

    private func numberWindow(rig: Rig, pads: ValuePadHost, label: String) async throws -> UIWindow {
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(x: 0, y: 0, width: 402, height: 1200); window.windowLevel = .alert + 1
        let root = NavigationStack {
            DescribedPage(pages: rig.pages, dispatcher: rig.dispatcher, category: "dsp", pageId: "dsp.filterPresets")
        }.valuePads(pads).environment(\.filterPresetsTablet, false).preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root); host.view.frame = window.bounds
        window.rootViewController = host; window.makeKeyAndVisible()
        await ShotWait.laidOut(window)
        #expect(rig.editor.selectedSlot == rig.editor.rows.first?.slot,
                "The native numeric fixture must retain its selected first Core slot")
        try diagnostics(label, rig: rig, window: window)
        try save(label, window: window)
        return window
    }

    private func pushObservedLow(_ value: Int, slot: Int, rig: Rig) throws {
        let catalog = try #require(rig.mirror.object("catalog"))
        guard case .text(let json)? = catalog["json"], case .int(let revision)? = catalog["revision"] else {
            throw SetupRefusal(reason: "Missing test Core catalogue")
        }
        var root = try #require(JSONSerialization.jsonObject(with: Data(json.utf8)) as? [String: Any])
        var bank = try #require(root["filterPresets"] as? [String: [[String: Any]]])
        var rows = try #require(bank["USB"])
        let index = try #require(rows.firstIndex { $0["slot"] as? Int == slot })
        rows[index]["lowHz"] = value; bank["USB"] = rows; root["filterPresets"] = bank
        let observed = String(decoding: try JSONSerialization.data(withJSONObject: root), as: UTF8.self)
        rig.mirror.apply(.delta(.init(key: "catalog", properties: [.init(name: "json", value: .utf8(observed)),
            .init(name: "revision", value: .i64(revision + 1))])))
    }

    @Test("actual described page, approved phone inline rows and iPad grid, light/dark/large type")
    func nativePageAndApprovedShots() async throws {
        let accessibility = SetupTypedEntryTests.applicationAccessibility()
        SetupTypedEntryTests.setApplicationAccessibility(true)
        defer { SetupTypedEntryTests.setApplicationAccessibility(accessibility) }
        let rig = Rig(); try await rig.connect()
        let page = try #require(rig.pages.page("dsp.filterPresets", in: "dsp"))
        let controls = page.sections.flatMap(\.controls)
        #expect(controls.filter { if case .filterPresets? = $0.binding { return true }; return false }.allSatisfy { DescribedControl.row(for: $0) == .panel })
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let shots: [(String, CGFloat, CGFloat, Bool, Int?, DynamicTypeSize, ColorScheme)] = [
            ("phone-overview-dark", 402, 874, false, nil, .large, .dark),
            ("phone-first-inline-dark", 402, 874, false, 0, .large, .dark),
            ("phone-last-inline-dark", 402, 874, false, rig.editor.rows.last?.slot, .large, .dark),
            ("phone-first-inline-light", 402, 874, false, 0, .large, .light),
            ("phone-large-type", 402, 874, false, 0, .accessibility2, .dark),
            ("ipad-portrait", 1024, 1366, true, 0, .large, .dark),
            ("ipad-landscape-light", 1366, 1024, true, 0, .large, .light),
            ("ipad-large-type", 1024, 1366, true, 0, .accessibility2, .dark)]
        for (name, width, height, tablet, selection, type, scheme) in shots {
            rig.editor.selectedSlot = selection
            let pads = ValuePadHost()
            let window = UIWindow(windowScene: scene)
            window.frame = CGRect(x: 0, y: 0, width: width, height: height); window.windowLevel = .alert + 1
            let root = NavigationStack {
                DescribedPage(pages: rig.pages, dispatcher: rig.dispatcher, category: "dsp", pageId: "dsp.filterPresets")
            }.valuePads(pads).environment(\.filterPresetsTablet, tablet).environment(\.dynamicTypeSize, type).preferredColorScheme(scheme)
            let host = UIHostingController(rootView: root); host.view.frame = window.bounds
            window.rootViewController = host; window.makeKeyAndVisible()
            defer { pads.close(); window.isHidden = true; window.rootViewController = nil }
            await ShotWait.laidOut(window)
            #expect(rig.editor.selectedSlot == selection,
                    "The native page must retain the fixture's exact selected Core slot")
            try diagnostics(name + "-prelookup", rig: rig, window: window)
            try save(name + "-prelookup", window: window)
            try #require(SetupTypedEntryTests.element("dsp.filterPresets.mode", in: window))
            if tablet { try #require(SetupTypedEntryTests.element("dsp.filterPresets.presets.grid", in: window)) }
            if !tablet, selection == 0 {
                try #require(SetupTypedEntryTests.element("dsp.filterPresets.presets.slot0.name", in: window))
                #expect(SetupTypedEntryTests.element("dsp.filterPresets.presets.slot1.name", in: window) == nil)
            }
            try save(name, window: window)
            if !tablet, selection == 0, type == .large {
                let row = try #require(SetupTypedEntryTests.element("dsp.filterPresets.presets.slot0.lowHz", in: window))
                #expect(SetupTypedEntryTests.actions(row).contains(SetupNumberRow.enterValueAction))
                #expect(SetupTypedEntryTests.perform(SetupNumberRow.enterValueAction, on: row))
                let pad = try #require(pads.pad)
                #expect(pad.numberRange == -10000...10000 && pad.decimals == 0)
                await ShotWait.laidOut(window)
                try save(name + "-pad", window: window)
                pads.close()
                await ShotWait.laidOut(window)
            }
            if let list = SetupDescribedPagesTests.firstScrollView(in: host.view) {
                list.contentOffset.y = max(-list.adjustedContentInset.top, list.contentSize.height + list.adjustedContentInset.bottom - list.bounds.height)
                await ShotWait.laidOut(window)
                try #require(SetupTypedEntryTests.element("dsp.filterPresets.resetAll", in: window))
                try save(name + "-actions", window: window)
            }
        }
        #expect(rig.sent.messages.isEmpty, "Rendering and selecting an editor sends no settings or slice retune")
    }
    /// One bounded traversal records both UIKit views and SwiftUI accessibility
    /// nodes without replacing any exact identifier or action assertions. The
    /// model selection plus disclosure values distinguish a collapsed fixture
    /// from inherited AX identity or a child outside the List viewport.
    private func diagnostics(_ name: String, rig: Rig, window: UIWindow) throws {
        var pending: [NSObject] = [window]
        var seen: Set<ObjectIdentifier> = []
        var nodes: [[String: Any]] = []
        let viewport = window.convert(window.bounds, to: nil)
        while !pending.isEmpty, seen.count < 20000 {
            let node = pending.removeFirst()
            guard seen.insert(ObjectIdentifier(node)).inserted else { continue }
            let frame = node.accessibilityFrame
            var entry: [String: Any] = ["class": String(reflecting: type(of: node)),
                "identifier": SetupTypedEntryTests.identifier(node) ?? "",
                "label": node.accessibilityLabel ?? "", "value": node.accessibilityValue ?? "",
                "isAccessibilityElement": node.isAccessibilityElement,
                "frame": NSCoder.string(for: frame), "intersectsWindow": viewport.intersects(frame),
                "actions": SetupTypedEntryTests.actions(node)]
            if let view = node as? UIView {
                entry["viewFrame"] = NSCoder.string(for: view.convert(view.bounds, to: window))
                entry["hidden"] = view.isHidden
                if let scroll = view as? UIScrollView {
                    entry["contentSize"] = NSCoder.string(for: scroll.contentSize)
                    entry["contentOffset"] = NSCoder.string(for: scroll.contentOffset)
                }
                pending += view.subviews
            }
            if let elements = node.accessibilityElements as? [NSObject] { pending += elements }
            else {
                let count = node.accessibilityElementCount()
                if count != NSNotFound, count > 0 {
                    pending += (0..<min(count, 20000)).compactMap { node.accessibilityElement(at: $0) as? NSObject }
                }
            }
            nodes.append(entry)
        }
        let control = try rig.control()
        let payload: [String: Any] = ["scenario": name, "selectedSlot": rig.editor.selectedSlot.map(String.init) ?? "nil",
            "mode": rig.editor.mode ?? "nil", "rows": rig.editor.rows.map { ["slot": String($0.slot), "name": $0.name] },
            "tableReason": rig.editor.reason(for: control) ?? "nil", "pageCurrent": rig.pages.isCurrent,
            "snapshotComplete": rig.mirror.isSnapshotComplete, "snapshotStale": rig.mirror.isStale,
            "sentCount": rig.sent.messages.count, "visited": seen.count, "truncated": !pending.isEmpty,
            "nodes": nodes]
        let data = try JSONSerialization.data(withJSONObject: payload, options: [.prettyPrinted, .sortedKeys])
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty {
            try data.write(to: URL(fileURLWithPath: directory).appendingPathComponent("filter-presets-diagnostic-" + name + ".json"))
        } else {
            print("FILTER_PRESETS_NATIVE_DIAGNOSTIC " + String(decoding: data, as: UTF8.self))
        }
    }

    private func save(_ name: String, window: UIWindow) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty else { return }
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in window.drawHierarchy(in: window.bounds, afterScreenUpdates: true) }
        try #require(image.pngData()).write(to: URL(fileURLWithPath: directory).appendingPathComponent("filter-presets-" + name + ".png"))
    }
}
