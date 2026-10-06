// no-port-check: NereusSDR-original glue class.  TxAnalyzer wraps Thetis's
// WDSP analyzer + siphon infrastructure (analyzer.c + siphon.c, vendored
// in third_party/wdsp/src/) for the TX-side panadapter display.  The DSP
// itself is faithful Thetis WDSP; this class is just the Qt host that
// drives XCreateAnalyzer / SetAnalyzer / GetPixels and bridges the
// Spectrum0-fed pixel ring into a Qt signal/slot path.
//
// =================================================================
// src/core/TxAnalyzer.h  (NereusSDR)
// =================================================================
//
// TxAnalyzer — TX-side panadapter source via WDSP analyzer.
//
// Background
// ----------
// Pre-PR-#212 NereusSDR fed its single panadapter from the radio's
// RX DDC stream unconditionally — even during MOX.  That meant the
// TX waterfall showed antenna readback (PA bleed, IMD, splatter)
// rather than the intended TX signal.  Thetis instead source-switches
// on MOX edge: GetPixels(rxDispId) → GetPixels(txDispId), where the
// TX disp is fed by the WDSP `sip1` siphon at TXA.c:586 (BEFORE xiqc
// — i.e. the clean intended pre-PA signal).
//
// This class brings up the WDSP analyzer infrastructure for the TX
// channel.  It allocates one analyzer instance (kTxDispId=5), arms it
// with parameters from Thetis initAnalyzer (typ=1 complex I/Q, Hamming
// win, 4096-bin FFT, 15 fps output), and polls GetPixels on a QTimer.
// MainWindow source-switches the SpectrumWidget connection on MOX
// edge (FFTEngine for RX → TxAnalyzer for TX, reverse on un-key).
//
// Source-first cite map
// ---------------------
//   /Users/j.j.boyd/Thetis/Project Files/Source/Console/cmaster.cs:411,534-540
//     [v2.10.3.13+501e3f51] — cmRCVR=5; TXASetSipMode + TXASetSipDisplay setup
//   /Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/console.cs:24399-24462
//     [v2.10.3.13+501e3f51] — display-loop MOX-aware GetPixels source switch
//   /Users/j.j.boyd/Thetis/Project Files/Source/Console/HPSDR/specHPSDR.cs:504-643
//     [v2.10.3.13+501e3f51] — initAnalyzer / SetAnalyzer parameter derivation
//   /Users/j.j.boyd/Thetis/Project Files/Source/wdsp/TXA.c:585-590
//     [v2.10.3.13+501e3f51] — xsiphon position pre-IQC (line 586)
//   /Users/j.j.boyd/Thetis/Project Files/Source/wdsp/siphon.c:129-132
//     [v2.10.3.13+501e3f51] — mode-1 dispatch: Spectrum0(1, disp, 0, 0, in)
//
// Thread placement
// ----------------
// TxAnalyzer lives on the main thread (constructed by MainWindow,
// QTimer fires on the main thread, GetPixels uses WDSP's internal
// SetAnalyzerSection critical section for thread safety vs the audio
// thread's xsiphon → Spectrum0 push path).  No thread migration.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-07 — Created by J.J. Boyd (KG4VCF) for the PR #212
//                 follow-up TX waterfall fix (Option 1: full WDSP
//                 analyzer port).  AI-assisted source-first protocol
//                 via Anthropic Claude Code.
//   2026-09-25 : R-R3-39 (station Task 32) by J.J. Boyd (KG4VCF): with a
//                 transmit lane every analyzer call (create, configure,
//                 GetPixels, destroy) runs there; the poll hands the pixels
//                 back to this object's thread. AI-assisted implementation
//                 via Anthropic Claude Code.
//   2026-09-25 : R-R3-39 / R-IOS-03 by J.J. Boyd (KG4VCF): applyStationRates(),
//                 the rate and frame rate the desktop window and nereusd
//                 both give their TX analyzer. AI-assisted implementation
//                 via Anthropic Claude Code.
//   2026-09-26 : Task 27 (R-R3-49) by J.J. Boyd (KG4VCF): TxAnalyzerArgs and
//                 currentArgs(), the one computation of every SetAnalyzer
//                 argument, read by applySetAnalyzer and the skirt test.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26 : Task 28 (R-R3-49, A11) by J.J. Boyd (KG4VCF): TxDisplayView
//                 and clampViewToBaseband(), the rule MainWindow's
//                 syncTxAnalyzerToView applied, moved here unchanged for
//                 TxDisplayFeed; numPixels() and outputFps() readers.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26 : Tasks 27-29 fix wave (R-R3-49) by J.J. Boyd (KG4VCF):
//                 setView() sets the window and the pixel count with one
//                 SetAnalyzer; setAnalyzerCount() counts them for tests.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27 : Parity Task 30 (R-R3-49, R-R3-21, A12) by J.J. Boyd
//                 (KG4VCF): reloadSetting(), a window's write of one of the
//                 nine keys applied to the Core's analyzer at once; the key
//                 names and defaults as constants. AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

namespace NereusSDR {

class DspControlThread;

/// Every value TxAnalyzer hands WDSP's SetAnalyzer and SetDisplaySampleRate,
/// in SetAnalyzer's argument order (wdsp/analyzer.c SetAnalyzer). Filled by
/// TxAnalyzer::currentArgs(), the one computation applySetAnalyzer passes to
/// WDSP, so a test reads exactly what the analyzer is given.
struct TxAnalyzerArgs {
    int nPixout{0};
    int nFft{0};
    int typ{0};
    int sz{0};
    int bfSz{0};
    int winType{0};
    double pi{0.0};
    int ovrlp{0};
    int clp{0};
    double fscLin{0.0};
    double fscHin{0.0};
    int nPix{0};
    int nStch{0};
    int calset{0};
    double fmin{0.0};
    double fmax{0.0};
    int maxW{0};
    double sampleRateHz{0.0};
};

/// The transmit display's view (Task 28): the carrier it sits on, the
/// analyzer's window edges relative to that carrier (100 Hz steps, what
/// TxAnalyzer::setSpectrumWindow takes) and the analyzer's pixel count.
/// An empty window (lowHz == highHz) from clampViewToBaseband means "keep
/// the last good view".
struct TxDisplayView {
    double carrierHz{0.0};
    int lowHz{0};
    int highHz{0};
    int pixels{0};

    bool empty() const noexcept { return highHz <= lowHz; }
    double centreHz() const noexcept { return carrierHz + (lowHz + highHz) / 2.0; }
    double spanHz() const noexcept { return static_cast<double>(highHz - lowHz); }
    bool operator==(const TxDisplayView&) const = default;
};

class NEREUS_CORE_EXPORT TxAnalyzer : public QObject {
    Q_OBJECT

public:
    /// WDSP analyzer display ID reserved for the TX panadapter.
    /// From Thetis cmaster.cs:411 [v2.10.3.13+501e3f51] — cmRCVR = 5;
    /// cmaster.inid(1, 0) = cmRCVR + 0 = 5, which is the txinid passed to
    /// TXASetSipDisplay.  Phase 3M-4 PureSignal AmpView would use a separate
    /// disp ID.
    static constexpr int kTxDispId = 5;
    // WDSP's 72 display slots: receiver displays occupy 0..3 and the
    // primary TX siphon uses 5. Slot 6 is reserved for the one mini TX
    // analyzer attached through TXASetSipAllocDisps.
    static constexpr int kMiniTxDispId = 6;

    // R-R3-39: with `lane` (RadioModel::transmitLane), every WDSP analyzer
    // call runs there in the order it is made, and each poll's pixels come
    // back to this object's thread; without one they run here, as before.
    explicit TxAnalyzer(int dispId = kTxDispId, QObject* parent = nullptr,
                        DspControlThread* lane = nullptr,
                        bool persistSettings = true);
    ~TxAnalyzer() override;
    bool analyzerReady() const noexcept { return m_analyzerCreated; }

    /// Bins to clip from the low and high ends of the FFT so the analyzer
    /// emits only `lowHz`..`highHz` around the carrier.
    ///
    /// Verbatim port of Thetis specHPSDR.cs:762-775 [v2.10.3.15]
    /// CalcSpectrum, whose results become SetAnalyzer's fscLin / fscHin:
    ///   high_clip_bw = 0.5*rate - high;  low_clip_bw = 0.5*rate + low
    ///   fsclipH = floor(high_clip_bw / bin_width)
    ///   fsclipL = ceil (low_clip_bw  / bin_width)
    /// Note the asymmetry -- floor on one, ceil on the other -- is Thetis's
    /// and is preserved.
    ///
    /// Returned as {fsclipL, fsclipH}.
    /// The view a pan asks for (`centreHz`, `spanHz`), held inside the
    /// siphon's +/-48 kHz baseband around `carrierHz`, its edges relative to
    /// the carrier and quantised to 100 Hz. A clamped span under 1000 Hz
    /// comes back empty (lowHz == highHz == 0): the caller keeps the last
    /// good view. The rule MainWindow's syncTxAnalyzerToView applied, moved
    /// here unchanged so every viewer of the transmit display shares it.
    static TxDisplayView clampViewToBaseband(double carrierHz, double centreHz,
                                             double spanHz, int pixels);

    static std::pair<int, int> spanClipBins(int lowHz, int highHz,
                                            double sampleRateHz, int fftSize);

    /// Samples the siphon hands the analyzer per push, i.e. the TXA chain's
    /// dsp_size. Becomes SetAnalyzer's `bf_sz`.
    ///
    /// This is NOT the FFT size, and getting it wrong is not a rounding
    /// error. Spectrum0() takes no length argument (analyzer.c), so the
    /// analyzer consumes exactly `bf_sz` samples from the pointer the
    /// siphon gives it. Declaring the FFT size here told WDSP to read tens
    /// of thousands of samples out of a buffer holding a few hundred, and
    /// the resulting spectrum is dominated by whatever follows it in
    /// memory -- which is why every window, detector and averaging setting
    /// looked identical at the bench on 2026-08-05: the window was being
    /// applied to garbage.
    ///
    /// From Thetis specHPSDR.cs:624-642 [v2.10.3.15], initAnalyzer passes
    /// `fft_size` for sz and `blocksize` for bf_sz -- two different values.
    void setBlockSize(int frames);

    int blockSize() const noexcept { return m_blockSize; }

    /// The SetAnalyzer / SetDisplaySampleRate arguments for the state as it
    /// stands now. applySetAnalyzer passes exactly these to WDSP.
    TxAnalyzerArgs currentArgs() const;

    /// Restrict analyzer output to `lowHz`..`highHz` around the carrier.
    /// Pass {0, 0} to go back to the full unclipped span.
    ///
    /// This is what makes the transmit panadapter's frequency axis honest.
    /// Without it the analyzer emits the whole +/-48 kHz baseband, the pan
    /// still carries its RX window, and SpectrumWidget stretches the one
    /// across the other -- so the trace lands at the wrong dial frequency
    /// while the RF is perfectly correct. Bench 2026-08-04.
    void setSpectrumWindow(int lowHz, int highHz);

    int spectrumWindowLowHz()  const noexcept { return m_spanLowHz;  }
    int spectrumWindowHighHz() const noexcept { return m_spanHighHz; }

    /// Update output pixel count to match the panadapter's display width.
    /// Called when SpectrumWidget resizes.  Triggers a SetAnalyzer re-call
    /// with the new n_pix; safe to call from the main thread (WDSP's
    /// SetAnalyzerSection blocks briefly while the analyzer reconfigures).
    void setNumPixels(int n);
    int numPixels() const noexcept { return m_numPixels; }

    /// setSpectrumWindow and setNumPixels together, with one SetAnalyzer
    /// when either changed (TxDisplayFeed's view). `pixels` <= 0 keeps the
    /// pixel count.
    void setView(int lowHz, int highHz, int pixels);
    /// How many SetAnalyzer calls this analyzer has posted (test seam).
    int setAnalyzerCount() const noexcept { return m_setAnalyzerCount; }

    /// Update analyzer sample rate.  TX is always at the WDSP DSP rate
    /// (96 kHz — see WdspEngine::kTxDspSampleRate, matches Thetis
    /// cmaster.c:182 [v2.10.3.13]).  Called once at startup and on rate
    /// changes.
    void setSampleRate(double rateHz);

    /// Update output frame rate (frames per second).  Affects the
    /// analyzer overlap calculation per specHPSDR.cs:784 [v2.10.3.13+501e3f51].
    /// Default 15 fps per specHPSDR.cs:335 [v2.10.3.13+501e3f51].
    void setOutputFps(int fps);
    int outputFps() const noexcept { return m_outputFps; }

    /// The station's set-up of this analyzer, shared by the desktop window
    /// and nereusd so the two cannot drift: the TX DSP rate (96 kHz, see
    /// setSampleRate) and Thetis's 15 frames a second (setOutputFps).
    void applyStationRates();

    /// Begin polling GetPixels at outputFps.  Called by MainWindow on
    /// MOX-up.  No-op if already running.
    void start();

    /// Stop polling.  Called by MainWindow on MOX-down.  No-op if not
    /// running.  Does NOT destroy the analyzer — siphon may still push
    /// data; we just stop draining the pixel ring.
    void stop();

    /// True if the timer is currently running.
    bool isRunning() const noexcept;

    /// WDSP disp ID this analyzer owns.
    int dispId() const noexcept { return m_dispId; }

    // ── Phase 3M-5d: 9-control state surface ────────────────────────────
    // Each property below mirrors a Thetis SpecHPSDR field and is wired
    // to the same WDSP setter Thetis uses.  Setters persist to
    // AppSettings on every mutation (PascalCase keys, no per-pan
    // suffix — TX has one analyzer for the single TX disp).
    //
    // FFT size: 4096 * 2^slider per setup.cs:18138 [v2.10.3.13+501e3f51].
    void setFftSize(int n);
    int  fftSize() const noexcept { return m_fftSize; }

    /// Slider position 0..N → fftSize = 4096 * 2^position.  Mirrors Thetis
    /// tbTXDisplayFFTSize_Scroll at setup.cs:18138 [v2.10.3.13+501e3f51].
    void setFftSizeSliderPosition(int position);

    /// Current bin width in Hz = sampleRate / fftSize.  Mirrors Thetis
    /// setup.cs:18140 [v2.10.3.13+501e3f51].
    double binWidthHz() const noexcept;

    // Window type 0..6 per comboTXDispWinType ordering at
    // setup.designer.cs:36555-36562 [v2.10.3.13+501e3f51]:
    //   0=Rectangular, 1=Blackman-Harris 4T, 2=Hann, 3=Flat-Top,
    //   4=Hamming, 5=Kaiser, 6=Blackman-Harris 7T.
    // Default 4 (Hamming) per specHPSDR.cs:134 [v2.10.3.13+501e3f51].
    void setWindowType(int t);
    int  windowType() const noexcept { return m_windowType; }

    // Pan detector 0..4 per comboTXDispPanDetector items at
    // setup.designer.cs:36718-36723 [v2.10.3.13+501e3f51]:
    //   0=Peak, 1=Rosenfell, 2=Average, 3=Sample, 4=RMS.
    // Default 0 per specHPSDR.cs:301 [v2.10.3.13+501e3f51].
    void setPanDetector(int d);
    int  panDetector() const noexcept { return m_panDetector; }

    // Pan averaging mode 0..3 per comboTXDispPanAveraging items at
    // setup.designer.cs:36693-36697 [v2.10.3.13+501e3f51]:
    //   0=None, 1=Recursive, 2=Time Window, 3=Log Recursive.
    // Default 0 per specHPSDR.cs:382 [v2.10.3.13+501e3f51].
    void setPanAveraging(int m);
    int  panAveraging() const noexcept { return m_panAveraging; }

    // Pan averaging time in ms; tau seconds = ms * 0.001 per Thetis
    // setup.cs:18125 [v2.10.3.13+501e3f51].  Default 30 ms per
    // setup.designer.cs:36753 [v2.10.3.13+501e3f51].
    void   setPanAvTimeMs(int ms);
    int    panAvTimeMs() const noexcept { return m_panAvTimeMs; }
    double panAvTauSeconds() const noexcept;

    // Pan normalize-to-1-Hz checkbox per setup.cs:18129-18134.  Gated on
    // DetTypePan in {2,3,4} per setup.cs:18111-18112 [v2.10.3.13+501e3f51]
    // MW0LGE comment.
    void setPanNormalize(bool on);
    bool panNormalize()        const noexcept { return m_panNormalize; }
    bool panNormalizeEnabled() const noexcept;  // gated on detector >= 2

    // WF detector 0..3 per comboTXDispWFDetector items at
    // setup.designer.cs:36459-36463 [v2.10.3.13+501e3f51]:
    //   0=Peak, 1=Rosenfell, 2=Average, 3=Sample.  (No RMS slot for WF.)
    // Default 0 per specHPSDR.cs:313 [v2.10.3.13+501e3f51].
    void setWfDetector(int d);
    int  wfDetector() const noexcept { return m_wfDetector; }

    // WF averaging mode 0..3 per comboTXDispWFAveraging items at
    // setup.designer.cs:36434-36438 [v2.10.3.13+501e3f51]:
    //   0=None, 1=Recursive, 2=Time Window, 3=Log Recursive.
    // Default 0 per specHPSDR.cs:402 [v2.10.3.13+501e3f51].
    void setWfAveraging(int m);
    int  wfAveraging() const noexcept { return m_wfAveraging; }

    // WF averaging time in ms; tau seconds = ms * 0.001 per Thetis
    // setup.cs:18169 [v2.10.3.13+501e3f51].  Default 120 ms per
    // setup.designer.cs:36493 [v2.10.3.13+501e3f51].
    void   setWfAvTimeMs(int ms);
    int    wfAvTimeMs() const noexcept { return m_wfAvTimeMs; }
    double wfAvTauSeconds() const noexcept;

    // Number of WDSP pixel-out planes the analyzer is configured for.
    // 3M-5d ships n_pixout=2 (pan + wf independent) per Thetis
    // specHPSDR.cs:471-480 [v2.10.3.13+501e3f51] _pixel_out default.
    int nPixout() const noexcept { return m_nPixout; }

    // Test seam: ticks each time TxAnalyzer pushes a config to WDSP
    // (SetAnalyzer / SetDisplay*).  Used by
    // tests/tst_tx_analyzer_settings.cpp `setAnalyzer_called_on_each_setter`.
    int analyzerConfigCount() const noexcept { return m_analyzerConfigCount; }

    // ── Remote-window parity Task 30 (R-R3-49, R-R3-21, A12) ─────────────
    // The nine settings keys above, as loadSettings/saveSettings name
    // them, and the value each takes when unset (the member defaults
    // below, whose cites are there).
    static constexpr const char* kFftSizeKey       = "DisplayTxFftSize";
    static constexpr const char* kWindowTypeKey    = "DisplayTxWindowType";
    static constexpr const char* kPanDetectorKey   = "DisplayTxPanDetector";
    static constexpr const char* kPanAveragingKey  = "DisplayTxPanAveraging";
    static constexpr const char* kPanAvTimeMsKey   = "DisplayTxPanAvTimeMs";
    static constexpr const char* kPanNormalizeKey  = "DisplayTxPanNormalize";
    static constexpr const char* kWfDetectorKey    = "DisplayTxWfDetector";
    static constexpr const char* kWfAveragingKey   = "DisplayTxWfAveraging";
    static constexpr const char* kWfAvTimeMsKey    = "DisplayTxWfAvTimeMs";
    static constexpr int  kDefaultFftSize      = 32768;
    static constexpr int  kDefaultWindowType   = 4;
    static constexpr int  kDefaultPanDetector  = 0;
    static constexpr int  kDefaultPanAveraging = 0;
    static constexpr int  kDefaultPanAvTimeMs  = 30;
    static constexpr bool kDefaultPanNormalize = false;
    static constexpr int  kDefaultWfDetector   = 0;
    static constexpr int  kDefaultWfAveraging  = 0;
    static constexpr int  kDefaultWfAvTimeMs   = 120;

    /// True for exactly the nine keys above.
    static bool isSettingsKey(const QString& key);

    /// The FFT size slider's position (0..6) for an FFT size: the first
    /// position whose 4096 * 2^position reaches `fftSize`, 6 above that.
    /// The rule the TX Display page has always used to show a size.
    static int fftSizeSliderPositionFor(int fftSize) noexcept;

    /// A Core applies a window's settings.write (or removal) of one of the
    /// nine keys: reads `key` from AppSettings and applies it through the
    /// setter the local TX Display page calls, at once, keyed or not, as
    /// Thetis's TX Display handlers do (setup.cs:18146-18210). An unset key
    /// takes its default; a value the setter would not hold (not a number,
    /// out of range, an FFT size that is not a slider position) applies
    /// what the setter makes of it and is written back as that value. Only
    /// `key` is written: the other eight stay as they are. Any other key
    /// is ignored. Emits settingReloaded(key) once applied.
    void reloadSetting(const QString& key);

signals:
    void analyzerCreated(bool ready);
    /// FFT bins ready (in dBm) for the spectrum trace plane (pixout=0).
    /// Compatible signature with FFTEngine::fftReady so
    /// SpectrumWidget::updateSpectrum can be connected interchangeably.
    /// receiverId arg is sentinel -1 to distinguish from RX (receiverId 0).
    void txFftReady(int receiverId, const QVector<float>& binsDbm);

    /// FFT bins ready (in dBm) for the waterfall plane (pixout=1).
    /// 3M-5d split off from txFftReady so the WF plane uses
    /// DetTypeWF + AverageModeWF (configured independently in WDSP)
    /// instead of sharing the pan plane's detector + averaging.
    /// receiverId arg is sentinel -1, same convention as txFftReady.
    void txWaterfallReady(int receiverId, const QVector<float>& binsDbm);

    /// Parity Task 30: reloadSetting applied `key` (a remote window's
    /// change on the Core). A TX Display page open on the Core's own
    /// window shows the analyzer's values again.
    void settingReloaded(const QString& key);

private slots:
    /// Polled at outputFps.  Calls GetPixels(dispId, 0, ...) for the
    /// spectrum plane and GetPixels(dispId, 1, ...) for the waterfall
    /// plane; emits txFftReady / txWaterfallReady on each new frame
    /// (flag != 0).
    void poll();

private:
    void applySetAnalyzer();
    void applyDetectorMode(int pixout, int mode);
    void applyAverageMode(int pixout, int mode);
    void applyAvTau(int pixout, int avTimeMs);
    void applyNormalizePan();

    void loadSettings();
    void saveSettings();
    // Task 30: set while reloadSetting applies a key, so the setter's
    // saveSettings does not write all nine keys back (reloadSetting writes
    // back only its own key, and only when the setter changed its value).
    bool m_reloadingSetting{false};
    const bool m_persistSettings{true};
    // Secondary creation completes on the TX lane. Its destructor queues a
    // conditional destroy after creation even if the GUI callback was lost.
    std::shared_ptr<std::atomic<bool>> m_createSucceeded;

    const int m_dispId;
    int m_numPixels{2048};   // matches typical SpectrumWidget width
    // Thetis ships tbTXDisplayFFTSize.Value = 3 (setup.designer.cs:36642
    // [v2.10.3.13+501e3f51]); formula 4096 * 2^3 = 32768.  Earlier 3M-5d
    // spec table said 4096 (Thetis slider position 0); that was a spec
    // error caught at bench.  Slider Maximum = 6 → max FFT = 262144 which
    // matches the m_size passed to XCreateAnalyzer (TxAnalyzer.cpp).
    int m_fftSize{kDefaultFftSize};
    double m_sampleRate{96000.0};   // matches WdspEngine::kTxDspSampleRate

    // Display window around the carrier, in Hz, from
    // MainWindow's transmit window. Both zero means "no clipping, full
    // span", which is the state before the first MOX edge and after the
    // fall edge clears it.
    // SetAnalyzer's bf_sz. Zero means "not told yet", in which case
    // applySetAnalyzer falls back to the FFT size to preserve the
    // pre-2026-08-05 behaviour rather than pass a nonsense zero.
    /// True until the first start(). Holds off SetAnalyzer, and with it
    /// FFTW plan construction, so a cold launch does not plan a 32768
    /// point transform on the GUI thread inside buildUI().
    bool m_deferSetAnalyzer{false};
    int m_setAnalyzerCount{0};

    int m_blockSize {0};

    int m_spanLowHz {0};
    int m_spanHighHz{0};
    // From Thetis specHPSDR.cs:335 [v2.10.3.13+501e3f51] — frame_rate default = 15.
    int m_outputFps{15};

    // From Thetis specHPSDR.cs:134 [v2.10.3.13+501e3f51] — window_type
    // default = 4 (Hamming).  3M-5d reverts the 3M-5b NereusSDR
    // BH4 (= 1) divergence per controller decision 2026-05-10.
    int m_windowType{kDefaultWindowType};

    // From Thetis specHPSDR.cs:301, :382 [v2.10.3.13+501e3f51] — det_type
    // and av_mode defaults are 0 (Peak / None).
    int m_panDetector{kDefaultPanDetector};
    int m_panAveraging{kDefaultPanAveraging};

    // From Thetis setup.designer.cs:36753 [v2.10.3.13+501e3f51] —
    // udTXDisplayAVGTime.Value = 30.  Spec table at master plan §Phase 3
    // says 120 for both; source-read overrides — Thetis pan default is 30.
    int m_panAvTimeMs{kDefaultPanAvTimeMs};

    // From Thetis specHPSDR.cs:324 [v2.10.3.13+501e3f51] — norm_oneHz_pan
    // default = false.
    bool m_panNormalize{kDefaultPanNormalize};

    int m_wfDetector{kDefaultWfDetector};
    int m_wfAveraging{kDefaultWfAveraging};

    // From Thetis setup.designer.cs:36493 [v2.10.3.13+501e3f51] —
    // udTXDisplayAVTime.Value = 120.
    int m_wfAvTimeMs{kDefaultWfAvTimeMs};

    // 3M-5d: bump from 1 to 2 so pan + wf detector/averaging diverge.
    // From Thetis specHPSDR.cs:471 [v2.10.3.13+501e3f51] — _pixel_out
    // default = 2.
    int m_nPixout{2};

    // R-R3-39: runs `job` on the lane (in call order) or, with no lane, at
    // once. Jobs carry values, never this object.
    void runWdsp(std::function<void()> job) const;
    // Guarded: the desktop deletes its RadioModel (and the lane with it)
    // before this analyzer; from then on the calls run here, after the TX
    // channel feeding the analyzer has closed.
    QPointer<DspControlThread> m_lane;
    // A poll still on the lane: the next tick skips rather than queue more.
    std::shared_ptr<std::atomic<bool>> m_pollInFlight{
        std::make_shared<std::atomic<bool>>(false)};

    QTimer m_pollTimer;
    QVector<float> m_pixBuf;     // pixout=0 (spectrum trace)
    QVector<float> m_pixBufWf;   // pixout=1 (waterfall)

    bool m_analyzerCreated{false};

    // Test seam: ticks on each WDSP config push.  Exposed via
    // analyzerConfigCount() for tst_tx_analyzer_settings.
    int m_analyzerConfigCount{0};
};

} // namespace NereusSDR
