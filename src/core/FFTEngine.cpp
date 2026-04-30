// =================================================================
// src/core/FFTEngine.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source [v2.10.3.13 @ 501e3f51]:
//   Project Files/Source/Console/display.cs        — windows, dBm offset, BUFFER_SIZE
//   Project Files/Source/Console/PanDisplay.cs     — sliding-window stride formula
//   (original licences from Thetis sources are included below)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-29 — Sliding-window FFT (constant-rate output regardless of
//                 fftSize) ported from PanDisplay.cs:4699-4700, by
//                 J.J. Boyd (KG4VCF) with AI-assisted transformation via
//                 Anthropic Claude Code.
// =================================================================

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// --- From PanDisplay.cs ---
//=================================================================
// pandisplay.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2014 Doug Wigley (W5WC)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//=================================================================
//
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
//

#include "FFTEngine.h"
#include "LogCategories.h"

#include <cmath>
#include <cstring>

namespace NereusSDR {

FFTEngine::FFTEngine(int receiverId, QObject* parent)
    : QObject(parent)
    , m_receiverId(receiverId)
{
}

FFTEngine::~FFTEngine()
{
#ifdef HAVE_FFTW3
    if (m_plan) {
        fftwf_destroy_plan(m_plan);
    }
    if (m_fftIn) {
        fftwf_free(m_fftIn);
    }
    if (m_fftOut) {
        fftwf_free(m_fftOut);
    }
#endif
}

void FFTEngine::setFftSize(int size)
{
    if (size < 1024 || size > kMaxFftSize) {
        return;
    }
    if ((size & (size - 1)) != 0) {
        return;
    }
    // Defer to next feedIQ call — coalesces rapid slider changes
    m_pendingFftSize.store(size);
}

void FFTEngine::setWindowFunction(WindowFunction wf)
{
    m_windowFunc.store(static_cast<int>(wf));
}

void FFTEngine::setSampleRate(double rateHz)
{
    m_sampleRate.store(rateHz);
}

void FFTEngine::setOutputFps(int fps)
{
    m_targetFps.store(qBound(1, fps, 60));
}

void FFTEngine::feedIQ(const QVector<float>& interleavedIQ)
{
#ifdef HAVE_FFTW3
    // Apply any pending FFT size change (coalesces rapid slider drags)
    int pending = m_pendingFftSize.exchange(0);
    if (pending > 0 && pending != m_currentFftSize) {
        m_fftSize.store(pending);
        replanFft();
    }

    if (m_currentFftSize <= 0 || m_iqRing.size() != 2 * m_currentFftSize) {
        return;
    }

    // Refresh stride if sampleRate or targetFps changed since last call.
    recomputeStride();

    // Refresh window coefficients if the user picked a new window function
    // since last call. m_window is only ever read on this worker thread, so
    // recomputing it here is safe and avoids cross-thread fft state surgery.
    const int wfNow = m_windowFunc.load();
    if (wfNow != m_lastWindowFunc) {
        m_lastWindowFunc = wfNow;
        computeWindow();
    }

    const int N = m_currentFftSize;
    const int numPairs = interleavedIQ.size() / 2;
    for (int i = 0; i < numPairs; ++i) {
        // Swap I↔Q for spectrum display — matches Thetis analyzer.c:1757-1758
        // Audio path (WDSP fexchange2) uses normal I/Q order; display is inverted.
        m_iqRing[m_ringWritePos * 2]     = interleavedIQ[i * 2 + 1];  // Q→I
        m_iqRing[m_ringWritePos * 2 + 1] = interleavedIQ[i * 2];      // I→Q

        m_ringWritePos = (m_ringWritePos + 1) % N;
        if (m_warmupCount < N) {
            ++m_warmupCount;
        }
        ++m_samplesSinceLastFft;

        // Stride-based trigger — Thetis PanDisplay.cs:4699-4700 [@501e3f51]:
        //   overlap = max(0, ceil(fft_size - sample_rate / frame_rate))
        //   incr    = fft_size - overlap
        // We collapse incr to max(1, sampleRate/targetFps) so the FFT output
        // rate is targetFps regardless of fft_size (high zoom no longer
        // chokes the waterfall scroll rate).
        if (m_warmupCount >= N && m_samplesSinceLastFft >= m_incrSamples) {
            m_samplesSinceLastFft -= m_incrSamples;
            processFrame();
        }
    }
#else
    Q_UNUSED(interleavedIQ);
#endif
}

void FFTEngine::recomputeStride()
{
    const double sr  = m_sampleRate.load();
    const int    fps = m_targetFps.load();
    if (sr == m_lastSampleRate && fps == m_lastTargetFps && m_incrSamples > 0) {
        return;
    }
    m_lastSampleRate = sr;
    m_lastTargetFps  = fps;
    const int safeFps = (fps > 0) ? fps : 30;
    const double safeSr = (sr > 0.0) ? sr : 48000.0;
    m_incrSamples = std::max<qint64>(
        1, static_cast<qint64>(safeSr / safeFps));
}

void FFTEngine::replanFft()
{
#ifdef HAVE_FFTW3
    int size = m_fftSize.load();
    qCInfo(lcDsp) << "FFTEngine: replanning FFT size" << size;

    // Destroy old plan and buffers
    if (m_plan) {
        fftwf_destroy_plan(m_plan);
        m_plan = nullptr;
    }
    if (m_fftIn) {
        fftwf_free(m_fftIn);
        m_fftIn = nullptr;
    }
    if (m_fftOut) {
        fftwf_free(m_fftOut);
        m_fftOut = nullptr;
    }

    // Allocate aligned buffers
    m_fftIn  = fftwf_alloc_complex(size);
    m_fftOut = fftwf_alloc_complex(size);

    // FFTW_ESTIMATE: fast plan without measurement (avoids global FFTW mutex
    // contention with WDSP audio thread). Startup wisdom covers common sizes.
    m_plan = fftwf_plan_dft_1d(size, m_fftIn, m_fftOut, FFTW_FORWARD, FFTW_ESTIMATE);

    m_currentFftSize = size;

    // Resize the sliding-window ring buffer and clear the warmup counter so
    // the next emit waits for a full fftSize of samples.
    m_iqRing.assign(2 * size, 0.0f);
    m_ringWritePos        = 0;
    m_warmupCount         = 0;
    m_samplesSinceLastFft = 0;

    // Recompute window coefficients
    computeWindow();

    // Force stride recompute on next feedIQ — fftSize change can affect the
    // floor case where m_incrSamples > N. computeWindow() ran inline, so
    // sync m_lastWindowFunc with the atomic to avoid a redundant recompute.
    m_lastSampleRate = 0.0;
    m_lastTargetFps  = 0;
    m_lastWindowFunc = m_windowFunc.load();

    qCInfo(lcDsp) << "FFTEngine: plan created, window computed";
#endif
}

// Window function coefficients.
// From gpu-waterfall.md lines 184-216, constants match Thetis WDSP SetAnalyzer options.
void FFTEngine::computeWindow()
{
    int size = m_currentFftSize;
    m_window.resize(size);

    WindowFunction wf = static_cast<WindowFunction>(m_windowFunc.load());

    switch (wf) {
    case WindowFunction::BlackmanHarris4: {
        // 4-term Blackman-Harris — from gpu-waterfall.md:190-200
        constexpr float a0 = 0.35875f;
        constexpr float a1 = 0.48829f;
        constexpr float a2 = 0.14128f;
        constexpr float a3 = 0.01168f;
        for (int i = 0; i < size; ++i) {
            float n = static_cast<float>(i) / static_cast<float>(size);
            m_window[i] = a0
                        - a1 * std::cos(2.0f * static_cast<float>(M_PI) * n)
                        + a2 * std::cos(4.0f * static_cast<float>(M_PI) * n)
                        - a3 * std::cos(6.0f * static_cast<float>(M_PI) * n);
        }
        break;
    }
    case WindowFunction::Hanning:
        for (int i = 0; i < size; ++i) {
            float n = static_cast<float>(i) / static_cast<float>(size);
            m_window[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * n));
        }
        break;
    case WindowFunction::Hamming:
        for (int i = 0; i < size; ++i) {
            float n = static_cast<float>(i) / static_cast<float>(size);
            m_window[i] = 0.54f - 0.46f * std::cos(2.0f * static_cast<float>(M_PI) * n);
        }
        break;
    case WindowFunction::Flat:
        // Flat-top for calibration — minimal scalloping loss
        for (int i = 0; i < size; ++i) {
            float n = static_cast<float>(i) / static_cast<float>(size);
            float pi = static_cast<float>(M_PI);
            m_window[i] = 1.0f
                        - 1.93f  * std::cos(2.0f * pi * n)
                        + 1.29f  * std::cos(4.0f * pi * n)
                        - 0.388f * std::cos(6.0f * pi * n)
                        + 0.028f * std::cos(8.0f * pi * n);
        }
        break;
    case WindowFunction::None:
        std::fill(m_window.begin(), m_window.end(), 1.0f);
        break;
    default:
        // BlackmanHarris7, Kaiser — TODO: implement when needed
        // For now fall back to BlackmanHarris4
        {
            constexpr float a0 = 0.35875f;
            constexpr float a1 = 0.48829f;
            constexpr float a2 = 0.14128f;
            constexpr float a3 = 0.01168f;
            for (int i = 0; i < size; ++i) {
                float n = static_cast<float>(i) / static_cast<float>(size);
                m_window[i] = a0
                            - a1 * std::cos(2.0f * static_cast<float>(M_PI) * n)
                            + a2 * std::cos(4.0f * static_cast<float>(M_PI) * n)
                            - a3 * std::cos(6.0f * static_cast<float>(M_PI) * n);
            }
        }
        break;
    }

    // Compute window coherent gain for dBm normalization.
    // Power normalization: divide |X[k]|² by (sum of window)² to get
    // correct absolute power. The dBm offset accounts for this.
    float sum = 0.0f;
    for (float w : m_window) {
        sum += w;
    }
    // dbmOffset: 10*log10(1/sum²) = -20*log10(sum)
    // This normalizes the FFT output so a full-scale sine reads 0 dBFS.
    m_dbmOffset = -20.0f * std::log10(sum > 0.0f ? sum : 1.0f);
}

void FFTEngine::processFrame()
{
#ifdef HAVE_FFTW3
    if (!m_plan || m_currentFftSize <= 0 || m_warmupCount < m_currentFftSize) {
        return;
    }

    // Reconstruct the most recent N samples in chronological order (oldest →
    // newest) from the ring buffer and apply the window into m_fftIn. After
    // the warmup, m_ringWritePos sits on the next-write slot which is also
    // the oldest sample once the ring is full.
    const int N = m_currentFftSize;
    int src = m_ringWritePos;
    for (int i = 0; i < N; ++i) {
        const float w = m_window[i];
        m_fftIn[i][0] = m_iqRing[src * 2]     * w;
        m_fftIn[i][1] = m_iqRing[src * 2 + 1] * w;
        src = (src + 1) % N;
    }

    // Execute FFT
    fftwf_execute(m_plan);

    // Convert to dBm: 10 * log10(I² + Q²) + normalization
    // Complex I/Q FFT: output all N bins with FFT-shift.
    // Raw FFT order: [DC..+fs/2, -fs/2..DC)
    // Shifted order:  [-fs/2..DC..+fs/2) — matches spectrum display left-to-right
    int half = N / 2;
    QVector<float> binsDbm(N);

    // Normalization: the FFT output magnitude needs to be divided by the
    // window's coherent gain (sum of window coefficients) to get correct
    // power. We apply this as a dB offset: m_dbmOffset = -20*log10(sum).
    for (int i = 0; i < N; ++i) {
        // FFT-shift: swap halves so negative freqs are on left.
        // I/Q swap in feedIQ handles spectrum orientation (no mirror needed).
        int srcIdx = (i + half) % N;
        float re = m_fftOut[srcIdx][0];
        float im = m_fftOut[srcIdx][1];
        float powerSq = re * re + im * im;

        // Avoid log(0) — floor at -200 dBm
        // From Thetis display.cs:2842 — initializes display data to -200
        if (powerSq < 1e-20f) {
            binsDbm[i] = -200.0f;
        } else {
            binsDbm[i] = 10.0f * std::log10(powerSq) + m_dbmOffset;
        }
    }

    emit fftReady(m_receiverId, binsDbm);
#endif
}

} // namespace NereusSDR
