// NereusSDR for iOS: the UI tests' launch, with no other copy of the app running, failing at once when it shows Welcome where the test asked for the band
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import XCTest

extension XCUIApplication {
    /// Launches the app with the test's arguments once any copy of it still
    /// running has ended: one launched without them (ios/scripts/sim-ready.sh's
    /// warm launch) or one left by the test before.
    ///
    /// With `-NereusShowBand` among the arguments, the app draws the band
    /// from its first frame. A launch that shows Welcome instead was made by a
    /// process that never had the test's arguments (the PR 653 re-review e,
    /// I1), so the test fails here and says so, rather than wait out its own
    /// look for the band and read as a layout failure. It is not launched
    /// again: a repeat must be seen, not hidden. The app logs each process's
    /// arguments as it starts (`NereusSDRApp.logLaunch`), which tells the two
    /// possible causes apart.
    @MainActor
    func launchFresh(file: StaticString = #filePath, line: UInt = #line) {
        if state != .notRunning {
            terminate()
        }
        launch()
        guard launchArguments.contains("-NereusShowBand") else {
            return
        }
        let welcome = [descendants(matching: .any)["findMyCore"], descendants(matching: .any)["welcomeAbout"]]
        let band = [descendants(matching: .any)["bandSurface"], buttons["ptt"]]
        let deadline = Date().addingTimeInterval(15)
        while Date() < deadline {
            if welcome.contains(where: { $0.exists }) {
                XCTFail("launched on Welcome: launch arguments not applied (asked for "
                        + launchArguments.joined(separator: " ") + ")", file: file, line: line)
                return
            }
            if band.contains(where: { $0.exists }) {
                return
            }
            RunLoop.current.run(until: Date().addingTimeInterval(0.2))
        }
    }
}
