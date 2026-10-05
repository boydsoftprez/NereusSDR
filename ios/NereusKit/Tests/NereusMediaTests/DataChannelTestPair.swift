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
    private let observer: (@Sendable (DtlsDiagnosticReceipt) -> Void)?

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var offererEvents: [RtcEvent] = []
    private var answererEvents: [RtcEvent] = []
    private var pump: Task<Void, Never>?

    init(wrongFingerprint: Bool, observer: (@Sendable (DtlsDiagnosticReceipt) -> Void)? = nil) throws {
        self.observer = observer
        observer?(.init(phase: .setupEntry))
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
        if let observer {
            try Self.observedCall(.localOffer, side: .offerer, observer: observer) {
                try offerer.setLocalDescription(type: "offer")
            }
        } else {
            try offerer.setLocalDescription(type: "offer")
        }
        observer?(.init(phase: .setupExit))
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
        if let observer {
            observer(Self.eventReceipt(event, side: .offerer, events: offererLog))
        }
    }

    fileprivate func record(answerer event: RtcEvent) {
        lock.withLock { answererEvents.append(event) }
        if let observer {
            observer(Self.eventReceipt(event, side: .answerer, events: answererLog))
        }
    }

    private func relay(wrongFingerprint: Bool) async {
        observer?(.init(phase: .relayEntry))
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
                if let observer {
                    observer(.init(phase: .mutation, side: .offerer,
                                   prefixPresent: offer.contains("a=fingerprint:sha-256 "), changed: sdp != offer))
                    try? Self.observedCall(.remoteOffer, side: .answerer, observer: observer) {
                        try answerer.setRemoteDescription(sdp, type: "offer")
                    }
                    try? Self.observedCall(.localAnswer, side: .answerer, observer: observer) {
                        try answerer.setLocalDescription(type: "answer")
                    }
                } else {
                    try? answerer.setRemoteDescription(sdp, type: "offer")
                    try? answerer.setLocalDescription(type: "answer")
                }
            }
            if offerSent, !answerSent, let answer = Self.description(in: fromAnswerer) {
                answerSent = true
                if let observer {
                    try? Self.observedCall(.remoteAnswer, side: .offerer, observer: observer) {
                        try offerer.setRemoteDescription(answer, type: "answer")
                    }
                } else {
                    try? offerer.setRemoteDescription(answer, type: "answer")
                }
            }
            if answerSent {
                let toAnswerer = Self.candidates(in: fromOfferer)
                for (candidate, mid) in toAnswerer.dropFirst(offererCandidates) {
                    if let loopback = LoopbackCandidate.rewrite(candidate), added.insert("a" + loopback).inserted {
                        if let observer {
                            observer(.init(phase: .candidateAdmitted, side: .answerer))
                            try? Self.observedCall(.remoteCandidate, side: .answerer, observer: observer) {
                                try answerer.addRemoteCandidate(loopback, mid: mid)
                            }
                        } else {
                            try? answerer.addRemoteCandidate(loopback, mid: mid)
                        }
                    }
                }
                if let observer, offererCandidates != toAnswerer.count {
                    observer(.init(phase: .candidateCursor, side: .answerer, count: toAnswerer.count))
                }
                offererCandidates = toAnswerer.count
                let toOfferer = Self.candidates(in: fromAnswerer)
                for (candidate, mid) in toOfferer.dropFirst(answererCandidates) {
                    if let loopback = LoopbackCandidate.rewrite(candidate), added.insert("o" + loopback).inserted {
                        if let observer {
                            observer(.init(phase: .candidateAdmitted, side: .offerer))
                            try? Self.observedCall(.remoteCandidate, side: .offerer, observer: observer) {
                                try offerer.addRemoteCandidate(loopback, mid: mid)
                            }
                        } else {
                            try? offerer.addRemoteCandidate(loopback, mid: mid)
                        }
                    }
                }
                if let observer, answererCandidates != toOfferer.count {
                    observer(.init(phase: .candidateCursor, side: .offerer, count: toOfferer.count))
                }
                answererCandidates = toOfferer.count
            }
            try? await Task.sleep(for: .milliseconds(5))
        }
    }

    // Diagnostic-only observations remain synchronous on the existing flow.
    // With no observer, none of these snapshots or receipt values are evaluated.
    func diagnosticSnapshot(_ phase: DtlsDiagnosticReceipt.Phase) {
        guard let observer else { return }
        observer(Self.snapshot(phase, side: .offerer, events: offererLog))
        observer(Self.snapshot(phase, side: .answerer, events: answererLog))
    }

    private static func observedCall(_ call: DtlsDiagnosticReceipt.Call,
                                     side: DtlsDiagnosticReceipt.Side,
                                     observer: @Sendable (DtlsDiagnosticReceipt) -> Void,
                                     _ operation: () throws -> Void) rethrows {
        observer(.init(phase: .callBefore, side: side, call: call))
        do {
            try operation()
            observer(.init(phase: .callAfter, side: side, call: call, code: 0, success: true))
        } catch {
            let code: Int32?
            if let mediaError = error as? MediaPeerError, case let .library(_, value) = mediaError {
                code = value
            } else {
                code = nil
            }
            observer(.init(phase: .callAfter, side: side, call: call, code: code, success: false))
            throw error
        }
    }

    private static func snapshot(_ phase: DtlsDiagnosticReceipt.Phase,
                                 side: DtlsDiagnosticReceipt.Side, events: [RtcEvent]) -> DtlsDiagnosticReceipt {
        var descriptions = 0
        var candidates = 0
        var opened = 0
        var state: Int32?
        var failed = false
        var closed = false
        var connected = false
        for event in events {
            switch event {
            case .localDescription: descriptions += 1
            case .localCandidate: candidates += 1
            case .open: opened += 1
            case .peerState(let value):
                state = value.rawValue
                failed = failed || value == .failed
                closed = closed || value == .closed
                connected = connected || value == .connected
            default: break
            }
        }
        return .init(phase: phase, side: side, state: state, count: events.count,
                     descriptions: descriptions, candidates: candidates, opened: opened,
                     failed: failed, closed: closed, connected: connected)
    }

    private static func eventReceipt(_ event: RtcEvent, side: DtlsDiagnosticReceipt.Side,
                                     events: [RtcEvent]) -> DtlsDiagnosticReceipt {
        var receipt = snapshot(.event, side: side, events: events)
        switch event {
        case .localDescription: receipt.event = .localDescription
        case .localCandidate: receipt.event = .localCandidate
        case .peerState(let state):
            receipt.event = .peerState
            receipt.state = state.rawValue
        case .dataChannel: receipt.event = .dataChannel
        case .track: receipt.event = .track
        case .open: receipt.event = .open
        case .closed: receipt.event = .closed
        case .error: receipt.event = .error
        case .message: receipt.event = .message
        case .textMessage: receipt.event = .textMessage
        case .gatheringComplete: receipt.event = .gatheringComplete
        }
        return receipt
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

// Fixed diagnostic fields only: never signalling, addresses, messages or certificates.
struct DtlsDiagnosticReceipt: Sendable {
    enum Phase: String, Sendable {
        case setupEntry, setupExit, relayEntry, mutation, callBefore, callAfter
        case candidateAdmitted, candidateCursor, event, waitEntry, waitTimeout, waitDone, finalSnapshot
    }
    enum Side: String, Sendable { case offerer, answerer }
    enum Call: String, Sendable { case localOffer, remoteOffer, localAnswer, remoteAnswer, remoteCandidate }
    enum Event: String, Sendable {
        case localDescription, localCandidate, peerState, dataChannel, track, open, closed
        case error, message, textMessage, gatheringComplete
    }
    let phase: Phase
    var side: Side? = nil
    var call: Call? = nil
    var event: Event? = nil
    var code: Int32? = nil
    var success: Bool? = nil
    var prefixPresent: Bool? = nil
    var changed: Bool? = nil
    var state: Int32? = nil
    var count: Int? = nil
    var descriptions: Int? = nil
    var candidates: Int? = nil
    var opened: Int? = nil
    var failed: Bool? = nil
    var closed: Bool? = nil
    var connected: Bool? = nil
}
