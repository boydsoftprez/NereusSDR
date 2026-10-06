// NereusSDR for iOS: the microphone's samples from the audio input thread to the encoder, lock-free and without allocating on the input thread
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CAudioRing
import Foundation

/// One channel of the microphone at the input's own rate, between the
/// audio input thread (the only writer) and the encoder's queue (the only
/// reader). Writing never locks or allocates, so the input thread only
/// copies; the conversion to 48 kHz and the encoding happen on the reader's
/// side. Samples that find the ring full are dropped and counted.
public final class MicrophoneRing: @unchecked Sendable {
    private let ring: OpaquePointer
    private let overflowCount: OpaquePointer

    /// A ring of `capacity` samples, or nil when the memory cannot be had.
    public init?(capacity: Int) {
        guard capacity > 0,
              let ring = nereus_audio_ring_create(capacity, 1) else {
            return nil
        }
        guard let overflow = nereus_shared_value_create(0) else {
            nereus_audio_ring_destroy(ring)
            return nil
        }
        self.ring = ring
        overflowCount = overflow
    }

    deinit {
        nereus_audio_ring_destroy(ring)
        nereus_shared_value_destroy(overflowCount)
    }

    /// Writer side: copies up to `count` samples in and returns how many fit.
    @discardableResult
    public func write(_ samples: UnsafePointer<Float>, count: Int) -> Int {
        let written = nereus_audio_ring_write(ring, samples, count)
        if written < count {
            nereus_shared_value_store(overflowCount,
                                      nereus_shared_value_load(overflowCount) + Int64(count - written))
        }
        return written
    }

    /// Reader side: everything waiting, oldest first.
    public func readAll() -> [Float] {
        let waiting = nereus_audio_ring_readable(ring)
        guard waiting > 0 else {
            return []
        }
        var samples = [Float](repeating: 0, count: waiting)
        let read = samples.withUnsafeMutableBufferPointer { buffer in
            nereus_audio_ring_read(ring, buffer.baseAddress, waiting)
        }
        if read < waiting {
            samples.removeLast(waiting - read)
        }
        return samples
    }

    /// Samples dropped because the ring was full.
    public var overflowed: Int {
        Int(nereus_shared_value_load(overflowCount))
    }
}
