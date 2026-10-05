// NereusSDR for iOS: keeps the S-meter's menu choices on this device
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// The S-meter's ``SMeterSettings``, kept on this device in `UserDefaults`
/// as JSON under the app's `phone.` prefix and never sent to the Core; the
/// unit as the desktop keeps it (`S`, `dBm` or `uV`). A choice the stored
/// value lacks, or one this version does not know, takes the desktop's
/// default.
@MainActor
public final class SMeterSettingsStore {
    public static let key = "phone.smeter.settings"

    private let defaults: UserDefaults
    private static let logger = Logger(subsystem: "NereusSDR", category: "meter")

    public init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
    }

    /// The kept choices, the desktop's defaults where none are kept.
    public var settings: SMeterSettings {
        guard let data = defaults.data(forKey: Self.key),
              let stored = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return .desktopDefaults
        }
        var settings = SMeterSettings.desktopDefaults
        if let text = stored["rxMode"] as? String, let mode = SMeterRxMode(rawValue: text) {
            settings.rxMode = mode
        }
        if let text = stored["txMode"] as? String, let mode = SMeterTxMode(rawValue: text) {
            settings.txMode = mode
        }
        if let on = stored["peakHold"] as? Bool {
            settings.peakHold = on
        }
        if let text = stored["peakDecay"] as? String, let decay = SMeterPeakDecay(rawValue: text) {
            settings.peakDecay = decay
        }
        if let text = stored["face"] as? String, let face = SMeterFace(rawValue: text) {
            settings.face = face
        }
        if let text = stored["unit"] as? String, let unit = SMeterUnit(rawValue: text) {
            settings.unit = unit
        }
        if let on = stored["showDecimal"] as? Bool {
            settings.showDecimal = on
        }
        return settings
    }

    /// Keeps `settings` on this device.
    public func setSettings(_ settings: SMeterSettings) {
        do {
            defaults.set(try JSONEncoder().encode(settings), forKey: Self.key)
        } catch {
            Self.logger.warning("The S-meter's choices could not be kept")
        }
    }
}
