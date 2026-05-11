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
// =================================================================

#pragma once

#include <QObject>
#include <QTimer>
#include <QVector>

namespace NereusSDR {

class TxAnalyzer : public QObject {
    Q_OBJECT

public:
    /// WDSP analyzer display ID reserved for the TX panadapter.
    /// From Thetis cmaster.cs:411 [v2.10.3.13+501e3f51] — cmRCVR = 5;
    /// cmaster.inid(1, 0) = cmRCVR + 0 = 5, which is the txinid passed to
    /// TXASetSipDisplay.  Phase 3M-4 PureSignal AmpView would use a separate
    /// disp ID.
    static constexpr int kTxDispId = 5;

    explicit TxAnalyzer(int dispId = kTxDispId, QObject* parent = nullptr);
    ~TxAnalyzer() override;

    /// Update output pixel count to match the panadapter's display width.
    /// Called when SpectrumWidget resizes.  Triggers a SetAnalyzer re-call
    /// with the new n_pix; safe to call from the main thread (WDSP's
    /// SetAnalyzerSection blocks briefly while the analyzer reconfigures).
    void setNumPixels(int n);

    /// Update analyzer sample rate.  TX is always at the WDSP DSP rate
    /// (96 kHz — see WdspEngine::kTxDspSampleRate, matches Thetis
    /// cmaster.c:182 [v2.10.3.13]).  Called once at startup and on rate
    /// changes.
    void setSampleRate(double rateHz);

    /// Update output frame rate (frames per second).  Affects the
    /// analyzer overlap calculation per specHPSDR.cs:784 [v2.10.3.13+501e3f51].
    /// Default 15 fps per specHPSDR.cs:335 [v2.10.3.13+501e3f51].
    void setOutputFps(int fps);

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

signals:
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

    const int m_dispId;
    int m_numPixels{2048};   // matches typical SpectrumWidget width
    // Thetis ships tbTXDisplayFFTSize.Value = 3 (setup.designer.cs:36642
    // [v2.10.3.13+501e3f51]); formula 4096 * 2^3 = 32768.  Earlier 3M-5d
    // spec table said 4096 (Thetis slider position 0); that was a spec
    // error caught at bench.  Slider Maximum = 6 → max FFT = 262144 which
    // matches the m_size passed to XCreateAnalyzer (TxAnalyzer.cpp).
    int m_fftSize{32768};
    double m_sampleRate{96000.0};   // matches WdspEngine::kTxDspSampleRate
    // From Thetis specHPSDR.cs:335 [v2.10.3.13+501e3f51] — frame_rate default = 15.
    int m_outputFps{15};

    // From Thetis specHPSDR.cs:134 [v2.10.3.13+501e3f51] — window_type
    // default = 4 (Hamming).  3M-5d reverts the 3M-5b NereusSDR
    // BH4 (= 1) divergence per controller decision 2026-05-10.
    int m_windowType{4};

    // From Thetis specHPSDR.cs:301, :382 [v2.10.3.13+501e3f51] — det_type
    // and av_mode defaults are 0 (Peak / None).
    int m_panDetector{0};
    int m_panAveraging{0};

    // From Thetis setup.designer.cs:36753 [v2.10.3.13+501e3f51] —
    // udTXDisplayAVGTime.Value = 30.  Spec table at master plan §Phase 3
    // says 120 for both; source-read overrides — Thetis pan default is 30.
    int m_panAvTimeMs{30};

    // From Thetis specHPSDR.cs:324 [v2.10.3.13+501e3f51] — norm_oneHz_pan
    // default = false.
    bool m_panNormalize{false};

    int m_wfDetector{0};
    int m_wfAveraging{0};

    // From Thetis setup.designer.cs:36493 [v2.10.3.13+501e3f51] —
    // udTXDisplayAVTime.Value = 120.
    int m_wfAvTimeMs{120};

    // 3M-5d: bump from 1 to 2 so pan + wf detector/averaging diverge.
    // From Thetis specHPSDR.cs:471 [v2.10.3.13+501e3f51] — _pixel_out
    // default = 2.
    int m_nPixout{2};

    QTimer m_pollTimer;
    QVector<float> m_pixBuf;     // pixout=0 (spectrum trace)
    QVector<float> m_pixBufWf;   // pixout=1 (waterfall)

    bool m_analyzerCreated{false};

    // Test seam: ticks on each WDSP config push.  Exposed via
    // analyzerConfigCount() for tst_tx_analyzer_settings.
    int m_analyzerConfigCount{0};
};

} // namespace NereusSDR
