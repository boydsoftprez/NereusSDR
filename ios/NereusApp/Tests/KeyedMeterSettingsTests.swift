// NereusSDR for iOS: saved keyed-meter choices and equipment availability
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
@testable import NereusSDR
import XCTest

final class KeyedMeterSettingsTests: XCTestCase {
    private let online = KeyedMeterAvailability(amp: .available, tuner: .available)

    func testNoEquipmentLeavesOnlyRadioAndCustom() {
        let availability = KeyedMeterAvailability()
        XCTAssertEqual(availability.visiblePages, [.radio, .custom])
        XCTAssertEqual(availability.selectablePages, [.radio, .custom])
        XCTAssertEqual(availability.pickerMeters, [.rfPower, .swr, .mic, .eq, .leveler, .levelerGain,
                                                  .cfc, .cfcGain, .alcGain, .alcGroup])
    }

    func testOfflineEquipmentStaysVisibleAndRetainsChoices() {
        var settings = KeyedMeterSettings()
        XCTAssertTrue(settings.choose(.amp, availability: online))
        XCTAssertTrue(settings.replace(slot: 2, with: .alcGain, on: .amp, availability: online))
        let offline = KeyedMeterAvailability(amp: .offline("Amplifier offline"), tuner: .available)
        XCTAssertEqual(offline.visiblePages, [.radio, .amp, .tuner, .custom])
        XCTAssertEqual(offline.selectablePages, [.radio, .tuner, .custom])
        XCTAssertEqual(offline.status(for: KeyedMeter.ampPower), .offline("Amplifier offline"))
        XCTAssertTrue(offline.pickerMeters.contains(.ampPower))
        XCTAssertEqual(settings.displayPage(availability: offline), .radio)
        XCTAssertEqual(settings.page, .amp)
        XCTAssertEqual(settings.meters(on: .amp), [.ampPower, .ampSwr, .alcGain])
        XCTAssertEqual(settings.displayPage(availability: online), .amp)
    }

    func testReplacingOneSlotDoesNotChangePageOrOtherSets() {
        var settings = KeyedMeterSettings()
        XCTAssertTrue(settings.replace(slot: 0, with: .eq, on: .radio, availability: online))
        XCTAssertEqual(settings.page, .radio)
        XCTAssertEqual(settings.meters(on: .radio), [.eq, .swr, .mic])
        XCTAssertEqual(settings.meters(on: .amp), [.ampPower, .ampSwr, .ampTemperature])
        XCTAssertTrue(settings.choose(.amp, availability: online))
        XCTAssertTrue(settings.replace(slot: 2, with: .alcGroup, on: .amp, availability: online))
        XCTAssertTrue(settings.choose(.radio, availability: online))
        XCTAssertEqual(settings.meters(on: .radio), [.eq, .swr, .mic])
        XCTAssertEqual(settings.meters(on: .amp), [.ampPower, .ampSwr, .alcGroup])
    }

    func testSwipeSkipsAbsentAndOfflinePagesAndWraps() {
        var settings = KeyedMeterSettings()
        let availability = KeyedMeterAvailability(amp: .offline("Offline"), tuner: .available)
        XCTAssertTrue(settings.swipe(direction: 1, availability: availability))
        XCTAssertEqual(settings.page, .tuner)
        XCTAssertTrue(settings.swipe(direction: 1, availability: availability))
        XCTAssertEqual(settings.page, .custom)
        XCTAssertTrue(settings.swipe(direction: 1, availability: availability))
        XCTAssertEqual(settings.page, .radio)
        XCTAssertTrue(settings.swipe(direction: -1, availability: KeyedMeterAvailability()))
        XCTAssertEqual(settings.page, .custom)
        XCTAssertFalse(settings.swipe(direction: 0, availability: online))
    }

    func testUnavailableSelectionsAndInvalidSlotsLeaveSettingsUnchanged() {
        var settings = KeyedMeterSettings()
        let original = settings
        XCTAssertFalse(settings.choose(.amp, availability: KeyedMeterAvailability()))
        XCTAssertFalse(settings.replace(slot: 0, with: .ampPower, on: .radio, availability: KeyedMeterAvailability()))
        XCTAssertFalse(settings.replace(slot: -1, with: .eq, on: .radio, availability: online))
        XCTAssertFalse(settings.replace(slot: 3, with: .eq, on: .radio, availability: online))
        XCTAssertFalse(settings.replace(slot: 0, with: .eq, on: .radio,
                                       availability: KeyedMeterAvailability(stagesReason: "Older Core")))
        XCTAssertEqual(settings, original)
    }

    func testSavedChoicesRoundTripEvenWithoutEquipment() throws {
        var settings = KeyedMeterSettings()
        settings.choose(.amp, availability: online)
        settings.replace(slot: 0, with: .eq, on: .amp, availability: online)
        settings.replace(slot: 1, with: .levelerGain, on: .custom, availability: online)
        let stored = try XCTUnwrap(settings.data())
        let restored = KeyedMeterSettings(data: stored)
        XCTAssertEqual(restored, settings)
        XCTAssertEqual(restored.displayPage(availability: KeyedMeterAvailability()), .radio)
        XCTAssertEqual(restored.meters(on: .amp), [.eq, .ampSwr, .ampTemperature])
        XCTAssertEqual(restored.meters(on: .custom), [.mic, .levelerGain, .alcGain])
    }

    func testUnknownSavedValuesRecoverPerSlotWithoutDiscardingKnownChoices() {
        let json = #"{"page":"Future","sets":{"Radio":["eq","Future","alcGroup"],"Amp":["ampPower"],"Custom":["cfcGain","eq","alcGain","mic"]}}"#
        let settings = KeyedMeterSettings(data: Data(json.utf8))
        XCTAssertEqual(settings.page, .radio)
        XCTAssertEqual(settings.meters(on: .radio), [.eq, .swr, .alcGroup])
        XCTAssertEqual(settings.meters(on: .amp), [.ampPower, .ampSwr, .ampTemperature])
        XCTAssertEqual(settings.meters(on: .custom), [.cfcGain, .eq, .alcGain])
        XCTAssertEqual(KeyedMeterSettings(data: Data("bad json".utf8)), KeyedMeterSettings())
    }
}
