// NereusSDR for iOS: local persistence for keyed overlay choices
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
@testable import NereusSDR
import XCTest

final class KeyedMetersModelTests: XCTestCase {
    func testEditsPersistOnThisDeviceWithoutChangingOtherTabs() async throws {
        try await MainActor.run {
            let suite = "KeyedMetersModelTests-\(UUID().uuidString)"
            let defaults = try XCTUnwrap(UserDefaults(suiteName: suite))
            defer { defaults.removePersistentDomain(forName: suite) }
            let first = KeyedMetersModel(defaults: defaults)
            let online = KeyedMeterAvailability(amp: .available, tuner: .available)
            first.choose(.amp, availability: online)
            first.replace(slot: 2, with: .alcGain, on: .amp, availability: online)
            first.replace(slot: 0, with: .eq, on: .radio, availability: online)
            let second = KeyedMetersModel(defaults: defaults)
            XCTAssertEqual(second.settings.page, .amp)
            XCTAssertEqual(second.settings.meters(on: .amp), [.ampPower, .ampSwr, .alcGain])
            XCTAssertEqual(second.settings.meters(on: .radio), [.eq, .swr, .mic])
            XCTAssertEqual(second.settings.meters(on: .tuner), [.tunerPower, .tunerSwr, .mic])
            let stored = defaults.data(forKey: KeyedMeterSettings.key)
            second.choose(.tuner, availability: KeyedMeterAvailability())
            second.replace(slot: 2, with: .tunerPower, on: .amp, availability: KeyedMeterAvailability())
            XCTAssertEqual(defaults.data(forKey: KeyedMeterSettings.key), stored)
            XCTAssertEqual(second.settings.page, .amp)
        }
    }
}
