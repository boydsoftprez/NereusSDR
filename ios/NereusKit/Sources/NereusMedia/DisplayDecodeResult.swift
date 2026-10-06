// NereusSDR for iOS: the result of decoding one display datagram
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The result of `DisplayFrameDecoder.decode(_:)`: a frame only when the
/// disposition is `accepted`.
public struct DisplayDecodeResult: Equatable, Sendable {
    public let disposition: DisplayDecodeDisposition
    public let reason: DisplayDecodeReason
    public let frame: DisplayFrame?

    public init(disposition: DisplayDecodeDisposition, reason: DisplayDecodeReason, frame: DisplayFrame?) {
        self.disposition = disposition
        self.reason = reason
        self.frame = frame
    }
}
