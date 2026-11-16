// NereusSDR for iOS: a model a test builds starts clean and leaves nothing in the shared store
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
import NereusLink
@testable import NereusSDR
import Testing

/// Every test shares the test host's standard defaults, and they outlive the
/// run. A saved mute or route left there changes what a later test sees.
@Suite("Phone settings in tests", .serialized)
@MainActor
struct PhoneDefaultsIsolationTests {
    @Test("a model built with no settings writes nothing to the standard defaults")
    func leavesTheSharedStoreAlone() {
        let keys = [PhoneSettings.audioMutedKey, PhoneSettings.audioRouteKey, PhoneSettings.lastCoreKey,
                    PhoneSettings.navSetupKey, PhoneSettings.displayPillTabKey]
            .map { PhoneSettings.keyPrefix + $0 }
        let shared = UserDefaults.standard
        let before = keys.map { shared.object(forKey: $0) }
        defer {
            for (key, value) in zip(keys, before) {
                shared.set(value, forKey: key)
            }
        }

        let model = AppModel()
        let settings = model.phoneSettings
        model.setAudioMuted(!model.audioMuted)
        settings.audioRoute = "earpiece"
        settings.lastCore = "not-a-core"
        settings.navSetup = ["not-a-page"]
        settings.displayPillTab = settings.displayPillTab == .spectrum ? .waterfall : .spectrum

        for (key, value) in zip(keys, before) {
            #expect(shared.object(forKey: key) as? NSObject == value as? NSObject, "\(key) reached the standard defaults")
        }
    }

    @Test("a model built with no settings starts from the defaults, whatever an earlier one kept")
    func startsClean() throws {
        let suite = "PhoneDefaultsIsolationTests-\(UUID().uuidString)"
        let defaults = try #require(UserDefaults(suiteName: suite))
        defer { defaults.removePersistentDomain(forName: suite) }
        let display = BandDisplaySettingsStore(defaults: defaults)
        let shapes: [() -> AppModel] = [
            { AppModel() },
            { AppModel(displaySettings: display) },
            { AppModel(mediaPeerFactory: nil, displaySettings: display, mirrorClock: SystemLinkClock()) },
            { AppModel(platform: TransmitModel.Platform()) },
        ]
        for build in shapes {
            let first = build()
            first.setAudioMuted(true)
            first.phoneSettings.audioRoute = "speaker"

            let second = build()
            #expect(!second.audioMuted)
            #expect(second.phoneSettings.audioRoute == nil)
        }
    }
}
