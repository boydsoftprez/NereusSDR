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

#include <QDateTime>

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
    if (dB < 0) { dB = 0; }
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

        if (anyRed) {
            if (m_autoAttMode == AutoAttMode::Classic) {
                applyClassicAutoAtt(redAdc);
            } else {
                applyAdaptiveAutoAtt(redAdc);
            }
        } else if (m_autoAttApplied) {
            // No overload — try undo.
            if (m_autoAttMode == AutoAttMode::Classic) {
                applyClassicUndo();
            }
            // Adaptive decay is handled inside applyAdaptiveAutoAtt
            // even when there's no red — but only if previously applied.
            if (m_autoAttMode == AutoAttMode::Adaptive && m_autoAttApplied) {
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
    for (int b = 0; b < static_cast<int>(Band::Count); ++b) {
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
    m_autoAttMode = (modeStr == QStringLiteral("Adaptive"))
                        ? AutoAttMode::Adaptive : AutoAttMode::Classic;
    m_autoUndoEnabled = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx1Undo"),
                                         QStringLiteral("False")).toString() == QStringLiteral("True");
    m_adaptiveHoldMs = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx1HoldSeconds"),
                                       2).toInt() * 1000;
    m_autoUndoDelaySec = s.hardwareValue(mac, QStringLiteral("options/autoAtt/rx1UndoDelaySec"),
                                         5).toInt();

    // Per-band ATT values and preamp modes.
    for (int b = 0; b < static_cast<int>(Band::Count); ++b) {
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

    // Notify UI of restored values.
    emit attenuationChanged(m_attDb);
    emit preampModeChanged(m_preampMode);
}

}  // namespace NereusSDR
