// NereusSDR for iOS: the app opens on the welcome with nothing paired, and reaches the address screen
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class ConnectUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    @MainActor
    func testWelcomeLeadsToAnAddressAndItsPort() {
        let app = XCUIApplication()
        app.launchArguments = ["-NereusFreshWelcome"]
        app.launch()
        let find = app.buttons["findMyCore"]
        XCTAssertTrue(find.waitForExistence(timeout: 30))
        XCTAssertTrue(app.buttons["setUpCore"].exists)
        // XCTest can expose Welcome's accessibility tree while Springboard
        // is still animating the app open. Wait until the button can take a tap.
        let hittable = XCTNSPredicateExpectation(predicate: NSPredicate(format: "hittable == true"), object: find)
        XCTAssertEqual(XCTWaiter.wait(for: [hittable], timeout: 30), .completed)
        find.tap()
        let enter = app.buttons["enterAddress"]
        XCTAssertTrue(enter.waitForExistence(timeout: 10))
        XCTAssertTrue(app.buttons["pairWithCode"].exists)
        enter.tap()
        let address = app.textViews["addressField"].exists ? app.textViews["addressField"]
            : app.textFields["addressField"]
        XCTAssertTrue(address.waitForExistence(timeout: 10))
        let port = app.textFields["portField"]
        XCTAssertTrue(port.exists)
        XCTAssertEqual(port.value as? String, "47910")
        XCTAssertTrue(app.buttons["connectAddress"].exists)
    }
}
