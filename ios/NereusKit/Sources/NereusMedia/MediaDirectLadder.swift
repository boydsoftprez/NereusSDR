// NereusSDR for iOS: the direct media ladder: a direct-only replace while media rides the tunnel, and the fallback when a direct path goes quiet
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The direct media ladder (the link document, sections 6.1 and 6.3; the
/// media control document, "Replacing the media connection"). The phone
/// declares `mediaDirect` 1; a Core that offers it sends `mediaDirectVersion`
/// 1 and `mediaStunUrls`, and takes a `replace` carrying
/// `"mediaDirectVersion": 1` as a direct-only connection: STUN and host
/// candidates, no tunnel and no relay.
///
/// ``MediaControlClient`` runs both halves on the phone:
///
/// - While media rides the Core's tunnel, a direct-only replace at each
///   step of ``directSteps`` (5, 30, 120 and 300 seconds, the last
///   repeating), skipped while this phone is keyed, has VOX armed, or the
///   Core is on the air. A step that is skipped, refused or not ready by
///   the connect deadline waits for the next. A new media start begins the
///   schedule again.
/// - On a direct path (not the tunnel, not a relay) with receive audio
///   wanted, when no audio or display packet arrives for ``silenceFallback``
///   while the control session still runs, the ordinary three-field replace
///   onto a connection that offers the tunnel alone, and the schedule starts
///   again at its first step. Once per silence. If no media has arrived one
///   window after that fallback finished, a new media start.
public enum MediaDirectLadder {
    /// The Core's `PathRacer::kUpgradeRetryMs` (src/core/session/PathRacer.h),
    /// the control upgrade schedule the direct-only replace follows.
    public static let directSteps: [Duration] = [.seconds(5), .seconds(30), .seconds(120), .seconds(300)]
    /// The Core's `RemoteMediaController::kDirectMediaSilenceFallbackMs`
    /// (src/gui/RemoteMediaController.h): 5000 ms, JJ's ruling of 2026-09-29.
    public static let silenceFallbackMs: UInt64 = 5_000
    public static var silenceFallback: Duration { .milliseconds(Int64(silenceFallbackMs)) }
    /// How soon a step, fallback or recovery that waited on transmit looks again.
    static let transmitPollMs: UInt64 = 500

    /// The capability that carries the Core's STUN servers.
    public static let stunUrlsCapability = "mediaStunUrls"

    /// The Core's `mediaStunUrls`: a compact JSON array of strings. Only
    /// `stun:` and `stuns:` URLs are kept, and none carrying `@` or `?`
    /// (never a relay, a credential or a token), in the Core's order. Nil
    /// when the text is not a JSON array of strings.
    public static func stunUrls(fromCapability text: String) -> [String]? {
        guard let data = text.data(using: .utf8),
              let array = try? JSONSerialization.jsonObject(with: data) as? [Any] else {
            return nil
        }
        var urls: [String] = []
        for entry in array {
            guard let url = entry as? String else { return nil }
            let lowered = url.lowercased()
            guard lowered.hasPrefix("stun:") || lowered.hasPrefix("stuns:"),
                  !url.contains(where: { $0 == "@" || $0 == "?" || $0.isWhitespace }) else { continue }
            urls.append(url)
        }
        return urls
    }
}

/// What the phone's transmit state allows the ladder to do, read before
/// every step, fallback and recovery. Nothing on the ladder changes media
/// while this phone is keyed.
public struct MediaLadderState: Sendable, Equatable {
    /// This phone is keying, keyed, or ending its transmit.
    public var keyed: Bool
    /// This phone has VOX armed.
    public var voxArmed: Bool
    /// The Core is on the air (any device's transmit, tune or two-tone).
    public var coreOnAir: Bool
    /// The control connection runs through a relay: media there keeps the
    /// stall rule, as on the tunnel.
    public var controlRelayed: Bool

    public init(keyed: Bool, voxArmed: Bool, coreOnAir: Bool, controlRelayed: Bool) {
        self.keyed = keyed
        self.voxArmed = voxArmed
        self.coreOnAir = coreOnAir
        self.controlRelayed = controlRelayed
    }

    /// A direct-only step may start.
    public var quiet: Bool { !keyed && !voxArmed && !coreOnAir }

    /// Whether a ladder replace of this kind may start: a direct-only step
    /// needs ``quiet``; the fallback needs only that nothing is keyed and
    /// the Core is not on the air (the Core refuses a replace then).
    func allows(_ kind: MediaControlClient.ReplacementKind) -> Bool {
        switch kind {
        case .direct: quiet
        case .tunnelFallback, .move: !keyed && !coreOnAir
        }
    }

    /// Nothing known: nothing starts.
    public static let unknown = MediaLadderState(keyed: true, voxArmed: true, coreOnAir: true,
                                                 controlRelayed: true)
}

/// The peers the ladder makes, and how it reads transmit. Set by the app
/// for each logical session (``MediaControlClient/useDirectLadder(_:owner:)``).
public struct DirectLadderPeers: Sendable {
    /// A direct-only replace's peer: STUN and host candidates, no tunnel,
    /// no relay (``MediaPeer/Configuration/directAddress(stunServers:)``
    /// on ``MediaPeer/Route/direct``).
    public var direct: MediaControlClient.PeerFactory
    /// The fallback's peer: the tunnel's candidate alone
    /// (``MediaPeer/Configuration/tunnelAlone``). Nil where media has no
    /// tunnel; the fallback then makes the client's ordinary peer.
    public var tunnelAlone: MediaControlClient.PeerFactory?
    public var state: @Sendable () async -> MediaLadderState

    public init(direct: @escaping MediaControlClient.PeerFactory,
                tunnelAlone: MediaControlClient.PeerFactory?,
                state: @escaping @Sendable () async -> MediaLadderState) {
        self.direct = direct
        self.tunnelAlone = tunnelAlone
        self.state = state
    }
}
