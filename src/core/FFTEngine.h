#pragma once

// =================================================================
// src/core/FFTEngine.h  (NereusSDR)
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

#include <QObject>
#include <QVector>

#include <atomic>

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

namespace NereusSDR {

// Window function types for FFT pre-processing.
// From Thetis: these match the WDSP SetAnalyzer window options.
enum class WindowFunction : int {
    None = 0,
    Hanning,
    Hamming,
    BlackmanHarris4,   // 4-term Blackman-Harris (default, -92 dB sidelobes)
    BlackmanHarris7,   // 7-term Blackman-Harris
    Kaiser,
    Flat,              // flat-top for calibration
    Count
};

// Per-receiver FFT computation engine.
// Lives on a worker thread. Receives raw interleaved I/Q samples,
// accumulates to FFT size, applies window, computes FFT via FFTW3,
// converts to dBm bins, and emits for display.
//
// Thread-safe parameter updates via std::atomic for values set from
// the main thread (FFT size changes require replan on next frame).
//
// From Thetis display.cs:215 — BUFFER_SIZE = 16384 (max FFT size)
class FFTEngine : public QObject {
    Q_OBJECT

public:
    explicit FFTEngine(int receiverId, QObject* parent = nullptr);
    ~FFTEngine() override;

    // --- Configuration (thread-safe, main thread sets these) ---

    void setFftSize(int size);
    int  fftSize() const { return m_fftSize.load(); }

    void setWindowFunction(WindowFunction wf);
    WindowFunction windowFunction() const {
        return static_cast<WindowFunction>(m_windowFunc.load());
    }

    void setSampleRate(double rateHz);
    double sampleRate() const { return m_sampleRate.load(); }

    void setOutputFps(int fps);
    int  outputFps() const { return m_targetFps.load(); }

public slots:
    // Feed raw interleaved I/Q samples from RadioConnection.
    // Format: [I0, Q0, I1, Q1, ...] as float pairs.
    // Accumulates until fftSize samples are collected, then runs FFT.
    void feedIQ(const QVector<float>& interleavedIQ);

signals:
    // Emitted when a new FFT frame is ready.
    // binsDbm contains fftSize/2 float values (positive frequencies only),
    // representing power in dBm at each frequency bin.
    void fftReady(int receiverId, const QVector<float>& binsDbm);

private:
    void replanFft();
    void computeWindow();
    void processFrame();
    void recomputeStride();  // refresh m_incrSamples from sampleRate + targetFps

    int m_receiverId;

    // FFT configuration (atomics for cross-thread access)
    std::atomic<int>    m_fftSize{4096};
    std::atomic<int>    m_pendingFftSize{0};  // 0 = no change pending
    std::atomic<int>    m_windowFunc{static_cast<int>(WindowFunction::BlackmanHarris4)};
    std::atomic<double> m_sampleRate{48000.0};
    std::atomic<int>    m_targetFps{30};

    // Internal state (only accessed on worker thread)
    int m_currentFftSize{0};  // last planned size (triggers replan on mismatch)

#ifdef HAVE_FFTW3
    fftwf_complex* m_fftIn{nullptr};
    fftwf_complex* m_fftOut{nullptr};
    fftwf_plan     m_plan{nullptr};
#endif

    // Window function coefficients (precomputed for current fftSize)
    QVector<float> m_window;

    // Sliding-window I/Q ring buffer (interleaved I/Q pairs).
    // Size is 2*m_currentFftSize floats; m_ringWritePos is the next-write
    // slot, modulo m_currentFftSize. The oldest sample lives at
    // m_ringWritePos once the ring has filled; reads start there and
    // wrap forward to reconstruct the most recent fftSize samples in
    // chronological order.
    QVector<float> m_iqRing;
    int            m_ringWritePos{0};
    qint64         m_warmupCount{0};         // counts up to fftSize before first emit
    qint64         m_samplesSinceLastFft{0}; // resets each FFT trigger

    // Stride between consecutive FFTs, in sample pairs (one "pair" = one I/Q
    // complex sample). Recomputed on the worker thread when sampleRate or
    // targetFps changes; see PanDisplay.cs:4699-4700 for the upstream formula.
    qint64 m_incrSamples{0};
    double m_lastSampleRate{0.0};
    int    m_lastTargetFps{0};

    // Last window-function ordinal observed by the worker thread. When the
    // main thread flips m_windowFunc via setWindowFunction(), feedIQ() picks
    // up the change at the top of the next chunk and recomputes m_window.
    // Without this, the dropdown was a silent no-op between fft-size changes.
    int m_lastWindowFunc{-1};

    // dBm calibration offset (accounts for window gain + FFT normalization)
    float m_dbmOffset{0.0f};

    // From Thetis display.cs:215
    static constexpr int kMaxFftSize = 65536;
};

} // namespace NereusSDR
