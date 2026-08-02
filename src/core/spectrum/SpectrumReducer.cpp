// =================================================================
// src/core/spectrum/SpectrumReducer.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See SpectrumReducer.h for why this
// is a refactor rather than a relocation.
//
// The arithmetic below is transcribed from SpectrumWidget, and the
// transcription is deliberately literal: same floor/ceil convention, same
// clamp order, same guards, same detector and avenger argument lists in the
// same order. The only substitutions are
//
//   m_centerHz     -> m_cfg.centreHz          m_ddcCenterHz  -> m_cfg.streamCentreHz
//   m_bandwidthHz  -> m_cfg.spanHz            m_sampleRateHz -> m_cfg.sampleRateHz
//   displayWidth   -> m_cfg.pixels
//
// where displayWidth was qMax(width() - effectiveStripW(), 800).
//
// Upstream citations travelling with the transcribed code are preserved
// verbatim from the widget per the inline-comment-preservation rule.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 3 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/spectrum/SpectrumReducer.h"

#include "core/spectrum/SpectrumDetector.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// Transcribed from SpectrumWidget::visibleBinRange (SpectrumWidget.cpp:4061-4084).
std::pair<int, int> SpectrumReducer::visibleBinRange(int binCount,
                                                     const ReducerConfig& cfg)
{
    if (binCount <= 0 || cfg.sampleRateHz <= 0.0) {
        return {0, -1};  // empty range — callers compute count = 0
    }

    double binWidth = cfg.sampleRateHz / binCount;
    double fftLowHz = cfg.streamCentreHz - cfg.sampleRateHz / 2.0;

    double displayLowHz  = cfg.centreHz - cfg.spanHz / 2.0;
    double displayHighHz = cfg.centreHz + cfg.spanHz / 2.0;

    int firstBin = static_cast<int>(std::floor((displayLowHz - fftLowHz) / binWidth));
    int lastBin  = static_cast<int>(std::ceil((displayHighHz - fftLowHz) / binWidth));

    firstBin = std::clamp(firstBin, 0, binCount - 1);
    lastBin  = std::clamp(lastBin, 0, binCount - 1);

    if (firstBin > lastBin) {
        firstBin = lastBin;
    }

    return {firstBin, lastBin};
}

// Transcribed from the reduction section of
// SpectrumWidget::updateSpectrumLinear (SpectrumWidget.cpp:2679-2734).
const QVector<float>& SpectrumReducer::reduce(const QVector<float>& binsLinear,
                                             double windowEnb,
                                             double dbmOffset)
{
    // updateSpectrumLinear's first line: no bins, no update. The previously
    // rendered frame stays on screen.
    if (binsLinear.isEmpty()) { return m_outputDbm; }

    // Display pixel count. In the widget this was
    //   const int displayWidth = qMax(width() - effectiveStripW(), 800);
    // with the comment: per Thetis Display.cs:4970 DrawPanadapterDX2D(int W,
    // ...) signature and :4993 nDecimatedWidth = W / m_nDecimation
    // [v2.10.3.13]. Thetis S16 display-side decimation (an additional W / N
    // reduction) is a Phase 2 follow-up; for now we use the full panel width.
    const int displayWidth = m_cfg.pixels;

    // New input domain: pixels is caller-supplied now, so it can be
    // non-positive in a way widget geometry never allowed (that expression
    // floored at 800). Bail before any resize() sees a negative length.
    if (displayWidth <= 0) { return m_outputDbm; }

    const double fftWindowEnb = qMax(windowEnb, 1e-9);

    // Visible bin slice -- CTUN zoom support.  visibleBinRange() maps the
    // current centre +/- span/2 window against the stream centre + sample
    // rate.  When zoomed out, slice == full FFT.
    auto [firstBin, lastBin] = visibleBinRange(binsLinear.size(), m_cfg);
    const int sliceCount = lastBin - firstBin + 1;
    if (sliceCount <= 0) { return m_outputDbm; }

    const double pixPerBin = static_cast<double>(displayWidth) / sliceCount;
    const double binPerPix = (pixPerBin > 0.0) ? 1.0 / pixPerBin : 1.0;
    const double invEnb    = 1.0 / fftWindowEnb;
    // dbmOffset folded into the avenger's power-domain scale so that
    // 10·log10(linear · scale) == 10·log10(linear) + dbmOffset, matching
    // FFTEngine.cpp:348 [v2.10.3.13] (binsDbm = 10·log10 + offset).
    const double dbmScale  = std::pow(10.0, dbmOffset / 10.0);

    const QVector<double> noCorrection;  // per-pixel sub-band gain compensation

    // --- detector -> avenger -> output (dBm) ---
    if (m_linearPixels.size() != displayWidth) {
        m_linearPixels.resize(displayWidth);
    }
    if (m_avenger.numPixels() != displayWidth) {
        m_avenger.resize(displayWidth);
    }
    applySpectrumDetector(m_cfg.detector,
                          sliceCount,
                          displayWidth,
                          pixPerBin,
                          binPerPix,
                          binsLinear.constData() + firstBin,
                          m_linearPixels.data(),
                          invEnb,
                          0.0,
                          static_cast<double>(sliceCount),
                          0.0);
    m_avenger.apply(m_linearPixels,
                    m_cfg.averageMode,
                    m_cfg.averageTau,
                    dbmScale,
                    noCorrection,
                    false,
                    0.0,
                    m_outputDbm);

    return m_outputDbm;
}

}  // namespace NereusSDR
