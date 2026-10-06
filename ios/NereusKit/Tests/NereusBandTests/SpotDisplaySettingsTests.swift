// NereusSDR for iOS: the spot display settings' desktop defaults, Auto mode and the label background the band draws
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusBand

/// Spec section 5.7 item 3, Task 62 Step 3: the desktop Spot Hub Display
/// tab's defaults and ranges (override colour yellow, background black at
/// 48 of 0 to 100 percent, Auto mode on), the background the band draws
/// behind a callsign, and an older stored value keeping the new members'
/// defaults.
@Suite struct SpotDisplaySettingsTests {
    @Test func theDesktopsDefaults() {
        let settings = SpotDisplaySettings.desktopDefaults
        #expect(settings.autoMode)
        #expect(!settings.overrideColours && settings.overrideColour == "#FFFF00")
        #expect(settings.overrideBackground && settings.backgroundColour == "#000000")
        #expect(settings.backgroundOpacity == 48)
        #expect(SpotDisplaySettings.backgroundOpacityRange == 0...100)
    }

    @Test func theLabelBackgroundFollowsTheOverrideItsColourAndItsOpacity() throws {
        var settings = SpotDisplaySettings.desktopDefaults
        let standard = try #require(settings.labelBackground)
        #expect(standard.colour == "#000000" && abs(standard.opacity - 0.48) < 1e-9)
        settings.backgroundColour = "#203040"
        settings.backgroundOpacity = 100
        let changed = try #require(settings.labelBackground)
        #expect(changed.colour == "#203040" && changed.opacity == 1)
        settings.backgroundOpacity = 0
        #expect(settings.labelBackground?.opacity == 0)
        settings.overrideBackground = false
        #expect(settings.labelBackground == nil)
    }

    @Test func autoModeAndTheColoursAreKeptAndAnOlderValueKeepsTheirDefaults() throws {
        var settings = SpotDisplaySettings.desktopDefaults
        settings.autoMode = false
        settings.overrideColour = "#00FF00"
        settings.backgroundColour = "#102030"
        settings.backgroundOpacity = 130
        let data = try JSONEncoder().encode(settings.clamped)
        let back = try JSONDecoder().decode(SpotDisplaySettings.self, from: data)
        #expect(!back.autoMode && back.overrideColour == "#00FF00" && back.backgroundColour == "#102030")
        #expect(back.backgroundOpacity == 100)
        // A value stored before Auto mode was kept on the phone.
        let older = try JSONDecoder().decode(SpotDisplaySettings.self, from: Data(#"{"maxLevels": 4}"#.utf8))
        #expect(older.autoMode && older.maxLevels == 4)
    }
}
