// =================================================================
// src/core/accessories/ApolloController.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs:15566-15590
//   (chkApolloPresent_CheckedChanged, chkApolloFilter_CheckedChanged,
//    chkApolloTuner_CheckedChanged — Apollo PA + ATU + LPF accessory toggles)
//   Project Files/Source/Console/console.cs:19060-19105
//   (ApolloPresent property, ApolloTunerEnabled property)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Models the Apollo PA + ATU + LPF accessory
//                board present/filter/tuner booleans. No tuner-state enum
//                found in Thetis source — only a bool enable flag. Per-MAC
//                persistence via AppSettings. Setup UI deferred to a
//                follow-up if Apollo owners request it.
// =================================================================
//
// === Verbatim Thetis Console/setup.cs header (lines 1-43) ===
//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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
// =================================================================

#include "ApolloController.h"
#include "core/AppSettings.h"

namespace NereusSDR {

// Source: setup.cs:15566-15590 + console.cs:19068-19105 [@501e3f5]
// Defaults match Thetis private fields:
//   private bool apollopresent = false;        [console.cs:19068]
//   (no explicit filter default — effectively false until chkApolloFilter toggled)
//   private bool apollo_tuner_enabled = false; [console.cs:19097]
ApolloController::ApolloController(QObject* parent) : QObject(parent)
{
    // all fields default-initialised to false in the header (m_present, m_filterEnabled, m_tunerEnabled)
}

// Source: console.cs:19070 ApolloPresent getter [@501e3f5]
bool ApolloController::present() const { return m_present; }

// Source: console.cs:19073 ApolloPresent setter [@501e3f5]
// "apollopresent = value;"
// "if (apollopresent) { if (!comboMeterTXMode.Items.Contains(...)) comboMeterTXMode.Items.Insert(1, "Ref Pwr"); }
//  else { if (!initializing) { if (comboMeterTXMode.Items.Contains(...)) comboMeterTXMode.Items.Remove("Ref Pwr"); } }
//  if (oldValue != apollopresent) ApolloPresentChangedHandlers?.Invoke(oldValue, apollopresent); //MW0LGE_[2.9.0.7]"
// NereusSDR: UI side-effects (comboMeterTXMode) handled by UI layer when it listens to presentChanged().
void ApolloController::setPresent(bool on)
{
    if (m_present == on) { return; }
    m_present = on;
    emit presentChanged(on);
}

// Source: setup.cs:15582 chkApolloFilter_CheckedChanged [@501e3f5]
// "if (chkApolloFilter.Checked) NetworkIO.EnableApolloFilter(1); else NetworkIO.EnableApolloFilter(0);"
// NereusSDR: protocol wire-up (NetworkIO equivalent) deferred to codec layer.
bool ApolloController::filterEnabled() const { return m_filterEnabled; }

void ApolloController::setFilterEnabled(bool on)
{
    if (m_filterEnabled == on) { return; }
    m_filterEnabled = on;
    emit filterEnabledChanged(on);
}

// Source: setup.cs:15587 chkApolloTuner_CheckedChanged + console.cs:19097 ApolloTunerEnabled [@501e3f5]
// "console.ApolloTunerEnabled = chkApolloTuner.Checked;"
// "apollo_tuner_enabled = value; if (apollo_tuner_enabled) NetworkIO.EnableApolloTuner(1);
//  else NetworkIO.EnableApolloTuner(0);"
// NereusSDR: protocol wire-up (NetworkIO.EnableApolloTuner) deferred to codec layer.
bool ApolloController::tunerEnabled() const { return m_tunerEnabled; }

void ApolloController::setTunerEnabled(bool on)
{
    if (m_tunerEnabled == on) { return; }
    m_tunerEnabled = on;
    emit tunerEnabledChanged(on);
}

void ApolloController::setMacAddress(const QString& mac) { m_mac = mac; }

QString ApolloController::persistenceKey() const
{
    return QStringLiteral("hardware/%1/apollo").arg(m_mac);
}

void ApolloController::load()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString base = persistenceKey();
    m_present       = (s.value(QStringLiteral("%1/present").arg(base),       QStringLiteral("False")).toString() == QStringLiteral("True"));
    m_filterEnabled = (s.value(QStringLiteral("%1/filterEnabled").arg(base), QStringLiteral("False")).toString() == QStringLiteral("True"));
    m_tunerEnabled  = (s.value(QStringLiteral("%1/tunerEnabled").arg(base),  QStringLiteral("False")).toString() == QStringLiteral("True"));
    emit presentChanged(m_present);
    emit filterEnabledChanged(m_filterEnabled);
    emit tunerEnabledChanged(m_tunerEnabled);
}

void ApolloController::save()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString base = persistenceKey();
    s.setValue(QStringLiteral("%1/present").arg(base),       m_present       ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("%1/filterEnabled").arg(base), m_filterEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("%1/tunerEnabled").arg(base),  m_tunerEnabled  ? QStringLiteral("True") : QStringLiteral("False"));
}

} // namespace NereusSDR
