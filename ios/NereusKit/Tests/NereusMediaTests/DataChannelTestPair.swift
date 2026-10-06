// NereusSDR for iOS: two in-process libdatachannel peers with a data channel only, for the DTLS fingerprint tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
@testable import NereusMedia

/// An offerer and an answerer in this process, both libdatachannel with
/// Mbed TLS, connected over 127.0.0.1 with no media line: one data channel,
/// the shape of the control connection. Signalling is relayed here; with
/// `wrongFingerprint` the offer reaches the answerer with one digit of its
/// DTLS fingerprint changed, so only the answerer's fingerprint check can
/// stop the connection.
final class DataChannelTestPair: @unchecked Sendable {
    static let settings = RtcPeerSettings(iceServers: [], mtu: 1000, maxMessageSize: 65536,
                                          enableIceTcp: false, forceMediaTransport: false,
                                          disableAutoNegotiation: true)

    let offerer: RtcBridge
    let answerer: RtcBridge
    let offererChannel: Int32

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var offererEvents: [RtcEvent] = []
    private var answererEvents: [RtcEvent] = []
    private var pump: Task<Void, Never>?

    init(wrongFingerprint: Bool) throws {
        let box = EventBox()
        offerer = try RtcBridge(settings: Self.settings) { box.offerer($0) }
        answerer = try RtcBridge(settings: Self.settings) { box.answerer($0) }
        offererChannel = try offerer.createDataChannel(label: "control", unordered: false,
                                                       maxRetransmits: nil)
        // Nothing happens on either peer before the offer is made below.
        box.pair = self
        pump = Task { [weak self] in
            await self?.relay(wrongFingerprint: wrongFingerprint)
        }
        try offerer.setLocalDescription(type: "offer")
    }

    deinit {
        close()
    }

    func close() {
        pump?.cancel()
        offerer.close()
        answerer.close()
    }

    var answererLog: [RtcEvent] {
        lock.withLock { answererEvents }
    }

    var offererLog: [RtcEvent] {
        lock.withLock { offererEvents }
    }

    /// The data channels the answerer was handed, and those that opened.
    var answererChannels: (handed: [Int32], opened: [Int32]) {
        let events = answererLog
        let handed = events.compactMap { event -> Int32? in
            if case .dataChannel(let id) = event { return id }
            return nil
        }
        let opened = events.compactMap { event -> Int32? in
            if case .open(let id) = event, handed.contains(id) { return id }
            return nil
        }
        return (handed, opened)
    }

    /// The answerer's peer connection states, in order.
    var answererStates: [RtcPeerState] {
        answererLog.compactMap { event in
            if case .peerState(let state) = event { return state }
            return nil
        }
    }

    /// The offerer's peer connection states, in order.
    var offererStates: [RtcPeerState] {
        offererLog.compactMap { event in
            if case .peerState(let state) = event { return state }
            return nil
        }
    }

    /// Binary messages the answerer received.
    var answererMessages: [Data] {
        answererLog.compactMap { event in
            if case .message(_, let data) = event { return data }
            return nil
        }
    }

    fileprivate func record(offerer event: RtcEvent) {
        lock.withLock { offererEvents.append(event) }
    }

    fileprivate func record(answerer event: RtcEvent) {
        lock.withLock { answererEvents.append(event) }
    }

    private func relay(wrongFingerprint: Bool) async {
        var offerSent = false
        var answerSent = false
        var offererCandidates = 0
        var answererCandidates = 0
        var added = Set<String>()
        while !Task.isCancelled {
            let fromOfferer = offererLog
            let fromAnswerer = answererLog
            if !offerSent, let offer = Self.description(in: fromOfferer) {
                offerSent = true
                let sdp = wrongFingerprint ? Self.withWrongFingerprint(offer) : offer
                try? answerer.setRemoteDescription(sdp, type: "offer")
                try? answerer.setLocalDescription(type: "answer")
            }
            if offerSent, !answerSent, let answer = Self.description(in: fromAnswerer) {
                answerSent = true
                try? offerer.setRemoteDescription(answer, type: "answer")
            }
            if answerSent {
                let toAnswerer = Self.candidates(in: fromOfferer)
                for (candidate, mid) in toAnswerer.dropFirst(offererCandidates) {
                    if let loopback = LoopbackCandidate.rewrite(candidate), added.insert("a" + loopback).inserted {
                        try? answerer.addRemoteCandidate(loopback, mid: mid)
                    }
                }
                offererCandidates = toAnswerer.count
                let toOfferer = Self.candidates(in: fromAnswerer)
                for (candidate, mid) in toOfferer.dropFirst(answererCandidates) {
                    if let loopback = LoopbackCandidate.rewrite(candidate), added.insert("o" + loopback).inserted {
                        try? offerer.addRemoteCandidate(loopback, mid: mid)
                    }
                }
                answererCandidates = toOfferer.count
            }
            try? await Task.sleep(for: .milliseconds(5))
        }
    }

    private static func description(in events: [RtcEvent]) -> String? {
        for event in events {
            if case .localDescription(let sdp, _) = event {
                return sdp
            }
        }
        return nil
    }

    private static func candidates(in events: [RtcEvent]) -> [(String, String)] {
        events.compactMap { event in
            if case .localCandidate(let candidate, let mid) = event { return (candidate, mid) }
            return nil
        }
    }

    /// The SDP with the first hex digit of its SHA-256 fingerprint changed.
    static func withWrongFingerprint(_ sdp: String) -> String {
        let prefix = "a=fingerprint:sha-256 "
        guard let range = sdp.range(of: prefix), range.upperBound < sdp.endIndex else {
            return sdp
        }
        let digit = sdp[range.upperBound]
        var changed = sdp
        changed.replaceSubrange(range.upperBound...range.upperBound, with: digit == "0" ? "1" : "0")
        return changed
    }
}

/// Carries each bridge's events to the pair once it exists.
private final class EventBox: @unchecked Sendable {
    private let lock = NSLock()
    private weak var storedPair: DataChannelTestPair?

    var pair: DataChannelTestPair? {
        get { lock.withLock { storedPair } }
        set { lock.withLock { storedPair = newValue } }
    }

    func offerer(_ event: RtcEvent) {
        pair?.record(offerer: event)
    }

    func answerer(_ event: RtcEvent) {
        pair?.record(answerer: event)
    }
}
