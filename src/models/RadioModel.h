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

#include "Band.h"
#include "BandPlanManager.h"
#include "SliceModel.h"
#include "PanadapterModel.h"
#include "MeterModel.h"
#include "TransmitModel.h"
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

#include <QObject>
#include <QString>
#include <QList>
#include <QThread>

namespace NereusSDR {

class ReceiverManager;
class AudioEngine;
class WdspEngine;
class RxDspWorker;
class NoiseFloorTracker;

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
        m_testCapsOverride = true;
        m_testCapsHasAlex = hasAlex;
    }
    NereusSDR::Band lastBand() const { return m_lastBand; }
#endif

    // Connection
    void connectToRadio(const RadioInfo& info);
    void disconnectFromRadio();

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

signals:
    void infoChanged();
    void connectionStateChanged();
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

private slots:
    void onConnectionStateChanged(NereusSDR::ConnectionState state);

private:
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
    void loadSliceState(SliceModel* slice);
    void saveSliceState(SliceModel* slice);
    void scheduleSettingsSave();

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
    bool m_testCapsOverride{false};
    bool m_testCapsHasAlex{false};
#endif

    // Phase 3M-0 Task 6: Ganymede PA-trip live state.
    // From Thetis Andromeda/Andromeda.cs:914 [v2.10.3.13] (_ganymede_pa_issue volatile bool).
    // G8NJJ: handlers for Ganymede 500W PA protection
    bool m_paTripped{false};
    // From Thetis Andromeda/Andromeda.cs:854-866 [v2.10.3.13] (_ganymedePresent / GanymedePresent setter).
    bool m_ganymedePresent{false};

    // AGC bidirectional sync guard — prevents infinite feedback loop between
    // agcThresholdChanged and rfGainChanged handlers.
    // From Thetis console.cs:45960-46006 — bidirectional sync pattern.
    bool m_syncingAgc{false};

    // From Thetis v2.10.3.13 console.cs:46057 — tmrAutoAGC (500ms interval)
    QTimer* m_autoAgcTimer{nullptr};
    NoiseFloorTracker* m_noiseFloorTracker{nullptr};
};

} // namespace NereusSDR
