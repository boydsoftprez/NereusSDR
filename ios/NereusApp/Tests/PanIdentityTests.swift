// NereusSDR for iOS: tests for the pan identity and the radio's pan and slice limits
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMirror
@testable import NereusSDR
import Testing

@Suite("Pan identity and limits")
struct PanIdentityTests {
    private func capabilities(slices: Int64?, receivers: Int64?) -> [String: MirrorValue] {
        var values: [String: MirrorValue] = ["remoteTxVersion": .int(1)]
        if let slices { values["effectiveMaxSlices"] = .int(slices) }
        if let receivers { values["userDdcCount"] = .int(receivers) }
        return values
    }

    /// The capabilities the link suite's default session sends, read from the checkout.
    private func connectableCapabilities() throws -> [String: MirrorValue] {
        let source = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../../../tests/data/link/v1/sessions/connect-connectable.json").standardizedFileURL
        let root = try #require(try JSONSerialization.jsonObject(with: Data(contentsOf: source)) as? [String: Any])
        let steps = try #require(root["steps"] as? [[String: Any]])
        let message = try #require(steps.compactMap { $0["message"] as? [String: Any] }
            .first { $0["type"] as? String == "capabilities" })
        var values: [String: MirrorValue] = [:]
        for property in try #require(message["properties"] as? [[String: Any]]) {
            if property["kind"] as? String == "i64", let name = property["name"] as? String,
               let value = property["value"] as? Int {
                values[name] = .int(Int64(value))
            }
        }
        return values
    }

    @Test("a pan's number, store key, Core key and title")
    func identity() {
        #expect(PanIdentity.first.number == 1)
        #expect(PanIdentity.first.storeKey == "1")
        #expect(PanIdentity.first.corePanKey == "pan-0")
        #expect(PanIdentity.first.title == "Pan 1")
        #expect(PanIdentity(number: 3).corePanKey == "pan-2")
        #expect(PanIdentity(corePanKey: "pan-0") == .first)
        #expect(PanIdentity(corePanKey: "pan-1")?.number == 2)
        #expect(PanIdentity(corePanKey: "x") == nil)
        #expect(PanIdentity(corePanKey: "pan-") == nil)
        #expect(PanIdentity(corePanKey: "pan--1") == nil)
        #expect(PanIdentity(corePanKey: "phone:pan-7") == nil)
        #expect(PanIdentity(number: 1) < PanIdentity(number: 2))
    }

    @Test("the pan limit is the fewer of the radio's slices and receivers")
    func panCountByBoard() throws {
        // kSaturn, ANAN-G2: src/core/BoardCapabilities.cpp:1158-1159 (5 slices, 5 user DDCs).
        #expect(PanLimits.panCount(capabilities: capabilities(slices: 5, receivers: 5)) == 5)
        // kHermesC10, ANAN-G2E: src/core/BoardCapabilities.cpp:810, :825 (5 and 4).
        #expect(PanLimits.panCount(capabilities: capabilities(slices: 5, receivers: 4)) == 4)
        // kHermesLite, Hermes Lite 2: src/core/BoardCapabilities.cpp:950-951 (5 and 2).
        #expect(PanLimits.panCount(capabilities: capabilities(slices: 5, receivers: 2)) == 2)
        // kAtlas: src/core/BoardCapabilities.cpp:349-350 (3 and 3).
        #expect(PanLimits.panCount(capabilities: capabilities(slices: 3, receivers: 3)) == 3)
        // The suite's default session: 5 and 2.
        let connectable = try connectableCapabilities()
        #expect(connectable["effectiveMaxSlices"] == .int(5) && connectable["userDdcCount"] == .int(2))
        #expect(PanLimits.panCount(capabilities: connectable) == 2)
        #expect(PanLimits.sliceCount(capabilities: connectable) == 5)
    }

    @Test("a missing count is the other's, and none at all is one pan")
    func panCountMissing() {
        #expect(PanLimits.panCount(capabilities: capabilities(slices: 4, receivers: nil)) == 4)
        #expect(PanLimits.panCount(capabilities: capabilities(slices: nil, receivers: 3)) == 3)
        #expect(PanLimits.panCount(capabilities: capabilities(slices: nil, receivers: nil)) == 1)
        #expect(PanLimits.panCount(capabilities: capabilities(slices: 0, receivers: 0)) == 1)
        #expect(PanLimits.sliceCount(capabilities: capabilities(slices: 3, receivers: 3)) == 3)
        #expect(PanLimits.sliceCount(capabilities: capabilities(slices: nil, receivers: 3)) == nil)
    }
}
