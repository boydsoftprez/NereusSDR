// NereusSDR for iOS: the Roundtable sheet under a finger: its heights, the hold PTT that never keys on a swipe, and the first-run question
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-24: Roundtable plan Task 17, the phone's Roundtable sheet and its
// press-and-hold PTT under a finger. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 19, on an iPad the sheet asks through its
// Choose line, which opens the centered form. J.J. Boyd (KG4VCF), AI-assisted
// via Claude Code.
// 2026-10-25: JJ's answer: on a phone the first-run question opens at full
// height, Stay hidden and Go live on screen without a scroll. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-11-04: One sheet on every frequency under a finger: toggle, Back in place, the more
// button, and a band drag that leaves it open (overlay design D22, D27, D36, D38).
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
//
// The fixture (`-NereusRoundtableFixture`) has no Core: PTT takes presses
// and writes each press and release the hold makes in `pttHoldLog`, so a
// swipe or a drag that keyed, or a release that never came, shows there.
final class RoundtableUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    /// Spec 11.3: the flag's Roundtable line opens the sheet at half
    /// height, the handle makes it full height and back, and the close
    /// button puts the band's tap PTT back.
    @MainActor
    func testTheFlagLineOpensTheSheetAtHalfAndTheHandleMakesItFull() throws {
        let app = launch()
        XCTAssertTrue(app.buttons["ptt"].exists || app.descendants(matching: .any)["ptt"].exists)
        XCTAssertEqual(app.descendants(matching: .any)["ptt"].label, "Push to talk, tap to key and tap again to unkey")
        let sheet = try openSheet(app)
        let band = app.descendants(matching: .any)["flagA"].frame
        let half = sheet.frame
        app.buttons["roundtableHandle"].tap()
        waitUntil("the handle did not make the sheet full height") { sheet.frame.minY < half.minY - 100 }
        let full = sheet.frame
        XCTAssertLessThanOrEqual(full.minY, band.minY, "full height starts at the band's top")
        app.buttons["roundtableHandle"].tap()
        waitUntil("the handle did not bring the sheet back to half") { abs(sheet.frame.minY - half.minY) < 2 }
        try save("roundtable-ui-half")
        app.buttons["roundtableClose"].tap()
        XCTAssertTrue(sheet.waitForNonExistence(timeout: 10))
        XCTAssertEqual(app.descendants(matching: .any)["ptt"].label, "Push to talk, tap to key and tap again to unkey")
        XCTAssertEqual(holdLog(app), "none")
    }

    /// D38: the flag line, the card and a tag open one sheet. The same thing closes it,
    /// Back stays up on your own, and the slice's flag never changes.
    @MainActor
    func testOneSheetTogglesAndBackStaysOnYourOwn() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom == .pad, "An iPad shows the applet or the panel")
        let app = launch(frequency: true)
        let flag = app.descendants(matching: .any)["flagA"]
        let line = app.descendants(matching: .any)["roundtableLineA"]
        XCTAssertTrue(line.waitForExistence(timeout: 30))
        let before = flag.label
        let sheet = try openSheet(app)
        XCTAssertTrue(app.descendants(matching: .any)["roundtableViewTuneReason"].waitForExistence(timeout: 10))
        XCTAssertFalse(app.descendants(matching: .any)["roundtableViewTune"].isEnabled)
        XCTAssertTrue(app.descendants(matching: .any)["roundtableBox"].exists, "your own keeps its box")
        try save("roundtable-ui-one-sheet-own")
        line.tap()
        XCTAssertTrue(sheet.waitForNonExistence(timeout: 10), "the same line did not close the sheet")
        let tag = app.descendants(matching: .any)["roundtableTag.lsb-7225000"]
        XCTAssertTrue(tag.waitForExistence(timeout: 10))
        tag.tap()
        XCTAssertTrue(sheet.waitForExistence(timeout: 10))
        let back = app.descendants(matching: .any)["roundtableBackToSlice"]
        XCTAssertTrue(back.waitForExistence(timeout: 10))
        let half = sheet.frame
        try save("roundtable-ui-one-sheet-viewed")
        back.tap()
        XCTAssertTrue(back.waitForNonExistence(timeout: 10), "Back did not return to your own")
        XCTAssertTrue(sheet.exists, "Back closed the sheet")
        XCTAssertEqual(sheet.frame.minY, half.minY, accuracy: 2, "Back moved the sheet")
        tag.tap()
        XCTAssertTrue(back.waitForExistence(timeout: 10))
        tag.tap()
        XCTAssertTrue(sheet.waitForNonExistence(timeout: 10), "the same tag did not close the sheet")
        XCTAssertEqual(flag.label, before, "a tap moved the slice")
        XCTAssertEqual(holdLog(app), "none")
    }

    /// D36: the sheet's more button has Signal and Cancel and nothing else.
    @MainActor
    func testTheMoreButtonHasSignalAndCancel() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom == .pad, "An iPad shows the applet or the panel")
        let app = launch(frequency: true)
        _ = try openSheet(app)
        let more = app.buttons["roundtableMore"]
        XCTAssertTrue(more.waitForExistence(timeout: 10))
        more.tap()
        let signal = app.buttons.matching(NSPredicate(format: "label BEGINSWITH 'Signal: '")).firstMatch
        XCTAssertTrue(signal.waitForExistence(timeout: 10))
        try save("roundtable-ui-more")
        XCTAssertTrue(app.buttons["Cancel"].exists)
        XCTAssertFalse(app.buttons["Read reports"].exists)
        // The sheet's own Tune is the only one: the menu has none.
        XCTAssertEqual(app.buttons.matching(NSPredicate(format: "label BEGINSWITH 'Tune slice'")).count, 1)
        app.buttons["Cancel"].tap()
        XCTAssertTrue(signal.waitForNonExistence(timeout: 10))
    }

    /// D27, D38: a drag on the band tunes and leaves the sheet open on what it shows.
    @MainActor
    func testDraggingTheBandLeavesTheSheetOpen() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom == .pad, "An iPad shows the applet or the panel")
        let app = launch(frequency: true)
        let flag = app.descendants(matching: .any)["flagA"]
        XCTAssertTrue(flag.waitForExistence(timeout: 30))
        let before = flag.label
        let tag = app.descendants(matching: .any)["roundtableTag.lsb-7225000"]
        XCTAssertTrue(tag.waitForExistence(timeout: 10))
        tag.tap()
        let sheet = app.descendants(matching: .any)["roundtableSheet"]
        XCTAssertTrue(sheet.waitForExistence(timeout: 10))
        let back = app.descendants(matching: .any)["roundtableBackToSlice"]
        XCTAssertTrue(back.waitForExistence(timeout: 10))
        // A drag on the slice's flag is the band's own tuning gesture, above the sheet.
        flag.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
            .press(forDuration: 0.05, thenDragTo: flag.coordinate(withNormalizedOffset: CGVector(dx: 0.1, dy: 0.5)))
        waitUntil("the drag did not tune the slice") { flag.label != before }
        XCTAssertTrue(sheet.exists, "the drag closed the sheet")
        XCTAssertTrue(back.exists, "the drag ended the view")
        XCTAssertEqual(holdLog(app), "none")
        try save("roundtable-ui-drag-keeps-sheet")
    }

    /// U2: with the sheet open PTT is press and hold beside the box, and
    /// a press that rests on it keys until the finger lifts.
    @MainActor
    func testTheHoldPttKeysWhileHeldAndLetsGoOnLift() throws {
        let app = launch()
        _ = try openSheet(app)
        let ptt = app.descendants(matching: .any)["ptt"]
        XCTAssertTrue(ptt.waitForExistence(timeout: 10))
        XCTAssertEqual(ptt.label, "Push to talk, press and hold to talk")
        XCTAssertLessThan(ptt.frame.width, 70, "the hold PTT is smaller than the band's")
        let box = app.descendants(matching: .any)["roundtableBox"]
        XCTAssertTrue(box.exists)
        XCTAssertGreaterThanOrEqual(box.frame.minX - ptt.frame.maxX, 24, "PTT keeps 24 points clear of the box")
        ptt.press(forDuration: 1.0)
        waitUntil("the press and its release were not both made") { self.holdLog(app) == "press,release" }
        // A quick tap is not a press.
        ptt.tap()
        Thread.sleep(forTimeInterval: 0.5)
        XCTAssertEqual(holdLog(app), "press,release")
    }

    /// A swipe or a drag that starts on the hold PTT never keys, whichever
    /// way it goes, and nor does a drag that starts elsewhere and passes
    /// over it.
    @MainActor
    func testSwipesAndDragsOverTheHoldPttNeverKey() throws {
        let app = launch()
        let sheet = try openSheet(app)
        let ptt = app.descendants(matching: .any)["ptt"]
        XCTAssertTrue(ptt.waitForExistence(timeout: 10))
        let centre = ptt.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
        // Up, as the sheet's own drag would go.
        centre.press(forDuration: 0.02, thenDragTo: centre.withOffset(CGVector(dx: 0, dy: -250)))
        // Right, across the message box.
        centre.press(forDuration: 0.02, thenDragTo: centre.withOffset(CGVector(dx: 250, dy: 0)))
        // Down, off the screen's foot.
        centre.press(forDuration: 0.02, thenDragTo: centre.withOffset(CGVector(dx: 0, dy: 120)))
        // A swipe.
        ptt.swipeUp()
        // A drag from the sheet's room down over the PTT.
        let above = centre.withOffset(CGVector(dx: 0, dy: -180))
        above.press(forDuration: 0.05, thenDragTo: centre.withOffset(CGVector(dx: 0, dy: 10)))
        // A drag on the handle that passes over the PTT on its way down.
        let handle = app.buttons["roundtableHandle"].coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
        if sheet.exists {
            handle.press(forDuration: 0.05, thenDragTo: centre)
        }
        Thread.sleep(forTimeInterval: 0.5)
        XCTAssertEqual(holdLog(app), "none", "a swipe or a drag keyed")
    }

    /// A held finger that slides off the button lets go there.
    @MainActor
    func testAHeldPressThatSlidesOffLetsGo() throws {
        let app = launch()
        _ = try openSheet(app)
        let ptt = app.descendants(matching: .any)["ptt"]
        XCTAssertTrue(ptt.waitForExistence(timeout: 10))
        let centre = ptt.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
        centre.press(forDuration: 0.8, thenDragTo: centre.withOffset(CGVector(dx: 200, dy: -200)))
        waitUntil("the slide off did not let go") { self.holdLog(app) == "press,release" }
    }

    /// Closing the sheet takes the hold PTT away and puts the band's tap
    /// PTT back; sending the app to the background and back keys nothing.
    @MainActor
    func testClosingTheSheetAndLeavingTheAppKeyNothing() throws {
        let app = launch()
        let sheet = try openSheet(app)
        app.buttons["roundtableClose"].tap()
        XCTAssertTrue(sheet.waitForNonExistence(timeout: 10))
        _ = try openSheet(app)
        XCUIDevice.shared.press(.home)
        app.activate()
        XCTAssertTrue(app.descendants(matching: .any)["ptt"].waitForExistence(timeout: 10))
        XCTAssertEqual(holdLog(app), "none")
    }

    /// Spec 11.2: before the station's answer the sheet asks it, Hide my
    /// station moves the main button, and an answer with no Core to take
    /// it says so and leaves the question open.
    @MainActor
    func testTheFirstRunQuestionIsAskedAndItsAnswerNeedsTheCore() throws {
        let app = launch(firstRun: true)
        let sheet = try openSheet(app, dismissNote: false)
        // On an iPad the sheet asks through its Choose line (spec 11.4).
        let choose = app.buttons["roundtableFirstRunChoose"]
        if UIDevice.current.userInterfaceIdiom == .pad, choose.waitForExistence(timeout: 10) {
            choose.tap()
        }
        let question = app.descendants(matching: .any)["roundtableFirstRun"]
        XCTAssertTrue(question.waitForExistence(timeout: 10))
        XCTAssertTrue(app.staticTexts["Roundtable is public"].exists)
        XCTAssertFalse(app.descendants(matching: .any)["roundtableBox"].exists, "the box shows before the answer")
        if UIDevice.current.userInterfaceIdiom == .phone {
            // The line opens the sheet at half; the question takes it to full
            // height (its handle offers half again), well above the middle.
            let screen = app.windows.firstMatch.frame
            waitUntil("the first-run question did not take the full height") {
                app.buttons["roundtableHandle"].label == "Half height" && sheet.frame.minY < screen.midY - 100
            }
            // Both answers on screen, no scroll to reach them.
            for id in ["roundtableStayHidden", "roundtableGoLiveFirstRun"] {
                let button = app.buttons[id]
                XCTAssertTrue(button.waitForExistence(timeout: 10), "\(id) is missing")
                XCTAssertTrue(button.isHittable, "\(id) needs a scroll")
                XCTAssertTrue(screen.contains(button.frame), "\(id) runs off the screen: \(button.frame)")
            }
        }
        let hide = app.buttons["roundtableHideSwitch"]
        XCTAssertEqual(hide.value as? String, "Off")
        hide.tap()
        waitUntil("Hide my station did not turn on") { hide.value as? String == "On" }
        try save("roundtable-ui-first-run")
        app.buttons["roundtableStayHidden"].tap()
        XCTAssertTrue(app.descendants(matching: .any)["roundtableFirstRunError"].waitForExistence(timeout: 30))
        XCTAssertTrue(question.exists, "the question closed with no Core to take the answer")
    }

    // MARK: Helpers

    @MainActor
    private func openSheet(_ app: XCUIApplication, dismissNote: Bool = true) throws -> XCUIElement {
        let line = app.descendants(matching: .any)["roundtableLineA"]
        if !line.waitForExistence(timeout: 30) {
            try save("no-roundtable-line")
            print(app.debugDescription)
            XCTFail("slice A's Roundtable line never showed")
        }
        line.tap()
        let sheet = app.descendants(matching: .any)["roundtableSheet"]
        XCTAssertTrue(sheet.waitForExistence(timeout: 10))
        let ok = app.buttons["roundtableFirstRunNoteOk"]
        if dismissNote, ok.waitForExistence(timeout: 2) {
            ok.tap()
        }
        return sheet
    }

    @MainActor
    private func holdLog(_ app: XCUIApplication) -> String {
        app.descendants(matching: .any)["pttHoldLog"].label
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
    private func launch(firstRun: Bool = false, frequency: Bool = false) -> XCUIApplication {
        let app = XCUIApplication()
        // The band with slice A in a Roundtable room, with no Core (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", "-NereusRoundtableFixture"]
            + (firstRun ? ["-NereusRoundtableFirstRun"] : [])
            + (frequency ? ["-NereusRoundtableFrequency"] : [])
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launch()
        return app
    }
}
