// NereusSDR for iOS: Turn beam on a spot's details, ready with a rotor and a bearing, greyed with its reason without
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class SpotTurnBeamUITests: XCTestCase {
    /// RotorModel.noBearingReason and StationToolList.noRotorReason, as the operator reads them.
    private static let noBearing = "The Core has no bearing for this spot. It needs your grid square and a callsign it can place."
    private static let noRotor = "No rotor is set up on this Core."

    override func setUp() {
        continueAfterFailure = false
    }

    /// A connected rotor: JA1ABC, which the Core places at 330 degrees, shows
    /// its bearing both ways and an enabled "Turn beam 330°" between Tune and
    /// Close; W1AW, which it cannot place, shows Turn beam greyed with why.
    @MainActor
    func testTurnBeamReadyWithABearingAndGreyedWithout() {
        let app = launch(rotorArgument: "-NereusRotorFixture")
        openDetails("JA1ABC", in: app)
        let tune = app.buttons["spotDetailsTune"]
        let beam = app.buttons["spotDetailsTurnBeam"]
        let close = app.buttons["spotDetailsClose"]
        XCTAssertTrue(beam.waitForExistence(timeout: 10))
        XCTAssertEqual(beam.label, "Turn beam 330\u{00B0}")
        XCTAssertTrue(beam.isEnabled)
        XCTAssertTrue(app.staticTexts["330\u{00B0} short \u{00B7} 150\u{00B0} long"].exists)
        // Turn beam sits between Tune and Close.
        XCTAssertLessThan(tune.frame.midX, beam.frame.midX)
        XCTAssertLessThan(beam.frame.midX, close.frame.midX)
        XCTAssertFalse(app.staticTexts[Self.noBearing].exists)
        close.tap()
        XCTAssertTrue(app.descendants(matching: .any)["spotDetails"].waitForNonExistence(timeout: 10))

        openDetails("W1AW", in: app)
        XCTAssertTrue(beam.waitForExistence(timeout: 10))
        XCTAssertEqual(beam.label, "Turn beam")
        XCTAssertFalse(beam.isEnabled, "Turn beam is greyed, never hidden, with no bearing")
        XCTAssertTrue(app.staticTexts[Self.noBearing].exists)
    }

    /// A Core that controls rotors with none set up: Turn beam stays shown,
    /// greyed, with the reason, even on a spot with a bearing.
    @MainActor
    func testTurnBeamGreyedWithNoRotor() {
        let app = launch(rotorArgument: "-NereusNoRotorFixture")
        openDetails("JA1ABC", in: app)
        let beam = app.buttons["spotDetailsTurnBeam"]
        XCTAssertTrue(beam.waitForExistence(timeout: 10))
        XCTAssertEqual(beam.label, "Turn beam 330\u{00B0}")
        XCTAssertFalse(beam.isEnabled, "Turn beam is greyed, never hidden, with no rotor")
        XCTAssertTrue(app.staticTexts[Self.noRotor].exists)
    }

    /// Press and hold a spot on the band: its details open.
    @MainActor
    private func openDetails(_ call: String, in app: XCUIApplication) {
        let spot = app.buttons[call]
        XCTAssertTrue(spot.waitForExistence(timeout: 30), "\(call) is not on the band")
        spot.press(forDuration: 0.8)
        XCTAssertTrue(app.descendants(matching: .any)["spotDetails"].waitForExistence(timeout: 10),
                      "\(call)'s details did not open")
    }

    @MainActor
    private func launch(rotorArgument: String) -> XCUIApplication {
        let app = XCUIApplication()
        // The band with the Core's catalogue, two spots and a rotor fixture,
        // with no Core (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", rotorArgument, "-NereusSpotFixture"]
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launch()
        return app
    }
}
