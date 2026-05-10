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
// CalcSpectrum path at specHPSDR.cs:738-806 [v2.10.3.13], adapted for the
// TX siphon source.  The siphon pushes interleaved I/Q doubles via
// Spectrum0(1, disp, 0, 0, in) inside xsiphon mode 1, so typ=1 (complex
// I/Q) is correct.  Window type 1 (Blackman-Harris 4-term) and 30 fps
// match the panadapter defaults.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-07 — Created by J.J. Boyd (KG4VCF) for the PR #212
//                 follow-up TX waterfall fix.  AI-assisted source-first
//                 protocol via Anthropic Claude Code.
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

    // BENCH DIAGNOSTIC (PR #212 follow-up TX waterfall debug, 2026-05-07):
    // log frame arrival rate + bin-layout probe so we can tell whether
    // (a) the analyzer outputs FFT-shifted [-Nyq..+Nyq] (top bins cluster
    //     around N/2) or raw FFT order (top bins at small + large indices)
    // (b) the absolute peak is uncalibrated (vs Thetis -50 dBm-ish).
    // Drop before commit.
    {
        static int s_frameCount = 0;
        if (flag != 0) s_frameCount++;
        if (flag != 0 && (s_frameCount % 60) == 1) {
            // Find top 5 bin indices by descending dBm.
            const int n = m_pixBuf.size();
            int topIdx[5] = {-1, -1, -1, -1, -1};
            float topVal[5] = {-1e9f, -1e9f, -1e9f, -1e9f, -1e9f};
            for (int i = 0; i < n; ++i) {
                const float v = m_pixBuf[i];
                for (int k = 0; k < 5; ++k) {
                    if (v > topVal[k]) {
                        // Shift down to make room.
                        for (int j = 4; j > k; --j) {
                            topVal[j] = topVal[j-1];
                            topIdx[j] = topIdx[j-1];
                        }
                        topVal[k] = v;
                        topIdx[k] = i;
                        break;
                    }
                }
            }
            // Probe values at canonical positions to detect FFT-shift.
            const int probeIdx[5] = {0, n/4, n/2, 3*n/4, n-1};
            qCInfo(lcDsp).nospace()
                << "TxAnalyzer probe: bins=" << n
                << " top5=[(" << topIdx[0] << "," << topVal[0] << ")"
                << " (" << topIdx[1] << "," << topVal[1] << ")"
                << " (" << topIdx[2] << "," << topVal[2] << ")"
                << " (" << topIdx[3] << "," << topVal[3] << ")"
                << " (" << topIdx[4] << "," << topVal[4] << ")]"
                << " probes=[bin0=" << m_pixBuf[probeIdx[0]]
                << " bin" << probeIdx[1] << "=" << m_pixBuf[probeIdx[1]]
                << " binN/2=" << m_pixBuf[probeIdx[2]]
                << " bin" << probeIdx[3] << "=" << m_pixBuf[probeIdx[3]]
                << " binEnd=" << m_pixBuf[probeIdx[4]] << "]"
                << " rate=" << m_sampleRate;
        }
    }

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
    // Parameter derivation from Thetis specHPSDR.cs:738-806 CalcSpectrum
    // [v2.10.3.13] — TX path (single subspan, no spur elimination, complex
    // I/Q input from xsiphon at TXA.c:586).
    //
    //   n_pixout = 1                  // single pixel output
    //   n_fft    = 1                  // no spur elimination on TX
    //   typ      = 1                  // complex I/Q (siphon delivers
    //                                   interleaved doubles via Spectrum0)
    //   flp      = {0}                // no high-side LO flip
    //   sz       = m_fftSize          // 4096 default (matches FFTEngine)
    //   bf_sz    = m_fftSize          // each xsiphon→Spectrum0 call delivers
    //                                   the full TXA midbuff (sized to
    //                                   ch[channel].dsp_size = 4096 per
    //                                   cmaster.c:113 [v2.10.3.13]).  The
    //                                   prior bench attempt used bf_sz=64
    //                                   (mic input size) which caused the
    //                                   analyzer to read only the first 64
    //                                   samples of each 4096-sample buffer
    //                                   → 1 FFT/sec instead of 30 fps.
    //   win_type = 1                  // Blackman-Harris 4-term
    //   pi       = 0.0                // unused (Kaiser-only)
    //   ovrlp    = max(0, fft_size - sampleRate / fps)  // specHPSDR.cs:784
    //   clp      = 0                  // no per-subspan clip
    //   fscLin   = 0.0
    //   fscHin   = 0.0
    //   n_pix    = m_numPixels        // panadapter width
    //   n_stch   = 1                  // single stitched span
    //   calset   = 0                  // no calibration
    //   fmin     = 0.0
    //   fmax     = 0.0
    //   max_w    = fft_size + min(KEEP_TIME * sampleRate,
    //                              KEEP_TIME * fft_size * fps)
    //              // KEEP_TIME = 0.1 per specHPSDR.cs:779
    int flp[1] = {0};
    constexpr double kKeepTime = 0.1;  // specHPSDR.cs:779 [v2.10.3.13]
    const double samplesPerFrame = m_sampleRate / static_cast<double>(m_outputFps);
    const int ovrlp = std::max(0,
        static_cast<int>(std::ceil(m_fftSize - samplesPerFrame)));
    const int max_w = m_fftSize + static_cast<int>(std::min(
        kKeepTime * m_sampleRate,
        kKeepTime * static_cast<double>(m_fftSize) * static_cast<double>(m_outputFps)));

    // fscLin / fscHin = 0 — output bins span the full ±sample_rate/2 FFT
    // range so the SpectrumWidget can zoom into whatever sub-window the
    // user has set on the panadapter (mirrors RX behavior: the analyzer
    // produces a full-bandwidth spectrum and the widget's visibleBinRange
    // does the zoom).  Thetis's CalcSpectrum (specHPSDR.cs:765-774
    // [v2.10.3.13]) does in-analyzer clipping to the TX filter passband
    // because Thetis's TX panadapter is a fixed filter-band view; we
    // intentionally diverge here so MOX-up keeps the user's RX zoom
    // level rather than yanking it to a 3 kHz filter window.

    SetAnalyzer(
        m_dispId,
        /*n_pixout=*/1,
        /*n_fft=*/1,
        /*typ=*/1,
        flp,
        /*sz=*/m_fftSize,
        /*bf_sz=*/m_fftSize,
        /*win_type=*/1,
        /*pi=*/0.0,
        /*ovrlp=*/ovrlp,
        /*clp=*/0,
        /*fscLin=*/0.0,
        /*fscHin=*/0.0,
        /*n_pix=*/m_numPixels,
        /*n_stch=*/1,
        /*calset=*/0,
        /*fmin=*/0.0,
        /*fmax=*/0.0,
        /*max_w=*/max_w);

    // Detector + average defaults (peak detect, no averaging) — matches
    // panadapter first-time setup in specHPSDR.cs.
    SetDisplayDetectorMode(m_dispId, /*pixout=*/0, /*mode=*/0);
    SetDisplayAverageMode(m_dispId, /*pixout=*/0, /*mode=*/0);
    SetDisplayNumAverage(m_dispId, /*pixout=*/0, /*num=*/1);
    SetDisplaySampleRate(m_dispId, static_cast<int>(m_sampleRate));
}
#endif // HAVE_WDSP

} // namespace NereusSDR
