// NereusSDR for iOS: one tap opens Sound; the link dot opens connection diagnostics
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class SoundPanelUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    /// D77: one tap on the speaker button opens the Sound panel over the
    /// band; its Mute switch mutes; Earpiece chosen there is ticked in
    /// Setup, Audio, On this phone, and Speaker chosen in Setup is ticked
    /// in the panel. With `NEREUS_CONNECT_SHOTS` set (through
    /// `TEST_RUNNER_NEREUS_CONNECT_SHOTS`), the panel is written there
    /// upright and sideways as PNGs.
    @MainActor
    func testOneTapOpensTheSoundPanelInStepWithSetup() throws {
        addTeardownBlock { @MainActor in
            XCUIDevice.shared.orientation = .portrait
        }
        let app = XCUIApplication()
        // The band and its tabs, with no Core (a debug build's launch argument).
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let speaker = app.buttons["speaker"]
        XCTAssertTrue(speaker.waitForExistence(timeout: 30))
        XCTAssertEqual(speaker.label, "Sound options")
        XCTAssertEqual(speaker.value as? String, "On")

        speaker.tap()
        let mute = app.switches["soundMute"]
        XCTAssertTrue(mute.waitForExistence(timeout: 10))
        let speakerRow = app.buttons["soundRoute.speaker"]
        let earpieceRow = app.buttons["soundRoute.earpiece"]
        XCTAssertTrue(speakerRow.exists)
        // An iPad has no earpiece, so it lists no row for one.
        let iPad = UIDevice.current.userInterfaceIdiom == .pad
        XCTAssertEqual(earpieceRow.exists, !iPad)
        // No headphones in the simulator: no row for them.
        XCTAssertFalse(app.buttons["soundRoute.external"].exists)
        // The band stays in sight beside the panel.
        XCTAssertTrue(app.otherElements["Sections"].exists)

        mute.switches.firstMatch.tap()
        XCTAssertEqual(mute.value as? String, "1")
        XCTAssertEqual(speaker.value as? String, "Muted")
        mute.switches.firstMatch.tap()
        XCTAssertEqual(mute.value as? String, "0")

        if iPad {
            // No second route to move between: the panel closes, and opens sideways too.
            app.coordinate(withNormalizedOffset: CGVector(dx: 0.3, dy: 0.4)).tap()
            XCTAssertTrue(mute.waitForNonExistence(timeout: 10))
            XCUIDevice.shared.orientation = .landscapeLeft
            XCTAssertTrue(app.otherElements["toolbarIPad"].waitForExistence(timeout: 20))
            app.buttons["speaker"].tap()
            XCTAssertTrue(mute.waitForExistence(timeout: 10))
            return
        }

        earpieceRow.tap()
        XCTAssertTrue(earpieceRow.isSelected)
        XCTAssertFalse(speakerRow.isSelected)
        Thread.sleep(forTimeInterval: 0.6)
        try save("sound-panel")

        // Close it with a tap on the band away from it, then look in Setup.
        app.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.6)).tap()
        XCTAssertTrue(mute.waitForNonExistence(timeout: 10))
        app.otherElements["Sections"].buttons["Setup"].tap()
        app.buttons["Audio"].tap()
        app.buttons["On this phone"].tap()
        let setupEarpiece = app.buttons["audioRoute.Earpiece"]
        XCTAssertTrue(setupEarpiece.waitForExistence(timeout: 10))
        XCTAssertTrue(setupEarpiece.isSelected)
        let setupSpeaker = app.buttons["audioRoute.iPhone speaker"]
        setupSpeaker.tap()
        XCTAssertTrue(setupSpeaker.isSelected)

        app.otherElements["Sections"].buttons["Panadapter"].tap()
        speaker.tap()
        XCTAssertTrue(speakerRow.waitForExistence(timeout: 10))
        XCTAssertTrue(speakerRow.isSelected)
        XCTAssertFalse(earpieceRow.isSelected)
        app.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.6)).tap()
        XCTAssertTrue(mute.waitForNonExistence(timeout: 10))

        XCUIDevice.shared.orientation = .landscapeLeft
        XCTAssertTrue(app.otherElements["toolbarSideways"].waitForExistence(timeout: 20))
        app.buttons["speaker"].tap()
        XCTAssertTrue(mute.waitForExistence(timeout: 10))
        Thread.sleep(forTimeInterval: 0.6)
        try save("sound-panel-sideways")
    }

    /// D89: the link dot opens the same diagnostics page as Tools.
    /// It is a button, named for the link and what it opens, with at least
    /// 44 points to hit.
    @MainActor
    func testLinkDotOpensConnectionPerformanceWhileRadioTabStaysRadio() {
        addTeardownBlock { @MainActor in
            XCUIDevice.shared.orientation = .portrait
        }
        let app = XCUIApplication()
        // The band and its tabs, with no Core (a debug build's launch argument).
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let link = app.buttons["link"]
        XCTAssertTrue(link.waitForExistence(timeout: 30))
        XCTAssertTrue(link.label.hasSuffix(", opens Connection and performance"), link.label)
        XCTAssertGreaterThanOrEqual(link.frame.height, 44, "\(link.frame)")
        XCTAssertGreaterThanOrEqual(link.frame.width, 44, "\(link.frame)")
        link.tap()
        let tools = app.otherElements["Sections"].buttons["Tools"]
        XCTAssertTrue(tools.waitForExistence(timeout: 10))
        XCTAssertTrue(tools.isSelected)
        XCTAssertTrue(app.scrollViews["connectionPerformance.page"].waitForExistence(timeout: 10))
        XCTAssertTrue(app.buttons["connectionPerformance.section"].exists)
        let radio = app.otherElements["Sections"].buttons["Radio"]
        radio.tap()
        XCTAssertTrue(radio.isSelected)
        XCTAssertTrue(app.buttons["radioDisconnect"].waitForExistence(timeout: 10))

        // Sideways too.
        app.otherElements["Sections"].buttons["Panadapter"].tap()
        XCUIDevice.shared.orientation = .landscapeLeft
        // An iPad's toolbar takes its own form either way up.
        let toolbar = app.otherElements[UIDevice.current.userInterfaceIdiom == .pad ? "toolbarIPad" : "toolbarSideways"]
        XCTAssertTrue(toolbar.waitForExistence(timeout: 20))
        toolbar.buttons["link"].tap()
        XCTAssertTrue(app.otherElements["Sections"].buttons["Tools"].isSelected)
        XCTAssertTrue(app.scrollViews["connectionPerformance.page"].exists)
        XCTAssertTrue(app.buttons["connectionPerformance.section"].exists)
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
