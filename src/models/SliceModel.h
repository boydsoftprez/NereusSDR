#pragma once

// =================================================================
// src/models/SliceModel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
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

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
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

//
// Upstream source 'Project Files/Source/Console/setup.designer.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

#include "Band.h"
#include "core/NbFamily.h"
#include "core/WdspTypes.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <optional>
#include <utility>

namespace NereusSDR {

// Forward declaration — full type used only in refreshAntennasFromAlex()
// which takes a const-ref. The full header is included in SliceModel.cpp.
class AlexController;

// Stage 1 stub ladder — Stage 2 replaces with Thetis tune_step_list
// (console.cs tune_step_list has 11 entries; Stage 1 uses this 6-entry
// subset only for the X/RIT STEP cycle button).
inline constexpr int kStageOneStepLadder[] = {1, 10, 100, 500, 1000, 10000};
inline constexpr int kStageOneStepLadderSize =
    static_cast<int>(sizeof(kStageOneStepLadder) / sizeof(kStageOneStepLadder[0]));

// Represents a single receiver slice.
// In NereusSDR, slices are a client-side abstraction — the radio has
// no concept of slices. Each slice owns a WDSP channel for independent
// DSP processing.
//
// The slice is the single source of truth for VFO state (frequency,
// mode, filter, AGC, gains, antenna). Changes propagate to WDSP and
// the radio via signals wired in RadioModel.
//
// From AetherSDR SliceModel pattern: Q_PROPERTY + signals for each state.
class SliceModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(double     frequency    READ frequency    WRITE setFrequency    NOTIFY frequencyChanged)
    Q_PROPERTY(NereusSDR::DSPMode dspMode READ dspMode   WRITE setDspMode      NOTIFY dspModeChanged)
    Q_PROPERTY(int        filterLow    READ filterLow    WRITE setFilterLow    NOTIFY filterChanged)
    Q_PROPERTY(int        filterHigh   READ filterHigh   WRITE setFilterHigh   NOTIFY filterChanged)
    Q_PROPERTY(NereusSDR::AGCMode agcMode READ agcMode   WRITE setAgcMode      NOTIFY agcModeChanged)
    Q_PROPERTY(int        stepHz       READ stepHz       WRITE setStepHz       NOTIFY stepHzChanged)
    Q_PROPERTY(int        afGain       READ afGain       WRITE setAfGain       NOTIFY afGainChanged)
    Q_PROPERTY(int        rfGain       READ rfGain       WRITE setRfGain       NOTIFY rfGainChanged)
    Q_PROPERTY(QString    rxAntenna    READ rxAntenna    WRITE setRxAntenna    NOTIFY rxAntennaChanged)
    Q_PROPERTY(QString    txAntenna    READ txAntenna    WRITE setTxAntenna    NOTIFY txAntennaChanged)
    Q_PROPERTY(bool       active       READ isActive     NOTIFY activeChanged)
    Q_PROPERTY(bool       txSlice      READ isTxSlice    NOTIFY txSliceChanged)

    // ── Phase 3G-10 Stage 1 stubs (DSP state, Stage 2 wires to RxChannel) ──
    Q_PROPERTY(bool   locked          READ locked          WRITE setLocked          NOTIFY lockedChanged)
    Q_PROPERTY(bool   muted           READ muted           WRITE setMuted           NOTIFY mutedChanged)
    Q_PROPERTY(double audioPan        READ audioPan        WRITE setAudioPan        NOTIFY audioPanChanged)
    Q_PROPERTY(bool   ssqlEnabled     READ ssqlEnabled     WRITE setSsqlEnabled     NOTIFY ssqlEnabledChanged)
    Q_PROPERTY(double ssqlThresh      READ ssqlThresh      WRITE setSsqlThresh      NOTIFY ssqlThreshChanged)
    Q_PROPERTY(bool   amsqEnabled     READ amsqEnabled     WRITE setAmsqEnabled     NOTIFY amsqEnabledChanged)
    Q_PROPERTY(double amsqThresh      READ amsqThresh      WRITE setAmsqThresh      NOTIFY amsqThreshChanged)
    Q_PROPERTY(bool   fmsqEnabled     READ fmsqEnabled     WRITE setFmsqEnabled     NOTIFY fmsqEnabledChanged)
    Q_PROPERTY(double fmsqThresh      READ fmsqThresh      WRITE setFmsqThresh      NOTIFY fmsqThreshChanged)
    Q_PROPERTY(int    agcThreshold    READ agcThreshold    WRITE setAgcThreshold    NOTIFY agcThresholdChanged)
    Q_PROPERTY(int    agcHang         READ agcHang         WRITE setAgcHang         NOTIFY agcHangChanged)
    Q_PROPERTY(int    agcSlope        READ agcSlope        WRITE setAgcSlope        NOTIFY agcSlopeChanged)
    Q_PROPERTY(int    agcAttack       READ agcAttack       WRITE setAgcAttack       NOTIFY agcAttackChanged)
    Q_PROPERTY(int    agcDecay        READ agcDecay        WRITE setAgcDecay        NOTIFY agcDecayChanged)
    Q_PROPERTY(bool   autoAgcEnabled  READ autoAgcEnabled  WRITE setAutoAgcEnabled  NOTIFY autoAgcEnabledChanged)
    Q_PROPERTY(double autoAgcOffset   READ autoAgcOffset   WRITE setAutoAgcOffset   NOTIFY autoAgcOffsetChanged)
    Q_PROPERTY(int    agcFixedGain    READ agcFixedGain    WRITE setAgcFixedGain    NOTIFY agcFixedGainChanged)
    Q_PROPERTY(int    agcHangThreshold READ agcHangThreshold WRITE setAgcHangThreshold NOTIFY agcHangThresholdChanged)
    Q_PROPERTY(int    agcMaxGain      READ agcMaxGain      WRITE setAgcMaxGain      NOTIFY agcMaxGainChanged)
    Q_PROPERTY(bool   ritEnabled      READ ritEnabled      WRITE setRitEnabled      NOTIFY ritEnabledChanged)
    Q_PROPERTY(int    ritHz           READ ritHz           WRITE setRitHz           NOTIFY ritHzChanged)
    Q_PROPERTY(bool   xitEnabled      READ xitEnabled      WRITE setXitEnabled      NOTIFY xitEnabledChanged)
    Q_PROPERTY(int    xitHz           READ xitHz           WRITE setXitHz           NOTIFY xitHzChanged)
    // From phase3g-rx-experience-epic-design.md §sub-epic B — tri-state cycle.
    Q_PROPERTY(NereusSDR::NbMode nbMode READ nbMode WRITE setNbMode NOTIFY nbModeChanged)
    // --- Noise reduction (Sub-epic C-1) ---
    // Single source of truth for active NR slot. Mutual exclusion enforced
    // in setActiveNr. See Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13].
    Q_PROPERTY(NereusSDR::NrSlot activeNr READ activeNr WRITE setActiveNr NOTIFY activeNrChanged)

    // NR1 (ANR) tuning — 5 knobs.
    // Gain/Leakage stored in WDSP-domain values (not UI-units — UI applies
    // 1e-6/1e-3 scaling before calling). See RxChannel::Nr1Tuning notes.
    Q_PROPERTY(int    nr1Taps     READ nr1Taps     WRITE setNr1Taps     NOTIFY nr1TapsChanged)
    Q_PROPERTY(int    nr1Delay    READ nr1Delay    WRITE setNr1Delay    NOTIFY nr1DelayChanged)
    Q_PROPERTY(double nr1Gain     READ nr1Gain     WRITE setNr1Gain     NOTIFY nr1GainChanged)
    Q_PROPERTY(double nr1Leakage  READ nr1Leakage  WRITE setNr1Leakage  NOTIFY nr1LeakageChanged)
    Q_PROPERTY(NereusSDR::NrPosition nr1Position READ nr1Position WRITE setNr1Position NOTIFY nr1PositionChanged)

    // NR2 (EMNR) tuning — gain-method + npe-method + AE + Position +
    // trainT1/T2 + post2 cascade.
    Q_PROPERTY(NereusSDR::EmnrGainMethod nr2GainMethod READ nr2GainMethod WRITE setNr2GainMethod NOTIFY nr2GainMethodChanged)
    Q_PROPERTY(NereusSDR::EmnrNpeMethod  nr2NpeMethod  READ nr2NpeMethod  WRITE setNr2NpeMethod  NOTIFY nr2NpeMethodChanged)
    Q_PROPERTY(double nr2TrainT1     READ nr2TrainT1     WRITE setNr2TrainT1     NOTIFY nr2TrainT1Changed)
    Q_PROPERTY(double nr2TrainT2     READ nr2TrainT2     WRITE setNr2TrainT2     NOTIFY nr2TrainT2Changed)
    Q_PROPERTY(bool   nr2AeFilter    READ nr2AeFilter    WRITE setNr2AeFilter    NOTIFY nr2AeFilterChanged)
    Q_PROPERTY(NereusSDR::NrPosition nr2Position READ nr2Position WRITE setNr2Position NOTIFY nr2PositionChanged)
    Q_PROPERTY(bool   nr2Post2Run    READ nr2Post2Run    WRITE setNr2Post2Run    NOTIFY nr2Post2RunChanged)
    Q_PROPERTY(double nr2Post2Level  READ nr2Post2Level  WRITE setNr2Post2Level  NOTIFY nr2Post2LevelChanged)
    Q_PROPERTY(double nr2Post2Factor READ nr2Post2Factor WRITE setNr2Post2Factor NOTIFY nr2Post2FactorChanged)
    Q_PROPERTY(double nr2Post2Rate   READ nr2Post2Rate   WRITE setNr2Post2Rate   NOTIFY nr2Post2RateChanged)
    Q_PROPERTY(int    nr2Post2Taper  READ nr2Post2Taper  WRITE setNr2Post2Taper  NOTIFY nr2Post2TaperChanged)

    // NR3 (RNNR) — Position + UseDefaultGain (model path is GLOBAL, not per-slice).
    Q_PROPERTY(NereusSDR::NrPosition nr3Position       READ nr3Position       WRITE setNr3Position       NOTIFY nr3PositionChanged)
    Q_PROPERTY(bool nr3UseDefaultGain READ nr3UseDefaultGain WRITE setNr3UseDefaultGain NOTIFY nr3UseDefaultGainChanged)

    // NR4 (SBNR) — 5 spinboxes + algo radio.
    Q_PROPERTY(double nr4Reduction  READ nr4Reduction  WRITE setNr4Reduction  NOTIFY nr4ReductionChanged)
    Q_PROPERTY(double nr4Smoothing  READ nr4Smoothing  WRITE setNr4Smoothing  NOTIFY nr4SmoothingChanged)
    Q_PROPERTY(double nr4Whitening  READ nr4Whitening  WRITE setNr4Whitening  NOTIFY nr4WhiteningChanged)
    Q_PROPERTY(double nr4Rescale    READ nr4Rescale    WRITE setNr4Rescale    NOTIFY nr4RescaleChanged)
    Q_PROPERTY(double nr4PostThresh READ nr4PostThresh WRITE setNr4PostThresh NOTIFY nr4PostThreshChanged)
    Q_PROPERTY(NereusSDR::SbnrAlgo nr4Algo READ nr4Algo WRITE setNr4Algo NOTIFY nr4AlgoChanged)

    // DFNR — AttenLimit + PostFilterBeta.
    Q_PROPERTY(double dfnrAttenLimit     READ dfnrAttenLimit     WRITE setDfnrAttenLimit     NOTIFY dfnrAttenLimitChanged)
    Q_PROPERTY(double dfnrPostFilterBeta READ dfnrPostFilterBeta WRITE setDfnrPostFilterBeta NOTIFY dfnrPostFilterBetaChanged)

    // BNR — Strength (single knob; filter class deferred to follow-up PR).
    Q_PROPERTY(double bnrStrength READ bnrStrength WRITE setBnrStrength NOTIFY bnrStrengthChanged)

    // MNR — Strength, Oversub (aggressiveness), Floor (min Wiener gain),
    //        Alpha (decision-directed smoothing), Bias (noise-floor bias),
    //        Gsmooth (temporal gain smoothing).
    Q_PROPERTY(double mnrStrength  READ mnrStrength  WRITE setMnrStrength  NOTIFY mnrStrengthChanged)
    Q_PROPERTY(double mnrOversub   READ mnrOversub   WRITE setMnrOversub   NOTIFY mnrOversubChanged)
    Q_PROPERTY(double mnrFloor     READ mnrFloor     WRITE setMnrFloor     NOTIFY mnrFloorChanged)
    Q_PROPERTY(double mnrAlpha     READ mnrAlpha     WRITE setMnrAlpha     NOTIFY mnrAlphaChanged)
    Q_PROPERTY(double mnrBias      READ mnrBias      WRITE setMnrBias      NOTIFY mnrBiasChanged)
    Q_PROPERTY(double mnrGsmooth   READ mnrGsmooth   WRITE setMnrGsmooth   NOTIFY mnrGsmoothChanged)
    Q_PROPERTY(bool   snbEnabled      READ snbEnabled      WRITE setSnbEnabled      NOTIFY snbEnabledChanged)
    Q_PROPERTY(bool   apfEnabled      READ apfEnabled      WRITE setApfEnabled      NOTIFY apfEnabledChanged)
    Q_PROPERTY(int    apfTuneHz       READ apfTuneHz       WRITE setApfTuneHz       NOTIFY apfTuneHzChanged)
    Q_PROPERTY(bool   binauralEnabled READ binauralEnabled WRITE setBinauralEnabled NOTIFY binauralEnabledChanged)
    Q_PROPERTY(int    fmCtcssMode     READ fmCtcssMode     WRITE setFmCtcssMode     NOTIFY fmCtcssModeChanged)
    Q_PROPERTY(double fmCtcssValueHz  READ fmCtcssValueHz  WRITE setFmCtcssValueHz  NOTIFY fmCtcssValueHzChanged)
    Q_PROPERTY(int    fmOffsetHz      READ fmOffsetHz      WRITE setFmOffsetHz      NOTIFY fmOffsetHzChanged)
    Q_PROPERTY(NereusSDR::FmTxMode fmTxMode READ fmTxMode WRITE setFmTxMode NOTIFY fmTxModeChanged)
    Q_PROPERTY(bool   fmReverse       READ fmReverse       WRITE setFmReverse       NOTIFY fmReverseChanged)
    Q_PROPERTY(int    diglOffsetHz    READ diglOffsetHz    WRITE setDiglOffsetHz    NOTIFY diglOffsetHzChanged)
    Q_PROPERTY(int    diguOffsetHz    READ diguOffsetHz    WRITE setDiguOffsetHz    NOTIFY diguOffsetHzChanged)
    Q_PROPERTY(int    rttyMarkHz      READ rttyMarkHz      WRITE setRttyMarkHz      NOTIFY rttyMarkHzChanged)
    Q_PROPERTY(int    rttyShiftHz     READ rttyShiftHz     WRITE setRttyShiftHz     NOTIFY rttyShiftHzChanged)

public:
    explicit SliceModel(QObject* parent = nullptr);
    // Convenience constructor that initialises m_sliceIndex directly.
    explicit SliceModel(int sliceId, QObject* parent = nullptr);
    ~SliceModel() override;

    // ---- Frequency ----

    double frequency() const { return m_frequency; }
    void setFrequency(double freq);

    // ---- Demodulation mode ----

    DSPMode dspMode() const { return m_dspMode; }
    // Sets mode AND applies the default filter for that mode.
    // Emits dspModeChanged + filterChanged.
    void setDspMode(DSPMode mode);

    // ---- Bandpass filter ----

    int filterLow() const { return m_filterLow; }
    void setFilterLow(int low);

    int filterHigh() const { return m_filterHigh; }
    void setFilterHigh(int high);

    // Set both filter edges atomically. Emits filterChanged once.
    void setFilter(int low, int high);

    // ---- AGC ----

    AGCMode agcMode() const { return m_agcMode; }
    void setAgcMode(AGCMode mode);

    // ---- Tuning step ----

    int stepHz() const { return m_stepHz; }
    void setStepHz(int hz);

    // ---- Gains ----

    int afGain() const { return m_afGain; }
    void setAfGain(int gain);

    int rfGain() const { return m_rfGain; }
    void setRfGain(int gain);

    // ---- Antenna selection ----
    // Per-slice RX/TX antenna (e.g., "ANT1", "ANT2", "ANT3").
    // Maps to Alex antenna register bits on OpenHPSDR radios.
    // From AetherSDR SliceModel::rxAntenna/txAntenna pattern.

    QString rxAntenna() const { return m_rxAntenna; }
    void setRxAntenna(const QString& ant);

    QString txAntenna() const { return m_txAntenna; }
    void setTxAntenna(const QString& ant);

    // ---- Slice identity ----

    bool isActive() const { return m_active; }
    void setActive(bool active);

    /// True when this slice is the currently-active (foreground) receiver.
    ///
    /// Used by AudioEngine::rxBlockReady (E.4) to gate the per-slice RX-audio
    /// push during MOX: the active TX slice is silenced; non-active slices
    /// keep playing — matching Thetis IVAC mox state-machine in
    /// audio.cs:349-384 [v2.10.3.13].
    ///
    /// This is a thin alias for isActive() to make the audio-gate callsite
    /// self-documenting. The underlying flag is m_active, written by
    /// RadioModel::setActiveSlice() on the main thread and read by the audio
    /// thread in AudioEngine::rxBlockReady. The read is not atomic; the
    /// gate is protected by the cross-thread m_moxActive atomic in AudioEngine
    /// (acquire load) which acts as a barrier before the isActiveSlice() read.
    /// A stale read here is harmless: worst case the active-slice RX leaks for
    /// one block (~10 ms) before the next atomic-acquire sync catches up.
    ///
    /// Plan: 3M-1b E.4.
    bool isActiveSlice() const { return m_active; }

    bool isTxSlice() const { return m_txSlice; }
    void setTxSlice(bool tx);

    int sliceIndex() const { return m_sliceIndex; }
    void setSliceIndex(int idx) { m_sliceIndex = idx; }

    // Panadapter assignment (-1 = unassigned)
    int panId() const { return m_panId; }
    void setPanId(int id) { m_panId = id; }

    // Which receiver/DDC this slice feeds (-1 = unassigned)
    int receiverIndex() const { return m_receiverIndex; }
    void setReceiverIndex(int idx) { m_receiverIndex = idx; }

    int wdspChannelId() const { return m_wdspChannelId; }
    void setWdspChannelId(int id) { m_wdspChannelId = id; }

    // ── Phase 3G-10 Stage 1 stubs (DSP state, Stage 2 wires to RxChannel) ──

    bool   locked()          const { return m_locked; }
    void   setLocked(bool v);

    bool   muted()           const { return m_muted; }
    void   setMuted(bool v);

    double audioPan()        const { return m_audioPan; }
    void   setAudioPan(double pan);

    bool   ssqlEnabled()     const { return m_ssqlEnabled; }
    void   setSsqlEnabled(bool v);

    double ssqlThresh()      const { return m_ssqlThresh; }
    void   setSsqlThresh(double dB);

    bool   amsqEnabled()     const { return m_amsqEnabled; }
    void   setAmsqEnabled(bool v);

    double amsqThresh()      const { return m_amsqThresh; }
    void   setAmsqThresh(double dB);

    bool   fmsqEnabled()     const { return m_fmsqEnabled; }
    void   setFmsqEnabled(bool v);

    double fmsqThresh()      const { return m_fmsqThresh; }
    void   setFmsqThresh(double dB);

    int    agcThreshold()    const { return m_agcThreshold; }
    void   setAgcThreshold(int dBu);

    int    agcHang()         const { return m_agcHang; }
    void   setAgcHang(int ms);

    int    agcSlope()        const { return m_agcSlope; }
    void   setAgcSlope(int dB);

    int    agcAttack()       const { return m_agcAttack; }
    void   setAgcAttack(int ms);

    int    agcDecay()        const { return m_agcDecay; }
    void   setAgcDecay(int ms);

    // From Thetis v2.10.3.13 console.cs:45926 — m_bAutoAGCRX1
    bool   autoAgcEnabled()  const { return m_autoAgcEnabled; }
    void   setAutoAgcEnabled(bool on);

    // From Thetis v2.10.3.13 console.cs:45913 — m_dAutoAGCOffsetRX1
    double autoAgcOffset()   const { return m_autoAgcOffset; }
    void   setAutoAgcOffset(double dB);

    // From Thetis v2.10.3.13 setup.designer.cs:39320 — udDSPAGCFixedGaindB
    int    agcFixedGain()    const { return m_agcFixedGain; }
    void   setAgcFixedGain(int dB);

    // From Thetis v2.10.3.13 setup.designer.cs:39418 — tbDSPAGCHangThreshold
    int    agcHangThreshold() const { return m_agcHangThreshold; }
    void   setAgcHangThreshold(int val);

    // From Thetis v2.10.3.13 setup.designer.cs:39245 — udDSPAGCMaxGaindB
    int    agcMaxGain()      const { return m_agcMaxGain; }
    void   setAgcMaxGain(int dB);

    bool   ritEnabled()      const { return m_ritEnabled; }
    void   setRitEnabled(bool v);

    int    ritHz()           const { return m_ritHz; }
    void   setRitHz(int hz);

    bool   xitEnabled()      const { return m_xitEnabled; }
    void   setXitEnabled(bool v);

    int    xitHz()           const { return m_xitHz; }
    void   setXitHz(int hz);

    NereusSDR::NbMode nbMode() const { return m_nbMode; }
    void   setNbMode(NereusSDR::NbMode v);

    // Per-slice NB TUNING (threshold / tau / lag / lead) removed 2026-04-22
    // for strict Thetis parity. Thetis has a single global NB tuning set
    // per DSPRX via Setup → DSP → NB; no per-band overrides. All NB tuning
    // now lives globally inside NbFamily (seeded from AppSettings at
    // channel create + live-pushed from the Setup page).

    // --- NR accessors (Sub-epic C-1) ---
    NereusSDR::NrSlot activeNr() const { return m_activeNr; }
    void              setActiveNr(NereusSDR::NrSlot slot);

    // NR1
    int    nr1Taps()    const { return m_nr1Taps; }
    void   setNr1Taps(int v);
    int    nr1Delay()   const { return m_nr1Delay; }
    void   setNr1Delay(int v);
    double nr1Gain()    const { return m_nr1Gain; }
    void   setNr1Gain(double v);
    double nr1Leakage() const { return m_nr1Leakage; }
    void   setNr1Leakage(double v);
    NereusSDR::NrPosition nr1Position() const { return m_nr1Position; }
    void                  setNr1Position(NereusSDR::NrPosition p);

    // NR2
    NereusSDR::EmnrGainMethod nr2GainMethod() const { return m_nr2GainMethod; }
    void                      setNr2GainMethod(NereusSDR::EmnrGainMethod v);
    NereusSDR::EmnrNpeMethod  nr2NpeMethod()  const { return m_nr2NpeMethod; }
    void                      setNr2NpeMethod(NereusSDR::EmnrNpeMethod v);
    double nr2TrainT1()     const { return m_nr2TrainT1; }
    void   setNr2TrainT1(double v);
    double nr2TrainT2()     const { return m_nr2TrainT2; }
    void   setNr2TrainT2(double v);
    bool   nr2AeFilter()    const { return m_nr2AeFilter; }
    void   setNr2AeFilter(bool v);
    NereusSDR::NrPosition nr2Position() const { return m_nr2Position; }
    void                  setNr2Position(NereusSDR::NrPosition p);
    bool   nr2Post2Run()    const { return m_nr2Post2Run; }
    void   setNr2Post2Run(bool v);
    double nr2Post2Level()  const { return m_nr2Post2Level; }
    void   setNr2Post2Level(double v);
    double nr2Post2Factor() const { return m_nr2Post2Factor; }
    void   setNr2Post2Factor(double v);
    double nr2Post2Rate()   const { return m_nr2Post2Rate; }
    void   setNr2Post2Rate(double v);
    int    nr2Post2Taper()  const { return m_nr2Post2Taper; }
    void   setNr2Post2Taper(int v);

    // NR3
    NereusSDR::NrPosition nr3Position() const { return m_nr3Position; }
    void                  setNr3Position(NereusSDR::NrPosition p);
    bool   nr3UseDefaultGain() const { return m_nr3UseDefaultGain; }
    void   setNr3UseDefaultGain(bool v);

    // NR4
    double nr4Reduction()  const { return m_nr4Reduction; }
    void   setNr4Reduction(double v);
    double nr4Smoothing()  const { return m_nr4Smoothing; }
    void   setNr4Smoothing(double v);
    double nr4Whitening()  const { return m_nr4Whitening; }
    void   setNr4Whitening(double v);
    double nr4Rescale()    const { return m_nr4Rescale; }
    void   setNr4Rescale(double v);
    double nr4PostThresh() const { return m_nr4PostThresh; }
    void   setNr4PostThresh(double v);
    NereusSDR::SbnrAlgo nr4Algo() const { return m_nr4Algo; }
    void                setNr4Algo(NereusSDR::SbnrAlgo v);

    // DFNR
    double dfnrAttenLimit()     const { return m_dfnrAttenLimit; }
    void   setDfnrAttenLimit(double v);
    double dfnrPostFilterBeta() const { return m_dfnrPostFilterBeta; }
    void   setDfnrPostFilterBeta(double v);

    // BNR + MNR
    double bnrStrength() const { return m_bnrStrength; }
    void   setBnrStrength(double v);
    double mnrStrength() const { return m_mnrStrength; }
    void   setMnrStrength(double v);
    double mnrOversub()  const { return m_mnrOversub; }
    void   setMnrOversub(double v);
    double mnrFloor()    const { return m_mnrFloor; }
    void   setMnrFloor(double v);
    double mnrAlpha()    const { return m_mnrAlpha; }
    void   setMnrAlpha(double v);
    double mnrBias()     const { return m_mnrBias; }
    void   setMnrBias(double v);
    double mnrGsmooth()  const { return m_mnrGsmooth; }
    void   setMnrGsmooth(double v);

    bool   snbEnabled()      const { return m_snbEnabled; }
    void   setSnbEnabled(bool v);

    bool   apfEnabled()      const { return m_apfEnabled; }
    void   setApfEnabled(bool v);

    int    apfTuneHz()       const { return m_apfTuneHz; }
    void   setApfTuneHz(int hz);

    bool   binauralEnabled() const { return m_binauralEnabled; }
    void   setBinauralEnabled(bool v);

    int    fmCtcssMode()     const { return m_fmCtcssMode; }
    void   setFmCtcssMode(int mode);

    double fmCtcssValueHz()  const { return m_fmCtcssValueHz; }
    void   setFmCtcssValueHz(double hz);

    int    fmOffsetHz()      const { return m_fmOffsetHz; }
    void   setFmOffsetHz(int hz);

    FmTxMode fmTxMode()        const { return m_fmTxMode; }
    void   setFmTxMode(FmTxMode mode);

    bool   fmReverse()       const { return m_fmReverse; }
    void   setFmReverse(bool v);

    int    diglOffsetHz()    const { return m_diglOffsetHz; }
    void   setDiglOffsetHz(int hz);

    int    diguOffsetHz()    const { return m_diguOffsetHz; }
    void   setDiguOffsetHz(int hz);

    int    rttyMarkHz()      const { return m_rttyMarkHz; }
    void   setRttyMarkHz(int hz);

    int    rttyShiftHz()     const { return m_rttyShiftHz; }
    void   setRttyShiftHz(int hz);

    // ---- RIT helper ----
    // Effective receive frequency = base frequency + RIT offset (when enabled).
    // This is the frequency fed to the WDSP shift path for demodulation.
    // RIT is a demodulation offset, NOT a VFO/hardware frequency change.
    // From Thetis console.cs — RIT applies a receive-side offset without
    // retuning the hardware VFO.
    double effectiveRxFrequency() const {
        return m_frequency + (m_ritEnabled ? static_cast<double>(m_ritHz) : 0.0);
    }

    // ---- Per-mode filter defaults ----
    // Returns the F5 (default) filter low/high for a given mode.
    // Ported from Thetis console.cs:5180-5575 InitFilterPresets.
    static std::pair<int, int> defaultFilterForMode(DSPMode mode);

    // Returns human-readable mode name (e.g., DSPMode::LSB → "LSB")
    static QString modeName(DSPMode mode);

    // Returns DSPMode from name string (e.g., "LSB" → DSPMode::LSB)
    static DSPMode modeFromName(const QString& name);

    // ── Phase 3G-10 Stage 2: per-slice-per-band persistence ──────────────────
    //
    // Key namespace (see AppSettings.h §Per-slice-per-band DSP state):
    //   Per-band DSP: Slice<N>/Band<key>/AgcThreshold  etc.
    //   Session state: Slice<N>/Locked  etc.
    //
    // saveToSettings(band):
    //   Writes per-band DSP keys and session-state keys to AppSettings.
    //   Does NOT call AppSettings::save() — caller is responsible for flushing.
    //
    // restoreFromSettings(band):
    //   Reads per-band DSP keys and session-state keys from AppSettings.
    //   Missing keys fall back to current SliceModel defaults (no change).
    //
    // migrateLegacyKeys():
    //   One-shot migration. Checks for the legacy "VfoFrequency" flat key.
    //   If found, migrates to Slice0/Band<current>/... using the persisted
    //   frequency to derive the band. Then removes all legacy Vfo* keys.
    //   Call once on startup, before restoreFromSettings().
    //
    // Returns true iff the per-band AppSettings namespace for this slice
    // has any persisted state for `band` (probes the DspMode key).
    // Used by RadioModel::onBandButtonClicked (#118) to decide whether a
    // band click should seed defaults or restore last-used state.
    bool hasSettingsFor(Band band) const;

    void saveToSettings(NereusSDR::Band band);
    void restoreFromSettings(NereusSDR::Band band);
    static void migrateLegacyKeys();

    // Reads the persisted "last band" marker for this slice index from
    // AppSettings (Slice<N>/LastBand). saveToSettings(band) writes this
    // every time it runs, so on next startup the caller can restore the
    // user's actual last-used band rather than falling back to the
    // panadapter's default. Returns std::nullopt when the key is absent
    // (fresh install, or pre-LastBand settings file). Static so RadioModel
    // can read it before any SliceModel exists.
    static std::optional<NereusSDR::Band> loadLastBandFromSettings(int sliceIndex);

    // Load band-agnostic slice settings (e.g. VAX channel) from AppSettings.
    // Called on startup when no specific band context is needed. Does NOT
    // restore per-band DSP state — use restoreFromSettings(band) for that.
    void loadFromSettings();

    // ── Phase 3O VAX routing ──────────────────────────────────────────────────
    // VAX routing (Phase 3O) — 0=Off, 1..4=VAX channel.
    int vaxChannel() const { return m_vaxChannel.load(std::memory_order_acquire); }
    void setVaxChannel(int ch);

public slots:
    // Phase 3P-I-a T13 — refresh cached antenna values from AlexController
    // for the given band. Called by RadioModel on
    // AlexController::antennaChanged for the current band so VFO Flag /
    // RxApplet button labels stay in sync when the user edits the per-band
    // grid in Setup. Relies on AlexController::setRxAnt/setTxAnt being
    // idempotent (no signal on equal value) to avoid a feedback loop
    // with the T12 slice→AlexController write path.
    void refreshAntennasFromAlex(const NereusSDR::AlexController& alex,
                                 NereusSDR::Band band);

signals:
    void frequencyChanged(double freq);
    void dspModeChanged(NereusSDR::DSPMode mode);
    void filterChanged(int low, int high);
    void agcModeChanged(NereusSDR::AGCMode mode);
    void stepHzChanged(int hz);
    void afGainChanged(int gain);
    void rfGainChanged(int gain);
    void rxAntennaChanged(const QString& ant);
    void txAntennaChanged(const QString& ant);
    void activeChanged(bool active);
    void txSliceChanged(bool tx);

    // ── Phase 3G-10 Stage 1 stubs ──
    void lockedChanged(bool v);
    void mutedChanged(bool v);
    void audioPanChanged(double pan);
    void ssqlEnabledChanged(bool v);
    void ssqlThreshChanged(double dB);
    void amsqEnabledChanged(bool v);
    void amsqThreshChanged(double dB);
    void fmsqEnabledChanged(bool v);
    void fmsqThreshChanged(double dB);
    void agcThresholdChanged(int dBu);
    void agcHangChanged(int ms);
    void agcSlopeChanged(int dB);
    void agcAttackChanged(int ms);
    void agcDecayChanged(int ms);
    void autoAgcEnabledChanged(bool on);
    void autoAgcOffsetChanged(double dB);
    void agcFixedGainChanged(int dB);
    void agcHangThresholdChanged(int val);
    void agcMaxGainChanged(int dB);
    void ritEnabledChanged(bool v);
    void ritHzChanged(int hz);
    void xitEnabledChanged(bool v);
    void xitHzChanged(int hz);
    void nbModeChanged(NereusSDR::NbMode v);
    // NR signals (Sub-epic C-1)
    void activeNrChanged(NereusSDR::NrSlot slot);
    void nr1TapsChanged(int v);
    void nr1DelayChanged(int v);
    void nr1GainChanged(double v);
    void nr1LeakageChanged(double v);
    void nr1PositionChanged(NereusSDR::NrPosition v);
    void nr2GainMethodChanged(NereusSDR::EmnrGainMethod v);
    void nr2NpeMethodChanged(NereusSDR::EmnrNpeMethod v);
    void nr2TrainT1Changed(double v);
    void nr2TrainT2Changed(double v);
    void nr2AeFilterChanged(bool v);
    void nr2PositionChanged(NereusSDR::NrPosition v);
    void nr2Post2RunChanged(bool v);
    void nr2Post2LevelChanged(double v);
    void nr2Post2FactorChanged(double v);
    void nr2Post2RateChanged(double v);
    void nr2Post2TaperChanged(int v);
    void nr3PositionChanged(NereusSDR::NrPosition v);
    void nr3UseDefaultGainChanged(bool v);
    void nr4ReductionChanged(double v);
    void nr4SmoothingChanged(double v);
    void nr4WhiteningChanged(double v);
    void nr4RescaleChanged(double v);
    void nr4PostThreshChanged(double v);
    void nr4AlgoChanged(NereusSDR::SbnrAlgo v);
    void dfnrAttenLimitChanged(double v);
    void dfnrPostFilterBetaChanged(double v);
    void bnrStrengthChanged(double v);
    void mnrStrengthChanged(double v);
    void mnrOversubChanged(double v);
    void mnrFloorChanged(double v);
    void mnrAlphaChanged(double v);
    void mnrBiasChanged(double v);
    void mnrGsmoothChanged(double v);
    void snbEnabledChanged(bool v);
    void apfEnabledChanged(bool v);
    void apfTuneHzChanged(int hz);
    void binauralEnabledChanged(bool v);
    void fmCtcssModeChanged(int mode);
    void fmCtcssValueHzChanged(double hz);
    void fmOffsetHzChanged(int hz);
    void fmTxModeChanged(NereusSDR::FmTxMode mode);
    void fmReverseChanged(bool v);
    void diglOffsetHzChanged(int hz);
    void diguOffsetHzChanged(int hz);
    void rttyMarkHzChanged(int hz);
    void rttyShiftHzChanged(int hz);

    // ── Phase 3O VAX routing ──────────────────────────────────────────────────
    void vaxChannelChanged(int ch);

private:
    double  m_frequency{14225000.0};     // Default: 14.225 MHz (20m USB)
    DSPMode m_dspMode{DSPMode::USB};
    int     m_filterLow{100};            // USB default from Thetis F5
    int     m_filterHigh{3000};
    AGCMode m_agcMode{AGCMode::Med};
    int     m_stepHz{100};               // From Thetis tune_step_list[5] = 100 Hz
    int     m_afGain{50};                // 0-100, maps to 0.0-1.0 volume
    int     m_rfGain{80};                // 0-100, maps to AGC gain
    QString m_rxAntenna{QStringLiteral("ANT1")};
    QString m_txAntenna{QStringLiteral("ANT1")};
    bool    m_active{false};
    bool    m_txSlice{false};
    int     m_sliceIndex{0};
    int     m_panId{-1};
    int     m_receiverIndex{-1};
    int     m_wdspChannelId{-1};

    // ── Phase 3G-10 Stage 1 stubs (DSP state, Stage 2 wires to RxChannel) ──
    bool   m_locked{false};           // Neutral default — no Thetis citation needed
    bool   m_muted{false};            // Neutral default — no Thetis citation needed
    double m_audioPan{0.0};           // Neutral default — center pan (−1..+1), no Thetis citation needed
    bool   m_ssqlEnabled{false};      // Neutral default — feature off at start
    double m_ssqlThresh{16.0};        // Slider units 0–100; From Thetis radio.cs:1187 _fSSqlThreshold = 0.16f → 16 in slider scale
    bool   m_amsqEnabled{false};      // Neutral default — feature off at start
    double m_amsqThresh{-150.0};      // From Thetis radio.cs:1164-1165 — rx_squelch_threshold = -150.0f (AM reuses same field)
    bool   m_fmsqEnabled{false};      // Neutral default — feature off at start
    double m_fmsqThresh{-150.0};      // dB domain (plan default); Thetis radio.cs:1274-1275 stores fm_squelch_threshold = 1.0f on a different (linear 0..1) scale. Scale reconciliation deferred to Stage 2 gate.
    int    m_agcThreshold{-20};       // Thetis default — citation pending Stage 2 gate (no explicit default found in radio.cs AGCThreshold)
    int    m_agcHang{250};            // From Thetis radio.cs:1056-1057 — rx_agc_hang = 250 ms
    int    m_agcSlope{0};             // From Thetis radio.cs:1107-1108 — rx_agc_slope = 0 dB
    int    m_agcAttack{2};            // Thetis default — citation pending Stage 2 gate (AGC attack not exposed as explicit property in radio.cs)
    int    m_agcDecay{250};           // From Thetis radio.cs:1037-1038 — rx_agc_decay = 250 ms
    bool   m_autoAgcEnabled{false};   // From Thetis v2.10.3.13 console.cs:45926 — m_bAutoAGCRX1
    double m_autoAgcOffset{20.0};     // From Thetis v2.10.3.13 setup.designer.cs:38630 — udRX1AutoAGCOffset
    int    m_agcFixedGain{20};        // From Thetis v2.10.3.13 setup.designer.cs:39320 — udDSPAGCFixedGaindB
    int    m_agcHangThreshold{0};     // From Thetis v2.10.3.13 setup.designer.cs:39418 — tbDSPAGCHangThreshold
    int    m_agcMaxGain{90};          // From Thetis v2.10.3.13 setup.designer.cs:39245 — udDSPAGCMaxGaindB
    bool   m_ritEnabled{false};       // Neutral default — no Thetis citation needed
    int    m_ritHz{0};                // Neutral default — zero offset
    bool   m_xitEnabled{false};       // Neutral default — no Thetis citation needed
    int    m_xitHz{0};                // Neutral default — zero offset
    NereusSDR::NbMode   m_nbMode{NereusSDR::NbMode::Off};  // Tri-state: Off/NB/NB2

    // --- NR state (Sub-epic C-1) ---
    // See Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13].
    NereusSDR::NrSlot m_activeNr{NereusSDR::NrSlot::Off};

    // NR1 — from RxChannel::Nr1Tuning defaults (Task 8 commit 8747ae4),
    // which in turn match Thetis radio.cs:673-699 [v2.10.3.13].
    int    m_nr1Taps    = 64;           // radio.cs:674   nr_taps = 64
    int    m_nr1Delay   = 16;           // radio.cs:675   nr_delay = 16
    double m_nr1Gain    = 16e-4;        // radio.cs:677   nr_gain = 16e-4 (WDSP-domain)
    double m_nr1Leakage = 10e-7;        // radio.cs:679   nr_leak = 10e-7 (WDSP-domain)
    NereusSDR::NrPosition m_nr1Position = NereusSDR::NrPosition::PostAgc;  // setup.cs:8723

    // NR2 — from RxChannel::Nr2Tuning defaults (Task 8 commit 8747ae4),
    // matching Thetis radio.cs:2062-2213, setup.cs:34711-34748 [v2.10.3.13].
    NereusSDR::EmnrGainMethod m_nr2GainMethod = NereusSDR::EmnrGainMethod::Gamma;  // setup.cs:17359-17468
    NereusSDR::EmnrNpeMethod  m_nr2NpeMethod  = NereusSDR::EmnrNpeMethod::Osms;    // setup.cs:17374-17404
    double m_nr2TrainT1 = -0.5;         // EMNR zetaThresh default (unsigned domain)
    double m_nr2TrainT2 = 0.20;         // EMNR t2 default
    bool   m_nr2AeFilter = true;        // radio.cs:2103  rx_nr2_ae_run = 1
    NereusSDR::NrPosition m_nr2Position = NereusSDR::NrPosition::PostAgc;    // radio.cs:2237
    bool   m_nr2Post2Run    = false;    // radio.cs:2122  default off
    double m_nr2Post2Level  = 15.0;    // radio.cs:2139  rx_nr2_ae_post2_nlevel = 15.0
    double m_nr2Post2Factor = 15.0;    // radio.cs:2158  rx_nr2_ae_post2_factor = 15.0
    double m_nr2Post2Rate   = 5.0;     // radio.cs:2177  rx_nr2_ae_post2_rate = 5.0
    int    m_nr2Post2Taper  = 12;      // radio.cs:2196  rx_nr2_ae_post2_taper = 12

    // NR3 — from RxChannel::Nr3Tuning defaults (Task 8 commit 8747ae4),
    // matching Thetis radio.cs:2257-2311, setup.cs:35460-35462 [v2.10.3.13].
    NereusSDR::NrPosition m_nr3Position = NereusSDR::NrPosition::PostAgc;  // radio.cs:2275
    bool   m_nr3UseDefaultGain = true;  // setup.cs:35460  RXANR3FixedGain default

    // NR4 — from RxChannel::Nr4Tuning defaults (Task 8 commit 8747ae4),
    // matching Thetis radio.cs:2312-2355, setup.cs:34511-34527 [v2.10.3.13].
    double m_nr4Reduction  = 10.0;     // setup.cs default
    double m_nr4Smoothing  = 65.0;     // setup.cs default
    double m_nr4Whitening  = 2.0;      // setup.cs default
    double m_nr4Rescale    = 2.0;      // setup.cs default
    double m_nr4PostThresh = -10.0;    // setup.cs default
    NereusSDR::SbnrAlgo m_nr4Algo = NereusSDR::SbnrAlgo::Algo2;  // setup.cs:34511-34527

    // DFNR — AetherSDR DeepFilterFilter defaults [@0cd4559] (post-WDSP, not
    // in Thetis). m_attenLimit{100.0f}, m_postFilterBeta{0.0f} verbatim.
    double m_dfnrAttenLimit     = 100.0;
    double m_dfnrPostFilterBeta = 0.0;

    // BNR + MNR — AetherSDR filter defaults (post-WDSP, not in Thetis).
    double m_bnrStrength = 1.0;
    double m_mnrStrength = 1.0;
    double m_mnrOversub  = 4.0;    // MacNRFilter::DEF_OVER;   range 0.01-1000 at filter
    double m_mnrFloor    = 0.05;   // MacNRFilter::DEF_FLOOR;  range 0.0-2.0 at filter
    double m_mnrAlpha    = 0.92;   // MacNRFilter::DEF_ALPHA;  range 0.0-1.0
    double m_mnrBias     = 1.2;    // MacNRFilter::DEF_BIAS;   range 0.0-10.0
    double m_mnrGsmooth  = 0.70;   // MacNRFilter::DEF_GSMOOTH; range 0.0-1.0

    bool   m_snbEnabled{false};       // Neutral default — feature off at start
    bool   m_apfEnabled{false};       // Neutral default — feature off at start
    int    m_apfTuneHz{0};            // Neutral default — zero tune offset
    bool   m_binauralEnabled{false};  // Neutral default — feature off at start
    int    m_fmCtcssMode{0};          // Neutral default — Off (0 = disabled)
    double m_fmCtcssValueHz{100.0};   // From Thetis console.cs:40500 — ctcss_freq = 100.0; radio.cs:2899 — ctcss_freq_hz = 100.0
    int      m_fmOffsetHz{0};           // Neutral default — zero offset
    FmTxMode m_fmTxMode{FmTxMode::Simplex};  // From Thetis console.cs:20873 — current_fm_tx_mode = FMTXMode.Simplex
    bool     m_fmReverse{false};        // Neutral default — normal direction
    // Upstream inline attribution preserved verbatim:
    //   console.cs:14669  //reset preset filter's center frequency - W4TME
    int      m_diglOffsetHz{0};         // From Thetis console.cs:14672 — DIGLClickTuneOffset default 0 Hz
    int      m_diguOffsetHz{0};         // From Thetis console.cs:14637 — DIGUClickTuneOffset default 0 Hz
    int    m_rttyMarkHz{2295};        // From Thetis setup.designer.cs:40635 — tooltip "RTTY MARK frequency" on udDSPRX1DollyF1; value 2295 (line 40637). F0=2125 is SPACE (line 40665).
    int    m_rttyShiftHz{170};        // From Thetis radio.cs:2043-2044 — rx_dolly_freq1 = 2295, rx_dolly_freq0 = 2125 → shift = 2295−2125 = 170 Hz

    // ── Phase 3O VAX routing ──────────────────────────────────────────────────
    std::atomic<int> m_vaxChannel{0};  // 0=Off, 1..4=VAX N. Atomic for audio-thread-safe reads.
};

} // namespace NereusSDR
