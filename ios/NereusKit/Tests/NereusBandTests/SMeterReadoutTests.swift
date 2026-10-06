// NereusSDR for iOS: the Multimeter page's decimal point and units change every printed signal reading exactly
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusModels
import Testing
@testable import NereusBand

/// Display V12's Multimeter rows (JJ, 2026-09-29): Show decimal point and
/// Display units reach the phone's S-meter readings. The fixture's meter
/// puts S0 at -130 dBm, 5 dB an S-unit, S9 at -85 dBm.
@Suite struct SMeterReadoutTests {
    private static let withDecimal = SMeterReadout(unit: .dBm, showDecimal: true)
    private static let whole = SMeterReadout(unit: .dBm, showDecimal: false)

    @Test("the desktop's defaults: dBm with the decimal point, and the options in the Core's order")
    func defaults() {
        #expect(SMeterReadout.desktopDefaults == SMeterReadout(unit: .dBm, showDecimal: true))
        #expect(SMeterSettings.desktopDefaults.readout == .desktopDefaults)
        #expect(SMeterUnit.allCases == [.sUnits, .dBm, .microvolts])
        #expect(SMeterUnit.allCases.map(\.rawValue) == ["S", "dBm", "uV"])
    }

    @Test("S-units: a tenth of a unit with the decimal point, the nearest unit without it")
    func sUnits() throws {
        let meter = try SMeterFixtures.meters().sMeter
        let s = SMeterReadout(unit: .sUnits, showDecimal: true)
        let sWhole = SMeterReadout(unit: .sUnits, showDecimal: false)
        #expect(s.sUnits(dbm: -103.5, meter: meter) == "S5.3")
        #expect(sWhole.sUnits(dbm: -103.5, meter: meter) == "S5")
        #expect(s.sUnits(dbm: -71.6, meter: meter) == "S9+13.4")
        #expect(sWhole.sUnits(dbm: -71.6, meter: meter) == "S9+13")
        #expect(s.sUnits(dbm: -85, meter: meter) == "S9.0")
        #expect(sWhole.sUnits(dbm: -85, meter: meter) == "S9")
        #expect(s.sUnits(dbm: -140, meter: meter) == "S0.0")
        #expect(sWhole.sUnits(dbm: -140, meter: meter) == "S0")
        // The printed reading in S is the S-unit reading; it needs the Core's meter.
        #expect(s.text(dbm: -103.5, meter: meter) == "S5.3")
        #expect(sWhole.text(dbm: -71.6, meter: meter) == "S9+13")
        #expect(s.text(dbm: -103.5, meter: nil) == nil)
        #expect(s.spoken(dbm: -103.5, meter: meter) == "S5.3")
        #expect(s.noReadingText == "--")
    }

    @Test("dBm: one decimal place with the decimal point, a whole number without it")
    func dBm() throws {
        let meter = try SMeterFixtures.meters().sMeter
        #expect(Self.withDecimal.text(dbm: -85.64, meter: meter) == "-85.6 dBm")
        #expect(Self.whole.text(dbm: -85.64, meter: meter) == "-86 dBm")
        #expect(Self.withDecimal.text(dbm: -61, meter: nil) == "-61.0 dBm")
        #expect(Self.whole.text(dbm: -61, meter: nil) == "-61 dBm")
        #expect(Self.whole.spoken(dbm: -85.64, meter: meter) == "-86 dBm")
        #expect(Self.withDecimal.noReadingText == "-- dBm")
    }

    @Test("uV: microvolts at 50 ohms, two places under one microvolt and one above with the decimal point")
    func microvolts() throws {
        let meter = try SMeterFixtures.meters().sMeter
        #expect(abs(SMeterReadout.microvolts(dbm: -107) - 0.998_814_876) < 1e-6)
        #expect(abs(SMeterReadout.microvolts(dbm: -73) - 50.059_326_485) < 1e-6)
        let uV = SMeterReadout(unit: .microvolts, showDecimal: true)
        let uVWhole = SMeterReadout(unit: .microvolts, showDecimal: false)
        #expect(uV.text(dbm: -73, meter: meter) == "50.1 uV")
        #expect(uVWhole.text(dbm: -73, meter: meter) == "50 uV")
        #expect(uV.text(dbm: -107, meter: meter) == "1.00 uV")
        #expect(uV.text(dbm: -113, meter: meter) == "0.50 uV")
        #expect(uVWhole.text(dbm: -113, meter: meter) == "1 uV")
        #expect(uV.text(dbm: -127, meter: nil) == "0.10 uV")
        #expect(uVWhole.text(dbm: -127, meter: nil) == "0 uV")
        #expect(uVWhole.text(dbm: -13, meter: nil) == "50059 uV")
        #expect(uV.spoken(dbm: -73, meter: meter) == "50.1 microvolts")
        #expect(uV.noReadingText == "-- uV")
    }

    @Test("the S-meter face: the S-units on the left, the reading in the chosen unit on the right")
    func face() throws {
        let meters = try SMeterFixtures.meters()
        func shown(_ unit: SMeterUnit, decimal: Bool, dbm: Double = -103.5) -> SMeterDisplay {
            var state = SMeterState(settings: SMeterSettings(unit: unit, showDecimal: decimal))
            state.apply(SMeterReadings(peakDbm: dbm), at: 0)
            return state.display(at: 0, meters: meters)
        }
        var face = shown(.dBm, decimal: true)
        #expect((face.left, face.right, face.spoken) == ("S5.3", "-103.5 dBm", "S5.3, -103.5 dBm"))
        face = shown(.dBm, decimal: false)
        #expect((face.left, face.right, face.spoken) == ("S5", "-104 dBm", "S5, -104 dBm"))
        face = shown(.microvolts, decimal: true, dbm: -73)
        #expect((face.left, face.right, face.spoken) == ("S9+12.0", "50.1 uV", "S9+12.0, 50.1 microvolts"))
        face = shown(.microvolts, decimal: false, dbm: -73)
        #expect((face.left, face.right) == ("S9+12", "50 uV"))
        // S: the left readout already says it, so the right one stays empty.
        face = shown(.sUnits, decimal: true)
        #expect((face.left, face.right, face.spoken) == ("S5.3", "", "S5.3"))
        face = shown(.sUnits, decimal: false)
        #expect((face.left, face.right, face.spoken) == ("S5", "", "S5"))
        // No reading: dashes in the chosen unit.
        face = shown(.microvolts, decimal: true, dbm: -400)
        #expect((face.left, face.right, face.spoken) == ("--", "-- uV", "No signal reading"))
        face = shown(.sUnits, decimal: true, dbm: -400)
        #expect((face.left, face.right) == ("--", ""))
        // The needle and the scales do not move with the unit.
        #expect(shown(.sUnits, decimal: false).needle == shown(.microvolts, decimal: true).needle)
        #expect(shown(.sUnits, decimal: false).receiveScale == shown(.dBm, decimal: true).receiveScale)
    }

    @Test("choosing the unit and the decimal point changes the face at once; transmit readouts stay as they are")
    func choosing() throws {
        let meters = try SMeterFixtures.meters()
        var state = SMeterState()
        state.apply(SMeterReadings(peakDbm: -85.64), at: 0)
        #expect(state.display(at: 0, meters: meters).right == "-85.6 dBm")
        state.setShowDecimal(false)
        #expect(state.display(at: 0, meters: meters).right == "-86 dBm")
        state.chooseUnit(.microvolts)
        #expect(state.display(at: 0, meters: meters).right == "12 uV")
        #expect(state.settings.readout == SMeterReadout(unit: .microvolts, showDecimal: false))
        state.apply(SMeterReadings(transmitting: true, transmitSent: true, forwardWatts: 60), at: 1)
        #expect(state.display(at: 1, meters: meters).right == "60 W")
    }

    @Test("the unit and the decimal point persist on this device, the unit as the desktop's text")
    @MainActor
    func persist() throws {
        let name = "SMeterReadoutTests.\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: name))
        defer { defaults.removePersistentDomain(forName: name) }
        let store = SMeterSettingsStore(defaults: defaults)
        #expect(store.settings.readout == .desktopDefaults)
        var chosen = SMeterSettings.desktopDefaults
        chosen.readout = SMeterReadout(unit: .microvolts, showDecimal: false)
        store.setSettings(chosen)
        #expect(SMeterSettingsStore(defaults: defaults).settings == chosen)
        let kept = try #require(defaults.data(forKey: SMeterSettingsStore.key))
        let object = try #require(try JSONSerialization.jsonObject(with: kept) as? [String: Any])
        #expect(object["unit"] as? String == "uV")
        #expect(object["showDecimal"] as? Bool == false)
        // An unknown unit, or a store from before these rows, takes the desktop's defaults.
        let older = try JSONSerialization.data(withJSONObject: ["face": "carbon", "unit": "volts"])
        defaults.set(older, forKey: SMeterSettingsStore.key)
        let read = SMeterSettingsStore(defaults: defaults).settings
        #expect(read.face == .carbon)
        #expect(read.readout == .desktopDefaults)
    }
}
