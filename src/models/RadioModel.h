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
#include <QHash>
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
#include <array>      // std::array (HL2 temp averaging ring)
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
class VaxTxMicSource;  // VAX TX consumer (added 2026-05-06).
class CompositeTxMicRouter;
// 3M-1c L.1 / L.2: forward declarations for the MicProfileManager bank
// (chunk F) + the TwoToneController activation orchestrator (chunk I).
class MicProfileManager;
class TwoToneController;
// 3M-4 Task 7: PureSignal coordinator (cal lifecycle, MOX integration,
// auto-attention, polling, save/restore, two-tone wiring).
class PureSignal;
class PsccPump;
// Phase 4 Agent 4A of issue #167: PaProfileManager forward declaration.
// RadioModel owns the per-MAC PA gain profile bank (parallel to
// MicProfileManager); the active profile is passed by reference to
// TransmitModel::setPowerUsingTargetDbm at every drive-slider /
// TUNE / two-tone callsite.
class PaProfileManager;
// 3M-1c TX pump architecture redesign — TxWorkerThread.
class TxWorkerThread;
// Stage C2 filter preset editor — user-override layer over Thetis defaults.
class FilterPresetStore;

// Phase 3J-2 H2: spot-system forward declarations. RadioModel owns the
// seven spot-ingest clients (DxCluster, RBN, WSJT-X, SpotCollector,
// POTA, FreeDV Reporter, PSK Reporter), the three view models
// (SpotModel, FreeDVStationModel, RxDecodeModel), and the
// DxccColorProvider. Each client's spotReceived(DxSpot) signal lands
// in a per-source adapter slot that builds the QMap<QString,QString>
// kvs SpotModel::applySpotStatus expects.
class DxClusterClient;
class WsjtxClient;
class SpotCollectorClient;
class PotaClient;
class FreeDVReporterClient;
class PskReporterClient;
class DxccColorProvider;
class SpotModel;
class SpotTableModel;
class FreeDVStationModel;
class RxDecodeModel;
struct DxSpot;
struct FreeDVStation;

// Phase 3R Task I5: forward declaration for the RadeChannel codec wrapper.
// RadioModel does not own the channel (J2 / J3 create one per slice as
// mode flips to RADE), but exposes wireRadeChannel(sliceId, channel, slice)
// to attach the channel's snrChanged / syncChanged / rxTextDecoded
// signals into the slot graph.
class RadeChannel;

// Phase 3R K-bench forward decl: Resampler is used by RadioModel to
// upsample RadeChannel's 24 kHz baseband output to the radio's TX
// I/Q wire rate before m_connection->sendTxIq.  Lives in core/Resampler.h.
class Resampler;

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
    // Phase 4 Agent 4A of issue #167 — also propagates to TransmitModel
    // so the ATT-on-TX-on-power-change safety gate inside
    // setPowerUsingTargetDbm can call ctrl->setAttOnTxValue(31) when the
    // gate fires (Thetis console.cs:46740-46748 [v2.10.3.13] [2.10.3.5]MW0LGE).
    // Implementation in RadioModel.cpp.
    void setStepAttController(class StepAttenuatorController* c);
    NoiseFloorTracker* noiseFloorTracker() const { return m_noiseFloorTracker; }
    void setNoiseFloorTracker(NoiseFloorTracker* t) { m_noiseFloorTracker = t; }
    // Task 3.1: MeterPoller view hook so MultimeterPage can apply live
    // polling-interval and averaging-window changes without a MainWindow
    // round-trip.  Non-owning; MainWindow calls setMeterPoller() after
    // creating MeterPoller (see MainWindow.cpp construction block).
    class MeterPoller* meterPoller() const { return m_meterPoller; }
    void setMeterPoller(class MeterPoller* p) { m_meterPoller = p; }
    // Task 3.2: ContainerManager view hook so MultimeterPage can broadcast
    // unit-mode changes to all live MeterItems via forEachMeterItem().
    // Non-owning; MainWindow calls setContainerManager() after creating
    // ContainerManager (same pattern as setMeterPoller above).
    class ContainerManager* containerManager() const { return m_containerManager; }
    void setContainerManager(class ContainerManager* cm) { m_containerManager = cm; }
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

    // Phase 4 Agent 4A of issue #167: expose PaProfileManager so the future
    // PaGainByBandPage (Phase 6 Agent 6A) and tests can hand the per-MAC
    // profile bank around.  Non-owning; lifetime is RadioModel's lifetime.
    // Constructed once in the RadioModel ctor; setMacAddress + load() are
    // called per-connect inside connectToRadio() (mirrors MicProfileManager
    // wiring at lines ~1191).  Active profile is passed by reference to
    // TransmitModel::setPowerUsingTargetDbm at every callsite.
    PaProfileManager* paProfileManager() const { return m_paProfileManager; }

    // 3M-1c Phase L.2: expose TwoToneController so MainWindow can hand it to
    // TxApplet (J.2 setter) for the 2-TONE button + status mirror.
    // Non-owning; lifetime is RadioModel's lifetime.
    TwoToneController* twoToneController() const { return m_twoToneController; }

    // 3M-4 Task 7: expose PureSignal coordinator so PsForm, PureSignalApplet,
    // TxApplet [PS-A], and PsaIndicatorWidget can subscribe to its
    // Q_PROPERTY signals (cal lifecycle, MOX integration, FB level updates).
    // Non-owning view; RadioModel owns via std::unique_ptr.
    // Cited in design §8 + plan §Task 7.  Created lazily inside the WDSP-init
    // lambda once m_txChannel + m_psFeedbackChannel are live.  Returns nullptr
    // before that point (and after teardown).
    PureSignal* pureSignal() const { return m_pureSignal.get(); }

    // Stage C2: expose FilterPresetStore so RxApplet, VfoWidget, and
    // FilterPresetsSetupPage can read/write user-customised presets.
    // Constructed once in RadioModel ctor; lifetime is RadioModel's lifetime.
    FilterPresetStore* filterPresetStore() const { return m_filterPresetStore; }

    // ── Phase 3J-2 H2: spot-system accessors ────────────────────────────────
    // RadioModel owns the seven spot-ingest clients, three view models, and
    // the DxccColorProvider as std::unique_ptr members. Each accessor returns
    // a non-owning pointer; lifetime is RadioModel's lifetime. MainWindow
    // (H1) consumes these to instantiate SpotHubDialog + FreeDVReporterDialog
    // with shared model pointers, and the M3 follow-up task wires the
    // `<Source>/AutoConnect` AppSettings keys to actually start each client.
    //
    // Constructed in RadioModel ctor with identity / endpoint defaults from
    // AppSettings; startConnection() is NOT called at construction time.
    SpotModel*            spotModel()           const { return m_spotModel.get(); }
    // 2026-05-12 bench fix: moved from SpotHubDialog ownership so the
    // table stays populated from app start regardless of whether the
    // dialog is open.  Spots from auto-connected sources were
    // previously dropped on the floor until the user opened Tools →
    // Spot Hub for the first time (the SpotTableModel didn't exist
    // before then), forcing a manual disconnect+reconnect to repopulate.
    SpotTableModel*       spotTableModel()      const { return m_spotTableModel.get(); }
    FreeDVStationModel*   freeDvStationModel()  const { return m_freeDvStationModel.get(); }
    RxDecodeModel*        rxDecodeModel()       const { return m_rxDecodeModel.get(); }
    DxccColorProvider*    dxccColorProvider()   const { return m_dxccColorProvider.get(); }
    DxClusterClient*      dxCluster()           const { return m_dxCluster.get(); }
    DxClusterClient*      rbn()                 const { return m_rbn.get(); }
    WsjtxClient*          wsjtx()               const { return m_wsjtx.get(); }
    SpotCollectorClient*  spotCollector()       const { return m_spotCollector.get(); }
    PotaClient*           pota()                const { return m_pota.get(); }
    FreeDVReporterClient* freeDvReporter()      const { return m_freeDvReporter.get(); }
    PskReporterClient*    pskReporter()         const { return m_pskReporter.get(); }

    // ── Phase 3J-2 + 3R M3: spot-client auto-start state restore ────────────
    //
    // Reads each per-source AutoConnect / AutoStart key from AppSettings
    // and, when True, calls the corresponding start method with the
    // persisted identity / port / interval params. Designed to be called
    // once at launch from MainWindow (sibling to tryAutoReconnect for the
    // radio connection itself).
    //
    // Keys consulted (all flat PascalCase, matching SpotHubDialog F2):
    //   DxClusterAutoConnect   -> connectToCluster(host, port, callsign)
    //   RbnAutoConnect         -> same shape, different host default
    //   WsjtxAutoStart         -> startListening(address, port)
    //   SpotCollectorAutoStart -> startListening(port)
    //   PotaAutoStart          -> startPolling(intervalSec)
    //   FreeDvAutoStart        -> startConnection() (identity / URL
    //                              already plumbed by RadioModel ctor)
    //   PskReporterAutoStart   -> no-op (PSK Reporter is send-only)
    //
    // Safe to call multiple times. Each client's start method already
    // guards against double-start.
    void restoreSpotClientAutoStartState();

    // ── Phase 3R Task I5: RadeChannel slot-graph wiring ─────────────────────
    //
    // wireRadeChannel attaches a freshly-created RadeChannel into RadioModel's
    // slot graph. Phase J calls this from createRadeChannel (J2) / mode-swap
    // (J3) after constructing the channel.
    //
    // The channel's per-channel signals (snrChanged / syncChanged /
    // rxTextDecoded) do not carry a slice ID; the wiring adapts each through
    // a captured-sliceId lambda so the receiving RadioModel slots know which
    // slice to apply the event to.
    //
    // Routing:
    //   RadeChannel::snrChanged       -> onRadeSnrChanged     -> SliceModel::setSnrDb
    //                                                         -> radeSnrChanged re-emit
    //   RadeChannel::syncChanged      -> onRadeSyncChanged    -> radeSyncChanged
    //                                                            (only on transition)
    //   RadeChannel::rxTextDecoded    -> onRadeTextDecoded    -> RxDecodeModel::addDecode
    //
    // Null channel or null slice is a safe no-op.
    void wireRadeChannel(int sliceId, NereusSDR::RadeChannel* channel,
                         NereusSDR::SliceModel* slice);

    // Reads the latest RADE sync state for the given slice ID. Returns
    // false when the slice has no recorded sync state (e.g. RADE was
    // never wired for that slice). Surface for the future Phase L
    // RadeApplet status indicator + status-bar SYNC badge.
    bool radeSynced(int sliceId) const;

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

    // Task 1.6 — Sample-rate live-apply coordinator.
    //
    // Changes the sample rate of the active radio connection without
    // disconnecting.  The sequence is:
    //   1. Quiesce the DSP worker (stop I/Q feed into RxDspWorker).
    //   2. Notify AudioEngine of the impending change (pauseInput hook).
    //   3. Rebuild all WDSP channels with the new rate.
    //   4. Update the hardware:
    //      - P1: stop + re-arm EP6 sender with new rate + start.
    //      - P2: send updated CmdRx/CmdTx (already contains new rate).
    //   5. Update RxDspWorker buffer sizes to match the new rate.
    //   6. Reconnect the I/Q feed into RxDspWorker (resume DSP worker).
    //   7. Notify AudioEngine (resumeInput hook).
    //   8. Persist the new rate, update m_connectionSampleRateHz, and
    //      emit wireSampleRateChanged(newRateHz).
    //
    // Returns elapsed milliseconds for the whole operation.  Returns -1
    // if no connection is active or WDSP is not initialized.
    //
    // Must be called on the main thread.
    //
    // Caveats:
    //   - P1 restart is untested on live hardware (design §5C risk note).
    //     A brief audio dropout (one buffer interval) is expected on P1;
    //     P2 is glitch-free in practice.
    //   - TxWorkerThread is stopped before TX channel rebuild and restarted
    //     after.  If MOX is asserted during the change, MOX is silently
    //     dropped.  Callers should ensure MOX is off before calling.
    //   - dspChangeMeasured(qint64) signal (Task 1.8) is emitted on completion.
    //     The elapsed time is also returned synchronously.
    qint64 setSampleRateLive(int newRateHz);

    // Task 1.7 — Active-RX-count live-apply coordinator.
    //
    // Enables or disables the secondary receiver (RX2) without disconnecting.
    // The sequence mirrors setSampleRateLive() (Task 1.6):
    //   1. Quiesce the DSP worker (stop I/Q feed into RxDspWorker).
    //   2. Pause AudioEngine.
    //   3. Create/destroy WDSP RX channels to match the new count.
    //   4. Update ReceiverManager DDC mapping (activate/deactivate receivers).
    //   5. Update the hardware:
    //      - P1: update m_activeRxCount in P1RadioConnection so the next
    //            bank-0 C&C frame encodes the correct nrx bits, then issue
    //            a stop+prime+start cycle so the radio re-arms EP6 with the
    //            new per-frame slot count.  The static parseEp6Frame already
    //            accepts numRx as a parameter; m_activeRxCount in the instance
    //            is used on every parse call, so updating it is sufficient —
    //            no MetisFrameParser rework required (MetisFrameParser does not
    //            exist as a separate class; parsing is in P1RadioConnection).
    //      - P2: setActiveReceiverCount() already sends sendCmdRx() when
    //            running, which updates DDC enable bits in the hardware.
    //   6. Reconnect DSP worker I/Q feed (resume DSP worker).
    //   7. Resume AudioEngine.
    //   8. Persist the new count per-MAC, update m_connectionActiveRxCount, and
    //      emit activeRxCountChanged(newCount).
    //
    // Returns elapsed milliseconds.  Returns -1 if no connection is active or
    // WDSP is not initialized.  Returns 0 if newCount == current count
    // (idempotent).
    //
    // Must be called on the main thread.
    //
    // Note on P1 MetisFrameParser: the plan (design §5D) flagged a potential
    // need to rework MetisFrameParser to handle mid-stream RX-count changes.
    // Investigation found that no separate MetisFrameParser class exists —
    // EP6 parsing is in P1RadioConnection::parseEp6Frame(frame, numRx, ...)
    // which accepts numRx as a parameter on every call and reads
    // m_activeRxCount from the instance.  There is no per-receiver cache to
    // invalidate.  Strategy A (full live-apply, both protocols) is therefore
    // possible without any parser rework.
    qint64 setActiveRxCountLive(int newCount);

    // Returns the active-RX count last pushed to hardware (0 when disconnected).
    int connectionActiveRxCount() const { return m_connectionActiveRxCount; }

    // Task 4.2 — Per-mode DSP-Options live-apply (called from DspOptionsPage).
    //
    // Reads the per-mode AppSettings keys for forMode and calls
    // RxChannel::onModeChanged() (+ TxChannel::onModeChanged()) if WDSP is
    // initialized and a channel exists.  No-op if disconnected or uninitialized.
    //
    // Emits dspChangeMeasured(elapsedMs) if a WDSP channel rebuild occurred.
    //
    // DspOptionsPage calls this when a combo changes and the combo's mode
    // matches the current slice mode (design Section 4B: live-apply if same
    // mode, persist-only otherwise — applies on next mode-switch).
    //
    // Must be called on the main thread.
    void rebuildDspOptionsForMode(DSPMode forMode);

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
    // B6 — XIT: allow tests to trigger wireSliceSignals() directly after
    // injecting a mock connection, mirroring what wireConnectionSignals() does
    // when a real radio connects.
    void wireSliceSignalsForTest() { wireSliceSignals(); }
    // Issue #182 — invoke the mic_ptt_disabled wiring helper directly so
    // tst_radio_model_mic_ptt_wire can verify the signal/slot bind + prime
    // path without spinning up the full wireConnectionSignals pipeline.
    void wireMicPttDisabledForTest() { connectMicPttDisabledSignal(); }
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
    // Issue #177 — drive the tune-off settle delay synchronously in tests.
    // Production default is 100 ms (mirrors `await Task.Delay(100)` at Thetis
    // console.cs:30107 [v2.10.3.13]).  Setting this to 0 makes completeTuneOff
    // schedule on the next event loop iteration, so QCoreApplication::processEvents
    // can drive the deferred completion synchronously.
    void setTuneOffSettleMsForTest(int ms) noexcept { m_tuneOffSettleMs = ms; }
    bool tuneOffPendingForTest()          const noexcept { return m_pendingTuneOff; }

    // TUN state, exported for H.3 UI polling and for issue #177 tests.
    // True between setTune(true) and the completion of the corresponding
    // setTune(false) → completeTuneOff() chain.
    // Cite: Thetis console.cs:30010 [v2.10.3.13] — _tuning = true (read by
    // many UI/meter/PA paths in console.cs).
    bool isTune() const noexcept { return m_isTuning; }
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
        // The test seam injects telemetry as if the radio were
        // transmitting — bypass the MOX gate that handlePaTelemetry
        // applies in production (which forces TX-domain readings to 0
        // when MoxController state != Tx so late samples don't refill
        // the meters after un-key).  Tests calling this seam mean
        // "behave as if in TX"; flipping the flag lets the routing
        // pipeline run exactly as it would on a live transmit sample.
        m_forceTxForTest = true;
        handlePaTelemetry(fwdRaw, revRaw, exciterRaw,
                          userAdc0Raw, userAdc1Raw, supplyRaw);
        m_forceTxForTest = false;
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

    // Phase 4 Agent 4A of issue #167 — test seam to inject a non-owning
    // TxChannel pointer so the drive-slider / TUNE rewrite tests can spy on
    // setTxFixedGain() without standing up the full WdspEngine pipeline.
    // Production code never calls this — m_txChannel is wired by the
    // WDSP-init lambda inside connectToRadio() (see "createTxChannel(1)"
    // around RadioModel.cpp:1514).
    void injectTxChannelForTest(class TxChannel* ch) { m_txChannel = ch; }

    // Phase 4 Agent 4A of issue #167 — test seam to inject the HPSDRModel
    // hardware profile directly. setBoardForTest(HPSDRHW::OrionMKII) maps
    // through defaultModelForBoard() to ORIONMKII (the *first* model
    // matching that board), but K2GX's regression specifically pins
    // ANAN8000D values; this seam lets tests pick the exact HPSDRModel.
    //
    // v0.4.1 hotfix: routes through applyHpsdrModel() so tests get the
    // same TransmitModel + ReceiverManager fan-out as production
    // connectToRadio.  Without this, the test seam drifts from
    // production and tests miss regressions in the ReceiverManager
    // push (root cause of the v0.4.0 PureSignal-broken-on-Hermes bug).
    void setHpsdrModelForTest(HPSDRModel m) {
        applyHpsdrModel(m);
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

    // ── Phase 3J-1 follow-up: TCI Q_INVOKABLE shims (bench wire-up) ──────────
    //
    // TciProtocol calls into RadioModel by *method name string* via
    // QMetaObject::invokeMethod(...).  For Qt to resolve those names, the
    // methods must be marked Q_INVOKABLE (or be slots, or Q_PROPERTY
    // READ/WRITE — Q_INVOKABLE is the explicit choice here).
    //
    // Phase 6 wired the call sites in TciProtocol.cpp but added the matching
    // Q_INVOKABLE shims only on TestMockRadioModel.  The matrix runner asserts
    // byte-for-byte parity against the mock, so all 80+ matrix rows pass — but
    // when a real client (WSJT-X / ESDR3 / SunSDR) connects against the live
    // RadioModel, every set/query silently no-ops because the meta-object has
    // no entry under those names.
    //
    // This block adds the WSJT-X minimum: PTT (trx), VFO (vfo), mode
    // (modulation), and split_enable.  Subsequent commits will fill the long
    // tail (DSP toggles, AGC, SQL, RIT/XIT, balance, audio configs,
    // calibration).
    //
    // Signatures MUST match the Q_ARG / Q_RETURN_ARG types at each call site
    // in src/core/TciProtocol.cpp:
    //   handleVfo       (line 1086 set / 1104 query)
    //   handleModulation(line 1236 query / 1275 set)
    //   handleTrx       (line 1365 set  / 1380 query)
    //   handleSplit     (line 1416 set  / 1430 query)
    //
    // Connection type: TciProtocol invokes with Qt::DirectConnection (test
    // thread) but the runtime TciServer pumps from the main thread (same
    // thread as RadioModel), so DirectConnection is fine for production too.

    /// Set MOX (PTT).  Routes to MoxController if installed, else
    /// TransmitModel.  Mirrors AppMod::PttSource:TCI in Thetis.
    /// From Thetis TCIServer.cs:3454-3500 [v2.10.3.13] — handleTrx, set path.
    Q_INVOKABLE void setMox(bool on);

    /// Query MOX (PTT).  Returns the current MOX latch state.
    /// From Thetis TCIServer.cs:3555-3558 [v2.10.3.13] — sendMOX.
    Q_INVOKABLE bool mox() const;

    /// Set VFO frequency for receiver `rx`, channel `chan` (0=A, 1=B).
    /// NereusSDR has one frequency per slice; `chan==1` (VFO B) is silently
    /// ignored because the second VFO concept maps to a separate slice, not
    /// to a per-slice secondary frequency.
    /// From Thetis TCIServer.cs:3719-3793 [v2.10.3.13] — handleVfo, set path.
    Q_INVOKABLE void setVfoHz(int rx, int chan, qint64 hz);

    /// Query VFO frequency for receiver `rx`, channel `chan`.  Returns
    /// the slice frequency regardless of `chan` (see setVfoHz note).
    /// From Thetis TCIServer.cs:3793-3833 [v2.10.3.13] — handleVfo, query path.
    Q_INVOKABLE qint64 vfoHz(int rx, int chan) const;

    /// Set demodulation mode for receiver `rx`.  `modeStr` is uppercase
    /// (LSB, USB, CWL, CWU, AM, FM, DIGL, DIGU, etc.).
    /// CWbecomesCWUabove10mhz transform from [2.10.3.6]MW0LGE fixes #365
    /// (TCIServer.cs:3868-3895) is DEFERRED — `cw` maps to CWL until VFOATX /
    /// VFOBTX state plumbing arrives.
    //[2.10.3.6]MW0LGE fixes #365  [original inline tag from TCIServer.cs:3868]
    /// From Thetis TCIServer.cs:3835-3942 [v2.10.3.13] — handleModulation, set.
    Q_INVOKABLE void setMode(int rx, QString modeStr);

    /// Query demodulation mode for receiver `rx`.  Returns uppercase name.
    /// From Thetis TCIServer.cs:3942-3954 [v2.10.3.13] — handleModulation, query.
    Q_INVOKABLE QString mode(int rx) const;

    /// Set split-TX enable for receiver `rx`.  NereusSDR does not yet
    /// implement per-slice split; this shim accepts the value, broadcasts the
    /// confirmation notification (handled by TciProtocol), but does not yet
    /// change radio state.  WSJT-X "Split Operation: None/Fake It" is the
    /// supported configuration until proper split lands in Phase 3F.
    /// From Thetis TCIServer.cs:3091-3127 [v2.10.3.13] — handleSplitEnableMessage.
    Q_INVOKABLE void setSplit(int rx, bool on);

    /// Query split-TX state.  Currently returns false (see setSplit note).
    Q_INVOKABLE bool split(int rx) const;

    // ── Phase 3J-1 closeout Item 3 (2026-05-12): TCI Q_INVOKABLE long tail ──
    //
    // ~56 additional shims that TciProtocol calls via QMetaObject::invokeMethod.
    // Without these the matrix test (against TestMockRadioModel) passes but
    // ESDR3 / N1MM / Log4OM / SunSDR-native clients hit silent no-ops on the
    // production RadioModel.  Each shim is documented with what it does
    // semantically (mock parity) and what underlying state it writes (real
    // model side).  Stubs are explicitly labeled "stub until <feature> lands".

    // VFO lock — routes to SliceModel::locked.  TCI carries two-chan-per-rx
    // semantics from Thetis (VFOALock + VFOBLock); NereusSDR collapses them
    // because per-slice VFO B isn't modeled.  Both chan==0 and chan==1
    // read/write the same slice-level locked flag.
    Q_INVOKABLE void setVfoLock(int rx, int chan, bool locked);
    Q_INVOKABLE bool vfoLock(int rx, int chan) const;
    Q_INVOKABLE void setLock(int rx, bool locked);
    Q_INVOKABLE bool lock(int rx) const;

    // Mute — routes to SliceModel::muted (per-slice) and a new RadioModel
    // member m_globalMute (global).  Global mute is broadcast-only state;
    // when global mute is on, all slices' audio is suppressed downstream.
    Q_INVOKABLE void setGlobalMute(bool on);
    Q_INVOKABLE bool globalMute() const;
    Q_INVOKABLE void setRxMute(int rx, bool on);
    Q_INVOKABLE bool rxMute(int rx) const;

    // Filter — routes to SliceModel::filterLow / filterHigh.  setFilterBand
    // sets BOTH cutoffs atomically.
    Q_INVOKABLE void setFilterBand(int rx, int lowHz, int highHz);
    Q_INVOKABLE int  filterLow(int rx) const;
    Q_INVOKABLE int  filterHigh(int rx) const;

    // AGC mode — routes to SliceModel::agcMode (enum).  Mock uses uppercase
    // strings: "OFF" / "LONG" / "SLOW" / "MED" / "FAST" / "CUSTOM".
    Q_INVOKABLE void    setAgcMode(int rx, const QString& mode);
    Q_INVOKABLE QString agcMode(int rx) const;

    // AGC gain (threshold) — routes to SliceModel::agcThreshold (-20..120).
    Q_INVOKABLE void setAgcGain(int rx, int gain);
    Q_INVOKABLE int  agcGain(int rx) const;

    // Squelch — routes to SliceModel::ssqlEnabled / ssqlThresh.  TCI level
    // is int (-140..0 dBm); SliceModel::ssqlThresh is double in same units.
    Q_INVOKABLE void setSqlEnable(int rx, bool on);
    Q_INVOKABLE bool sqlEnable(int rx) const;
    Q_INVOKABLE void setSqlLevel(int rx, int level);
    Q_INVOKABLE int  sqlLevel(int rx) const;

    // RIT / XIT — routes to SliceModel::ritEnabled/ritHz/xitEnabled/xitHz on
    // the active slice.  Thetis treats these as radio-global (single VFO
    // pair); NereusSDR collapses to active-slice for symmetry with mode/mox.
    Q_INVOKABLE void setRitEnable(bool on);
    Q_INVOKABLE bool ritEnable() const;
    Q_INVOKABLE void setRitOffset(int hz);
    Q_INVOKABLE int  ritOffset() const;
    Q_INVOKABLE void setXitEnable(bool on);
    Q_INVOKABLE bool xitEnable() const;
    Q_INVOKABLE void setXitOffset(int hz);
    Q_INVOKABLE int  xitOffset() const;

    // RX balance / audio pan — routes to SliceModel::audioPan.  TCI uses
    // double in [-1, 1]; SliceModel matches.  chan arg is ignored (single
    // pan per slice, not per VFO).
    Q_INVOKABLE void   setRxBalance(int rx, int chan, double balance);
    Q_INVOKABLE double rxBalance(int rx, int chan) const;

    // CTUN — per-slice stub (no CTUN model state yet; spectrum-widget owns
    // the interaction mode).  Stored in m_tciStubRxCtun, set-and-read only
    // until a CTUN model lands.
    Q_INVOKABLE void setRxCtun(int rx, bool on);
    Q_INVOKABLE bool rxCtun(int rx) const;

    // ── DSP toggles (NbMode + activeNr based) ────────────────────────────
    // setRxNb: maps bool to NbMode (true -> last-non-None mode; false -> None).
    Q_INVOKABLE void setRxNb(int rx, bool on);
    Q_INVOKABLE bool rxNb(int rx) const;
    // setRxNr: maps (bool, int nrIndex) to activeNr enum slot.
    Q_INVOKABLE void setRxNr(int rx, bool on, int nrIndex);
    Q_INVOKABLE bool rxNr(int rx) const;
    Q_INVOKABLE int  rxNrIndex(int rx) const;
    // setRxAnf: maps bool to "activeNr == ANF" semantics (ANF is one of the
    // NrSlot values).
    Q_INVOKABLE void setRxAnf(int rx, bool on);
    Q_INVOKABLE bool rxAnf(int rx) const;

    // ── Stub categories: SliceModel doesn't expose these as Q_PROPERTYs yet ─
    // Each stub stores the requested value in a small per-slice array so
    // round-trip (set then get) returns the operator's last value.  Real
    // wiring to WDSP comes when the underlying feature lands.
    Q_INVOKABLE void setRxBin(int rx, bool on);
    Q_INVOKABLE bool rxBin(int rx) const;
    Q_INVOKABLE void setRxApf(int rx, bool on);
    Q_INVOKABLE bool rxApf(int rx) const;
    Q_INVOKABLE void setRxNf(int rx, bool on);
    Q_INVOKABLE bool rxNf(int rx) const;
    Q_INVOKABLE void setRxEnable(int rx, bool on);
    Q_INVOKABLE bool rxEnable(int rx) const;

    // ── Volume (linear int) ──────────────────────────────────────────────
    // setAfLinear: TCI sends 0..32767; we store and let the audio path read.
    // monLinear: TX monitor volume; same range.  These are NereusSDR-global
    // (not per-slice) -- matches Thetis console.cs handleAFVolume.
    Q_INVOKABLE void setAfLinear(int v);
    Q_INVOKABLE int  afLinear() const;
    Q_INVOKABLE void setMonLinear(int v);
    Q_INVOKABLE int  monLinear() const;

    // ── IQ sample rate ───────────────────────────────────────────────────
    // setIqSampleRate: TCI echoes the rate back per Thetis pattern; the
    // radio hardware doesn't actually change rate from a TCI command.
    Q_INVOKABLE void setIqSampleRate(int sr);
    Q_INVOKABLE int  iqSampleRate() const;

    // ── Audio stream config ──────────────────────────────────────────────
    // Per-client state lives in TciClientSession; TciServer intercepts these
    // commands BEFORE the invokeMethod fires, so the production shims here
    // are dead-code parity with the mock.  Kept for symmetry + matrix-test
    // compatibility.  They store last-seen value but no other side effect.
    Q_INVOKABLE void    setAudioSampleRate(int sr);
    Q_INVOKABLE int     audioSampleRate() const;
    Q_INVOKABLE void    setAudioStreamSampleType(const QString& t);
    Q_INVOKABLE QString audioStreamSampleType() const;
    Q_INVOKABLE void    setAudioStreamChannels(int n);
    Q_INVOKABLE int     audioStreamChannels() const;
    Q_INVOKABLE void    setAudioStreamSamples(int n);
    Q_INVOKABLE int     audioStreamSamples() const;

    // ── TX profile (via MicProfileManager) ───────────────────────────────
    // setTxProfile: name lookup against the operator's profile library.
    // txProfilesList: enumerates installed profiles.
    Q_INVOKABLE void        setTxProfile(const QString& name);
    Q_INVOKABLE QString     txProfile() const;
    Q_INVOKABLE QStringList txProfilesList() const;

    // ── Calibration (getter-only stubs returning 0.0) ────────────────────
    // No calibration model in RadioModel yet.  Mock semantics: set/get pair;
    // production has setters absent (caller side never sets these), so
    // getters return 0.0.  Real calibration data would live in a future
    // CalibrationModel + per-slice persistence.
    Q_INVOKABLE double calibrationMeter(int rx) const;
    Q_INVOKABLE double calibrationDisplay(int rx) const;
    Q_INVOKABLE double calibrationXvtr(int rx) const;
    Q_INVOKABLE double calibrationSixMeter(int rx) const;
    Q_INVOKABLE double calibrationTxDisplay(int rx) const;

    // ── Phase 3R Task I5: RadeChannel signal-graph slots ────────────────────
    //
    // Public slots so Qt's auto-connection queues them correctly when the
    // emitting RadeChannel ever moves to a worker thread (J2/J3 currently
    // keep it on the main thread; the public-slot declaration is forward-
    // safe regardless).
    //
    // All three accept an int sliceId so a single RadioModel can route
    // multiple per-slice RadeChannels through one set of slots. The
    // wireRadeChannel helper captures the slice ID in a lambda at wire
    // time and adapts the channel's per-channel signals into these.

    // Forwards a decoded callsign (+ optional grid) into RxDecodeModel as
    // a single decode row. mode="RADE", source="rade_text". Grid is
    // appended to the payload only when non-empty (I4 Option B does not
    // carry grid; the field is present for future text-channel revs).
    void onRadeTextDecoded(int sliceId, const QString& callsign,
                           const QString& grid);

    // Tracks per-slice RADE decoder sync state. Emits radeSyncChanged
    // only on actual transitions: repeated identical values collapse to
    // a single emit (de-duplication keeps the future status-bar
    // indicator from flickering on repeated sync=true reports from the
    // codec).
    void onRadeSyncChanged(int sliceId, bool synced);

    // Forwards the codec's SNR estimate to the slice's snrDb property
    // (D5 added SliceModel::setSnrDb). Cast-up float->double at the
    // boundary; the slice setter no-ops on identical numeric values
    // so repeated identical SNR updates do not spam UI repaint.
    void onRadeSnrChanged(int sliceId, float snrDb);

    // 2026-05-12 bench: throttled FreeDV Reporter freq publish.  See
    // m_freedvFreqDwellTimer member declaration for the policy.  Called
    // from the slice.frequencyChanged subscriber at line ~4945 in
    // RadioModel.cpp.  Caller passes the new VFO Hz; this method either
    // publishes immediately (initial baseline / band-jump fast-path /
    // MOX force) or restarts the dwell timer for a deferred publish.
    void publishFreedvFrequencyDwelled(quint64 hz);
    // Force-publish the current pending freq right now and reset the
    // dwell.  Called from MoxController::txAboutToBegin so a TX engage
    // never leaves the reporter showing a stale freq.
    void flushFreedvFrequencyDwell();

    // Phase 3J-1 closeout follow-up (2026-05-12): FreeDV Reporter is a
    // dashboard for FreeDV / RADE operators.  Our station should be
    // visible there only when we're actually using RADE (RADE_U or
    // RADE_L) -- not when we're on SSB / WSJT-X / CW.  Mirrors
    // freedv-gui's connect-and-hide-when-not-on-FreeDV behavior; we stay
    // connected so we can still see other FreeDV stations and report
    // their decodes via sendRxReport, but our own row stays hidden on
    // the public dashboard unless we're TX-capable in RADE.
    //
    // Wired on:
    //   - active slice's dspModeChanged (mode switch during operation)
    //   - active slice swap (different slice becomes active)
    //   - FreeDVReporterClient::connected (initial state after connect)
    void updateFreedvReporterVisibility();

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
    // Task 1.7: emitted after setActiveRxCountLive() successfully applies
    // the new receiver count to both hardware and WDSP channels.
    void activeRxCountChanged(int newCount);
    // Fires on each transition to Connected with the RadioInfo of the live
    // connection. HardwarePage (Phase 3I) listens to this to repopulate
    // sub-tabs with per-radio fields.
    void currentRadioChanged(const NereusSDR::RadioInfo& info);

    // ── Phase 3M-4 Task 13: late-bound PureSignal coordinator handoff ──────
    // Fires when m_pureSignal is created (post-WDSP-init) or torn down.
    // Carries the live PureSignal* (nullptr on disconnect).  Subscribers
    // (PureSignalApplet, TxApplet [PS-A]) re-wire their controls when the
    // coordinator becomes available.
    //
    // The coordinator does not exist at MainWindow construction time
    // (RadioModel::pureSignal() returns nullptr until connectToRadio()'s
    // WDSP-init lambda fires, see RadioModel.cpp:1884 [v2.10.3.13]).  This
    // signal is the late-binding seam.  Tests call
    // emit pureSignalCoordinatorReady(...) directly to inject a test-owned
    // coordinator into the applet wiring.
    void pureSignalCoordinatorReady(NereusSDR::PureSignal* coordinator);
    void sliceAdded(int index);
    void sliceRemoved(int index);
    void activeSliceChanged(int index);
    // Emitted once at the end of loadSliceState() after the slice has been
    // restored from AppSettings. Mirrors Thetis console.cs:27204 [v2.10.3.13]
    // chkPower_CheckedChanged calling txtVFOAFreq_LostFocus() as the
    // explicit "push state to display" step at power-on. Listeners
    // (MainWindow, SpectrumWidget bridge) push the now-correct slice
    // freq/mode/filter into views, since the wireSliceToSpectrum() seed
    // ran with the slice's pre-restore default values.
    void sliceStateRestored(int index);
    // Issue #153 sub-bug 2 — diagnostic + test observation hook.
    // Emitted by pushTxModeAndBandpass() when an active slice exists,
    // BEFORE the queued setter dispatch to TxWorkerThread.  Carries the
    // slice's current DSPMode + audio-space filter cutoffs.  Tests use
    // it as a proxy for "push helper triggered with X"; production code
    // can wire it into diagnostic logging.
    void txModeAndBandpassPushed(NereusSDR::DSPMode mode,
                                 int audioLowHz, int audioHighHz);
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

    // Task 1.8: DSP rebuild elapsed time signal.
    // Emitted whenever a live DSP change (sample rate, active RX count,
    // DSP-Options buffer/filter changes) completes. The argument is the
    // elapsed wall-clock milliseconds for the rebuild. Used by
    // DspOptionsPage's "Time to last change" readout.
    void dspChangeMeasured(qint64 elapsedMs);

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

    // ── Plan 4 D8: per-profile TX filter relay signal ─────────────────────────
    //
    // Intermediate signal that carries the 3-arg filter request (audio Hz + mode)
    // from the main-thread lambda (subscribed to TransmitModel::filterChanged)
    // across to TxChannel::requestFilterChange on the audio thread.
    //
    // TransmitModel lives on the main thread; TxChannel lives on TxWorkerThread
    // after RadioModel's moveToThread call.  A direct lambda-connect from
    // TransmitModel::filterChanged → m_txChannel lambda would fire on the main
    // thread (because TransmitModel is the sender and its thread is main).
    // Routing through this intermediate signal ensures Qt auto-connection
    // selects QueuedConnection for TxChannel::requestFilterChange, which runs
    // the slot on TxWorkerThread where the debounce timer is live.
    //
    // NereusSDR-original glue (no Thetis equivalent needed).
    void txFilterRequest(int audioLowHz, int audioHighHz, NereusSDR::DSPMode mode);

    // ── Phase 3R Task I5: RadeChannel slot-graph re-emit signals ─────────────
    //
    // Re-emitted after RadioModel internalises the corresponding
    // RadeChannel signal. UI consumers (RadeApplet, status bar SYNC
    // badge, future SNR readout in VfoWidget) subscribe here rather
    // than to the per-channel RadeChannel directly, so they survive
    // mode-swap / channel-rebuild cycles without re-wiring.
    //
    // radeSyncChanged fires only on actual transitions (de-duplicated
    // by onRadeSyncChanged via m_radeSyncedSlices). radeSnrChanged
    // fires on every onRadeSnrChanged invocation: no de-dup here,
    // the slice setter's NaN-aware short-circuit handles repaint thrash.
    void radeSyncChanged(int sliceId, bool synced);
    void radeSnrChanged(int sliceId, float snrDb);

    // Phase 3R Task L2: RADE carrier-frequency offset re-emit. Wired
    // alongside the I5 trio for the RadeApplet freq-offset readout.
    // No model-side de-dup: the codec already coalesces by emitting
    // only on actual offset change.
    void radeFreqOffsetChanged(int sliceId, float hz);

private slots:
    void onConnectionStateChanged(NereusSDR::ConnectionState state);

    // ── #202 deep-fix: Audio.RadioVolume setter analogue ─────────────────────
    //
    // Mirrors Thetis audio.cs:262-271 [v2.10.3.13]:
    //   public static double RadioVolume {
    //       set {
    //           radio_volume = value;
    //           NetworkIO.SetOutputPower((float)(value * 1.02));   // wire byte
    //           cmaster.CMSetTXOutputLevel();                       // IQ scalar
    //       }
    //   }
    //
    // Connected to TransmitModel::audioVolumeChanged so every call to
    // setPowerUsingTargetDbm (drive slider, TUNE-on, TUN-off restore,
    // two-tone) and any future audio_volume mutator pumps the wire byte +
    // IQ scalar uniformly.  Also re-pumped on TransmitModel::
    // swrProtectFactorChanged (mirrors console.cs:26102-26109 [v2.10.3.13]
    // `Audio.RadioVolume = Audio.RadioVolume` re-emission when SWRProtect
    // changes mid-TX).
    //
    // Wire byte composition is byte-for-byte equivalent to Thetis
    // NetworkIO.cs:201-211 [v2.10.3.13]:
    //   if (f < 0.0) f = 0.0F;
    //   if (f >= 1.0) f = 1.0F;
    //   int i = (int)(255 * f * _swr_protect);
    // IQ scalar mirrors cmaster.cs:1115-1119 [v2.10.3.13]:
    //   double level = Audio.RadioVolume * Audio.HighSWRScale;
    // where HighSWRScale is set to 1.0 once at console.cs:29194 and never
    // reassigned anywhere in baseline Thetis — effectively no-op.
    void pumpAudioVolume(double audioVolume);

    // ── Phase 3J-2 H2: per-source spot-adapter slots ────────────────────────
    //
    // Each ingest client emits spotReceived(DxSpot); the adapter slot
    // translates that into the QMap<QString,QString> kvs shape
    // SpotModel::applySpotStatus expects (TCI-style sink). Per-source
    // lifetime and color defaults are read from AppSettings under the
    // <Source>SpotLifetimeSec / <Source>SpotColor key family.
    //
    // The WSJT-X adapter is special: it also pushes to RxDecodeModel so the
    // "what my radio just heard" feed tracks live decodes (NereusSDR design;
    // freedv-gui has no equivalent feed). WsjtxClient does not have a
    // separate decodeReceived signal; the single spotReceived signal is the
    // source for both sinks.
    void onClusterSpotReceived(const NereusSDR::DxSpot& spot);
    void onRbnSpotReceived(const NereusSDR::DxSpot& spot);
    void onWsjtxSpotReceived(const NereusSDR::DxSpot& spot);
    void onSpotCollectorSpotReceived(const NereusSDR::DxSpot& spot);
    void onPotaSpotReceived(const NereusSDR::DxSpot& spot);
    void onFreeDvReporterSpotReceived(const NereusSDR::DxSpot& spot);
    void onPskReporterSpotReceived(const NereusSDR::DxSpot& spot);

private:
    // Phase 3Q-1: drives the RadioModel-level connection state machine.
    // Guards against redundant transitions (no emit if state unchanged).
    void setConnectionState(ConnectionState s);

    // v0.4.1 hotfix: single point that fans the connected hardware
    // HPSDRModel out to every sub-model that needs it.  Updates
    // m_hardwareProfile, then pushes the model into TransmitModel
    // (issue #175 HL2 mi0bot polymorphic-clamp setup) AND
    // ReceiverManager (drives per-board codec dispatch in
    // applyPureSignalDdcConfig — without this fan-out, the codec
    // sees the default HPSDRModel::HPSDR enum, falls through its
    // model switch's default branch, and emits an empty PsDdcConfig
    // → PsccPump never activates → PureSignal correction never
    // lands).  Called from connectToRadio() and the test-only
    // setHpsdrModelForTest() seam so production and tests stay in
    // sync.
    void applyHpsdrModel(HPSDRModel m);

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

    // Issue #182 — wire TransmitModel::micPttDisabledChanged →
    // RadioConnection::setMicPTTDisabled and prime the connection with the
    // current model value once.  Extracted so the connect() can be exercised
    // in isolation by tst_radio_model_mic_ptt_wire without needing to spin
    // up the full DSP-thread pipeline that wireConnectionSignals starts.
    void connectMicPttDisabledSignal();

    // Issue #177 — deferred completion of the TUN-off path.
    //
    // Called from the rxReady → settle-timer slot wired in the constructor.
    // Performs everything that used to run synchronously inside setTune(false)
    // EXCEPT the MoxController::setTune(false) call: gen1 OFF, DSP-mode
    // restore (CWL/CWU), tune-power restore through the dBm path, TX VFO
    // un-offset, and the m_isTuning / m_pendingTuneOff state clears.
    //
    // Cite: Thetis console.cs:30106-30148 [v2.10.3.13] — chkTUN_CheckedChanged
    // TUN-off branch.  Thetis runs the equivalent block AFTER
    // chkMOX.Checked = false (which is synchronous and blocks ~30 ms inside
    // chkMOX_CheckedChanged2) and AFTER `await Task.Delay(100)`.  In NereusSDR
    // this method is invoked from a QTimer::singleShot(m_tuneOffSettleMs)
    // chained off MoxController::rxReady, so the same total ~130 ms gap
    // separates the user's click from gen1 going off.
    void completeTuneOff();

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

    // Restore a slice's persisted state from AppSettings.  Public so unit
    // tests can drive it without spinning up the full connectToRadio()
    // pipeline.  Production callers: connectToRadio() at RadioModel.cpp
    // line ~1377 — fires once per session per slice on Connected. Emits
    // sliceStateRestored(index) on completion (see comment on the signal).
    void loadSliceState(SliceModel* slice);

    // Issue #153 sub-bug 2 — push the active slice's DSPMode + the
    // user's configured TX bandpass (TransmitModel::filterLow/High) to
    // TxChannel.  No-op if no active slice.
    //
    // Filter source is TransmitModel, NOT SliceModel.  TransmitModel
    // stores audio-space TX cutoffs (positive, low<=high invariant),
    // which is what TxChannel::requestFilterChange + applyTxFilterForMode
    // expect.  SliceModel filter values are RX-passband IQ-space
    // (negative for LSB-family); routing them through
    // applyTxFilterForMode would double-negate on LSB and clobber
    // any user-configured TX bandwidth on every connect/MOX.  Mirrors
    // the canonical wire at RadioModel.cpp:2550-2560 which sources
    // audio cutoffs from TransmitModel::filterChanged.
    //
    // Read happens on RadioModel's main thread; the TxChannel setter
    // call is queued to TxWorkerThread via QMetaObject::invokeMethod
    // (receiver=m_txChannel) so the receiver-thread invariant holds —
    // mirrors the F.1 / F.2 / H.1 wires inside connectToRadio's txSetup
    // lambda.  Emits txModeAndBandpassPushed(mode, audioLow, audioHigh)
    // before the queued dispatch as a test/diagnostic observation hook
    // (fires even when m_txChannel is null so test fixtures can drive
    // the helper without standing up the full TX pipeline).
    //
    // Wire targets (set up inside the txSetup lambda + wireSliceSignals):
    //   - createTxChannel success → pushTxModeAndBandpass (initial seed)
    //   - SliceModel::dspModeChanged → pushTxModeAndBandpass
    //   - MoxController::txAboutToBegin → pushTxModeAndBandpass
    //
    // Source-of-truth: Thetis SetTXFilters at console.cs:8091 +
    // CurrentDSPMode setter at radio.cs:2670-2696 [v2.10.3.13], wired
    // into the mode-change handler at console.cs:33937 [v2.10.3.13].
    // The MOX-engage trigger is NereusSDR's belt-and-suspenders re-seed
    // (Thetis seeds at mode-change only; we additionally re-seed at
    // MOX-engage so prior TUN-state desync cannot starve SSB MOX).
    void pushTxModeAndBandpass();

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

    // HL2 temperature averaging ring (only populated when model ==
    // HPSDRModel::HERMESLITE). HL2 firmware overloads the C&C
    // exciter_power AIN5 field to carry on-die FPGA temperature ADC
    // counts; we mirror mi0bot's 100-sample averaging window before
    // publishing to RadioStatus to suppress per-frame noise.
    //
    // Port of mi0bot console.cs:24917-24985 + 25069-25082
    // [v2.10.3.13-beta2 @c26a8a4]:
    //   private ConcurrentQueue<int> _tempQueue = new ConcurrentQueue<int>();   // MI0BOT: HL2 temperature
    //   ...
    //   _tempQueue.Enqueue(NetworkIO.getExciterPower());
    //   while (_tempQueue.Count > 100 && nTries < 100) //  MI0BOT: HL2 temperature, keep max 100 in the queue
    //       _tempQueue.TryDequeue(out int tmp);
    //   ...
    //   float tempAverage = _tempQueue.Count > 0 ? (float)_tempQueue.Average() : 0;     // MI0BOT: HL2 temperature
    std::array<quint16, 100> m_hl2TempRing{};
    int m_hl2TempCount{0};   // 0..100 — slots filled
    int m_hl2TempHead{0};    // next slot to write

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

    // Task 1.7: active-RX count last pushed to the wire (0 = disconnected).
    // Updated by setActiveRxCountLive() after hardware reconfiguration completes.
    // Also written by connectToRadio() via the resolveActiveRxCount() call.
    int m_connectionActiveRxCount{0};

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

    // Test-only override for the handlePaTelemetry MOX gate.
    // Toggled true by handlePaTelemetryForTest() before the call and false
    // after, simulating "the radio just sent us a transmit sample" without
    // requiring the full MoxController state machine to be driven into
    // MoxState::Tx.  Always false in production code paths.
    // Bench-reported #167 follow-up.
    bool     m_forceTxForTest{false};

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
    // Task 3.1 view hook — non-owning, set by MainWindow.
    class MeterPoller*      m_meterPoller{nullptr};
    // Task 3.2 view hook — non-owning, set by MainWindow.
    class ContainerManager* m_containerManager{nullptr};

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

    // m_lastAudioVolume: cache of the most recent value emitted by
    //   TransmitModel::audioVolumeChanged.  Used by the swrProtectFactorChanged
    //   re-pump path (mirrors Thetis console.cs:26108 [v2.10.3.13]
    //   `Audio.RadioVolume = Audio.RadioVolume` self-assign re-emission when
    //   SWRProtect changes mid-TX).  Updated only by pumpAudioVolume.
    double m_lastAudioVolume{0.0};

    // ── Issue #177 fix — Thetis-faithful TUN-off ordering ────────────────────
    //
    // m_pendingTuneOff: latched true at the START of the setTune(false) path,
    //   cleared inside completeTuneOff().  setTune(false) now only kicks off
    //   the MoxController TX→RX walk; the rest (gen1 off, mode restore, drive
    //   restore, VFO restore) runs from completeTuneOff() AFTER MoxController
    //   emits rxReady AND an additional 100 ms settle elapses.
    //
    //   This mirrors Thetis console.cs:30106-30109 [v2.10.3.13]:
    //     chkMOX.Checked = false;        // synchronously walks TX→RX (~30 ms)
    //     await Task.Delay(100);
    //     radio.GetDSPTX(0).TXPostGenRun = 0;
    //
    //   Without the deferral, gen1 was killed at T+0 while the WDSP TX channel
    //   was still pumping fexchange0 (setRunning(false) does not fire until
    //   txaFlushed at T+10 ms).  The hard step at gen1's output produced a
    //   filter-ringing transient through the 31-stage TXA chain that briefly
    //   exceeded steady-state amplitude on the wire.  Combined with the wire
    //   drive byte staying at TUNE level for one EP2 frame after MOX-off
    //   (round-robin priority bank0 > bank10), this produced an RF spike past
    //   the radio's spec at high tune-slider settings.  Issue #177.
    bool m_pendingTuneOff{false};

    // m_tuneOffSettleMs: explicit 100 ms wait between MoxController::rxReady
    //   and completeTuneOff().  Mirrors `await Task.Delay(100)` at Thetis
    //   console.cs:30107 [v2.10.3.13].  Tests override via the *ForTest seam.
    int m_tuneOffSettleMs{100};

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

    // Phase 3J-1 closeout Item 3 (2026-05-12): TCI Q_INVOKABLE long-tail
    // state.  See setGlobalMute / setAfLinear / setIqSampleRate / etc. for
    // semantics.  Defaults chosen to match TestMockRadioModel initial
    // values so the production path passes the existing matrix tests.
    //
    // Per-slice stub state for DSP toggles SliceModel doesn't yet expose
    // as Q_PROPERTYs: rxBin / rxApf / rxNf / rxEnable.  Sized to the max
    // RX count NereusSDR supports today (4 for the four-DDC SKUs); the
    // setter clamps the index so an out-of-range slice silently no-ops.
    static constexpr int kTciStubSliceMax = 4;
    bool        m_tciGlobalMute{false};
    int         m_tciAfLinear{0};
    int         m_tciMonLinear{0};
    int         m_tciIqSampleRate{0};
    // Audio-stream config: per-client semantics live in TciClientSession;
    // these mirror "last value any client sent" for matrix-test parity.
    int         m_tciAudioSampleRate{48000};
    int         m_tciAudioStreamChannels{2};
    int         m_tciAudioStreamSamples{2048};
    QString     m_tciAudioStreamSampleType{QStringLiteral("Float32")};
    // Per-slice DSP toggle stubs (set-and-read only; not wired to WDSP).
    std::array<bool, kTciStubSliceMax> m_tciStubRxBin{};
    std::array<bool, kTciStubSliceMax> m_tciStubRxApf{};
    std::array<bool, kTciStubSliceMax> m_tciStubRxNf{};
    std::array<bool, kTciStubSliceMax> m_tciStubRxCtun{};
    std::array<bool, kTciStubSliceMax> m_tciStubRxEnable{ {true, false, false, false} };

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
    // VAX TX consumer (added 2026-05-06, eager-borg-d64bed).  Pulls
    // audio from /nereussdr-vax-tx shared memory via AudioEngine and
    // is registered with m_compositeMicRouter via setVaxSource().
    // Reset before m_compositeMicRouter on teardown — see notes
    // around teardownConnection().
    std::unique_ptr<VaxTxMicSource>        m_vaxTxMicSource;
    std::unique_ptr<CompositeTxMicRouter>  m_compositeMicRouter;

    // ── 3M-1c Phase L: cross-cutting ownership ──────────────────────────────
    //
    // L.1 — MicProfileManager (chunk F).  QObject child of RadioModel so the
    // dtor cleans it up automatically.  Constructed once in the RadioModel
    // ctor; setMacAddress + load() are called per-connect inside
    // connectToRadio(); setMacAddress("") is called in teardownConnection so
    // mutators silently no-op while no radio is selected.
    MicProfileManager* m_micProfileMgr{nullptr};

    // Phase 4 Agent 4A of issue #167 — PaProfileManager.  QObject child of
    // RadioModel; mirrors m_micProfileMgr lifecycle exactly.  Constructed
    // once in the ctor; setMacAddress + load(connectedModel) are called
    // per-connect inside connectToRadio().  The active profile is read at
    // every drive-slider / TUNE callsite via paProfileManager()->activeProfile()
    // and passed by reference to TransmitModel::setPowerUsingTargetDbm.
    PaProfileManager* m_paProfileManager{nullptr};
    //
    // L.2 — TwoToneController (chunk I).  QObject child of RadioModel.
    // Construction-time deps that DON'T require a live connection
    // (TransmitModel, MoxController, SliceModel) are wired in the RadioModel
    // ctor; setTxChannel(...) is called inside the WDSP-init lambda once
    // m_txChannel is live.  setTxChannel(nullptr) is called in teardown.
    TwoToneController* m_twoToneController{nullptr};

    // 3M-4 Task 7: PureSignal coordinator.  Owned via unique_ptr (NOT a
    // raw QObject child) so the destructor can drain the polling timers
    // before the WdspEngine / TxChannel pointers are torn down — the
    // QObject child-deletion path doesn't guarantee that ordering.  See
    // PureSignal.h for the design.  Constructed inside the WDSP-init
    // lambda alongside TwoToneController; reset() in teardown.
    std::unique_ptr<PureSignal> m_pureSignal;

    // 3M-4 Task 17 chunk C: pscc() driver — pairs per-DDC IQ streams
    // (PS-feedback on DDC0, TX-monitor on DDC1) into paired blocks for
    // calcc.  Without this driver, calcc never runs and info[16] stays
    // at zero.  See PsccPump.h for the architectural narrative.  Owned
    // via unique_ptr alongside m_pureSignal so destruction ordering is
    // explicit (drain pump before TxChannel goes away).
    std::unique_ptr<PsccPump> m_psccPump;
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

    // Phase 3R K-bench: RADE TX 24 -> txSampleRate upsampler.  Lazily
    // constructed on first txModemReady arrival once we know the
    // connection's txSampleRate() (P1 = 48 kHz; P2 = 192 kHz; per
    // RadioConnection.h:408 default + P2RadioConnection.h:265).
    // m_radeTxResamplerHwRate stores the rate the resampler was
    // built against so a reconnect at a different rate triggers a
    // rebuild rather than silently producing wrong-rate audio.
    //
    // m_radeTxMonoScratch / m_radeTxIqScratch are reused per emission
    // to avoid per-call allocations.  Both are sized once on first use.
    std::unique_ptr<Resampler> m_radeTxResampler;
    int                        m_radeTxResamplerHwRate{0};
    std::vector<float>         m_radeTxMonoScratch;
    std::vector<float>         m_radeTxIqScratch;

    // Phase 3R K-bench (bench feedback): RADE RX speakers-side
    // upsamplers. RadeChannel emits 24 kHz stereo float; AudioEngine
    // expects 48 kHz. Without these upsamplers the speech plays at
    // 2x speed ("chipmunk sounding"). One Resampler per leg so each
    // channel sees a self-consistent stream.
    std::unique_ptr<Resampler> m_radeRxSpeechL;
    std::unique_ptr<Resampler> m_radeRxSpeechR;
    std::vector<float>         m_radeRxLScratch;
    std::vector<float>         m_radeRxRScratch;
    std::vector<float>         m_radeRxInterleaved48k;

    // Stage C2 — filter preset user-override store.
    // Constructed in RadioModel ctor; QObject child so dtor cleans up.
    FilterPresetStore* m_filterPresetStore{nullptr};

    // ── Phase 3J-2 H2: spot-system ownership ────────────────────────────────
    //
    // RadioModel becomes the wiring hub for Phase 3J-2's spot system. View
    // models are constructed first (the adapter slots below depend on
    // m_spotModel + m_rxDecodeModel + m_freeDvStationModel being live), then
    // the ingest clients. Each client emits spotReceived(DxSpot); a per-
    // source adapter slot translates that into the kvs map
    // SpotModel::applySpotStatus expects.
    //
    // None of the clients start their network I/O at construction time;
    // startConnection() / startListening() / startPolling() is the M3
    // follow-up task. H2 only wires the in-process signal graph.
    std::unique_ptr<SpotModel>            m_spotModel;
    std::unique_ptr<SpotTableModel>       m_spotTableModel;
    std::unique_ptr<FreeDVStationModel>   m_freeDvStationModel;
    std::unique_ptr<RxDecodeModel>        m_rxDecodeModel;
    std::unique_ptr<DxccColorProvider>    m_dxccColorProvider;

    std::unique_ptr<DxClusterClient>      m_dxCluster;      // DX cluster (DxSpider / AR-Cluster / CC-Cluster)
    std::unique_ptr<DxClusterClient>      m_rbn;            // Reverse Beacon Network (RBN-suffixed spotter)
    std::unique_ptr<WsjtxClient>          m_wsjtx;
    std::unique_ptr<SpotCollectorClient>  m_spotCollector;
    std::unique_ptr<PotaClient>           m_pota;
    std::unique_ptr<FreeDVReporterClient> m_freeDvReporter;
    std::unique_ptr<PskReporterClient>    m_pskReporter;

    // Monotonic index passed to SpotModel::applySpotStatus on every adapter
    // dispatch. Increments once per emitted spot regardless of source.
    int m_nextSpotIndex{0};

    // ── Phase 3R Task I5: per-slice RADE sync state cache ───────────────────
    //
    // Latest RADE decoder sync state per slice ID, updated by
    // onRadeSyncChanged on every transition. radeSynced(sliceId) reads
    // this; a missing key reads as false (slice never had RADE wired).
    //
    // Stored on RadioModel rather than SliceModel so future UI surfaces
    // can iterate sync state across all slices without recursing into
    // each slice's WDSP channel pointer. The dedup logic in
    // onRadeSyncChanged also lives here for the same reason.
    QHash<int, bool> m_radeSyncedSlices;

    // 2026-05-12 bench: per-slice timestamp of the most recent sync
    // FALLING edge (true -> false transition).  Used by onRadeSyncChanged
    // to debounce the "clear cached speaker callsign on sync rise"
    // behaviour: only count a rising edge as a "new transmission /
    // new speaker" event if sync was down for >= kRadeSyncDropClearDebounceMs.
    // Brief flickers (< debounce) keep the previous over's callsign on
    // the VFO flag.  Per bench design refinement 2026-05-12 (option B
    // debounce-by-sync-loss-duration).
    QHash<int, QDateTime> m_radeSyncDropAt;
    static constexpr int kRadeSyncDropClearDebounceMs = 2000;

    // 2026-05-12 bench: FreeDV Reporter freq-publish throttle.
    //
    // Spinning the VFO would otherwise fire a Socket.IO freq_change
    // event on every sub-Hz movement (mouse wheel cadence) -- the
    // qso.freedv.org server gets DoS'd and other operators see the
    // dashboard flicker.  Throttle policy (per JJ bench design
    // 2026-05-12):
    //
    //   1. Trailing dwell: restart a single-shot timer on every freq
    //      change; only publish when the timer expires
    //      (kFreedvFreqDwellMs = 7000 ms).  Spinning across a band
    //      publishes exactly once, 7 s after the user stops.
    //   2. Band-jump fast-path: if the new freq is >= kFreedvFreqJumpHz
    //      (100 kHz) from the last *published* freq, bypass the dwell
    //      and publish immediately -- band changes don't lag.
    //   3. MOX force-publish: TX engage flushes any pending dwell so
    //      the reporter never shows "TXing on stale freq."
    //
    // Driven by publishFreedvFrequencyDwelled() called from
    // SliceModel::frequencyChanged.  m_freedvLastPublishedHz is the
    // baseline for the band-jump comparison.  Initial publish on
    // Socket.IO ACK bypasses this and uses setFrequency() directly so
    // the first packet establishes the baseline.
    QTimer*  m_freedvFreqDwellTimer{nullptr};
    quint64  m_freedvLastPublishedHz{0};
    quint64  m_freedvPendingHz{0};
    static constexpr int     kFreedvFreqDwellMs = 7000;
    static constexpr quint64 kFreedvFreqJumpHz  = 100'000;
};

} // namespace NereusSDR
