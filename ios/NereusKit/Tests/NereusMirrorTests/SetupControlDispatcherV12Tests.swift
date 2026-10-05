// NereusSDR for iOS: V12's buttons, phone-key dependencies and per-band labels through the Setup dispatcher
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import LinkSessionTestSupport
import NereusLink
import Testing
@testable import NereusMirror

@MainActor
private final class V12PhoneKeys: SetupPhoneKeys {
    var values: [String: SetupValue] = [
        "DisplaySpectrumDetector": .integer(0), "DisplayDispNormalize": .bool(false),
        "DisplayGridMax": .integer(-40), "DisplayWfUseSpectrumMinMax": .bool(false),
        "DisplayWfHighLevel": .integer(-62), "DisplayWfColorScheme": .integer(0),
    ]
    var ran: [String] = []
    var band: String?
    var refreshRate: Int? = 120
    private let subject = PassthroughSubject<Void, Never>()
    func value(forPhoneKey key: String) -> SetupValue? { values[key] }
    func set(_ value: SetupValue, forPhoneKey key: String) -> Bool {
        guard values[key] != nil else { return false }
        values[key] = value
        subject.send()
        return true
    }
    var changes: AnyPublisher<Void, Never> { subject.eraseToAnyPublisher() }
    func canPerform(_ action: String) -> Bool { action == "smoothDefaults" }
    func perform(_ action: String) -> Bool {
        guard canPerform(action) else { return false }
        ran.append(action)
        return true
    }
    var perBandName: String? { band }
    var screenRefreshRate: Int? { refreshRate }
    func unavailableOptions(forPhoneKey key: String) -> [Int64: String] {
        key == "DisplayWfColorScheme" ? [6: "Not here."] : [:]
    }
    func unavailableReason(forPhoneKey key: String) -> String? {
        key == "MultimeterSignalHistoryDurationMs" ? "No graph here." : nil
    }
}

/// V12 (`display-v12-for-phone.md`): buttons run the phone's action, Get
/// Monitor Hz writes the Core's FPS setting as an FPS edit, a phone-key
/// dependency greys its row, and a per-band row names its band.
@MainActor
@Suite struct SetupControlDispatcherV12Tests {
    @MainActor final class Rig {
        let route = TestRoute()
        let store: MirrorStore
        let settings: SettingsProxyClient
        let commands: CommandClient
        let feed: SetupDescriptionFeed
        fileprivate let phone = V12PhoneKeys()
        let dispatcher: SetupControlDispatcher

        init() {
            let route = route
            store = MirrorStore(send: { _ in })
            settings = SettingsProxyClient(send: { _ in }, captureSender: { route.capture() })
            commands = CommandClient(send: { _ in }, captureSender: { route.capture() })
            feed = SetupDescriptionFeed(store: store)
            dispatcher = SetupControlDispatcher(store: store, feed: feed, settings: settings, commands: commands,
                                                captureSender: { route.capture() }, selectedSlice: { nil },
                                                selectionChanges: Just(nil).eraseToAnyPublisher(), phone: phone)
        }

        func event(_ event: StationSession.Event) async {
            store.handle(event)
            settings.handle(event)
            await commands.handle(event)
        }

        /// A session with the Core's own V12 Display description and FPS 30.
        func connect(fps: String = "30") async throws {
            let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
                .appendingPathComponent("../../../../resources/setup/display.json").standardizedFileURL
            let display = try String(contentsOf: source, encoding: .utf8)
            route.use(1)
            await event(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [
                FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                .capabilities(.init(properties: [
                    .init(name: "setupDescriptionVersion", value: .i64(12)),
                    .init(name: "displayExtrasVersion", value: .i64(4)),
                ])),
                .schema(.init(className: "SetupDescription", fields: [
                    .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "display", kind: .utf8),
                ])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "display", value: .utf8(display)),
                ])),
                .settingsSnapshot(.init(properties: [.init(name: "DisplaySpectrumFps", value: .utf8(fps))])),
                .snapshotComplete,
            ]
            for message in messages { await event(.message(message)) }
            await event(.stateChanged(.ready))
            for _ in 0..<5 { await Task.yield() }
        }

        func control(_ id: String) throws -> SetupDescription.Control {
            let description = try #require(feed.description(for: "display"))
            return try #require(description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == id })
        }
    }

    @Test func aButtonRunsThePhonesActionAndOneItLacksStaysGreyed() async throws {
        let rig = Rig()
        try await rig.connect()
        #expect(rig.feed.description(for: "display")?.version == 12)
        let smooth = try rig.control("display.spectrumDefaults.smoothDefaults")
        #expect(rig.dispatcher.state(of: smooth, in: "display") == .init(value: nil, editable: true, reason: nil))
        #expect(await rig.dispatcher.edit(smooth, in: "display", to: nil) == .applied)
        #expect(rig.phone.ran == ["smoothDefaults"])
        let copy = try rig.control("display.waterfallDefaults.copySpectrumMinMax")
        #expect(rig.dispatcher.state(of: copy, in: "display").reason == SetupControlDispatcher.notOnThisPhoneReason)
        #expect(await rig.dispatcher.edit(copy, in: "display", to: nil)
                == .notSent(SetupControlDispatcher.notOnThisPhoneReason))
        #expect(rig.phone.ran == ["smoothDefaults"])
        #expect(rig.route.sent.isEmpty)
    }

    @Test func getMonitorHzWritesTheRefreshRateHeldToTheFpsRange() async throws {
        let rig = Rig()
        try await rig.connect()
        let monitor = try rig.control("display.spectrumDefaults.getMonitorHz")
        #expect(rig.dispatcher.state(of: monitor, in: "display").editable)
        // 120 Hz is held to the FPS row's 60, written as the Core's setting.
        let pressing = Task { await rig.dispatcher.edit(monitor, in: "display", to: nil) }
        #expect(await rig.route.waitForCount(1, unless: pressing))
        guard case .settingsWrite(let write) = rig.route.sent[0].message else {
            Issue.record("no settings write"); return
        }
        #expect(write.key == "DisplaySpectrumFps" && write.properties.first?.value == .utf8("60"))
        rig.settings.apply(.settingsValue(.init(key: write.key, origin: write.origin, properties: write.properties)))
        #expect(await pressing.value == .applied)
        // A screen rate the phone cannot read greys it.
        rig.phone.refreshRate = nil
        #expect(rig.dispatcher.state(of: monitor, in: "display").reason == SetupControlDispatcher.notOnThisPhoneReason)
    }

    @Test func getMonitorHzIsGreyedWheneverTheFpsRowIs() async throws {
        let rig = Rig()
        try await rig.connect(fps: "not a number")
        let fps = try rig.control("display.spectrumDefaults.fps")
        let monitor = try rig.control("display.spectrumDefaults.getMonitorHz")
        let reason = rig.dispatcher.state(of: fps, in: "display").reason
        #expect(reason == SetupControlDispatcher.valueMissingReason)
        #expect(rig.dispatcher.state(of: monitor, in: "display").reason == reason)
        #expect(await rig.dispatcher.edit(monitor, in: "display", to: nil) == .notSent(reason!))
        #expect(rig.route.sent.isEmpty)
    }

    @Test func aPhoneKeyDependencyGreysItsRowUntilTheValueFits() async throws {
        let rig = Rig()
        try await rig.connect()
        let normalize = try rig.control("display.spectrumDefaults.normalize")
        // Detector Peak: Normalize waits for Average, Sample or RMS.
        #expect(rig.dispatcher.state(of: normalize, in: "display")
                == .init(value: .bool(false), editable: false, reason: SetupControlDispatcher.dependsReason))
        #expect(await rig.dispatcher.edit(normalize, in: "display", to: .bool(true))
                == .notSent(SetupControlDispatcher.dependsReason))
        rig.phone.values["DisplaySpectrumDetector"] = .integer(3)
        #expect(rig.dispatcher.state(of: normalize, in: "display").editable)
        #expect(await rig.dispatcher.edit(normalize, in: "display", to: .bool(true)) == .applied)
        #expect(rig.phone.values["DisplayDispNormalize"] == .bool(true))
        // A switch dependency: the thresholds wait while the waterfall uses the spectrum's range.
        let high = try rig.control("display.waterfallDefaults.highThreshold")
        #expect(rig.dispatcher.state(of: high, in: "display").editable)
        rig.phone.values["DisplayWfUseSpectrumMinMax"] = .bool(true)
        #expect(rig.dispatcher.state(of: high, in: "display").reason == SetupControlDispatcher.dependsReason)
    }

    @Test func anOptionThePhoneCannotShowIsListedAndRefusedWithItsReason() async throws {
        let rig = Rig()
        try await rig.connect()
        let scheme = try rig.control("display.waterfallDefaults.colorScheme")
        #expect(rig.dispatcher.unavailableOptions(of: scheme) == [6: "Not here."])
        #expect(rig.dispatcher.state(of: scheme, in: "display").editable)
        #expect(await rig.dispatcher.edit(scheme, in: "display", to: .integer(6)) == .notSent("Not here."))
        #expect(rig.phone.values["DisplayWfColorScheme"] == .integer(0))
        #expect(await rig.dispatcher.edit(scheme, in: "display", to: .integer(7)) == .applied)
        #expect(rig.phone.values["DisplayWfColorScheme"] == .integer(7))
        // Only a phone key's options.
        #expect(rig.dispatcher.unavailableOptions(of: try rig.control("display.spectrumDefaults.window")).isEmpty)
    }

    @Test func aPhoneKeyThePhoneCannotKeepIsGreyedWithThePhonesOwnReason() async throws {
        let rig = Rig()
        try await rig.connect()
        let history = try rig.control("display.multimeter.historyDuration")
        #expect(rig.dispatcher.state(of: history, in: "display")
                == .init(value: nil, editable: false, reason: "No graph here."))
        #expect(await rig.dispatcher.edit(history, in: "display", to: .integer(30_000)) == .notSent("No graph here."))
        // A phone key the phone neither keeps nor explains keeps the general reason.
        let unit = try rig.control("display.multimeter.unitMode")
        #expect(rig.dispatcher.state(of: unit, in: "display").reason == SetupControlDispatcher.notOnThisPhoneReason)
        #expect(rig.route.sent.isEmpty)
    }

    @Test func aPerBandRowNamesTheBandThePanIsOn() async throws {
        let rig = Rig()
        try await rig.connect()
        let dbMax = try rig.control("display.gridScales.dbMax")
        // Before the Core names a band, the Core's own label.
        #expect(rig.dispatcher.label(of: dbMax) == "dB Max (per band):")
        rig.phone.band = "20m"
        #expect(rig.dispatcher.label(of: dbMax) == "dB Max (20m):")
        #expect(rig.dispatcher.label(of: try rig.control("display.gridScales.dbStep")) == "dB Step (global):")
    }
}
