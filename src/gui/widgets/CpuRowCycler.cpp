// no-port-check: NereusSDR-original. See header.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/widgets/CpuRowCycler.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for full
// Modification history (NereusSDR).
// =================================================================

#include "gui/widgets/CpuRowCycler.h"

namespace NereusSDR {

void CpuRowCycler::setSource(Source source)
{
    m_source = source;
    m_sinceSwitchMs = 0;
}

void CpuRowCycler::setThisComputer(double percent)
{
    m_local = percent;
}

void CpuRowCycler::setCore(std::optional<double> percent, const QString& unavailableReason)
{
    m_core = percent;
    m_coreReason = percent ? QString() : unavailableReason;
    if (!m_core) {
        m_showingCore = false;
    }
}

void CpuRowCycler::advance(qint64 elapsedMs)
{
    if (m_source != Source::Cycle || !m_core) {
        m_sinceSwitchMs = 0;
        return;
    }
    const bool localHot = m_local > kHotPercent;
    const bool coreHot = *m_core > kHotPercent;
    if (localHot != coreHot) {
        // One reading is hot: stop on it until it drops back.
        m_showingCore = coreHot;
        m_sinceSwitchMs = 0;
        return;
    }
    if (localHot && coreHot) {
        // Both hot: stay where the row is.
        m_sinceSwitchMs = 0;
        return;
    }
    m_sinceSwitchMs += elapsedMs;
    if (m_sinceSwitchMs >= kCycleMs) {
        m_showingCore = !m_showingCore;
        m_sinceSwitchMs = 0;
    }
}

CpuRowCycler::Row CpuRowCycler::row() const
{
    bool core = false;
    if (m_core) {
        core = m_source == Source::Core
            || (m_source == Source::Cycle && m_showingCore);
    }
    Row r;
    r.core = core;
    r.label = core ? QStringLiteral("Core") : QStringLiteral("CPU");
    r.percent = core ? *m_core : m_local;
    r.warning = r.percent > kHotPercent;
    if (!m_core) {
        r.toolTip = m_coreReason;
    } else {
        r.toolTip = core ? QStringLiteral("The Core's CPU.") : QStringLiteral("This computer's CPU.");
    }
    return r;
}

QString CpuRowCycler::sourceKey(Source source)
{
    switch (source) {
    case Source::ThisComputer: return QStringLiteral("ThisComputer");
    case Source::Core: return QStringLiteral("Core");
    case Source::Cycle: break;
    }
    return QStringLiteral("Cycle");
}

CpuRowCycler::Source CpuRowCycler::sourceFromKey(const QString& key)
{
    if (key == QLatin1String("ThisComputer")) { return Source::ThisComputer; }
    if (key == QLatin1String("Core")) { return Source::Core; }
    return Source::Cycle;
}

} // namespace NereusSDR
