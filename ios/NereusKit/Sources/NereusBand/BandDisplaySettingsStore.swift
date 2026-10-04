// NereusSDR for iOS: keeps each pan's display settings on this phone
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import os

/// Each pan's ``BandDisplaySettings``, kept on this phone in `UserDefaults`
/// and never sent to the Core (display settings are each device's own).
/// Stored as JSON under the app's `phone.` prefix, one key per pan. A
/// setting a stored value lacks (one added by a later version) takes the
/// desktop's default.
@MainActor
public final class BandDisplaySettingsStore {
    /// The key prefix, under the app's own `phone.` prefix.
    public static let keyPrefix = "phone.band.display."

    private let defaults: UserDefaults
    private static let logger = Logger(subsystem: "NereusSDR", category: "band")

    public init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
    }

    /// The settings of pan `panId`, the desktop's defaults when none are kept.
    public func settings(forPan panId: String) -> BandDisplaySettings {
        guard let data = defaults.data(forKey: Self.keyPrefix + panId) else {
            return .desktopDefaults
        }
        do {
            // Stored values over the defaults, so a missing member keeps its default.
            let base = try JSONEncoder().encode(BandDisplaySettings.desktopDefaults)
            guard var merged = try JSONSerialization.jsonObject(with: base) as? [String: Any],
                  var stored = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
                return .desktopDefaults
            }
            // Which way the band is turned is the screen's now, not what was kept.
            stored.removeValue(forKey: "sideways")
            Self.readEarlierStrip(&stored)
            Self.readEarlierDefaults(&stored)
            Self.readEarlierLevelMode(&stored)
            Self.readRewindAsOffered(&stored)
            merged.merge(stored) { _, new in new }
            let json = try JSONSerialization.data(withJSONObject: merged)
            return try JSONDecoder().decode(BandDisplaySettings.self, from: json)
        } catch {
            Self.logger.warning("A pan's display settings could not be read; the defaults apply")
            return .desktopDefaults
        }
    }

    /// Settings kept before the strip's size (D79) had the strip on or
    /// off: on reads as Small, off as Off. The plan they chose on this
    /// phone is dropped, since the plan is now the Core's.
    private static func readEarlierStrip(_ stored: inout [String: Any]) {
        if let on = stored.removeValue(forKey: "bandPlanStrip") as? Bool, stored["bandPlanSize"] == nil {
            stored["bandPlanSize"] = (on ? BandPlanSize.small : BandPlanSize.off).rawValue
        }
        stored.removeValue(forKey: "bandPlanId")
    }

    /// A look-back depth kept before the phone offered only the Core's
    /// depths reads as the nearest offered one.
    private static func readRewindAsOffered(_ stored: inout [String: Any]) {
        if let seconds = stored["rewindSeconds"] as? Int, !BandDisplaySettings.rewindChoices.contains(seconds) {
            stored["rewindSeconds"] = BandDisplaySettings.nearestRewind(seconds)
        }
    }

    /// Settings kept before the three level switches held one mode: it
    /// reads as the switches that choose it, the others at their defaults.
    private static func readEarlierLevelMode(_ stored: inout [String: Any]) {
        guard let raw = stored.removeValue(forKey: "waterfallLevelMode") as? String,
              stored["clarityEnabled"] == nil, stored["waterfallNfAgc"] == nil, stored["waterfallAgc"] == nil,
              let mode = BandDisplaySettings.WaterfallLevelMode(rawValue: raw) else {
            return
        }
        var settings = BandDisplaySettings.desktopDefaults
        settings.waterfallLevelMode = mode
        stored["clarityEnabled"] = settings.clarityEnabled
        stored["waterfallNfAgc"] = settings.waterfallNfAgc
        stored["waterfallAgc"] = settings.waterfallAgc
    }

    /// The phone's own defaults before the band took the desktop's colours
    /// and sizes (D83), each with the desktop's value that replaces it.
    static let earlierDefaults: [(key: String, earlier: String, desktop: String)] = [
        ("traceColour", "#22D3EE", "#00E5FF"),
        ("gridTextColour", "#F5E663FF", "#FFFF00FF"),
        ("rxZeroLineColour", "#FF6666FF", "#FF0000FF"),
        ("peakValueColour", "#7FB2FFFF", "#1E90FFFF"),
        ("noiseFloorTextColour", "#F5E663FF", "#FFFF00FF"),
        ("peakBlobColour", "#FF6B3DFF", "#FF4500FF"),
        ("peakBlobTextColour", "#FFFFFFFF", "#7FFF00FF"),
    ]
    static let earlierFillOpacity = 0.35
    static let earlierPeakValueDelayMs = 400

    /// Settings kept before the desktop's colours and sizes (D83): a value
    /// still at the phone's earlier default reads as the desktop's; one the
    /// operator changed is kept.
    private static func readEarlierDefaults(_ stored: inout [String: Any]) {
        guard stored["desktopValuesVersion"] == nil else {
            return
        }
        let desktop = BandDisplaySettings.desktopDefaults
        for entry in earlierDefaults {
            if let text = stored[entry.key] as? String, text.uppercased() == entry.earlier {
                stored[entry.key] = entry.desktop
            }
        }
        if let fill = stored["traceFillOpacity"] as? Double, abs(fill - earlierFillOpacity) < 1e-9 {
            stored["traceFillOpacity"] = desktop.traceFillOpacity
        }
        if let delay = stored["peakValueDelayMs"] as? Int, delay == earlierPeakValueDelayMs {
            stored["peakValueDelayMs"] = desktop.peakValueDelayMs
        }
    }

    public func setSettings(_ settings: BandDisplaySettings, forPan panId: String) {
        do {
            defaults.set(try JSONEncoder().encode(settings), forKey: Self.keyPrefix + panId)
        } catch {
            Self.logger.warning("A pan's display settings could not be kept")
        }
    }

    /// Forgets pan `panId`'s settings, so the defaults apply again.
    public func reset(pan panId: String) {
        defaults.removeObject(forKey: Self.keyPrefix + panId)
    }
}
