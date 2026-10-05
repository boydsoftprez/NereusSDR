// NereusSDR for iOS: why the media peer refused signalling or a packet
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// Why a ``MediaPeer`` call was refused. The signalling limits are the media
/// control document's (`2026-09-20-remote-media-control-v1.md`, "Media
/// peer"), checked before anything reaches the transport library.
public enum MediaPeerError: Error, Equatable, Sendable, CustomStringConvertible {
    /// An SDP of more than ``MediaPeer/maxDescriptionBytes`` bytes.
    case descriptionTooLarge(bytes: Int)
    /// An empty SDP.
    case emptyDescription
    /// An SDP that embeds candidates; candidates travel only as trickle.
    case candidatesInDescription
    /// The remote description was already set; this version negotiates once.
    case remoteDescriptionAlreadySet
    /// A candidate of more than ``MediaPeer/maxCandidateBytes`` bytes, or empty.
    case invalidCandidateLength(bytes: Int)
    /// A mid of more than ``MediaPeer/maxMidBytes`` bytes, or empty.
    case invalidMidLength(bytes: Int)
    /// A NUL byte in an SDP, a candidate or a mid.
    case containsNul
    /// More than ``MediaPeer/maxRemoteCandidates`` remote candidates.
    case tooManyCandidates
    /// A candidate other than a host candidate while only host candidates are allowed.
    case notHostCandidate
    /// A relay candidate on a connection that takes none (a direct-address session).
    case relayCandidate
    /// A candidate from the Core on a connection that takes only its own
    /// route's (the tunnel alone, the direct media ladder's fallback).
    case notRouteCandidate
    /// An RTP packet the audio line cannot carry, or sent before the line is open.
    case audioNotReady
    /// An RTP packet longer than ``MediaPeer/maxRtpBytes`` bytes.
    case packetTooLarge(bytes: Int)
    /// A packet for the microphone line while this connection has no open
    /// line, or with an SSRC other than the line's.
    case microphoneNotReady
    /// A message for the "tx" data channel while it is not open.
    case txChannelNotReady
    /// The peer was closed.
    case closed
    /// libdatachannel returned this error code for the named call.
    case library(call: String, code: Int32)

    public var description: String {
        switch self {
        case .descriptionTooLarge(let bytes):
            return "a session description of \(bytes) bytes is over the limit"
        case .emptyDescription:
            return "the session description is empty"
        case .candidatesInDescription:
            return "the session description embeds candidates"
        case .remoteDescriptionAlreadySet:
            return "the remote session description was already set"
        case .invalidCandidateLength(let bytes):
            return "a candidate of \(bytes) bytes is outside the limit"
        case .invalidMidLength(let bytes):
            return "a mid of \(bytes) bytes is outside the limit"
        case .containsNul:
            return "signalling text holds a NUL byte"
        case .tooManyCandidates:
            return "too many remote candidates"
        case .notHostCandidate:
            return "only host candidates are accepted"
        case .relayCandidate:
            return "relay candidates are not accepted on this connection"
        case .notRouteCandidate:
            return "this connection takes only the tunnel's candidate"
        case .audioNotReady:
            return "the audio line is not open"
        case .packetTooLarge(let bytes):
            return "an RTP packet of \(bytes) bytes is over the limit"
        case .microphoneNotReady:
            return "the microphone line is not open"
        case .txChannelNotReady:
            return "the tx data channel is not open"
        case .closed:
            return "the media peer is closed"
        case .library(let call, let code):
            return "\(call) failed with libdatachannel error \(code)"
        }
    }
}
