// NereusSDR for iOS: the Core's logging categories with their labels, read from radio.logCategoryList
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMirror

/// R-IOS-36, D95 (link document section 7.1, `radio`'s `logCategoryList`,
/// `logCategoryListVersion` 1): the Core's categories with their Support
/// dialog labels, in the order sent; unknown keys are ignored; an entry
/// without an id is skipped, one without a label is named by its id.
@Suite struct LogCategoryListTests {
    /// The list the log-category-list session's Core sends with its refusal of a write.
    static func fixtureList() throws -> String {
        for message in try FixtureReplay.stationMessages("session-log-category-list") {
            if case .propertyResult(let result) = message, result.key == "radio",
               let entry = result.results.first(where: { $0.property == LogCategoryList.propertyName }),
               case .utf8(let json)? = entry.value?.value {
                return json
            }
        }
        Issue.record("the session sends no list")
        return ""
    }

    @Test("the session's list reads as sent: twelve categories in the Support dialog's order")
    func fixture() throws {
        let list = try #require(LogCategoryList(json: try Self.fixtureList()))
        #expect(list.categories.map(\.label) == ["Discovery", "Connection", "Protocol", "Receiver", "Audio", "DSP",
                                                 "Spectrum", "Container", "Meter", "MMIO", "TCI", "Spots"])
        #expect(list.categories.first?.id == "nereus.discovery")
        #expect(list.categories.last?.id == "nereus.spots")
    }

    @Test("unknown keys are ignored, an entry without an id is skipped, one without a label shows its id")
    func tolerant() throws {
        let json = "{\"future\":1,\"categories\":[{\"id\":\"a.one\",\"label\":\"One\",\"extra\":true},"
            + "{\"label\":\"No id\"},{\"id\":\"\",\"label\":\"Empty id\"},{\"id\":\"b.two\"},\"odd\","
            + "{\"id\":\"c.three\",\"label\":\"\"}]}"
        let list = try #require(LogCategoryList(json: json))
        #expect(list.categories == [LogCategoryList.Category(id: "a.one", label: "One"),
                                    LogCategoryList.Category(id: "b.two", label: "b.two"),
                                    LogCategoryList.Category(id: "c.three", label: "c.three")])
    }

    @Test("nothing sent reads as nil; a value that is not the list's shape reads as nil")
    func unreadable() {
        #expect(LogCategoryList(json: "") == nil)
        for json in ["not json", "[1]", "{}", "{\"categories\":\"a\"}"] {
            #expect(LogCategoryList(json: json) == nil, "\(json)")
        }
        #expect(LogCategoryList(json: "{\"categories\":[]}")?.categories == [])
    }

    @Test("the gate: the hello feature, the capability and the property")
    func gate() {
        #expect(LogCategoryList.featureName == "logCategoryList")
        #expect(LogCategoryList.capabilityName == "logCategoryListVersion")
        #expect(LogCategoryList.propertyName == "logCategoryList")
    }
}
