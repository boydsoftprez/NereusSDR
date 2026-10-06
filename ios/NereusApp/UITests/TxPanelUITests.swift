// NereusSDR for iOS: the approved native TX panel's pinned controls and visible settings route
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

final class TxPanelUITests: XCTestCase {
    override func setUp() { continueAfterFailure = false }
    override func tearDown() { XCUIDevice.shared.orientation = .portrait }

    @MainActor
    func testPinnedSettingsBackPreservesTimingAndScroll() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom == .pad, "The phone drawer is tried on a phone")
        XCUIDevice.shared.orientation = .portrait
        let app = XCUIApplication()
        app.launchArguments = ["-NereusShowBand"]
        let lightValidation = ProcessInfo.processInfo.environment["NEREUS_TX_APPEARANCE"] == "light"
        if lightValidation { app.launchArguments.append("-NereusTxPanelLightValidation") }
        app.launch()
        if lightValidation {
            let scheme = app.descendants(matching: .any)["nativeTxValidationScheme"]
            XCTAssertTrue(scheme.waitForExistence(timeout: 10))
            XCTAssertEqual(scheme.label, "Light", "The real RootView receives the light scheme")
        }
        XCTAssertGreaterThan(app.windows.firstMatch.frame.height, app.windows.firstMatch.frame.width)
        let panelButton = app.buttons["TX panel"]
        XCTAssertTrue(panelButton.waitForExistence(timeout: 30))
        panelButton.tap()
        let settings = app.buttons["txSettings"]
        XCTAssertTrue(settings.waitForExistence(timeout: 10))
        let drawer = app.descendants(matching: .any)["txPanelDrawer"]
        assertPinnedControls(app, container: drawer, parentScroll: false)
        try save("native-txpanel-pinned")
        XCTAssertFalse(app.descendants(matching: .any)["txSwrProt"].exists)
        XCTAssertFalse(app.descendants(matching: .any)["txAmpNotSetUp"].exists)
        XCTAssertFalse(app.descendants(matching: .any)["txTunerNotSetUp"].exists)
        let timing = app.buttons["txVoiceTiming"]
        for _ in 0..<8 where !timing.isHittable { drawer.swipeUp() }
        XCTAssertTrue(timing.isHittable)
        XCTAssertFalse(app.descendants(matching: .any)["txVoxDelay"].exists)
        timing.tap()
        XCTAssertTrue(app.descendants(matching: .any)["txVoxDelay"].exists)
        XCTAssertTrue(app.descendants(matching: .any)["txAntiVoxTime"].exists)
        assertPinnedControls(app, container: drawer, parentScroll: false)
        try save("native-txpanel-timing-expanded")
        let timingFrame = timing.frame
        XCTAssertTrue(settings.isHittable, "Settings stays pinned while scrolled")
        settings.tap()
        let filter = app.descendants(matching: .any)["modesTxFilterLow"]
        XCTAssertTrue(filter.waitForExistence(timeout: 10))
        XCTAssertTrue(filter.isHittable, "Settings focuses the Transmit section")
        XCTAssertGreaterThanOrEqual(filter.frame.height, 44)
        try save("native-txpanel-modes-transmit")
        app.buttons["modesBackToTx"].tap()
        XCTAssertTrue(settings.waitForExistence(timeout: 10))
        XCTAssertTrue(app.descendants(matching: .any)["txVoxDelay"].exists)
        XCTAssertEqual(timing.frame.minY, timingFrame.minY, accuracy: 2, "Back keeps the open drawer's scroll")
        XCTAssertEqual(panelButton.value as? String, "Open")
        assertPinnedControls(app, container: drawer, parentScroll: false)
        try save("native-txpanel-back")
    }

    @MainActor
    func testIPadColumnPinnedSettingsAndBack() throws {
        try XCTSkipIf(UIDevice.current.userInterfaceIdiom != .pad, "The column is tried on an iPad")
        XCUIDevice.shared.orientation = .landscapeLeft
        let app = XCUIApplication()
        app.launchArguments = ["-NereusShowBand"]
        app.launch()
        let column = app.descendants(matching: .any)["appletColumn"]
        XCTAssertTrue(column.waitForExistence(timeout: 30))
        let settings = app.buttons["txSettings"]
        for _ in 0..<8 {
            let viewport = try iPadColumnViewport(app, column: column)
            if settings.exists && settings.isHittable && viewport.contains(settings.frame) { break }
            column.swipeUp()
        }
        XCTAssertTrue(settings.isHittable)
        let settingsViewport = try iPadColumnViewport(app, column: column)
        XCTAssertTrue(settingsViewport.contains(settings.frame), "The entire Settings target is inside the actual column viewport")
        try save("native-txpanel-ipad-full-column")
        let timing = app.buttons["txVoiceTiming"]
        try revealIPadTiming(timing, in: column, app: app)
        XCTAssertTrue(timing.isHittable)
        timing.tap()
        XCTAssertEqual(timing.value as? String, "Expanded", "Timing is expanded before visiting Modes")
        XCTAssertTrue(app.descendants(matching: .any)["txVoxDelay"].exists,
                      "VOX delay exists before visiting Modes")
        let timingFrame = timing.frame
        assertPinnedControls(app, container: column, parentScroll: true)
        try save("native-txpanel-ipad-full-pinned")
        XCTAssertTrue(settings.isHittable)
        let modesViewport = try iPadColumnViewport(app, column: column)
        XCTAssertTrue(modesViewport.contains(settings.frame), "The entire Settings target is visible before visiting Modes")
        settings.tap()
        let filter = app.descendants(matching: .any)["modesTxFilterLow"]
        XCTAssertTrue(filter.waitForExistence(timeout: 10))
        XCTAssertTrue(filter.isHittable)
        XCTAssertGreaterThanOrEqual(filter.frame.height, 44)
        try save("native-txpanel-ipad-modes-transmit")
        app.buttons["modesBackToTx"].tap()
        XCTAssertTrue(column.waitForExistence(timeout: 10))
        XCTAssertTrue(settings.isHittable)
        XCTAssertTrue(app.descendants(matching: .any)["txVoxDelay"].exists)
        XCTAssertEqual(timing.frame.minY, timingFrame.minY, accuracy: 2)
        assertPinnedControls(app, container: column, parentScroll: true)
        try save("native-txpanel-ipad-full-back")
    }

    /// Hittability can include a target covered by the pinned TX controls.
    /// Use the actual parent viewport, clipped between the toolbar and tab bar.
    @MainActor
    private func iPadColumnViewport(_ app: XCUIApplication, column: XCUIElement) throws -> CGRect {
        let parent = column.scrollViews.firstMatch
        let toolbar = app.descendants(matching: .any)["toolbarIPad"]
        let tabs = app.descendants(matching: .any)["Sections"]
        let parentFrame = try XCTUnwrap(parent.exists ? parent.frame : nil, "The actual column scroll view exists")
        let toolbarFrame = try XCTUnwrap(toolbar.exists ? toolbar.frame : nil, "The iPad toolbar exists")
        let tabsFrame = try XCTUnwrap(tabs.exists ? tabs.frame : nil, "The tab bar exists")
        let clipped = parentFrame.intersection(app.windows.firstMatch.frame)
        let top = max(clipped.minY, toolbarFrame.maxY)
        let bottom = min(clipped.maxY, tabsFrame.minY)
        let visible = CGRect(x: clipped.minX, y: top, width: clipped.width, height: max(bottom - top, 0))
        return try XCTUnwrap(!visible.isEmpty && visible.width >= 44 && visible.height >= 44 ? visible : nil,
                             "The actual column viewport fits a 44pt target")
    }

    /// Scroll with real finger drags in the measured leading margin of TX.
    /// Keep both endpoints clear of the border, pinned keys and viewport edges.
    @MainActor
    private func revealIPadTiming(_ timing: XCUIElement, in column: XCUIElement,
                                  app: XCUIApplication) throws {
        let parent = column.scrollViews.firstMatch
        let keys = ["txTune", "txMox", "txTwoTone", "txPsa", "txSettings"].map { app.buttons[$0] }
        func belowTXKeys(in viewport: CGRect) -> CGRect? {
            guard keys.allSatisfy({ $0.exists }), let keyBottom = keys.map({ $0.frame.maxY }).max() else { return nil }
            // Half a 44pt target also clears the header's padding below the key row.
            let top = max(viewport.minY, keyBottom + 22)
            let clear = CGRect(x: viewport.minX, y: top, width: viewport.width, height: max(viewport.maxY - top, 0))
            return !clear.isEmpty && clear.height >= 44 ? clear : nil
        }
        for _ in 0..<8 {
            let viewport = try iPadColumnViewport(app, column: column).insetBy(dx: 0, dy: 22)
            let clear = try XCTUnwrap(!viewport.isEmpty && viewport.height >= 44 ? viewport : nil,
                                     "The column has room for a finger drag clear of its edges")
            let belowKeys = belowTXKeys(in: clear)
            let frame = timing.exists ? timing.frame : .zero
            if let visible = belowKeys, timing.exists && timing.isHittable && visible.contains(frame) { break }
            let gestureArea = belowKeys ?? clear
            let requested = belowKeys != nil && timing.exists
                ? gestureArea.midY - frame.midY : -gestureArea.height * 0.6
            let movement = max(-gestureArea.height, min(gestureArea.height, requested))
            _ = try XCTUnwrap(abs(movement) > 1 ? movement : nil, "Timing requires a nonzero scroll to become hittable")
            // Read TX geometry after Settings is fully revealed, so lazy content can load.
            let childFrames = ["txMicGain", "txStageMeters", "txVoiceTiming"].compactMap { id -> CGRect? in
                let child = app.descendants(matching: .any)[id]
                guard child.exists else { return nil }
                let childFrame = child.frame
                return !childFrame.isEmpty && childFrame.width >= 44 && childFrame.height >= 44 ? childFrame : nil
            }
            let contentLeft = try XCTUnwrap(childFrames.map(\.minX).min(), "TX content supplies the measured leading margin")
            let parentFrame = parent.frame
            let gap = contentLeft - parentFrame.minX
            _ = try XCTUnwrap(gap > 2 ? gap : nil, "The leading margin clears the one-point column border")
            let gestureX = parentFrame.minX + gap / 2
            XCTAssertGreaterThan(gestureX, parentFrame.minX + 1)
            XCTAssertLessThan(gestureX, contentLeft - 1)
            XCTAssertTrue(gestureArea.minX < gestureX && gestureX < gestureArea.maxX)
            let start = parent.coordinate(withNormalizedOffset: .zero)
                .withOffset(CGVector(dx: gestureX - parentFrame.minX,
                                     dy: gestureArea.midY - movement / 2 - parentFrame.minY))
            let finish = start.withOffset(CGVector(dx: 0, dy: movement))
            start.press(forDuration: 0.1, thenDragTo: finish)
        }
        let viewport = try iPadColumnViewport(app, column: column).insetBy(dx: 0, dy: 22)
        let visible = try XCTUnwrap(belowTXKeys(in: viewport), "The TX key row has entered the viewport before tapping Timing")
        let frame = try XCTUnwrap(timing.exists ? timing.frame : nil, "Timing exists before tapping")
        XCTAssertGreaterThanOrEqual(frame.height, 44)
        XCTAssertGreaterThanOrEqual(frame.width, 44)
        XCTAssertTrue(visible.contains(frame), "The entire Timing target is below the pinned TX keys and inside the actual viewport")
    }

    /// Whole-frame visibility is measured against the actual drawer or the
    /// iPad's parent UIScrollView, clipped between toolbar and tab bar.
    @MainActor
    private func assertPinnedControls(_ app: XCUIApplication, container: XCUIElement, parentScroll: Bool,
                                      file: StaticString = #filePath, line: UInt = #line) {
        let parent = parentScroll ? container.scrollViews.firstMatch : container
        XCTAssertTrue(parent.exists, "The actual panel viewport exists", file: file, line: line)
        let toolbar = app.descendants(matching: .any)[parentScroll ? "toolbarIPad" : "toolbar"]
        let tabs = app.descendants(matching: .any)["Sections"]
        XCTAssertTrue(toolbar.exists && tabs.exists, "Visible viewport boundaries exist", file: file, line: line)
        let clipped = parent.frame.intersection(app.windows.firstMatch.frame)
        let top = max(clipped.minY, toolbar.frame.maxY)
        let bottom = min(clipped.maxY, tabs.frame.minY)
        let visible = CGRect(x: clipped.minX, y: top, width: clipped.width, height: max(bottom - top, 0))
        XCTAssertFalse(visible.isEmpty, file: file, line: line)
        for id in ["txTune", "txMox", "txTwoTone", "txPsa", "txSettings"] {
            let key = app.buttons[id]
            XCTAssertTrue(key.exists && key.isHittable, id, file: file, line: line)
            XCTAssertGreaterThanOrEqual(key.frame.height, 44, id, file: file, line: line)
            XCTAssertGreaterThanOrEqual(key.frame.width, 44, id, file: file, line: line)
            XCTAssertTrue(visible.insetBy(dx: -0.5, dy: -0.5).contains(key.frame),
                          "\(id) entire frame \(key.frame) must be inside visible viewport \(visible)", file: file, line: line)
            print("TX native whole-frame pin \(id): control=\(key.frame) visible=\(visible)")
        }
    }

    @MainActor
    private func save(_ name: String) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_CONNECT_SHOTS"], !directory.isEmpty else { return }
        let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
        try XCUIScreen.main.screenshot().pngRepresentation.write(to: url)
        print("Wrote \(url.path)")
    }
}
