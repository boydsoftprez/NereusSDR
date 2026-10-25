#pragma once

// =================================================================
// src/core/FFTEngine.h  (NereusSDR)
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
//   2026-09-23: setTransformsFollowFrameRate, NereusSDR-original: while
//               nereusd's Core is busy a transform advances a whole frame
//               period, so a lower frame rate saves FFT work (R-R3-08,
//               R-R3-40). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-27: the decimation cite corrected: Thetis's range is 1 to 16;
//               1 to 32 is NereusSDR's own (R-IOS-06). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-10-11: Follow-up wave F6, NereusSDR-original: each frame carries
//               the generation of the input purge it was computed after
//               (purgeInputForGeneration, fftReadyLinear's last argument).
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-21: NereusSDR-original: frameInputPosition(), the I/Q pair
//               count a frame's samples end at, for the Core's sample-time
//               stamps; minimumGapSkips(), the 5 ms rule's skips counted
//               and logged instead of silent. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-22: requestWindowReset() and windowResets(), ported from
//               Longpath src/core/FFTEngine.h:196-211 [@383f058]
//               (Longpath PR 62, caddf95b): a lost I/Q packet discards
//               the window in progress instead of painting a line across
//               the spectrum. Longpath's banner on this file is
//               NereusSDR's own header renamed, with no Longpath author
//               entry, so it is not copied; the project-level LICENSE
//               applies. Ported by J.J. Boyd (KG4VCF), with AI-assisted
//               transformation via Claude Code.
//   2026-10-23: NereusSDR-original bound on the stall those resets can
//               cause: a reset is skipped once the engine has produced no
//               frame for kGapResetStallFrameIntervals frame intervals
//               (windowResetsSkipped(), applyPendingWindowReset()). JJ's
//               ruling (option a) after the frozen waterfall on lossy
//               Wi-Fi and HL2 links. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Claude Code.
//   2026-10-24: The bound holds once a gap has been let through (JJ's
//               ruling, option i): later gaps are let through too, until a
//               whole window of samples passes with no gap. A lossy link
//               keeps its full frame rate with a slight smear; a single gap
//               on a healthy link still drops the window. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Claude Code.
//   2026-10-24: JJ's ruling (option c) replaces that one-window rule: a
//               gap less than kGapLossyLinkWindows windows after the last
//               gap of any kind is let through, once a frame has come since
//               the last reset. Gaps one to four windows apart keep the
//               full frame rate too. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Claude Code.
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

#include "core/NereusCoreExport.h"
#include <QObject>
#include <QVector>
#include <QElapsedTimer>

#include <atomic>
#include <limits>

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

namespace NereusSDR {

// Window function types for FFT pre-processing.
// Integer values match Thetis WDSP analyzer.c:52-173 [v2.10.3.13]
// new_window() switch type codes exactly, so the values are wire-compatible
// with Thetis's combo and any future WDSP API surface.  Enum order also
// matches Thetis comboDispWinType item ordering (setup.designer.cs:34966-
// 34973 [v2.10.3.13]) so combo-index == enum-value with no remap.
enum class WindowFunction : int {
    Rectangular        = 0,  // analyzer.c case 0
    BlackmanHarris4    = 1,  // 4-term Blackman-Harris, analyzer.c case 1
    Hann               = 2,  // analyzer.c case 2
    FlatTop            = 3,  // flat-top for calibration, analyzer.c case 3
    Hamming            = 4,  // analyzer.c case 4
    Kaiser             = 5,  // parameterised by KaiserPi, analyzer.c case 5
    BlackmanHarris7    = 6,  // 7-term Blackman-Harris, analyzer.c case 6
    Count              = 7
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
class NEREUS_CORE_EXPORT FFTEngine : public QObject {
    Q_OBJECT

public:
    explicit FFTEngine(int receiverId, QObject* parent = nullptr);
    ~FFTEngine() override;

    /// The engine owns the supported FFT bound; callers validate requests
    /// through this accessor instead of duplicating a DSP constant.
    static constexpr int maximumFftSize() { return kMaxFftSize; }

    // --- Configuration (thread-safe, main thread sets these) ---

    void setFftSize(int size);
    int  fftSize() const { return m_fftSize.load(); }

    void setWindowFunction(WindowFunction wf);
    WindowFunction windowFunction() const {
        return static_cast<WindowFunction>(m_windowFunc.load());
    }

    // Kaiser window shape parameter (PiAlpha = pi * alpha).  Higher values
    // narrow the main lobe + raise sidelobes; lower values widen.  Only
    // applied when windowFunction() == Kaiser.
    // From Thetis specHPSDR.cs:145 [v2.10.3.13] private double kaiser_pi = 14.0;
    void setKaiserPi(double pi);
    double kaiserPi() const { return m_kaiserPi.load(); }

    // FFT size display calibration offset, in dB.  Higher FFT sizes raise
    // the noise floor on the spectrum render (because each bin sees a
    // smaller slice of the input power).  Thetis compensates by setting
    // this offset to slider.Value * 2 dB on every FFT slider scroll
    // (setup.cs:16154 [v2.10.3.13]) and subtracting it from agc_cal_offset
    // (console.cs:33304).  Without the compensation, the AGC threshold
    // drifts up to 12 dB across the slider's 0..6 range.
    // From Thetis Display.cs:1389-1397 [v2.10.3.13] RX1FFTSizeOffset.
    //
    // NereusSDR, 2026-10-11: Auto AGC-T no longer reads this offset. It
    // reads a noise floor referred to a 4096-point FFT instead
    // (NoiseFloorTracker::agcNoiseFloor; RadioModel::updateAutoAgc says
    // why). The offset is still stored on every size change and nothing
    // reads it at present.
    void setFftSizeOffsetDb(double db);
    double fftSizeOffsetDb() const { return m_fftSizeOffsetDb.load(); }

    // The offset an FFT size baseline carries: its slider position times
    // 2 dB, where the position is log2(size / 4096). setFftSizeBaseline()
    // stores it, so every FFT size change sets it, wherever it came from.
    // From Thetis setup.cs:16148 [v2.10.3.13] (position 0 is 4096)
    static constexpr int kFftSizeOffsetZeroSize = 4096;
    // From Thetis setup.cs:16154 [v2.10.3.13]
    //   Display.RX1FFTSizeOffset = tbDisplayFFTSize.Value * 2;
    static constexpr double kFftSizeOffsetDbPerPosition = 2.0;
    static double fftSizeOffsetDbForSize(int size);

    // FFT-size baseline -- the user's slider-set "FFT size at full DDC
    // bandwidth (bwHz == sampleRate)" choice.  NereusSDR-original auto-zoom
    // (no Thetis equivalent) uses this as the target K = baseline /
    // displayWidth bins-per-pixel; on zoom, computes the FFT size needed
    // to maintain the same K and replans accordingly.  Slider handler
    // sets this, auto-zoom reads it as the floor (fftSize never goes
    // below baseline).  See MainWindow auto-zoom lambda for the
    // baseline * sampleRate / bwHz formula.
    void setFftSizeBaseline(int size);
    int  fftSizeBaseline() const { return m_fftSizeBaseline.load(); }

    // The FFT sizing mode changed (design D43; R-DSP-67) and left this
    // engine's baseline as it was (Auto to Hz/bin and back: both 4096).
    // Stores the offset the baseline carries and says fftSizeBaselineChanged
    // once, as a changed baseline does, so the noise-floor trackers this
    // engine feeds restart their fast attack. FftEnginePool::setSizing calls
    // one or the other for a mode change, never both.
    void announceFftSizingChanged();

    // Hz-per-bin auto-zoom override.  When > 0, the auto-zoom replan
    // formula (in MainWindow) targets this Hz/bin instead of the default
    // bins-in-window = baseline policy:
    //   targetSize = sampleRate / hzPerBinTarget
    // The FFT size becomes effectively zoom-INDEPENDENT — same FFT delivers
    // the requested resolution regardless of visible bandwidth.  Set to 0
    // to disable (fall back to bins-in-window default).  Useful for
    // hunting narrow signals (CW, digital modes) where you want the same
    // frequency resolution at any zoom level.  Floor at baseline still
    // applies, so the slider remains a minimum-FFT-size knob.
    void  setHzPerBinTarget(double hzPerBin);
    double hzPerBinTarget() const { return m_hzPerBinTarget.load(); }

    void setSampleRate(double rateHz);
    double sampleRate() const { return m_sampleRate.load(); }

    void setOutputFps(int fps);
    int  outputFps() const { return m_targetFps.load(); }

    // NereusSDR-original (R-R3-08, R-R3-40), no Thetis equivalent. Off by
    // default, and never turned on by the desktop app: only nereusd, while
    // its display budget is lowered because the Core computer is busy.
    // Off: each transform advances min(sampleRate / fps, fftSize) samples,
    // so a small FFT runs more transforms than frames are sent. On: each
    // transform advances sampleRate / fps samples, skipping the samples
    // between one transform's window and the next, so transforms per
    // second equal the frame rate and a lower rate saves FFT work.
    void setTransformsFollowFrameRate(bool on);
    bool transformsFollowFrameRate() const { return m_transformsFollowFrameRate.load(); }

    // setDecimation — apply I/Q decimation before FFT processing.
    // Only every Nth I/Q sample pair is passed to the FFT accumulator.
    // Higher decimation reduces effective bandwidth and FFT resolution,
    // mirroring Thetis setup.designer.cs:33732 udDisplayDecimation [v2.10.3.13].
    void setDecimation(int factor);
    int  decimation() const { return m_decimation.load(); }

    // Equivalent Noise Bandwidth of the current window function, expressed
    // as a multiplier on the natural per-bin bandwidth (sample_rate / N).
    //
    //   ENB_bins = N × Σ(w[i]²) / (Σ w[i])²
    //
    // For typical windows: rectangular = 1.00, Hann = 1.50, Hamming ≈ 1.36,
    // Blackman-Harris 4-term ≈ 2.00, Flat-Top ≈ 3.77.  Used by the Thetis
    // detector function (analyzer.c:283-462 [v2.10.3.13]) to scale Average
    // / Sample / RMS modes via inv_enb so that integrated noise reads
    // independent of window choice.  Computed inside computeWindow().
    double windowEnb() const { return m_windowEnb; }

public slots:
    // Feed raw interleaved I/Q samples from RadioConnection.
    // Format: [I0, Q0, I1, Q1, ...] as float pairs.
    // Accumulates until fftSize samples are collected, then runs FFT.
    void feedIQ(const QVector<float>& interleavedIQ);

    // From Longpath src/core/FFTEngine.h:196-211 [@383f058] (PR 62).
    /// Das angefangene Fenster verwerfen — nach einem Loch im Datenstrom.
    ///
    /// Der Panadapter schiebt seine Abtastwerte in ein gleitendes Fenster
    /// und rechnet erst, wenn es voll ist. Fehlen mittendrin Pakete (WLAN,
    /// ausgelastete Gegenstelle), steht im Fenster ein Sprung — und ein
    /// Sprung ist breitbandig: er malt einen Schmierer ueber das ganze
    /// Bild, der aussieht wie ein Signal. Diese Methode wirft das
    /// angefangene Fenster weg, sodass das naechste Bild nur aus Werten
    /// nach dem Loch entsteht.
    ///
    /// Darf von JEDEM Faden gerufen werden: sie setzt nur eine Fahne, die
    /// feedIQ() auf seinem eigenen Faden abholt.
    ///
    /// NereusSDR: in English, a lost I/Q packet leaves a jump in the
    /// window, and a jump is broadband, so the next frame shows a line
    /// across the whole width. The window in progress (and the overlap
    /// tail) is dropped at the next feedIQ(). The callers deliver it on
    /// this engine's thread, queued in order with the samples, so the
    /// flag is always taken by the first batch after the gap.
    ///
    /// NereusSDR-original (2026-10-23, option c 2026-10-24): the reset is
    /// bounded. On a healthy link a single gap still drops the window, as
    /// above. A gap is let through instead (the window keeps its samples
    /// and windowResetsSkipped() counts it) when either holds:
    ///   - the link is lossy: the last gap, let through or reset, came less
    ///     than kGapLossyLinkWindows windows of samples (the FFT size, in
    ///     input pairs) before, and a frame has come since the last reset;
    ///   - the engine has produced no frame for
    ///     kGapResetStallFrameIntervals frame intervals (while the window
    ///     refills after a reset, this alone decides).
    /// So a link that loses packets up to kGapLossyLinkWindows windows
    /// apart keeps its normal frame rate with a slight smear instead of a
    /// frozen or crawling display, and a gap after that many clean windows
    /// drops the window again.
    void requestWindowReset() { m_windowResetPending.store(true); }

    /// Wie oft ein Fenster deshalb verworfen wurde (Diagnose).
    quint64 windowResets() const { return m_windowResets.load(); }

    /// Drops every accumulated and overlap sample on this engine's worker
    /// thread. Call at an RF-source context boundary before accepting input
    /// for the new context; configuration setters alone do not clear the
    /// overlap ring when their values happen to be unchanged.
    void resetInputHistory();

    /// resetInputHistory(), and every frame from now on carries
    /// `generation` (fftReadyLinear's last argument). Runs on this engine's
    /// worker thread, so a frame computed before the purge keeps the
    /// generation it was computed under however late it is delivered.
    /// NereusSDR-original.
    void purgeInputForGeneration(quint64 generation);

public:
    /// NereusSDR-original (2026-10-23): requested resets let through
    /// without dropping the window: the engine had produced no frame for
    /// kGapResetStallFrameIntervals frame intervals, or (2026-10-24) the
    /// link was lossy (requestWindowReset() says when).
    quint64 windowResetsSkipped() const { return m_windowResetsSkipped.load(); }

    /// NereusSDR-original (2026-10-23). This engine's worker thread only.
    /// Takes a pending requestWindowReset() now rather than at the next
    /// feedIQ(), which takes it the same way. Returns true when the window
    /// was dropped, false when no reset was pending or the bound let the
    /// gap through (requestWindowReset() says when). The daemon's spectrum
    /// source uses the answer to restart its sample clock only when the
    /// samples really restart.
    bool applyPendingWindowReset();

    /// NereusSDR-original (2026-10-23): the gap-reset stall bound, in frame
    /// intervals: with no frame for this long, the next gap is the first
    /// one let through (requestWindowReset()). NereusSDR's own value:
    /// neither Longpath (which resets on every throttled gap) nor AetherSDR
    /// (which resets on every gap) bounds the stall. A frame interval is
    /// what paces this engine's output: the window advance processFrame()
    /// takes, sampleRate / outputFps samples, no more than the FFT size
    /// unless transforms follow the frame rate.
    static constexpr int kGapResetStallFrameIntervals = 3;

    /// NereusSDR-original (2026-10-24, JJ's ruling, option c): how recent
    /// a gap makes a link lossy, in windows of samples (the FFT size, in
    /// input pairs), counted from the last gap, let through or reset. A
    /// gap within this many windows of the one before is let through once
    /// a frame has come since the last reset (requestWindowReset()). Why
    /// 4: gaps up to 4 windows apart all smear, so the waterfall keeps its
    /// full rate where a reset each time would cost a window of frames per
    /// gap, and a healthy link that drops one packet now and then, more
    /// than 4 windows apart, still drops the window. NereusSDR's own value:
    /// neither Longpath nor AetherSDR has one.
    static constexpr int kGapLossyLinkWindows = 4;

    /// The generation frames are stamped with now. 0 until the first
    /// purgeInputForGeneration() or setInputGeneration().
    quint64 inputGeneration() const { return m_inputGeneration.load(); }
    /// The stamp alone, with no purge: for an engine made after a purge,
    /// which holds no input from before it.
    void setInputGeneration(quint64 generation) { m_inputGeneration.store(generation); }

    /// NereusSDR-original. How many I/Q pairs feedIQ had taken, before
    /// decimation, when the frame being emitted was computed: the position
    /// its samples end at, give or take one decimation step. It counts every
    /// pair fed since construction, including pairs decimation passes over
    /// and pairs the 5 ms rule drops, so the difference between two frames'
    /// positions over the sample rate is the time between their samples.
    /// Read it on this engine's thread, from a slot connected directly to
    /// fftReady or fftReadyLinear.
    qint64 frameInputPosition() const { return m_frameInputPosition; }

    /// NereusSDR-original. Times processFrame's 5 ms minimum gap held a
    /// full buffer back and feedIQ dropped the rest of its batch, and the
    /// I/Q pairs so dropped. Any thread.
    quint64 minimumGapSkips() const { return m_minimumGapSkips.load(); }
    quint64 minimumGapDroppedPairs() const { return m_minimumGapDroppedPairs.load(); }

signals:
    // Emitted when a new FFT frame is ready.
    // binsDbm contains fftSize float values (full FFT-shifted bins, neg-freq
    // first then pos-freq), representing power in dBm at each frequency bin.
    void fftReady(int receiverId, const QVector<float>& binsDbm);

    // Linear-power side-channel for the Thetis-faithful display pipeline.
    //
    // binsLinear contains the same length and ordering as binsDbm but values
    // are |X[k]|² (linear power, no log conversion, no dBm normalization).
    // Required by the Thetis WDSP analyzer detector + averaging pipeline
    // which operates on linear-domain bins and applies the 10×log₁₀
    // conversion only at final pixel output (analyzer.c:464-554
    // [v2.10.3.13] avenger() does the log step per av_mode).
    //
    // windowEnb is the Equivalent Noise Bandwidth of the window in bins.
    // The downstream detector applies inv_enb = 1/windowEnb to scale
    // Average/Sample/RMS modes (analyzer.c:368-441 [v2.10.3.13]).
    //
    // dbmOffset is the window coherent-gain compensation -20·log10(Σw[i])
    // that fftReady's binsDbm[i] adds after 10·log10(|X[k]|²).  The avenger
    // applies it via its `scale` parameter as scale = 10^(dbmOffset/10) so
    // the post-avenger pixels read the same dBm as fftReady.
    //
    // Both metadata values are emitted in-band so the slot always has the
    // values matching the bins it just received — no separate setter
    // coordination needed.
    //
    // Emitted in lock-step with fftReady — every frame, both signals fire.
    // Cost: one extra QVector<float> heap allocation + 16 bytes per frame.
    //
    // inputGeneration is the generation of the last input purge before the
    // samples of this frame (purgeInputForGeneration); a slot that does not
    // need it leaves it out.
    void fftReadyLinear(int receiverId, const QVector<float>& binsLinear,
                        double windowEnb, double dbmOffset,
                        quint64 inputGeneration = 0);

    // Emitted whenever a setFftSize() call results in an actual size
    // change (oldSize != newSize).  Mirrors Thetis
    // SpectrumSettingsChangedHandlers?.Invoke(rx) at setup.cs:16164
    // [v2.10.3.13] -- Thetis subscribers (chrome / waterfall / AGC panel)
    // refresh their derived state.  NereusSDR has no subscribers today;
    // emitted for forward compatibility.
    void spectrumSettingsChanged(int receiverId);

    // Emitted when setFftSizeBaseline() changes the baseline (once per
    // change; a size that is already the baseline emits nothing), and by
    // announceFftSizingChanged() for a sizing mode change that left the
    // baseline alone, from the thread that called it. MainWindow restarts
    // the fast attack of the noise-floor trackers this engine feeds.
    void fftSizeBaselineChanged(int size);

private:
    void replanFft();
    void computeWindow();
    void processFrame();
    // NereusSDR-original (2026-10-21): counts one 5 ms rule skip that
    // dropped `droppedPairs` I/Q pairs, and warns at most once per interval.
    void noteMinimumGapSkip(int droppedPairs);

    int m_receiverId;

    // FFT configuration (atomics for cross-thread access)
    std::atomic<int>    m_fftSize{4096};
    std::atomic<int>    m_pendingFftSize{0};  // 0 = no change pending
    std::atomic<int>    m_windowFunc{static_cast<int>(WindowFunction::BlackmanHarris4)};
    // Kaiser window shape parameter.  Default 14.0 from Thetis specHPSDR.cs:
    // 145 [v2.10.3.13] private double kaiser_pi = 14.0.
    std::atomic<double> m_kaiserPi{14.0};
    // FFT size display calibration offset (dB).  Updated by the slider
    // handler to slider.Value * 2 per Thetis setup.cs:16154 [v2.10.3.13].
    std::atomic<double> m_fftSizeOffsetDb{0.0};
    // The generation frames are stamped with (purgeInputForGeneration).
    std::atomic<quint64> m_inputGeneration{0};
    // User's slider-set FFT size baseline (NereusSDR-original auto-zoom
    // anchor).  Default matches m_fftSize so first-launch behavior is
    // identical to a system without auto-zoom (slider == FFT size).
    std::atomic<int>    m_fftSizeBaseline{4096};
    // 0 = disabled (use bins-in-window auto-zoom).  > 0 = target Hz/bin.
    std::atomic<double> m_hzPerBinTarget{0.0};
    std::atomic<double> m_sampleRate{48000.0};
    std::atomic<int>    m_targetFps{30};
    std::atomic<bool>   m_transformsFollowFrameRate{false};
    // Thetis's udDisplayDecimation (setup.designer.cs:33732 [v2.10.3.13])
    // spans 1..16 (setup.designer.cs:33834 [v2.10.3.15]); NereusSDR now uses
    // that shared range too. 1 = no decimation (pass every sample).
    std::atomic<int>    m_decimation{1};

    // Internal state (only accessed on worker thread)
    int m_currentFftSize{0};  // last planned size (triggers replan on mismatch)
    int m_decimationCounter{0};  // counts input sample pairs; reset each decimation pass
    // Input sample pairs still to pass over before the next transform's
    // window starts (setTransformsFollowFrameRate only).
    int m_skipPending{0};

#ifdef HAVE_FFTW3
    // Raw (un-windowed) I/Q ring buffer.  feedIQ writes here.  processFrame
    // copies into m_fftIn applying the window function.  Keeping the raw
    // samples in a separate buffer allows overlap-style frame advance:
    // after each FFT we copy the trailing (fftSize - advance) samples
    // back to the head so successive FFT frames share data.  Mirrors
    // WDSP analyzer.c overlap behaviour driven by Thetis specHPSDR.cs:155
    // [v2.10.3.13] private int overlap = 30000.
    fftwf_complex* m_iqRaw{nullptr};
    // Windowed FFT input + output buffers.  m_fftIn is overwritten each
    // frame (window applied to m_iqRaw on the way in).
    fftwf_complex* m_fftIn{nullptr};
    fftwf_complex* m_fftOut{nullptr};
    fftwf_plan     m_plan{nullptr};
#endif

    // Window-recompute flag.  Set by setWindowFunction / setKaiserPi
    // (main thread); checked + cleared at the top of processFrame
    // (FFT worker thread).  Without this, window changes via the combo
    // are silently ignored until the FFT size also changes -- the only
    // other path that re-runs computeWindow().
    std::atomic<bool> m_windowDirty{true};

    // Window function coefficients (precomputed for current fftSize)
    QVector<float> m_window;

    // I/Q accumulation buffer (interleaved pairs)
    QVector<float> m_iqBuffer;
    int m_iqWritePos{0};  // write position in sample pairs
    // From Longpath src/core/FFTEngine.h:343-344 [@383f058] (PR 62).
    std::atomic<bool>    m_windowResetPending{false};  // von requestWindowReset()
    std::atomic<quint64> m_windowResets{0};            // nur Diagnose
    // NereusSDR-original (2026-10-23): the stall bound. Resets the bound
    // let through, and the decimated input pairs (written to the window or
    // skipped between windows) since the last frame (worker thread only).
    std::atomic<quint64> m_windowResetsSkipped{0};
    qint64 m_pairsSinceFrame{0};
    // NereusSDR-original (2026-10-24), JJ's ruling (option c): decimated
    // input pairs since the last gap, let through or reset (counted like
    // m_pairsSinceFrame), and whether the window is refilling after a
    // reset, with no frame since (worker thread only). Before the first
    // gap the count stands further back than any kGapLossyLinkWindows
    // windows, and adding a pair at a time cannot overflow it.
    qint64 m_pairsSinceGap{std::numeric_limits<qint64>::max() / 2};
    bool m_refillingAfterReset{false};
    // The input pairs one frame interval spans (kGapResetStallFrameIntervals).
    qint64 frameIntervalPairs() const;

    // Output rate limiting
    QElapsedTimer m_frameTimer;
    bool m_frameTimerStarted{false};
    // NereusSDR-original (2026-10-21): the 5 ms rule's skips, counted, and
    // a warning at most once per kMinimumGapWarningIntervalMs for skips
    // that broke an overlapped window (worker thread only, but the counts).
    std::atomic<quint64> m_minimumGapSkips{0};
    std::atomic<quint64> m_minimumGapDroppedPairs{0};
    bool m_minimumGapHeldFrame{false};
    int m_lastOverlap{0};
    QElapsedTimer m_minimumGapWarningTimer;
    quint64 m_minimumGapSkipsWarned{0};
    quint64 m_minimumGapPairsWarned{0};
    static constexpr qint64 kMinimumGapWarningIntervalMs = 10'000;
    // NereusSDR-original (2026-10-21): I/Q pairs fed since construction,
    // and the count at the frame being emitted (frameInputPosition()).
    qint64 m_inputPairsFed{0};
    qint64 m_frameInputPosition{0};

    // dBm calibration offset (accounts for window gain + FFT normalization)
    float m_dbmOffset{0.0f};

    // Equivalent Noise Bandwidth of the current window in bins:
    //   m_windowEnb = N × Σ(w[i]²) / (Σ w[i])²
    // Recomputed every time the window changes (computeWindow()).  Used by
    // downstream consumers (e.g. SpectrumDetector) for inv_enb scaling
    // matching analyzer.c:368-441 [v2.10.3.13].
    double m_windowEnb{1.0};

    // Maximum FFT size.  Sized to match the Thetis FFT slider's high end
    // (4096 << 6 = 262144 bins).  Setting fftSize beyond this is rejected
    // by setFftSize().
    // From Thetis tbDisplayFFTSize.Maximum=6 + setup.cs:16148 [v2.10.3.13]
    // FFTSize = 4096 * Math.Pow(2, slider.Value).
    static constexpr int kMaxFftSize = 262144;
};

} // namespace NereusSDR
