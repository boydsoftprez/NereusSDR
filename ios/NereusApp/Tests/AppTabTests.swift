// NereusSDR for iOS: the tab bar's tabs and their order
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

@testable import NereusSDR
import Testing

@Suite("AppTab")
struct AppTabTests {
    @Test("five tabs: Panadapter, Modes, Tools, Radio, Setup")
    func order() {
        #expect(AppTab.allCases.map(\.title) == ["Panadapter", "Modes", "Tools", "Radio", "Setup"])
    }
}
