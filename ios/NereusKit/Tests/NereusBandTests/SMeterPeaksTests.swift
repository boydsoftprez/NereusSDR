// NereusSDR for iOS: the S-meter's held peak and peak hold line over time, and its choices kept on this device
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusBand

/// D86: the held peak falls 0.5 dB each 50 ms and drops to the reading
/// every 10 s; the line holds a second then falls; the choices persist.
@Suite struct SMeterPeaksTests {
    @Test("the held peak falls 0.5 dB each 50 ms until it meets the reading")
    func heldPeakFalls() {
        var peaks = SMeterPeaks()
        peaks.update(level: -60, at: 0)
        peaks.update(level: -90, at: 0.01)
        peaks.advance(to: 0.5)
        #expect(peaks.peak == -65)
        peaks.advance(to: 7)
        #expect(peaks.peak == -90)
        // Stopped at the reading, it stays when the reading drops, as the desktop's does.
        peaks.update(level: -100, at: 7.01)
        #expect(peaks.peak == -90)
    }

    @Test("every 10 seconds the held peak drops to the reading")
    func heldPeakResets() {
        var peaks = SMeterPeaks()
        peaks.update(level: -90, at: 0)
        peaks.update(level: -100, at: 1)
        peaks.advance(to: 9.99)
        #expect(peaks.peak == -90)
        peaks.advance(to: 10)
        #expect(peaks.peak == -100)
    }

    @Test("no reading clears both peaks; the next reading starts afresh")
    func noReadingClears() {
        var peaks = SMeterPeaks()
        peaks.update(level: -50, at: 0)
        peaks.update(level: nil, at: 0.1)
        #expect(peaks.peak == nil)
        #expect(peaks.hold(at: 0.1) == nil)
        peaks.update(level: -100, at: 0.2)
        #expect(peaks.peak == -100)
        #expect(peaks.hold(at: 0.2) == -100)
    }

    @Test("the choices persist on this device, and an unknown or missing one takes the desktop's default")
    @MainActor
    func settingsPersist() throws {
        let name = "SMeterPeaksTests.\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let store = SMeterSettingsStore(defaults: defaults)
        #expect(store.settings == .desktopDefaults)

        let chosen = SMeterSettings(rxMode: .maxBin, txMode: .swr, peakHold: false, peakDecay: .slow, face: .carbon)
        store.setSettings(chosen)
        #expect(SMeterSettingsStore(defaults: defaults).settings == chosen)

        let partial = try JSONSerialization.data(withJSONObject: ["rxMode": "signalPeak", "face": "someday"])
        defaults.set(partial, forKey: SMeterSettingsStore.key)
        let read = SMeterSettingsStore(defaults: defaults).settings
        #expect(read.rxMode == .signalPeak)
        #expect(read.face == .agedCream)
        #expect(read.peakHold)
        #expect(read.peakDecay == .medium)
    }
}
