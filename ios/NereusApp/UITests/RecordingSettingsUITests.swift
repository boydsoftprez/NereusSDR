// NereusSDR for iOS: Setup > Recording settings in the running app at the phone's own accessibility text sizes, every word inside the screen
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

/// PR 808: JJ's iPhone, at the first accessibility text size, drew Setup >
/// Recording settings wider than the screen, cut at both edges. The hosted
/// shot tests raise SwiftUI's own text size only; this opens the page in the
/// app itself, with the phone-wide text size set at launch, not connected and
/// against the fake Core with a named disk, a long unnamed Windows path and
/// long values for every string the Core gives the page, and checks that
/// every element on it lies inside the screen. With `NEREUS_RECSET_SHOTS`
/// set, each screen is written there.
final class RecordingSettingsUITests: XCTestCase {
    override func setUp() {
        continueAfterFailure = true
    }

    @MainActor
    func testRecordingSettingsStaysInsideTheScreenAtAccessibilitySizes() throws {
        let ax = [("ax1", "UICTContentSizeCategoryAccessibilityM"), ("ax2", "UICTContentSizeCategoryAccessibilityL")]
        let states: [(name: String, disks: String?)] = [
            ("not-connected", nil),
            ("named-disk", #"[{"root":"C:\\","name":"","free":41000000000,"total":64000000000,"sd":false,"own":true},"#
                + #"{"root":"E:\\","name":"Samsung T7","free":812000000000,"total":1000000000000,"sd":false,"own":false}]"#),
            // A Windows Core's disk with no name, shown by its whole path.
            ("long-disk-path", #"[{"root":"C:\\","name":"","free":41000000000,"total":64000000000,"sd":false,"own":true},"#
                + #"{"root":"\\\\?\\Volume{3f0c9a2e-7b1d-4c55-9e7a-2b8d6f4a1c90}\\","name":"","free":812000000000,"#
                + #""total":1000000000000,"sd":false,"own":false}]"#),
            // Every string the Core gives the page long: an unbroken disk
            // name, and the chosen disk a long path missing from the list.
            ("long-everything", #"[{"root":"C:\\","name":"","free":41000000000,"total":64000000000,"sd":false,"own":true},"#
                + #"{"root":"E:\\","name":"WD_BLACK_SN850X_NVMe_2TB_External_USB_C_Enclosure_Recordings","#
                + #""free":9876543210987,"total":19876543210987,"sd":false,"own":false}]"#),
        ]
        let cases = states.flatMap { state in
            (state.name == "long-everything"
                ? [("large", "UICTContentSizeCategoryL"), ("xxxlarge", "UICTContentSizeCategoryXXXL")] + ax : ax)
                .map { (name: "\(state.name)-\($0.0)", category: $0.1, disks: state.disks) }
        }
        for (name, category, disks) in cases {
            let app = XCUIApplication()
            app.launchArguments = ["-NereusShowBand", "-UIPreferredContentSizeCategoryName", category]
            if let disks {
                app.launchArguments += ["-NereusFlagsOnBand"]
                app.launchEnvironment["NEREUS_UITEST_RECORDINGS"] = Self.document(disks: disks)
            }
            app.launchFresh()
            let setup = app.otherElements["Sections"].buttons["Setup"]
            XCTAssertTrue(setup.waitForExistence(timeout: 30))
            setup.tap()
            let row = app.buttons["Recording settings"]
            try show(row, in: app)
            row.tap()
            let title = app.staticTexts["Recording settings"].firstMatch
            XCTAssertTrue(title.waitForExistence(timeout: 10))
            RunLoop.current.run(until: Date().addingTimeInterval(1))
            try save("recording-settings-\(name)-top")
            checkInside(app, "\(name) top")
            for step in 1...6 {
                app.swipeUp()
                RunLoop.current.run(until: Date().addingTimeInterval(0.5))
                try save("recording-settings-\(name)-\(step)")
                checkInside(app, "\(name) after \(step) swipes")
            }
            app.terminate()
        }
    }

    /// The Core's recordings document (version 3), its disks `disks`, the
    /// second chosen.
    private static func document(disks: String) -> String {
        let everything = disks.contains("WD_BLACK")
        let chosen = disks.contains("Volume{") ? #"\\\\?\\Volume{3f0c9a2e-7b1d-4c55-9e7a-2b8d6f4a1c90}\\"#
            : everything ? #"\\\\?\\Volume{3f0c9a2e-7b1d-4c55-9e7a-2b8d6f4a1c90}\\Users\\Station\\Documents\\NereusSDR\\Recordings\\"#
            : #"E:\\"#
        let slots = (1...8).map { #"{"n":\#($0),"name":"","lengthMs":0,"empty":true}"# }.joined(separator: ",")
        return #"{"version":3,"slices":[{"slice":0,"state":"idle","startUtc":"","startedBy":"","elapsedMs":0,"#
            + #""myVoice":false}],"kept":{"seq":0,"slice":-1,"id":"","name":"","lengthMs":0,"reason":"","device":""},"#
            + (everything
               ? #""index":{"revision":3,"count":123456,"usedBytes":987654321098765,"freeBytes":9876543210987654,"#
                 + #""folder":"C:\\Users\\Station\\Documents\\NereusSDR\\Recordings"},"#
                 + #""settings":{"keepMyVoice":true,"sidesApart":true,"autoStart":true,"autoStop":true,"quietMinutes":60,"#
               : #""index":{"revision":3,"count":4,"usedBytes":1427360000,"freeBytes":41000000000,"folder":"/r"},"#
                 + #""settings":{"keepMyVoice":true,"sidesApart":false,"autoStart":false,"autoStop":true,"quietMinutes":2,"#)
            + #""format":"\#(everything ? "wav" : "mp3")","disk":"\#(chosen)","disks":\#(disks)},"#
            + #""voiceKeyer":{"slots":[\#(slots)],"repeat":false,"repeatDelaySec":5,"sending":null}}"#
    }

    /// Every element on screen lies inside the screen's width, cut at neither edge.
    @MainActor
    private func checkInside(_ app: XCUIApplication, _ moment: String) {
        guard let snapshot = try? app.snapshot() else {
            XCTFail("\(moment): no snapshot")
            return
        }
        let width = app.windows.firstMatch.frame.width
        var outside: [String] = []
        func walk(_ element: XCUIElementSnapshot) {
            let frame = element.frame
            if !frame.isEmpty, frame.width > 0, frame.minX < -0.5 || frame.maxX > width + 0.5 {
                let words = element.identifier.isEmpty ? element.label : element.identifier
                outside.append("\(element.elementType.rawValue) '\(words.prefix(60))' \(Int(frame.minX))..\(Int(frame.maxX))")
            }
            element.children.forEach(walk)
        }
        walk(snapshot)
        XCTAssertTrue(outside.isEmpty, "\(moment): outside a screen \(Int(width)) wide: "
                      + outside.prefix(40).joined(separator: "; "))
    }

    @MainActor
    private func show(_ element: XCUIElement, in app: XCUIApplication) throws {
        for _ in 0..<10 {
            if element.exists && element.isHittable { return }
            app.swipeUp()
        }
        XCTFail("Could not scroll to \(element.identifier)")
    }

    @MainActor
    private func save(_ name: String) throws {
        guard let directory = ProcessInfo.processInfo.environment["NEREUS_RECSET_SHOTS"], !directory.isEmpty else {
            return
        }
        let url = URL(fileURLWithPath: directory).appendingPathComponent("\(name).png")
        try XCUIScreen.main.screenshot().pngRepresentation.write(to: url)
    }
}
