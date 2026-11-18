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
// 2026-11-05: These run upright on an iPad, where the flag line opens the
// sheet; on its side it scrolls the column instead. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-11-18: Your own frequency has no Tune and no note. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-11-18: The sheet stays where its handle is let go, down to its smallest height,
// and a long drag down closes it. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-11-18: SAVE, a tag, the card and the flag the sheet's top edge would cut are hidden whole,
// and back at the smallest height. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
//
// The fixture (`-NereusRoundtableFixture`) has no Core: PTT takes presses
// and writes each press and release the hold makes in `pttHoldLog`, so a
// swipe or a drag that keyed, or a release that never came, shows there.
final class RoundtableUITests: XCTestCase {
    /// Slice A's frequency in the fixture.
    private static let ownRoom = "lsb-7236400"

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
        XCTAssertTrue(app.descendants(matching: .any)["roundtableBox"].waitForExistence(timeout: 10), "your own keeps its box")
        XCTAssertFalse(app.descendants(matching: .any)["roundtableViewTune"].exists, "your own has no Tune")
        XCTAssertFalse(app.descendants(matching: .any)["roundtableViewTuneReason"].exists, "your own has no Tune note")
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
        // Your own frequency has no Tune, and the menu adds none.
        XCTAssertEqual(app.buttons.matching(NSPredicate(format: "label BEGINSWITH 'Tune slice'")).count, 0)
        app.buttons["Cancel"].tap()
        XCTAssertTrue(signal.waitForNonExistence(timeout: 10))
    }

    /// D27, D38: a drag on the slice's flag tunes and leaves the sheet open on what it shows.
    @MainActor
    func testDraggingTheSlicesFlagLeavesTheSheetOpen() throws {
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

    /// D27, D38: a drag on the empty band moves the band and leaves the sheet open on a view.
    @MainActor
    func testDraggingTheEmptyBandLeavesTheSheetOpenOnAView() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom == .pad, "An iPad shows the applet or the panel")
        let app = launch(frequency: true)
        let tag = app.descendants(matching: .any)["roundtableTag.lsb-7225000"]
        XCTAssertTrue(tag.waitForExistence(timeout: 30))
        tag.tap()
        let sheet = app.descendants(matching: .any)["roundtableSheet"]
        XCTAssertTrue(sheet.waitForExistence(timeout: 10))
        let back = app.descendants(matching: .any)["roundtableBackToSlice"]
        XCTAssertTrue(back.waitForExistence(timeout: 10))
        try dragTheEmptyBand(app, under: sheet, shot: "roundtable-ui-band-drag-view")
        XCTAssertTrue(back.exists, "the drag ended the view")
    }

    /// D27, D38: the same on your own frequency, where the sheet has no view to end.
    @MainActor
    func testDraggingTheEmptyBandLeavesTheSheetOpenOnYourOwn() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom == .pad, "An iPad shows the applet or the panel")
        let app = launch(frequency: true)
        let own = app.descendants(matching: .any)["roundtableTag.\(Self.ownRoom)"]
        XCTAssertTrue(own.waitForExistence(timeout: 30))
        own.tap()
        let sheet = app.descendants(matching: .any)["roundtableSheet"]
        XCTAssertTrue(sheet.waitForExistence(timeout: 10))
        XCTAssertTrue(app.descendants(matching: .any)["roundtableViewHead"].waitForExistence(timeout: 10))
        XCTAssertFalse(app.descendants(matching: .any)["roundtableBackToSlice"].exists)
        try dragTheEmptyBand(app, under: sheet, shot: "roundtable-ui-band-drag-own")
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

    /// The handle's drag: the sheet stays where it is let go, between its smallest
    /// height and full. A tap goes to full and back to that height, the sheet opens
    /// there again, and a drag let go well below its smallest height closes it.
    @MainActor
    func testTheSheetStaysWhereTheHandleIsLetGo() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom == .pad, "An iPad shows the applet or the panel")
        let app = launch(frequency: true)
        var sheet = try openSheet(app)
        let handle = app.buttons["roundtableHandle"]
        XCTAssertTrue(handle.waitForExistence(timeout: 10))
        let half = sheet.frame.minY
        // Up 120 points: it stays there, neither half nor full.
        drag(handle, by: -120)
        waitUntil("the sheet did not stay where the handle was let go") { abs(sheet.frame.minY - (half - 120)) < 6 }
        Thread.sleep(forTimeInterval: 0.5)
        let middle = sheet.frame.minY
        try save("roundtable-ui-free-height-middle")
        // A tap goes to full, and the next back to the dragged height.
        handle.tap()
        waitUntil("the tap did not make the sheet full height") { sheet.frame.minY < middle - 100 }
        handle.tap()
        waitUntil("the tap did not bring back the dragged height") { abs(sheet.frame.minY - middle) < 2 }
        // Closed and opened again, it opens at the dragged height.
        app.buttons["roundtableClose"].tap()
        XCTAssertTrue(sheet.waitForNonExistence(timeout: 10))
        sheet = try openSheet(app)
        waitUntil("the sheet did not open at the dragged height") { abs(sheet.frame.minY - middle) < 2 }
        // Down in short drags until it stops: its smallest height, still open.
        var top = sheet.frame.minY
        for _ in 0..<20 {
            drag(handle, by: 40)
            Thread.sleep(forTimeInterval: 0.5)
            if abs(sheet.frame.minY - top) < 1 {
                break
            }
            top = sheet.frame.minY
        }
        XCTAssertTrue(sheet.exists, "short drags closed the sheet")
        let person = app.descendants(matching: .any)["roundtableViewPerson"].firstMatch
        let box = app.descendants(matching: .any)["roundtableBox"]
        XCTAssertTrue(person.exists && box.exists)
        XCTAssertLessThanOrEqual(person.frame.maxY, box.frame.minY, "the first station row is under the box")
        XCTAssertGreaterThan(person.frame.minY, sheet.frame.minY, "the first station row is above the sheet")
        print("Roundtable sheet smallest height: \(sheet.frame.height) points, top \(sheet.frame.minY)")
        try save("roundtable-ui-free-height-smallest")
        // Closed and opened again at its smallest height, it still shows that station row.
        let smallest = sheet.frame.minY
        app.buttons["roundtableClose"].tap()
        XCTAssertTrue(sheet.waitForNonExistence(timeout: 10))
        sheet = try openSheet(app)
        waitUntil("the sheet did not open again at its smallest height") { abs(sheet.frame.minY - smallest) < 2 }
        waitUntil("the reopened sheet hides its first station row") {
            person.exists && person.frame.maxY <= box.frame.minY && person.frame.minY > sheet.frame.minY
        }
        // A drag let go well below the smallest height closes the sheet.
        drag(handle, by: 200)
        XCTAssertTrue(sheet.waitForNonExistence(timeout: 10), "a long drag down did not close the sheet")
        XCTAssertEqual(holdLog(app), "none")
    }

    /// What the sheet's top edge would cut on the band (SAVE, a tag, the talker card, the flag)
    /// is hidden whole while the edge runs through it, and is back at the smallest height.
    @MainActor
    func testWhatTheSheetEdgeWouldCutIsHiddenWhole() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom == .pad, "An iPad shows the applet or the panel")
        let app = launch(frequency: true)
        // The card on your own frequency carries its room's tag identifier.
        let card = app.descendants(matching: .any)["roundtableTag.\(Self.ownRoom)"]
        let tag = app.descendants(matching: .any)["roundtableTag.lsb-7225000"]
        let saveButton = app.descendants(matching: .any)["dsr.save"]
        XCTAssertTrue(card.waitForExistence(timeout: 30))
        XCTAssertTrue(tag.waitForExistence(timeout: 10))
        XCTAssertTrue(saveButton.waitForExistence(timeout: 10))
        // Where they stand on the band, before the sheet opens.
        let cardFrame = card.frame
        let tagFrame = tag.frame
        let saveFrame = saveButton.frame
        let flag = app.descendants(matching: .any)["flagA"]
        XCTAssertTrue(flag.exists)
        let flagFrame = flag.frame
        let sheet = try openSheet(app)
        let handle = app.buttons["roundtableHandle"]
        XCTAssertTrue(handle.waitForExistence(timeout: 10))
        // The edge through SAVE: SAVE is hidden.
        moveEdge(of: sheet, handle: handle, to: saveFrame.midY)
        waitUntil("SAVE cut by the sheet's edge is still on the band") { !saveButton.exists }
        try save("roundtable-ui-edge-save")
        // Through the tag row: the tag is hidden.
        moveEdge(of: sheet, handle: handle, to: tagFrame.midY)
        waitUntil("a tag cut by the sheet's edge is still on the band") { !tag.exists }
        try save("roundtable-ui-edge-tags")
        // Through the card's middle: the card is hidden, and the flag stays.
        moveEdge(of: sheet, handle: handle, to: cardFrame.midY)
        waitUntil("the card cut by the sheet's edge is still on the band") { !card.exists }
        XCTAssertTrue(flag.exists)
        try save("roundtable-ui-edge-card")
        // Through the flag: the flag and its round buttons are hidden.
        moveEdge(of: sheet, handle: handle, to: flagFrame.midY)
        waitUntil("the flag cut by the sheet's edge is still on the band") { !flag.exists }
        try save("roundtable-ui-edge-flag")
        // Down in short drags to the smallest height: all three are whole again.
        var top = sheet.frame.minY
        for _ in 0..<20 {
            drag(handle, by: 40)
            Thread.sleep(forTimeInterval: 0.5)
            if abs(sheet.frame.minY - top) < 1 {
                break
            }
            top = sheet.frame.minY
        }
        XCTAssertTrue(sheet.exists, "short drags closed the sheet")
        waitUntil("the card did not come back at the smallest height") { card.exists }
        waitUntil("the tag did not come back at the smallest height") { tag.exists }
        waitUntil("SAVE did not come back at the smallest height") { saveButton.exists }
        waitUntil("the flag did not come back at the smallest height") { flag.exists }
        XCTAssertLessThanOrEqual(card.frame.maxY, sheet.frame.minY, "the card is above the sheet")
        try save("roundtable-ui-edge-smallest")
        XCTAssertEqual(holdLog(app), "none")
    }

    // MARK: Helpers

    /// The sheet's top moved to `y` on the screen by a drag on its handle; a
    /// move too short for the drag to start goes up first and comes back.
    @MainActor
    private func moveEdge(of sheet: XCUIElement, handle: XCUIElement, to y: CGFloat) {
        if abs(y - sheet.frame.minY) < 20 {
            drag(handle, by: -60)
            Thread.sleep(forTimeInterval: 0.5)
        }
        drag(handle, by: y - sheet.frame.minY)
        waitUntil("the sheet's top did not reach \(y)") { abs(sheet.frame.minY - y) < 6 }
        Thread.sleep(forTimeInterval: 0.5)
    }

    /// A slow drag of `dy` points from the middle of `element`, held at its end.
    @MainActor
    private func drag(_ element: XCUIElement, by dy: CGFloat) {
        let start = element.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
        start.press(forDuration: 0.05, thenDragTo: start.withOffset(CGVector(dx: 0, dy: dy)),
                    withVelocity: 300, thenHoldForDuration: 0.1)
    }

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

    /// A drag across open band, clear of the flag, the tags and the sheet: the band moves
    /// under its flag, nothing tunes and the sheet stays up.
    @MainActor
    private func dragTheEmptyBand(_ app: XCUIApplication, under sheet: XCUIElement, shot: String) throws {
        let surface = app.descendants(matching: .any)["bandSurface"]
        let flag = app.descendants(matching: .any)["flagA"]
        XCTAssertTrue(surface.waitForExistence(timeout: 10))
        XCTAssertTrue(flag.waitForExistence(timeout: 10))
        let label = flag.label
        let before = flag.frame.minX
        let band = surface.frame
        let y = (flag.frame.maxY + sheet.frame.minY) / 2
        func at(_ x: CGFloat) -> XCUICoordinate {
            surface.coordinate(withNormalizedOffset: CGVector(dx: x, dy: (y - band.minY) / band.height))
        }
        at(0.75).press(forDuration: 0.05, thenDragTo: at(0.55))
        waitUntil("the drag did not move the band") { abs(flag.frame.minX - before) > 20 }
        XCTAssertEqual(flag.label, label, "a drag on open band tuned the slice")
        XCTAssertTrue(sheet.exists, "the drag closed the sheet")
        XCTAssertEqual(holdLog(app), "none")
        try save(shot)
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
        if UIDevice.current.userInterfaceIdiom == .pad {
            XCUIDevice.shared.orientation = .portrait
        }
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
