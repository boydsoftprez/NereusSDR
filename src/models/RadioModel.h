#pragma once

// =================================================================
// src/models/RadioModel.h  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-09-27 -- cwPitch()/setCwPitch()/cwPitchChanged(int) (Martin
//                 Fischer, OE5SOS, AI-assisted via Anthropic Claude).
//   2026-11-10 -- Auto AGC-T misst nicht mehr beim Senden: Messabschnitte
//                 (autoAgcAcquisitionEpoch, rawIqDataForAutoAgc), aus
//                 Upstream NereusSDR e5b3997bc (#720, J.J. Boyd KG4VCF);
//                 dazu folgt der MOX-Riegel des TransmitModel jetzt dem
//                 MoxController. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
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
#include "core/CouplerZero.h"
#include "core/FlexAutoRauschsperre.h"
#include "core/audio/VoiceKeyer.h"
#include "core/audio/VoiceKeyerPlayer.h"
#include "core/audio/QsoRecorderController.h"
#include "RecordingModel.h"
#include "core/PgxlConnection.h"
#include "core/Rf2ksConnection.h"
#include "core/SpeConnection.h"
#include "core/Kpa500Connection.h"
#include "core/AcomConnection.h"
#include "core/TgxlConnection.h"
#include "core/FaultLog.h"
#include "core/TxInterlockPolicy.h"
#include "core/TuneMemoryStore.h"
#include "models/TunerModel.h"
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
#include "core/SliceStreamAllocator.h"
#include "core/accessories/AlexController.h"
#include "core/accessories/ApolloController.h"
#include "core/accessories/PennyLaneController.h"
#include "core/CalibrationController.h"
#include "core/RadioDiscovery.h"
#include "core/FlexRadioDiscovery.h"
#include "core/FlexRadioConnection.h"
#include "core/FlexApdState.h"
#include "core/FlexVitaStream.h"
#include "core/RadioConnection.h"
#include "core/HardwareProfile.h"
#include "core/codec/CodecContext.h"  // SliceConfig (Phase 3F Sub-Epic B Task 16)
#include "core/DdcAssignment.h"       // DdcAssignment (Phase 3F Sub-Epic B Task 16)
#include "core/SkuUiProfile.h"  // issue #257 — setLastBandForTest passes the SKU into refreshAntennasFromAlex
#include "core/safety/SwrProtectionController.h"
#include "core/safety/SwrRiegel.h"
#include "core/safety/FlexAbstimmRegler.h"
#include "core/safety/TxInhibitMonitor.h"
#include "core/safety/BandPlanGuard.h"

#include <QDateTime>
#include <QPointer>

// Vollstaendig noetig: QPointer<T> kann T nicht vorwaerts deklariert
// benutzen (static_cast auf QObject*). Der Kopf ist winzig.
#include "core/RotorController.h"
#include "core/CatRigBruecke.h"
#include "core/CompanionRigController.h"
#include <QHash>
#include <QObject>
#include <QMap>
#include <QSet>     // Phase 3F: panBypassState takes PanadapterApplet::associatedSlices
#include <QString>
#include <QList>
#include <QThread>

#include <limits>   // 2026-05-22 NaN sentinel for m_lastEmittedRxMeterOffsetDb

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
#include <atomic>
#include <optional>

namespace Longpath {

class ReceiverManager;
class AudioEngine;
class WdspEngine;
class RxDspWorker;
class NoiseFloorTracker;
class MemoryList;
class BandStackController;
struct MemoryRecord;
// Phase 3F Sub-Epic F Task 5: per-ADC wideband FFT engine. Forward decl
// here; included in RadioModel.cpp so we don't pull fftw3.h into every
// translation unit that touches RadioModel.h.
class WidebandFftEngine;
// 3M-1a G.1: forward declarations for TX-side components.
class MoxController;
class Ts590Sprache;
class TxChannel;
class StripChain;
// Phase 3F Sub-Epic J Task 11: forward decl for rxChannelForSlice()'s
// return type (see below, near txChannel()).
class RxChannel;
// Phase 3F Sub-Epic C: TX-slice arbiter (single-TX invariant + RF-safe handoff).
class TxSliceArbiter;
// 3M-1b L.1: forward declarations for mic-source strategy objects.
class PcMicSource;
class RadioMicSource;
class VaxTxMicSource;  // VAX TX consumer (added 2026-05-06).
class CompositeTxMicRouter;
class HotKeyBindings;
// 3M-1c L.1 / L.2: forward declarations for the MicProfileManager bank
// (chunk F) + the TwoToneController activation orchestrator (chunk I).
class MicProfileManager;
class RxProfileManager;
class TwoToneController;
class SwrSweepController;
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
class SotaClient;
class FreeDVReporterClient;
class FreeDVRadeReporterBridge;
class PskReporterClient;
class DxccColorProvider;
class SpotModel;
class RoundtableHub;
class SpotTableModel;
class FreeDVStationModel;
class Ft8TxSource;
class RttyTxSource;
class RxDecodeModel;
// TNF (design section 5): the canonical notch store, owned by RadioModel
// alongside SpotModel. One list shared by every slice (design D1) because
// notch centres are absolute RF Hz, so a 20 m notch is inherently inert on a
// 40 m slice.
class NotchModel;
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
    Q_PROPERTY(bool rfKitEnabled READ rfKitEnabled WRITE setRfKitEnabled
               NOTIFY rfKitEnabledChanged)
    // Zeus-Punkt 7: SPE Expert (1.3K-FA / 1.5K-FA / 2K-FA). Gleiche Form
    // wie rfKitEnabled, eigener Schluessel.
    Q_PROPERTY(bool speEnabled READ speEnabled WRITE setSpeEnabled
               NOTIFY speEnabledChanged)
    Q_PROPERTY(bool kpa500Enabled READ kpa500Enabled WRITE setKpa500Enabled
               NOTIFY kpa500EnabledChanged)
    // ACOM S-Serie. Nicht auf Zeus' Liste, aber das Geraet, das der
    // Betreiber einmal anschliessen will (2026-10-09).
    Q_PROPERTY(bool acomEnabled READ acomEnabled WRITE setAcomEnabled
               NOTIFY acomEnabledChanged)

public:
    explicit RadioModel(QObject* parent = nullptr);
    ~RadioModel() override;

    // Sub-components
    RadioConnection*  connection()       { return m_connection; }
    const RadioConnection* connection() const { return m_connection; }
    RadioDiscovery*   discovery()        { return m_discovery; }
    // Flex-Treiber Stufe 1: die passive Erkennung auf UDP 4992. Getrennt
    // von m_discovery, weil die beiden entgegengesetzt arbeiten --
    // OpenHPSDR wird gefragt (Rundruf auf 1024, NIC-Lauf), FlexRadio
    // ruft von selbst. Siehe core/FlexRadioDiscovery.h.
    FlexRadioDiscovery* flexDiscovery()  { return m_flexDiscovery; }
    // Der Flex-Befehlskanal. Nur belegt, solange eine FlexRadio-Maschine
    // verbunden ist; sonst nullptr. Siehe core/FlexRadioConnection.h —
    // bewusst KEINE RadioConnection-Unterklasse, deshalb steht er
    // neben m_connection und nicht darin.
    FlexRadioConnection* flexConnection() { return m_flexConnection; }
    // Der VITA-49-Strom. Nur belegt, solange ein Flex verbunden ist.
    FlexVitaStream* flexStream() { return m_flexStream; }

    /// Empfangspegel der Flex-Scheibe in dBm, oder nichts.
    ///
    /// Beim Flex rechnet die Maschine: die WDSP-Empfangskette ist leer,
    /// und der S-Meter-Umlauf (MeterPoller) liest genau sie. Der
    /// Betreiber sah darum am 2026-10-22 ein totes S-Meter, obwohl die
    /// Maschine den Pegel laufend schickt (Messwert `LEVEL`, `src=SLC`).
    ///
    /// `nullopt`, wenn seit zwei Sekunden kein Messwertpaket kam. Ein
    /// eingefrorener Zeiger behauptet eine Messung, die nicht mehr
    /// stattfindet; lieber gar keine Anzeige.
    std::optional<double> flexPegelDbm() const;
    ReceiverManager*  receiverManager()  { return m_receiverManager; }
    AudioEngine*      audioEngine()      { return m_audioEngine; }
    WdspEngine*       wdspEngine()       { return m_wdspEngine; }

    /// Phase 3F Sub-Epic F Task 5: per-ADC wideband FFT engine accessor.
    /// Returns nullptr if adcIndex out of range (valid: 0 or 1).  Used by
    /// SpectrumWidget and bench rigs to inspect the wideband FFT pipeline
    /// without going through the widebandSpectrumReady signal hop.
    Longpath::WidebandFftEngine* widebandFftEngine(int adc) const {
        return (adc >= 0 && adc < 2) ? m_widebandFftEngines[adc] : nullptr;
    }

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
    /// Der Stehwellenriegel -- lesbar fuer die Oberflaeche (sie zeigt
    /// die Grenze an) und fuer den Prueflauf, der nachsieht, ob die
    /// Zahl aus den Einstellungen ueberhaupt ankommt. Eine Einheit,
    /// die geprueft ist, aber nicht angeschlossen, ist nichts wert --
    /// genau das war heute schon einmal der Fall (Multi Answering).
    const safety::FlexAbstimmRegler& flexAbstimmRegler() const
        { return m_flexAbstimmRegler; }
    safety::FlexAbstimmRegler& flexAbstimmRegler() { return m_flexAbstimmRegler; }

    const safety::SwrRiegel& flexSwrRiegel() const { return m_flexSwrRiegel; }
    safety::SwrRiegel&       flexSwrRiegel()       { return m_flexSwrRiegel; }

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

    // ── Phase 3F: per-panadapter RX preselector bypass state (WIDE badge) ────
    // Longpath-original; no upstream port. Design doc
    // 2026-05-26-phase3f-multi-pan-multi-slice-design.md §16.4.
    //
    // WIDE means one thing: the RX preselector chain feeding this pan is
    // bypassed on the wire right now. It reports an effect, not a cause;
    // the cause is named in `reason` (§16.4.3, §16.4.4).
    struct PanBypassState {
        bool    bypassed {false};
        QString reason;   ///< operator-facing sentence; empty unless bypassed
    };

    /// Resolve the WIDE state for the slices one panadapter is showing.
    ///
    /// Routing, per §16.4.2:
    ///     pan -> its slices -> their stream -> that stream's ADC -> effective
    ///
    /// The answer is per chain, not global: on a 2-chain SKU with the bypass
    /// on chain 1 only, pans on chain 0 come back clear. That is the whole
    /// point of the badge -- it tells the operator WHICH of their receivers
    /// is exposed. A pan with no slices, or whose slices have not bound a
    /// stream, feeds off nothing and reports nothing.
    ///
    /// Takes slice indices (the keys PanadapterApplet::associatedSlices
    /// hands out) rather than SliceModel pointers so callers never have to
    /// resolve the model themselves.
    PanBypassState panBypassState(const QSet<int>& sliceIndices) const;

    /// The ADC chain feeding one slice, or -1 when it is on none.
    ///
    ///     slice -> its stream -> that stream's ADC
    ///
    /// The single resolver for that hop. panBypassState calls it to decide
    /// the WIDE pill, and MainWindow calls it to paint the CH tag sitting
    /// beside that pill, so the two cannot report different chains for one
    /// pan. Do not reach for SliceModel::chainIndex() instead: nothing in
    /// production writes it, so it answers 0 for every slice.
    ///
    /// Takes a slice ID (see sliceById), not a list position -- the same key
    /// PanadapterApplet::associatedSlices holds. Returns -1 for an unknown id
    /// and for a slice that has not bound a stream; an unbound slice feeds
    /// off nothing, which is not the same as being on chain 0.
    int sliceChainIndex(int sliceId) const;

    /// The filter chain feeding one DDC stream, or -1 when the stream index
    /// is not a stream. sliceChainIndex is the by-slice front end of this;
    /// republishAlexAdcSlices and bypassReasonForAdc take the stream form
    /// because they are already iterating streams.
    ///
    /// CHAIN, not ADC, and the distinction is load-bearing (defect D4). A
    /// chain is one preselector bank plus the ADC behind it (design §16.1.1),
    /// and a board can have more ADCs than chains: ANAN-100D and ANAN-200D
    /// are both NetworkIO.SetRxADC(2) yet neither appears in the setAlex2HPF
    /// model list at Thetis console.cs:15435-15443 [v2.10.3.15], so both
    /// their ADCs sit behind one filter bank. On such a board a stream really
    /// can be routed to ADC1 and is still behind chain 0, so it is folded
    /// onto chain 0 here. That is the physical truth on a one-chain board,
    /// not a workaround: with one bank in front of both ADCs, every slice's
    /// range has to be counted against that one bank.
    int chainForStream(int stream) const;

    /// The wideband state a chain should actually be in, as opposed to what
    /// one slice just asked for.
    ///
    /// Codex review, PR #293. The widebandExtensionRequestedChanged handler
    /// forwarded the changing slice's boolean straight through, so on a chain
    /// hosting two slices whichever one cleared last switched the chain off
    /// while the other was still zoomed out: the Alex preselector came back in
    /// and the P2 wideband-enable bit dropped underneath a live extended view.
    /// The answer is a property of the chain, not of the slice that moved, so
    /// it is recomputed here as an OR across every live slice on it.
    ///
    /// Also the one place BoardCapabilities::widebandAdcs is honoured. That
    /// field was declared per SKU and read by nothing, so a board that cannot
    /// stream wideband at all still had its preselector forced into bypass by
    /// a zoom it could never satisfy, costing receive filtering for nothing.
    bool widebandActiveForChain(int chainIdx) const;

    /// Test seam for the above. Read-only, no production caller.
    bool widebandActiveForChainForTest(int chainIdx) const {
        return widebandActiveForChain(chainIdx);
    }

    /// Recompute widebandActiveForChain(chainIdx) and push the answer to the
    /// Alex preselector and, on Protocol 2, to the radio's wideband enable
    /// mask.
    ///
    /// Codex review round 3, PR #293. The first fix recomputed on the request
    /// property's own edges, which is not the only way the answer changes:
    /// removing the slice that was the sole requester alters it without any
    /// property moving, so the chain stayed bypassed and the radio kept
    /// streaming wideband until some unrelated slice happened to toggle. Both
    /// callers go through here so there is one push and not two copies of it.
    void pushWidebandStateForChain(int chainIdx);

    /// Push every filter chain's wideband state.
    ///
    /// Codex review round 4, PR #293, and the reason this is a sweep rather
    /// than another trigger. The answer for a chain changes on more inputs
    /// than one signal can name: a slice's request moving, a slice being
    /// removed, and a slice migrating between chains when its antenna moves
    /// its DDC to the other ADC. Each of those was found in a separate review
    /// round, because each was wired as its own hook and the next one was
    /// always missing.
    ///
    /// Reconciling every chain from current state instead makes the operation
    /// idempotent and complete: any input can change however it likes, and one
    /// sweep afterwards is correct. Called from publishDdcAssignment, which
    /// already recomputes DDC, chain and psPaused for every slice the same
    /// way, so chain migration is covered by construction.
    void reconcileWidebandForAllChains();

    /// Operator-facing sentence naming WHY the given chain is bypassed.
    /// One string per cause, per design doc §16.4.4. Public so the Filter
    /// Policy dialog can show the same wording the badge tooltip carries.
    QString bypassReasonForAdc(int adc,
                               const AlexController::AlexAdcState& st) const;

    // Band-plan overlay manager — loaded once on construction from bundled
    // Qt resource JSON files. Active plan persists in AppSettings under
    // "BandPlanName". Phase 3G RX Epic sub-epic D.
    const BandPlanManager& bandPlanManager()        const { return m_bandPlanManager; }
    BandPlanManager&       bandPlanManagerMutable()       { return m_bandPlanManager; }

    // Apollo PA + ATU + LPF accessory controller — present/filter/tuner enable flags.
    // Loaded per-MAC at connect time. Setup UI deferred (Phase 3P-F Task 5a).
    // Sprachspeicher: zehn Ansagen mit Tasten (2026-08-19). Hier und
    // nicht im Applet, damit die Tastenkuerzel spaeter global greifen
    // und die Wiedergabe an den Sendeweg kommt, ohne dass zwei Stellen
    // dieselben Plaetze fuehren.
    VoiceKeyerStore& voiceKeyer() { return m_voiceKeyer; }
    const VoiceKeyerStore& voiceKeyer() const { return m_voiceKeyer; }
    // Senden, Wiederholen und Aufnehmen der Ansagen (2026-11-10): ein
    // Spieler fuer Panel und Tastenkuerzel. Tastet ueber den
    // MoxController, spielt ueber den Mikrofon-Router (MicSource::Dvk).
    VoiceKeyerPlayer* voiceKeyerPlayer() const { return m_voiceKeyerPlayer.get(); }
    Ts590Sprache* ts590Sprache() const { return m_ts590Sprache.get(); }
    // Die Tastenkuerzel (2026-11-10). Hier, damit Setup-Seite und
    // Sprachspeicher-Panel dieselbe Belegung sehen wie die Tastaturweiche
    // im Hauptfenster. Gelesen wird sie vom Hauptfenster, sobald dessen
    // Menues stehen (die Menue-Kuerzel sind fuer Tasten gesperrt).
    HotKeyBindings* hotKeyBindings() const { return m_hotKeyBindings.get(); }

    // Die QSO-Aufnahme. Sie haengt an AudioEngine und TxWorkerThread,
    // sobald beide stehen — angeschlossen wird in wireConnectionSignals,
    // damit sie einen Verbindungswechsel ueberlebt.
    QsoRecorderController& qsoRecorder() { return m_qsoRecorder; }
    const QsoRecorderController& qsoRecorder() const { return m_qsoRecorder; }

    /// Die Aufnahme aus Phase 3M (Ton, I/Q, Zeitplan). Liegt hier und
    /// nicht im Fenster, aus demselben Grund wie der QSO-Mitschnitt
    /// eine Zeile hoeher: eine Flaeche darf verschwinden, eine
    /// laufende Aufnahme nicht.
    RecordingModel& recording() { return m_recording; }
    const RecordingModel& recording() const { return m_recording; }

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
    /// Thetis' checkAntennaWarning in einer Zeile: der Text, mit dem der
    /// Schluessel bei offener Antenne faellt und zugeriegelt bleibt.
    /// Cite: console.cs:26166-26174 [v2.10.3.15].
    static QString openAntennaWarningText();

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

    /// The slice whose SliceModel::sliceIndex() equals `sliceId`, or nullptr.
    ///
    /// Slice ids are stable for the life of a slice and are NOT list
    /// positions: addSlice hands out the lowest free id and removeSlice does
    /// not renumber the survivors, so the two diverge after any mid-list
    /// removal. The id doubles as the slice's WDSP RX channel id.
    /// For positional access, index slices() directly.
    SliceModel* sliceById(int sliceId) const;

    SliceModel* activeSlice() const { return m_activeSlice; }

    /// Hand the transmitter to the slice with this ID, RF-safely.
    ///
    /// Takes a slice ID (see sliceById), not a list position, and converts.
    /// TxSliceArbiter::requestHandoff is positional, but every per-slice UI
    /// surface carries the stable id -- PanadapterApplet::activeSliceIndex()
    /// is resolved through sliceById by the status-overlay refresh -- so
    /// handing one straight to the other picks the wrong slice, or none at
    /// all, as soon as a mid-list removal makes ids and positions diverge.
    /// With A(0) B(1) C(2), removing B leaves C at id 2 / position 1: the
    /// unconverted call asks for position 2 and is rejected, so the
    /// transmitter silently stays where it was.
    ///
    /// Returns false without moving anything when the id resolves to no
    /// slice, or when there is no arbiter. Delegates the MOX drop to
    /// TxSliceArbiter::requestHandoff rather than reproducing it: that
    /// sequence is the whole reason the arbiter owns this.
    ///
    /// Factored out of the MainWindow badge handler so both the pan TX badge
    /// and a test can reach it without standing up a MainWindow, the same way
    /// requestSliceSampleRate is.
    bool requestTxHandoffToSlice(int sliceId);

    /// Phase 3F: hardware-capped user-facing slice count. Reads BoardCapabilities.maxSlices
    /// for the currently connected SKU. Returns 1 when disconnected (safe default).
    /// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.
    int maxSlices() const;

    // ── Phase 3F Sub-Epic I: DDC stream pool ────────────────────────────
    //
    // A stream is one hardware DDC plus its ReceiverManager receiver, its
    // FFTEngine, its panadapter window, and its noise blanker. Streams are
    // opened once at connect and reused, mirroring Thetis CreateRadio
    // (cmaster.cs:516 [v2.10.3.15]) and deskhpsdr's create-all loop
    // (radio.c:1259 [@f3d857c]). Neither upstream opens a WDSP channel at
    // runtime.
    //
    // Slices bind many-to-one: several whose frequencies fall inside one
    // window share it, differing only by shift offset. An idle stream costs
    // memory; its DDC stays out of the ddcEnable bitmask so the radio never
    // streams it.

    /// Size the pool to the connected SKU and clear all bindings.
    void configureStreamPool(int userDdcCount, int maxSlices, int defaultRateHz);

    /// Open the WDSP RX channel pool: one channel per slice the SKU allows.
    ///
    /// `poolSize` is BoardCapabilities::maxSlices; values below 1 are treated
    /// as 1, and anything above WdspEngine::kMaxSliceChannels is CLAMPED, not
    /// honoured. The clamp is the collision guard: WDSP's channel table is one
    /// global array (channel.c:29) and OpenChannel overwrites whatever sits at
    /// the id without closing it, so a pool that ran past kMaxSliceChannels
    /// would silently take over kTxChannelId and orphan the TXA's thread. A SKU
    /// that wants a bigger pool has to raise the reserved block, which moves TX
    /// and PS with it.
    ///
    /// Already-open channels are left alone, so this is idempotent and safe to
    /// re-run on reconnect.
    ///
    /// Called from the WdspEngine::initializedChanged lambda in
    /// connectToRadio(), after Slice A's channel has its state.
    void openRxChannelPool(int poolSize, int inputBufferSize,
                           int inputSampleRateHz);

    /// Switch on the WDSP channel of every slice that currently holds a stream.
    ///
    /// A pooled channel is opened stopped (WDSP `initial state = 0`, Thetis
    /// ChannelMaster/cmaster.c:80 [v2.10.3.15]) and RxChannel::processIq
    /// memsets its output to silence until setActive(true). Called at the tail
    /// of openRxChannelPool so a reconnect, which re-binds every slice before
    /// WDSP has any channels, still comes back with all of them audible.
    void activateBoundSliceChannels();

    /// Reconcile every OPEN pool channel with the current notch set.
    ///
    /// Design section 6.3. Runs at the tail of openRxChannelPool because
    /// activateSliceChannel is dead as a hook for Slice A: connectToRadio's
    /// WDSP-init lambda activates channel 0 before it opens the pool, and
    /// activateSliceChannel early-returns on an already-active channel.
    /// Covers reconnect for free, since teardownConnection destroys every
    /// channel.
    ///
    /// Reconciles channels that no slice is bound to yet as well: a notch
    /// database is created inert (third_party/wdsp/src/RXA.c:87) and this is
    /// where the run flag lands.
    void syncNotchesToAllChannels();

    /// Switch on one slice's WDSP channel, pushing its demodulation state
    /// first. No-op when the slice has no stream, has no channel yet, or is
    /// already live (Slice A, which connectToRadio activates after the full
    /// state push).
    ///
    /// Mirrors Thetis's receiver-enable order: push the DSPRX state, then
    /// SetChannelState(ch, 1, 0) (console.cs:37359-37361 [v2.10.3.15]).
    void activateSliceChannel(SliceModel* slice);

    /// Stop a slice's WDSP channel running. The channel object stays open for
    /// whichever slice takes the id next. Thetis's disable half:
    /// SetChannelState(ch, 0, 0) (console.cs:37398-37400 [v2.10.3.15]).
    void deactivateSliceChannel(int sliceId);

    /// Run the allocator for every slice that currently has no stream.
    ///
    /// Connect-time step: Slice A is created before the pool is sized, so its
    /// addSlice-time bind was a no-op, and after a teardown every slice is
    /// unbound (see releaseStreamBindings).
    void bindUnboundSlices();

    /// Push every stream's current slice set to the DSP worker.
    ///
    /// Phase 3F Sub-Epic I closeout, defect F1. connectToRadio sizes the pool
    /// and binds every slice BEFORE wireConnectionSignals constructs
    /// m_dspWorker, so each of those binds published into a null pointer and
    /// the freshly-built worker started life knowing only its constructor's
    /// seed ({stream 0: [slice 0]}). Anything on a non-zero stream then
    /// demodulated nothing until the operator happened to retune it. Called
    /// from wireConnectionSignals once the worker exists.
    void republishAllStreamBindings();

    /// Drop every slice's stream binding and idle the whole pool.
    ///
    /// Phase 3F Sub-Epic I closeout, defect F1. Teardown destroys the DSP
    /// worker but the slices kept their streamIndex, so the reconnect bind
    /// loop in connectToRadio (guarded on streamIndex() < 0) skipped them all
    /// and nothing was ever republished to the new worker. Deactivating the
    /// allocator's streams here keeps its bookkeeping consistent with the
    /// slices that just became unbound, rather than leaving live streams that
    /// no slice claims.
    void releaseStreamBindings();

    int streamPoolSize() const;
    int activeStreamCount() const;

    /// SliceModel::sliceIndex() of every slice currently bound to a stream,
    /// ascending. These double as WDSP channel ids (Sub-Epic I invariant:
    /// WDSP RX channel id == slice index).
    QVector<int> slicesOnStream(int streamIndex) const;

    /// Recompute every slice's shift oscillator against a new stream centre.
    ///
    /// A shared DDC window has one centre and N slices sitting at their own
    /// offsets inside it. When the centre moves (a CTUN drag, a band jump),
    /// each member's shift is (frequency - newCentreHz). Missing a co-host
    /// leaves it demodulating the wrong signal while its flag still reads
    /// the right number.
    ///
    /// Deliberately does NOT move the allocator's own centre: this is called
    /// from the CTUN drag, which retunes the DDC through
    /// ReceiverManager::forceHardwareFrequency precisely because it is
    /// bypassing the allocator's placement policy. Callers that DO own the
    /// placement (bindSliceToStream) already write the shift themselves.
    void reshiftSlicesOnStream(int streamIndex, double newCentreHz);

    /// Phase 3F Sub-Epic I Task 7b: hardware DDC currently routed to
    /// `streamIndex`, or -1 when that stream is idle (or no codec has run).
    /// This is the codec's choice, republished; every slice on the stream
    /// reports the same number through SliceModel::ddcIndex().
    int ddcForStream(int streamIndex) const;

    /// Phase 3F Sub-Epic I Task 10: change a DDC stream's sample rate.
    ///
    /// The rate IS the window width, so widening lets more slices share this
    /// stream and narrowing can push slices out of it. Every slice bound to
    /// the stream is therefore re-run through the allocator afterwards: an
    /// evicted slice migrates to a free DDC rather than being left aliased
    /// on a window that no longer contains it.
    ///
    /// Protocol 1 carries ONE rate for the whole radio in C&C bank 0
    /// (P1RadioConnection::composeCcBank0 takes a single sampleRate and
    /// encodes it as srBits), so on a P1 connection this applies the rate to
    /// every active stream. Protocol 2 carries a per-DDC rate in
    /// DdcAssignment::rate[], which the codecs populate per stream, so there
    /// it applies only to the stream named.
    bool setStreamSampleRate(int streamIndex, int rateHz);

    /// Phase 3F Sub-Epic I closeout, defect G2: the operator picked a sample
    /// rate on one slice's VFO flag.
    ///
    /// Takes a slice ID (see sliceById), not a list position, because that is
    /// what VfoWidget carries. Resolves the slice to its DDC stream and hands
    /// off to setStreamSampleRate, so the request necessarily resolves to a
    /// stream-wide rate: co-hosted slices share one DDC and cannot hold
    /// different widths. Unknown or unbound slices are ignored.
    ///
    /// This is the body of MainWindow's sampleRateRequested handler, factored
    /// out so both flag-wiring sites share it and so it is reachable from a
    /// test without a MainWindow.
    void requestSliceSampleRate(int sliceId, int rateHz);

    /// Push a slice's just-restored per-band sample rate onto its DDC.
    ///
    /// Codex review round 7, PR #293. SliceModel::restoreFromSettings reads
    /// the persisted per-band SampleRate and calls setSampleRateHz, which is
    /// a plain property setter: it moves the number the VFO menu displays
    /// and nothing else. The rate the receiver, codec and wire actually run
    /// at only changes through requestSliceSampleRate. So returning to a
    /// band you had left at 384 kHz showed 384 kHz while the DDC stayed on
    /// whatever the previous band was using, and the saved preference was in
    /// effect never restored.
    ///
    /// Called after restoreFromSettings rather than inside it: the restore
    /// sets the frequency first, which completes the allocator rebind, and
    /// the rate transaction has to run against the stream the slice ended up
    /// on. SliceModel also holds no RadioModel handle by design, and the
    /// rate is a stream-wide transaction rather than a slice property.
    void applyRestoredSampleRate(SliceModel* slice);

    /// True unless the connected radio's allowedStreamSampleRates() is known
    /// and does not contain rateHz. Guards the per-band restore, whose keys
    /// are not per radio.
    bool restoredRateAllowed(int rateHz) const;

    /// True when one sample rate covers the whole radio rather than one DDC.
    ///
    /// Protocol 1 encodes the rate as srBits in C&C bank 0
    /// (P1RadioConnection::composeCcBank0 takes a single sampleRate), so every
    /// stream shares it and setStreamSampleRate fans a change across all of
    /// them. Die SunSDR ebenso: ihr Stromstart-Rahmen 0x01 traegt einen Modus
    /// fuer das ganze Geraet (SunSdrProtocol.h, StromModus). Protocol 2
    /// carries a per-DDC rate in DdcAssignment::rate[]. UI that offers the
    /// rate from a per-slice surface has to disclose the radio-wide scope
    /// rather than imply a private rate. False when disconnected.
    ///
    /// Das ist nicht nur eine Beschriftung: nur fuer "radio-wide" schickt
    /// setStreamSampleRate die Aenderung ueber setSampleRateLive auf den
    /// Draht. Der Weg fuer eine Rate je DDC ist der DdcAssignment-Push, und
    /// der bedient ausschliesslich P2 (invokeCodecDdcAssignment). Eine
    /// Verbindung, die hier faelschlich false meldet, stellt ihren
    /// Empfangskanal um und laesst das Geraet auf der alten Rate stehen
    /// (2026-10-04 an der SunSDR2 QRP gemessen).
    bool sampleRateIsRadioWide() const;

    /// Sample rates the connected radio accepts, ascending; empty when
    /// disconnected.
    ///
    /// Thin wrapper over SampleRateCatalog::allowedSampleRates with the live
    /// protocol, board capabilities and SKU. Rate pickers must filter through
    /// this: P1 saturates srBits at 3 for anything >= 384 kHz, so offering a
    /// P2-only rate on a P1 board would leave the client configured for a
    /// width the radio is not sending.
    QVector<int> allowedStreamSampleRates() const;

    /// Die Rate, mit der der WDSP-Kanal von Empfaenger 0 gerade WIRKLICH
    /// laeuft; 0 ohne Verbindung. Fuer die RATE-Anzeige der Knopfleiste
    /// (2026-09-24) -- die GUI fragt keinen Kanal direkt.
    int rx0ChannelRateHz() const;

    // Phase 3F bench fix 2026-06-03: optional initialPanId is stamped on the
    // new SliceModel as a dynamic property BEFORE sliceAdded() emits, so the
    // MainWindow handler can route the VfoWidget to the owning pan. Passing
    // an empty string preserves the legacy single-pan behaviour.
    /// Returns the new slice's id — the lowest not currently in use, which
    /// is also its WDSP RX channel id and its A-E display letter. Returns
    /// -1 if the allocator refused to place it, in which case nothing is
    /// added: see the rollback in the definition.
    ///
    /// Whether the slice gets its own receiver window or shares an existing
    /// one is DERIVED from `initialPanId`, not passed in: a pan with no
    /// other slices is new and needs its own, a pan that already has slices
    /// is a host and sharing it is the point. It was briefly a caller-
    /// supplied flag; every caller that forgot it silently reintroduced the
    /// coupled-pan defect, so the decision lives with the data it depends on.
    /// suppressAutoStreamBinding: for a slice a foreign accessory (SunSDR,
    /// KiwiSDR) creates for itself, not for an operator-facing pan. Skips
    /// the initial bindSliceToStream() attempt below and marks the slice so
    /// its own frequencyChanged→bindSliceToStream wiring (also below) stays
    /// off too -- both would otherwise hand the slice a real streamIndex()
    /// the moment the accessory mirrors its own VFO onto it, which the
    /// safety code watching that field reads as "a real radio took over"
    /// (see SliceModel::m_suppressAutoStreamBinding). Cleared automatically
    /// the moment a real radio actually does claim the slice via
    /// bindUnboundSlices().
    int addSlice(const QString& initialPanId = QString(),
                 bool suppressAutoStreamBinding = false);

    /// Takes a slice ID (see sliceById), not a list position. sliceRemoved
    /// carries the same id.
    void removeSlice(int sliceId);

    /// NOTE: still a LIST POSITION, unlike sliceById / removeSlice above.
    /// This API remains positional for internal list navigation only.
    ///
    /// Prefer setActiveSliceById below for anything driven by a UI surface:
    /// every per-slice widget carries the stable id, not the position.
    void setActiveSlice(int index);

    /// Make the slice with this ID the active one.
    ///
    /// UI surfaces that select a slice all carry the stable slice ID
    /// (VfoWidget::sliceActivationRequested emits what createSliceFlag
    /// stamped from SliceModel::sliceIndex(); RxApplet::updateSliceButtons
    /// keys its button group the same way), while setActiveSlice above
    /// indexes m_slices positionally. Ids and positions diverge after any
    /// mid-list removal, because removeSlice does not renumber survivors.
    /// With A(0) B(1) C(2), closing B leaves C at id 2 / position 1: the
    /// unconverted call asked for position 2 of a two-element list and
    /// selected nothing at all, so clicking flag C did nothing.
    ///
    /// Returns false, changing nothing, when the id resolves to no slice.
    /// Leaving the previous active slice in place matters: every
    /// active-slice surface (container S-meter, RX applet, DSP menu) would
    /// otherwise be stranded on nullptr by one stale click.
    bool setActiveSliceById(int sliceId);

    /// Phase 3F Sub-Epic C Task 7: AetherSDR-faithful slice creation entry
    /// point.  Creates a new SliceModel (delegates to addSlice) and tags it
    /// with the supplied pan id as a dynamic property for Sub-Epic D wiring.
    /// Enforces the maxSlices() cap and emits sliceAddRejected with a human
    /// readable reason on overflow.
    /// Pattern from AetherSDR MainWindow.cpp:6849-6859 [@0cd4559a]
    /// (+RX button handler).
    Q_INVOKABLE void addSliceOnPan(const QString& panId);

    /// Re-point any slice whose pan is not in `livePanIds` at the first one
    /// that is. Returns how many moved.
    ///
    /// Codex review, PR #293. Shrinking the pan layout deleted the omitted
    /// PanadapterApplets, but the slice-side loop in MainWindow only ever
    /// added (`for (i = existing; i < target; ++i)`), so with existing >
    /// target its body never ran. Slices whose panKey named a deleted pane
    /// were left pointing at nothing: their VFO widgets went with the pane,
    /// re-expanding did not re-associate them because nothing emitted
    /// panKeyChanged, and they went on holding a DDC, a stream and audio.
    ///
    /// Rehomes rather than removes. A slice carries the operator's frequency,
    /// mode, filter and DSP state, and throwing that away as a side effect of
    /// picking a smaller layout is destructive and was never asked for.
    ///
    /// An empty `livePanIds` is a no-op: there is nowhere to move to, and
    /// leaving a slice on a stale pan id beats pointing it at an empty string
    /// that no pane will ever match.
    ///
    /// Lives here rather than in the MainWindow lambda that has the defect,
    /// because MainWindow is not constructible in the test harness and logic
    /// put there cannot be tested at all.
    int rehomeSlicesToPans(const QStringList& livePanIds);

    /// Which of `panIds` currently host no slice, in the order given.
    ///
    /// Codex review round 4, PR #293, and a regression rehomeSlicesToPans
    /// created. The layout handler decided which pans to populate with
    /// `for (i = slices().size(); i < target; ++i)`, which assumes the slice
    /// COUNT is the first unoccupied pan index. Rehoming breaks that: shrink
    /// a 2x2 to one pane and all four slices land on pan-0, so expanding back
    /// has existing == target == 4, the loop adds nothing, and three panes
    /// come up with no VFO and no RX entry. Co-hosting slices by hand, or
    /// removing a non-final slice id, breaks the same assumption.
    ///
    /// Occupancy is the question the caller is actually asking, so it is the
    /// question answered here. Co-hosted slices count once: a pan with three
    /// slices on it is occupied, not three-times occupied.
    QStringList pansWithoutSlices(const QStringList& panIds) const;

    /// Slices currently living on `panId`, optionally skipping one.
    ///
    /// `except` exists for the add path: addSlice appends the new slice to
    /// m_slices before binding it, so asking "does this pan already have
    /// slices" would otherwise always answer yes and count the newcomer
    /// itself. That distinction decides whether the slice opens a new
    /// receiver or joins an existing one, so getting it wrong is the whole
    /// difference between two independent pans and two views of one.
    QVector<SliceModel*> slicesOnPan(const QString& panId,
                                     const SliceModel* except = nullptr) const;

    /// Move surplus co-hosted slices onto pans in `panIds` that have none.
    /// Returns how many moved.
    ///
    /// Codex review round 5, PR #293, and a regression pansWithoutSlices
    /// created. After a 2x2 shrinks to one pane every slice sits on pan-0;
    /// expanding again finds three empty pans, and creating a slice for each
    /// spends the maxSlices budget on NEW slices while four co-hosted ones sit
    /// idle. On a five-slice radio that fills pan-1, hits the cap, and leaves
    /// pan-2 and pan-3 empty with a surplus fifth slice in the model.
    ///
    /// The slices needed are already there, so they are moved before any are
    /// made. Only genuinely surplus ones move: a pan holding a single slice is
    /// never raided, or expanding would just relocate the hole.
    int spreadSlicesOntoEmptyPans(const QStringList& panIds);

    /// Phase 3F closeout — public helper for invoking the antennaAutoSwitched
    /// signal from operator surfaces (Tools menu "Test antenna switch toast"
    /// entry) and from future conflict-detection logic in AlexController.
    /// Plain helper avoids the Qt private-signal access dance for callers.
    Q_INVOKABLE void emitAntennaAutoSwitched(int sliceIndex,
                                              const QString& oldAntenna,
                                              const QString& newAntenna);

    /// Phase 3F closeout — Sub-Epic E Task 7 consumer wire surface. Emits
    /// txBoundReRouteRequested(proposedAntenna, existingAntenna). Today this
    /// is invoked only from the Tools menu test entry that exercises the
    /// TxBoundConfirmDialog surface; real emission from addSliceOnPan when
    /// the slice-add would force a TX-bound chain re-route lands when the
    /// conflict-detection state machine ships.
    Q_INVOKABLE void requestTxBoundReRoute(const QString& proposedAntenna,
                                            const QString& existingAntenna);

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
    void onBandButtonClicked(Longpath::Band band);

    /// Richtet das Seitenband von `slice` nach `band` aus (160/80/40 m
    /// auf LSB, darueber USB) und legt die Korrektur ab. Beruehrt nur
    /// Sprechseitenbaender; CW, Digital, AM und FM bleiben stehen.
    void richteSeitenbandAus(SliceModel* slice, Longpath::Band band);

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

    // ── Fertiges Spektrum, gleich welches Geraet es gerechnet hat ────────
    //
    // Wer ein fertiges Bild hat, meldet es hier; wer eines braucht (der
    // TCI-Spektrumstrom), haengt an `spektrumBereit`. Bis 2026-11-02 hing
    // der TCI-Server stattdessen an `fftEngine()`, also an der FFTEngine von
    // Strom 0 — und die rechnet nur fuer OpenHPSDR-Geraete. Die Flex liefert
    // ihr Spektrum selbst (`flexSpectrumReady`), die SunSDR rechnet in einer
    // Engine mit Pseudo-Stromnummer: bei beiden feuerte `fftReady` von
    // Engine 0 nie, und der Strom blieb leer, obwohl das Protokoll Erfolg
    // meldete (0 Rahmen in 8 s gemessen, waehrend Longpath selbst ein
    // lebendes 40-m-Bild zeigte).
    //
    // `rx` ist die TCI-Empfaengernummer (0 oder 1), `spanneHz` die Breite,
    // die die Bins zusammen abdecken — die Abtastrate bei einer FFTEngine,
    // der Panadapter-Ausschnitt bei der Flex. 0 heisst unbekannt.
    //
    // Faden: darf von JEDEM Faden gerufen werden (die FFTEngine ruft vom
    // FFT-Faden). Das Signal wird dann von dort gesendet; wer anhaengt,
    // bekommt es ueber Qt::AutoConnection in seinem eigenen Faden.
    //
    // `mitteHz` (seit 2026-11-03, zweiter Schritt): die Frequenz in der
    // Bildmitte. 0 heisst unbekannt; der TCI-Server nimmt dann den
    // Strom-Mittelpunkt des Zuteilers (streamCentreHz). Die Flex sagt sie
    // selbst — dort ist `dds:` nur das VFO, nicht die Bildmitte.
    void meldeSpektrum(int rx, const QVector<float>& dbm, int spanneHz,
                       double mitteHz = 0.0)
    {
        emit spektrumBereit(rx, dbm, spanneHz, mitteHz);
    }

    // Mittelfrequenz des Stroms (DDC-Mitte), wie der Zuteiler sie fuehrt;
    // 0, wenn der Strom nicht vergeben ist. Fuer den TCI-Spektrumstrom.
    double streamCentreHz(int streamIndex) const
    {
        return m_streamAllocator.streamCentreHz(streamIndex);
    }

    // Der Flex-Einstieg: ein fertiger FFT-Rahmen der Maschine. Sendet
    // `flexSpectrumReady` (fuer das Fenster) UND meldet ueber
    // meldeSpektrum() als Empfaenger 0 mit dem Panadapter-Ausschnitt als
    // Spanne. Oeffentlich und benannt, damit ein Pruefstand genau diesen
    // Weg fahren kann — es war der Weg, auf dem am 2026-11-02 nichts ankam.
    void meldeFlexSpektrum(quint32 streamId, const QVector<float>& dbm);

    // Pruefstand: den Panadapter-Ausschnitt der Maschine setzen, ohne eine
    // Flex-Statuszeile zu zerlegen.
    void setFlexAusschnittForTest(double tiefHz, double hochHz)
    {
        m_flexAusschnittTiefHz = tiefHz;
        m_flexAusschnittHochHz = hochHz;
    }
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
    void setNoiseFloorTracker(NoiseFloorTracker* t);

    /// Messabschnitt fuer Auto AGC-T (Upstream NereusSDR e5b3997bc, #720).
    /// Ungerade: gesperrt -- keine Verbindung, MOX, oder der MoxController
    /// ist nicht in Rx (also auch der Weg hinein und heraus). Gerade: es
    /// wird empfangen. Jeder Wechsel zaehlt eins weiter; forkIqToTaps
    /// stempelt jeden I/Q-Block mit dem Stand beim Eintreffen.
    quint64 autoAgcAcquisitionEpoch() const noexcept {
        return m_autoAgcAcquisitionEpoch.load();
    }
    bool autoAgcAcquisitionInhibited() const noexcept {
        return (autoAgcAcquisitionEpoch() & 1U) != 0;
    }

    /// Register the tracker measuring one DDC stream's band.
    ///
    /// Auto AGC-T derives its threshold from the noise floor, so a slice has
    /// to measure the band it is on. With one shared tracker (stream 0's), a
    /// 20m slice would take its threshold from 40m's noise floor.
    void setStreamNoiseFloorTracker(int streamIndex, NoiseFloorTracker* t);

    /// The tracker for this slice's stream, falling back to the global one
    /// when the slice is unbound or its stream has no tracker yet.
    NoiseFloorTracker* noiseFloorTrackerForSlice(const SliceModel* s) const {
        if (s) {
            const int stream = s->streamIndex();
            if (stream >= 0) {
                if (NoiseFloorTracker* t = m_streamNoiseFloors.value(stream, nullptr)) {
                    return t;
                }
            }
        }
        return m_noiseFloorTracker;
    }
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

    // Phase 3F Sub-Epic C: TX-slice arbiter (single-TX invariant + RF-safe
    // handoff). Owned by RadioModel (Qt parent), wired to slice list +
    // MoxController during construction. MAC injected + load() driven on
    // every currentRadioChanged emit; save() runs from teardownConnection.
    // Used by the upcoming VfoWidget TX-badge click handoff path and any
    // future code that needs the authoritative TX-bound slice index.
    TxSliceArbiter* txSliceArbiter() const { return m_txSliceArbiter; }

    // The slice bound to the transmitter — the source of every transmit
    // frequency. NOT activeSlice(), which is only the slice the operator is
    // looking at; in multi-slice those diverge, and taking the transmit
    // frequency from the wrong one puts the PA on the wrong band (and, via
    // the Alex low-pass, behind the wrong filter).
    //
    // Thetis draws the same distinction: its VFO A arm is guarded by
    // `!chkVFOBTX.Checked` so it stands down when VFO B is transmitting
    // (console.cs:31889-31893 [v2.10.3.15]), and the VFO B handler assigns
    // tx_dds_freq_mhz itself in that case (console.cs:32866-32869).
    //
    // Returns nullptr when no arbiter binding resolves. TX-global callers
    // must fail safely rather than substituting listening/UI state.
    SliceModel* txBoundSlice() const;

    // Phase 3F Sub-Epic D Task 13: Longpath-original FFT fan-out router.
    // Wires receiverId -> N pans so a single DDC FFT pipeline can feed
    // multiple zoom levels of the same I/Q data. MainWindow registers
    // pan-to-receiver mappings on sliceAdded; the per-receiver FFTEngine
    // fan-out pump is wired in Sub-Epic E / F polish (the routing table
    // is correct as soon as the mappings are populated).
    class FFTRouter* fftRouter() const { return m_fftRouter; }

    // 3M-1c Phase L.1: expose MicProfileManager so MainWindow / SetupDialog
    // can hand the per-MAC profile bank to TxApplet (J.1 setter) and
    // TxProfileSetupPage (J.3 ctor).  Non-owning; lifetime is RadioModel's
    // lifetime.  See header §3M-1c L.1 for the construction + connect flow.
    MicProfileManager* micProfileManager() const { return m_micProfileMgr; }

    // Receive profiles (AGC / NR / NB / ANF / squelch of a slice under a
    // name), global rather than per MAC. Non-owning; RadioModel's lifetime.
    RxProfileManager* rxProfileManager() const { return m_rxProfileMgr; }

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

    /// 2026-08-13 SWR sweep analyzer (radio as antenna analyzer).
    /// Non-null from construction; MainWindow hands it to the Antenna
    /// window's sweep tab. Telemetry-fed from handlePaTelemetry.
    SwrSweepController* swrSweepController() const { return m_swrSweep; }

    // ── The directional coupler's raw counts, as last reported ───────
    //
    // Unscaled, straight off the wire. Everything else in the program
    // shows watts, and watts are the far end of a chain — ADC count,
    // per-board triplet, calibration — so a wrong number there does not
    // say which link is at fault.
    //
    // 2026-08-14 cost a day to that: the forward reading sat at 0.01 W
    // and no display in the program could say whether the radio was
    // reporting nothing or the scaling was eating it. These two numbers
    // answer that in one glance. The PA Values page in Setup has shown
    // them all along, which nobody found.
    quint16 lastFwdAdcRaw() const noexcept { return m_lastFwdRaw; }
    quint16 lastRevAdcRaw() const noexcept { return m_lastRevRaw; }

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

    // Bandstapel (2026-11-11): je Band eine Liste gespeicherter Stellen;
    // ein Druck auf die Bandtaste geht zur Stelle, ein weiterer zur
    // naechsten. Angelegt am Ende des Konstruktors, lebt so lange wie das
    // Modell. Siehe src/models/bandstack/BandStackController.h.
    BandStackController* bandStack() const { return m_bandStack; }

    // ── CW-Tonhoehe (2026-09-27) ───────────────────────────────────────────
    // Thetis console.cs CWPitch: EIN Wert fuer alle Empfaenger, der die
    // CW-Filter, den APF und die Anzeige traegt. Gespeichert in
    // AppSettings "CWPitch" (SliceModel::cwPitchHz liest ihn), gesetzt
    // nur hier: setCwPitch klemmt auf 200..2250, schiebt die CW-Vorgaben
    // (FilterPresetStore), die gespeicherten und die laufenden CW-
    // Durchlaesse jeder Scheibe und die APF-Mitte nach und meldet
    // cwPitchChanged -- nur, wenn sich der Wert wirklich geaendert hat.
    // Senden (Mithoerton, VFO-Versatz bei MOX) ist bewusst NICHT dabei:
    // 3M-2 CW-TX ist zurueckgestellt.
    int  cwPitch() const;
    void setCwPitch(int hz);

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
    // Roundtable (2026-11-14): die Bruecke zwischen dem Client und den
    // Scheiben; die anderen Stationen landen als Spots in spotModel().
    RoundtableHub*        roundtableHub()       const { return m_roundtableHub.get(); }

    // ── Der Rotor, ohne Fenster dazwischen (2026-10-05) ──────────────────
    //
    // Hier steht eine REGISTRIERUNG, kein Besitz: angelegt wird der
    // RotctldClient weiterhin von `RotorLogbookPanel`, das ihn auch
    // bedient. Diese Stelle gibt es nur, damit der TCI-Server ihn
    // erreichen kann, ohne in ein Fenster zu greifen -- derselbe Grund,
    // aus dem `WorkedBefore` beim Logbuch nicht angefasst wurde: ein
    // Netzdienst, der an einem Fenster haengt, stirbt mit dem Fenster.
    //
    // DER SAUBERERE WEG waere, den Besitz hierher zu holen (wie bei
    // `m_spotModel`, das 2026-05-12 aus `SpotHubDialog` hierher kam).
    // Das ist eine Architekturaenderung an einem Geraet, das an Martins
    // Mast haengt, und CLAUDE.md verlangt dafuer den Betreiber. Bis
    // dahin ist dies der kleine, umkehrbare Schritt.
    //
    // QPointer, nicht roher Zeiger: wird das Fenster geschlossen, steht
    // hier nachher nullptr statt einer Leiche.
    RotorController*      rotor()               const { return m_rotor; }
    void setRotor(RotorController* r)                 { m_rotor = r; }

    // ── Das Begleitgeraet (2026-10-28) ──────────────────────────────
    //
    // Hier BESITZ, nicht Registrierung -- anders als beim Rotor eine
    // Zeile darueber. Der Grund dort war, dass der Rotor am Mast des
    // Betreibers haengt und eine Besitzumstellung eine
    // Architekturaenderung an einem laufenden Geraet gewesen waere.
    // Das Begleitgeraet ist neu und haengt an nichts: es kann von
    // Anfang an dort liegen, wo es hingehoert.
    //
    // Der Unterschied ist nicht akademisch. Stufe 1 hatte die
    // Verbindung in der Einstellseite, und eine SetupPage wird bei
    // jedem Oeffnen neu gebaut: die Verbindung war beim ersten Blick
    // auf eine andere Seite weg.
    //
    // Angelegt im Ctor, NICHT verbunden. Verbunden wird in
    // MainWindow, nachdem die Oberflaeche steht -- und auch dann nur,
    // wenn der Bediener es eingeschaltet hat und dieser Lauf von
    // selbst ins Netz darf.
    CompanionRigController* companionRig() const { return m_companionRig.get(); }
    // 2026-05-12 bench fix: moved from SpotHubDialog ownership so the
    // table stays populated from app start regardless of whether the
    // dialog is open.  Spots from auto-connected sources were
    // previously dropped on the floor until the user opened Tools →
    // Spot Hub for the first time (the SpotTableModel didn't exist
    // before then), forcing a manual disconnect+reconnect to repopulate.
    SpotTableModel*       spotTableModel()      const { return m_spotTableModel.get(); }
    FreeDVStationModel*   freeDvStationModel()  const { return m_freeDvStationModel.get(); }
    RxDecodeModel*        rxDecodeModel()       const { return m_rxDecodeModel.get(); }

    /// Ein Fund des EIGENEN FT8/FT4-Dekoders (Ft8Applet, 2026-10-15).
    ///
    /// Derselbe Weg wie bei WSJT-X — SpotModel mit Entdopplung,
    /// RxDecodeModel, PSK Reporter —, nur mit eigener Quelle
    /// („Longpath"), damit sich die beiden in der Liste nicht
    /// vermischen. Ohne das blieben die eigenen Funde in einem
    /// Textfeld und erreichten die Panadapter-Ueberlagerung nie.
    ///
    /// OEFFENTLICH, anders als die Adapter-Schlitze der
    /// Netz-Quellen weiter unten: die haengen an einem Client, der
    /// sich selbst verbindet. Hier ruft das Applet herein, und ein
    /// `connect` auf einen privaten Schlitz geht in der
    /// Zeiger-Schreibweise nicht.
    void onNativeFt8Decode(const Longpath::DxSpot& spot);
    /// Derselbe Weg fuer den eigenen RTTY-Dekoder (2026-10-17):
    /// Spotliste, Sammler, PSK Reporter. Siehe meldeEigenenFund.
    void onNativeRttyDecode(const Longpath::DxSpot& spot);

    /// Der eigene FT8/FT4-Sender, oder null vor dem Verbinden
    /// (2026-10-15). Siehe core/Ft8Tx.h.
    Ft8TxSource* ft8TxSource() const { return m_ft8TxSource.get(); }

    // ── Die FT8-Quelle gibt es auch ohne eigenen Sendekanal (2026-10-28) ─
    //
    // `m_ft8TxSource` entstand bisher an EINER Stelle: im WDSP-Aufbau
    // nach dem Verbinden mit einem eigenen Funkgeraet. Bei der Flex
    // laeuft der nie — sie rechnet selbst. Also war die Quelle null,
    // `Ft8Applet::sendeNachricht()` brach mit „no transmit path" ab,
    // und weil diese Zeile 200 ms spaeter vom Takt ueberschrieben
    // wird, sah der Betreiber am 2026-10-28 nur einen Send-Knopf, der
    // nichts tat.
    //
    // Anlegen heisst nicht senden: die Quelle ist still, bis jemand
    // `vorbereiten()` + `starten()` ruft, und getastet wird sie hier
    // ueberhaupt nicht.
    void stelleFt8SenderBereit();

    /// Der eigene RTTY-Sender, oder null vor dem Verbinden
    /// (2026-10-16). Siehe core/RttyTx.h.
    RttyTxSource* rttyTxSource() const { return m_rttyTxSource.get(); }

    /// Der Mikrofon-Umschalter, oder null vor dem Verbinden. Ueber ihn
    /// laeuft das Umlegen auf die FT8-Quelle beim Senden.
    CompositeTxMicRouter* micRouter() const
    {
        return m_compositeMicRouter.get();
    }

    DxccColorProvider*    dxccColorProvider()   const { return m_dxccColorProvider.get(); }
    DxClusterClient*      dxCluster()           const { return m_dxCluster.get(); }
    DxClusterClient*      rbn()                 const { return m_rbn.get(); }
    WsjtxClient*          wsjtx()               const { return m_wsjtx.get(); }
    SpotCollectorClient*  spotCollector()       const { return m_spotCollector.get(); }
    PotaClient*           pota()                const { return m_pota.get(); }
    SotaClient*           sota()                const { return m_sota.get(); }
    FreeDVReporterClient* freeDvReporter()      const { return m_freeDvReporter.get(); }
    PskReporterClient*    pskReporter()         const { return m_pskReporter.get(); }

    // ── TNF (design section 8.1): the canonical notch store ─────────────────
    //
    // Constructed in the RadioModel ctor and restored from AppSettings there,
    // before any WDSP channel exists, so the openRxChannelPool-tail reconcile
    // (section 6.3) always has the full list to install. Non-owning pointer;
    // lifetime is RadioModel's. Consumed by the TCI rx_nf_enable repoint
    // (section 6.4), the +TNF button and status-bar light (section 7), and
    // MnfSetupPage (section 9).
    NotchModel*           notchModel()          const { return m_notchModel.get(); }

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
    //
    // In einer Sandbox (LONGPATH_CONFIG_DIR gesetzt) tut diese Funktion
    // NICHTS -- eine Sonde, die sich mit dem Rufzeichen des Betreibers
    // am Cluster anmeldet, wirft seine eigene Sitzung hinaus (gemessen
    // 2026-10-21, und schon einmal 2026-09-17). Siehe
    // AppSettings::darfVonSelbstVerbinden(). Von Hand bleibt in der
    // Sonde alles erreichbar; nur von selbst passiert nichts.
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
    void wireRadeChannel(int sliceId, Longpath::RadeChannel* channel,
                         Longpath::SliceModel* slice);

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
    /// FM-CTCSS des Sende-Slices an den Sendekanal (2026-09-27).
    void pushFmToneFromTxSlice();
    /// Schaltet den CTCSS-Tonsquelch dieses Slices nach `fmCtcssMode`,
    /// `fmCtcssValueHz` und der Betriebsart. Longpath-eigen, kein
    /// Thetis-Port -- siehe RxChannel::setCtcssSquelch.
    void pushCtcssSquelchForSlice(const SliceModel* slice);

    /// FM-Deviation fuer Senden und Empfang aller Kanaele, 5000 oder
    /// 2500 Hz (Thetis console FMDeviation_Hz, radFMDeviation5kHz /
    /// radFMDeviation2kHz). Gespeichert als FmDeviationHz. 2026-09-27.
    int fmDeviationHz() const { return m_fmDeviationHz; }
    void setFmDeviationHz(int hz);
    /// Die Deviation an alle offenen RX-Kanaele und den Sendekanal.
    void pushFmDeviationToChannels();

    /// Longpath Audio Channel Strip — the client-side transmit chain.
    ///
    /// Always there, from the constructor to the destructor, radio or
    /// no radio. It used to be built with the transmit pump and torn
    /// down with it, which left the whole strip window dead on a
    /// workbench with nothing plugged in: no master switch, no stage
    /// to set, no preset to apply, and the Voice check's "through
    /// your strip" silently doing nothing. Setting a voice up is
    /// exactly the work one does *before* going on the air
    /// (2026-10-09, "kann diese nicht aktivieren").
    ///
    /// Callers may still see null in tests that build a bare model
    /// through a path that skips the constructor body; the panels
    /// keep their checks.
    StripChain* stripChain() const { return m_stripChain.get(); }

    // Phase 3F Sub-Epic J Task 11: the one place GUI code may resolve a
    // slice's WDSP channel. Mirrors txChannel()'s shape. Added so MainWindow
    // and the Setup pages can stop calling wdspEngine()->rxChannel()
    // directly -- that direct reach is what let ANF-on-slice-B toggle slice
    // A (this same sub-epic, Task 1). RadioModel stays the only layer that
    // touches WdspEngine::rxChannel(); everything under src/gui/ goes
    // through this accessor instead (enforced by
    // scripts/verify-no-gui-dsp-access.py).
    // Returns nullptr if WDSP has not initialised yet or sliceIndex has no
    // channel.
    RxChannel* rxChannelForSlice(int sliceIndex) const;

    // Phase 3P-II: PGXL / TGXL / Tuner accessors.
    // PgxlConnection and TgxlConnection are QObject children of RadioModel
    // (constructed once in the ctor with parent=this). TunerModel is likewise
    // a QObject child that binds its connection once in the ctor.
    // All three accessors return non-null pointers from construction time.
    PgxlConnection* pgxlConnection() { return m_pgxlConnection; }
    Rf2ksConnection* rfKitConnection() const { return m_rfKitConnection.get(); }
    // Der SPE-Treiber lebt wie der RF2K-S fuer die ganze Laufzeit; das
    // Fenster verbindet sich einmal darauf und nie neu.
    SpeConnection* speConnection() const { return m_speConnection.get(); }
    Kpa500Connection* kpa500Connection() const { return m_kpa500Connection.get(); }
    AcomConnection* acomConnection() const { return m_acomConnection.get(); }
    TgxlConnection* tgxlConnection() { return m_tgxlConnection; }
    TunerModel*     tunerModel()     { return m_tunerModel;     }
    // SmartSDR API server on TCP 4992. Owned by RadioModel; lifetime matches.
    // Used by MainWindow to push slice/transmit state so PGXL/TGXL pull the
    // current band/freq via the SmartSDR API rather than from a stale cache.
    class SmartSdrApiListener* smartSdrListener() { return m_smartSdrListener; }

    // Live toggle for the 4O3A master switch (Settings -> CAT & Network ->
    // 4O3A -> General tab).  Starts or stops the TCP 4992 listener
    // without requiring an app restart.  Persisted automatically via
    // AppSettings key "FourO3A_Enabled".  Default OFF on first run.
    //
    // When false: TCP 4992 not bound, PGXL/TGXL auto-connect skipped,
    // and the detail tabs (PowerGenius XL / Tuner Genius XL / Diagnostics)
    // are disabled in the Setup UI.
    //
    // When true: listener starts (if not already running), AppSettings
    // persisted, FourO3APage updates its enabled state.
    /// Soll der FlexRadio-Ausruf laufen? Zwei Tore, UND-verknuepft.
    ///
    /// Betreiber 2026-10-18 auf seinem Maestro: dort stand ein
    /// „FLEX-6400" in der Geraeteliste, das er nicht besitzt -- es war
    /// Longpath selbst. Der Ausruf haengt seither am
    /// 4O3A-HAUPTSCHALTER (Vorgabe „False"), nicht mehr allein am
    /// Feinschalter PGXL_BroadcastDiscovery (Vorgabe „True").
    ///
    /// Statisch und ohne Zustand, damit die REGEL pruefbar ist und
    /// nicht nur die Verdrahtung: tests/tst_kein_phantom_flex.cpp
    /// faehrt die ganze Wahrheitstafel.
    static bool flexAusrufSollLaufen(bool fourO3AEnabled,
                                     bool broadcastDiscovery)
    { return fourO3AEnabled && broadcastDiscovery; }

    void setFourO3AEnabled(bool enabled);
    bool fourO3AEnabled() const;

    // Phase 3P-III RF-Kit RF2K-S master toggle.
    // Persisted per-MAC under hardware/<mac>/peripherals/RfKit_Enabled.
    // Default OFF on first run. When true the Rf2ksApplet and Setup tab
    // become active; when false the REST poller is idle and the applet
    // is hidden. No-op when not connected (no MAC scope to write under).
    void setRfKitEnabled(bool enabled);
    // Pushes RfKit_AutoReconnect + RfKit_PollIntervalMs from AppSettings into
    // the live connection. Called before every connectToAmp().
    void applyRfKitOperatorSettings();
    bool rfKitEnabled() const;

    // ── SPE Expert (Zeus-Punkt 7) ────────────────────────────────────────
    //
    // Gleiche Form wie der RF2K-S darueber, ein Unterschied: der SPE kann
    // ueber Netz ODER seriell angeschlossen sein, und welches von beiden
    // steht in Spe_Mode. Die Schluessel liegen im selben per-MAC-Bereich:
    //   Spe_Enabled "True"|"False", Spe_Mode "Serial"|"Network",
    //   Spe_SerialPort, Spe_Host, Spe_Port, Spe_AutoReconnect
    //
    // Default AUS beim ersten Start. Ohne Anschlussangabe (leerer Rechner
    // bzw. leerer Anschlussname) wird NICHT verbunden -- es gibt keine
    // Suche im Netz: ser2net antwortet auf keine Rundfrage, und ein
    // serieller Anschluss laesst sich nicht erraten.
    void setSpeEnabled(bool enabled);
    bool speEnabled() const;
    // Schiebt Spe_AutoReconnect aus dem per-MAC-Bereich in die lebende
    // Verbindung. Vor jedem Verbinden gerufen -- derselbe Grund wie bei
    // applyRfKitOperatorSettings(): ein Schalter, den niemand zurueckliest,
    // ist ein Schalter, der nichts tut.
    void applySpeOperatorSettings();
    // Verbindet nach den gespeicherten Angaben, oder trennt. Gerufen vom
    // Umschalter, von der Einstellseite nach einer Aenderung und beim
    // Verbinden eines Funkgeraets (dann ist der per-MAC-Bereich erst da).
    void applySpeConnection();

    // ── Elecraft KPA500 (Zeus-Punkt 7) ───────────────────────────────────
    //
    // Wie der SPE: Netz ODER seriell, Umschalter in Spe_Mode's Gegenstueck
    // Kpa500_Mode. EIN Unterschied, und er ist wichtig: die Datenrate des
    // seriellen Anschlusses laesst sich beim KPA500 NICHT erraten (vier
    // Werte, Vorgabe nicht dokumentiert -- siehe core/Kpa500Connection.h).
    // Sie steht darum in Kpa500_Baud und gehoert dem Betreiber.
    //
    // Schluessel im per-MAC-Bereich:
    //   Kpa500_Enabled "True"|"False", Kpa500_Mode "Serial"|"Network",
    //   Kpa500_SerialPort, Kpa500_Baud, Kpa500_Host, Kpa500_Port,
    //   Kpa500_AutoReconnect
    void setKpa500Enabled(bool enabled);
    bool kpa500Enabled() const;
    void applyKpa500Connection();

    // ── ACOM S-Serie (Betreiberwunsch 2026-10-09) ────────────────────────
    //
    // Wie SPE und KPA500: Netz ODER seriell. Zwei Unterschiede, beide aus
    // dem Protokoll:
    //   * seriell ist die Datenrate FEST 9600 8N1 (die Beschreibung
    //     schreibt sie vor) -- kein Feld dafuer, anders als beim KPA500
    //   * ueber Netz braucht es einen ROHEN Vermittler. Der Datenstrom
    //     enthaelt legitim 0xFF, und Telnets IAC-Maskierung verfaelscht
    //     genau das. Beim SPE ging Telnet noch, hier nicht.
    //
    // Schluessel im per-MAC-Bereich:
    //   Acom_Enabled "True"|"False", Acom_Mode "Serial"|"Network",
    //   Acom_SerialPort, Acom_Host, Acom_Port, Acom_AutoReconnect
    void setAcomEnabled(bool enabled);
    bool acomEnabled() const;
    void applyAcomConnection();

    // ── Per-radio peripherals scope (RF-Kit / 4O3A / PGXL / TGXL) ────────
    //
    // The four external-amp accessories used to read GLOBAL AppSettings
    // keys so they fired on every radio regardless of which one was
    // connected.  As of this refactor each accessory's enable + connection
    // info is scoped under hardware/<mac>/peripherals/<key>.
    //
    // Storage format:
    //   hardware/<mac>/peripherals/RfKit_Enabled       "True" | "False"
    //   hardware/<mac>/peripherals/RfKit_ManualIp      string
    //   hardware/<mac>/peripherals/RfKit_ManualPort    int (string-encoded)
    //   hardware/<mac>/peripherals/FourO3A_Enabled     "True" | "False"
    //   hardware/<mac>/peripherals/PGXL_ManualIp       string
    //   hardware/<mac>/peripherals/PGXL_ManualPort     int (string-encoded)
    //   hardware/<mac>/peripherals/TGXL_ManualIp       string
    //   hardware/<mac>/peripherals/TGXL_ManualPort     int (string-encoded)
    //   hardware/<mac>/peripherals/Spe_Enabled         "True" | "False"
    //   hardware/<mac>/peripherals/Spe_Mode            "Serial" | "Network"
    //   hardware/<mac>/peripherals/Spe_SerialPort      string
    //   hardware/<mac>/peripherals/Spe_Host            string
    //   hardware/<mac>/peripherals/Spe_Port            int (string-encoded)
    //   hardware/<mac>/peripherals/Spe_AutoReconnect   "True" | "False"
    //
    // peripheralValue(key, default):
    //   Returns the per-MAC value when connected, defaultValue otherwise.
    //   Use this for every read of the keys listed above.
    //
    // setPeripheralValue(key, value):
    //   Writes the per-MAC value when connected.  When NOT connected this
    //   is a no-op + qCWarning(lcConnection) so a Setup page that fires
    //   before a radio is selected can't silently lose data.  Setup pages
    //   should gray themselves out via connectionStateChanged().
    //
    // currentRadioMac():
    //   Returns m_lastRadioInfo.macAddress when connected, empty otherwise.
    //   Setup pages use this to populate the "Editing peripherals for ..."
    //   banner.  Tests reach for setLastRadioInfoForTest() to drive the
    //   per-MAC scope without a live RadioConnection.
    QString peripheralValue(const QString& key,
                            const QString& defaultValue = QString{}) const;
    void    setPeripheralValue(const QString& key, const QString& value);
    QString currentRadioMac() const;

    bool hasAmplifier() const { return m_hasAmplifier; }
    bool ampOperate()  const  { return m_ampOperate; }

    // Cross-vendor "is any external amp currently amplifying?" predicate.
    // True if PGXL is connected + in OPERATE OR if the RF-Kit RF2K-S is in
    // OPERATE.  Used by MainWindow's power-scale wiring to decide whether
    // to feed the TX power/SWR display from the radio's barefoot meters or
    // from an external amp's telemetry.  Phase 3P-III bench fix 2026-05-25
    // KG4VCF:
    // without this gate, PGXL Connect 2 (radio TX power) was overwriting
    // RF-Kit Connect B (amp forward power) at the radio's higher emit rate.
    bool isAnyExternalAmpInOperate() const;

    // RF-Kit-only counterpart to the predicate above: true when the RF2K-S
    // is connected AND reporting OPERATE, ignoring PGXL entirely.
    //
    // Needed because externalAmpFwdSwrUpdated carries RF-Kit telemetry
    // exclusively, so gating that feed on the cross-vendor predicate let a
    // PGXL in OPERATE wave through RF-Kit /power polls from an amp sitting
    // in STANDBY, overwriting the live PGXL meter with RF-Kit's 0 W.  A
    // per-source feed needs a per-source gate.  Codex review, PR #291.
    bool isRfKitInOperate() const;

    // Phase 3P-II Task 86: TxInterlockPolicy -- Longpath-native TX gate.
    // Constructed once in the ctor (Qt parent-ownership). Non-null from
    // construction time. Shared with PgxlInterlockPage (non-owning read/write)
    // and MoxController (non-owning gate via setInterlockPolicy).
    TxInterlockPolicy* txInterlockPolicy() { return m_txInterlockPolicy; }

    // Phase 3P-II Phase 4 Task 89: TuneMemoryStore -- shared per-(antenna,band)
    // TGXL relay position cache. Constructed once in the ctor (Qt parent-ownership).
    // Non-null from construction time. Shared by TgxlAdvancedPage (non-owning
    // view/edit) and TunerApplet (non-owning save/recall from context menu).
    TuneMemoryStore* tuneMemoryStore() { return m_tuneMemoryStore; }

    // Phase 3P-II Phase 4 Task 94: FaultLog -- shared PGXL / TGXL fault ring buffers.
    // Constructed once in the ctor (Qt parent-ownership). Non-null from construction
    // time. RadioModel captures PGXL FAULT state transitions into m_pgxlFaultLog.
    // Shared (non-owning) with PgxlAdvancedPage and TgxlAdvancedPage so their
    // Fault History tables reflect live captures.
    FaultLog* pgxlFaultLog() { return m_pgxlFaultLog; }
    FaultLog* tgxlFaultLog() { return m_tgxlFaultLog; }

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

    // ── RX meter calibration offset (Thetis-faithful port) ────────────────
    //
    // Returns the cumulative dB offset applied to WDSP S-meter readings
    // (RXA_S_PK, RXA_S_AV) and MaxBin readings before display.  Without
    // this offset, raw WDSP meter values are in ADC dBFS rather than
    // antenna dBm.
    //
    // Ported from Thetis console.cs:21040 [v2.10.3.13]:
    //   public float RXOffset(int rx) {
    //       return RXPreampOffset(rx) + RXCalibrationOffset(rx);
    //   }
    //
    // RXPreampOffset (console.cs:20989) selects between attenuator_data
    // (when step-att enabled) and preamp_offset[mode] (when disabled).
    //
    // RXCalibrationOffset (console.cs:21022) sums per-radio meter cal +
    // XVTR + 6m offsets.  Longpath currently applies only the per-radio
    // meter cal (defaults from rxMeterCalOffsetDefaultFor() and the user
    // override AppSettings key RX1_MeterCalOffsetDb); XVTR/6m offsets
    // ride a future XVTR/transverter epic.
    //
    // Consumed by MeterPoller::pollSMeter and MeterPoller::poll for the
    // SignalPeak / SignalAvg / SIGNAL_MAX_BIN bindings only; matches
    // Thetis console.cs:46824 + :46881 [v2.10.3.13] where +offset is
    // applied to those exact reading types.  ADC_PK / ADC_AV / AGC_PK /
    // AGC_AV / AGC_GAIN do NOT take the offset (Thetis line 46831-46835
    // omit +offset for the same reason).
    double rxMeterOffsetDb() const;
    /// Den Pegelversatz neu rechnen und bei Aenderung rxMeterOffsetChanged
    /// senden (Abschwaecher, Vorverstaerker, Transverter; 2026-11-08).
    void pruefeRxMeterOffset();

signals:
    // Emitted when rxMeterOffsetDb() changes (model swap, preamp change,
    // step-att enable/disable, attenuator dB change, or AppSettings
    // RX1_MeterCalOffsetDb override).  MeterPoller connects this to
    // refresh its cached offset value.
    void rxMeterOffsetChanged(double db);
    void fmDeviationHzChanged(int hz);

public:

    bool isConnected() const;

    // ── Einmessen an einem Traeger (2026-11-08) ────────────────────────
    //
    // Wohin wirkt eine Traegermessung? Liegt der Traeger im ZF-Fenster
    // eines aktiven Transverters, ist es dessen LO-Fehler (2 m); haengt
    // ein OpenHPSDR-Geraet (P1/P2) dran, dessen Korrekturfaktor (KW,
    // Thetis Freq Cal); sonst wird nur angezeigt -- fuer die Flex u. a.
    // gibt es keine belegte Quelle, wie deren Bezug zu stellen waere.
    enum class EichZiel { Transverter, Geraet, NurAnzeige };
    EichZiel eichZielFuer(double geraetHz) const;

    /// Uebernimmt eine Messung: der Traeger erschien bei geraetHz (am
    /// Geraet) bzw. anzeigeHz (auf der Antenne), wahr ist wahrHz (auf
    /// der Antenne). false bei NurAnzeige oder externem 10-MHz-Bezug.
    bool uebernimmEichung(double geraetHz, double anzeigeHz, double wahrHz);

    /// Steht IRGENDEINE Funkverbindung — eigenes Geraet ODER Flex.
    ///
    /// `isConnected()` kennt nur `m_connection`, also OpenHPSDR. Bei
    /// der FLEX-8400 ist der Zeiger null, und damit war jeder
    /// `if (isConnected())`-Zweig fuer sie tot. Einer davon ist der
    /// Selbstschluss der Verbindungsmaske: der Betreiber musste am
    /// 2026-10-22 jedes Mal von Hand „Close" druecken, um ueberhaupt
    /// an das Funkgeraet zu kommen — und bis dahin lagen alle
    /// schwebenden Fenster hinter der Maske.
    ///
    /// Bewusst NEBEN `isConnected()` und nicht darin: die neunzehn
    /// Aufrufer von `isConnected()` greifen teils gleich danach auf
    /// `connection()` zu, und das gibt es beim Flex nicht.
    bool istIrgendwieVerbunden() const;

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
    qint64 setSampleRateLive(int newRateHz,
                             bool reconcileDiversity = true);

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
    // Stands in for the connectToRadio() write of the wire rate / RX count
    // so a test can replay the state sequence around it without a radio.
    void setConnectionRateForTest(int rateHz, int activeRxCount) {
        m_connectionSampleRateHz = rateHz;
        m_connectionActiveRxCount = activeRxCount;
    }

    // Test-only: walk the FULL Connected/Disconnected handler so tests
    // can drive the peripherals lifecycle (applyPeripheralsForCurrentMac
    // / teardownPeripherals) without standing up a RadioConnection.
    // Mirrors the production signal-driven path -- equivalent to wiring
    // a fake RadioConnection and emitting its connectionStateChanged
    // signal, but without the QObject overhead.
    void onConnectionStateChangedForTest(ConnectionState s) {
        onConnectionStateChanged(s);
    }

    // Test-only: targeted seams for the peripherals lifecycle.  Tests
    // that don't want the full onConnectionStateChanged side-effects
    // (settings-hygiene validation, currentRadioChanged emit, etc.)
    // can drive applyPeripheralsForCurrentMac / teardownPeripherals
    // directly after setting the connection state via
    // setConnectionStateForTest + setLastRadioInfoForTest.
    void applyPeripheralsForTest() { applyPeripheralsForCurrentMac(); }
    void teardownPeripheralsForTest() { teardownPeripherals(); }

    // ── Kein Testzugriff: TxVoiceCheckDialog benutzt das ─────────────
    //
    // Stand bis 2026-08-17 INNERHALB des #ifdef LONGPATH_BUILD_TESTS
    // weiter unten — direkt unter txWorkerForTest(), dessen Kommentar
    // ausdruecklich sagt "Test-only accessor — do not use in production
    // code". Der Nachbar hat den Platz geerbt, aber nicht die
    // Eigenschaft.
    //
    // Folge: die App war seit 49d10dc8 NICHT MEHR VON GRUND AUF BAUBAR.
    // TxVoiceCheckDialog.cpp ruft txWorker() an fuenf Stellen, und ohne
    // das Makro gibt es die Funktion nicht. Gesehen hat es niemand,
    // weil ./build.sh inkrementell baut und die alte Objektdatei liegen
    // blieb, waehrend ./tools/run_tests.sh mit
    // LONGPATH_BUILD_TESTS=ON konfiguriert und deshalb immer durchlief.
    // Erst ein rm -rf build hat es gezeigt.
    //
    // Voice-check access to the worker's pre/post-strip taps
    // (2026-08-11). Null until a connection constructs the worker — the
    // dialog re-arms on its level-watch tick exactly as it does for
    // txChannel().
    TxWorkerThread* txWorker() { return m_txWorker.get(); }

#ifdef LONGPATH_BUILD_TESTS
public:
    // Test-only: inject board caps without a live radio connection.
    // Mirrors P1RadioConnection::setBoardForTest pattern.
    void setBoardForTest(HPSDRHW board) {
        m_hardwareProfile = ::Longpath::profileForModel(
            defaultModelForBoard(board));
    }

    // Phase 3P-I-a T14 — test-only hooks. Allow tests to inject a mock
    // RadioConnection, simulate band crossings, trigger the Connected
    // state handler, and override board capabilities. Production code
    // must never use these.
    void injectConnectionForTest(RadioConnection* conn) { m_connection = conn; }

    // Die Modell-/Kenndatenwahl ohne Verbindung: ein Pruefstand kann so
    // nachsehen, mit welcher Abtastrate ein gemeldetes Board gefahren
    // wuerde, ohne einen Socket zu oeffnen (tst_sunsdr_board_caps).
    void applyHardwareProfileForTest(const RadioInfo& info) {
        applyHardwareProfileFor(info);
    }
    // Install a real PureSignal coordinator without the full WDSP/connect
    // pipeline so codec-context tests can distinguish the auto-cal preference
    // from the cmd-state machine's effective PSEnabled state. RadioModel owns
    // the returned coordinator, matching production lifetime.
    PureSignal* installPureSignalForTest(TxChannel* tx);
    // Phase 3F Sub-Epic I closeout, defect F1: attach a DSP worker without
    // standing up the connection / DSP-thread pipeline, so a test can
    // reproduce connectToRadio's real ordering (pool sized and slices bound
    // FIRST, worker constructed second) and assert the bindings still reach
    // it. Non-owning, exactly like the production m_dspWorker.
    void attachDspWorkerForTest(RxDspWorker* w) { m_dspWorker = w; }
    // Phase 3F Sub-Epic I closeout, defect F3: force the radio-state inputs
    // the codec branches on, so the PureSignal and diversity branches are
    // reachable without standing up a connection, a WDSP engine and a
    // PureSignal coordinator. Sticky once set; production code must never
    // call this.
    void setDdcContextForTest(bool mox, bool puresignalRun, bool diversity) {
        m_ddcCtxForTest    = true;
        m_ddcCtxMoxForTest = mox;
        m_ddcCtxPsForTest  = puresignalRun;
        m_ddcCtxDivForTest = diversity;
    }
    // B6 — XIT: allow tests to trigger wireSliceSignals() directly after
    // injecting a mock connection, mirroring what wireConnectionSignals() does
    // when a real radio connects.
    void wireSliceSignalsForTest() { wireSliceSignals(m_activeSlice); }
    // The transmit-frequency derivation the push and both TUNE arms share.
    // setTune() itself is unreachable from a unit test (it requires a live
    // connection AND an audio engine, console.cs:30035-30043 [v2.10.3.15]'s
    // PowerOn guard), so the shared derivation is what gets pinned.
    quint64 txFrequencyForSliceForTest(const SliceModel* s) const {
        return txFrequencyForSlice(s);
    }
    void installBandPlanMoxCheckForTest() { installBandPlanMoxCheck(); }
    // Issue #182 — invoke the mic_ptt_disabled wiring helper directly so
    // tst_radio_model_mic_ptt_wire can verify the signal/slot bind + prime
    // path without spinning up the full wireConnectionSignals pipeline.
    void wireMicPttDisabledForTest() { connectMicPttDisabledSignal(); }

    /// Nur fuer Prueflaeufe: den Sendeweg so weit aufsetzen, dass
    /// Ft8Applet wirklich senden kann — ohne Funkverbindung.
    ///
    /// ft8TxSource() und micRouter() entstehen sonst erst beim
    /// Verbinden (siehe dort), und ohne sie bricht
    /// Ft8Applet::sendeNachricht() mit „no transmit path" ab, bevor
    /// irgendetwas geprueft werden kann. Setzt nur die beiden
    /// Bausteine, nichts vom Rest der Kette: ein Pruefstand, der
    /// senden WILL, soll nicht nebenbei eine Tonausgabe aufmachen.
    void installFt8TxForTest();
    /// Dasselbe fuer den RTTY-Sendeweg (2026-10-16). Eigener
    /// Einbau statt eines gemeinsamen, damit ein Prueflauf, der nur
    /// RTTY will, nicht nebenbei den FT8-Sender anmeldet.
    void installRttyTxForTest();
    void wireMicInputsForTest() { connectMicInputSignals(); }
    void setLastBandForTest(Longpath::Band b) {
        const bool cross = (b != m_lastBand);
        m_lastBand = b;
        if (cross) {
            applyAlexAntennaForBand(b);
            // Mirror the production T10 path so tests catch regressions
            // in the slice-label refresh (see RadioModel.cpp frequencyChanged
            // handler for the canonical version).
            //
            // Issue #257: production now passes the SkuUiProfile so the
            // RX-only label slot wins over the ANT* default. Mirror that
            // here so band-cross tests exercise the same path.
            if (m_activeSlice) {
                const Longpath::SkuUiProfile sku =
                    Longpath::skuUiProfileFor(m_hardwareProfile.model);
                m_activeSlice->refreshAntennasFromAlex(m_alexController, b, &sku);
            }
        }
    }
    void onConnectedForTest() {
        applyAlexAntennaForBand(m_lastBand);
    }
    // Phase 3P-I-b (T6): expose with isTx parameter for composition tests.
    void applyAlexAntennaForBandForTest(Longpath::Band band, bool isTx) {
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
        emit currentRadioChanged(Longpath::RadioInfo{});
    }
    Longpath::Band lastBand() const { return m_lastBand; }

    // Phase 3F: expose the codec's input array so tests can assert what the
    // per-board codec is actually handed (SliceConfig::txBound in
    // particular, which is the OR of SliceModel::isTxSlice across the slices
    // sharing a DDC stream). Production callers reach the same builder
    // through requestDdcAssignment.
    std::array<Longpath::SliceConfig, 5> buildStreamConfigsForCodecForTest() const {
        return buildStreamConfigsForCodec();
    }

    // Companion to the above for the other half of the codec's inputs.
    // Added with the D2 fix (CodecContext::adcCtrl was never seeded on
    // Protocol 2), so a test can assert the seed without standing up a
    // connection to observe it through a wire frame.
    Longpath::CodecContext currentCodecContextForTest() const {
        return currentCodecContext();
    }

    // Publish a hand-built assignment through the real production path.
    //
    // Added with the D1 fix. The per-stream ADC now reaches the model in
    // exactly one way: a codec composes a DdcAssignment and this function
    // decodes its adcCtrl bytes. A test that wants a slice on chain 1 has to
    // go through here or it is seeding a field nothing reads, which is the
    // defect D1 was.
    //
    // Deliberately the whole function rather than a setter for m_streamAdc.
    // A narrow ADC setter could drift from what publishDdcAssignment
    // actually writes and the suite would never notice; routing through the
    // real body means the decode (Longpath::adcForDdc) is under test too.
    //
    // For tests that want the codec's own answer instead of a hand-built
    // one, inject a codec and use requestDdcAssignment: that is the fuller
    // path and it is what tst_alex_per_adc_bpf_wire drives.
    void publishDdcAssignmentForTest(const Longpath::DdcAssignment& a) {
        publishDdcAssignment(a);
    }

    // Per-radio peripherals scope: tests pin m_lastRadioInfo without
    // standing up a fake RadioConnection so peripheralValue / setPeripheralValue
    // can resolve their per-MAC scope.  Production code populates this via
    // the connection-thread handshake.
    void setLastRadioInfoForTest(const Longpath::RadioInfo& info) {
        m_lastRadioInfo = info;
    }

    // Codex review round 7, PR #293 — drive the I/Q tap fork directly.
    // Production reaches it from the iqDataForReceiver lambda installed in
    // wireConnectionSignals, which needs DSP threads, an RxDspWorker and a
    // live connection. Same on*ForTest pattern as handlePaTelemetryForTest.
    void forkIqToTapsForTest(int receiverIndex, const QVector<float>& samples) {
        forkIqToTaps(receiverIndex, samples);
    }

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

    // Derselbe Weg OHNE die TX-Vortaeuschung (2026-10-28).
    //
    // Die Naht darueber heisst "tu als wuerde gesendet", und genau das
    // macht sie fuer das MOX-Tor der SWR-Rechnung unsichtbar. Dieser
    // Zwilling laesst das Tor zu, damit sich pruefen laesst, dass ein
    // Nachlauf-Rahmen nach dem Unkey den gemessenen Stehwert NICHT mehr
    // auf 1,00 zurueckstellt -- Thetis' `if (_mox)`
    // (console.cs:25991 [v2.10.3.15]).
    void handlePaTelemetryNotKeyedForTest(quint16 fwdRaw, quint16 revRaw,
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

    // Phase 4 Agent 4A of issue #167 — test seam to inject a non-owning
    // TxChannel pointer so the drive-slider / TUNE rewrite tests can spy on
    // setTxFixedGain() without standing up the full WdspEngine pipeline.
    // Production code never calls this — m_txChannel is wired by the
    // WDSP-init lambda inside connectToRadio() (see "createTxChannel(kTxChannelId)"
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

    // TUN state, exported for H.3 UI polling and for issue #177 tests.
    // True between setTune(true) and the completion of the corresponding
    // setTune(false) → completeTuneOff() chain.
    // Cite: Thetis console.cs:30010 [v2.10.3.13] — _tuning = true (read by
    // many UI/meter/PA paths in console.cs).
    //
    // Lives OUTSIDE the LONGPATH_BUILD_TESTS block because production code
    // (TunerApplet::onTuneClicked at TunerApplet.cpp:183) uses it to gate
    // local-tune carrier engage on hardware-TUNE entry.  Prior placement
    // inside the test block compiled green on Linux (-DLONGPATH_BUILD_TESTS=ON
    // in CI) but broke macOS / Windows where the option defaults OFF.
    bool isTune() const noexcept { return m_isTuning; }

    // Connection
    void connectToRadio(const RadioInfo& info);
    void disconnectFromRadio();

    // ── Phase 3M: eine Aufnahme als Quelle ───────────────────────────────────
    // Oeffnet `path` und verbindet mit ProtocolVersion::Playback, damit die
    // Datei denselben Weg speist wie ein Geraet (WDSP, FFT, Ton). Eine
    // laufende Geraeteverbindung wird vorher geschlossen. false heisst: es
    // laeuft nichts, und `error` sagt warum. Gesendet wird aus einer
    // Aufnahme nie -- PlaybackRadioConnection nimmt MOX und TX-I/Q an und
    // tut nichts damit.
    bool startPlaybackFromFile(const QString& path, QString* error = nullptr);
    void stopPlayback();

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
    /// Flush any coalesced notch edit immediately. Called when a notch drag
    /// ends so the committed position is exact rather than up to one
    /// coalescing window stale. See scheduleNotchEditPush.
    void commitPendingNotchEdits();

    /// The single creation route for every notch. Resolves the minimum
    /// realisable width from THE GIVEN SLICE's channel and clamps to it, so a
    /// notch is never stored or drawn at a width WDSP will silently widen.
    ///
    /// Codex review of PR #313 found three separate add routes with three
    /// different behaviours: the panadapter gesture clamped against
    /// activeSlice() rather than the pan that was clicked, and the +TNF button
    /// and the settings-page Add button did not clamp at all. Every route goes
    /// through here now, and `slice` is always the slice the operator acted
    /// on, per the standing rule that a control drawn on a pan targets that
    /// pan.
    ///
    /// Returns the new notch id, or -1 if the model refused it.
    int addNotchForSlice(SliceModel* slice, double centerHz, double widthHz);

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
    // Longpath's MeterModel does not yet expose a TX-mode selector (that
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

    // TGXL autotune orchestration (Longpath-native, no Thetis source).
    //
    // Bench-driven on 2026-05-20: TGXL refuses to run its relay sweep when
    // PGXL is in OPERATE -- the amp is amplifying the radio's tune carrier
    // and TGXL can't calibrate against an amplified signal. The operator
    // workflow with real FlexRadio is: put PGXL in STANDBY, run TGXL
    // autotune at radio tunepower (~10-25 W), then re-arm PGXL to OPERATE.
    //
    // This method orchestrates that sequence:
    //   1. Save current PGXL operate-state (m_pgxlSavedOperate)
    //   2. Send `operate=0` to PGXL if it was operating
    //   3. Engage local CW tune carrier via setTune(true). Thetis-faithful
    //      `chkTUN_CheckedChanged` already swaps rfpower -> tunepower for
    //      the cycle (console.cs:30075 [v2.10.3.13]).
    //   4. After 200 ms settle, send `autotune` to TGXL on :9010 (unless
    //      fromHardware=true, in which case TGXL is already running its
    //      own internal cycle and we only need to provide the carrier).
    //   5. Wait for TGXL tuning=0 -> drop carrier (handled by the
    //      TunerApplet tuningChanged path that calls setTune(false)).
    //   6. On carrier drop, if m_pgxlSavedOperate was true, send
    //      `operate=1` to restore PGXL.
    //
    // fromHardware: true when the cycle was initiated by a TGXL hardware
    // TUNE button (we received `transmit tune on` via LAN PTT). Skips
    // step 4 because TGXL is already running its own sweep internally.
    void startTgxlAutotune(bool fromHardware);

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
    /// Longpath has one frequency per slice; `chan==1` (VFO B) is silently
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

    /// Query split-TX state.  Always returns false: Phase 3F deletes the
    /// `setSplit` stub per design §3 ("split is replaced with XIT for plus
    /// or minus 10 kHz tuning offset, or addSliceOnPan to create a second
    /// slice for full retune"). The query stays so TciProtocol's init burst
    /// can still emit `split_enable:rx,false;` for wire-protocol stability
    /// with WSJT-X / N1MM / Log4OM clients ("Split Operation: None/Fake It"
    /// is the supported configuration).
    /// See `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md`
    /// §3 ("VFO A/B / split: not implemented").
    Q_INVOKABLE bool split(int rx) const;

    // ── Frequency memories (Thetis Memory/MemoryForm.cs + console.cs) ──
    //
    // The memory list lives in memory.xml next to the settings file, in
    // Thetis's own layout (see models/MemoryList.h), restored when the model
    // is built (console.cs:2006-2007 [@852bf0e]) and saved by the dialog
    // after every change, as MemoryForm does.
    MemoryList* memories() const { return m_memories; }
    bool saveMemories(QString* error = nullptr) const;
    /// Where memory.xml lives: the directory of the settings file.
    QString memoriesDir() const;

    /// The active slice's state as one record, the way MemoryForm's Add
    /// button captures it (Memory/MemoryForm.cs:502-546 [@852bf0e]).
    MemoryRecord captureMemory() const;
    /// Apply a record to the active slice: console.cs:40527-40556
    /// [@852bf0e] RecallMemory.
    void recallMemory(const MemoryRecord& record);

    /// Quick memory: one frequency + mode + filter, console.cs:36442-36455
    /// [@852bf0e] btnMemoryQuickSave_Click / btnMemoryQuickRestore_Click.
    /// The frequency text persists as Thetis's txtMemoryQuick
    /// (console.cs:2769/:3955 [@852bf0e], AppSettings "MemoryQuick").
    void memoryQuickSave();
    void memoryQuickRestore();
    bool hasQuickMemory() const;

    // ── Phase 3J-1 closeout Item 3 (2026-05-12): TCI Q_INVOKABLE long tail ──
    //
    // ~56 additional shims that TciProtocol calls via QMetaObject::invokeMethod.
    // Without these the matrix test (against TestMockRadioModel) passes but
    // ESDR3 / N1MM / Log4OM / SunSDR-native clients hit silent no-ops on the
    // production RadioModel.  Each shim is documented with what it does
    // semantically (mock parity) and what underlying state it writes (real
    // model side).  Stubs are explicitly labeled "stub until <feature> lands".

    // VFO lock — routes to SliceModel::locked.  TCI carries two-chan-per-rx
    // semantics from Thetis (VFOALock + VFOBLock); Longpath collapses them
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

    // AGC gain — routes to SliceModel::rfGain (-20..120), i.e. Thetis's
    // AGC-T / WDSP AGC top (SetAgcT -> RF -> ptbRF -> AGCMaxGain ->
    // SetRXAAGCTop, console.cs:51746 / radio.cs:1022 [v2.10.3.15]). NOT
    // agcThreshold, which is the -160..2 dB SetRXAAGCThresh point.
    Q_INVOKABLE void setAgcGain(int rx, int gain);
    Q_INVOKABLE int  agcGain(int rx) const;

    // TX-Leistung / Abstimmleistung — Fassade für die TCI-Kommandos `drive:`
    // und `tune_drive:`.  Beide fehlten bis 2026-09-30 vollständig, weshalb
    // die Leistungsschieber jeder Fernbedienung (TCI Remote von ON7OFF,
    // N1MM+, Log4OM) still wirkungslos blieben: TciProtocol verwirft
    // unbekannte Namen ohne Antwort (TciProtocol.cpp, dispatch-Ende).
    //
    // From Thetis TCIServer.cs:4138-4164 [v2.10.3.13] — handleDrive /
    // handleTuneDrive: 1 Argument = Abfrage, 2 Argumente = Setzen; Ziel ist
    // consoleThreadSafe.PWR bzw. TUNPower.  Der Bereich 0..100 wird dort
    // beim SENDEN geprüft (sendDrivePower / sendTunePower,
    // TCIServer.cs:2328-2341: `if (drive < 0 || drive > 100) return;`).
    // Longpath klemmt statt dessen schon hier, damit ein fremder Client mit
    // einem Ausreißer nicht erst eine unmögliche Leistung setzt und dann
    // eine Meldung bekommt, die nie gesendet wird.
    //
    // Ziel ist TransmitModel::power / tunePower — dieselben Eigenschaften,
    // die das TX-Applet über seine Schieber setzt (TxApplet.cpp:866).
    Q_INVOKABLE void setDrivePower(int pct);
    Q_INVOKABLE int  drivePower() const;
    Q_INVOKABLE void setTuneDrivePower(int pct);
    Q_INVOKABLE int  tuneDrivePower() const;

    // Squelch — routes to SliceModel::ssqlEnabled / ssqlThresh.  TCI level
    // is int (-140..0 dBm); SliceModel::ssqlThresh is double in same units.
    Q_INVOKABLE void setSqlEnable(int rx, bool on);
    Q_INVOKABLE bool sqlEnable(int rx) const;
    Q_INVOKABLE void setSqlLevel(int rx, int level);
    Q_INVOKABLE int  sqlLevel(int rx) const;

    // RIT / XIT — routes to SliceModel::ritEnabled/ritHz/xitEnabled/xitHz on
    // the active slice.  Thetis treats these as radio-global (single VFO
    // pair); Longpath collapses to active-slice for symmetry with mode/mox.
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
    // setRxAnf / rxAnf: routes to SliceModel::anfEnabled (Phase 3F Sub-Epic J
    // Task 1 added ANF as its own Q_PROPERTY, independent of the activeNr
    // slot enum).  Previously this pair stubbed ANF state into
    // m_tciStubRxApf -- the APF array -- so toggling ANF via TCI silently
    // flipped APF's stored bit too, and neither one touched real WDSP ANF.
    // Sub-Epic J Task 10 (rx_volume) closeout fixed the routing.
    Q_INVOKABLE void setRxAnf(int rx, bool on);
    Q_INVOKABLE bool rxAnf(int rx) const;

    // setRxBin / rxBin: routes to SliceModel::binauralEnabled.
    // setRxApf / rxApf: routes to SliceModel::apfEnabled.
    // Both are per-receiver upstream (handleRxBinEnable TCIServer.cs:1854-1869,
    // handleRxApfEnable TCIServer.cs:1870-1894 [v2.10.3.15]) and both already
    // had SliceModel properties wired to RxChannel, but Phase 3F chip
    // task_c1e6fbad found these two shims still storing into private arrays
    // and reading straight back out, so a TCI client could set either one, be
    // told it took effect, and change nothing in the DSP chain.
    Q_INVOKABLE void setRxBin(int rx, bool on);
    Q_INVOKABLE bool rxBin(int rx) const;
    Q_INVOKABLE void setRxApf(int rx, bool on);
    Q_INVOKABLE bool rxApf(int rx) const;

    // NOT a stub since TNF section 6.4: these read and write the NotchModel
    // master enable.  Global despite the per-rx command shape, exactly as
    // Thetis GetMNF is (console.cs:52317-52330 [v2.10.3.15]).
    Q_INVOKABLE void setRxNf(int rx, bool on);
    Q_INVOKABLE bool rxNf(int rx) const;

    // ── Stub categories: SliceModel doesn't expose these as Q_PROPERTYs yet ─
    // Each stub stores the requested value in a small per-slice array so
    // round-trip (set then get) returns the operator's last value.  Real
    // wiring to WDSP comes when the underlying feature lands.
    Q_INVOKABLE void setRxEnable(int rx, bool on);
    Q_INVOKABLE bool rxEnable(int rx) const;

    // ── Per-slice AF gain (rx_volume: query source) ──────────────────────
    // Distinct from afLinear() below: afLinear is the single radio-global
    // master volume slider (Thetis console AF field, handleVolume /
    // "volume:" line).  afGain(rx) is the per-receiver AF gain (Thetis
    // RX0Gain/RX1Gain/RX2Gain, handleRxVolume / "rx_volume:" lines), routed
    // to SliceModel::afGain (WDSP RXA panel gain1 -- see the afGainChanged
    // connect in the constructor).  Getter-only: TciProtocol has no
    // rx_volume set/query dispatch case today (only the init burst reads
    // this), matching the calibration getters above.  See
    // TciProtocol.cpp's buildInitialRadioStateLines rx_volume block for the
    // receiver -> slice id mapping this feeds and its active-slice fallback.
    Q_INVOKABLE int afGain(int rx) const;

    // ── Volume (linear int) ──────────────────────────────────────────────
    // setAfLinear: TCI sends 0..32767; we store and let the audio path read.
    // monLinear: TX monitor volume; same range.  These are Longpath-global
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

    // ── Phase 3J-1 closeout (2026-05-22): init-burst live-state shims ───────
    //
    // Added so TciProtocol::buildInitialRadioStateLines can read live
    // RadioModel state instead of emitting the Phase 4 Task 4.2 hardcoded
    // placeholders.  Each shim is a thin Q_INVOKABLE wrapper over existing
    // state or trivial derivation -- no new member variables, no behavior
    // changes.  Architectural divergences (where Longpath's storage model
    // differs from Thetis Console) are documented in each shim header AND
    // mirrored in the TciProtocol.cpp call site so reviewers see the same
    // story from both sides.

    /// "RX2 enabled" -- derived from connectionActiveRxCount >= 2.
    /// Thetis console.cs:37278 [v2.10.3.15] backs RX2Enabled with the
    /// rx2_enabled member (chkRX2.Checked).  Longpath uses the active-
    /// receiver count (set by RadioModel::setActiveRxCountLive) as the
    /// authoritative source.
    Q_INVOKABLE bool rx2Enabled() const;

    /// TX monitor enable -- forwards to TransmitModel::monEnabled().
    /// Thetis console.cs:18656-18663 [v2.10.3.15] -- MON = chkMON.Checked.
    /// Longpath's m_transmitModel.m_monEnabled defaults false and is never
    /// persisted (safety: MON loads OFF always, matching Thetis audio.cs:406).
    Q_INVOKABLE bool monEnabled() const;

    /// Tune state -- m_isTuning (latched true between setTune(true) and
    /// completeTuneOff()).  Thetis console.cs:18677-18684 [v2.10.3.15] --
    /// TUN = chkTUN.Checked.  Semantically identical.
    ///
    /// Separate from the existing isTune() accessor (which is noexcept and
    /// cannot be Q_INVOKABLE).  Same backing field.
    Q_INVOKABLE bool tune() const;

    /// Power-on -- forwards to isConnected().
    ///
    /// Architectural divergence from Thetis console.cs:19799-19803
    /// [v2.10.3.15] PowerOn = chkPower.Checked.  In Thetis PowerOn and
    /// connection state are SEPARATE concepts: a user can "power off" the
    /// radio while remaining connected.  Longpath has no such mode -- the
    /// connection IS the power switch.  TCI clients see powerOn = true
    /// while connected, false while disconnected (no "soft off" state).
    Q_INVOKABLE bool powerOn() const;

    /// Der Empfangspegel in dBm fuer den CAT-Befehl SM; (2026-10-13).
    ///
    /// Gibt -200.0, wenn es noch keinen WDSP-Kanal gibt (vor dem
    /// Verbinden) -- ein Wert, den jeder Aufrufer wegklammert, statt ihn
    /// fuer ein Signal zu halten.
    ///
    /// Uebernommen aus der zweiten CAT-Fassung dieses Projekts (Zweig
    /// fix/discovery-quietpolls-thetis-parity), die ihn fuer SM und fuer
    /// rigctlds STRENGTH gebaut hat. Siehe
    /// docs/architecture/2026-10-12-cat-zwei-fassungen.md.
    Q_INVOKABLE double rxSignalDbm(int rx) const;

    /// DIGL click-tune offset -- active slice's diglOffsetHz.
    ///
    /// Architectural divergence from Thetis console.cs:14693-14749
    /// [v2.10.3.15] DIGLClickTuneOffset (radio-global private member,
    /// default 2210 Hz).  Longpath stores per-slice on SliceModel
    /// (m_diglOffsetHz, default 0 Hz per SliceModel.h:928).  For TCI we
    /// expose the active slice's value as the radio's "current" DIGL
    /// offset; falls back to 0 when no slice is active (pre-connect probe).
    Q_INVOKABLE int diglOffset() const;

    /// DIGU click-tune offset -- active slice's diguOffsetHz.
    ///
    /// Same Thetis-vs-Longpath divergence as diglOffset.  Thetis
    /// console.cs:14658-14691 [v2.10.3.15] -- DIGUClickTuneOffset
    /// (radio-global, default 1500 Hz).  Longpath per-slice
    /// (m_diguOffsetHz, default 0 Hz per SliceModel.h:929).
    Q_INVOKABLE int diguOffset() const;



    /// Gate for the radio's own mic-PTT bit. `session` is the radio
    /// session the report belongs to; anything from an older session is
    /// dropped instead of reaching MoxController.
    ///
    /// The report crosses threads as a QUEUED call, and Qt delivers a
    /// call that was already posted even after the connection behind it
    /// was disconnected or destroyed. Without this gate a mic_ptt=1 from
    /// the dying session lands in the replacement session and keys it --
    /// upstream NereusSDR fixed the same thing after 2026.10.0
    /// ("Prevent a callback from an old radio session from keying a
    /// replacement session", #335).
    ///
    /// Q_INVOKABLE so the test can deliver a report from a stale session
    /// the way Qt would.
    Q_INVOKABLE void handleMicPttFromRadio(quint64 session, bool pressed);

    /// The session number a mic-PTT report must carry to be acted on.
    /// Incremented on every connect and on every teardown.
    quint64 radioSession() const { return m_radioSession; }

    /// Retire the current radio session, so anything still in flight from
    /// it is dropped by handleMicPttFromRadio. Called from the teardown
    /// path; Q_INVOKABLE so the test can retire a session the same way.
    Q_INVOKABLE void retireRadioSession() { ++m_radioSession; }
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
    /// Die SWR-Ueberwachung hat eine offene Antenne erkannt, abgetastet und
    /// den Schluessel zugeriegelt. MainWindow zeigt den Text im Warnstreifen
    /// -- derselbe Weg wie bei moxRejected und den Bandplan-Absagen.
    /// Cite: console.cs:26166-26174 [v2.10.3.15] (checkAntennaWarning).
    void openAntennaWarning(const QString& text);

    // ── Senden ueber die Flex (2026-10-23, Stufe 2) ──────────────────
    /// Die MASCHINE hat das Senden erlaubt oder gesperrt.
    void flexSendefreigabeGeaendert(bool darf);
    /// Die Aussteuerung des Flex-Mikrofonwegs hat sich geaendert.
    void flexMikGainGeaendert(int db);
    /// Der Zustand der Flex-Vorverzerrung hat sich geaendert — der
    /// Schalter, der Messpunkt oder die gelernte Kurve.
    void flexApdGeaendert();
    /// Der Zweiton der Maschine laeuft (oder nicht mehr). Oeffnet und
    /// schliesst das Tor der IMD-Messung am Flex.
    void flexZweitonGeaendert(bool aktiv);
    /// Der Stehwellenriegel hat zugemacht: entweder wurde gar nicht
    /// erst getastet, oder eine laufende Sendung wurde gebrochen.
    /// Betreiber am 2026-10-30: „Sollte SWR ueber 2,5 sein, bitte
    /// nicht senden!" Siehe `safety::SwrRiegel`.
    void flexSwrGesperrt(float swr, float grenze);
    /// Der Abstimmregler ist am Anschlag und kommt trotzdem nicht auf
    /// das Ziel — der Hebel reicht nicht. Mit dem, was wirklich
    /// herauskam, und dem Ziel.
    void flexAbstimmZielUnerreichbar(double istWatt, double zielWatt);
    /// Die Maschine sendet / sendet nicht mehr — gleich wer getastet hat.
    void flexSendetGeaendert(bool sendet);
    /// Auto-SQL hat eine Stufe gestellt (rauschDbm = gemessenes Rauschen
    /// der Scheibe, stufe = was an die Maschine ging).
    void flexAutoRauschsperreGestellt(double rauschDbm, int stufe);

    /// Die Maschine hat ihre Scheibe gemeldet -- ab jetzt kann ein
    /// Befehl an sie ueberhaupt hinausgehen.
    ///
    /// Gemessen am 2026-10-31: der Anmeldeschub laeuft 143 ms VOR
    /// dieser Meldung, und alles, was in dieser Luecke hinaus will,
    /// faellt still an der fehlenden Scheibennummer aus. Statt
    /// siebenmal einen Merkposten zu bauen, sagt die Maschine hier
    /// einmal Bescheid, und wer etwas nachzutragen hat, haengt sich
    /// daran.
    void flexScheibennummerDa();

    void infoChanged();
    // Phase 3Q-1: parametrized — state passed so UI consumers can act without
    // a secondary RadioModel::connectionState() read under race conditions.
    // Existing no-arg slot connections (ConnectionPanel, MainWindow, SpectrumWidget)
    // remain valid: Qt discards excess signal args when slot arity is lower.
    void connectionStateChanged(Longpath::ConnectionState newState);
    // Emitted when the on-air sample rate for the current connection is
    // known. MainWindow reacts by updating FFTEngine + SpectrumWidget so
    // bin math matches the wire rate (P1=192k, P2=768k).
    void wireSampleRateChanged(double rateHz);
    // Task 1.7: emitted after setActiveRxCountLive() successfully applies
    // the new receiver count to both hardware and WDSP channels.
    void activeRxCountChanged(int newCount);

    // Fires when the 4O3A master toggle flips (Setup → CAT & Network →
    // 4O3A → General). Consumers (e.g., MainWindow's applet visibility
    // wiring) react to grey out / hide the Amplifier and Tuner applets
    // when 4O3A is off.
    void fourO3AEnabledChanged(bool enabled);
    // Fires when the RF-Kit master toggle flips (Setup -> CAT & Network ->
    // RF-Kit -> General). Consumers (e.g. MainWindow applet visibility)
    // react to show/hide the RF2K-S applet.
    void rfKitEnabledChanged(bool enabled);
    void speEnabledChanged(bool enabled);
    void kpa500EnabledChanged(bool enabled);
    void acomEnabledChanged(bool enabled);
    // Die CW-Tonhoehe hat sich geaendert (setCwPitch). Thetis:
    // CWPitchChangedHandlers (console.cs:18241). Abnehmer: CW-Decoder
    // (Suchband), KiwiSDR-Nachfuehrung, Setup-Feld.
    void cwPitchChanged(int hz);
    // Fires on each transition to Connected with the RadioInfo of the live
    // connection. HardwarePage (Phase 3I) listens to this to repopulate
    // sub-tabs with per-radio fields.
    void currentRadioChanged(const Longpath::RadioInfo& info);

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
    void pureSignalCoordinatorReady(Longpath::PureSignal* coordinator);
    void sliceAdded(int index);
    void sliceRemoved(int index);

    /// Phase 3F Sub-Epic F Task 5: emitted after the per-ADC wideband FFT
    /// completes.  adcIndex is 0 or 1; dbmBins is 8192 entries (kOutputBins
    /// from WidebandFftEngine).  SpectrumWidget consumes this in extended
    /// pan rendering; visual paint wires in Sub-Epic F polish (T7-T10).
    void widebandSpectrumReady(int adcIndex, QVector<float> dbmBins);
    // Phase 3F Sub-Epic C Task 7: emitted when addSliceOnPan rejects a
    // request because the maxSlices() cap has been reached.  Status-bar /
    // toast subscribers wire to this signal in Sub-Epic C Tasks 8-9.
    void sliceAddRejected(QString reason);

    /// Phase 3F Sub-Epic I closeout, defect F4: the operator retuned a slice
    /// to a frequency no DDC can reach, and the frequency has been rolled
    /// back to the last one that bound. Distinct from sliceAddRejected
    /// because that one talks about adding a slice, which is not what
    /// happened -- the operator turned the knob.
    ///
    /// After this fires, the slice's frequency, stream binding and shift
    /// offset all agree again. `reason` is plain English, ready for a status
    /// bar, and names the frequency the slice stayed on.
    void sliceRetuneRejected(int sliceIndex, const QString& reason);

    /// Phase 3F Sub-Epic I: a stream's slice set changed. Consumers rebuild
    /// FFT routing; RadioModel republishes the set to RxDspWorker.
    void streamBindingsChanged(int streamIndex, const QVector<int>& sliceIndices);

    /// Phase 3F Sub-Epic I closeout, defect F3: streams that still host slices
    /// but that the per-board codec left without a DDC, so the radio has
    /// stopped streaming them. Emitted on transitions only (both into and out
    /// of the suspended state; an empty list means everything is back).
    ///
    /// This happens legitimately on the 1-ADC HERMES class, where Thetis
    /// collapses to a single synced pair whenever PureSignal transmits or
    /// diversity engages (console.cs:8448-8456 [v2.10.3.15]). The behaviour is
    /// upstream-faithful; the silence around it was not, which is what this
    /// signal fixes. `reason` is plain English, ready for a status bar.
    void streamsSuspended(const QVector<int>& streamIndices, const QString& reason);

    /// Phase 3F Sub-Epic I: a stream was activated or retuned; its FFTEngine
    /// and panadapter window must follow.
    void streamCentreChanged(int streamIndex, double centreHz, int sampleRateHz);

    /// Phase 3F Sub-Epic I: emitted whenever the slice or stream set changes
    /// such that the per-board codec must recompute the DDC assignment.
    /// Observation hook; invokeCodecDdcAssignment does the work. Task 7b: it
    /// no longer no-ops while disconnected; only the wire push is gated on
    /// a connection, because the client-side mapping has to be correct
    /// before the first packet arrives.
    void ddcAssignmentRequested();

    // Phase 3F closeout — Sub-Epic E Task 6 consumer wire. Emitted when
    // AlexController auto-switches an antenna due to a conflict-policy
    // re-route (Sub-Epic E Tasks 11-13 will fill in the real detection;
    // for now the signal surface exists so MainWindow can wire
    // AntennaSwitchToast). Carries the slice that moved plus the old and
    // new antenna names. UNDO action is consumer-defined.
    void antennaAutoSwitched(int sliceIndex, QString oldAntenna,
                              QString newAntenna);
    // Phase 3F closeout — Sub-Epic E Task 7 consumer wire. Emitted when
    // adding a slice would force a TX-bound chain re-route to a different
    // antenna. Consumer (MainWindow) opens TxBoundConfirmDialog. Real
    // emission from addSliceOnPan lands when the conflict-policy state
    // machine ships in a follow-up.
    void txBoundReRouteRequested(QString proposedAntenna,
                                  QString existingAntenna);
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
    // Emitted by pushTxModeAndBandpass() when a TX-bound slice exists,
    // BEFORE the queued setter dispatch to TxWorkerThread.  Carries the
    // slice's current DSPMode + audio-space filter cutoffs.  Tests use
    // it as a proxy for "push helper triggered with X"; production code
    // can wire it into diagnostic logging.
    void txModeAndBandpassPushed(Longpath::DSPMode mode,
                                 int audioLowHz, int audioHighHz);
    void panadapterAdded(int index);
    void panadapterRemoved(int index);

    // Raw interleaved I/Q for spectrum display (tapped before WDSP processing)
    void rawIqData(const QVector<float>& interleavedIQ);

    // Phase 3F Sub-Epic I Task 8: stream-tagged companion to rawIqData,
    // which is kept for existing single-slice subscribers. MainWindow's
    // per-stream FFTEngine pool subscribes to this one so each DDC's bins
    // reach its own engine (and from there its own panadapter).
    // Emitted from the same Connection-thread fork as rawIqData.
    void rawIqDataForStream(int streamIndex, const QVector<float>& samples);
    // Derselbe Block mit dem Messabschnitt, den er beim Eintreffen auf dem
    // Verbindungsfaden hatte -- bevor eine Warteschlange dazwischenkommt.
    // Upstream NereusSDR e5b3997bc (#720).
    void rawIqDataForAutoAgc(int streamIndex, const QVector<float>& samples,
                             quint64 epoch);
    // Der Messabschnitt hat gewechselt (siehe autoAgcAcquisitionEpoch).
    void autoAgcAcquisitionChanged(quint64 epoch);

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
    void bandClickIgnored(Longpath::Band band, QString reason);

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

    /// Ein Verbindungsversuch ist gescheitert — EGAL auf welchem Weg,
    /// mit der Begruendung im Klartext.
    ///
    /// Bis zum 2026-08-22 gab es das nicht: connectFailed trug einen
    /// erklaerenden Text, und das Modell warf ihn mit Q_UNUSED(detail)
    /// weg. Gemeldet wurde nur der Auto-Verbinden-Weg. Wer von Hand
    /// verband und scheiterte, sah "Disconnected" und sonst nichts —
    /// der Betreiber hat daraufhin drei Tage die App verdaechtigt,
    /// waehrend es die Netzwerkstrecke war.
    // Flex-Treiber Stufe 1: die vom Gerät gelesene Frequenz einer
    // Scheibe, in Hz. Hört heute nur das Protokoll — bei Flex rechnet
    // das Radio, und wohin Ton und Bild gehen, entscheidet Stufe 2
    // (docs/architecture/2026-10-18-flex-stufe1-design.md §5).
    void flexSliceFrequencyChanged(int sliceId, double frequencyHz);
    /// Was die Maschine über ihre eigene Rauschminderung meldet.
    /// Kommt aus jedem Scheiben-Zustandsschub, der `nr=`, `nb=` oder
    /// `anf=` trägt — auch dann, wenn jemand an der Maschine selbst
    /// oder in SmartSDR geschaltet hat.
    void flexSliceNoiseChanged(bool nr, bool nb, bool anf);
    // Die Maschine hat Longpath als GUI-Client angenommen.
    void flexGuiClientRegistered(const QString& guiClientId);
    // Fertiger FFT-Rahmen in dBm, und die Wasserfallzeile (roh, ein
    // Farbmaszstab — keine dBm, siehe core/FlexVitaReader.h).
    void flexSpectrumReady(quint32 streamId, const QVector<float>& dbm);
    // Fertiges Spektrum in dBm von IRGENDEINEM Geraet, siehe
    // meldeSpektrum(). Kann aus einem fremden Faden kommen.
    void spektrumBereit(int rx, const QVector<float>& dbm, int spanneHz,
                        double mitteHz);
    void flexWaterfallRowReady(quint32 streamId, const QVector<quint16>& values,
                               double lowFreqMhz, double highFreqMhz,
                               quint32 autoBlackLevel);
    // Ton: float32 Stereo verschraenkt, 24 kHz (gemessen 24 012,3 Hz).
    /// Der Ausschnitt, den die Maschine fuer ihren Panadapter angibt
    /// (Hertz). Massgeblich fuer die FFT-Bins — NICHT die Breite der
    /// Wasserfallkachel, die ihren eigenen Rand hat.
    ///
    /// `eigenerZug` sagt, WER ihn verstellt hat: false, wenn die
    /// Meldung von der Maschine kommt, true, wenn Longpath selbst
    /// gezogen hat (`zieheFlexPanadapterAuf`). Ohne diesen
    /// Unterschied zieht das Fenster den eigenen Zug wieder zurueck —
    /// siehe core/FlexFensterZug.h, gemessen am 2026-10-30.
    void flexAusschnittGeaendert(double tiefHz, double hochHz,
                                 bool eigenerZug);

    void flexAudioSamplesReady(quint32 streamId,
                               const QVector<float>& interleavedStereo);

    void connectAttemptFailed(Longpath::ConnectFailure reason,
                              const QString& detail);

    // ── Phase 3M-1a Task G.4: TUNE refused ──────────────────────────────────
    // Emitted when setTune(true) is called but the power-on guard fires
    // (radio not connected / audio engine not active).
    // Cite: Thetis console.cs:29983-29991 [v2.10.3.13] — MessageBox "Power must be on".
    // Longpath equivalent: emit signal; UI reacts with a toast or status bar message.
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
    // Longpath-original glue (no Thetis equivalent needed).
    void txFilterRequest(int audioLowHz, int audioHighHz, Longpath::DSPMode mode);

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

    // Phase 3P-II: PGXL amplifier presence / state / meter signals.
    // amplifierChanged: fires once on the first statusUpdated from PgxlConnection
    //   (m_hasAmplifier transitions false -> true). present=true only.
    // ampStateChanged: fires whenever m_ampOperate changes (OPERATE-family vs not).
    // ampMetersChanged: fires on each statusUpdated that carries peakfwd + swr keys.
    //   fwd is forward power in watts (dBm input converted: watts = 10^(dbm/10)/1000).
    //   swr is the SWR ratio (return-loss dB input: ratio = 10^(-rl/20), clamped >= 1.0).
    void amplifierChanged(bool present);
    void ampStateChanged();
    void ampMetersChanged(float fwd, float swr);

    // Phase 3P-III Task 13: cross-vendor external-amp aggregator signals.
    // Both PgxlConnection state transitions and Rf2ksConnection::operateModeUpdated
    // feed externalAmpOperateChanged so TxApplet and any other consumer can
    // subscribe once rather than per-brand.
    //
    // externalAmpOperateChanged(bool inOperate):
    //   true  when any connected external amp enters OPERATE.
    //   false when all external amps leave OPERATE (or disconnect).
    //
    // externalAmpFwdSwrUpdated(int forwardW, float swr):
    //   fired for every RF-Kit power snapshot so consumers can feed the TX
    //   needle without knowing which amp brand supplied the reading.
    void externalAmpOperateChanged(bool inOperate);
    void externalAmpFwdSwrUpdated(int forwardW, float swr);

private slots:
    void onConnectionStateChanged(Longpath::ConnectionState state);

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

    /// Recompute the drive byte through the NORMAL (non-tune) power path.
    ///
    /// Ports the restore mi0bot performs on every MOX-to-TX transition, at
    /// console.cs:30272 [v2.10.3.13-beta2] inside chkMOX_CheckedChanged2's
    /// `if (tx)` branch:
    ///
    ///   if (!chkTUN.Checked && !chk2TONE.Checked) ptbPWR_Scroll(this, EventArgs.Empty);
    ///   //MW0LGE_22b need this here as we may have adjusted power via tune slider when not in mox
    ///
    /// `ptbPWR_Scroll` calls setPowerFromDriveSlider (console.cs:47601-47607),
    /// which is SetPowerUsingTargetDBM with bFromTune=false. Without it a
    /// preceding TUNE leaves its drive value in place for the next normal
    /// transmit. On the HL2 that value is 0, because the mi0bot carve-out at
    /// console.cs:47660-47673 deliberately zeroes the drive byte for tune
    /// powers at or below 51 and carries the level in the post-gen tone
    /// magnitude instead, so a TUNE silences every following SSB transmit.
    void restoreNormalTxDrive();

    // ── Phase 3J-2 H2: per-source spot-adapter slots ────────────────────────
    //
    // Each ingest client emits spotReceived(DxSpot); the adapter slot
    // translates that into the QMap<QString,QString> kvs shape
    // SpotModel::applySpotStatus expects (TCI-style sink). Per-source
    // lifetime and color defaults are read from AppSettings under the
    // <Source>SpotLifetimeSec / <Source>SpotColor key family.
    //
    // The WSJT-X adapter is special: it also pushes to RxDecodeModel so the
    // "what my radio just heard" feed tracks live decodes (Longpath design;
    // freedv-gui has no equivalent feed). WsjtxClient does not have a
    // separate decodeReceived signal; the single spotReceived signal is the
    // source for both sinks.
    void onClusterSpotReceived(const Longpath::DxSpot& spot);
    void onRbnSpotReceived(const Longpath::DxSpot& spot);
    void onWsjtxSpotReceived(const Longpath::DxSpot& spot);
    void onSpotCollectorSpotReceived(const Longpath::DxSpot& spot);
    void onPotaSpotReceived(const Longpath::DxSpot& spot);
    void onSotaSpotReceived(const Longpath::DxSpot& spot);
    void onFreeDvReporterSpotReceived(const Longpath::DxSpot& spot);
    void onPskReporterSpotReceived(const Longpath::DxSpot& spot);

    // Phase 3P-II Task 19: PGXL status update handler.
    // Called on every statusUpdated from PgxlConnection. On first call sets
    // m_hasAmplifier and emits amplifierChanged(true). Parses the "state" key
    // to update m_ampOperate and emits ampStateChanged() on transition. Parses
    // "peakfwd" (dBm) and "swr" (return-loss dB) and emits ampMetersChanged.
    void onPgxlStatus(const QMap<QString, QString>& kvs);

    // Phase 3P-II Task 62: runs the amplifierCreate + flexradioPair +
    // enableKeepalive sequence once PgxlConnection reports connected.
    // Reads PGXL_PairAttempt / PGXL_FlexAmpSlice / PGXL_TxAnt / PGXL_AntMap
    // from AppSettings. Serial is "Longpath-<macAddress>".
    void onPgxlConnected();

    // Phase 3P-II Phase 4 Task 96: auto-recall TGXL tune memory when the
    // TX-bound slice crosses a band boundary. Connected to
    // SliceModel::bandChanged from addSlice(). Fires only when
    // TGXL_AutoTuneMemoryRecall == "True" and a stored entry exists for
    // (activeAntenna, newBand).  Falls back to issuing "tune start" per
    // design bench-caveat (absolute relay-write API not yet confirmed).
    void onSliceBandChanged(SliceModel* source, Longpath::Band band);

private:
    // Phase 3Q-1: drives the RadioModel-level connection state machine.
    // Guards against redundant transitions (no emit if state unchanged).
    void setConnectionState(ConnectionState s);

    // ── Ein eigener Fund, dreifach weitergereicht (2026-10-17) ──────
    //
    // Spotliste (und damit Panadapter), der Sammler „was mein Geraet
    // gehoert hat", und PSK Reporter. Einmal geschrieben fuer die
    // eigenen FT8- und RTTY-Funde: der Rumpf stand vorher nur in
    // onNativeFt8Decode, und eine zweite Abschrift waere die Stelle
    // gewesen, an der die beiden auseinanderlaufen. Lebensdauer,
    // Farbschluessel und die Betriebsart, die gilt, wenn im Kommentar
    // keine steht, kommen vom Aufrufer.
    void meldeEigenenFund(const Longpath::DxSpot& spot,
                          const QString& lebensdauerSchluessel,
                          int lebensdauerVorgabeSek,
                          const QString& farbSchluessel,
                          const QString& modusVorgabe);

    // ── Per-radio peripherals lifecycle ────────────────────────────────────
    // Drive the RF-Kit / 4O3A / PGXL / TGXL connections from the connection
    // state machine.  applyPeripheralsForCurrentMac() runs after the radio
    // reports Connected (and m_lastRadioInfo.macAddress is populated);
    // teardownPeripherals() runs on Disconnected / LinkLost.
    //
    // migratePeripheralGlobalsIfNeeded() is a one-shot that folds the legacy
    // GLOBAL RfKit_* / FourO3A_Enabled / PGXL_Manual* / TGXL_Manual* keys
    // into the currently connected radio's hardware/<mac>/peripherals/
    // scope on the FIRST Connected event after this code lands.  Subsequent
    // launches see PeripheralsMigrationDone="True" and skip.
    void applyPeripheralsForCurrentMac();
    void teardownPeripherals();
    void migratePeripheralGlobalsIfNeeded();

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

    // Welches Modell fuer dieses Geraet gilt, und welche Kenndaten. Aus
    // connectToRadio herausgeloest, weil die SunSDR hier eine Ausnahme
    // braucht (sie steht in keiner HPSDRModel-Liste) und die Entscheidung
    // ohne Netz pruefbar sein soll.
    void applyHardwareProfileFor(const RadioInfo& info);

    // Pushes AlexController's per-band antenna state to the connection.
    // Full port of Thetis HPSDR/Alex.cs:310-413 UpdateAlexAntSelection.
    // Phase 3P-I-b (T6): adds isTx branch, Ext1/Ext2OnTx mapping, xvtrActive
    // gating, and rxOutOverride clamp. MOX coupling and Aries clamp deferred
    // to Phase 3M-1 (TX bring-up). isTx defaults to false so existing callers
    // are unaffected.
    //
    // Source: Thetis HPSDR/Alex.cs:310-413 [@501e3f5].
    void applyAlexAntennaForBand(Longpath::Band band, bool isTx = false);
    // Reconciles the TX-bound slice's stored antenna intent into the
    // per-band Alex state. Called at every authority boundary (slice edit,
    // TX handoff, and immediately before MOX routing).
    void applyTxAntennaFromBoundSlice();

    void wireConnectionSignals(int wdspInSize);
    /// Wire one slice's property changes to its OWN WDSP channel and to the
    /// radio. Call for every slice, not just the active one: this used to
    /// read m_activeSlice and run once, leaving 65 per-slice DSP handlers
    /// (AGC, filter, mode, NB, SNB, APF, RIT/XIT, squelch, mute, pan and the
    /// whole NR parameter set) wired for Slice A alone -- and every one of
    /// them writing rxChannel(0). Idempotent per slice: the handlers use
    /// Qt::UniqueConnection-safe member targets or are wired once at
    /// addSlice time.
    void wireSliceSignals(SliceModel* slice);

    /// Connect NotchModel's mutation signals to the per-channel WDSP
    /// fan-out. Called once from the ctor; NotchModel outlives every
    /// connection.
    void wireNotchModel();

    /// Push the full notch state at one channel: the list, the master run
    /// flag, the auto-increase flag and the NBP tune frequency. `channelId`
    /// is also the slice index (Sub-Epic I invariant), which is how the
    /// hosting stream's centre is resolved for the tune frequency
    /// (design section 4.1).
    void syncNotchesToChannel(RxChannel* ch, int channelId);

    /// Every WDSP RX channel that currently backs a slice. The fan-out
    /// target set for a live notch mutation (design section 6.3).
    QVector<RxChannel*> sliceRxChannels() const;

    /// Design section 6.2: our list position IS the WDSP notch index, so a
    /// count divergence is a correctness bug. Detect and recover with a full
    /// resync rather than assert, which a release build compiles out.
    void reconcileNotchCount(RxChannel* ch);

    // Recomputes the transmit frequency from the TX-bound slice and pushes it
    // at the connection. The single place that answers "what frequency is the
    // radio transmitting on", so the Alex TX low-pass, the TX NCO and the
    // drive-level band gate cannot disagree about it.
    //
    // Mirrors Thetis UpdateTXDDSFreq(), which likewise recomputes from
    // tx_dds_freq_mhz and fans out to setAlexLPF(..., true) and
    // NetworkIO.VFOfreq(0, tx_dds_freq_mhz, 1) together
    // (console.cs:15464-15485 [v2.10.3.15]).
    //
    // XIT is included and RIT is not, per Thetis console.cs:31782-31784
    // [v2.10.3.15]: udXIT lands on tx_freq, udRIT on rx_freq.
    void pushTxFrequencyFromTxSlice();

    // The total WDSP shift for a slice: the allocator's offset from its
    // hosting stream's centre, plus RIT, plus the per-mode DIG click-tune
    // offset. Five sites push the shift (bindSliceToStream,
    // activateSliceChannel, reshiftSlicesOnStream,
    // commitStreamSampleRateChange and the RIT/DIG lambda in
    // wireSliceSignals) and they used to disagree about which terms belonged
    // in it, so each clobbered the others'. Toggling RIT on a shifted slice
    // threw away the stream offset; retuning with RIT on threw away the RIT.
    //
    // Reads slice->shiftOffsetHz(), so every caller must commit the stream
    // term to the model before calling.
    // See docs/architecture/2026-07-28-tunable-notch-filter-design.md 4.4.
    double composedShiftHz(const SliceModel* slice) const;
    /// Shift derived from an EXPLICIT stream centre, so it cannot disagree
    /// with the NOTCHDB::tunefreq written alongside it. Design section 4.1.
    double composedShiftHz(const SliceModel* slice, double streamCentreHz) const;
    /// The ONLY writer of the notch RF origin. Writes tunefreq and shift
    /// together from one centre; WDSP sums them (nbp.c:192).
    void pushNotchOrigin(SliceModel* slice, RxChannel* ch, double streamCentreHz);

    /// Coalesces notch edits during a drag. RXANBPEditNotch runs a full
    /// UpdateNBPFilters (an FFT per partition at nc=4096, plus a bpsnba
    /// recalculation) and swaps the masks under the DSP lock, so pushing one
    /// per mouse-move costs ~50 filter redesigns per second PER CHANNEL.
    /// Thetis does push per move (console.cs:49967 [v2.10.3.15]) but for one
    /// notch on one channel; multi-pan multiplies that by the channel count.
    /// The marker is drawn from the model and does not wait for this, so the
    /// only thing rate-limited is the DSP redesign.
    void scheduleNotchEditPush(int id);
    void flushNotchEditPush();

    QTimer* m_notchEditTimer{nullptr};
    QSet<int> m_pendingNotchEdits;

    // The connect-time DDC seed, factored out of the wireSliceSignals
    // singleShot so it can be driven without a live connection. Commands the
    // centre of whichever stream hosts `slice`, then re-seeds the TX NCO.
    // See docs/architecture/2026-07-28-tunable-notch-filter-design.md 4.5.
    void seedConnectFrequency(SliceModel* slice);

    // The frequency a slice would actually transmit on: its dial plus XIT.
    //
    // One answer for three callers, because they had drifted apart. The
    // transmit-frequency push folded XIT in; both TUNE arms read the raw
    // dial. Keying TUNE with XIT set therefore put the carrier somewhere the
    // transmit chain had not been told about, and with XIT straddling a
    // filter edge that included the Alex transmit low-pass.
    //
    // From Thetis console.cs:31774-31783 [v2.10.3.15]
    //   double tx_freq = freq;
    //   ...
    //   if (chkXIT.Checked) tx_freq += (int)udXIT.Value * 0.000001;
    // The TUNE offsets are applied to that same tx_freq afterwards
    // (console.cs:31845-31860 [v2.10.3.15]) before it becomes
    // tx_dds_freq_mhz at console.cs:31891, so TUNE transmits on the
    // XIT-shifted frequency too.
    //
    // Returns 0 for a null slice, and clamps at 0 rather than wrapping.
    quint64 txFrequencyForSlice(const SliceModel* slice) const;
    void teardownConnection();

    // Derives the 16-digit dashed FlexRadio-style serial number from the
    // radio's MAC address. SHA-256(mac + salt) -> first 8 bytes -> uint64 ->
    // mod 10^16 -> "XXXX-XXXX-XXXX-XXXX". Used by both onPgxlConnected() and
    // connectToRadio() (FlexRadio discovery beacon) so the serial matches in
    // both contexts. If PGXL_FlexRadioSerial is set in AppSettings, returns
    // that override instead of the derived value.
    QString derivedFlexSerial(const QString& mac) const;

    // Issue #182 — wire TransmitModel::micPttDisabledChanged →
    // RadioConnection::setMicPTTDisabled and prime the connection with the
    // current model value once.  Extracted so the connect() can be exercised
    // in isolation by tst_radio_model_mic_ptt_wire without needing to spin
    // up the full DSP-thread pipeline that wireConnectionSignals starts.
    void connectMicPttDisabledSignal();
    // Werkbank Runde 4: Boost / Line-In / Tip-Ring / Bias / XLR an den Draht.
    void connectMicInputSignals();

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
    // chkMOX_CheckedChanged2) and AFTER `await Task.Delay(100)`.  In Longpath
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

    // Issue #153 sub-bug 2 — push the TX-bound slice's DSPMode plus the
    // TransmitModel's positive audio-space filter cutoffs to TxChannel.
    // No-op if no TX binding resolves.
    //
    // SliceModel filter bounds are RX/IQ-space and signed for LSB-family
    // modes, so they are deliberately not a TX bandpass source.
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
    // The MOX-engage trigger is Longpath's belt-and-suspenders re-seed
    // (Thetis seeds at mode-change only; we additionally re-seed at
    // MOX-engage so prior TUN-state desync cannot starve SSB MOX).
    void pushTxModeAndBandpass();
    void installBandPlanMoxCheck();

    // ── Phase 3F Sub-Epic B Task 16: multi-slice codec glue ─────────────────
    // Build the 5-element codec input array. Phase 3F Sub-Epic I Task 7b:
    // indexed by DDC STREAM, not by slice. Slot [st] is live when the
    // allocator reports stream `st` active; frequencyHz is the stream's
    // window CENTRE (the DDC tunes there; slices sit at shift offsets inside
    // it) and the per-slice flags are folded across slicesOnStream(st).
    // Indexing by slice handed two co-hosted slices two different DDCs,
    // contradicting the sharing model they were bound under.
    // Longpath-original; no Thetis equivalent (Thetis builds UpdateDDCs
    // inputs inline in console.cs:8186-8538 [v2.10.3.15]).
    std::array<Longpath::SliceConfig, 5> buildStreamConfigsForCodec() const;

    // Phase 3F Sub-Epic I Task 7b: run the per-board codec over the current
    // stream set and return its DdcAssignment. Pure: no wire I/O, no model
    // mutation, safe to call while disconnected. Split out of
    // invokeCodecDdcAssignment so the mapping is testable without a socket.
    //
    // Codec source: the RadioConnection owns the codec and is authoritative
    // whenever a connection object exists; ReceiverManager holds the same
    // non-owning pointer (wired at connect, cleared in reset()) and is the
    // fallback when it does not.
    //
    // std::nullopt when no codec has been selected yet, which is NOT the same
    // fact as an assignment whose streamDdc entries are all -1 and must not be
    // spelled the same way. Bench report 2026-07-31 (JJ, KG4VCF): this used to
    // return an all-idle assignment for "nobody has been asked", and
    // publishDdcAssignment cannot tell that apart from a codec answering that
    // the radio has stopped every stream. connectToRadio binds the slice pool
    // before it installs the codec, so every connect published a fabricated
    // "no DDCs anywhere", deactivated the receiver it had activated forty
    // lines earlier, and dropped every I/Q packet until the operator moved the
    // VFO. See tst_connect_routes_first_iq.
    std::optional<Longpath::DdcAssignment> computeDdcAssignment() const;

    /// Phase 3F Sub-Epic I closeout, defect F3: single read of the radio-state
    /// codec inputs (MOX / PureSignal / diversity), so computeDdcAssignment and
    /// describeSuspendedStreams cannot disagree about them.
    Longpath::CodecContext currentCodecContext() const;

    /// Whether the radio is running the diversity DDC pair right now.
    /// Extracted from currentCodecContext so republishAlexAdcSlices reads the
    /// same answer the codec branched on, including under the
    /// setDdcContextForTest seam. Two reads of this would be two chances for
    /// the filter decision and the DDC map to disagree about the same
    /// transmit-critical state.
    bool diversityActive() const;

    /// Reconcile the process-wide WDSP slot and the DSP worker's paired raw-DDC
    /// route against one complete codec assignment. This is the sole start
    /// owner, which keeps source selection and hardware publication atomic from
    /// the model's point of view.
    void reconcileExternalDiversityRoute(
        const Longpath::DdcAssignment& assignment);

    /// Resolve the stable target's primary DDC plus the assignment's DDC0 sync
    /// partner. Returns false when PureSignal owns the pair or the codec did not
    /// publish two equal-rate diversity legs. The sync partner need not also
    /// appear in ddcEnable; Hermes-class Thetis assignments enable DDC0 and
    /// activate DDC1 through syncEnable alone.
    bool resolveExternalDiversitySources(
        const Longpath::DdcAssignment& assignment,
        const SliceModel* target, int& primaryDdc, int& secondaryDdc) const;

    /// Apply the target's current phase/gain rotation to an already-created
    /// external-diversity slot.
    void configureExternalDiversityRotation(const SliceModel* target);

    // Phase 3F Sub-Epic I Task 7b: publish a computed assignment onto the
    // client-side model. Routes each stream's hardware DDC to its logical
    // receiver via ReceiverManager::setDdcMapping, stamps every slice with
    // the DDC of the stream hosting it, and reconciles ReceiverManager's
    // per-stream active flag against slicesOnStream(). No wire I/O, so it
    // runs whether or not a connection exists.
    void publishDdcAssignment(const Longpath::DdcAssignment& assignment);

    // Drive the per-board codec's applyDdcAssignment(), forward the result to
    // P2RadioConnection (P1 wire integration deferred to Sub-Epic C), then
    // publish the mapping client-side. The wire push is gated on an actual
    // connection; the client-side publish is not.
    void invokeCodecDdcAssignment();

    /// Plain-English sentence naming the affected slice letters and why they
    /// lost their receiver. Empty when nothing is suspended.
    QString describeSuspendedStreams(const QVector<int>& streams) const;

    /// Publish one I/Q frame to the two taps: rawIqDataForStream always,
    /// rawIqData only for stream zero.
    ///
    /// Codex review round 7, PR #293. The untagged rawIqData signal
    /// predates multi-stream, and its only subscriber (TciServer) still
    /// hardcodes receiver 0. Forwarding every stream through it fed a
    /// single TCI IQ client frames from unrelated frequencies under one
    /// header. Named rather than left inline so the rule has somewhere to
    /// be stated and somewhere to be tested.
    void forkIqToTaps(int receiverIndex, const QVector<float>& samples);

    // ── Phase 3F Sub-Epic I: slice-to-stream binding ───────────────────────

    /// Run the allocator for `slice` at `frequencyHz` and apply the result
    /// (stream binding, shift offset, stream centre, codec recompute).
    /// Returns false and emits sliceAddRejected when the hardware has no
    /// room. Returns false silently when the pool has not been sized yet
    /// (disconnected): there is no DDC to bind to, and a slice with
    /// streamIndex() < 0 is unbound and feeds nothing.
    /// `preferOwnStream` is forwarded to SliceStreamAllocator::placeSlice on a
    /// first bind, and says the caller wants an independent window rather than
    /// the cheapest placement. Set by the +PAN path; see that header for why a
    /// pan and a slice want different answers. Ignored on a retune, which
    /// already owns a stream.
    bool bindSliceToStream(SliceModel* slice, double frequencyHz,
                           bool preferOwnStream = false);

    /// Mirror a stream's liveness into ReceiverManager's active-receiver set,
    /// which is what decides whether that hardware DDC's samples are forwarded
    /// or dropped. Called from bindSliceToStream on both edges. Idempotent.
    /// See the definition for the bench defect that showed the two were never
    /// connected.
    void syncReceiverToStream(int streamIndex, bool live);

    /// Push the current slice set for `streamIndex` to RxDspWorker and emit
    /// streamBindingsChanged. Called after every bind / unbind.
    void republishStreamBindings(int streamIndex);

    /// Emit ddcAssignmentRequested and drive the per-board codec recompute.
    void requestDdcAssignment();

    /// Phase 3F: group the live slices by the ADC their stream sits on, hand
    /// each group to AlexController::notifySlicesOnAdc, and push the resulting
    /// per-chain band-pass decision at the connection.
    ///
    /// Closes the gap CT1IQI reported on PR #293: the per-ADC analysis existed
    /// but had no producer and no consumer, so the wire took its HPF from
    /// whichever receiver was retuned last and a second slice on another band
    /// made the first one deaf.
    void republishAlexAdcSlices();

    /// Phase 3F Sub-Epic I closeout, defect H1: put the DSP side of the pool
    /// back in step with the allocator after anything moves a stream's rate
    /// or moves a slice between streams.
    ///
    /// Two halves of one geometry, both derived from the stream's rate
    /// through the single bufferSizeForRate() in the tree:
    ///   * RxDspWorker's accumulator drain threshold for that stream, and
    ///   * SetInputSamplerate / SetInputBuffsize on the WDSP channel of every
    ///     slice bound to it.
    ///
    /// This is ChannelMaster's SetXcmInrate, split across the two objects
    /// Longpath keeps the state in:
    ///   From Thetis cmaster.c:461,473-475 [v2.10.3.15]
    ///     pcm->xcm_insize[in_id] = getbuffsize (rate);
    ///     for (i = 0; i < pcm->cmSubRCVR; i++) {
    ///         SetInputSamplerate (chid (in_id, i), rate);
    ///         SetInputBuffsize (chid (in_id, i), pcm->xcm_insize[in_id]);
    ///     }
    ///
    /// The two must never disagree while a drain can run: fexchange2 copies
    /// ch[channel].in_size samples out of the buffer it is handed
    /// (iobuffs.c:532-536 [WDSP v1.29]) and ignores any count we pass, so a
    /// drain threshold below the channel's in_size reads past the end of the
    /// accumulator. Rather than order the two writes (the safe order inverts
    /// between widening and narrowing), this quiesces the I/Q feed for the
    /// duration exactly as setSampleRateLive steps 2 and 10 do, so no drain
    /// can observe a half-applied geometry at all.
    ///
    /// Cheap and side-effect-free when nothing is out of step, which is every
    /// call on a single-rate radio.
    void applyStreamDspGeometry();

    /// Re-run the codec after a MOX, effective PureSignal, or diversity
    /// transition through the same complete request used by slice binding.
    /// Protocol 2 therefore has one full DdcAssignment wire owner; Protocol 1
    /// keeps its legacy PsDdcConfig wire path while this request publishes
    /// the client-side assignment.
    void refreshDdcAssignmentForRadioState();

    /// Prevent new paired feeds, then stop and destroy WDSP external-diversity
    /// slot 0. The worker clear is synchronous when it lives on the DSP thread,
    /// so no queued raw-I/Q delivery can process against a torn-down slot.
    void stopExternalDiversityRoute();

    /// Streams the codec left without a DDC while slices are still bound to
    /// them. Empty in steady state.
    QVector<int> suspendedStreams() const { return m_suspendedStreams; }

    /// Phase 3F Sub-Epic I closeout, defect F4 test seam: a stream's window
    /// centre, so a test can reconstruct the frequency WDSP is actually
    /// demodulating (centre + the slice's shift offset) and require it to
    /// match what the VFO reads.
    double streamCentreHzForTest(int streamIndex) const {
        return m_streamAllocator.streamCentreHz(streamIndex);
    }
    int streamSampleRateHzForTest(int streamIndex) const {
        return m_streamAllocator.streamSampleRateHz(streamIndex);
    }
    /// Die Rate, die der Zuteiler diesem Strom gibt, 0 ausserhalb des
    /// Pools (z. B. der SunSDR-Pseudostrom). MainWindow liest sie beim
    /// Ratenwechsel fuer jeden zwischengespeicherten Strom (2026-11-10).
    int streamSampleRateHz(int streamIndex) const {
        return m_streamAllocator.streamSampleRateHz(streamIndex);
    }
    bool streamActiveForTest(int streamIndex) const {
        return m_streamAllocator.isStreamActive(streamIndex);
    }

    /// TNF Task 1 test seam (design doc 4.5): runs the connect-time DDC seed
    /// without a live connection, so a test can assert the quantity it
    /// commands.
    void seedConnectFrequencyForTest(SliceModel* slice) {
        seedConnectFrequency(slice);
    }

private:
    struct PlannedSlicePlacement {
        int sliceId{-1};
        int previousStream{-1};
        SliceStreamAllocator::Placement placement;
        int resolvedRateHz{0};
    };

    struct StreamRateChangePlan {
        SliceStreamAllocator allocator;
        QVector<PlannedSlicePlacement> slices;
    };

    std::optional<StreamRateChangePlan>
    planStreamSampleRateChange(int streamIndex, int rateHz) const;

    void commitStreamSampleRateChange(const StreamRateChangePlan& plan);

    // Flex-Treiber Stufe 1: oeffnet den Befehlskanal zu einer
    // FlexRadio-Maschine. Getrennt von connectToRadio()s uebrigem Weg,
    // weil dort alles OpenHPSDR ist (HardwareProfile, Alex, OC-Matrix,
    // Platinenfaehigkeiten) und fuer ein Flex nichts davon gilt.
    void connectFlexRadio(const RadioInfo& info);
    void teardownFlexConnection();
    // Startet den VITA-49-Strom, meldet den Port und laesst sich Bild
    // und Ton schicken. Laeuft erst, wenn die GUI-Anmeldung durch ist:
    // ohne sie gibt es keinen Panadapter, dem der Strom gehoeren koennte.
    void startFlexStreams();
    /// Richtet den Flex-Panadapter ein (Breite, Hoehe, dBm-Bereich) und
    /// gibt denselben Maszstab an den Leser. Darf mehrfach laufen; wird
    /// aus startFlexStreams UND aus dem Statuseingang gerufen, weil die
    /// Kennung oft erst nach der Anmeldung eintrifft.
    void applyFlexPanSetup();
    /// Stellt den Ton der Scheibe her — aber erst, wenn der
    /// Zustandsschub der Maschine gesagt hat, was dort steht, und nur
    /// das, was wirklich im Weg ist.
    void applyFlexAudioSetup();

public:
    /// Die Flex auf eine Frequenz stellen (Hertz).
    ///
    /// Bis zum 2026-10-22 ging die Steuerung nur in eine Richtung —
    /// und auch die nur halb: `flexSliceFrequencyChanged` wurde
    /// ausgesendet und von niemandem gehoert. Longpaths VFO zeigte
    /// also nie, worauf die Maschine steht, und drehen liess sie sich
    /// gar nicht.
    ///
    /// From AetherSDR SliceModel.cpp:124 [@d58e2b8a] — die Befehlsform
    /// samt `autopan=0`: „SmartSDR pcap confirms: scroll-wheel uses
    /// 'slice tune <id> <freq> autopan=0'". Das `autopan=0` ist hier
    /// nicht Beiwerk — ohne es zentriert die Maschine den Panadapter
    /// neu, und der Ausschnitt, an dem unser Bild haengt, springt.
    void tuneFlexSlice(double hz);

    /// Die Betriebsart der Flex-Scheibe stellen.
    ///
    /// Bis zum 2026-10-22 war `slice tune` der EINZIGE Befehl, den
    /// Longpath an eine Flex-Scheibe schicken konnte. Die Betriebsart
    /// wurde einmal beim Verbinden gesetzt und danach nie wieder: die
    /// Bedienung zeigte LSB, die Maschine stand auf USB. Der
    /// Betreiber hoerte Stationen, aber keine verstaendliche Sprache —
    /// „als waere es eine andere modulation". Er hatte recht, es WAR
    /// eine andere.
    ///
    /// Longpaths Namen und die der Maschine decken sich nicht: CWL und
    /// CWU heiszen dort beide `CW`, und DSB, SPEC, DRM sowie die
    /// RADE-Betriebsarten gibt es nicht. Was kein Gegenstueck hat,
    /// wird NICHT geschickt — eine abgewiesene Betriebsart liesze die
    /// Scheibe im alten Zustand und die Anzeige im neuen, also wieder
    /// zwei Wahrheiten.
    void setFlexSliceMode(DSPMode mode);

    /// Die Filterflanken der Flex-Scheibe stellen (Hertz, bezogen auf
    /// die Traegerfrequenz — genau wie `filter_lo`/`filter_hi` im
    /// Scheibenzustand).
    ///
    /// Derselbe Befund wie bei der Betriebsart, gemeldet in einem
    /// Atemzug: „und der bandfilter zeigt auch zu wenig an". Die
    /// Flanken standen in Longpath und nirgends sonst.
    void setFlexSliceFilter(int lowHz, int highHz);

    /// Rauschminderung, Störaustaster und Kerbfilter der Flex-Scheibe
    /// stellen — also in der MASCHINE, nicht in WDSP.
    ///
    /// Der Betreiber am 2026-10-28: „ich kann zwar klicken, aber kein
    /// unterschied zu hören." Flex-Ton kommt fertig demoduliert in den
    /// Mischer und läuft nie durch den WDSP-Empfangskanal, in dem
    /// Longpaths NR/NB/ANF sitzen. Der Knopf ging an, das Rauschen
    /// blieb. Jetzt geht er dorthin, wo gerechnet wird.
    void setFlexSliceNoise(bool nr, bool nb, bool anf);

    /// Den Wasserfallstrom der Maschine einrichten. Er ist von ihrem
    /// Panadapter GETRENNT und hat eigene Einstellungen; Longpath hat
    /// sie bis zum 2026-10-23 nie angefasst.
    void richteFlexWasserfallEin();

    /// Den Panadapter der Maschine auf eine Frequenz ziehen, ohne
    /// Randbedingung. Beim Verbinden gebraucht: Longpath uebernimmt
    /// erst den Ausschnitt der Maschine, danach stellt das
    /// Layoutprofil eine womoeglich ganz andere Scheibe her — ohne
    /// diesen Griff haben die beiden keine Ueberlappung, und der
    /// Panadapter hat nichts zu zeichnen.
    void zieheFlexPanadapterAuf(double hz);

    /// Die Frequenz, die die MASCHINE zuletzt gemeldet hat (Hertz).
    /// Beim Verbinden gewinnt sie gegen das gespeicherte Profil: der
    /// Betreiber hat die Maschine womoeglich gerade von SmartSDR oder
    /// der Frontplatte aus benutzt, und ihn davon wegzuziehen ist das
    /// Letzte, was ein Programm beim Starten tun soll.
    double flexGeraeteFrequenzHz() const { return m_flexGeraeteFrequenzHz; }

    // ── Senden ueber die Flex (2026-10-23, Stufe 2) ──────────────────
    //
    // Nichts davon tastet von selbst. `flexDarfSenden()` ist das Tor,
    // das die MASCHINE aufmacht (`interlock tx_allowed`), und
    // `flexSendetGerade()` sagt, ob sie wirklich sendet — auch dann,
    // wenn jemand die Taste am Mikrofon drueckt oder ein zweiter
    // Client tastet.

    /// Die Maschine erlaubt das Senden, so wie sie es zuletzt gemeldet
    /// hat. Siehe auch `flexVerriegelungGehoert()`: ohne Meldung ist
    /// dieser Wert eine VERMUTUNG, kein Befund.
    bool flexDarfSenden() const { return m_flexDarfSenden; }

    /// Ob ueberhaupt je eine Verriegelungsmeldung kam. Trennt „die
    /// Maschine sagt nein" von „die Maschine hat nichts gesagt" —
    /// zwei Dinge, die am 2026-10-27 einen Abend gekostet haben, weil
    /// das Tasttor sie fuer dasselbe hielt.
    bool flexVerriegelungGehoert() const { return m_flexVerriegelungGehoert; }

    /// Die Maschine sendet gerade — gleich wer sie getastet hat.
    bool flexSendetGerade() const { return m_flexSendetGerade; }

    /// Die rohe Lage der Verriegelung (RECEIVE, PTT_REQUESTED,
    /// TRANSMITTING, UNKEY_REQUESTED, TIMEOUT, STUCK_INPUT, TX_FAULT …)
    /// und, wenn die Maschine einen nennt, der Grund.
    QString flexVerriegelungsLage() const { return m_flexVerriegelungsLage; }
    QString flexVerriegelungsGrund() const { return m_flexVerriegelungsGrund; }
    QString flexVerriegelungsQuelle() const { return m_flexVerriegelungsQuelle; }

    /// Einen fertigen Flex-Befehl hinausschicken. Fuer die Faelle, in
    /// denen der Aufrufer den Wortlaut aus FlexCommandWire schon hat.
    /// Gibt false zurueck, wenn keine Verbindung steht.
    bool sendeFlexBefehl(const QString& befehl);

    /// Tasten. Gibt true zurueck, wenn der Befehl hinausging.
    ///
    /// Beim Tasten (true) wird vorher geprueft: Verbindung da,
    /// Sendescheibe gesetzt, und die Maschine hat das Senden erlaubt.
    /// Beim LOESEN (false) wird NICHTS geprueft — ein Befehl, der
    /// aufhoert zu senden, geht immer hinaus.
    bool flexTasten(bool tasten);

    /// Zu welchem Antennenweg eine Stehwellenmessung gehoert
    /// (`bandKeyName`, also "20m"). Eine Messung auf einem Band sagt
    /// ueber ein anderes nichts — siehe `SwrRiegel`, Nachtrag 1 vom
    /// 2026-10-31.
    QString flexSwrBandKennung() const;

    /// Die Rauschsperre an die Maschine schicken. Bei der Flex macht
    /// SIE die Aufbereitung -- unsere WDSP-Sperre hat dort kein Ziel
    /// (Befund des Betreibers, 2026-10-31).
    void sendeFlexRauschsperre(bool an, double stufe0bis100);

    /// Auto-SQL an der Flex (2026-11-08, siehe FlexAutoRauschsperre.h):
    /// die Stufe folgt dem Pegel der Scheibe plus Abstand. Gemessen
    /// wird immer (ausser beim Senden), gesendet nur, solange an.
    void setFlexAutoRauschsperre(bool an);
    bool flexAutoRauschsperre() const { return m_flexAutoSqlAn; }
    void setFlexAutoMarge(int db);
    int  flexAutoMarge() const { return m_flexAutoSql.marge(); }
    const FlexAutoRauschsperre& flexAutoSchaetzer() const { return m_flexAutoSql; }
    /// Fuer den Pruefstand: ein Pegel der Scheibe, als kaeme er aus dem
    /// Messwertstrom -- derselbe Weg wie dort.
    void flexPegelFuerTest(double dbm, qint64 jetztMs) { fuehreFlexAutoSqlNach(dbm, jetztMs); }

    /// Die uebrigen Regler der Scheibe, einzeln an die Maschine.
    ///
    /// Alle aus demselben Grund wie die Rauschsperre:
    /// `wireSliceSignals` steigt bei `!m_connection` aus, und das ist
    /// die OpenHPSDR-Verbindung -- am Flex also nie da. Was dort
    /// wirken soll, musz der Flex-Weg selbst schicken.
    ///
    /// `agcModus` nimmt nur die vier Namen, die die Maschine fuehrt
    /// (off/slow/med/fast); alles andere geht nicht hinaus und sagt
    /// das im Protokoll.
    void sendeFlexAgcModus(AGCMode modus);
    /// Die AGC-Schwelle. Unser Regler laeuft -160..0 (dBm-Anmutung
    /// aus Thetis), die der Maschine 0..100. Die Umrechnung ist
    /// VERHAELTNISMAESZIG, keine physikalische -- siehe die
    /// Begruendung an der Umsetzung.
    void sendeFlexAgcSchwelle(int unsereSchwelle);

    /// Welchen Minderer der Maschine unser Steckplatz bedeutet.
    ///
    /// Leer heiszt: diesen gibt es dort nicht (DFNR, BNR, MNR rechnen
    /// wir selbst; NNR hat kein Gegenstueck).
    static QString flexNrSchluessel(NrSlot steckplatz);
    /// Genau EINEN der fuenf einschalten, die anderen aus.
    void sendeFlexNrVerfahren(NrSlot steckplatz);
    /// Die Staerke des gewaehlten Verfahrens (0..100).
    void sendeFlexNrStufe(NrSlot steckplatz, int stufe);
    /// Staerke der automatischen Kerbe an der Maschine (anf_level, 0..100).
    void sendeFlexAnfStufe(int stufe);
    /// Knackunterdrueckung und Auto-Notch -- die beiden, die neben
    /// dem gewaehlten Verfahren weiterlaufen duerfen.
    void setFlexSliceNoiseNbAnf(bool nb, bool anf);
    void sendeFlexTonpegel(int stufe0bis100);
    void sendeFlexStumm(bool stumm);
    void sendeFlexBalance(double unserWert);
    void sendeFlexRit(bool an, int hz);
    void sendeFlexXit(bool an, int hz);
    void sendeFlexApf(bool an);
    /// Die gemeinsame Mitte der sieben: Verbindung, Scheibennummer,
    /// Befehl -- und jedes dieser drei Tore meldet sich, wenn es
    /// schlieszt. `was` ist der Name fuer das Protokoll.
    void schickeFlexScheibenbefehl(const QString& befehl, const QString& was);

    /// Die Scheibe bestimmen, die senden soll. Ohne das weist die
    /// Maschine `xmit` ab.
    bool setzeFlexSendescheibe(const QString& sliceId);
    QString flexSendescheibe() const { return m_flexSendescheibenId; }

    /// Haengt gerade eine FlexRadio an? Nicht dasselbe wie
    /// `isConnected()`: es gibt mehrere Geraeteklassen, und an der
    /// Flex kommen Bild und Ton ueber ganz andere Wege.
    bool istFlexVerbunden() const;

    /// Sende- und Abstimmleistung in Prozent (0..100).
    bool setzeFlexSendeleistung(int prozent);
    bool setzeFlexAbstimmleistung(int prozent);

    /// ── Die Aussteuerung des Mikrofonwegs (2026-10-29) ──────────────
    ///
    /// Der Flex-Sendeweg haengt an KEINER WDSP-Kette: der Ton geht roh
    /// von `AudioEngine::pullTxMic` an die Maschine. Damit gibt es auf
    /// diesem Weg auch keine Verstaerkungsstufe — und am Geraet
    /// gemessen heiszt das beides:
    ///
    ///   18:56  MICPEAK  +2,54 dBFS  ->  FWDPWR 35,4 dBm = 3,4 W
    ///   20:15  MICPEAK  −9,72 dBFS  ->  FWDPWR 18,6 dBm = 73 mW
    ///
    /// Dasselbe Programm, dieselbe Einstellung, zwei verschiedene
    /// Mikrofone: einmal zweieinhalb Dezibel ueber Vollaussteuerung,
    /// einmal zehn darunter. Leistung folgt dem Quadrat, also liegen
    /// zwischen den beiden fast siebzehn Dezibel Ausgangsleistung.
    ///
    /// Der Regler ist von Hand, nicht automatisch: „ich glaube, wir
    /// lassen das automatische verbessern" (Betreiber, 2026-10-09).
    /// Der Riegel bei −1 dBFS in der Pumpe bleibt dahinter stehen.
    int flexMikGainDb() const { return m_flexMikGainDb; }
    void setzeFlexMikGainDb(int db);

    /// Der Abstimmtraeger. Taktet mit der Abstimmleistung.
    bool flexAbstimmen(bool an);

    // ── Vorverzerrung (APD) der Maschine ────────────────────────────
    //
    // Die Flex rechnet sie selbst; Longpath schaltet sie und waehlt
    // den Messpunkt. Warum das nicht unser PureSignal sein kann,
    // steht im Kopf von core/FlexApdState.h.
    //
    // Alle drei geben false zurueck, wenn nichts hinausging.
    const Longpath::FlexApdState& flexApd() const { return m_flexApd; }
    bool flexApdEinschalten(bool an);
    bool flexApdMesspunktSetzen(const QString& sendeantenne, const QString& messpunkt);
    bool flexApdZuruecksetzen();

    // ── Der Zweiton der Maschine (2026-11-01) ───────────────────────
    //
    // Nur der MODUS. Getastet wird nicht von hier: der Abstimmtraeger
    // geht mit `flexAbstimmen(true)` hinaus, und den drueckt der
    // Betreiber. Ein Programm, das zum Messen selbst tastet, ist genau
    // das, was hier nie passieren soll.
    bool flexZweitonModus(bool an);

    /// Laeuft gerade ein Zweiton auf dem Sender? Beides muss stimmen:
    /// der Abstimmtraeger ist an UND der Modus ist `two_tone`. Das ist
    /// das Tor fuer die IMD-Messung am Flex.
    bool flexZweitonAktiv() const { return m_flexTuneAn && m_flexTuneZweiton; }

#ifdef LONGPATH_BUILD_TESTS
    /// Eine Statuszeile der Maschine einspeisen, ohne Maschine. Geht
    /// durch DENSELBEN Weg wie der Zustandsschub
    /// (`verarbeiteFlexZustandszeile`) — ein Prueflauf oder ein
    /// Entwurfsblatt zeigt damit, was die echten Zeilen ergeben, nicht
    /// was jemand hineingeschrieben hat.
    ///
    /// Der Name sagt „Apd", weil er dafuer entstand; er traegt
    /// inzwischen auch die Sendeebene (`transmit`).
    void flexApdStatusForTest(const QString& object,
                              const QMap<QString, QString>& kvs)
    {
        verarbeiteFlexZustandszeile(object, kvs);
    }
#endif

    /// Den Sendetonstrom anlegen lassen. Die Antwort traegt die
    /// Kennung, die danach in jedes Tonpaket gehoert.
    void legeFlexSendetonstromAn();
    quint32 flexSendetonstromId() const { return m_flexSendetonstromId; }

    /// Ein Stueck Mikrofonton an die Maschine. `stereoInterleaved`
    /// sind `frames` Rahmen zu zwei Kanaelen, Fliesskomma -1..+1.
    /// Gibt die Zahl gesendeter Pakete zurueck.
    int sendeFlexMikrofonton(const float* stereoInterleaved, int frames);

    /// Die Verriegelungsmeldung der Maschine auswerten. Liest nur und
    /// tastet nichts — oeffentlich, damit ein Pruefstand das Tor vor
    /// dem Tasten pruefen kann, ohne ein Funkgeraet anzufassen (und
    /// ohne einen `#ifdef`-Wächter, der lokal gruen und in der CI rot
    /// ist; siehe memory longpath-pch-verdeckt-fehlende-deklaration).
    void verarbeiteFlexVerriegelung(const QMap<QString, QString>& kv);

private:
    /// Den selbst gesetzten Ausschnitt uebernehmen. Noetig, weil die
    /// Maschine eine von uns gesetzte Panadaptermitte NICHT
    /// zurueckechot — gemessen am 2026-10-23.
    void uebernimmFlexPanMitte(double mitteMhz);

public:

private:
    /// Was die Maschine als `y_pixels` meldet, einordnen: ein echter
    /// Wert wird zum Teiler, eine Ruecksetzvorgabe loest ein erneutes
    /// Hinschicken der Masze aus.
    void uebernimmFlexBildhoehe(int gemeldet);

public:
    /// Ist diese gemeldete Bildhoehe die Ruecksetzvorgabe der Maschine
    /// und damit KEIN Maszstab?
    ///
    /// From AetherSDR RadioModel.cpp:86 + :10994-11018 [@d58e2b8a] —
    /// Schwelle 100. Oeffentlich, weil genau diese Grenze der Grund
    /// ist, warum das Bild entweder stimmt oder zwanzig Stufen hat;
    /// eine stille „Vereinfachung" soll am Prueflauf auffallen.
    static bool flexHoeheIstVorgabe(int gemeldet)
    {
        constexpr int kVorgabeSchwelle = 100;
        return gemeldet <= kVorgabeSchwelle;
    }

private:

    // Sub-components (owned, main thread)
    RadioDiscovery*  m_discovery{nullptr};
    FlexRadioDiscovery* m_flexDiscovery{nullptr};  // Flex-Treiber Stufe 1
    FlexRadioConnection* m_flexConnection{nullptr};  // nur bei Flex belegt
    FlexVitaStream*      m_flexStream{nullptr};
    QString              m_flexPanId;        // „40000000"
    QString              m_flexAudioStreamId;
    // Was die MASCHINE eingestellt hatte, einmal mitgelesen, bevor
    // Longpath etwas umstellt — leer heisst „nie gelesen", und dann
    // wird auch nichts zurueckgestellt (HAUSSTIL Regel 7: Unbekannt
    // ist ein Strich, keine Null). Martins Geraet stand auf
    // audio_level=37 und mute_local_audio_when_remote=0; die erste
    // Fassung schrieb 50 hin und stellte beim Trennen eine 1 zurueck,
    // die dort nie gestanden hatte.
    QString              m_flexSliceId;           // „0"
    QString              m_flexAudioLevelWas;
    QString              m_flexAudioMuteWas;
    QString              m_flexMuteLocalWas;
    bool                 m_flexAudioWanted{false};
    bool                 m_flexRestoreAudioLevel{false};
    bool                 m_flexRestoreAudioMute{false};
    bool                 m_flexRestoreMuteLocal{false};
    /// Die Bildhoehe, mit der die MASCHINE zeichnet — der Teiler in
    /// `binToDbm()`. 0 heisst: sie hat noch nichts dazu gesagt, dann
    /// gilt unsere Wunschzahl. Sagt sie etwas, GILT IHRE ZAHL — auch
    /// gegen ein `display pan set`, das sie mit Code 0 quittiert und
    /// dann ignoriert hat (am 2026-10-21 genau so gemessen).
    int                  m_flexYPixelsGemeldet{0};
    /// Wie oft die Masze nach einer Ruecksetzvorgabe neu hingeschickt
    /// wurden. Gedeckelt, damit eine Maschine, die sie nie annimmt,
    /// keine endlose Schleife erzeugt.
    int                  m_flexMasseNachgereicht{0};
    /// Zuletzt an die Maschine geschickte Frequenz (Hertz), damit
    /// ihr Zustandsschub nicht denselben Befehl wieder ausloest.
    double               m_flexZuletztGestellteHz{0.0};
    /// Dasselbe fuer Betriebsart und Filterflanken.
    QString              m_flexZuletztGestellteArt;
    /// Die Kennung des Wasserfallstroms, aus `waterfall=` im
    /// Panadapter-Zustand.
    QString              m_flexWasserfallId;
    /// Mitte und Breite des Panadapters, EINZELN gemerkt: die Maschine
    /// schickt beim Drehen nur `center=`, nicht beides zusammen.
    double               m_flexPanMitteMhz{0.0};
    double               m_flexPanBreiteMhz{0.0};
    /// Zuletzt von der Maschine gemeldete Scheibenfrequenz (Hertz).
    double               m_flexGeraeteFrequenzHz{0.0};

    // Senden ueber die Flex (Stufe 2). Vorgabe: gesperrt.
    bool                 m_flexDarfSenden{false};
    /// Aussteuerung des Flex-Mikrofonwegs in dB, siehe
    /// setzeFlexMikGainDb(). 0 heiszt: roh hinaus, wie bisher.
    int                  m_flexMikGainDb{0};
    bool                 m_flexVerriegelungGehoert{false};

    /// Eine Zustandszeile der Flex auswerten — Vorverzerrung und
    /// Sendeebene. EINE Stelle fuer Verbindung und Prueflauf.
    void verarbeiteFlexZustandszeile(const QString& object,
                                     const QMap<QString, QString>& kv);

    /// Der Zustand der Flex-eigenen Vorverzerrung, wie die Maschine
    /// ihn meldet. Beim Trennen geleert — ein gemerkter Zustand ohne
    /// Maschine dahinter ist eine Behauptung.
    Longpath::FlexApdState m_flexApd;

    /// `transmit tune=` und `transmit tune_mode=`, wie die Maschine
    /// sie meldet. Zusammen ergeben sie `flexZweitonAktiv()`.
    bool m_flexTuneAn{false};
    bool m_flexTuneZweiton{false};
    /// „Sollte SWR ueber 2,5 sein, bitte nicht senden!" (Betreiber,
    /// 2026-10-30). Zwei Riegel: einer vor dem Tasten in
    /// `flexTasten()`, einer bei jeder Messung — FT8 tastet 13,2 s am
    /// Stueck, da reicht die Pruefung davor nicht.
    safety::SwrRiegel    m_flexSwrRiegel;

    // ── Die Abstimmleistung gegenlesen (2026-10-31) ──────────────────
    //
    // Betreiber: „tunen will ich grundsaetzlich mit einem watt." Am
    // Geraet gemessen ergaben 10 % aber 23 W — `tunepower` ist
    // Prozent auf einer nicht linearen Kurve. Wer Watt will, musz das
    // Ergebnis lesen. Siehe `FlexAbstimmRegler`: waehrend des
    // Traegers geht es nur hinunter.
    safety::FlexAbstimmRegler m_flexAbstimmRegler;
    bool                      m_flexAbstimmtGerade{false};
    int                       m_flexAbstimmProzent{0};
    QString                   m_flexAbstimmBand;
    bool                 m_flexVerriegelungGezeigt{false};
    bool                 m_flexLeistungGesehen{false};
    /// Welche Zustandsarten die Maschine schon geschickt hat — je Art
    /// eine Protokollzeile, damit „kommt das ueberhaupt an?" ohne
    /// Mitschnitt zu beantworten ist.
    QSet<QString>        m_flexGesehenenArten;
    bool                 m_flexSendetGerade{false};
    QString              m_flexVerriegelungsLage;
    QString              m_flexVerriegelungsGrund;
    QString              m_flexVerriegelungsQuelle;
    QString              m_flexSendescheibenId;
    quint32              m_flexSendetonstromId{0};
    quint8               m_flexSendetonZaehler{0};
    QVector<float>       m_flexSendetonRest;
    /// Der Ausschnitt, den die Wasserfallkachel zuletzt meldete
    /// (Hertz). `tuneFlexSlice` erkennt daran, wann die Scheibe aus
    /// dem Bild laeuft.
    double               m_flexAusschnittTiefHz{0.0};
    double               m_flexAusschnittHochHz{0.0};
    /// Nachzutragen, solange die Scheibennummer der Maschine fehlt.
    DSPMode              m_flexArtNachzutragen{DSPMode::USB};
    bool                 m_flexHatArtNachzutragen{false};
    int                  m_flexFlankeTiefNachzutragen{0};
    int                  m_flexFlankeHochNachzutragen{0};
    bool                 m_flexHatFlankenNachzutragen{false};
    /// Rauschsperre: beim Anmelden will sie hinaus, bevor die Maschine
    /// ihre erste Scheibenmeldung geschickt hat (gemessen 2026-10-31:
    /// 143 ms zu frueh). Dann wird sie hier gemerkt und mit der
    /// Scheibennummer nachgereicht -- wie Art und Flanken darueber.
    bool                 m_flexRauschsperreAnNachzutragen{false};
    double               m_flexRauschsperreStufeNachzutragen{0.0};
    bool                 m_flexHatRauschsperreNachzutragen{false};
    /// Rauschminderung: zuletzt geschickt und, solange die
    /// Scheibennummer fehlt, nachzutragen. Vorgabe aus: eine Maschine,
    /// die beim Verbinden etwas anderes meldet, korrigiert das über
    /// den Rückweg.
    bool                 m_flexZuletztNr{false};
    bool                 m_flexZuletztNb{false};
    bool                 m_flexZuletztAnf{false};
    bool                 m_flexHatRauschNachzutragen{false};
    bool                 m_flexRauschGeschickt{false};
    int                  m_flexZuletztGestellteFlankeTief{0};
    int                  m_flexZuletztGestellteFlankeHoch{0};
    /// Index (1…59) -> {Name, Einheit} aus den `meter`-Statuszeilen.
    /// Ohne diese Tabelle ist ein Messwertpaket eine Zahlenreihe ohne
    /// Bedeutung: das Paket traegt weder Namen noch Einheit, und der
    /// Teiler haengt an der Einheit.
    QHash<int, QPair<QString, QString>> m_flexMesswertNamen;
    /// Index -> `src` derselben Zeilen (SLC, COD, RAD, TX-, …).
    /// Ohne die Quelle ist `LEVEL` nicht eindeutig: die Maschine fuehrt
    /// mehrere Messwerte mit demselben Namen, und nur der mit `src=SLC`
    /// ist der Empfangspegel einer Scheibe.
    QHash<int, QString>  m_flexMesswertQuellen;
    qint64               m_flexMesswertZuletztGemeldet{0};
    /// Letzter Empfangspegel der Flex-Scheibe (dBm) und sein Zeitpunkt.
    /// Der S-Meter-Umlauf liest WDSP, und beim Flex rechnet das Radio —
    /// dort steht nichts. Darum kommt die Zahl von der Maschine.
    double               m_flexPegelDbm{0.0};
    void                 fuehreFlexAutoSqlNach(double dbm, qint64 jetztMs);
    void                 sendeFlexAutoStufe(qint64 jetztMs);
    void                 flexAutoSqlNeuMessen(const char* grund);
    FlexAutoRauschsperre m_flexAutoSql;
    bool                 m_flexAutoSqlAn{false};
    double               m_flexAutoSqlBezugHz{0.0};
    qint64               m_flexPegelStempel{0};
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

    // Phase 3F Sub-Epic F Task 5: per-ADC WidebandFftEngine instances.
    // Indexed by adcIndex (0 or 1). Constructed in the RadioModel ctor with
    // a default 122.88 MHz ADC sample rate. Owned via QObject parent.
    std::array<Longpath::WidebandFftEngine*, 2> m_widebandFftEngines{};

    // Band-plan overlay manager — app-global, loaded once from Qt resources.
    // Phase 3G RX Epic sub-epic D.
    BandPlanManager m_bandPlanManager;

    // Apollo PA + ATU + LPF accessory state (present/filter/tuner enable bools).
    // MAC and load() are called on connect. Phase 3P-F Task 5a.
    ApolloController m_apolloController;
    VoiceKeyerStore m_voiceKeyer;
    // VOR m_compositeMicRouter erklaert und damit NACH ihm zerstoert:
    // der Router haelt einen nackten Zeiger auf die Quelle des Spielers.
    std::unique_ptr<VoiceKeyerPlayer> m_voiceKeyerPlayer;
    std::unique_ptr<HotKeyBindings> m_hotKeyBindings;
    int m_dvkQuelleVorher{0};   // MicSource vor der Ansage
    QsoRecorderController m_qsoRecorder;
    RecordingModel        m_recording;
    // Der Pfad wartet hier, bis die Fabrik die Verbindung gebaut hat:
    // RadioConnection::create() kennt nur die RadioInfo, und die traegt
    // keinen Dateinamen.
    QString               m_playbackPath;

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

    // Phase 3F Sub-Epic I: which DDC stream hosts which slice. Pure policy;
    // sized by configureStreamPool at connect, empty (and therefore
    // bind-refusing) while disconnected.
    Longpath::SliceStreamAllocator m_streamAllocator;

    // Rate handed to configureStreamPool, used when a stream is claimed
    // before m_connectionSampleRateHz has been set (Slice A binds during
    // connectToRadio, before wireConnectionSignals records the wire rate).
    int m_streamDefaultRateHz{192000};

    // Phase 3F Sub-Epic I closeout, defect H1: the drain size last published
    // to RxDspWorker for each stream, keyed by stream index. Seeded by
    // configureStreamPool to bufferSizeForRate(defaultRateHz), which is
    // exactly what the worker's global default already is, so a pool that
    // never leaves its connect rate is recognised as already in step and
    // applyStreamDspGeometry stays a no-op. Main thread only.
    QHash<int, int> m_streamInSizePushed;

    // Phase 3F Sub-Epic I Task 7b: the codec's last per-stream DDC choice,
    // indexed by stream. -1 = that stream is idle, so an emptied stream
    // leaves no stale DDC behind. Backs ddcForStream().
    std::array<int, 5> m_streamDdc{{-1, -1, -1, -1, -1}};

    // Process-wide WDSP pdiv[0] lifecycle shadow. Main-thread-only: the worker
    // route itself is published/cleared synchronously on m_dspThread before
    // this state changes.
    static constexpr int kExternalDiversityId = 0;
    static constexpr int kExternalDiversityTargetSliceId = 0;
    bool m_externalDiversityRouteActive{false};
    int m_externalDiversityPrimaryDdc{-1};
    int m_externalDiversitySecondaryDdc{-1};
    int m_externalDiversityChunkSize{0};

    // Defect D1: the ADC that same assignment routed each stream to, decoded
    // from its adcCtrl bytes. Backs chainForStream(), which is what the Alex
    // per-chain filter decision groups by.
    //
    // Held here rather than read back out of ReceiverManager, even though
    // publishDdcAssignment mirrors it there too. ReceiverConfig only exists
    // for a receiver that has been created, and connectToRadio creates one
    // per stream, so a RadioModel driven without a connection (every unit
    // test, and the pre-connect model state) has no ReceiverConfig to answer
    // from and would silently report ADC0 for everything. That is precisely
    // the failure mode D1 was: an ADC field that always says 0 and a wire
    // that says otherwise.
    //
    // 0 rather than -1 for an idle stream: chainForStream returns -1 for
    // "not on a chain" off the stream index itself, and a stream with no DDC
    // has no ADC to name.
    std::array<int, 5> m_streamAdc{{0, 0, 0, 0, 0}};

    // Phase 3F Sub-Epic I closeout, defect F3: last-published set of streams
    // that host slices but have no DDC. Change-gates the streamsSuspended
    // emit so it fires on transitions rather than on every codec run.
    QVector<int> m_suspendedStreams;

    // Phase 3F Sub-Epic I closeout, defect F4: guards the rollback
    // setFrequency in the retune handler from re-entering the allocator.
    // Everyone else still sees the rolled-back frequencyChanged.
    bool m_rollingBackFrequency{false};

    // Phase 3F Sub-Epic J Task 6: guards the nbModeChanged mirror from
    // re-entering itself. The blanker is per-DDC, not per-slice (Thetis
    // cmaster.h:74-82 [v2.10.3.15]), so a change on one slice writes
    // setNbMode on every co-host; each of those emits its own
    // nbModeChanged, which would otherwise walk the stream again.
    bool m_mirroringNbMode{false};
    // Re-entrancy guard for the NB1 / NB2 detailed-tuning mirror, which
    // spreads one slice's blanker tuning across every co-host on the same
    // DDC. Separate from m_mirroringNbMode: the two mirrors run off different
    // signals and a shared flag would let one suppress the other. Shared
    // across the five tuning knobs is fine, because each mirror only ever
    // writes the same property it fired on, so they never nest.
    bool m_mirroringNbTuning{false};

    // Phase 3F Sub-Epic I closeout, defect F4: the allocator's own words for
    // the last rejected placement, handed to the retune handler so its
    // status-bar line can explain what the hardware ran out of.
    QString m_lastPlacementRejectReason;

    // Phase 3F Sub-Epic I closeout, defect F3: injected via
    // setDdcContextForTest. Off in production; currentCodecContext() reads
    // MoxController / PureSignal / the slice diversity flag as before.
    bool m_ddcCtxForTest{false};
    bool m_ddcCtxMoxForTest{false};
    bool m_ddcCtxPsForTest{false};
    bool m_ddcCtxDivForTest{false};

    // View hooks (non-owning, set by MainWindow). Phase 3G-8 + 3G-9c.
    class SpectrumWidget*     m_spectrumWidget{nullptr};
    class FFTEngine*          m_fftEngine{nullptr};
    class ClarityController*  m_clarityController{nullptr};
    class StepAttenuatorController* m_stepAttController{nullptr};


    // Radio-session counter for handleMicPttFromRadio. Starts at 1 so a
    // report carrying the default 0 is never mistaken for the live
    // session.
    quint64 m_radioSession{1};
    // 2026-05-22 spectrum-calibration fix: cache of the last rxMeterOffsetDb
    // value we emitted via rxMeterOffsetChanged. NaN sentinel forces the
    // first call to compare unequal so subscribers always get the initial
    // value (matches the MoxController NaN-sentinel pattern). Updated only
    // by setStepAttController's recompute lambda; rxMeterOffsetDb itself
    // stays const and recomputes on every call.
    mutable double m_lastEmittedRxMeterOffsetDb{std::numeric_limits<double>::quiet_NaN()};

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
    int m_fmDeviationHz{5000};

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

    // Remote bench 2026-08-12 (third round of the restore-to-wire fix):
    // the persisted per-band rate as the FIRST unbound restore of the
    // session read it — the only moment the settings key is guaranteed
    // un-stomped. Between that restore and the Connected transition, a
    // coalesced saveSliceState can write all slice keys from property
    // values, and after bindSliceToStream's adoption the property holds
    // the stream default again, so the key itself gets overwritten.
    // The Connected re-apply therefore uses THIS anchor instead of
    // re-reading settings. 0 = nothing pending; consumed (reset to 0)
    // by the Connected handler. Single-slice by design until Phase 3F.
    int m_pendingRestoredRateHz{0};

    // Settings save coalescing
    bool m_settingsSaveScheduled{false};
    // Phase 3P-I-a — dirty flag for AlexController persistence.
    // AlexController::antennaChanged can fire 14× during load(); the
    // flag + scheduleSettingsSave() timer coalesces them into a single
    // write at flush time. Set from the antennaChanged/blockTxChanged
    // handlers in wireSlice<Slot>, cleared by saveSliceState().
    bool m_alexControllerDirty{false};
    // Phase 3F re-entrancy guard for republishAlexAdcSlices().
    // republishAlexAdcSlices feeds AlexController::notifySlicesOnAdc, which
    // recomputes and can emit bpfStateChanged, which is wired back to
    // republishAlexAdcSlices so an operator override or a wideband toggle
    // reaches the wire on its own trigger. The re-entry lands between the
    // ADC0 and ADC1 notifications, so the inner pass would compose ADC1
    // from state the outer pass has not refreshed yet. The guard drops the
    // nested call; the outer one finishes the loop and pushes once, from
    // fully-updated state.
    bool m_republishingAlexBpf{false};

#ifdef LONGPATH_BUILD_TESTS
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

    // AGC bidirectional sync guard — prevents infinite feedback loop between
    // agcThresholdChanged and rfGainChanged handlers.
    // From Thetis console.cs:45960-46006 — bidirectional sync pattern.
    bool m_syncingAgc{false};

    // From Thetis v2.10.3.13 console.cs:46057 — tmrAutoAGC (500ms interval)
    QTimer* m_autoAgcTimer{nullptr};
    // Start: gesperrt (1), bis eine Verbindung steht. Atomar, weil
    // forkIqToTaps ihn auf dem Verbindungsfaden liest.
    std::atomic<quint64> m_autoAgcAcquisitionEpoch{1};
    void refreshAutoAgcAcquisition();
    NoiseFloorTracker* m_noiseFloorTracker{nullptr};
    QMap<int, NoiseFloorTracker*> m_streamNoiseFloors;
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
    // Stable slice identity paired with m_savedTxDspMode. Listening focus
    // may move while the asynchronous TUN-off sequence is settling.
    int m_savedTxDspSliceId{-1};
    //
    // m_savedPowerPct: power slider value (0-100) before the tune-power push.
    //   Cite: Thetis console.cs:30033 [v2.10.3.13] — PreviousPWR = ptbPWR.Value.
    //   //MW0LGE_22b  [original inline comment from console.cs:30033]
    //   Restored to the connection on TUN-off so the slider snaps back.
    // Default 100 matches TransmitModel::m_power default (TransmitModel.h).
    // G.4 fixup: changed from 50 (initial value mismatch with TransmitModel);
    // harmless after the cold-off guard in setTune(false) but kept for hygiene.
    int m_savedPowerPct{100};
    // Das Sendeband, auf dem m_savedPowerPct gemerkt wurde (2026-11-10).
    // Wechselt das Band, bevor TUNE ganz aus ist, gehoert der gemerkte Wert
    // dem alten Band -- completeTuneOff stellt ihn dann nicht zurueck.
    Band m_savedPowerBand{Band::GEN};
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

    // Stable WDSP RX identity stopped at MOX entry. Release restores this
    // exact channel even if listening focus changes before key-up.
    int m_moxStoppedRxChannel{-1};

    // Phase 3F Sub-Epic C: TX-slice arbiter (single-TX invariant + RF-safe
    // handoff). QObject child of RadioModel (Qt parent ownership). Wired
    // to &m_slices + m_moxController in the constructor body, fed MAC +
    // load() on every currentRadioChanged emit. See txSliceArbiter()
    // accessor and docs/architecture/2026-05-26-phase3f-sub-epic-c-tx-arbiter-lifecycle-plan.md
    // Task 6.
    TxSliceArbiter* m_txSliceArbiter{nullptr};

    // Phase 3F Sub-Epic D Task 13: receiver -> pan FFT fan-out router.
    // QObject child of RadioModel. Constructed in the ctor body after
    // m_txSliceArbiter; the per-receiver FFTEngine pump and pan FFT
    // subscriber wiring lands in Sub-Epic E / F polish.
    class FFTRouter* m_fftRouter{nullptr};

    // Phase 3J-1 closeout Item 3 (2026-05-12): TCI Q_INVOKABLE long-tail
    // state.  See setGlobalMute / setAfLinear / setIqSampleRate / etc. for
    // semantics.  Defaults chosen to match TestMockRadioModel initial
    // values so the production path passes the existing matrix tests.
    //
    // Per-slice stub state for DSP toggles SliceModel doesn't yet expose
    // as Q_PROPERTYs: rxCtun / rxEnable.  Sized to the max RX count
    // Longpath supports today (4 for the four-DDC SKUs); the setter clamps
    // the index so an out-of-range slice silently no-ops.
    // rxNf left this set in TNF section 6.4: it is the global notch master
    // enable and now reads and writes NotchModel::globalEnabled.
    static constexpr int kTciStubSliceMax = 4;
    bool        m_tciGlobalMute{false};
    // AF / MON volume fallback defaults match the live-source defaults
    // they back-fill (AudioEngine::m_masterVolume{0.5f} and
    // TransmitModel::m_monitorVolume{0.5f} both = 50 linear).  Live
    // sources are non-null on a constructed RadioModel, so these stubs
    // are only read in degenerate test paths -- but using 50 instead of
    // 0 means a future change that drops the live-source guard won't
    // silently emit `volume:-60.0;` (full mute) on first client connect.
    // PR #279 review P1 (2026-05-22).
    int         m_tciAfLinear{50};
    int         m_tciMonLinear{50};
    // iqSampleRate fallback: prefers live connectionSampleRateHz(); this
    // stub is only read pre-connect.  Use the canonical HPSDR P2 baseline
    // (192 kHz, matches SampleRateCatalog::kDefaultSampleRate) so first
    // client connect doesn't see iq_samplerate:0; which real TCI clients
    // reject.  PR #279 review P1 (2026-05-22).
    int         m_tciIqSampleRate{192000};
    // Audio-stream config: per-client semantics live in TciClientSession;
    // these mirror "last value any client sent" for matrix-test parity.
    int         m_tciAudioSampleRate{48000};
    int         m_tciAudioStreamChannels{2};
    int         m_tciAudioStreamSamples{2048};
    // audioStreamSampleType: Thetis TCI wire-format tokens (audio_stream
    // sample-type field) are all lower-case in golden captures
    // ("float32" / "int16" / "int24" / "int32").  Default "Float32"
    // (capital F) made the first-connect frame non-canonical.
    // PR #279 review P1 (2026-05-22).
    QString     m_tciAudioStreamSampleType{QStringLiteral("float32")};
    // Per-slice DSP toggle stubs (set-and-read only; not wired to WDSP).
    // BIN and APF used to live here too; Phase 3F chip task_c1e6fbad routed
    // them to SliceModel::binauralEnabled / apfEnabled, which are wired to
    // RxChannel, and deleted their arrays rather than leaving state nothing
    // reads. NF followed them out in TNF section 6.4, onto
    // NotchModel::globalEnabled -- see the setRxNf comment in RadioModel.cpp.
    std::array<bool, kTciStubSliceMax> m_tciStubRxCtun{};
    std::array<bool, kTciStubSliceMax> m_tciStubRxEnable{ {true, false, false, false} };

    // Non-owning view of the WDSP TX channel (WdspEngine::kTxChannelId,
    // == WDSP.id(1, 0)).
    // WdspEngine owns the channel via m_txChannels. This pointer is valid only
    // after m_wdspEngine->initializedChanged fires and createTxChannel(kTxChannelId) is
    // called inside the initializedChanged lambda. null before that.
    // Callers must guard: if (m_txChannel) { ... }.
    // Thread safety: read only from the main thread. WDSP TX processing happens
    // on the DSP thread (m_dspThread), but the run-flag mutations called here
    // (setRunning / setTuneTone) are non-realtime control-path calls that are
    // safe to call from the main thread per the WDSP API contract.
    // From Thetis dsp.cs:926-944 [v2.10.3.13] — WDSP.id(1, 0) = channel 1.
    TxChannel* m_txChannel{nullptr};
    // Off by default and owned here rather than by the worker, so the
    // panels can reach it without going through the audio thread.
    // Declared BEFORE m_txWorker: the worker holds a raw pointer to
    // it, and members die in reverse order of declaration.
    std::unique_ptr<StripChain> m_stripChain;

    /// Build and restore the strip chain if it is not there yet.
    /// Called from the constructor and again when the pump starts.
    void ensureStripChain();

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
    /// Der eigene FT8/FT4-Sender (2026-10-15). Am Router angemeldet,
    /// liefert Stille, bis ihn jemand startet. Siehe core/Ft8Tx.h.
    std::unique_ptr<Ft8TxSource> m_ft8TxSource;
    std::unique_ptr<RttyTxSource> m_rttyTxSource;
    std::unique_ptr<CompositeTxMicRouter>  m_compositeMicRouter;

    // ── 3M-1c Phase L: cross-cutting ownership ──────────────────────────────
    //
    // L.1 — MicProfileManager (chunk F).  QObject child of RadioModel so the
    // dtor cleans it up automatically.  Constructed once in the RadioModel
    // ctor; setMacAddress + load() are called per-connect inside
    // connectToRadio(); setMacAddress("") is called in teardownConnection so
    // mutators silently no-op while no radio is selected.
    MicProfileManager* m_micProfileMgr{nullptr};
    RxProfileManager*  m_rxProfileMgr{nullptr};

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
    // 2026-08-13 SWR sweep analyzer — owned QObject child.
    SwrSweepController* m_swrSweep{nullptr};
    // Latest raw coupler counts; see lastFwdAdcRaw().
    quint16 m_lastFwdRaw{0};
    quint16 m_lastRevRaw{0};
    // The coupler's zero, learned from this radio while it receives.
    // Replaces the per-model adc_cal_offset table — see CouplerZero.h.
    CouplerZero m_couplerZero;

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
    BandStackController* m_bandStack{nullptr};

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
    std::unique_ptr<RoundtableHub>        m_roundtableHub;
    // Registriert von RotorLogbookPanel, nicht besessen -- siehe rotor().
    QPointer<RotorController>             m_rotor;
    // Besessen -- siehe companionRig().
    std::unique_ptr<CompanionRigController> m_companionRig;
    // Nur im Betrieb ohne SDR, siehe core/CatRigConnection.h. Steht hinter
    // m_companionRig: die Bruecke geht beim Abbau zuerst.
    std::unique_ptr<CatRigBruecke>          m_catRigBruecke;
    // Sprachspeicher ueber den 590. Steht hinter m_companionRig: geht beim
    // Abbau zuerst und laesst dabei einen getasteten Sender noch los.
    std::unique_ptr<Ts590Sprache>           m_ts590Sprache;
    QMetaObject::Connection                 m_catSpiegel;
    QMetaObject::Connection                 m_catTraeger;
    void starteCatBetriebWennNoetig();
    void beendeCatBetrieb();
    std::unique_ptr<SpotTableModel>       m_spotTableModel;
    std::unique_ptr<FreeDVStationModel>   m_freeDvStationModel;
    std::unique_ptr<RxDecodeModel>        m_rxDecodeModel;
    std::unique_ptr<DxccColorProvider>    m_dxccColorProvider;

    // Frequency memories (Thetis MemoryList), owned here, restored in the
    // constructor from memoriesDir().
    MemoryList*  m_memories{nullptr};
    // Quick memory (Thetis txtMemoryQuick + quick_save_mode / quick_save_filter)
    double  m_quickSaveFreqHz{0.0};
    DSPMode m_quickSaveMode{DSPMode::LSB};
    int     m_quickSaveFilterLow{0};
    int     m_quickSaveFilterHigh{0};
    bool    m_quickSaveValid{false};

    std::unique_ptr<DxClusterClient>      m_dxCluster;      // DX cluster (DxSpider / AR-Cluster / CC-Cluster)
    std::unique_ptr<DxClusterClient>      m_rbn;            // Reverse Beacon Network (RBN-suffixed spotter)
    std::unique_ptr<WsjtxClient>          m_wsjtx;
    std::unique_ptr<SpotCollectorClient>  m_spotCollector;
    std::unique_ptr<PotaClient>           m_pota;
    std::unique_ptr<SotaClient>           m_sota;
    std::unique_ptr<FreeDVReporterClient> m_freeDvReporter;
    std::unique_ptr<PskReporterClient>    m_pskReporter;

    // TNF (design section 5): notch store. Persisted globally rather than
    // per-MAC (design D3) because a notch tracks a QRM source at the
    // operator's location and band, not a property of the radio.
    std::unique_ptr<NotchModel>           m_notchModel;

    // Phase 3R-bridge: drives the freedv-gui-style RADE "sync-only"
    // rx_report upload (empty callsign, "RADEV1" mode, 1 Hz) into
    // m_freeDvReporter. Ported from freedv-gui src/main.cpp:
    // 1971-1996 [@77e793a]. Path A (callsign-decoded via EOO) is
    // separately driven by RadioModel::onRadeTextDecoded.
    std::unique_ptr<FreeDVRadeReporterBridge> m_radeReporterBridge;

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

    // 2026-07-27 (ANAN-G2E lockup): discovery quiet period after any
    // teardown.  Must outlast (a) the radio's stop-transition settling
    // (observed death window: up to ~1 s after run=0 in the 2026-07-27 TZSP
    // captures) and (b) the gateware's ~2 s C&C deadman edge
    // (Hermes.v:398-414, HW_TIMEOUT at 250e6 cycles @ 125 MHz), so the first
    // probe a stopped radio hears arrives with its state machines fully
    // settled.  Thetis's post-stop behaviour is total silence; 3 s of quiet
    // approximates that without making the reopened panel feel dead.
    static constexpr std::chrono::milliseconds kPostDisconnectScanQuietMs{3000};

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

    // Phase 3P-II Task 19: PGXL / TGXL / Tuner ownership.
    // All three are QObject children of RadioModel (parent=this, constructed
    // once in the ctor). Raw pointer pattern follows m_moxController et al.
    PgxlConnection* m_pgxlConnection{nullptr};
    TgxlConnection* m_tgxlConnection{nullptr};
    TunerModel*     m_tunerModel{nullptr};

    // Phase 3P-III: RF-Kit RF2K-S connection. unique_ptr with Qt parent=this
    // so destruction order is deterministic and QObject hierarchy is intact.
    // Constructed once in the ctor; non-null from that point.
    std::unique_ptr<Rf2ksConnection> m_rfKitConnection;
    std::unique_ptr<SpeConnection>   m_speConnection;
    std::unique_ptr<Kpa500Connection> m_kpa500Connection;
    std::unique_ptr<AcomConnection>   m_acomConnection;

    // Phase 3P-III review fix I2: last-seen RF-Kit operate state, used to gate
    // externalAmpOperateChanged so the cross-vendor signal fires only on actual
    // transitions, not on every 1 Hz REST poll. Initialized false (STANDBY).
    bool m_lastRfKitInOperate{false};

    // Amplifier presence and operate-state cache (driven by onPgxlStatus).
    bool m_hasAmplifier{false};
    bool m_ampOperate{false};

    // TGXL autotune orchestration state (Longpath-native).
    // Event-driven flow (mirrors the FlexAPI interlock handshake pattern):
    //   1. startTgxlAutotune() snapshots PGXL state into m_pgxlSavedOperate
    //      and sets m_tgxlAutotuneInProgress + m_pgxlStandbyPending
    //   2. Send `operate=0` to PGXL (if it was operating)
    //   3. Wait for ampStateChanged(false) confirmation (PGXL transitioned
    //      to STANDBY), then call continueTgxlAutotuneAfterStandby()
    //   4. Engage local TUN carrier; set m_awaitingInterlockForAutotune
    //   5. Wait for interlockGranted from SmartSdrApiListener (TGXL has
    //      now received S0|interlock state=TRANSMITTING and knows PTT is
    //      live), then send `autotune` to TGXL on :9010
    //   6. On tuningChanged(false) -> drop carrier -> manualMoxChanged(false)
    //      -> restore PGXL to m_pgxlSavedOperate state
    //
    // m_tgxlAutotuneFromHardware: true if TGXL initiated (LAN PTT). Skips
    //   the `autotune` cmd because TGXL is already sweeping. We also skip
    //   the interlockGranted wait in that case.
    // m_awaitingInterlockForAutotune: true between continueTgxlAutotune-
    //   AfterStandby and the interlockGranted handler. Gates the autotune
    //   command on TRANSMITTING actually being broadcast so TGXL sees PTT
    //   before it gets the sweep command (previously a 200 ms fixed timer
    //   raced the interlock chain and caused first-press "no PTT in"
    //   aborts on cold caches).
    // 1500 ms failsafe in case PGXL never confirms standby (e.g. amp
    //   disconnected / unresponsive) -- proceed anyway and log a warning.
    // 1500 ms failsafe also on the interlockGranted wait, for the same
    //   degraded-amp case (e.g. amp disconnected mid-cycle before ACK).
    bool m_pgxlSavedOperate{false};
    bool m_tgxlAutotuneInProgress{false};
    bool m_pgxlStandbyPending{false};
    bool m_tgxlAutotuneFromHardware{false};
    bool m_awaitingInterlockForAutotune{false};
    void continueTgxlAutotuneAfterStandby();
    void sendTgxlAutotuneCmd();

    // RF-flow gate state (Longpath-native, deck item #3).
    //
    // When MoxController::txReady fires (rfDelay elapsed, radio ready to
    // TX), we normally call TxChannel::setRunning(true) which causes the
    // audio pump to start feeding samples to the radio. With an external
    // amp like PGXL in the chain, this is too early: PGXL needs to ACK
    // PTT_REQUESTED and switch its relays from bypass to amp path BEFORE
    // the carrier arrives, or it sees ~250 ms of carrier through bypass
    // into a (possibly unmatched) antenna and intermittently trips its
    // own SWR protection.
    //
    // The fix: defer setRunning(true) until interlockGranted fires
    // (TRANSMITTING was just broadcast to all amps; PGXL has ACKed or
    // the 500 ms lenient grant has fired). The lambda in the ctor reads
    // this flag and calls setRunning(true) when the grant arrives.
    //
    // 1500 ms failsafe (same budget as the autotune gate): if interlock-
    // Granted doesn't fire, start the audio anyway so the operator isn't
    // stuck with a silent TX.
    bool m_awaitingInterlockForTx{false};
    // RF-flow gate two-condition tracker (deck item #3, ordering fix
    // 2026-05-20 21:19): TxChannel::setRunning(true) needs BOTH
    //   (a) MoxController::txReady fired (radio is in TX mode), and
    //   (b) SmartSdrApiListener::interlockGranted fired (amp in
    //       TRANSMITTING state, relays switched to amp path).
    // Whichever signal fires SECOND triggers setRunning. We track each
    // independently because Qt event-queue ordering can race; we cannot
    // rely on one always firing before the other when amp ACK is fast.
    // m_txReadyReceived is set in the txReady wire, cleared on TX-off
    // (moxStateChanged(false)). m_awaitingInterlockForTx is set in the
    // txAboutToBegin wire (BEFORE PTT_REQUESTED is sent so the gate is
    // armed before any interlockGranted can fire) and cleared by the
    // grant handler.
    bool m_txReadyReceived{false};

    // Phase 3P-II Task 86: TxInterlockPolicy -- Longpath-native TX gate.
    // Qt parent-ownership (parent=this); non-null from construction time.
    TxInterlockPolicy* m_txInterlockPolicy{nullptr};

    // Phase 3P-II Phase 4 Task 89: TuneMemoryStore -- shared per-(antenna,band)
    // TGXL relay position cache. Qt parent-ownership (parent=this); non-null from
    // construction time. Shared (non-owning) with TgxlAdvancedPage and TunerApplet.
    TuneMemoryStore* m_tuneMemoryStore{nullptr};

    // Phase 3P-II Phase 4 Task 94: FaultLog ring buffers for PGXL and TGXL.
    // Qt parent-ownership (parent=this); non-null from construction time.
    // Shared (non-owning) with PgxlAdvancedPage and TgxlAdvancedPage.
    FaultLog* m_pgxlFaultLog{nullptr};
    FaultLog* m_tgxlFaultLog{nullptr};

    // Phase 3P-II Phase 4 Task 94: last known PGXL state string.
    // Tracks "previous state" so we capture only on FAULT *transitions*
    // (not on every repeated FAULT status push).
    QString m_lastPgxlState;

    // FlexRadio UDP 4992 discovery beacon. Owned by RadioModel (Qt parent=this).
    // Constructed once in the ctor; configured and started in connectToRadio()
    // once m_lastRadioInfo.macAddress is known; stopped in teardownConnection().
    // Allows PGXL/TGXL to auto-discover Longpath in their FlexRadio dropdown
    // without any manual IP entry.
    class FlexRadioDiscoveryBroadcaster* m_flexBroadcaster{nullptr};

    // Passive SmartSDR API listener on TCP 4992. Bench-recon stub: logs every
    // line PGXL sends so we can design the response layer in a follow-up.
    // Phase 3P-II follow-up: replace with a full SmartSDR API server.
    class SmartSdrApiListener* m_smartSdrListener{nullptr};
};

} // namespace Longpath
