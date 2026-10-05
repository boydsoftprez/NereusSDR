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

    typealias PhaseObserver = @Sendable (DtlsPhaseObservation) -> Void
    private let phaseObserver: PhaseObserver?

    let offerer: RtcBridge
    let answerer: RtcBridge
    let offererChannel: Int32

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var offererEvents: [RtcEvent] = []
    private var answererEvents: [RtcEvent] = []
    private var pump: Task<Void, Never>?

    init(wrongFingerprint: Bool, phaseObserver: PhaseObserver? = nil) throws {
        self.phaseObserver = phaseObserver
        phaseObserver?(DtlsPhaseObservation(phase: 1))
        var completed = false
        defer { phaseObserver?(DtlsPhaseObservation(phase: 2, first: completed ? 1 : 0)) }
        let box = EventBox()
        let createdOfferer = try Self.observedCall(1, side: 1, observer: phaseObserver) {
            try RtcBridge(settings: Self.settings) { box.offerer($0) }
        }
        let createdAnswerer = try Self.observedCall(2, side: 2, observer: phaseObserver) {
            try RtcBridge(settings: Self.settings) { box.answerer($0) }
        }
        offerer = createdOfferer
        answerer = createdAnswerer
        offererChannel = try Self.observedCall(3, side: 1, observer: phaseObserver) {
            try createdOfferer.createDataChannel(label: "control", unordered: false,
                                                       maxRetransmits: nil)
        }
        // Nothing happens on either peer before the offer is made below.
        box.pair = self
        pump = Task { [weak self] in
            await self?.relay(wrongFingerprint: wrongFingerprint)
        }
        try Self.observedCall(4, side: 1, observer: phaseObserver) {
            try offerer.setLocalDescription(type: "offer")
        }
        completed = true
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
        lock.withLock {
            offererEvents.append(event)
            phaseObserver?(Self.eventObservation(event, side: 1, count: offererEvents.count))
        }
    }

    fileprivate func record(answerer event: RtcEvent) {
        lock.withLock {
            answererEvents.append(event)
            phaseObserver?(Self.eventObservation(event, side: 2, count: answererEvents.count))
        }
    }

    private func relay(wrongFingerprint: Bool) async {
        phaseObserver?(DtlsPhaseObservation(phase: 3))
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
                if let phaseObserver {
                    phaseObserver(DtlsPhaseObservation(phase: 7,
                        first: offer.contains("a=fingerprint:sha-256 ") ? 1 : 0,
                        second: sdp != offer ? 1 : 0))
                }
                try? Self.observedCall(5, side: 2, observer: phaseObserver) {
                    try answerer.setRemoteDescription(sdp, type: "offer")
                }
                try? Self.observedCall(6, side: 2, observer: phaseObserver) {
                    try answerer.setLocalDescription(type: "answer")
                }
            }
            if offerSent, !answerSent, let answer = Self.description(in: fromAnswerer) {
                answerSent = true
                try? Self.observedCall(7, side: 1, observer: phaseObserver) {
                    try offerer.setRemoteDescription(answer, type: "answer")
                }
            }
            if answerSent {
                let toAnswerer = Self.candidates(in: fromOfferer)
                for (candidate, mid) in toAnswerer.dropFirst(offererCandidates) {
                    if let loopback = observedRewrite(candidate, side: 2), added.insert("a" + loopback).inserted {
                        phaseObserver?(DtlsPhaseObservation(phase: 8, side: 2, detail: 2))
                        try? Self.observedCall(8, side: 2, observer: phaseObserver) {
                            try answerer.addRemoteCandidate(loopback, mid: mid)
                        }
                    }
                }
                offererCandidates = toAnswerer.count
                let toOfferer = Self.candidates(in: fromAnswerer)
                for (candidate, mid) in toOfferer.dropFirst(answererCandidates) {
                    if let loopback = observedRewrite(candidate, side: 1), added.insert("o" + loopback).inserted {
                        phaseObserver?(DtlsPhaseObservation(phase: 8, side: 1, detail: 2))
                        try? Self.observedCall(9, side: 1, observer: phaseObserver) {
                            try offerer.addRemoteCandidate(loopback, mid: mid)
                        }
                    }
                }
                answererCandidates = toOfferer.count
            }
            try? await Task.sleep(for: .milliseconds(5))
        }
    }

    // Diagnostic-only calls keep the original result/throw and the caller's try? suppression.
    private static func observedCall<T>(_ call: Int, side: Int, observer: PhaseObserver?,
                                        _ body: () throws -> T) rethrows -> T {
        guard let observer else { return try body() }
        observer(DtlsPhaseObservation(phase: 4, side: side, detail: call))
        do {
            let result = try body()
            observer(DtlsPhaseObservation(phase: 5, side: side, detail: call, first: 1))
            return result
        } catch {
            var libraryCode: Int64 = 0
            var hasLibraryCode: Int64 = 0
            if let mediaError = error as? MediaPeerError,
               case .library(_, let code) = mediaError {
                libraryCode = Int64(code)
                hasLibraryCode = 1
            }
            observer(DtlsPhaseObservation(phase: 5, side: side, detail: call,
                                          second: hasLibraryCode, third: libraryCode))
            throw error
        }
    }

    private func observedRewrite(_ candidate: String, side: Int) -> String? {
        let rewritten = LoopbackCandidate.rewrite(candidate)
        phaseObserver?(DtlsPhaseObservation(phase: 8, side: side, detail: 1,
                                           first: rewritten != nil ? 1 : 0))
        return rewritten
    }

    private static func eventObservation(_ event: RtcEvent, side: Int, count: Int) -> DtlsPhaseObservation {
        let type: Int
        var state: Int64 = -1
        switch event {
        case .localDescription: type = 1
        case .localCandidate: type = 2
        case .peerState(let value): type = 3; state = Int64(value.rawValue)
        case .dataChannel: type = 4
        case .track: type = 5
        case .open: type = 6
        case .closed: type = 7
        case .error: type = 8
        case .message: type = 9
        case .textMessage: type = 10
        case .gatheringComplete: type = 11
        }
        return DtlsPhaseObservation(phase: 6, side: side, detail: type,
                                    first: Int64(count), second: state)
    }

    // Only existing Swift event histories are inspected; no native getter is called.
    func phaseSnapshots() -> [DtlsPhaseObservation] {
        lock.withLock {
            [(1, offererEvents), (2, answererEvents)].map { side, events in
                let states = events.compactMap { event -> RtcPeerState? in
                    if case .peerState(let state) = event { return state }
                    return nil
                }
                let handed = events.compactMap { event -> Int32? in
                    if case .dataChannel(let id) = event { return id }
                    return nil
                }
                let opened = events.filter { event in
                    if case .open(let id) = event {
                        return side == 1 ? id == offererChannel : handed.contains(id)
                    }
                    return false
                }.count
                return DtlsPhaseObservation(phase: 10, side: side,
                    first: Int64(events.count), second: states.last.map { Int64($0.rawValue) } ?? -1,
                    third: Int64(opened))
            }
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

/// Numeric fields only: no native strings, identifiers, addresses or payloads.
struct DtlsPhaseObservation: Sendable {
    let phase: Int
    var side: Int = 0
    var detail: Int = 0
    var first: Int64 = 0
    var second: Int64 = 0
    var third: Int64 = 0
}
