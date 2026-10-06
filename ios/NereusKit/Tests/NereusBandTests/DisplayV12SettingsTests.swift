// NereusSDR for iOS: the band settings behind Setup description V12: the three level switches, the noise floor's width and fast colour, the transmit low colour
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMedia
import Testing
@testable import NereusBand

@MainActor
@Suite struct DisplayV12SettingsTests {
    static let v1 = MediaFeatureGates(agreedMinor: 11) { _ in 1 }
    static let v4 = MediaFeatureGates(agreedMinor: 11) { $0 == "displayExtrasVersion" ? 4 : 1 }

    @Test func theThreeLevelSwitchesChooseTheModeAsTheDesktopsDo() {
        var settings = BandDisplaySettings()
        // The desktop's defaults: Clarity on, AGC on, NF-AGC off.
        #expect(settings.clarityEnabled && settings.waterfallAgc && !settings.waterfallNfAgc)
        #expect(settings.waterfallLevelMode == .clarity)
        settings.clarityEnabled = false
        #expect(settings.waterfallLevelMode == .agc)
        settings.waterfallNfAgc = true
        #expect(settings.waterfallLevelMode == .noiseFloorAgc)
        settings.waterfallAgc = false
        #expect(settings.waterfallLevelMode == .noiseFloorAgc)
        settings.waterfallNfAgc = false
        #expect(settings.waterfallLevelMode == .manual)
        // Choosing a mode sets the switches that give it.
        for mode in BandDisplaySettings.WaterfallLevelMode.allCases {
            var chosen = BandDisplaySettings()
            chosen.waterfallLevelMode = mode
            #expect(chosen.waterfallLevelMode == mode, "\(mode)")
        }
        var agc = BandDisplaySettings()
        agc.waterfallLevelMode = .agc
        #expect(!agc.clarityEnabled && agc.waterfallAgc && !agc.waterfallNfAgc)
        // The subscription carries the chosen mode.
        var nf = BandDisplaySettings()
        nf.clarityEnabled = false
        nf.waterfallNfAgc = true
        #expect(nf.extrasRequest(gates: Self.v1)?.waterfallLevels?.mode == .noiseFloorAgc)
    }

    @Test func settingsKeptWithTheOldModeReadAsItsSwitches() throws {
        let suite = "DisplayV12SettingsTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let store = BandDisplaySettingsStore(defaults: defaults)
        for (mode, switches) in [("manual", (false, false, false)), ("agc", (false, false, true)),
                                 ("noiseFloorAgc", (false, true, true)), ("clarity", (true, false, true))] {
            let json = #"{"waterfallLevelMode":"\#(mode)","desktopValuesVersion":1}"#
            defaults.set(Data(json.utf8), forKey: BandDisplaySettingsStore.keyPrefix + "1")
            let read = store.settings(forPan: "1")
            #expect(read.waterfallLevelMode.rawValue == mode)
            #expect((read.clarityEnabled, read.waterfallNfAgc, read.waterfallAgc) == switches, "\(mode)")
        }
        // Kept with the switches, they round-trip.
        var settings = BandDisplaySettings()
        settings.clarityEnabled = false
        settings.waterfallAgc = false
        store.setSettings(settings, forPan: "1")
        #expect(store.settings(forPan: "1").waterfallLevelMode == .manual)
    }

    @Test func theNoiseFloorAsksForFastAttackOnlyFromAVersionFourCore() {
        var settings = BandDisplaySettings()
        settings.noiseFloorLine = true
        settings.noiseFloorShiftDb = 1.5
        #expect(settings.extrasRequest(gates: Self.v4)?.noiseFloor == .init(enabled: true, shiftDb: 1.5, fastAttack: true))
        #expect(settings.extrasRequest(gates: Self.v1)?.noiseFloor == .init(enabled: true, shiftDb: 1.5))
        // The grid following the floor asks for it too, the line off.
        var grid = BandDisplaySettings()
        grid.gridFollowsNoiseFloor = true
        #expect(grid.extrasRequest(gates: Self.v4)?.noiseFloor == .init(enabled: true, shiftDb: 0, fastAttack: true))
        #expect(BandDisplaySettings().extrasRequest(gates: Self.v4)?.noiseFloor == nil)
    }

    @Test func theNoiseFloorLineWidthIsTheDesktopsOneToFive() {
        var settings = BandDisplaySettings()
        #expect(settings.noiseFloorLineWidth == 1 && BandRenderer.noiseFloorLineWidth(settings) == 1)
        settings.noiseFloorLineWidth = 2.5
        #expect(BandRenderer.noiseFloorLineWidth(settings) == 2.5)
        settings.noiseFloorLineWidth = 9
        #expect(BandRenderer.noiseFloorLineWidth(settings) == 5)
        settings.noiseFloorLineWidth = .nan
        #expect(BandRenderer.noiseFloorLineWidth(settings) == 1)
        #expect(BandDisplaySettings().noiseFloorFastColour == "#C8C8C8FF")
    }

    @Test func theTransmitWaterfallDrawsItsLowColourUnderItsLowLevel() {
        var settings = BandDisplaySettings()
        #expect(settings.txWaterfallLowColour == "#000000FF" && settings.waterfallLowColour == nil)
        settings.txWaterfallLowColour = "#102030FF"
        #expect(settings.keyedOverlay.waterfallLowColour == "#102030FF")
        #expect(settings.waterfallLowColour == nil)
        let table = BandPalette.table(custom: PaletteStop.customDefault)
        let low = BandPalette.table(table, lowColour: "#102030FF")
        #expect(Array(low.prefix(4)) == [0x10, 0x20, 0x30, 0xFF])
        #expect(low.dropFirst(4) == table.dropFirst(4))
        #expect(BandPalette.table(table, lowColour: nil) == table)
        #expect(BandPalette.table(table, lowColour: "not a colour") == table)
    }

    // MARK: Grid noise-floor tracking (the V12 note, update 2)

    static func following(clarity: Bool = false, keepsRange: Bool = false, offset: Int = 0) -> BandDisplaySettings {
        var settings = BandDisplaySettings()
        settings.gridFollowsNoiseFloor = true
        settings.clarityEnabled = clarity
        settings.gridKeepsRange = keepsRange
        settings.gridNoiseFloorOffsetDb = offset
        return settings
    }

    @Test func theGridFollowsTheDisplayFloorEvery500msByTwoDbOrMore() {
        let settings = Self.following(offset: -5)
        var tracker = GridFloorTracker()
        var moved = false
        var held = false
        #expect(GridFloorTracker.intervalSeconds == 0.5 && GridFloorTracker.stepDb == 2)
        #expect(GridFloorTracker.claritySmoothingSeconds == 3)
        tracker.noteDisplayFloor(-120, fastAttack: false)
        moved = tracker.run(settings, transmitting: false, at: 10)
        #expect(moved)
        #expect(tracker.range == -125 ... -40)
        // Within 500 ms the rule does not run, and the reading waits.
        tracker.noteDisplayFloor(-110, fastAttack: false)
        held = tracker.run(settings, transmitting: false, at: 10.4)
        #expect(!held)
        moved = tracker.run(settings, transmitting: false, at: 10.5)
        #expect(moved)
        #expect(tracker.range == -115 ... -40)
        // Each reading once: nothing new, nothing moves.
        held = tracker.run(settings, transmitting: false, at: 11.5)
        #expect(!held)
        // Under 2 dB from the minimum now: no move; 2 dB exactly: a move.
        tracker.noteDisplayFloor(-111.5, fastAttack: false)
        held = tracker.run(settings, transmitting: false, at: 12)
        #expect(!held)
        #expect(tracker.range == -115 ... -40)
        tracker.noteDisplayFloor(-112, fastAttack: false)
        moved = tracker.run(settings, transmitting: false, at: 13)
        #expect(moved)
        #expect(tracker.range == -117 ... -40)
        // The change is drawn, never kept in the settings.
        #expect(settings.scaleBottomDbm == BandDisplaySettings.scaleBottomDefaultDbm)
        #expect(tracker.applied(to: settings).scaleRange == -117 ... -40)
    }

    @Test func theGridHoldsInFastAttackAndWhileThePanTransmits() {
        let settings = Self.following()
        var tracker = GridFloorTracker()
        var moved = false
        var held = false
        tracker.noteDisplayFloor(-100, fastAttack: true)
        held = tracker.run(settings, transmitting: false, at: 1)
        #expect(!held)
        #expect(tracker.range == nil)
        // The fast-attack reading was used up: a later tick without a new one holds too.
        held = tracker.run(settings, transmitting: false, at: 2)
        #expect(!held)
        tracker.noteDisplayFloor(-100, fastAttack: false)
        held = tracker.run(settings, transmitting: true, at: 3)
        #expect(!held)
        #expect(tracker.range == nil)
        // A Core that does not say reads as not in fast attack.
        tracker.noteDisplayFloor(-100, fastAttack: nil)
        moved = tracker.run(settings, transmitting: false, at: 4)
        #expect(moved)
        #expect(tracker.range == -100 ... -40)
    }

    @Test func maintainGridRangeMovesTheMaximumByTheOldRange() {
        let settings = Self.following(keepsRange: true)
        var tracker = GridFloorTracker()
        var moved = false
        tracker.noteDisplayFloor(-120, fastAttack: false)
        moved = tracker.run(settings, transmitting: false, at: 1)
        #expect(moved)
        #expect(tracker.range == -120 ... -20)
        tracker.noteDisplayFloor(-130, fastAttack: false)
        moved = tracker.run(settings, transmitting: false, at: 2)
        #expect(moved)
        #expect(tracker.range == -130 ... -30)
    }

    @Test func withClarityOnTheGridFollowsTheSmoothedNoiseFloorOperation() {
        let settings = Self.following(clarity: true)
        var tracker = GridFloorTracker()
        var moved = false
        var held = false
        // The display floor is not Clarity's estimate.
        tracker.noteDisplayFloor(-90, fastAttack: false)
        held = tracker.run(settings, transmitting: false, at: 0)
        #expect(!held)
        tracker.noteClarityFloor(-120, at: 0)
        moved = tracker.run(settings, transmitting: false, at: 1)
        #expect(moved)
        #expect(tracker.range == -120 ... -40)
        // Three seconds later a value 10 dB up moves the estimate by 1 - exp(-1) of it.
        tracker.noteClarityFloor(-110, at: 3)
        moved = tracker.run(settings, transmitting: false, at: 3)
        #expect(moved)
        let expected = -120 + (1 - exp(-1.0)) * 10
        #expect(abs((tracker.range?.lowerBound ?? 0) - expected) < 1e-9)
    }

    @Test func turningTrackingOffForgetsWhatItSet() {
        var settings = Self.following()
        var tracker = GridFloorTracker()
        var moved = false
        tracker.noteDisplayFloor(-100, fastAttack: false)
        moved = tracker.run(settings, transmitting: false, at: 1)
        #expect(moved)
        settings.gridFollowsNoiseFloor = false
        moved = tracker.run(settings, transmitting: false, at: 2)
        #expect(moved)
        #expect(tracker.range == nil && tracker.applied(to: settings).scaleRange == settings.scaleRange)
    }

    // MARK: Rewind depths: the Core's four

    @Test func thePhoneOffersTheCoresRewindDepthsAndReadsOthersAsTheNearest() throws {
        #expect(BandDisplaySettings.rewindChoices == [60, 300, 900, 1200])
        #expect(BandDisplaySettings.nearestRewind(60) == 60 && BandDisplaySettings.nearestRewind(120) == 60)
        #expect(BandDisplaySettings.nearestRewind(200) == 300 && BandDisplaySettings.nearestRewind(1000) == 900)
        // Halfway (the phone's old 10 minutes): the longer.
        #expect(BandDisplaySettings.nearestRewind(600) == 900)
        #expect(BandDisplaySettings.nearestRewind(5000) == 1200 && BandDisplaySettings.nearestRewind(0) == 60)
        let suite = "DisplayV12SettingsTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let store = BandDisplaySettingsStore(defaults: defaults)
        for (kept, read) in [(600, 900), (60, 60), (240, 300), (1200, 1200)] {
            defaults.set(Data(#"{"rewindSeconds":\#(kept),"desktopValuesVersion":1}"#.utf8),
                         forKey: BandDisplaySettingsStore.keyPrefix + "1")
            #expect(store.settings(forPan: "1").rewindSeconds == read, "\(kept)")
        }
    }
}
