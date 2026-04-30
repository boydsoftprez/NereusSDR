// =================================================================
// src/core/FFTEngine.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
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

    // Append interleaved I/Q to accumulation buffer
    int numPairs = interleavedIQ.size() / 2;
    for (int i = 0; i < numPairs; ++i) {
        if (m_iqWritePos >= m_currentFftSize) {
            // Buffer full — process and reset
            processFrame();
            m_iqWritePos = 0;
        }
        // Swap I↔Q for spectrum display — matches Thetis analyzer.c:1757-1758
        // Audio path (WDSP fexchange2) uses normal I/Q order; display is inverted.
        m_fftIn[m_iqWritePos][0] = interleavedIQ[i * 2 + 1] * m_window[m_iqWritePos];  // Q→I
        m_fftIn[m_iqWritePos][1] = interleavedIQ[i * 2]     * m_window[m_iqWritePos];  // I→Q
        m_iqWritePos++;
    }
#else
    Q_UNUSED(interleavedIQ);
#endif
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
    m_iqWritePos = 0;

    // Recompute window coefficients
    computeWindow();

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
    if (!m_plan || m_iqWritePos < m_currentFftSize) {
        return;
    }

    // Safety cap: never emit faster than 60 fps to avoid flooding the main thread
    // at very small FFT sizes or very high sample rates. Display-side timer in
    // SpectrumWidget controls the actual repaint rate independently.
    if (m_frameTimerStarted && m_frameTimer.elapsed() < 16) {
        return;
    }

    // Execute FFT (window already applied during accumulation in feedIQ)
    fftwf_execute(m_plan);

    // Convert to dBm: 10 * log10(I² + Q²) + normalization
    // Complex I/Q FFT: output all N bins with FFT-shift.
    // Raw FFT order: [DC..+fs/2, -fs/2..DC)
    // Shifted order:  [-fs/2..DC..+fs/2) — matches spectrum display left-to-right
    int N = m_currentFftSize;
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

    // Restart frame timer
    m_frameTimer.restart();
    m_frameTimerStarted = true;

    emit fftReady(m_receiverId, binsDbm);
#endif
}

} // namespace NereusSDR
