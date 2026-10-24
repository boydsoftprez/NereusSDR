// NereusSDR for iOS: what the display decoder has counted across its life
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-22: NSDC v2 decoder counters (display codec v2 document, section 9.2).
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

/// The decoder's running counts, written from
/// docs/architecture/2026-10-22-display-codec-v2-intra-rows.md section 9.2.
/// They feed the display diagnostics and survive `DisplayFrameDecoder.reset()`.
public struct DisplayDecodeCounters: Equatable, Sendable {
    /// Frames that never arrived: the sequence distance between two released
    /// frames of one context, less one.
    public var framesLost: UInt64
    /// Split frames released with only one of their two parts.
    public var partsLost: UInt64
    /// Messages refused as not newer than the last released frame.
    public var staleMessages: UInt64
    /// A part arriving again while it is already held.
    public var duplicateParts: UInt64
    /// Structural refusals, and pairing faults the table calls malformed.
    public var malformed: UInt64

    public init(framesLost: UInt64 = 0, partsLost: UInt64 = 0, staleMessages: UInt64 = 0,
                duplicateParts: UInt64 = 0, malformed: UInt64 = 0) {
        self.framesLost = framesLost
        self.partsLost = partsLost
        self.staleMessages = staleMessages
        self.duplicateParts = duplicateParts
        self.malformed = malformed
    }
}
