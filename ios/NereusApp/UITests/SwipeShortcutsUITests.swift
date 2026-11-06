// NereusSDR for iOS: the Panadapter tab's swipes under a finger: the edge zones, the tab bar, and a TX panel that never keys
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 18, the swipes on the panadapter under a
// finger. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-05: Right on an iPad too: it keeps the RX and TX panels on screen
// and starts both edges Off, so these tests turn the edges on there and look
// only at panels over the band. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code.

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
//
// The fixture (`-NereusRoundtableFixture`) has no Core. Every ask to key
// (PTT, MOX, the hold PTT, TUNE) is written to `pttKeyLog` before anything
// decides whether it may, so a swipe that keyed shows there even with no
// Core to key.
//
// On an iPad the same swipes run upright, with two differences by design.
// Both edges start Off (SwipeShortcuts.defaultTarget, spec 11.4, S2), so
// launch(rightEdge:) turns them on as Setup would. RxPanel and TxPanel, with
// their "rxPanelDrawer" and "txPanelDrawer" identifiers, always show in the
// front panel under the band (IPadLayout.frontPanel, FrontPanelColumns), so
// these tests ask overPanel(_:_:) for a panel over the band, not the
// identifier alone.
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
        waitUntil("the left edge did not open the RX panel") { self.overPanel(app, "rxPanelDrawer") != nil }
        waitUntil("the RX panel did not settle at the left") {
            self.overPanel(app, "rxPanelDrawer").map { $0.minX.magnitude < 1 } ?? false
        }
        try save("swipe-ui-rx-open")
        // While it is up, the right edge opens nothing.
        drag(app, from: CGPoint(x: width - 6, y: y), to: CGPoint(x: width - 230, y: y))
        Thread.sleep(forTimeInterval: 0.6)
        XCTAssertNil(overPanel(app, "txPanelDrawer"), "a swipe opened over a panel")
        // A tap on the dimmed band closes it.
        app.coordinate(withNormalizedOffset: .zero).withOffset(CGVector(dx: width - 40, dy: y)).tap()
        waitUntil("a tap on the band did not close the RX panel") { self.overPanel(app, "rxPanelDrawer") == nil }
        drag(app, from: CGPoint(x: width - 6, y: y), to: CGPoint(x: width - 230, y: y))
        waitUntil("the right edge did not open the TX panel") { self.overPanel(app, "txPanelDrawer") != nil }
        waitUntil("the TX panel did not settle at the right") {
            self.overPanel(app, "txPanelDrawer").map { abs($0.maxX - width) < 1 } ?? false
        }
        try save("swipe-ui-tx-open")
        // A swipe back toward its side closes it.
        drag(app, from: CGPoint(x: 30, y: y), to: CGPoint(x: 160, y: y))
        waitUntil("a swipe back did not close the TX panel") { self.overPanel(app, "txPanelDrawer") == nil }
        Thread.sleep(forTimeInterval: 0.5)
        XCTAssertEqual(keyLog(app), "none", "a swipe keyed")
        // The toolbar's buttons stay RX left and TX right. An iPad's toolbar has neither:
        // the panels sit in the front panel (IPadUITests.testUprightFrontPanelAndOnItsSideTheColumn).
        guard !onIPad else {
            return
        }
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
        XCTAssertNil(overPanel(app, "rxPanelDrawer"), "a short drag opened the panel")
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
        XCTAssertNil(overPanel(app, "rxPanelDrawer"))
        XCTAssertNil(overPanel(app, "txPanelDrawer"))
    }

    /// With the right edge set to Off, a drag in from it moves the band.
    @MainActor
    func testOffGivesTheEdgeBackToTheBand() throws {
        let app = launch(rightEdge: "off")
        let y = try zoneY(app)
        let width = app.windows.firstMatch.frame.width
        let flag = app.descendants(matching: .any)["flagA"]
        let before = flag.frame
        drag(app, from: CGPoint(x: width - 6, y: y), to: CGPoint(x: width - 126, y: y))
        waitUntil("the edge set to Off did not move the band") { flag.frame.minX < before.minX - 60 }
        XCTAssertNil(overPanel(app, "txPanelDrawer"), "Off still opened the TX panel")
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
        waitUntil("the right edge did not open the TX panel") { self.overPanel(app, "txPanelDrawer") != nil }
        Thread.sleep(forTimeInterval: 1.0)
        XCTAssertEqual(keyLog(app), "none", "opening the TX panel by a swipe keyed")
        app.coordinate(withNormalizedOffset: .zero).withOffset(CGVector(dx: 30, dy: y)).tap()
        waitUntil("a tap on the band did not close the TX panel") { self.overPanel(app, "txPanelDrawer") == nil }
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
        XCTAssertNil(overPanel(app, "rxPanelDrawer"), "a drag from PTT opened a panel")
        XCTAssertNil(overPanel(app, "txPanelDrawer"), "a drag from PTT opened a panel")
        // The control: a tap on PTT asks to key.
        ptt.tap()
        waitUntil("a tap on PTT did not ask to key") { self.keyLog(app).hasPrefix("tap") }
    }

    // MARK: Helpers

    private var onIPad: Bool {
        UIDevice.current.userInterfaceIdiom == .pad
    }

    /// Where the panel `identifier` shows over the band, or nil. On a phone
    /// that is the panel; upright on an iPad the same panel also stands in the
    /// front panel under the band, and that one does not count. All from one
    /// snapshot: a panel closing can't vanish between one frame and the next.
    @MainActor
    private func overPanel(_ app: XCUIApplication, _ identifier: String) -> CGRect? {
        guard let root = try? app.snapshot() else {
            XCTFail("no snapshot of the app")
            return nil
        }
        var panels: [CGRect] = []
        var frontTop: CGFloat?
        func visit(_ element: XCUIElementSnapshot) {
            if element.identifier == identifier {
                panels.append(element.frame)
            } else if element.identifier == "frontPanel", frontTop == nil {
                frontTop = element.frame.minY
            }
            element.children.forEach(visit)
        }
        visit(root)
        guard let frontTop else {
            return panels.first
        }
        return panels.first { $0.midY < frontTop }
    }

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
    private func launch(rightEdge: String? = nil) -> XCUIApplication {
        var edges: [String] = []
        if onIPad {
            // Upright, so the band has the whole width. Both edges start Off
            // here, so these tests turn them on as Setup would.
            XCUIDevice.shared.orientation = .portrait
            edges = ["-phone.swipe.leftEdge", "rxPanel", "-phone.swipe.rightEdge", rightEdge ?? "txPanel"]
        } else if let rightEdge {
            edges = ["-phone.swipe.rightEdge", rightEdge]
        }
        let app = XCUIApplication()
        // The band with slice A in a Roundtable room, with no Core (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", "-NereusRoundtableFixture"] + edges
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launch()
        return app
    }
}
