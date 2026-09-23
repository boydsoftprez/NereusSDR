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
//   2026-05-10 — Phase 3M-5d: added 9-control state surface (FFT size,
//                 window type, pan/wf detector + averaging + AvTime +
//                 pan normalize) wired to the same WDSP setters Thetis
//                 uses, plus n_pixout bumped from 1 to 2 so pan + wf
//                 receive independent detector + averaging.  Reverted
//                 the BH4 default-window divergence to Thetis-faithful
//                 Hamming (combo index 4) per controller decision
//                 2026-05-10.  AI-assisted source-first via Anthropic
//                 Claude Code.
// =================================================================

#include "TxAnalyzer.h"

#include "AppSettings.h"
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
    // Phase 3M-5d: persisted settings get hydrated BEFORE XCreateAnalyzer +
    // the first applySetAnalyzer call so the WDSP analyzer comes up with
    // user choices already in effect (no flash of pre-default config on
    // launch).  loadSettings is silent on missing keys; defaults from
    // header member initialisers remain in effect.
    loadSettings();

    m_pixBuf.resize(m_numPixels);
    m_pixBufWf.resize(m_numPixels);

    m_pollTimer.setTimerType(Qt::PreciseTimer);
    m_pollTimer.setInterval(1000 / m_outputFps);
    connect(&m_pollTimer, &QTimer::timeout, this, &TxAnalyzer::poll);

#ifdef HAVE_WDSP
    // Allocate the WDSP analyzer instance.  Parameters from
    // Thetis MeterManager.cs:42024 [v2.10.3.13+501e3f51]:
    //   _disp = cmaster.AllocAnalyzer(..., 262144);
    //                            // must be pow2, 262144 = max size
    // The 262144 cap matches the Thetis FFT slider max position
    // (setup.designer.cs:36638 Maximum=6 + setup.cs:18138 formula
    // 4096 * 2^slider).  Earlier draft of 3M-5d shipped 16384 which
    // is correct for the wideband display (wbDisplay.cs:4655) but
    // silently truncates panadapter FFTs that exceed it — re-planning
    // FFTW with sz > buffer size at analyzer.c:1223 produces garbage
    // output instead of any visible change.  Fixed at 3M-5d bench.
    // app_data_path is empty: FFTW wisdom is managed centrally by
    // WdspEngine, not per-analyzer.
    int success = 0;
    char emptyPath[1] = {0};
    XCreateAnalyzer(m_dispId,
                    &success,
                    /*m_size=*/262144,
                    /*m_LO=*/1,
                    /*m_stitch=*/1,
                    emptyPath);
    if (success == 0) {
        m_analyzerCreated = true;
        // Deliberately NOT applySetAnalyzer() here.
        //
        // SetAnalyzer builds FFTW_PATIENT plans the first time a size is set
        // (analyzer.c:1221-1224), and at the 32768-point default that is not
        // cheap. This constructor runs from buildUI(), before any radio
        // connection has started WdspEngine::initialize() and its background
        // wisdom path, so planning here happens synchronously on the GUI
        // thread before the connection UI is even on screen -- a cold launch
        // would appear to hang. Deferred to the first start(), which is
        // behind the MOX edge and therefore behind the connection flow.
        // Found by Codex on PR #317.
        //
        // Every setter below still records its state; applySetAnalyzer is a
        // no-op until armed, so the analyzer comes up with all of it applied
        // at once on the first key-up.
        m_deferSetAnalyzer = true;
        // 3M-5d: SetAnalyzer alone does not push the per-pixout detector /
        // averaging / normalize state — those need their own WDSP calls so
        // pixout 0 + 1 carry the user's persisted choices on cold boot.
        applyDetectorMode(/*pixout=*/0, m_panDetector);
        applyDetectorMode(/*pixout=*/1, m_wfDetector);
        applyAverageMode (/*pixout=*/0, m_panAveraging);
        applyAverageMode (/*pixout=*/1, m_wfAveraging);
        applyAvTau       (/*pixout=*/0, m_panAvTimeMs);
        applyAvTau       (/*pixout=*/1, m_wfAvTimeMs);
        applyNormalizePan();
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
    m_pixBufWf.resize(m_numPixels);
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
    // First key-up is where the deferred FFTW planning happens: behind the
    // connection flow, and behind WdspEngine's wisdom path, rather than on
    // the GUI thread during buildUI().
    if (m_deferSetAnalyzer) {
        m_deferSetAnalyzer = false;
        if (m_analyzerCreated) {
            applySetAnalyzer();
            applyDetectorMode(/*pixout=*/0, m_panDetector);
            applyDetectorMode(/*pixout=*/1, m_wfDetector);
            applyAverageMode (/*pixout=*/0, m_panAveraging);
            applyAverageMode (/*pixout=*/1, m_wfAveraging);
            applyAvTau       (/*pixout=*/0, m_panAvTimeMs);
            applyAvTau       (/*pixout=*/1, m_wfAvTimeMs);
            applyNormalizePan();
        }
    }
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
    // 3M-5d: drain both analyzer pixel-out planes per Thetis
    // specHPSDR.cs:471-480 [v2.10.3.13+501e3f51] _pixel_out = 2 default.
    // pixout 0 = spectrum trace (DetTypePan + AverageMode applied by WDSP);
    // pixout 1 = waterfall      (DetTypeWF  + AverageModeWF applied by WDSP).
    // The two planes are emitted as separate signals so SpectrumWidget can
    // wire each one to its dedicated render path (trace vs pushWaterfallRow)
    // without re-applying detector + averaging on top.
    //
    // sentinel receiverId = -1 to signal "TX panadapter" to consumers.
    int flagPan = 0;
    GetPixels(m_dispId, /*pixout=*/0, m_pixBuf.data(), &flagPan);
    if (flagPan != 0) {
        emit txFftReady(/*receiverId=*/-1, m_pixBuf);
    }

    int flagWf = 0;
    GetPixels(m_dispId, /*pixout=*/1, m_pixBufWf.data(), &flagWf);
    if (flagWf != 0) {
        emit txWaterfallReady(/*receiverId=*/-1, m_pixBufWf);
    }
#endif
}

#ifdef HAVE_WDSP
// ---------------------------------------------------------------------------
// spanClipBins — turn that window into SetAnalyzer's fscLin / fscHin
//
// From Thetis specHPSDR.cs:762-775 [v2.10.3.15] — CalcSpectrum:
//
//     int upper_freq = filter_high;
//     int lower_freq = filter_low;
//     //bandwidth to clip off on the high and low sides
//     double high_clip_bw = 0.5 * sample_rate - upper_freq;
//     double low_clip_bw = 0.5 * sample_rate + lower_freq;
//     //calculate the width of each frequency bin
//     double bin_width = (double)sample_rate / fft_size;
//     //calculate span clip parameters
//     int fsclipH = (int)Math.Floor(high_clip_bw / bin_width);
//     int fsclipL = (int)Math.Ceiling(low_clip_bw / bin_width);
//
// floor on the high side and ceil on the low side is not a typo in the
// port; it is what upstream does, and it biases the surviving span to
// sit inside the requested window rather than overhang it.
// ---------------------------------------------------------------------------
std::pair<int, int> TxAnalyzer::spanClipBins(int lowHz, int highHz,
                                             double sampleRateHz, int fftSize)
{
    if (sampleRateHz <= 0.0 || fftSize <= 0) {
        return {0, 0};
    }
    const double highClipBw = 0.5 * sampleRateHz - static_cast<double>(highHz);
    const double lowClipBw  = 0.5 * sampleRateHz + static_cast<double>(lowHz);
    const double binWidth   = sampleRateHz / static_cast<double>(fftSize);

    int fsclipH = static_cast<int>(std::floor(highClipBw / binWidth));
    int fsclipL = static_cast<int>(std::ceil(lowClipBw / binWidth));

    // A window wider than the baseband, or an inverted one, would ask for a
    // negative clip. WDSP has no meaning for that, and the sum must leave at
    // least one bin standing or the analyzer emits nothing at all and the
    // pan goes blank -- indistinguishable, from the operator's seat, from
    // the frozen waterfall this whole change exists to fix.
    if (fsclipH < 0) { fsclipH = 0; }
    if (fsclipL < 0) { fsclipL = 0; }
    if (fsclipL + fsclipH >= fftSize) {
        return {0, 0};
    }
    return {fsclipL, fsclipH};
}

void TxAnalyzer::setBlockSize(int frames)
{
    if (frames <= 0 || m_blockSize == frames) {
        return;
    }
    m_blockSize = frames;
    if (m_analyzerCreated) {
        applySetAnalyzer();
    }
}

void TxAnalyzer::setSpectrumWindow(int lowHz, int highHz)
{
    if (m_spanLowHz == lowHz && m_spanHighHz == highHz) {
        return;
    }
    m_spanLowHz  = lowHz;
    m_spanHighHz = highHz;
    if (m_analyzerCreated) {
        applySetAnalyzer();
    }
}

void TxAnalyzer::applySetAnalyzer()
{
    // Held off until the first start(); see the constructor.
    if (m_deferSetAnalyzer) {
        return;
    }

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

    // Span clip. Both zero leaves the analyzer emitting the full +/-48 kHz
    // baseband, which is what it did before the 2026-08-04 bench and is
    // still the state until MOX configures a filter-derived window.
    const bool windowed = !(m_spanLowHz == 0 && m_spanHighHz == 0);
    const auto [fsclipL, fsclipH] =
        windowed
            ? spanClipBins(m_spanLowHz, m_spanHighHz, m_sampleRate, m_fftSize)
            : std::pair<int, int>{0, 0};

    // Symmetric clip must go to zero once a span window is in play, and this
    // is not a tidy-up: WDSP subtracts the span clips from a span ALREADY
    // reduced by 2*clp.
    //
    //   From wdsp/analyzer.c:1283 [TAPR v1.29]:
    //     a->pix_per_bin = (double)a->num_pixels /
    //       ((double)(a->num_stitch * (a->out_size - 1 - 2 * a->clip))
    //        - a->fsclipL - a->fsclipH - 1.0);
    //
    // With the 0.04 clip left in at 32768 bins that denominator goes
    // NEGATIVE for a 3 kHz window (30147 - 15295 - 16384 - 1), and the
    // analyzer emits nothing at all -- a black pan, which at the bench is
    // indistinguishable from the frozen waterfall this work exists to fix.
    // Bench 2026-08-05: TUNE on a 7000DLE showed exactly that.
    //
    // Thetis says so in as many words, and this is the line that was missed
    // when the fsclip computation was ported without its companion.
    //   From Thetis specHPSDR.cs:776-777 [v2.10.3.15], inside CalcSpectrum:
    //     //no need for any symmetrical clipping
    //     int sclip = 0;
    // The 0.04 CLIP_FRACTION belongs to the OTHER path, initAnalyzer
    // (specHPSDR.cs:529), which does no span clipping and therefore has
    // room for it.
    const int effectiveClip = windowed ? 0 : clip;

    // 3M-5d: n_pixout = 2 mirrors Thetis specHPSDR.cs:471 [v2.10.3.13+501e3f51]
    // (_pixel_out default = 2) so pan + waterfall planes carry independent
    // DetType + AverageMode applied via SetDisplayDetectorMode /
    // SetDisplayAverageMode below.  Previous NereusSDR n_pixout=1 forced
    // pan and waterfall to share one pixel-out — the WF combos at Setup
    // had no effect.
    //
    // win_type now reads m_windowType (default 4 = Hamming).  The
    // 3M-5b BH4 divergence was reverted by 3M-5d per controller decision
    // 2026-05-10; user can still pick BH4 via Setup → Display → TX → FFT
    // → Window combo if splatter returns.
    SetAnalyzer(
        m_dispId,
        /*n_pixout=*/m_nPixout,
        /*n_fft=*/1,
        /*typ=*/1,
        flp,
        /*sz=*/m_fftSize,
        // bf_sz is the SIPHON's push size, not the FFT size. See
        // setBlockSize. Falls back to m_fftSize only when nothing has told
        // us the real block size yet.
        /*bf_sz=*/(m_blockSize > 0 ? m_blockSize : m_fftSize),
        /*win_type=*/m_windowType,
        /*pi=*/14.0,               // Thetis default (unused for non-Kaiser)
        /*ovrlp=*/ovrlp,
        /*clp=*/effectiveClip,     // 0 while span-clipped; see above
        // fscLin / fscHin are BIN COUNTS to clip from the low and high ends,
        // not frequencies. Thetis computes them in CalcSpectrum
        // (specHPSDR.cs:772-774 [v2.10.3.15]) and passes them in these two
        // slots. Leaving them at zero, as this did before, is what made the
        // transmit trace land at the wrong dial frequency: the analyzer
        // emitted the whole baseband while the pan kept its RX window, and
        // SpectrumWidget stretched one across the other.
        /*fscLin=*/static_cast<double>(fsclipL),
        /*fscHin=*/static_cast<double>(fsclipH),
        /*n_pix=*/m_numPixels,
        /*n_stch=*/1,
        /*calset=*/0,
        /*fmin=*/0.0,
        /*fmax=*/0.0,
        /*max_w=*/max_w);

    SetDisplaySampleRate(m_dispId, static_cast<int>(m_sampleRate));
    ++m_analyzerConfigCount;

    // Every parameter WDSP is actually given, on each reconfiguration.
    // Kept because it is what made the 2026-08-05 bench tractable: bf_sz
    // silently carrying the FFT size, and the symmetric clip overrunning the
    // span, are both invisible from the outside and obvious here. Fires only
    // when the analyzer is reconfigured, not per frame.
    qCDebug(lcDsp).nospace()
        << "TxAnalyzer SetAnalyzer: disp=" << m_dispId
        << " fft=" << m_fftSize
        << " bf_sz=" << (m_blockSize > 0 ? m_blockSize : m_fftSize)
        << " (blockSize=" << m_blockSize << ")"
        << " win=" << m_windowType
        << " ovrlp=" << ovrlp
        << " clp=" << effectiveClip
        << " fsclipL=" << fsclipL << " fsclipH=" << fsclipH
        << " n_pix=" << m_numPixels
        << " rate=" << m_sampleRate
        << " window=[" << m_spanLowHz << "," << m_spanHighHz << "]";
}

void TxAnalyzer::applyDetectorMode(int pixout, int mode)
{
    SetDisplayDetectorMode(m_dispId, pixout, mode);
    ++m_analyzerConfigCount;
}

void TxAnalyzer::applyAverageMode(int pixout, int mode)
{
    // From Thetis specHPSDR.cs:382-418 [v2.10.3.13+501e3f51] — AverageMode
    // / AverageModeWF setters call SetDisplayAverageMode(disp, pixout,
    // value).  NereusSDR omits Thetis's peak_on / average_on toggle
    // wrapping (those are top-of-pan UI buttons not present in
    // NereusSDR's TX Display tab).  The combo selection writes through
    // directly.
    SetDisplayAverageMode(m_dispId, pixout, mode);
    ++m_analyzerConfigCount;
}

void TxAnalyzer::applyAvTau(int pixout, int avTimeMs)
{
    // From Thetis specHPSDR.cs:351-380 [v2.10.3.13+501e3f51] —
    //   tau = ms * 0.001
    //   avb = exp(-1.0 / (frame_rate * tau))
    //   display_average = max(2, min(MAX_AV_FRAMES, frame_rate * tau))
    //   MAX_AV_FRAMES = 60 at :348
    constexpr int kMaxAvFrames = 60;
    const double tau = 0.001 * static_cast<double>(std::max(1, avTimeMs));
    const double fps = static_cast<double>(m_outputFps);
    const double frameTau = fps * tau;
    const double avb = std::exp(-1.0 / std::max(1e-9, frameTau));
    const int displayAverage = std::max(2,
        std::min(kMaxAvFrames, static_cast<int>(frameTau)));
    SetDisplayAvBackmult(m_dispId, pixout, avb);
    SetDisplayNumAverage(m_dispId, pixout, displayAverage);
    ++m_analyzerConfigCount;
}

void TxAnalyzer::applyNormalizePan()
{
    // From Thetis specHPSDR.cs:288-294 [v2.10.3.13+501e3f51] —
    //   if (norm_oneHz_pan && det_type_pan in {2,3,4})
    //       SetDisplayNormOneHz(disp, 0, true);
    //   else
    //       SetDisplayNormOneHz(disp, 0, false);
    // Mirrors Thetis updateNormalizePan() exactly.
    const bool gated = m_panNormalize
        && (m_panDetector == 2 || m_panDetector == 3 || m_panDetector == 4);
    SetDisplayNormOneHz(m_dispId, /*pixout=*/0, gated ? 1 : 0);
    ++m_analyzerConfigCount;
}
#endif // HAVE_WDSP

// ── Phase 3M-5d: 9-control setters ───────────────────────────────────────
// Each setter persists via AppSettings and pushes the new value into WDSP
// (either via SetAnalyzer for whole-analyzer reconfigure, or via the
// finer-grained Set*Display* setters for per-pixout state).  The bare
// applySetAnalyzer fallback inside HAVE_WDSP-undefined unit-test builds
// is a no-op; the test seam counter (m_analyzerConfigCount) still ticks
// from the setters themselves so coverage of the wiring contract does
// not depend on a live WDSP runtime.

void TxAnalyzer::setFftSize(int n)
{
    if (n <= 0 || n == m_fftSize) {
        return;
    }
    m_fftSize = n;
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applySetAnalyzer();
    } else {
        ++m_analyzerConfigCount;
    }
#else
    ++m_analyzerConfigCount;
#endif
    saveSettings();
}

void TxAnalyzer::setFftSizeSliderPosition(int position)
{
    // From Thetis setup.cs:18138 [v2.10.3.13+501e3f51]:
    //   FFTSize = (int)(4096 * Math.Pow(2, Math.Floor(slider.Value)));
    position = std::max(0, position);
    const int newSize = 4096 << position;
    setFftSize(newSize);
}

double TxAnalyzer::binWidthHz() const noexcept
{
    if (m_fftSize <= 0) {
        return 0.0;
    }
    return m_sampleRate / static_cast<double>(m_fftSize);
}

void TxAnalyzer::setWindowType(int t)
{
    // Combo index range 0..6 per comboTXDispWinType ordering at
    // setup.designer.cs:36555-36562 [v2.10.3.13+501e3f51].
    t = std::clamp(t, 0, 6);
    if (t == m_windowType) {
        return;
    }
    m_windowType = t;
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applySetAnalyzer();   // window_type is a SetAnalyzer parameter
    } else {
        ++m_analyzerConfigCount;
    }
#else
    ++m_analyzerConfigCount;
#endif
    saveSettings();
}

void TxAnalyzer::setPanDetector(int d)
{
    // From Thetis specHPSDR.cs:301-311 [v2.10.3.13+501e3f51]:
    //   det_type_pan = value;
    //   SetDisplayDetectorMode(disp, 0, value);
    //   updateNormalizePan();
    d = std::clamp(d, 0, 4);   // 0=Peak..4=RMS per Pan combo
    if (d == m_panDetector) {
        return;
    }
    m_panDetector = d;
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applyDetectorMode(/*pixout=*/0, m_panDetector);
        applyNormalizePan();  // gate may have flipped
    } else {
        ++m_analyzerConfigCount;
    }
#else
    ++m_analyzerConfigCount;
#endif
    saveSettings();
}

void TxAnalyzer::setPanAveraging(int m)
{
    // From Thetis specHPSDR.cs:382-398 [v2.10.3.13+501e3f51]:
    //   av_mode = value;
    //   SetDisplayAverageMode(disp, 0, avm);
    m = std::clamp(m, 0, 3);   // 0=None..3=Log Recursive
    if (m == m_panAveraging) {
        return;
    }
    m_panAveraging = m;
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applyAverageMode(/*pixout=*/0, m_panAveraging);
    } else {
        ++m_analyzerConfigCount;
    }
#else
    ++m_analyzerConfigCount;
#endif
    saveSettings();
}

void TxAnalyzer::setPanAvTimeMs(int ms)
{
    // From Thetis setup.cs:18122-18127 [v2.10.3.13+501e3f51]:
    //   AvTau = 0.001 * (double)udTXDisplayAVGTime.Value;
    // NumericUpDownTS Min=1, Max=9999 per setup.designer.cs:36743-36746.
    ms = std::clamp(ms, 1, 9999);
    if (ms == m_panAvTimeMs) {
        return;
    }
    m_panAvTimeMs = ms;
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applyAvTau(/*pixout=*/0, m_panAvTimeMs);
    } else {
        ++m_analyzerConfigCount;
    }
#else
    ++m_analyzerConfigCount;
#endif
    saveSettings();
}

double TxAnalyzer::panAvTauSeconds() const noexcept
{
    return 0.001 * static_cast<double>(m_panAvTimeMs);
}

void TxAnalyzer::setPanNormalize(bool on)
{
    // From Thetis setup.cs:18129-18134 [v2.10.3.13+501e3f51]:
    //   NormOneHzPan = chkDispTXNormalize.Checked;
    // The WDSP-side gate on det_type_pan in {2,3,4} lives in
    // applyNormalizePan() (ported from specHPSDR.cs:288-294).
    if (on == m_panNormalize) {
        return;
    }
    m_panNormalize = on;
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applyNormalizePan();
    } else {
        ++m_analyzerConfigCount;
    }
#else
    ++m_analyzerConfigCount;
#endif
    saveSettings();
}

bool TxAnalyzer::panNormalizeEnabled() const noexcept
{
    // From Thetis setup.cs:18111-18112 [v2.10.3.13+501e3f51]:
    //   //[2.10.3.5]MW0LGE note: see updateNormalizePan() in specHPSDR as
    //   //it only applies to pan detector type 2,3,4
    //   chkDispTXNormalize.Enabled = ...DetTypePan >= 2;
    return m_panDetector >= 2;
}

void TxAnalyzer::setWfDetector(int d)
{
    // From Thetis specHPSDR.cs:313-322 [v2.10.3.13+501e3f51]:
    //   det_type_wf = value;
    //   SetDisplayDetectorMode(disp, 1, value);
    d = std::clamp(d, 0, 3);   // 0=Peak..3=Sample per WF combo (no RMS)
    if (d == m_wfDetector) {
        return;
    }
    m_wfDetector = d;
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applyDetectorMode(/*pixout=*/1, m_wfDetector);
    } else {
        ++m_analyzerConfigCount;
    }
#else
    ++m_analyzerConfigCount;
#endif
    saveSettings();
}

void TxAnalyzer::setWfAveraging(int m)
{
    // From Thetis specHPSDR.cs:402-418 [v2.10.3.13+501e3f51]:
    //   av_mode_wf = value;
    //   SetDisplayAverageMode(disp, 1, avm);
    m = std::clamp(m, 0, 3);   // 0=None..3=Log Recursive
    if (m == m_wfAveraging) {
        return;
    }
    m_wfAveraging = m;
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applyAverageMode(/*pixout=*/1, m_wfAveraging);
    } else {
        ++m_analyzerConfigCount;
    }
#else
    ++m_analyzerConfigCount;
#endif
    saveSettings();
}

void TxAnalyzer::setWfAvTimeMs(int ms)
{
    // From Thetis setup.cs:18166-18171 [v2.10.3.13+501e3f51]:
    //   AvTauWF = 0.001 * (double)udTXDisplayAVTime.Value;
    ms = std::clamp(ms, 1, 9999);
    if (ms == m_wfAvTimeMs) {
        return;
    }
    m_wfAvTimeMs = ms;
#ifdef HAVE_WDSP
    if (m_analyzerCreated) {
        applyAvTau(/*pixout=*/1, m_wfAvTimeMs);
    } else {
        ++m_analyzerConfigCount;
    }
#else
    ++m_analyzerConfigCount;
#endif
    saveSettings();
}

double TxAnalyzer::wfAvTauSeconds() const noexcept
{
    return 0.001 * static_cast<double>(m_wfAvTimeMs);
}

// ── Settings persistence ────────────────────────────────────────────────
// 9 keys, PascalCase per NereusSDR convention.  Booleans as
// "True" / "False" strings.  No per-pan-index suffix — there is only one
// TX analyzer (single TX disp at kTxDispId=5).
//
// All defaults match Thetis ship values verified at v2.10.3.13+501e3f51;
// see header member initialisers + the cite comments alongside each.

void TxAnalyzer::loadSettings()
{
    auto& s = AppSettings::instance();
    auto readInt = [&s](const QString& key, int fallback) -> int {
        bool ok = false;
        const int v = s.value(key, fallback).toInt(&ok);
        return ok ? v : fallback;
    };
    auto readBool = [&s](const QString& key, bool fallback) -> bool {
        const QString v = s.value(key,
            fallback ? QStringLiteral("True") : QStringLiteral("False"))
                .toString();
        return v.compare(QStringLiteral("True"), Qt::CaseInsensitive) == 0;
    };

    m_fftSize       = readInt (QStringLiteral("DisplayTxFftSize"),      m_fftSize);
    m_windowType    = readInt (QStringLiteral("DisplayTxWindowType"),   m_windowType);
    m_panDetector   = readInt (QStringLiteral("DisplayTxPanDetector"),  m_panDetector);
    m_panAveraging  = readInt (QStringLiteral("DisplayTxPanAveraging"), m_panAveraging);
    m_panAvTimeMs   = readInt (QStringLiteral("DisplayTxPanAvTimeMs"),  m_panAvTimeMs);
    m_panNormalize  = readBool(QStringLiteral("DisplayTxPanNormalize"), m_panNormalize);
    m_wfDetector    = readInt (QStringLiteral("DisplayTxWfDetector"),   m_wfDetector);
    m_wfAveraging   = readInt (QStringLiteral("DisplayTxWfAveraging"),  m_wfAveraging);
    m_wfAvTimeMs    = readInt (QStringLiteral("DisplayTxWfAvTimeMs"),   m_wfAvTimeMs);
}

void TxAnalyzer::saveSettings()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("DisplayTxFftSize"),      QString::number(m_fftSize));
    s.setValue(QStringLiteral("DisplayTxWindowType"),   QString::number(m_windowType));
    s.setValue(QStringLiteral("DisplayTxPanDetector"),  QString::number(m_panDetector));
    s.setValue(QStringLiteral("DisplayTxPanAveraging"), QString::number(m_panAveraging));
    s.setValue(QStringLiteral("DisplayTxPanAvTimeMs"),  QString::number(m_panAvTimeMs));
    s.setValue(QStringLiteral("DisplayTxPanNormalize"),
               m_panNormalize ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("DisplayTxWfDetector"),   QString::number(m_wfDetector));
    s.setValue(QStringLiteral("DisplayTxWfAveraging"),  QString::number(m_wfAveraging));
    s.setValue(QStringLiteral("DisplayTxWfAvTimeMs"),   QString::number(m_wfAvTimeMs));
    s.save();
}

} // namespace NereusSDR
