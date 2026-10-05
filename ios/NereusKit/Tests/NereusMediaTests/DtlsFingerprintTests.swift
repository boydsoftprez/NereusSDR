// NereusSDR for iOS: the DTLS fingerprint check itself refuses a mismatched certificate, with no media line involved
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Dispatch
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

        let receipts = DtlsPhaseReceipts()
        // This defer also emits when construction throws before the pair exists.
        defer { receipts.emit() }
        let pair = try DataChannelTestPair(wrongFingerprint: true,
                                          phaseObserver: { receipts.append($0) })
        defer {
            receipts.finish(with: pair.phaseSnapshots())
            receipts.emit()
            pair.close()
        }
        try await Self.waitUntil("the answerer to fail", phaseObserver: { receipts.append($0) }) {
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
                                  phaseObserver: DataChannelTestPair.PhaseObserver? = nil,
                                  _ condition: () -> Bool) async throws {
        phaseObserver?(DtlsPhaseObservation(phase: 9))
        let clock = ContinuousClock()
        let deadline = clock.now + timeout
        while !condition() {
            guard clock.now < deadline else {
                phaseObserver?(DtlsPhaseObservation(phase: 11, detail: 2))
                throw TimedOut(description: "timed out waiting for \(what)")
            }
            try await Task.sleep(for: .milliseconds(10))
        }
        phaseObserver?(DtlsPhaseObservation(phase: 11, detail: 1))
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

/// A test-local bounded receipt. The last three slots are reserved for wait/final snapshots.
private final class DtlsPhaseReceipts: @unchecked Sendable {
    private struct Stamped {
        let nanoseconds: UInt64
        let mainThread: Bool
        let observation: DtlsPhaseObservation
    }

    private let lock = NSLock()
    private let start = DispatchTime.now().uptimeNanoseconds
    private var stored: [Stamped] = []
    private var final: [Stamped] = []
    private var omitted = 0
    private var sealed = false
    private var eventCounts = Array(repeating: Array(repeating: 0, count: 12), count: 3)
    // Per destination: rewrite attempts, successful rewrites, admissions, deliveries, delivery failures.
    private var candidateCounts = Array(repeating: Array(repeating: 0, count: 5), count: 3)

    func append(_ observation: DtlsPhaseObservation) {
        lock.withLock {
            guard !sealed else { return }
            if observation.phase == 6 {
                eventCounts[observation.side][observation.detail] += 1
            } else if observation.phase == 8 {
                if observation.detail == 1 {
                    candidateCounts[observation.side][0] += 1
                    candidateCounts[observation.side][1] += Int(observation.first)
                } else if observation.detail == 2 {
                    candidateCounts[observation.side][2] += 1
                }
            } else if observation.phase == 5, observation.detail == 8 || observation.detail == 9 {
                candidateCounts[observation.side][observation.first == 1 ? 3 : 4] += 1
            }
            if observation.phase == 11 {
                if final.count < 3 {
                    final.append(stamp(observation))
                } else {
                    omitted += 1
                }
            } else if stored.count < 125 {
                stored.append(stamp(observation))
            } else {
                omitted += 1
            }
        }
    }

    func finish(with snapshots: [DtlsPhaseObservation]) {
        lock.withLock {
            guard !sealed else { return }
            for snapshot in snapshots {
                if final.count < 3 {
                    final.append(stamp(snapshot))
                } else {
                    omitted += 1
                }
            }
        }
    }

    private func stamp(_ observation: DtlsPhaseObservation) -> Stamped {
        Stamped(nanoseconds: DispatchTime.now().uptimeNanoseconds - start,
                mainThread: Thread.isMainThread, observation: observation)
    }

    func emit() {
        let line: String? = lock.withLock {
            guard !sealed else { return nil }
            sealed = true
            let records = (stored + final).sorted { $0.nanoseconds < $1.nanoseconds }.map { record in
                let value = record.observation
                return "\(record.nanoseconds),\(record.mainThread ? 1 : 0),\(value.phase),\(value.side),\(value.detail),\(value.first),\(value.second),\(value.third)"
            }.joined(separator: ";")
            let events = (1...2).map { side in
                eventCounts[side].dropFirst().map(String.init).joined(separator: ",")
            }.joined(separator: ";")
            let candidates = (1...2).map { side in
                candidateCounts[side].map(String.init).joined(separator: ",")
            }.joined(separator: ";")
            return "DTLS_PHASE_RECEIPT version=1 cap=128 omitted=\(omitted) records=\(records) event_counts=\(events) candidate_counts=\(candidates) native_getters=0"
        }
        if let line { print(line) }
    }
}
