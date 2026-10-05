// NereusSDR for iOS: decodes the Core's Opus audio to 48 kHz interleaved float samples
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
internal import COpus

/// One Opus decoder at 48000 Hz. Output is interleaved, `channels` floats per
/// frame, nominally within -1...1. A decoder keeps state from packet to
/// packet, so one stream uses one decoder, packets in order.
public final class OpusDecoder {
    /// The rate every decoder runs at.
    public static let sampleRate = 48000
    /// The most frames one packet can hold: 120 ms at 48000 Hz.
    public static let maximumFrames = 5760

    public let channels: Int
    private let state: OpaquePointer

    public init(channels: Int) throws {
        guard channels == 1 || channels == 2 else {
            throw OpusCodecError.unsupportedChannelCount(channels)
        }
        var error: Int32 = OPUS_OK
        let created = opus_decoder_create(Int32(Self.sampleRate), Int32(channels), &error)
        guard let created, error == OPUS_OK else {
            if let created {
                opus_decoder_destroy(created)
            }
            throw OpusCodecError.library(code: error)
        }
        self.channels = channels
        self.state = created
    }

    deinit {
        opus_decoder_destroy(state)
    }

    /// Decodes one Opus packet (the RTP payload, not the RTP packet).
    public func decode(_ packet: Data) throws -> [Float] {
        guard !packet.isEmpty else {
            throw OpusCodecError.emptyPacket
        }
        var pcm = [Float](repeating: 0, count: Self.maximumFrames * channels)
        let decoded: Int32 = packet.withUnsafeBytes { raw in
            pcm.withUnsafeMutableBufferPointer { out in
                opus_decode_float(state,
                                  raw.bindMemory(to: UInt8.self).baseAddress,
                                  Int32(raw.count),
                                  out.baseAddress!,
                                  Int32(Self.maximumFrames),
                                  0)
            }
        }
        return try trimmed(pcm, decoded: decoded)
    }

    /// Fills `frames` frames of audio that did not arrive, continuing from the
    /// last packet decoded. `frames` is a multiple of 2.5 ms (120 frames).
    public func concealLoss(frames: Int) throws -> [Float] {
        guard frames > 0, frames <= Self.maximumFrames else {
            throw OpusCodecError.invalidFrameCount(frames)
        }
        var pcm = [Float](repeating: 0, count: frames * channels)
        let decoded: Int32 = pcm.withUnsafeMutableBufferPointer { out in
            opus_decode_float(state, nil, 0, out.baseAddress!, Int32(frames), 0)
        }
        return try trimmed(pcm, decoded: decoded)
    }

    private func trimmed(_ pcm: [Float], decoded: Int32) throws -> [Float] {
        guard decoded >= 0 else {
            throw OpusCodecError.library(code: decoded)
        }
        return Array(pcm.prefix(Int(decoded) * channels))
    }
}
