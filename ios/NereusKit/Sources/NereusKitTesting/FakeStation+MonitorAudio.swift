// NereusSDR for iOS: the fake Core's transmit monitor: txMonitorAudioVersion, monitor-audio and its context
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

extension FakeStation.Additions {
    /// The transmit monitor (the media control document, "Transmit monitor
    /// (monitor-audio)"): `txMonitorAudioVersion` 1. A media start that
    /// declares it may send `monitor-audio`, answered with one
    /// `monitor-audio-context` as the Core answers it. Not in ``all``; it
    /// needs media, so goes beside ``wideband``.
    public static let monitorAudio = FakeStation.Additions(rawValue: 1 << 45)
}

extension FakeStation {
    /// One `monitor-audio` the fake took.
    public struct MonitorAudioRequest: Sendable, Equatable {
        public var connectionId: String
        public var revision: UInt32
        public var route: String
    }

    /// What the app's media starts declared and asked of the monitor,
    /// behind its own lock.
    final class MonitorAudioState: @unchecked Sendable {
        private let lock = NSLock()
        private var declared = false
        private var lastRevision: UInt32 = 0
        private var requests: [MonitorAudioRequest] = []

        func read<Result>(_ body: (inout Bool, inout UInt32, inout [MonitorAudioRequest]) -> Result) -> Result {
            lock.withLock { body(&declared, &lastRevision, &requests) }
        }
    }

    /// Every `monitor-audio` the fake took, in order.
    public var monitorAudioRequests: [MonitorAudioRequest] {
        monitorAudioState.read { _, _, requests in requests }
    }

    /// Whether the app's latest media `start` declared `txMonitorAudioVersion`.
    public var monitorAudioDeclared: Bool {
        monitorAudioState.read { declared, _, _ in declared }
    }

    static func monitorAudioCapabilityVersions(_ additions: Additions) -> [(String, Int)] {
        additions.contains(.monitorAudio) ? [("txMonitorAudioVersion", 1)] : []
    }

    /// The fake's answer to a monitor operation; nil leaves the operation
    /// to the rest of the fake. A `start` is noted (the route is forgotten
    /// with each connection) and left to the rest.
    func monitorAudioReplies(_ payload: [String: LinkJSON]) -> [LinkMessage]? {
        guard additions.contains(.monitorAudio) else {
            return nil
        }
        if payload["op"] == .string("start") {
            monitorAudioState.read { declared, lastRevision, _ in
                declared = payload["txMonitorAudioVersion"] != nil
                lastRevision = 0
            }
            return nil
        }
        guard payload["op"] == .string("monitor-audio") else {
            return nil
        }
        // Exactly four keys, a nonzero uint32 revision above the last one
        // taken and a known route, from a start that declared the monitor;
        // anything else is ignored, as the Core ignores it.
        guard payload.count == 4, case .string(let id)? = payload["connectionId"],
              case .number(let number)? = payload["revision"], number >= 1, number <= Double(UInt32.max),
              number.rounded(.towardZero) == number,
              case .string(let route)? = payload["route"], ["speakers", "headphones", "none"].contains(route) else {
            return []
        }
        let revision = UInt32(number)
        let taken = monitorAudioState.read { declared, lastRevision, requests -> Bool in
            guard declared, lastRevision == 0 || Int32(bitPattern: revision &- lastRevision) > 0 else {
                return false
            }
            lastRevision = revision
            requests.append(MonitorAudioRequest(connectionId: id, revision: revision, route: route))
            return true
        }
        guard taken else {
            return []
        }
        // The app never declares the headphones mix, so the Core applies
        // headphones as its main stream.
        return [.mediaControl(LinkMessage.MediaControl(payload: [
            "op": .string("monitor-audio-context"),
            "connectionId": .string(id),
            "revision": .number(Double(revision)),
            "route": .string(route == "headphones" ? "speakers" : route),
        ]))]
    }
}
