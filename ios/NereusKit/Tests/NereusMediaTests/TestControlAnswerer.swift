// NereusSDR for iOS: the Core's end of a control connection in this process, for the dial tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink
@testable import NereusMedia

/// A libdatachannel peer in the answerer's role, as the Core answers an
/// introduction (link document section 20): it takes the phone's offer,
/// answers with one application line and no candidate, and gives its host
/// candidates one at a time, on 127.0.0.1 so the tests keep to this
/// computer. The channel the phone opens is recorded with what arrives on
/// it. Its certificate is a one-off; ``certificateSHA256`` is the digest in
/// its own description, which the DTLS handshake holds it to.
final class TestControlAnswerer: @unchecked Sendable {
    static let settings = RtcPeerSettings(iceServers: [], mtu: IceSettings.mtu,
                                          maxMessageSize: ControlChannelFraming.chunkBytes, enableIceTcp: false,
                                          forceMediaTransport: false, disableAutoNegotiation: true)

    private(set) var bridge: RtcBridge!
    private let lock = NSLock()
    private var events: [RtcEvent] = []

    init() throws {
        bridge = try RtcBridge(settings: Self.settings) { [weak self] event in
            self?.record(event)
        }
    }

    deinit {
        close()
    }

    func close() {
        bridge?.close()
    }

    private func record(_ event: RtcEvent) {
        lock.withLock { events.append(event) }
    }

    var log: [RtcEvent] { lock.withLock { events } }

    /// Takes the phone's offer and returns the answer without candidates.
    func answer(_ offer: String) async throws -> String {
        try bridge.setRemoteDescription(offer, type: "offer")
        try bridge.setLocalDescription(type: "answer")
        for _ in 0..<2_000 {
            if let sdp = description {
                return Self.withoutCandidates(sdp)
            }
            try await Task.sleep(for: .milliseconds(5))
        }
        throw TimedOut(description: "the answer")
    }

    var description: String? {
        for event in log {
            if case .localDescription(let sdp, _) = event {
                return sdp
            }
        }
        return nil
    }

    /// Its candidates so far, on 127.0.0.1, `candidate:...`.
    var candidates: [String] {
        log.compactMap { event in
            guard case .localCandidate(let candidate, _) = event else {
                return nil
            }
            let value = candidate.hasPrefix("a=") ? String(candidate.dropFirst(2)) : candidate
            return LoopbackCandidate.rewrite(value)
        }
    }

    /// Waits for at least one host candidate.
    func firstCandidates() async throws -> [String] {
        for _ in 0..<2_000 {
            let found = candidates
            if !found.isEmpty {
                return found
            }
            try await Task.sleep(for: .milliseconds(5))
        }
        throw TimedOut(description: "a candidate")
    }

    /// One of the phone's candidates, moved to 127.0.0.1.
    func add(_ candidate: String) {
        guard let loopback = LoopbackCandidate.rewrite(candidate) else {
            return
        }
        try? bridge.addRemoteCandidate(loopback, mid: "")
    }

    /// The data channels the phone opened, and the binary messages on them.
    var channels: [Int32] {
        log.compactMap { event in
            if case .dataChannel(let id) = event { return id }
            return nil
        }
    }

    var opened: Bool {
        log.contains { event in
            if case .open(let id) = event { return channels.contains(id) }
            return false
        }
    }

    var messages: [Data] {
        log.compactMap { event in
            if case .message(_, let data) = event { return data }
            return nil
        }
    }

    /// The SHA-256 its description claims for its certificate.
    var certificateSHA256: Data? {
        guard let sdp = description,
              let line = sdp.split(whereSeparator: \.isNewline).first(where: { $0.hasPrefix("a=fingerprint:") }) else {
            return nil
        }
        return ControlPeer.sha256(fromFingerprint: String(line.dropFirst("a=fingerprint:".count)))
    }

    static func withoutCandidates(_ sdp: String) -> String {
        sdp.split(separator: "\r\n", omittingEmptySubsequences: false)
            .filter { !$0.hasPrefix("a=candidate:") && !$0.hasPrefix("a=end-of-candidates") }
            .joined(separator: "\r\n")
    }

    struct TimedOut: Error, CustomStringConvertible {
        let description: String
    }
}
