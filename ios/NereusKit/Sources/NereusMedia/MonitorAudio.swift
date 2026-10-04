// NereusSDR for iOS: the transmit monitor's media op: where this phone asks the Core to put MON, and the Core's answer
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusLink

/// Which of this device's streams the Core puts the transmit monitor (MON)
/// in: the `route` of `monitor-audio` and of its `monitor-audio-context`
/// (the media control document, "Transmit monitor (monitor-audio)").
public enum MonitorRoute: String, Sendable, Equatable {
    /// The main stream.
    case speakers
    /// The headphones stream; the main stream for a device whose start did
    /// not declare the headphones mix, which this phone never does.
    case headphones
    /// Nowhere.
    case none
}

extension MediaControlEvent {
    /// An accepted `monitor-audio-context`: the route as the Core applied it
    /// for the request of this revision.
    public struct MonitorAudioContext: Sendable, Equatable {
        public var revision: UInt32
        public var route: MonitorRoute

        public init(revision: UInt32, route: MonitorRoute) {
            self.revision = revision
            self.route = route
        }
    }
}

extension MediaControlDecoder {
    /// `monitor-audio-context`: exactly `op`, `connectionId`, a nonzero
    /// uint32 `revision` and a known `route`.
    public static func monitorAudioContext(_ payload: [String: LinkJSON]) -> MediaControlEvent.MonitorAudioContext? {
        guard payload.count == 4, isOp(payload, "monitor-audio-context"),
              string(payload["connectionId"]) != nil,
              let revision = whole(payload["revision"], 1...maxUInt32),
              let name = string(payload["route"]),
              let route = MonitorRoute(rawValue: name) else {
            return nil
        }
        return MediaControlEvent.MonitorAudioContext(revision: UInt32(revision), route: route)
    }
}
