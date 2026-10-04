// NereusSDR for iOS: application payload counts for one media peer lifetime
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// Exact cumulative payload counts for one peer. RTP includes its header
/// and packets later rejected by parsing, SSRC or a bounded receive queue.
/// Submitted bytes are local attempts after the peer's admission gates,
/// including a later native send failure, and do not prove delivery.
public struct MediaTrafficObservation: Sendable, Equatable {
    public let lifetime: UUID
    public let active: Bool
    public let receivedDisplayPayloadBytes: UInt64
    public let receivedRtpBytes: UInt64
    /// The phone has no outbound display channel; this is measured zero.
    public let submittedDisplayPayloadBytes: UInt64
    public let submittedRtpBytes: UInt64
    /// TX channel messages are separate from the desktop's display/RTP total.
    public let submittedTxChannelBytes: UInt64

    public init(lifetime: UUID, active: Bool, receivedDisplayPayloadBytes: UInt64,
                receivedRtpBytes: UInt64, submittedDisplayPayloadBytes: UInt64 = 0,
                submittedRtpBytes: UInt64, submittedTxChannelBytes: UInt64) {
        self.lifetime = lifetime
        self.active = active
        self.receivedDisplayPayloadBytes = receivedDisplayPayloadBytes
        self.receivedRtpBytes = receivedRtpBytes
        self.submittedDisplayPayloadBytes = submittedDisplayPayloadBytes
        self.submittedRtpBytes = submittedRtpBytes
        self.submittedTxChannelBytes = submittedTxChannelBytes
    }
}
