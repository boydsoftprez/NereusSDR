// NereusSDR for iOS: sideways, a tap where the PTT was keyed and is held lands on it and unkeys, through the system's own hit testing
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class KeyedPttUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    override func tearDown() {
        MainActor.assumeIsolated {
            XCUIDevice.shared.orientation = .portrait
        }
    }

    /// DC15 as amended (the PR 653 re-review d, point a): on the phone held
    /// sideways the PTT moved right of what the column has out keys there
    /// and is held there for the whole key, even once that has gone, and a
    /// second tap at the very same point on the screen lands on it and
    /// unkeys. On this phone's sideways band (about 290 points tall) no
    /// slice flag reaches the PTT's row, so the column's open Display pill
    /// moves it: keyed past the pill, the pill then closed, the PTT is
    /// held where DC15's keyed place (beside the column) is not, over bare
    /// band, whose own UIKit surface lies under it. The taps are made at a
    /// point, not on the element, so they go through the system's and
    /// SwiftUI's own hit testing: anything drawn or laid over the PTT
    /// there, a clear layer or the band's surface included, would take the
    /// tap and leave the radio keyed.
    @MainActor
    func testTapWhereThePttWasKeyedUnkeysSideways() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .phone, "The phone held sideways: run on an iPhone")
        XCUIDevice.shared.orientation = .landscapeLeft
        let app = launch()
        let ptt = app.buttons["ptt"]
        XCTAssertTrue(ptt.waitForExistence(timeout: 30), "The band has no PTT")
        XCTAssertTrue(waitUntil { ptt.isEnabled }, "PTT is not usable: the fixture's link is not up")
        let column = app.buttons["bandColumn.display"]
        XCTAssertTrue(column.waitForExistence(timeout: 10), "The sideways band has no column")
        let beside = ptt.frame
        // Receiving: the open pill reaches the PTT's row and the PTT moves past it.
        column.tap()
        let pill = app.descendants(matching: .any)["bandColumnMenu.display"]
        XCTAssertTrue(pill.waitForExistence(timeout: 10), "Display opened no pill beside the column")
        XCTAssertTrue(waitUntil { ptt.frame.minX > pill.frame.maxX },
                      "The PTT at \(ptt.frame) did not move past the pill at \(pill.frame)")
        let receive = ptt.frame
        XCTAssertGreaterThan(receive.minX, beside.maxX, "The PTT did not move off its place beside the column")
        let spot = CGPoint(x: receive.midX, y: receive.midY)
        let gauges = app.descendants(matching: .any)["keyedGauges"]
        XCTAssertFalse(gauges.exists, "The keyed meters show before the PTT is keyed")

        tap(app, at: spot)
        XCTAssertTrue(gauges.waitForExistence(timeout: 10), "A tap at the PTT did not key: no keyed meters")
        XCTAssertTrue(ptt.isSelected, "The PTT does not read keyed")
        XCTAssertEqual(ptt.frame, receive, "The PTT moved when transmitting began")
        // The pill closes while keyed (a tap on the band beside it): the PTT stays.
        app.descendants(matching: .any)["bandColumnClose"].coordinate(withNormalizedOffset: CGVector(dx: 0.85, dy: 0.15)).tap()
        XCTAssertTrue(pill.waitForNonExistence(timeout: 10), "A tap off the pill did not close it")
        XCTAssertEqual(ptt.frame, receive, "The PTT moved when the pill closed while keyed")
        XCTAssertTrue(gauges.exists && ptt.isSelected, "Closing the pill unkeyed")
        XCTAssertFalse(gauges.frame.intersects(ptt.frame), "The keyed meters at \(gauges.frame) cover the PTT")

        // A second tap at the same point: what is on top there is the PTT, which unkeys.
        tap(app, at: spot)
        XCTAssertTrue(gauges.waitForNonExistence(timeout: 10),
                      "A tap where the PTT was keyed did not unkey: the keyed meters still show")
        XCTAssertFalse(ptt.isSelected, "The PTT still reads keyed")
    }

    /// DC15 as amended (the PR 653 re-review e, Minor N3): a flag that opens
    /// out under a held PTT draws below it, so a tap at the PTT's spot still
    /// lands on the PTT and unkeys, through the system's own hit testing.
    /// The PTT is held mid-band as above (keyed past the open pill, the pill
    /// then closed); slice A's Audio panel then opens out over the band. The
    /// test also writes down where the keyed meters went beside the held
    /// PTT, for the case it cannot reach on this phone: the meters cut to
    /// fit above a held PTT need a band under about 240 points with their
    /// reasons showing (the SE sideways is about 180) and a PTT held with
    /// under 204 points beside it either side; every simulator the project
    /// runs is taller and wider than that.
    @MainActor
    func testFlagOpenedUnderAHeldPttLeavesItTappable() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .phone, "The phone held sideways: run on an iPhone")
        XCUIDevice.shared.orientation = .landscapeLeft
        let app = launch()
        let ptt = app.buttons["ptt"]
        XCTAssertTrue(ptt.waitForExistence(timeout: 30), "The band has no PTT")
        XCTAssertTrue(waitUntil { ptt.isEnabled }, "PTT is not usable: the fixture's link is not up")
        let column = app.buttons["bandColumn.display"]
        XCTAssertTrue(column.waitForExistence(timeout: 10), "The sideways band has no column")
        column.tap()
        let pill = app.descendants(matching: .any)["bandColumnMenu.display"]
        XCTAssertTrue(pill.waitForExistence(timeout: 10), "Display opened no pill beside the column")
        XCTAssertTrue(waitUntil { ptt.frame.minX > pill.frame.maxX },
                      "The PTT at \(ptt.frame) did not move past the pill at \(pill.frame)")
        let held = ptt.frame
        let spot = CGPoint(x: held.midX, y: held.midY)
        let gauges = app.descendants(matching: .any)["keyedGauges"]
        tap(app, at: spot)
        XCTAssertTrue(gauges.waitForExistence(timeout: 10), "A tap at the PTT did not key: no keyed meters")
        app.descendants(matching: .any)["bandColumnClose"].coordinate(withNormalizedOffset: CGVector(dx: 0.85, dy: 0.15)).tap()
        XCTAssertTrue(pill.waitForNonExistence(timeout: 10), "A tap off the pill did not close it")
        XCTAssertEqual(ptt.frame, held, "The PTT moved when the pill closed while keyed")
        let band = app.descendants(matching: .any)["bandSurface"].frame
        print("""
            KeyedPttUITests N3: band \(band), column \(column.frame), held PTT \(held), \
            keyed meters \(gauges.frame), room over the PTT \(held.minY - band.minY - 6)
            """)

        let flagTab = app.buttons["flagTabAudioA"]
        XCTAssertTrue(flagTab.waitForExistence(timeout: 10), "Slice A's flag has no Audio tab")
        flagTab.tap()
        let panel = app.descendants(matching: .any)["flagPanelAudio"]
        XCTAssertTrue(panel.waitForExistence(timeout: 10), "Slice A's Audio panel did not open")
        print("KeyedPttUITests N3: Audio panel \(panel.frame), held PTT \(ptt.frame)")
        XCTAssertEqual(ptt.frame, held, "The PTT moved when the flag's panel opened while keyed")
        XCTAssertTrue(gauges.exists && ptt.isSelected, "Opening the flag's panel unkeyed")
        try XCTSkipUnless(panel.frame.contains(spot),
                          "Slice A's Audio panel at \(panel.frame) does not open out under the PTT at \(held)")
        // What is on top at the PTT's spot is the PTT, not the panel under it.
        tap(app, at: spot)
        XCTAssertTrue(gauges.waitForNonExistence(timeout: 10),
                      "A tap where the PTT is held over the flag's panel did not unkey")
        XCTAssertFalse(ptt.isSelected, "The PTT still reads keyed")
    }

    /// A tap at `point` in the app's own coordinates.
    @MainActor
    private func tap(_ app: XCUIApplication, at point: CGPoint) {
        app.coordinate(withNormalizedOffset: .zero).withOffset(CGVector(dx: point.x, dy: point.y)).tap()
    }

    @MainActor
    private func waitUntil(_ condition: () -> Bool) -> Bool {
        for _ in 0..<100 {
            if condition() {
                return true
            }
            RunLoop.current.run(until: Date().addingTimeInterval(0.1))
        }
        return condition()
    }

    @MainActor
    private func launch() -> XCUIApplication {
        let app = XCUIApplication()
        // The band with slice A and the Core's catalogue, and a Core that
        // lets this device key, with no Core (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", "-NereusKeyedFixture"]
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        // The Core's Display description, so the pill shows in full.
        app.launchEnvironment["NEREUS_UITEST_DISPLAY"] = root.appendingPathComponent("resources/setup/display.json").path
        app.launchFresh()
        return app
    }
}
