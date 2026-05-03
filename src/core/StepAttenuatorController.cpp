// =================================================================
// src/core/StepAttenuatorController.cpp  (NereusSDR)
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

#include "StepAttenuatorController.h"
#include "AppSettings.h"
#include "RadioConnection.h"
#include "ReceiverManager.h"
#include "WdspTypes.h"

#include <QDateTime>
#include <QMetaObject>

namespace NereusSDR {

StepAttenuatorController::StepAttenuatorController(QObject* parent)
    : QObject(parent)
{
    m_tickTimer.setInterval(kTickIntervalMs);
    connect(&m_tickTimer, &QTimer::timeout, this, &StepAttenuatorController::tick);
    m_tickTimer.start();
}

// --- Timer control ---

void StepAttenuatorController::setTickTimerEnabled(bool on)
{
    if (on) {
        m_tickTimer.start();
    } else {
        m_tickTimer.stop();
    }
}

// --- Accessors ---

int StepAttenuatorController::overloadCounter(int adc) const
{
    if (adc < 0 || adc >= kMaxAdcs) {
        return 0;
    }
    return m_adcState[static_cast<size_t>(adc)].level;
}

OverloadLevel StepAttenuatorController::overloadLevel(int adc) const
{
    return levelToSeverity(overloadCounter(adc));
}

// --- Configuration setters ---

void StepAttenuatorController::setAutoAttEnabled(bool on)
{
    m_autoAttEnabled = on;
    if (!on && m_autoAttApplied) {
        // Clear auto-att state when disabled.
        // From Thetis console.cs:21509-21514: clear stack when auto-att off.
        applyClassicUndo();
    }
}

void StepAttenuatorController::setAutoAttMode(AutoAttMode mode)
{
    // P1 full-parity §4.1: gate Adaptive on BoardCapabilities flag.
    // Silent-coerce to Classic when the connected board lacks per-step
    // ATT calibration support.  Refusing would break Setup combo flows
    // that may try to set Adaptive on any radio.
    if (mode == AutoAttMode::Adaptive && !m_hasStepAttCal) {
        mode = AutoAttMode::Classic;
    }
    m_autoAttMode = mode;
}

void StepAttenuatorController::setAutoAttUndo(bool on)
{
    m_autoUndoEnabled = on;
}

void StepAttenuatorController::setAutoUndoDelaySec(int sec)
{
    m_autoUndoDelaySec = sec;
}

void StepAttenuatorController::setAutoAttHoldSeconds(double sec)
{
    m_adaptiveHoldMs = static_cast<int>(sec * 1000.0);
}

void StepAttenuatorController::setAdaptiveDecayMs(int ms)
{
    m_adaptiveDecayMs = ms;
}

void StepAttenuatorController::setStepAttEnabled(bool on)
{
    if (m_stepAttEnabled == on) { return; }
    m_stepAttEnabled = on;
    emit stepAttEnabledChanged(on);
}

void StepAttenuatorController::setBand(Band band)
{
    if (m_currentBand == band) {
        return;
    }

    // Save current ATT/preamp to old band.
    m_bandState[static_cast<int>(m_currentBand)] = { m_attDb, m_preampMode };

    m_currentBand = band;

    // Restore ATT/preamp from new band (if any stored).
    auto it = m_bandState.find(static_cast<int>(band));
    if (it != m_bandState.end()) {
        if (it->second.attDb != m_attDb) {
            m_attDb = it->second.attDb;
            emit attenuationChanged(m_attDb);
        }
        if (it->second.preamp != m_preampMode) {
            m_preampMode = it->second.preamp;
            emit preampModeChanged(m_preampMode);
        }
    }

    // Clear auto-att state on band change.
    // From Thetis console.cs:21526-21529: keep_att_entries_for_band.
    if (m_autoAttApplied) {
        setAutoAttApplied(false);
        m_classicSavedAttDb = -1;
    }
}

void StepAttenuatorController::setAttenuation(int dB, int rx)
{
    // Clamp to the active board's signed range.  HL2 advertises
    // [m_minAttDb=-28, m_maxAttDb=+32] (mi0bot setup.cs:1087-1088
    // [v2.10.3.13-beta2]); legacy ANAN/Hermes boards keep m_minAttDb=0
    // so behaviour is unchanged for them.  Without honouring m_minAttDb
    // here, any negative HL2 value selected in the RX UI gets snapped
    // back to 0 before reaching P1RadioConnection — Codex P1 in PR #157.
    if (dB < m_minAttDb) { dB = m_minAttDb; }
    if (dB > m_maxAttDb) { dB = m_maxAttDb; }
    if (m_attDb == dB) { return; }
    m_attDb = dB;

    // Send to hardware — from Thetis console.cs RX1AttenuatorData property.
    if (m_connection) {
        m_connection->setAttenuator(dB);
    }

    emit attenuationChanged(m_attDb);

    // ADC-linked: force the other RX to match.
    if (m_adcLinked && (rx == 0 || rx == 1)) {
        // Prevent infinite recursion — only propagate once.
        int otherRx = (rx == 0) ? 1 : 0;
        Q_UNUSED(otherRx);
        // The linked receiver should track this value.
        // (Full per-RX ATT arrays are a future enhancement;
        //  for now both share m_attDb.)
    }
}

void StepAttenuatorController::setPreampMode(PreampMode mode)
{
    if (m_preampMode == mode) { return; }
    m_preampMode = mode;

    // Send to hardware — from Thetis console.cs comboPreamp_SelectedIndexChanged.
    if (m_connection) {
        m_connection->setPreamp(mode != PreampMode::Off);
    }

    emit preampModeChanged(m_preampMode);
}

void StepAttenuatorController::setMaxAttenuation(int dB)
{
    m_maxAttDb = dB;
}

void StepAttenuatorController::setMinAttenuation(int dB)
{
    m_minAttDb = dB;
}

// --- Per-band TX ATT storage (F.2) ---
// From Thetis console.cs:48012-48022 [v2.10.3.13]
//   getTXstepAttenuatorForBand / setTXstepAttenuatorForBand

void StepAttenuatorController::setTxAttenuationForBand(Band band, int dB)
{
    // From Thetis console.cs:48018-48022 [v2.10.3.13]:
    //   private void setTXstepAttenuatorForBand(Band b, int att)
    //   { if (b <= Band.FIRST || b >= Band.LAST) return;
    //     tx_step_attenuator_by_band[(int)b] = att; }
    int idx = static_cast<int>(band);
    if (idx < 0 || idx >= static_cast<int>(Band::SwlFirst)) { return; }
    if (dB < 0)            { dB = 0; }
    if (dB > m_maxAttDb)   { dB = m_maxAttDb; }
    m_txAttByBand[static_cast<size_t>(idx)] = dB;
}

int StepAttenuatorController::txAttenuationForBand(Band band) const
{
    return applyTxAttenuationForBand(band);
}

// Private helper — used by onMoxHardwareFlipped and the test seam.
int StepAttenuatorController::applyTxAttenuationForBand(Band band) const
{
    // From Thetis console.cs:48012-48017 [v2.10.3.13]:
    //   private int getTXstepAttenuatorForBand(Band b)
    //   { if (b <= Band.FIRST || b >= Band.LAST) return 31;
    //     return tx_step_attenuator_by_band[(int)b]; }
    // NereusSDR: Band::GEN is index 0 (not FIRST sentinel); no sentinels in
    // our enum, so range-check by Count.
    int idx = static_cast<int>(band);
    if (idx < 0 || idx >= static_cast<int>(Band::SwlFirst)) { return 0; }
    return m_txAttByBand[static_cast<size_t>(idx)];
}

// --- HPSDR preamp save/restore (F.2) ---
// From Thetis console.cs:29550-29561 [v2.10.3.13]
//   temp_mode = RX1PreampMode;
//   SetupForm.RX1EnableAtt = false;
//   RX1PreampMode = PreampMode.HPSDR_OFF;   // set to -20dB
//
// The else-branch for non-HPSDR boards follows at console.cs:29561 [v2.10.3.13]:
//MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps

void StepAttenuatorController::saveRxPreampMode()
{
    // Cache current preamp mode before TX.
    // From Thetis console.cs:29550 [v2.10.3.13]: temp_mode = RX1PreampMode
    m_savedPreampMode = m_preampMode;
}

void StepAttenuatorController::restoreRxPreampMode()
{
    // Restore preamp mode after TX.
    // Symmetric with saveRxPreampMode.
    setPreampMode(m_savedPreampMode);
}

// --- shouldForce31Db predicate (F.2) ---
// From Thetis console.cs:29563-29566 [v2.10.3.13] //MW0LGE [2.9.0.7] added:
//   if ((!chkFWCATUBypass.Checked && _forceATTwhenPSAoff) ||
//          (radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWL ||
//           radio.GetDSPTX(0).CurrentDSPMode == DSPMode.CWU)) txAtt = 31;
//
// NereusSDR mapping:
//   !chkFWCATUBypass.Checked  ≡  !m_psActive  (PS-A not active)
//   _forceATTwhenPSAoff        ≡  m_forceAttWhenPsOff
//   isPsOff                    ≡  !m_psActive  (passed by caller)

bool StepAttenuatorController::shouldForce31Db(DSPMode dspMode, bool isPsOff) const
{
    // From Thetis console.cs:29561 [v2.10.3.13]:
    //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
    if (!m_attOnTxEnabled) {
        // ATT-on-TX is disabled — never force.
        return false;
    }
    if (m_forceAttWhenPsOff && isPsOff) {
        // PS-A is off AND the "force 31 when PS off" setting is enabled.
        return true;  //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
    }
    // CW modes always force 31 dB (prevent PA damage via TX ATT).
    // From Thetis console.cs:29565-29566 [v2.10.3.13]: CWL || CWU → txAtt = 31
    return (dspMode == DSPMode::CWL || dspMode == DSPMode::CWU);
}

// --- onMoxHardwareFlipped slot (F.2) ---
// From Thetis console.cs:29546-29576 [v2.10.3.13] (§6.2-§6.4)

void StepAttenuatorController::onMoxHardwareFlipped(bool isTx)
{
    if (isTx) {
        // RX→TX transition.
        if (!m_attOnTxEnabled) {
            // ATT-on-TX disabled: clear TX ATT (NetworkIO.SetTxAttenData(0)).
            // From Thetis console.cs:29575-29576 [v2.10.3.13]:
            //   NetworkIO.SetTxAttenData(0);
            //   Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes
            // Marshalled to connection thread — m_connection is connection-thread owned.
            if (m_connection) {
                RadioConnection* conn = m_connection.get();
                QMetaObject::invokeMethod(conn, [conn]() {
                    conn->setTxStepAttenuation(0); //[2.10.3.6]MW0LGE att_fixes
                });
            }
#ifdef NEREUS_BUILD_TESTS
            m_lastTxStepAttDb = 0;
#endif
            return;
        }

        if (m_isHpsdrBoard) {
            // HPSDR variant: save preamp mode, then force PreampMode::Off
            // (≡ Thetis PreampMode.HPSDR_OFF, −20 dB).
            // From Thetis console.cs:29550-29556 [v2.10.3.13]:
            //   temp_mode = RX1PreampMode;
            //   SetupForm.RX1EnableAtt = false;
            //   RX1PreampMode = PreampMode.HPSDR_OFF;  // set to -20dB
            saveRxPreampMode();
            setPreampMode(PreampMode::Minus20);  // -20 dB ≡ HPSDR_OFF
        } else {
            // Non-HPSDR standard board: TX ATT lookup + force-31 override.
            // From Thetis console.cs:29562-29568 [v2.10.3.13]:
            //   int txAtt = getTXstepAttenuatorForBand(_tx_band);
            //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
            //   if ((!chkFWCATUBypass.Checked && _forceATTwhenPSAoff) ||
            //       (CWL || CWU)) txAtt = 31; // reset when PS is OFF or in CW mode
            //   SetupForm.ATTOnRX1 = getRX1stepAttenuatorForBand(rx1_band); //[2.10.3.6]MW0LGE att_fixes
            //   SetupForm.ATTOnTX = txAtt; //[2.10.3.6]MW0LGE att_fixes NOTE: this will eventually call Display.TXAttenuatorOffset with the value
            int txAtt = applyTxAttenuationForBand(m_currentBand);
            const bool psOff = !m_psActive;
            if (shouldForce31Db(m_currentDspMode, psOff)) {
                txAtt = 31; // reset when PS is OFF or in CW mode
            }
            // Marshalled to connection thread — m_connection is connection-thread owned.
            if (m_connection) {
                RadioConnection* conn = m_connection.get();
                QMetaObject::invokeMethod(conn, [conn, txAtt]() {
                    conn->setTxStepAttenuation(txAtt); //[2.10.3.6]MW0LGE att_fixes
                });
            }
#ifdef NEREUS_BUILD_TESTS
            m_lastTxStepAttDb = txAtt;
#endif
        }
    } else {
        // TX→RX transition: restore RX state.
        if (m_isHpsdrBoard) {
            // HPSDR: restore the preamp mode saved at TX start.
            restoreRxPreampMode();
        } else {
            // Standard board: re-apply the current band's RX ATT.
            // setBand() with the same band is a no-op (idempotent guard),
            // so we call a direct restore of m_bandState.
            // Clear TX ATT back to 0.
            // From Thetis console.cs:29658 [v2.10.3.13]:
            //   NetworkIO.SetTxAttenData(0);
            //   Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes
            // Marshalled to connection thread — m_connection is connection-thread owned.
            if (m_connection) {
                RadioConnection* conn = m_connection.get();
                QMetaObject::invokeMethod(conn, [conn]() {
                    conn->setTxStepAttenuation(0); //[2.10.3.6]MW0LGE att_fixes
                });
            }
#ifdef NEREUS_BUILD_TESTS
            m_lastTxStepAttDb = 0;
#endif
            // Restore RX ATT: re-apply stored per-band value.
            auto it = m_bandState.find(static_cast<int>(m_currentBand));
            if (it != m_bandState.end() && it->second.attDb != m_attDb) {
                setAttenuation(it->second.attDb, 0);
            }
        }
    }
}

// --- Tick ---

void StepAttenuatorController::tick()
{
    // From Thetis console.cs:21359-21382 — per-ADC hysteresis counter.
    for (int i = 0; i < kMaxAdcs; ++i) {
        AdcState& st = m_adcState[static_cast<size_t>(i)];

        if (st.overflowed) {
            st.level++;
            if (st.level > kMaxOverloadLevel) {
                st.level = kMaxOverloadLevel;
            }
        } else {
            if (st.level > 0) {
                st.level--;
            }
        }

        // Clear the per-tick overflow flag for next cycle.
        st.overflowed = false;

        // Check for level transition and emit.
        OverloadLevel newLevel = levelToSeverity(st.level);
        if (newLevel != st.lastEmitted) {
            st.lastEmitted = newLevel;
            emit overloadStatusChanged(i, newLevel);
        }
    }

    // Auto-attenuate on red overload.
    if (m_autoAttEnabled) {
        bool anyRed = false;
        int redAdc = -1;
        for (int i = 0; i < kMaxAdcs; ++i) {
            if (m_adcState[static_cast<size_t>(i)].level > kRedThreshold) {
                anyRed = true;
                redAdc = i;
                break;
            }
        }

        // P1 full-parity §4.1: defence-in-depth — the setAutoAttMode +
        // loadSettings gates above already prevent m_autoAttMode reaching
        // Adaptive on a hasStepAttenuatorCal=false board, but if a stale
        // state slips through (e.g. flag toggled mid-run) treat it as
        // Classic here too.
        const bool adaptiveAllowed =
            (m_autoAttMode == AutoAttMode::Adaptive) && m_hasStepAttCal;

        if (anyRed) {
            if (!adaptiveAllowed) {
                applyClassicAutoAtt(redAdc);
            } else {
                applyAdaptiveAutoAtt(redAdc);
            }
        } else if (m_autoAttApplied) {
            // No overload — try undo.
            if (!adaptiveAllowed) {
                applyClassicUndo();
            }
            // Adaptive decay is handled inside applyAdaptiveAutoAtt
            // even when there's no red — but only if previously applied.
            if (adaptiveAllowed && m_autoAttApplied) {
                applyAdaptiveAutoAtt(-1);  // decay path
            }
        }
    }
}

// --- Slots ---

void StepAttenuatorController::onAdcOverflow(int adc)
{
    if (adc < 0 || adc >= kMaxAdcs) {
        return;
    }
    m_adcState[static_cast<size_t>(adc)].overflowed = true;
}

// --- Classic auto-att ---

void StepAttenuatorController::applyClassicAutoAtt(int adc)
{
    // From Thetis console.cs:21548-21567 — bump ATT by step shift on red.
    if (m_stepAttEnabled) {
        int shift = m_adcState[static_cast<size_t>(adc)].level;
        int newAtt = m_attDb + shift;
        if (newAtt > m_maxAttDb) {
            newAtt = m_maxAttDb;
        }
        if (newAtt != m_attDb) {
            if (m_classicSavedAttDb < 0) {
                m_classicSavedAttDb = m_attDb;
            }
            applyAttToHardware(newAtt);
            m_lastAutoAttTimeMs = QDateTime::currentMSecsSinceEpoch();
            setAutoAttApplied(true);
        }
    } else {
        // Preamp mode fallback — From Thetis console.cs:21574-21594.
        PreampMode newMode = m_preampMode;
        switch (m_preampMode) {
        case PreampMode::Off:
        case PreampMode::On:
            newMode = PreampMode::Minus10;
            break;
        case PreampMode::Minus10:
            newMode = PreampMode::Minus20;
            break;
        case PreampMode::Minus20:
            newMode = PreampMode::Minus30;
            break;
        default:
            break;
        }
        if (newMode != m_preampMode) {
            if (m_classicSavedAttDb < 0) {
                m_classicSavedPreamp = m_preampMode;
                m_classicSavedAttDb = 0;  // sentinel: we have a saved value
            }
            m_preampMode = newMode;
            m_lastAutoAttTimeMs = QDateTime::currentMSecsSinceEpoch();
            emit preampModeChanged(m_preampMode);
            setAutoAttApplied(true);
        }
    }
}

void StepAttenuatorController::applyClassicUndo()
{
    // From Thetis console.cs:21597-21618 — timer-gated undo.
    if (!m_autoAttApplied) {
        return;
    }

    // Timer-based undo: only gate on hold period when auto-undo is enabled.
    // The explicit disable path (setAutoAttEnabled(false)) always restores.
    if (m_autoUndoEnabled) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 holdMs = static_cast<qint64>(m_autoUndoDelaySec) * 1000;
        if ((now - m_lastAutoAttTimeMs) < holdMs) {
            return;  // Hold period not elapsed.
        }
    }

    // Always restore the saved ATT/preamp value — the undo-enabled flag
    // only gates the timer-based automatic path, not explicit disable.
    if (m_stepAttEnabled && m_classicSavedAttDb >= 0) {
        if (m_classicSavedAttDb != m_attDb) {
            applyAttToHardware(m_classicSavedAttDb);
        }
    } else if (!m_stepAttEnabled && m_classicSavedAttDb >= 0) {
        if (m_classicSavedPreamp != m_preampMode) {
            m_preampMode = m_classicSavedPreamp;
            emit preampModeChanged(m_preampMode);
        }
    }

    m_classicSavedAttDb = -1;
    setAutoAttApplied(false);
}

// --- Adaptive auto-att (NereusSDR extension) ---

void StepAttenuatorController::applyAdaptiveAutoAtt(int adc)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (adc >= 0) {
        // Attack: 1 dB per tick on red overload.
        int newAtt = m_attDb + 1;
        if (newAtt > m_maxAttDb) {
            newAtt = m_maxAttDb;
        }
        if (newAtt != m_attDb) {
            applyAttToHardware(newAtt);
            m_adaptiveLastAttackMs = now;
            setAutoAttApplied(true);
        }
    } else {
        // Decay path — only if hold period elapsed since last attack.
        if ((now - m_adaptiveLastAttackMs) < m_adaptiveHoldMs) {
            return;  // Still in hold period.
        }
        if ((now - m_adaptiveLastDecayMs) < m_adaptiveDecayMs) {
            return;  // Decay rate limit.
        }

        // Decay by 1 dB toward the per-band floor.
        int floor = m_adaptiveFloorDb;
        auto it = m_bandState.find(static_cast<int>(m_currentBand));
        if (it != m_bandState.end()) {
            floor = it->second.attDb;
        }

        if (m_attDb > floor) {
            applyAttToHardware(m_attDb - 1);
            m_adaptiveLastDecayMs = now;
            if (m_attDb <= floor) {
                setAutoAttApplied(false);
            }
        } else {
            setAutoAttApplied(false);
        }
    }
}

// --- Hardware push helper ---

void StepAttenuatorController::applyAttToHardware(int dB)
{
    m_attDb = dB;
    if (m_connection) {
        m_connection->setAttenuator(dB);
    }
    emit attenuationChanged(m_attDb);
}

// --- Helpers ---

void StepAttenuatorController::setAutoAttApplied(bool applied)
{
    if (m_autoAttApplied == applied) {
        return;
    }
    m_autoAttApplied = applied;
    emit autoAttActiveChanged(applied);
}

OverloadLevel StepAttenuatorController::levelToSeverity(int level) const
{
    // From Thetis console.cs:21369/21378:
    //   level > 0 → yellow (any overload)
    //   level > 3 → red (sustained overload)
    if (level > kRedThreshold) {
        return OverloadLevel::Red;
    }
    if (level > 0) {
        return OverloadLevel::Yellow;
    }
    return OverloadLevel::None;
}

// --- RadioConnection wiring ---

void StepAttenuatorController::setRadioConnection(RadioConnection* conn)
{
    // Disconnect from old connection.
    if (m_adcOverflowConn) {
        disconnect(m_adcOverflowConn);
        m_adcOverflowConn = {};
    }

    m_connection = conn;

    if (conn) {
        m_adcOverflowConn = connect(conn, &RadioConnection::adcOverflow,
                                    this, &StepAttenuatorController::onAdcOverflow);
        m_tickTimer.start();
    } else {
        m_tickTimer.stop();
    }
}

// --- ReceiverManager wiring ---

void StepAttenuatorController::setReceiverManager(ReceiverManager* mgr)
{
    m_receiverManager = mgr;
    // ReceiverManager doesn't currently emit a ddcMappingChanged signal,
    // so checkAdcLinked() is called explicitly when mapping changes.
}

// --- ADC-linked synchronization ---

void StepAttenuatorController::checkAdcLinked()
{
    if (!m_receiverManager) {
        if (m_adcLinked) {
            m_adcLinked = false;
            emit adcLinkedChanged(false);
        }
        return;
    }

    // Compare ADC assignments for RX0 and RX1.
    ReceiverConfig cfg0 = m_receiverManager->receiverConfig(0);
    ReceiverConfig cfg1 = m_receiverManager->receiverConfig(1);

    bool linked = (cfg0.receiverIndex >= 0 && cfg1.receiverIndex >= 0 &&
                   cfg0.adcIndex == cfg1.adcIndex);

    if (linked != m_adcLinked) {
        m_adcLinked = linked;
        emit adcLinkedChanged(linked);
    }
}

void StepAttenuatorController::onDdcMappingChanged()
{
    checkAdcLinked();
}

// --- Per-MAC persistence ---

void StepAttenuatorController::saveSettings(const QString& mac)
{
    auto& s = AppSettings::instance();

    // Step attenuator global config.
    s.setHardwareValue(mac, QStringLiteral("options/stepAtt/rx1Enabled"),
                       m_stepAttEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setHardwareValue(mac, QStringLiteral("options/stepAtt/rx1Value"),
                       QString::number(m_attDb));

    // Auto-att config.
    s.setHardwareValue(mac, QStringLiteral("options/autoAtt/rx1Enabled"),
                       m_autoAttEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setHardwareValue(mac, QStringLiteral("options/autoAtt/rx1Mode"),
                       m_autoAttMode == AutoAttMode::Classic
                           ? QStringLiteral("Classic") : QStringLiteral("Adaptive"));
    s.setHardwareValue(mac, QStringLiteral("options/autoAtt/rx1Undo"),
                       m_autoUndoEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setHardwareValue(mac, QStringLiteral("options/autoAtt/rx1HoldSeconds"),
                       QString::number(m_adaptiveHoldMs / 1000));
    s.setHardwareValue(mac, QStringLiteral("options/autoAtt/rx1UndoDelaySec"),
                       QString::number(m_autoUndoDelaySec));

    // Per-band ATT values and preamp modes.
    for (int b = 0; b < static_cast<int>(Band::SwlFirst); ++b) {
        Band band = static_cast<Band>(b);
        QString key = bandKeyName(band);
        auto it = m_bandState.find(b);
        if (it != m_bandState.end()) {
            s.setHardwareValue(mac,
                QStringLiteral("options/stepAtt/rx1Band/") + key,
                QString::number(it->second.attDb));
            s.setHardwareValue(mac,
                QStringLiteral("options/preamp/rx1Band/") + key,
                QString::number(static_cast<int>(it->second.preamp)));
        }
    }

    // Save current band state (may not yet be stored in m_bandState).
    {
        QString key = bandKeyName(m_currentBand);
        s.setHardwareValue(mac,
            QStringLiteral("options/stepAtt/rx1Band/") + key,
            QString::number(m_attDb));
        s.setHardwareValue(mac,
            QStringLiteral("options/preamp/rx1Band/") + key,
            QString::number(static_cast<int>(m_preampMode)));
    }

    // Adaptive floor.
    s.setHardwareValue(mac, QStringLiteral("options/autoAtt/rx1AdaptiveFloor"),
                       QString::number(m_adaptiveFloorDb));

    // --- TX-path settings (F.2) ---
    // Keys "options/stepAtt/attOnTxEnabled" and "options/stepAtt/forceAttWhenPsOff"
    // are first introduced in F.2 (no pre-existing 3G-13/3M-0 keys at these paths).
    s.setHardwareValue(mac, QStringLiteral("options/stepAtt/attOnTxEnabled"),
                       m_attOnTxEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setHardwareValue(mac, QStringLiteral("options/stepAtt/forceAttWhenPsOff"),
                       m_forceAttWhenPsOff ? QStringLiteral("True") : QStringLiteral("False"));

    // Per-band TX ATT values.
    // Key casing ("txBand/") follows the existing RX convention used above
    // ("rx1Band/") — camelCase sub-path is the established per-controller style.
    for (int b = 0; b < static_cast<int>(Band::SwlFirst); ++b) {
        Band band = static_cast<Band>(b);
        QString key = bandKeyName(band);
        s.setHardwareValue(mac,
            QStringLiteral("options/stepAtt/txBand/") + key,
            QString::number(m_txAttByBand[static_cast<size_t>(b)]));
    }

    s.save();
}

void StepAttenuatorController::loadSettings(const QString& mac)
{
    auto& s = AppSettings::instance();

    // Step attenuator global config.
    m_stepAttEnabled = s.hardwareValue(mac, QStringLiteral("options/stepAtt/rx1Enabled"),
                                       QStringLiteral("True")).toString() == QStringLiteral("True");
    m_attDb = s.hardwareValue(mac, QStringLiteral("options/stepAtt/rx1Value"),
                              0).toInt();

    // Auto-att config.
    m_autoAttEnabled = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx1Enabled"),
                                       QStringLiteral("False")).toString() == QStringLiteral("True");
    QString modeStr = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx1Mode"),
                                      QStringLiteral("Classic")).toString();
    // P1 full-parity §4.1: clamp persisted "Adaptive" to Classic when the
    // connected board lacks per-step ATT cal support.  Handles cross-radio
    // reconnects where a user set Adaptive on a hasStepAttenuatorCal=true
    // radio, then reconnects with a different board that lacks the feature.
    m_autoAttMode = (modeStr == QStringLiteral("Adaptive") && m_hasStepAttCal)
                        ? AutoAttMode::Adaptive : AutoAttMode::Classic;
    m_autoUndoEnabled = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx1Undo"),
                                         QStringLiteral("False")).toString() == QStringLiteral("True");
    m_adaptiveHoldMs = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx1HoldSeconds"),
                                       2).toInt() * 1000;
    m_autoUndoDelaySec = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx1UndoDelaySec"),
                                         5).toInt();

    // Per-band ATT values and preamp modes.
    for (int b = 0; b < static_cast<int>(Band::SwlFirst); ++b) {
        Band band = static_cast<Band>(b);
        QString key = bandKeyName(band);

        QVariant attVal = s.hardwareValue(mac,
            QStringLiteral("options/stepAtt/rx1Band/") + key);
        QVariant preampVal = s.hardwareValue(mac,
            QStringLiteral("options/preamp/rx1Band/") + key);

        if (attVal.isValid() || preampVal.isValid()) {
            BandAttState& st = m_bandState[b];
            if (attVal.isValid()) {
                st.attDb = attVal.toInt();
            }
            if (preampVal.isValid()) {
                st.preamp = static_cast<PreampMode>(preampVal.toInt());
            }
        }
    }

    // Restore current band's ATT/preamp from per-band storage.
    auto it = m_bandState.find(static_cast<int>(m_currentBand));
    if (it != m_bandState.end()) {
        m_attDb = it->second.attDb;
        m_preampMode = it->second.preamp;
    }

    // Adaptive floor.
    m_adaptiveFloorDb = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx1AdaptiveFloor"),
                                        0).toInt();

    // --- TX-path settings (F.2) ---
    m_attOnTxEnabled = s.hardwareValue(mac, QStringLiteral("options/stepAtt/attOnTxEnabled"),
                                       QStringLiteral("True")).toString() == QStringLiteral("True");
    m_forceAttWhenPsOff = s.hardwareValue(mac, QStringLiteral("options/stepAtt/forceAttWhenPsOff"),
                                          QStringLiteral("True")).toString() == QStringLiteral("True");

    // Per-band TX ATT values.
    for (int b = 0; b < static_cast<int>(Band::SwlFirst); ++b) {
        Band band = static_cast<Band>(b);
        QString key = bandKeyName(band);
        QVariant txAttVal = s.hardwareValue(mac,
            QStringLiteral("options/stepAtt/txBand/") + key);
        if (txAttVal.isValid()) {
            m_txAttByBand[static_cast<size_t>(b)] = txAttVal.toInt();
        }
    }

    // Notify UI of restored values.
    emit attenuationChanged(m_attDb);
    emit preampModeChanged(m_preampMode);
}

}  // namespace NereusSDR
