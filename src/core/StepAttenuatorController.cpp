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
//   2026-05-03 — Added setAttOnTxValue / attOnTxValue setter pair (Phase 1
//                 Agent 1D of #167) — the ATT-on-TX-on-power-change safety
//                 gate used by TransmitModel::setPowerUsingTargetDbm
//                 (Thetis console.cs:46740-46748 [v2.10.3.13]).  Mirrors
//                 Thetis SetupForm.ATTOnTX (mi0bot setup.cs:3988-4017
//                 [v2.10.3.13]); range clamped to [m_minAttDb, 31].
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23: R-R3-46 / R-R3-11 / R-R3-13: change signals for every
//                 setting, settingsReloaded(), the opt-in debounced save and
//                 the RX1 preamp.  NereusSDR-original; no new Thetis logic.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23: R-R3-46 fix wave: loadSettings clears the band memory,
//                 setBand stores the old band only once a radio is loaded,
//                 restored attenuation stays within the radio's range, and
//                 the ATT-on-TX value follows its own transmit band.
//                 2026-09-24: before a radio loads, setBand only notes the
//                 band, and markSettingsUnloaded drops the band memory.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25: R-R3-49 (group A fix wave, M6): ATT on TX, its value and
//                 Force ATT schedule the Core's debounced save.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: A11 / R-R3-49 (parity Task 31): the display's TX attenuator
//                 offset follows every TX step attenuation applied, as
//                 Thetis sets Display.TXAttenuatorOffset beside each
//                 NetworkIO.SetTxAttenData. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: the ten Thetis preamp modes (SA_MINUS10/20/30
//                added), setBoardIdentity and the once-per-radio move of
//                stored preamp modes to that numbering. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: each preamp mode drives the step attenuator,
//                the preamp bit and the Alex attenuator (console.cs:
//                19218-19330 [v2.10.3.15]); the step attenuator enable
//                switches between the two (console.cs:10952-10972); above
//                31 dB an Alex board takes the Alex attenuator and the
//                value + 2 (console.cs:11027-11065); the HPSDR's MOX
//                preamp is HPSDR_OFF. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - Level Cal: classic auto-att steps the preamp to
//                SA_MINUS10/20/30 and each step and its undo drive the
//                radio (console.cs:21611-21651 [v2.10.3.15]). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal fix wave: RX2's own preamp mode with its band
//                memory and drive (console.cs:19413-19520 [v2.10.3.15]),
//                and the HPSDR MOX path turns RX1's step attenuator off and
//                holds RX2's mode (console.cs:29598-29608, 29688-29692).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Level Cal 2 review: RX2's own attenuator is held to the
//                second ADC's 0-31 dB field (kRx2StepAttMaxDb,
//                rx2MaxAttenuation), so no RX2 value wraps on the wire.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
#include "BoardCapabilities.h"
#include "RadioConnection.h"
#include "P2RadioConnection.h"
#include "ReceiverManager.h"
#include "WdspTypes.h"

#include <algorithm>

#include <QDateTime>
#include <QMetaObject>

namespace NereusSDR {

namespace {
// options/preamp/modeVersion: 2 once the stored preamp modes use the ten
// Thetis modes (SA_MINUS10..30 as 7..9). Absent reads as 1.
constexpr int kPreampModeVersion = 2;
} // namespace

StepAttenuatorController::StepAttenuatorController(QObject* parent)
    : QObject(parent)
{
    m_tickTimer.setInterval(kTickIntervalMs);
    connect(&m_tickTimer, &QTimer::timeout, this, &StepAttenuatorController::tick);
    m_tickTimer.start();

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDebounceMs);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() {
        saveSettings(m_loadedMac);
    });
}

// --- Opt-in debounced save (R-R3-46 / R-R3-11) ---

void StepAttenuatorController::setDebouncedSaveEnabled(bool on)
{
    m_debouncedSave = on;
    if (!on) {
        m_saveTimer.stop();
    }
}

void StepAttenuatorController::scheduleSave()
{
    // A value moved during TX is the TX window's, not the operator's RX
    // choice; the TX->RX restore and teardown save cover it.
    if (!m_debouncedSave || m_loadedMac.isEmpty() || m_isMox) {
        return;
    }
    m_saveTimer.start();
}

void StepAttenuatorController::flushPendingSave()
{
    if (!m_saveTimer.isActive()) {
        return;
    }
    m_saveTimer.stop();
    saveSettings(m_loadedMac);
}

void StepAttenuatorController::setRx1Preamp(bool on)
{
    if (m_rx1Preamp == on) {
        return;
    }
    m_rx1Preamp = on;
    // Same route as the local RX applet's toggle (RxApplet.cpp, Phase 3P-B
    // Task 10): P2RadioConnection::setRx1Preamp on the connection thread.
    // Only P2 dual-ADC boards have it; any other connection ignores it.
    if (auto* conn = qobject_cast<P2RadioConnection*>(m_connection.get())) {
        QMetaObject::invokeMethod(conn, [conn, on]() {
            conn->setRx1Preamp(on);
        });
    }
    emit rx1PreampChanged(on);
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
    if (m_autoAttEnabled == on) {
        return;
    }
    m_autoAttEnabled = on;
    if (!on && m_autoAttApplied) {
        // Clear auto-att state when disabled.
        // From Thetis console.cs:21509-21514: clear stack when auto-att off.
        applyClassicUndo();
    }
    // RxApplet listens here to flip S-ATT ↔ A-ATT on HL2.  See
    // mi0bot-Thetis console.cs:21342-21365 [v2.10.3.13-beta2] AutoAttRX1
    // setter — same convention, same label.
    emit autoAttEnabledChanged(on);
    scheduleSave();
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
    if (m_autoAttMode == mode) {
        return;
    }
    m_autoAttMode = mode;
    emit autoAttModeChanged(mode);
    scheduleSave();
}

void StepAttenuatorController::setAutoAttUndo(bool on)
{
    if (m_autoUndoEnabled == on) {
        return;
    }
    m_autoUndoEnabled = on;
    emit autoAttUndoChanged(on);
    scheduleSave();
}

void StepAttenuatorController::setAutoUndoDelaySec(int sec)
{
    if (m_autoUndoDelaySec == sec) {
        return;
    }
    m_autoUndoDelaySec = sec;
    emit autoUndoDelayChanged(sec);
    scheduleSave();
}

void StepAttenuatorController::setAutoAttHoldSeconds(double sec)
{
    const int ms = static_cast<int>(sec * 1000.0);
    if (m_adaptiveHoldMs == ms) {
        return;
    }
    m_adaptiveHoldMs = ms;
    emit autoAttHoldChanged(ms);
    scheduleSave();
}

void StepAttenuatorController::setAdaptiveDecayMs(int ms)
{
    m_adaptiveDecayMs = ms;
}

void StepAttenuatorController::setStepAttEnabled(bool on)
{
    if (m_stepAttEnabled != on) {
        m_stepAttEnabled = on;
        // Level Cal: turning the step attenuator on sends its value, off
        // sends the preamp mode's drive and its Alex bits.
        // From Thetis console.cs:10959-10972 [v2.10.3.15] (RX1StepAttEnabled):
        //   if (_rx1_step_att_enabled)
        //   {
        //       udRX1StepAttData.Value = validateRX1StepAttData(getRX1stepAttenuatorForBand(rx1_band)); //MW0LGE [2.10.3.6] added //[2.10.3.9]MW0LGE validated
        //       udRX1StepAttData_ValueChanged(this, EventArgs.Empty);
        //   }
        //   else
        //   {
        //       comboPreamp_SelectedIndexChanged(this, EventArgs.Empty);
        //
        //       if (alexpresent)
        //           NetworkIO.SetAlexAtten(alex_atten); // normal up alex attenuator setting
        //   }
        // (applyPreampDrive sends the Alex bits itself. m_attDb already
        // holds the band's value. Nothing is sent before a radio's settings
        // load, or while keyed, when the TX values are on the wire.)
        if (m_boardKnown && m_connection && !m_isMox && !m_loadedMac.isEmpty()) {
            if (on) {
                sendRx1Attenuation(m_attDb);
            } else {
                applyPreampDrive();
            }
        }
        emit stepAttEnabledChanged(on);
        scheduleSave();
    }
    // R-R3-46 / R-R3-11: on one ADC RX2's enable is RX1's (Thetis Setup's
    // mirror, setup.cs 15791-15793 [v2.10.3.15]).
    if (!rx2OnItsOwnAdc() && m_rx2StepAttEnabled != on) {
        m_rx2StepAttEnabled = on;
        emit rx2StepAttEnabledChanged(on);
        scheduleSave();
    }
}

void StepAttenuatorController::setBand(Band band)
{
    m_txBand = band;
    if (m_currentBand == band) {
        return;
    }

    // R-R3-46: before a radio's settings are loaded (the first connect,
    // or a new radio between the old one's teardown and this one's load)
    // the band is only noted. The values in hand are not what the operator
    // used on the band left, and the band memory is not this radio's, so
    // nothing is stored, restored or sent; loadSettings then restores the
    // noted band for the radio.
    if (m_loadedMac.isEmpty()) {
        m_currentBand = band;
        return;
    }

    // Save current ATT/preamp to old band.
    m_bandState[static_cast<int>(m_currentBand)] = { m_attDb, m_preampMode };

    m_currentBand = band;

    // Restore ATT/preamp from new band (if any stored).
    //
    // R-R3-46: with setBandRestoreToRadio(true) (the Core) the restored
    // values also go to the radio, as in Thetis's RX1Band setter:
    // From Thetis console.cs:17325 [v2.10.3.15]:
    //   SetupForm.ATTOnTX = getTXstepAttenuatorForBand(_tx_band); //[2.10.3.6]MW0LGE att_fixes
    //   RX1PreampMode = rx1_preamp_by_band[(int)rx1_band];
    //   RX1AttenuatorData = getRX1stepAttenuatorForBand(rx1_band);
    //   //[2.10.3.6]MW0LGE this tmp is needed because RX1AGCMode causes an update to the setup form
    // (RX1PreampMode and RX1AttenuatorData each send their value to the
    // radio.)  NereusSDR's ATT-on-TX side is handled on MOX, not here.
    auto it = m_bandState.find(static_cast<int>(band));
    if (it != m_bandState.end()) {
        // R-R3-46: within this radio's range (a band remembered at 45 dB
        // reads 31 on a radio that stops at 31, as the radio clamps it).
        const int restoredDb = std::clamp(it->second.attDb, m_minAttDb, m_maxAttDb);
        if (restoredDb != m_attDb) {
            m_attDb = restoredDb;
            if (m_bandRestoreToRadio && !m_isMox) {
                // R-R3-46 / R-R3-11: to slice A's ADC (and the linked one).
                sendRx1Attenuation(m_attDb);
            }
            emit attenuationChanged(m_attDb);
        }
        const PreampMode restoredPreamp = clampPreampForBoard(it->second.preamp);
        if (restoredPreamp != m_preampMode) {
            m_preampMode = restoredPreamp;
            if (m_bandRestoreToRadio && !m_isMox) {
                // Level Cal: the mode's whole drive, as RX1PreampMode sends.
                applyPreampDrive();
            }
            emit preampModeChanged(m_preampMode);
        }
    }

    // Clear auto-att state on band change.
    // From Thetis console.cs:21526-21529: keep_att_entries_for_band.
    if (m_autoAttApplied) {
        setAutoAttApplied(false);
        m_classicSavedAttDb = -1;
    }

    // The band just left keeps the value it had; persist it.
    scheduleSave();
}

void StepAttenuatorController::setAttenuation(int dB, int rx)
{
    // Clamp to the active board's signed range.  HL2 advertises
    // [m_minAttDb=-28, m_maxAttDb=+31] (issue #175 follow-up — capped at
    // +31 per maintainer approval; mi0bot's +32 widening at
    // console.cs:11043 [v2.10.3.13-beta2] is an off-by-one upstream bug).
    // Legacy ANAN/Hermes boards keep m_minAttDb=0
    // so behaviour is unchanged for them.  Without honouring m_minAttDb
    // here, any negative HL2 value selected in the RX UI gets snapped
    // back to 0 before reaching P1RadioConnection — Codex P1 in PR #157.
    if (dB < m_minAttDb) { dB = m_minAttDb; }
    if (dB > m_maxAttDb) { dB = m_maxAttDb; }
    if (m_attDb == dB) { return; }
    m_attDb = dB;

    // Send to hardware — from Thetis console.cs RX1AttenuatorData property.
    //
    // v0.4.1 hotfix — marshal across the controller↔connection thread
    // boundary via QMetaObject::invokeMethod so the connection-thread
    // state mutation (m_stepAttn[0] write + m_forceBank11Next flush
    // flag) happens on the connection thread.  Mirrors the established
    // pattern used by the four setTxStepAttenuation call sites in this
    // file (lines 364, 469, 535, 557); without this, the call ran
    // directly on the controller thread and bypassed Qt's queued
    // dispatch — a latent thread-safety hazard that surfaces as
    // delayed / paired-with-stale-reads on weaker memory models
    // (ARM64).
    //
    // R-R3-46 / R-R3-11: to the ADC slice A is on, as Thetis sends RX1's
    // value to nRX1ADCinUse, and to the other ADC too while diversity links
    // them (sendRx1Attenuation).
    sendRx1Attenuation(dB);

    emit attenuationChanged(m_attDb);

    // Two receivers in linked diversity keep one value: the other ADC's own
    // value follows RX1's, as Thetis sets RX2AttenuatorData from RX1's.
    // From Thetis console.cs:11080-11086 [v2.10.3.15]:
    //   bool bRX1RX2diversity = m_bDiversityAttLinkForRX1andRX2 && (diversityForm != null && Diversity2 && diversityForm.EXTDIVOutput == 2); // if using diversity, and both rx's are linked, then we need to attenuate both
    //   if (((nRX1ADCinUse == nRX2ADCinUse) || bRX1RX2diversity) && RX2AttenuatorData != _rx1_attenuator_data)
    //   {
    //       _setFromOtherAttenuator = true;
    //       if (SetupForm.RX2EnableAtt != SetupForm.RX1EnableAtt) SetupForm.RX2EnableAtt = SetupForm.RX1EnableAtt;
    //       RX2AttenuatorData = _rx1_attenuator_data;
    //       _setFromOtherAttenuator = false;
    // (NereusSDR keeps one enable for both, so the enable line has nothing
    // to copy. Receivers on one ADC need no copy: they read one value.)
    if (m_adcAttLinked && !m_isMox && m_rx2AttDb != m_attDb) {
        m_rx2AttDb = m_attDb;
        m_rx2BandAttDb[static_cast<int>(m_rx2Band)] = m_rx2AttDb;
        emit rx2AttenuationChanged(m_rx2AttDb);
    }

    // Issue #200 fix — mirror live RX value into the per-band slot when not
    // in MOX, so the band-state stays coherent with m_attDb.  Without this,
    // the user's spinbox change diverges from m_bandState[currentBand], and
    // the TX→RX restore path's "defensive" resync could snap m_attDb back
    // to the stale slot value (often 0 from a never-touched persisted slot).
    // From Thetis console.cs:11050-11051 [v2.10.3.13]:
    //   if (!_mox) //[2.10.3.9]MW0LGE note, this is not technically required
    //              //  for all radios except for the RedPitaya. See BODGE
    //       setRX1stepAttenuatorForBand(rx1_band, _rx1_attenuator_data);
    if (!m_isMox) {
        m_bandState[static_cast<int>(m_currentBand)].attDb = m_attDb;
    }
    scheduleSave();

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
    // Level Cal: a board without Alex takes Off for the Alex settings.
    mode = clampPreampForBoard(mode);
    if (m_preampMode == mode) { return; }
    m_preampMode = mode;

    // Send to hardware: Thetis's RX1PreampMode setter, which
    // comboPreamp_SelectedIndexChanged calls (applyPreampDrive). Each send
    // is marshalled to the connection thread (v0.4.1 hotfix).
    applyPreampDrive();

    emit preampModeChanged(m_preampMode);
    scheduleSave();

    // Level Cal: receivers on one ADC, or linked in diversity, keep one
    // mode, so RX2's follows.
    // From Thetis console.cs:19384-19394 [v2.10.3.15] (RX1PreampMode):
    //   if (!_mox && !_setFromOtherAttenuator)
    //   {
    //       bool bRX1RX2diversity = m_bDiversityAttLinkForRX1andRX2 && (diversityForm != null && Diversity2 && diversityForm.EXTDIVOutput == 2); // if using diversity, and both rx's are linked, then we need to attenuate both
    //       if (((nRX1ADCinUse == nRX2ADCinUse) || bRX1RX2diversity) && RX2PreampMode != rx1_preamp_mode)
    //       {
    //           _setFromOtherAttenuator = true;
    //           if (SetupForm.RX2EnableAtt != SetupForm.RX1EnableAtt) SetupForm.RX2EnableAtt = SetupForm.RX1EnableAtt;
    //           RX2PreampMode = rx1_preamp_mode;
    //           _setFromOtherAttenuator = false;
    //       }
    //   }
    // (The enables are already one while RX2 shares the ADC: setStepAttEnabled.)
    if (!m_isMox && !m_setFromOtherPreamp && !rx2OnItsOwnAdc() && m_rx2PreampMode != m_preampMode) {
        m_setFromOtherPreamp = true;
        setRx2PreampMode(m_preampMode);
        m_setFromOtherPreamp = false;
    }
}

// --- Level Cal: RX2's preamp mode ---

StepAttenuatorController::Rx2PreampDrive
StepAttenuatorController::rx2PreampDriveFor(PreampMode mode) noexcept
{
    // From Thetis console.cs:19431-19479 [v2.10.3.15] (RX2PreampMode):
    //   int rx2_preamp = 0;
    //   int rx2_att_value = 0;
    //
    //   //MW0LGE_22b
    //   _from_preampmode[1] = true;
    //   switch (rx2_preamp_mode)
    //   {
    //       case PreampMode.HPSDR_ON:  //0dB
    //           rx2_att_value = 0;
    //           rx2_preamp = 1; //no attn
    //       case PreampMode.HPSDR_OFF: //-20dB
    //           rx2_att_value = 20;
    //           rx2_preamp = 0; //attn inline
    //       case PreampMode.HPSDR_MINUS10:
    //           rx2_att_value = 10;
    //           rx2_preamp = 1;
    //           comboRX2Preamp.Text = "-10db"; //MW0LGE_22b lower
    //       case PreampMode.HPSDR_MINUS20:
    //           rx2_att_value = 20;
    //           rx2_preamp = 1;
    //           comboRX2Preamp.Text = "-20db";  //MW0LGE_22b lower
    //       case PreampMode.HPSDR_MINUS30:
    //           rx2_att_value = 30;
    //           rx2_preamp = 1;
    //           comboRX2Preamp.Text = "-30db";  //MW0LGE_22b lower
    //       SA_MINUS10 10/0, SA_MINUS20 20/0, SA_MINUS30 30/0
    // HPSDR_MINUS40 and HPSDR_MINUS50 have no case and keep the 0/0 the
    // switch starts from.
    switch (mode) {
    case PreampMode::On:        return {0, true};    //0dB
    case PreampMode::Off:       return {20, false};  //-20dB
    case PreampMode::Minus10:   return {10, true};
    case PreampMode::Minus20:   return {20, true};
    case PreampMode::Minus30:   return {30, true};
    case PreampMode::SaMinus10: return {10, false};
    case PreampMode::SaMinus20: return {20, false};
    case PreampMode::SaMinus30: return {30, false};
    case PreampMode::Minus40:
    case PreampMode::Minus50:
        break;
    }
    return {0, false};
}

bool StepAttenuatorController::rx2PreampDrivesAdc() const noexcept
{
    // From Thetis console.cs:19488-19497 [v2.10.3.15]:
    //   if (!_rx2_step_att_enabled && (HardwareSpecific.Model == HPSDRModel.ANAN100D ||  //MW0LGE_22b we dont want to do this if we are using SA
    //       HardwareSpecific.Model == HPSDRModel.ANAN200D ||
    //       HardwareSpecific.Model == HPSDRModel.ORIONMKII ||
    //       HardwareSpecific.Model == HPSDRModel.ANAN7000D ||
    //       HardwareSpecific.Model == HPSDRModel.ANAN8000D ||
    //       HardwareSpecific.Model == HPSDRModel.ANAN_G2E ||  //N1GP G2E added
    //       HardwareSpecific.Model == HPSDRModel.ANAN_G2 ||
    //       HardwareSpecific.Model == HPSDRModel.ANAN_G2_1K ||
    //       HardwareSpecific.Model == HPSDRModel.ANVELINAPRO3 ||
    //       HardwareSpecific.Model == HPSDRModel.REDPITAYA)) //DH1KLM
    if (!m_boardKnown) {
        return false;
    }
    switch (m_hpsdrModel) {
    case HPSDRModel::ANAN100D:
    case HPSDRModel::ANAN200D:
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANAN8000D:
    case HPSDRModel::ANAN_G2E:  //N1GP G2E added
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::REDPITAYA: //DH1KLM
        return true;
    default:
        return false;
    }
}

bool StepAttenuatorController::rx2PreampPresent() const noexcept
{
    // From Thetis console.cs:14788-14854 [v2.10.3.15] (SetupForHPSDRModel):
    // _rx2_preamp_present = true for ANAN100D, ANAN200D, ORIONMKII,
    // ANAN7000D, ANAN8000D, ANAN_G2, ANAN_G2_1K, ANVELINAPRO3 and
    // REDPITAYA (//DH1KLM); false for HERMES, ANAN10, ANAN10E, ANAN100,
    // ANAN100B and ANAN_G2E (//N1GP G2E added); any other model keeps the
    // field's initial false (console.cs:15068).
    //   case HPSDRModel.ANAN_G2_1K:                          // G8NJJ: likely to need further changes for PA
    //   RX2PreampPresent = _rx2_preamp_present; //[2.10.3.11]MW0LGE we were setting the member var above, but this was not actually having any effect/update
    if (!m_boardKnown) {
        return false;
    }
    switch (m_hpsdrModel) {
    case HPSDRModel::ANAN100D:
    case HPSDRModel::ANAN200D:
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANAN8000D:
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::REDPITAYA: //DH1KLM
        return true;
    default:
        return false;
    }
}

void StepAttenuatorController::applyRx2PreampDrive()
{
    if (!m_connection || !m_boardKnown) {
        return;
    }
    const Rx2PreampDrive drive = rx2PreampDriveFor(m_rx2PreampMode);
    // From Thetis console.cs:19491-19505 [v2.10.3.15]:
    //       HardwareSpecific.Model == HPSDRModel.ANAN_G2E ||  //N1GP G2E added
    //       ...
    //       HardwareSpecific.Model == HPSDRModel.REDPITAYA)) //DH1KLM
    //   {
    //       if (nRX2ADCinUse == 0) NetworkIO.SetADC1StepAttenData(rx2_att_value);
    //       else if (nRX2ADCinUse == 1) NetworkIO.SetADC2StepAttenData(rx2_att_value);
    //       else if (nRX2ADCinUse == 2) NetworkIO.SetADC3StepAttenData(rx2_att_value);
    //   }
    //
    //   if (HardwareSpecific.Model == HPSDRModel.HPSDR)
    //       NetworkIO.SetRX2Preamp(rx2_preamp);
    // RX2's ADC is the other ADC in use; while every slice is on slice A's
    // ADC there is none, and RX1's mode, which RX2's follows, drives it.
    if (!m_rx2StepAttEnabled && rx2PreampDrivesAdc() && m_rx2Adc >= 0 && m_rx2Adc != m_rx1Adc) {
        sendAttenuatorToAdc(m_rx2Adc, drive.attDb);
    }
    if (isHpsdrModel()) {
        RadioConnection* conn = m_connection.get();
        const bool bit = drive.preamp;
        QMetaObject::invokeMethod(conn, [conn, bit]() {
            conn->setRx2Preamp(bit);
        });
    }
}

void StepAttenuatorController::setRx2PreampMode(PreampMode mode)
{
    if (m_rx2PreampMode == mode) {
        return;
    }
    m_rx2PreampMode = mode;
    applyRx2PreampDrive();
    // From Thetis console.cs:19507 [v2.10.3.15]:
    //   rx2_preamp_by_band[(int)rx2_band] = rx2_preamp_mode;
    m_rx2BandPreamp[static_cast<int>(m_rx2Band)] = m_rx2PreampMode;
    emit rx2PreampModeChanged(m_rx2PreampMode);
    scheduleSave();

    // From Thetis console.cs:19509-19519 [v2.10.3.15]:
    //   if (!_mox && !_setFromOtherAttenuator)
    //   {
    //       bool bRX1RX2diversity = m_bDiversityAttLinkForRX1andRX2 && (diversityForm != null && Diversity2 && diversityForm.EXTDIVOutput == 2); // if using diversity, and both rx's are linked, then we need to attenuate both
    //       if (((nRX1ADCinUse == nRX2ADCinUse) || bRX1RX2diversity) && RX1PreampMode != rx2_preamp_mode)
    //       {
    //           _setFromOtherAttenuator = true;
    //           if (SetupForm.RX1EnableAtt != SetupForm.RX2EnableAtt) SetupForm.RX1EnableAtt = SetupForm.RX2EnableAtt;
    //           RX1PreampMode = rx2_preamp_mode;
    //           _setFromOtherAttenuator = false;
    //       }
    //   }
    if (!m_isMox && !m_setFromOtherPreamp && !rx2OnItsOwnAdc() && m_preampMode != m_rx2PreampMode) {
        m_setFromOtherPreamp = true;
        setPreampMode(m_rx2PreampMode);
        m_setFromOtherPreamp = false;
    }
}

void StepAttenuatorController::setMaxAttenuation(int dB)
{
    m_rawMaxAttDb = dB;
    recomputeMaxAtt();
}

void StepAttenuatorController::setMinAttenuation(int dB)
{
    if (m_minAttDb == dB) {
        return;
    }
    m_minAttDb = dB;
    emit attenuationRangeChanged(m_minAttDb, m_maxAttDb);
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
    // 2 m keeps its own slot (per-band state slots, Band.h).
    int idx = perBandStateSlot(band);
    if (idx < 0) { return; }
    if (dB < 0)            { dB = 0; }
    if (dB > m_maxAttDb)   { dB = m_maxAttDb; }
    m_txAttByBand[static_cast<size_t>(idx)] = dB;
}

int StepAttenuatorController::txAttenuationForBand(Band band) const
{
    return applyTxAttenuationForBand(band);
}

// --- ATT-on-TX scalar setter / getter (Phase 1 Agent 1D of #167) ---
// From Thetis SetupForm.ATTOnTX setter (mi0bot setup.cs:3988-4017
// [v2.10.3.13] - HL2 fork widens negative range to -28):
//   public int ATTOnTX
//   { ...
//       if (value > 31) value = 31;
//       if (HPSDRModel.HERMESLITE == HardwareSpecific.Model)
//       { if (value < -28) value = -28; } //MI0BOT: HL2 has a greater range and can go negative
//       else
//       { if (value < 0) value = 0; } //MW0LGE [2.9.0.7] added after mi0bot source review
//       ...
//   }
//
// The Setup-form spinbox setter ultimately calls into console.TxAttenData
// (setup.cs:17240 [v2.10.3.13]:
//   console.TxAttenData = (int)udATTOnTX.Value;
// ), which writes the validated value into the per-band TX storage and
// (when m_bATTonTX is true) pushes it to hardware:
//   //[2.10.3.6]MW0LGE att_fixes #399
//   setTXstepAttenuatorForBand(_tx_band, _tx_attenuator_data);
//   if (m_bATTonTX) NetworkIO.SetTxAttenData(_tx_attenuator_data); //[2.10.3.6]MW0LGE att_fixes
//   else            NetworkIO.SetTxAttenData(0);
//
// We mirror the Thetis chain: clamp, write to current band slot, push to
// hardware when ATT-on-TX is enabled.

void StepAttenuatorController::setAttOnTxValue(int dB)
{
    // From Thetis (mi0bot) setup.cs:3999 [v2.10.3.13]:
    //   if (value > 31) value = 31;
    if (dB > 31) { dB = 31; }

    // From Thetis (mi0bot) setup.cs:4001-4008 [v2.10.3.13]:
    //   if (HPSDRModel.HERMESLITE) { if (value < -28) value = -28; } //MI0BOT
    //   else                       { if (value < 0)   value = 0;   } //MW0LGE [2.9.0.7]
    // NereusSDR: m_minAttDb encodes the per-board lower bound (HL2: -28
    // via setMinAttenuation(-28) on connect; legacy boards: 0).
    if (dB < m_minAttDb) { dB = m_minAttDb; }

    // From Thetis console.cs:10612 TxAttenData setter [v2.10.3.13]
    //   //[2.10.3.6]MW0LGE att_fixes #399
    //   setTXstepAttenuatorForBand(_tx_band, _tx_attenuator_data);
    // Write directly to the per-band array — the public setter
    // setTxAttenuationForBand() hard-clamps negatives to 0, which would
    // strip the HL2 signed range.  Bypassing the clamp here is correct:
    // the value above is already validated against [m_minAttDb, 31].
    int idx = perBandStateSlot(m_txBand);
    int oldValue = 0;
    if (idx >= 0) {
        oldValue = m_txAttByBand[static_cast<size_t>(idx)];
        m_txAttByBand[static_cast<size_t>(idx)] = dB;
    }

    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): emit attOnTxValueChanged so
    // the Setup → Transmit → Power udATTOnTX spinbox (and any other UI
    // surface) mirrors AutoAtt's adjustments + user-side changes.  Old/new
    // guard prevents a feedback loop when the spinbox writes back to its
    // own bound setter.
    if (dB != oldValue) {
        emit attOnTxValueChanged(dB);
        // R-R3-49 (group A fix wave, M6): the Core saves the operator's
        // change at once (scheduleSave skips a move made while keyed, such
        // as PureSignal's auto-attenuate).
        scheduleSave();
    }

    // From Thetis console.cs:10613-10622 TxAttenData setter [v2.10.3.13]:
    //   if (m_bATTonTX) {
    //       NetworkIO.SetTxAttenData(_tx_attenuator_data); //[2.10.3.6]MW0LGE att_fixes
    //       Display.TXAttenuatorOffset = _tx_attenuator_data; //[2.10.3.6]MW0LGE att_fixes
    //   } else {
    //       NetworkIO.SetTxAttenData(0);
    //       Display.TXAttenuatorOffset = 0;
    //   }
    // Hardware push only when ATT-on-TX is enabled.  When disabled, the
    // per-band slot is still recorded (so re-enabling later picks up the
    // user's preference) but no wire push occurs.
    if (m_attOnTxEnabled && m_connection) {
        RadioConnection* conn = m_connection.get();
        const int dBcopy = dB;
        QMetaObject::invokeMethod(conn, [conn, dBcopy]() {
            conn->setTxStepAttenuation(dBcopy); //[2.10.3.6]MW0LGE att_fixes
        });
    }
    // Display.TXAttenuatorOffset = _tx_attenuator_data (or 0), as quoted
    // above (console.cs:10620-10626 [v2.10.3.15]).
    setTxAttenuatorOffset(m_attOnTxEnabled ? dB : 0); //[2.10.3.6]MW0LGE att_fixes
#ifdef NEREUS_BUILD_TESTS
    if (m_attOnTxEnabled) {
        m_lastTxStepAttDb = dB;
    }
#endif
}

void StepAttenuatorController::setAttOnTxEnabled(bool on)
{
    // From Thetis console.cs:19071-19094 [v2.10.3.15] ATTOnTX setter:
    //   if (!value && _auto_attTX_when_not_in_ps) return; // ignore in this case
    //   m_bATTonTX = value;
    //   updateAttNudsCombos();
    //   if (PowerOn) {
    //       if (m_bATTonTX) {
    //           int txatt = getTXstepAttenuatorForBand(_tx_band);
    //           NetworkIO.SetTxAttenData(txatt); //[2.10.3.6]MW0LGE att_fixes
    //           Display.TXAttenuatorOffset = txatt; //[2.10.3.6]MW0LGE att_fixes
    //       } else {
    //           NetworkIO.SetTxAttenData(0);
    //           Display.TXAttenuatorOffset = 0;
    //       }
    //   }
    // NereusSDR has no _auto_attTX_when_not_in_ps option, so the early
    // return has nothing to test. The HL2 "31 - txatt" wire form
    // (mi0bot console.cs:19164-19167 [v2.10.3.13-beta2] // MI0BOT: Greater
    // range for HL2) lives in the HL2 codec's setTxStepAttenuation.
    if (m_attOnTxEnabled == on) { return; }
    m_attOnTxEnabled = on;
    emit attOnTxEnabledChanged(on);
    scheduleSave();  // R-R3-49 (group A fix wave, M6): saved at once on the Core

    // updateAttNudsCombos(): while keyed Thetis shows udTXStepAttData over
    // the receive attenuator only when m_bATTonTX is set
    // (console.cs:29287-29291 [v2.10.3.15]). NereusSDR has one bound S-ATT
    // readout, so it swaps the value the same way onMoxHardwareFlipped does.
    // The HPSDR board keys through the preamp path instead and keeps it.
    const int txAtt = m_attOnTxEnabled ? applyTxAttenuationForBand(m_txBand) : 0;
    if (m_isMox && !m_isHpsdrBoard) {
        if (m_attOnTxEnabled) {
            if (!m_savedRxAttDbValid) {
                m_savedRxAttDbForTx = m_attDb;
                m_savedRxAttDbValid = true;
            }
            if (m_attDb != txAtt) {
                m_attDb = txAtt;
                emit attenuationChanged(txAtt);
            }
        } else if (m_savedRxAttDbValid) {
            m_savedRxAttDbValid = false;
            if (m_attDb != m_savedRxAttDbForTx) {
                m_attDb = m_savedRxAttDbForTx;
                emit attenuationChanged(m_attDb);
            }
        }
    }

    // if (PowerOn): the radio is connected.
    if (m_connection) {
        RadioConnection* conn = m_connection.get();
        QMetaObject::invokeMethod(conn, [conn, txAtt]() {
            conn->setTxStepAttenuation(txAtt); //[2.10.3.6]MW0LGE att_fixes
        });
        setTxAttenuatorOffset(txAtt); //[2.10.3.6]MW0LGE att_fixes
#ifdef NEREUS_BUILD_TESTS
        m_lastTxStepAttDb = txAtt;
#endif
    }
}

void StepAttenuatorController::setTxAttenuatorOffset(int dB)
{
    // From Thetis display.cs:1365-1370 [v2.10.3.15]:
    //   private static float tx_attenuator_offset = 0.0f;
    //   public static float TXAttenuatorOffset { get; set; }
    if (m_txAttOffsetDb == dB) {
        return;
    }
    m_txAttOffsetDb = dB;
    emit txAttenuatorOffsetChanged(dB);
}

int StepAttenuatorController::attOnTxValue() const
{
    // Mirrors Thetis ATTOnTX getter (mi0bot setup.cs:3990-3994 [v2.10.3.13]):
    //   get { if (udATTOnTX != null) return (int)udATTOnTX.Value;
    //         else return -1; }
    // NereusSDR: return the current band's stored TX ATT value (the
    // setter wrote the same value here on its last invocation).
    return applyTxAttenuationForBand(m_txBand);
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
    int idx = perBandStateSlot(band);
    if (idx < 0) { return 0; }
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
    // Mirror MOX state for auto-att gating in tick().  Set BEFORE the path
    // branches below so a tick() racing this slot reads the new value.
    m_isMox = isTx;

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
            setTxAttenuatorOffset(0); //[2.10.3.6]MW0LGE att_fixes
#ifdef NEREUS_BUILD_TESTS
            m_lastTxStepAttDb = 0;
#endif
            return;
        }

        if (m_isHpsdrBoard) {
            // HPSDR variant: save preamp mode, turn RX1's step attenuator
            // off, then force PreampMode::Off (Thetis PreampMode.HPSDR_OFF,
            // -20 dB), and the same for RX2's mode where the model has it.
            // From Thetis console.cs:29599-29608 [v2.10.3.15]:
            // [original inline comment from console.cs:29612, on the else
            // branch below]
            //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
            //   if (HardwareSpecific.Model == HPSDRModel.HPSDR)
            //   {
            //       temp_mode = RX1PreampMode;
            //       SetupForm.RX1EnableAtt = false;
            //       RX1PreampMode = PreampMode.HPSDR_OFF;			// set to -20dB
            //       if (_rx2_preamp_present)
            //       {
            //           temp_mode2 = RX2PreampMode;
            //           RX2PreampMode = PreampMode.HPSDR_OFF;
            //       }
            //   }
            // Level Cal: PreampMode::Off is HPSDR_OFF now that the ten
            // Thetis modes are held (Minus20 is HPSDR_MINUS20).
            // Thetis leaves RX1EnableAtt off at the unkey; so does this.
            saveRxPreampMode();
            setStepAttEnabled(false);
            setPreampMode(PreampMode::Off);			// set to -20dB
            if (rx2PreampPresent()) {
                m_savedRx2PreampMode = m_rx2PreampMode;
                setRx2PreampMode(PreampMode::Off);
            }
        } else {
            // Non-HPSDR standard board: TX ATT lookup + force-31 override.
            // From Thetis console.cs:29562-29568 [v2.10.3.13]:
            //   int txAtt = getTXstepAttenuatorForBand(_tx_band);
            //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps
            //   if ((!chkFWCATUBypass.Checked && _forceATTwhenPSAoff) ||
            //       (CWL || CWU)) txAtt = 31; // reset when PS is OFF or in CW mode
            //   SetupForm.ATTOnRX1 = getRX1stepAttenuatorForBand(rx1_band); //[2.10.3.6]MW0LGE att_fixes
            //   SetupForm.ATTOnTX = txAtt; //[2.10.3.6]MW0LGE att_fixes NOTE: this will eventually call Display.TXAttenuatorOffset with the value
            int txAtt = applyTxAttenuationForBand(m_txBand);
            const bool psOff = !m_psActive;
            if (shouldForce31Db(m_currentDspMode, psOff)) {
                txAtt = 31; // reset when PS is OFF or in CW mode
            }

            // Issue #175 follow-up bench (2026-05-04 JJ): the S-ATT spinbox
            // on RxApplet binds to attenuationChanged, so to make it follow
            // the live applied attenuation during TX (matching Thetis), we
            // stash the current RX att and emit the TX value here.
            //
            // Mirrors mi0bot console.cs:29960-30002 [v2.10.3.13-beta2]
            // updateAttNudsCombos() — Thetis swaps a separate udTXStepAttData
            // spinbox over udRX1StepAttData during MOX (same screen position,
            // different control).  NereusSDR has a single bound spinbox so
            // we update m_attDb + emit instead.  User-visible result is
            // identical: "the spinbox jumped to 31 during TX".
            //
            // Do NOT overwrite m_bandState[currentBand].attDb — that's the
            // user's RX-time setting and must survive the TX window untouched
            // (a band change during MOX would otherwise corrupt the stored
            // RX value).  Use the dedicated m_savedRxAttDbForTx stash.
            //
            // m_savedRxAttDbValid (#175 PR #194 review fix) gates the
            // matching TX→RX restore so it only runs when this branch
            // actually populated the stash.  Without the flag, the restore
            // ran unconditionally and clobbered the user's RX att value
            // with the default-zero stash whenever ATT-on-TX was OFF.
            m_savedRxAttDbForTx = m_attDb;
            m_savedRxAttDbValid = true;
            if (m_attDb != txAtt) {
                m_attDb = txAtt;
                emit attenuationChanged(txAtt);
            }

            // Marshalled to connection thread — m_connection is connection-thread owned.
            if (m_connection) {
                RadioConnection* conn = m_connection.get();
                QMetaObject::invokeMethod(conn, [conn, txAtt]() {
                    conn->setTxStepAttenuation(txAtt); //[2.10.3.6]MW0LGE att_fixes
                });
            }
            // SetupForm.ATTOnTX = txAtt; //[2.10.3.6]MW0LGE att_fixes NOTE: this will eventually call Display.TXAttenuatorOffset with the value
            setTxAttenuatorOffset(txAtt);
#ifdef NEREUS_BUILD_TESTS
            m_lastTxStepAttDb = txAtt;
#endif
        }
    } else {
        // TX→RX transition: restore RX state.
        if (m_isHpsdrBoard) {
            // HPSDR: restore the preamp modes saved at TX start, only with
            // ATT on TX on, as the key saved them only then.
            // From Thetis console.cs:29686-29693 [v2.10.3.15]:
            // [original inline comments from console.cs:29698-29699, on the
            // else branch below]
            //   //comboRX2Preamp.Enabled = true; //[2.10.3.6]MW0LGE att_fixes
            //   //udRX2StepAttData.Enabled = true; //[2.10.3.6]MW0LGE att_fixes
            //   if (m_bATTonTX)
            //   {
            //       if (HardwareSpecific.Model == HPSDRModel.HPSDR)
            //       {
            //           RX1PreampMode = temp_mode;
            //           if (_rx2_preamp_present)
            //               RX2PreampMode = temp_mode2;
            //       }
            if (m_attOnTxEnabled) {
                restoreRxPreampMode();
                if (rx2PreampPresent()) {
                    setRx2PreampMode(m_savedRx2PreampMode);
                }
            }
        } else {
            // Standard board: clear TX ATT back to 0 + restore the saved RX
            // att so the S-ATT spinbox tracks the un-keyed value.
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
            setTxAttenuatorOffset(0); //[2.10.3.6]MW0LGE att_fixes
#ifdef NEREUS_BUILD_TESTS
            m_lastTxStepAttDb = 0;
#endif

            // Issue #175 follow-up bench (2026-05-04 JJ): restore the RX att
            // we stashed on RX→TX so the spinbox snaps back to the un-keyed
            // value.  Mirrors Thetis updateAttNudsCombos() un-keying its
            // udTXStepAttData overlay and re-exposing udRX1StepAttData
            // (mi0bot console.cs:30004-30018 [v2.10.3.13-beta2]).
            //
            // Use direct emit rather than setAttenuation() to avoid pushing
            // to connection (connection layer already cleared TX ATT above;
            // the RX att will get re-pushed on the next CC bank that reads
            // m_stepAttn — already-stored unchanged from before MOX).
            //
            // #175 PR #194 review fix (2026-05-04): gate on
            // m_savedRxAttDbValid so the restore only runs when the matching
            // RX→TX branch actually populated the stash.  Without this
            // gate, MOX cycles with ATT-on-TX OFF clobbered the user's RX
            // att with the default-zero (or stale) stash value.
            //
            // Issue #200 fix (2026-05-08): the previous "defensive" band-state
            // re-sync below this line ran in the WRONG direction (band-state
            // → live), which clobbered the user's RX att with the stale
            // per-band slot value (often 0 from a never-touched persisted
            // slot) on every MOX cycle.  setAttenuation() now mirrors live →
            // per-band when not in MOX (Thetis-faithful, console.cs:11050-11051
            // [v2.10.3.13]), so the slot always matches the live value at
            // MOX-entry time and the stash is the only restore source needed.
            if (m_savedRxAttDbValid) {
                if (m_attDb != m_savedRxAttDbForTx) {
                    m_attDb = m_savedRxAttDbForTx;
                    emit attenuationChanged(m_attDb);
                }
                m_savedRxAttDbValid = false;
            }
        }
        // R-R3-46 / R-R3-11: an ADC whose attenuator moved while keyed
        // (setAdcRouting holds its sends through MOX, when attenuatorDb()
        // may hold the TX value) takes its receive value now.
        if (m_adcSendsHeldForMox) {
            m_adcSendsHeldForMox = false;
            for (int adc = 0; adc < kMaxAdcs; ++adc) {
                if (adc == m_rx1Adc || adc == m_rx2Adc) {
                    sendAttenuatorToAdc(adc, wireAttDbForAdc(adc));
                }
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

    // R-R3-46 / R-R3-11: a receive ADC past the red level.
    const auto red = [this](int adc) {
        return adc >= 0 && adc < kMaxAdcs
            && m_adcState[static_cast<size_t>(adc)].level > kRedThreshold;
    };

    // RX2's auto-attenuate has its own enable, apart from RX1's, and runs
    // only while receiving, as Thetis's does:
    // From Thetis console.cs:21548-21563 [v2.10.3.15]:
    //   // deal with RX
    //   if (!_mox)
    //   {
    //       ...
    //       if (!_auto_att_rx2)
    //       {
    //           if (_historic_attenuator_readings_rx2.Any())
    //           {
    //               _historic_attenuator_readings_rx2.Clear();
    //               AutoAttAppliedRX2 = false;
    if (!m_isMox) {
        if (m_rx2AutoAttEnabled) {
            runRx2AutoAtt(!m_adcAttLinked && red(m_rx2Adc));
        } else {
            m_rx2AutoAttHistory.clear();
        }
    }

    // Auto-attenuate on red overload.
    if (m_autoAttEnabled) {
        // R-R3-46 / R-R3-11: per receiver, on the ADC it uses, as Thetis:
        // From Thetis console.cs:21584-21588 [v2.10.3.15]:
        //   int nRX1ADCinUse = GetADCInUse(nRX1DDCinUse); // (rx1)
        //
        //   // rx1
        //   if (((_adc_overloaded[0] && _adc_overload_level[0] > 3) && nRX1ADCinUse == 0) || ((_adc_overloaded[1] && _adc_overload_level[1] > 3) && nRX1ADCinUse == 1)) // rx1 overload
        // and the same for RX2 on nRX2ADCinUse (console.cs 21668-21672).
        // Slice A's attenuator answers only its own ADC's overload (and the
        // other ADC's while diversity links them: Thetis's RX2 bump then
        // sets RX1's through the link); the other ADC's own attenuator
        // answers its ADC (runRx2AutoAtt, above).
        bool anyRed = false;
        int redAdc = -1;
        if (red(m_rx1Adc)) {
            anyRed = true;
            redAdc = m_rx1Adc;
        } else if (m_adcAttLinked && red(m_rx2Adc)) {
            anyRed = true;
            redAdc = m_rx2Adc;
        }

        // P1 full-parity §4.1: defence-in-depth — the setAutoAttMode +
        // loadSettings gates above already prevent m_autoAttMode reaching
        // Adaptive on a hasStepAttenuatorCal=false board, but if a stale
        // state slips through (e.g. flag toggled mid-run) treat it as
        // Classic here too.
        const bool adaptiveAllowed =
            (m_autoAttMode == AutoAttMode::Adaptive) && m_hasStepAttCal;

        // Skip auto-att during MOX.  Any ADC overflow during TX is own-TX
        // leakage — the operator-set ATT-on-TX value (or force-31 path)
        // already governs RX desensitization for this state.  Letting auto-
        // att run here would bump m_attDb past the intended TX value (e.g.
        // 31 → 32 → ...) on every overflow tick.
        if (m_isMox) {
            return;
        }

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

// --- Auto-attenuate for the other ADC's attenuator (R-R3-46 / R-R3-11) ---
//
// Thetis's RX2 auto-attenuate, run on the other ADC in use:
// From Thetis console.cs:21675-21690 [v2.10.3.15]:
//   if (_rx2_step_att_enabled)
//   {
//       har.stepAttenuator = RX2AttenuatorData;
//       har.band = RX2Band;
//
//       int att = har.stepAttenuator + (_adc_overloaded[0] ? _adc_step_shift[0] : _adc_step_shift[1]);
//       if (att > 31) att = 31;
//
//       if (att != har.stepAttenuator)
//       {
//           RX2AttenuatorData = att;
//           _auto_att_last_hold_time_rx2 = now;
//           _historic_attenuator_readings_rx2.Push(har);
// and its undo when the overload clears:
// From Thetis console.cs:21721-21736 [v2.10.3.15]:
//   else if ((nRX2ADCinUse == 0 || nRX2ADCinUse == 1) && _historic_attenuator_readings_rx2.Any()) // no overload rx2
//   {
//       if (!_auto_att_undo_rx2 || (_auto_att_undo_rx2 && ((now - _auto_att_last_hold_time_rx2).TotalSeconds >= _auto_att_hold_delay_rx2)))
//       {
//           // unwind
//           HistoricAttenuatorReading har = _historic_attenuator_readings_rx2.Pop();
//
//           bool adcs_linked = nRX1ADCinUse == nRX2ADCinUse;
//
//           if (!adcs_linked && har != null && _auto_att_undo_rx2) //[2.10.3.9]MW0LGE ignore if adcs linked, as will be maintained by rx1 data
//           {
//               if (_rx2_step_att_enabled && har.stepAttenuator != -1)
//               {
//                   if (har.stepAttenuator != RX2AttenuatorData) RX2AttenuatorData = har.stepAttenuator;
// RX2 keeps its own enable, undo and hold (setRx2AutoAtt...). The step is
// the ADC's overload level and the ceiling the attenuator's maximum, as
// slice A's Classic bump takes them (applyClassicAutoAtt). Thetis has no
// mode choice for RX2: Adaptive is a NereusSDR extension of slice A's.
// Level Cal: with RX2's step attenuator off, Thetis steps RX2's preamp mode
// instead, and the undo puts the mode back:
// From Thetis console.cs:21693-21716 [v2.10.3.15]:
//   else
//   {
//       har.preampMode = RX2PreampMode;
//
//       PreampMode pam = har.preampMode;
//       switch (pam)
//       {
//           case PreampMode.HPSDR_OFF:
//           case PreampMode.HPSDR_ON:
//               pam = PreampMode.SA_MINUS10;
//               break;
//           case PreampMode.SA_MINUS10:
//               pam = PreampMode.SA_MINUS20;
//               break;
//           case PreampMode.SA_MINUS20:
//               pam = PreampMode.SA_MINUS30;
//               break;
//       }
//       if (pam != har.preampMode)
//       {
//           RX2PreampMode = pam;
//           _auto_att_last_hold_time_rx2 = now;
//           _historic_attenuator_readings_rx2.Push(har);
// and in the unwind (console.cs 21737-21740):
//               else if (har.preampMode != PreampMode.FIRST)
//               {
//                   if (har.preampMode != RX2PreampMode) RX2PreampMode = har.preampMode;
//               }
void StepAttenuatorController::runRx2AutoAtt(bool overloaded)
{
    if (m_rx2Adc < 0 || m_adcAttLinked) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (overloaded) {
        if (!m_rx2StepAttEnabled) {
            PreampMode pam = m_rx2PreampMode;
            switch (pam) {
            case PreampMode::Off:
            case PreampMode::On:
                pam = PreampMode::SaMinus10;
                break;
            case PreampMode::SaMinus10:
                pam = PreampMode::SaMinus20;
                break;
            case PreampMode::SaMinus20:
                pam = PreampMode::SaMinus30;
                break;
            default:
                break;
            }
            if (pam != m_rx2PreampMode) {
                Rx2AttReading har;
                har.preampMode = m_rx2PreampMode;
                m_rx2AutoAttHistory.push_back(har);
                setRx2PreampMode(pam);
                m_rx2LastAutoAttTimeMs = now;
            }
            return;
        }
        const int shift = m_adcState[static_cast<size_t>(m_rx2Adc)].level;
        const int newAtt = std::min(m_rx2AttDb + shift, rx2MaxAttenuation());
        if (newAtt != m_rx2AttDb) {
            Rx2AttReading har;
            har.stepAttenuator = m_rx2AttDb;
            m_rx2AutoAttHistory.push_back(har);
            m_rx2AttDb = newAtt;
            sendRx2Attenuation();
            emit rx2AttenuationChanged(m_rx2AttDb);
            m_rx2LastAutoAttTimeMs = now;
        }
        return;
    }
    if (m_rx2AutoAttHistory.empty()) {
        return;
    }
    const qint64 holdMs = static_cast<qint64>(m_rx2AutoUndoDelaySec) * 1000;
    if (m_rx2AutoUndoEnabled && (now - m_rx2LastAutoAttTimeMs) < holdMs) {
        return;
    }
    // Unwind one reading; put it back only with undo on.
    const Rx2AttReading previous = m_rx2AutoAttHistory.back();
    m_rx2AutoAttHistory.pop_back();
    if (m_rx2AutoUndoEnabled) {
        if (m_rx2StepAttEnabled && previous.stepAttenuator != -1) {
            if (previous.stepAttenuator != m_rx2AttDb) {
                m_rx2AttDb = previous.stepAttenuator;
                sendRx2Attenuation();
                emit rx2AttenuationChanged(m_rx2AttDb);
            }
        } else if (previous.preampMode.has_value()) {
            if (*previous.preampMode != m_rx2PreampMode) {
                setRx2PreampMode(*previous.preampMode);
            }
        }
    }
    m_rx2LastAutoAttTimeMs = now;
}

// --- RX2's own step attenuator enable and auto-attenuate settings ---
//
// From Thetis console.cs:11109 [v2.10.3.15]:
//   private bool _rx2_step_att_enabled = false;
// From Thetis console.cs:21260-21265 [v2.10.3.15]:
//   private bool _auto_att_rx1 = false;
//   private bool _auto_att_rx2 = false;
//   private bool _auto_att_undo_rx1 = false;
//   private bool _auto_att_undo_rx2 = false;
//   private int _auto_att_hold_delay_rx1 = 5;
//   private int _auto_att_hold_delay_rx2 = 5;
// The defaults are Thetis's: RX2's enable off, auto-attenuate off, undo
// off, 5 s. A value saved for the radio is kept. While RX2 shares slice A's
// ADC its enable is RX1's anyway (the mirror below).
//
// Receivers on one ADC keep one enable, as Thetis's Setup mirrors the two
// checkboxes when the ADCs are the same:
// From Thetis setup.cs:15851-15854 [v2.10.3.15] (chkRX2StepAtt_CheckedChanged):
//   if (nRX1ADCinUse == nRX2ADCinUse && chkHermesStepAttenuator.Checked != chkRX2StepAtt.Checked)
//   {
//       chkHermesStepAttenuator.Checked = chkRX2StepAtt.Checked;
//   }
// (and the other way at setup.cs 15791-15793, and in updateAttenuationInfo
// at 15758-15760). Linked diversity keeps one value, so one enable too.
bool StepAttenuatorController::rx2OnItsOwnAdc() const noexcept
{
    return m_rx2Adc >= 0 && m_rx2Adc != m_rx1Adc && !m_adcAttLinked;
}

void StepAttenuatorController::setRx2StepAttEnabled(bool on)
{
    if (m_rx2StepAttEnabled != on) {
        m_rx2StepAttEnabled = on;
        emit rx2StepAttEnabledChanged(on);
        scheduleSave();
        // Thetis's chkRX2StepAtt re-applies RX2's value when it turns on
        // (udHermesStepAttenuatorDataRX2_ValueChanged).
        // Level Cal: turning it off sends RX2's preamp mode.
        // From Thetis console.cs:11117-11128 [v2.10.3.15] (RX2StepAttEnabled):
        //   if (_rx2_preamp_present)
        //   {
        //       if (_rx2_step_att_enabled)
        //       {
        //           udRX2StepAttData.Value = validateRX2StepAttData(getRX2stepAttenuatorForBand(rx2_band)); //[2.10.3.9]MW0LGE validated
        //           udRX2StepAttData_ValueChanged(this, EventArgs.Empty);
        //       }
        //       else
        //       {
        //           comboRX2Preamp_SelectedIndexChanged(this, EventArgs.Empty);
        //       }
        //   }
        if (on) {
            sendRx2Attenuation();
        } else if (rx2PreampPresent() && !m_isMox) {
            applyRx2PreampDrive();
        }
    }
    if (!rx2OnItsOwnAdc() && m_stepAttEnabled != on) {
        setStepAttEnabled(on);
    }
}

void StepAttenuatorController::setRx2AutoAttEnabled(bool on)
{
    if (m_rx2AutoAttEnabled == on) {
        return;
    }
    m_rx2AutoAttEnabled = on;
    if (!on) {
        m_rx2AutoAttHistory.clear();
    }
    emit rx2AutoAttEnabledChanged(on);
    scheduleSave();
}

void StepAttenuatorController::setRx2AutoAttUndo(bool on)
{
    if (m_rx2AutoUndoEnabled == on) {
        return;
    }
    m_rx2AutoUndoEnabled = on;
    emit rx2AutoAttUndoChanged(on);
    scheduleSave();
}

void StepAttenuatorController::setRx2AutoUndoDelaySec(int sec)
{
    if (m_rx2AutoUndoDelaySec == sec) {
        return;
    }
    m_rx2AutoUndoDelaySec = sec;
    emit rx2AutoUndoDelayChanged(sec);
    scheduleSave();
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
        // Preamp mode fallback: the step attenuator settings, set through
        // the RX1PreampMode setter so the radio receives each one.
        // From Thetis console.cs:21611-21633 [v2.10.3.15]:
        //   PreampMode pam = har.preampMode;
        //   switch (pam)
        //   {
        //       case PreampMode.HPSDR_OFF:
        //       case PreampMode.HPSDR_ON:
        //           pam = PreampMode.SA_MINUS10;
        //           break;
        //       case PreampMode.SA_MINUS10:
        //           pam = PreampMode.SA_MINUS20;
        //           break;
        //       case PreampMode.SA_MINUS20:
        //           pam = PreampMode.SA_MINUS30;
        //           break;
        //   }
        //   if (pam != har.preampMode)
        //   {
        //       RX1PreampMode = pam;
        PreampMode newMode = m_preampMode;
        switch (m_preampMode) {
        case PreampMode::Off:
        case PreampMode::On:
            newMode = PreampMode::SaMinus10;
            break;
        case PreampMode::SaMinus10:
            newMode = PreampMode::SaMinus20;
            break;
        case PreampMode::SaMinus20:
            newMode = PreampMode::SaMinus30;
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
            applyPreampDrive();
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
            // console.cs:21648-21651 [v2.10.3.15]: the undo goes through the
            // RX1PreampMode setter too:
            //   if (har.preampMode != RX1PreampMode) RX1PreampMode = har.preampMode;
            m_preampMode = m_classicSavedPreamp;
            applyPreampDrive();
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
    // v0.4.1 hotfix — same QMetaObject::invokeMethod marshalling as
    // setAttenuation / setPreampMode above.  applyAttToHardware is the
    // auto-attenuate convergence + classic / band-restore push path
    // (called from PureSignal::autoAttentionTick and the MOX-flip
    // restoration handlers); same thread-safety contract applies.
    // R-R3-46 / R-R3-11: RX1's value, so to slice A's ADC.
    sendRx1Attenuation(dB);
    emit attenuationChanged(m_attDb);
}

// --- Per-ADC receive attenuators (R-R3-46 / R-R3-11) ---
//
// See the header. Thetis sends each receiver's value to the ADC it is using:
// From Thetis console.cs:11062-11064 [v2.10.3.15] (RX1AttenuatorData):
//   if (nRX1ADCinUse == 0) NetworkIO.SetADC1StepAttenData(_rx1_attenuator_data);
//   else if (nRX1ADCinUse == 1) NetworkIO.SetADC2StepAttenData(_rx1_attenuator_data);
//   else if (nRX1ADCinUse == 2) NetworkIO.SetADC3StepAttenData(_rx1_attenuator_data);
//   ...
//   if (!_mox) //[2.10.3.9]MW0LGE note, this is not technically required for all radios except for the RedPitaya. See BODGE
// and RX2AttenuatorData the same with its own value:
// From Thetis console.cs:11228-11234 [v2.10.3.15]:
//   if (nRX2ADCinUse == 0) NetworkIO.SetADC1StepAttenData(rx2_attenuator_data);
//   else if (nRX2ADCinUse == 1) NetworkIO.SetADC2StepAttenData(rx2_attenuator_data);
//   else if (nRX2ADCinUse == 2) NetworkIO.SetADC3StepAttenData(rx2_attenuator_data);
//   }
//   }
//
//   if (!_mox || (_mox && VFOATX)) //[2.10.3.9]MW0LGE we should be able to do this if txing on rx1
//
// Above 31 dB on an Alex board Thetis also switches the Alex attenuator and
// sends RX1's value + 2 (console.cs 11044-11056); sendRx1Attenuation does
// that on a known board (Level Cal). RX2's setter has no Alex attenuator of
// its own, so RX2's own value is held to the second ADC's 0-31 dB field
// (rx2MaxAttenuation, kRx2StepAttMaxDb; Deliberate divergence (operator
// decision 2026-09-30)) rather than sent + 2 for the gateware to wrap.

bool StepAttenuatorController::adcUsesRx1Attenuator(int adc) const noexcept
{
    return m_adcAttLinked || adc < 0 || adc != m_rx2Adc || m_rx2Adc == m_rx1Adc;
}

int StepAttenuatorController::attenuatorDbForAdc(int adc) const noexcept
{
    return adcUsesRx1Attenuator(adc) ? m_attDb : m_rx2AttDb;
}

void StepAttenuatorController::setAttenuationForAdc(int adc, int dB)
{
    if (adcUsesRx1Attenuator(adc)) {
        setAttenuation(dB);
    } else {
        setRx2Attenuation(dB);
    }
}

StepAttenuatorController::AdcAttSnapshot StepAttenuatorController::adcAttSnapshot() const
{
    AdcAttSnapshot snap;
    for (int adc = 0; adc < kMaxAdcs; ++adc) {
        const auto i = static_cast<size_t>(adc);
        snap.inUse[i] = adc == m_rx1Adc || adc == m_rx2Adc;
        snap.dB[i] = wireAttDbForAdc(adc);
    }
    return snap;
}

void StepAttenuatorController::sendAttenuatorToAdc(int adc, int dB)
{
    if (!m_connection || adc < 0 || adc >= kMaxAdcs) {
        return;
    }
    // Marshalled to the connection thread, as every attenuator send here.
    RadioConnection* conn = m_connection.get();
    QMetaObject::invokeMethod(conn, [conn, adc, dB]() {
        conn->setAttenuatorForAdc(adc, dB);
    });
}

void StepAttenuatorController::sendAdcAttenuatorChanges(const AdcAttSnapshot& before)
{
    const AdcAttSnapshot after = adcAttSnapshot();
    for (int adc = 0; adc < kMaxAdcs; ++adc) {
        const auto i = static_cast<size_t>(adc);
        if (!after.inUse[i]) {
            continue;
        }
        if (!before.inUse[i] || before.dB[i] != after.dB[i]) {
            sendAttenuatorToAdc(adc, after.dB[i]);
        }
    }
}

void StepAttenuatorController::sendRx1Attenuation(int dB)
{
    // Level Cal: on a known board, Thetis's RX1AttenuatorData sends only
    // while the step attenuator is on, and on an Alex board above 31 dB
    // switches in the Alex attenuator and sends the value + 2.
    // From Thetis console.cs:11030-11065 [v2.10.3.15]:
    //   (the per-band store that follows carries //[2.10.3.9]MW0LGE)
    //   if (_rx1_step_att_enabled)
    //   {
    //       if (alexpresent &&
    //           ...
    //           HardwareSpecific.Model != HPSDRModel.ANAN_G2E && //N1GP G2E added
    //           ...
    //           HardwareSpecific.Model != HPSDRModel.REDPITAYA) //DH1KLM
    //       {
    //           if (_rx1_attenuator_data <= 31)
    //           {
    //               NetworkIO.SetAlexAtten(0); // 0dB Alex Attenuator
    //           ...
    //               NetworkIO.SetAlexAtten(3); // -30dB Alex Attenuator
    //               if (nRX1ADCinUse == 0) NetworkIO.SetADC1StepAttenData(_rx1_attenuator_data + 2);
    //           ...
    //       else
    //       {
    //           NetworkIO.SetAlexAtten(0);
    // (The linked other ADC takes the same value, as NereusSDR has done.)
    int wireDb = dB;
    if (m_boardKnown) {
        if (!m_stepAttEnabled) {
            return;
        }
        const bool alexOn = stepAttAlexEligible() && dB > 31;
        sendAlexAtten(alexOn ? 3 : 0);
        wireDb = rx1WireAttDbFor(dB);
    }
    sendAttenuatorToAdc(m_rx1Adc, wireDb);
    if (m_adcAttLinked && m_rx2Adc >= 0 && m_rx2Adc != m_rx1Adc) {
        sendAttenuatorToAdc(m_rx2Adc, wireDb);
    }
}

// --- Level Cal: the preamp mode's drive ---

StepAttenuatorController::PreampDrive
StepAttenuatorController::preampDriveFor(PreampMode mode) noexcept
{
    // From Thetis console.cs:19232-19284 [v2.10.3.15]:
    //   case PreampMode.HPSDR_ON:  //0dB
    //       rx1_att_value = 0;
    //       merc_preamp = 1; //no attn
    //       alex_atten = 0;
    //   case PreampMode.HPSDR_OFF: //-20dB
    //       rx1_att_value = 20;
    //       merc_preamp = 0; //attn inline
    //       alex_atten = 0;
    //   HPSDR_MINUS10 0/1/1, HPSDR_MINUS20 0/1/2, HPSDR_MINUS30 0/1/3,
    //   HPSDR_MINUS40 20/0/2, HPSDR_MINUS50 20/0/3, SA_MINUS10 10/0/0,
    //   SA_MINUS20 20/0/0, SA_MINUS30 30/0/0 (att / merc_preamp / alex_atten)
    switch (mode) {
    case PreampMode::On:        return {0, true, 0};
    case PreampMode::Off:       return {20, false, 0};
    case PreampMode::Minus10:   return {0, true, 1};
    case PreampMode::Minus20:   return {0, true, 2};
    case PreampMode::Minus30:   return {0, true, 3};
    case PreampMode::Minus40:   return {20, false, 2};
    case PreampMode::Minus50:   return {20, false, 3};
    case PreampMode::SaMinus10: return {10, false, 0};
    case PreampMode::SaMinus20: return {20, false, 0};
    case PreampMode::SaMinus30: return {30, false, 0};
    }
    return {0, false, 0};
}

bool StepAttenuatorController::isHpsdrModel() const noexcept
{
    return m_boardKnown ? (m_hpsdrModel == HPSDRModel::HPSDR) : m_isHpsdrBoard;
}

bool StepAttenuatorController::stepAttAlexEligible() const noexcept
{
    // From Thetis console.cs:11031-11043 [v2.10.3.15] (//N1GP, //DH1KLM):
    //   if (alexpresent &&
    //       HardwareSpecific.Model != HPSDRModel.ANAN10 &&
    //       HardwareSpecific.Model != HPSDRModel.ANAN10E &&
    //       HardwareSpecific.Model != HPSDRModel.ANAN7000D &&
    //       HardwareSpecific.Model != HPSDRModel.ANAN8000D &&
    //       HardwareSpecific.Model != HPSDRModel.ORIONMKII &&
    //       HardwareSpecific.Model != HPSDRModel.ANAN_G2E && //N1GP G2E added
    //       HardwareSpecific.Model != HPSDRModel.ANAN_G2 &&
    //       HardwareSpecific.Model != HPSDRModel.ANAN_G2_1K &&
    //       HardwareSpecific.Model != HPSDRModel.ANVELINAPRO3 &&
    //       HardwareSpecific.Model != HPSDRModel.REDPITAYA) //DH1KLM
    if (!m_boardKnown || !m_alexPresent) {
        return false;
    }
    switch (m_hpsdrModel) {
    case HPSDRModel::ANAN10:
    case HPSDRModel::ANAN10E:
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANAN8000D:
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN_G2E: //N1GP G2E added
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::REDPITAYA: //DH1KLM
        return false;
    default:
        return true;
    }
}

PreampMode StepAttenuatorController::clampPreampForBoard(PreampMode mode) const noexcept
{
    // From Thetis console.cs:19220-19227 [v2.10.3.15]:
    //   if (!alexpresent && ((rx1_preamp_mode == PreampMode.HPSDR_MINUS10) ||
    //                       ...
    //                       (rx1_preamp_mode == PreampMode.HPSDR_MINUS50)))
    //   {
    //       rx1_preamp_mode = PreampMode.HPSDR_OFF;
    //   }
    if (!m_boardKnown || m_alexPresent) {
        return mode;
    }
    switch (mode) {
    case PreampMode::Minus10:
    case PreampMode::Minus20:
    case PreampMode::Minus30:
    case PreampMode::Minus40:
    case PreampMode::Minus50:
        return PreampMode::Off;
    default:
        return mode;
    }
}

void StepAttenuatorController::recomputeMaxAtt()
{
    // Above 31 dB is the Alex attenuator plus the step attenuator, which
    // Thetis uses only on a board in its Alex list (stepAttAlexEligible);
    // any other known board stops at its own 31 dB.
    int effective = m_rawMaxAttDb;
    if (m_boardKnown && !stepAttAlexEligible() && effective > 31) {
        effective = 31;
    }
    if (effective == m_maxAttDb) {
        return;
    }
    m_maxAttDb = effective;
    emit attenuationRangeChanged(m_minAttDb, m_maxAttDb);
}

int StepAttenuatorController::rx1WireAttDbFor(int dB) const noexcept
{
    if (!m_boardKnown) {
        return dB;
    }
    if (!m_stepAttEnabled) {
        return preampDriveFor(m_preampMode).attDb;
    }
    return (stepAttAlexEligible() && dB > 31) ? dB + 2 : dB;
}

int StepAttenuatorController::wireAttDbForAdc(int adc) const noexcept
{
    if (adcUsesRx1Attenuator(adc)) {
        return rx1WireAttDbFor(m_attDb);
    }
    // Level Cal: with RX2's step attenuator off, the models in Thetis's
    // list carry RX2's preamp mode on its ADC (applyRx2PreampDrive).
    if (!m_rx2StepAttEnabled && rx2PreampDrivesAdc()) {
        return rx2PreampDriveFor(m_rx2PreampMode).attDb;
    }
    // Level Cal 2 review: RX2's own value never leaves the 5-bit field
    // (kRx2StepAttMaxDb), so the wire cannot wrap it.
    return std::clamp(m_rx2AttDb, m_minAttDb, rx2MaxAttenuation());
}

void StepAttenuatorController::applyPreampDrive()
{
    if (!m_connection) {
        return;
    }
    RadioConnection* conn = m_connection.get();
    if (!m_boardKnown) {
        // Before the board is known: the preamp bit alone, as before.
        const bool enabled = (m_preampMode != PreampMode::Off);
        QMetaObject::invokeMethod(conn, [conn, enabled]() {
            conn->setPreamp(enabled);
        });
        return;
    }
    const PreampDrive drive = preampDriveFor(m_preampMode);
    // From Thetis console.cs:19293-19330 [v2.10.3.15] (the Alex list in it
    // carries //N1GP G2E added and //DH1KLM):
    //   if (HardwareSpecific.Model != HPSDRModel.HPSDR)
    //   {
    //       if (!_rx1_step_att_enabled)
    //       {
    //           if (nRX1ADCinUse == 0) NetworkIO.SetADC1StepAttenData(rx1_att_value);
    //           ...
    //   }
    //   else
    //   {
    //       NetworkIO.SetRX1Preamp(merc_preamp);
    //   }
    //
    //   if (_rx1_step_att_enabled)
    //   {
    //       (the Alex list) ... SetAlexAtten(_rx1_attenuator_data <= 31 ? 0 : 3)
    //       else NetworkIO.SetAlexAtten(0);
    //   }
    //   else
    //   {
    //       NetworkIO.SetAlexAtten(alex_atten);
    if (!isHpsdrModel()) {
        if (!m_stepAttEnabled) {
            sendAttenuatorToAdc(m_rx1Adc, drive.attDb);
        }
    } else {
        const bool merc = drive.mercPreamp;
        QMetaObject::invokeMethod(conn, [conn, merc]() {
            conn->setPreamp(merc);
        });
    }
    if (m_stepAttEnabled) {
        sendAlexAtten((stepAttAlexEligible() && m_attDb > 31) ? 3 : 0);
    } else {
        sendAlexAtten(drive.alexAtten);
    }
}

void StepAttenuatorController::sendAlexAtten(int bits)
{
    if (!m_connection) {
        return;
    }
    RadioConnection* conn = m_connection.get();
    QMetaObject::invokeMethod(conn, [conn, bits]() {
        conn->setAlexAtten(bits);
    });
}

void StepAttenuatorController::sendRx2Attenuation()
{
    if (m_rx2Adc < 0 || m_rx2Adc == m_rx1Adc) {
        return;
    }
    sendAttenuatorToAdc(m_rx2Adc, wireAttDbForAdc(m_rx2Adc));
}

void StepAttenuatorController::setRx2Attenuation(int dB)
{
    // RX1's range while linked (setAttenuation takes it); RX2's own up to
    // the second ADC's field (rx2MaxAttenuation, kRx2StepAttMaxDb).
    const int asked = dB;
    dB = std::clamp(dB, m_minAttDb, m_adcAttLinked ? m_maxAttDb : rx2MaxAttenuation());
    // Linked (diversity): one value for both, set through RX1's, which
    // copies it here (setAttenuation), as Thetis's RX2 setter sets RX1's.
    // From Thetis console.cs:11246-11251 [v2.10.3.15] (RX2AttenuatorData):
    //   bool bRX1RX2diversity = m_bDiversityAttLinkForRX1andRX2 && (diversityForm != null && Diversity2 && diversityForm.EXTDIVOutput == 2); // if using diversity, and both rx's are linked, then we need to attenuate both //MW0LGE_[2.9.0.6]
    //   if (((nRX1ADCinUse == nRX2ADCinUse) || bRX1RX2diversity) && RX1AttenuatorData != rx2_attenuator_data)
    //   {
    //       _setFromOtherAttenuator = true;
    //       if (SetupForm.RX1EnableAtt != SetupForm.RX2EnableAtt) SetupForm.RX1EnableAtt = SetupForm.RX2EnableAtt;
    //       RX1AttenuatorData = rx2_attenuator_data;
    if (m_adcAttLinked) {
        setAttenuation(dB);
        return;
    }
    if (m_rx2AttDb == dB) {
        // A value above the range left it where it was: say so, so a
        // control showing the asked value returns to the kept one.
        if (asked != dB) {
            emit rx2AttenuationChanged(m_rx2AttDb);
        }
        return;
    }
    m_rx2AttDb = dB;
    sendRx2Attenuation();
    // The band keeps the value it was set to (Thetis
    // setRX2stepAttenuatorForBand(rx2_band, rx2_attenuator_data), guarded
    // by !_mox as RX1's is here).
    if (!m_isMox) {
        m_rx2BandAttDb[static_cast<int>(m_rx2Band)] = m_rx2AttDb;
    }
    emit rx2AttenuationChanged(m_rx2AttDb);
    scheduleSave();
}

void StepAttenuatorController::setRx2Band(Band band)
{
    if (m_rx2Band == band) {
        return;
    }
    // As setBand: before this radio's settings are loaded the band is only
    // noted; loadSettings restores the noted band's value.
    if (m_loadedMac.isEmpty()) {
        m_rx2Band = band;
        return;
    }
    // Save the band left, restore the band entered (if it has a value).
    // From Thetis console.cs:17479-17491 [v2.10.3.15] (RX2Band setter),
    // after the transverter band lookup:
    //   lo_band = BandByFreq(XVTRForm.TranslateFreq(VFOBFreq), rx2_xvtr_index, current_region);//MW0LGE use rx2_xvtr_index
    //                                                                                          //MW0LGE this was changed in RX1Band but not here
    //   if (!initializing && rx2_preamp_mode > PreampMode.FIRST)
    //   {
    //       rx2_preamp_by_band[(int)old_band] = rx2_preamp_mode;
    //       setRX2stepAttenuatorForBand(old_band, rx2_attenuator_data);
    //   }
    //   ...
    //       // save values for old band
    //       ...
    //       RX2PreampMode = rx2_preamp_by_band[(int)rx2_band];
    //       RX2AttenuatorData = getRX2stepAttenuatorForBand(rx2_band);
    //       int tmp = rx2_agct_by_band[(int)rx2_band]; //[2.10.3.6]MW0LGE see comment in RX1Band
    // Level Cal: RX2's preamp mode is kept per band as well
    // (rx2_preamp_by_band); like the attenuation, a band never visited
    // keeps the current mode.
    // Thetis keep_att_entries_for_band drops RX2's auto-attenuate history on
    // a band change (console.cs 21567-21569); the band keeps the value it
    // had, raised or not, as Thetis's RX2Band setter saves it.
    m_rx2AutoAttHistory.clear();
    m_rx2BandAttDb[static_cast<int>(m_rx2Band)] = m_rx2AttDb;
    m_rx2BandPreamp[static_cast<int>(m_rx2Band)] = m_rx2PreampMode;
    m_rx2Band = band;
    const auto it = m_rx2BandAttDb.find(static_cast<int>(band));
    if (it != m_rx2BandAttDb.end()) {
        const int restoredDb = std::clamp(it->second, m_minAttDb,
                                          m_adcAttLinked ? m_maxAttDb : rx2MaxAttenuation());
        if (restoredDb != m_rx2AttDb) {
            m_rx2AttDb = restoredDb;
            emit rx2AttenuationChanged(m_rx2AttDb);
        }
    }
    // The restored mode reaches the radio with the ADC's value
    // (setAdcRouting sends it; the HPSDR has no second ADC to move to).
    const auto pit = m_rx2BandPreamp.find(static_cast<int>(band));
    if (pit != m_rx2BandPreamp.end() && pit->second != m_rx2PreampMode) {
        m_rx2PreampMode = pit->second;
        emit rx2PreampModeChanged(m_rx2PreampMode);
    }
    scheduleSave();
}

void StepAttenuatorController::setAdcRouting(int rx1Adc, int rx2Adc, Band rx2Band, bool linked,
                                             quint32 rx2SliceMask)
{
    if (rx1Adc < 0 || rx1Adc >= kMaxAdcs) {
        rx1Adc = 0;
    }
    if (rx2Adc < 0 || rx2Adc >= kMaxAdcs || rx2Adc == rx1Adc) {
        rx2Adc = -1;
    }
    // Linking needs a second ADC to link to.
    linked = linked && rx2Adc >= 0;
    if (linked || rx2Adc < 0) {
        rx2SliceMask = 0;
    }

    const AdcAttSnapshot before = adcAttSnapshot();
    const bool moved = rx1Adc != m_rx1Adc || rx2Adc != m_rx2Adc || linked != m_adcAttLinked;
    const bool maskMoved = rx2SliceMask != m_rx2SliceMask;
    const bool linking = linked && !m_adcAttLinked;
    m_rx1Adc = rx1Adc;
    m_rx2Adc = rx2Adc;
    m_adcAttLinked = linked;
    m_rx2SliceMask = rx2SliceMask;

    // The other ADC's attenuator follows its controlling slice's band. With
    // no slice on it nothing controls it, so its band (and the band memory)
    // stays where the last slice left it.
    if (rx2Adc >= 0) {
        setRx2Band(rx2Band);
    }

    // Diversity links the two ADCs: both take RX1's value from here on.
    if (linking && m_rx2AttDb != m_attDb) {
        m_rx2AttDb = m_attDb;
        emit rx2AttenuationChanged(m_rx2AttDb);
    }
    // Level Cal 2 review: unlinked, RX2's own value is held to its field
    // (a linked value above 31 was RX1's, with the Alex attenuator in).
    if (!m_adcAttLinked && m_rx2AttDb > rx2MaxAttenuation()) {
        m_rx2AttDb = rx2MaxAttenuation();
        m_rx2BandAttDb[static_cast<int>(m_rx2Band)] = m_rx2AttDb;
        emit rx2AttenuationChanged(m_rx2AttDb);
    }

    // Send what moved: an ADC newly in use, or one whose value changed (a
    // swap, a new controlling slice's band, the link). A band restore alone
    // goes to the radio as RX1's does (setBandRestoreToRadio). Nothing goes
    // while keyed, when attenuatorDb() may hold the TX value: the change is
    // sent on the fall (onMoxHardwareFlipped).
    if (m_isMox) {
        m_adcSendsHeldForMox = m_adcSendsHeldForMox || moved || m_bandRestoreToRadio;
    } else if (moved || m_bandRestoreToRadio) {
        sendAdcAttenuatorChanges(before);
    }
    // Back on one ADC (or linked): RX2's enable follows RX1's, as Thetis's
    // updateAttenuationInfo does (setup.cs 15758-15760 [v2.10.3.15]).
    if (!rx2OnItsOwnAdc() && m_rx2StepAttEnabled != m_stepAttEnabled) {
        m_rx2StepAttEnabled = m_stepAttEnabled;
        emit rx2StepAttEnabledChanged(m_rx2StepAttEnabled);
    }
    // Level Cal: and RX2's preamp mode is RX1's, as the linked setters
    // leave them (console.cs:19384-19394, 19509-19519 [v2.10.3.15]).
    if (!rx2OnItsOwnAdc() && m_rx2PreampMode != m_preampMode) {
        m_rx2PreampMode = m_preampMode;
        m_rx2BandPreamp[static_cast<int>(m_rx2Band)] = m_rx2PreampMode;
        emit rx2PreampModeChanged(m_rx2PreampMode);
    }
    if (moved || maskMoved) {
        emit adcRoutingChanged();
    }
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
        // R-R3-46 / R-R3-11: a new connection's other ADC starts at 0 dB;
        // give it its own value now (nothing else would until it changes).
        sendRx2Attenuation();
    } else {
        m_tickTimer.stop();
    }
}

// --- ReceiverManager wiring ---

void StepAttenuatorController::setBoardIdentity(HPSDRHW board, HPSDRModel model,
                                                bool alexPresent)
{
    m_board = board;
    m_hpsdrModel = model;
    m_alexPresent = alexPresent;
    m_boardKnown = true;
    // Level Cal: a board outside Thetis's Alex list stops at 31 dB.
    recomputeMaxAtt();
}

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
    // Issue #259 gate: refuse to save until loadSettings has populated this
    // controller for the same MAC. Without this, the teardownConnection
    // that runs INSIDE RadioModel::connectToRadio (when a previous m_connection
    // still exists, e.g. auto-reconnect retry) would persist the constructor
    // defaults — m_attDb=0, m_stepAttEnabled=true — over the user's real
    // saved state. The bench trace at /tmp/nereus259/run.log shows the
    // failure mode: 18:31:11.242 save with m_attDb=0 wipes the prior session's
    // m_attDb=6, followed at 18:31:15.854 by a loadSettings that reads back
    // the freshly-clobbered zero.
    //
    // Empty mac is also rejected — same guard pattern as the
    // m_transmitModel.persistToSettings block at the matching site in
    // RadioModel::teardownConnection().
    if (mac.isEmpty() || m_loadedMac != mac) {
        return;
    }

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
    for (const Band band : kPerBandStateBands) {
        const int b = static_cast<int>(band);
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

    // R-R3-46 / R-R3-11: the other ADC's own value and band memory
    // (Thetis rx2_step_attenuator_by_band, console.cs 3076-3079), its enable
    // and its auto-attenuate settings.
    s.setHardwareValue(mac, QStringLiteral("options/stepAtt/rx2Value"),
                       QString::number(m_rx2AttDb));
    s.setHardwareValue(mac, QStringLiteral("options/stepAtt/rx2Enabled"),
                       m_rx2StepAttEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setHardwareValue(mac, QStringLiteral("options/autoAtt/rx2Enabled"),
                       m_rx2AutoAttEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setHardwareValue(mac, QStringLiteral("options/autoAtt/rx2Undo"),
                       m_rx2AutoUndoEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setHardwareValue(mac, QStringLiteral("options/autoAtt/rx2UndoDelaySec"),
                       QString::number(m_rx2AutoUndoDelaySec));
    for (const auto& [b, dB] : m_rx2BandAttDb) {
        if (b < 0 || b >= static_cast<int>(Band::SwlFirst)) {
            continue;
        }
        s.setHardwareValue(mac,
            QStringLiteral("options/stepAtt/rx2Band/") + bandKeyName(static_cast<Band>(b)),
            QString::number(dB));
    }
    if (static_cast<int>(m_rx2Band) < static_cast<int>(Band::SwlFirst)) {
        s.setHardwareValue(mac,
            QStringLiteral("options/stepAtt/rx2Band/") + bandKeyName(m_rx2Band),
            QString::number(m_rx2AttDb));
    }
    // Level Cal: RX2's preamp mode and its band memory (Thetis
    // rx2_preamp_by_band, saved at console.cs:3062-3065 [v2.10.3.15]), in
    // the ten-mode numbering from the start.
    s.setHardwareValue(mac, QStringLiteral("options/preamp/rx2Mode"),
                       QString::number(static_cast<int>(m_rx2PreampMode)));
    for (const auto& [b, mode] : m_rx2BandPreamp) {
        if (b < 0 || b >= static_cast<int>(Band::SwlFirst)) {
            continue;
        }
        s.setHardwareValue(mac,
            QStringLiteral("options/preamp/rx2Band/") + bandKeyName(static_cast<Band>(b)),
            QString::number(static_cast<int>(mode)));
    }
    if (static_cast<int>(m_rx2Band) < static_cast<int>(Band::SwlFirst)) {
        s.setHardwareValue(mac,
            QStringLiteral("options/preamp/rx2Band/") + bandKeyName(m_rx2Band),
            QString::number(static_cast<int>(m_rx2PreampMode)));
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
    for (int b = 0; b < kPerBandStateCount; ++b) {
        Band band = bandFromPerBandStateSlot(b);
        QString key = bandKeyName(band);
        s.setHardwareValue(mac,
            QStringLiteral("options/stepAtt/txBand/") + key,
            QString::number(m_txAttByBand[static_cast<size_t>(b)]));
    }

    s.save();
}

void StepAttenuatorController::loadSettings(const QString& mac)
{
    // Issue #259: record the MAC we're loading for so saveSettings can refuse
    // pre-load writes. Set BEFORE reading values so a synchronous early-
    // return path (none today, but a future maintainer's guard could
    // bail mid-load) still leaves the gate in a consistent state — the
    // save would re-emit our just-read state which is at worst identical
    // to disk.
    m_loadedMac = mac;
    // R-R3-46: the band memory is this radio's alone. A controller that
    // switches radios would otherwise restore the previous radio's values
    // on this one and save them under this MAC.
    m_bandState.clear();
    m_rx2BandAttDb.clear();
    m_rx2BandPreamp.clear();

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

    // Preamp modes stored before the SA modes existed used 0..6 on every
    // board with the old per-board labels. Move them once per radio to the
    // mode the same label now carries (BoardCapsTable::preampModeFromV1).
    // That needs the board, so an unknown board reads them as they are and
    // leaves the move for a load that knows it.
    const QString preampVersionKey = QStringLiteral("options/preamp/modeVersion");
    const bool movePreampModes = m_boardKnown
        && s.hardwareValue(mac, preampVersionKey, 1).toInt() < kPreampModeVersion;

    // Per-band ATT values and preamp modes.
    for (const Band band : kPerBandStateBands) {
        const int b = static_cast<int>(band);
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
                int mode = preampVal.toInt();
                if (movePreampModes) {
                    const int moved = BoardCapsTable::preampModeFromV1(
                        m_board, m_alexPresent, mode);
                    if (moved != mode) {
                        mode = moved;
                        s.setHardwareValue(mac,
                            QStringLiteral("options/preamp/rx1Band/") + key, mode);
                    }
                }
                st.preamp = static_cast<PreampMode>(mode);
            }
        }
    }

    if (movePreampModes) {
        s.setHardwareValue(mac, preampVersionKey, kPreampModeVersion);
    }

    // Restore current band's ATT/preamp from per-band storage.
    auto it = m_bandState.find(static_cast<int>(m_currentBand));
    if (it != m_bandState.end()) {
        m_attDb = it->second.attDb;
        m_preampMode = it->second.preamp;
    }
    // R-R3-46: a restored value is kept within this radio's range, as the
    // radio itself clamps it (setBand does the same).
    m_attDb = std::clamp(m_attDb, m_minAttDb, m_maxAttDb);

    // R-R3-46 / R-R3-11: the other ADC's own value and band memory, its
    // enable and its auto-attenuate settings, Thetis's defaults where none
    // is saved (_rx2_step_att_enabled = false, console.cs:11109).
    m_rx2StepAttEnabled = s.hardwareValue(mac, QStringLiteral("options/stepAtt/rx2Enabled"),
                                          QStringLiteral("False"))
                              .toString() == QStringLiteral("True");
    if (!rx2OnItsOwnAdc()) {
        m_rx2StepAttEnabled = m_stepAttEnabled;
    }
    m_rx2AutoAttEnabled = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx2Enabled"),
                                          QStringLiteral("False")).toString() == QStringLiteral("True");
    m_rx2AutoUndoEnabled = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx2Undo"),
                                           QStringLiteral("False")).toString() == QStringLiteral("True");
    m_rx2AutoUndoDelaySec = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx2UndoDelaySec"),
                                            5).toInt();
    m_rx2AutoAttHistory.clear();
    m_rx2AttDb = s.hardwareValue(mac, QStringLiteral("options/stepAtt/rx2Value"), 0).toInt();
    for (int b = 0; b < static_cast<int>(Band::SwlFirst); ++b) {
        const QVariant v = s.hardwareValue(mac,
            QStringLiteral("options/stepAtt/rx2Band/") + bandKeyName(static_cast<Band>(b)));
        if (v.isValid()) {
            m_rx2BandAttDb[b] = v.toInt();
        }
    }
    if (const auto rx2It = m_rx2BandAttDb.find(static_cast<int>(m_rx2Band));
        rx2It != m_rx2BandAttDb.end()) {
        m_rx2AttDb = rx2It->second;
    }
    m_rx2AttDb = std::clamp(m_rx2AttDb, m_minAttDb, rx2MaxAttenuation());
    if (m_adcAttLinked) {
        m_rx2AttDb = m_attDb;
    }
    // Level Cal: RX2's preamp mode and band memory, HPSDR_ON where none is
    // saved (rx2_preamp_by_band starts HPSDR_ON on every band,
    // console.cs:1814 [v2.10.3.15]).
    const auto validMode = [](int v) {
        return v >= static_cast<int>(PreampMode::Off)
            && v <= static_cast<int>(PreampMode::SaMinus30);
    };
    {
        const int v = s.hardwareValue(mac, QStringLiteral("options/preamp/rx2Mode"),
                                      static_cast<int>(PreampMode::On)).toInt();
        m_rx2PreampMode = validMode(v) ? static_cast<PreampMode>(v) : PreampMode::On;
    }
    for (int b = 0; b < static_cast<int>(Band::SwlFirst); ++b) {
        const QVariant v = s.hardwareValue(mac,
            QStringLiteral("options/preamp/rx2Band/") + bandKeyName(static_cast<Band>(b)));
        if (v.isValid() && validMode(v.toInt())) {
            m_rx2BandPreamp[b] = static_cast<PreampMode>(v.toInt());
        }
    }
    if (const auto rx2Pit = m_rx2BandPreamp.find(static_cast<int>(m_rx2Band));
        rx2Pit != m_rx2BandPreamp.end()) {
        m_rx2PreampMode = rx2Pit->second;
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
    for (int b = 0; b < kPerBandStateCount; ++b) {
        Band band = bandFromPerBandStateSlot(b);
        QString key = bandKeyName(band);
        QVariant txAttVal = s.hardwareValue(mac,
            QStringLiteral("options/stepAtt/txBand/") + key);
        if (txAttVal.isValid()) {
            m_txAttByBand[static_cast<size_t>(b)] = txAttVal.toInt();
        }
    }

    // Notify UI of restored values.
    //
    // Issue #259 fix: stepAttEnabledChanged is emitted here so the
    // "RX1 Enable" checkbox on Setup → General → Options picks up the
    // restored value on connect. Without this emit, the controller's
    // m_stepAttEnabled is correct but the checkbox stays at its
    // constructor default (unchecked) until the user clicks it — so
    // persistence appears broken even when the round-trip works.
    // Level Cal: a stored Alex setting reads Off on a board without Alex,
    // as Thetis's RX1PreampMode setter makes it (console.cs:19220-19227
    // [v2.10.3.15]).
    m_preampMode = clampPreampForBoard(m_preampMode);
    // Level Cal: on one ADC (or linked) RX2's mode is RX1's, as InitConsole
    // leaves them: RX1PreampMode's link sets RX2's first.
    if (!rx2OnItsOwnAdc()) {
        m_rx2PreampMode = m_preampMode;
    }
    emit attenuationChanged(m_attDb);
    emit preampModeChanged(m_preampMode);
    emit rx2PreampModeChanged(m_rx2PreampMode);
    emit stepAttEnabledChanged(m_stepAttEnabled);
    emit rx2AttenuationChanged(m_rx2AttDb);
    emit rx2StepAttEnabledChanged(m_rx2StepAttEnabled);
    emit rx2AutoAttEnabledChanged(m_rx2AutoAttEnabled);
    emit rx2AutoAttUndoChanged(m_rx2AutoUndoEnabled);
    emit rx2AutoUndoDelayChanged(m_rx2AutoUndoDelaySec);
    // R-R3-46 / R-R3-11: the restored values reach the radio now, as Thetis
    // sends the stored band values when it starts, by setting each through
    // its property with `initializing` briefly cleared (the setters return
    // early while it is set):
    // From Thetis console.cs:2174-2179 [v2.10.3.15] (InitConsole):
    //   initializing = false;
    //   RX1PreampMode = rx1_preamp_by_band[(int)rx1_band];
    //   RX1AttenuatorData = getRX1stepAttenuatorForBand(rx1_band);
    //   RX2PreampMode = rx2_preamp_by_band[(int)rx2_band];
    //   RX2AttenuatorData = getRX2stepAttenuatorForBand(rx2_band);
    //   initializing = true;
    // (SetupForHPSDRModel's SetComboPreampForHPSDR, console.cs 40891-40897,
    // does the same for a model change.) Without this slice A's restored
    // attenuation and preamp were not sent until they next changed. RX2's
    // restored preamp mode goes after them (Level Cal).
    // Level Cal: the preamp mode's whole drive (applyPreampDrive), then the
    // step attenuator, in InitConsole's order.
    if (m_connection && !m_isMox) {
        applyPreampDrive();
        sendRx1Attenuation(m_attDb);
        // RX2PreampMode = rx2_preamp_by_band[(int)rx2_band]; as above.
        if (!m_rx2StepAttEnabled) {
            applyRx2PreampDrive();
        }
    }
    // The other ADC in use takes its restored value now (its byte is not
    // otherwise sent until the value changes).
    sendRx2Attenuation();
    // R-R3-46: auto-attenuate mode, undo, undo delay and hold changed
    // silently above; a mirrored object re-reads everything here.
    emit settingsReloaded();
}

}  // namespace NereusSDR
