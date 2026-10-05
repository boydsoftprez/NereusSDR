// NereusSDR for iOS: the lossless profile's L16 packets: 48 kHz stereo, 192 frames of 16-bit big-endian samples
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

/// The lossless profile's packets, as the Core's L16 encoder describes
/// them (the media control document: codec `l16`, 48000 Hz, two channels,
/// 192 frames, 16 bits, payload type 96): 768 bytes of interleaved
/// big-endian samples, one packet every 4 ms, about 1.6 Mbit/s. The
/// Core's quantiser clips to plus or minus 1, scales by 32768, rounds to
/// nearest and holds the top at 32767; a sample decodes as code / 32768,
/// so every code round-trips.
public enum L16Audio {
    public static let payloadType: UInt8 = 96
    public static let packetFrames = 192
    public static let channels = 2
    public static let payloadBytes = packetFrames * channels * 2

    /// One sample as its 16-bit code. Not a number is silence.
    public static func quantise(_ sample: Float) -> Int16 {
        guard !sample.isNaN else {
            return 0
        }
        let clipped = Double(min(max(sample, -1), 1))
        let scaled = (clipped * 32768).rounded(.toNearestOrAwayFromZero)
        return Int16(min(max(scaled, -32768), 32767))
    }

    /// One packet's interleaved stereo samples, or nil when the payload is
    /// not exactly one packet.
    public static func decode(_ payload: Data) -> [Float]? {
        guard payload.count == payloadBytes else {
            return nil
        }
        var samples = [Float](repeating: 0, count: packetFrames * channels)
        payload.withUnsafeBytes { (raw: UnsafeRawBufferPointer) in
            for index in 0..<samples.count {
                let code = Int16(bitPattern: UInt16(raw[2 * index]) << 8 | UInt16(raw[2 * index + 1]))
                samples[index] = Float(code) / 32768
            }
        }
        return samples
    }

    /// One 20 ms microphone frame (960 mono samples) as five packets'
    /// payloads back to back, for ``MediaUplink/sendMicrophoneL16(_:)``.
    public static func microphoneFrame(mono: [Float]) -> Data {
        var frame = Data()
        frame.reserveCapacity(mono.count / packetFrames * payloadBytes)
        var start = 0
        while start + packetFrames <= mono.count {
            frame.append(stereoPayload(mono: mono[start..<(start + packetFrames)]))
            start += packetFrames
        }
        return frame
    }

    /// One packet from `packetFrames` mono samples, each in both channels,
    /// as the desktop sends its microphone (the Core takes their mean).
    public static func stereoPayload(mono: ArraySlice<Float>) -> Data {
        precondition(mono.count == packetFrames)
        var bytes = [UInt8](repeating: 0, count: payloadBytes)
        var offset = 0
        for sample in mono {
            let bits = UInt16(bitPattern: quantise(sample))
            let high = UInt8(bits >> 8)
            let low = UInt8(bits & 0xff)
            bytes[offset] = high
            bytes[offset + 1] = low
            bytes[offset + 2] = high
            bytes[offset + 3] = low
            offset += 4
        }
        return Data(bytes)
    }
}
