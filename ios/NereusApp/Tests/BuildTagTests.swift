// NereusSDR for iOS: the test build's name, read from the Info.plist
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
@testable import NereusSDR
import Testing

@Suite("BuildTag")
struct BuildTagTests {
    @Test("a tag in the Info.plist is the build's name")
    func present() {
        #expect(BuildTag.tag(in: ["NereusBuildTag": "claude/iphone-app@1a2b3c4d"]) == "claude/iphone-app@1a2b3c4d")
        #expect(BuildTag.label(for: "claude/iphone-app@1a2b3c4d") == "Build claude/iphone-app@1a2b3c4d")
    }

    @Test("an empty, blank or absent value means no name")
    func absent() {
        #expect(BuildTag.tag(in: ["NereusBuildTag": ""]) == nil)
        #expect(BuildTag.tag(in: ["NereusBuildTag": " "]) == nil)
        #expect(BuildTag.tag(in: ["NereusBuildTag": 7]) == nil)
        #expect(BuildTag.tag(in: [:]) == nil)
        #expect(BuildTag.tag(in: nil) == nil)
    }

    @Test("the app's Info.plist never carries the unexpanded build setting")
    func expanded() throws {
        let info = try #require(Bundle.main.infoDictionary)
        let raw = try #require(info[BuildTag.infoKey] as? String, "the app's Info.plist has no NereusBuildTag")
        #expect(!raw.contains("$("), "NereusBuildTag was not expanded: \(raw)")
        #expect(BuildTag.current == BuildTag.tag(in: info))
    }
}
