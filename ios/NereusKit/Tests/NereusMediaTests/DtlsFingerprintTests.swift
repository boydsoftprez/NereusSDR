// NereusSDR for iOS: the DTLS fingerprint check itself refuses a mismatched certificate, with no media line involved
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Darwin
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
        let receipts = DtlsDiagnosticReceipts()
        defer { receipts.printOnce() }
        let log = LogLines()
        RtcBridge.setLogSink { log.append($0) }
        defer { RtcBridge.setLogSink(nil) }

        let pair = try DataChannelTestPair(wrongFingerprint: true, observer: receipts.mark)
        defer { pair.close() }
        defer {
            pair.diagnosticSnapshot(.finalSnapshot)
            receipts.printOnce()
        }
        try await Self.waitUntil("the answerer to fail", onWait: { phase in pair.diagnosticSnapshot(phase) }) {
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
                                  onWait: (@Sendable (DtlsDiagnosticReceipt.Phase) -> Void)? = nil,
                                  _ condition: () -> Bool) async throws {
        let clock = ContinuousClock()
        let deadline = clock.now + timeout
        onWait?(.waitEntry)
        while !condition() {
            guard clock.now < deadline else {
                onWait?(.waitTimeout)
                throw TimedOut(description: "timed out waiting for \(what)")
            }
            try await Task.sleep(for: .milliseconds(10))
        }
        onWait?(.waitDone)
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

// Diagnostic-only collection adds clock/thread reads and lock/copy overhead.
// It is capped and prints once before the existing pair teardown; no waits are added.
private final class DtlsDiagnosticReceipts: @unchecked Sendable {
    private struct Entry {
        let elapsed: Duration
        let threadID: UInt64
        let mainThread: Bool
        let receipt: DtlsDiagnosticReceipt
    }
    private let lock = NSLock()
    private let clock = ContinuousClock()
    private let started = ContinuousClock.now
    private var entries: [Entry] = []
    private var omitted = 0
    private var printed = false

    func mark(_ receipt: DtlsDiagnosticReceipt) {
        lock.withLock {
            guard !printed else { return }
            let terminal = receipt.phase == .waitTimeout || receipt.phase == .waitDone || receipt.phase == .finalSnapshot
            guard entries.count < (terminal ? 128 : 120) else {
                omitted += 1
                return
            }
            var threadID: UInt64 = 0
            _ = pthread_threadid_np(nil, &threadID)
            entries.append(Entry(elapsed: clock.now - started, threadID: threadID,
                                 mainThread: Thread.isMainThread, receipt: receipt))
        }
    }

    func printOnce() {
        let snapshot: ([Entry], Int)? = lock.withLock {
            guard !printed else { return nil }
            printed = true
            return (entries, omitted)
        }
        guard let (entries, omitted) = snapshot else { return }
        var lines = ["KIT DTLS WRONG-FINGERPRINT RECEIPTS count=\(entries.count) omitted=\(omitted)"]
        for (index, entry) in entries.enumerated() {
            let r = entry.receipt
            var fields = ["n=\(index)", "elapsed=\(entry.elapsed)", "thread=\(entry.threadID)",
                          "main=\(entry.mainThread)", "phase=\(r.phase.rawValue)"]
            if let value = r.side { fields.append("side=\(value.rawValue)") }
            if let value = r.call { fields.append("call=\(value.rawValue)") }
            if let value = r.event { fields.append("event=\(value.rawValue)") }
            if let value = r.code { fields.append("code=\(value)") }
            if let value = r.success { fields.append("success=\(value)") }
            if let value = r.prefixPresent { fields.append("prefixPresent=\(value)") }
            if let value = r.changed { fields.append("changed=\(value)") }
            if let value = r.state { fields.append("state=\(value)") }
            if let value = r.count { fields.append("count=\(value)") }
            if let value = r.descriptions { fields.append("descriptions=\(value)") }
            if let value = r.candidates { fields.append("candidates=\(value)") }
            if let value = r.opened { fields.append("opened=\(value)") }
            if let value = r.failed { fields.append("failed=\(value)") }
            if let value = r.closed { fields.append("closed=\(value)") }
            if let value = r.connected { fields.append("connected=\(value)") }
            lines.append(fields.joined(separator: " "))
        }
        print(lines.joined(separator: "\n"))
    }
}
