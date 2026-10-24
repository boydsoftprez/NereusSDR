// NereusSDR for iOS: the Display pill's Setup defaults link opens each tab's Setup page on its pan, and NB Blank agrees between the two
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class DisplayPillUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    /// Each tab's first control on the Setup page its Setup defaults link
    /// opens, and whether that page has the pans' strip: FFT & Engine holds
    /// only rows every pan shares, so it has none.
    private static let firstControls: [(tab: String, control: String, strip: Bool)] = [
        ("spectrum", "display.spectrumDefaults.detector", true),
        ("levels", "display.levels.scaleMode", true),
        ("waterfall", "display.waterfallDefaults.colorScheme", true),
        ("view", "display.threeD.renderMode", true),
        ("fft", "display.spectrumDefaults.fftSizing", false),
    ]

    /// R-IOS-55, DC4: Setup defaults on each tab opens that tab's Setup >
    /// Display page, with the pill's pan, Pan 1, chosen on the strip.
    @MainActor
    func testSetupDefaultsOpensEachTabsPageOnThePan() throws {
        let app = launch()
        for (tab, control, strip) in Self.firstControls {
            openPill(app, tab: tab)
            let link = app.buttons["displayPillSetupDefaults"]
            XCTAssertTrue(link.waitForExistence(timeout: 5), "The \(tab) tab has no Setup defaults link")
            link.tap()
            let first = app.descendants(matching: .any)[control]
            XCTAssertTrue(first.waitForExistence(timeout: 10), "Setup defaults on \(tab) did not open its page")
            let pan = app.descendants(matching: .any)["displayStrip.pan1"]
            guard strip else {
                XCTAssertFalse(pan.exists, "The \(tab) page shows a strip for rows every pan shares")
                backToBand(app)
                continue
            }
            XCTAssertTrue(pan.waitForExistence(timeout: 5), "The \(tab) page has no strip")
            XCTAssertTrue(pan.isSelected, "Setup defaults on \(tab) did not choose Pan 1 on the strip")
            XCTAssertFalse(app.descendants(matching: .any)["displayStrip.all"].isSelected)
            backToBand(app)
        }
    }

    /// DC4: NB Blank turned on in the sheet's Waterfall tab is on in Setup >
    /// Display > Waterfall for Pan 1; turned off there, the sheet shows it off
    /// and its threshold greyed with the reason.
    @MainActor
    func testNbBlankAgreesBetweenTheSheetAndSetup() throws {
        let app = launch()
        openPill(app, tab: "waterfall")
        // NB Blank's switch sits on its threshold's line, which names the
        // line's elements by the threshold's key.
        let pillSwitch = app.buttons.matching(NSPredicate(
            format: "identifier == 'displayPill.WaterfallBlankingThresholdDb' AND label == 'NB Blank' AND (value == 'On' OR value == 'Off')"))
            .firstMatch
        XCTAssertTrue(pillSwitch.waitForExistence(timeout: 5))
        // The simulator keeps this app's settings between runs: start from off.
        if pillSwitch.value as? String == "On" {
            pillSwitch.tap()
        }
        XCTAssertEqual(pillSwitch.value as? String, "Off")
        pillSwitch.tap()
        XCTAssertEqual(pillSwitch.value as? String, "On", "NB Blank did not turn on in the sheet")
        // The pan now keeps its own NB Blank: its row's dot shows.
        XCTAssertTrue(app.buttons["Pan 1's own NB Blank. Tap the dot to follow the Setup default again."].exists)

        app.buttons["displayPillSetupDefaults"].tap()
        let setupSwitch = app.switches["display.waterfall.blanking"]
        XCTAssertTrue(setupSwitch.waitForExistence(timeout: 10), "Setup's NB Blank is not on the page")
        XCTAssertTrue(app.descendants(matching: .any)["displayStrip.pan1"].isSelected)
        XCTAssertEqual(setupSwitch.value as? String, "1", "Setup does not show the sheet's NB Blank for Pan 1")
        // The Setup switch takes its tap on the track at the row's end;
        // bring the row on screen first.
        for _ in 0..<6 where !setupSwitch.isHittable {
            app.swipeUp()
        }
        setupSwitch.coordinate(withNormalizedOffset: CGVector(dx: 1, dy: 0.5))
            .withOffset(CGVector(dx: -26, dy: 0)).tap()
        let off = expectation(for: NSPredicate(format: "value == '0'"), evaluatedWith: setupSwitch)
        _ = XCTWaiter.wait(for: [off], timeout: 5)
        XCTAssertEqual(setupSwitch.value as? String, "0", "NB Blank did not turn off in Setup")

        backToBand(app)
        openPill(app, tab: "waterfall")
        XCTAssertTrue(pillSwitch.waitForExistence(timeout: 5))
        XCTAssertEqual(pillSwitch.value as? String, "Off", "The sheet does not show Setup's NB Blank")
        XCTAssertTrue(app.staticTexts["NB Blank is off, so there is no threshold to set."].exists,
                      "The threshold is not greyed with its reason")
    }

    /// Opens the Display sheet from the toolbar on `tab`.
    @MainActor
    private func openPill(_ app: XCUIApplication, tab: String) {
        let display = app.buttons["display"]
        XCTAssertTrue(display.waitForExistence(timeout: 30), "The toolbar has no Display button")
        display.tap()
        let tabButton = app.descendants(matching: .any)["displayPillTab.\(tab)"]
        XCTAssertTrue(tabButton.waitForExistence(timeout: 10), "The Display pill has no \(tab) tab")
        tabButton.tap()
    }

    /// Back to the band from Setup.
    @MainActor
    private func backToBand(_ app: XCUIApplication) {
        let band = app.otherElements["Sections"].buttons["Panadapter"]
        XCTAssertTrue(band.waitForExistence(timeout: 5))
        band.tap()
    }

    @MainActor
    private func launch() -> XCUIApplication {
        let app = XCUIApplication()
        // The band with the Core's catalogue and its version 29 Display
        // description, with no Core (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand"]
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launchEnvironment["NEREUS_UITEST_DISPLAY"] = root.appendingPathComponent("resources/setup/display.json").path
        app.launch()
        return app
    }
}
