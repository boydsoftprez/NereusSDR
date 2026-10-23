// NereusSDR for iOS: the result of decoding one display datagram
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-10-22: NSDC v2 releases zero, one or two frames per message (frames).
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

/// The result of `DisplayFrameDecoder.decode(_:)`: frames only when the
/// disposition is `accepted`. NSDC v1 releases at most one frame; NSDC v2
/// releases one or two, oldest first.
public struct DisplayDecodeResult: Equatable, Sendable {
    public let disposition: DisplayDecodeDisposition
    public let reason: DisplayDecodeReason
    /// The first released frame, `frames.first`.
    public let frame: DisplayFrame?
    /// Every released frame, oldest first: none, one, or (NSDC v2) two.
    public let frames: [DisplayFrame]

    public init(disposition: DisplayDecodeDisposition, reason: DisplayDecodeReason, frame: DisplayFrame?) {
        self.init(disposition: disposition, reason: reason, frames: frame.map { [$0] } ?? [])
    }

    public init(disposition: DisplayDecodeDisposition, reason: DisplayDecodeReason, frames: [DisplayFrame]) {
        self.disposition = disposition
        self.reason = reason
        self.frame = frames.first
        self.frames = frames
    }
}
