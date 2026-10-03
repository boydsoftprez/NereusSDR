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
//   2026-09-25 : R-R3-39 (station Task 32) by J.J. Boyd (KG4VCF): with a
//                 transmit lane every WDSP analyzer call runs there
//                 (runWdsp) and the poll's pixels come back through
//                 DspControlThread::request. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-25 : R-R3-39 / R-IOS-03 by J.J. Boyd (KG4VCF): applyStationRates()
//                 holds the rate and frame rate MainWindow set, so nereusd
//                 sets up its analyzer the same way. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-26 : Task 27 (R-R3-49) by J.J. Boyd (KG4VCF): currentArgs()
//                 computes the SetAnalyzer arguments once; applySetAnalyzer
//                 passes them. AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-26 : Task 28 (R-R3-49, A11) by J.J. Boyd (KG4VCF):
//                 clampViewToBaseband(), MainWindow's syncTxAnalyzerToView
//                 rule moved here unchanged for TxDisplayFeed. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-26 : Tasks 27-29 fix wave (R-R3-49) by J.J. Boyd (KG4VCF):
//                 setView(), the window and pixel count in one SetAnalyzer.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27 : Parity Task 30 (R-R3-49, R-R3-21, A12) by J.J. Boyd
//                 (KG4VCF): reloadSetting(), a remote window's write of one
//                 of the nine keys applied to the Core's analyzer at once,
//                 keyed or not, through the local page's setters.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "TxAnalyzer.h"

#include "AppSettings.h"
#include "DspControlThread.h"
#include "LogCategories.h"
#include "wdsp_api.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

TxAnalyzer::TxAnalyzer(int dispId, QObject* parent, DspControlThread* lane,
                       bool persistSettings)
    : QObject(parent)
    , m_persistSettings(persistSettings)
    , m_dispId(dispId)
{
    m_lane = lane;
    // Phase 3M-5d: persisted settings get hydrated BEFORE XCreateAnalyzer +
    // the first applySetAnalyzer call so the WDSP analyzer comes up with
    // user choices already in effect (no flash of pre-default config on
    // launch).  loadSettings is silent on missing keys; defaults from
    // header member initialisers remain in effect.
    if (m_persistSettings) { loadSettings(); }

    m_pixBuf.resize(m_numPixels);
    m_pixBufWf.resize(m_numPixels);

    m_pollTimer.setTimerType(Qt::PreciseTimer);
    m_pollTimer.setInterval(1000 / m_outputFps);
    connect(&m_pollTimer, &QTimer::timeout, this, &TxAnalyzer::poll);

#ifdef HAVE_WDSP
    if (!m_persistSettings && !m_lane.isNull()) {
        // A secondary display may only enter the siphon's extra-displays
        // array after this result is known. Keep a lane-owned success flag
        // so destruction is safe if the GUI callback never runs.
        m_createSucceeded = std::make_shared<std::atomic<bool>>(false);
        const auto createdFlag = m_createSucceeded;
        m_lane->post([dispId = m_dispId, createdFlag]() {
            int created = 0;
            char path[1] = {0};
            XCreateAnalyzer(dispId, &created, 262144, 1, 1, path);
            createdFlag->store(created == 0);
        });
        m_lane->request<bool>([createdFlag]() { return createdFlag->load(); },
                              this, [this](bool ready) {
            m_analyzerCreated = ready;
            if (ready) {
                m_deferSetAnalyzer = true;
                applyDetectorMode(0, m_panDetector);
                applyDetectorMode(1, m_wfDetector);
                applyAverageMode(0, m_panAveraging);
                applyAverageMode(1, m_wfAveraging);
                applyAvTau(0, m_panAvTimeMs);
                applyAvTau(1, m_wfAvTimeMs);
                applyNormalizePan();
                if (m_pollTimer.isActive()) {
                    m_deferSetAnalyzer = false;
                    applySetAnalyzer();
                }
            } else {
                qCWarning(lcDsp) << "TxAnalyzer: XCreateAnalyzer failed for disp" << m_dispId;
            }
            emit analyzerCreated(ready);
        });
        return;
    }
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
    if (!m_lane.isNull()) {
        // R-R3-39: created on the transmit lane, ahead of every call below,
        // and taken as created here (the answer is not waited for); a
        // failure is logged on the lane.
        runWdsp([dispId = m_dispId]() {
            int created = 0;
            char path[1] = {0};
            XCreateAnalyzer(dispId, &created, /*m_size=*/262144, /*m_LO=*/1,
                            /*m_stitch=*/1, path);
            if (created != 0) {
                qCWarning(lcDsp) << "TxAnalyzer: XCreateAnalyzer failed for disp"
                                 << dispId << "success=" << created;
            }
        });
    } else {
        char emptyPath[1] = {0};
        XCreateAnalyzer(m_dispId,
                        &success,
                        /*m_size=*/262144,
                        /*m_LO=*/1,
                        /*m_stitch=*/1,
                        emptyPath);
    }
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
    if (m_createSucceeded && !m_lane.isNull()) {
        const auto createdFlag = m_createSucceeded;
        runWdsp([dispId = m_dispId, createdFlag]() {
            if (createdFlag->load()) { DestroyAnalyzer(dispId); }
        });
    } else if (m_analyzerCreated) {
        runWdsp([dispId = m_dispId]() { DestroyAnalyzer(dispId); });
        m_analyzerCreated = false;
    }
#endif
}

void TxAnalyzer::runWdsp(std::function<void()> job) const
{
    DspControlThread* lane = m_lane.data();
    if (lane == nullptr || lane->isCurrentThread()) {
        job();
        return;
    }
    lane->post(std::move(job));
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
        runWdsp([dispId = m_dispId, rateHz]() {
            SetDisplaySampleRate(dispId, static_cast<int>(rateHz));
        });
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

void TxAnalyzer::applyStationRates()
{
    // TX dsp_rate = 96 kHz per WdspEngine::kTxDspSampleRate (= cmaster.c:182
    // [v2.10.3.13] hardcoded 96000). The siphon at TXA.c:586 delivers
    // dsp_size = 4096 complex samples per fexchange0 cycle at this rate.
    setSampleRate(96000.0);
    setOutputFps(15);  // Thetis frame_rate default per specHPSDR.cs:335 [v2.10.3.13+501e3f51]
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
    if (DspControlThread* lane = m_lane.data()) {
        // R-R3-39: GetPixels runs on the transmit lane; the planes come back
        // here. One poll at a time: a tick while one is still queued skips.
        if (m_pollInFlight->exchange(true)) {
            return;
        }
        struct Planes {
            QVector<float> pan;
            QVector<float> wf;
            bool panReady{false};
            bool wfReady{false};
        };
        lane->request<Planes>(
            [dispId = m_dispId, numPixels = m_numPixels, inFlight = m_pollInFlight]() {
                Planes planes;
                planes.pan.resize(numPixels);
                planes.wf.resize(numPixels);
                int flagPanOnLane = 0;
                GetPixels(dispId, /*pixout=*/0, planes.pan.data(), &flagPanOnLane);
                int flagWfOnLane = 0;
                GetPixels(dispId, /*pixout=*/1, planes.wf.data(), &flagWfOnLane);
                planes.panReady = (flagPanOnLane != 0);
                planes.wfReady = (flagWfOnLane != 0);
                inFlight->store(false);
                return planes;
            },
            this,
            [this](Planes planes) {
                if (planes.panReady) {
                    m_pixBuf = planes.pan;
                    emit txFftReady(/*receiverId=*/-1, m_pixBuf);
                }
                if (planes.wfReady) {
                    m_pixBufWf = planes.wf;
                    emit txWaterfallReady(/*receiverId=*/-1, m_pixBufWf);
                }
            });
        return;
    }
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
TxDisplayView TxAnalyzer::clampViewToBaseband(double carrierHz, double centreHz,
                                               double spanHz, int pixels)
{
    // Task 28: moved unchanged from MainWindow's syncTxAnalyzerToView (the
    // PR #317 rule), so a local pan and a remote one get the same view.
    TxDisplayView view;
    view.carrierHz = carrierHz;
    view.pixels = pixels;
    if (!std::isfinite(carrierHz) || !std::isfinite(centreHz) || !std::isfinite(spanHz)
        || spanHz <= 0.0) {
        return view;
    }

    // Nothing exists outside the siphon's baseband.
    constexpr double kHalfBaseband = 48000.0;

    // Clamp the VIEW, not only the analyzer's span: the transmit display
    // cannot show more than 96 kHz, so a view past the baseband is pulled
    // back inside it rather than stretched (Codex, PR #317).
    const double viewBw = std::min(spanHz, 2.0 * kHalfBaseband);
    double lo = centreHz - viewBw / 2.0 - carrierHz;
    double hi = lo + viewBw;
    if (lo < -kHalfBaseband) { lo = -kHalfBaseband; hi = lo + viewBw; }
    if (hi >  kHalfBaseband) { hi =  kHalfBaseband; lo = hi - viewBw; }

    // lo / hi are RELATIVE TO THE CARRIER, because that is where the
    // siphon's baseband sits; asymmetric on purpose, so a panned view keeps
    // the trace under the cursor (bench 2026-08-05).
    if (hi - lo < 1000.0) {
        return view; // empty: the caller keeps its last good view
    }

    // Quantised to 100 Hz: SetAnalyzer reconfigures the analyzer, and a
    // sub-bin change nobody can see is not worth a reconfiguration.
    view.lowHz = static_cast<int>(std::round(lo / 100.0)) * 100;
    view.highHz = static_cast<int>(std::round(hi / 100.0)) * 100;
    return view;
}

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

void TxAnalyzer::setView(int lowHz, int highHz, int pixels)
{
    const bool windowMoved = m_spanLowHz != lowHz || m_spanHighHz != highHz;
    const bool pixelsMoved = pixels > 0 && pixels != m_numPixels;
    if (!windowMoved && !pixelsMoved) {
        return;
    }
    m_spanLowHz  = lowHz;
    m_spanHighHz = highHz;
    if (pixelsMoved) {
        m_numPixels = pixels;
        m_pixBuf.resize(m_numPixels);
        m_pixBufWf.resize(m_numPixels);
    }
    if (m_analyzerCreated) {
        applySetAnalyzer();
    }
}

TxAnalyzerArgs TxAnalyzer::currentArgs() const
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
    TxAnalyzerArgs args;
    args.nPixout = m_nPixout;
    args.nFft = 1;
    args.typ = 1;
    args.sz = m_fftSize;
    // bf_sz is the SIPHON's push size, not the FFT size. See setBlockSize.
    // Falls back to m_fftSize only when nothing has told us the real block
    // size yet.
    args.bfSz = (m_blockSize > 0 ? m_blockSize : m_fftSize);
    args.winType = m_windowType;
    args.pi = 14.0;   // Thetis default (unused for non-Kaiser)
    args.ovrlp = ovrlp;
    args.clp = effectiveClip;   // 0 while span-clipped; see above
    // fscLin / fscHin are BIN COUNTS to clip from the low and high ends,
    // not frequencies. Thetis computes them in CalcSpectrum
    // (specHPSDR.cs:772-774 [v2.10.3.15]) and passes them in these two
    // slots. Leaving them at zero, as this did before, is what made the
    // transmit trace land at the wrong dial frequency: the analyzer
    // emitted the whole baseband while the pan kept its RX window, and
    // SpectrumWidget stretched one across the other.
    args.fscLin = static_cast<double>(fsclipL);
    args.fscHin = static_cast<double>(fsclipH);
    args.nPix = m_numPixels;
    args.nStch = 1;
    args.calset = 0;
    args.fmin = 0.0;
    args.fmax = 0.0;
    args.maxW = max_w;
    args.sampleRateHz = m_sampleRate;
    return args;
}

void TxAnalyzer::applySetAnalyzer()
{
    // Held off until the first start(); see the constructor.
    if (m_deferSetAnalyzer) {
        return;
    }

    const TxAnalyzerArgs args = currentArgs();
    ++m_setAnalyzerCount;

    // R-R3-39: on the transmit lane with the values as they stand now.
    runWdsp([dispId = m_dispId, args]() {
        int flpOnLane[1] = {0};
        SetAnalyzer(dispId, args.nPixout, args.nFft, args.typ, flpOnLane, args.sz,
                    args.bfSz, args.winType, args.pi, args.ovrlp, args.clp,
                    args.fscLin, args.fscHin, args.nPix, args.nStch, args.calset,
                    args.fmin, args.fmax, args.maxW);

        SetDisplaySampleRate(dispId, static_cast<int>(args.sampleRateHz));
    });
    ++m_analyzerConfigCount;

    // Every parameter WDSP is actually given, on each reconfiguration.
    // Kept because it is what made the 2026-08-05 bench tractable: bf_sz
    // silently carrying the FFT size, and the symmetric clip overrunning the
    // span, are both invisible from the outside and obvious here. Fires only
    // when the analyzer is reconfigured, not per frame.
    qCDebug(lcDsp).nospace()
        << "TxAnalyzer SetAnalyzer: disp=" << m_dispId
        << " fft=" << args.sz
        << " bf_sz=" << args.bfSz
        << " (blockSize=" << m_blockSize << ")"
        << " win=" << args.winType
        << " ovrlp=" << args.ovrlp
        << " clp=" << args.clp
        << " fsclipL=" << args.fscLin << " fsclipH=" << args.fscHin
        << " n_pix=" << args.nPix
        << " max_w=" << args.maxW
        << " rate=" << args.sampleRateHz
        << " window=[" << m_spanLowHz << "," << m_spanHighHz << "]";
}

void TxAnalyzer::applyDetectorMode(int pixout, int mode)
{
    runWdsp([dispId = m_dispId, pixout, mode]() {
        SetDisplayDetectorMode(dispId, pixout, mode);
    });
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
    runWdsp([dispId = m_dispId, pixout, mode]() {
        SetDisplayAverageMode(dispId, pixout, mode);
    });
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
    runWdsp([dispId = m_dispId, pixout, avb, displayAverage]() {
        SetDisplayAvBackmult(dispId, pixout, avb);
        SetDisplayNumAverage(dispId, pixout, displayAverage);
    });
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
    runWdsp([dispId = m_dispId, gated]() {
        SetDisplayNormOneHz(dispId, /*pixout=*/0, gated ? 1 : 0);
    });
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
    if (!m_persistSettings || m_reloadingSetting) {
        return;   // reloadSetting writes back its own key only
    }
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

// ── Remote-window parity Task 30 (R-R3-49, R-R3-21, A12) ─────────────────

bool TxAnalyzer::isSettingsKey(const QString& key)
{
    for (const char* name : {kFftSizeKey, kWindowTypeKey, kPanDetectorKey, kPanAveragingKey,
                             kPanAvTimeMsKey, kPanNormalizeKey, kWfDetectorKey,
                             kWfAveragingKey, kWfAvTimeMsKey}) {
        if (key == QLatin1String(name)) {
            return true;
        }
    }
    return false;
}

int TxAnalyzer::fftSizeSliderPositionFor(int fftSize) noexcept
{
    // tbTXDisplayFFTSize: Maximum = 6 (setup.designer.cs:36638-36642
    // [v2.10.3.13+501e3f51]); the page's own derivation, unchanged.
    for (int p = 0; p <= 6; ++p) {
        if ((4096 << p) >= fftSize) {
            return p;
        }
    }
    return 6;
}

void TxAnalyzer::reloadSetting(const QString& key)
{
    if (!m_persistSettings || !isSettingsKey(key)) {
        return;
    }
    auto& s = AppSettings::instance();
    const bool present = s.contains(key);
    const QString stored = present ? s.value(key).toString() : QString();
    // A number the setter can take, the key's default when the key is
    // unset, or the analyzer's current value when the text is not a number.
    const auto number = [&](int fallbackDefault, int current) -> int {
        if (!present) {
            return fallbackDefault;
        }
        bool ok = false;
        const int v = stored.toInt(&ok);
        return ok ? v : current;
    };
    QString applied;
    m_reloadingSetting = true;
    // Each handler sets the analyzer at once, with no MOX check, as Thetis's
    // TX Display handlers do:
    // From Thetis setup.cs:18146-18210 [v2.10.3.15]
    //   comboTXDispPanDetector_SelectedIndexChanged:
    //     console.specRX.GetSpecRX(cmaster.inid(1, 0)).DetTypePan = comboTXDispPanDetector.SelectedIndex;
    //     //[2.10.3.5]MW0LGE note: see updateNormalizePan() in specHPSDR as it only applies to pan detector type 2,3,4
    //   comboTXDispPanAveraging_SelectedIndexChanged: ...AverageMode = SelectedIndex;
    //   udTXDisplayAVGTime_ValueChanged: ...AvTau = 0.001 * (double)udTXDisplayAVGTime.Value;
    //   chkDispTXNormalize_CheckedChanged: ...NormOneHzPan = chkDispTXNormalize.Checked;
    //   tbTXDisplayFFTSize_Scroll: ...FFTSize = (int)(4096 * Math.Pow(2, Math.Floor(...)));
    //   comboTXDispWinType_SelectedIndexChanged: ...WindowType = SelectedIndex;
    //   comboTXDispWFDetector_SelectedIndexChanged: ...DetTypeWF = SelectedIndex;
    //   comboTXDispWFAveraging_SelectedIndexChanged: ...AverageModeWF = SelectedIndex;
    //   udTXDisplayAVTime_ValueChanged: ...AvTauWF = 0.001 * (double)udTXDisplayAVTime.Value;
    // each followed by console.UpdateTXSpectrumDisplayVars().
    if (key == QLatin1String(kFftSizeKey)) {
        // The page's slider is the only writer of this key: a size that is
        // not one of its positions takes the position the page would show.
        const int size = number(kDefaultFftSize, m_fftSize);
        setFftSizeSliderPosition(fftSizeSliderPositionFor(size));
        applied = QString::number(m_fftSize);
    } else if (key == QLatin1String(kWindowTypeKey)) {
        setWindowType(number(kDefaultWindowType, m_windowType));
        applied = QString::number(m_windowType);
    } else if (key == QLatin1String(kPanDetectorKey)) {
        setPanDetector(number(kDefaultPanDetector, m_panDetector));
        applied = QString::number(m_panDetector);
    } else if (key == QLatin1String(kPanAveragingKey)) {
        setPanAveraging(number(kDefaultPanAveraging, m_panAveraging));
        applied = QString::number(m_panAveraging);
    } else if (key == QLatin1String(kPanAvTimeMsKey)) {
        setPanAvTimeMs(number(kDefaultPanAvTimeMs, m_panAvTimeMs));
        applied = QString::number(m_panAvTimeMs);
    } else if (key == QLatin1String(kPanNormalizeKey)) {
        // As loadSettings reads it: "True" (any case) is on.
        const bool on = present
            ? stored.compare(QStringLiteral("True"), Qt::CaseInsensitive) == 0
            : kDefaultPanNormalize;
        setPanNormalize(on);
        applied = m_panNormalize ? QStringLiteral("True") : QStringLiteral("False");
    } else if (key == QLatin1String(kWfDetectorKey)) {
        setWfDetector(number(kDefaultWfDetector, m_wfDetector));
        applied = QString::number(m_wfDetector);
    } else if (key == QLatin1String(kWfAveragingKey)) {
        setWfAveraging(number(kDefaultWfAveraging, m_wfAveraging));
        applied = QString::number(m_wfAveraging);
    } else {
        setWfAvTimeMs(number(kDefaultWfAvTimeMs, m_wfAvTimeMs));
        applied = QString::number(m_wfAvTimeMs);
    }
    m_reloadingSetting = false;
    // An unset key stays unset (its default is what the analyzer holds). A
    // stored value the setter changed goes back as the value applied, so
    // every window shows what the analyzer runs.
    if (present && stored != applied) {
        s.setValue(key, applied);
    }
    emit settingReloaded(key);
}

} // namespace NereusSDR
