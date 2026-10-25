// NereusSDR for iOS: the iPad upright shows its applets below the band; on its side the corner button hides and shows the column; the band column's menus keep clear of the PTT
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class IPadUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    override func tearDown() {
        XCUIDevice.shared.orientation = .portrait
    }

    /// Spec section 5.6, D30, D31: upright the S-meter, RX and TX sit below
    /// the band with no column button; turned on its side they go to the
    /// column on the right, whose corner button hides it and brings it back.
    @MainActor
    func testUprightFrontPanelAndOnItsSideTheColumn() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The iPad's layouts are tried on an iPad")
        XCUIDevice.shared.orientation = .portrait
        let app = XCUIApplication()
        // The band and its tabs, with no Core (a debug build's launch argument).
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let frontPanel = app.descendants(matching: .any)["frontPanel"]
        XCTAssertTrue(frontPanel.waitForExistence(timeout: 30))
        XCTAssertTrue(app.descendants(matching: .any)["analogSMeter"].exists)
        XCTAssertTrue(app.descendants(matching: .any)["rxPanelDrawer"].exists)
        XCTAssertTrue(app.descendants(matching: .any)["txPanelDrawer"].exists)
        XCTAssertTrue(app.otherElements["toolbarIPad"].exists)
        XCTAssertFalse(app.buttons["appletColumnButton"].exists)
        XCTAssertFalse(app.buttons["rxPanel"].exists)
        XCTAssertFalse(app.buttons["txPanel"].exists)
        // The band sits over the applets.
        let band = app.buttons["ptt"]
        XCTAssertTrue(band.exists)
        XCTAssertLessThan(band.frame.maxY, frontPanel.frame.minY)

        XCUIDevice.shared.orientation = .landscapeLeft
        let column = app.descendants(matching: .any)["appletColumn"]
        XCTAssertTrue(column.waitForExistence(timeout: 20))
        XCTAssertFalse(frontPanel.exists)
        let button = app.buttons["appletColumnButton"]
        XCTAssertTrue(button.exists)
        XCTAssertEqual(button.label, "Applet column")
        XCTAssertEqual(button.value as? String, "Shown")
        // The column is on the band's right, under the corner button.
        XCTAssertGreaterThan(column.frame.minX, app.windows.firstMatch.frame.midX)
        button.tap()
        XCTAssertTrue(column.waitForNonExistence(timeout: 10))
        XCTAssertEqual(button.value as? String, "Hidden")
        button.tap()
        XCTAssertTrue(column.waitForExistence(timeout: 10))
        XCTAssertEqual(button.value as? String, "Shown")
    }

    /// D86, D78: the ☰ on the S-meter's title bar opens its menu, RX Mode,
    /// TX Mode, Peak Hold and Meter Face; with no Core, Max Bin is disabled
    /// with its reason; a press and hold on the meter opens the same menu,
    /// and a face chosen there takes.
    @MainActor
    func testMeterMenuFromItsButtonAndPressAndHold() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The analog S-meter is the iPad's")
        XCUIDevice.shared.orientation = .portrait
        let app = XCUIApplication()
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let meter = app.descendants(matching: .any)["analogSMeter"]
        XCTAssertTrue(meter.waitForExistence(timeout: 30))
        let menuButton = app.buttons["sMeterMenu"]
        XCTAssertTrue(menuButton.exists)
        XCTAssertEqual(menuButton.label, "Meter menu")

        menuButton.tap()
        for part in ["RX Mode", "TX Mode", "Peak Hold", "Meter Face"] {
            XCTAssertTrue(app.buttons[part].waitForExistence(timeout: 5), part)
        }
        shoot(app, "ipad-meter-menu")
        app.buttons["RX Mode"].tap()
        let maxBin = choice(app, "Max Bin")
        XCTAssertTrue(maxBin.waitForExistence(timeout: 5))
        XCTAssertFalse(maxBin.isEnabled)
        XCTAssertTrue(choice(app, "Sig Avg").isEnabled)
        shoot(app, "ipad-meter-menu-rx-mode")
        choice(app, "Signal Peak").tap()
        XCTAssertTrue(app.buttons["RX Mode"].waitForNonExistence(timeout: 5))

        // Press and hold on the meter: the same menu.
        meter.press(forDuration: 1.2)
        XCTAssertTrue(app.buttons["Meter Face"].waitForExistence(timeout: 5))
        XCTAssertTrue(app.buttons["TX Mode"].exists)
        shoot(app, "ipad-meter-press-and-hold")
        app.buttons["Meter Face"].tap()
        let classic = choice(app, "Classic")
        XCTAssertTrue(classic.waitForExistence(timeout: 5))
        classic.tap()
        XCTAssertTrue(app.buttons["Meter Face"].waitForNonExistence(timeout: 5))
        shoot(app, "ipad-meter-classic")

        // Back to the desktop's defaults for the tests that follow.
        menuButton.tap()
        XCTAssertTrue(app.buttons["Meter Face"].waitForExistence(timeout: 5))
        app.buttons["Meter Face"].tap()
        let cream = choice(app, "Aged Cream")
        XCTAssertTrue(cream.waitForExistence(timeout: 5))
        cream.tap()
        menuButton.tap()
        XCTAssertTrue(app.buttons["RX Mode"].waitForExistence(timeout: 5))
        app.buttons["RX Mode"].tap()
        let signal = app.buttons.matching(NSPredicate(format: "label == 'Signal' OR label BEGINSWITH 'Signal,'"))
            .firstMatch
        XCTAssertTrue(signal.waitForExistence(timeout: 5))
        signal.tap()
    }

    /// R-IOS-58, DC7, DC8: the band column at the band's top-left, its
    /// buttons in the desktop's order; each of BAND, ANT, Display, VAX and
    /// ATT opens its menu beside the column, one at a time, with the PTT
    /// just right of it and never under it, on its side and upright; a tap
    /// anywhere else closes it. +RX asks for a slice on the pan by its key,
    /// +TNF a notch for the active slice, and ◀ folds the column.
    @MainActor
    func testBandColumnMenusKeepClearOfThePtt() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The band column is tried on an iPad")
        XCUIDevice.shared.orientation = .landscapeLeft
        let app = XCUIApplication()
        // The band menu's Core with no Core (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", "-NereusBandMenuFixture"]
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launchEnvironment["NEREUS_UITEST_DISPLAY"] = root.appendingPathComponent("resources/setup/display.json").path
        app.launch()
        let column = app.descendants(matching: .any)["bandColumn"]
        XCTAssertTrue(column.waitForExistence(timeout: 30), "The iPad on its side has no band column")
        let ids = ["fold", "rx", "tnf", "band", "antenna", "display", "vax", "attenuator"]
        let buttons = ids.map { app.buttons["bandColumn.\($0)"] }
        for (id, button) in zip(ids, buttons) {
            XCTAssertTrue(button.exists, "The column has no \(id) button")
            XCTAssertEqual(button.frame.height, 30, accuracy: 1, "\(id) is not 30 points tall")
        }
        for (upper, lower) in zip(buttons, buttons.dropFirst()) {
            XCTAssertLessThan(upper.frame.minY, lower.frame.minY, "The column's buttons are out of order")
        }
        let band = app.descendants(matching: .any)["bandSurface"]
        if band.exists {
            XCTAssertLessThan(column.frame.minX - band.frame.minX, 20, "The column is not at the band's left")
            XCTAssertLessThan(column.frame.minY - band.frame.minY, 20, "The column is not at the band's top")
        }

        checkMenusKeepClearOfThePtt(app, column: column, shots: "ipad-ui-column")
        XCUIDevice.shared.orientation = .portrait
        XCTAssertTrue(column.waitForExistence(timeout: 20), "The iPad upright has no band column")
        checkMenusKeepClearOfThePtt(app, column: column, shots: "ipad-ui-upright-column")

        // +RX and +TNF: what they send, read back in the fixture's refusal.
        let note = app.descendants(matching: .any)["bandColumnNote"]
        app.buttons["bandColumn.rx"].tap()
        XCTAssertTrue(note.waitForExistence(timeout: 10), "+RX sent nothing")
        XCTAssertEqual(note.label, "UI fixture: addSliceOnPan panId pan-0")
        app.buttons["bandColumn.tnf"].tap()
        XCTAssertTrue(waitUntil { note.exists && note.label == "UI fixture: notch.addAtSlice sliceId 0" },
                      "+TNF sent \(note.exists ? note.label : "nothing")")

        // ◀ folds the column to one button, and back.
        let fold = app.buttons["bandColumn.fold"]
        XCTAssertEqual(fold.label, "Fold the buttons away")
        fold.tap()
        XCTAssertTrue(app.buttons["bandColumn.band"].waitForNonExistence(timeout: 5), "◀ did not fold the column")
        XCTAssertEqual(fold.label, "Show the buttons")
        fold.tap()
        XCTAssertTrue(app.buttons["bandColumn.band"].waitForExistence(timeout: 5), "▶ did not bring the buttons back")
    }

    /// R-IOS-59, DC9, spec section 6.3: the Pan sheet's Add a pan opens Pan
    /// 2 below Pan 1 with its own column, the toolbar naming the pan last
    /// touched; Pan 2's column opens Pan 2's pill; Close this pan on Pan 2's
    /// sheet closes it, and Pan 1 is the toolbar's again.
    @MainActor
    func testTwoPansFromThePanSheet() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "A second pan is the iPad's")
        XCUIDevice.shared.orientation = .landscapeLeft
        let app = XCUIApplication()
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", "-NereusBandMenuFixture",
                               "-NereusTwoPansFixture"]
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launchEnvironment["NEREUS_UITEST_DISPLAY"] = root.appendingPathComponent("resources/setup/display.json").path
        app.launch()
        let panButton = app.buttons["pan"]
        XCTAssertTrue(panButton.waitForExistence(timeout: 30))
        XCTAssertEqual(panButton.label, "Pan 1")
        panButton.tap()
        let add = app.buttons["panSheet.addPan"]
        let close = app.buttons["panSheet.closePan"]
        XCTAssertTrue(add.waitForExistence(timeout: 10), "The Pan sheet has no Add a pan")
        XCTAssertTrue(add.isEnabled)
        XCTAssertFalse(close.isEnabled, "Close this pan is not greyed on Pan 1")
        add.tap()
        let second = app.descendants(matching: .any)["pan.2"]
        XCTAssertTrue(second.waitForExistence(timeout: 10), "Add a pan opened no Pan 2")
        let first = app.descendants(matching: .any)["pan.1"]
        XCTAssertTrue(first.exists)
        // Pan 1 on top, the two sharing the height.
        XCTAssertLessThan(first.frame.minY, second.frame.minY)
        XCTAssertEqual(first.frame.height, second.frame.height, accuracy: 2)
        XCTAssertTrue(waitUntil { panButton.label == "Pan 2" }, "The toolbar does not follow Pan 2")
        let displays = app.buttons.matching(identifier: "bandColumn.display")
        XCTAssertTrue(waitUntil { displays.count == 2 }, "Each pan has no column of its own")
        // The Pan sheet stays down after Add a pan; a tap off it closes it.
        app.buttons["Close the sheet"].firstMatch
            .coordinate(withNormalizedOffset: CGVector(dx: 0.97, dy: 0.9)).tap()
        XCTAssertTrue(app.descendants(matching: .any)["panSheet"].waitForNonExistence(timeout: 10),
                      "A tap off the Pan sheet left it down")

        // Pan 1's BAND touches Pan 1: the toolbar follows it.
        let bands = app.buttons.matching(identifier: "bandColumn.band")
        let firstBand = bands.allElementsBoundByIndex.min { $0.frame.minY < $1.frame.minY }
        firstBand?.tap()
        XCTAssertTrue(waitUntil { panButton.label == "Pan 1" }, "Touching Pan 1 left the toolbar on Pan 2")
        app.descendants(matching: .any)["bandColumnClose"].firstMatch
            .coordinate(withNormalizedOffset: CGVector(dx: 0.92, dy: 0.3)).tap()

        // Pan 2's Display opens Pan 2's pill.
        let secondDisplay = displays.allElementsBoundByIndex.max { $0.frame.minY < $1.frame.minY }
        secondDisplay?.tap()
        let pillPan = app.staticTexts["displayPillPan"]
        XCTAssertTrue(pillPan.waitForExistence(timeout: 10), "Pan 2's Display opened no pill")
        XCTAssertTrue(pillPan.label.hasPrefix("Pan 2"), "The pill is \(pillPan.label)")
        XCTAssertTrue(waitUntil { panButton.label == "Pan 2" })
        XCTAssertGreaterThan(pillPan.frame.minY, second.frame.minY, "Pan 2's pill is not on Pan 2")
        shoot(app, "ipad-ui-two-pans-pill-2")

        // Close this pan on Pan 2's sheet.
        panButton.tap()
        XCTAssertTrue(close.waitForExistence(timeout: 10))
        XCTAssertTrue(close.isEnabled, "Close this pan is greyed on Pan 2")
        close.tap()
        XCTAssertTrue(second.waitForNonExistence(timeout: 10), "Close this pan left Pan 2 open")
        XCTAssertTrue(waitUntil { panButton.label == "Pan 1" })
    }

    /// R-IOS-60, DC12: Pan 2's pill's Setup defaults opens the tab's Setup
    /// page with Pan 2 chosen on the strip, and the strip knows Pan 2 is open.
    @MainActor
    func testSecondPansSetupDefaultsChoosesPanTwo() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "A second pan is the iPad's")
        XCUIDevice.shared.orientation = .landscapeLeft
        let app = XCUIApplication()
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand", "-NereusBandMenuFixture",
                               "-NereusTwoPansFixture"]
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launchEnvironment["NEREUS_UITEST_DISPLAY"] = root.appendingPathComponent("resources/setup/display.json").path
        app.launch()
        let panButton = app.buttons["pan"]
        XCTAssertTrue(panButton.waitForExistence(timeout: 30))
        panButton.tap()
        let add = app.buttons["panSheet.addPan"]
        XCTAssertTrue(add.waitForExistence(timeout: 10), "The Pan sheet has no Add a pan")
        add.tap()
        let second = app.descendants(matching: .any)["pan.2"]
        XCTAssertTrue(second.waitForExistence(timeout: 10), "Add a pan opened no Pan 2")
        app.buttons["Close the sheet"].firstMatch
            .coordinate(withNormalizedOffset: CGVector(dx: 0.97, dy: 0.9)).tap()
        XCTAssertTrue(app.descendants(matching: .any)["panSheet"].waitForNonExistence(timeout: 10))
        let displays = app.buttons.matching(identifier: "bandColumn.display")
        XCTAssertTrue(waitUntil { displays.count == 2 }, "Each pan has no column of its own")
        let secondDisplay = displays.allElementsBoundByIndex.max { $0.frame.minY < $1.frame.minY }
        secondDisplay?.tap()
        let pillPan = app.staticTexts["displayPillPan"]
        XCTAssertTrue(pillPan.waitForExistence(timeout: 10), "Pan 2's Display opened no pill")
        XCTAssertTrue(pillPan.label.hasPrefix("Pan 2"), "The pill is \(pillPan.label)")
        let tab = app.descendants(matching: .any)["displayPillTab.spectrum"]
        XCTAssertTrue(tab.waitForExistence(timeout: 10))
        tab.tap()
        let link = app.buttons["displayPillSetupDefaults"]
        XCTAssertTrue(link.waitForExistence(timeout: 5), "Pan 2's pill has no Setup defaults link")
        link.tap()
        XCTAssertTrue(app.descendants(matching: .any)["display.spectrumDefaults.detector"].waitForExistence(timeout: 10),
                      "Setup defaults did not open the Spectrum page")
        let pan2 = app.descendants(matching: .any)["displayStrip.pan2"]
        XCTAssertTrue(pan2.waitForExistence(timeout: 5), "The page's strip has no Pan 2")
        XCTAssertTrue(pan2.isSelected, "Setup defaults on Pan 2's pill did not choose Pan 2")
        XCTAssertFalse(app.descendants(matching: .any)["displayStrip.pan1"].isSelected)
        let note = app.descendants(matching: .any)["displayStripNote"]
        if note.exists {
            XCTAssertFalse(note.label.contains("is not open now"), "The strip calls an open Pan 2 not open")
        }
        shoot(app, "ipad-ui-pan-2-setup-defaults")
    }

    /// Each of the column's menus open in turn: one at a time, the PTT
    /// clear of it and of the column, closed by a tap elsewhere.
    @MainActor
    private func checkMenusKeepClearOfThePtt(_ app: XCUIApplication, column: XCUIElement, shots: String) {
        let ptt = app.buttons["ptt"]
        XCTAssertTrue(ptt.waitForExistence(timeout: 10))
        let home = ptt.frame
        var previous: XCUIElement?
        for id in ["band", "antenna", "display", "vax", "attenuator"] {
            let button = app.buttons["bandColumn.\(id)"]
            button.tap()
            let menu = app.descendants(matching: .any)["bandColumnMenu.\(id)"]
            XCTAssertTrue(menu.waitForExistence(timeout: 10), "\(id) opened no menu")
            XCTAssertEqual(button.value as? String, "Open")
            if let previous {
                XCTAssertFalse(previous.exists, "Two of the column's menus are open at once")
            }
            XCTAssertGreaterThanOrEqual(menu.frame.minX, column.frame.maxX - 1, "\(id)'s menu is not beside the column")
            XCTAssertTrue(waitUntil { !menu.frame.intersects(ptt.frame) }, "\(id)'s menu covers the PTT")
            XCTAssertFalse(column.frame.intersects(ptt.frame), "The column covers the PTT")
            if menu.frame.maxY > home.minY {
                XCTAssertLessThan(ptt.frame.minX - menu.frame.maxX, 20, "The PTT is not just right of \(id)'s menu")
            }
            shoot(app, "\(shots)-\(id)")
            previous = menu
        }
        // A tap anywhere else closes it, and the PTT comes home.
        app.descendants(matching: .any)["bandColumnClose"]
            .coordinate(withNormalizedOffset: CGVector(dx: 0.92, dy: 0.15)).tap()
        XCTAssertTrue(previous?.waitForNonExistence(timeout: 5) ?? false, "A tap off the menu did not close it")
        XCTAssertTrue(waitUntil { abs(ptt.frame.minX - home.minX) < 1 }, "The PTT did not come home")
    }

    @MainActor
    private func waitUntil(_ condition: () -> Bool) -> Bool {
        for _ in 0..<50 {
            if condition() {
                return true
            }
            RunLoop.current.run(until: Date().addingTimeInterval(0.1))
        }
        return condition()
    }

    /// A menu item whose label starts with `title` (a reason may follow it).
    @MainActor
    private func choice(_ app: XCUIApplication, _ title: String) -> XCUIElement {
        app.buttons.matching(NSPredicate(format: "label BEGINSWITH %@", title)).firstMatch
    }

    /// Writes the screen to `NEREUS_IPAD_SHOTS` when it names a directory,
    /// and attaches it to the test's results.
    @MainActor
    private func shoot(_ app: XCUIApplication, _ name: String) {
        let shot = app.screenshot()
        let attachment = XCTAttachment(screenshot: shot)
        attachment.name = name
        attachment.lifetime = .keepAlways
        add(attachment)
        if let directory = ProcessInfo.processInfo.environment["NEREUS_IPAD_SHOTS"], !directory.isEmpty {
            try? shot.pngRepresentation.write(to: URL(fileURLWithPath: directory).appendingPathComponent("\(name).png"))
        }
    }
}
