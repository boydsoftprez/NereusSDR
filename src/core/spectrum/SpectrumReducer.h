#pragma once
// =================================================================
// src/core/spectrum/SpectrumReducer.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The reduction mathematics is lifted
// verbatim out of SpectrumWidget::updateSpectrumLinear() and
// SpectrumWidget::visibleBinRange() (src/gui/SpectrumWidget.cpp:2679-2734
// and :4061-4084 as of 2026-08-02); those two blocks are themselves
// NereusSDR-original glue around the WDSP analyzer.c ports that live in
// SpectrumDetector.h / SpectrumAvenger.h, and their own upstream citations
// travel with them into SpectrumReducer.cpp.
//
// Why this is a refactor and not a relocation: the stage lived inside a
// QRhiWidget, visibleBinRange() was private and read member state, and the
// output pixel count was derived from widget geometry:
//
//   const int displayWidth = qMax(width() - effectiveStripW(), 800);
//
// where effectiveStripW() is itself a visibility query on the dBm scale
// strip. Design section 9.4 requires the pixel count to be per-endpoint and
// client-requested, so it becomes an explicit input (ReducerConfig::pixels)
// and the crop window becomes explicit too (centreHz / spanHz against
// streamCentreHz / sampleRateHz) instead of reading m_centerHz /
// m_bandwidthHz / m_ddcCenterHz / m_sampleRateHz off a widget.
//
// SpectrumWidget keeps computing displayWidth from its own width and feeds
// it in as ReducerConfig::pixels, so local rendering is unchanged; the
// daemon supplies its own value with no widget in the process.
//
// One instance == one plane. SpectrumWidget holds two (spectrum trace and
// waterfall), matching WDSP's per-plane ANALYZER_INFO[] model where
// detector type and averaging mode are independent per plane.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 3 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/spectrum/SpectrumAvenger.h"
#include "core/spectrum/SpectrumDetectorMode.h"

#include <QVector>

#include <utility>

namespace NereusSDR {

/// Everything the crop-and-reduce stage needs, with no widget in sight.
///
/// The crop window is (centreHz +/- spanHz/2) evaluated against a stream
/// whose bins span (streamCentreHz +/- sampleRateHz/2). These are the
/// former SpectrumWidget members m_centerHz, m_bandwidthHz, m_ddcCenterHz
/// and m_sampleRateHz respectively.
struct ReducerConfig {
    /// Output pixel count. Explicit input, never widget geometry: per
    /// design section 9.4a this is a per-endpoint, client-requested
    /// bandwidth lever. SpectrumWidget passes its own displayWidth here.
    int    pixels        {1024};
    double centreHz      {0.0};    // crop window centre
    double spanHz        {0.0};    // crop window width
    double streamCentreHz{0.0};    // DDC centre the bins are relative to
    double sampleRateHz  {0.0};

    /// Bin-to-pixel reduction policy (WDSP analyzer detector modes).
    SpectrumDetectorMode detector {SpectrumDetectorMode::Peak};

    /// WDSP av_mode wire code: -1 peak-hold / 0 none / 1 recursive linear /
    /// 2 window linear / 3 recursive log. See SpectrumAvenger.h. Kept as an
    /// int rather than an enum because that is the wire format, and because
    /// the GUI's SpectrumAveraging enum stays GUI-side.
    int    averageMode   {0};

    /// Recursive-averaging back-multiplier, passed straight through as
    /// SpectrumAvenger::apply()'s avBackmult argument (modes 1 and 3).
    /// SpectrumWidget supplies m_spectrumAverageAlpha here, which it
    /// derives from a time constant via the Thetis formula
    /// alpha = exp(-1 / (fps * tau)) (specHPSDR.cs:351-380 [v2.10.3.13]) --
    /// hence the name; the value on the wire is the alpha, not the tau.
    double averageTau    {0.12};
};

/// Crop a linear-power FFT bin array to a frequency window and reduce it to
/// a fixed pixel count, then apply frame averaging and convert to dB.
///
/// Owns the per-plane state that used to be SpectrumWidget members: the
/// detector's linear-power scratch buffer, the SpectrumAvenger accumulators,
/// and the dB output buffer.
class SpectrumReducer {
public:
    void setConfig(const ReducerConfig& cfg) { m_cfg = cfg; }
    const ReducerConfig& config() const { return m_cfg; }

    /// binsLinear is |X[k]|^2 as FFTEngine::fftReadyLinear emits.
    /// Writes exactly config().pixels dBm values into pixelsOut, resizing it
    /// if needed. Never resizes to a widget.
    ///
    /// windowEnb is the FFT window's equivalent noise bandwidth in bins,
    /// clamped to 1e-9 before inversion exactly as the widget did.
    /// dbmOffset is folded into the avenger's power-domain scale, so that
    /// 10*log10(linear * scale) == 10*log10(linear) + dbmOffset.
    ///
    /// pixelsOut is a caller-owned out-parameter rather than a reference to
    /// an internal buffer, and that is load-bearing on the render hot path.
    /// QVector is implicitly shared: handing back a reference to a member
    /// would put the caller's buffer and ours on one data block at refcount
    /// 2, and the avenger's non-const pixelsOut[i] writes on the NEXT frame
    /// would detach -- a fresh allocation plus a full-buffer memcpy that is
    /// then immediately overwritten, every frame, on both planes. Writing
    /// through the caller's buffer keeps it at refcount 1 and allocates
    /// nothing in steady state.
    ///
    /// Degenerate input (empty bins, a crop window that selects no bins --
    /// which happens whenever sampleRateHz is still 0 -- or a non-positive
    /// pixel count) is a no-op leaving pixelsOut untouched. That mirrors
    /// updateSpectrumLinear's early returns, which leave the last good frame
    /// on screen.
    void reduce(const QVector<float>& binsLinear,
                double windowEnb,
                double dbmOffset,
                QVector<float>& pixelsOut);

    /// Drop accumulated averaging history. Callers use this on anything
    /// that invalidates the running average: FFT size change, averaging
    /// mode change, sample rate change. Mirrors WDSP
    /// SetDisplayAverageMode's re-init at analyzer.c:1854 [v2.10.3.13].
    void clearAveraging() { m_avenger.clear(); }

    /// First and last source bin covered by the crop window, clamped to range.
    ///
    /// Returns {0, -1} -- an empty range, so that (last - first + 1) <= 0 --
    /// when there are no bins or the stream rate is not yet known. Callers
    /// must treat a non-positive count as "skip this frame"; they must not
    /// index the bin array with the returned pair without checking.
    static std::pair<int, int> visibleBinRange(int binCount,
                                               const ReducerConfig& cfg);

private:
    ReducerConfig  m_cfg;
    QVector<float> m_linearPixels;  // detector output, linear power
    SpectrumAvenger m_avenger;
};

}  // namespace NereusSDR
