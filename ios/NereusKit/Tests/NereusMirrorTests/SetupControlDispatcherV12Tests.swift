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
    var writes: [(String, SetupValue)] = []
    var ran: [String] = []
    var actionVersions: [Int] = []
    var reasons: [String: String] = [:]
    var band: String?
    var refreshRate: Int? = 120
    private let subject = PassthroughSubject<Void, Never>()
    func value(forPhoneKey key: String) -> SetupValue? { values[key] }
    func set(_ value: SetupValue, forPhoneKey key: String) -> Bool {
        guard values[key] != nil else { return false }
        values[key] = value
        writes.append((key, value))
        subject.send()
        return true
    }
    var changes: AnyPublisher<Void, Never> { subject.eraseToAnyPublisher() }
    func canPerform(_ action: String) -> Bool {
        ["smoothDefaults", "editReceiveGradient", "editTransmitGradient", "ClarityRetune"].contains(action)
    }
    func perform(_ action: String) -> Bool {
        guard canPerform(action) else { return false }
        ran.append(action)
        return true
    }
    func perform(_ action: String, argument: String?, descriptionVersion: Int) -> Bool {
        actionVersions.append(descriptionVersion)
        return perform(action, argument: argument)
    }
    var perBandName: String? { band }
    var screenRefreshRate: Int? { refreshRate }
    func unavailableOptions(forPhoneKey key: String) -> [Int64: String] {
        key == "DisplayWfColorScheme" ? [6: "Not here."] : [:]
    }
    func unavailableReason(forPhoneKey key: String) -> String? {
        reasons[key] ?? (key == "MultimeterSignalHistoryDurationMs" ? "No graph here." : nil)
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

        /// A session with the Core's own Display description and FPS 30: at
        /// `version` 12 the form a Core sends a phone below 26, at 26 the
        /// form it sends a phone below 28, at 28 the Core's file as it is.
        /// `stored` adds the Core's settings the session starts with.
        func connect(fps: String = "30", version: Int64 = 12, media: Int64 = 0, extras: Int64 = 4, stored: [String: String] = [:]) async throws {
            let resource = version >= 29 ? "display" : "legacy/display-v28"
            let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
                .appendingPathComponent("../../../../resources/setup/\(resource).json").standardizedFileURL
            var display = try String(contentsOf: source, encoding: .utf8)
            if version < 28 {
                let root = try #require(JSONSerialization.jsonObject(with: Data(display.utf8)) as? [String: Any])
                let older = try JSONSerialization.data(withJSONObject: version < 26
                    ? DisplayV12Fixture.document(root) : DisplayV26Fixture.document(root))
                display = String(decoding: older, as: UTF8.self)
            }
            route.use(1)
            await event(.stateChanged(.receivingSnapshot))
            let messages: [LinkMessage] = [
                FixtureReplay.stationHello(minor: 11), FixtureReplay.accepted,
                .capabilities(.init(properties: [
                    .init(name: "setupDescriptionVersion", value: .i64(version)),
                    .init(name: "displayExtrasVersion", value: .i64(extras)),
                    .init(name: "remoteMediaVersion", value: .i64(media)),
                ])),
                .schema(.init(className: "SetupDescription", fields: [
                    .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "display", kind: .utf8),
                ])),
                .objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
                    .init(ordinal: 0, name: "revision", value: .i64(1)),
                    .init(ordinal: 1, name: "display", value: .utf8(display)),
                ])),
                .settingsSnapshot(.init(properties: [.init(name: "DisplaySpectrumFps", value: .utf8(fps))]
                    + stored.sorted { $0.key < $1.key }.map { .init(name: $0.key, value: .utf8($0.value)) })),
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

    @Test(arguments: [Int64(28), 29])
    func waterfallRateKeepsElevenAndTwelveMilliseconds(version: Int64) async throws {
        let rig = Rig()
        rig.phone.values["DisplayWfUpdatePeriodMs"] = .integer(10)
        try await rig.connect(version: version)
        let row = try rig.control("display.waterfallDefaults.updatePeriod")
        #expect(rig.dispatcher.resolved(row).range?.step == 1)
        for value: Int64 in [11, 12] {
            #expect(await rig.dispatcher.edit(row, in: "display", to: .integer(value)) == .applied)
            #expect(rig.phone.values["DisplayWfUpdatePeriodMs"] == .integer(value))
        }
        #expect(rig.phone.writes.map { $0.1 } == [.integer(11), .integer(12)])
        #expect(rig.route.sent.isEmpty)
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

    @Test func admittedSmoothActionCarriesOnlyItsExistingRowVersionAndSendsNothing() async throws {
        let versions: [Int64] = [12, 28, 29]
        for version in versions {
            let rig = Rig()
            try await rig.connect(version: version)
            let smooth = try rig.control("display.spectrumDefaults.smoothDefaults")
            let outcome = await rig.dispatcher.edit(smooth, in: "display", to: nil)
            #expect(outcome == .applied)
            #expect(rig.phone.actionVersions == [smooth.requiresDescriptionVersion ?? SetupDescription.highestVersion])
            #expect(rig.phone.ran == ["smoothDefaults"] && rig.route.sent.isEmpty)
        }
    }

    @Test func getMonitorHzWritesTheRefreshRateHeldToTheFpsRange() async throws {
        let rig = Rig()
        try await rig.connect()
        let monitor = try rig.control("display.spectrumDefaults.getMonitorHz")
        #expect(rig.dispatcher.state(of: monitor, in: "display").editable)
        // 120 Hz is held to the FPS row's 60, written as the Core's setting.
        let pressing = Task { await rig.dispatcher.edit(monitor, in: "display", to: nil) }
        try #require(await rig.route.waitForCount(1, unless: pressing))
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

    /// V26: Waterfall Defaults' level rows follow Clarity, Use spectrum
    /// min/max and AGC; a row is live only while every term holds.
    @Test func aV26LevelRowIsLiveOnlyWhileEveryTermHolds() async throws {
        let rig = Rig()
        rig.phone.values.merge(["ClarityEnabled": .bool(false), "DisplayWfAgc": .bool(true),
                                "DisplayWfLowLevel": .integer(-122), "WaterfallNFAGCEnabled": .bool(false),
                                "WaterfallAGCOffsetDb": .integer(0)]) { $1 }
        try await rig.connect(version: 26)
        #expect(rig.feed.description(for: "display")?.version == 26)
        let levels = ["display.waterfallDefaults.highThreshold", "display.waterfallDefaults.lowThreshold",
                      "display.waterfallDefaults.agc"]
        let underAgc = ["display.waterfallDefaults.nfAgc", "display.waterfallDefaults.nfAgcOffset"]
        let minMax = "display.waterfallDefaults.useSpectrumMinMax"
        func live(_ id: String) throws -> Bool {
            let state = rig.dispatcher.state(of: try rig.control(id), in: "display")
            #expect(state.editable == (state.reason == nil), "\(id)")
            #expect(state.editable || state.reason == SetupControlDispatcher.dependsReason, "\(id)")
            return state.editable
        }
        func expectLive(_ ids: [String], _ expected: Bool, _ what: String) throws {
            for id in ids { #expect(try live(id) == expected, "\(id): \(what)") }
        }
        // Clarity off, min/max off, AGC on: every row is live.
        try expectLive(levels + underAgc + [minMax], true, "all hold")
        let high = try rig.control("display.waterfallDefaults.highThreshold")
        #expect(await rig.dispatcher.edit(high, in: "display", to: .integer(-70)) == .applied)
        #expect(rig.phone.values["DisplayWfHighLevel"] == .integer(-70))
        // AGC off: only its NF-AGC rows wait.
        rig.phone.values["DisplayWfAgc"] = .bool(false)
        try expectLive(levels + [minMax], true, "AGC off")
        try expectLive(underAgc, false, "AGC off")
        rig.phone.values["DisplayWfAgc"] = .bool(true)
        // Use spectrum min/max on: the levels wait, the switch itself stays live.
        rig.phone.values["DisplayWfUseSpectrumMinMax"] = .bool(true)
        try expectLive(levels + underAgc, false, "min/max on")
        try expectLive([minMax], true, "min/max on")
        #expect(await rig.dispatcher.edit(high, in: "display", to: .integer(-60))
                == .notSent(SetupControlDispatcher.dependsReason))
        #expect(rig.phone.values["DisplayWfHighLevel"] == .integer(-70))
        rig.phone.values["DisplayWfUseSpectrumMinMax"] = .bool(false)
        // Clarity on: it sets the levels, so all six wait.
        rig.phone.values["ClarityEnabled"] = .bool(true)
        try expectLive(levels + underAgc + [minMax], false, "Clarity on")
        // A term whose key the phone does not keep does not hold.
        rig.phone.values["ClarityEnabled"] = nil
        try expectLive(levels + underAgc + [minMax], false, "Clarity not kept")
        rig.phone.values["ClarityEnabled"] = .bool(false)
        try expectLive(levels + underAgc + [minMax], true, "all hold again")
    }

    /// A phone at 26 with a Core that stops at 25 reads version 12's rows:
    /// the thresholds follow Use spectrum min/max alone.
    @Test func aCoreBelowTwentySixKeepsTheVersionTwelveDependency() async throws {
        let rig = Rig()
        rig.phone.values["ClarityEnabled"] = .bool(true)
        try await rig.connect(version: 25)
        #expect(rig.feed.description(for: "display")?.version == 12)
        let high = try rig.control("display.waterfallDefaults.highThreshold")
        #expect(high.phoneAllOf == nil)
        #expect(rig.dispatcher.state(of: high, in: "display").editable)
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


extension SetupControlDispatcherV12Tests {
    @Test func localTokenChoicesDecodeEncodeValidateAndNeverEchoToCore() async throws {
        let rig = Rig()
        rig.phone.values["DisplayWaterfallLevelsMode"] = .text("Clarity")
        rig.phone.values["DisplaySpectrumScaleMode"] = .text("Manual")
        try await rig.connect(version: 29)
        let levels = try rig.control("display.levels.waterfallMode")
        let scale = try rig.control("display.levels.scaleMode")
        #expect(rig.dispatcher.state(of: levels, in: "display").value == .integer(0))
        #expect(rig.dispatcher.state(of: scale, in: "display").value == .integer(0))
        #expect(rig.phone.writes.isEmpty && rig.route.sent.isEmpty)
        #expect(await rig.dispatcher.edit(levels, in: "display", to: .integer(4)) == .applied)
        #expect(rig.phone.values["DisplayWaterfallLevelsMode"] == .text("Scale"))
        #expect(rig.dispatcher.state(of: levels, in: "display").value == .integer(4))
        #expect(await rig.dispatcher.edit(scale, in: "display", to: .integer(1)) == .applied)
        #expect(rig.phone.values["DisplaySpectrumScaleMode"] == .text("TrackFloor"))
        #expect(rig.phone.writes.count == 2 && rig.route.sent.isEmpty)
        for invalid in [SetupValue.integer(-1), .integer(5), .text("Scale"), .decimal(1.5)] {
            #expect(await rig.dispatcher.edit(levels, in: "display", to: invalid)
                    == .notSent(SetupControlDispatcher.outOfRangeReason))
        }
        #expect(rig.phone.writes.count == 2 && rig.route.sent.isEmpty)
        rig.phone.values["DisplayWaterfallLevelsMode"] = .text("Unknown")
        #expect(rig.dispatcher.state(of: levels, in: "display").value == nil)
        rig.phone.values["DisplayWaterfallLevelsMode"] = .integer(0)
        #expect(rig.dispatcher.state(of: levels, in: "display").value == nil)
        #expect(rig.phone.writes.count == 2 && rig.route.sent.isEmpty)
    }

    @Test func legacyPhoneOptionsRemainIntegersAndNeverWriteTokens() async throws {
        let rig = Rig()
        try await rig.connect(version: 28, media: 1)
        let detector = try rig.control("display.spectrumDefaults.detector")
        #expect(detector.modern?.optionLiterals.isEmpty != false)
        #expect(await rig.dispatcher.edit(detector, in: "display", to: .integer(1)) == .applied)
        #expect(rig.phone.values["DisplaySpectrumDetector"] == .integer(1))
        #expect(rig.phone.writes.count == 1 && rig.route.sent.isEmpty)
        #expect(await rig.dispatcher.edit(detector, in: "display", to: .text("1"))
                == .notSent(SetupControlDispatcher.outOfRangeReason))
        #expect(rig.phone.writes.count == 1)
    }
}


extension SetupControlDispatcherV12Tests {
    @Test func v29LocalButtonsAreVisibleButRefuseEditsWhenTheirPhoneDependencyChanges() async throws {
        let rig = Rig()
        rig.phone.values["DisplayWfColorScheme"] = .integer(0)
        rig.phone.values["DisplayTxWfPalette"] = .integer(1)
        rig.phone.values["DisplayWaterfallLevelsMode"] = .text("Manual")
        try await rig.connect(version: 29)
        let receive = try rig.control("display.waterfall.customGradient")
        let transmit = try rig.control("display.transmit.customGradient")
        let retune = try rig.control("display.levels.retune")
        for control in [receive, transmit, retune] {
            #expect(!rig.dispatcher.state(of: control, in: "display").editable)
            #expect(await rig.dispatcher.edit(control, in: "display", to: nil)
                    == .notSent(SetupControlDispatcher.dependsReason))
        }
        #expect(rig.phone.ran.isEmpty && rig.route.sent.isEmpty)
        rig.phone.values["DisplayWfColorScheme"] = .integer(6)
        rig.phone.values["DisplayTxWfPalette"] = .integer(6)
        rig.phone.values["DisplayWaterfallLevelsMode"] = .text("Clarity")
        for control in [receive, transmit, retune] {
            #expect(rig.dispatcher.state(of: control, in: "display").editable)
            #expect(await rig.dispatcher.edit(control, in: "display", to: nil) == .applied)
        }
        #expect(rig.phone.ran == ["editReceiveGradient", "editTransmitGradient", "ClarityRetune"])
        #expect(rig.route.sent.isEmpty)
        guard case .success(let admission) = rig.dispatcher.admit(receive, in: "display") else {
            Issue.record("Custom gradient was not admitted"); return
        }
        rig.phone.values["DisplayWfColorScheme"] = .integer(0)
        #expect(await rig.dispatcher.perform(admission, value: nil)
                == .notSent(SetupControlDispatcher.changedFirstReason))
        #expect(rig.phone.ran.count == 3 && rig.route.sent.isEmpty)
    }
}


extension SetupControlDispatcherV12Tests {
    @Test func onlyCanonicalCurrentLevelsTransmitReasonsPrecedeMissingCapabilities() async throws {
        let cases = [("display.gridScales.noiseFloorOffset", "DisplayNFOffsetGridFollow", SetupValue.integer(2)),
                     ("display.gridScales.maintainGridRange", "DisplayMaintainNFAdjustDelta", SetupValue.bool(false))]
        let versions: [Int64] = [12, 28, 29]
        for version in versions {
            let rig = Rig()
            rig.phone.values["DisplaySpectrumScaleMode"] = .text("TrackFloor")
            rig.phone.values["DisplayAdjustGridMinToNoiseFloor"] = .bool(true)
            try await rig.connect(version: version, media: 1, extras: 2)
            for (id, key, value) in cases {
                rig.phone.values[key] = value
                let transmit = "Pan 1 is transmitting; this receive setting is kept."
                rig.phone.reasons[key] = transmit
                let row = try rig.control(id)
                let expected = version == 29 ? transmit : SetupControlDispatcher.needsNewerCoreReason
                #expect(rig.dispatcher.state(of: row, in: "display").reason == expected)
                let outcome = await rig.dispatcher.edit(row, in: "display", to: value)
                #expect(outcome == .notSent(expected))
                if version == 29 {
                    // Reuse the closed parser fixture: a forged field must
                    // fail metadata admission before any local context reason.
                    var forged = try SetupDescriptionV29Tests.raw(id)
                    forged["unexpected"] = true
                    let invalid = try SetupDescriptionV12Tests.single(forged, category: "display", version: 29)
                    #expect(invalid.metadataIssue != nil)
                    #expect(rig.dispatcher.state(of: invalid, in: "display").reason == SetupControlDispatcher.unreadableReason)
                    let invalidOutcome = await rig.dispatcher.edit(invalid, in: "display", to: value)
                    #expect(invalidOutcome == .notSent(SetupControlDispatcher.unreadableReason))
                }
                // Outside that local context (including All), the original
                // missing-capability gate must remain in force for every peer.
                rig.phone.reasons[key] = nil
                #expect(rig.dispatcher.state(of: row, in: "display").reason == SetupControlDispatcher.needsNewerCoreReason)
            }
            #expect(rig.phone.writes.isEmpty && rig.route.sent.isEmpty)
        }
    }
}
