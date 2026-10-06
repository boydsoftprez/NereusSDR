// NereusSDR for iOS: Setup's Logs page against a fake Core: the live log, Clear on this phone only, the category switches
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

/// D95, R-IOS-36, spec section 5.2 item 11: Setup, Diagnostics, Logs shows
/// the Core's log as it happens, the backlog first and then each new line,
/// asked for only while the page is open; Clear empties this phone's view
/// and sends nothing; the category switches send `support.setLogCategories`
/// and follow the Core's `logCategories`, labelled by the Core's own
/// `logCategoryList` when it sends one, else by the phone's list behind the
/// ``LogCategorySource`` seam; an older Core greys the page with its reason.
/// With `NEREUS_MAIN_SHOTS` set, the page's pictures are written there.
@Suite("Core logs page", .serialized)
@MainActor
struct CoreLogsTests {
    private let platform = TestPlatform()

    @Test("Setup's Diagnostics has the Logs page, marked Core, even before the Core describes a page")
    func inTheTree() throws {
        let diagnostics = try #require(SetupTree.categories.first { $0.title == "Diagnostics" })
        #expect(diagnostics.pages.map(\.title) == ["Logs"])
        #expect(diagnostics.pages.map(\.tag) == [.core])
        #expect(SetupTree.route(to: .coreLogs) == [.category("Diagnostics"), .page(.coreLogs)])
    }

    @Test("the lines arrive in order, the backlog then each new one, and leaving the page stops the log")
    func liveLog() async throws {
        let (model, station) = try await connected(additions: [.spots, .supportBundle])
        let page = CoreLogsModel(log: model.coreLog)
        #expect(await settle { page.log.reason == nil })
        #expect(!station.messages.contains { Self.asksFor($0) })
        page.setOpen(true)
        #expect(await settle { page.lines.count == 3 })
        #expect(page.lines.map(\.line) == FakeStation.coreLogLines)
        await station.deliverCoreLog(["[18:35:00.000] INF: Slice A tuned", "[18:35:01.000] INF: Band 20m"])
        #expect(await settle { page.lines.count == 5 })
        #expect(page.lines.map(\.line) == FakeStation.coreLogLines
                + ["[18:35:00.000] INF: Slice A tuned", "[18:35:01.000] INF: Band 20m"])
        #expect(page.text == page.lines.map(\.line).joined(separator: "\n"))
        page.setOpen(false)
        #expect(await settle { page.lines.isEmpty })
        #expect(await settle { station.messages.contains { Self.stops($0) } })
        await model.disconnect()
    }

    @Test("Clear empties this phone's view and sends nothing to the Core; new lines then show, and Reload shows all")
    func clearIsLocal() async throws {
        let (model, station) = try await connected(additions: [.spots, .supportBundle])
        let page = CoreLogsModel(log: model.coreLog)
        page.setOpen(true)
        #expect(await settle { page.lines.count == 3 })
        #expect(page.canClear)
        let before = station.messages.count
        page.clear()
        #expect(page.lines.isEmpty && page.text == CoreLogsModel.clearedText)
        #expect(!page.canClear)
        // Nothing about the log went to the Core, and its log is as it was.
        try await Task.sleep(for: .milliseconds(300))
        #expect(!station.messages.dropFirst(before).contains { Self.touchesLog($0) })
        #expect(model.coreLog.lines.count == 3)
        await station.deliverCoreLog(["[18:36:00.000] INF: After the clear"])
        #expect(await settle { page.lines.map(\.line) == ["[18:36:00.000] INF: After the clear"] })
        #expect(page.text == "[18:36:00.000] INF: After the clear")
        // Reload reads the Core's newest lines again, all of them.
        page.reload()
        #expect(await settle { station.messages.filter { Self.asksFor($0) }.count == 2 && page.lines.count == 4 })
        page.setOpen(false)
        await model.disconnect()
    }

    @Test("the Logs page and the Support Bundle page share the log: one leaving never stops the other's")
    func sharedWithSupportBundle() async throws {
        let (model, station) = try await connected(additions: [.spots, .supportBundle])
        let page = CoreLogsModel(log: model.coreLog)
        page.setOpen(true)
        model.coreLog.setOpen(true)
        #expect(await settle { model.coreLog.lines.count == 3 })
        page.setOpen(false)
        try await Task.sleep(for: .milliseconds(300))
        #expect(!station.messages.contains { Self.stops($0) })
        #expect(model.coreLog.lines.count == 3)
        #expect(station.messages.filter { Self.asksFor($0) }.count == 1)
        model.coreLog.setOpen(false)
        #expect(await settle { station.messages.contains { Self.stops($0) } })
        await model.disconnect()
    }

    @Test("the switches send support.setLogCategories and follow the Core's logCategories")
    func switches() async throws {
        let (model, station) = try await connected(additions: [.spots, .supportBundle])
        let log = model.coreLog
        #expect(await settle { log.reason == nil })
        #expect(log.categories == PhoneHeldLogCategories.list)
        log.setCategory("nereus.connection", true)
        #expect(await settle { station.coreLogCategories == "nereus.connection" })
        #expect(await settle { log.on == ["nereus.connection"] })
        // A change made elsewhere (the desktop's Support dialog) shows here.
        await station.deliver(.delta(LinkMessage.Delta(key: "radio", properties: [
            .init(ordinal: try ordinal("RadioModel", "logCategories"), name: "logCategories",
                  value: .utf8("nereus.dsp,nereus.tci")),
        ])))
        #expect(await settle { log.on == ["nereus.dsp", "nereus.tci"] })
        log.setAll(false)
        #expect(await settle { station.coreLogCategories == "" })
        await model.disconnect()
    }

    @Test("the seam's list gives way to the Core's own labels, in the Core's order, when a source supplies them")
    func coreLabelsReplaceTheSeam() async throws {
        struct CoreLabels: LogCategorySource {
            func categories(radio: MirrorObject?) -> [CoreLogModel.Category] {
                [CoreLogModel.Category(id: "nereus.tci", title: "TCI server"),
                 CoreLogModel.Category(id: "nereus.discovery", title: "Finding radios")]
            }
        }
        let (model, station) = try await connected(additions: [.spots, .supportBundle])
        let log = CoreLogModel(mirror: model.mirror, commands: model.commands, records: model.records,
                               categories: CoreLabels())
        #expect(await settle { log.reason == nil && log.categories.map(\.title) == ["TCI server", "Finding radios"] })
        log.setAll(true)
        #expect(await settle { station.coreLogCategories == "nereus.tci,nereus.discovery" })
        await model.disconnect()
    }

    @Test("the Logs page's switches carry the Core's own labels when it sends logCategoryList")
    func coreLabelsFromTheCore() async throws {
        let (model, station) = try await connected(additions: [.spots, .supportBundle, .logCategoryList])
        let page = CoreLogsModel(log: model.coreLog)
        let suite = try #require(LogCategoryList(json: try FakeStation.suiteLogCategoryList()))
        #expect(await settle { page.log.reason == nil })
        #expect(page.log.categories.map(\.id) == suite.categories.map(\.id))
        #expect(page.log.categories.map(\.title) == suite.categories.map(\.label))
        page.log.setCategory("nereus.spots", true)
        #expect(await settle { station.coreLogCategories == "nereus.spots" })
        await model.disconnect()
    }

    @Test("a Core without supportBundleVersion 1 greys the page with its reason and sends nothing")
    func olderCore() async throws {
        let (model, station) = try await connected(additions: [.spots], without: ["supportBundleVersion"])
        let page = CoreLogsModel(log: model.coreLog)
        page.setOpen(true)
        #expect(await settle { page.log.reason == CoreLogModel.olderCoreReason })
        #expect(page.text == CoreLogModel.olderCoreReason)
        #expect(!page.canClear && !page.canReload)
        page.log.setCategory("nereus.tci", true)
        page.clear()
        page.reload()
        try await Task.sleep(for: .milliseconds(300))
        #expect(!station.messages.contains { AccessoryPagesTests.invoke($0)?.verb == "support.setLogCategories" })
        #expect(!station.messages.contains { Self.asksFor($0) })
        page.setOpen(false)
        await model.disconnect()
    }

    @Test("pictures of the Logs page with live lines, and greyed with its reason on an older Core")
    func pictures() async throws {
        let (model, station) = try await connected(additions: [.spots, .supportBundle])
        _ = try await model.commands.invoke(CoreLogLine.setCategoriesVerb, arguments: [
            CommandArgument(name: "categories", value: .text("nereus.discovery,nereus.connection,nereus.tci")),
        ], timeout: .seconds(10))
        #expect(await settle { model.coreLog.on.count == 3 })
        let flow = Self.flow(model, station)
        let router = SetupRouter()
        try await shoot("setup-logs", model: model, flow: flow, router: router,
                        ready: { model.coreLog.lines.count == 3 })
        await station.deliverCoreLog(["[18:35:00.000] INF: Slice A tuned to 14.074 MHz"])
        try await shoot("setup-logs-live", model: model, flow: flow, router: router,
                        ready: { model.coreLog.lines.count == 4 })
        await model.disconnect()

        let (older, olderStation) = try await connected(additions: [.spots], without: ["supportBundleVersion"])
        #expect(await settle { older.coreLog.reason == CoreLogModel.olderCoreReason })
        try await shoot("setup-logs-older-core", model: older, flow: Self.flow(older, olderStation),
                        router: SetupRouter())
        await older.disconnect()
    }

    // MARK: Helpers

    private static func flow(_ model: AppModel, _ station: FakeStation) -> ConnectionFlow {
        ConnectionFlow(app: model, dependencies: ConnectionFlow.Dependencies(
            keyStore: KeychainKeyStore(item: InMemorySecretItem()),
            stations: PairedStationStore(item: InMemorySecretItem()), kind: .phone,
            transportFactory: station.transportFactory, clock: TestLinkClock(),
            microphone: ConnectionFlowTests.FakeMicrophone(), network: ConnectionFlowTests.FakeNetwork(),
            appMajors: [1], now: Date.init, browser: nil))
    }

    private func shoot(_ name: String, model: AppModel, flow: ConnectionFlow, router: SetupRouter,
                       ready: () -> Bool = { true }) async throws {
        let size = CGSize(width: 402, height: 2000)
        let scene = try #require(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first)
        let window = UIWindow(windowScene: scene)
        window.frame = CGRect(origin: .zero, size: size)
        window.windowLevel = .alert + 1
        router.path = [.category("Diagnostics"), .page(.coreLogs)]
        let root = VStack(spacing: 0) {
            SetupTab(app: model, flow: flow, router: router, buildTag: nil)
            TabBar(selection: .constant(.setup), sideways: false)
        }
        .background(ChromeColours.bar.ignoresSafeArea())
        .preferredColorScheme(.dark)
        let host = UIHostingController(rootView: root)
        host.view.frame = CGRect(origin: .zero, size: size)
        window.rootViewController = host
        window.isHidden = false
        defer {
            window.isHidden = true
            window.rootViewController = nil
        }
        await ShotWait.laidOut(window)
        #expect(await settle(ready), "\(name) is ready")
        await ShotWait.laidOut(window)
        let image = UIGraphicsImageRenderer(bounds: window.bounds).image { _ in
            window.drawHierarchy(in: window.bounds, afterScreenUpdates: true)
        }
        #expect(image.size == size)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_MAIN_SHOTS"], !directory.isEmpty,
           let data = image.pngData() {
            let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
            try data.write(to: url)
            print("Wrote \(url.path)")
        }
    }

    private func connected(additions: FakeStation.Additions = [],
                           without: Set<String> = []) async throws -> (AppModel, FakeStation) {
        let defaults = try #require(UserDefaults(suiteName: "CoreLogsTests-\(UUID().uuidString)"))
        let model = AppModel(phoneSettings: PhoneSettings(defaults: defaults),
                             displaySettings: BandDisplaySettingsStore(defaults: defaults),
                             platform: platform.platform)
        let station = try FakeStation(additions: additions, withoutCapabilities: without)
        await model.connect(to: station.endpoint, trust: station.trust, authenticator: station.authenticator,
                            transportFactory: station.transportFactory)
        #expect(await station.waitUntilLive())
        #expect(await settle { model.connection == .connected && model.mirror.isSnapshotComplete })
        return (model, station)
    }

    private func ordinal(_ className: String, _ property: String) throws -> UInt16 {
        try #require(try FakeStation.schema(ofClass: className).fields.first { $0.name == property }?.ordinal)
    }

    /// The message asks the Core for its log.
    private static func asksFor(_ message: LinkMessage) -> Bool {
        let invoke = AccessoryPagesTests.invoke(message)
        return invoke?.verb == "records.subscribe" && invoke?.args.first?.value == .utf8("coreLog")
    }

    /// The message asks anything of the Core's log or its logging: a
    /// support verb, or a start or stop of its log stream.
    private static func touchesLog(_ message: LinkMessage) -> Bool {
        guard let invoke = AccessoryPagesTests.invoke(message) else {
            return false
        }
        return invoke.verb.hasPrefix("support.") || asksFor(message) || stops(message)
    }

    /// The message stops the Core's log.
    private static func stops(_ message: LinkMessage) -> Bool {
        let invoke = AccessoryPagesTests.invoke(message)
        return invoke?.verb == "records.unsubscribe" && invoke?.args.first?.value == .utf8("coreLog")
    }

    private func settle(seconds: Double = 30, _ condition: () -> Bool) async -> Bool {
        let deadline = Date().addingTimeInterval(seconds)
        while Date() < deadline {
            if condition() {
                return true
            }
            await Task.yield()
            try? await Task.sleep(for: .milliseconds(20))
        }
        return condition()
    }
}
