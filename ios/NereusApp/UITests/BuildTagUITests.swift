// NereusSDR for iOS: a test build names itself in Setup and Diagnostics
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class BuildTagUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    /// The tag this build was given. The UI test bundle's Info.plist carries
    /// the same NEREUS_BUILD_TAG build setting as the app's.
    private var expectedTag: String? {
        let raw = Bundle(for: BuildTagUITests.self).object(forInfoDictionaryKey: "NereusBuildTag") as? String
        let trimmed = raw?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        return trimmed.isEmpty ? nil : trimmed
    }

    @MainActor
    func testBuildRowFollowsTheBuildTag() {
        let app = XCUIApplication()
        // The band and its tabs, with no Core (a debug build's launch argument).
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        // The tab bar is NereusSDR's own, not the system's.
        let setup = app.otherElements["Sections"].buttons["Setup"]
        XCTAssertTrue(setup.waitForExistence(timeout: 30))
        setup.tap()
        XCTAssertTrue(app.buttons["About this app"].waitForExistence(timeout: 30))

        // The build row is the last row of Setup's list; on a shorter phone
        // (iPhone 17 Pro) it sits below the fold, so scroll to the list's end.
        let setupRow = app.staticTexts["SetupBuildTag"]
        if let tag = expectedTag {
            XCTAssertTrue(reveal(setupRow, in: app), "Setup shows no build row")
            XCTAssertEqual(setupRow.label, "Build \(tag)")
        } else {
            XCTAssertTrue(reveal(app.staticTexts["setupNote"], in: app), "Setup's note cannot be reached")
            app.swipeUp()
            XCTAssertFalse(setupRow.exists, "Setup shows a build row for an untagged build")
        }

        XCTAssertTrue(reveal(app.buttons["About this app"], in: app, swipingUp: false),
                      "Setup's About this app row cannot be reached")
        app.buttons["About this app"].tap()
        XCTAssertTrue(app.staticTexts["AboutAppIdentity"].waitForExistence(timeout: 10))
        let aboutRow = app.staticTexts["AboutBuildTag"]
        if let tag = expectedTag {
            XCTAssertTrue(aboutRow.waitForExistence(timeout: 10), "About shows no build row")
            XCTAssertEqual(aboutRow.label, "Build \(tag)")
        } else {
            XCTAssertFalse(aboutRow.exists, "About shows a build row for an untagged build")
        }
    }

    /// Swipes the list until `element` is on screen and hittable, as the
    /// About and tab bar UI tests do. Swipes up to go down the list, or down
    /// to go back up it.
    @MainActor
    private func reveal(_ element: XCUIElement, in app: XCUIApplication, swipingUp: Bool = true) -> Bool {
        for _ in 0..<10 {
            if element.exists && element.isHittable { return true }
            if swipingUp { app.swipeUp() } else { app.swipeDown() }
        }
        return element.exists && element.isHittable
    }
}
