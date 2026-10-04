// NereusSDR for iOS: a drag in a flag's panel or menu never moves the band, and the toolbar's sheets take their own touches
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
final class PanelDragUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = false
    }

    /// R-IOS-11 (JJ 2026-09-30): the AF gain slider in slice A's Audio
    /// panel is dragged across, and the band behind the panel stays where
    /// it is: A's flag does not move. The panel closes with a tap on the
    /// band away from it, and a drag there still moves the band, and the
    /// flag with it.
    @MainActor
    func testDragInsideTheAudioPanelLeavesTheBandWhereItIs() throws {
        let app = launch()
        let flag = app.descendants(matching: .any)["flagA"]
        try requireFlag(app, flag)
        let before = flag.frame

        app.buttons["flagTabAudioA"].tap()
        let panel = app.descendants(matching: .any)["flagPanelAudio"]
        XCTAssertTrue(panel.waitForExistence(timeout: 10))
        let slider = panel.descendants(matching: .any).matching(NSPredicate(format: "label == %@", "AF gain")).firstMatch
        XCTAssertTrue(slider.waitForExistence(timeout: 10))
        waitUntil("slice A's AF gain never showed") { Self.text(slider.value) == "37" }
        let start = Self.text(slider.value)
        // Across the slider's right half, well away from A's line and passband.
        // The first drag raises AF gain from slice A's 37: the drag reached the slider.
        slider.coordinate(withNormalizedOffset: CGVector(dx: 0.55, dy: 0.5))
            .press(forDuration: 0.1, thenDragTo: slider.coordinate(withNormalizedOffset: CGVector(dx: 0.95, dy: 0.5)))
        waitUntil("the drag did not raise AF gain from \(start)") { (Double(Self.text(slider.value)) ?? 0) > 37 }
        slider.coordinate(withNormalizedOffset: CGVector(dx: 0.95, dy: 0.5))
            .press(forDuration: 0.1, thenDragTo: slider.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5)))
        // Nothing to wait for: the band is checked a moment after, still where it was.
        Thread.sleep(forTimeInterval: 0.5)
        XCTAssertTrue(panel.exists, "the drag closed the panel")
        XCTAssertEqual(flag.frame.minX, before.minX, accuracy: 1, "the band moved under the panel")

        // Outside the panel the band still closes it on a tap and pans on a drag.
        app.coordinate(withNormalizedOffset: CGVector(dx: 0.8, dy: 0.12)).tap()
        XCTAssertTrue(panel.waitForNonExistence(timeout: 10))
        XCTAssertEqual(flag.frame.minX, before.minX, accuracy: 1, "the tap moved the band")
        let from = app.coordinate(withNormalizedOffset: CGVector(dx: 0.7, dy: 0.45))
        from.press(forDuration: 0.1, thenDragTo: app.coordinate(withNormalizedOffset: CGVector(dx: 0.9, dy: 0.45)))
        waitUntil("a drag on the band did not move it") { abs(flag.frame.minX - before.minX) > 20 }
        XCTAssertGreaterThan(abs(flag.frame.minX - before.minX), 20, "a drag on the band did not move it")
    }

    /// The same for the antenna menu: a drag that starts on it leaves the
    /// band where it is. The redrawn flag holds no step menu: its step is
    /// the X/RIT panel's cycle button, which opens nothing.
    @MainActor
    func testDragOnTheAntennaMenuLeavesTheBandWhereItIs() throws {
        let app = launch()
        let flag = app.descendants(matching: .any)["flagA"]
        try requireFlag(app, flag)
        let before = flag.frame

        for (button, menu) in [("flagAntennasA", "flagMenu")] {
            app.buttons[button].tap()
            let opened = app.descendants(matching: .any)[menu]
            XCTAssertTrue(opened.waitForExistence(timeout: 10), menu)
            opened.coordinate(withNormalizedOffset: CGVector(dx: 0.2, dy: 0.5))
                .press(forDuration: 0.1, thenDragTo: opened.coordinate(withNormalizedOffset: CGVector(dx: 0.9, dy: 0.5)))
            Thread.sleep(forTimeInterval: 0.5)
            XCTAssertEqual(flag.frame.minX, before.minX, accuracy: 1, "the band moved under \(menu)")
            app.coordinate(withNormalizedOffset: CGVector(dx: 0.8, dy: 0.7)).tap()
            XCTAssertTrue(opened.waitForNonExistence(timeout: 10), menu)
        }
    }

    /// JJ 2026-09-30, sideways: the toolbar's Display and Pan 1 sheets sit
    /// over the flag, its round buttons and the band's zoom, and none of
    /// those takes a touch through the sheet. With `NEREUS_CONNECT_SHOTS`
    /// set (through `TEST_RUNNER_NEREUS_CONNECT_SHOTS`), the Display sheet
    /// is written there sideways as a PNG.
    @MainActor
    func testToolbarSheetsSitOverTheFlagAndZoomSideways() throws {
        addTeardownBlock { @MainActor in
            XCUIDevice.shared.orientation = .portrait
        }
        let app = launch()
        try requireFlag(app, app.descendants(matching: .any)["flagA"])
        XCUIDevice.shared.orientation = .landscapeLeft
        let toolbar = app.otherElements[UIDevice.current.userInterfaceIdiom == .pad ? "toolbarIPad" : "toolbarSideways"]
        XCTAssertTrue(toolbar.waitForExistence(timeout: 20))
        // The band moved so A's flag straddles the Display sheet's right
        // edge, as JJ saw it: part under the sheet, part beside it.
        let flagA = app.descendants(matching: .any)["flagA"]
        toolbar.buttons["display"].tap()
        let display = app.descendants(matching: .any)["displaySheet"]
        XCTAssertTrue(display.waitForExistence(timeout: 10))
        let edge = display.frame.maxX
        toolbar.buttons["display"].tap()
        XCTAssertTrue(display.waitForNonExistence(timeout: 10))
        let move = edge - 40 - flagA.frame.minX
        let from = app.coordinate(withNormalizedOffset: .zero).withOffset(CGVector(dx: edge + 60, dy: flagA.frame.maxY + 90))
        from.press(forDuration: 0.1, thenDragTo: from.withOffset(CGVector(dx: move, dy: 0)))
        Thread.sleep(forTimeInterval: 0.5)
        let band = [app.buttons["flagCloseA"], app.buttons["flagLockA"], app.buttons["flagMoreA"],
                    app.buttons["Zoom in"], app.buttons["Zoom out"], app.buttons["flagTabAudioA"],
                    app.buttons["flagFrequencyA"]]
        for (button, sheetId) in [("display", "displaySheet"), ("pan", "panSheet")] {
            toolbar.buttons[button].tap()
            let sheet = app.descendants(matching: .any)[sheetId]
            XCTAssertTrue(sheet.waitForExistence(timeout: 10), sheetId)
            Thread.sleep(forTimeInterval: 0.6)
            if sheetId == "displaySheet" {
                try save("display-sheet-sideways")
            }
            // A tap where each covered button lies lands on the sheet: no
            // menu or panel opens, the band neither zooms nor moves, and
            // the sheet stays open.
            let flag = app.descendants(matching: .any)["flagA"]
            let before = flag.frame
            var covered = 0
            for element in band where element.exists {
                let centre = CGPoint(x: element.frame.midX, y: element.frame.midY)
                guard sheet.frame.insetBy(dx: 4, dy: 4).contains(centre) else {
                    continue
                }
                covered += 1
                app.coordinate(withNormalizedOffset: .zero)
                    .withOffset(CGVector(dx: centre.x, dy: centre.y)).tap()
                Thread.sleep(forTimeInterval: 0.4)
                let what = "\(element.identifier) \(element.label) under \(sheetId)"
                XCTAssertFalse(app.staticTexts["Slice A always stays open."].exists, "\(what): the flag took the tap")
                XCTAssertTrue(sheet.exists, "\(what): the sheet closed")
                XCTAssertFalse(app.descendants(matching: .any)["flagMenu"].exists, "\(what) opened a menu")
                XCTAssertFalse(app.descendants(matching: .any)["flagPanelAudio"].exists, "\(what) opened a panel")
                XCTAssertFalse(app.descendants(matching: .any)["tuneStepMenu"].exists, "\(what) opened the step menu")
                XCTAssertEqual(flag.frame, before, "\(what) moved or zoomed the band")
            }
            XCTAssertGreaterThan(covered, 0, "nothing of the band's lies under \(sheetId)")
            toolbar.buttons[button].tap()
            XCTAssertTrue(sheet.waitForNonExistence(timeout: 10), sheetId)
        }
    }

    /// Slice A's flag is on the band; if not, the screen is written (with
    /// `NEREUS_CONNECT_SHOTS` set) and its elements printed before failing.
    @MainActor
    private func requireFlag(_ app: XCUIApplication, _ flag: XCUIElement) throws {
        guard !flag.waitForExistence(timeout: 30) else {
            return
        }
        // The first launch after the test run installs the app has come up
        // on Welcome, as if without its launch arguments (seen twice on
        // 2026-09-30): once, the app is launched again.
        if app.buttons["Find my Core"].exists {
            try save("welcome-\(name.filter { $0.isLetter })")
            app.terminate()
            app.launch()
            if flag.waitForExistence(timeout: 30) {
                return
            }
        }
        try save("no-flag-\(name.filter { $0.isLetter })")
        print(app.debugDescription)
        XCTFail("slice A's flag never showed")
    }

    /// An element's accessibility value as text, empty when it has none.
    private static func text(_ value: Any?) -> String {
        value.map { "\($0)" } ?? ""
    }

    /// Waits up to `timeout` seconds for `condition`, read on the main
    /// thread, to hold; fails with `what` when it never does.
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
    private func launch() -> XCUIApplication {
        let app = XCUIApplication()
        // The band with slice A and the Core's catalogue, with no Core (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusFlagsOnBand"]
        let root = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
            .deletingLastPathComponent()
        app.launchEnvironment["NEREUS_UITEST_CATALOGUE"] = root
            .appendingPathComponent("tests/data/link/v1/sessions/catalog-anan-g2.json").path
        app.launch()
        return app
    }
}
