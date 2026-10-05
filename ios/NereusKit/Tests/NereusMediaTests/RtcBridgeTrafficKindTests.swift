// NereusSDR for iOS: tests of what kind the bridge counts each channel's and track's bytes as
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMedia

/// Plan Task 68 step 1: the measurement log reads the app's traffic by
/// kind, so the bridge counts each channel and track as what the peer
/// took it for, and anything it did not take as other.
@Suite("Bridge traffic kinds")
struct RtcBridgeTrafficKindTests {
    @Test("a channel counts as what it was named, an unnamed or rejected one as other")
    func kinds() throws {
        let bridge = try RtcBridge(settings: DataChannelTestPair.settings) { _ in }
        defer { bridge.close() }
        let named = try bridge.createDataChannel(label: "control", unordered: false, maxRetransmits: nil)
        let unnamed = try bridge.createDataChannel(label: "spare", unordered: false, maxRetransmits: nil)
        #expect(bridge.trafficKind(of: named) == .other)
        bridge.countTraffic(on: named, as: .control)
        #expect(bridge.trafficKind(of: named) == .control)
        #expect(bridge.trafficKind(of: unnamed) == .other)
        bridge.countTraffic(on: unnamed, as: .display)
        bridge.reject(unnamed)
        #expect(bridge.trafficKind(of: unnamed) == .other)
        #expect(bridge.trafficKind(of: named) == .control)
    }
}
