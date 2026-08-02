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
#include "MemoryLock.h"
#include "PerfMonitor.h"

#include <QElapsedTimer>

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
    // Unlock BEFORE free so the kernel doesn't carry a stale lock
    // past the lifetime of the page (paired with the lockMemory calls
    // in replanFft).
    const std::size_t fftBytes =
        sizeof(fftwf_complex) * m_currentFftSize;
    if (m_iqRaw) {
        unlockMemory(m_iqRaw, fftBytes);
        fftwf_free(m_iqRaw);
    }
    if (m_fftIn) {
        unlockMemory(m_fftIn, fftBytes);
        fftwf_free(m_fftIn);
    }
    if (m_fftOut) {
        unlockMemory(m_fftOut, fftBytes);
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
    // Capture the size we'll appear-to-be at after this call.  The actual
    // m_currentFftSize change is deferred to the next feedIQ replan, but
    // m_fftSize.load() already returns the new value to outside callers,
    // so the comparison must use it as the post-state -- old vs new is
    // captured against m_fftSize.load() before the store below.
    const int oldSize = m_fftSize.load();
    if (oldSize != size) {
        m_fftSize.store(size);
    }
    // Defer the actual replan to next feedIQ call -- coalesces rapid
    // slider changes into a single replan.
    m_pendingFftSize.store(size);
    if (oldSize != size) {
        // Mirrors Thetis setup.cs:16162-16165 [v2.10.3.13]:
        //   if (old_fft != ...FFTSize)
        //       SpectrumSettingsChangedHandlers?.Invoke(1);
        emit spectrumSettingsChanged(m_receiverId);
    }
}

void FFTEngine::setWindowFunction(WindowFunction wf)
{
    m_windowFunc.store(static_cast<int>(wf));
    // Trigger window recompute on next FFT frame.  Without this the new
    // window function would be silently ignored until the FFT size also
    // changes (which is the only other path that calls computeWindow).
    m_windowDirty.store(true);
}

// From Thetis specHPSDR.cs:145 [v2.10.3.13] -- Kaiser window shape parameter.
// Range mirrors Thetis's runtime range (any positive double); Thetis's UI
// surfaces a numeric editor rather than a slider for this control.
void FFTEngine::setKaiserPi(double pi)
{
    if (pi <= 0.0) { return; }
    m_kaiserPi.store(pi);
    // Kaiser shape parameter feeds the bessi0 coefficients in computeWindow;
    // changing it requires the same recompute as a window-function change.
    m_windowDirty.store(true);
}

// FFT size display calibration offset.  Mirrors Thetis Display.cs:1393-1396
// [v2.10.3.13] RX1FFTSizeOffset setter (a plain field assignment with no
// side effects).  Consumers (RadioModel auto-AGC) read via
// fftSizeOffsetDb() each time they recompute the cal offset.
void FFTEngine::setFftSizeOffsetDb(double db)
{
    m_fftSizeOffsetDb.store(db);
}

// FFT-size baseline (NereusSDR-original).  Same validity check as
// setFftSize: powers of two in [1024, kMaxFftSize].  Unlike setFftSize,
// this does NOT trigger a replan -- it's pure state for the auto-zoom
// lambda to read.  Set by the DisplaySetupPages slider handler alongside
// setFftSize so they stay in lock-step at the user's intent.
void FFTEngine::setFftSizeBaseline(int size)
{
    if (size < 1024 || size > kMaxFftSize) { return; }
    if ((size & (size - 1)) != 0) { return; }
    m_fftSizeBaseline.store(size);
}

// Atomic store; the auto-zoom lambda in MainWindow reads it on each
// bandwidthChangeRequested signal.  Negative or NaN inputs are clamped
// to 0 (disabled).  Atypically large values (>200 Hz/bin) round-trip
// fine but produce sub-baseline FFT sizes which the MainWindow floor
// will overwrite — effectively a no-op.
void FFTEngine::setHzPerBinTarget(double hzPerBin)
{
    if (!(hzPerBin > 0.0)) { hzPerBin = 0.0; }  // catches NaN + negatives
    m_hzPerBinTarget.store(hzPerBin);
}

// Modified Bessel function of the first kind, order 0.  Verbatim port of
// WDSP analyzer.c:33-50 [v2.10.3.13] (Numerical Recipes polynomial
// approximations: low-x branch via 6th-order series in (x/3.75)^2,
// high-x branch via asymptotic expansion).  Used by the Kaiser window
// constructor in computeWindow().
//
// From WDSP analyzer.c:7 -- Copyright (C) 2012 David McQuate, WA8YWQ
// "Kaiser window & Bessel function added".
static double bessi0(double x)
{
    double ax, ans, y;
    if ((ax = std::fabs(x)) < 3.75) {
        y = x / 3.75;
        y = y * y;
        ans = 1.0 + y * (3.5156229 + y * (3.0899424 + y * (1.2067492
            + y * (0.2659732 + y * (0.360768e-1 + y * 0.45813e-2)))));
    } else {
        y = 3.75 / ax;
        ans = (std::exp(ax) / std::sqrt(ax)) * (0.39894228 + y * (0.1328592e-1
            + y * (0.225319e-2 + y * (-0.157565e-2 + y * (0.916281e-2
            + y * (-0.2057706e-1 + y * (0.2635537e-1 + y * (-0.1647633e-1
            + y * 0.392377e-2))))))));
    }
    return ans;
}

void FFTEngine::setSampleRate(double rateHz)
{
    m_sampleRate.store(rateHz);
}

void FFTEngine::setOutputFps(int fps)
{
    m_targetFps.store(qBound(1, fps, 60));
}

// From Thetis setup.designer.cs:33732 udDisplayDecimation [v2.10.3.13].
// Range 1..32; 1 = no decimation (every sample is used).
void FFTEngine::setDecimation(int factor)
{
    if (factor < 1 || factor > 32) { return; }
    m_decimation.store(factor);
    // Reset the counter so the new factor takes effect cleanly on the
    // next feedIQ call rather than mid-stride.
    m_decimationCounter = 0;
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

    // Append interleaved I/Q to RAW ring buffer (m_iqRaw), honouring
    // decimation.  processFrame() applies the window into m_fftIn just
    // before fftwf_execute and then shifts the trailing overlap region
    // back to the head -- mirrors the WDSP analyzer overlap pattern
    // driven by Thetis specHPSDR.cs:155 [v2.10.3.13] overlap=30000.
    // From Thetis setup.designer.cs:33732 udDisplayDecimation [v2.10.3.13]:
    // only every Nth sample pair is passed to the FFT accumulator.
    const int dec = m_decimation.load();
    const int numPairs = interleavedIQ.size() / 2;
    for (int i = 0; i < numPairs; ++i) {
        if (dec > 1) {
            if (m_decimationCounter != 0) {
                m_decimationCounter = (m_decimationCounter + 1) % dec;
                continue;
            }
            m_decimationCounter = (m_decimationCounter + 1) % dec;
        }
        if (m_iqWritePos >= m_currentFftSize) {
            // Buffer full -- process and shift overlap region back to head.
            processFrame();
            // If processFrame returned WITHOUT running the FFT (defensive
            // 5 ms cap fired), m_iqWritePos is still at fftSize.  Drop
            // the rest of this batch; buffer stays full; next feedIQ
            // call retries processFrame (timer will have advanced past
            // the cap by then).  Without this guard we'd overflow
            // m_iqRaw on the next sample write.
            if (m_iqWritePos >= m_currentFftSize) {
                return;
            }
        }
        // Swap I<->Q for spectrum display.  Matches Thetis analyzer.c:
        // 1757-1758 [v2.10.3.13].  Audio path (WDSP fexchange2) uses
        // normal I/Q order; display is inverted.  Window is applied later
        // in processFrame so we can reuse raw samples after the overlap
        // shift.
        m_iqRaw[m_iqWritePos][0] = interleavedIQ[i * 2 + 1];  // Q -> I
        m_iqRaw[m_iqWritePos][1] = interleavedIQ[i * 2];      // I -> Q
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

    // Destroy old plan and buffers.  Unlock memory BEFORE free so the
    // kernel doesn't carry a stale lock past the lifetime of the page.
    if (m_plan) {
        fftwf_destroy_plan(m_plan);
        m_plan = nullptr;
    }
    if (m_iqRaw) {
        unlockMemory(m_iqRaw, sizeof(fftwf_complex) * m_currentFftSize);
        fftwf_free(m_iqRaw);
        m_iqRaw = nullptr;
    }
    if (m_fftIn) {
        unlockMemory(m_fftIn, sizeof(fftwf_complex) * m_currentFftSize);
        fftwf_free(m_fftIn);
        m_fftIn = nullptr;
    }
    if (m_fftOut) {
        unlockMemory(m_fftOut, sizeof(fftwf_complex) * m_currentFftSize);
        fftwf_free(m_fftOut);
        m_fftOut = nullptr;
    }

    // Allocate aligned buffers.  m_iqRaw holds the un-windowed sliding
    // sample window so we can overlap successive FFT frames without
    // losing data to the per-frame window multiply.
    m_iqRaw  = fftwf_alloc_complex(size);
    m_fftIn  = fftwf_alloc_complex(size);
    m_fftOut = fftwf_alloc_complex(size);

    // 2026-05-26 KG4VCF: pin the FFT scratch buffers so heavy memory
    // pressure under build load can not compress them.  These get
    // touched every FFT frame; a compression stall here would block
    // the spectrum thread and starve the waterfall ticker.
    const std::size_t fftBytes = sizeof(fftwf_complex) * size;
    lockMemory(m_iqRaw,  fftBytes, "FFTEngine::m_iqRaw");
    lockMemory(m_fftIn,  fftBytes, "FFTEngine::m_fftIn");
    lockMemory(m_fftOut, fftBytes, "FFTEngine::m_fftOut");

    // FFTW_ESTIMATE: fast plan without measurement (avoids global FFTW mutex
    // contention with WDSP audio thread). Startup wisdom covers common sizes.
    m_plan = fftwf_plan_dft_1d(size, m_fftIn, m_fftOut, FFTW_FORWARD, FFTW_ESTIMATE);

    m_currentFftSize = size;
    m_iqWritePos = 0;

    // Recompute window coefficients
    computeWindow();
    m_windowDirty.store(false);  // computeWindow just ran; clear pending bit

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

    // Diagnostic log -- helps confirm the dirty-flag path runs when the
    // user changes the window combo.  Static last-logged value avoids
    // log spam from same-window FFT-size changes (which also re-run
    // computeWindow via replanFft).
    static int lastLoggedWf = -1;
    static int lastLoggedSize = 0;
    const int wfInt = static_cast<int>(wf);
    if (wfInt != lastLoggedWf || size != lastLoggedSize) {
        qCInfo(lcDsp) << "FFTEngine::computeWindow: window=" << wfInt
                      << " size=" << size
                      << " kaiserPi=" << m_kaiserPi.load();
        lastLoggedWf = wfInt;
        lastLoggedSize = size;
    }

    // All 7 cases mirror WDSP analyzer.c:52-173 [v2.10.3.13] new_window()
    // switch ordering exactly (case 0 = Rectangular ... case 6 = BH-7T).
    // WDSP normalises the window in-place by inv_coherent_gain at line 79
    // / 95 / 113 / 128 / 144 / 168; NereusSDR keeps the raw window and
    // applies the gain compensation post-FFT via m_dbmOffset (computed
    // below from sum), so coefficient values here are the bare
    // mathematical definitions without WDSP's normalisation step.
    const double pi = M_PI;
    const double arg0 = (size > 1) ? 2.0 * pi / static_cast<double>(size - 1) : 0.0;

    switch (wf) {
    case WindowFunction::Rectangular:
        // From WDSP analyzer.c:59-66 [v2.10.3.13] case 0 -- all 1.0.
        std::fill(m_window.begin(), m_window.end(), 1.0f);
        break;

    case WindowFunction::BlackmanHarris4: {
        // From WDSP analyzer.c:67-83 [v2.10.3.13] case 1 -- 4-term BH.
        for (int i = 0; i < size; ++i) {
            const double a = arg0 * static_cast<double>(i);
            m_window[i] = static_cast<float>(
                  0.35875
                - 0.48829 * std::cos(a)
                + 0.14128 * std::cos(2.0 * a)
                - 0.01168 * std::cos(3.0 * a));
        }
        break;
    }

    case WindowFunction::Hann:
        // From WDSP analyzer.c:84-99 [v2.10.3.13] case 2 -- Hann.
        for (int i = 0; i < size; ++i) {
            m_window[i] = static_cast<float>(
                0.5 * (1.0 - std::cos(static_cast<double>(i) * arg0)));
        }
        break;

    case WindowFunction::FlatTop:
        // From WDSP analyzer.c:100-116 [v2.10.3.13] case 3 -- 5-term flat-top
        // (note: WDSP's flat-top uses the .21557895 / .41663158 / .277263158
        // / .083578947 / .006947368 normalised coefficients, which differ
        // from the un-normalised set NereusSDR previously shipped).
        for (int i = 0; i < size; ++i) {
            const double a = arg0 * static_cast<double>(i);
            m_window[i] = static_cast<float>(
                  0.21557895
                - 0.41663158  * std::cos(a)
                + 0.277263158 * std::cos(2.0 * a)
                - 0.083578947 * std::cos(3.0 * a)
                + 0.006947368 * std::cos(4.0 * a));
        }
        break;

    case WindowFunction::Hamming:
        // From WDSP analyzer.c:117-132 [v2.10.3.13] case 4 -- Hamming.
        for (int i = 0; i < size; ++i) {
            m_window[i] = static_cast<float>(
                0.54 - 0.46 * std::cos(static_cast<double>(i) * arg0));
        }
        break;

    case WindowFunction::Kaiser: {
        // From WDSP analyzer.c:133-148 [v2.10.3.13] case 5 -- Kaiser via
        // bessi0 (modified Bessel function of the first kind, order 0).
        // I(beta * sqrt(1 - (2i/(N-1) - 1)^2)) / I(beta), beta = PiAlpha.
        const double piAlpha = m_kaiserPi.load();
        const double i0Beta  = bessi0(piAlpha);
        const double denom   = static_cast<double>(size - 1);
        for (int i = 0; i < size; ++i) {
            const double t = (denom > 0.0)
                ? (2.0 * static_cast<double>(i) / denom - 1.0)
                : 0.0;
            const double arg = piAlpha * std::sqrt(std::max(0.0, 1.0 - t * t));
            m_window[i] = static_cast<float>(bessi0(arg) / i0Beta);
        }
        break;
    }

    case WindowFunction::BlackmanHarris7: {
        // From WDSP analyzer.c:149-172 [v2.10.3.13] case 6 -- 7-term BH
        // expressed as a Chebyshev polynomial in cos(arg).  Note the
        // WDSP source uses Horner-form evaluation; coefficients preserved
        // verbatim including scientific notation.
        for (int i = 0; i < size; ++i) {
            const double c = std::cos(arg0 * static_cast<double>(i));
            m_window[i] = static_cast<float>(
                + 6.3964424114390378e-02
                + c * (- 2.3993864599352804e-01
                + c * (+ 3.5015956323820469e-01
                + c * (- 2.4774111897080783e-01
                + c * (+ 8.5438256055858031e-02
                + c * (- 1.2320203369293225e-02
                + c * (+ 4.3778825791773474e-04 ))))))
            );
        }
        break;
    }

    case WindowFunction::Count:
    default:
        // Sentinel value -- treat as Rectangular to avoid undefined state.
        std::fill(m_window.begin(), m_window.end(), 1.0f);
        break;
    }

    // Compute window coherent gain for dBm normalization.
    // Power normalization: divide |X[k]|² by (sum of window)² to get
    // correct absolute power. The dBm offset accounts for this.
    double sum = 0.0;       // Σw[i]
    double sumSq = 0.0;     // Σw[i]²  — needed for ENB
    for (float w : m_window) {
        sum   += w;
        sumSq += static_cast<double>(w) * static_cast<double>(w);
    }
    // dbmOffset: 10*log10(1/sum²) = -20*log10(sum)
    // This normalizes the FFT output so a full-scale sine reads 0 dBFS.
    m_dbmOffset = -20.0f * std::log10(sum > 0.0 ? static_cast<float>(sum) : 1.0f);

    // Equivalent Noise Bandwidth — N × Σw[i]² / (Σw[i])²  (in bins).
    // Required by Thetis WDSP analyzer detector function for inv_enb scaling
    // of Average / Sample / RMS modes (analyzer.c:368-441 [v2.10.3.13]).
    // For rectangular ENB = 1.0; Hann ≈ 1.50; BH-4T ≈ 2.00; Flat-Top ≈ 3.77.
    if (sum > 0.0) {
        m_windowEnb = static_cast<double>(m_window.size()) * sumSq / (sum * sum);
    } else {
        m_windowEnb = 1.0;
    }
}

void FFTEngine::processFrame()
{
#ifdef HAVE_FFTW3
    if (!m_plan || m_iqWritePos < m_currentFftSize) {
        return;
    }

    // Defensive emit-rate ceiling.  With the overlap design (advance =
    // sampleRate / targetFps, see overlap-shift block at the bottom of
    // this function) the natural FFT emit rate equals targetFps, which
    // setOutputFps clamps to [1, 60].  In steady-state operation this
    // cap is dead code.  It exists as a safety net for burst-processing
    // jitter where two buffer-full events arrive within microseconds
    // of each other.
    //
    // Threshold: 5 ms (= 200 fps cap).  Old code used 16 ms, which sat
    // right at the natural 16.67 ms emit interval at targetFps=60 and
    // tripped on small timing jitter -- and that cap-fire path used to
    // reset m_iqWritePos = 0, discarding the accumulated overlap region
    // (~13k samples worth of progress).  At 768 kHz DDC + 60 fps that
    // discard caused visible frame drops on JJ's bench when zooming
    // (each cap-fire silently lost 17 ms of accumulated samples).
    //
    // Cap-fire NO LONGER resets the buffer.  m_iqWritePos stays at
    // fftSize so the next feedIQ call sees a full buffer and retries
    // processFrame; the feedIQ loop has a matching guard that drops
    // its remaining batch samples when processFrame returned without
    // running the FFT.  This preserves the overlap region across the
    // (rare) cap fire.
    if (m_frameTimerStarted && m_frameTimer.elapsed() < 5) {
        return;  // buffer stays full; next feedIQ call retries
    }

    // 2026-05-26 KG4VCF perf instrumentation: time the FFT compute
    // path (window apply -> fftwf_execute -> dBm conversion ->
    // post-FFT detector/avenger) so the in-spectrum perf overlay can
    // report avg/max FFT cost per frame.
    QElapsedTimer perfTimer;
    perfTimer.start();

    // Window-recompute pending?  Set by setWindowFunction or setKaiserPi
    // since the last FFT frame.  Without this check, window changes via
    // the combo are silently ignored until the FFT size also changes.
    if (m_windowDirty.exchange(false)) {
        computeWindow();
    }

    // Apply window function to raw I/Q -> windowed FFT input.  m_iqRaw
    // holds the un-windowed sliding sample window; the multiply happens
    // here so the un-windowed tail can be overlapped into the next FFT
    // frame's head without needing to undo a window multiply.
    for (int i = 0; i < m_currentFftSize; ++i) {
        m_fftIn[i][0] = m_iqRaw[i][0] * m_window[i];
        m_fftIn[i][1] = m_iqRaw[i][1] * m_window[i];
    }

    // Execute FFT
    fftwf_execute(m_plan);

    // Convert to dBm: 10 * log10(I² + Q²) + normalization
    // Complex I/Q FFT: output all N bins with FFT-shift.
    // Raw FFT order: [DC..+fs/2, -fs/2..DC)
    // Shifted order:  [-fs/2..DC..+fs/2) — matches spectrum display left-to-right
    int N = m_currentFftSize;
    int half = N / 2;
    QVector<float> binsDbm(N);
    // Linear-power side-channel for the Thetis-faithful detector + avenger
    // pipeline (analyzer.c:283-554 [v2.10.3.13]).  Same FFT-shifted ordering
    // as binsDbm; values are |X[k]|² post-FFT, no log conversion, no dBm
    // offset (the avenger applies window-gain scaling and 10·log₁₀ later).
    QVector<float> binsLinear(N);

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
        binsLinear[i] = powerSq;

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

    // Emit both signals in lock-step.  fftReady serves chrome / AGC
    // consumers that need full-band dBm (ClarityController, NoiseFloorTracker);
    // fftReadyLinear feeds the Thetis-faithful spectrum render pipeline
    // (detector + avenger) which operates on linear-domain bins.  The
    // downstream detector needs windowEnb for invEnb scaling (analyzer.c:
    // 368-441 [v2.10.3.13]) and the avenger needs dbmOffset to apply the
    // window coherent-gain compensation that binsDbm gets via the +offset
    // term at line 348 above.  Both ride the same signal — keeps slot
    // and bins in lock-step without a separate setter coordination dance.
    emit fftReady(m_receiverId, binsDbm);
    emit fftReadyLinear(m_receiverId, binsLinear,
                        m_windowEnb, static_cast<double>(m_dbmOffset));

    // Overlap shift: copy the trailing (fftSize - advance) samples of
    // m_iqRaw back to the head, then resume writing at position 'overlap'.
    // 'advance' is the number of NEW samples consumed per FFT frame =
    // sampleRate / targetFps, clamped to the FFT size so very small
    // FFTs don't get pinned to a 1-sample advance.  This decouples FFT
    // frame rate from FFT size: at fftSize=262144 with sampleRate=768k
    // and target 30 fps, advance ~ 25600, frame rate stays ~30 fps
    // (vs ~3 fps without overlap).  Mirrors the WDSP analyzer overlap
    // pattern driven by Thetis specHPSDR.cs:155 [v2.10.3.13]
    // private int overlap = 30000.
    const double sr  = m_sampleRate.load();
    const int    fps = qMax(1, m_targetFps.load());
    int advance = (sr > 0.0)
        ? static_cast<int>(std::round(sr / static_cast<double>(fps)))
        : m_currentFftSize;
    advance = qBound(1, advance, m_currentFftSize);
    const int overlap = m_currentFftSize - advance;
    if (overlap > 0) {
        std::memmove(m_iqRaw,
                     m_iqRaw + advance,
                     static_cast<size_t>(overlap) * sizeof(fftwf_complex));
        m_iqWritePos = overlap;
    } else {
        m_iqWritePos = 0;
    }

    // 2026-05-26 KG4VCF: record FFT compute time after all per-frame
    // post-processing (detector / avenger / overlap-shift).  ns to ms
    // via nsecsElapsed() for sub-millisecond precision; the perf
    // overlay rounds for display.
    NereusSDR::PerfMonitor::instance().recordFftCompute(
        static_cast<double>(perfTimer.nsecsElapsed()) / 1e6);
#endif
}

} // namespace NereusSDR
