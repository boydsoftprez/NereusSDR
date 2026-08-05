// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/audio/RadeTxFilters.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file.  DSP helpers for the RADE TX path:
//
//   * RadeTxHpf80   80 Hz Butterworth HPF biquad (direct-form II
//                    transposed) for mic conditioning before the
//                    RADE neural codec's LPCNet feature extractor.
//
//   * RadeTx48to16  48 kHz -> 16 kHz mono downsampler wrapping
//                    r8brain CDSPResampler24 (vendored at
//                    third_party/r8brain/, MIT license).  RADE's
//                    LPCNet expects 16 kHz mono float32 frames; the
//                    TX worker thread runs at 48 kHz so a 3:1
//                    decimation sits in front of every encode call.
//
// Both helpers are standalone DSP units with no thread affinity of
// their own; the caller (K-bench's TxWorkerThread RADE branch) is
// responsible for not driving the same instance from multiple
// threads concurrently.  Each instance is single-channel mono.
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task K3. Initial
//                 implementation. NereusSDR-native; HPF coefficient
//                 derivation cites Robert Bristow-Johnson's "Audio
//                 EQ Cookbook" (the canonical reference for digital
//                 biquad design).  r8brain integration follows the
//                 pattern established by src/core/Resampler (3R
//                 Task I2a port from AetherSDR [@0cd4559]).
//                 AI tooling: Anthropic Claude Code.
// =================================================================

#pragma once

#include <memory>

namespace NereusSDR {

// ─────────────────────────────────────────────────────────────────────
// RadeTxHpf80 - 80 Hz Butterworth high-pass filter (biquad).
//
// Single-channel mono biquad, direct-form II transposed.  Coefficients
// derived once at construction from the standard cookbook formula
// (Robert Bristow-Johnson, "Audio EQ Cookbook") for a 2nd-order
// Butterworth HPF at fs=48000 Hz, fc=80 Hz, Q=1/sqrt(2):
//
//   w0    = 2*pi*fc/fs
//   alpha = sin(w0) / (2*Q)
//   cos0  = cos(w0)
//   b0 =  (1 + cos0) / 2
//   b1 = -(1 + cos0)
//   b2 =  (1 + cos0) / 2
//   a0 =  1 + alpha
//   a1 = -2 * cos0
//   a2 =  1 - alpha
//   (then normalize b0/b1/b2/a1/a2 by a0)
//
// Direct-form II transposed update (one sample in -> one sample out):
//   y    = b0*x + z1
//   z1'  = b1*x - a1*y + z2
//   z2'  = b2*x - a2*y
// ─────────────────────────────────────────────────────────────────────
class RadeTxHpf80 {
public:
    RadeTxHpf80();

    // Reset internal state (z1, z2 = 0).  Call on PTT transitions to
    // avoid carrying a stale tail into the next TX cycle.
    void reset();

    // Filter one sample.  Returns the filtered value.
    float process(float sample);

    // Filter a buffer in place.  Equivalent to calling process(float)
    // n times but with the loop body inlined.  No-op if n <= 0.
    void process(float* buffer, int n);

private:
    // Precomputed biquad coefficients (normalised by a0).
    float m_b0;
    float m_b1;
    float m_b2;
    float m_a1;
    float m_a2;

    // Direct-form II transposed state variables.
    float m_z1{0.0f};
    float m_z2{0.0f};
};

// ─────────────────────────────────────────────────────────────────────
// RadeTx48to16 - 48 kHz -> 16 kHz mono downsampler.
//
// Thin wrapper around r8brain CDSPResampler24 (vendored at
// third_party/r8brain/, MIT license).  CDSPResampler24 internally
// does the float32 <-> double conversion via a per-instance scratch
// buffer.  One instance handles one direction; create a separate
// instance per call site.
//
// Thread safety: NOT thread-safe.  Use one instance per thread.
// ─────────────────────────────────────────────────────────────────────
class RadeTx48to16 {
public:
    RadeTx48to16();
    ~RadeTx48to16();

    // Process `n` 48 kHz mono float samples and write up to `outMax`
    // 16 kHz output samples to `out`.  Returns the number of samples
    // actually written (may be 0 during the first few calls while
    // r8brain's filterbank warms up).  Caller is responsible for
    // sizing `out` to at least `outMax`.
    int process(const float* in, int n, float* out, int outMax);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace NereusSDR
