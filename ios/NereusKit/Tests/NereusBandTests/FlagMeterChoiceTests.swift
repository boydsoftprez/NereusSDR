// NereusSDR for iOS: a slice flag's level follows the S-meter's RX Mode, as a desktop window's flags do
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing
@testable import NereusBand

@Suite struct FlagMeterChoiceTests {
    @Test("Each RX Mode shows as one of the flag's three choices; only a choice off the default has a tag")
    func choices() {
        #expect(FlagMeterChoice(.signal) == .peak)
        #expect(FlagMeterChoice(.signalPeak) == .peak)
        #expect(FlagMeterChoice(.signalAverage) == .average)
        #expect(FlagMeterChoice(.maxBin) == .maxBin)
        #expect(FlagMeterChoice.allCases.map(\.label) == ["Peak", "Average", "Max Bin"])
        #expect(FlagMeterChoice(SMeterSettings.desktopDefaults.rxMode).tag == nil)
        #expect(FlagMeterChoice.average.tag == "AV")
        #expect(FlagMeterChoice.maxBin.tag == "MAX")
    }

    @Test("A choice sets the RX Mode; Peak keeps Signal Peak when it is on")
    func setsRxMode() {
        #expect(FlagMeterChoice.average.rxMode(from: .signal) == .signalAverage)
        #expect(FlagMeterChoice.maxBin.rxMode(from: .signalPeak) == .maxBin)
        #expect(FlagMeterChoice.peak.rxMode(from: .maxBin) == .signal)
        #expect(FlagMeterChoice.peak.rxMode(from: .signalAverage) == .signal)
        #expect(FlagMeterChoice.peak.rxMode(from: .signalPeak) == .signalPeak)
    }

    @Test("The flag reads the peak, the average or this device's Max Bin, and never the Core's no-reading value")
    func level() {
        func level(_ mode: SMeterRxMode, maxBin: Double? = -70) -> Double? {
            FlagMeterChoice.level(mode: mode, peakDbm: -80, averageDbm: -90, legacyDbm: -95, maxBinDbm: maxBin)
        }
        #expect(level(.signal) == -80)
        #expect(level(.signalPeak) == -80)
        #expect(level(.signalAverage) == -90)
        #expect(level(.maxBin) == -70)
        #expect(level(.maxBin, maxBin: nil) == nil)
        #expect(FlagMeterChoice.level(mode: .signal, peakDbm: -400, averageDbm: -90, legacyDbm: -95,
                                      maxBinDbm: nil) == nil)
        #expect(FlagMeterChoice.level(mode: .signalAverage, peakDbm: -80, averageDbm: .nan, legacyDbm: -95,
                                      maxBinDbm: nil) == nil)
    }

    @Test("A Core that sends only signalStrengthDbm still lights the flag in Peak and Average")
    func olderCore() {
        #expect(FlagMeterChoice.level(mode: .signal, peakDbm: nil, averageDbm: nil, legacyDbm: -95,
                                      maxBinDbm: nil) == -95)
        #expect(FlagMeterChoice.level(mode: .signalAverage, peakDbm: nil, averageDbm: nil, legacyDbm: -95,
                                      maxBinDbm: nil) == -95)
    }
}
