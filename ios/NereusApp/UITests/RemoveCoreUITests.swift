// NereusSDR for iOS: visible saved-Core removal and its local confirmation
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

final class RemoveCoreUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    @MainActor
    func testVisibleRemoveConfirmsThenDeletesOnlySelectedRow() throws {
        let app = XCUIApplication()
        app.launchArguments = ["-NereusShotCores"]
        app.launch()

        let actions = app.buttons["coreActions-KG4VCF/shack"]
        XCTAssertTrue(actions.waitForExistence(timeout: 30))
        actions.tap()
        let remove = app.buttons["removeCore-KG4VCF/shack"]
        XCTAssertTrue(remove.waitForExistence(timeout: 10))
        XCTAssertTrue(remove.isEnabled)
        try save("remove-core-menu")
        remove.tap()
        XCTAssertTrue(app.staticTexts[
            "This removes the saved Core and its addresses from this phone. It does not remove this phone from the Core's Devices list."
        ].waitForExistence(timeout: 10))
        try save("remove-core-confirmation")
        app.alerts.buttons["Cancel"].tap()
        XCTAssertTrue(actions.exists)

        actions.tap()
        app.buttons["removeCore-KG4VCF/shack"].tap()
        app.alerts.buttons["Remove Core"].tap()
        XCTAssertTrue(actions.waitForNonExistence(timeout: 10))
        XCTAssertTrue(app.buttons["coreActions-KG4VCF/field"].exists)
        try save("remove-core-after")
    }

    @MainActor
    private func save(_ name: String) throws {
        guard let path = ProcessInfo.processInfo.environment["NEREUS_REMOVE_CORE_SHOTS"], !path.isEmpty else {
            return
        }
        let url = URL(fileURLWithPath: path).appendingPathComponent("\(name).png")
        try XCUIScreen.main.screenshot().pngRepresentation.write(to: url)
    }
}
