#pragma once

// =================================================================
// src/models/RadioModel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include "core/ConnectionState.h"
#include "Band.h"
#include "BandPlanManager.h"
#include "SliceModel.h"
#include "PanadapterModel.h"
#include "MeterModel.h"
#include "TransmitModel.h"
#include "core/Hl2OptionsModel.h"
#include "core/OcMatrix.h"
#include "core/IoBoardHl2.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "core/RadioStatus.h"
#include "core/SettingsHygiene.h"
#include "core/accessories/AlexController.h"
#include "core/accessories/ApolloController.h"
#include "core/accessories/PennyLaneController.h"
#include "core/CalibrationController.h"
#include "core/RadioDiscovery.h"
#include "core/RadioConnection.h"
#include "core/HardwareProfile.h"
#include "core/safety/SwrProtectionController.h"
#include "core/safety/TxInhibitMonitor.h"
#include "core/safety/BandPlanGuard.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QList>
#include <QThread>

// 3M-1a G.1: TxMicRouter is a plain (non-QObject) strategy interface.
// Include required directly so unique_ptr destructor is available here.
#include "core/TxMicRouter.h"
// (Phase 3M-1c L.4 added a `core/audio/MicReBlocker.h` include for the
//  unique_ptr<MicReBlocker> destructor.  The TX pump architecture
//  redesign (2026-04-29) deleted MicReBlocker; replaced with
//  TxWorkerThread which drives TxChannel directly.)
#include <algorithm>  // std::clamp (used by computeWireDriveForTest)
#include <memory>  // std::unique_ptr

namespace NereusSDR {

class ReceiverManager;
class AudioEngine;
class WdspEngine;
class RxDspWorker;
class NoiseFloorTracker;
// 3M-1a G.1: forward declarations for TX-side components.
class MoxController;
class TxChannel;
// 3M-1b L.1: forward declarations for mic-source strategy objects.
class PcMicSource;
class RadioMicSource;
class CompositeTxMicRouter;
// 3M-1c L.1 / L.2: forward declarations for the MicProfileManager bank
// (chunk F) + the TwoToneController activation orchestrator (chunk I).
class MicProfileManager;
class TwoToneController;
// 3M-1c TX pump architecture redesign — TxWorkerThread.
class TxWorkerThread;

// RadioModel is the central data model for a connected radio.
// It owns the RadioConnection (on a worker thread), ReceiverManager,
// and all sub-models. It routes signals between components.
//
// Thread architecture:
//   Main thread: RadioModel, ReceiverManager, all sub-models, GUI,
//                AudioEngine (timer-driven QAudioSink drain)
//   Connection thread: RadioConnection (sockets, protocol I/O)
//   DSP thread:  RxDspWorker — runs RxChannel::processIq → fexchange2;
//                kept off main because WDSP fexchange2 with bfo=1 can
//                block on Sem_OutReady and would otherwise freeze the
//                Qt event loop, deadlocking against wdspmain.
class RadioModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString name        READ name        NOTIFY infoChanged)
    Q_PROPERTY(QString model       READ model       NOTIFY infoChanged)
    Q_PROPERTY(QString version     READ version     NOTIFY infoChanged)
    Q_PROPERTY(bool    connected   READ isConnected NOTIFY connectionStateChanged)

public:
    explicit RadioModel(QObject* parent = nullptr);
    ~RadioModel() override;

    // Sub-components
    RadioConnection*  connection()       { return m_connection; }
    const RadioConnection* connection() const { return m_connection; }
    RadioDiscovery*   discovery()        { return m_discovery; }
    ReceiverManager*  receiverManager()  { return m_receiverManager; }
    AudioEngine*      audioEngine()      { return m_audioEngine; }
    WdspEngine*       wdspEngine()       { return m_wdspEngine; }

    // OC matrix — single instance shared between the OC Outputs UI and the
    // codec layer (P1/P2 buildCodecContext). Loaded per-MAC at connect time.
    // Phase 3P-D Task 3.
    const OcMatrix& ocMatrix()        const { return m_ocMatrix; }
    OcMatrix&       ocMatrixMutable()       { return m_ocMatrix; }

    // HL2 I/O board model — single instance; non-null on any HL2 connection.
    // Pushed into P1RadioConnection::setIoBoard() at connect time so the
    // codec layer can dequeue I2C transactions.  Phase 3P-E Task 2.
    const IoBoardHl2& ioBoard()        const { return m_ioBoard; }
    IoBoardHl2&       ioBoardMutable()       { return m_ioBoard; }

    // HL2 Options model — 9 HL2-specific behavior knobs (mi0bot tpHL2Options).
    // Loaded per-MAC at connect time, mirrors OcMatrix ownership pattern.
    // Phase 3L commit #9.  Wire-format emission deferred to a follow-up PR.
    const Hl2OptionsModel& hl2Options()        const { return m_hl2Options; }
    Hl2OptionsModel&       hl2OptionsMutable()       { return m_hl2Options; }

    // HL2 bandwidth monitor — single instance; pushed into P1RadioConnection
    // via setBandwidthMonitor() at connect time when hasBandwidthMonitor.
    // Phase 3P-E Task 3.
    const HermesLiteBandwidthMonitor& bwMonitor()        const { return m_bwMonitor; }
    HermesLiteBandwidthMonitor&       bwMonitorMutable()       { return m_bwMonitor; }

    // Live PA telemetry and PTT state — single instance owned here.
    // Setters called by connection layer on each status packet.
    // Backed by Phase 3P-H Task 1 RadioStatus model.
    // Phase 3P-H Task 2.
    const RadioStatus& radioStatus()        const { return m_radioStatus; }
    RadioStatus&       radioStatus()              { return m_radioStatus; }

    // Settings hygiene validation — single instance owned here.
    // Call validate() after each successful connect.
    // Phase 3P-H Task 2.
    const SettingsHygiene& settingsHygiene()        const { return m_settingsHygiene; }
    SettingsHygiene&       settingsHygiene()              { return m_settingsHygiene; }

    // Alex antenna controller — per-band TX/RX/RX-only antenna assignment.
    // Loaded per-MAC at connect time. Backs Antenna Control sub-sub-tab UI
    // (AntennaAlexAntennaControlTab — Phase 3P-F Task 3).
    const AlexController& alexController()        const { return m_alexController; }
    AlexController&       alexControllerMutable()       { return m_alexController; }

    // Band-plan overlay manager — loaded once on construction from bundled
    // Qt resource JSON files. Active plan persists in AppSettings under
    // "BandPlanName". Phase 3G RX Epic sub-epic D.
    const BandPlanManager& bandPlanManager()        const { return m_bandPlanManager; }
    BandPlanManager&       bandPlanManagerMutable()       { return m_bandPlanManager; }

    // Apollo PA + ATU + LPF accessory controller — present/filter/tuner enable flags.
    // Loaded per-MAC at connect time. Setup UI deferred (Phase 3P-F Task 5a).
    const ApolloController& apolloController()        const { return m_apolloController; }
    ApolloController&       apolloControllerMutable()       { return m_apolloController; }

    // PennyLane / Penelope external-control master toggle.
    // Loaded per-MAC at connect time. OC bitmask logic lives in OcMatrix (Phase 3P-D).
    // Setup UI deferred (Phase 3P-F Task 5b).
    const PennyLaneController& pennyLaneController()        const { return m_pennyLaneController; }
    PennyLaneController&       pennyLaneControllerMutable()       { return m_pennyLaneController; }

    // Calibration controller — HPSDR NCO correction factor, level offsets, LNA
    // offsets, TX display cal, PA current sens/offset. Loaded per-MAC at connect.
    // Backs CalibrationTab UI and P2RadioConnection::hzToPhaseWord(). Phase 3P-G.
    const CalibrationController& calibrationController()        const { return m_calController; }
    CalibrationController&       calibrationControllerMutable()       { return m_calController; }

    // Phase 3M-0 Task 17: safety controller accessors.
    // SwrProtectionController and TxInhibitMonitor are QObject-owned by RadioModel.
    // BandPlanGuard is a plain value class (no Qt parent).
    safety::SwrProtectionController& swrProt() noexcept { return m_swrProt; }
    const safety::SwrProtectionController& swrProt() const noexcept { return m_swrProt; }
    safety::TxInhibitMonitor& txInhibit() noexcept { return m_txInhibit; }
    const safety::TxInhibitMonitor& txInhibit() const noexcept { return m_txInhibit; }
    safety::BandPlanGuard& bandPlan() noexcept { return m_bandPlan; }
    const safety::BandPlanGuard& bandPlan() const noexcept { return m_bandPlan; }

    // Sub-models
    MeterModel&       meterModel()       { return m_meterModel; }
    TransmitModel&    transmitModel()    { return m_transmitModel; }

    // Slice management (client-side — radio has no slice concept)
    QList<SliceModel*> slices() const { return m_slices; }
    SliceModel* sliceAt(int index) const;
    SliceModel* activeSlice() const { return m_activeSlice; }
    int addSlice();
    void removeSlice(int index);
    void setActiveSlice(int index);

    // Band-button click handler. Routes both SpectrumOverlayPanel::bandSelected
    // and ContainerWidget::bandClicked through one code path. On first
    // visit to `band`, applies BandDefaults::seedFor(band) and persists;
    // on subsequent visits, restores last-used per-band state via the
    // 3G-10 Stage 2 persistence already on SliceModel.
    //
    // Same-band click is a no-op. XVTR with no seed and no persisted
    // state is a logged no-op. Locked slices freeze frequency (mode still
    // changes, matching Thetis lock semantics).
    //
    // Acts on activeSlice(). No-op if active slice is null.
    //
    // Issue #118.
    void onBandButtonClicked(NereusSDR::Band band);

    // Panadapter management (client-side)
    QList<PanadapterModel*> panadapters() const { return m_panadapters; }
    int addPanadapter();
    void removePanadapter(int index);

    // View hooks: non-owning pointers to the primary spectrum widget and
    // FFT engine so setup pages (Phase 3G-8+) can call renderer/FFT
    // setters without depending on MainWindow. Wired by MainWindow after
    // constructing each view. Not owned, not lifetime-tracked — MainWindow
    // outlives both.
    class SpectrumWidget* spectrumWidget() const { return m_spectrumWidget; }
    void setSpectrumWidget(class SpectrumWidget* w) { m_spectrumWidget = w; }
    class FFTEngine* fftEngine() const { return m_fftEngine; }
    void setFftEngine(class FFTEngine* e) { m_fftEngine = e; }
    class ClarityController* clarityController() const { return m_clarityController; }
    void setClarityController(class ClarityController* c) { m_clarityController = c; }
    class StepAttenuatorController* stepAttController() const { return m_stepAttController; }
    void setStepAttController(class StepAttenuatorController* c) { m_stepAttController = c; }
    NoiseFloorTracker* noiseFloorTracker() const { return m_noiseFloorTracker; }
    void setNoiseFloorTracker(NoiseFloorTracker* t) { m_noiseFloorTracker = t; }
    QTimer* autoAgcTimer() const { return m_autoAgcTimer; }

    // 3M-1a G.1: expose MoxController so MainWindow can wire
    // StepAttenuatorController::onMoxHardwareFlipped (F.2 connect) after
    // both objects exist.  Non-owning; lifetime is RadioModel's lifetime.
    // Master design §5.1.1; pre-code review §1.6.
    MoxController* moxController() const { return m_moxController; }

    // 3M-1c Phase L.1: expose MicProfileManager so MainWindow / SetupDialog
    // can hand the per-MAC profile bank to TxApplet (J.1 setter) and
    // TxProfileSetupPage (J.3 ctor).  Non-owning; lifetime is RadioModel's
    // lifetime.  See header §3M-1c L.1 for the construction + connect flow.
    MicProfileManager* micProfileManager() const { return m_micProfileMgr; }

    // 3M-1c Phase L.2: expose TwoToneController so MainWindow can hand it to
    // TxApplet (J.2 setter) for the 2-TONE button + status mirror.
    // Non-owning; lifetime is RadioModel's lifetime.
    TwoToneController* twoToneController() const { return m_twoToneController; }

    // 3M-1a G.1: expose TxChannel view so TxApplet and G.4 TUNE function
    // can call setTuneTone / setRunning without depending on WdspEngine.
    // Non-owning; WdspEngine owns the channel. Null until WDSP initializes.
    // Master design §5.1.1; pre-code review §2.5.
    // TxChannel::setConnection() + setMicRouter() inject the production loop
    // pointers in the WDSP-init lambda (see connectToRadio). The 5 ms QTimer
    // in TxChannel drives fexchange2 → sendTxIq (SPSC ring) while running.
    // Wired by 3M-1a Task G.1 (bench fix: TUNE carrier now reaches the radio).
    TxChannel* txChannel() const { return m_txChannel; }

    // Phase 3G-9b: one-shot profile that sets the 7 smooth-default recipe
    // values on SpectrumWidget. Called from the constructor exactly once
    // on first launch (gated by AppSettings key "DisplayProfileApplied").
    // Also callable on demand via the "Reset to Smooth Defaults" button
    // on SpectrumDefaultsPage, in which case it unconditionally applies
    // regardless of the gate.
    //
    // See docs/architecture/waterfall-tuning.md for the rationale behind
    // each value.
    void applyClaritySmoothDefaults();

    // Radio info
    QString name() const { return m_name; }
    QString model() const { return m_model; }
    QString version() const { return m_version; }
    const HardwareProfile& hardwareProfile() const { return m_hardwareProfile; }

    // Returns the BoardCapabilities for the current (or last) board.
    // Falls back to the Unknown board caps when no radio has ever connected.
    // Phase 3P-A Task 15: exposes caps so RxApplet can set slider range at
    // construction time, not only after a connection is established.
    const BoardCapabilities& boardCapabilities() const;

    bool isConnected() const;

    // ── Phase 3Q sub-PR-3: NetworkDiagnosticsDialog text accessors ───────────
    // Each returns an em-dash placeholder ("—") when disconnected.
    // m_connectionStartedAt is set in setConnectionState() on the
    // Connected → anything transition; cleared on non-Connected states.
    QString connectionUptimeText() const;     // "14m 32s" / "—"
    QString connectedRadioName() const;       // RadioInfo.name / "—"
    QString connectionProtocolText() const;   // "1" or "2" / "—"
    QString connectionFirmwareText() const;   // "v27" / "—"
    QString connectionIpText() const;         // "192.168.x.y : port" / "—"
    QString connectionMacText() const;        // "AA:BB:CC:DD:EE:FF" / "—"
    int     connectionSampleRateHz() const;   // 0 if disconnected
    QString connectionSampleRateText() const; // "192 kHz" / "—"

    // Phase 3Q Sub-PR-4 D.3: Hover tooltip for the TitleBar ConnectionSegment.
    // Returns a multi-line string with radio name, uptime, IP, MAC, protocol,
    // firmware, sample rate, and live throughput. Disconnected state returns a
    // short invitation to connect. Owned by RadioModel so the segment stays a
    // thin presentation layer.
    QString buildConnectionTooltip() const;

    // Phase 3Q-1: single source of truth for the connection lifecycle state.
    // UI components (TitleBar, ConnectionPanel, status bar, spectrum overlay)
    // read this instead of deriving state from RadioConnection directly.
    ConnectionState connectionState() const { return m_connectionState; }

    // Test-only: allow tests to drive transitions without standing up
    // a fake RadioConnection. Production transitions go through the
    // private setConnectionState() called from connection signals.
    void setConnectionStateForTest(ConnectionState s) { setConnectionState(s); }

#ifdef NEREUS_BUILD_TESTS
public:
    // Test-only: inject board caps without a live radio connection.
    // Mirrors P1RadioConnection::setBoardForTest pattern.
    void setBoardForTest(HPSDRHW board) {
        m_hardwareProfile = ::NereusSDR::profileForModel(
            defaultModelForBoard(board));
    }

    // Phase 3P-I-a T14 — test-only hooks. Allow tests to inject a mock
    // RadioConnection, simulate band crossings, trigger the Connected
    // state handler, and override board capabilities. Production code
    // must never use these.
    void injectConnectionForTest(RadioConnection* conn) { m_connection = conn; }
    void setLastBandForTest(NereusSDR::Band b) {
        const bool cross = (b != m_lastBand);
        m_lastBand = b;
        if (cross) {
            applyAlexAntennaForBand(b);
            // Mirror the production T10 path so tests catch regressions
            // in the slice-label refresh (see RadioModel.cpp frequencyChanged
            // handler for the canonical version).
            if (m_activeSlice) {
                m_activeSlice->refreshAntennasFromAlex(m_alexController, b);
            }
        }
    }
    void onConnectedForTest() {
        applyAlexAntennaForBand(m_lastBand);
    }
    // Phase 3P-I-b (T6): expose with isTx parameter for composition tests.
    void applyAlexAntennaForBandForTest(NereusSDR::Band band, bool isTx) {
        applyAlexAntennaForBand(band, isTx);
    }
    void setCapsForTest(bool hasAlex) {
        m_testCapsOverride  = true;
        m_testCapsHasAlex   = hasAlex;
        m_testCapsIsRxOnly  = false;  // reset sibling so combined state is unambiguous
    }
    // 3M-1a G.2: inject isRxOnlySku without a live HermesLiteRxOnly board.
    // (HermesLiteRxOnly has no HPSDRModel entry so setBoardForTest cannot
    // reach its caps via the normal profileForModel path.)
    // Resets m_testCapsHasAlex so chaining setCapsForTest + setCapsRxOnlyForTest
    // in the same fixture does not silently combine both flags.
    void setCapsRxOnlyForTest(bool isRxOnly) {
        m_testCapsOverride  = true;
        m_testCapsHasAlex   = false;  // reset sibling so combined state is unambiguous
        m_testCapsIsRxOnly  = isRxOnly;
    }
    // 3M-1b I.1: inject hasMicJack without a live radio board.
    // HL2 sets hasMicJack=false; all other boards set true (default).
    // Does not reset other test-cap flags — compose with setCapsRxOnlyForTest
    // if a combined cap override is needed (each flag is independent).
    void setCapsHasMicJackForTest(bool hasMicJack) {
        m_testCapsOverride     = true;
        m_testCapsHasMicJack   = hasMicJack;
    }
    // 3M-1b I.3: inject HPSDRHW board type to select the per-family Radio Mic
    // group box in AudioTxInputPage without a live radio connection.
    // Does not reset other test-cap flags — independent of hasMicJack.
    void setCapsHwForTest(HPSDRHW hw) {
        m_testCapsOverride = true;
        m_testCapsHw       = hw;
    }
    // Emit currentRadioChanged with a default-constructed RadioInfo for test use.
    // Use this to simulate a reconnect when testing signal-driven visibility updates.
    void emitCurrentRadioChangedForTest() {
        emit currentRadioChanged(NereusSDR::RadioInfo{});
    }
    NereusSDR::Band lastBand() const { return m_lastBand; }

    // P1 full-parity §3.4 test hook — invoke the per-sample PA telemetry
    // handler directly without spinning up the full wireConnectionSignals
    // pipeline (which constructs DSP threads and the RxDspWorker).  Mirrors
    // the existing on*ForTest pattern (setConnectionStateForTest /
    // onConnectedForTest / setLastBandForTest).  Production code reaches
    // the same handler via the lambda installed in wireConnectionSignals.
    void handlePaTelemetryForTest(quint16 fwdRaw, quint16 revRaw,
                                  quint16 exciterRaw, quint16 userAdc0Raw,
                                  quint16 userAdc1Raw, quint16 supplyRaw) {
        handlePaTelemetry(fwdRaw, revRaw, exciterRaw,
                          userAdc0Raw, userAdc1Raw, supplyRaw);
    }

    // P1 full-parity §3.5 test seam — pure-function counterpart of the
    // percent-to-wire-byte SWR-foldback formula inlined at every
    // setTxDrive call site (voice powerChanged lambda, TUNE-engage,
    // TUNE-restore).  Tests assert against this helper to verify the
    // formula in isolation; production callsites use the same three-line
    // expression (see RadioModel.cpp).  A regression in the helper is a
    // regression in the inlined production code by construction.
    //
    // Source: mi0bot NetworkIO.cs:209-211 [v2.10.3.14-beta1]
    //   int i = (int)(255 * f * _swr_protect);   // f normalised 0..1,
    //                                            // _swr_protect ≤ 1.0
    static int computeWireDriveForTest(int powerPct, float swrProtectFactor) {
        const float f          = std::clamp(powerPct / 100.0f, 0.0f, 1.0f);
        const float swrProtect = std::clamp(swrProtectFactor, 0.0f, 1.0f);
        return std::clamp(int(255.0f * f * swrProtect), 0, 255);
    }

    // 3M-1b L.1 test seams: expose raw pointers into the mic-source strategy
    // objects so ownership, threading, and lifecycle tests can inspect state
    // without coupling to production API surfaces.
    // All three return nullptr before the first connectToRadio() / after
    // teardownConnection() — exactly the lifecycle the tests verify.
    const PcMicSource*           pcMicSourceForTest()          const { return m_pcMicSource.get(); }
    const RadioMicSource*        radioMicSourceForTest()        const { return m_radioMicSource.get(); }
    const CompositeTxMicRouter*  compositeMicRouterForTest()   const { return m_compositeMicRouter.get(); }

    // 3M-1c TX pump architecture redesign test seam: returns the unique_ptr's
    // raw pointer to the TxWorkerThread.  Returns nullptr before the first
    // connectToRadio() (m_txWorker is constructed inside the WDSP-init lambda
    // once m_audioEngine and m_txChannel are both live) and after
    // teardownConnection() resets it.  Used by tst_radio_model_3m1b_ownership
    // to verify worker construction/destruction follows the documented
    // lifecycle.
    // Test-only accessor — do not use in production code.
    const TxWorkerThread* txWorkerForTest() const { return m_txWorker.get(); }
    // Phase 3M-1c TX pump v3 — TxMicSource is constructed alongside
    // TxWorkerThread; allow tests to verify the pre/post-connect ownership.
    const class TxMicSource* txMicSourceForTest() const { return m_txMicSource.get(); }

    // 3M-1b L.3 test seam: simulate connectToRadio()'s loadFromSettings +
    // HL2 force-Pc sequence without a live radio connection.
    // Call setCapsHasMicJackForTest(bool) first to inject the board caps,
    // then call this to run the exact same two-step sequence as
    // connectToRadio(): loadFromSettings(mac) → setMicSourceLocked(!hasMicJack).
    // After this call, transmitModel().micSource() and isMicSourceLocked()
    // reflect the HL2 (or non-HL2) post-connect state.
    void simulateConnectLoadForTest(const QString& mac) {
        m_transmitModel.loadFromSettings(mac);
        m_transmitModel.setMicSourceLocked(!boardCapabilities().hasMicJack);
    }

    // Release the lock, mirroring teardownConnection()'s setMicSourceLocked(false).
    // Use between simulated reconnects in the same test.
    void simulateDisconnectForTest() {
        m_transmitModel.setMicSourceLocked(false);
    }
#endif

    // Connection
    void connectToRadio(const RadioInfo& info);
    void disconnectFromRadio();

    // Phase 3Q Task 10: arm / disarm the auto-connect-in-progress flag.
    // Called by MainWindow::tryAutoReconnect() before and after the probe.
    // When armed, RadioModel::wireConnectionSignals wires RadioConnection::connectFailed
    // to emit autoConnectFailed(mac, reason) and then disarms automatically.
    void setAutoConnectInProgress(bool inProgress, const QString& chosenMac = {}) {
        m_autoConnectInProgress = inProgress;
        m_autoConnectChosenMac  = inProgress ? chosenMac : QString{};
    }

    // Phase 3Q Task 10: called by MainWindow when multiple saved radios have
    // autoConnect = true. Emits autoConnectAmbiguous so the MainWindow lambda
    // can post the status-bar warning without the caller reaching into our signals.
    void notifyAutoConnectAmbiguous(int count, const QString& chosenMac) {
        emit autoConnectAmbiguous(count, chosenMac);
    }

    // ── Phase 3M-0 Task 6: Ganymede PA-trip live state ───────────────────────
    // G8NJJ: handlers for Ganymede 500W PA protection
    // From Thetis Andromeda/Andromeda.cs:914-948 [v2.10.3.13]
    // (CATHandleAmplifierTripMessage + GanymedeResetPressed).

    /// True iff a Ganymede PA trip is currently latched.
    /// From Thetis Andromeda/Andromeda.cs:914-920 [v2.10.3.13] (CATHandleAmplifierTripMessage).
    bool paTripped() const noexcept { return m_paTripped; }

    /// Apply a Ganymede CAT trip message. tripState != 0 latches the trip,
    /// 0 clears. As a safety side-effect, latching also drops MOX
    /// (Andromeda.cs:920 [v2.10.3.13]: `if (_ganymede_pa_issue && MOX) MOX = false`).
    void handleGanymedeTrip(int tripState);

    /// Clear the trip latch. Mirrors GanymedeResetPressed().
    /// Cite: Andromeda/Andromeda.cs (GanymedeResetPressed function) [v2.10.3.13].
    void resetGanymedePa();

    /// Setter for GanymedePresent capability. When set to false while a
    /// trip is latched, clears the trip (the radio no longer reports a PA).
    /// From Thetis Andromeda/Andromeda.cs:855-866 [v2.10.3.13] (GanymedePresent setter). //G8NJJ
    void setGanymedePresent(bool present);

public slots:
    // ── Phase 3M-1a Task F.1: MoxController::hardwareFlipped fan-out ───────────
    // Slot connected to MoxController::hardwareFlipped(bool isTx).
    // Fans out hardware-flip side-effects to AlexController + RadioConnection
    // in Thetis HdwMOXChanged step order (pre-code review §2.3):
    //   1. applyAlexAntennaForBand(currentBand, isTx)  — §2.3 step 8
    //   2. m_connection->setMox(isTx)                  — §2.3 / §1.4 step 12
    //   3. m_connection->setTrxRelay(isTx)             — §2.3 step 10
    //
    // Must be under public slots: so Qt's auto-connection queues this correctly
    // when the emitting object (MoxController) lives on a different thread.
    // G.1's connect() call uses Qt::QueuedConnection so the slot body runs on
    // RadioModel's thread; steps 2+3 then marshal to the connection thread via
    // QMetaObject::invokeMethod (see implementation).
    void onMoxHardwareFlipped(bool isTx);

    // ── Phase 3M-1a Task G.4: TUN function orchestrator ─────────────────────
    // Activate / release the TUNE function.
    //
    // Orchestrates all TUN side-effects across the model:
    //   TUN-on:  save DSP mode + power; swap CW→LSB/USB if needed;
    //            set tune tone; push tune power; drive MoxController.
    //   TUN-off: drive MoxController; release tone; restore DSP mode,
    //            power, and meter mode.
    //
    // Coordinates with:
    //   - MoxController::setTune(bool) for MOX state machine + flags.
    //   - TxChannel::setTuneTone(bool, freqHz, mag) for WDSP gen1 PostGen.
    //   - SliceModel::setDspMode() for CW→LSB/USB swap and restore.
    //   - TransmitModel::tunePowerForBand() + m_connection->setTxDrive()
    //     for per-band tune power push and restore.
    //
    // Power-on guard: emits tuneRefused(reason) and returns without any
    // state change if the radio is not connected (matching Thetis
    // console.cs:29983-29991 [v2.10.3.13] MessageBox "Power must be on").
    //
    // Meter mode save/restore: Thetis saves current_meter_tx_mode and
    // restores it on TUN-off (console.cs:30011-30015 [v2.10.3.13]).
    // NereusSDR's MeterModel does not yet expose a TX-mode selector (that
    // is H.3 territory); this method saves and restores m_transmitModel.power()
    // as the "slider power" position instead.  The full meter-mode lock
    // (switch to FORWARD_POWER display) is deferred to H.3 or 3M-1b when
    // MeterModel gains a setTxDisplayMode() setter.
    //
    // Inline attribution preserved from Thetis:
    //   //MW0LGE_21k9d  [original inline comment from console.cs:29980]
    //   //MW0LGE_21a    [original inline comment from console.cs:29997]
    //   //MW0LGE_22b    [original inline comment from console.cs:30033]
    //   //MW0LGE_21k8   [original inline comment from console.cs:30086]
    //   //MW0LGE_21j    [original inline comment from console.cs:30136]
    //
    // Cite: Thetis console.cs:29978-30157 [v2.10.3.13] — chkTUN_CheckedChanged.
    void setTune(bool on);

signals:
    void infoChanged();
    // Phase 3Q-1: parametrized — state passed so UI consumers can act without
    // a secondary RadioModel::connectionState() read under race conditions.
    // Existing no-arg slot connections (ConnectionPanel, MainWindow, SpectrumWidget)
    // remain valid: Qt discards excess signal args when slot arity is lower.
    void connectionStateChanged(NereusSDR::ConnectionState newState);
    // Emitted when the on-air sample rate for the current connection is
    // known. MainWindow reacts by updating FFTEngine + SpectrumWidget so
    // bin math matches the wire rate (P1=192k, P2=768k).
    void wireSampleRateChanged(double rateHz);
    // Fires on each transition to Connected with the RadioInfo of the live
    // connection. HardwarePage (Phase 3I) listens to this to repopulate
    // sub-tabs with per-radio fields.
    void currentRadioChanged(const NereusSDR::RadioInfo& info);
    void sliceAdded(int index);
    void sliceRemoved(int index);
    void activeSliceChanged(int index);
    void panadapterAdded(int index);
    void panadapterRemoved(int index);

    // Raw interleaved I/Q for spectrum display (tapped before WDSP processing)
    void rawIqData(const QVector<float>& interleavedIQ);

    // Phase 3Q-6: forwarded from the active RadioConnection::frameReceived()
    // so TitleBar::ConnectionSegment can pulse its activity LED without
    // holding a reference to a connection that may be recreated on reconnect.
    // Re-emitted from wireConnectionSignals() for every new connection.
    void frameReceived();

    // Emitted when onBandButtonClicked short-circuits in a user-visible way
    // (locked slice, XVTR no-seed). MainWindow connects this to the status
    // bar so the user learns why their band click did nothing — prevents
    // silent failure. `reason` is a one-line human-readable message.
    // Issue #118.
    void bandClickIgnored(NereusSDR::Band band, QString reason);

    // Phase 3M-0 Task 6: Ganymede PA-trip live state.
    // Emitted whenever the trip latch changes (true = tripped, false = clear).
    // From Thetis Andromeda/Andromeda.cs:914-920 [v2.10.3.13]
    // (CATHandleAmplifierTripMessage). G8NJJ: handlers for Ganymede 500W PA protection.
    void paTrippedChanged(bool tripped);

    // Phase 3Q Task 10: auto-connect failure signals.
    //
    // autoConnectFailed — emitted when an auto-connect-on-launch attempt fails
    // (RadioConnection::connectFailed fires while m_autoConnectInProgress is set).
    // `mac`    — the saved-radio MAC key that was attempted.
    // `reason` — typed failure code (Timeout is the most common: radio unreachable).
    // MainWindow reacts by opening the ConnectionPanel and posting a status-bar message.
    void autoConnectFailed(const QString& mac, NereusSDR::ConnectFailure reason);

    // autoConnectAmbiguous — emitted when tryAutoReconnect finds more than one
    // saved radio with autoConnect = true. The most-recently-connected MAC wins;
    // MainWindow surfaces a one-time status-bar warning pointing to Manage Radios.
    // `count`      — total number of autoConnect-flagged radios.
    // `chosenMac`  — the MAC selected (most recently connected).
    void autoConnectAmbiguous(int count, const QString& chosenMac);

    // ── Phase 3M-1a Task G.4: TUNE refused ──────────────────────────────────
    // Emitted when setTune(true) is called but the power-on guard fires
    // (radio not connected / audio engine not active).
    // Cite: Thetis console.cs:29983-29991 [v2.10.3.13] — MessageBox "Power must be on".
    // NereusSDR equivalent: emit signal; UI reacts with a toast or status bar message.
    // Subscribers should uncheck the TUN button and display `reason` to the user.
    void tuneRefused(const QString& reason);

private slots:
    void onConnectionStateChanged(NereusSDR::ConnectionState state);

private:
    // Phase 3Q-1: drives the RadioModel-level connection state machine.
    // Guards against redundant transitions (no emit if state unchanged).
    void setConnectionState(ConnectionState s);

    // Pushes AlexController's per-band antenna state to the connection.
    // Full port of Thetis HPSDR/Alex.cs:310-413 UpdateAlexAntSelection.
    // Phase 3P-I-b (T6): adds isTx branch, Ext1/Ext2OnTx mapping, xvtrActive
    // gating, and rxOutOverride clamp. MOX coupling and Aries clamp deferred
    // to Phase 3M-1 (TX bring-up). isTx defaults to false so existing callers
    // are unaffected.
    //
    // Source: Thetis HPSDR/Alex.cs:310-413 [@501e3f5].
    void applyAlexAntennaForBand(NereusSDR::Band band, bool isTx = false);

    void wireConnectionSignals(int wdspInSize);
    void wireSliceSignals();
    void teardownConnection();

    // P1 full-parity §3.4 — per-sample PA telemetry handler.
    // Applies per-board ADC→watts scaling (scaleFwdPowerWatts /
    // scaleRevPowerWatts / scalePaVolts / scalePaAmps), routes the FWD
    // reading through CalibrationController::calibratedFwdPowerWatts()
    // (Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13]) and
    // publishes the calibrated values to RadioStatus + SwrProtectionController.
    //
    // Wired by wireConnectionSignals to RadioConnection::paTelemetryUpdated
    // via a thin forwarding lambda.  Extracted from that lambda so the test
    // hook handlePaTelemetryForTest can drive it directly without spinning
    // up the full wireConnectionSignals DSP-thread pipeline.
    void handlePaTelemetry(quint16 fwdRaw, quint16 revRaw, quint16 exciterRaw,
                           quint16 userAdc0Raw, quint16 userAdc1Raw,
                           quint16 supplyRaw);
    void loadSliceState(SliceModel* slice);
    void saveSliceState(SliceModel* slice);
    void scheduleSettingsSave();

public:
    // Force-run any pending coalesced slice save synchronously. Call this
    // from app-quit paths (MainWindow::closeEvent, aboutToQuit) and at the
    // top of teardownConnection() so the 500 ms debounce in
    // scheduleSettingsSave() can't swallow the user's last AF / step / freq
    // tweak when they immediately close the app. No-op when nothing's
    // pending. Idempotent — calling repeatedly is safe.
    void flushPendingSettingsSave();

private:
    // Sub-components (owned, main thread)
    RadioDiscovery*  m_discovery{nullptr};
    ReceiverManager* m_receiverManager{nullptr};
    AudioEngine*     m_audioEngine{nullptr};
    WdspEngine*      m_wdspEngine{nullptr};

    // Connection (owned, lives on m_connThread)
    RadioConnection* m_connection{nullptr};
    QThread*         m_connThread{nullptr};

    // I/Q DSP worker (owned, lives on m_dspThread). Fed by a queued
    // connection from ReceiverManager::iqDataForReceiver.
    RxDspWorker*     m_dspWorker{nullptr};
    QThread*         m_dspThread{nullptr};

    // Sub-models
    MeterModel    m_meterModel;
    TransmitModel m_transmitModel;

    // Phase 3M-0 Task 17: PA safety controllers.
    // Declared AFTER m_transmitModel so the ingest lambda can read
    // m_transmitModel.isTune() safely at any point post-construction.
    // SwrProtectionController and TxInhibitMonitor are QObject children
    // (parent=this); BandPlanGuard is a plain value class.
    safety::SwrProtectionController m_swrProt{this};
    safety::TxInhibitMonitor        m_txInhibit{this};
    safety::BandPlanGuard           m_bandPlan;

    // OC matrix — per-band × per-pin × {RX,TX} bit assignments.
    // Owned here so both OcOutputsTab UI and P1/P2 codec layer read
    // the same instance. MAC and load() are called on connect.
    // Phase 3P-D Task 3.
    OcMatrix      m_ocMatrix;

    // HL2 Options model — 9 HL2-specific behavior knobs.  Owned here
    // so the Hl2OptionsTab and (eventually) the P1 codec wire-format
    // layer share one instance.  MAC and load() are called on connect.
    // Phase 3L commit #9.
    Hl2OptionsModel m_hl2Options;

    // HL2 I/O board model — owns I2C queue and register mirror.
    // Shared with P1RadioConnection::setIoBoard() at connect time.
    // Phase 3P-E Task 2.
    IoBoardHl2    m_ioBoard;

    // HL2 LAN PHY bandwidth monitor — owns byte-rate + throttle state.
    // Pushed into P1RadioConnection::setBandwidthMonitor() at connect time.
    // Phase 3P-E Task 3.
    HermesLiteBandwidthMonitor m_bwMonitor;

    // Live PA telemetry + PTT state from status packets.
    // Phase 3P-H Task 2.
    RadioStatus m_radioStatus;

    // Settings hygiene — validated against caps at connect time.
    // Phase 3P-H Task 2.
    SettingsHygiene m_settingsHygiene;

    // Alex antenna controller — per-band TX/RX/RX-only port assignment.
    // MAC and load() are called on connect, matching OcMatrix ownership pattern.
    // Phase 3P-F Task 3.
    AlexController m_alexController;

    // Band-plan overlay manager — app-global, loaded once from Qt resources.
    // Phase 3G RX Epic sub-epic D.
    BandPlanManager m_bandPlanManager;

    // Apollo PA + ATU + LPF accessory state (present/filter/tuner enable bools).
    // MAC and load() are called on connect. Phase 3P-F Task 5a.
    ApolloController m_apolloController;

    // PennyLane external-control master toggle. Composes with OcMatrix (Phase 3P-D).
    // MAC and load() are called on connect. Phase 3P-F Task 5b.
    PennyLaneController m_pennyLaneController;

    // Calibration controller — HPSDR NCO correction factor, level offsets, PA current.
    // MAC and load() are called on connect. Backs CalibrationTab UI and
    // P2RadioConnection::hzToPhaseWord(). Phase 3P-G.
    CalibrationController m_calController;

    // Slices and panadapters (client-managed)
    QList<SliceModel*> m_slices;
    QList<PanadapterModel*> m_panadapters;
    SliceModel* m_activeSlice{nullptr};

    // View hooks (non-owning, set by MainWindow). Phase 3G-8 + 3G-9c.
    class SpectrumWidget*     m_spectrumWidget{nullptr};
    class FFTEngine*          m_fftEngine{nullptr};
    class ClarityController*  m_clarityController{nullptr};
    class StepAttenuatorController* m_stepAttController{nullptr};

    // Radio info
    QString m_name;
    QString m_model;
    QString m_version;
    HardwareProfile m_hardwareProfile;

    // Phase 3Q-1: RadioModel-level connection state machine.
    // Drives UI (TitleBar, ConnectionPanel, status bar, spectrum overlay).
    ConnectionState m_connectionState{ConnectionState::Disconnected};

    // Phase 3Q sub-PR-3: uptime tracking for NetworkDiagnosticsDialog.
    // Set to current time on Connected transition, cleared (default-constructed)
    // on any non-Connected state. connectionUptimeText() reads this.
    QDateTime m_connectionStartedAt;

    // Phase 3Q sub-PR-3: sample rate as last pushed to the wire.
    // Written from the wireSampleRateChanged path in connectToRadio().
    // connectionSampleRateHz() / connectionSampleRateText() read this.
    int m_connectionSampleRateHz{0};

    // Reconnect state
    RadioInfo m_lastRadioInfo;
    bool m_intentionalDisconnect{false};

    // I/Q accumulator and per-batch buffer sizes now live in
    // RxDspWorker (src/models/RxDspWorker.h) so the DSP thread owns
    // its own state and the main thread never touches it.

    // Per-slice-per-band persistence: tracks which band the VFO is currently
    // on so the coalesced scheduleSettingsSave() timer writes to the right
    // per-band slot. From Thetis console.cs:45312 handleBSFChange
    // [@501e3f5] — bandstack state is recalled via band-button
    // press, not via VFO tune, so this lambda only tracks; it does NOT
    // save or restore at the boundary.
    Band m_lastBand{Band::Band20m};

    // Settings save coalescing
    bool m_settingsSaveScheduled{false};
    // Phase 3P-I-a — dirty flag for AlexController persistence.
    // AlexController::antennaChanged can fire 14× during load(); the
    // flag + scheduleSettingsSave() timer coalesces them into a single
    // write at flush time. Set from the antennaChanged/blockTxChanged
    // handlers in wireSlice<Slot>, cleared by saveSliceState().
    bool m_alexControllerDirty{false};

#ifdef NEREUS_BUILD_TESTS
    bool     m_testCapsOverride{false};
    bool     m_testCapsHasAlex{false};
    bool     m_testCapsIsRxOnly{false};              // 3M-1a G.2: injected via setCapsRxOnlyForTest
    bool     m_testCapsHasMicJack{true};             // 3M-1b I.1: injected via setCapsHasMicJackForTest
    HPSDRHW  m_testCapsHw{HPSDRHW::Unknown};        // 3M-1b I.3: injected via setCapsHwForTest
#endif

    // Phase 3M-0 Task 6: Ganymede PA-trip live state.
    // From Thetis Andromeda/Andromeda.cs:914 [v2.10.3.13] (_ganymede_pa_issue volatile bool).
    // G8NJJ: handlers for Ganymede 500W PA protection
    bool m_paTripped{false};
    // From Thetis Andromeda/Andromeda.cs:854-866 [v2.10.3.13] (_ganymedePresent / GanymedePresent setter).
    bool m_ganymedePresent{false};

    // Phase 3Q Task 10: auto-connect failure path.
    // Set by MainWindow::tryAutoReconnect() before starting the probe;
    // cleared (to false / empty) on success OR failure so that a subsequent
    // user-initiated Connect does not trip the failure handler.
    bool    m_autoConnectInProgress{false};
    QString m_autoConnectChosenMac;

    // AGC bidirectional sync guard — prevents infinite feedback loop between
    // agcThresholdChanged and rfGainChanged handlers.
    // From Thetis console.cs:45960-46006 — bidirectional sync pattern.
    bool m_syncingAgc{false};

    // From Thetis v2.10.3.13 console.cs:46057 — tmrAutoAGC (500ms interval)
    QTimer* m_autoAgcTimer{nullptr};
    NoiseFloorTracker* m_noiseFloorTracker{nullptr};

    // ── 3M-1a G.4: TUN state save/restore ───────────────────────────────────
    // Fields that preserve pre-TUN state across the setTune(true)/setTune(false)
    // pair so TUN-off can restore exactly what TUN-on changed.
    //
    // m_savedTxDspMode: DSP mode before the CW→LSB/USB swap.
    //   Cite: Thetis console.cs:30042 [v2.10.3.13] — old_dsp_mode = ...CurrentDSPMode.
    //   Default USB (matches SliceModel default). Used only when old_dsp_mode
    //   was CWL or CWU; restored unconditionally on TUN-off.
    DSPMode m_savedTxDspMode{DSPMode::USB};
    //
    // m_savedPowerPct: power slider value (0-100) before the tune-power push.
    //   Cite: Thetis console.cs:30033 [v2.10.3.13] — PreviousPWR = ptbPWR.Value.
    //   //MW0LGE_22b  [original inline comment from console.cs:30033]
    //   Restored to the connection on TUN-off so the slider snaps back.
    // Default 100 matches TransmitModel::m_power default (TransmitModel.h).
    // G.4 fixup: changed from 50 (initial value mismatch with TransmitModel);
    // harmless after the cold-off guard in setTune(false) but kept for hygiene.
    int m_savedPowerPct{100};
    //
    // m_isTuning: True while TUN is engaged (between setTune(true) and
    //   setTune(false)).  Used as the idempotent guard at the top of
    //   setTune(false) — prevents a cold-off (no prior setTune(true)) from
    //   restoring stale saved state over the user's actual settings.  Also
    //   exported for H.3 UI polling.
    //   Cite: Thetis console.cs:30010 [v2.10.3.13] — _tuning = true.
    bool m_isTuning{false};

    // ── 3M-1a G.1: TX-side integration ──────────────────────────────────────
    // Master design §5.1.1; pre-code review §1.6 + §2.5.

    // MOX state machine — lives on the main thread (QTimers must be on
    // the event loop of the thread they fire on; RadioModel is main-thread).
    // Owned by RadioModel (Qt parent = this, set in constructor).
    // Wired: hardwareFlipped(bool) → onMoxHardwareFlipped(bool)
    //                              → StepAttenuatorController::onMoxHardwareFlipped
    //        txReady()             → m_txChannel->setRunning(true)
    //        txaFlushed()          → m_txChannel->setRunning(false)
    // From Thetis console.cs:29311-29678 [v2.10.3.13] — chkMOX_CheckedChanged2.
    //
    // Inline attribution tags preserved verbatim from the cited range:
    //[2.10.1.0]MW0LGE changed  [original inline comment from console.cs:29355]
    //MW0LGE [2.9.0.7]  [original inline comment from console.cs:29400]
    //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29561-29576]
    // Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE  [console.cs:29603]
    //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29647-29659]
    MoxController* m_moxController{nullptr};

    // Non-owning view of the WDSP TX channel (channel ID = 1 = WDSP.id(1, 0)).
    // WdspEngine owns the channel via m_txChannels. This pointer is valid only
    // after m_wdspEngine->initializedChanged fires and createTxChannel(1) is
    // called inside the initializedChanged lambda. null before that.
    // Callers must guard: if (m_txChannel) { ... }.
    // Thread safety: read only from the main thread. WDSP TX processing happens
    // on the DSP thread (m_dspThread), but the run-flag mutations called here
    // (setRunning / setTuneTone) are non-realtime control-path calls that are
    // safe to call from the main thread per the WDSP API contract.
    // From Thetis dsp.cs:926-944 [v2.10.3.13] — WDSP.id(1, 0) = channel 1.
    TxChannel* m_txChannel{nullptr};

    // TX mic source — strategy interface for silence (3M-1a) or real mic (3M-1b).
    // Owned by RadioModel via unique_ptr. NullMicSource for 3M-1a; replaced with
    // PcMicSource / RadioMicSource in 3M-1b per user preference and board caps.
    // Not a QObject — no thread affinity. pullSamples() is called from whatever
    // thread drives the TX I/Q production loop; for 3M-1a (TUNE carrier via WDSP
    // gen1 PostGen) it is never actually invoked, since gen1 overwrites the input.
    // Master design §5.2 (3M-1a NullMicSource; 3M-1b concrete sources).
    std::unique_ptr<TxMicRouter> m_txMicRouter;

    // 3M-1b L.1: concrete mic-source objects owned by RadioModel.
    // Constructed in connectToRadio() after m_connection is live (so
    // PcMicSource has AudioEngine and RadioMicSource has a valid connection
    // pointer). Destroyed in teardownConnection() in reverse-construction order
    // (composite first, then radio, then pc) to avoid dangling raw pointers
    // inside CompositeTxMicRouter.
    //
    // When null (before first connect or after disconnect):
    //   m_txChannel->setMicRouter() is called with nullptr via teardownConnection,
    //   matching the G.1 convention for nulling injection pointers on teardown.
    //
    // PcMicSource does NOT inherit QObject — no Qt parent. AudioEngine lifetime
    // is RadioModel's lifetime, so the non-owning AudioEngine* is always valid
    // while m_pcMicSource is alive.
    //
    // RadioMicSource IS a QObject but its parent is set to nullptr here because
    // RadioModel manages its lifetime via unique_ptr. This matches the convention
    // used by TxChannel (non-owning view, managed externally).
    //
    // Plan: 3M-1b Task L.1. Pre-code review §0.3 + master design §5.2.4.
    std::unique_ptr<PcMicSource>           m_pcMicSource;
    std::unique_ptr<RadioMicSource>        m_radioMicSource;
    std::unique_ptr<CompositeTxMicRouter>  m_compositeMicRouter;

    // ── 3M-1c Phase L: cross-cutting ownership ──────────────────────────────
    //
    // L.1 — MicProfileManager (chunk F).  QObject child of RadioModel so the
    // dtor cleans it up automatically.  Constructed once in the RadioModel
    // ctor; setMacAddress + load() are called per-connect inside
    // connectToRadio(); setMacAddress("") is called in teardownConnection so
    // mutators silently no-op while no radio is selected.
    MicProfileManager* m_micProfileMgr{nullptr};
    //
    // L.2 — TwoToneController (chunk I).  QObject child of RadioModel.
    // Construction-time deps that DON'T require a live connection
    // (TransmitModel, MoxController, SliceModel) are wired in the RadioModel
    // ctor; setTxChannel(...) is called inside the WDSP-init lambda once
    // m_txChannel is live.  setTxChannel(nullptr) is called in teardown.
    TwoToneController* m_twoToneController{nullptr};
    //
    // (Phase 3M-1c L.4 introduced a `std::unique_ptr<MicReBlocker>` here
    //  to bridge AudioEngine 720-sample emits to TxChannel 256-sample
    //  pushes.  The TX pump architecture redesign (2026-04-29) deleted
    //  MicReBlocker entirely; replaced with TxWorkerThread below.)
    //
    // 3M-1c TX pump architecture redesign — dedicated QThread that
    // drives the TX DSP pump off the main thread.  Mirrors Thetis's
    // `cm_main` worker-thread pattern (cmbuffs.c:151-168 [v2.10.3.13]):
    // QTimer-driven 5 ms tick, pulls 256-sample mic blocks via
    // AudioEngine::pullTxMic, calls TxChannel::driveOneTxBlock(samples,
    // 256).  Constructed inside the WDSP-init lambda once m_txChannel
    // is live; TxChannel is moveToThread'd to the worker before
    // startPump().  Teardown: stopPump() → quit() + wait() → move
    // TxChannel back to main thread → reset.  See plan §5.2.
    std::unique_ptr<TxWorkerThread> m_txWorker;

    // Phase 3M-1c TX pump v3 — TxMicSource (Thetis Inbound/cm_main port).
    // Constructed inside the WDSP-init lambda alongside TxWorkerThread,
    // wired into both the worker (consumer) and the connection (producer).
    // Teardown order: stopPump (worker exits) → micSource->stop (already
    // happens inside stopPump, but reset only after the worker is torn
    // down so the consumer side is fully disconnected).
    std::unique_ptr<class TxMicSource> m_txMicSource;
};

} // namespace NereusSDR
