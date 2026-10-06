// NereusSDR for iOS: the transmit display on the media connection: the declaration, the transmit window, DUP and the context's transmit
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// Task 54f (R-IOS-11, R-IOS-13; the media control document, "Transmit
/// display"): the app declares `txDisplayVersion` in its `start` exactly
/// when the Core sends it (3 where it offers display duplex, else 1), its
/// subscribes carry the transmit window (both edges) and `duplex` only on
/// a connection that declared them, and every context on such a
/// connection carries `transmit`.
@Suite struct TransmitDisplayMediaTests {
    typealias Rig = MediaControlClientTests.Rig

    static func capabilities(txDisplay: Int64) -> [String: Int64] {
        ["remoteMediaVersion": 1, "spectrumGrantVersion": 1, "txDisplayVersion": txDisplay]
    }

    // MARK: The gates

    @Test(arguments: [
        (UInt16(11), Int64(0), false, false, false, Int?.none),
        (UInt16(11), Int64(1), true, false, false, Int?.some(1)),
        (UInt16(11), Int64(2), true, true, false, Int?.some(1)),
        (UInt16(11), Int64(3), true, true, true, Int?.some(3)),
        (UInt16(10), Int64(3), false, false, false, Int?.none),
    ])
    func theGatesFollowTheCoresVersionAtMinor11(minor: UInt16, version: Int64, display: Bool, settings: Bool,
                                                duplex: Bool, declared: Int?) {
        let gates = MediaFeatureGates(agreedMinor: minor) { name in
            ["remoteMediaVersion": 1, "txDisplayVersion": version][name] ?? 0
        }
        #expect(gates.txDisplay == display)
        #expect(gates.txDisplaySettings == settings)
        #expect(gates.displayDuplex == duplex)
        #expect(gates.declaredTxDisplayVersion == declared)
        // Media off: none of it.
        let off = MediaFeatureGates(agreedMinor: minor) { $0 == "txDisplayVersion" ? version : 0 }
        #expect(!off.txDisplay && !off.displayDuplex && off.declaredTxDisplayVersion == nil)
    }

    @Test func aConnectionUsesOnlyWhatItsStartDeclared() {
        let three = MediaFeatureGates(agreedMinor: 11) { ["remoteMediaVersion": 1, "txDisplayVersion": 3][$0] ?? 0 }
        #expect(three.declaring(txDisplayVersion: 3).displayDuplex)
        #expect(three.declaring(txDisplayVersion: 1).txDisplay)
        #expect(!three.declaring(txDisplayVersion: 1).displayDuplex)
        #expect(!three.declaring(txDisplayVersion: nil).txDisplay)
    }

    // MARK: The declaration

    @Test(arguments: [(Int64(0), LinkJSON?.none), (Int64(1), LinkJSON?.some(1)), (Int64(2), LinkJSON?.some(1)),
                      (Int64(3), LinkJSON?.some(3))])
    func theStartDeclaresTheVersionTheCoreSends(version: Int64, declared: LinkJSON?) async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(txDisplay: version))
        let start = try #require(rig.recorder.sent("start").first)
        #expect(start["txDisplayVersion"] == declared)
        var keys: Set<String> = ["op", "connectionId"]
        if declared != nil {
            keys.insert("txDisplayVersion")
        }
        #expect(Set(start.keys) == keys)
    }

    // MARK: Subscribe

    @Test func aDeclaringConnectionSendsTheTransmitWindowAndAtThreeDuplex() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(txDisplay: 3))
        var subscription = MediaControlClientTests.subscription(endpointId: 1)
        subscription.txWindow = -80...30
        subscription.duplex = true
        try await rig.client.subscribe(subscription)
        let sent = try #require(rig.recorder.sent("subscribe").first)
        let plain = DisplayEndpointRequest.subscribe(MediaControlClientTests.subscription(endpointId: 1),
                                                     connectionId: "x", gates: .none)
        #expect(Set(sent.keys) == Set(plain.keys).union(["txMinDbm", "txMaxDbm", "duplex"]))
        #expect(sent["txMinDbm"] == .number(-80))
        #expect(sent["txMaxDbm"] == .number(30))
        #expect(sent["duplex"] == .bool(true))

        // DUP off sends nothing for it: absent is false.
        var off = MediaControlClientTests.subscription(endpointId: 1, revision: 2)
        off.txWindow = -80...30
        off.duplex = false
        try await rig.client.subscribe(off)
        let second = try #require(rig.recorder.sent("subscribe").last)
        #expect(second["duplex"] == nil)
        #expect(second["txMinDbm"] == .number(-80))
    }

    @Test func belowThreeDuplexIsLeftOutAndTheWindowStillGoes() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(txDisplay: 1))
        var subscription = MediaControlClientTests.subscription(endpointId: 1)
        subscription.txWindow = -80...30
        subscription.duplex = true
        try await rig.client.subscribe(subscription)
        let sent = try #require(rig.recorder.sent("subscribe").first)
        #expect(sent["duplex"] == nil)
        #expect(sent["txMinDbm"] == .number(-80))
    }

    @Test func aConnectionThatDidNotDeclareSendsTodaysWire() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(txDisplay: 0))
        var subscription = MediaControlClientTests.subscription(endpointId: 1)
        subscription.txWindow = -80...30
        subscription.duplex = true
        try await rig.client.subscribe(subscription)
        let sent = try #require(rig.recorder.sent("subscribe").first)
        let plain = DisplayEndpointRequest.subscribe(MediaControlClientTests.subscription(endpointId: 1),
                                                     connectionId: "x", gates: .none)
        #expect(Set(sent.keys) == Set(plain.keys))
    }

    @Test(arguments: [(-80.0, -80.0), (-401.0, 0.0), (-80.0, 101.0)])
    func aWindowTheCoreWouldRefuseNeverLeaves(low: Double, high: Double) throws {
        let gates = MediaFeatureGates(agreedMinor: 11) { ["remoteMediaVersion": 1, "txDisplayVersion": 1][$0] ?? 0 }
        var subscription = MediaControlClientTests.subscription()
        subscription.txWindow = low...high
        #expect(throws: DisplayEndpointRequest.Invalid.txWindow) {
            try DisplayEndpointRequest.validate(subscription, gates: gates)
        }
        subscription.txWindow = -80...30
        #expect(throws: DisplayEndpointRequest.Invalid.txDisplayUnavailable) {
            try DisplayEndpointRequest.validate(subscription, gates: .none)
        }
    }

    // MARK: The context

    static func context(_ id: String, generation: UInt32, transmit: Bool?) -> [String: LinkJSON] {
        var payload = MediaControlClientTests.context(id, generation: generation, grant: true)
        if let transmit {
            payload["transmit"] = .bool(transmit)
            if transmit {
                payload["sampleRateHz"] = 96_000
                payload["centreHz"] = 14_100_000
                payload["spanHz"] = 8_000
                payload["limit"] = "shared"
            }
        }
        return payload
    }

    @Test func aDeclaringConnectionsContextsCarryTransmitAndOthersAreRefused() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(txDisplay: 1))
        let id = try await rig.connectionId
        try await rig.client.subscribe(MediaControlClientTests.subscription(endpointId: 1))
        // Without the field, the shape is not this connection's.
        await rig.deliver(Self.context(id, generation: 1, transmit: nil))
        await rig.deliver(Self.context(id, generation: 2, transmit: false))
        await rig.deliver(Self.context(id, generation: 3, transmit: true))
        await rig.recorder.settle { false }
        let contexts = rig.recorder.events.compactMap { event -> MediaControlEvent.DisplayContext? in
            if case .context(let context) = event { return context } else { return nil }
        }
        #expect(contexts.map(\.contextGeneration) == [2, 3])
        #expect(contexts.map(\.transmit) == [false, true])
        #expect(contexts.last?.sampleRateHz == 96_000)
        #expect(contexts.last?.grant?.limit == .shared)
    }

    @Test func withoutTheDeclarationATransmitFieldIsAnotherShape() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(txDisplay: 0))
        let id = try await rig.connectionId
        try await rig.client.subscribe(MediaControlClientTests.subscription(endpointId: 1))
        await rig.deliver(Self.context(id, generation: 1, transmit: false))
        await rig.deliver(Self.context(id, generation: 2, transmit: nil))
        await rig.recorder.settle { false }
        let contexts = rig.recorder.events.compactMap { event -> MediaControlEvent.DisplayContext? in
            if case .context(let context) = event { return context } else { return nil }
        }
        #expect(contexts.map(\.contextGeneration) == [2])
        #expect(contexts.first?.transmit == nil)
    }

    @Test func theDecoderReadsTransmitOnlyAsABoolean() {
        let id = "3f2504e0-4f89-41d3-9a0c-0305e82c3301"
        var payload = Self.context(id, generation: 1, transmit: true)
        #expect(MediaControlDecoder.context(payload, wideband: false, grant: true, transmit: true)?.transmit == true)
        #expect(MediaControlDecoder.context(payload, wideband: false, grant: true, transmit: false) == nil)
        payload["transmit"] = 1
        #expect(MediaControlDecoder.context(payload, wideband: false, grant: true, transmit: true) == nil)
    }
}
