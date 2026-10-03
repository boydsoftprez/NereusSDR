// =================================================================
// src/core/session/media/RtpDuplicateFilter.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 29 (R-IOS-16): see RtpDuplicateFilter.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/media/RtpDuplicateFilter.h"

#include <QtEndian>

#include <algorithm>

namespace NereusSDR {

bool RtpDuplicateFilter::admit(quint32 ssrc, quint32 timestamp)
{
    std::deque<quint32>& seen = m_seen[ssrc];
    if (std::find(seen.cbegin(), seen.cend(), timestamp) != seen.cend()) {
        ++m_dropped;
        return false;
    }
    seen.push_back(timestamp);
    while (seen.size() > static_cast<std::size_t>(kWindow)) {
        seen.pop_front();
    }
    return true;
}

bool RtpDuplicateFilter::admit(const QByteArray& packet)
{
    if (packet.size() < 12) {
        return true;
    }
    const quint32 timestamp = qFromBigEndian<quint32>(packet.constData() + 4);
    const quint32 ssrc = qFromBigEndian<quint32>(packet.constData() + 8);
    return admit(ssrc, timestamp);
}

void RtpDuplicateFilter::clear()
{
    m_seen.clear();
    m_dropped = 0;
}

} // namespace NereusSDR
