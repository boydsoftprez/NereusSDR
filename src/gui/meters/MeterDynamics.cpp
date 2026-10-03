// =================================================================
// MeterDynamics (NereusSDR)
// Ported from Thetis Project Files/Source/Console/MeterManager.cs.
// Modification history (NereusSDR):
//   2026-10-02 — C++20 timestamp scheduling by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via OpenAI Codex.
// =================================================================
/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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


#include "MeterDynamics.h"
#include <algorithm>
#include <cmath>
namespace NereusSDR {
void MeterDynamics::configure(double attack, double release, int intervalMs,
                              int historyMs, int ignoreHistoryMs)
{
    m_attack = std::clamp(attack, 0.0, 1.0);
    m_release = std::clamp(release, 0.0, 1.0);
    m_interval = std::max(1, intervalMs);
    // From Thetis MeterManager.cs:21409-21419 [v2.10.3.15]
    m_historyMs = std::max(m_interval, historyMs);
    m_ignoreMs = std::max(0, ignoreHistoryMs);
}
void MeterDynamics::reset(double minimum)
{
    m_minimum = minimum; m_value = minimum; m_input = minimum;
    m_available = false; m_lastTime = -1;
    clearHistory();
}
void MeterDynamics::clearHistory()
{
    // From Thetis MeterManager.cs:21449-21458 [v2.10.3.15]
    m_history.clear();
    m_ignoreSamples = m_ignoreMs / m_interval; // ignore next N readings to make up 2 second of ignore
}
void MeterDynamics::push(double reading, bool available)
{
    m_available = available && std::isfinite(reading);
    if (m_available) { m_input = reading; }
    else { m_value = m_minimum; m_history.clear(); }
}
bool MeterDynamics::advance(qint64 monotonicMs)
{
    if (m_lastTime >= 0 && monotonicMs - m_lastTime < m_interval) { return false; }
    m_lastTime = monotonicMs;
    if (!m_available) { return false; }
    const double previous = m_value;
    const double oldMin = minHistory(), oldMax = maxHistory();
    // From Thetis MeterManager.cs:21323-21385 [v2.10.3.15]
    // get latest reading
    const double reading = m_input;
    if (reading > m_value) { m_value = reading * m_attack + m_value * (1.0 - m_attack); }
    else { m_value = reading * m_release + m_value * (1.0 - m_release); }
    // Timestamp expiry is the NereusSDR adaptation for missed GUI frames.
    m_history.removeIf([&](const Sample& s) { return monotonicMs - s.time >= m_historyMs; });
    if (m_ignoreSamples <= 0) {
        // signal history
        m_history.append({monotonicMs, m_value}); // adds to end of the list
        const int numberToRemove = m_history.size() - m_historyMs / m_interval;
        // the list is sized based on delay
        if (numberToRemove > 0) { m_history.remove(0, numberToRemove); } // remove the oldest, the head of the list
    } else { --m_ignoreSamples; }
    return previous != m_value || oldMin != minHistory() || oldMax != maxHistory();
}
// From Thetis MeterManager.cs:21427-21448 [v2.10.3.15]
double MeterDynamics::minHistory() const
{
    double result = m_history.isEmpty() ? m_value : m_history.first().value;
    for (const Sample& s : m_history) { result = std::min(result, s.value); }
    return result;
}
double MeterDynamics::maxHistory() const
{
    double result = m_history.isEmpty() ? m_value : m_history.first().value;
    for (const Sample& s : m_history) { result = std::max(result, s.value); }
    return result;
}
}
