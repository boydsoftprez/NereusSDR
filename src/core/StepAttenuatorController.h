// =================================================================
// src/core/StepAttenuatorController.h  (NereusSDR)
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

#pragma once

#include "models/Band.h"
#include "core/WdspTypes.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <array>
#include <unordered_map>

namespace NereusSDR {

class RadioConnection;
class ReceiverManager;

// --- Enums ---

// ADC overload severity level.
// From Thetis console.cs:21369 — yellow when level > 0, red when level > 3.
enum class OverloadLevel {
    None,
    Yellow,
    Red
};

// Auto-attenuate algorithm selection.
enum class AutoAttMode {
    Classic,    // Thetis 1:1 bump + stack undo
    Adaptive    // NereusSDR attack/hold/decay with per-band memory
};

// Preamp mode (Thetis PreampMode enum, console.cs:21574-21586).
// Used by classic auto-att when step-att is disabled.
enum class PreampMode {
    Off,
    On,
    Minus10,
    Minus20,    // MW0LGE_21d step atten [Thetis enums.cs:246]
    Minus30,
    Minus40,
    Minus50
};

// --- Controller ---

class StepAttenuatorController : public QObject {
    Q_OBJECT
public:
    explicit StepAttenuatorController(QObject* parent = nullptr);

    // --- Accessors ---

    // Per-ADC overload level (0-5). From Thetis console.cs:21212.
    int overloadCounter(int adc) const;

    // Derived severity for a given ADC.
    OverloadLevel overloadLevel(int adc) const;

    // Current step attenuator value (dB).
    int attenuatorDb() const noexcept { return m_attDb; }

    // Current preamp mode.
    PreampMode preampMode() const noexcept { return m_preampMode; }

    // Step-att enabled (S-ATT mode vs ATT preamp combo mode).
    bool stepAttEnabled() const noexcept { return m_stepAttEnabled; }

    // Auto-att state.
    AutoAttMode autoAttMode() const noexcept { return m_autoAttMode; }
    bool autoAttEnabled() const noexcept { return m_autoAttEnabled; }
    bool autoAttApplied() const noexcept { return m_autoAttApplied; }

    // --- Configuration setters ---

    void setAutoAttEnabled(bool on);
    void setAutoAttMode(AutoAttMode mode);

    // Classic mode: enable timer-based undo of auto-applied attenuation.
    void setAutoAttUndo(bool on);
    void setAutoUndoDelaySec(int sec);

    // Adaptive mode tunables.
    void setAutoAttHoldSeconds(double sec);
    void setAdaptiveDecayMs(int ms);

    // Step-att vs preamp mode (Thetis _rx1_step_att_enabled).
    void setStepAttEnabled(bool on);

    // Current band — for per-band ATT/preamp storage.
    void setBand(Band band);

    // Set ATT value (e.g. from UI or persistence restore).
    void setAttenuation(int dB, int rx = 0);

    // Set preamp mode (e.g. from UI).
    void setPreampMode(PreampMode mode);

    // Attenuator value bounds (hardware limits).
    //   Most boards: 0..31 dB unsigned.
    //   HL2 (mi0bot setup.cs:16085-16086 [v2.10.3.13-beta2]): −28..+32 dB
    //     signed; wire byte derives via `wire = 31 - userDb` in P1CodecHl2.
    int maxAttenuation() const { return m_maxAttDb; }
    void setMaxAttenuation(int dB);
    int minAttenuation() const { return m_minAttDb; }
    void setMinAttenuation(int dB);

    // --- TX-path configuration (F.2) ---
    //
    // ATT-on-TX master enable (Thetis _m_bATTonTX, console.cs:19041 [v2.10.3.13]).
    // When false, TX ATT is cleared to 0 dB on MOX-on.
    void setAttOnTxEnabled(bool on) { m_attOnTxEnabled = on; }
    bool attOnTxEnabled() const noexcept { return m_attOnTxEnabled; }

    // Force-31-dB when PS-A is off (Thetis _forceATTwhenPSAoff,
    // console.cs:29285 [v2.10.3.13] //MW0LGE [2.9.0.7] added).
    void setForceAttWhenPsOff(bool on) { m_forceAttWhenPsOff = on; }
    bool forceAttWhenPsOff() const noexcept { return m_forceAttWhenPsOff; }

    // PS-A active state: true when PureSignal auto-cal is ON.
    // Set by RadioModel when the PS state changes.
    // shouldForce31Db uses !m_psActive as the "PS off" input
    // (equivalent to !chkFWCATUBypass.Checked in Thetis).
    void setPsActive(bool on) { m_psActive = on; }
    bool psActive() const noexcept { return m_psActive; }

    // Current TX DSP mode (set by RadioModel from SliceModel::dspMode).
    // Used by shouldForce31Db to detect CWL/CWU.
    void setCurrentDspMode(DSPMode mode) { m_currentDspMode = mode; }
    DSPMode currentDspMode() const noexcept { return m_currentDspMode; }

    // HPSDR-board flag: true ⟺ connected radio is HPSDRModel::HPSDR
    // (Atlas/Metis kit), which uses the preamp save/restore path instead
    // of per-band TX ATT (Thetis console.cs:29548-29558 [v2.10.3.13]).
    void setIsHpsdrBoard(bool on) { m_isHpsdrBoard = on; }
    bool isHpsdrBoard() const noexcept { return m_isHpsdrBoard; }

    // Per-band TX ATT storage (Thetis tx_step_attenuator_by_band,
    // console.cs:205/48012-48022 [v2.10.3.13]).
    // Default 0 dB for all bands.
    void setTxAttenuationForBand(Band band, int dB);
    int txAttenuationForBand(Band band) const;

    // shouldForce31Db predicate.
    //
    // Returns true ⟺ the TX attenuator must be forced to 31 dB.
    // From Thetis console.cs:29563-29566 [v2.10.3.13] //MW0LGE [2.9.0.7] added:
    //   txAtt = 31 ⟺ (!chkFWCATUBypass.Checked && _forceATTwhenPSAoff)
    //                 || (CurrentDSPMode == CWL || CurrentDSPMode == CWU)
    bool shouldForce31Db(DSPMode dspMode, bool isPsOff) const;

    // Wire to a RadioConnection for adcOverflow signals.
    void setRadioConnection(RadioConnection* conn);

    // Wire to ReceiverManager for DDC mapping changes.
    void setReceiverManager(ReceiverManager* mgr);

    // Per-MAC persistence — save/load all ATT/preamp/auto-att settings.
    void saveSettings(const QString& mac);
    void loadSettings(const QString& mac);

    // --- Tick (public for testability) ---

    // Stop/start the internal tick timer. Tests call setTickTimerEnabled(false)
    // then drive tick() manually for deterministic cycle control.
    void setTickTimerEnabled(bool on);

    // Called on each poll cycle (~100ms). Updates per-ADC hysteresis
    // counters and emits overloadStatusChanged on level transitions.
    // In production, driven by an internal QTimer; exposed for tests.
    void tick();

public slots:
    // Receives adcOverflow(int adc) from RadioConnection.
    // Marks the ADC as overloaded for the current tick cycle.
    void onAdcOverflow(int adc);

    // TX-path activation slot — F.2.
    //
    // Porting from Thetis console.cs:29546-29576 [v2.10.3.13] §6.2-§6.4.
    // Called when MoxController::hardwareFlipped(bool isTx) fires.
    //
    // isTx=true (RX→TX):
    //   HPSDR board: save current preamp mode, then force PreampMode::Off
    //     (≡ Thetis PreampMode.HPSDR_OFF, −20 dB attenuation).
    //   Standard board: look up per-band TX ATT; apply force-31-dB override
    //     if shouldForce31Db() is true; call setTxStepAttenuation() on the
    //     RadioConnection.
    //   If m_attOnTxEnabled is false: push 0 dB TX ATT (no attenuation on TX).
    //
    // isTx=false (TX→RX):
    //   HPSDR board: restore the saved preamp mode.
    //   Standard board: re-apply the current band's RX ATT via setBand()
    //     (existing path restores the per-band RX state).
    //
    // NOTE: the connect() call wiring this slot to MoxController::hardwareFlipped
    // is deferred to Task G.1 (same pattern as F.1 / AlexController). F.2 only
    // adds the slot logic and supporting helpers.
    void onMoxHardwareFlipped(bool isTx);

signals:
    // Emitted when any ADC's overload level transitions between
    // None/Yellow/Red. UI surfaces connect here for badge updates.
    void overloadStatusChanged(int adc, NereusSDR::OverloadLevel level);

    // Emitted when auto-att changes the attenuator value.
    void attenuationChanged(int dB);

    // Emitted when auto-att changes the preamp mode.
    void preampModeChanged(NereusSDR::PreampMode mode);

    // Emitted when auto-att applied/cleared state changes.
    void autoAttActiveChanged(bool applied);

    // Emitted when step-att-enabled changes (ATT ↔ S-ATT mode switch).
    void stepAttEnabledChanged(bool enabled);

    // Emitted when ADC-linked state changes (both RX share same ADC).
    void adcLinkedChanged(bool linked);

private:
    static constexpr int kMaxAdcs = 3;
    // From Thetis console.cs:21366 — counter caps at 5.
    static constexpr int kMaxOverloadLevel = 5;
    // From Thetis console.cs:21369 — red threshold.
    static constexpr int kRedThreshold = 3;
    // Default step attenuator bounds (dB).
    static constexpr int kDefaultMaxAttDb = 31;
    static constexpr int kDefaultMinAttDb = 0;
    // Tick interval (ms) — Thetis pollOverloadSyncSeqErr ~400ms,
    // NereusSDR uses 100ms for snappier response.
    static constexpr int kTickIntervalMs = 100;

    // Push a new ATT value to hardware + emit signal.  Used by auto-att
    // paths that bypass setAttenuation() (which also stores per-band state).
    void applyAttToHardware(int dB);

    // Per-ADC state. From Thetis console.cs:21212-21214.
    struct AdcState {
        bool overflowed = false;    // set by onAdcOverflow, cleared by tick
        int level = 0;             // hysteresis counter (0-kMaxOverloadLevel)
        OverloadLevel lastEmitted = OverloadLevel::None;
    };
    std::array<AdcState, kMaxAdcs> m_adcState{};

    // Step attenuator / preamp state.
    int m_attDb = 0;
    PreampMode m_preampMode = PreampMode::Off;
    bool m_stepAttEnabled = true;
    int m_maxAttDb = kDefaultMaxAttDb;
    int m_minAttDb = kDefaultMinAttDb;

    // Auto-att configuration.
    bool m_autoAttEnabled = false;
    AutoAttMode m_autoAttMode = AutoAttMode::Classic;
    bool m_autoAttApplied = false;

    // Classic mode state — Thetis uses a stack of historic readings.
    // We simplify to tracking the pre-auto-att value.
    int m_classicSavedAttDb = -1;
    PreampMode m_classicSavedPreamp = PreampMode::Off;
    bool m_autoUndoEnabled = false;
    int m_autoUndoDelaySec = 5;  // From Thetis console.cs:21224
    qint64 m_lastAutoAttTimeMs = 0;

    // Adaptive mode state.
    int m_adaptiveHoldMs = 2000;
    int m_adaptiveDecayMs = 500;
    qint64 m_adaptiveLastAttackMs = 0;
    qint64 m_adaptiveLastDecayMs = 0;
    int m_adaptiveFloorDb = 0;

    // Per-band RX ATT/preamp storage.
    Band m_currentBand = Band::GEN;
    struct BandAttState {
        int attDb = 0;
        PreampMode preamp = PreampMode::Off;
    };
    std::unordered_map<int, BandAttState> m_bandState;

    // --- TX-path state (F.2) ---

    // ATT-on-TX master enable. From Thetis console.cs:19041 [v2.10.3.13]
    //   private bool m_bATTonTX = true;
    bool m_attOnTxEnabled{true};

    // Force-31-dB when PS-A off. From Thetis console.cs:29285 [v2.10.3.13]
    //   private bool _forceATTwhenPSAoff = true; //MW0LGE [2.9.0.7] added
    bool m_forceAttWhenPsOff{true};

    // PureSignal-A active state (true = PS-A on = chkFWCATUBypass.Checked).
    // Set by RadioModel. shouldForce31Db uses !m_psActive as "PS off" input.
    bool m_psActive{false};

    // Current TX DSP mode for shouldForce31Db CW detection.
    DSPMode m_currentDspMode{DSPMode::LSB};

    // HPSDR-board (Atlas/Metis) flag.
    // From Thetis console.cs:29548 [v2.10.3.13]:
    //   if (HardwareSpecific.Model == HPSDRModel.HPSDR) { ... preamp save/restore ... }
    bool m_isHpsdrBoard{false};

    // Per-band TX step attenuator (0-31 dB, default 0).
    // From Thetis console.cs:205 [v2.10.3.13]:
    //   private int[] tx_step_attenuator_by_band;
    // Thetis default: 31 dB per band (console.cs:1810 [v2.10.3.13]):
    //   setTXstepAttenuatorForBand((Band)i, 31);
    // NereusSDR default: 0 (no TX ATT until user configures it).
    // Per-band TX ATT spans HF amateur + GEN/WWV/XVTR only.  SWL bands
    // (Band::SwlFirst..SwlLast, Phase 3L extension) inherit ham-band
    // values — the HL2 ATT chip is a single hardware register regardless
    // of the SWL slice you tune to.  Sized at Band::SwlFirst (=14).
    std::array<int, static_cast<size_t>(Band::SwlFirst)> m_txAttByBand{};

    // HPSDR-only preamp save/restore (F.2).
    // Distinct from m_classicSavedPreamp (which is for the auto-att
    // Classic mode undo path and serves a different purpose).
    PreampMode m_savedPreampMode{PreampMode::Off};

    // Internal tick timer.
    QTimer m_tickTimer;

    // RadioConnection for adcOverflow wiring.
    QPointer<RadioConnection> m_connection;
    QMetaObject::Connection m_adcOverflowConn;

    // ReceiverManager for DDC mapping.
    QPointer<ReceiverManager> m_receiverManager;

    // ADC-linked state (both RX0 and RX1 share the same ADC).
    bool m_adcLinked{false};

    // --- Helpers ---
    void applyClassicAutoAtt(int adc);
    void applyClassicUndo();
    void applyAdaptiveAutoAtt(int adc);
    void setAutoAttApplied(bool applied);
    OverloadLevel levelToSeverity(int level) const;
    void checkAdcLinked();

    // TX-path helpers (F.2).

    // Look up the per-band TX ATT value. Returns m_txAttByBand[band] or 0
    // if band is out of range.
    // From Thetis console.cs:48012-48017 [v2.10.3.13] getTXstepAttenuatorForBand.
    int applyTxAttenuationForBand(Band band) const;

    // HPSDR-only: cache the current preamp mode before TX.
    // From Thetis console.cs:29550 [v2.10.3.13]: temp_mode = RX1PreampMode.
    void saveRxPreampMode();

    // HPSDR-only: restore the cached preamp mode after TX.
    // From Thetis console.cs:29550 [v2.10.3.13]: RX1PreampMode = temp_mode.
    void restoreRxPreampMode();

private slots:
    void onDdcMappingChanged();

#ifdef NEREUS_BUILD_TESTS
public:
    // Test seams — expose internal TX-path state for white-box unit tests.
    PreampMode savedPreampModeForTest() const noexcept { return m_savedPreampMode; }
    int txAttByBandForTest(Band band) const { return applyTxAttenuationForBand(band); }
    // Expose the last TX ATT value pushed to hardware (via m_lastTxStepAttDb).
    int lastTxStepAttForTest() const noexcept { return m_lastTxStepAttDb; }
private:
    int m_lastTxStepAttDb{-1};  // set by onMoxHardwareFlipped; -1 = never called
#endif
};

}  // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::OverloadLevel)
Q_DECLARE_METATYPE(NereusSDR::PreampMode)
Q_DECLARE_METATYPE(NereusSDR::AutoAttMode)
