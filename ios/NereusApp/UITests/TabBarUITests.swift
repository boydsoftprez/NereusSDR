// NereusSDR for iOS: the app launches to the band, its toolbar and tab bar, and reaches the licences screen
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class TabBarUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    override func tearDown() {
        XCUIDevice.shared.orientation = .portrait
    }

    /// The tests run on an iPad, whose main screen lays its applets out its own way.
    static var iPad: Bool {
        UIDevice.current.userInterfaceIdiom == .pad
    }

    @MainActor
    func testLaunchesToFiveTabsInOrder() {
        let app = XCUIApplication()
        // The band and its tabs, with no Core (a debug build's launch argument).
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let tabBar = app.otherElements["Sections"]
        XCTAssertTrue(tabBar.waitForExistence(timeout: 30))
        let titles = tabBar.buttons.allElementsBoundByIndex.map(\.label)
        XCTAssertEqual(titles, ["Panadapter", "Modes", "Tools", "Radio", "Setup"])
        XCTAssertTrue(tabBar.buttons["Panadapter"].isSelected)
    }

    @MainActor
    func testToolbarIsInTheBoardsOrderWithTheTxPanelAtTheRightEnd() {
        let app = XCUIApplication()
        // The band and its tabs, with no Core (a debug build's launch argument).
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        // An iPad upright has no panel buttons: its applets sit below the band (IPadUITests).
        let names = Self.iPad ? ["speaker", "slice", "pan", "display", "link"]
            : ["rxPanel", "speaker", "slice", "pan", "display", "link", "txPanel"]
        var lastX = -CGFloat.greatestFiniteMagnitude
        for name in names {
            let item = app.descendants(matching: .any)[name]
            XCTAssertTrue(item.waitForExistence(timeout: 30), "\(name) is not on the toolbar")
            XCTAssertGreaterThan(item.frame.minX, lastX, "\(name) is out of order")
            lastX = item.frame.minX
        }
        // PTT is never hidden; with no Core it is there and cannot be used.
        let ptt = app.buttons["ptt"]
        XCTAssertTrue(ptt.waitForExistence(timeout: 10))
        XCTAssertFalse(ptt.isEnabled)
    }

    @MainActor
    func testTxPanelSlidesInAndOut() throws {
        try XCTSkipIf(Self.iPad, "An iPad's RX and TX sit beside or below the band, not in sliding panels (IPadUITests)")
        let app = XCUIApplication()
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let button = app.buttons["txPanel"]
        XCTAssertTrue(button.waitForExistence(timeout: 30))
        button.tap()
        XCTAssertTrue(app.otherElements["txPanelDrawer"].waitForExistence(timeout: 10))
        button.tap()
        XCTAssertTrue(app.otherElements["txPanelDrawer"].waitForNonExistence(timeout: 10))
    }

    @MainActor
    func testRxPanelSlidesInAndOut() throws {
        try XCTSkipIf(Self.iPad, "An iPad's RX and TX sit beside or below the band, not in sliding panels (IPadUITests)")
        let app = XCUIApplication()
        // The band and its tabs, with no Core (a debug build's launch argument).
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let button = app.buttons["rxPanel"]
        XCTAssertTrue(button.waitForExistence(timeout: 30))
        button.tap()
        XCTAssertTrue(app.otherElements["rxPanelDrawer"].waitForExistence(timeout: 10))
        button.tap()
        XCTAssertTrue(app.otherElements["rxPanelDrawer"].waitForNonExistence(timeout: 10))
    }

    @MainActor
    func testSidewaysTheToolbarStaysOnTopAndThePanelSlidesIn() throws {
        try XCTSkipIf(Self.iPad, "An iPad's RX and TX sit beside or below the band, not in sliding panels (IPadUITests)")
        let app = XCUIApplication()
        // The band and its tabs, with no Core (a debug build's launch argument).
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        XCTAssertTrue(app.buttons["rxPanel"].waitForExistence(timeout: 30))
        XCUIDevice.shared.orientation = .landscapeLeft
        // Sideways the toolbar takes its sideways form, the Core's name in the middle.
        let toolbar = app.otherElements["toolbarSideways"]
        XCTAssertTrue(toolbar.waitForExistence(timeout: 20))
        let rxPanel = toolbar.buttons["rxPanel"]
        let link = toolbar.descendants(matching: .any)["link"]
        XCTAssertTrue(rxPanel.exists)
        XCTAssertTrue(link.exists)
        XCTAssertFalse(app.otherElements["toolbar"].exists)
        rxPanel.tap()
        XCTAssertTrue(app.otherElements["rxPanelDrawer"].waitForExistence(timeout: 10))
        let tabBar = app.otherElements["Sections"]
        XCTAssertTrue(tabBar.exists)
        XCTAssertEqual(tabBar.buttons.count, 5)
    }

    @MainActor
    func testRadioTabDisconnectShowsYourCores() {
        let app = XCUIApplication()
        // The band and its tabs, with made-up Cores listed and none connected
        // (a debug build's launch arguments).
        app.launchArguments = ["-NereusShotCores", "-NereusShowBand"]
        app.launch()
        let radio = app.otherElements["Sections"].buttons["Radio"]
        XCTAssertTrue(radio.waitForExistence(timeout: 30))
        radio.tap()
        let disconnect = app.buttons["radioDisconnect"]
        XCTAssertTrue(disconnect.waitForExistence(timeout: 10))
        XCTAssertEqual(disconnect.label, "Disconnect")
        XCTAssertTrue(app.descendants(matching: .any)["radioLink"].exists)
        disconnect.tap()
        // Your Cores, where Connect and the rest are.
        XCTAssertTrue(app.staticTexts["KG4VCF/shack"].waitForExistence(timeout: 10))
        XCTAssertFalse(app.otherElements["Sections"].exists)
    }

    @MainActor
    func testSetupAboutListsLicences() {
        let app = XCUIApplication()
        // The band and its tabs, with no Core (a debug build's launch argument).
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let setup = app.otherElements["Sections"].buttons["Setup"]
        XCTAssertTrue(setup.waitForExistence(timeout: 30))
        setup.tap()
        app.buttons["About this app"].tap()
        for _ in 0..<20 {
            if app.buttons["AboutLicences"].isHittable { break }
            app.swipeUp()
        }
        XCTAssertTrue(app.buttons["AboutLicences"].isHittable)
        app.buttons["AboutLicences"].tap()
        for name in ["Opus", "libdatachannel", "libjuice", "libsrtp", "usrsctp", "plog", "Mbed TLS"] {
            XCTAssertTrue(app.staticTexts[name].waitForExistence(timeout: 10), "\(name) is not listed")
        }
        app.staticTexts["Opus"].tap()
        XCTAssertTrue(app.navigationBars["Opus"].waitForExistence(timeout: 10))
    }
}
