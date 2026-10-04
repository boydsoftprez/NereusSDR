// NereusSDR for iOS: tests for the phone's display settings and what they ask the Core for
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import NereusMedia
import Testing
@testable import NereusBand

/// R-IOS-11: the phone's display settings start from the desktop's defaults
/// and ask the Core for exactly what those defaults imply.
@MainActor
@Suite struct BandDisplaySettingsTests {
    static let open = MediaFeatureGates(agreedMinor: 11) { _ in 1 }
    static let closed = MediaFeatureGates(agreedMinor: 11) { $0 == "displayExtrasVersion" ? 0 : 1 }

    static func subscription() -> DisplaySubscription {
        DisplaySubscription(endpointId: 1, revision: 1, sliceId: 0, tier: .fine, fftSize: 4096, windowType: 0,
                            centreHz: 7_236_400, spanHz: 48_000, pixels: 1179, fps: 30, framesPerLine: 1,
                            trace: .init(detector: 0, averageMode: 0, averageAlpha: 0),
                            waterfall: .init(detector: 0, averageMode: 0, averageAlpha: 0),
                            minDbm: -160, maxDbm: 0, wideSpanFactor: 0)
    }

    static let extrasKeys: Set<String> = [
        "peakBlobs", "activePeakHold", "noiseFloor", "waterfallLevels", "normalize", "calibrationOffsetDb",
        "averageTimeMs", "waterfallAverageTimeMs",
    ]

    @Test func theDefaultsAreTheDesktops() {
        let settings = BandDisplaySettings.desktopDefaults
        #expect(settings.waterfallLevelMode == .clarity)
        #expect(settings.waterfallLowDbm == -122)
        #expect(settings.waterfallHighDbm == -62)
        #expect(!settings.noiseFloorLine)
        #expect(settings.noiseFloorShiftDb == 0)
        #expect(!settings.normalize)
        #expect(settings.calibrationOffsetDb == 0)
        #expect(settings.spectrumAverageTimeMs == 30)
        #expect(settings.waterfallAverageTimeMs == 120)
        #expect(!settings.peakBlobs)
        #expect(settings.peakBlobCount == 3)
        #expect(!settings.peakBlobHold)
        #expect(settings.peakBlobHoldMs == 500)
        #expect(!settings.peakBlobFall)
        #expect(settings.peakBlobFallDbPerSec == 6)
        #expect(!settings.peakBlobsInsideFilterOnly)
        #expect(!settings.activePeakHold)
        #expect(settings.activePeakHoldMs == 2000)
        #expect(settings.activePeakHoldFallDbPerSec == 6)
        #expect(settings.bandPlanSize == .small)
        #expect(settings.bandPlanStrip)
        #expect(!settings.extendedView)
    }

    @Test func theDefaultsAskForExactlyWhatTheDesktopsDefaultsImply() throws {
        let subscription = BandDisplaySettings.desktopDefaults.applied(to: Self.subscription(), gates: Self.open)
        try DisplayEndpointRequest.validate(subscription, gates: Self.open)
        let payload = DisplayEndpointRequest.subscribe(subscription, connectionId: "c", gates: Self.open)
        let asked = Set(payload.keys).intersection(Self.extrasKeys)
        #expect(asked == ["waterfallLevels", "averageTimeMs", "waterfallAverageTimeMs"])
        #expect(payload["waterfallLevels"] == .object([
            "mode": .string("clarity"), "lowDbm": .number(-122), "highDbm": .number(-62), "offsetDb": .number(0),
        ]))
        #expect(payload["averageTimeMs"] == .number(30))
        #expect(payload["waterfallAverageTimeMs"] == .number(120))
    }

    @Test func nothingIsAskedWhileTheCoreOffersNoExtras() throws {
        #expect(BandDisplaySettings.desktopDefaults.extrasRequest(gates: Self.closed) == nil)
        #expect(!BandDisplaySettings.extrasAvailable(gates: Self.closed))
        var everything = BandDisplaySettings.desktopDefaults
        everything.peakBlobs = true
        everything.activePeakHold = true
        everything.noiseFloorLine = true
        let subscription = everything.applied(to: Self.subscription(), gates: Self.closed)
        try DisplayEndpointRequest.validate(subscription, gates: Self.closed)
        let payload = DisplayEndpointRequest.subscribe(subscription, connectionId: "c", gates: Self.closed)
        #expect(Set(payload.keys).isDisjoint(with: Self.extrasKeys))
    }

    @Test func aFeatureTurnedOnIsAskedForAsSet() throws {
        var settings = BandDisplaySettings.desktopDefaults
        settings.peakBlobs = true
        settings.peakBlobHold = true
        settings.activePeakHold = true
        settings.noiseFloorLine = true
        settings.noiseFloorShiftDb = 3
        settings.normalize = true
        settings.calibrationOffsetDb = -1.5
        settings.waterfallLevelMode = .manual
        let request = try #require(settings.extrasRequest(gates: Self.open))
        try request.validate()
        #expect(request.peakBlobs == .init(count: 3, holdMs: 500, fallDbPerSec: 0, insideOnly: false))
        #expect(request.activePeakHold == .init(enabled: true, holdMs: 2000, fallDbPerSec: 6))
        #expect(request.noiseFloor == .init(enabled: true, shiftDb: 3))
        #expect(request.normalize == true)
        // Calibration stays with the Core, which already applies its own (row 17).
        #expect(request.calibrationOffsetDb == nil)
        #expect(request.waterfallLevels?.mode == .manual)
    }

    /// Display description V11 (the Core's Spectrum Peaks page): Update
    /// during TX goes as `onTx` only to a Core that sent
    /// `displayExtrasVersion` 3; an older Core would refuse the member.
    @Test func updateDuringTransmitGoesOnlyToACoreThatTakesIt() throws {
        let three = MediaFeatureGates(agreedMinor: 11) { $0 == "displayExtrasVersion" ? 3 : 1 }
        #expect(three.peakHoldOnTx && !Self.open.peakHoldOnTx && !Self.closed.peakHoldOnTx)
        var settings = BandDisplaySettings.desktopDefaults
        #expect(!settings.activePeakHoldOnTx)
        settings.activePeakHold = true
        settings.activePeakHoldMs = 4000
        #expect(try #require(settings.extrasRequest(gates: three)).activePeakHold
                == .init(enabled: true, holdMs: 4000, fallDbPerSec: 6, onTx: false))
        settings.activePeakHoldOnTx = true
        #expect(try #require(settings.extrasRequest(gates: three)).activePeakHold?.onTx == true)
        #expect(try #require(settings.extrasRequest(gates: Self.open)).activePeakHold?.onTx == nil)
        let sent = DisplayEndpointRequest.subscribe(settings.applied(to: Self.subscription(), gates: three),
                                                    connectionId: "c", gates: three)
        #expect(sent["activePeakHold"] == .object(["enabled": .bool(true), "holdMs": .number(4000),
                                                   "fallDbPerSec": .number(6), "onTx": .bool(true)]))
        // A request carrying it is still sent without it to an older Core.
        let older = DisplayEndpointRequest.subscribe(settings.applied(to: Self.subscription(), gates: three),
                                                     connectionId: "c", gates: Self.open)
        #expect(older["activePeakHold"] == .object(["enabled": .bool(true), "holdMs": .number(4000),
                                                    "fallDbPerSec": .number(6)]))
    }

    /// The blobs' hold, drop and fall rate reach the Core as the desktop's
    /// switches: with the hold off, drop and fall rate are ignored; with
    /// the hold on and drop off, a blob goes at the end of its hold.
    @Test func theBlobsHoldDropAndFallAreTheDesktopsSwitches() throws {
        var settings = BandDisplaySettings.desktopDefaults
        settings.peakBlobs = true
        settings.peakBlobFall = true
        settings.peakBlobFallDbPerSec = 15
        #expect(try #require(settings.extrasRequest(gates: Self.open)).peakBlobs
                == .init(count: 3, holdMs: 0, fallDbPerSec: 0, insideOnly: false))
        settings.peakBlobHold = true
        #expect(try #require(settings.extrasRequest(gates: Self.open)).peakBlobs
                == .init(count: 3, holdMs: 500, fallDbPerSec: 15, insideOnly: false))
        settings.peakBlobFall = false
        #expect(try #require(settings.extrasRequest(gates: Self.open)).peakBlobs
                == .init(count: 3, holdMs: 500, fallDbPerSec: 0, insideOnly: false))
        settings.peakBlobs = false
        #expect(try #require(settings.extrasRequest(gates: Self.open)).peakBlobs == nil)
    }

    @Test func eachPanKeepsItsOwnSettingsOnThePhone() throws {
        let suite = "band-settings-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let store = BandDisplaySettingsStore(defaults: defaults)
        #expect(store.settings(forPan: "pan-1") == .desktopDefaults)
        var changed = BandDisplaySettings.desktopDefaults
        changed.traceColour = "#FF0000"
        changed.bandPlanSize = .huge
        changed.activePeakHold = true
        store.setSettings(changed, forPan: "pan-1")
        #expect(store.settings(forPan: "pan-1") == changed)
        #expect(store.settings(forPan: "pan-2") == .desktopDefaults)
        // A kept value that lacks a later setting takes that setting's default.
        defaults.set(Data(##"{"traceColour":"#00FF00"}"##.utf8), forKey: BandDisplaySettingsStore.keyPrefix + "pan-3")
        var expected = BandDisplaySettings.desktopDefaults
        expected.traceColour = "#00FF00"
        #expect(store.settings(forPan: "pan-3") == expected)
        store.reset(pan: "pan-1")
        #expect(store.settings(forPan: "pan-1") == .desktopDefaults)
    }

    /// JJ, 2026-09-28: as on the desktop, the transmit filter shows on the
    /// receive waterfall by default; every kept choice stays as it was.
    @Test func theTransmitFilterOverlayStartsOnAndEveryKeptChoiceStays() throws {
        #expect(BandDisplaySettings().showTxFilterOnWaterfall)
        #expect(BandDisplaySettings.desktopDefaults.showTxFilterOnWaterfall)
        var off = BandDisplaySettings.desktopDefaults
        off.showTxFilterOnWaterfall = false
        #expect(off.resetKeepingPlace().showTxFilterOnWaterfall, "a reset takes the new default")

        let suite = "band-settings-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let store = BandDisplaySettingsStore(defaults: defaults)
        #expect(store.settings(forPan: "new").showTxFilterOnWaterfall, "nothing kept: on")
        store.setSettings(off, forPan: "kept-off")
        #expect(!store.settings(forPan: "kept-off").showTxFilterOnWaterfall)
        var on = BandDisplaySettings.desktopDefaults
        on.showTxFilterOnWaterfall = true
        store.setSettings(on, forPan: "kept-on")
        #expect(store.settings(forPan: "kept-on").showTxFilterOnWaterfall)
        // Kept by an earlier version, when the default was off: its stored false stays.
        defaults.set(Data(##"{"showTxFilterOnWaterfall":false,"traceColour":"#00E5FF"}"##.utf8),
                     forKey: BandDisplaySettingsStore.keyPrefix + "earlier-off")
        #expect(!store.settings(forPan: "earlier-off").showTxFilterOnWaterfall)
        defaults.set(Data(##"{"showTxFilterOnWaterfall":true}"##.utf8),
                     forKey: BandDisplaySettingsStore.keyPrefix + "earlier-on")
        #expect(store.settings(forPan: "earlier-on").showTxFilterOnWaterfall)
        // A kept value without the key takes the default.
        defaults.set(Data(##"{"traceColour":"#00FF00"}"##.utf8),
                     forKey: BandDisplaySettingsStore.keyPrefix + "missing")
        #expect(store.settings(forPan: "missing").showTxFilterOnWaterfall)
        store.reset(pan: "kept-off")
        #expect(store.settings(forPan: "kept-off").showTxFilterOnWaterfall, "a reset pan takes the default")
    }

    @Test func settingsKeptBeforeTheExtendedViewReadAsOff() throws {
        let suite = "band-settings-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let store = BandDisplaySettingsStore(defaults: defaults)
        // Every setting an earlier build kept, and no extendedView among them.
        var earlier = try #require(try JSONSerialization.jsonObject(
            with: try JSONEncoder().encode(BandDisplaySettings.desktopDefaults)) as? [String: Any])
        earlier.removeValue(forKey: "extendedView")
        earlier["peakBlobs"] = true
        defaults.set(try JSONSerialization.data(withJSONObject: earlier), forKey: BandDisplaySettingsStore.keyPrefix + "1")
        let read = store.settings(forPan: "1")
        #expect(!read.extendedView)
        #expect(read.peakBlobs)
        var on = read
        on.extendedView = true
        store.setSettings(on, forPan: "1")
        #expect(store.settings(forPan: "1").extendedView)
    }

    @Test func theTracesWidthStartsAtHalfAPointAndKeptSettingsWithoutItReadAsThat() throws {
        #expect(BandDisplaySettings.desktopDefaults.traceWidthPoints == 0.5)
        let suite = "band-settings-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let store = BandDisplaySettingsStore(defaults: defaults)
        var earlier = try #require(try JSONSerialization.jsonObject(
            with: try JSONEncoder().encode(BandDisplaySettings.desktopDefaults)) as? [String: Any])
        earlier.removeValue(forKey: "traceWidthPoints")
        earlier["traceFillOpacity"] = 0.7
        defaults.set(try JSONSerialization.data(withJSONObject: earlier), forKey: BandDisplaySettingsStore.keyPrefix + "1")
        let read = store.settings(forPan: "1")
        #expect(read.traceWidthPoints == 0.5)
        #expect(read.traceFillOpacity == 0.7)
        var thin = read
        thin.traceWidthPoints = 1.0 / 3
        store.setSettings(thin, forPan: "1")
        #expect(store.settings(forPan: "1").traceWidthPoints == 1.0 / 3)
    }

    @Test func theTracesWidthRunsFromOneScreenPixelToThreePointsAndIsNeverThinnerThanAPixel() {
        #expect(BandDisplaySettings.traceWidthRange(scale: 3) == (1.0 / 3)...3)
        #expect(BandDisplaySettings.traceWidthRange(scale: 2) == 0.5...3)
        var settings = BandDisplaySettings.desktopDefaults
        #expect(settings.traceWidthPixels(scale: 3) == 1.5)
        #expect(settings.traceWidthPixels(scale: 1) == 1)
        settings.traceWidthPoints = 3
        #expect(settings.traceWidthPixels(scale: 3) == 9)
        settings.traceWidthPoints = 0.1
        #expect(settings.traceWidthPixels(scale: 3) == 1)
    }

    // MARK: The band plan's size (D79)

    @Test func theStripsSizesAreTheDesktopsAndItsHeightIsTheSizePlusFourPoints() {
        #expect(BandPlanSize.allCases == [.off, .small, .medium, .large, .huge])
        #expect(BandPlanSize.allCases.map(\.labelPoints) == [0, 6, 10, 12, 16])
        #expect(BandPlanSize.allCases.map(\.stripHeightPoints) == [0, 10, 14, 16, 20])
        #expect(BandPlanSize.allCases.map(\.label) == ["Off", "Small", "Medium", "Large", "Huge"])
        var settings = BandDisplaySettings.desktopDefaults
        for size in BandPlanSize.allCases {
            settings.bandPlanSize = size
            #expect(settings.bandPlanStrip == (size != .off))
        }
    }

    @Test func theSizeIsKeptAndSettingsKeptWithTheStripOnOrOffReadAsSmallOrOff() throws {
        let suite = "band-settings-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let store = BandDisplaySettingsStore(defaults: defaults)
        for size in BandPlanSize.allCases {
            var settings = BandDisplaySettings.desktopDefaults
            settings.bandPlanSize = size
            store.setSettings(settings, forPan: "kept")
            #expect(store.settings(forPan: "kept").bandPlanSize == size)
        }
        // What an earlier build kept: the strip on or off, and a plan of this phone's.
        var earlier = try #require(try JSONSerialization.jsonObject(
            with: try JSONEncoder().encode(BandDisplaySettings.desktopDefaults)) as? [String: Any])
        earlier.removeValue(forKey: "bandPlanSize")
        earlier["bandPlanId"] = "iaru-r1"
        earlier["traceFillOpacity"] = 0.6
        for (on, size) in [(true, BandPlanSize.small), (false, .off)] {
            earlier["bandPlanStrip"] = on
            defaults.set(try JSONSerialization.data(withJSONObject: earlier),
                         forKey: BandDisplaySettingsStore.keyPrefix + "earlier")
            let read = store.settings(forPan: "earlier")
            #expect(read.bandPlanSize == size)
            #expect(read.traceFillOpacity == 0.6)
            // Kept again, it keeps the size and neither earlier member.
            store.setSettings(read, forPan: "earlier")
            let keptData = try #require(defaults.data(forKey: BandDisplaySettingsStore.keyPrefix + "earlier"))
            let kept = try #require(try JSONSerialization.jsonObject(with: keptData) as? [String: Any])
            #expect(kept["bandPlanSize"] as? String == size.rawValue)
            #expect(kept["bandPlanStrip"] == nil)
            #expect(kept["bandPlanId"] == nil)
        }
    }
}
