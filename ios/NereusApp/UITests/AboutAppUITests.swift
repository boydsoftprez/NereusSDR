// NereusSDR for iOS: About access without a Core and readable layout pictures
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

final class AboutAppUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    override func tearDown() {
        XCUIDevice.shared.orientation = .portrait
    }

    @MainActor
    func testWelcomeAndSavedCoresCanOpenAboutWithoutAConnection() throws {
        let welcome = XCUIApplication()
        welcome.launchArguments = ["-NereusFreshWelcome"]
        welcome.launch()
        XCTAssertFalse(welcome.buttons["coresNotice"].exists)
        XCTAssertTrue(welcome.buttons["welcomeAbout"].waitForExistence(timeout: 30))
        try save("about-welcome-entry")
        welcome.buttons["welcomeAbout"].tap()
        XCTAssertTrue(welcome.staticTexts["AboutAppIdentity"].waitForExistence(timeout: 10))
        welcome.buttons["Done"].tap()

        let cores = XCUIApplication()
        cores.launchArguments = ["-NereusShotCores"]
        cores.launch()
        XCTAssertTrue(cores.buttons["coresAbout"].waitForExistence(timeout: 30))
        try save("about-cores-entry")
        cores.buttons["coresAbout"].tap()
        XCTAssertTrue(cores.staticTexts["AboutAppIdentity"].waitForExistence(timeout: 10))
        cores.buttons["Done"].tap()
        XCTAssertTrue(cores.buttons["coreActions-KG4VCF/shack"].exists)
    }

    @MainActor
    func testPortraitLandscapeAndLargeTypePictures() throws {
        for (name, category) in [("normal", "UICTContentSizeCategoryM"),
                                 ("large", "UICTContentSizeCategoryAccessibilityM")] {
            let app = XCUIApplication()
            app.launchArguments = ["-NereusShowBand", "-UIPreferredContentSizeCategoryName", category]
            app.launch()
            XCTAssertFalse(app.buttons["coresNotice"].exists)
            let setup = app.otherElements["Sections"].buttons["Setup"]
            XCTAssertTrue(setup.waitForExistence(timeout: 30))
            setup.tap()
            // At accessibility sizes on a shorter phone (iPhone 17 Pro) the
            // row sits below the fold of Setup's list.
            try show(app.buttons["About this app"], in: app)
            app.buttons["About this app"].tap()
            XCTAssertTrue(app.staticTexts["AboutAppIdentity"].waitForExistence(timeout: 10))
            try save("about-\(name)-portrait")
            XCUIDevice.shared.orientation = .landscapeLeft
            XCTAssertTrue(app.staticTexts["AboutAppIdentity"].waitForExistence(timeout: 10))
            try save("about-\(name)-landscape")
            XCUIDevice.shared.orientation = .portrait
            app.swipeUp()
            try save("about-\(name)-contributors")
            try show(app.buttons["AboutLink-NereusSDR releases / What's New"], in: app)
            try save("about-\(name)-links")
            try show(app.buttons["AboutLink-Corresponding Source"], in: app)
            XCTAssertTrue(app.buttons["AboutLink-Corresponding Source"].isEnabled)
            try show(app.buttons["AboutLink-Privacy"], in: app)
            XCTAssertTrue(app.buttons["AboutLink-Privacy"].isEnabled)
            try save("about-\(name)-source-and-privacy")
            try show(app.buttons["AboutLicences"], in: app)
            try save("about-\(name)-built-with")
            app.buttons["AboutLicences"].tap()
            XCTAssertTrue(app.navigationBars["Licenses"].waitForExistence(timeout: 10))
            try save("about-\(name)-licences")
            app.staticTexts["Opus"].tap()
            XCTAssertTrue(app.navigationBars["Opus"].waitForExistence(timeout: 10))
            try save("about-\(name)-third-party-notice")
            app.navigationBars["Opus"].buttons.firstMatch.tap()
            app.navigationBars["Licenses"].buttons.firstMatch.tap()
            try show(app.buttons["NereusSDR for iOS license and App Store permission"], in: app)
            try save("about-\(name)-legal")
            app.buttons["NereusSDR for iOS license and App Store permission"].tap()
            XCTAssertTrue(app.navigationBars["iOS license"].waitForExistence(timeout: 10))
            try save("about-\(name)-phone-licence")
            app.terminate()
        }
    }

    @MainActor
    func testFailedExternalLinkExplainsItLocally() throws {
        let app = XCUIApplication()
        app.launchArguments = ["-NereusFreshWelcome", "-NereusAboutOpenFailure"]
        app.launch()
        XCTAssertTrue(app.buttons["welcomeAbout"].waitForExistence(timeout: 30))
        app.buttons["welcomeAbout"].tap()
        try show(app.buttons["AboutLink-NereusSDR releases / What's New"], in: app)
        app.buttons["AboutLink-NereusSDR releases / What's New"].tap()
        let feedback = app.staticTexts["AboutLinkError"]
        XCTAssertTrue(feedback.waitForExistence(timeout: 10))
        XCTAssertTrue(feedback.label.contains("Could not open"))
        try show(app.buttons["AboutLink-Corresponding Source"], in: app)
        app.buttons["AboutLink-Corresponding Source"].tap()
        XCTAssertTrue(feedback.waitForExistence(timeout: 10))
        XCTAssertTrue(feedback.label.contains("Could not open Corresponding Source"))
    }

    @MainActor
    private func show(_ element: XCUIElement, in app: XCUIApplication, tappable: Bool = true) throws {
        for _ in 0..<10 {
            if element.exists && (!tappable || element.isHittable) { return }
            app.swipeUp()
        }
        XCTFail("Could not scroll to \(element.identifier)")
    }

    @MainActor
    private func save(_ name: String) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_ABOUT_SHOTS"], !directory.isEmpty else {
            return
        }
        let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
        try XCUIScreen.main.screenshot().pngRepresentation.write(to: url)
    }
}
