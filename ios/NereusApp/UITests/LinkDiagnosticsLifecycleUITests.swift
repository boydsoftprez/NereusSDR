// NereusSDR for iOS: offline real scene lifecycle diagnostic coverage
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

final class LinkDiagnosticsLifecycleUITests: XCTestCase {
    override func setUp() { continueAfterFailure = false }

    @MainActor
    func testOfflineLaunchBackgroundAndReactivation() {
        let app = XCUIApplication()
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        XCTAssertTrue(app.otherElements["Sections"].waitForExistence(timeout: 30))
        XCTAssertEqual(app.state, .runningForeground)
        XCUIDevice.shared.press(.home)
        XCTAssertTrue(app.wait(for: .runningBackground, timeout: 10))
        app.activate()
        XCTAssertTrue(app.wait(for: .runningForeground, timeout: 10))
        XCTAssertTrue(app.otherElements["Sections"].waitForExistence(timeout: 10))
    }
}
