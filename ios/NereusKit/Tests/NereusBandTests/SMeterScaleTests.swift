// NereusSDR for iOS: the S-meter's scales: the desktop's layout of the Core's S-meter and the transmit scales
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels
import Testing
@testable import NereusBand

/// D86, D83: the receive scale puts S9 at 60% of the arc; each transmit
/// scale has the desktop meter's range, ticks and red point.
@Suite struct SMeterScaleTests {
    @Test("S0 to S9 take the left 60% of the arc, S9 to S9+60 the rest")
    func receiveFraction() throws {
        let s = try SMeterFixtures.meters().sMeter
        #expect(SMeterScale.receiveFraction(dbm: -130, meter: s) == 0)
        #expect(SMeterScale.receiveFraction(dbm: -200, meter: s) == 0)
        #expect(abs(SMeterScale.receiveFraction(dbm: -85, meter: s) - 0.6) < 1e-9)
        #expect(abs(SMeterScale.receiveFraction(dbm: -107.5, meter: s) - 0.3) < 1e-9)
        #expect(abs(SMeterScale.receiveFraction(dbm: -55, meter: s) - 0.8) < 1e-9)
        #expect(SMeterScale.receiveFraction(dbm: 10, meter: s) == 1)
    }

    @Test("the power scale ticks every 10 W below 600 W, and labels by face")
    func power() throws {
        let power = try SMeterFixtures.meters().rfPower
        let classic = SMeterScale.transmit(.power, power: power, style: .classic)
        #expect(classic.ticks.count == 13)
        #expect(classic.ticks.compactMap(\.label) == ["0", "40", "80", "100", "120"])
        #expect(classic.ticks.filter(\.red).count == 3)
        #expect(abs((classic.redFrom ?? 0) - 100.0 / 120.0) < 1e-9)
        let vintage = SMeterScale.transmit(.power, power: power, style: .vintage)
        #expect(vintage.ticks.compactMap(\.label) == ["0", "20", "40", "60", "80", "100", "120"])

        let amp = try SMeterFixtures.meters(maxW: 2000, redFromW: 1500).rfPower
        let big = SMeterScale.transmit(.power, power: amp, style: .classic)
        #expect(big.ticks.count == 21)
        #expect(big.ticks.compactMap(\.label) == ["0", "500", "1k", "1.5k", "2k"])
        #expect(SMeterScale.transmit(.power, power: nil, style: .classic).ticks.isEmpty)
    }

    @Test("SWR runs 1 to 3, red from 2.5")
    func swr() {
        let classic = SMeterScale.transmit(.swr, power: nil, style: .classic)
        #expect(classic.ticks.compactMap(\.label) == ["1", "1.5", "2", "2.5", "3"])
        #expect(classic.ticks.filter(\.red).compactMap(\.label) == ["2.5", "3"])
        #expect(classic.redFrom == 0.75)
        let vintage = SMeterScale.transmit(.swr, power: nil, style: .vintage)
        #expect(vintage.ticks.count == 21)
        #expect(vintage.ticks.compactMap(\.label) == ["1", "1.5", "2", "2.5", "3"])
        #expect(SMeterScale.transmitFraction(2, mode: .swr, power: nil) == 0.5)
        #expect(SMeterScale.transmitFraction(9, mode: .swr, power: nil) == 1)
    }

    @Test("Level runs -40 to +5 dB, red from 0; Compression -25 to 0 dB, no red")
    func levelAndCompression() {
        let level = SMeterScale.transmit(.level, power: nil, style: .vintage)
        #expect(level.ticks.count == 10)
        #expect(level.ticks.compactMap(\.label) == ["-40", "-30", "-20", "-10", "0"])
        #expect(abs((level.redFrom ?? 0) - 40.0 / 45.0) < 1e-9)
        let classicLevel = SMeterScale.transmit(.level, power: nil, style: .classic)
        #expect(classicLevel.ticks.filter(\.red).compactMap(\.label) == ["0"])

        let compression = SMeterScale.transmit(.compression, power: nil, style: .vintage)
        #expect(compression.ticks.count == 26)
        #expect(compression.ticks.compactMap(\.label) == ["-25", "-20", "-15", "-10", "-5", "0"])
        #expect(compression.redFrom == nil)
        #expect(compression.ticks.allSatisfy { !$0.red })
        #expect(SMeterScale.transmitFraction(-30, mode: .compression, power: nil) == 0)
    }

    @Test("the vintage receive band starts just past S9, so the 9 stays in ink")
    func vintageRed() throws {
        let scale = SMeterScale.receive(try SMeterFixtures.meters().sMeter, style: .vintage)
        #expect(scale.ticks.first { $0.label == "9" }?.red == false)
        #expect(scale.ticks.filter(\.red).count == 6)
        #expect(scale.ticks.filter(\.major).count == 8)
    }
}
