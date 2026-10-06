// NereusSDR for iOS: the DTLS fingerprint check itself refuses a mismatched certificate, with no media line involved
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
import Testing
@testable import NereusMedia

/// R-IOS-01: the fingerprint in the remote description is what authenticates
/// the peer's DTLS certificate. These peers carry a data channel only, as
/// the control connection will, so there are no SRTP keys whose derivation
/// could fail instead: only the handshake's own check can stop a wrong
/// certificate (ios/patches/libdatachannel/0001, Mbed TLS). Serialized,
/// since libdatachannel's log is process-wide.
@Suite(.serialized) struct DtlsFingerprintTests {
    @Test func aDataChannelPeerWithAWrongFingerprintInTheOfferNeverOpensItsChannel() async throws {
        let log = LogLines()
        RtcBridge.setLogSink { log.append($0) }
        defer { RtcBridge.setLogSink(nil) }

        let pair = try DataChannelTestPair(wrongFingerprint: true)
        defer { pair.close() }
        try await Self.waitUntil("the answerer to fail") {
            pair.answererStates.contains(.failed) || pair.answererStates.contains(.closed)
        }

        #expect(pair.answererChannels.opened.isEmpty, "\(pair.answererLog)")
        #expect(!pair.answererStates.contains(.connected))
        #expect(pair.answererStates.contains(.failed))
        let lines = log.lines
        #expect(lines.contains { $0.contains("Invalid fingerprint") }, "the check ran")
        // The log is process-wide and other suites' peers finish their
        // handshakes into it, so this pair's own states say its handshake
        // never finished: a peer connection is connected only once DTLS is.
        #expect(!pair.offererStates.contains(.connected), "\(pair.offererLog)")
    }

    @Test func aDataChannelPeerWithTheMatchingFingerprintConnects() async throws {
        let log = LogLines()
        RtcBridge.setLogSink { log.append($0) }
        defer { RtcBridge.setLogSink(nil) }

        let pair = try DataChannelTestPair(wrongFingerprint: false)
        defer { pair.close() }
        try await Self.waitUntil("the answerer's channel to open") {
            !pair.answererChannels.opened.isEmpty
        }
        #expect(pair.answererStates.contains(.connected))
        #expect(pair.answerer.label(ofChannel: pair.answererChannels.opened[0]) == "control")

        // The traffic counter counts each channel as what it carries; the
        // counter is the app's one, so other suites only ever add to it.
        pair.offerer.countTraffic(on: pair.offererChannel, as: .control)
        let before = TrafficCounter.shared.readingByKind
        try pair.offerer.send(Data("ping".utf8), on: pair.offererChannel)
        try await Self.waitUntil("the message to arrive") {
            pair.answererMessages.contains(Data("ping".utf8))
        }
        let counted = TrafficCounter.shared.readingByKind.since(before)
        #expect(counted[.control].bytesOut >= 4)
        #expect(counted[.other].bytesIn >= 4, "the answerer's channel was never named")
        #expect(log.lines.contains { $0.contains("DTLS handshake finished") })
        #expect(!log.lines.contains { $0.contains("Invalid fingerprint") })
    }

    struct TimedOut: Error, CustomStringConvertible {
        let description: String
    }

    private static func waitUntil(_ what: String, timeout: Duration = .seconds(10),
                                  _ condition: () -> Bool) async throws {
        let clock = ContinuousClock()
        let deadline = clock.now + timeout
        while !condition() {
            guard clock.now < deadline else {
                throw TimedOut(description: "timed out waiting for \(what)")
            }
            try await Task.sleep(for: .milliseconds(10))
        }
    }
}

/// libdatachannel's log lines, collected from its threads.
private final class LogLines: @unchecked Sendable {
    private let lock = NSLock()
    private var stored: [String] = []

    func append(_ line: String) {
        lock.withLock { stored.append(line) }
    }

    var lines: [String] {
        lock.withLock { stored }
    }
}
