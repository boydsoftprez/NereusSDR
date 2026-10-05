// NereusSDR for iOS: keeps keyed overlay preferences on this phone
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import Foundation

/// Local display choices only; no command or radio connection is used here.
@MainActor
final class KeyedMetersModel: ObservableObject {
    @Published private(set) var settings: KeyedMeterSettings
    private let defaults: UserDefaults

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        settings = KeyedMeterSettings(data: defaults.data(forKey: KeyedMeterSettings.key))
    }

    func choose(_ page: KeyedMeterPage, availability: KeyedMeterAvailability) {
        change { $0.choose(page, availability: availability) }
    }

    func replace(slot: Int, with meter: KeyedMeter, on page: KeyedMeterPage, availability: KeyedMeterAvailability) {
        change { $0.replace(slot: slot, with: meter, on: page, availability: availability) }
    }

    func swipe(direction: Int, availability: KeyedMeterAvailability) {
        change { $0.swipe(direction: direction, availability: availability) }
    }

    private func change(_ action: (inout KeyedMeterSettings) -> Bool) {
        var kept = settings
        guard action(&kept) else { return }
        settings = kept
        if let data = kept.data() { defaults.set(data, forKey: KeyedMeterSettings.key) }
    }
}
