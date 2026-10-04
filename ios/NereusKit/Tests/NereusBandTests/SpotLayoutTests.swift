// NereusSDR for iOS: where spots sit on the band: the start under the flags, the rows that fit, the +N badges
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CoreGraphics
import Foundation
import Testing
@testable import NereusBand

/// D13, spec section 5.1 item 10: spots start at the lower of the start
/// setting and just under the lowest flag, use only the rows that fit
/// (never more than the levels setting), and put the rest in +N badges.
/// Upright, with the flags at the top, that is halfway down.
@Suite struct SpotLayoutTests {
    /// Every character 10 points wide, so the boxes are easy to count.
    private static func width(_ text: String, _ size: CGFloat) -> CGFloat {
        CGFloat(text.count) * 10
    }

    /// A band 1 kHz per point, 7.200 MHz at x 0, `height` points of spectrum.
    private static func geometry(width: CGFloat = 400, height: CGFloat) -> BandGeometry {
        BandGeometry(centerHz: 7_200_000 + Double(width) * 500, spanHz: Double(width) * 1000,
                     size: CGSize(width: width, height: height), dbmRange: -140 ... -40)
    }

    private static func spot(_ id: String, kHz: Double, call: String = "K1ABC",
                             source: String = "Cluster") -> SpotLayout.Spot {
        SpotLayout.Spot(id: id, frequencyHz: 7_200_000 + kHz * 1000, call: call, source: source)
    }

    private static func layout(_ spots: [SpotLayout.Spot], flags: [FlagPlacement] = [], height: CGFloat = 400,
                               settings: SpotDisplaySettings = .desktopDefaults) -> SpotLayout.Result {
        SpotLayout.layout(spots: spots, flags: flags, geometry: geometry(height: height), settings: settings,
                          textWidth: width)
    }

    @Test func theDefaultsAreTheDesktops() {
        let settings = SpotDisplaySettings.desktopDefaults
        #expect(settings.enabled)
        #expect(settings.maxLevels == 3 && SpotDisplaySettings.levelsRange == 1...10)
        #expect(settings.startPercent == 50 && SpotDisplaySettings.startPercentRange == 0...100)
        #expect(settings.fontSize == 16 && SpotDisplaySettings.fontSizeRange == 8...32)
        #expect(!settings.overrideColours)
        #expect(settings.overrideBackground && settings.backgroundOpacity == 48)
        #expect(settings.hiddenSources.isEmpty)
    }

    @Test func uprightWithTheFlagsAtTheTopSpotsStartHalfwayDown() throws {
        // The active slice's full flag and a folded tag under it end at 145, above halfway (200).
        let flags: [FlagPlacement] = [.full(CGRect(x: 0, y: 0, width: 200, height: 111)),
                                      .folded(CGRect(x: 220, y: 117, width: 120, height: 28))]
        let result = Self.layout([Self.spot("1", kHz: 100)], flags: flags)
        #expect(result.startY == 200)
        #expect(result.levels == 3)
        #expect(result.rowHeight == 21)
        let label = try #require(result.labels.first)
        // Centred on its line: 5 characters, 50 points, plus 6.
        #expect(label.rect == CGRect(x: 100 - 28, y: 200, width: 56, height: 21))
        #expect(label.lineX == 100)
    }

    @Test func sidewaysSpotsStartUnderTheLowestFlagAndUseOnlyTheRowsThatFit() {
        // A short spectrum (sideways): the flag's foot, 111, is below halfway (80).
        let flags: [FlagPlacement] = [.full(CGRect(x: 0, y: 0, width: 200, height: 111))]
        let result = Self.layout([Self.spot("1", kHz: 300)], flags: flags, height: 160)
        #expect(result.startY == 115)
        // (160 - 115 - 21 - 2) / 21 is one row, with the badges' row under it.
        #expect(result.levels == 1)
        #expect(result.labels.first?.rect.minY == 115)
    }

    @Test func whereNoRowFitsUnderTheFlagsEverySpotGoesIntoABadgeInsideTheSpectrum() {
        let flags: [FlagPlacement] = [.full(CGRect(x: 0, y: 0, width: 200, height: 111))]
        let result = Self.layout([Self.spot("1", kHz: 300), Self.spot("2", kHz: 301)], flags: flags, height: 120)
        #expect(result.levels == 0)
        #expect(result.labels.isEmpty)
        #expect(result.badges.map(\.spotIds) == [["1", "2"]])
        // At the spectrum's foot, never on the waterfall below it.
        #expect(result.badges.first?.rect.maxY == CGFloat(120))
    }

    @Test func aSpectrumShorterThanARowShowsNoSpots() {
        let result = Self.layout([Self.spot("1", kHz: 300)], height: 15)
        #expect(result.labels.isEmpty && result.badges.isEmpty)
    }

    @Test func overlappingCallsignsDropARowAndTheRestGoIntoABadge() throws {
        let spots = (0..<5).map { Self.spot("\($0)", kHz: 100 + Double($0)) }
        let result = Self.layout(spots)
        // Three rows at 200, 221 and 242, then a badge of two under them.
        #expect(result.labels.map(\.rect.minY) == [200, 221, 242])
        #expect(result.labels.map(\.spotId) == ["0", "1", "2"])
        let badge = try #require(result.badges.first)
        #expect(result.badges.count == 1)
        #expect(badge.spotIds == ["3", "4"])
        #expect(badge.text == "+2")
        #expect(badge.rect.minY == CGFloat(200 + 63 + 2))
        // Its text is two points smaller; the badge sits at the pair's
        // average, clear of every label.
        #expect(badge.rect.width == 30)
        #expect(result.labels.allSatisfy { !FlagLayout.overlaps($0.rect, badge.rect) })
    }

    @Test func theLevelsSettingLimitsTheRows() {
        var settings = SpotDisplaySettings.desktopDefaults
        settings.maxLevels = 1
        let spots = (0..<3).map { Self.spot("\($0)", kHz: 100 + Double($0)) }
        let result = Self.layout(spots, settings: settings)
        #expect(result.levels == 1)
        #expect(result.labels.count == 1)
        #expect(result.badges.map(\.spotIds) == [["1", "2"]])
    }

    @Test func spotsFarApartShareNoBadgeTheyAreBinnedEvery40Points() {
        var settings = SpotDisplaySettings.desktopDefaults
        settings.maxLevels = 1
        // Two pairs: each pair's second spot overflows into its own bin.
        let spots = [Self.spot("a", kHz: 50), Self.spot("b", kHz: 51), Self.spot("c", kHz: 250),
                     Self.spot("d", kHz: 251)]
        let result = Self.layout(spots, settings: settings)
        #expect(result.badges.map(\.spotIds) == [["b"], ["d"]])
    }

    @Test func theStartAndTheFontFollowTheSettings() {
        var settings = SpotDisplaySettings.desktopDefaults
        settings.startPercent = 25
        settings.fontSize = 20
        let result = Self.layout([Self.spot("1", kHz: 100)], settings: settings)
        #expect(result.startY == 100)
        #expect(result.rowHeight == 25)
        #expect(result.labels.first?.rect.height == 25)
    }

    @Test func spotsOffTheBandHiddenOrSwitchedOffAreNotPlaced() {
        var settings = SpotDisplaySettings.desktopDefaults
        settings.hiddenSources = ["POTA"]
        let spots = [Self.spot("low", kHz: -5), Self.spot("high", kHz: 405), Self.spot("park", kHz: 100, source: "POTA"),
                     Self.spot("dx", kHz: 200)]
        #expect(Self.layout(spots, settings: settings).labels.map(\.spotId) == ["dx"])
        settings.enabled = false
        let off = Self.layout(spots, settings: settings)
        #expect(off.labels.isEmpty && off.badges.isEmpty)
    }

    @Test func settingsOutsideTheirRangesAreBroughtInside() throws {
        let json = #"{"maxLevels": 40, "fontSize": 2, "startPercent": 150}"#
        let settings = try JSONDecoder().decode(SpotDisplaySettings.self, from: Data(json.utf8))
        #expect(settings.maxLevels == 10 && settings.fontSize == 8 && settings.startPercent == 100)
        // A member the stored value lacks keeps its default.
        #expect(settings.overrideBackground && settings.backgroundOpacity == 48)
    }

    @Test func theSystemFontsBoldTextHasAWidth() {
        #expect(SpotLayout.boldTextWidth("VK2XYZ", 16) > 40)
        #expect(SpotLayout.boldTextWidth("VK2XYZ", 32) > SpotLayout.boldTextWidth("VK2XYZ", 16))
    }
}
