// NereusSDR for iOS: on a full-width iPad Setup's list sits on the left with the open page beside it; on a compact width it stays one list
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class SetupSplitUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    override func tearDown() {
        XCUIDevice.shared.orientation = .portrait
    }

    /// R-IOS-52, W8: in a full-screen iPad window the Setup list stays on the
    /// left and the open page sits beside it. Choosing a category opens it
    /// under its name with its first page shown; choosing a page shows it on
    /// the right and marks it in the list. The tab bar stays. Turned upright
    /// the window is still regular-width and the page stays open.
    @MainActor
    func testFullScreenShowsTheListBesideThePage() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The split is the iPad's")
        XCUIDevice.shared.orientation = .landscapeLeft
        let app = launch()
        let tabBar = app.otherElements["Sections"]
        XCTAssertTrue(tabBar.waitForExistence(timeout: 30))
        tabBar.buttons["Setup"].tap()

        let list = app.descendants(matching: .any)["setupSplitList"]
        XCTAssertTrue(list.waitForExistence(timeout: 10), "Setup shows no list beside the page")
        let page = app.descendants(matching: .any)["setupSplitPage"]
        XCTAssertTrue(page.exists)
        XCTAssertLessThanOrEqual(list.frame.maxX, page.frame.minX + 1)
        // Nothing chosen yet: the first category's page, Devices, marked.
        XCTAssertTrue(app.buttons["Devices"].isSelected)

        let category = app.buttons["CAT & Network"]
        XCTAssertTrue(category.waitForExistence(timeout: 10))
        category.tap()
        // The category opens under its name, its first page shown on the right.
        let row = app.buttons["Rotor"]
        XCTAssertTrue(row.waitForExistence(timeout: 10), "CAT & Network did not open in the list")
        XCTAssertGreaterThan(row.frame.minY, category.frame.maxY - 1)
        XCTAssertLessThan(row.frame.maxX, page.frame.minX + 1)
        let driver = app.descendants(matching: .any)["rotor.setup.driver"]
        XCTAssertTrue(driver.waitForExistence(timeout: 10), "The Rotor page is not beside the list")
        row.tap()
        XCTAssertTrue(row.isSelected, "The open page is not marked in the list")
        XCTAssertFalse(app.buttons["Devices"].isSelected)
        XCTAssertGreaterThan(driver.frame.minX, list.frame.maxX)
        // The page is its column's root: the list is not covered. Its title
        // heads the page's column, starting where its section titles do.
        let title = pageTitle(app, "Rotor")
        XCTAssertTrue(title.waitForExistence(timeout: 5), "The Rotor page has no title in its column")
        let connection = app.staticTexts["Connection"]
        XCTAssertTrue(connection.exists)
        XCTAssertEqual(title.frame.minX, connection.frame.minX, accuracy: 2,
                       "The title does not line up with the page's sections")
        XCTAssertGreaterThan(title.frame.minX, list.frame.maxX + 16, "The title sits against the list")
        XCTAssertLessThan(title.frame.maxY, connection.frame.minY)
        XCTAssertTrue(category.isHittable)
        XCTAssertTrue(tabBar.isHittable, "The tab bar went")
        shoot(app, "ipad-setup-split-rotor")

        // Another category's page replaces it on the right.
        app.buttons["General"].tap()
        let navigation = app.buttons["Navigation"]
        XCTAssertTrue(navigation.waitForExistence(timeout: 10))
        navigation.tap()
        XCTAssertTrue(navigation.isSelected)
        XCTAssertTrue(pageTitle(app, "Navigation").waitForExistence(timeout: 5))
        XCTAssertFalse(app.buttons["Rotor"].exists, "CAT & Network stayed open")

        // Upright the window is still regular-width: the split, with the page open.
        XCUIDevice.shared.orientation = .portrait
        XCTAssertTrue(pageTitle(app, "Navigation").waitForExistence(timeout: 10))
        XCTAssertTrue(list.exists)
        XCTAssertTrue(navigation.isSelected)
        shoot(app, "ipad-setup-split-upright")
    }

    /// R-IOS-52: a Setup link from elsewhere, the Display sheet's More display
    /// options in Setup, opens Display's page on the right, with Display open
    /// in the list and its page marked.
    @MainActor
    func testSetupLinkFromElsewhereOpensItsPageOnTheRight() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The split is the iPad's")
        XCUIDevice.shared.orientation = .landscapeLeft
        let app = launch()
        let toolbar = app.otherElements["toolbarIPad"]
        XCTAssertTrue(toolbar.waitForExistence(timeout: 30))
        toolbar.buttons["display"].tap()
        let more = app.buttons["moreDisplayOptions"]
        XCTAssertTrue(more.waitForExistence(timeout: 10))
        more.tap()

        let list = app.descendants(matching: .any)["setupSplitList"]
        XCTAssertTrue(list.waitForExistence(timeout: 10), "Setup opened without its list")
        XCTAssertTrue(pageTitle(app, "Display").waitForExistence(timeout: 10))
        let palette = app.descendants(matching: .any)["displayPalette"]
        XCTAssertTrue(palette.waitForExistence(timeout: 10), "Display's page did not open")
        XCTAssertGreaterThan(palette.frame.minX, list.frame.maxX)
        let row = app.buttons["On this phone"]
        XCTAssertTrue(row.exists, "Display is not open in the list")
        XCTAssertTrue(row.isSelected)
        XCTAssertLessThan(row.frame.maxX, palette.frame.minX)
    }

    /// R-IOS-52: where the app's window is compact-width (an iPhone; on an
    /// iPad a narrow Split View or Slide Over window, which a UI test cannot
    /// open), Setup is one list and a page slides in over it, with a way
    /// back.
    @MainActor
    func testCompactWidthKeepsOneListWithPagesThatSlideIn() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom == .pad,
                      "A full-screen iPad window is regular-width; the compact layout is tried on an iPhone")
        XCUIDevice.shared.orientation = .portrait
        let app = launch()
        let tabBar = app.otherElements["Sections"]
        XCTAssertTrue(tabBar.waitForExistence(timeout: 30))
        tabBar.buttons["Setup"].tap()
        let category = app.buttons["CAT & Network"]
        XCTAssertTrue(category.waitForExistence(timeout: 10))
        XCTAssertFalse(app.descendants(matching: .any)["setupSplitList"].exists)
        var swipes = 0
        while !category.isHittable && swipes < 6 {
            app.swipeUp()
            swipes += 1
        }
        category.tap()
        let row = app.buttons["Rotor"]
        XCTAssertTrue(row.waitForExistence(timeout: 10))
        // The category's pages replace the list.
        XCTAssertFalse(category.exists && category.isHittable)
        row.tap()
        let bar = app.navigationBars["Rotor"]
        XCTAssertTrue(bar.waitForExistence(timeout: 10))
        XCTAssertTrue(bar.buttons.firstMatch.exists, "The page has no way back")
        // Here the title stays the bar's large title, not one in the page.
        XCTAssertFalse(app.staticTexts["setupPageTitle"].exists)
    }

    /// The page's own title in the split's right-hand column.
    @MainActor
    private func pageTitle(_ app: XCUIApplication, _ title: String) -> XCUIElement {
        app.staticTexts.matching(NSPredicate(format: "identifier == 'setupPageTitle' AND label == %@", title))
            .firstMatch
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
