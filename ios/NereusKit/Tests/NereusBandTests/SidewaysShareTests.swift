// NereusSDR for iOS: tests that sideways the band starts at 55 percent spectrum and upright at 40, each kept for the pan
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import Testing
@testable import NereusBand

/// D84: a sideways phone is too short for a flag and a spot row at 40
/// percent, so sideways the band starts at 55; upright it keeps the
/// desktop's 40. The drag and the Spectrum height slider move whichever
/// way the band is turned, and each is kept for the pan.
@MainActor
@Suite struct SidewaysShareTests {
    @Test func sidewaysStartsAtFiftyFiveAndUprightAtForty() {
        var settings = BandDisplaySettings.desktopDefaults
        #expect(settings.spectrumShare == 0.4)
        settings.sideways = true
        #expect(settings.spectrumShare == 0.55)
        #expect(settings.currentSpectrumSharePercent == BandDisplaySettings.spectrumShareSidewaysDefaultPercent)
        let size = CGSize(width: 874, height: 300)
        let shared = size.height - BandLayout.scaleHeightPoints
        #expect(BandLayout(size: size, scale: 1, settings: settings).spectrum.height == (shared * 0.55).rounded())
    }

    @Test func eachWayKeepsItsOwnShare() {
        var settings = BandDisplaySettings.desktopDefaults
        settings.sideways = true
        settings.currentSpectrumSharePercent = 70
        #expect(settings.spectrumSharePercentSideways == 70)
        #expect(settings.spectrumSharePercent == 40)
        settings.sideways = false
        #expect(settings.spectrumShare == 0.4)
        settings.currentSpectrumSharePercent = 25
        #expect(settings.spectrumSharePercent == 25)
        #expect(settings.spectrumSharePercentSideways == 70)
        // Within 20 and 80 either way.
        settings.sideways = true
        settings.currentSpectrumSharePercent = 95
        #expect(settings.spectrumShare == 0.8)
        // Reset keeps which way the band is turned.
        #expect(settings.resetKeepingPlace().sideways)
        #expect(settings.resetKeepingPlace().spectrumShare == 0.55)
    }

    @Test func anUprightShareKeptEarlierCarriesOverAndSidewaysStartsAtItsDefault() throws {
        let suite = "SidewaysShareTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        // Settings kept before D84: an upright share of 62, no sideways share.
        var earlier = try #require(try JSONSerialization.jsonObject(
            with: JSONEncoder().encode(BandDisplaySettings.desktopDefaults)) as? [String: Any])
        earlier.removeValue(forKey: "spectrumSharePercentSideways")
        earlier.removeValue(forKey: "sideways")
        earlier["spectrumSharePercent"] = 62
        defaults.set(try JSONSerialization.data(withJSONObject: earlier), forKey: BandDisplaySettingsStore.keyPrefix + "1")
        let store = BandDisplaySettingsStore(defaults: defaults)
        var read = store.settings(forPan: "1")
        #expect(read.spectrumSharePercent == 62)
        #expect(read.spectrumSharePercentSideways == 55)
        // Which way the band was turned is never read back.
        read.sideways = true
        store.setSettings(read, forPan: "1")
        #expect(!store.settings(forPan: "1").sideways)
        #expect(store.settings(forPan: "1").spectrumSharePercent == 62)
    }
}
