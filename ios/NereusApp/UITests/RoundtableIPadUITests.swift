// NereusSDR for iOS: the iPad's Roundtable applet under a finger: its place in the column, the flag line's scroll, widen and narrow, the sheet with the column hidden, and the first run as a form sheet
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 19, the iPad applet's UI tests.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
//
// The fixture (`-NereusRoundtableFixture`) has no Core: slice A sits in a
// Roundtable room of four, with two messages, one naming you.
final class RoundtableIPadUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    override func tearDown() async throws {
        await MainActor.run { XCUIDevice.shared.orientation = .portrait }
    }

    /// Spec 11.4: on its side the applet sits second, under the S-Meter,
    /// in the 320-point column, and nothing in it runs past the column.
    /// While it is in view its box is pinned at the column's foot.
    @MainActor
    func testTheAppletSitsSecondAndFitsTheColumn() throws {
        let app = try launch()
        let column = app.descendants(matching: .any)["appletColumn"]
        let applet = try applet(app)
        let meter = app.descendants(matching: .any)["analogSMeter"]
        XCTAssertTrue(meter.exists)
        XCTAssertEqual(column.frame.width, 320, accuracy: 1)
        XCTAssertGreaterThanOrEqual(applet.frame.minY, meter.frame.maxY - 1, "Roundtable is not under the S-Meter")
        let rx = app.descendants(matching: .any)["rxPanel"]
        if rx.exists, rx.frame.height > 0 {
            XCTAssertGreaterThan(rx.frame.minY, applet.frame.minY, "RX is above Roundtable")
        }
        let box = app.descendants(matching: .any)["roundtableBox"]
        XCTAssertTrue(box.waitForExistence(timeout: 10), "the box is not pinned while the applet is in view")
        XCTAssertLessThanOrEqual(box.frame.maxY, column.frame.maxY + 1)
        XCTAssertTrue(app.descendants(matching: .any)["roundtableMentionPill"].exists, "the @1 pill is missing")
        try save("roundtable-ipad-ui-applet")
        assertInside(applet, column: column.frame)
        assertInside(app.descendants(matching: .any)["roundtableAppletFoot"], column: column.frame)
    }

    /// Spec 11.4: the flag's Roundtable line scrolls the column to the
    /// applet, and opens no sheet.
    @MainActor
    func testTheFlagLineScrollsTheColumnToTheApplet() throws {
        let app = try launch()
        let column = app.descendants(matching: .any)["appletColumn"]
        let applet = try applet(app)
        // Scroll the column on down to TX.
        for _ in 0..<4 {
            column.swipeUp()
        }
        waitUntil("the column did not scroll past the applet") { applet.frame.maxY < column.frame.minY + 120 }
        XCTAssertFalse(app.descendants(matching: .any)["roundtableBox"].exists, "the box stayed pinned off the applet")
        let line = app.descendants(matching: .any)["roundtableLineA"]
        XCTAssertTrue(line.exists)
        line.tap()
        waitUntil("the line did not scroll the column to the applet") {
            abs(applet.frame.minY - column.frame.minY) < 30
        }
        XCTAssertFalse(app.descendants(matching: .any)["roundtableSheet"].exists, "the line opened the sheet")
        XCTAssertTrue(app.descendants(matching: .any)["roundtableBox"].waitForExistence(timeout: 10))
        try save("roundtable-ipad-ui-flag-line-scrolled")
    }

    /// Spec 11.4, S2: Widen and Narrow, and the swipe up from the tab bar,
    /// widen the column to the room list beside the room and put it back.
    @MainActor
    func testWidenAndTheTabBarSwipeWidenAndNarrow() throws {
        let app = try launch()
        let column = app.descendants(matching: .any)["appletColumn"]
        _ = try applet(app)
        let widen = app.buttons["roundtableWiden"]
        XCTAssertEqual(widen.label, "Widen Roundtable")
        widen.tap()
        waitUntil("Widen did not widen the column") { abs(column.frame.width - 640) < 2 }
        let list = app.descendants(matching: .any)["roundtableWideList"]
        XCTAssertTrue(list.waitForExistence(timeout: 10))
        XCTAssertEqual(list.frame.width, 250, accuracy: 2)
        XCTAssertFalse(app.descendants(matching: .any)["analogSMeter"].exists, "the S-Meter shows widened")
        if !app.descendants(matching: .any)["roundtableBox"].waitForExistence(timeout: 10) {
            try save("roundtable-ipad-ui-widened-no-box")
            print(app.descendants(matching: .any)["roundtableApplet"].debugDescription)
            XCTFail("the widened room has no box")
        }
        try save("roundtable-ipad-ui-widened")
        assertInside(column, column: column.frame)
        XCTAssertEqual(widen.label, "Narrow Roundtable")
        widen.tap()
        waitUntil("Narrow did not put the column back") { abs(column.frame.width - 320) < 2 }

        let bar = app.descendants(matching: .any)["Sections"]
        XCTAssertTrue(bar.exists)
        let start = CGPoint(x: bar.frame.midX + 20, y: bar.frame.minY + 10)
        drag(app, from: start, to: CGPoint(x: start.x, y: start.y - 240))
        waitUntil("up from the tab bar did not widen Roundtable") { abs(column.frame.width - 640) < 2 }
        drag(app, from: start, to: CGPoint(x: start.x, y: start.y - 240))
        waitUntil("the same swipe did not narrow it") { abs(column.frame.width - 320) < 2 }
        XCTAssertFalse(app.descendants(matching: .any)["roundtableSheet"].exists)
    }

    /// Spec 11.4: with the column hidden the iPad uses the phone's sheet.
    @MainActor
    func testWithTheColumnHiddenTheLineOpensThePhoneSheet() throws {
        let app = try launch()
        let column = app.descendants(matching: .any)["appletColumn"]
        _ = try applet(app)
        app.buttons["appletColumnButton"].tap()
        XCTAssertTrue(column.waitForNonExistence(timeout: 10))
        app.descendants(matching: .any)["roundtableLineA"].tap()
        let sheet = app.descendants(matching: .any)["roundtableSheet"]
        XCTAssertTrue(sheet.waitForExistence(timeout: 10), "the line did not open the sheet with the column hidden")
        let ok = app.buttons["roundtableFirstRunNoteOk"]
        if ok.waitForExistence(timeout: 2) {
            ok.tap()
        }
        try save("roundtable-ipad-ui-column-hidden-sheet")
        // Showing the column again gives the room back to the applet.
        app.buttons["appletColumnButton"].tap()
        XCTAssertTrue(column.waitForExistence(timeout: 10))
        XCTAssertTrue(sheet.waitForNonExistence(timeout: 10))
    }

    /// Spec 11.4: the first-run question is a form sheet in the middle of
    /// the screen, from the applet and from the phone's sheet alike.
    @MainActor
    func testTheFirstRunIsACenteredFormSheet() throws {
        let app = try launch(firstRun: true)
        let applet = try applet(app)
        let prompt = app.descendants(matching: .any)["roundtableFirstRunPrompt"]
        XCTAssertTrue(prompt.waitForExistence(timeout: 10))
        XCTAssertFalse(app.descendants(matching: .any)["roundtableBox"].exists, "the box shows before the answer")
        assertInside(applet, column: app.descendants(matching: .any)["appletColumn"].frame)
        try save("roundtable-ipad-ui-first-run-line")
        app.buttons["roundtableFirstRunChoose"].tap()
        try assertCenteredQuestion(app)
        try save("roundtable-ipad-ui-first-run-form")
        app.swipeDown(velocity: .fast)
        let question = app.descendants(matching: .any)["roundtableFirstRun"]
        if question.exists {
            question.swipeDown(velocity: .fast)
        }
        XCTAssertTrue(question.waitForNonExistence(timeout: 10))

        // With the column hidden, the sheet asks from the same form sheet.
        app.buttons["appletColumnButton"].tap()
        app.descendants(matching: .any)["roundtableLineA"].tap()
        XCTAssertTrue(app.descendants(matching: .any)["roundtableSheet"].waitForExistence(timeout: 10))
        XCTAssertTrue(prompt.waitForExistence(timeout: 10))
        app.buttons["roundtableFirstRunChoose"].tap()
        try assertCenteredQuestion(app)
        try save("roundtable-ipad-ui-first-run-form-sheet-path")
    }

    // MARK: Helpers

    @MainActor
    private func assertCenteredQuestion(_ app: XCUIApplication) throws {
        let question = app.descendants(matching: .any)["roundtableFirstRun"]
        XCTAssertTrue(question.waitForExistence(timeout: 10))
        Thread.sleep(forTimeInterval: 0.6)
        let window = app.windows.firstMatch.frame
        XCTAssertEqual(question.frame.midX, window.midX, accuracy: 4, "the question is not centered")
        XCTAssertLessThan(question.frame.width, window.width - 200, "the question is not a form sheet")
        XCTAssertTrue(app.staticTexts["Roundtable is public"].exists)
    }

    /// What a reader sees: text, controls and pictures.
    private static let content: Set<XCUIElement.ElementType> = [.staticText, .button, .textField, .textView, .image,
                                                                 .slider, .switch, .menuButton, .link]

    /// Everything a reader sees in `element` lies within the column's
    /// sides. (Containers are left out: a selectable message's container
    /// reports 4 points of inset past its text.)
    @MainActor
    private func assertInside(_ element: XCUIElement, column: CGRect, file: StaticString = #filePath,
                              line: UInt = #line) {
        XCTAssertTrue(element.exists, file: file, line: line)
        let all = element.descendants(matching: .any).allElementsBoundByIndex
        var outside: [String] = []
        for item in all {
            let frame = item.frame
            guard Self.content.contains(item.elementType), frame.width > 0, frame.height > 0,
                  frame.minY < column.maxY, frame.maxY > column.minY else {
                continue
            }
            if frame.minX < column.minX - 1 || frame.maxX > column.maxX + 1 {
                outside.append("\(item.elementType.rawValue) '\(item.identifier)' '\(item.label)' \(frame)")
            }
        }
        XCTAssertTrue(outside.isEmpty, "outside the column \(column): " + outside.joined(separator: "; "),
                      file: file, line: line)
    }

    @MainActor
    private func applet(_ app: XCUIApplication) throws -> XCUIElement {
        let applet = app.descendants(matching: .any)["roundtableApplet"]
        if !applet.waitForExistence(timeout: 30) {
            try save("roundtable-ipad-ui-no-applet")
            print(app.debugDescription)
            XCTFail("the Roundtable applet never showed")
        }
        let ok = app.buttons["roundtableFirstRunNoteOk"]
        if ok.waitForExistence(timeout: 2) {
            ok.tap()
        }
        // Let the column settle before reading where things are.
        Thread.sleep(forTimeInterval: 0.5)
        return applet
    }

    @MainActor
    private func drag(_ app: XCUIApplication, from: CGPoint, to: CGPoint) {
        let origin = app.coordinate(withNormalizedOffset: .zero)
        let start = origin.withOffset(CGVector(dx: from.x, dy: from.y))
        start.press(forDuration: 0.05, thenDragTo: origin.withOffset(CGVector(dx: to.x, dy: to.y)),
                    withVelocity: .default, thenHoldForDuration: 0.05)
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
    private func launch(firstRun: Bool = false) throws -> XCUIApplication {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The applet column is the iPad's")
        XCUIDevice.shared.orientation = .landscapeLeft
        let app = XCUIApplication()
        // The band with slice A in a Roundtable room, with no Core (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", "-NereusRoundtableFixture"]
            + (firstRun ? ["-NereusRoundtableFirstRun"] : [])
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launch()
        return app
    }
}
