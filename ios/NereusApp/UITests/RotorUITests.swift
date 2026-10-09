// NereusSDR for iOS: the Tools tab's Rotor row and the Radio tab's Rotor accessory open the same Rotor page; Setup holds its setup
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class RotorUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    /// Both ways in reach one page: the same heading, and the same Turn, STOP
    /// and nudge controls. The setup is not on it; it is in Setup.
    @MainActor
    func testToolsRowAndAccessoriesRowOpenTheSameRotorPage() {
        let app = launch()
        let tabBar = app.otherElements["Sections"]
        XCTAssertTrue(tabBar.waitForExistence(timeout: 30))

        tabBar.buttons["Tools"].tap()
        let toolsRow = app.descendants(matching: .any)["tools.rotor"]
        reveal(toolsRow, in: app)
        XCTAssertTrue(toolsRow.isEnabled, "The rotor the fixture sets up is offered")
        toolsRow.tap()
        let fromTools = page(in: app)
        let toolsBack = app.buttons["connectBack"]
        XCTAssertTrue(toolsBack.waitForExistence(timeout: 10))
        XCTAssertEqual(toolsBack.label, "Back to Tools")
        // Back to the list, so the Radio tab's page is the only Rotor page.
        toolsBack.tap()
        XCTAssertTrue(app.descendants(matching: .any)["rotorPage"].waitForNonExistence(timeout: 10))

        tabBar.buttons["Radio"].tap()
        let accessoryRow = app.descendants(matching: .any)["accessory.rotor"]
        reveal(accessoryRow, in: app)
        accessoryRow.tap()
        let fromRadio = page(in: app)
        let radioBack = app.buttons["connectBack"]
        XCTAssertTrue(radioBack.waitForExistence(timeout: 10))
        XCTAssertEqual(radioBack.label, "Back to Radio")

        XCTAssertEqual(fromTools, fromRadio)
        XCTAssertEqual(fromRadio.heading, "047\u{00B0}")
    }

    /// Setup, CAT & Network, Rotor holds the rotor's setup, after RF-Kit as
    /// on the desktop: the controller, and the presets list with the Core's
    /// presets, Add, Edit and Save presets.
    @MainActor
    func testSetupCatNetworkRotorHoldsTheSetupAndPresets() {
        let app = launch()
        let tabBar = app.otherElements["Sections"]
        XCTAssertTrue(tabBar.waitForExistence(timeout: 30))
        tabBar.buttons["Setup"].tap()
        let category = app.buttons["CAT & Network"]
        reveal(category, in: app)
        category.tap()
        let row = app.buttons["Rotor"]
        XCTAssertTrue(row.waitForExistence(timeout: 10), "CAT & Network lists no Rotor page")
        row.tap()

        let driver = app.descendants(matching: .any)["rotor.setup.driver"]
        XCTAssertTrue(driver.waitForExistence(timeout: 10), "The Rotor page shows no controller")
        XCTAssertTrue(driver.isEnabled)
        // The fixture's three presets, in order, in the list.
        let first = app.textFields["rotor.setup.preset.0.name"]
        reveal(first, in: app)
        XCTAssertEqual(first.value as? String, "EU")
        XCTAssertEqual(app.textFields["rotor.setup.preset.0.heading"].value as? String, "45")
        XCTAssertEqual(app.textFields["rotor.setup.preset.2.name"].value as? String, "VK")
        for id in ["rotor.setup.presets.add", "rotor.setup.presets.edit", "rotor.setup.presets.save"] {
            let button = app.buttons[id]
            reveal(button, in: app)
            XCTAssertTrue(button.isEnabled, "\(id) is greyed")
        }
        // Edit shows a Remove control on each row; Remove takes the row out.
        app.buttons["rotor.setup.presets.edit"].tap()
        let remove = app.buttons["rotor.setup.preset.2.remove"]
        XCTAssertTrue(remove.waitForExistence(timeout: 5))
        remove.tap()
        XCTAssertTrue(app.textFields["rotor.setup.preset.2.name"].waitForNonExistence(timeout: 5))
    }

    /// What the Rotor page shows, to compare the two ways in.
    private struct Page: Equatable {
        let heading: String
        let status: String
        let controls: [String]
    }

    @MainActor
    private func page(in app: XCUIApplication) -> Page {
        let root = app.descendants(matching: .any)["rotorPage"]
        XCTAssertTrue(root.waitForExistence(timeout: 10), "The Rotor page did not open")
        let heading = app.descendants(matching: .any)["rotor.heading"]
        XCTAssertTrue(heading.waitForExistence(timeout: 10))
        // STOP sits between the two nudge buttons.
        let ccw = app.descendants(matching: .any)["rotor.nudge.0"]
        let stop = app.buttons["rotor.stop"]
        let cw = app.descendants(matching: .any)["rotor.nudge.1"]
        XCTAssertTrue(ccw.exists && stop.exists && cw.exists)
        XCTAssertLessThan(ccw.frame.midX, stop.frame.midX)
        XCTAssertLessThan(stop.frame.midX, cw.frame.midX)
        XCTAssertTrue(stop.isEnabled)
        let ids = ["rotor.dial", "rotor.shortPath", "rotor.longPath"]
            .filter { app.descendants(matching: .any)[$0].exists }
        // The setup moved to Setup, CAT & Network, Rotor.
        XCTAssertFalse(app.descendants(matching: .any)["rotor.setup.driver"].exists)
        return Page(heading: heading.value as? String ?? "",
                    status: app.descendants(matching: .any)["rotor.status"].label,
                    controls: ids)
    }

    /// Scrolls the page until `element` can be tapped.
    @MainActor
    private func reveal(_ element: XCUIElement, in app: XCUIApplication) {
        // A list builds its rows as they scroll in: swipe until it is there.
        var swipes = 0
        while !element.waitForExistence(timeout: swipes == 0 ? 10 : 2) && swipes < 6 {
            app.swipeUp()
            swipes += 1
        }
        XCTAssertTrue(element.exists, "\(element) is not on the page")
        swipes = 0
        while !element.isHittable && swipes < 6 {
            app.swipeUp()
            swipes += 1
        }
        XCTAssertTrue(element.isHittable, "\(element) could not be reached")
    }

    @MainActor
    private func launch() -> XCUIApplication {
        let app = XCUIApplication()
        // The band with the Core's catalogue and a connected rotor, with no Core
        // (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", "-NereusRotorFixture"]
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launch()
        return app
    }
}
