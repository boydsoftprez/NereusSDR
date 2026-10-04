// NereusSDR for iOS: the opening request names the Core as RFC 7230 requires, and an opening nobody answers ends
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Testing
@testable import NereusLink

/// Network framework writes the Host header of a WebSocket opening as the
/// bare address with no port (`::1` for `wss://[::1]:47910/`); a Core reads
/// no IPv6 literal from it and never answers, so the opening waited for
/// ever. The transport sends one Host header of its own, and an opening
/// still waiting at the connect deadline fails as "no reply".
@Suite(.serialized) struct WebSocketOpeningTests {
    /// Opens a transport against a listener that never answers, until the
    /// listener has the opening request; returns the request's Host lines.
    private static func hostLines(dialling address: String) async throws -> (port: UInt16, lines: [String]) {
        let listener = try SilentTLSListener(address: address)
        let port = try await listener.start()
        defer { listener.stop() }
        let transport = WebSocketLinkTransport(endpoint: StationEndpoint(host: address, port: port), trust: .pairing)
        let opening = Task { try await transport.open { _ in } }
        let clock = ContinuousClock()
        let giveUp = clock.now + .seconds(10)
        while listener.receivedRequests.isEmpty && clock.now < giveUp {
            try await Task.sleep(for: .milliseconds(20))
        }
        transport.close()
        _ = try? await opening.value
        let request = try #require(listener.receivedRequests.first, "no opening request reached \(address)")
        let lines = request.split(separator: "\r\n").map(String.init)
            .filter { $0.lowercased().hasPrefix("host:") }
        return (port, lines)
    }

    @Test func anIPv6CoreIsNamedInBracketsWithItsPort() async throws {
        let (port, lines) = try await Self.hostLines(dialling: "::1")
        #expect(lines == ["Host: [::1]:\(port)"])
    }

    @Test func anIPv4CoreIsNamedBareWithItsPort() async throws {
        let (port, lines) = try await Self.hostLines(dialling: "127.0.0.1")
        #expect(lines == ["Host: 127.0.0.1:\(port)"])
    }

    @Test func ownedBinarySendDoesNotReenterItsPermitOnIdleQueue() async throws {
        let listener = try SilentTLSListener(address: "127.0.0.1")
        let port = try await listener.start()
        defer { listener.stop() }
        let transport = WebSocketLinkTransport(endpoint: StationEndpoint(host: "127.0.0.1", port: port),
                                               trust: .pairing)
        let opening = Task { try? await transport.open { _ in } }
        defer { transport.close(); opening.cancel() }
        let deadline = ContinuousClock.now + .seconds(5)
        while listener.receivedRequests.isEmpty && ContinuousClock.now < deadline {
            try await Task.sleep(for: .milliseconds(10))
        }
        try #require(!listener.receivedRequests.isEmpty)
        let ownership = BinaryMediaOwnership()
        let result = LockedBinarySend()
        let frame = Data([2] + Array(repeating: UInt8(7), count: 17))
        // Its own thread, holding none of the transport's locks. A global
        // queue's block can wait seconds for a thread while other suites
        // keep that pool busy, which is not the send locking itself. A send
        // that locks itself never returns, so the deadline can be generous.
        Thread.detachNewThread {
            result.record(transport.sendBinary(frame, ownership: ownership))
        }
        let sendDeadline = ContinuousClock.now + .seconds(30)
        while result.value == nil && ContinuousClock.now < sendDeadline {
            try await Task.sleep(for: .milliseconds(10))
        }
        #expect(result.value == true, "owned binary send must return without locking itself")
        if result.value != nil { ownership.retire() }
    }

    private final class LockedBinarySend: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: Bool?
        var value: Bool? { lock.withLock { stored } }
        func record(_ value: Bool) { lock.withLock { stored = value } }
    }

    @Test(arguments: [
        ("::1", "[::1]:47910"),
        ("fe80::1%en0", "[fe80::1%25en0]:47910"),
        ("127.0.0.1", "127.0.0.1:47910"),
        ("core.example", "core.example:47910"),
    ])
    func theHostHeaderFollowsRFC7230(host: String, expected: String) {
        #expect(WebSocketLinkTransport.hostHeader(for: StationEndpoint(host: host, port: 47910)) == expected)
    }

    @Test func theOpeningIsBoundedByTheLinksConnectDeadline() {
        #expect(WebSocketLinkTransport.openDeadline == .milliseconds(30_000))
        #expect(WebSocketLinkTransport.openDeadline == StationSession.connectDeadline)
    }

    @Test(arguments: ["::1", "127.0.0.1"])
    func anOpeningNobodyAnswersFailsAsNoReply(address: String) async throws {
        let listener = try SilentTLSListener(address: address)
        let port = try await listener.start()
        defer { listener.stop() }
        let transport = WebSocketLinkTransport(endpoint: StationEndpoint(host: address, port: port), trust: .pairing,
                                               openDeadline: .milliseconds(400))
        // Without a bound the opening would wait for ever; the test closes
        // it after 10 s, which fails as "closed", not "no reply".
        let backstop = Task {
            try await Task.sleep(for: .seconds(10))
            transport.close()
        }
        defer { backstop.cancel() }
        let clock = ContinuousClock()
        let started = clock.now
        await #expect(throws: LinkTransportError.failed("no reply")) {
            _ = try await transport.open { _ in }
        }
        #expect(clock.now - started < .seconds(5))
        #expect(!listener.receivedRequests.isEmpty, "the listener saw no opening, so nothing was waited on")
    }
}
