// NereusSDR for iOS: what became of one display datagram the phone decoded
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The outcome of decoding one display datagram. The names match the
/// station's decoder and the link's conformance vectors.
public enum DisplayDecodeDisposition: String, Equatable, Sendable {
    /// The frame decoded and is now the endpoint's history.
    case accepted
    /// The datagram cannot be used until the Core sends a keyframe.
    case needKeyframe
    /// The datagram was refused; the history is unchanged.
    case rejected
}
