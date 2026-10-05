// NereusSDR for iOS: how spots look on this phone's band, with the desktop's defaults
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// How spots look on the band (spec section 5.7 item 3, R-IOS-25): set on
/// each phone and never sent to the Core. The defaults and ranges are the
/// desktop's (D83): spots on, 3 levels of 1 to 10, starting halfway (0 to
/// 100 percent down the spectrum), 16 point text of 8 to 32, override
/// colours off (yellow when on), override background on in black at 48
/// percent (0 to 100), Auto mode on. Each spot source can be hidden from
/// this phone's band. The desktop keeps these on each computer, not at the
/// Core, and so does the phone.
public struct SpotDisplaySettings: Codable, Equatable, Sendable {
    public static let levelsRange = 1...10
    public static let startPercentRange = 0...100
    public static let fontSizeRange = 8...32
    public static let backgroundOpacityRange = 0...100

    public var enabled = true
    public var maxLevels = 3
    public var startPercent = 50
    public var fontSize = 16
    public var overrideColours = false
    /// The text colour while colours are overridden, `#RRGGBB`.
    public var overrideColour = "#FFFF00"
    public var overrideBackground = true
    /// The label background while it is overridden, `#RRGGBB`.
    public var backgroundColour = "#000000"
    public var backgroundOpacity = 48
    /// The spot sources hidden from this phone's band, by the source's
    /// label as the Core names it (`Cluster`, `RBN`, `POTA`, `PSK`, `FreeDV`).
    public var hiddenSources: Set<String> = []
    /// Auto mode: a tap on a spot also sets the slice to the spot's mode.
    public var autoMode = true

    public init() {}

    /// The desktop's defaults.
    public static let desktopDefaults = SpotDisplaySettings()

    /// The same settings with every number inside its range.
    public var clamped: SpotDisplaySettings {
        var copy = self
        copy.maxLevels = Self.clamp(maxLevels, Self.levelsRange)
        copy.startPercent = Self.clamp(startPercent, Self.startPercentRange)
        copy.fontSize = Self.clamp(fontSize, Self.fontSizeRange)
        copy.backgroundOpacity = Self.clamp(backgroundOpacity, Self.backgroundOpacityRange)
        return copy
    }

    /// The label's background on the band, or nil while it is not
    /// overridden: its `#RRGGBB` colour and its opacity from 0 to 1.
    public var labelBackground: (colour: String, opacity: Double)? {
        guard overrideBackground else {
            return nil
        }
        return (backgroundColour, Double(Self.clamp(backgroundOpacity, Self.backgroundOpacityRange)) / 100)
    }

    /// Whether spots from `source` show on this phone's band.
    public func shows(source: String) -> Bool {
        enabled && !hiddenSources.contains(source)
    }

    private static func clamp(_ value: Int, _ range: ClosedRange<Int>) -> Int {
        min(max(value, range.lowerBound), range.upperBound)
    }

    // A stored value that lacks a member (one added later) keeps its default.
    public init(from decoder: any Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let defaults = SpotDisplaySettings()
        enabled = try container.decodeIfPresent(Bool.self, forKey: .enabled) ?? defaults.enabled
        maxLevels = try container.decodeIfPresent(Int.self, forKey: .maxLevels) ?? defaults.maxLevels
        startPercent = try container.decodeIfPresent(Int.self, forKey: .startPercent) ?? defaults.startPercent
        fontSize = try container.decodeIfPresent(Int.self, forKey: .fontSize) ?? defaults.fontSize
        overrideColours = try container.decodeIfPresent(Bool.self, forKey: .overrideColours) ?? defaults.overrideColours
        overrideColour = try container.decodeIfPresent(String.self, forKey: .overrideColour) ?? defaults.overrideColour
        overrideBackground = try container.decodeIfPresent(Bool.self, forKey: .overrideBackground)
            ?? defaults.overrideBackground
        backgroundColour = try container.decodeIfPresent(String.self, forKey: .backgroundColour)
            ?? defaults.backgroundColour
        backgroundOpacity = try container.decodeIfPresent(Int.self, forKey: .backgroundOpacity)
            ?? defaults.backgroundOpacity
        hiddenSources = try container.decodeIfPresent(Set<String>.self, forKey: .hiddenSources)
            ?? defaults.hiddenSources
        autoMode = try container.decodeIfPresent(Bool.self, forKey: .autoMode) ?? defaults.autoMode
        self = clamped
    }
}
