// NereusSDR for iOS: the transmit monitor on the media connection: the declaration, monitor-audio and its context
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkSessionTestSupport
import LinkTestSupport
import NereusLink
import Testing
@testable import NereusMedia

/// Task 55b (R-IOS-13, R-IOS-20, D80; the media control document,
/// "Transmit monitor (monitor-audio)"): the app declares
/// `txMonitorAudioVersion` 1 in its `start` exactly when the Core offers it
/// at minor 11, sends `monitor-audio` with exactly its four keys and a
/// revision that only rises, sends the route again on each new connection,
/// and reports only the `monitor-audio-context` that answers its latest
/// request.
@Suite struct MonitorAudioMediaTests {
    typealias Rig = MediaControlClientTests.Rig

    static func capabilities(monitor: Int64) -> [String: Int64] {
        ["remoteMediaVersion": 1, "txMonitorAudioVersion": monitor]
    }

    static func context(_ id: String, revision: Double, route: String) -> [String: LinkJSON] {
        ["op": "monitor-audio-context", "connectionId": .string(id), "revision": .number(revision),
         "route": .string(route)]
    }

    // MARK: The gate and the declaration

    @Test(arguments: [
        (UInt16(11), Int64(0), false), (UInt16(11), Int64(1), true), (UInt16(11), Int64(2), true),
        (UInt16(10), Int64(1), false),
    ])
    func theGateNeedsMinor11AndVersion1(minor: UInt16, version: Int64, on: Bool) {
        let gates = MediaFeatureGates(agreedMinor: minor) { ["remoteMediaVersion": 1, "txMonitorAudioVersion": version][$0] ?? 0 }
        #expect(gates.txMonitorAudio == on)
        let mediaOff = MediaFeatureGates(agreedMinor: minor) { $0 == "txMonitorAudioVersion" ? version : 0 }
        #expect(!mediaOff.txMonitorAudio)
    }

    @Test(arguments: [(Int64(0), LinkJSON?.none), (Int64(1), LinkJSON?.some(1)), (Int64(2), LinkJSON?.some(1))])
    func theStartDeclaresVersion1WhenOffered(version: Int64, declared: LinkJSON?) async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(monitor: version))
        let start = try #require(rig.recorder.sent("start").first)
        #expect(start["txMonitorAudioVersion"] == declared)
        #expect(Set(start.keys) == (declared == nil ? ["op", "connectionId"]
                                                     : ["op", "connectionId", "txMonitorAudioVersion"]))
    }

    // MARK: monitor-audio

    @Test func theRouteGoesOutWithExactlyFourKeysAndARisingRevision() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(monitor: 1))
        try await rig.connect()
        let id = try await rig.connectionId
        await rig.client.setMonitorRoute(.headphones)
        await rig.client.setMonitorRoute(.none)
        let sent = rig.recorder.sent("monitor-audio")
        #expect(sent == [
            ["op": "monitor-audio", "connectionId": .string(id), "revision": 1, "route": "headphones"],
            ["op": "monitor-audio", "connectionId": .string(id), "revision": 2, "route": "none"],
        ])
    }

    @Test func nothingGoesOutWithoutTheDeclaration() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(monitor: 0))
        try await rig.connect()
        await rig.client.setMonitorRoute(.headphones)
        await rig.recorder.settle { false }
        #expect(rig.recorder.sent("monitor-audio").isEmpty)
    }

    @Test func aRouteAskedBeforeConnectingGoesOutWhenTheConnectionComesUp() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(monitor: 1))
        await rig.client.setMonitorRoute(.headphones)
        #expect(rig.recorder.sent("monitor-audio").isEmpty)
        try await rig.connect()
        await rig.recorder.waitUntilSent("monitor-audio", count: 1)
        #expect(rig.recorder.sent("monitor-audio").count == 1)
        #expect(rig.recorder.sent("monitor-audio").first?["route"] == "headphones")
    }

    @Test func aNewConnectionIsToldTheRouteAgainAndNoneNeedsNothing() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(monitor: 1))
        try await rig.connect()
        await rig.client.setMonitorRoute(.headphones)
        try rig.peer.become(.closed)
        await rig.recorder.waitUntilSent("start", count: 2)
        let second = try await rig.connectionId
        try await rig.connect()
        await rig.recorder.waitUntilSent("monitor-audio", count: 2)
        let again = try #require(rig.recorder.sent("monitor-audio").last)
        #expect(again == ["op": "monitor-audio", "connectionId": .string(second), "revision": 2,
                          "route": "headphones"])

        // With the route at none, a third connection gets nothing.
        await rig.client.setMonitorRoute(.none)
        try rig.peer.become(.closed)
        await rig.recorder.waitUntilSent("start", count: 3)
        try await rig.connect()
        await rig.recorder.settle { false }
        #expect(rig.recorder.sent("monitor-audio").count == 3)
    }

    // MARK: monitor-audio-context

    @Test func onlyTheAnswerToTheLatestRequestIsReported() async throws {
        let rig = try Rig()
        await rig.open(minor: 11, capabilities: Self.capabilities(monitor: 1))
        try await rig.connect()
        let id = try await rig.connectionId
        await rig.client.setMonitorRoute(.headphones)
        await rig.client.setMonitorRoute(.none)
        await rig.client.setMonitorRoute(.headphones)
        // A stale revision, another connection, an extra key, an unknown
        // route and a zero revision are dropped.
        await rig.deliver(Self.context(id, revision: 2, route: "none"))
        await rig.deliver(Self.context("another", revision: 3, route: "speakers"))
        var extra = Self.context(id, revision: 3, route: "speakers")
        extra["extra"] = 1
        await rig.deliver(extra)
        await rig.deliver(Self.context(id, revision: 3, route: "loud"))
        await rig.deliver(Self.context(id, revision: 0, route: "speakers"))
        // The answer: the phone did not declare the headphones mix, so the
        // Core applies headphones as its main stream.
        await rig.deliver(Self.context(id, revision: 3, route: "speakers"))
        let isMonitor: (MediaControlEvent) -> Bool = {
            if case .monitorAudioContext = $0 { return true }
            return false
        }
        await rig.recorder.settle { rig.recorder.events.contains(where: isMonitor) }
        await rig.recorder.settle { false }
        #expect(rig.recorder.events.filter(isMonitor)
                == [.monitorAudioContext(.init(revision: 3, route: .speakers))])
    }

    @Test func theDecoderTakesExactlyTheFourKeys() {
        #expect(MediaControlDecoder.monitorAudioContext(Self.context("c", revision: 7, route: "none"))
                == .init(revision: 7, route: .none))
        #expect(MediaControlDecoder.monitorAudioContext(Self.context("c", revision: 1.5, route: "none")) == nil)
        #expect(MediaControlDecoder.monitorAudioContext(Self.context("c", revision: 4_294_967_296, route: "none")) == nil)
        var wrongOp = Self.context("c", revision: 1, route: "none")
        wrongOp["op"] = "monitor-audio"
        #expect(MediaControlDecoder.monitorAudioContext(wrongOp) == nil)
        var missing = Self.context("c", revision: 1, route: "none")
        missing["route"] = nil
        #expect(MediaControlDecoder.monitorAudioContext(missing) == nil)
    }
}
