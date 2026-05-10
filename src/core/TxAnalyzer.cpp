// no-port-check: NereusSDR-original glue class.  See TxAnalyzer.h header
// for the architectural narrative and source-first cite map.
//
// =================================================================
// src/core/TxAnalyzer.cpp  (NereusSDR)
// =================================================================
//
// Implementation notes
// --------------------
// The XCreateAnalyzer / SetAnalyzer parameter values come from Thetis's
// initAnalyzer path at specHPSDR.cs:504-650 [v2.10.3.13+501e3f51] — the
// PANAFALL/PANADAPTER analyzer setup.  attempt 1 mistakenly sourced from
// CalcSpectrum (specHPSDR.cs:738-806), which is the SPECTRUM/HISTOGRAM/
// SPECTRASCOPE path that PANAFALL never reaches per console.cs:8015-8020 +
// :8098-8108 [v2.10.3.13+501e3f51].  See
// docs/architecture/tx-display-attempt2-design.md §3.1 for the param
// deltas.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-07 — Created by J.J. Boyd (KG4VCF) for the PR #212
//                 follow-up TX waterfall fix.  AI-assisted source-first
//                 protocol via Anthropic Claude Code.
//   2026-05-10 — Strict Thetis-parity attempt 2 by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.  Swapped
//                 CalcSpectrum-derived params to initAnalyzer-derived
//                 params; updated kTxDispId from 2 to 5.
// =================================================================

#include "TxAnalyzer.h"

#include "LogCategories.h"
#include "wdsp_api.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

TxAnalyzer::TxAnalyzer(int dispId, QObject* parent)
    : QObject(parent)
    , m_dispId(dispId)
{
    m_pixBuf.resize(m_numPixels);

    m_pollTimer.setTimerType(Qt::PreciseTimer);
    m_pollTimer.setInterval(1000 / m_outputFps);
    connect(&m_pollTimer, &QTimer::timeout, this, &TxAnalyzer::poll);

#ifdef HAVE_WDSP
    // Allocate the WDSP analyzer instance.  Parameters from
    // wbDisplay.cs:4655 [v2.10.3.13] (max FFT 16384) + the TX path's
    // single-LO / single-stitch defaults.  app_data_path is empty —
    // FFTW wisdom is managed centrally by WdspEngine, not per-analyzer.
    int success = 0;
    char emptyPath[1] = {0};
    XCreateAnalyzer(m_dispId,
                    &success,
                    /*m_size=*/16384,
                    /*m_LO=*/1,
                    /*m_stitch=*/1,
                    emptyPath);
    if (success == 0) {
        m_analyzerCreated = true;
        applySetAnalyzer();
    } else {
        qCWarning(lcDsp) << "TxAnalyzer: XCreateAnalyzer failed for disp"
                         << m_dispId << "success=" << success;
    }
#else
    qCInfo(lcDsp) << "TxAnalyzer: HAVE_WDSP not defined — analyzer is a stub";
#endif
}

TxAnalyzer::~TxAnalyzer()
{
    stop();
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        DestroyAnalyzer(m_dispId);
        m_analyzerCreated = false;
    }
#endif
}

void TxAnalyzer::setNumPixels(int n)
{
    if (n <= 0 || n == m_numPixels) {
        return;
    }
    m_numPixels = n;
    m_pixBuf.resize(m_numPixels);
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applySetAnalyzer();
    }
#endif
}

void TxAnalyzer::setSampleRate(double rateHz)
{
    if (rateHz <= 0.0 || qFuzzyCompare(rateHz + 1.0, m_sampleRate + 1.0)) {
        return;
    }
    m_sampleRate = rateHz;
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        SetDisplaySampleRate(m_dispId, static_cast<int>(rateHz));
        // Re-derive overlap (depends on rate * fps).  Cite specHPSDR.cs:784
        // [v2.10.3.13]: ovrlp = max(0, ceil(fft_size - sampleRate / fps))
        applySetAnalyzer();
    }
#endif
}

void TxAnalyzer::setOutputFps(int fps)
{
    if (fps <= 0 || fps == m_outputFps) {
        return;
    }
    m_outputFps = fps;
    m_pollTimer.setInterval(1000 / fps);
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applySetAnalyzer();
    }
#endif
}

void TxAnalyzer::start()
{
    if (!m_pollTimer.isActive()) {
        m_pollTimer.start();
    }
}

void TxAnalyzer::stop()
{
    if (m_pollTimer.isActive()) {
        m_pollTimer.stop();
    }
}

bool TxAnalyzer::isRunning() const noexcept
{
    return m_pollTimer.isActive();
}

void TxAnalyzer::poll()
{
#ifdef HAVE_WDSP
    if (!m_analyzerCreated) {
        return;
    }
    int flag = 0;
    GetPixels(m_dispId, /*pixout=*/0, m_pixBuf.data(), &flag);

    if (flag != 0) {
        // sentinel receiverId = -1 to signal "TX panadapter" to consumers
        // (SpectrumWidget Q_UNUSEDs the id today; -1 lets future code
        // distinguish RX1 / TX cleanly without a separate signal).
        emit txFftReady(/*receiverId=*/-1, m_pixBuf);
    }
#endif
}

#ifdef HAVE_WDSP
void TxAnalyzer::applySetAnalyzer()
{
    // From Thetis specHPSDR.cs:529 + :534-643 [v2.10.3.13+501e3f51] —
    // initAnalyzer case 1 (complex FFT) + the SetAnalyzer call at :624.
    //
    // Defaults: window_type=4 (Hamming) at :134; kaiser_pi=14.0 at :145;
    // frame_rate=15 at :335; CLIP_FRACTION=0.04 at :529; KEEP_TIME=0.1
    // at :779.
    constexpr double kClipFraction = 0.04;
    constexpr double kKeepTime     = 0.1;
    const int clip = static_cast<int>(
        std::floor(kClipFraction * static_cast<double>(m_fftSize)));
    const double samplesPerFrame =
        m_sampleRate / static_cast<double>(m_outputFps);
    const int ovrlp = std::max(0,
        static_cast<int>(std::ceil(static_cast<double>(m_fftSize) -
                                    samplesPerFrame)));
    const int max_w = m_fftSize + static_cast<int>(std::min(
        kKeepTime * m_sampleRate,
        kKeepTime * static_cast<double>(m_fftSize) *
                    static_cast<double>(m_outputFps)));
    int flp[1] = {0};

    SetAnalyzer(
        m_dispId,
        /*n_pixout=*/1,
        /*n_fft=*/1,
        /*typ=*/1,
        flp,
        /*sz=*/m_fftSize,
        /*bf_sz=*/m_fftSize,
        // Deliberate divergence from Thetis default (4 = Hamming).  Bench
        // on 2026-05-10 showed Hamming's -42 dB sidelobes spread tone power
        // across the entire useable bin range, and NereusSDR's waterfall
        // colormap (auto-AGC running min/max + 12 dB margin per
        // SpectrumWidget.cpp:3900-3920) has no instantaneous averaging, so
        // every bin lit up warm.  BH4 (-92 dB sidelobes) drops far-from-tone
        // bins below the colormap floor.  Spectrum trace is fine either way
        // because LogRecursive averaging masks instantaneous wide content.
        /*win_type=*/1,            // 1 = Blackman-Harris 4-term (NereusSDR pick)
        /*pi=*/14.0,               // Thetis default (unused for non-Kaiser)
        /*ovrlp=*/ovrlp,
        /*clp=*/clip,              // Thetis: floor(0.04 * fft_size) = 163
        /*fscLin=*/0.0,
        /*fscHin=*/0.0,
        /*n_pix=*/m_numPixels,
        /*n_stch=*/1,
        /*calset=*/0,
        /*fmin=*/0.0,
        /*fmax=*/0.0,
        /*max_w=*/max_w);

    // From Thetis specHPSDR.cs:301-322 [v2.10.3.13+501e3f51] — DetTypePan
    // / DetTypeWF setters.  Default UI state is peak detection (mode 0),
    // average off (mode 0), num_avg = 1.
    SetDisplayDetectorMode(m_dispId, /*pixout=*/0, /*mode=*/0);
    SetDisplayAverageMode (m_dispId, /*pixout=*/0, /*mode=*/0);
    SetDisplayNumAverage  (m_dispId, /*pixout=*/0, /*num=*/1);
    SetDisplaySampleRate  (m_dispId, static_cast<int>(m_sampleRate));
}
#endif // HAVE_WDSP

} // namespace NereusSDR
