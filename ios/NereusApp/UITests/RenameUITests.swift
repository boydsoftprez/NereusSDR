// NereusSDR for iOS: a Core's row in Your Cores offers Rename from its actions button and, as a shortcut, press and hold
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class RenameUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    /// D76: a row's press-and-hold menu offers Rename, which opens Rename
    /// this Core with the field filled; a Core the phone last found too old
    /// shows Rename greyed with "Needs a newer Core". With
    /// `NEREUS_CONNECT_SHOTS` set (through `TEST_RUNNER_NEREUS_CONNECT_SHOTS`),
    /// each menu is written there as a PNG.
    @MainActor
    func testPressAndHoldOffersRename() throws {
        let app = XCUIApplication()
        // Your Cores with made-up Cores kept in memory (a debug build's launch argument).
        app.launchArguments = ["-NereusShotCores"]
        app.launch()

        let shack = app.descendants(matching: .any)["core-KG4VCF/shack"]
        XCTAssertTrue(shack.waitForExistence(timeout: 30))
        shack.press(forDuration: 1.2)
        let rename = app.buttons["rename-KG4VCF/shack"]
        XCTAssertTrue(rename.waitForExistence(timeout: 10))
        XCTAssertTrue(rename.isEnabled)
        // The menu settles after its opening animation.
        Thread.sleep(forTimeInterval: 1.0)
        try save("36-press-and-hold-menu")
        rename.tap()
        let field = app.textFields["renameField"]
        XCTAssertTrue(field.waitForExistence(timeout: 10))
        XCTAssertEqual(field.value as? String, "KG4VCF/shack")
        XCTAssertTrue(app.staticTexts["Rename this Core"].exists)
        XCTAssertTrue(app.staticTexts["Your callsign, then / and a name, like KG4VCF/shack."].exists)
        app.buttons["renameCancel"].tap()
        XCTAssertTrue(field.waitForNonExistence(timeout: 10))

        // The Core with no name is listed by its address.
        XCTAssertTrue(app.descendants(matching: .any)["core-192.0.2.40"].exists)

        let field2 = app.descendants(matching: .any)["core-KG4VCF/field"]
        field2.press(forDuration: 1.2)
        let greyed = app.buttons["rename-KG4VCF/field"]
        XCTAssertTrue(greyed.waitForExistence(timeout: 10))
        XCTAssertFalse(greyed.isEnabled)
        XCTAssertTrue(greyed.label.contains("Needs a newer Core"), greyed.label)
        Thread.sleep(forTimeInterval: 1.0)
        try save("37-press-and-hold-menu-needs-a-newer-core")
    }

    /// D78: each Core row has a visible "⋯" button whose menu offers
    /// Rename, the same flow as pressing and holding the row. With
    /// `NEREUS_CONNECT_SHOTS` set, the rows and the open menu are written
    /// there upright and sideways.
    @MainActor
    func testActionsButtonOffersRename() throws {
        addTeardownBlock { @MainActor in
            XCUIDevice.shared.orientation = .portrait
        }
        let app = XCUIApplication()
        // Your Cores with made-up Cores kept in memory (a debug build's launch argument).
        app.launchArguments = ["-NereusShotCores"]
        app.launch()

        let actions = app.buttons["coreActions-KG4VCF/shack"]
        XCTAssertTrue(actions.waitForExistence(timeout: 30))
        XCTAssertEqual(actions.label, "Actions for KG4VCF/shack")
        XCTAssertGreaterThanOrEqual(actions.frame.width, 44, "\(actions.frame)")
        XCTAssertGreaterThanOrEqual(actions.frame.height, 44, "\(actions.frame)")
        XCTAssertTrue(app.buttons["coreActions-192.0.2.40"].exists)
        try save("core-rows-actions")
        actions.tap()
        let rename = app.buttons["rename-KG4VCF/shack"]
        XCTAssertTrue(rename.waitForExistence(timeout: 10))
        XCTAssertTrue(rename.isEnabled)
        Thread.sleep(forTimeInterval: 1.0)
        try save("core-row-actions-menu")
        rename.tap()
        let field = app.textFields["renameField"]
        XCTAssertTrue(field.waitForExistence(timeout: 10))
        XCTAssertEqual(field.value as? String, "KG4VCF/shack")
        app.buttons["renameCancel"].tap()
        XCTAssertTrue(field.waitForNonExistence(timeout: 10))

        XCUIDevice.shared.orientation = .landscapeLeft
        XCTAssertTrue(actions.waitForExistence(timeout: 20))
        Thread.sleep(forTimeInterval: 1.0)
        try save("core-rows-actions-sideways")
    }

    @MainActor
    private func save(_ name: String) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_CONNECT_SHOTS"], !directory.isEmpty else {
            return
        }
        let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
        try XCUIScreen.main.screenshot().pngRepresentation.write(to: url)
        print("Wrote \(url.path)")
    }
}
