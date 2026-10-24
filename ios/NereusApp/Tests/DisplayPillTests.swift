// NereusSDR for iOS: the Display pill's tabs against the desktop's order, and its rows live on the pan's band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation
import NereusBand
import NereusLink
import NereusMirror
import Testing
@testable import NereusSDR

/// R-IOS-55: each tab lists the desktop pill's keys in the order its page
/// function builds them, every key is bound by the version 29 Display
/// description, and the rows draw from that description through the pill's
/// own dispatcher: live on the pan's band, Cal Offset and the Levels rows
/// greyed with the pill's words, the phone's rows working while the link
/// is lost.
@Suite("Display pill", .serialized)
@MainActor
struct DisplayPillTests {
    typealias Rig = DisplaySheetTests.Rig

    // MARK: The tables

    /// The brief's lists, from src/gui/SpectrumOverlayPanel.cpp's pill page
    /// functions (Spectrum :3152, Levels :3490, Waterfall :4018, View
    /// :4233, FFT :3659).
    static let expected: [DisplayPillTab: [DisplayPillRow]] = [
        .spectrum: [
            .group("Trace"),
            .setting(key: "DisplaySpectrumDetector"), .setting(key: "DisplayDispNormalize"),
            .setting(key: "DisplayPanFill"), .setting(key: "DisplayGradientEnabled"),
            .setting(key: "DisplayFillColor"), .setting(key: "DisplayFftFillAlpha"),
            .setting(key: "DisplaySpectrumAveraging"), .setting(key: "DisplaySpectrumAverageTimeMs"),
            .setting(key: "DisplayTraceColor"), .setting(key: "DisplayLineWidth"),
            .setting(key: "DisplayPeakHoldEnabled"), .setting(key: "DisplayPeakHoldResetMs"),
            .phone(.spectrumHeight), .setting(key: "DisplayCalOffset"),
            .group("Grid"),
            .setting(key: "DisplayGridEnabled"), .setting(key: "DisplayFreqGridSpacing"),
            .setting(key: "DisplayFreqScaleFontPx"),
            .group("Markers"),
            .setting(key: "DisplayShowNoiseFloor"), .setting(key: "DisplayShowBinWidth"),
            .setting(key: "DisplayShowPeakValueOverlay"), .setting(key: "DisplayShowCursorFreq"),
            .link(title: "Peaks & Markers in Setup \u{203A}", pageId: "display.spectrumPeaks"),
        ],
        .levels: [
            .group("Spectrum scale"),
            .setting(key: "SpectrumScaleMode"), .setting(key: "DisplayGridMax"), .setting(key: "DisplayGridMin"),
            .setting(key: "DisplayNFOffsetGridFollow"), .setting(key: "DisplayMaintainNFAdjustDelta"),
            .group("Waterfall levels"),
            .setting(key: "WaterfallLevelsMode"), .setting(key: "ClarityRetune"),
            .setting(key: "DisplayWfBlackLevel"), .setting(key: "DisplayWfColorGain"),
            .setting(key: "WaterfallAGCOffsetDb"), .setting(key: "DisplayWfLowLevel"),
            .setting(key: "DisplayWfHighLevel"),
        ],
        .waterfall: [
            .group("Drawing"),
            .setting(key: "DisplayWfColorScheme"), .setting(key: "DisplayWfUpdatePeriodMs"),
            .setting(key: "DisplayWfOpacity"), .setting(key: "WaterfallBlankingEnabled"),
            .setting(key: "WaterfallBlankingThresholdDb"), .setting(key: "DisplayWaterfallAveraging"),
            .setting(key: "DisplayWaterfallAverageTimeMs"),
            .group("Overlays"),
            .setting(key: "WaterfallStopOnTx"), .setting(key: "DisplayShowRxFilterOnWaterfall"),
            .setting(key: "DisplayShowTxFilterOnRxWaterfall"), .setting(key: "DisplayShowRxZeroLine"),
            .setting(key: "DisplayShowTxZeroLine"),
            .group("Time"),
            .setting(key: "DisplayWfTimestampPos"), .setting(key: "DisplayWaterfallHistoryMs"),
            .phone(.lookBack),
        ],
        .view: [
            .group("View"),
            .setting(key: "DisplaySpectrumRenderMode"), .setting(key: "Display3DFloor"),
            .setting(key: "Display3DGain"), .setting(key: "Display3DSpan"), .setting(key: "Display3DAngle"),
            .setting(key: "Display3DSpeed"), .setting(key: "Display3DSliceShadow"),
            .group("Background"),
            .setting(key: "DisplayBackgroundImage"), .setting(key: "DisplayBackgroundEnabled"),
            .setting(key: "DisplayBackgroundOpacity"), .setting(key: "DisplayBackgroundColor"),
            .group("On this phone"),
            .phone(.ctun), .phone(.dup),
        ],
        .fft: [
            .group("FFT"),
            .setting(key: "DisplayFftSizingMode"), .setting(key: "DisplayFftSize"),
            .setting(key: "DisplayHzPerBinTarget"), .fftReadout,
            .setting(key: "DisplaySpectrumFps"), .setting(key: "DisplayFftWindow"),
            .group("On this phone"),
            .phone(.decimation),
        ],
    ]

    @Test func theTabsAreTheDesktopsFiveWithTheirSetupPages() throws {
        #expect(DisplayPillTab.allCases.map(\.title) == ["Spectrum", "Levels", "Waterfall", "View", "FFT"])
        #expect(DisplayPillTab.allCases.map(\.setupPageId)
                == ["display.spectrumDefaults", "display.levels", "display.waterfallDefaults",
                    "display.threeD", "display.fftEngine"])
        let description = try Self.description()
        for tab in DisplayPillTab.allCases {
            #expect(description.pages.contains { $0.id == tab.setupPageId }, "no page \(tab.setupPageId)")
        }
        #expect(description.pages.contains { $0.id == DisplayPillRows.peaksPageId })
    }

    @Test(arguments: DisplayPillTab.allCases)
    func eachTabListsTheDesktopsKeysInItsOrder(tab: DisplayPillTab) {
        #expect(DisplayPillRows.rows(tab) == Self.expected[tab])
    }

    @Test(arguments: DisplayPillTab.allCases)
    func everyKeyIsBoundByTheVersion29Description(tab: DisplayPillTab) throws {
        let description = try Self.description()
        #expect(description.version == 29)
        for key in DisplayPillRows.settingKeys(tab) {
            #expect(DisplayPillRows.control(for: key, in: description) != nil,
                    "the version 29 Display description binds no control to \(key)")
        }
    }

    // MARK: The rows

    @Test func calOffsetIsTheCoresAndTheNoiseBlankerThresholdFollowsItsSwitch() async throws {
        let (rig, source, defaults) = try await Self.pill()
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let cal = try #require(source.control(for: "DisplayCalOffset"))
        let state = source.dispatcher.state(of: cal, in: "display")
        #expect(!state.editable)
        #expect(state.reason == "The Core calibrates for its radio.")

        rig.main.changeDisplay { $0.waterfallBlanking = false }
        let blanking = try #require(source.control(for: "WaterfallBlankingEnabled"))
        let threshold = try #require(source.control(for: "WaterfallBlankingThresholdDb"))
        let off = source.dispatcher.state(of: threshold, in: "display")
        #expect(!off.editable)
        #expect(off.reason == SetupControlDispatcher.dependsReason)
        // NB Blank on from the Waterfall tab: Pan 1's own value, and the
        // threshold row takes changes.
        #expect(await source.dispatcher.edit(blanking, in: "display", to: .bool(true)) == .applied)
        #expect(rig.store.settings(forPan: PanIdentity.first.storeKey).waterfallBlanking)
        #expect(rig.main.band.settings.waterfallBlanking)
        #expect(source.dispatcher.state(of: threshold, in: "display").editable)
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    @Test func aChangeIsLiveOnThePansOwnValuesEvenFromAll() async throws {
        let (rig, source, defaults) = try await Self.pill()
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let fill = try #require(source.control(for: "DisplayPanFill"))
        let before = rig.store.settings(forPan: PanIdentity.first.storeKey).traceFill
        // Setup was left on All; the pill acts on its own pan.
        rig.main.displayEditScope = .all
        #expect(source.keys.set(.bool(!before), forPhoneKey: "DisplayPanFill"))
        // Same turn: the store's value for the pan has changed already.
        #expect(rig.store.settings(forPan: PanIdentity.first.storeKey).traceFill == !before)
        #expect(rig.main.displayEditScope == .pan(.first))
        #expect(await source.dispatcher.edit(fill, in: "display", to: .bool(before)) == .applied)
        #expect(rig.store.settings(forPan: PanIdentity.first.storeKey).traceFill == before)
        #expect(rig.main.band.settings.traceFill == before)
    }

    @Test func everyLevelsRowIsGreyedWhileThePanTransmits() async throws {
        let (rig, source, defaults) = try await Self.pill()
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        rig.mirror.apply(.delta(.init(key: "txState", properties: [.init(ordinal: 0, name: "keyed", value: .bool(true))])))
        for _ in 0..<100 {
            if rig.main.band.transmitOverlay { break }
            try await Task.sleep(for: .milliseconds(2))
        }
        try #require(rig.main.band.transmitOverlay)
        let kept = rig.store.settings(forPan: PanIdentity.first.storeKey)
        for key in DisplayPillRows.settingKeys(.levels) {
            let control = try #require(source.control(for: key))
            let state = source.dispatcher.state(of: control, in: "display")
            #expect(!state.editable, "\(key)")
            #expect(state.reason == "Transmit has its own levels, on Transmit Display.", "\(key): \(state.reason ?? "nil")")
        }
        #expect(!source.keys.set(.integer(25), forPhoneKey: "DisplayWfBlackLevel"))
        #expect(!source.keys.perform("ClarityRetune"))
        #expect(rig.store.settings(forPan: PanIdentity.first.storeKey) == kept)
        // Another tab's row still changes.
        let fill = try #require(source.control(for: "DisplayPanFill"))
        #expect(source.dispatcher.state(of: fill, in: "display").editable)
    }

    @Test func withTheLinkLostTheCoresRowsSayNotConnectedAndThePhonesStillWork() async throws {
        let (rig, source, defaults) = try await Self.pill()
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        rig.mirror.handle(.stateChanged(.ready))
        rig.settings.handle(.stateChanged(.ready))
        rig.mirror.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        rig.settings.handle(.stateChanged(.waitingToRetry(seconds: 1)))
        try #require(!rig.mirror.isSnapshotComplete)
        source.pages.refresh()
        // The pill keeps its last description and values.
        #expect(source.description != nil)
        for key in DisplayPillRows.settingKeys(.fft) + ["DisplayGridMax", "DisplayGridMin"] {
            let control = try #require(source.control(for: key))
            let state = source.dispatcher.state(of: control, in: "display")
            #expect(!state.editable, "\(key)")
            #expect(state.reason == "Not connected", "\(key): \(state.reason ?? "nil")")
        }
        let fill = try #require(source.control(for: "DisplayPanFill"))
        #expect(source.dispatcher.state(of: fill, in: "display").editable)
        let before = rig.store.settings(forPan: PanIdentity.first.storeKey).traceFill
        #expect(await source.dispatcher.edit(fill, in: "display", to: .bool(!before)) == .applied)
        #expect(rig.store.settings(forPan: PanIdentity.first.storeKey).traceFill == !before)
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    @Test func anOlderCoreKeepsTodaysSheetAndAnUndescribedRowNeedsANewerCore() async throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let json = try String(contentsOf: Self.setupFolder.appendingPathComponent("legacy/display-v28.json"), encoding: .utf8)
        let source = try await Self.source(rig, json: json, version: 28)
        #expect(source.pages.categories["display"] != nil)
        #expect(source.description == nil)
        #expect(source.control(for: "DisplayPanFill") == nil)
        #expect(DisplayDropSheet.olderCoreNote == "Needs a newer Core for the desktop's Display tabs")
        #expect(SetupControlDispatcher.needsNewerCoreReason == "Needs a newer Core")
    }

    // MARK: Inside

    static var setupFolder: URL {
        URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent().deletingLastPathComponent().appendingPathComponent("resources/setup")
    }

    static func description() throws -> SetupDescription {
        try SetupDescription.parse(json: String(contentsOf: setupFolder.appendingPathComponent("display.json"),
                                                encoding: .utf8))
    }

    /// A pill over the rig's main screen, its Core describing Display at
    /// version 29, off the air.
    static func pill() async throws -> (Rig, DisplayPillSource, UserDefaults) {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        let json = try String(contentsOf: setupFolder.appendingPathComponent("display.json"), encoding: .utf8)
        let source = try await source(rig, json: json, version: 29)
        try #require(source.description != nil)
        return (rig, source, defaults)
    }

    static func source(_ rig: Rig, json: String, version: Int64) async throws -> DisplayPillSource {
        let feed = SetupDescriptionFeed(store: rig.mirror)
        let pages = SetupDescribedPages(feed: feed, store: rig.mirror, hello: Just(nil).eraseToAnyPublisher())
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        let source = DisplayPillSource(pages: pages, store: rig.mirror, feed: feed, settings: rig.settings,
                                       commands: CommandClient(send: { _ in }), captureSender: { nil },
                                       selectedSlice: { 0 }, selectionChanges: Just(Optional(0)).eraseToAnyPublisher(),
                                       main: rig.main, inner: PhoneSetupKeys(main: rig.main, defaults: defaults))
        rig.mirror.apply(.capabilities(.init(properties: [
            .init(name: "setupDescriptionVersion", value: .i64(version)),
            .init(name: "remoteMediaVersion", value: .i64(1)),
            .init(name: "remoteWidebandDisplayVersion", value: .i64(1)),
            .init(name: "displayExtrasVersion", value: .i64(4)),
            .init(name: "stationCatalogVersion", value: .i64(1)),
            .init(name: "txStateVersion", value: .i64(2)),
        ])))
        rig.mirror.apply(.schema(.init(className: "SetupDescription", fields: [
            .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "display", kind: .utf8),
        ])))
        rig.mirror.apply(.objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
            .init(ordinal: 0, name: "revision", value: .i64(1)), .init(ordinal: 1, name: "display", value: .utf8(json)),
        ])))
        rig.mirror.apply(.objectCreate(.init(key: "txState", className: "TransmitState", properties: [
            .init(ordinal: 0, name: "keyed", value: .bool(false)),
            .init(ordinal: 3, name: "txSliceId", value: .i64(0)),
        ])))
        rig.mirror.apply(.snapshotComplete)
        for _ in 0..<100 {
            if feed.description(for: "display") != nil { break }
            try await Task.sleep(for: .milliseconds(2))
        }
        _ = try #require(feed.description(for: "display"))
        pages.refresh()
        return source
    }
}
