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

    // Maximum attenuator value (hardware limit, typically 31 dB).
    int maxAttenuation() const { return m_maxAttDb; }
    void setMaxAttenuation(int dB);

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
    // Default max step attenuator (dB).
    static constexpr int kDefaultMaxAttDb = 31;
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

    // Per-band storage.
    Band m_currentBand = Band::GEN;
    struct BandAttState {
        int attDb = 0;
        PreampMode preamp = PreampMode::Off;
    };
    std::unordered_map<int, BandAttState> m_bandState;

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

private slots:
    void onDdcMappingChanged();
};

}  // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::OverloadLevel)
Q_DECLARE_METATYPE(NereusSDR::PreampMode)
Q_DECLARE_METATYPE(NereusSDR::AutoAttMode)
