// =================================================================
// src/core/RadioStatus.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/NetworkIOImports.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//                 SWR formula from console.cs:6642 SWR(adc_fwd,adc_rev)
//                 [@501e3f5].  PA forward/reflected power getters from
//                 NetworkIOImports.cs:264-267 [@501e3f5].
// =================================================================

// --- From console.cs ---

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

// --- From NetworkIOImports.cs ---
// NetworkIOImports.cs — Upstream source has no top-of-file GPL header;
// project-level LICENSE (GPLv2-or-later) applies per Thetis repo root.

#include "RadioStatus.h"
#include <QDateTime>
#include <algorithm>
#include <cmath>

namespace NereusSDR {

RadioStatus::RadioStatus(QObject* parent)
    : QObject(parent)
{
}

// ── Accessors ──────────────────────────────────────────────────────────────

double RadioStatus::forwardPowerWatts() const { return m_forward; }
double RadioStatus::reflectedPowerWatts() const { return m_reflected; }
double RadioStatus::swrRatio() const { return m_swr; }
int    RadioStatus::exciterPowerMw() const { return m_exciterMw; }
double RadioStatus::paTemperatureCelsius() const { return m_paTemp; }
double RadioStatus::paCurrentAmps() const { return m_paCurrent; }
PttSource RadioStatus::activePttSource() const { return m_pttSource; }
bool   RadioStatus::isTransmitting() const { return m_transmitting; }

QVector<RadioStatus::PttEvent> RadioStatus::recentPttEvents() const
{
    return m_pttHistory;
}

// ── Setters ────────────────────────────────────────────────────────────────

void RadioStatus::setForwardPower(double watts)
{
    if (qFuzzyCompare(m_forward, watts)) { return; }
    m_forward = watts;
    m_swr = computeSwr(m_forward, m_reflected);
    emit powerChanged(m_forward, m_reflected, m_swr);
}

void RadioStatus::setReflectedPower(double watts)
{
    if (qFuzzyCompare(m_reflected, watts)) { return; }
    m_reflected = watts;
    m_swr = computeSwr(m_forward, m_reflected);
    emit powerChanged(m_forward, m_reflected, m_swr);
}

void RadioStatus::setExciterPowerMw(int mw)
{
    m_exciterMw = mw;
}

void RadioStatus::setPaTemperature(double celsius)
{
    if (qFuzzyCompare(m_paTemp, celsius)) { return; }
    m_paTemp = celsius;
    emit paTemperatureChanged(m_paTemp);
}

void RadioStatus::setPaCurrent(double amps)
{
    if (qFuzzyCompare(m_paCurrent, amps)) { return; }
    m_paCurrent = amps;
    emit paCurrentChanged(m_paCurrent);
}

void RadioStatus::setActivePttSource(PttSource source)
{
    bool wasTransmitting = m_transmitting;
    PttSource prev = m_pttSource;
    m_pttSource = source;
    // Any non-None source means transmitting.
    m_transmitting = (source != PttSource::None);

    bool stateChanged = (m_transmitting != wasTransmitting) || (m_pttSource != prev);
    if (stateChanged) {
        recordPttEvent(source, m_transmitting);
        emit pttChanged(m_pttSource, m_transmitting);
    }
}

void RadioStatus::setTransmitting(bool tx)
{
    if (m_transmitting == tx) { return; }
    m_transmitting = tx;
    if (!tx) {
        // Release — preserve last source but record the event.
        recordPttEvent(m_pttSource, false);
        m_pttSource = PttSource::None;
    }
    emit pttChanged(m_pttSource, m_transmitting);
}

// ── Private helpers ────────────────────────────────────────────────────────

void RadioStatus::recordPttEvent(PttSource s, bool start)
{
    PttEvent ev;
    ev.timestampMs = QDateTime::currentMSecsSinceEpoch();
    ev.source      = s;
    ev.isStart     = start;
    m_pttHistory.prepend(ev);

    // Cap ring at kMaxPttHistory entries.
    while (m_pttHistory.size() > kMaxPttHistory) {
        m_pttHistory.removeLast();
    }
}

// SWR computation — watts-domain equivalent of Thetis console.cs:6642
// SWR(int adc_fwd, int adc_rev) [@501e3f5].
//
// Thetis original (voltage domain):
//   if (adc_fwd == 0 && adc_rev == 0) return 1.0;
//   else if (adc_rev > adc_fwd)       return 50.0;
//   double Ef = ScaledVoltage(adc_fwd);
//   double Er = ScaledVoltage(adc_rev);
//   swr = (Ef + Er) / (Ef - Er);
//
// Since voltage ∝ sqrt(watts), the ARRL equivalent applied to watts is:
//   rho = sqrt(refl/fwd);  swr = (1 + rho) / (1 - rho)
// which is identical in shape to Thetis's (Ef+Er)/(Ef−Er) with Ef=sqrt(fwd),
// Er=sqrt(refl).  Result clamped to [1.0, 99.0].
double RadioStatus::computeSwr(double fwd, double refl)
{
    // From Thetis console.cs:6644 [@501e3f5]: both zero → 1.0
    if (fwd <= 0.0 && refl <= 0.0) { return 1.0; }

    // NereusSDR-original safety: forward-power noise floor.  Below 0.5 W
    // SWR is meaningless (no meaningful drive on the line, sqrt(refl/0)
    // → infinity).  HL2 specifically can have refl > 0 with fwd = 0
    // during MOX-on/MOX-off transitions, which would trip the
    // "refl >= fwd → 50.0" branch below and spike the meter to max scale.
    // Standard meter convention is to display 1.0 when there's no real
    // forward signal to measure.  Thetis uses the same idea via a
    // "high SWR alarm" suppression at low power but the threshold lives
    // in protection.cs not in computeSwr; we apply it here at the source.
    constexpr double kSwrNoiseFloorWatts = 0.5;
    if (fwd < kSwrNoiseFloorWatts) { return 1.0; }

    // From Thetis console.cs:6646 [@501e3f5]: rev > fwd → 50.0 (open/short limit)
    if (refl >= fwd) { return 50.0; }

    double rho = std::sqrt(refl / fwd);
    double denom = 1.0 - rho;
    if (denom <= 0.0) { return 99.0; }

    double swr = (1.0 + rho) / denom;
    // Clamp to [1.0, 99.0]
    return std::clamp(swr, 1.0, 99.0);
}

} // namespace NereusSDR
