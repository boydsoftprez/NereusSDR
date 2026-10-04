// NereusSDR for iOS: the Core's question appears over whatever screen shows and is answered there with a tap
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import XCTest

// UI tests drive the running app through XCUIApplication, which only
// XCTest provides; swift-testing has no UI testing.
/// D85, from TestFlight build 5: the Core asks "This changes what the
/// MacBook hears" while the phone is on the Tuner Genius page, and the
/// question must appear there, not only on the band. With no Core, a debug
/// build hands itself the Core's question each time this test posts
/// ``signal`` (the app's `UITestQuestion`), and the test taps the answers
/// where the question appears: over the Tuner Genius page, the TX panel,
/// Modes and Setup, and across a tab change.
final class ConfirmWhereTappedUITests: XCTestCase {
    static let signal = "com.boydsoftprez.NereusSDR.uitest.ask"

    override func setUp() {
        continueAfterFailure = false
    }

    @MainActor
    func testAnsweredOverTheTunerGeniusPage() {
        let app = launch()
        tab(app, "Radio")
        let row = app.buttons["accessory.tgxl"]
        XCTAssertTrue(row.waitForExistence(timeout: 10))
        row.tap()
        XCTAssertTrue(app.descendants(matching: .any)["tunerGeniusPage"].waitForExistence(timeout: 10))
        ask()
        answerWithSet(app)
    }

    @MainActor
    func testAnsweredOverTheTxPanel() {
        let app = launch()
        let button = app.buttons["txPanel"]
        XCTAssertTrue(button.waitForExistence(timeout: 30))
        button.tap()
        XCTAssertTrue(app.otherElements["txPanelDrawer"].waitForExistence(timeout: 10))
        ask()
        answerWithCancel(app)
        // The panel it was asked over is still there.
        XCTAssertTrue(app.otherElements["txPanelDrawer"].exists)
    }

    @MainActor
    func testAnsweredOverModes() {
        let app = launch()
        tab(app, "Modes")
        XCTAssertTrue(app.descendants(matching: .any)["modesPage"].waitForExistence(timeout: 10))
        ask()
        answerWithSet(app)
    }

    @MainActor
    func testAnsweredOverSetup() {
        let app = launch()
        tab(app, "Setup")
        XCTAssertTrue(app.descendants(matching: .any)["setupNote"].waitForExistence(timeout: 10))
        ask()
        answerWithCancel(app)
    }

    @MainActor
    func testATabChangeLeavesTheQuestionUp() {
        let app = launch()
        tab(app, "Setup")
        ask()
        let go = app.buttons["Set ANT1"]
        XCTAssertTrue(go.waitForExistence(timeout: 10))
        // The tab bar stays in reach under the question, and the question stays.
        for name in ["Modes", "Radio", "Tools", "Panadapter"] {
            tab(app, name)
            XCTAssertTrue(app.otherElements["Sections"].buttons[name].isSelected, "\(name) was not chosen")
            XCTAssertTrue(go.exists, "the question went with the change to \(name)")
            XCTAssertTrue(go.isHittable, "the question cannot be reached over \(name)")
            XCTAssertEqual(app.buttons.matching(NSPredicate(format: "label == %@", "Set ANT1")).count, 1)
        }
        answerWithCancel(app)
    }

    // MARK: Inside

    @MainActor
    private func launch() -> XCUIApplication {
        let app = XCUIApplication()
        // The band and its tabs with no Core, taking the question on the signal
        // (a debug build's launch arguments).
        app.launchArguments = ["-NereusShowBand", "-NereusAskOnSignal"]
        app.launch()
        XCTAssertTrue(app.otherElements["Sections"].waitForExistence(timeout: 30))
        return app
    }

    @MainActor
    private func tab(_ app: XCUIApplication, _ name: String) {
        let button = app.otherElements["Sections"].buttons[name]
        XCTAssertTrue(button.waitForExistence(timeout: 10))
        button.tap()
    }

    /// The app hands itself the Core's question.
    private func ask() {
        CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(),
                                             CFNotificationName(Self.signal as CFString), nil, nil, true)
    }

    /// The question is up, once, in the Core's words, with both answers in reach.
    @MainActor
    private func expectQuestion(_ app: XCUIApplication) -> (go: XCUIElement, cancel: XCUIElement) {
        // SwiftUI exposes the sheet's identifier on its children, including
        // this Cancel. The dimmed screen behind is a separate Cancel.
        let go = app.buttons["Set ANT1"]
        let cancel = app.buttons.matching(NSPredicate(format: "label == %@ AND identifier == %@", "Cancel",
                                                      "sharedChangeSheet")).firstMatch
        XCTAssertTrue(go.waitForExistence(timeout: 10), "the question did not appear")
        XCTAssertTrue(app.descendants(matching: .any)
            .matching(NSPredicate(format: "label == %@", "This changes what the MacBook hears"))
            .firstMatch.exists)
        XCTAssertTrue(go.isHittable)
        XCTAssertTrue(cancel.isHittable)
        XCTAssertEqual(app.buttons.matching(NSPredicate(format: "label == %@", "Set ANT1")).count, 1)
        return (go, cancel)
    }

    /// Set ANT1, tapped where the question appeared. With no Core to answer,
    /// the phone says so and Close ends it.
    @MainActor
    private func answerWithSet(_ app: XCUIApplication) {
        let question = expectQuestion(app)
        question.go.tap()
        let close = app.buttons["Close"]
        XCTAssertTrue(close.waitForExistence(timeout: 20), "Set ANT1 did not reach the question")
        close.tap()
        XCTAssertTrue(question.go.waitForNonExistence(timeout: 10))
    }

    /// Cancel, tapped where the question appeared: it closes at once.
    @MainActor
    private func answerWithCancel(_ app: XCUIApplication) {
        let question = expectQuestion(app)
        question.cancel.tap()
        XCTAssertTrue(question.go.waitForNonExistence(timeout: 10), "Cancel did not reach the question")
    }
}
