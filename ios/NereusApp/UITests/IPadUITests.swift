// NereusSDR for iOS: the iPad upright shows its applets below the band; on its side the corner button hides and shows the column
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class IPadUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    override func tearDown() {
        XCUIDevice.shared.orientation = .portrait
    }

    /// Spec section 5.6, D30, D31: upright the S-meter, RX and TX sit below
    /// the band with no column button; turned on its side they go to the
    /// column on the right, whose corner button hides it and brings it back.
    @MainActor
    func testUprightFrontPanelAndOnItsSideTheColumn() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The iPad's layouts are tried on an iPad")
        XCUIDevice.shared.orientation = .portrait
        let app = XCUIApplication()
        // The band and its tabs, with no Core (a debug build's launch argument).
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let frontPanel = app.descendants(matching: .any)["frontPanel"]
        XCTAssertTrue(frontPanel.waitForExistence(timeout: 30))
        XCTAssertTrue(app.descendants(matching: .any)["analogSMeter"].exists)
        XCTAssertTrue(app.descendants(matching: .any)["rxPanelDrawer"].exists)
        XCTAssertTrue(app.descendants(matching: .any)["txPanelDrawer"].exists)
        XCTAssertTrue(app.otherElements["toolbarIPad"].exists)
        XCTAssertFalse(app.buttons["appletColumnButton"].exists)
        XCTAssertFalse(app.buttons["rxPanel"].exists)
        XCTAssertFalse(app.buttons["txPanel"].exists)
        // The band sits over the applets.
        let band = app.buttons["ptt"]
        XCTAssertTrue(band.exists)
        XCTAssertLessThan(band.frame.maxY, frontPanel.frame.minY)

        XCUIDevice.shared.orientation = .landscapeLeft
        let column = app.descendants(matching: .any)["appletColumn"]
        XCTAssertTrue(column.waitForExistence(timeout: 20))
        XCTAssertFalse(frontPanel.exists)
        let button = app.buttons["appletColumnButton"]
        XCTAssertTrue(button.exists)
        XCTAssertEqual(button.label, "Applet column")
        XCTAssertEqual(button.value as? String, "Shown")
        // The column is on the band's right, under the corner button.
        XCTAssertGreaterThan(column.frame.minX, app.windows.firstMatch.frame.midX)
        button.tap()
        XCTAssertTrue(column.waitForNonExistence(timeout: 10))
        XCTAssertEqual(button.value as? String, "Hidden")
        button.tap()
        XCTAssertTrue(column.waitForExistence(timeout: 10))
        XCTAssertEqual(button.value as? String, "Shown")
    }

    /// D86, D78: the ☰ on the S-meter's title bar opens its menu, RX Mode,
    /// TX Mode, Peak Hold and Meter Face; with no Core, Max Bin is disabled
    /// with its reason; a press and hold on the meter opens the same menu,
    /// and a face chosen there takes.
    @MainActor
    func testMeterMenuFromItsButtonAndPressAndHold() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The analog S-meter is the iPad's")
        XCUIDevice.shared.orientation = .portrait
        let app = XCUIApplication()
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let meter = app.descendants(matching: .any)["analogSMeter"]
        XCTAssertTrue(meter.waitForExistence(timeout: 30))
        let menuButton = app.buttons["sMeterMenu"]
        XCTAssertTrue(menuButton.exists)
        XCTAssertEqual(menuButton.label, "Meter menu")

        menuButton.tap()
        for part in ["RX Mode", "TX Mode", "Peak Hold", "Meter Face"] {
            XCTAssertTrue(app.buttons[part].waitForExistence(timeout: 5), part)
        }
        shoot(app, "ipad-meter-menu")
        app.buttons["RX Mode"].tap()
        let maxBin = choice(app, "Max Bin")
        XCTAssertTrue(maxBin.waitForExistence(timeout: 5))
        XCTAssertFalse(maxBin.isEnabled)
        XCTAssertTrue(choice(app, "Sig Avg").isEnabled)
        shoot(app, "ipad-meter-menu-rx-mode")
        choice(app, "Signal Peak").tap()
        XCTAssertTrue(app.buttons["RX Mode"].waitForNonExistence(timeout: 5))

        // Press and hold on the meter: the same menu.
        meter.press(forDuration: 1.2)
        XCTAssertTrue(app.buttons["Meter Face"].waitForExistence(timeout: 5))
        XCTAssertTrue(app.buttons["TX Mode"].exists)
        shoot(app, "ipad-meter-press-and-hold")
        app.buttons["Meter Face"].tap()
        let classic = choice(app, "Classic")
        XCTAssertTrue(classic.waitForExistence(timeout: 5))
        classic.tap()
        XCTAssertTrue(app.buttons["Meter Face"].waitForNonExistence(timeout: 5))
        shoot(app, "ipad-meter-classic")

        // Back to the desktop's defaults for the tests that follow.
        menuButton.tap()
        XCTAssertTrue(app.buttons["Meter Face"].waitForExistence(timeout: 5))
        app.buttons["Meter Face"].tap()
        let cream = choice(app, "Aged Cream")
        XCTAssertTrue(cream.waitForExistence(timeout: 5))
        cream.tap()
        menuButton.tap()
        XCTAssertTrue(app.buttons["RX Mode"].waitForExistence(timeout: 5))
        app.buttons["RX Mode"].tap()
        let signal = app.buttons.matching(NSPredicate(format: "label == 'Signal' OR label BEGINSWITH 'Signal,'"))
            .firstMatch
        XCTAssertTrue(signal.waitForExistence(timeout: 5))
        signal.tap()
    }

    /// A menu item whose label starts with `title` (a reason may follow it).
    @MainActor
    private func choice(_ app: XCUIApplication, _ title: String) -> XCUIElement {
        app.buttons.matching(NSPredicate(format: "label BEGINSWITH %@", title)).firstMatch
    }

    /// Writes the screen to `NEREUS_IPAD_SHOTS` when it names a directory,
    /// and attaches it to the test's results.
    @MainActor
    private func shoot(_ app: XCUIApplication, _ name: String) {
        let shot = app.screenshot()
        let attachment = XCTAttachment(screenshot: shot)
        attachment.name = name
        attachment.lifetime = .keepAlways
        add(attachment)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty {
            try? shot.pngRepresentation.write(to: URL(fileURLWithPath: directory).appendingPathComponent("\(name).png"))
        }
    }
}
