// NereusSDR for iOS: two in-process libdatachannel peers with a data channel only, for the DTLS fingerprint tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import LinkTestSupport
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
    private let diagnostic: HostedDiagnosticReceipts?
    private var emittedOffererCandidates = 0
    private var emittedAnswererCandidates = 0

    init(wrongFingerprint: Bool, diagnostic: HostedDiagnosticReceipts? = nil) throws {
        self.diagnostic = diagnostic
        diagnostic?.mark("pair construction entry")
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
        diagnostic?.mark("initial offer setLocal entry")
        do {
            try offerer.setLocalDescription(type: "offer")
            diagnostic?.mark("initial offer setLocal returned")
        } catch {
            diagnostic?.mark("initial offer setLocal threw " + Self.errorCode(error))
            throw error
        }
        diagnostic?.mark("pair construction returned")
    }

    deinit {
        close()
    }

    func close() {
        diagnostic?.mark("pair close entry")
        pump?.cancel()
        offerer.close()
        answerer.close()
        let counts = lock.withLock { (emittedOffererCandidates, emittedAnswererCandidates) }
        diagnostic?.mark("pair close returned; emitted offerer \(counts.0) answerer \(counts.1)")
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
        let entered = diagnostic == nil ? nil : HostedDiagnosticReceipts.capture("offerer callback entry")
        let captured = lock.withLock { () -> HostedDiagnosticReceipts.Captured? in
            offererEvents.append(event)
            if case .localCandidate = event { emittedOffererCandidates += 1 }
            return observation(event, side: "offerer", candidateCount: emittedOffererCandidates)
        }
        if let entered { diagnostic?.append(entered) }
        if let captured { diagnostic?.append(captured) }
    }

    fileprivate func record(answerer event: RtcEvent) {
        let entered = diagnostic == nil ? nil : HostedDiagnosticReceipts.capture("answerer callback entry")
        let captured = lock.withLock { () -> HostedDiagnosticReceipts.Captured? in
            answererEvents.append(event)
            if case .localCandidate = event { emittedAnswererCandidates += 1 }
            return observation(event, side: "answerer", candidateCount: emittedAnswererCandidates)
        }
        if let entered { diagnostic?.append(entered) }
        if let captured { diagnostic?.append(captured) }
    }

    /// Called under the existing event lock: capture only; append follows unlock.
    private func observation(_ event: RtcEvent, side: String, candidateCount: Int) -> HostedDiagnosticReceipts.Captured? {
        guard diagnostic != nil else { return nil }
        let stage: String
        switch event {
        case .localDescription(_, let type): stage = type == "offer" ? "description offer" : "description answer"
        case .localCandidate: guard candidateCount <= 32 else { return nil }; stage = "candidate emitted \(candidateCount)"
        case .peerState(let state): stage = "peer state \(state.rawValue)"
        case .dataChannel: stage = "channel handed"
        case .open: stage = "channel open"
        case .closed: stage = "channel closed"
        case .error: stage = "channel error"
        case .gatheringComplete: stage = "gathering complete"
        default: return nil
        }
        return HostedDiagnosticReceipts.capture(side + " " + stage)
    }

    private static func errorCode(_ error: Error) -> String {
        if let error = error as? MediaPeerError, case .library(_, let code) = error { return "library code \(code)" }
        return "other"
    }

    /// Records the original discarded failure and continues exactly as try? did.
    @discardableResult private func observeCall(_ name: String, _ call: () throws -> Void) -> Bool {
        diagnostic?.mark(name + " entry")
        do { try call(); diagnostic?.mark(name + " returned"); return true }
        catch { diagnostic?.mark(name + " threw " + Self.errorCode(error)); return false }
    }

    private func relay(wrongFingerprint: Bool) async {
        diagnostic?.mark("relay task entry")
        defer { diagnostic?.mark("relay task exit") }
        var offerSent = false
        var answerSent = false
        var offererCandidates = 0
        var answererCandidates = 0
        var added = Set<String>()
        var filtered = 0, rewritten = 0, deduplicated = 0, accepted = 0, rejected = 0
        defer { diagnostic?.mark("relay totals filtered \(filtered) rewritten \(rewritten) duplicates \(deduplicated) accepted \(accepted) rejected \(rejected)") }
        while !Task.isCancelled {
            let fromOfferer = offererLog
            let fromAnswerer = answererLog
            if !offerSent, let offer = Self.description(in: fromOfferer) {
                offerSent = true
                diagnostic?.mark("fixture offerSent flag true")
                let sdp = wrongFingerprint ? Self.withWrongFingerprint(offer) : offer
                diagnostic?.mark("offer mutation changed \(sdp != offer); wrong requested \(wrongFingerprint)")
                observeCall("answerer setRemote offer") { try answerer.setRemoteDescription(sdp, type: "offer") }
                observeCall("answerer setLocal answer") { try answerer.setLocalDescription(type: "answer") }
            }
            if offerSent, !answerSent, let answer = Self.description(in: fromAnswerer) {
                answerSent = true
                diagnostic?.mark("fixture answerSent flag true")
                observeCall("offerer setRemote answer") { try offerer.setRemoteDescription(answer, type: "answer") }
            }
            if answerSent {
                let toAnswerer = Self.candidates(in: fromOfferer)
                if toAnswerer.count != offererCandidates { diagnostic?.mark("offerer candidate relay batch from \(offererCandidates) to \(toAnswerer.count)") }
                for (candidate, mid) in toAnswerer.dropFirst(offererCandidates) {
                    if let loopback = LoopbackCandidate.rewrite(candidate) {
                        rewritten += 1
                        if added.insert("a" + loopback).inserted {
                            if observeCall("answerer addRemote candidate", { try answerer.addRemoteCandidate(loopback, mid: mid) }) {
                                accepted += 1
                            } else { rejected += 1 }
                        } else { deduplicated += 1 }
                    } else { filtered += 1 }
                }
                if toAnswerer.count != offererCandidates { diagnostic?.mark("relay batch totals filtered \(filtered) rewritten \(rewritten) duplicates \(deduplicated) accepted \(accepted) rejected \(rejected)") }
                offererCandidates = toAnswerer.count
                let toOfferer = Self.candidates(in: fromAnswerer)
                if toOfferer.count != answererCandidates { diagnostic?.mark("answerer candidate relay batch from \(answererCandidates) to \(toOfferer.count)") }
                for (candidate, mid) in toOfferer.dropFirst(answererCandidates) {
                    if let loopback = LoopbackCandidate.rewrite(candidate) {
                        rewritten += 1
                        if added.insert("o" + loopback).inserted {
                            if observeCall("offerer addRemote candidate", { try offerer.addRemoteCandidate(loopback, mid: mid) }) {
                                accepted += 1
                            } else { rejected += 1 }
                        } else { deduplicated += 1 }
                    } else { filtered += 1 }
                }
                if toOfferer.count != answererCandidates { diagnostic?.mark("relay batch totals filtered \(filtered) rewritten \(rewritten) duplicates \(deduplicated) accepted \(accepted) rejected \(rejected)") }
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
