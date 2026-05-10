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

signals:
    /// FFT bins ready (in dBm).  Compatible signature with
    /// FFTEngine::fftReady so SpectrumWidget::updateSpectrum can be
    /// connected interchangeably.  receiverId arg is sentinel -1 to
    /// distinguish from RX (receiverId 0).
    void txFftReady(int receiverId, const QVector<float>& binsDbm);

private slots:
    /// Polled at outputFps.  Calls GetPixels(dispId, 0, ...); if the
    /// flag returns 1 (new frame available), emits txFftReady.
    void poll();

private:
    void applySetAnalyzer();

    const int m_dispId;
    int m_numPixels{2048};   // matches typical SpectrumWidget width
    int m_fftSize{4096};
    double m_sampleRate{96000.0};   // matches WdspEngine::kTxDspSampleRate
    // From Thetis specHPSDR.cs:335 [v2.10.3.13+501e3f51] — frame_rate default = 15.
    int m_outputFps{15};

    QTimer m_pollTimer;
    QVector<float> m_pixBuf;  // pre-allocated GetPixels output buffer

    bool m_analyzerCreated{false};
};

} // namespace NereusSDR
