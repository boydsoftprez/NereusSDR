// NereusSDR for iOS: the band stack block under a finger: a hold on a band, its picker, its menus, Reorder, and the BAND menu staying open
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
//
// The fixture's Core refuses each verb with its arguments in the reason, so
// what was sent reads back in the block's note, and no note means no command.
final class BandStackUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    override func tearDown() {
        MainActor.assumeIsolated {
            XCUIDevice.shared.orientation = .portrait
        }
    }

    /// The gesture pairing on `PanelButton`: a half-second hold on a band shows
    /// its list and sends nothing, and the finger's lift is not a tap. A tap
    /// after it still presses, and "40m" is the visible way to the same list.
    @MainActor
    func testHoldOnABandOpensItsListAndSendsNothing() throws {
        try skipOnIPad()
        let app = launchOnPanSheet()
        let block = app.descendants(matching: .any)["bandStackBlock"]
        XCTAssertTrue(block.waitForExistence(timeout: 10), "The Pan sheet has no stack block")
        let count = element(app, "bandStackCount")
        XCTAssertTrue(waitUntil { count.exists && count.label == "1 of 2" }, "40m reads \(count.label)")
        save("band-stack-pan-sheet")

        let twenty = app.buttons["bandGrid.20"]
        twenty.press(forDuration: 0.8)
        XCTAssertTrue(waitUntil { count.label == "1 saved" }, "The hold did not show 20m's list: \(count.label)")
        XCTAssertTrue(element(app, "bandStackRow.20m-ft8").waitForExistence(timeout: 5))
        XCTAssertFalse(app.buttons["bandStackPrevious"].isEnabled, "The arrows act while viewing another band")
        XCTAssertFalse(app.buttons["bandStackNext"].isEnabled)
        XCTAssertFalse(app.buttons["bandStackAdd"].isEnabled, "Add acts while viewing another band")
        XCTAssertTrue(element(app, "bandStackReason").exists, "The grey arrows carry no reason")
        settle()
        XCTAssertFalse(element(app, "bandStackNote").exists, "The hold sent a command")
        save("band-stack-viewing-another")

        // A hold's flag does not swallow the next tap, and a short tap is no hold.
        twenty.tap()
        let note = element(app, "bandStackNote")
        XCTAssertTrue(note.waitForExistence(timeout: 10), "The tap after a hold sent nothing")
        XCTAssertTrue(note.label.contains("bandStack.press"), note.label)
        XCTAssertTrue(note.label.contains("band 20m"), note.label)
        XCTAssertTrue(note.label.contains("direction next"), note.label)
        // A press goes back to the slice's list.
        XCTAssertTrue(waitUntil { count.label == "1 of 2" })

        // A hold that slides off the button before lifting leaves the next tap alone.
        let from = twenty.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
        from.press(forDuration: 0.8, thenDragTo: from.withOffset(CGVector(dx: 0, dy: 200)))
        XCTAssertTrue(waitUntil { count.label == "1 saved" }, "The hold and slide did not show 20m's list")
        app.buttons["bandGrid.40"].tap()
        XCTAssertTrue(waitUntil { note.exists && note.label.contains("band 40m") },
                      "The tap after a slid hold was swallowed: \(note.exists ? note.label : "no note")")
        XCTAssertTrue(waitUntil { count.label == "1 of 2" })

        // The visible way to the same list.
        app.buttons["bandStackNoteClose"].tap()
        XCTAssertTrue(note.waitForNonExistence(timeout: 5), "The note did not close")
        app.buttons["bandStackPicker"].tap()
        XCTAssertTrue(app.staticTexts["Show the stack for"].waitForExistence(timeout: 5), "The picker has no heading")
        save("band-stack-picker")
        app.buttons.matching(NSPredicate(format: "label BEGINSWITH %@", "80m")).firstMatch.tap()
        XCTAssertTrue(waitUntil { count.label == "1 saved" }, "The picker did not show 80m's list")
        XCTAssertTrue(element(app, "bandStackRow.80m-low").waitForExistence(timeout: 5))
        settle()
        XCTAssertFalse(note.exists, "The picker sent a command")
    }

    /// Spec section 8: a tap on the lit band is the next entry, the same verb
    /// any band's tap sends, and the sheet stays down to show where it went.
    @MainActor
    func testATapOnTheLitBandEchoesPressAndTheSheetStays() throws {
        try skipOnIPad()
        let app = launchOnPanSheet()
        let lit = app.buttons["bandGrid.40"]
        XCTAssertTrue(lit.waitForExistence(timeout: 10))
        XCTAssertTrue(lit.isSelected, "The slice's own band is not lit")
        lit.tap()
        let note = element(app, "bandStackNote")
        XCTAssertTrue(note.waitForExistence(timeout: 10), "The tap on the lit band sent nothing")
        XCTAssertTrue(note.label.contains("bandStack.press"), note.label)
        XCTAssertTrue(note.label.contains("band 40m"), note.label)
        XCTAssertTrue(note.label.contains("direction next"), note.label)
        XCTAssertTrue(element(app, "panSheet").exists, "The Pan sheet closed on a press with the capability")
        save("band-stack-lit-tap")
    }

    /// Spec section 8: the row's five items and the band's return and clear,
    /// each sending its verb.
    @MainActor
    func testRowAndBandMenus() throws {
        try skipOnIPad()
        let app = launchOnPanSheet()
        let note = element(app, "bandStackNote")
        let rowMenu = app.buttons["bandStackRowMenu.40m-ssb"]
        XCTAssertTrue(rowMenu.waitForExistence(timeout: 10))
        rowMenu.tap()
        for item in ["Save this spot here", "Rename", "Lock", "Always return here", "Delete"] {
            XCTAssertTrue(choice(app, item).waitForExistence(timeout: 5), "The row's menu has no \(item)")
        }
        save("band-stack-row-menu")
        choice(app, "Always return here").tap()
        XCTAssertTrue(waitUntil { note.exists && note.label.contains("bandStack.setReturn") },
                      "Always return here sent nothing")
        XCTAssertTrue(note.label.contains("mode specific") && note.label.contains("entryId 40m-ssb"), note.label)

        // The net is locked: Unlock in place of Lock, and nothing saved over it.
        let lockedMenu = app.buttons["bandStackRowMenu.40m-net"]
        lockedMenu.tap()
        XCTAssertTrue(choice(app, "Unlock").waitForExistence(timeout: 5), "A locked row offers no Unlock")
        XCTAssertFalse(choice(app, "Save this spot here").isEnabled, "A locked row can be saved over")
        choice(app, "Unlock").tap()
        XCTAssertTrue(waitUntil { note.label.contains("bandStack.edit") && note.label.contains("entryId 40m-net") },
                      note.label)

        rowMenu.tap()
        choice(app, "Rename").tap()
        let field = app.textFields["bandStackRenameField"]
        XCTAssertTrue(field.waitForExistence(timeout: 5), "Rename has no field")
        save("band-stack-rename")
        field.typeText("Rag chew\n")
        XCTAssertTrue(field.waitForNonExistence(timeout: 5), "Return did not end Rename")
        XCTAssertTrue(waitUntil { note.label.contains("name Rag chew") }, note.label)

        app.buttons["bandStackBandMenu"].tap()
        for item in ["Current entry", "Last frequency used", "Specific entry", "Clear unlocked 40m entries"] {
            XCTAssertTrue(choice(app, item).waitForExistence(timeout: 5), "The band's menu has no \(item)")
        }
        save("band-stack-band-menu")
        choice(app, "Last frequency used").tap()
        XCTAssertTrue(waitUntil { note.label.contains("mode lastUsed") }, note.label)
        app.buttons["bandStackBandMenu"].tap()
        choice(app, "Clear unlocked 40m entries").tap()
        let clear = app.buttons["Clear"]
        XCTAssertTrue(clear.waitForExistence(timeout: 5), "Clear asks nothing")
        save("band-stack-clear")
        clear.tap()
        XCTAssertTrue(waitUntil { note.label.contains("bandStack.clear") && note.label.contains("band 40m") }, note.label)

        // The arrows and add, on the slice's own band.
        app.buttons["bandStackNext"].tap()
        XCTAssertTrue(waitUntil { note.label.contains("bandStack.step") && note.label.contains("direction next") }, note.label)
        app.buttons["bandStackPrevious"].tap()
        XCTAssertTrue(waitUntil { note.label.contains("direction previous") }, note.label)
        app.buttons["bandStackAdd"].tap()
        XCTAssertTrue(waitUntil { note.label.contains("bandStack.add") }, note.label)
        // A row's tap goes there.
        app.buttons["bandStackRow.40m-ssb"].tap()
        XCTAssertTrue(waitUntil { note.label.contains("bandStack.goTo") && note.label.contains("entryId 40m-ssb") }, note.label)
    }

    /// Grips only in the mode, a drag and the grip's tap each moving a row, and
    /// every id sent on Done.
    @MainActor
    func testReorderIsAModeThatSendsEveryIdOnDone() throws {
        try skipOnIPad()
        let app = launchOnPanSheet()
        let reorder = app.buttons["bandStackReorder"]
        XCTAssertTrue(reorder.waitForExistence(timeout: 10))
        XCTAssertEqual(reorder.label, "Reorder")
        let gripNet = element(app, "bandStackGrip.40m-net")
        let gripSsb = element(app, "bandStackGrip.40m-ssb")
        XCTAssertFalse(gripNet.exists, "A grip shows out of the mode")
        reorder.tap()
        XCTAssertTrue(gripNet.waitForExistence(timeout: 5), "The mode has no grips")
        XCTAssertFalse(app.buttons["bandStackRowMenu.40m-net"].exists, "A row's menu shows in the mode")
        XCTAssertEqual(reorder.label, "Done")
        save("band-stack-reorder")

        // Nothing goes yet: a drag moves the net below the other row.
        gripNet.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
            .press(forDuration: 0.1, thenDragTo: gripSsb.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.8)))
        XCTAssertTrue(waitUntil { gripSsb.frame.minY < gripNet.frame.minY }, "The drag moved nothing")
        settle()
        XCTAssertFalse(element(app, "bandStackNote").exists, "A move sent a command")

        // The way with no drag: the grip's tap, Move up.
        gripNet.tap()
        let up = app.buttons["Move up"]
        XCTAssertTrue(up.waitForExistence(timeout: 5), "The grip's tap offers no Move up")
        save("band-stack-reorder-move")
        up.tap()
        XCTAssertTrue(waitUntil { gripNet.frame.minY < gripSsb.frame.minY }, "Move up moved nothing")
        // And back down, for the order to be read on Done.
        gripNet.tap()
        XCTAssertTrue(app.buttons["Move down"].waitForExistence(timeout: 5))
        app.buttons["Move down"].tap()
        XCTAssertTrue(waitUntil { gripSsb.frame.minY < gripNet.frame.minY }, "Move down moved nothing")

        reorder.tap()
        let note = element(app, "bandStackNote")
        XCTAssertTrue(note.waitForExistence(timeout: 10), "Done sent nothing")
        XCTAssertTrue(note.label.contains("bandStack.reorder"), note.label)
        XCTAssertTrue(note.label.contains("band 40m"), note.label)
        XCTAssertTrue(note.label.contains(#"ids ["40m-ssb","40m-net"]"#), note.label)
        XCTAssertTrue(gripNet.waitForNonExistence(timeout: 5), "The grips stayed after Done")
        XCTAssertEqual(reorder.label, "Reorder")
    }

    /// The BAND menu sideways: two panels beside the column, staying open on a
    /// press, clear of the PTT.
    @MainActor
    func testBandMenuSidewaysStaysOpenOnAPressAndKeepsThePttClear() throws {
        try skipOnIPad()
        let app = launch(stack: true)
        XCUIDevice.shared.orientation = .landscapeLeft
        let band = app.buttons["bandColumn.band"]
        XCTAssertTrue(band.waitForExistence(timeout: 30), "The phone on its side has no band column")
        band.tap()
        let menu = element(app, "bandColumnMenu.band")
        XCTAssertTrue(menu.waitForExistence(timeout: 10))
        let grid = app.buttons["bandGrid.20"]
        let block = element(app, "bandStackBlock")
        XCTAssertTrue(grid.waitForExistence(timeout: 5))
        XCTAssertTrue(block.waitForExistence(timeout: 5))
        XCTAssertLessThan(grid.frame.maxX, block.frame.minX + 1, "The grid and the stack are not side by side")
        // The header fits a panel's width with the band's name whole.
        XCTAssertGreaterThan(app.buttons["bandStackPicker"].frame.width, 44, "The picker is squeezed")
        let ptt = app.buttons["ptt"]
        XCTAssertTrue(waitUntil { !menu.frame.intersects(ptt.frame) }, "The BAND menu covers the PTT")
        save("band-stack-menu-sideways")

        grid.tap()
        let note = element(app, "bandStackNote")
        XCTAssertTrue(note.waitForExistence(timeout: 10), "The press sent nothing")
        XCTAssertTrue(note.label.contains("bandStack.press"), note.label)
        XCTAssertTrue(menu.exists, "The BAND menu closed on a press with the capability")
        save("band-stack-menu-sideways-note")
        // The lit band's tap, the next entry, leaves it open too.
        app.buttons["bandGrid.40"].tap()
        XCTAssertTrue(waitUntil { note.label.contains("band 40m") }, note.label)
        XCTAssertTrue(menu.exists, "The BAND menu closed on the lit band's tap")
        // The block's foot can be reached on a short band.
        let reorder = app.buttons["bandStackReorder"]
        if !reorder.isHittable {
            block.swipeUp()
        }
        XCTAssertTrue(waitUntil { reorder.isHittable }, "The stack's foot cannot be reached sideways")
        save("band-stack-menu-sideways-foot")
    }

    /// The older Core: the grid opens bands as today, the BAND menu closes on a
    /// press, and the block is one greyed row with its reason.
    @MainActor
    func testOlderCore() throws {
        try skipOnIPad()
        let app = launch(stack: false)
        let pan = app.buttons["pan"]
        XCTAssertTrue(pan.waitForExistence(timeout: 30))
        pan.tap()
        let row = app.buttons["bandStackUnavailable"]
        XCTAssertTrue(row.waitForExistence(timeout: 10), "An older Core shows no greyed row")
        XCTAssertEqual(row.label, "Needs a newer Core")
        XCTAssertFalse(row.isEnabled)
        XCTAssertEqual(element(app, "bandStackUnavailableReason").label,
                       "This Core does not have band stacks. Updating the Core may help.")
        save("band-stack-older-core")
        // A hold on a band there is nothing but a long tap: it opens the band.
        app.buttons["bandGrid.20"].press(forDuration: 0.8)
        let panNote = element(app, "panSheetNote")
        XCTAssertTrue(panNote.waitForExistence(timeout: 10), "A long press on an older Core opened nothing")
        XCTAssertTrue(panNote.label.contains("slice.selectBand"), panNote.label)
        app.buttons["Close the sheet"].firstMatch.coordinate(withNormalizedOffset: CGVector(dx: 0.97, dy: 0.9)).tap()
        XCTAssertTrue(element(app, "panSheet").waitForNonExistence(timeout: 10))

        XCUIDevice.shared.orientation = .landscapeLeft
        let band = app.buttons["bandColumn.band"]
        XCTAssertTrue(band.waitForExistence(timeout: 30))
        band.tap()
        let menu = element(app, "bandColumnMenu.band")
        XCTAssertTrue(menu.waitForExistence(timeout: 10))
        save("band-stack-older-core-sideways")
        app.buttons["bandGrid.20"].tap()
        XCTAssertTrue(menu.waitForNonExistence(timeout: 10), "An older Core's BAND menu stayed open on a press")
    }

    // MARK: The iPad's applet

    /// Spec section 8: upright the applet sits under the S-meter in the first
    /// column, its controls send their verbs, and its menus and question show.
    @MainActor
    func testIPadAppletUprightUnderTheSMeter() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The Band Stack applet is the iPad's")
        let app = launch(stack: true)
        let applet = element(app, "bandStackApplet")
        XCTAssertTrue(applet.waitForExistence(timeout: 30), "The iPad upright has no Band Stack applet")
        let panel = element(app, "frontPanel")
        let meter = element(app, "analogSMeter")
        XCTAssertEqual(applet.frame.minX, panel.frame.minX, accuracy: 1, "The applet is not in the first column")
        XCTAssertEqual(applet.frame.width, 278, accuracy: 1, "The applet is not the column's width")
        XCTAssertGreaterThanOrEqual(applet.frame.minY, meter.frame.maxY - 1, "The applet is not under the S-meter")
        XCTAssertEqual(applet.frame.maxY, panel.frame.maxY, accuracy: 1, "The applet does not reach the panel's foot")
        XCTAssertTrue(applet.staticTexts["Band Stack"].exists)
        let count = element(app, "bandStackCount")
        XCTAssertTrue(waitUntil { count.exists && count.label == "1 of 2" }, "40m reads \(count.label)")
        XCTAssertTrue(applet.buttons["bandStackAppletSlice.A"].exists, "The title has no slice letters")
        save("band-stack-ipad-upright")

        let note = element(app, "bandStackNote")
        applet.buttons["bandStackRow.40m-ssb"].tap()
        XCTAssertTrue(waitUntil { note.exists && note.label.contains("bandStack.goTo") }, "A row's tap sent nothing")
        applet.buttons["bandStackNoteClose"].tap()
        XCTAssertTrue(note.waitForNonExistence(timeout: 5))

        applet.buttons["bandStackRowMenu.40m-ssb"].tap()
        for item in ["Save this spot here", "Rename", "Lock", "Always return here", "Delete"] {
            XCTAssertTrue(choice(app, item).waitForExistence(timeout: 5), "The row's menu has no \(item)")
        }
        save("band-stack-ipad-row-menu")
        choice(app, "Rename").tap()
        let field = app.textFields["bandStackRenameField"]
        XCTAssertTrue(field.waitForExistence(timeout: 5), "Rename has no field")
        XCTAssertTrue(waitUntil { field.isHittable }, "The name field is behind something")
        save("band-stack-ipad-rename")
        field.typeText("Rag chew\n")
        XCTAssertTrue(field.waitForNonExistence(timeout: 5), "Return did not end Rename")
        XCTAssertTrue(waitUntil { note.exists && note.label.contains("name Rag chew") }, "The name was not sent")
        applet.buttons["bandStackNoteClose"].tap()
        XCTAssertTrue(note.waitForNonExistence(timeout: 5))

        // Clear's question: on an iPad a popover with no Cancel, and a tap outside it says no.
        applet.buttons["bandStackBandMenu"].tap()
        XCTAssertTrue(choice(app, "Current entry").waitForExistence(timeout: 5))
        save("band-stack-ipad-band-menu")
        choice(app, "Clear unlocked 40m entries").tap()
        let clear = app.buttons["Clear"]
        XCTAssertTrue(clear.waitForExistence(timeout: 5), "Clear unlocked asks nothing")
        save("band-stack-ipad-clear")
        element(app, "analogSMeter").tap()
        XCTAssertTrue(clear.waitForNonExistence(timeout: 5), "A tap outside did not close the question")
        settle()
        XCTAssertFalse(note.exists, "Saying no sent a command")
        applet.buttons["bandStackBandMenu"].tap()
        choice(app, "Clear unlocked 40m entries").tap()
        XCTAssertTrue(clear.waitForExistence(timeout: 5))
        clear.tap()
        XCTAssertTrue(waitUntil { note.exists && note.label.contains("bandStack.clear") }, "Clear sent nothing")
    }

    /// The applet and the BAND menu are two blocks on one model: a name is
    /// typed in one field, and closing the menu leaves what the applet started.
    @MainActor
    func testIPadAppletBesideTheBandMenu() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The Band Stack applet is the iPad's")
        let app = launch(stack: true)
        let applet = element(app, "bandStackApplet")
        XCTAssertTrue(applet.waitForExistence(timeout: 30))
        let band = app.buttons["bandColumn.band"]
        XCTAssertTrue(band.waitForExistence(timeout: 10))
        band.tap()
        let menu = element(app, "bandColumnMenu.band")
        XCTAssertTrue(menu.waitForExistence(timeout: 10))
        save("band-stack-ipad-band-column-menu")

        menu.buttons["bandStackRowMenu.40m-ssb"].tap()
        choice(app, "Rename").tap()
        let fields = app.textFields.matching(identifier: "bandStackRenameField")
        XCTAssertTrue(fields.firstMatch.waitForExistence(timeout: 5), "Rename has no field")
        settle()
        XCTAssertEqual(fields.count, 1, "Two blocks each drew the name field")
        XCTAssertTrue(menu.textFields["bandStackRenameField"].exists, "The field is not where Rename was asked for")
        fields.firstMatch.typeText("Net\n")
        XCTAssertTrue(fields.firstMatch.waitForNonExistence(timeout: 5), "Return did not end Rename")
        let note = menu.descendants(matching: .any)["bandStackNote"]
        XCTAssertTrue(waitUntil { note.exists && note.label.contains("name Net") }, "The name was not sent")

        // Reorder from the applet outlives the menu.
        let reorder = applet.buttons["bandStackReorder"]
        reorder.tap()
        XCTAssertTrue(waitUntil { reorder.label == "Done" }, "Reorder did not start")
        XCTAssertTrue(applet.descendants(matching: .any)["bandStackGrip.40m-ssb"].waitForExistence(timeout: 5))
        band.tap()
        XCTAssertTrue(menu.waitForNonExistence(timeout: 10), "The BAND menu did not close")
        XCTAssertEqual(reorder.label, "Done", "Closing the BAND menu ended the applet's Reorder")

        // On its side it is in the column, the mode still on.
        XCUIDevice.shared.orientation = .landscapeLeft
        let column = element(app, "appletColumn")
        XCTAssertTrue(column.waitForExistence(timeout: 20))
        let sideways = column.descendants(matching: .any)["bandStackApplet"]
        XCTAssertTrue(sideways.waitForExistence(timeout: 10), "The column has no Band Stack applet")
        XCTAssertEqual(sideways.frame.width, 320, accuracy: 1)
        XCTAssertGreaterThanOrEqual(sideways.frame.minY, element(app, "analogSMeter").frame.maxY - 1)
        XCTAssertLessThan(sideways.frame.minY, element(app, "rxPanelDrawer").frame.minY)
        XCTAssertEqual(sideways.buttons["bandStackReorder"].label, "Done")
        save("band-stack-ipad-column-reorder")
    }

    /// An older Core: the applet stays, greyed, with the reason, either way up.
    @MainActor
    func testIPadAppletOnAnOlderCore() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The Band Stack applet is the iPad's")
        let app = launch(stack: false)
        let applet = element(app, "bandStackApplet")
        XCTAssertTrue(applet.waitForExistence(timeout: 30), "An older Core has no Band Stack applet")
        let row = applet.buttons["bandStackUnavailable"]
        XCTAssertTrue(row.waitForExistence(timeout: 10), "An older Core's applet has no greyed row")
        XCTAssertEqual(row.label, "Needs a newer Core")
        XCTAssertFalse(row.isEnabled)
        XCTAssertEqual(applet.descendants(matching: .any)["bandStackUnavailableReason"].label,
                       "This Core does not have band stacks. Updating the Core may help.")
        save("band-stack-ipad-older-core")

        XCUIDevice.shared.orientation = .landscapeLeft
        let column = element(app, "appletColumn")
        XCTAssertTrue(column.waitForExistence(timeout: 20))
        let sideways = column.buttons["bandStackUnavailable"]
        XCTAssertTrue(sideways.waitForExistence(timeout: 10), "The column drops the applet on an older Core")
        XCTAssertFalse(sideways.isEnabled)
        save("band-stack-ipad-older-core-column")
    }

    // MARK: Markers on the spectrum

    /// The shared sample with the two 40 m entries moved into the fixture's
    /// 48 kHz view around 7.2445 MHz.
    private static let markersDocument = "phone-markers-document"

    /// V18, in the older Display sheet: off until turned on, a chip per entry
    /// in view with the active and the locked one marked, a tap sends goTo,
    /// and the switch survives a relaunch.
    @MainActor
    func testSheetSwitchShowsMarkersAndATapGoesThere() throws {
        try skipOnIPad()
        let app = launch(stack: true, document: Self.markersDocument)
        let net = element(app, "bandStackMarker.40m-net")
        setMarkers(app, on: true, id: "bandStackMarkersSwitch")
        XCTAssertTrue(net.waitForExistence(timeout: 10), "The switch drew no chip for 7.230")
        XCTAssertTrue(net.label.contains("7.230") && net.label.contains("locked") && net.label.contains("active"),
                      net.label)
        XCTAssertFalse(element(app, "bandStackMarker.20m-ft8").exists, "A chip for 20 m on the 40 m band")
        save("band-stack-markers")

        XCTAssertTrue(net.isHittable, "The chip cannot be tapped")
        net.tap()
        let note = element(app, "bandStackMarkerNote")
        XCTAssertTrue(note.waitForExistence(timeout: 10), "The tap sent nothing")
        XCTAssertTrue(note.label.contains("bandStack.goTo") && note.label.contains("entryId 40m-net"), note.label)
        save("band-stack-markers-note")
        app.buttons["bandStackMarkerNoteClose"].tap()
        XCTAssertTrue(note.waitForNonExistence(timeout: 5), "The note did not close")

        app.terminate()
        app.launchFresh()
        XCTAssertTrue(net.waitForExistence(timeout: 30), "The switch did not survive a relaunch")
        setMarkers(app, on: false, id: "bandStackMarkersSwitch")
        XCTAssertTrue(net.waitForNonExistence(timeout: 10), "Off left the chips")
    }

    /// V18, in the pill's "On this phone".
    @MainActor
    func testPillSwitchShowsMarkers() throws {
        try skipOnIPad()
        let app = launch(stack: true, pill: true, document: Self.markersDocument)
        setMarkers(app, on: true, id: "displayPillBandStack", pillTab: "view")
        let ssb = element(app, "bandStackMarker.40m-ssb")
        XCTAssertTrue(ssb.waitForExistence(timeout: 10), "The pill's switch drew no chip")
        setMarkers(app, on: false, id: "displayPillBandStack", pillTab: "view")
        XCTAssertTrue(ssb.waitForNonExistence(timeout: 10), "Off left the chips")
    }

    /// Spec section 8: hidden on air, and back when the radio stops.
    @MainActor
    func testMarkersHideOnAir() throws {
        try skipOnIPad()
        let app = launch(stack: true, keyed: true, document: Self.markersDocument)
        setMarkers(app, on: true, id: "bandStackMarkersSwitch")
        let net = element(app, "bandStackMarker.40m-net")
        XCTAssertTrue(net.waitForExistence(timeout: 10), "The switch drew no chip")
        let ptt = app.buttons["ptt"]
        XCTAssertTrue(waitUntil { ptt.isEnabled }, "PTT is not usable: the fixture's link is not up")
        ptt.tap()
        XCTAssertTrue(net.waitForNonExistence(timeout: 10), "The chips stayed on air")
        ptt.tap()
        XCTAssertTrue(net.waitForExistence(timeout: 10), "The chips did not come back after the air")
        setMarkers(app, on: false, id: "bandStackMarkersSwitch")
    }

    /// The iPad draws the phone's chips and the older sheet's switch: either
    /// way up, off until turned on, and a tap on a chip sends goTo.
    @MainActor
    func testIPadSwitchShowsMarkersAndATapGoesThere() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "This is the iPad's column")
        let app = launch(stack: true, document: Self.markersDocument)
        let net = element(app, "bandStackMarker.40m-net")
        XCTAssertTrue(app.buttons["bandColumn.display"].waitForExistence(timeout: 30), "The iPad has no band column")
        XCTAssertFalse(net.exists, "A chip draws with the switch off")
        setMarkers(app, on: true, id: "bandStackMarkersSwitch", opener: "bandColumn.display",
                   container: "bandColumnMenu.display")
        XCTAssertTrue(net.waitForExistence(timeout: 10), "The switch drew no chip for 7.230")
        XCTAssertTrue(net.isHittable, "The chip cannot be tapped")
        save("band-stack-ipad-markers")
        net.tap()
        let note = element(app, "bandStackMarkerNote")
        XCTAssertTrue(note.waitForExistence(timeout: 10), "The tap sent nothing")
        XCTAssertTrue(note.label.contains("bandStack.goTo") && note.label.contains("entryId 40m-net"), note.label)
        XCTAssertGreaterThanOrEqual(note.frame.minX, element(app, "bandColumn").frame.maxX, "The column covers the note")
        save("band-stack-ipad-markers-note")
        app.buttons["bandStackMarkerNoteClose"].tap()
        XCTAssertTrue(note.waitForNonExistence(timeout: 5), "The note did not close")

        XCUIDevice.shared.orientation = .landscapeLeft
        XCTAssertTrue(element(app, "appletColumn").waitForExistence(timeout: 20))
        XCTAssertTrue(waitUntil { net.exists && net.isHittable }, "On its side the chip is gone or covered")
        save("band-stack-ipad-markers-sideways")
        setMarkers(app, on: false, id: "bandStackMarkersSwitch", opener: "bandColumn.display",
                   container: "bandColumnMenu.display")
        XCTAssertTrue(net.waitForNonExistence(timeout: 10), "Off left the chips")
    }

    /// An older Core: the switch stays, greyed with the reason, in the sheet
    /// and in the pill, and no chip draws.
    @MainActor
    func testOlderCoreGreysTheMarkersSwitch() throws {
        try skipOnIPad()
        let reason = "This Core does not have band stacks. Updating the Core may help."
        let app = launch(stack: false)
        app.buttons["display"].tap()
        let sheet = app.buttons["bandStackMarkersSwitch"]
        reach(sheet, scrolling: element(app, "displaySheet"))
        XCTAssertFalse(sheet.isEnabled, "An older Core's switch can be pressed")
        XCTAssertEqual(element(app, "bandStackMarkersNote").label, "Band stack: \(reason)")
        save("band-stack-markers-older-sheet")

        let pill = launch(stack: false, pill: true)
        pill.buttons["display"].tap()
        let tab = element(pill, "displayPillTab.view")
        XCTAssertTrue(tab.waitForExistence(timeout: 10))
        tab.tap()
        let chip = pill.buttons["displayPillBandStack"]
        reach(chip, scrolling: element(pill, "displayPill"))
        XCTAssertFalse(chip.isEnabled, "An older Core's switch can be pressed")
        XCTAssertTrue(pill.staticTexts[reason].exists, "The grey switch shows no reason")
        save("band-stack-markers-older-pill")
    }

    // MARK: Setup

    /// Spec section 8: the Pan sheet's link opens Setup's page, whose rows
    /// show the Core's options and send its verbs, and each question asks
    /// with both its buttons.
    @MainActor
    func testSetupPageFromThePanSheet() throws {
        try skipOnIPad()
        let app = launchOnPanSheet()
        let link = app.buttons["bandStackSetupLink"]
        for _ in 0..<6 where !(link.exists && link.isHittable) {
            element(app, "panSheet").swipeUp()
        }
        XCTAssertEqual(link.label, "Band stack settings in Setup \u{203A}")
        save("band-stack-setup-link")
        link.tap()
        let off = app.buttons["bandStackSetup.autoSave.0"]
        XCTAssertTrue(off.waitForExistence(timeout: 10), "The link did not open the Band Stack page")
        XCTAssertTrue(element(app, "panSheet").waitForNonExistence(timeout: 5), "The link left the sheet open")
        XCTAssertTrue(app.otherElements["Sections"].buttons["Setup"].isSelected, "The link did not go to the Setup tab")
        XCTAssertTrue(app.navigationBars.staticTexts["Band Stack"].exists || app.staticTexts["Band Stack"].exists)
        XCTAssertTrue(app.buttons["bandStackSetup.autoSave.30"].isSelected, "Auto-save does not show the Core's 30 seconds")
        XCTAssertFalse(off.isSelected)
        save("band-stack-setup")

        let close = app.buttons["bandStackSetup.noteClose"]
        app.buttons["bandStackSetup.autoSave.60"].tap()
        XCTAssertTrue(close.waitForExistence(timeout: 10), "Auto-save sent nothing")
        XCTAssertTrue(close.label.contains("bandStack.options") && close.label.contains("dwellSeconds 60"), close.label)
        close.tap()
        XCTAssertTrue(close.waitForNonExistence(timeout: 5))

        let expiry = app.buttons["bandStackSetup.autoExpiry.15"]
        XCTAssertTrue(expiry.isSelected, "Auto-expiry does not show the Core's 15 minutes")
        app.buttons["bandStackSetup.autoExpiry.5"].tap()
        XCTAssertTrue(close.waitForExistence(timeout: 10), "Auto-expiry sent nothing")
        XCTAssertTrue(close.label.contains("expiryMinutes 5"), close.label)
        close.tap()
        XCTAssertTrue(close.waitForNonExistence(timeout: 5))

        let reset = app.buttons["bandStackSetup.reset"]
        for _ in 0..<4 where !reset.isHittable {
            app.swipeUp()
        }
        XCTAssertTrue(reset.label.contains("United States. Locked entries you made stay."), reset.label)
        save("band-stack-setup-foot")
        let duplicates = element(app, "bandStackSetup.duplicates")
        XCTAssertTrue(duplicates.exists, "The page has no duplicate protection switch")
        // On the switch itself, at the row's right.
        duplicates.coordinate(withNormalizedOffset: CGVector(dx: 0.9, dy: 0.5)).tap()
        XCTAssertTrue(close.waitForExistence(timeout: 10), "The switch sent nothing")
        XCTAssertTrue(close.label.contains("dupeProtection"), close.label)
        close.tap()
        XCTAssertTrue(close.waitForNonExistence(timeout: 5))
        for _ in 0..<4 where !reset.isHittable {
            app.swipeUp()
        }

        // Reset's question, with Cancel, which sends nothing.
        reset.tap()
        let question = app.alerts.firstMatch
        XCTAssertTrue(question.waitForExistence(timeout: 5), "Reset asks nothing")
        XCTAssertTrue(question.staticTexts["Replace the unlocked entries with the defaults for United States? Your locked entries stay."].exists,
                      "Reset asks in other words")
        XCTAssertTrue(question.buttons["Cancel"].exists && question.buttons["Reset"].exists, "Reset's buttons are not both there")
        save("band-stack-setup-reset")
        question.buttons["Cancel"].firstMatch.tap()
        XCTAssertTrue(question.waitForNonExistence(timeout: 5))
        settle()
        XCTAssertFalse(close.exists, "Cancel sent a command")
        reset.tap()
        XCTAssertTrue(question.waitForExistence(timeout: 5))
        question.buttons["Reset"].firstMatch.tap()
        XCTAssertTrue(close.waitForExistence(timeout: 10), "Reset sent nothing")
        XCTAssertTrue(close.label.contains("bandStack.reseed"), close.label)
        close.tap()
        XCTAssertTrue(close.waitForNonExistence(timeout: 5))
        for _ in 0..<4 where !app.buttons["bandStackSetup.clear"].isHittable {
            app.swipeUp()
        }

        // Clear's question, for every band: no band goes with it.
        app.buttons["bandStackSetup.clear"].tap()
        XCTAssertTrue(question.waitForExistence(timeout: 5), "Clear asks nothing")
        XCTAssertTrue(question.staticTexts["Clear every unlocked entry on every band? Locked entries stay."].exists)
        XCTAssertTrue(question.buttons["Cancel"].exists && question.buttons["Clear"].exists, "Clear's buttons are not both there")
        save("band-stack-setup-clear")
        question.buttons["Clear"].firstMatch.tap()
        XCTAssertTrue(close.waitForExistence(timeout: 10), "Clear sent nothing")
        XCTAssertTrue(close.label.contains("bandStack.clear") && !close.label.contains("band "), close.label)
    }

    /// Spec section 8: an older Core lists the page and every row stays, greyed, with the reason.
    @MainActor
    func testOlderCoreGreysTheSetupPage() throws {
        try skipOnIPad()
        let app = launch(stack: false)
        openBandStackPage(app)
        let reason = element(app, "bandStackSetup.reason")
        XCTAssertTrue(reason.waitForExistence(timeout: 10), "The older Core's page has no reason")
        XCTAssertEqual(reason.label, "This Core does not have band stacks. Updating the Core may help.")
        for id in ["bandStackSetup.autoSave.0", "bandStackSetup.autoSave.30", "bandStackSetup.autoExpiry.15"] {
            XCTAssertTrue(app.buttons[id].exists, "\(id) is gone on an older Core")
            XCTAssertFalse(app.buttons[id].isEnabled, "\(id) can be pressed on an older Core")
            XCTAssertFalse(app.buttons[id].isSelected, "\(id) shows a choice an older Core does not have")
        }
        save("band-stack-setup-older")
        for id in ["bandStackSetup.reset", "bandStackSetup.clear"] {
            XCTAssertTrue(app.buttons[id].exists, "\(id) is gone on an older Core")
            XCTAssertFalse(app.buttons[id].isEnabled, "\(id) can be pressed on an older Core")
        }
    }

    /// Spec V1: this device's region edit asks with the desktop's words and
    /// both buttons, Keep sends nothing, Replace sends `reseed`, a tap outside
    /// keeps, and a refused edit asks nothing.
    @MainActor
    func testRegionEditAsks() throws {
        try skipOnIPad()
        let app = launch(stack: true, general: true)
        let tabs = app.otherElements["Sections"]
        XCTAssertTrue(tabs.waitForExistence(timeout: 30))
        tabs.buttons["Setup"].tap()
        let general = app.buttons["General"]
        XCTAssertTrue(general.waitForExistence(timeout: 10))
        general.tap()
        let options = app.buttons["Options"]
        XCTAssertTrue(options.waitForExistence(timeout: 10), "General has no Options page")
        options.tap()

        let keep = app.buttons["bandStackRegionKeep"]
        let replace = app.buttons["bandStackRegionReplace"]
        chooseRegion(app, "Europe")
        XCTAssertTrue(keep.waitForExistence(timeout: 10), "The region edit asked nothing")
        XCTAssertTrue(replace.exists, "The question has no Replace")
        XCTAssertTrue(keep.isHittable && replace.isHittable, "A button of the question cannot be reached")
        XCTAssertEqual(keep.label, "Keep my entries")
        XCTAssertEqual(replace.label, "Replace")
        XCTAssertEqual(element(app, "bandStackRegionText").label,
                       "You changed the region to Europe. Replace the unlocked entries with the defaults for Europe? Your locked entries stay.")
        save("band-stack-region-prompt")
        keep.tap()
        XCTAssertTrue(keep.waitForNonExistence(timeout: 5), "Keep did not close the question")
        settle()
        XCTAssertFalse(element(app, "confirmationRefused").exists, "Keep sent a command")

        // Replace: the fixture refuses it with what was sent, and the question stays with those words.
        chooseRegion(app, "Italy")
        XCTAssertTrue(replace.waitForExistence(timeout: 10), "The second region edit asked nothing")
        let secondText = element(app, "bandStackRegionText")
        let secondSeen = waitUntil { secondText.label.contains("region to Italy") }
        save("band-stack-region-second")
        XCTAssertTrue(secondSeen, "The second question reads: \(secondText.label)")
        replace.tap()
        let refused = element(app, "confirmationRefused")
        XCTAssertTrue(refused.waitForExistence(timeout: 10), "Replace sent nothing")
        XCTAssertTrue(refused.label.contains("bandStack.reseed"), refused.label)
        save("band-stack-region-refused")
        keep.tap()
        XCTAssertTrue(keep.waitForNonExistence(timeout: 5))

        // A tap outside keeps.
        chooseRegion(app, "India")
        XCTAssertTrue(keep.waitForExistence(timeout: 10), "The third region edit asked nothing")
        app.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.15)).tap()
        XCTAssertTrue(keep.waitForNonExistence(timeout: 5), "A tap outside did not close the question")

        // A refused edit: its words under the row, and no question.
        chooseRegion(app, "Japan")
        XCTAssertTrue(app.staticTexts["UI fixture: this region is refused"].waitForExistence(timeout: 10),
                      "The refused edit shows no reason")
        settle()
        XCTAssertFalse(keep.exists, "A refused edit asked")
    }

    /// The mockup's title bar menu leads to the same page.
    @MainActor
    func testIPadAppletMenuOpensTheSetupPage() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The Band Stack applet is the iPad's")
        let app = launch(stack: true)
        let menu = app.buttons["bandStackAppletMenu"]
        XCTAssertTrue(menu.waitForExistence(timeout: 30), "The applet's title bar has no menu")
        XCTAssertEqual(menu.label, "Band stack settings in Setup")
        menu.tap()
        XCTAssertTrue(app.buttons["bandStackSetup.autoSave.0"].waitForExistence(timeout: 10),
                      "The applet's menu did not open the Band Stack page")
        save("band-stack-setup-ipad")
    }

    /// Setup, General, Band Stack, from the tab bar.
    @MainActor
    private func openBandStackPage(_ app: XCUIApplication) {
        let tabs = app.otherElements["Sections"]
        XCTAssertTrue(tabs.waitForExistence(timeout: 30))
        tabs.buttons["Setup"].tap()
        let general = app.buttons["General"]
        XCTAssertTrue(general.waitForExistence(timeout: 10))
        general.tap()
        let page = app.buttons["Band Stack"]
        XCTAssertTrue(page.waitForExistence(timeout: 10), "General has no Band Stack page")
        page.tap()
    }

    /// The Options page's region menu, set to `region`.
    @MainActor
    private func chooseRegion(_ app: XCUIApplication, _ region: String) {
        let menu = app.buttons["general.options.region"]
        XCTAssertTrue(menu.waitForExistence(timeout: 10), "Options has no region menu")
        XCTAssertTrue(waitUntil { menu.isEnabled && menu.isHittable }, "The region menu cannot be used")
        // The list is long and opens scrolled to the region set, so an item
        // can start off its ends. A tap as the page settles can open nothing.
        let item = app.buttons[region]
        let list = app.collectionViews.firstMatch
        if !list.exists {
            menu.tap()
        }
        XCTAssertTrue(waitUntil { list.exists || item.exists }, "The region menu did not open")
        save("band-stack-region-menu")
        for swipe in 0..<6 where !(item.exists && item.isHittable) {
            if swipe < 3 {
                list.swipeDown()
            } else {
                list.swipeUp()
            }
        }
        XCTAssertTrue(item.exists, "The region menu has no \(region)")
        item.tap()
    }

    // MARK: Helpers

    /// The sheet draws only the rows on show, so the switch is there once scrolled to.
    @MainActor
    private func reach(_ button: XCUIElement, scrolling sheet: XCUIElement) {
        XCTAssertTrue(sheet.waitForExistence(timeout: 10), "Display did not open")
        for _ in 0..<6 where !(button.exists && button.isHittable) {
            if sheet.identifier == "displayPill" {
                sheet.swipeUp()
            } else {
                // Along the older sheet's left edge, clear of its sliders, which would take the drag.
                sheet.coordinate(withNormalizedOffset: CGVector(dx: 0.02, dy: 0.85))
                    .press(forDuration: 0.05, thenDragTo: sheet.coordinate(withNormalizedOffset: CGVector(dx: 0.02, dy: 0.15)))
            }
        }
        let reached = button.waitForExistence(timeout: 5)
        if !reached {
            save("band-stack-switch-not-reached")
        }
        XCTAssertTrue(reached, "Display has no Band stack switch")
    }

    /// Opens Display, sets the switch and closes Display.
    @MainActor
    private func setMarkers(_ app: XCUIApplication, on: Bool, id: String, pillTab: String? = nil,
                            opener: String = "display", container: String? = nil) {
        let display = app.buttons[opener]
        XCTAssertTrue(display.waitForExistence(timeout: 30), "The toolbar has no Display button")
        display.tap()
        if let pillTab {
            let tab = element(app, "displayPillTab.\(pillTab)")
            XCTAssertTrue(tab.waitForExistence(timeout: 10), "The pill has no \(pillTab) tab")
            tab.tap()
        }
        let button = app.buttons[id]
        reach(button, scrolling: element(app, container ?? (pillTab == nil ? "displaySheet" : "displayPill")))
        XCTAssertTrue(waitUntil { button.isEnabled }, "The Band stack switch stays grey")
        XCTAssertTrue(waitUntil { button.isHittable }, "The Band stack switch cannot be reached")
        if (button.value as? String == "On") != on {
            button.tap()
        }
        XCTAssertTrue(waitUntil { (button.value as? String == "On") == on }, "The switch reads \(String(describing: button.value))")
        save(on ? "band-stack-markers-switch-on" : "band-stack-markers-switch-off")
        display.tap()
        XCTAssertTrue(button.waitForNonExistence(timeout: 10), "Display did not close")
    }

    /// These look a block's controls up by name, and an iPad draws a second
    /// block in its applet. The iPad's own tests above name which block.
    @MainActor
    private func skipOnIPad() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom == .pad, "These are the phone's one block")
    }

    @MainActor
    private func launchOnPanSheet() -> XCUIApplication {
        let app = launch(stack: true)
        let pan = app.buttons["pan"]
        XCTAssertTrue(pan.waitForExistence(timeout: 30))
        pan.tap()
        XCTAssertTrue(element(app, "panSheet").waitForExistence(timeout: 10))
        return app
    }

    @MainActor
    private func launch(stack: Bool, pill: Bool = false, keyed: Bool = false, general: Bool = false,
                        document: String = "facade-document") -> XCUIApplication {
        XCUIDevice.shared.orientation = .portrait
        let app = XCUIApplication()
        // The band with slice A on 40 m and a Core, with none (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", "-NereusBandMenuFixture",
                               stack ? "-NereusBandStackFixture" : "-NereusBandSelectFixture"]
            + (keyed ? ["-NereusKeyedFixture"] : [])
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launchEnvironment["NEREUS_UITEST_BANDSTACK"] = root
            .appendingPathComponent("tests/data/bandstack/\(document).json").path
        if pill {
            // The Core's Display description, so Display opens the pill.
            app.launchEnvironment["NEREUS_UITEST_DISPLAY"] = root.appendingPathComponent("resources/setup/display.json").path
        }
        if general {
            // The Core's General description, for the Options page's region menu.
            app.launchEnvironment["NEREUS_UITEST_GENERAL"] = root.appendingPathComponent("resources/setup/general.json").path
        }
        app.launchFresh()
        return app
    }

    @MainActor
    private func element(_ app: XCUIApplication, _ id: String) -> XCUIElement {
        app.descendants(matching: .any)[id]
    }

    /// A menu item whose label starts with `title` (a reason may follow it).
    @MainActor
    private func choice(_ app: XCUIApplication, _ title: String) -> XCUIElement {
        app.buttons.matching(NSPredicate(format: "label BEGINSWITH %@", title)).firstMatch
    }

    @MainActor
    private func waitUntil(_ condition: () -> Bool) -> Bool {
        let deadline = Date().addingTimeInterval(15)
        while Date() < deadline {
            if condition() {
                return true
            }
            RunLoop.current.run(until: Date().addingTimeInterval(0.1))
        }
        return condition()
    }

    /// Time for a command, had one been sent, to come back as a note.
    @MainActor
    private func settle() {
        RunLoop.current.run(until: Date().addingTimeInterval(1))
    }

    @MainActor
    private func save(_ name: String) {
        let environment = ProcessInfo.processInfo.environment
        let own = UIDevice.current.userInterfaceIdiom == .pad ? "NEREUS_IPAD_SHOTS" : "NEREUS_MAIN_SHOTS"
        guard let directory = [environment[own], environment["NEREUS_CONNECT_SHOTS"]]
            .compactMap({ $0 }).first(where: { !$0.isEmpty }) else {
            return
        }
        let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
        try? XCUIScreen.main.screenshot().pngRepresentation.write(to: url)
        print("Wrote \(url.path)")
    }
}
