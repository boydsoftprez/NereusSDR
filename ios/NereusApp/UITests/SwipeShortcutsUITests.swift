// NereusSDR for iOS: the Panadapter tab's swipes under a finger: the edge zones, the tab bar, and a TX panel that never keys
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 18, the swipes on the panadapter under a
// finger. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
//
// The fixture (`-NereusRoundtableFixture`) has no Core. Every ask to key
// (PTT, MOX, the hold PTT, TUNE) is written to `pttKeyLog` before anything
// decides whether it may, so a swipe that keyed shows there even with no
// Core to key.
final class SwipeShortcutsUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    /// Spec 11.3: a swipe in from the left edge opens the RX panel from the
    /// left, one from the right edge the TX panel from the right; while one
    /// is up another swipe opens nothing; a tap on the band or a swipe back
    /// closes it. Neither keys.
    @MainActor
    func testTheEdgesOpenTheRxAndTxPanelsFromTheirSides() throws {
        let app = launch()
        let y = try zoneY(app)
        let width = app.windows.firstMatch.frame.width
        drag(app, from: CGPoint(x: 6, y: y), to: CGPoint(x: 230, y: y))
        let rx = app.descendants(matching: .any)["rxPanelDrawer"]
        XCTAssertTrue(rx.waitForExistence(timeout: 10), "the left edge did not open the RX panel")
        waitUntil("the RX panel did not settle at the left") { rx.frame.minX.magnitude < 1 }
        try save("swipe-ui-rx-open")
        // While it is up, the right edge opens nothing.
        drag(app, from: CGPoint(x: width - 6, y: y), to: CGPoint(x: width - 230, y: y))
        Thread.sleep(forTimeInterval: 0.6)
        XCTAssertFalse(app.descendants(matching: .any)["txPanelDrawer"].exists, "a swipe opened over a panel")
        // A tap on the dimmed band closes it.
        app.coordinate(withNormalizedOffset: .zero).withOffset(CGVector(dx: width - 40, dy: y)).tap()
        XCTAssertTrue(rx.waitForNonExistence(timeout: 10), "a tap on the band did not close the RX panel")
        drag(app, from: CGPoint(x: width - 6, y: y), to: CGPoint(x: width - 230, y: y))
        let tx = app.descendants(matching: .any)["txPanelDrawer"]
        XCTAssertTrue(tx.waitForExistence(timeout: 10), "the right edge did not open the TX panel")
        waitUntil("the TX panel did not settle at the right") { abs(tx.frame.maxX - width) < 1 }
        try save("swipe-ui-tx-open")
        // A swipe back toward its side closes it.
        drag(app, from: CGPoint(x: 30, y: y), to: CGPoint(x: 160, y: y))
        XCTAssertTrue(tx.waitForNonExistence(timeout: 10), "a swipe back did not close the TX panel")
        Thread.sleep(forTimeInterval: 0.5)
        XCTAssertEqual(keyLog(app), "none", "a swipe keyed")
        // The toolbar's buttons stay RX left and TX right.
        let rxButton = app.buttons["rxPanel"]
        let txButton = app.buttons["txPanel"]
        XCTAssertTrue(rxButton.exists && txButton.exists)
        XCTAssertLessThan(rxButton.frame.midX, txButton.frame.midX)
    }

    /// A short drag in from an edge springs back: no panel, and the band
    /// does not move.
    @MainActor
    func testAShortDragFromAnEdgeSpringsBack() throws {
        let app = launch()
        let y = try zoneY(app)
        let flag = app.descendants(matching: .any)["flagA"]
        let before = flag.frame
        drag(app, from: CGPoint(x: 6, y: y), to: CGPoint(x: 46, y: y))
        Thread.sleep(forTimeInterval: 0.8)
        XCTAssertFalse(app.descendants(matching: .any)["rxPanelDrawer"].exists, "a short drag opened the panel")
        XCTAssertEqual(flag.frame.minX, before.minX, accuracy: 1, "a short swipe moved the band")
    }

    /// A drag that starts anywhere but an edge zone moves the band, as it always did.
    @MainActor
    func testADragInTheMiddleMovesTheBand() throws {
        let app = launch()
        let y = try zoneY(app)
        let flag = app.descendants(matching: .any)["flagA"]
        let before = flag.frame
        // Leftward, so slice A's flag has room to follow the band (rightward it meets B's and steps aside).
        drag(app, from: CGPoint(x: 300, y: y), to: CGPoint(x: 200, y: y))
        waitUntil("a drag in the middle did not move the band") { flag.frame.minX < before.minX - 50 }
        XCTAssertFalse(app.descendants(matching: .any)["rxPanelDrawer"].exists)
        XCTAssertFalse(app.descendants(matching: .any)["txPanelDrawer"].exists)
    }

    /// With the right edge set to Off, a drag in from it moves the band.
    @MainActor
    func testOffGivesTheEdgeBackToTheBand() throws {
        let app = launch(extra: ["-phone.swipe.rightEdge", "off"])
        let y = try zoneY(app)
        let width = app.windows.firstMatch.frame.width
        let flag = app.descendants(matching: .any)["flagA"]
        let before = flag.frame
        drag(app, from: CGPoint(x: width - 6, y: y), to: CGPoint(x: width - 126, y: y))
        waitUntil("the edge set to Off did not move the band") { flag.frame.minX < before.minX - 60 }
        XCTAssertFalse(app.descendants(matching: .any)["txPanelDrawer"].exists, "Off still opened the TX panel")
    }

    /// Up from the tab bar opens Roundtable on my frequency, half height.
    @MainActor
    func testUpFromTheTabBarOpensRoundtable() throws {
        let app = launch()
        _ = try zoneY(app)
        let bar = app.descendants(matching: .any)["Sections"]
        XCTAssertTrue(bar.waitForExistence(timeout: 10))
        let start = CGPoint(x: bar.frame.midX + 20, y: bar.frame.minY + 10)
        drag(app, from: start, to: CGPoint(x: start.x, y: start.y - 240))
        let sheet = app.descendants(matching: .any)["roundtableSheet"]
        XCTAssertTrue(sheet.waitForExistence(timeout: 10), "up from the tab bar did not open Roundtable")
        let ok = app.buttons["roundtableFirstRunNoteOk"]
        if ok.waitForExistence(timeout: 2) {
            ok.tap()
        }
        try save("swipe-ui-roundtable")
        XCTAssertEqual(keyLog(app), "none")
    }

    /// The never-key rule under a finger: the TX panel opened by a swipe
    /// keys nothing, and nor does any swipe or drag that starts on PTT.
    /// A tap on PTT does ask to key, so the record can see one.
    @MainActor
    func testTheTxPanelBySwipeAndSwipesFromPttNeverKey() throws {
        let app = launch()
        let y = try zoneY(app)
        let width = app.windows.firstMatch.frame.width
        drag(app, from: CGPoint(x: width - 6, y: y), to: CGPoint(x: width - 260, y: y))
        let tx = app.descendants(matching: .any)["txPanelDrawer"]
        XCTAssertTrue(tx.waitForExistence(timeout: 10))
        Thread.sleep(forTimeInterval: 1.0)
        XCTAssertEqual(keyLog(app), "none", "opening the TX panel by a swipe keyed")
        app.coordinate(withNormalizedOffset: .zero).withOffset(CGVector(dx: 30, dy: y)).tap()
        XCTAssertTrue(tx.waitForNonExistence(timeout: 10))
        let ptt = app.descendants(matching: .any)["ptt"]
        XCTAssertTrue(ptt.waitForExistence(timeout: 10))
        let centre = ptt.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
        // Right, toward the band; up, toward the left edge zone; left, off the edge.
        centre.press(forDuration: 0.02, thenDragTo: centre.withOffset(CGVector(dx: 250, dy: 0)))
        centre.press(forDuration: 0.02, thenDragTo: centre.withOffset(CGVector(dx: 0, dy: -250)))
        centre.press(forDuration: 0.02, thenDragTo: centre.withOffset(CGVector(dx: -60, dy: 0)))
        // From PTT's left edge inward, as a left-edge swipe would go.
        let edge = ptt.coordinate(withNormalizedOffset: CGVector(dx: 0.05, dy: 0.5))
        edge.press(forDuration: 0.02, thenDragTo: edge.withOffset(CGVector(dx: 220, dy: 0)))
        ptt.swipeRight()
        ptt.swipeLeft()
        Thread.sleep(forTimeInterval: 0.6)
        XCTAssertEqual(keyLog(app), "none", "a swipe or a drag from PTT keyed")
        XCTAssertFalse(app.descendants(matching: .any)["rxPanelDrawer"].exists, "a drag from PTT opened a panel")
        XCTAssertFalse(tx.exists, "a drag from PTT opened a panel")
        // The control: a tap on PTT asks to key.
        ptt.tap()
        waitUntil("a tap on PTT did not ask to key") { self.keyLog(app).hasPrefix("tap") }
    }

    // MARK: Helpers

    /// A height inside the edge zones: on the band, below the flags, above PTT.
    @MainActor
    private func zoneY(_ app: XCUIApplication) throws -> CGFloat {
        let flag = app.descendants(matching: .any)["flagA"]
        if !flag.waitForExistence(timeout: 30) {
            try save("swipe-ui-no-flag")
            XCTFail("slice A's flag never showed")
        }
        let ptt = app.descendants(matching: .any)["ptt"]
        XCTAssertTrue(ptt.waitForExistence(timeout: 10))
        // Let the band settle before reading where things are.
        Thread.sleep(forTimeInterval: 0.5)
        return ptt.frame.minY - 60
    }

    @MainActor
    private func drag(_ app: XCUIApplication, from: CGPoint, to: CGPoint) {
        let origin = app.coordinate(withNormalizedOffset: .zero)
        let start = origin.withOffset(CGVector(dx: from.x, dy: from.y))
        start.press(forDuration: 0.05, thenDragTo: origin.withOffset(CGVector(dx: to.x, dy: to.y)),
                    withVelocity: .default, thenHoldForDuration: 0.05)
    }

    @MainActor
    private func keyLog(_ app: XCUIApplication) -> String {
        app.descendants(matching: .any)["pttKeyLog"].label
    }

    @MainActor
    private func waitUntil(_ what: String, timeout: TimeInterval = 10, _ condition: @escaping @MainActor () -> Bool) {
        let predicate = NSPredicate { _, _ in
            if Thread.isMainThread {
                return MainActor.assumeIsolated { condition() }
            }
            return DispatchQueue.main.sync { MainActor.assumeIsolated { condition() } }
        }
        let expectation = XCTNSPredicateExpectation(predicate: predicate, object: nil)
        XCTAssertEqual(XCTWaiter.wait(for: [expectation], timeout: timeout), .completed, what)
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

    @MainActor
    private func launch(extra: [String] = []) -> XCUIApplication {
        let app = XCUIApplication()
        // The band with slice A in a Roundtable room, with no Core (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", "-NereusRoundtableFixture"] + extra
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launch()
        return app
    }
}
