// NereusSDR for iOS: the Display pill's Setup defaults link opens each tab's Setup page on its pan, NB Blank agrees between the two, and the band menu a hold opens, on each iPad pan too
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class DisplayPillUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    override func tearDown() {
        MainActor.assumeIsolated {
            XCUIDevice.shared.orientation = .portrait
        }
    }

    /// Each tab's first control on the Setup page its Setup defaults link
    /// opens, and whether that page has the pans' strip: FFT & Engine holds
    /// only rows every pan shares, so it has none.
    private static let firstControls: [(tab: String, control: String, strip: Bool)] = [
        ("spectrum", "display.spectrumDefaults.detector", true),
        ("levels", "display.levels.scaleMode", true),
        ("waterfall", "display.waterfallDefaults.colorScheme", true),
        ("view", "display.threeD.renderMode", true),
        ("fft", "display.spectrumDefaults.fftSizing", false),
    ]

    /// R-IOS-55, DC4: Setup defaults on each tab opens that tab's Setup >
    /// Display page, with the pill's pan, Pan 1, chosen on the strip.
    @MainActor
    func testSetupDefaultsOpensEachTabsPageOnThePan() throws {
        let app = launch()
        for (tab, control, strip) in Self.firstControls {
            openPill(app, tab: tab)
            let link = app.buttons["displayPillSetupDefaults"]
            XCTAssertTrue(link.waitForExistence(timeout: 5), "The \(tab) tab has no Setup defaults link")
            link.tap()
            let first = app.descendants(matching: .any)[control]
            XCTAssertTrue(first.waitForExistence(timeout: 10), "Setup defaults on \(tab) did not open its page")
            let pan = app.descendants(matching: .any)["displayStrip.pan1"]
            guard strip else {
                XCTAssertFalse(pan.exists, "The \(tab) page shows a strip for rows every pan shares")
                backToBand(app)
                continue
            }
            XCTAssertTrue(pan.waitForExistence(timeout: 5), "The \(tab) page has no strip")
            XCTAssertTrue(pan.isSelected, "Setup defaults on \(tab) did not choose Pan 1 on the strip")
            XCTAssertFalse(app.descendants(matching: .any)["displayStrip.all"].isSelected)
            backToBand(app)
        }
    }

    /// DC4: NB Blank turned on in the sheet's Waterfall tab is on in Setup >
    /// Display > Waterfall for Pan 1; turned off there, the sheet shows it off
    /// and its threshold greyed with the reason.
    @MainActor
    func testNbBlankAgreesBetweenTheSheetAndSetup() throws {
        let app = launch()
        openPill(app, tab: "waterfall")
        // NB Blank's switch sits on its threshold's line, which names the
        // line's elements by the threshold's key.
        let pillSwitch = app.buttons.matching(NSPredicate(
            format: "identifier == 'displayPill.WaterfallBlankingThresholdDb' AND label == 'NB Blank' AND (value == 'On' OR value == 'Off')"))
            .firstMatch
        XCTAssertTrue(pillSwitch.waitForExistence(timeout: 5))
        // The simulator keeps this app's settings between runs: start from off.
        if pillSwitch.value as? String == "On" {
            pillSwitch.tap()
        }
        XCTAssertEqual(pillSwitch.value as? String, "Off")
        pillSwitch.tap()
        XCTAssertEqual(pillSwitch.value as? String, "On", "NB Blank did not turn on in the sheet")
        // The pan now keeps its own NB Blank: its row's dot shows.
        XCTAssertTrue(app.buttons["Pan 1's own NB Blank. Tap the dot to follow the Setup default again."].exists)

        app.buttons["displayPillSetupDefaults"].tap()
        let setupSwitch = app.switches["display.waterfall.blanking"]
        XCTAssertTrue(setupSwitch.waitForExistence(timeout: 10), "Setup's NB Blank is not on the page")
        XCTAssertTrue(app.descendants(matching: .any)["displayStrip.pan1"].isSelected)
        XCTAssertEqual(setupSwitch.value as? String, "1", "Setup does not show the sheet's NB Blank for Pan 1")
        // The Setup switch takes its tap on the track at the row's end;
        // bring the row on screen first.
        for _ in 0..<6 where !setupSwitch.isHittable {
            app.swipeUp()
        }
        setupSwitch.coordinate(withNormalizedOffset: CGVector(dx: 1, dy: 0.5))
            .withOffset(CGVector(dx: -26, dy: 0)).tap()
        let off = expectation(for: NSPredicate(format: "value == '0'"), evaluatedWith: setupSwitch)
        _ = XCTWaiter.wait(for: [off], timeout: 5)
        XCTAssertEqual(setupSwitch.value as? String, "0", "NB Blank did not turn off in Setup")

        backToBand(app)
        openPill(app, tab: "waterfall")
        XCTAssertTrue(pillSwitch.waitForExistence(timeout: 5))
        XCTAssertEqual(pillSwitch.value as? String, "Off", "The sheet does not show Setup's NB Blank")
        XCTAssertTrue(app.staticTexts["NB Blank is off, so there is no threshold to set."].exists,
                      "The threshold is not greyed with its reason")
    }

    /// R-IOS-57, DC6: a tap on the band tunes and opens nothing; a hold on a
    /// spot opens its details, never the band menu; a half-second hold on
    /// empty band opens the band menu with the desktop's four groups, and
    /// each item acts: Ref Level and Dyn Range move the band's scale on the
    /// Core, CTUN turns on and off, Add notch here sends `notch.add` at the
    /// finger (the fixture's Core refuses it with what it was sent), and
    /// Display settings... opens the Display sheet.
    @MainActor
    func testHoldOnEmptyBandOpensTheBandMenu() throws {
        XCUIDevice.shared.orientation = .portrait
        let app = launch(bandMenu: true)
        let surface = app.descendants(matching: .any)["bandSurface"]
        XCTAssertTrue(surface.waitForExistence(timeout: 30), "The band has no touch surface")
        let menu = app.descendants(matching: .any)["bandMenu"]

        surface.coordinate(withNormalizedOffset: CGVector(dx: 0.2, dy: 0.65)).tap()
        XCTAssertFalse(menu.waitForExistence(timeout: 1.5), "A tap opened the band menu")

        let spot = app.buttons["JA1ABC"]
        XCTAssertTrue(spot.waitForExistence(timeout: 10), "JA1ABC is not on the band")
        spot.press(forDuration: 0.8)
        XCTAssertTrue(app.descendants(matching: .any)["spotDetails"].waitForExistence(timeout: 10),
                      "A hold on a spot did not open its details")
        XCTAssertFalse(menu.exists, "A hold on a spot opened the band menu")
        app.buttons["spotDetailsClose"].tap()
        XCTAssertTrue(app.descendants(matching: .any)["spotDetails"].waitForNonExistence(timeout: 10))

        holdOnBand(app, at: CGVector(dx: 0.6, dy: 0.65))
        XCTAssertTrue(menu.waitForExistence(timeout: 5), "A hold on empty band did not open the band menu")
        for (id, words) in [("bandMenuDisplayRange", "Display Range"), ("bandMenuTuning", "Tuning"),
                            ("bandMenuNotch", "Notch")] {
            XCTAssertEqual(app.staticTexts[id].label, words)
        }
        let settings = app.buttons["bandMenuDisplaySettings"]
        XCTAssertEqual(settings.label, "Display settings...")
        // The groups run down the menu in the desktop's order.
        XCTAssertLessThan(app.staticTexts["bandMenuDisplayRange"].frame.minY, app.staticTexts["bandMenuTuning"].frame.minY)
        XCTAssertLessThan(app.staticTexts["bandMenuTuning"].frame.minY, app.staticTexts["bandMenuNotch"].frame.minY)
        XCTAssertLessThan(app.staticTexts["bandMenuNotch"].frame.minY, settings.frame.minY)

        // Display Range: the fixture's Core keeps nothing for 40 m, so the desktop's defaults.
        let ref = app.descendants(matching: .any)["bandMenuRefLevel"]
        let dyn = app.descendants(matching: .any)["bandMenuDynRange"]
        XCTAssertEqual(ref.value as? String, "-40 dBm")
        XCTAssertEqual(dyn.value as? String, "100 dB")
        XCTAssertFalse(app.staticTexts["bandMenuRangeReason"].exists, "The range is greyed on a Core that shares it")
        // A drag across each slider's right half raises Ref Level, which keeps the span.
        drag(ref, from: 0.6, to: 0.9)
        XCTAssertTrue(waitUntil { (Self.number(ref) ?? -40) > -40 }, "Ref Level did not move up")
        let top = Self.number(ref)
        XCTAssertEqual(dyn.value as? String, "100 dB", "Ref Level did not keep the span")
        // Dyn Range down, which keeps the top.
        drag(dyn, from: 0.7, to: 0.4)
        XCTAssertTrue(waitUntil { (Self.number(dyn) ?? 100) < 100 }, "Dyn Range did not move down")
        XCTAssertEqual(Self.number(ref), top, "Dyn Range did not keep the top")

        // Tuning: the simulator keeps this app's settings between runs, so start from off.
        let ctun = app.buttons["bandMenuCtun"]
        XCTAssertEqual(ctun.label, "CTUN (independent pan)")
        if ctun.value as? String == "On" {
            ctun.tap()
        }
        XCTAssertTrue(waitForValue(ctun, "Off"))
        ctun.tap()
        XCTAssertTrue(waitForValue(ctun, "On"), "CTUN did not turn on")
        ctun.tap()
        XCTAssertTrue(waitForValue(ctun, "Off"), "CTUN did not turn off")

        // Notch: at the frequency under the finger, 200 Hz wide, for slice A.
        let add = app.buttons["bandMenuAddNotch"]
        XCTAssertEqual(add.label, "Add notch here")
        XCTAssertTrue(add.isEnabled)
        let hz = app.staticTexts["bandMenuNotchHz"].label
        XCTAssertTrue(hz.hasSuffix(" MHz") && hz.hasPrefix("7.2"), "The notch row shows \(hz)")
        add.tap()
        let sent = app.staticTexts["bandMenuNotchReason"]
        XCTAssertTrue(sent.waitForExistence(timeout: 10), "Add notch here sent nothing")
        XCTAssertEqual(sent.label, "UI fixture: notch.add at \(hz), 200 Hz wide, slice 0")
        XCTAssertTrue(menu.exists, "The band menu closed after Add notch here, which the desktop's keeps open")

        settings.tap()
        XCTAssertTrue(menu.waitForNonExistence(timeout: 5), "Display settings... left the band menu open")
        XCTAssertTrue(app.descendants(matching: .any)["displayPillTab.spectrum"].waitForExistence(timeout: 10),
                      "Display settings... did not open the Display sheet")
    }

    /// R-IOS-57, DC6: sideways, the band menu opens beside the finger, on
    /// its right, clear of PTT.
    @MainActor
    func testBandMenuOpensBesideTheFingerSideways() throws {
        XCUIDevice.shared.orientation = .landscapeLeft
        let app = launch(bandMenu: true)
        let surface = app.descendants(matching: .any)["bandSurface"]
        XCTAssertTrue(surface.waitForExistence(timeout: 30), "The band has no touch surface")
        let at = CGVector(dx: 0.45, dy: 0.6)
        holdOnBand(app, at: at)
        let menu = app.descendants(matching: .any)["bandMenu"]
        XCTAssertTrue(menu.waitForExistence(timeout: 5), "A hold on empty band did not open the band menu sideways")
        let fingerX = surface.frame.minX + surface.frame.width * at.dx
        XCTAssertGreaterThan(menu.frame.minX, fingerX, "The band menu does not open beside the finger")
        let ptt = app.buttons["ptt"]
        XCTAssertTrue(ptt.exists)
        XCTAssertFalse(menu.frame.intersects(ptt.frame), "The band menu covers PTT")
        app.descendants(matching: .any)["bandMenuClose"].coordinate(withNormalizedOffset: CGVector(dx: 0.05, dy: 0.1)).tap()
        XCTAssertTrue(menu.waitForNonExistence(timeout: 5), "A tap off the band menu did not close it")
    }

    /// R-IOS-57, DC6, DC9: on the iPad with two pans stacked, a hold on
    /// each pan's empty band opens the band menu on that pan and acts on
    /// it: the toolbar follows the pan, and Add notch here sends
    /// `notch.add` for that pan's slice (A on Pan 1, B on Pan 2) at the
    /// finger. A finger held still: the hold must not wait for it to move.
    @MainActor
    func testHoldOnEmptyBandOpensTheBandMenuOnEachIPadPan() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad,
                      "Two pans are the iPad's: run DisplayPillUITests on an iPad Pro 13-inch for this one")
        XCUIDevice.shared.orientation = .portrait
        let app = launch(bandMenu: true, spots: false, twoPans: true)
        let panButton = app.buttons["pan"]
        XCTAssertTrue(panButton.waitForExistence(timeout: 30))
        panButton.tap()
        let add = app.buttons["panSheet.addPan"]
        XCTAssertTrue(add.waitForExistence(timeout: 10), "The Pan sheet has no Add a pan")
        add.tap()
        let panFrames = [app.descendants(matching: .any)["pan.1"], app.descendants(matching: .any)["pan.2"]]
        XCTAssertTrue(panFrames[1].waitForExistence(timeout: 10), "Add a pan opened no Pan 2")
        app.buttons["Close the sheet"].firstMatch
            .coordinate(withNormalizedOffset: CGVector(dx: 0.97, dy: 0.9)).tap()
        XCTAssertTrue(app.descendants(matching: .any)["panSheet"].waitForNonExistence(timeout: 10))
        let surfaces = app.descendants(matching: .any).matching(identifier: "bandSurface")
        XCTAssertTrue(waitUntil { surfaces.count == 2 }, "The two pans have no touch surface each")

        let menu = app.descendants(matching: .any)["bandMenu"]
        for (index, pan) in panFrames.enumerated() {
            let surface = surfaces.allElementsBoundByIndex.first { pan.frame.contains(CGPoint(x: $0.frame.midX, y: $0.frame.midY)) }
            guard let surface else {
                XCTFail("Pan \(index + 1) has no touch surface")
                return
            }
            surface.coordinate(withNormalizedOffset: CGVector(dx: 0.6, dy: 0.7)).press(forDuration: 0.8)
            XCTAssertTrue(menu.waitForExistence(timeout: 5), "A hold on Pan \(index + 1)'s empty band did not open the band menu")
            XCTAssertTrue(pan.frame.contains(CGPoint(x: menu.frame.minX + 1, y: menu.frame.minY + 1)),
                          "Pan \(index + 1)'s band menu opened at \(menu.frame), off the pan at \(pan.frame)")
            XCTAssertTrue(waitUntil { panButton.label == "Pan \(index + 1)" }, "The toolbar did not follow Pan \(index + 1)")
            let hz = app.staticTexts["bandMenuNotchHz"].label
            XCTAssertTrue(hz.hasSuffix(" MHz") && hz.hasPrefix("7.2"), "Pan \(index + 1)'s notch row shows \(hz)")
            app.buttons["bandMenuAddNotch"].tap()
            let sent = app.staticTexts["bandMenuNotchReason"]
            XCTAssertTrue(sent.waitForExistence(timeout: 10), "Add notch here on Pan \(index + 1) sent nothing")
            XCTAssertEqual(sent.label, "UI fixture: notch.add at \(hz), 200 Hz wide, slice \(index)")
            app.descendants(matching: .any)["bandMenuClose"].coordinate(withNormalizedOffset: CGVector(dx: 0.05, dy: 0.1)).tap()
            XCTAssertTrue(menu.waitForNonExistence(timeout: 5), "A tap off Pan \(index + 1)'s band menu did not close it")
        }
    }

    /// Holds a finger still on the band at `offset` for longer than the band menu's half second.
    @MainActor
    private func holdOnBand(_ app: XCUIApplication, at offset: CGVector) {
        app.descendants(matching: .any)["bandSurface"].coordinate(withNormalizedOffset: offset).press(forDuration: 0.8)
    }

    @MainActor
    private func drag(_ element: XCUIElement, from: CGFloat, to: CGFloat) {
        element.coordinate(withNormalizedOffset: CGVector(dx: from, dy: 0.5))
            .press(forDuration: 0.1, thenDragTo: element.coordinate(withNormalizedOffset: CGVector(dx: to, dy: 0.5)))
    }

    /// The number a value such as "-40 dBm" starts with.
    @MainActor
    private static func number(_ element: XCUIElement) -> Double? {
        (element.value as? String)?.split(separator: " ").first.flatMap { Double($0) }
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

    @MainActor
    private func waitForValue(_ element: XCUIElement, _ value: String) -> Bool {
        let shown = expectation(for: NSPredicate(format: "value == %@", value), evaluatedWith: element)
        return XCTWaiter.wait(for: [shown], timeout: 5) == .completed
    }

    /// Opens the Display sheet from the toolbar on `tab`.
    @MainActor
    private func openPill(_ app: XCUIApplication, tab: String) {
        let display = app.buttons["display"]
        XCTAssertTrue(display.waitForExistence(timeout: 30), "The toolbar has no Display button")
        display.tap()
        let tabButton = app.descendants(matching: .any)["displayPillTab.\(tab)"]
        XCTAssertTrue(tabButton.waitForExistence(timeout: 10), "The Display pill has no \(tab) tab")
        tabButton.tap()
    }

    /// Back to the band from Setup.
    @MainActor
    private func backToBand(_ app: XCUIApplication) {
        let band = app.otherElements["Sections"].buttons["Panadapter"]
        XCTAssertTrue(band.waitForExistence(timeout: 5))
        band.tap()
    }

    @MainActor
    private func launch(bandMenu: Bool = false, spots: Bool = true, twoPans: Bool = false) -> XCUIApplication {
        let app = XCUIApplication()
        // The band with the Core's catalogue and its version 29 Display
        // description, with no Core (a debug build's launch arguments); for
        // the band menu, with the spots and the menu's Core as well.
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand"]
            + (bandMenu ? ["-NereusBandMenuFixture"] + (spots ? ["-NereusSpotFixture"] : []) : [])
            + (twoPans ? ["-NereusTwoPansFixture"] : [])
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launchEnvironment["NEREUS_UITEST_DISPLAY"] = root.appendingPathComponent("resources/setup/display.json").path
        app.launch()
        return app
    }
}
