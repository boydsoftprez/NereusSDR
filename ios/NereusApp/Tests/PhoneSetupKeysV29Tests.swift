// NereusSDR for iOS: local V29 phone carriers and measured-floor boundary
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// 2026-10-12: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

import Combine
import CoreGraphics
import Foundation
import NereusBand
import NereusLink
import NereusMedia
import NereusMirror
import Testing
import UIKit
@testable import NereusSDR

@MainActor
@Suite("V29 phone carriers", .serialized)
struct PhoneSetupKeysV29Tests {
    typealias Rig = DisplaySheetTests.Rig

    @Test func localModeTokensUseCoherentFlagsAndNeverWriteAStationSetting() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        #expect(keys.value(forPhoneKey: "DisplayWaterfallLevelsMode") == .text("Clarity"))
        for token in ["Auto", "Floor", "Manual", "Scale", "Clarity"] {
            #expect(keys.set(.text(token), forPhoneKey: "DisplayWaterfallLevelsMode"))
            #expect(keys.value(forPhoneKey: "DisplayWaterfallLevelsMode") == .text(token))
            if token == "Floor" {
                #expect(rig.main.band.settings.waterfallAgc && rig.main.band.settings.waterfallNfAgc)
            }
        }
        let held = rig.main.band.settings
        #expect(!keys.set(.text("Unknown"), forPhoneKey: "DisplayWaterfallLevelsMode"))
        #expect(!keys.set(.integer(0), forPhoneKey: "DisplayWaterfallLevelsMode"))
        #expect(rig.main.band.settings == held)
        #expect(keys.value(forPhoneKey: "DisplaySpectrumScaleMode") == .text("Manual"))
        #expect(keys.set(.text("TrackFloor"), forPhoneKey: "DisplaySpectrumScaleMode"))
        #expect(rig.main.band.settings.gridFollowsNoiseFloor)
        #expect(keys.value(forPhoneKey: "DisplaySpectrumScaleMode") == .text("TrackFloor"))
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    @Test func newNumericCarriersUsePixelWidthCurrentTrimsAndDerivedTransmitMin() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        let scale = Double(UIScreen.main.scale)
        let points = rig.main.band.settings.traceWidthPoints
        #expect(keys.value(forPhoneKey: "DisplayLineWidth")?.number == points * scale)
        #expect(keys.set(.decimal(1.2), forPhoneKey: "DisplayLineWidth"))
        #expect(abs(rig.main.band.settings.traceWidthPoints - 1.2 / scale) < 0.001)
        #expect(keys.value(forPhoneKey: "DisplayWfBlackLevel") == .integer(57))
        #expect(keys.value(forPhoneKey: "DisplayWfColorGain") == .integer(60))
        #expect(keys.set(.integer(80), forPhoneKey: "DisplayWfBlackLevel"))
        #expect(keys.set(.integer(35), forPhoneKey: "DisplayWfColorGain"))
        #expect(rig.main.band.settings.waterfallBlackLevel == 80 && rig.main.band.settings.waterfallColorGain == 35)
        #expect(!keys.set(.integer(104), forPhoneKey: "DisplayWfBlackLevel"))
        #expect(keys.value(forPhoneKey: "DisplayTxGridMin") == .integer(-80))
        #expect(keys.set(.integer(-60), forPhoneKey: "DisplayTxGridMin"))
        #expect(rig.main.band.settings.txGridTopDbm == 20 && rig.main.band.settings.txGridRangeDb == 80)
        #expect(keys.value(forPhoneKey: "DisplayTxGridMin") == .integer(-60))
        #expect(keys.unavailableReason(forPhoneKey: "DisplayThreadPriority")?.isEmpty == false)
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    @Test func describedCustomSixSelectsTheLocalPaletteWithoutCoreGradientTransport() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        for key in ["DisplayWfColorScheme", "DisplayTxWfPalette"] {
            #expect(keys.unavailableOptions(forPhoneKey: key)[6] == nil)
            #expect(keys.set(.integer(6), forPhoneKey: key))
            #expect(keys.value(forPhoneKey: key) == .integer(6))
        }
        #expect(rig.main.band.settings.waterfallPaletteId == BandPalette.customPaletteId)
        #expect(rig.main.band.settings.txWaterfallPaletteId == BandPalette.customPaletteId)
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    @Test func seededExtrasArePendingAndOnlyTheFullSourceEventDrivesClarityLevels() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let band = rig.main.band
        var now = 100.0
        band.clock = { now }
        band.endpointId = 1
        let context = try #require(TuningShotTests.context(centreHz: 7_236_400, spanHz: 48_000))
        band.receive(.context(context))
        rig.main.changeDisplay {
            $0.waterfallLevelMode = .clarity
            $0.waterfallBlackLevel = 57
            $0.waterfallColorGain = 60
        }
        func draw(_ sequence: UInt32, displayFloor: Float? = nil) {
            band.receive(.displayFrame(BandFixturesApp.frame(level: -100, sequence: sequence)))
            band.receive(.displayExtras(DisplayExtras(endpointId: 1, contextGeneration: 1,
                encoderSequence: sequence, noiseFloorDbm: displayFloor,
                waterfallLevels: .init(lowDbm: -122, highDbm: -62))))
        }
        draw(1, displayFloor: -20)
        // These are the seeded Core pair, not a measured Clarity floor.
        #expect(band.state.history.currentLevels == .init(lowDbm: -122, highDbm: -62))
        let manual = band.settings.manualLevels
        func floor(_ dbm: Double, endpoint: Double = 1) throws {
            let payload: [String: LinkJSON] = ["op": .string("noise-floor"), "connectionId": .string("c"),
                "endpointId": .number(endpoint), "revision": .number(Double(context.revision)),
                "contextGeneration": .number(1), "floorDbm": .number(dbm)]
            band.receive(.noiseFloor(try #require(MediaControlDecoder.noiseFloor(payload))))
        }
        try floor(-120)
        draw(2, displayFloor: -20)
        let measured = try #require(band.state.history.currentLevels)
        #expect(abs(measured.lowDbm - (-116.5)) < 0.001)
        #expect(abs(measured.highDbm - (-78.5)) < 0.001)
        try floor(-80, endpoint: 2)
        draw(3, displayFloor: -10)
        #expect(band.state.history.currentLevels == measured)
        #expect(band.settings.manualLevels == manual)
        now += 1
        try floor(-110)
        draw(4)
        let smoothed = -120 + (1 - exp(-1.0 / 3.0)) * 10
        let next = try #require(band.state.history.currentLevels)
        #expect(abs(Double(next.lowDbm) - (smoothed + 3.5)) < 0.001)
        #expect(band.settings.manualLevels == manual)
    }
}


extension PhoneSetupKeysV29Tests {
    @Test func nativeChoicesMatchTheFiveClosedTokensAndNewBlackRange() {
        #expect(DisplaySheetModel.Level.allCases.map(\.label) == ["Clarity", "Auto", "Floor", "Manual", "Scale"])
        #expect(DisplayOnThisPhonePage.levelModes.map(\.title) == ["Clarity", "Auto", "Floor", "Manual", "Scale"])
        #expect(DisplayOnThisPhonePage.blackLevelRange == 0...100)
        #expect(BandDisplaySettings.blackLevelRange == 0...100)
    }
}

extension PhoneSetupKeysV29Tests {
    @Test func remainingPictureCarriersHaveRealDefaultsAndLocalRoundTrips() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        let cases: [(String, SetupValue, SetupValue)] = [
            ("DisplayFreqGridSpacing", .integer(0), .integer(5000)),
            ("DisplayFreqScaleFontPx", .integer(10), .integer(12)),
            ("Display3DSpeed", .integer(0), .integer(3)),
            ("WaterfallBlankingEnabled", .bool(false), .bool(true)),
            ("WaterfallBlankingThresholdDb", .integer(6), .integer(9)),
            ("DisplayBackgroundEnabled", .bool(false), .bool(true)),
            ("DisplayBackgroundColor", .text("#0A0A14FF"), .text("#102030FF")),
            ("DisplayBackgroundOpacity", .integer(80), .integer(60)),
        ]
        for (key, start, changed) in cases {
            let before = keys.value(forPhoneKey: key)
            #expect(before == start, "\(key)")
            let accepted = keys.set(changed, forPhoneKey: key)
            #expect(accepted, "\(key)")
            let after = keys.value(forPhoneKey: key)
            #expect(after == changed, "\(key)")
        }
        for (key, bad) in [("Display3DSpeed", 11), ("DisplayFreqScaleFontPx", 15),
                           ("DisplayShowNoiseFloorPosition", 4), ("WaterfallBlankingThresholdDb", 21)] {
            let accepted = keys.set(.integer(Int64(bad)), forPhoneKey: key)
            #expect(!accepted, "\(key)")
        }
        #expect(rig.recorded.settingsSent.isEmpty)
        #expect(keys.value(forPhoneKey: "DisplayShowNoiseFloorPosition") == .integer(2))
        let unusedPositionEdit = keys.set(.integer(1), forPhoneKey: "DisplayShowNoiseFloorPosition")
        #expect(!unusedPositionEdit)
        #expect(keys.value(forPhoneKey: "DisplayShowNoiseFloorPosition") == .integer(2))
        #expect(keys.unavailableReason(forPhoneKey: "DisplayShowNoiseFloorPosition") == "Not used: the noise floor text sits beside its box. The setting is kept but has no visual effect.")
        #expect(keys.value(forPhoneKey: "DisplayCalOffset") == .decimal(0))
        #expect(!keys.set(.decimal(1), forPhoneKey: "DisplayCalOffset"))
        #expect(keys.unavailableReason(forPhoneKey: "DisplayCalOffset") == BandDisplaySettings.calibrationReason)
    }

    @Test func localGradientActionsRemainDistinctAndCheckTheSelectedLocalCustomMode() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        let actionResult1 = keys.canPerform("editReceiveGradient")
        #expect(!actionResult1)
        let actionResult2 = keys.canPerform("editTransmitGradient")
        #expect(!actionResult2)
        let actionResult3 = keys.perform("editReceiveGradient")
        #expect(!actionResult3)
        let actionResult4 = keys.set(.integer(6), forPhoneKey: "DisplayWfColorScheme")
        #expect(actionResult4)
        let actionResult5 = keys.canPerform("editReceiveGradient")
        #expect(actionResult5)
        let actionResult6 = keys.canPerform("editTransmitGradient")
        #expect(!actionResult6)
        let actionResult7 = keys.perform("editReceiveGradient")
        #expect(actionResult7)
        let actionResult8 = keys.set(.integer(6), forPhoneKey: "DisplayTxWfPalette")
        #expect(actionResult8)
        let actionResult9 = keys.canPerform("editTransmitGradient")
        #expect(actionResult9)
        let actionResult10 = keys.perform("editTransmitGradient")
        #expect(actionResult10)
        #expect(rig.recorded.settingsSent.isEmpty)
    }
}


extension PhoneSetupKeysV29Tests {
    @Test func actualAllContextReadsMasterAndSameValueEditsClearClosedOwnOnlyForThatField() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        let scale = Double(UIScreen.main.scale)
        var closed = rig.store.settings(forPan: "27")
        closed.traceWidthPoints = 1.4
        closed.traceFill = false
        rig.store.setSettings(closed, forPan: "27")
        let ownAccepted = keys.set(.decimal(1.2), forPhoneKey: "DisplayLineWidth")
        #expect(ownAccepted)
        rig.main.displayEditScope = .all
        let masterWidth = keys.value(forPhoneKey: "DisplayLineWidth")?.number
        #expect(masterWidth == 0.5 * scale)
        let allAccepted = keys.set(.decimal(0.5 * scale), forPhoneKey: "DisplayLineWidth")
        #expect(allAccepted)
        #expect(rig.store.settings(forPan: "27").traceWidthPoints == 0.5)
        #expect(!rig.store.settings(forPan: "27").traceFill)
        #expect(rig.main.band.settings.traceWidthPoints == 0.5)
        #expect(rig.store.ownFields(forPan: "1", among: ["traceWidthPoints"]).isEmpty)
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    @Test func actualPanContextOwnMarkerClearsOnEqualityAndReturnsToNativePanValues() throws {
        let rig = try Rig()
        defer { UserDefaults(suiteName: rig.suite)?.removePersistentDomain(forName: rig.suite) }
        rig.main.changeDisplay(fields: ["traceFill"]) { $0.traceFill = false }
        #expect(rig.main.ownDisplayFields(among: ["traceFill", "txGridTopDbm"]) == ["traceFill"])
        rig.main.displayEditScope = .all
        #expect(rig.main.displayEditingSettings.traceFill)
        #expect(!rig.main.band.settings.traceFill) // Drawing remains the effective focusedPan.
        rig.main.displayEditScope = .pan(.first)
        #expect(!rig.main.displayEditingSettings.traceFill)
        rig.main.changeDisplay(fields: ["traceFill"]) { $0.traceFill = true }
        #expect(rig.main.ownDisplayFields(among: ["traceFill"]).isEmpty)
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    @Test func actualPageCopyAndResetExcludeBandAndSharedFieldsAndRefreshTheFocusedRuntime() throws {
        let rig = try Rig()
        defer { UserDefaults(suiteName: rig.suite)?.removePersistentDomain(forName: rig.suite) }
        rig.main.changeDisplay {
            $0.traceFill = false; $0.backgroundEnabled = true
            $0.txGridTopDbm = 35; $0.scaleTopDbm = -30
        }
        let selected: Set<String> = ["traceFill", "txGridTopDbm", "scaleTopDbm", "customPalette"]
        let copied = rig.main.copyOwnDisplayPictureToMaster(fields: selected)
        #expect(copied)
        #expect(!rig.store.masterSettings(forPan: "27").traceFill)
        #expect(rig.main.ownDisplayFields(among: ["traceFill"]).isEmpty)
        #expect(rig.main.ownDisplayFields(among: ["backgroundEnabled"]) == ["backgroundEnabled"])
        rig.main.resetDisplayPicture(fields: ["backgroundEnabled", "txGridTopDbm", "scaleTopDbm"])
        #expect(!rig.main.band.settings.backgroundEnabled)
        #expect(rig.main.band.settings.txGridTopDbm == 35 && rig.main.band.settings.scaleTopDbm == -30)
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    @Test func actualSaveRefreshesItsReadSnapshotAndLaterMasterChangesDoNotGetOverwritten() throws {
        let rig = try Rig()
        defer { UserDefaults(suiteName: rig.suite)?.removePersistentDomain(forName: rig.suite) }
        let before = rig.main.band.settings.storeSnapshotVersion.revision
        rig.main.changeDisplay { $0.traceFill = false }
        #expect(rig.main.band.settings.storeSnapshotVersion.revision > before)
        var master = rig.store.masterSettings(forPan: "1")
        master.traceWidthPoints = 0.8
        rig.store.setMasterSettings(master, fields: ["traceWidthPoints"])
        rig.main.changeDisplay { $0.backgroundEnabled = true }
        #expect(rig.main.band.settings.traceWidthPoints == 0.8)
        #expect(rig.main.band.settings.backgroundEnabled)
        #expect(rig.store.settings(forPan: "1").traceWidthPoints == 0.8)
    }

    @Test func allContextHasNoUniqueEffectiveNumbersAndTrackFloorGatesActualActions() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        rig.main.changeDisplay { $0.waterfallLevelMode = .manual }
        #expect(rig.main.displayEffectiveValue(.waterfallLow) != nil)
        #expect(rig.main.displayEffectiveValue(.spectrumMin) != nil)
        rig.main.displayEditScope = .all
        for readout in [SetupDescription.EffectiveReadout.waterfallLow, .waterfallHigh, .spectrumMax, .spectrumMin] {
            #expect(rig.main.displayEffectiveValue(readout) == nil)
        }
        rig.main.displayEditScope = .pan(.first)
        rig.main.changeDisplay { $0.gridFollowsNoiseFloor = true; $0.gridKeepsRange = false }
        #expect(keys.unavailableReason(forPhoneKey: "DisplayGridMin")?.isEmpty == false)
        let minAccepted = keys.set(.integer(-90), forPhoneKey: "DisplayGridMin")
        #expect(!minAccepted)
        #expect(keys.unavailableReason(forPhoneKey: "DisplayGridMax") == nil)
        rig.main.changeDisplay { $0.gridKeepsRange = true }
        #expect(keys.unavailableReason(forPhoneKey: "DisplayGridMax")?.isEmpty == false)
        let maxAccepted = keys.set(.integer(-35), forPhoneKey: "DisplayGridMax")
        #expect(!maxAccepted)
        rig.main.changeDisplay { $0.waterfallLevelMode = .clarity }
        #expect(rig.main.displayEffectiveValue(.spectrumMin) == nil) // No valid full-source floor yet.
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    @Test func version29TreeKeepsNativeEntryThenAllEightCanonicalPagesAndLegacyTreeKeepsItsNames() throws {
        let folder = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent().deletingLastPathComponent().appendingPathComponent("resources/setup")
        let live = try SetupDescription.parse(json: String(contentsOf: folder.appendingPathComponent("display.json"), encoding: .utf8))
        let tree = SetupTree.categories(described: ["display": live], order: ["display"], unreadable: [:], stackOffered: true)
        let display = try #require(tree.first { $0.title == "Display" })
        #expect(display.pages.map(\.title) == ["On this phone", "Spectrum", "Levels", "Waterfall", "View", "Peaks & Markers", "FFT & Engine", "Transmit Display", "Multimeter"])
        let frozen = try SetupDescription.parse(json: String(contentsOf: folder.appendingPathComponent("legacy/display-v28.json"), encoding: .utf8))
        let legacy = SetupTree.categories(described: ["display": frozen], order: ["display"], unreadable: [:], stackOffered: true)
        #expect(legacy.first { $0.title == "Display" }?.pages.map(\.title) == ["On this phone", "Spectrum Defaults", "Spectrum Peaks", "Waterfall Defaults", "Grid & Scales", "Multimeter", "TX Display", "3D View"])
    }
}


extension PhoneSetupKeysV29Tests {
    // Each band's dB Max and dB Min are the Core's now (CoreBandScale); on
    // this older Core the phone keeps one scale for every band, so All's
    // band choice edits that one scale, and 3D Floor still per band.
    @Test func allBandPickerEditsAnInactiveKeptBandWithoutRetuningOrChangingTheLivePicture() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        rig.main.changeDisplay { $0.enterBand("7"); $0.scaleTopDbm = -30; $0.scaleBottomDbm = -130; $0.threeDFloorDb = 8 }
        rig.main.band.endpointId = 1
        let context = try #require(TuningShotTests.context(centreHz: 7_236_400, spanHz: 48_000))
        rig.main.band.receive(.context(context))
        rig.main.band.receive(.displayFrame(BandFixturesApp.frame(level: -100, sequence: 1)))
        _ = rig.main.band.prepareToDraw(size: CGSize(width: 1206, height: 1500), scale: 3)
        let live = rig.main.band.settings
        let view = rig.main.band.requestedView
        let frame = try #require(rig.main.band.state.frame)
        let vfo = rig.mirror.object("slice:0")?["frequency"]
        let subscriptions = rig.sent.count
        rig.main.displayEditScope = .all
        rig.main.displayBandChoice = "5"
        #expect(rig.main.displayEditingBand == "5")
        #expect(keys.value(forPhoneKey: "DisplayGridMax") == .integer(-30))
        #expect(keys.unavailableReason(forPhoneKey: "DisplayGridMax") == nil)
        let maxAccepted = keys.set(.integer(-52), forPhoneKey: "DisplayGridMax")
        let minAccepted = keys.set(.integer(-142), forPhoneKey: "DisplayGridMin")
        let floorAccepted = keys.set(.integer(11), forPhoneKey: "Display3DFloorDepth")
        #expect(maxAccepted && minAccepted && floorAccepted)
        var stored = rig.store.settings(forPan: "1")
        stored.enterBand("5")
        #expect(stored.scaleTopDbm == -52 && stored.scaleBottomDbm == -142 && stored.threeDFloorDb == 11)
        stored.enterBand("7")
        #expect(stored.scaleTopDbm == -52 && stored.scaleBottomDbm == -142 && stored.threeDFloorDb == 8)
        #expect(rig.main.band.settings.scaleBand == live.scaleBand)
        #expect(rig.main.band.settings.scaleTopDbm == -52 && rig.main.band.settings.scaleBottomDbm == -142)
        #expect(rig.main.band.settings.threeDFloorDb == live.threeDFloorDb)
        #expect(rig.main.band.state.frame == frame && rig.main.band.requestedView == view)
        #expect(rig.mirror.object("slice:0")?["frequency"] == vfo)
        #expect(rig.sent.count == subscriptions && rig.retunes.isEmpty && rig.recorded.settingsSent.isEmpty)
        #expect(rig.main.displayEffectiveValue(.spectrumMin) == nil)
        #expect(rig.main.ownDisplayFields(among: ["scaleTopDbm", "scaleBottomDbm", "threeDFloors"]).isEmpty)
    }

    @Test func inactiveBandSelectionValuesSurviveRestartAndOpenPanBandStaysAuthoritative() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        rig.main.changeDisplay { $0.enterBand("7"); $0.scaleTopDbm = -30; $0.scaleBottomDbm = -130 }
        rig.main.displayEditScope = .all
        rig.main.displayBandChoice = "5"
        let accepted = keys.set(.integer(-55), forPhoneKey: "DisplayGridMax")
        #expect(accepted)
        let restarted = try Rig(suite: rig.suite)
        let restoredKeys = PhoneSetupKeys(main: restarted.main, defaults: defaults)
        restarted.main.displayEditScope = .all
        restarted.main.displayBandChoice = "5"
        #expect(restoredKeys.value(forPhoneKey: "DisplayGridMax") == .integer(-55))
        restarted.main.displayEditScope = .pan(.first)
        #expect(restarted.main.displayEditingBand == "7")
        // The phone's one scale, on this older Core.
        #expect(restoredKeys.value(forPhoneKey: "DisplayGridMax") == .integer(-55))
        #expect(restarted.recorded.settingsSent.isEmpty && restarted.retunes.isEmpty)
    }
}


extension PhoneSetupKeysV29Tests {
    @Test func actualFrozenAppearanceResetActionRetainsTheExactOriginalPhoneFields() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        func customize() {
            rig.main.changeDisplay {
                $0.traceColour = "#123456"; $0.traceLineColour = "#ABCDEF"
                $0.backgroundColour = "#102030FF"; $0.bandEdgeColour = "#00FF00FF"
                $0.gridColour = "#123123FF"
            }
        }
        customize()
        let native = keys.perform("resetColors")
        #expect(native)
        #expect(rig.main.band.settings.traceColour == "#123456")
        #expect(rig.main.band.settings.traceLineColour == "#ABCDEF" && rig.main.band.settings.backgroundColour == "#102030FF")
        #expect(rig.main.band.settings.bandEdgeColour == BandDisplaySettings.desktopDefaults.bandEdgeColour)
        let folder = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent().deletingLastPathComponent().appendingPathComponent("resources/setup")
        let live = try SetupDescription.parse(json: String(contentsOf: folder.appendingPathComponent("appearance.json"), encoding: .utf8))
        let frozen = try SetupDescription.parse(json: String(contentsOf: folder.appendingPathComponent("legacy/appearance-v27.json"), encoding: .utf8))
        func reset(_ description: SetupDescription) throws -> SetupDescription.Control {
            try #require(description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == "appearance.colorsTheme.resetColors" })
        }
        customize()
        let modern = DescribedControl.performPhoneAction(try reset(live), keys: keys)
        #expect(modern)
        #expect(rig.main.band.settings.traceColour == "#123456" && rig.main.band.settings.bandEdgeColour == BandDisplaySettings.desktopDefaults.bandEdgeColour)
        customize()
        let legacy = DescribedControl.performPhoneAction(try reset(frozen), keys: keys)
        #expect(legacy)
        // Exact ef74a7707 PhoneSetupKeys.swift:365–379: fill reset, chosen
        // line/background retained; original Phone had no band-edge reset.
        #expect(rig.main.band.settings.traceColour == BandDisplaySettings.desktopDefaults.traceColour)
        #expect(rig.main.band.settings.bandEdgeColour == "#00FF00FF")
        #expect(rig.main.band.settings.traceLineColour == "#ABCDEF" && rig.main.band.settings.backgroundColour == "#102030FF")
        #expect(rig.main.band.settings.gridColour == BandDisplaySettings.desktopDefaults.gridColour)
        #expect(rig.recorded.settingsSent.isEmpty)
    }
}


extension PhoneSetupKeysV29Tests {
    @Test func actualSmoothAllConvertsOnlyManualMasterAndClosedModesPreservingEveryInactiveBit() throws {
        func tuple(_ s: BandDisplaySettings) -> [Bool] {
            [s.clarityEnabled, s.waterfallAgc, s.waterfallNfAgc, s.useSpectrumMinMax]
        }
        func configured(_ mode: BandDisplaySettings.WaterfallLevelMode, from s: BandDisplaySettings) -> BandDisplaySettings {
            var result = s
            result.waterfallLevelMode = mode
            // Exercise inactive bits, not merely the five derived mode labels.
            if mode == .clarity { result.waterfallAgc = false; result.waterfallNfAgc = true; result.useSpectrumMinMax = true }
            if mode == .scale { result.waterfallAgc = false; result.waterfallNfAgc = true }
            if mode == .manual { result.waterfallNfAgc = true }
            result.waterfallLowDbm = -143; result.waterfallHighDbm = -63
            return result
        }
        let masterModes: [BandDisplaySettings.WaterfallLevelMode] = [.manual, .clarity, .noiseFloorAgc, .scale]
        for masterMode in masterModes {
            let rig = try Rig()
            let defaults = try #require(UserDefaults(suiteName: rig.suite))
            defer { defaults.removePersistentDomain(forName: rig.suite) }
            let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
            let master = configured(masterMode, from: rig.store.masterSettings(forPan: "1"))
            rig.store.setMasterSettings(master, fields: BandDisplaySettingsStore.waterfallModeFields)
            var kept: [String: BandDisplaySettings] = [:]
            for (id, mode) in [("1", BandDisplaySettings.WaterfallLevelMode.clarity), ("27", .manual), ("42", .noiseFloorAgc), ("93", .scale)] {
                var pan = configured(mode, from: rig.store.settings(forPan: id))
                pan.waterfallPaletteId = 2; pan.traceFill = true
                rig.store.setSettings(pan, forPan: id)
                kept[id] = rig.store.settings(forPan: id)
            }
            rig.main.displayEditScope = .all
            let applied = keys.perform("smoothDefaults")
            #expect(applied)
            let afterMaster = rig.store.masterSettings(forPan: "1")
            var expectedMaster = master
            if masterMode == .manual { expectedMaster.waterfallLevelMode = .agc }
            #expect(tuple(afterMaster) == tuple(expectedMaster))
            for (id, before) in kept {
                let after = rig.store.settings(forPan: id)
                var expected = before
                if before.waterfallLevelMode == .manual { expected.waterfallLevelMode = .agc }
                #expect(tuple(after) == tuple(expected))
                #expect(after.waterfallLowDbm == before.waterfallLowDbm && after.waterfallHighDbm == before.waterfallHighDbm)
                #expect(after.waterfallPaletteId == 7 && after.spectrumAverageTimeMs == 650)
                #expect(after.traceColour == "#FFFFFF" && after.traceLineColour == "#FFFFFF" && !after.traceFill)
                #expect(after.waterfallPeriodMs == 30)
            }
            #expect(rig.recorded.settingsSent.isEmpty && rig.retunes.isEmpty)
        }
    }

    @Test func frozenSmoothActionRetainsUnconditionalAgcWhileCurrentRowKeepsClarityInactiveFlags() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        let folder = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent().deletingLastPathComponent().appendingPathComponent("resources/setup")
        func row(_ file: String) throws -> SetupDescription.Control {
            let description = try SetupDescription.parse(json: String(contentsOf: folder.appendingPathComponent(file), encoding: .utf8))
            return try #require(description.pages.flatMap(\.sections).flatMap(\.controls).first { $0.id == "display.spectrumDefaults.smoothDefaults" })
        }
        func configure() {
            rig.main.changeDisplay {
                $0.clarityEnabled = true; $0.waterfallAgc = false; $0.waterfallNfAgc = true; $0.useSpectrumMinMax = true
                $0.waterfallLowDbm = -147; $0.waterfallHighDbm = -67
            }
        }
        configure()
        let current = DescribedControl.performPhoneAction(try row("display.json"), keys: keys)
        #expect(current && !rig.main.band.settings.waterfallAgc)
        configure()
        let legacy = DescribedControl.performPhoneAction(try row("legacy/display-v28.json"), keys: keys)
        #expect(legacy)
        // Exact ef74a7707 PhoneSetupKeys.swift:346–355: unconditional AGC,
        // without changing Clarity, NF-AGC, Scale or stored manual endpoints.
        let after = rig.main.band.settings
        #expect(after.clarityEnabled && after.waterfallAgc && after.waterfallNfAgc && after.useSpectrumMinMax)
        #expect(after.waterfallLowDbm == -147 && after.waterfallHighDbm == -67)
        #expect(after.waterfallPaletteId == 7 && after.spectrumAverageTimeMs == 650 && after.spectrumAveraging == .logRecursive)
        #expect(after.traceColour == "#FFFFFF" && after.traceLineColour == "#FFFFFF" && !after.traceFill && after.waterfallPeriodMs == 30)
        #expect(rig.recorded.settingsSent.isEmpty)
    }

    @Test func visibleNativeAndDescribedLineWidthUsePhysicalPixelsWithoutMigratingStoredPoints() throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let display = rig.main.display
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        let screenScale = Double(UIScreen.main.scale)
        #expect(rig.main.band.settings.traceWidthPoints == 0.5)
        #expect(display.line == 0.5 * screenScale)
        #expect(DisplaySheetModel.lineText(1.2) == "1.2 px")
        for scale: CGFloat in [1, 2, 3] {
            let limits = DisplaySheetModel.lineRange(scale: scale)
            #expect(limits.min == 0.1 && limits.max == 5 && limits.step == 0.1)
            display.setLine(0.1, scale: scale)
            #expect(abs(rig.main.band.settings.traceWidthPoints - 0.1 / Double(scale)) < 0.00001)
            #expect(abs(Double(rig.main.band.settings.traceWidthPixels(scale: scale)) - 0.1) < 0.00001)
            display.setLine(5, scale: scale)
            #expect(abs(rig.main.band.settings.traceWidthPoints - 5 / Double(scale)) < 0.00001)
            #expect(rig.main.band.settings.traceWidthPixels(scale: scale) == 5)
        }
        // An old raw point value remains kept; both visible codecs report
        // the approved physical paint bound, rather than an impossible 9px.
        rig.main.changeDisplay { $0.traceWidthPoints = 3 }
        #expect(rig.main.band.settings.traceWidthPoints == 3)
        #expect(keys.value(forPhoneKey: "DisplayLineWidth")?.number == min(3 * screenScale, 5))
        #expect(display.line == min(3 * screenScale, 5))
        let restarted = try Rig(suite: rig.suite)
        #expect(restarted.main.band.settings.traceWidthPoints == 3)
        #expect(rig.recorded.settingsSent.isEmpty)
    }
}


extension PhoneSetupKeysV29Tests {
    @Test func acceptedTransmitStateRejectsFocusedReceiveLevelsRowsButAllAndUnkeyKeepTheirNormalGates() async throws {
        let rig = try Rig()
        let defaults = try #require(UserDefaults(suiteName: rig.suite))
        defer { defaults.removePersistentDomain(forName: rig.suite) }
        let keys = PhoneSetupKeys(main: rig.main, defaults: defaults)
        let feed = SetupDescriptionFeed(store: rig.mirror)
        let commands = CommandClient(send: { _ in })
        let dispatcher = SetupControlDispatcher(store: rig.mirror, feed: feed, settings: rig.settings, commands: commands,
            captureSender: { nil }, selectedSlice: { 0 }, selectionChanges: Just(Optional(0)).eraseToAnyPublisher(), phone: keys)
        let folder = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent().deletingLastPathComponent().appendingPathComponent("resources/setup")
        let json = try String(contentsOf: folder.appendingPathComponent("display.json"), encoding: .utf8)
        rig.mirror.apply(.capabilities(.init(properties: [
            .init(name: "setupDescriptionVersion", value: .i64(29)),
            .init(name: "remoteMediaVersion", value: .i64(1)),
            .init(name: "remoteWidebandDisplayVersion", value: .i64(1)),
            .init(name: "displayExtrasVersion", value: .i64(2)),
            .init(name: "stationCatalogVersion", value: .i64(1)),
            .init(name: "txStateVersion", value: .i64(2)),
        ])))
        rig.mirror.apply(.schema(.init(className: "SetupDescription", fields: [
            .init(ordinal: 0, name: "revision", kind: .i64), .init(ordinal: 1, name: "display", kind: .utf8),
        ])))
        rig.mirror.apply(.objectCreate(.init(key: "setup", className: "SetupDescription", properties: [
            .init(ordinal: 0, name: "revision", value: .i64(1)), .init(ordinal: 1, name: "display", value: .utf8(json)),
        ])))
        // The existing Core-authoritative txState consumer, not a test-only
        // display boolean or an action which would key actual hardware.
        rig.mirror.apply(.objectCreate(.init(key: "txState", className: "TransmitState", properties: [
            .init(ordinal: 0, name: "keyed", value: .bool(false)),
            .init(ordinal: 3, name: "txSliceId", value: .i64(0)),
        ])))
        rig.mirror.apply(.snapshotComplete)
        for _ in 0..<100 {
            if feed.description(for: "display") != nil { break }
            try await Task.sleep(for: .milliseconds(2))
        }
        let description = try #require(feed.description(for: "display"))
        let page = try #require(description.pages.first { $0.id == "display.levels" })
        let rows = page.sections.flatMap(\.controls)
        #expect(rows.count == 14)
        rig.main.changeDisplay {
            $0.waterfallLevelMode = .manual; $0.gridFollowsNoiseFloor = true; $0.gridKeepsRange = false
            $0.waterfallLowDbm = -140; $0.waterfallHighDbm = -60
            $0.waterfallBlackLevel = 70; $0.waterfallColorGain = 80; $0.waterfallOffsetDb = 7
        }
        let receivedValues = rig.main.band.settings
        rig.mirror.apply(.delta(.init(key: "txState", properties: [.init(ordinal: 0, name: "keyed", value: .bool(true))])))
        for _ in 0..<100 {
            if rig.main.band.transmitOverlay { break }
            try await Task.sleep(for: .milliseconds(2))
        }
        try #require(rig.main.band.transmit.keyedHere && rig.main.band.transmitOverlay)
        let edits: [String: SetupValue] = [
            "DisplaySpectrumScaleMode": .text("Manual"), "DisplayGridMax": .integer(-30), "DisplayGridMin": .integer(-130),
            "DisplayNFOffsetGridFollow": .integer(9), "DisplayMaintainNFAdjustDelta": .bool(true),
            "DisplayWaterfallLevelsMode": .text("Floor"), "DisplayWfBlackLevel": .integer(25), "DisplayWfColorGain": .integer(35),
            "WaterfallAGCOffsetDb": .integer(12), "DisplayWfLowLevel": .integer(-130), "DisplayWfHighLevel": .integer(-55),
        ]
        for row in rows {
            let key = try #require(row.binding.flatMap { binding -> String? in
                if case .phone(let key) = binding { return key }
                return nil
            })
            let state = dispatcher.state(of: row, in: "display")
            #expect(!state.editable)
            let transmitReason = state.reason?.localizedCaseInsensitiveContains("transmit") == true
            #expect(transmitReason, "row=\(row.id) reason=\(state.reason ?? "nil")")
            let admitted: Bool
            switch dispatcher.admit(row, in: "display") { case .success: admitted = true; case .failure: admitted = false }
            #expect(!admitted)
            if let value = edits[key] {
                let accepted = keys.set(value, forPhoneKey: key)
                #expect(!accepted)
            } else {
                let available = keys.canPerform(key)
                let performed = keys.perform(key)
                #expect(!available && !performed)
            }
        }
        let rxRuntimeKept = rig.main.band.settings == receivedValues
        let rxPersistedKept = rig.store.settings(forPan: "1") == receivedValues
        #expect(rxRuntimeKept && rxPersistedKept)
        // All edits the saved master while the same actual pan stays keyed.
        rig.main.displayEditScope = .all
        let mode = try #require(rows.first { $0.id == "display.levels.waterfallMode" })
        #expect(dispatcher.state(of: mode, in: "display").editable)
        let allMode = keys.set(.text("Manual"), forPhoneKey: "DisplayWaterfallLevelsMode")
        let allScale = keys.set(.text("Manual"), forPhoneKey: "DisplaySpectrumScaleMode")
        let allBlack = keys.set(.integer(65), forPhoneKey: "DisplayWfBlackLevel")
        #expect(allMode && allScale && allBlack)
        #expect(rig.store.masterSettings(forPan: "1").waterfallBlackLevel == 65)
        #expect(rig.main.band.transmitOverlay)
        rig.main.displayEditScope = .pan(.first)
        rig.mirror.apply(.delta(.init(key: "txState", properties: [.init(ordinal: 0, name: "keyed", value: .bool(false))])))
        for _ in 0..<100 {
            if !rig.main.band.transmitOverlay { break }
            try await Task.sleep(for: .milliseconds(2))
        }
        try #require(!rig.main.band.transmitOverlay)
        #expect(dispatcher.state(of: mode, in: "display").editable)
        let black = try #require(rows.first { $0.id == "display.levels.blackLevel" })
        #expect(dispatcher.state(of: black, in: "display").editable)
        let low = try #require(rows.first { $0.id == "display.waterfallDefaults.lowThreshold" })
        #expect(dispatcher.state(of: low, in: "display").editable)
        let changed = keys.set(.text("Clarity"), forPhoneKey: "DisplayWaterfallLevelsMode")
        #expect(changed)
        #expect(!dispatcher.state(of: low, in: "display").editable)
        let retune = try #require(rows.first { $0.id == "display.levels.retune" })
        #expect(dispatcher.state(of: retune, in: "display").editable)
        #expect(rig.recorded.settingsSent.isEmpty && rig.retunes.isEmpty)
    }
}
