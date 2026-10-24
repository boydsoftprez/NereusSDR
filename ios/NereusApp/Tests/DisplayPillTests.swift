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
            .setting(key: "DisplaySpectrumRenderMode"),
            .group("3D"),
            .setting(key: "Display3DFloor"),
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
        let off = source.state(of: threshold, key: "WaterfallBlankingThresholdDb")
        #expect(!off.editable)
        #expect(off.reason == "NB Blank is off, so there is no threshold to set.")
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

    // MARK: The compact lines

    /// Each tab's lines draw every row of its table exactly once.
    @Test(arguments: DisplayPillTab.allCases)
    func eachTabsLinesDrawEveryRowOnce(tab: DisplayPillTab) {
        let drawn = DisplayPillLayout.rowsDrawn(tab)
        let rows = DisplayPillRows.rows(tab)
        #expect(drawn.count == rows.count, "\(tab): drew \(drawn.count) of \(rows.count)")
        for row in rows {
            #expect(drawn.filter { $0 == row }.count == 1, "\(tab): \(row)")
        }
    }

    /// No repeated label: a choice under its group's caption shows no label
    /// of its own, and NB Blank and NB Threshold are drawn once each.
    @Test func aChoiceUnderItsCaptionIsNotNamedAgain() {
        let levels = DisplayPillLayout.lines(.levels)
        #expect(levels.contains(.segment(key: "SpectrumScaleMode", showsLabel: false)))
        #expect(levels.contains(.segment(key: "WaterfallLevelsMode", showsLabel: false)))
        let view = DisplayPillLayout.lines(.view)
        #expect(view.contains(.segment(key: "DisplaySpectrumRenderMode", showsLabel: false)))
        #expect(DisplayPillLayout.lines(.fft).first == .coreHead)
        #expect(DisplayPillLayout.lines(.fft).contains(.segment(key: "DisplayFftSizingMode", showsLabel: true)))
        let blanking = DisplayPillLayout.rowsDrawn(.waterfall).filter {
            $0 == .setting(key: "WaterfallBlankingEnabled") || $0 == .setting(key: "WaterfallBlankingThresholdDb")
        }
        #expect(blanking.count == 2)
    }

    @Test func dbMaxAndDbMinNameTheBandAndTheCore() {
        #expect(DisplayPillSource.label("dB Max:", perBand: "40m", onCore: true) == "dB Max \u{00B7} 40 m, on the Core")
        #expect(DisplayPillSource.label("dB Min:", perBand: "40m", onCore: true) == "dB Min \u{00B7} 40 m, on the Core")
        #expect(DisplayPillSource.label("3D Floor:", perBand: "40m", onCore: false) == "3D Floor \u{00B7} 40 m")
        #expect(DisplayPillSource.label("dB Max:", perBand: nil, onCore: true) == "dB Max")
        #expect(DisplayPillSource.label("Detector:", perBand: nil, onCore: false) == "Detector")
    }

    /// With every choice a row follows turned the other way, every pill row
    /// says why in the desktop pill's words, never Setup's "Needs a
    /// different choice above."
    @Test func everyGreyedRowSaysWhyInTheDesktopsWords() async throws {
        let (rig, source, defaults) = try await Self.pill()
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        for (key, value) in [("DisplaySpectrumDetector", SetupValue.integer(0)),
                             ("DisplaySpectrumAveraging", .integer(0)), ("DisplayPanFill", .bool(false)),
                             ("DisplayGridEnabled", .bool(false)), ("WaterfallBlankingEnabled", .bool(false)),
                             ("DisplayWaterfallAveraging", .integer(0)), ("DisplayBackgroundEnabled", .bool(false)),
                             ("DisplayPeakHoldEnabled", .bool(false))] {
            #expect(source.keys.set(value, forPhoneKey: key), "\(key)")
        }
        rig.main.changeDisplay {
            $0.gridFollowsNoiseFloor = false
            $0.spectrumView = .flat
        }
        func reason(_ key: String) throws -> String? {
            source.state(of: try #require(source.control(for: key)), key: key).reason
        }
        for mode in [BandDisplaySettings.WaterfallLevelMode.clarity, .agc, .noiseFloorAgc, .manual, .scale] {
            rig.main.changeDisplay { $0.waterfallLevelMode = mode }
            for tab in DisplayPillTab.allCases {
                for key in DisplayPillRows.settingKeys(tab) {
                    #expect(try reason(key) != SetupControlDispatcher.dependsReason, "\(mode) \(key)")
                }
            }
        }
        // From src/gui/SpectrumOverlayPanel.cpp updatePillEnables (2643-2830).
        #expect(try reason("DisplayDispNormalize") == "Normalize to 1 Hz needs the Average, Sample or RMS detector.")
        #expect(try reason("DisplaySpectrumAverageTimeMs") == "Averaging is None, so there is no time to set.")
        #expect(try reason("DisplayFftFillAlpha") == "Fill is off.")
        #expect(try reason("DisplayFreqGridSpacing") == "Grid is off, so no grid lines are drawn.")
        #expect(try reason("WaterfallBlankingThresholdDb") == "NB Blank is off, so there is no threshold to set.")
        #expect(try reason("DisplayWaterfallAverageTimeMs") == "WF Avg is None, so there is no time to set.")
        #expect(try reason("DisplayBackgroundOpacity") == "Background is off.")
        #expect(try reason("DisplayBackgroundColor") == "Background is off.")
        #expect(try reason("DisplayPeakHoldResetMs") == "Turn Peak Hold on to set how long a peak is held.")
        #expect(try reason("DisplayNFOffsetGridFollow") == "Floor Offset is used by Track floor. Pick Track floor to use it.")
        #expect(try reason("DisplayMaintainNFAdjustDelta") == "Keep range is used by Track floor. Pick Track floor to use it.")
        #expect(try reason("Display3DGain") == "The 2D view has no 3D surface. Pick 3D Stacked Trace to use this row.")
        #expect(try reason("DisplayWfLowLevel") == "Low is the spectrum's dB Min. Pick Manual to set Low here.")
        #expect(try reason("DisplayWfHighLevel") == "High is the spectrum's dB Max. Pick Manual to set High here.")
        #expect(try reason("ClarityRetune") == "Re-tune makes Clarity measure the noise floor again. Pick Clarity to use it.")
        #expect(try reason("WaterfallAGCOffsetDb") == "NF Offset is used by Floor. Pick Floor to use it.")
        rig.main.changeDisplay { $0.waterfallLevelMode = .clarity }
        #expect(try reason("DisplayWfLowLevel") == "Clarity is setting Low. Pick Manual to set it yourself.")
        #expect(try reason("DisplayWfHighLevel") == "Clarity is setting High. Pick Auto, Floor or Manual to set it yourself.")
        rig.main.changeDisplay { $0.waterfallLevelMode = .agc }
        #expect(try reason("DisplayWfLowLevel") == "Auto is setting Low. Pick Manual to set it yourself.")
        rig.main.changeDisplay {
            $0.gridFollowsNoiseFloor = true
            $0.gridKeepsRange = true
        }
        #expect(try reason("DisplayGridMin")
                == "Track floor sets dB Min: the noise floor plus Floor Offset. Pick Manual to set it yourself.")
        #expect(try reason("DisplayGridMax")
                == "Keep range is on, so dB Max moves with dB Min. Turn Keep range off to set it yourself.")
        let size = try #require(source.control(for: "DisplayFftSize"))
        if source.dispatcher.sizingMode != .fixed {
            #expect(source.state(of: size, key: "DisplayFftSize").reason == DisplayPillReasons.depends(
                "DisplayFftSize", settings: source.keys.settings, sizing: source.dispatcher.sizingMode))
        }
    }

    /// The 3D rows are greyed in the 2D view and refuse a change, as the
    /// desktop pill greys them; Peak Hold's time while Peak Hold is off.
    @Test func the3DRowsAreGreyedIn2DAndPeakHoldsTimeWhileItIsOff() async throws {
        let (rig, source, defaults) = try await Self.pill()
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        rig.main.changeDisplay {
            $0.spectrumView = .flat
            $0.peakHold = false
        }
        for key in ["Display3DFloor", "Display3DGain", "Display3DSpan", "Display3DAngle", "Display3DSpeed",
                    "Display3DSliceShadow"] {
            let state = source.state(of: try #require(source.control(for: key)), key: key)
            #expect(!state.editable, "\(key)")
            #expect(state.reason == DisplayPillReasons.only3D, "\(key)")
        }
        #expect(!source.keys.set(.integer(40), forPhoneKey: "Display3DGain"))
        let hold = try #require(source.control(for: "DisplayPeakHoldResetMs"))
        #expect(!source.state(of: hold, key: "DisplayPeakHoldResetMs").editable)
        rig.main.changeDisplay {
            $0.spectrumView = .stacked
            $0.peakHold = true
        }
        let gain = try #require(source.control(for: "Display3DGain"))
        #expect(source.state(of: gain, key: "Display3DGain").editable)
        #expect(source.state(of: hold, key: "DisplayPeakHoldResetMs").editable)
        #expect(source.keys.set(.integer(40), forPhoneKey: "Display3DGain"))
    }

    /// Floor Offset and Keep range need display extras 4: a Core that sends
    /// it (src/core/session/StationServer.cpp:4776-4784) gets the rows, one
    /// that sends 2 reads "Needs a newer Core".
    @Test func floorOffsetAndKeepRangeFollowTheCoresDisplayExtras() async throws {
        let json = try String(contentsOf: Self.setupFolder.appendingPathComponent("display.json"), encoding: .utf8)
        for (extras, newer) in [(Int64(4), false), (2, true)] {
            let rig = try Rig()
            let defaults = try #require(UserDefaults(suiteName: rig.suite))
            defer { defaults.removePersistentDomain(forName: rig.suite) }
            let source = try await Self.source(rig, json: json, version: 29, extras: extras)
            rig.main.changeDisplay { $0.gridFollowsNoiseFloor = false }
            for key in ["DisplayNFOffsetGridFollow", "DisplayMaintainNFAdjustDelta"] {
                let state = source.state(of: try #require(source.control(for: key)), key: key)
                #expect((state.reason == SetupControlDispatcher.needsNewerCoreReason) == newer,
                        "extras \(extras) \(key): \(state.reason ?? "nil")")
            }
        }
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

    static func source(_ rig: Rig, json: String, version: Int64,
                       extras: Int64 = 4) async throws -> DisplayPillSource {
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
            .init(name: "displayExtrasVersion", value: .i64(extras)),
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
