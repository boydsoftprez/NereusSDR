// NereusSDR for iOS: carries signalling between the media peer and the station's offerer in the interop tests
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

// Foundation's Process, which runs the station's helper, exists only on
// macOS; on the iOS simulator the interop tests are not built.
#if os(macOS)

import Foundation
@testable import NereusMedia

/// Stands in for the session's signalling in the interop tests: the Core's
/// offer and candidates go to the ``MediaPeer``, and its answer and
/// candidates go back, all as the media control document carries them.
/// Every candidate's IPv4 host address is replaced by 127.0.0.1 on the way
/// (IPv6 ones are left out), so the media between the two processes runs
/// over the loopback interface only, whatever interfaces the machine has.
final class InteropRelay: @unchecked Sendable {
    let peer: MediaPeer
    let offerer: OffererProcess

    // Everything below is read and written under `lock`.
    private let lock = NSLock()
    private var answers: [String] = []
    private var candidates: [(candidate: String, mid: String)] = []
    private var states: [MediaPeer.State] = []
    private var refusals: [String] = []
    private var tasks: [Task<Void, Never>] = []

    init(peer: MediaPeer, offerer: OffererProcess) {
        self.peer = peer
        self.offerer = offerer
        let toOfferer = Task { [peer, offerer, weak self] in
            for await answer in peer.localDescription {
                self?.lock.withLock { self?.answers.append(answer) }
                offerer.send(["type": "description", "sdpType": "answer", "sdp": answer])
            }
        }
        let candidatesToOfferer = Task { [peer, offerer, weak self] in
            var sent = Set<String>()
            for await local in peer.localCandidates {
                self?.lock.withLock { self?.candidates.append(local) }
                if let loopback = LoopbackCandidate.rewrite(local.candidate), sent.insert(loopback).inserted {
                    offerer.send(["type": "candidate", "candidate": loopback, "mid": local.mid])
                }
            }
        }
        let stateWatch = Task { [peer, weak self] in
            for await state in peer.state {
                self?.lock.withLock { self?.states.append(state) }
            }
        }
        let fromOfferer = Task { [peer, offerer, weak self] in
            var handled = 0
            var added = Set<String>()
            while !Task.isCancelled {
                let lines = offerer.received
                while handled < lines.count {
                    let line = lines[handled]
                    handled += 1
                    self?.deliver(line, to: peer, added: &added)
                }
                try? await Task.sleep(for: .milliseconds(5))
            }
        }
        tasks = [toOfferer, candidatesToOfferer, stateWatch, fromOfferer]
    }

    deinit {
        tasks.forEach { $0.cancel() }
    }

    /// Ends the relay, the peer and the helper.
    func stop() {
        tasks.forEach { $0.cancel() }
        peer.close()
        offerer.stop()
    }

    var answer: String? {
        lock.withLock { answers.first }
    }

    var localCandidates: [(candidate: String, mid: String)] {
        lock.withLock { candidates }
    }

    var stateHistory: [MediaPeer.State] {
        lock.withLock { states }
    }

    /// Signalling the peer refused, with the reason.
    var refused: [String] {
        lock.withLock { refusals }
    }

    /// Waits until `condition` holds, polling, or throws at the deadline.
    func waitUntil(_ what: String, timeout: Duration = .seconds(10),
                   _ condition: () -> Bool) async throws {
        let clock = ContinuousClock()
        let deadline = clock.now + timeout
        while !condition() {
            guard clock.now < deadline else {
                throw OffererProcess.Failure(description: "timed out waiting for \(what)")
            }
            try await Task.sleep(for: .milliseconds(10))
        }
    }

    /// Waits until the peer is connected and the helper's transport is ready.
    func waitUntilConnected() async throws {
        try await waitUntil("the media peer to connect") {
            stateHistory.contains(.connected)
        }
        _ = try await offerer.waitFor("ready")
    }

    private func deliver(_ line: OffererProcess.Line, to peer: MediaPeer, added: inout Set<String>) {
        do {
            switch line.type {
            case "description":
                try peer.setRemoteDescription(line.strings["sdp"] ?? "")
            case "candidate":
                if let loopback = LoopbackCandidate.rewrite(line.strings["candidate"] ?? ""),
                   added.insert(loopback).inserted {
                    try peer.addRemoteCandidate(loopback, mid: line.strings["mid"] ?? "")
                }
            default:
                break
            }
        } catch {
            lock.withLock { refusals.append("\(line.type): \(error)") }
        }
    }
}

#endif
