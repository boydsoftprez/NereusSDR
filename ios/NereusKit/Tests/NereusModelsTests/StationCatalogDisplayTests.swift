// NereusSDR for iOS: tests for the Core's Setup > Display description, against the link's catalogue fixtures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
import Testing
@testable import NereusModels

/// Link document section 7.4 (`display`), R-IOS-06, R-IOS-18, R-IOS-27.
/// What the controls say is read from the Core's own fixtures; the numbers
/// the tests feed the rules are the tests' own.
@Suite struct StationCatalogDisplayTests {
    static func display(_ fixture: String = StationCatalogTests.fixtures[0]) throws -> StationCatalog.Display {
        let catalog = try #require(StationCatalog.parse(json: try StationCatalogTests.catalogueJson(fixture)))
        return try #require(catalog.display)
    }

    @Test func theControlsComeInTheDesktopPagesOrderWithWhereEachLives() throws {
        let display = try Self.display()
        let raw = try #require(try StationCatalogTests.object(try StationCatalogTests.catalogueJson(
            StationCatalogTests.fixtures[0]))["display"] as? [String: Any])
        let sent = try #require(raw["controls"] as? [[String: Any]])
        #expect(display.controls.map(\.label) == sent.map { $0["label"] as? String })
        #expect(display.controls.map(\.subscribe) == sent.map { $0["subscribe"] as? String })
        // The Core keeps the FFT's settings; the rest are each device's.
        let station = display.controls.filter(\.isStation).compactMap(\.settingsKey)
        #expect(station == ["DisplayFftSize", "DisplayFftWindow", "DisplayHzPerBinTarget", "DisplaySpectrumFps"])
        let others = display.controls.filter { !$0.isStation }
        #expect(others.allSatisfy { $0.isDevice })
        // Decimation writes no setting of the Core's.
        #expect(display.control(subscribe: "decimation")?.settingsKey == nil)
        #expect(display.control(settingsKey: "DisplayHzPerBinTarget")?.subscribe == "fftSize")
        #expect(display.fftPlan.minFftSize < display.fftPlan.maxFftSize)
        #expect(!display.binWidth.label.isEmpty)
    }

    @Test func bothRadiosDescribeTheSameDisplay() throws {
        let g2 = try Self.display(StationCatalogTests.fixtures[0])
        let hl2 = try Self.display(StationCatalogTests.fixtures[1])
        #expect(g2 == hl2)
    }

    @Test func aControlReadsAsTheCoreLabelsIt() throws {
        let display = try Self.display()
        let size = try #require(display.control(settingsKey: "DisplayFftSize"))
        #expect(size.kind == .slider && !size.options.isEmpty && size.range == nil)
        for option in size.options {
            #expect(size.text(option.value) == option.label)
        }
        let window = try #require(display.control(settingsKey: "DisplayFftWindow"))
        #expect(window.kind == .choice)
        #expect(window.text(window.options[1].value) == window.options[1].label)
        let target = try #require(display.control(settingsKey: "DisplayHzPerBinTarget"))
        let off = try #require(target.offValue)
        #expect(target.text(off) == target.offLabel)
        let range = try #require(target.range)
        let shown = target.text(range.min + range.step * 25)
        #expect(shown.hasSuffix(" \(target.unit ?? "")") && shown.contains("."))
    }

    @Test func aValueIsKeptInsideItsControl() throws {
        let display = try Self.display()
        let size = try #require(display.control(settingsKey: "DisplayFftSize"))
        let smallest = try #require(size.options.first?.value)
        let largest = try #require(size.options.last?.value)
        #expect(size.clamped(smallest + 1) == smallest)
        #expect(size.clamped(largest * 4) == largest)
        #expect(size.clamped(0) == smallest)
        let target = try #require(display.control(settingsKey: "DisplayHzPerBinTarget"))
        let range = try #require(target.range)
        #expect(target.clamped(range.max + 50) == range.max)
        #expect(target.clamped(range.min - 1) == range.min)
        #expect(target.clamped(range.min + range.step * 3.4) == range.min + range.step * 3)
        #expect(target.clamped(.nan) == target.defaultValue)
    }

    /// The FFT size to ask for (link 7.4, `fftPlan`), with the plan's own
    /// smallest and largest sizes.
    @Test func theSizeSliderIsTheFloorAndTheTargetHoldsTheBinWidth() throws {
        let plan = try Self.display().fftPlan
        let rate = 192_000.0
        // The whole rate across 1206 pixels: the floor decides.
        var result = plan.plan(sampleRateHz: rate, pixels: 1206, spanHz: rate, sizeSetting: 4096, hzPerBinTarget: 0)
        #expect(result.fftSize == 4096 && !result.fine)
        // Zoomed eight times: more bins than the floor, fine.
        result = plan.plan(sampleRateHz: rate, pixels: 1206, spanHz: rate / 8, sizeSetting: 4096, hzPerBinTarget: nil)
        #expect(result.fftSize == 16_384 && result.fine)
        // A target holds the width at or below it at any zoom.
        result = plan.plan(sampleRateHz: rate, pixels: 1206, spanHz: rate, sizeSetting: 4096, hzPerBinTarget: 5)
        #expect(result.fftSize == 65_536 && result.fine)
        #expect(rate / Double(result.fftSize) <= 5)
        // Never past the largest size, however fine the target.
        result = plan.plan(sampleRateHz: rate, pixels: 1206, spanHz: rate, sizeSetting: 4096, hzPerBinTarget: 0.01)
        #expect(result.fftSize == plan.maxFftSize)
        // A larger floor wins over a coarse view.
        result = plan.plan(sampleRateHz: rate, pixels: 100, spanHz: rate, sizeSetting: 70_000, hzPerBinTarget: 0)
        #expect(result.fftSize == 131_072 && !result.fine)
        #expect(plan.rounded(1) == plan.minFftSize)
    }

    @Test func theBinWidthIsThePansRateOverTheSize() throws {
        let width = try Self.display().binWidth
        // A G2 and an HL2 read differently for one size: the rule is the same.
        #expect(width.text(sampleRateHz: 192_000, fftSize: 4096) == "46.875")
        #expect(width.text(sampleRateHz: 48_000, fftSize: 4096) == "11.719")
        #expect(width.text(sampleRateHz: 0, fftSize: 4096) == nil)
        #expect(width.text(sampleRateHz: 48_000, fftSize: 0) == nil)
    }

    @Test func anUnreadableDescriptionCostsOnlyItself() throws {
        var raw = try StationCatalogTests.object(try StationCatalogTests.catalogueJson(StationCatalogTests.fixtures[0]))
        let plain = try #require(StationCatalog.parse(json: try StationCatalogTests.text(raw)))
        var display = try #require(raw["display"] as? [String: Any])
        var controls = try #require(display["controls"] as? [[String: Any]])
        // One control without a label is left out; the others stand.
        controls[1].removeValue(forKey: "label")
        display["controls"] = controls
        raw["display"] = display
        let partial = try #require(StationCatalog.parse(json: try StationCatalogTests.text(raw)))
        #expect(partial.display?.controls.count == (plain.display?.controls.count ?? 0) - 1)
        #expect(partial.display?.control(settingsKey: "DisplayFftWindow") == nil)
        // A description of the wrong shape is none, and the rest still reads.
        raw["display"] = ["controls": "Size"]
        let unreadable = try #require(StationCatalog.parse(json: try StationCatalogTests.text(raw)))
        #expect(unreadable.display == nil)
        #expect(unreadable.modes == plain.modes)
    }
}
