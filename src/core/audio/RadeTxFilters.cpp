// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/audio/RadeTxFilters.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR - DSP helpers for the RADE TX path.  See RadeTxFilters.h
// for the full attribution block and design narrative.
//
// =================================================================

// no-port-check: NereusSDR-original implementation.  HPF coefficient
// derivation cites Robert Bristow-Johnson's "Audio EQ Cookbook" (the
// canonical reference for digital biquad design); the cookbook is in
// the public domain.  r8brain CDSPResampler24 is vendored at
// third_party/r8brain/ under MIT license.

#include "core/audio/RadeTxFilters.h"

#include "CDSPResampler.h"

#include <cmath>
#include <vector>

namespace NereusSDR {

// ── HPF coefficient derivation -------------------------------------------------
//
// Designed once at construction for fs=48000 Hz, fc=80 Hz, Q=1/sqrt(2)
// (Butterworth response).  Cookbook formula (Robert Bristow-Johnson,
// "Audio EQ Cookbook", HPF section):
//
//   w0    = 2 * pi * fc / fs
//   cos0  = cos(w0)
//   sin0  = sin(w0)
//   alpha = sin0 / (2 * Q)
//
//   b0 =  (1 + cos0) / 2
//   b1 = -(1 + cos0)
//   b2 =  (1 + cos0) / 2
//   a0 =  1 + alpha
//   a1 = -2 * cos0
//   a2 =  1 - alpha
//
// Then normalise all five b/a coefficients by a0 so the recurrence
// reads y = b0*x + b1*x[-1] + b2*x[-2] - a1*y[-1] - a2*y[-2].
//
// Direct-form II transposed update (single sample in/out):
//   y    = b0 * x + z1
//   z1'  = b1 * x - a1 * y + z2
//   z2'  = b2 * x - a2 * y
// ----------------------------------------------------------------------

RadeTxHpf80::RadeTxHpf80()
{
    constexpr float kFs   = 48000.0f;
    constexpr float kFc   = 80.0f;
    constexpr float kQ    = 0.70710678f;          // 1/sqrt(2), Butterworth Q
    constexpr float kPi   = 3.14159265358979323846f;

    const float w0    = 2.0f * kPi * kFc / kFs;
    const float cos0  = std::cos(w0);
    const float sin0  = std::sin(w0);
    const float alpha = sin0 / (2.0f * kQ);

    const float b0 =  (1.0f + cos0) * 0.5f;
    const float b1 = -(1.0f + cos0);
    const float b2 =  (1.0f + cos0) * 0.5f;
    const float a0 =   1.0f + alpha;
    const float a1 =  -2.0f * cos0;
    const float a2 =   1.0f - alpha;

    // Normalise so the recurrence implicitly divides by a0 = 1.
    const float invA0 = 1.0f / a0;
    m_b0 = b0 * invA0;
    m_b1 = b1 * invA0;
    m_b2 = b2 * invA0;
    m_a1 = a1 * invA0;
    m_a2 = a2 * invA0;
}

void RadeTxHpf80::reset()
{
    m_z1 = 0.0f;
    m_z2 = 0.0f;
}

float RadeTxHpf80::process(float sample)
{
    const float y   = m_b0 * sample + m_z1;
    const float nz1 = m_b1 * sample - m_a1 * y + m_z2;
    const float nz2 = m_b2 * sample - m_a2 * y;
    m_z1 = nz1;
    m_z2 = nz2;
    return y;
}

void RadeTxHpf80::process(float* buffer, int n)
{
    if (buffer == nullptr || n <= 0) {
        return;
    }
    for (int i = 0; i < n; ++i) {
        buffer[i] = process(buffer[i]);
    }
}

// ── RadeTx48to16 -------------------------------------------------------
//
// Wraps r8brain CDSPResampler24 with a float32 input + float32 output
// interface.  r8brain operates internally on doubles, so process()
// does the float -> double conversion into m_inBuf, calls the
// resampler, then converts the resampler's double output back to
// float32 into the caller's buffer.
// ----------------------------------------------------------------------

class RadeTx48to16::Impl {
public:
    Impl()
        : m_resampler(48000.0, 16000.0, /*maxBlockSamples=*/2048)
    {
        m_inBuf.reserve(2048);
    }

    int process(const float* in, int n, float* out, int outMax)
    {
        if (in == nullptr || out == nullptr || n <= 0 || outMax <= 0) {
            return 0;
        }

        // float32 -> double conversion.
        m_inBuf.resize(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            m_inBuf[static_cast<size_t>(i)] = static_cast<double>(in[i]);
        }

        double* outPtr = nullptr;
        const int outLen =
            m_resampler.process(m_inBuf.data(), n, outPtr);
        if (outLen <= 0 || outPtr == nullptr) {
            return 0;
        }

        const int writeCount = (outLen < outMax) ? outLen : outMax;
        for (int i = 0; i < writeCount; ++i) {
            out[i] = static_cast<float>(outPtr[i]);
        }
        return writeCount;
    }

private:
    r8b::CDSPResampler24 m_resampler;
    std::vector<double>  m_inBuf;
};

RadeTx48to16::RadeTx48to16()
    : m_impl(std::make_unique<Impl>())
{
}

RadeTx48to16::~RadeTx48to16() = default;

int RadeTx48to16::process(const float* in, int n, float* out, int outMax)
{
    return m_impl->process(in, n, out, outMax);
}

}  // namespace NereusSDR
