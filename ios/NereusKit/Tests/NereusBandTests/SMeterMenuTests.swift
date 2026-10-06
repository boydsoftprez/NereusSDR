// NereusSDR for iOS: each part of the S-meter's menu changes the meter as the desktop's does
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels
import Testing
@testable import NereusBand

/// D86: RX Mode, TX Mode, Peak Hold and Meter Face, a test per group, and
/// the reasons a mode the Core cannot feed is off.
@Suite struct SMeterMenuTests {
    private static func receiving() -> SMeterReadings {
        SMeterReadings(peakDbm: -80, averageDbm: -90, maxBinDbm: -70, maxBinAvailable: true,
                       transmitSent: true)
    }

    private static func near(_ a: Double?, _ b: Double) -> Bool {
        guard let a else {
            return false
        }
        return abs(a - b) < 1e-9
    }

    @Test("RX Mode: Signal, Sig Avg and Max Bin read their own reading; Signal Peak holds the peak and marks it")
    func rxMode() throws {
        let meters = try SMeterFixtures.meters()
        let s = meters.sMeter
        var state = SMeterState()
        state.apply(Self.receiving(), at: 0)

        var shown = state.display(at: 0, meters: meters)
        #expect(Self.near(shown.needle, SMeterScale.receiveFraction(dbm: -80, meter: s)))
        #expect(shown.caption == "S-Meter")
        #expect(shown.left == "S9+5.0")
        #expect(shown.right == "-80.0 dBm")
        #expect(shown.peakMarker == nil)

        state.chooseRx(.signalAverage, at: 0)
        shown = state.display(at: 0, meters: meters)
        #expect(Self.near(shown.needle, SMeterScale.receiveFraction(dbm: -90, meter: s)))
        #expect(shown.caption == "Sig Avg")
        #expect(shown.right == "-90.0 dBm")

        state.chooseRx(.maxBin, at: 0)
        shown = state.display(at: 0, meters: meters)
        #expect(Self.near(shown.needle, SMeterScale.receiveFraction(dbm: -70, meter: s)))
        #expect(shown.caption == "Max Bin")
        #expect(shown.left == "S9+15.0")

        // Signal Peak: a -60 dBm burst, then -90. The needle holds the
        // burst and falls 0.5 dB each 50 ms; the marker shows where it is.
        state.chooseRx(.signalPeak, at: 0)
        var readings = Self.receiving()
        readings.peakDbm = -60
        state.apply(readings, at: 1)
        readings.peakDbm = -90
        state.apply(readings, at: 1.02)
        shown = state.display(at: 1.1, meters: meters)
        #expect(shown.caption == "S-Meter Peak")
        #expect(shown.right == "-61.0 dBm")
        #expect(Self.near(shown.needle, SMeterScale.receiveFraction(dbm: -61, meter: s)))
        #expect(Self.near(shown.peakMarker, SMeterScale.receiveFraction(dbm: -61, meter: s)))

        // Signal reads the same reading without holding it.
        state.chooseRx(.signal, at: 1.1)
        shown = state.display(at: 1.1, meters: meters)
        #expect(shown.right == "-90.0 dBm")
        #expect(shown.peakMarker == nil)
    }

    @Test("TX Mode: on the air the needle reads Power, SWR, Level or Compression on its own scale")
    func txMode() throws {
        let meters = try SMeterFixtures.meters()
        var state = SMeterState()
        state.apply(SMeterReadings(peakDbm: -80, transmitting: true, transmitSent: true, txReadingsVersion: 1,
                                   forwardWatts: 60, swr: 1.5, micLevelDb: -13, compressionDb: -5,
                                   compressionSent: true), at: 0)
        var shown = state.display(at: 0, meters: meters)
        #expect(shown.transmitting)
        #expect(shown.left == "TX")
        #expect(shown.caption == "Power")
        #expect(shown.right == "60 W")
        #expect(Self.near(shown.needle, 0.5))
        #expect(shown.title == "FORWARD POWER")

        state.chooseTx(.swr)
        shown = state.display(at: 0, meters: meters)
        #expect(shown.caption == "SWR")
        #expect(shown.right == "1.5 : 1")
        #expect(Self.near(shown.needle, 0.25))
        state.chooseFace(.classic)
        #expect(state.display(at: 0, meters: meters).right == "1.5")

        state.chooseTx(.level)
        shown = state.display(at: 0, meters: meters)
        #expect(shown.caption == "Level")
        #expect(shown.right == "-13 dB")
        #expect(Self.near(shown.needle, 27.0 / 45.0))

        state.chooseTx(.compression)
        shown = state.display(at: 0, meters: meters)
        #expect(shown.caption == "Compression")
        #expect(shown.right == "-5 dB")
        #expect(Self.near(shown.needle, 20.0 / 25.0))
        #expect(shown.transmitScale?.redFrom == nil)
        // Receiving again, the needle is back on the S scale.
        state.apply(SMeterReadings(peakDbm: -80, transmitSent: true), at: 1)
        #expect(state.display(at: 1, meters: meters).transmitting == false)
    }

    @Test("Peak Hold: the line holds a second, then falls at the chosen decay; Reset drops it; off hides it")
    func peakHold() throws {
        let meters = try SMeterFixtures.meters()
        let s = meters.sMeter
        var state = SMeterState()
        var readings = Self.receiving()
        readings.peakDbm = -60
        state.apply(readings, at: 0)
        readings.peakDbm = -90
        state.apply(readings, at: 0.5)

        #expect(Self.near(state.display(at: 0.9, meters: meters).holdLine,
                          SMeterScale.receiveFraction(dbm: -60, meter: s)))
        // Medium: 10 dB/s after the second's hold.
        #expect(Self.near(state.display(at: 2, meters: meters).holdLine,
                          SMeterScale.receiveFraction(dbm: -70, meter: s)))
        state.choosePeakDecay(.fast)
        #expect(Self.near(state.display(at: 2, meters: meters).holdLine,
                          SMeterScale.receiveFraction(dbm: -80, meter: s)))
        state.choosePeakDecay(.slow)
        #expect(Self.near(state.display(at: 2, meters: meters).holdLine,
                          SMeterScale.receiveFraction(dbm: -65, meter: s)))
        // Never below the reading.
        #expect(Self.near(state.display(at: 30, meters: meters).holdLine,
                          SMeterScale.receiveFraction(dbm: -90, meter: s)))

        state.resetPeak()
        let reset = state.display(at: 2, meters: meters)
        #expect(Self.near(reset.holdLine, reset.needle))

        state.setPeakHold(false)
        #expect(state.display(at: 2, meters: meters).holdLine == nil)
        #expect(state.settings.peakHold == false)
        state.setPeakHold(true)
        readings.peakDbm = -50
        state.apply(readings, at: 3)
        #expect(Self.near(state.display(at: 3, meters: meters).holdLine,
                          SMeterScale.receiveFraction(dbm: -50, meter: s)))
    }

    @Test("Meter Face: Classic labels a few ticks on its arc; a vintage card ticks finely under a title and legend")
    func face() throws {
        let meters = try SMeterFixtures.meters()
        var state = SMeterState()
        state.apply(Self.receiving(), at: 0)
        var shown = state.display(at: 0, meters: meters)
        #expect(state.settings.face == .agedCream)
        #expect(shown.receiveScale?.ticks.count == 16)
        #expect(shown.receiveScale?.ticks.compactMap(\.label) == ["1", "3", "5", "7", "9", "+20", "+40", "+60"])
        #expect(shown.title == "SIGNAL STRENGTH")
        #expect(shown.legend == "S UNITS")

        state.chooseFace(.classic)
        shown = state.display(at: 0, meters: meters)
        #expect(shown.receiveScale?.ticks.compactMap(\.label) == ["1", "3", "5", "7", "9", "+20", "+40"])
        #expect(shown.receiveScale?.redFrom == SMeterScale.s9Share)

        #expect(SMeterFace.allCases.map(\.label) == ["Aged Cream", "VU Amber", "Collins White", "Blackface",
                                                    "Carbon", "Ice", "Classic (flat)"])
        state.chooseRx(.signalAverage, at: 0)
        state.chooseFace(.ice)
        #expect(state.display(at: 0, meters: meters).legend == "S UNITS  AVERAGE")
    }

    @Test("a mode without its reading is off, with its reason; Max Bin lights with a live pan display")
    func reasons() {
        var readings = SMeterReadings()
        #expect(readings.reason(for: .maxBin) == SMeterReadings.maxBinUnavailableText)
        #expect(readings.reason(for: .signal) == nil)
        for mode in SMeterTxMode.allCases {
            #expect(readings.reason(for: mode) == SMeterReadings.transmitNotSentText)
        }
        readings.transmitSent = true
        #expect(readings.reason(for: .power) == nil)
        #expect(readings.reason(for: .swr) == nil)
        #expect(readings.reason(for: .level) == nil)
        #expect(readings.reason(for: .compression) == SMeterReadings.compressionNotSentText)
        readings.txReadingsVersion = 1
        #expect(readings.reason(for: .compression) == SMeterReadings.compressionNotSentText)
        readings.compressionSent = true
        #expect(readings.reason(for: .compression) == nil)

        readings.maxBinAvailable = true
        readings.maxBinDbm = -400
        #expect(readings.reason(for: .maxBin) == nil)
        #expect(readings.level(for: .maxBin) == nil)
        readings.maxBinDbm = -88
        #expect(readings.level(for: .maxBin) == -88)
    }

    @Test("Compression rests at zero at silence and clips the Core's raw peak to the face")
    func compressionSilence() {
        var readings = SMeterReadings(transmitSent: true, txReadingsVersion: 1,
                                      compressionDb: -30, compressionSent: true)
        #expect(readings.value(for: .compression) == 0)
        readings.compressionDb = -29
        #expect(readings.value(for: .compression) == -25)
        readings.compressionDb = -6
        #expect(readings.value(for: .compression) == -6)
        readings.compressionDb = 2
        #expect(readings.value(for: .compression) == 0)
        readings.compressionDb = -400
        #expect(readings.value(for: .compression) == nil)
        readings.compressionDb = .nan
        #expect(readings.value(for: .compression) == nil)
        readings.compressionDb = nil
        #expect(readings.value(for: .compression) == nil)
    }

    @Test("with no reading the readouts show dashes and the needle rests at the floor")
    func noReading() throws {
        let meters = try SMeterFixtures.meters()
        var state = SMeterState()
        state.apply(SMeterReadings(peakDbm: -400), at: 0)
        let shown = state.display(at: 0, meters: meters)
        #expect(shown.left == "--")
        #expect(shown.right == "-- dBm")
        #expect(shown.needle == 0)
        #expect(shown.holdLine == nil)
        #expect(shown.spoken == "No signal reading")
    }
}
