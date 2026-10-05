// NereusSDR for iOS: why an Opus encoder or decoder refused its work
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

internal import COpus

/// Why an ``OpusDecoder`` or ``OpusEncoder`` could not do what it was asked.
public enum OpusCodecError: Error, Equatable, Sendable, CustomStringConvertible {
    /// Opus codes one or two channels only.
    case unsupportedChannelCount(Int)
    /// A packet with no bytes; a lost packet is concealed with `concealLoss(frames:)`.
    case emptyPacket
    /// A frame count the codec cannot produce or take.
    case invalidFrameCount(Int)
    /// The encoder takes exactly one frame of samples at a time.
    case wrongSampleCount(expected: Int, got: Int)
    /// libopus returned this error code.
    case library(code: Int32)

    public var description: String {
        switch self {
        case .unsupportedChannelCount(let channels):
            return "Opus codes 1 or 2 channels, not \(channels)"
        case .emptyPacket:
            return "an Opus packet has at least one byte"
        case .invalidFrameCount(let frames):
            return "\(frames) is not a frame count Opus can code"
        case .wrongSampleCount(let expected, let got):
            return "expected \(expected) samples, got \(got)"
        case .library(let code):
            return "libopus error \(code): " + String(cString: opus_strerror(code))
        }
    }
}
