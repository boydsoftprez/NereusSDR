// SPDX-License-Identifier: GPL-3.0-or-later

// =================================================================
// src/core/roundtable/RoundtableSignalFloor.cpp  (Longpath)
// =================================================================
//
// See RoundtableSignalFloor.h.
//
// Uebernommen aus dem Upstream NereusSDR (`src/core/roundtable/RoundtableSignalFloor.cpp` @ 9036ffc04,
// J.J. Boyd KG4VCF). Longpath-Abweichung: Namensraum Longpath, kein Export-Makro.
//
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 10, R-RT-6).
// Modification history (Longpath):
//   2026-11-14 - Uebernommen fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.

#include "core/roundtable/RoundtableSignalFloor.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Longpath {

void RoundtableSignalFloor::retune(qint64 nowMs)
{
    m_window.clear();
    m_warmupEndsMs = nowMs + kWarmupMs;
    m_started = true;
    m_present = false;
}

void RoundtableSignalFloor::dropOlderThan(qint64 nowMs)
{
    while (!m_window.empty() && m_window.front().first <= nowMs - kWindowMs) {
        m_window.pop_front();
    }
}

bool RoundtableSignalFloor::floor(double* dbm) const
{
    if (m_window.empty()) {
        return false;
    }
    std::vector<double> values;
    values.reserve(m_window.size());
    for (const auto& entry : m_window) {
        values.push_back(entry.second);
    }
    // Nearest rank: the ceil(p * n)th smallest, at least the first.
    const auto n = values.size();
    auto rank = static_cast<size_t>(std::ceil(kFloorPercentile * static_cast<double>(n)));
    rank = std::max<size_t>(rank, 1);
    const auto nth = values.begin() + static_cast<std::ptrdiff_t>(rank - 1);
    std::nth_element(values.begin(), nth, values.end());
    if (dbm != nullptr) {
        *dbm = *nth;
    }
    return true;
}

bool RoundtableSignalFloor::update(double dbm, qint64 nowMs)
{
    if (!m_started) {
        retune(nowMs);
    }
    if (nowMs < m_warmupEndsMs) {
        // The floor forms; nothing is present yet.
        dropOlderThan(nowMs);
        m_window.emplace_back(nowMs, dbm);
        m_present = false;
        return false;
    }
    if (!m_present) {
        dropOlderThan(nowMs);
    }
    double floorDbm = 0.0;
    const bool haveFloor = floor(&floorDbm);
    m_present = haveFloor && dbm >= floorDbm + kPresentAboveFloorDb;
    if (!m_present) {
        m_window.emplace_back(nowMs, dbm);
    }
    return m_present;
}

} // namespace Longpath
