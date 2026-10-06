// NereusSDR for iOS: a direct connection that fails says why: refused, no route, or no reply
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Darwin
import Foundation
import Network
import Testing
@testable import NereusLink

/// R-IOS-16 (2026-09-29): the attempt record tells a refused connection, an
/// address with no route from this network and one that never answered
/// apart, so the next failed direct path shows why.
@Suite struct DialFailureReasonTests {
    @Test func networkErrorsOnADirectRouteNameTheirReason() {
        #expect(WebSocketLinkTransport.failure(for: .posix(.ECONNREFUSED), route: .direct) == .refused)
        #expect(WebSocketLinkTransport.failure(for: .posix(.ENETUNREACH), route: .direct) == .unreachable)
        #expect(WebSocketLinkTransport.failure(for: .posix(.EHOSTUNREACH), route: .direct) == .unreachable)
        #expect(WebSocketLinkTransport.failure(for: .posix(.ENETDOWN), route: .direct) == .unreachable)
        #expect(WebSocketLinkTransport.failure(for: .posix(.EADDRNOTAVAIL), route: .direct) == .unreachable)
        #expect(WebSocketLinkTransport.failure(for: .posix(.ETIMEDOUT), route: .direct).isNoReply)
        #expect(WebSocketLinkTransport.failure(for: .posix(.EPIPE), route: .direct)
                == .failed("the network did not open the connection"))
    }

    @Test func throughAProxyTheProxysAnswerIsNotTheCores() {
        let proxy = SystemProxyRoute.httpCONNECT(SystemProxyEndpoint(host: "proxy.example", port: 8080))
        #expect(WebSocketLinkTransport.failure(for: .posix(.ECONNREFUSED), route: proxy)
                == .failed("the network or its configured proxy did not open the connection"))
    }

    @Test func bothNoReplySpellingsReadAsNoReply() {
        #expect(LinkTransportError.noReply.isNoReply)
        #expect(LinkTransportError.failed(SystemProxyError.timedOut.openingFailureText).isNoReply)
        #expect(!LinkTransportError.refused.isNoReply)
        #expect(!LinkTransportError.failed("closed").isNoReply)
    }

    /// A port nothing listens on, on this computer: its connection is refused.
    @Test func aPortNothingListensOnIsRefused() async throws {
        let reservation = try RefusedPortReservation()
        defer { reservation.close() }
        try #require(reservation.port != 0)
        let transport = WebSocketLinkTransport(endpoint: StationEndpoint(host: "127.0.0.1", port: reservation.port),
                                               trust: .pairing, openDeadline: .seconds(10))
        await #expect(throws: LinkTransportError.refused) {
            _ = try await transport.open { _ in }
        }
    }
}

/// Keep an established pair alive to reserve the port after its listener closes.
/// New connections have no listener, and the reservation survives the tested open.
private final class RefusedPortReservation {
    let port: UInt16
    private var clientDescriptor: Int32
    private var acceptedDescriptor: Int32

    init() throws {
        var listener: Int32 = -1
        var client: Int32 = -1
        var accepted: Int32 = -1
        var retained = false
        defer {
            if listener >= 0 { Darwin.close(listener) }
            if !retained {
                if accepted >= 0 { Darwin.close(accepted) }
                if client >= 0 { Darwin.close(client) }
            }
        }

        listener = try Self.checked(Darwin.socket(AF_INET, SOCK_STREAM, 0))
        try Self.setTimeout(on: listener, option: SO_RCVTIMEO)
        var address = sockaddr_in()
        address.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        address.sin_family = sa_family_t(AF_INET)
        address.sin_addr.s_addr = inet_addr("127.0.0.1")
        address.sin_port = 0
        var length = socklen_t(MemoryLayout<sockaddr_in>.size)
        try withUnsafeMutablePointer(to: &address) { pointer in
            try pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { raw in
                _ = try Self.checked(Darwin.bind(listener, raw, length))
                _ = try Self.checked(Darwin.getsockname(listener, raw, &length))
            }
        }
        _ = try Self.checked(Darwin.listen(listener, 1))
        client = try Self.checked(Darwin.socket(AF_INET, SOCK_STREAM, 0))
        try Self.setTimeout(on: client, option: SO_SNDTIMEO)
        try withUnsafePointer(to: &address) { pointer in
            try pointer.withMemoryRebound(to: sockaddr.self, capacity: 1) { raw in
                _ = try Self.checked(Darwin.connect(client, raw, length))
            }
        }
        accepted = try Self.checked(Darwin.accept(listener, nil, nil))
        _ = try Self.checked(Darwin.close(listener))
        listener = -1

        port = UInt16(bigEndian: address.sin_port)
        clientDescriptor = client
        acceptedDescriptor = accepted
        retained = true
    }

    func close() {
        if acceptedDescriptor >= 0 {
            Darwin.close(acceptedDescriptor)
            acceptedDescriptor = -1
        }
        if clientDescriptor >= 0 {
            Darwin.close(clientDescriptor)
            clientDescriptor = -1
        }
    }

    deinit { close() }

    private static func checked(_ result: Int32) throws -> Int32 {
        guard result >= 0 else {
            throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO)
        }
        return result
    }

    private static func setTimeout(on descriptor: Int32, option: Int32) throws {
        var timeout = timeval(tv_sec: 2, tv_usec: 0)
        _ = try withUnsafePointer(to: &timeout) { pointer in
            try checked(Darwin.setsockopt(descriptor, SOL_SOCKET, option, pointer,
                                         socklen_t(MemoryLayout<timeval>.size)))
        }
    }
}
