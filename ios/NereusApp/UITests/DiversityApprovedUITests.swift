// NereusSDR for iOS: Diversity public native routes, actual touch areas and viewport proof
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
import XCTest
import UIKit

final class DiversityApprovedUITests: XCTestCase {
    // CGRect subtraction after rotation can differ by a handful of binary ulps.
    // This admits only arithmetic roundoff, never a meaningfully smaller target.
    private static let minimumTarget = CGFloat(44) - 16 * CGFloat(44).ulp
    override func setUp() { continueAfterFailure = false }
    override func tearDown() { XCUIDevice.shared.orientation = .portrait }

    @MainActor
    func testFullAndFoldedLiveBadgeExactPageAndFingerEdges() throws {
        for light in [false, true] {
            for full in [false, true] {
                for sideways in [false, true] {
                    let extra = full ? ["-NereusDiversityFullFlag"] + (sideways ? ["-NereusDiversitySeparated"] : []) : []
                    let app = launch(light: light, extra: extra)
                    XCUIDevice.shared.orientation = sideways ? .landscapeLeft : .portrait
                    let badge = app.buttons["flagDivB"]
                    XCTAssertTrue(badge.waitForExistence(timeout: 10))
                    XCTAssertEqual(app.buttons.matching(NSPredicate(format: "identifier BEGINSWITH 'flagDiv'")).count, 1)
                    XCTAssertGreaterThanOrEqual(badge.frame.width, Self.minimumTarget)
                    XCTAssertGreaterThanOrEqual(badge.frame.height, Self.minimumTarget)
                    let flag = app.otherElements[full ? "flagB" : "foldedFlagB"]
                    XCTAssertTrue(flag.exists)
                    try contains(flag.frame, in: app.frame)
                    try contains(badge.frame, in: flag.frame)
                    let tag = "\(full ? "full" : "folded")-\(light ? "light" : "dark")-\(sideways ? "landscape" : "portrait")"
                    try save("band-" + tag)
                    // Tap near the boundary, inside the real 44pt target, not only its central visual.
                    badge.coordinate(withNormalizedOffset: CGVector(dx: 0, dy: 0)).withOffset(CGVector(dx: 2, dy: 2)).tap()
                    let heading = sliceHeading(app)
                    XCTAssertTrue(heading.waitForExistence(timeout: 10))
                    XCTAssertTrue(heading.label.contains("B"))
                    try save("badge-page-" + tag)
                    app.buttons["Done"].tap()
                    XCTAssertTrue(badge.waitForExistence(timeout: 10))
                    // A folded DIV tap must not activate B or tune either visible slice.
                    XCTAssertTrue(app.otherElements[full && !sideways ? "flagB" : "flagA"].exists)
                    if !full { XCTAssertTrue(app.otherElements["foldedFlagB"].exists) }
                    if full && sideways { XCTAssertTrue(app.otherElements["flagB"].exists) }
                    if !full { XCTAssertEqual(app.buttons["flagFrequencyA"].value as? String, "7.236.400") }
                    app.terminate()
                }
            }
        }
    }

    @MainActor
    func testToolsSharesLiveOwnerAndCoordinatedUseOffRoutes() throws {
        let app = launch(light: false, extra: ["-NereusDiversityActiveC"])
        let badge = app.buttons["flagDivB"]
        XCTAssertTrue(badge.waitForExistence(timeout: 10))
        badge.tap()
        XCTAssertTrue(sliceHeading(app).waitForExistence(timeout: 10))
        XCTAssertTrue(sliceHeading(app).label.contains("B"))
        app.buttons["Done"].tap()
        app.otherElements["Sections"].buttons["Tools"].tap()
        app.buttons["tools.diversity"].tap()
        XCTAssertTrue(sliceHeading(app).waitForExistence(timeout: 10))
        XCTAssertTrue(sliceHeading(app).label.contains("B"))
        app.buttons["diversity.use"].tap()
        XCTAssertTrue(app.staticTexts["diversity.note"].waitForExistence(timeout: 10))
        XCTAssertEqual(app.staticTexts["diversity.note"].label, "Diversity moved from B to C. Both slices paused briefly.")
        XCTAssertTrue(sliceHeading(app).label.contains("C"))
        try save("tools-after-use-C")
        app.buttons["diversity.enabled"].tap()
        let off = NSPredicate(format: "value == 'Off'")
        expectation(for: off, evaluatedWith: app.buttons["diversity.enabled"])
        waitForExpectations(timeout: 10)
        XCTAssertTrue(sliceHeading(app).label.contains("C"))
        try save("tools-off-active-C")
        app.otherElements["Sections"].buttons["Panadapter"].tap()
        XCTAssertTrue(badge.waitForNonExistence(timeout: 10))
        XCTAssertEqual(app.buttons.matching(NSPredicate(format: "identifier BEGINSWITH 'flagDiv'")).count, 0)
    }

    @MainActor
    func testListeningOwnerBadgeIsReadableAndActionsAreDisabled() throws {
        let app = launch(light: false, extra: ["-NereusDiversityListening"])
        let badge = app.buttons["flagDivB"]
        XCTAssertTrue(badge.waitForExistence(timeout: 10))
        badge.tap()
        XCTAssertTrue(sliceHeading(app).waitForExistence(timeout: 10))
        XCTAssertTrue(sliceHeading(app).label.contains("B"))
        XCTAssertFalse(app.buttons["diversity.enabled"].isEnabled)
        XCTAssertFalse(app.buttons["diversity.use"].isEnabled)
        XCTAssertTrue(app.staticTexts["diversity.reason"].exists)
        try save("listening-B-page")
    }

    @MainActor
    func testLargeTypeMoreSwitchAndPageContainment() throws {
        for light in [false, true] {
            let app = launch(light: light, extra: ["-NereusDiversityFullFlag", "-UIPreferredContentSizeCategoryName", "UICTContentSizeCategoryAccessibilityM"])
            XCTAssertTrue(app.buttons["flagMoreB"].waitForExistence(timeout: 10))
            app.buttons["flagMoreB"].tap()
            let toggle = app.buttons["flagMoreDiversityEnabled"]
            XCTAssertTrue(toggle.waitForExistence(timeout: 10))
            XCTAssertGreaterThanOrEqual(toggle.frame.height, Self.minimumTarget)
            try contains(toggle.frame, in: app.frame)
            try save("more-large-" + (light ? "light" : "dark"))
            app.buttons["flagMoreDiversity"].tap()
            XCTAssertTrue(sliceHeading(app).waitForExistence(timeout: 10))
            try contains(app.buttons["diversity.use"].frame, in: app.frame)
            try contains(app.buttons["diversity.enabled"].frame, in: app.frame)
            try save("page-large-" + (light ? "light" : "dark"))
            app.terminate()
        }
    }

    @MainActor
    func testLargeFiniteReadonlyCoreFrequencyIsReadableWithoutAJoinedObject() throws {
        let app = launch(light: true, extra: ["-NereusDiversityHugeReadonly"])
        app.otherElements["Sections"].buttons["Tools"].tap()
        app.buttons["tools.diversity"].tap()
        let heading = sliceHeading(app)
        XCTAssertTrue(heading.waitForExistence(timeout: 10))
        XCTAssertTrue(heading.label.contains("E"))
        XCTAssertFalse(app.buttons["diversity.enabled"].isEnabled)
        try contains(heading.frame, in: app.frame)
        try save("readonly-E-large-finite-frequency")
    }

    @MainActor
    func testEnabledMemoryStoreChangeRecallUses44PointFingerTargets() throws {
        try memoryStoreChangeRecall(nativeFinger: false)
    }

    @MainActor
    func testNativeFingerMemoryStoreChangeRecallUses44PointFingerTargets() throws {
        try memoryStoreChangeRecall(nativeFinger: true)
    }

    @MainActor
    private func memoryStoreChangeRecall(nativeFinger: Bool) throws {
        let app = launch(light: false)
        let badge = app.buttons["flagDivB"]
        XCTAssertTrue(badge.waitForExistence(timeout: 10))
        badge.tap()
        XCTAssertTrue(sliceHeading(app).waitForExistence(timeout: 10))
        XCTAssertTrue(sliceHeading(app).label.contains("B"))
        let page = app.scrollViews.containing(.button, identifier: "diversity.store").element
        let store = app.buttons["diversity.store"]
        let memory = app.buttons["diversity.memory0"]
        let receipts = app.otherElements["diversity.fixture.blendReceipts"]
        let lifecycle = app.otherElements["diversity.fixture.sliderLifecycle"]
        addTeardownBlock { @MainActor in
            print("DIVERSITY_LIFECYCLE final=\(lifecycle.value ?? "missing") receipts=\(receipts.value ?? "missing") phaseUI=\(app.sliders["diversity.phase"].value ?? "missing") gainUI=\(app.sliders["diversity.gain"].value ?? "missing")")
        }
        XCTAssertTrue(receipts.waitForExistence(timeout: 10))
        XCTAssertEqual(receipts.value as? String, "phaseWrites=0;gainWrites=0;phase=123.4;gain=-2.0")
        try reveal(store, in: page)
        XCTAssertTrue(store.isEnabled)
        try fingerTarget(store, in: page)
        // The compact Store visual is 30pt; this tap proves its surrounding 44pt target.
        store.coordinate(withNormalizedOffset: .zero).withOffset(CGVector(dx: 2, dy: 2)).tap()
        expectation(for: NSPredicate(format: "value == 'On'"), evaluatedWith: store)
        waitForExpectations(timeout: 10)
        for slot in 0..<8 {
            let button = app.buttons["diversity.memory\(slot)"]
            try reveal(button, in: page)
            XCTAssertTrue(button.isEnabled)
            try fingerTarget(button, in: page)
        }
        try reveal(memory, in: page)
        memory.coordinate(withNormalizedOffset: .zero).withOffset(CGVector(dx: 2, dy: 2)).tap()
        expectation(for: NSPredicate(format: "value == 'Off'"), evaluatedWith: store)
        waitForExpectations(timeout: 10)
        XCTAssertEqual(memory.value as? String, "123.4°, −2.0 dB")
        XCTAssertTrue(memory.isEnabled)
        XCTAssertFalse(app.buttons["diversity.memory1"].isEnabled)
        // Store is local to this ephemeral phone; it sends no blend write.
        XCTAssertEqual(receipts.value as? String, "phaseWrites=0;gainWrites=0;phase=123.4;gain=-2.0")
        try save((nativeFinger ? "finger-" : "") + "memory-stored-local")

        let phase = app.sliders["diversity.phase"]
        let gain = app.sliders["diversity.gain"]
        try reveal(phase, in: page, upward: false)
        if nativeFinger {
            phase.coordinate(withNormalizedOffset: CGVector(dx: 0.35, dy: 0.5))
                .press(forDuration: 0.1, thenDragTo: phase.coordinate(withNormalizedOffset: CGVector(dx: 0.75, dy: 0.5)))
        } else { phase.adjust(toNormalizedSliderPosition: 0.75) }
        expectation(for: NSPredicate(format: "value MATCHES %@",
            "phaseWrites=[1-9][0-9]*;gainWrites=0;phase=.*;gain=.*"), evaluatedWith: receipts)
        waitForExpectations(timeout: 10)
        try reveal(gain, in: page, upward: false)
        if nativeFinger {
            gain.coordinate(withNormalizedOffset: CGVector(dx: 0.45, dy: 0.5))
                .press(forDuration: 0.1, thenDragTo: gain.coordinate(withNormalizedOffset: CGVector(dx: 0.25, dy: 0.5)))
        } else { gain.adjust(toNormalizedSliderPosition: 0.25) }
        expectation(for: NSPredicate(format: "value MATCHES %@",
            "phaseWrites=[1-9][0-9]*;gainWrites=[1-9][0-9]*;phase=.*;gain=.*"), evaluatedWith: receipts)
        waitForExpectations(timeout: 10)
        print("DIVERSITY_LIFECYCLE changed=\(lifecycle.value ?? "missing")")
        if nativeFinger {
            let events = String(describing: lifecycle.value ?? "missing").components(separatedBy: ";")
            for slider in ["diversity.phase", "diversity.gain"] {
                let begins = events.indices.filter { events[$0].contains(":\(slider).physical.begin:") }
                let ends = events.indices.filter { events[$0].contains(":\(slider).physical.end:") }
                let releases = events.indices.filter { events[$0].contains(":\(slider).release.accepted:") }
                XCTAssertEqual(begins.count, 1, "One pointer-down owns one editing lifetime")
                XCTAssertEqual(ends.count, 1, "Only UIKit's physical terminal event ends this drag")
                XCTAssertEqual(releases.count, 1, "Primary callbacks cannot flush each moving value")
                XCTAssertFalse(events.contains { $0.contains(":\(slider).physical.cancelled:") })
                if let begin = begins.first, let end = ends.first, let release = releases.first {
                    XCTAssertLessThan(begin, end)
                    XCTAssertLessThan(end, release)
                }
            }
        }
        let changed = try confirmedBlend(receipts)
        XCTAssertNotEqual(changed.phase, 123.4)
        XCTAssertNotEqual(changed.gain, -2)
        try save((nativeFinger ? "finger-" : "") + "memory-before-recall-synthetic-confirmed")

        try reveal(memory, in: page)
        XCTAssertTrue(memory.isEnabled)
        try fingerTarget(memory, in: page)
        memory.coordinate(withNormalizedOffset: CGVector(dx: 1, dy: 1))
            .withOffset(CGVector(dx: -2, dy: -2)).tap()
        expectation(for: NSPredicate(format: "value ENDSWITH 'phase=123.4;gain=-2.0'"), evaluatedWith: receipts)
        waitForExpectations(timeout: 10)
        print("DIVERSITY_LIFECYCLE recalled=\(lifecycle.value ?? "missing")")
        let recalled = try confirmedBlend(receipts)
        XCTAssertGreaterThan(recalled.phaseWrites, changed.phaseWrites)
        XCTAssertGreaterThan(recalled.gainWrites, changed.gainWrites)
        XCTAssertEqual(recalled.phase, 123.4)
        XCTAssertEqual(recalled.gain, -2)
        XCTAssertEqual(memory.value as? String, "123.4°, −2.0 dB")
        try reveal(phase, in: page, upward: false)
        expectation(for: NSPredicate(format: "value == '123.4°'"), evaluatedWith: phase)
        waitForExpectations(timeout: 10)
        expectation(for: NSPredicate(format: "value == '−2.0 dB'"), evaluatedWith: gain)
        waitForExpectations(timeout: 10)
        try save((nativeFinger ? "finger-" : "") + "memory-recalled-synthetic-confirmed")
        // This proves correlated reception by our no-network synthetic Core,
        // not a physical radio or persistence across a subsequent app launch.
    }

    @MainActor
    private func fingerTarget(_ button: XCUIElement, in page: XCUIElement) throws {
        XCTAssertGreaterThanOrEqual(button.frame.width, Self.minimumTarget)
        XCTAssertGreaterThanOrEqual(button.frame.height, Self.minimumTarget)
        try contains(button.frame, in: page.frame)
    }

    @MainActor
    private func reveal(_ element: XCUIElement, in page: XCUIElement, upward: Bool = true) throws {
        // Lazy memory cells may be absent from accessibility until scrolled into view.
        for _ in 0..<5 {
            if element.exists && element.isHittable && page.frame.contains(element.frame) { return }
            if upward { page.swipeUp() } else { page.swipeDown() }
        }
        XCTAssertTrue(element.waitForExistence(timeout: 10))
        XCTAssertTrue(element.isHittable)
        try contains(element.frame, in: page.frame)
    }

    @MainActor
    private func confirmedBlend(_ probe: XCUIElement) throws -> (phaseWrites: Int, gainWrites: Int, phase: Double, gain: Double) {
        let text = try XCTUnwrap(probe.value as? String)
        let fields = Dictionary(uniqueKeysWithValues: text.split(separator: ";").map { field in
            let pair = field.split(separator: "=", maxSplits: 1)
            return (String(pair[0]), pair.count == 2 ? String(pair[1]) : "")
        })
        return (try XCTUnwrap(fields["phaseWrites"].flatMap(Int.init)),
                try XCTUnwrap(fields["gainWrites"].flatMap(Int.init)),
                try XCTUnwrap(fields["phase"].flatMap(Double.init)),
                try XCTUnwrap(fields["gain"].flatMap(Double.init)))
    }

    @MainActor
    private func sliceHeading(_ app: XCUIApplication) -> XCUIElement {
        app.staticTexts.matching(identifier: "diversity.slice")
            .matching(NSPredicate(format: "label BEGINSWITH 'SLICE '")).element
    }

    @MainActor
    private func launch(light: Bool, extra: [String] = []) -> XCUIApplication {
        XCUIDevice.shared.orientation = .portrait
        let app = XCUIApplication()
        app.launchArguments = ["-NereusDiversityFixture", "-NereusDiversityEnvironmentProbe"] + extra
        if light { app.launchArguments.append("-NereusDiversityLightValidation") }
        app.launch()
        let probe = app.otherElements["diversity.fixture.scheme"]
        XCTAssertTrue(probe.waitForExistence(timeout: 10))
        XCTAssertEqual(probe.value as? String, light ? "light" : "dark") // Actual SwiftUI environment, not launch intent.
        return app
    }

    private func contains(_ rect: CGRect, in viewport: CGRect, file: StaticString = #filePath, line: UInt = #line) throws {
        XCTAssertFalse(rect.isEmpty, file: file, line: line)
        XCTAssertGreaterThanOrEqual(rect.minX, viewport.minX - 0.5, file: file, line: line)
        XCTAssertGreaterThanOrEqual(rect.minY, viewport.minY - 0.5, file: file, line: line)
        XCTAssertLessThanOrEqual(rect.maxX, viewport.maxX + 0.5, file: file, line: line)
        XCTAssertLessThanOrEqual(rect.maxY, viewport.maxY + 0.5, file: file, line: line)
    }

    @MainActor
    private func save(_ name: String) throws {
        let shot = XCUIScreen.main.screenshot()
        let attachment = XCTAttachment(screenshot: shot)
        attachment.name = name
        attachment.lifetime = .keepAlways
        add(attachment)
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_DIVERSITY_SHOTS"] else { return }
        let idiom = UIDevice.current.userInterfaceIdiom == .pad ? "ipad" : "iphone"
        let url = URL(fileURLWithPath: directory, isDirectory: true).appendingPathComponent("diversity-\(idiom)-\(name).png")
        try shot.pngRepresentation.write(to: url)
    }
}
